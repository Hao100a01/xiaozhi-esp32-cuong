#pragma once

#include <algorithm>
#include <mutex>
#include <string>

#include "uta_helpers.h"

namespace uta {

PinSettings DefaultPins();

enum class LcdDriver { Off, St7789, Ili9341, Gc9a01, St7735, Ssd1306, Sh1106 };
enum LcdPin { kLcdDataPin, kLcdClockPin, kLcdSelectPin, kLcdCommandPin, kLcdResetPin, kLcdLightPin };

struct LcdSettings {
    LcdDriver driver = LcdDriver::Off;
    int width = 240;
    int height = 240;
    std::array<int, 6> pins{-1, -1, -1, -1, -1, -1};
    bool operator==(const LcdSettings&) const = default;
};

inline constexpr size_t kExtraServoCount = 4;
inline constexpr size_t kWheelServoCount = 2;

enum RangePin { kTofSda, kTofScl, kTofShutdown, kUltrasonicTrig, kUltrasonicEcho, kRangePinCount };

enum CobPin { kCobRed, kCobGreen, kCobBlue, kCobEdison, kCobStrip, kCobPinCount };
enum CobLight { kRgbLight, kEdisonLight, kStripLight, kCobLightCount };

struct Configuration {
    PinSettings gpio;
    LcdSettings primary_lcd{LcdDriver::St7789};
    LcdSettings secondary_lcd;
    bool touch_enabled = false;
    bool robot_enabled = false;
    // 0: tắt, 1: L298N mini, 2: DRV8833; UART cũ không chạy đồng thời.
    int motor_driver = 0;
    int motor_speed = 60;
    bool reverse_left = false;
    bool reverse_right = false;
    std::array<bool, 2> servos_enabled{};
    // Left/right continuous wheels, followed by left/right positional arms.
    std::array<int, kExtraServoCount> extra_servo_pins{-1, -1, -1, -1};
    std::array<int, kRangePinCount> range_pins{-1, -1, -1, -1, -1};
    std::array<int, kCobPinCount> cob_pins{-1, -1, -1, -1, -1};
    bool rgb_common_anode = false;
    std::array<int, 2> servo_min{15, 15};
    std::array<int, 2> servo_max{165, 165};
    std::array<int, 2> servo_home{90, 90};
    std::string music_server;
    std::string music_key = "your-esp32-secret-key-2024";
    std::string radio_url = kStations[0].url;
    std::string radio_name = kStations[0].name;
};

inline bool IsOled(LcdDriver driver) {
    return driver == LcdDriver::Ssd1306 || driver == LcdDriver::Sh1106;
}

inline bool ValidLcdSize(const LcdSettings& lcd) {
    switch (lcd.driver) {
    case LcdDriver::St7789:
        return lcd.width == 240 && (lcd.height == 240 || lcd.height == 320);
    case LcdDriver::Ili9341:
        return lcd.width == 240 && lcd.height == 320;
    case LcdDriver::Gc9a01:
        return lcd.width == 240 && lcd.height == 240;
    case LcdDriver::St7735:
        return lcd.width == 128 && lcd.height == 160;
    case LcdDriver::Ssd1306:
        return lcd.width == 128 && (lcd.height == 32 || lcd.height == 64);
    case LcdDriver::Sh1106:
        return lcd.width == 128 && lcd.height == 64;
    default:
        return false;
    }
}

inline auto ActiveGpios(const Configuration& config) {
    const bool primary_oled = IsOled(config.primary_lcd.driver);
    auto occupied = config.gpio.pins;
    if (config.primary_lcd.driver == LcdDriver::Off)
        std::fill(occupied.begin(), occupied.begin() + kTouchMiso, -1);
    else if (primary_oled)
        std::fill(occupied.begin() + kLcdDc, occupied.begin() + kTouchMiso, -1);
    if (!config.touch_enabled)
        std::fill(occupied.begin() + kTouchMiso, occupied.begin() + kChassisTx, -1);
    if (!config.robot_enabled)
        std::fill(occupied.begin() + kChassisTx, occupied.begin() + kMicWs, -1);
    if (!config.motor_driver)
        std::fill(occupied.begin() + kMotorL1, occupied.begin() + kServoHead, -1);
    for (size_t i = 0; i < config.servos_enabled.size(); ++i)
        if (!config.servos_enabled[i])
            occupied[kServoHead + i] = -1;
    return occupied;
}

inline std::string ValidateDisplays(const Configuration& config) {
    const bool primary_off = config.primary_lcd.driver == LcdDriver::Off;
    if (!primary_off && !ValidLcdSize(config.primary_lcd))
        return "Driver hoặc kích thước màn hình chính không được hỗ trợ";
    const bool primary_oled = IsOled(config.primary_lcd.driver);
    if ((primary_oled || primary_off) && config.touch_enabled)
        return "Cảm ứng XPT2046 cần màn hình chính SPI";
    const auto& lcd = config.secondary_lcd;
    if (lcd.driver == LcdDriver::Off)
        return {};
    if (!ValidLcdSize(lcd))
        return "Driver hoặc kích thước màn hình phụ không được hỗ trợ";
    const bool oled = IsOled(lcd.driver);
    const bool shared = !oled && !primary_oled && !primary_off && lcd.pins[kLcdDataPin] == -1 &&
                        lcd.pins[kLcdClockPin] == -1;
    const auto occupied = ActiveGpios(config);
    for (size_t i = 0; i < (oled ? 2 : lcd.pins.size()); ++i) {
        const int pin = lcd.pins[i];
        if (pin == -1 && ((shared && i < kLcdSelectPin) || i >= kLcdResetPin))
            continue;
        if (!AllowedGpio(pin) || std::find(occupied.begin(), occupied.end(), pin) != occupied.end() ||
            std::find(lcd.pins.begin(), lcd.pins.begin() + i, pin) != lcd.pins.begin() + i)
            return "Chân màn hình phụ không hợp lệ hoặc trùng GPIO đã gán";
    }
    return {};
}

inline std::string ValidateExtraServos(const Configuration& config) {
    const auto& servos = config.extra_servo_pins;
    if ((servos[0] == -1) != (servos[1] == -1))
        return "Chọn đủ hai chân PWM cho servo bánh";
    const auto occupied = ActiveGpios(config);
    const auto& lcd = config.secondary_lcd;
    const auto lcd_end = lcd.pins.begin() + (lcd.driver == LcdDriver::Off ? 0 :
                                           IsOled(lcd.driver) ? 2 : lcd.pins.size());
    for (auto it = servos.begin(); it != servos.end(); ++it) {
        const int pin = *it;
        if (pin == -1)
            continue;
        if (!AllowedGpio(pin) || std::find(occupied.begin(), occupied.end(), pin) != occupied.end() ||
            std::find(lcd.pins.begin(), lcd_end, pin) != lcd_end ||
            std::find(servos.begin(), it, pin) != it)
            return "GPIO servo bánh hoặc tay không hợp lệ hoặc trùng với thiết bị đang bật";
    }
    return {};
}

inline int TofSharedOled(const Configuration& config) {
    const auto& pins = config.range_pins;
    if (IsOled(config.primary_lcd.driver) && pins[kTofSda] == config.gpio.pins[kLcdMosi] &&
        pins[kTofScl] == config.gpio.pins[kLcdClock])
        return 0;
    if (IsOled(config.secondary_lcd.driver) &&
        pins[kTofSda] == config.secondary_lcd.pins[kLcdDataPin] &&
        pins[kTofScl] == config.secondary_lcd.pins[kLcdClockPin])
        return 1;
    return -1;
}

inline std::string ValidateRangeSensors(const Configuration& config) {
    const auto& pins = config.range_pins;
    if ((pins[kTofSda] == -1) != (pins[kTofScl] == -1) ||
        (pins[kTofSda] == -1 && pins[kTofShutdown] != -1) ||
        (pins[kUltrasonicTrig] == -1) != (pins[kUltrasonicEcho] == -1))
        return "Chọn đủ cặp SDA/SCL hoặc TRIG/ECHO cho cảm biến đã bật";
    const int shared = TofSharedOled(config);
    if (pins[kTofSda] >= 0 && shared < 0 && IsOled(config.primary_lcd.driver) &&
        IsOled(config.secondary_lcd.driver))
        return "Hai bus I²C đã dùng cho OLED. Chọn cặp SDA/SCL của một OLED cho ToF.";
    const auto occupied = ActiveGpios(config);
    const auto& secondary = config.secondary_lcd;
    const auto secondary_end = secondary.pins.begin() +
        (secondary.driver == LcdDriver::Off ? 0 : IsOled(secondary.driver) ? 2 : secondary.pins.size());
    for (size_t i = 0; i < pins.size(); ++i) {
        const int pin = pins[i];
        if (pin == -1)
            continue;
        const bool shared_wire = i <= kTofScl;
        if (!AllowedGpio(pin) ||
            (!(shared_wire && shared == 0) &&
             std::find(occupied.begin(), occupied.end(), pin) != occupied.end()) ||
            (!(shared_wire && shared == 1) &&
             std::find(secondary.pins.begin(), secondary_end, pin) != secondary_end) ||
            std::find(config.extra_servo_pins.begin(), config.extra_servo_pins.end(), pin) !=
                config.extra_servo_pins.end() ||
            std::find(pins.begin(), pins.begin() + i, pin) != pins.begin() + i)
            return "GPIO cảm biến không hợp lệ hoặc trùng với thiết bị đang bật";
    }
    return {};
}

inline std::string ValidateCobLights(const Configuration& config) {
    const auto& pins = config.cob_pins;
    if ((pins[kCobRed] == -1) != (pins[kCobGreen] == -1) ||
        (pins[kCobRed] == -1) != (pins[kCobBlue] == -1))
        return "Chọn đủ ba chân R, G, B cho đèn RGB";
    const auto occupied = ActiveGpios(config);
    const auto& lcd = config.secondary_lcd;
    const auto lcd_end = lcd.pins.begin() + (lcd.driver == LcdDriver::Off ? 0 :
                                           IsOled(lcd.driver) ? 2 : lcd.pins.size());
    for (auto it = pins.begin(); it != pins.end(); ++it) {
        const int pin = *it;
        if (pin == -1)
            continue;
        if (!AllowedGpio(pin) || std::find(occupied.begin(), occupied.end(), pin) != occupied.end() ||
            std::find(lcd.pins.begin(), lcd_end, pin) != lcd_end ||
            std::find(config.extra_servo_pins.begin(), config.extra_servo_pins.end(), pin) !=
                config.extra_servo_pins.end() ||
            std::find(config.range_pins.begin(), config.range_pins.end(), pin) != config.range_pins.end() ||
            std::find(pins.begin(), it, pin) != it)
            return "GPIO đèn COB không hợp lệ hoặc trùng với thiết bị đang bật";
    }
    return {};
}

inline bool UsesGpio(const Configuration& config, int pin) {
    const auto has_pin = [pin](const auto& values) {
        return std::find(values.begin(), values.end(), pin) != values.end();
    };
    const auto& lcd = config.secondary_lcd;
    const auto lcd_end = lcd.pins.begin() + (lcd.driver == LcdDriver::Off ? 0 :
                                           IsOled(lcd.driver) ? 2 : lcd.pins.size());
    return has_pin(ActiveGpios(config)) || has_pin(config.extra_servo_pins) ||
           has_pin(config.range_pins) || has_pin(config.cob_pins) ||
           std::find(lcd.pins.begin(), lcd_end, pin) != lcd_end;
}

inline std::string ValidateMotion(const Configuration& config) {
    if (config.motor_driver < 0 || config.motor_driver > 2 || config.motor_speed < 1 ||
        config.motor_speed > 100)
        return "Driver hoặc tốc độ động cơ không hợp lệ";
    if (config.motor_driver && config.robot_enabled)
        return "Chỉ chọn điều khiển động cơ trực tiếp hoặc UART";
    for (size_t i = 0; i < 2; ++i) {
        if (config.servo_min[i] < 0 || config.servo_max[i] > 180 ||
            config.servo_min[i] >= config.servo_max[i] ||
            config.servo_home[i] < config.servo_min[i] ||
            config.servo_home[i] > config.servo_max[i])
            return "Góc servo phải nằm trong giới hạn 0–180 độ, góc giữa nằm trong khoảng nhỏ "
                   "nhất/lớn nhất";
    }
    return {};
}

class SettingsStore {
public:
    SettingsStore();
    Configuration Get();
    bool Save(const Configuration& value);

private:
    std::mutex mutex_;
    Configuration config_;
};

}  // namespace uta
