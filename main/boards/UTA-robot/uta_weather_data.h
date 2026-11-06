#pragma once

#include <cmath>
#include <cstdint>
#include <string_view>

#include <cJSON.h>

namespace uta {

enum class WeatherIcon : uint8_t {
    Unknown,
    Sun,
    Moon,
    PartlyCloudy,
    Cloud,
    Fog,
    Rain,
    Snow,
    Storm
};

struct WeatherCondition {
    std::string_view description;
    WeatherIcon icon;
};

constexpr WeatherCondition DescribeWeather(int code, bool day) {
    switch (code) {
        case 0:
            return {day ? "Trời quang" : "Đêm quang", day ? WeatherIcon::Sun : WeatherIcon::Moon};
        case 1:
        case 2:
            return {"Ít mây", WeatherIcon::PartlyCloudy};
        case 3:
            return {"Nhiều mây", WeatherIcon::Cloud};
        case 45:
        case 48:
            return {"Sương mù", WeatherIcon::Fog};
        case 51:
        case 53:
        case 55:
        case 56:
        case 57:
            return {"Mưa phùn", WeatherIcon::Rain};
        case 61:
        case 63:
        case 65:
        case 66:
        case 67:
            return {"Có mưa", WeatherIcon::Rain};
        case 80:
        case 81:
        case 82:
            return {"Mưa rào", WeatherIcon::Rain};
        case 71:
        case 73:
        case 75:
        case 77:
        case 85:
        case 86:
            return {"Có tuyết", WeatherIcon::Snow};
        case 95:
        case 96:
        case 99:
            return {"Dông", WeatherIcon::Storm};
        default:
            return {"Chưa cập nhật", WeatherIcon::Unknown};
    }
}

struct WeatherReading {
    double temperature = 0;
    int code = -1;
    bool day = true;
    int64_t observed_at = 0;
};

// Chỉ nhận dữ liệu hiện tại, đúng đơn vị và trường bắt buộc; không dùng giá trị 0 giả.
inline bool ParseWeather(const cJSON* root, WeatherReading& output) {
    if (!cJSON_IsObject(root) || cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "error")))
        return false;
    const auto* current = cJSON_GetObjectItemCaseSensitive(root, "current");
    const auto* units = cJSON_GetObjectItemCaseSensitive(root, "current_units");
    const auto* unit = cJSON_GetObjectItemCaseSensitive(units, "temperature_2m");
    const auto* time_unit = cJSON_GetObjectItemCaseSensitive(units, "time");
    const auto* temperature = cJSON_GetObjectItemCaseSensitive(current, "temperature_2m");
    const auto* code = cJSON_GetObjectItemCaseSensitive(current, "weather_code");
    const auto* day = cJSON_GetObjectItemCaseSensitive(current, "is_day");
    const auto* time = cJSON_GetObjectItemCaseSensitive(current, "time");
    if (!cJSON_IsString(unit) || !unit->valuestring ||
        std::string_view(unit->valuestring) != "°C" || !cJSON_IsString(time_unit) ||
        !time_unit->valuestring || std::string_view(time_unit->valuestring) != "unixtime" ||
        !cJSON_IsNumber(temperature) || !std::isfinite(temperature->valuedouble) ||
        temperature->valuedouble < -90 || temperature->valuedouble > 65 || !cJSON_IsNumber(code) ||
        !std::isfinite(code->valuedouble) || code->valuedouble != std::floor(code->valuedouble) ||
        code->valuedouble < 0 || code->valuedouble > 99 ||
        DescribeWeather(code->valueint, true).icon == WeatherIcon::Unknown ||
        !cJSON_IsNumber(day) || (day->valuedouble != 0 && day->valuedouble != 1) ||
        !cJSON_IsNumber(time) || !std::isfinite(time->valuedouble) ||
        time->valuedouble < 1735689600 || time->valuedouble > 4102444800 ||
        time->valuedouble != std::floor(time->valuedouble))
        return false;
    output = {temperature->valuedouble, code->valueint, day->valueint == 1,
              static_cast<int64_t>(time->valuedouble)};
    return true;
}

}  // namespace uta
