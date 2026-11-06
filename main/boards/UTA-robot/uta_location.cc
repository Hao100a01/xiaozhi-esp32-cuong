#include "uta_location.h"

#include <array>
#include <cmath>
#include <cstring>
#include <memory>

#include <esp_log.h>
#include <esp_timer.h>
#include <nvs.h>
#include <wifi_manager.h>

#include "board.h"
#include "http.h"
#include "settings.h"
#include "uta_location_json.h"
#include "uta_provinces.h"

namespace uta {
namespace {
using JsonPtr = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
constexpr const char* kTag = "UTA_ViTri";

bool PlaceName(const cJSON* value) {
    if (!cJSON_IsString(value) || !value->valuestring)
        return false;
    const size_t length = std::strlen(value->valuestring);
    if (length == 0 || length > 96)
        return false;
    for (size_t i = 0; i < length; ++i) {
        const auto byte = static_cast<unsigned char>(value->valuestring[i]);
        if (byte < 32 || byte == 127)
            return false;
    }
    return true;
}
}  // namespace

LocationService& LocationService::GetInstance() {
    static LocationService instance;
    return instance;
}

LocationService::LocationService() {
    Settings settings("uta_location");
    province_code_ = settings.GetString("province");
    if (!ValidProvinceCode(province_code_))
        province_code_.clear();
}

bool LocationService::Start() {
    if (task_)
        return true;
    return xTaskCreate([](void* context) { static_cast<LocationService*>(context)->Run(); },
                       "uta_vi_tri", 6144, this, 1, &task_) == pdPASS;
}

bool LocationService::SetProvince(const std::string& code) {
    if (!ValidProvinceCode(code))
        return false;
    std::lock_guard<std::mutex> lock(mutex_);
    if (code == province_code_)
        return true;
    nvs_handle_t handle = 0;
    if (nvs_open("uta_location", NVS_READWRITE, &handle) != ESP_OK)
        return false;
    esp_err_t error = nvs_set_str(handle, "province", code.c_str());
    if (error == ESP_OK)
        error = nvs_commit(handle);
    nvs_close(handle);
    if (error != ESP_OK)
        return false;
    province_code_ = code;
    location_ = {};
    ++revision_;
    return true;
}

std::string LocationService::ProvinceCode() {
    std::lock_guard<std::mutex> lock(mutex_);
    return province_code_;
}

Location LocationService::Get() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (const auto* province = FindProvince(province_code_))
        return {std::string(province->name), province->latitude, province->longitude, true, false};
    return location_;
}

bool LocationService::ParseIpResponse(const cJSON* json, Location& result) {
    if (!cJSON_IsObject(json) || !cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "success")))
        return false;
    const auto* latitude = cJSON_GetObjectItemCaseSensitive(json, "latitude");
    const auto* longitude = cJSON_GetObjectItemCaseSensitive(json, "longitude");
    if (!cJSON_IsNumber(latitude) || !cJSON_IsNumber(longitude) ||
        !std::isfinite(latitude->valuedouble) || !std::isfinite(longitude->valuedouble) ||
        std::abs(latitude->valuedouble) > 90 || std::abs(longitude->valuedouble) > 180)
        return false;
    const auto* name = cJSON_GetObjectItemCaseSensitive(json, "city");
    if (!PlaceName(name))
        name = cJSON_GetObjectItemCaseSensitive(json, "region");
    if (!PlaceName(name))
        return false;
    result = {name->valuestring, latitude->valuedouble, longitude->valuedouble, true, true};
    return true;
}

bool LocationService::LookupIp(Location& result) {
    auto* network = Board::GetInstance().GetNetwork();
    if (!network)
        return false;
    auto http = network->CreateHttp(0);
    if (!http)
        return false;
    http->SetTimeout(4000);
    http->SetHeader("Accept-Encoding", "identity");
    // Chỉ gửi IP công cộng qua HTTPS; không gửi SSID, MAC hoặc cấu hình thiết bị.
    if (!http->Open("GET", "https://ipwho.is/?fields=success,city,region,latitude,longitude"))
        return false;
    const auto status = http->GetStatusCode();
    if (!status || *status != 200) {
        http->Close();
        return false;
    }
    std::array<char, 512> buffer{};
    std::string body;
    const int64_t deadline = esp_timer_get_time() + 8000000;
    bool complete = false;
    while (esp_timer_get_time() < deadline) {
        const auto count = http->Read(buffer.data(), buffer.size());
        if (!count || *count < 0)
            break;
        if (*count == 0) {
            complete = true;
            break;
        }
        if (body.size() + *count > 2048)
            break;
        body.append(buffer.data(), *count);
    }
    http->Close();
    if (!complete || body.find('\0') != std::string::npos || !LocationJsonDepthSafe(body))
        return false;
    JsonPtr json(cJSON_ParseWithLengthOpts(body.c_str(), body.size() + 1, nullptr, true),
                 cJSON_Delete);
    return ParseIpResponse(json.get(), result);
}

void LocationService::Run() {
    int64_t retry_at = 0;
    unsigned seen_revision = 0;
    bool was_connected = false;
    for (;;) {
        const bool connected = WifiManager::GetInstance().IsConnected();
        unsigned revision;
        bool automatic;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            revision = revision_;
            automatic = province_code_.empty();
        }
        if (revision != seen_revision || (connected && !was_connected)) {
            retry_at = 0;
            seen_revision = revision;
        }
        was_connected = connected;
        if (automatic && connected && esp_timer_get_time() >= retry_at) {
            Location found;
            const bool success = LookupIp(found);
            // Không cho kết quả IP ghi đè lựa chọn vừa lưu trong lúc đang gọi mạng.
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (province_code_.empty() && revision == revision_ && success)
                    location_ = std::move(found);
            }
            retry_at = esp_timer_get_time() + (success ? 6LL * 3600 : 300) * 1000000;
            if (!success)
                ESP_LOGW(kTag, "Chưa định vị được qua IP; thử lại sau 5 phút");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

}  // namespace uta
