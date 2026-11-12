#include "uta_settings.h"

#include "config.h"
#include "settings.h"

namespace uta {

PinSettings DefaultPins() {
    constexpr PinSettings pins{
        {DISPLAY_MOSI_PIN, DISPLAY_CLK_PIN,  DISPLAY_DC_PIN, DISPLAY_CS_PIN, DISPLAY_BACKLIGHT_PIN,
         DISPLAY_RST_PIN,  DISPLAY_MISO_PIN, TOUCH_CS_PIN,   TOUCH_INT_PIN,  UTA_CHASSIS_TX,
         UTA_CHASSIS_RX,   MIC_WS_PIN,       MIC_SCK_PIN,    MIC_DIN_PIN,    SPK_DOUT_PIN,
         SPK_BCLK_PIN,     SPK_LRCK_PIN,     GPIO_NUM_8,     GPIO_NUM_9,     GPIO_NUM_10,
         GPIO_NUM_14,      GPIO_NUM_21,      GPIO_NUM_43}};
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
        config_.touch_enabled = stored.GetInt("cam_ung", stored.GetBool("cam_ung", false)) != 0;
        config_.robot_enabled = stored.GetInt("robot_uart", stored.GetBool("robot_uart", false)) != 0;
    }
    config_.primary_lcd.driver = static_cast<LcdDriver>(stored.GetInt("lcd_main_drv", 1));
    config_.primary_lcd.width = stored.GetInt("lcd_main_w", 240);
    config_.primary_lcd.height = stored.GetInt("lcd_main_h", 240);
    config_.secondary_lcd.driver = static_cast<LcdDriver>(stored.GetInt("lcd_sub_drv", 0));
    config_.secondary_lcd.width = stored.GetInt("lcd_sub_w", 240);
    config_.secondary_lcd.height = stored.GetInt("lcd_sub_h", 240);
    for (size_t i = 0; i < config_.secondary_lcd.pins.size(); ++i)
        config_.secondary_lcd.pins[i] = stored.GetInt("lcd_sub_p" + std::to_string(i), -1);
    if (config_.primary_lcd.driver != LcdDriver::Off && !ValidLcdSize(config_.primary_lcd))
        config_.primary_lcd = {LcdDriver::St7789};
    config_.motor_driver = stored.GetInt("driver_dc", 0);
    config_.motor_speed = stored.GetInt("toc_do_dc", 60);
    config_.reverse_left = stored.GetInt("dao_dc_trai", stored.GetBool("dao_dc_trai", false)) != 0;
    config_.reverse_right = stored.GetInt("dao_dc_phai", stored.GetBool("dao_dc_phai", false)) != 0;
    const bool legacy_servo = stored.GetInt("bat_servo", stored.GetBool("bat_servo", false)) != 0;
    config_.servos_enabled = {stored.GetInt("bat_servo_dau", legacy_servo) != 0,
                              stored.GetInt("bat_servo_co", legacy_servo) != 0};
    for (size_t i = 0; i < 2; ++i) {
        const auto suffix = std::to_string(i);
        config_.servo_min[i] = stored.GetInt(("goc_min_" + suffix).c_str(), 15);
        config_.servo_max[i] = stored.GetInt(("goc_max_" + suffix).c_str(), 165);
        config_.servo_home[i] = stored.GetInt(("goc_nha_" + suffix).c_str(), 90);
    }
    if (!ValidateMotion(config_).empty()) {
        config_.motor_driver = 0;
        config_.servos_enabled = {};
        config_.motor_speed = 60;
        config_.servo_min = {15, 15};
        config_.servo_max = {165, 165};
        config_.servo_home = {90, 90};
    }
    if (config_.primary_lcd.driver == LcdDriver::Off || IsOled(config_.primary_lcd.driver))
        config_.touch_enabled = false;
    if (!ValidateDisplays(config_).empty())
        config_.secondary_lcd = {};
    for (size_t i = 0; i < config_.extra_servo_pins.size(); ++i)
        config_.extra_servo_pins[i] = stored.GetInt("servo_ext_" + std::to_string(i), -1);
    if (!ValidateExtraServos(config_).empty())
        config_.extra_servo_pins.fill(-1);
    for (size_t i = 0; i < config_.range_pins.size(); ++i)
        config_.range_pins[i] = stored.GetInt("range_pin_" + std::to_string(i), -1);
    if (!ValidateRangeSensors(config_).empty())
        config_.range_pins.fill(-1);
    for (size_t i = 0; i < config_.cob_pins.size(); ++i)
        config_.cob_pins[i] = stored.GetInt("cob_pin_" + std::to_string(i), -1);
    config_.rgb_common_anode = stored.GetInt("cob_rgb_anode", 0) != 0;
    if (!ValidateCobLights(config_).empty())
        config_.cob_pins.fill(-1);
    config_.music_server = stored.GetString("may_chu_nhac");
    config_.music_key = stored.GetString("khoa_nhac", config_.music_key);
    config_.radio_url = stored.GetString("url_radio", config_.radio_url);
    config_.radio_name = stored.GetString("ten_radio", config_.radio_name);
    // Preserve the existing vector-mode preference when upgrading to Mimo.
    config_.mimo_eyes_enabled = stored.GetInt("eyes_proc", 0) != 0;
    // Khóa mật khẩu web cũ được giữ nguyên trong bộ nhớ để tương thích bản cũ,
    // nhưng không còn được đọc hoặc dùng để xác thực.
}

Configuration SettingsStore::Get() {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

bool SettingsStore::Save(const Configuration& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ValidatePins(value.gpio).empty() || !ValidateMotion(value).empty() ||
        !ValidateDisplays(value).empty() || !ValidateExtraServos(value).empty() ||
        !ValidateRangeSensors(value).empty() || !ValidateCobLights(value).empty())
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
    integer("lcd_main_drv", static_cast<int>(value.primary_lcd.driver));
    integer("lcd_main_w", value.primary_lcd.width);
    integer("lcd_main_h", value.primary_lcd.height);
    integer("lcd_sub_drv", static_cast<int>(value.secondary_lcd.driver));
    integer("lcd_sub_w", value.secondary_lcd.width);
    integer("lcd_sub_h", value.secondary_lcd.height);
    for (size_t i = 0; i < value.secondary_lcd.pins.size(); ++i)
        integer(("lcd_sub_p" + std::to_string(i)).c_str(), value.secondary_lcd.pins[i]);
    integer("driver_dc", value.motor_driver);
    integer("toc_do_dc", value.motor_speed);
    integer("dao_dc_trai", value.reverse_left);
    integer("dao_dc_phai", value.reverse_right);
    integer("bat_servo", value.servos_enabled[0] && value.servos_enabled[1]);
    integer("bat_servo_dau", value.servos_enabled[0]);
    integer("bat_servo_co", value.servos_enabled[1]);
    for (size_t i = 0; i < 2; ++i) {
        const auto suffix = std::to_string(i);
        integer(("goc_min_" + suffix).c_str(), value.servo_min[i]);
        integer(("goc_max_" + suffix).c_str(), value.servo_max[i]);
        integer(("goc_nha_" + suffix).c_str(), value.servo_home[i]);
    }
    for (size_t i = 0; i < value.extra_servo_pins.size(); ++i)
        integer(("servo_ext_" + std::to_string(i)).c_str(), value.extra_servo_pins[i]);
    for (size_t i = 0; i < value.range_pins.size(); ++i)
        integer(("range_pin_" + std::to_string(i)).c_str(), value.range_pins[i]);
    for (size_t i = 0; i < value.cob_pins.size(); ++i)
        integer(("cob_pin_" + std::to_string(i)).c_str(), value.cob_pins[i]);
    integer("cob_rgb_anode", value.rgb_common_anode);
    integer("phien_ban_gpio", 2);
    integer("robot_uart", value.robot_enabled);
    integer("cam_ung", value.touch_enabled);
    integer("eyes_proc", value.mimo_eyes_enabled);
    text("may_chu_nhac", value.music_server);
    text("khoa_nhac", value.music_key);
    text("url_radio", value.radio_url);
    text("ten_radio", value.radio_name);
    if (error == ESP_OK)
        error = nvs_commit(handle);
    nvs_close(handle);
    if (error != ESP_OK)
        return false;
    config_ = value;
    return true;
}

bool SettingsStore::SaveMimoEyesEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    nvs_handle_t handle = 0;
    if (nvs_open("uta_robot", NVS_READWRITE, &handle) != ESP_OK)
        return false;
    const auto result = nvs_set_i32(handle, "eyes_proc", enabled) == ESP_OK &&
                        nvs_commit(handle) == ESP_OK;
    nvs_close(handle);
    if (result)
        config_.mimo_eyes_enabled = enabled;
    return result;
}

}  // namespace uta
