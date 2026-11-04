#include "uta_settings.h"

#include <esp_random.h>

#include "config.h"
#include "settings.h"

namespace uta {

PinSettings DefaultPins() {
    constexpr PinSettings pins{
        {DISPLAY_MOSI_PIN, DISPLAY_CLK_PIN,  DISPLAY_DC_PIN, DISPLAY_CS_PIN, DISPLAY_BACKLIGHT_PIN,
         DISPLAY_RST_PIN,  DISPLAY_MISO_PIN, TOUCH_CS_PIN,   TOUCH_INT_PIN,  UTA_CHASSIS_TX,
         UTA_CHASSIS_RX,   MIC_WS_PIN,       MIC_SCK_PIN,    MIC_DIN_PIN,    SPK_DOUT_PIN,
         SPK_BCLK_PIN,     SPK_LRCK_PIN,     GPIO_NUM_8,     GPIO_NUM_9,     GPIO_NUM_10,
         GPIO_NUM_14,      GPIO_NUM_17,      GPIO_NUM_18,    GPIO_NUM_21,    GPIO_NUM_43}};
    static_assert(ValidatePins(pins).empty(), "Sơ đồ GPIO mặc định không hợp lệ");
    return pins;
}

SettingsStore::SettingsStore() {
    config_.gpio = DefaultPins();
    Settings stored("uta_robot", true);
    // Chỉ nạp sơ đồ chân thuộc phiên bản phần cứng hiện tại.
    if (stored.GetInt("phien_ban_gpio", 0) == 2) {
        for (size_t i = 0; i < kPinCount; ++i) {
            config_.gpio.pins[i] = stored.GetInt(kPinKeys[i], config_.gpio.pins[i]);
        }
        if (!ValidatePins(config_.gpio).empty()) {
            config_.gpio = DefaultPins();
        }
        config_.touch_enabled = stored.GetBool("cam_ung", false);
        config_.robot_enabled = stored.GetBool("robot_uart", false);
    }
    config_.motor_driver = stored.GetInt("driver_dc", 0);
    config_.motor_speed = stored.GetInt("toc_do_dc", 60);
    config_.reverse_left = stored.GetBool("dao_dc_trai", false);
    config_.reverse_right = stored.GetBool("dao_dc_phai", false);
    config_.servos_enabled = stored.GetBool("bat_servo", false);
    for (size_t i = 0; i < 2; ++i) {
        const auto suffix = std::to_string(i);
        config_.servo_min[i] = stored.GetInt(("goc_min_" + suffix).c_str(), 15);
        config_.servo_max[i] = stored.GetInt(("goc_max_" + suffix).c_str(), 165);
        config_.servo_home[i] = stored.GetInt(("goc_nha_" + suffix).c_str(), 90);
    }
    if (!ValidateMotion(config_).empty()) {
        config_.motor_driver = 0;
        config_.servos_enabled = false;
        config_.motor_speed = 60;
        config_.servo_min = {15, 15};
        config_.servo_max = {165, 165};
        config_.servo_home = {90, 90};
    }
    config_.music_server = stored.GetString("may_chu_nhac");
    config_.music_key = stored.GetString("khoa_nhac", config_.music_key);
    config_.radio_url = stored.GetString("url_radio", config_.radio_url);
    config_.radio_name = stored.GetString("ten_radio", config_.radio_name);
    config_.web_password = stored.GetString("mat_khau_web");
    if (config_.web_password.empty()) {
        constexpr char digits[] = "0123456789abcdef";
        std::array<uint8_t, 6> random{};
        esp_fill_random(random.data(), random.size());
        for (auto byte : random) {
            config_.web_password += digits[byte >> 4];
            config_.web_password += digits[byte & 15];
        }
        stored.SetString("mat_khau_web", config_.web_password);
    }
}

Configuration SettingsStore::Get() {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

bool SettingsStore::Save(const Configuration& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ValidatePins(value.gpio).empty() || !ValidateMotion(value).empty())
        return false;
    nvs_handle_t handle = 0;
    if (nvs_open("uta_robot", NVS_READWRITE, &handle) != ESP_OK)
        return false;
    esp_err_t error = ESP_OK;
    const auto integer = [&](const char* key, int number) {
        if (error == ESP_OK)
            error = nvs_set_i32(handle, key, number);
    };
    const auto text = [&](const char* key, const std::string& contents) {
        if (error == ESP_OK)
            error = nvs_set_str(handle, key, contents.c_str());
    };
    for (size_t i = 0; i < kPinCount; ++i) {
        integer(kPinKeys[i], value.gpio.pins[i]);
    }
    integer("driver_dc", value.motor_driver);
    integer("toc_do_dc", value.motor_speed);
    integer("dao_dc_trai", value.reverse_left);
    integer("dao_dc_phai", value.reverse_right);
    integer("bat_servo", value.servos_enabled);
    for (size_t i = 0; i < 2; ++i) {
        const auto suffix = std::to_string(i);
        integer(("goc_min_" + suffix).c_str(), value.servo_min[i]);
        integer(("goc_max_" + suffix).c_str(), value.servo_max[i]);
        integer(("goc_nha_" + suffix).c_str(), value.servo_home[i]);
    }
    integer("phien_ban_gpio", 2);
    integer("robot_uart", value.robot_enabled);
    integer("cam_ung", value.touch_enabled);
    text("may_chu_nhac", value.music_server);
    text("khoa_nhac", value.music_key);
    text("url_radio", value.radio_url);
    text("ten_radio", value.radio_name);
    text("mat_khau_web", value.web_password);
    if (error == ESP_OK)
        error = nvs_commit(handle);
    nvs_close(handle);
    if (error != ESP_OK)
        return false;
    config_ = value;
    return true;
}

}  // namespace uta
