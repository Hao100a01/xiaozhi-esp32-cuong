#include "uta_location_portal.h"

#include <array>
#include <cstring>
#include <memory>
#include <mutex>
#include <string_view>

#include <esp_http_server.h>
#include <esp_log.h>
#include "sdkconfig.h"

#include "uta_location.h"
#include "uta_location_json.h"
#include "uta_portal_style.h"
#include "uta_provinces.h"

extern const char wifi_portal_html[] asm("_binary_wifi_configuration_html_start");

extern "C" esp_err_t __real_httpd_register_uri_handler(httpd_handle_t server,
                                                       const httpd_uri_t* route);

namespace {
using JsonPtr = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
httpd_handle_t candidate_server = nullptr;
httpd_uri_t original_index{};
std::mutex registration_mutex;

esp_err_t JsonReply(httpd_req_t* request, const cJSON* root) {
    char* body = cJSON_PrintUnformatted(root);
    if (!body)
        return ESP_ERR_NO_MEM;
    httpd_resp_set_type(request, "application/json; charset=utf-8");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    const auto error = httpd_resp_sendstr(request, body);
    cJSON_free(body);
    return error;
}

esp_err_t LocationConfig(httpd_req_t* request) {
    JsonPtr root(cJSON_CreateObject(), cJSON_Delete);
    if (!root)
        return ESP_ERR_NO_MEM;
    auto& service = uta::LocationService::GetInstance();
    if (!cJSON_AddStringToObject(root.get(), "province", service.ProvinceCode().c_str()) ||
        !cJSON_AddStringToObject(root.get(), "ota_default", CONFIG_OTA_URL))
        return ESP_ERR_NO_MEM;
    auto* provinces = cJSON_AddArrayToObject(root.get(), "provinces");
    if (!provinces)
        return ESP_ERR_NO_MEM;
    for (const auto& province : uta::kProvinces) {
        JsonPtr item(cJSON_CreateObject(), cJSON_Delete);
        if (!item || !cJSON_AddStringToObject(item.get(), "code", province.code.data()) ||
            !cJSON_AddStringToObject(item.get(), "name", province.name.data()))
            return ESP_ERR_NO_MEM;
        if (!cJSON_AddItemToArray(provinces, item.get()))
            return ESP_ERR_NO_MEM;
        item.release();
    }
    const auto location = service.Get();
    if (!cJSON_AddBoolToObject(root.get(), "available", location.available) ||
        !cJSON_AddBoolToObject(root.get(), "from_ip", location.from_ip) ||
        !cJSON_AddStringToObject(root.get(), "name", location.name.c_str()))
        return ESP_ERR_NO_MEM;
    if (location.available &&
        (!cJSON_AddNumberToObject(root.get(), "latitude", location.latitude) ||
         !cJSON_AddNumberToObject(root.get(), "longitude", location.longitude)))
        return ESP_ERR_NO_MEM;
    return JsonReply(request, root.get());
}

esp_err_t SaveLocation(httpd_req_t* request) {
    std::array<char, 64> type{};
    std::array<char, 8> marker{};
    if (httpd_req_get_hdr_value_str(request, "Content-Type", type.data(), type.size()) != ESP_OK ||
        (std::string_view(type.data()) != "application/json" &&
         !std::string_view(type.data()).starts_with("application/json;")) ||
        httpd_req_get_hdr_value_str(request, "X-UTA-Yeu-Cau", marker.data(), marker.size()) !=
            ESP_OK ||
        std::strcmp(marker.data(), "1") != 0)
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid request headers");
    if (request->content_len == 0 || request->content_len > 80)
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid body length");
    std::array<char, 81> body{};
    size_t received = 0;
    while (received < request->content_len) {
        const int count =
            httpd_req_recv(request, body.data() + received, request->content_len - received);
        if (count <= 0)
            return httpd_resp_send_408(request);
        received += count;
    }
    // Mã tỉnh chỉ có chữ số ASCII; không nhận escape che giấu NUL trong cJSON.
    if (std::memchr(body.data(), '\0', received) || std::memchr(body.data(), '\\', received))
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid JSON");
    if (!uta::LocationJsonDepthSafe(std::string_view(body.data(), received)))
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid JSON depth");
    JsonPtr root(cJSON_ParseWithLengthOpts(body.data(), received + 1, nullptr, true), cJSON_Delete);
    const auto* code = cJSON_GetObjectItemCaseSensitive(root.get(), "province");
    if (!cJSON_IsObject(root.get()) || cJSON_GetArraySize(root.get()) != 1 ||
        !cJSON_IsString(code) || !code->valuestring || !uta::ValidProvinceCode(code->valuestring))
        return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "Invalid province code");
    if (!uta::LocationService::GetInstance().SetProvince(code->valuestring))
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Failed to save location");
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_sendstr(request, "{\"success\":true}");
}

esp_err_t PortalIndex(httpd_req_t* request) {
    const std::string_view page(wifi_portal_html);
    const auto head = page.find("</head>");
    const auto closing = page.find("</body>");
    if (head == std::string_view::npos || closing == std::string_view::npos || head >= closing)
        return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                                   "Unsupported WiFi portal template");
    httpd_resp_set_type(request, "text/html; charset=utf-8");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    httpd_resp_set_hdr(request, "Connection", "close");
    auto error = httpd_resp_send_chunk(request, page.data(), head);
    if (error == ESP_OK)
        error = httpd_resp_send_chunk(request, uta::kPortalStyle, sizeof(uta::kPortalStyle) - 1);
    if (error == ESP_OK)
        error = httpd_resp_send_chunk(request, page.data() + head, closing - head);
    if (error == ESP_OK)
        error =
            httpd_resp_send_chunk(request, uta::kLocationPortal, sizeof(uta::kLocationPortal) - 1);
    if (error == ESP_OK)
        error = httpd_resp_send_chunk(request, page.data() + closing, page.size() - closing);
    if (error == ESP_OK)
        error = httpd_resp_send_chunk(request, nullptr, 0);
    return error;
}

esp_err_t ExtendPortal(httpd_handle_t server) {
    const std::string_view page(wifi_portal_html);
    if (page.find("</head>") == std::string_view::npos ||
        page.find("</body>") == std::string_view::npos ||
        page.find("id=\"advanced-tab\"") == std::string_view::npos)
        return ESP_ERR_NOT_SUPPORTED;
    httpd_uri_t get{};
    get.uri = "/uta/location";
    get.method = HTTP_GET;
    get.handler = LocationConfig;
    auto error = __real_httpd_register_uri_handler(server, &get);
    if (error != ESP_OK)
        return error;
    auto post = get;
    post.method = HTTP_POST;
    post.handler = SaveLocation;
    error = __real_httpd_register_uri_handler(server, &post);
    if (error == ESP_OK) {
        error = httpd_unregister_uri_handler(server, "/", HTTP_GET);
        if (error == ESP_OK) {
            auto index = original_index;
            index.handler = PortalIndex;
            error = __real_httpd_register_uri_handler(server, &index);
            if (error != ESP_OK)
                __real_httpd_register_uri_handler(server, &original_index);
        }
    }
    if (error != ESP_OK) {
        httpd_unregister_uri_handler(server, "/uta/location", HTTP_GET);
        httpd_unregister_uri_handler(server, "/uta/location", HTTP_POST);
    }
    return error;
}
}  // namespace

// esp-wifi-connect 3.3.x chưa có API mở rộng portal. Bộ chuyển tiếp chỉ được
// liên kết cho UTA, nhận diện cùng server có GET / và GET /advanced/config.
// Không sửa managed_components, không thay handler Wi-Fi/OTA hoặc server :8080.
extern "C" esp_err_t __wrap_httpd_register_uri_handler(httpd_handle_t server,
                                                       const httpd_uri_t* route) {
    std::lock_guard<std::mutex> lock(registration_mutex);
    const auto result = __real_httpd_register_uri_handler(server, route);
    if (result != ESP_OK || !route || !route->uri)
        return result;
    if (route->method == HTTP_GET && std::strcmp(route->uri, "/") == 0 &&
        route->user_ctx == nullptr) {
        candidate_server = server;
        original_index = *route;
    }
    if (server == candidate_server && route->method == HTTP_GET &&
        std::strcmp(route->uri, "/advanced/config") == 0) {
        const auto error = ExtendPortal(server);
        if (error != ESP_OK)
            ESP_LOGE("UTA_Portal", "Không thêm được mục vị trí: %s", esp_err_to_name(error));
        // Lỗi phần mở rộng không được làm mất trang cấu hình Wi-Fi gốc.
        return result;
    }
    return result;
}
