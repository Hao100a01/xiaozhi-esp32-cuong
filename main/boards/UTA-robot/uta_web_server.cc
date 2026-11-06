#include "uta_web_server.h"

#include <array>
#include <cmath>
#include <cstring>
#include <memory>

#include "config.h"
#include "uta_web_page.h"

namespace uta {
namespace {
using JsonPtr = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;

esp_err_t Reply(httpd_req_t* request, const char* status, const std::string& text) {
    httpd_resp_set_status(request, status);
    httpd_resp_set_type(request, "application/json; charset=utf-8");
    JsonPtr root(cJSON_CreateObject(), cJSON_Delete);
    if (!root) {
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(root.get(), "thong_bao", text.c_str());
    char* encoded = cJSON_PrintUnformatted(root.get());
    if (!encoded) {
        return ESP_ERR_NO_MEM;
    }
    const auto result = httpd_resp_send(request, encoded, HTTPD_RESP_USE_STRLEN);
    cJSON_free(encoded);
    return result;
}

bool Text(const cJSON* body, const char* key, std::string& output, size_t limit) {
    const auto* item = cJSON_GetObjectItemCaseSensitive(body, key);
    if (!cJSON_IsString(item) || !item->valuestring || std::strlen(item->valuestring) > limit) {
        return false;
    }
    output = item->valuestring;
    for (unsigned char byte : output) {
        if (byte < 32 || byte == 127) {
            return false;
        }
    }
    return true;
}

bool Integer(const cJSON* body, const char* key, int& output, int minimum, int maximum) {
    const auto* item = cJSON_GetObjectItemCaseSensitive(body, key);
    if (!cJSON_IsNumber(item) || !std::isfinite(item->valuedouble) || item->valuedouble < minimum ||
        item->valuedouble > maximum || std::floor(item->valuedouble) != item->valuedouble) {
        return false;
    }
    output = item->valueint;
    return true;
}
}  // namespace

WebServer::WebServer(SettingsStore& settings, MediaPlayer& media, const Configuration& active,
                     WebActions actions)
    : settings_(settings), media_(media), active_(active), actions_(std::move(actions)) {}

bool WebServer::Start() {
    if (server_) {
        return true;
    }
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = UTA_HTTP_PORT;
    config.ctrl_port = 32771;
    config.stack_size = 8192;
    config.max_open_sockets = 4;
    config.recv_wait_timeout = 3;
    config.send_wait_timeout = 3;
    config.lru_purge_enable = true;
    if (httpd_start(&server_, &config) != ESP_OK) {
        return false;
    }
    constexpr std::array<const char*, 4> paths{"/", "/api/trang-thai", "/api/cai-dat", "/api/lenh"};
    for (size_t i = 0; i < paths.size(); ++i) {
        httpd_uri_t route{};
        route.uri = paths[i];
        route.method = i < 2 ? HTTP_GET : HTTP_POST;
        route.handler = Dispatch;
        route.user_ctx = this;
        if (httpd_register_uri_handler(server_, &route) != ESP_OK) {
            httpd_stop(server_);
            server_ = nullptr;
            return false;
        }
    }
    httpd_register_err_handler(
        server_, HTTPD_404_NOT_FOUND, [](httpd_req_t* request, httpd_err_code_t) -> esp_err_t {
            return Reply(request, "404 Not Found", "Không tìm thấy trang yêu cầu");
        });
    httpd_register_err_handler(server_, HTTPD_405_METHOD_NOT_ALLOWED,
                               [](httpd_req_t* request, httpd_err_code_t) -> esp_err_t {
                                   return Reply(request, "405 Method Not Allowed",
                                                "Cách gửi yêu cầu không được hỗ trợ");
                               });
    return true;
}

esp_err_t WebServer::Dispatch(httpd_req_t* request) {
    return static_cast<WebServer*>(request->user_ctx)->Handle(request);
}

esp_err_t WebServer::Handle(httpd_req_t* request) {
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    httpd_resp_set_hdr(request, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(request, "Referrer-Policy", "same-origin");
    if (request->method == HTTP_GET) {
        if (std::strcmp(request->uri, "/api/trang-thai") == 0) {
            return Status(request);
        }
        httpd_resp_set_hdr(
            request, "Content-Security-Policy",
            "default-src 'self'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; "
            "connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'");
        httpd_resp_set_type(request, "text/html; charset=utf-8");
        return httpd_resp_send(request, kWebPage, sizeof(kWebPage) - 1);
    }
    std::array<char, 64> content_type{};
    std::array<char, 8> marker{};
    if (httpd_req_get_hdr_value_str(request, "Content-Type", content_type.data(),
                                    content_type.size()) != ESP_OK ||
        std::string_view(content_type.data()).substr(0, 16) != "application/json" ||
        httpd_req_get_hdr_value_str(request, "X-UTA-Yeu-Cau", marker.data(), marker.size()) !=
            ESP_OK ||
        std::strcmp(marker.data(), "1") != 0) {
        return Reply(request, "400 Bad Request",
                     "Yêu cầu phải được gửi từ trang điều khiển UTA-robot");
    }
    if (request->content_len <= 0 || request->content_len > 4096) {
        return Reply(request, "413 Content Too Large", "Nội dung yêu cầu phải từ 1 đến 4096 byte");
    }
    std::string body(request->content_len, '\0');
    size_t received = 0;
    while (received < body.size()) {
        int count = httpd_req_recv(request, body.data() + received, body.size() - received);
        if (count <= 0) {
            return Reply(request, "408 Request Timeout", "Không nhận đủ nội dung yêu cầu");
        }
        received += count;
    }
    if (body.find('\0') != std::string::npos) {
        return Reply(request, "400 Bad Request", "Nội dung chứa ký tự không hợp lệ");
    }
    const char* end = nullptr;
    JsonPtr json(cJSON_ParseWithLengthOpts(body.c_str(), body.size() + 1, &end, true),
                 cJSON_Delete);
    if (!cJSON_IsObject(json.get())) {
        return Reply(request, "400 Bad Request", "Không đọc được nội dung yêu cầu");
    }
    if (std::strcmp(request->uri, "/api/cai-dat") == 0) {
        return Save(request, json.get());
    }
    return Command(request, json.get());
}

esp_err_t WebServer::Status(httpd_req_t* request) {
    const auto config = settings_.Get();
    JsonPtr root(cJSON_CreateObject(), cJSON_Delete);
    if (!root) {
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(root.get(), "trang_thai_nhac", media_.Status().c_str());
    cJSON_AddNumberToObject(root.get(), "goc_man_hinh", actions_.rotation());
    cJSON_AddStringToObject(root.get(), "may_chu_nhac", config.music_server.c_str());
    cJSON_AddStringToObject(root.get(), "url_radio", config.radio_url.c_str());
    cJSON_AddStringToObject(root.get(), "ten_radio", config.radio_name.c_str());
    cJSON_AddBoolToObject(root.get(), "cam_ung", config.touch_enabled);
    cJSON_AddBoolToObject(root.get(), "robot_uart", config.robot_enabled);
    cJSON_AddBoolToObject(root.get(), "robot_dang_bat",
                          active_.robot_enabled || active_.motor_driver);
    cJSON_AddBoolToObject(root.get(), "uart_dang_bat", active_.robot_enabled);
    cJSON_AddBoolToObject(root.get(), "servo_dang_bat", active_.servos_enabled[0] || active_.servos_enabled[1]);
    cJSON_AddNumberToObject(root.get(), "driver_dc", config.motor_driver);
    cJSON_AddNumberToObject(root.get(), "toc_do_dc", config.motor_speed);
    cJSON_AddBoolToObject(root.get(), "dao_dc_trai", config.reverse_left);
    cJSON_AddBoolToObject(root.get(), "dao_dc_phai", config.reverse_right);
    cJSON_AddBoolToObject(root.get(), "bat_servo", config.servos_enabled[0] || config.servos_enabled[1]);
    cJSON_AddBoolToObject(root.get(), "bat_servo_dau", config.servos_enabled[0]);
    cJSON_AddBoolToObject(root.get(), "bat_servo_co", config.servos_enabled[1]);
    cJSON_AddBoolToObject(root.get(), "dang_servo_dau", active_.servos_enabled[0]);
    cJSON_AddBoolToObject(root.get(), "dang_servo_co", active_.servos_enabled[1]);
    for (size_t i = 0; i < 2; ++i) {
        const auto suffix = std::to_string(i);
        cJSON_AddNumberToObject(root.get(), ("goc_min_" + suffix).c_str(), config.servo_min[i]);
        cJSON_AddNumberToObject(root.get(), ("goc_max_" + suffix).c_str(), config.servo_max[i]);
        cJSON_AddNumberToObject(root.get(), ("goc_nha_" + suffix).c_str(), config.servo_home[i]);
        cJSON_AddNumberToObject(root.get(), ("dang_min_" + suffix).c_str(), active_.servo_min[i]);
        cJSON_AddNumberToObject(root.get(), ("dang_max_" + suffix).c_str(), active_.servo_max[i]);
        cJSON_AddNumberToObject(root.get(), ("dang_nha_" + suffix).c_str(), active_.servo_home[i]);
    }
    auto* saved_servos = cJSON_CreateIntArray(config.extra_servo_pins.data(), kExtraServoCount);
    auto* active_servos = cJSON_CreateIntArray(active_.extra_servo_pins.data(), kExtraServoCount);
    if (!saved_servos || !active_servos) {
        cJSON_Delete(saved_servos);
        cJSON_Delete(active_servos);
        return ESP_ERR_NO_MEM;
    }
    if (!cJSON_AddItemToObject(root.get(), "servo_mo_rong", saved_servos)) {
        cJSON_Delete(saved_servos);
        cJSON_Delete(active_servos);
        return ESP_ERR_NO_MEM;
    }
    if (!cJSON_AddItemToObject(root.get(), "servo_mo_rong_dang_dung", active_servos)) {
        cJSON_Delete(active_servos);
        return ESP_ERR_NO_MEM;
    }
    auto* cob_pins = cJSON_CreateIntArray(config.cob_pins.data(), kCobPinCount);
    if (!cob_pins)
        return ESP_ERR_NO_MEM;
    if (!cJSON_AddItemToObject(root.get(), "chan_den_cob", cob_pins)) {
        cJSON_Delete(cob_pins);
        return ESP_ERR_NO_MEM;
    }
    auto* cob_active = cJSON_AddArrayToObject(root.get(), "den_cob_dang_dung");
    if (!cob_active)
        return ESP_ERR_NO_MEM;
    for (int light = 0; light < kCobLightCount; ++light) {
        auto* item = cJSON_CreateBool(actions_.cob_available(light));
        if (!item)
            return ESP_ERR_NO_MEM;
        if (!cJSON_AddItemToArray(cob_active, item)) {
            cJSON_Delete(item);
            return ESP_ERR_NO_MEM;
        }
    }
    cJSON_AddBoolToObject(root.get(), "rgb_anode", config.rgb_common_anode);
    auto* range_pins = cJSON_CreateIntArray(config.range_pins.data(), kRangePinCount);
    if (!range_pins)
        return ESP_ERR_NO_MEM;
    if (!cJSON_AddItemToObject(root.get(), "chan_cam_bien", range_pins)) {
        cJSON_Delete(range_pins);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddBoolToObject(root.get(), "tof_dang_dung", active_.range_pins[kTofSda] >= 0);
    cJSON_AddBoolToObject(root.get(), "sieu_am_dang_dung", active_.range_pins[kUltrasonicTrig] >= 0);
    cJSON_AddNumberToObject(root.get(), "tof_mm", actions_.tof_mm());
    cJSON_AddNumberToObject(root.get(), "sieu_am_mm", actions_.ultrasonic_mm());
    const auto add_lcd = [&](const char* key, const LcdSettings& lcd, bool secondary) {
        auto* node = cJSON_AddObjectToObject(root.get(), key);
        if (!node)
            return false;
        if (!cJSON_AddNumberToObject(node, "driver", static_cast<int>(lcd.driver)) ||
            !cJSON_AddNumberToObject(node, "width", lcd.width) ||
            !cJSON_AddNumberToObject(node, "height", lcd.height))
            return false;
        if (secondary) {
            auto* pins = cJSON_CreateIntArray(lcd.pins.data(), lcd.pins.size());
            if (!pins)
                return false;
            if (!cJSON_AddItemToObject(node, "pins", pins)) {
                cJSON_Delete(pins);
                return false;
            }
        }
        return true;
    };
    if (!add_lcd("man_chinh", config.primary_lcd, false) ||
        !add_lcd("man_phu", config.secondary_lcd, true))
        return ESP_ERR_NO_MEM;
    cJSON_AddBoolToObject(
        root.get(), "can_khoi_dong_lai",
        config.cob_pins != active_.cob_pins || config.rgb_common_anode != active_.rgb_common_anode ||
            config.range_pins != active_.range_pins ||
            config.extra_servo_pins != active_.extra_servo_pins ||
            config.primary_lcd != active_.primary_lcd || config.secondary_lcd != active_.secondary_lcd ||
            config.gpio.pins != active_.gpio.pins || config.touch_enabled != active_.touch_enabled ||
            config.robot_enabled != active_.robot_enabled ||
            config.motor_driver != active_.motor_driver ||
            config.motor_speed != active_.motor_speed ||
            config.reverse_left != active_.reverse_left ||
            config.reverse_right != active_.reverse_right ||
            config.servos_enabled != active_.servos_enabled ||
            config.servo_min != active_.servo_min || config.servo_max != active_.servo_max ||
            config.servo_home != active_.servo_home);
    cJSON* stations = cJSON_AddArrayToObject(root.get(), "danh_sach_dai");
    cJSON* pins = cJSON_AddArrayToObject(root.get(), "chan");
    if (!stations || !pins) {
        return ESP_ERR_NO_MEM;
    }
    for (const auto& station : kStations) {
        auto* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "ma", station.id);
        cJSON_AddStringToObject(item, "ten", station.name);
        cJSON_AddItemToArray(stations, item);
    }
    for (size_t i = 0; i < kPinCount; ++i) {
        auto* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "ma", kPinKeys[i]);
        cJSON_AddStringToObject(item, "ten", kPinLabels[i]);
        cJSON_AddNumberToObject(item, "gia_tri", config.gpio.pins[i]);
        cJSON_AddItemToArray(pins, item);
    }
    char* encoded = cJSON_PrintUnformatted(root.get());
    if (!encoded) {
        return ESP_ERR_NO_MEM;
    }
    httpd_resp_set_type(request, "application/json; charset=utf-8");
    const auto result = httpd_resp_send(request, encoded, HTTPD_RESP_USE_STRLEN);
    cJSON_free(encoded);
    return result;
}

esp_err_t WebServer::Save(httpd_req_t* request, const cJSON* body) {
    auto config = settings_.Get();
    std::string key;
    const auto* pins = cJSON_GetObjectItemCaseSensitive(body, "chan");
    const auto* robot = cJSON_GetObjectItemCaseSensitive(body, "robot_uart");
    const auto* touch = cJSON_GetObjectItemCaseSensitive(body, "cam_ung");
    if (!Text(body, "may_chu_nhac", config.music_server, 512) ||
        (!config.music_server.empty() && !IsHttpUrl(config.music_server)) ||
        !Text(body, "url_radio", config.radio_url, 512) || !IsHttpUrl(config.radio_url) ||
        !Text(body, "ten_radio", config.radio_name, 120) || config.radio_name.empty() ||
        !Text(body, "khoa_nhac", key, 128) || !cJSON_IsBool(touch) || !cJSON_IsObject(pins) ||
        !cJSON_IsBool(robot)) {
        return Reply(request, "400 Bad Request",
                     "Cài đặt không hợp lệ; kiểm tra địa chỉ và các lựa chọn");
    }
    for (size_t i = 0; i < kPinCount; ++i) {
        if (!Integer(pins, kPinKeys[i], config.gpio.pins[i], -1, 48)) {
            return Reply(request, "400 Bad Request", "Số chân GPIO phải là số nguyên hợp lệ");
        }
    }
    if (!Integer(body, "driver_dc", config.motor_driver, 0, 2) ||
        !Integer(body, "toc_do_dc", config.motor_speed, 1, 100))
        return Reply(request, "400 Bad Request", "Driver hoặc tốc độ không hợp lệ");
    for (const auto* key : {"dao_dc_trai", "dao_dc_phai", "bat_servo"}) {
        if (!cJSON_IsBool(cJSON_GetObjectItemCaseSensitive(body, key)))
            return Reply(request, "400 Bad Request", "Thiếu lựa chọn động cơ hoặc servo");
    }
    config.reverse_left = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(body, "dao_dc_trai"));
    config.reverse_right = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(body, "dao_dc_phai"));
    const auto* head = cJSON_GetObjectItemCaseSensitive(body, "bat_servo_dau");
    const auto* neck = cJSON_GetObjectItemCaseSensitive(body, "bat_servo_co");
    if ((head || neck) && (!cJSON_IsBool(head) || !cJSON_IsBool(neck)))
        return Reply(request, "400 Bad Request", "Thiếu lựa chọn servo đầu hoặc cổ");
    const bool legacy_servo = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(body, "bat_servo"));
    config.servos_enabled = {head ? cJSON_IsTrue(head) != 0 : legacy_servo,
                             neck ? cJSON_IsTrue(neck) != 0 : legacy_servo};
    config.robot_enabled = cJSON_IsTrue(robot);
    for (size_t i = 0; i < 2; ++i) {
        const auto suffix = std::to_string(i);
        if (!Integer(body, ("goc_min_" + suffix).c_str(), config.servo_min[i], 0, 180) ||
            !Integer(body, ("goc_max_" + suffix).c_str(), config.servo_max[i], 0, 180) ||
            !Integer(body, ("goc_nha_" + suffix).c_str(), config.servo_home[i], 0, 180))
            return Reply(request, "400 Bad Request", "Góc servo không hợp lệ");
    }
    const auto motion_error = ValidateMotion(config);
    if (!motion_error.empty())
        return Reply(request, "400 Bad Request", motion_error);
    const auto error = ValidatePins(config.gpio);
    if (!error.empty()) {
        return Reply(request, "400 Bad Request", error);
    }
    config.touch_enabled = cJSON_IsTrue(touch);
    config.robot_enabled = cJSON_IsTrue(robot);
    const auto read_lcd = [&](const char* key, LcdSettings& lcd, bool secondary) {
        const auto* node = cJSON_GetObjectItemCaseSensitive(body, key);
        if (!node)
            return true;
        int driver = 0;
        if (!cJSON_IsObject(node) || !Integer(node, "driver", driver, 0, 6) ||
            !Integer(node, "width", lcd.width, 1, 320) ||
            !Integer(node, "height", lcd.height, 1, 320))
            return false;
        lcd.driver = static_cast<LcdDriver>(driver);
        if (!secondary)
            return true;
        const auto* pins = cJSON_GetObjectItemCaseSensitive(node, "pins");
        if (!cJSON_IsObject(pins))
            return false;
        constexpr const char* keys[] = {"data", "clock", "cs", "dc", "rst", "bl"};
        for (size_t i = 0; i < lcd.pins.size(); ++i)
            if (!Integer(pins, keys[i], lcd.pins[i], -1, 48))
                return false;
        return true;
    };
    if (!read_lcd("man_chinh", config.primary_lcd, false) ||
        !read_lcd("man_phu", config.secondary_lcd, true))
        return Reply(request, "400 Bad Request", "Cấu hình màn hình không hợp lệ");
    if (const auto* extra = cJSON_GetObjectItemCaseSensitive(body, "servo_mo_rong")) {
        constexpr const char* keys[] = {"banh_trai", "banh_phai", "tay_trai", "tay_phai"};
        if (!cJSON_IsObject(extra))
            return Reply(request, "400 Bad Request", "Cấu hình servo mở rộng không hợp lệ");
        for (size_t i = 0; i < config.extra_servo_pins.size(); ++i) {
            if (!Integer(extra, keys[i], config.extra_servo_pins[i], -1, 48))
                return Reply(request, "400 Bad Request", "Chân PWM không hợp lệ");
        }
    }
    if (const auto* range = cJSON_GetObjectItemCaseSensitive(body, "chan_cam_bien")) {
        constexpr const char* keys[] = {"tof_sda", "tof_scl", "tof_xshut", "trig", "echo"};
        if (!cJSON_IsObject(range))
            return Reply(request, "400 Bad Request", "Cấu hình cảm biến không hợp lệ");
        for (size_t i = 0; i < config.range_pins.size(); ++i) {
            if (!Integer(range, keys[i], config.range_pins[i], -1, 48))
                return Reply(request, "400 Bad Request", "Chân cảm biến không hợp lệ");
        }
    }
    if (const auto* cob = cJSON_GetObjectItemCaseSensitive(body, "chan_den_cob")) {
        constexpr const char* keys[] = {"r", "g", "b", "edison", "din"};
        const auto* anode = cJSON_GetObjectItemCaseSensitive(cob, "rgb_anode");
        if (!cJSON_IsObject(cob) || !cJSON_IsBool(anode))
            return Reply(request, "400 Bad Request", "Cấu hình đèn COB không hợp lệ");
        config.rgb_common_anode = cJSON_IsTrue(anode);
        for (size_t i = 0; i < config.cob_pins.size(); ++i)
            if (!Integer(cob, keys[i], config.cob_pins[i], -1, 48))
                return Reply(request, "400 Bad Request", "Chân GPIO đèn COB không hợp lệ");
    }
    const auto cob_error = ValidateCobLights(config);
    if (!cob_error.empty())
        return Reply(request, "400 Bad Request", cob_error);
    const auto range_error = ValidateRangeSensors(config);
    if (!range_error.empty())
        return Reply(request, "400 Bad Request", range_error);
    const auto servo_error = ValidateExtraServos(config);
    if (!servo_error.empty())
        return Reply(request, "400 Bad Request", servo_error);
    const auto display_error = ValidateDisplays(config);
    if (!display_error.empty())
        return Reply(request, "400 Bad Request", display_error);
    if (!key.empty()) {
        config.music_key = key;
    }
    if (!settings_.Save(config)) {
        return Reply(request, "500 Internal Server Error",
                     "Không lưu được cài đặt vào bộ nhớ thiết bị");
    }
    return Reply(request, "200 OK", "Đã lưu. Chân GPIO và phụ kiện áp dụng sau khi khởi động lại");
}

esp_err_t WebServer::Command(httpd_req_t* request, const cJSON* body) {
    std::string command;
    if (!Text(body, "lenh", command, 32)) {
        return Reply(request, "400 Bad Request", "Thiếu tên lệnh hợp lệ");
    }
    bool accepted = true;
    std::string media_error;
    const auto accept_media = [&](MediaResult result) {
        if (!result)
            media_error = std::move(result.error());
        return result.has_value();
    };
    if (command == "nhac") {
        std::string song;
        std::string artist;
        accepted = Text(body, "bai_hat", song, 160) && Text(body, "ca_si", artist, 160) &&
                   accept_media(media_.PlaySong(song, artist));
    } else if (command == "video") {
        std::string url;
        accepted =
            Text(body, "url", url, 2048) && IsHttpUrl(url) && accept_media(media_.PlayVideo(url));
    } else if (command == "radio") {
        std::string station_id;
        accepted = Text(body, "ma_dai", station_id, 40);
        bool found = false;
        if (accepted && station_id == "tuy_chinh") {
            const auto config = settings_.Get();
            accepted = accept_media(media_.PlayRadio(config.radio_url, config.radio_name));
            found = true;
        } else if (accepted) {
            for (const auto& station : kStations) {
                if (station_id == station.id) {
                    accepted = accept_media(media_.PlayRadio(station.url, station.name));
                    found = true;
                    break;
                }
            }
        }
        accepted = accepted && found;
    } else if (command == "dung_nhac") {
        media_.Stop();
    } else if (command == "tam_dung" || command == "tiep_tuc") {
        accepted = accept_media(media_.Pause(command == "tam_dung"));
    } else if (command == "robot") {
        std::string direction;
        int duration = 0;
        accepted = Text(body, "huong", direction, 16) &&
                   Integer(body, "thoi_gian", duration, 1, 2000) &&
                   actions_.robot(direction, duration);
    } else if (command == "servo") {
        int axis = 0, angle = 0;
        accepted = Integer(body, "truc", axis, 0, 1) && Integer(body, "goc", angle, 0, 180) &&
                   actions_.servo(axis, angle);
    } else if (command == "den_cob") {
        int light = 0, value = 0;
        accepted = Integer(body, "loai", light, 0, kCobLightCount - 1) &&
                   Integer(body, "gia_tri", value, 0, light == kEdisonLight ? 255 : 0xffffff) &&
                   actions_.cob(light, value);
    } else if (command == "servo_mo_rong") {
        int axis = 0, value = 0;
        accepted = Integer(body, "truc", axis, 0, kExtraServoCount - 1) &&
                   (axis < kWheelServoCount ? Integer(body, "gia_tri", value, -100, 100)
                                           : Integer(body, "gia_tri", value, 0, 180)) &&
                   actions_.extra_servo(axis, value);
    } else if (command == "xoay_man_hinh") {
        int degrees = 0;
        if (!Integer(body, "gia_tri", degrees, 0, 270) || degrees % 90 != 0) {
            return Reply(request, "400 Bad Request", "Chọn góc 0°, 90°, 180° hoặc 270°");
        }
        if (!actions_.set_rotation(degrees)) {
            return Reply(request, "409 Conflict", "Chưa lưu được góc xoay màn hình");
        }
        return Reply(request, "200 OK", "Đã xoay màn hình và lưu góc");
    } else if (command == "am_luong" || command == "do_sang") {
        int value = 0;
        accepted = Integer(body, "gia_tri", value, command == "do_sang" ? 5 : 0, 100);
        if (accepted) {
            (command == "am_luong" ? actions_.volume : actions_.brightness)(value);
        }
    } else if (command == "khoi_dong_lai") {
        media_.Stop();
        actions_.restart();
    } else if (command == "cau_hinh_wifi") {
        media_.Stop();
        actions_.wifi();

    } else {
        accepted = false;
    }
    return Reply(request, accepted ? "202 Accepted" : "409 Conflict",
                 accepted ? "Đã nhận lệnh"
                 : !media_error.empty()
                     ? media_error
                     : "Chưa thực hiện được; kiểm tra tham số và trạng thái thiết bị");
}

}  // namespace uta
