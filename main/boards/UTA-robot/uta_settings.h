#pragma once

#include <mutex>
#include <string>

#include "uta_helpers.h"

namespace uta {

PinSettings DefaultPins();

struct Configuration {
    PinSettings gpio;
    bool touch_enabled = false;
    bool robot_enabled = false;
    // 0: tắt, 1: L298N, 2: DRV8833; UART cũ không chạy đồng thời.
    int motor_driver = 0;
    int motor_speed = 60;
    bool reverse_left = false;
    bool reverse_right = false;
    bool servos_enabled = false;
    std::array<int, 2> servo_min{15, 15};
    std::array<int, 2> servo_max{165, 165};
    std::array<int, 2> servo_home{90, 90};
    std::string music_server;
    std::string music_key = "your-esp32-secret-key-2024";
    std::string radio_url = kStations[0].url;
    std::string radio_name = kStations[0].name;
    std::string web_password;
};

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
