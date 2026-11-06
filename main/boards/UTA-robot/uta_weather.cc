#include "uta_weather.h"

#include <array>
#include <cstdio>
#include <ctime>
#include <memory>

#include <esp_timer.h>
#include <wifi_manager.h>

#include "application.h"
#include "board.h"
#include "http.h"
#include "uta_location_json.h"

namespace uta {
namespace {
bool SameLocation(const Location& a, const Location& b) {
    return a.available && b.available && a.name == b.name && a.latitude == b.latitude &&
           a.longitude == b.longitude && a.from_ip == b.from_ip;
}
}  // namespace

WeatherService& WeatherService::GetInstance() {
    static WeatherService instance;
    return instance;
}

bool WeatherService::Start() {
    if (task_)
        return true;
    return xTaskCreate([](void* self) { static_cast<WeatherService*>(self)->Run(); },
                       "uta_thoi_tiet", 6144, this, 1, &task_) == pdPASS;
}

WeatherSnapshot WeatherService::Get() {
    const auto location = LocationService::GetInstance().Get();
    std::lock_guard<std::mutex> lock(mutex_);
    auto result = snapshot_;
    if (!SameLocation(location, result.location))
        return {location, {}, false, true};
    result.stale = !WifiManager::GetInstance().IsConnected() ||
                   esp_timer_get_time() - fetched_at_ > 3600LL * 1000000;
    return result;
}

bool WeatherService::Fetch(const Location& location, WeatherReading& reading) {
    auto* network = Board::GetInstance().GetNetwork();
    if (!network)
        return false;
    auto http = network->CreateHttp(0);
    if (!http)
        return false;
    std::array<char, 300> url{};
    std::snprintf(url.data(), url.size(),
                  "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
                  "&current=temperature_2m,weather_code,is_day&temperature_unit=celsius"
                  "&timeformat=unixtime&timezone=UTC&forecast_days=1",
                  location.latitude, location.longitude);
    http->SetTimeout(4000);
    http->SetHeader("Accept-Encoding", "identity");
    if (!http->Open("GET", url.data()))
        return false;
    const auto status = http->GetStatusCode();
    if (!status || *status != 200) {
        http->Close();
        return false;
    }
    std::string body;
    std::array<char, 512> buffer{};
    bool complete = false;
    const auto deadline = esp_timer_get_time() + 8000000;
    while (esp_timer_get_time() < deadline) {
        const auto count = http->Read(buffer.data(), buffer.size());
        if (!count || *count < 0)
            break;
        if (*count == 0) {
            complete = true;
            break;
        }
        if (body.size() + *count > 4096)
            break;
        body.append(buffer.data(), *count);
    }
    http->Close();
    if (!complete || body.find('\0') != std::string::npos || !LocationJsonDepthSafe(body))
        return false;
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> json(
        cJSON_ParseWithLengthOpts(body.c_str(), body.size() + 1, nullptr, true), cJSON_Delete);
    return ParseWeather(json.get(), reading);
}

void WeatherService::Run() {
    Location requested;
    int64_t next_fetch = 0;
    for (;;) {
        const auto location = LocationService::GetInstance().Get();
        if (!SameLocation(location, requested)) {
            requested = location;
            next_fetch = 0;
        }
        // Không gọi HTTP từ UI, event loop hoặc tác vụ âm thanh; hoãn khi hội thoại.
        if (location.available && WifiManager::GetInstance().IsConnected() &&
            Application::GetInstance().GetDeviceState() == kDeviceStateIdle &&
            esp_timer_get_time() >= next_fetch) {
            WeatherReading reading;
            bool success = Fetch(location, reading);
            // Giờ hệ thống của dự án có thể đã cộng offset múi giờ từ OTA. Cho phép
            // sai khác <= 15 giờ, nhưng loại dữ liệu thời tiết nhiều ngày trước.
            const auto now = static_cast<int64_t>(std::time(nullptr));
            if (success && now >= 1735689600 && std::abs(now - reading.observed_at) > 15 * 3600)
                success = false;
            if (success && SameLocation(location, LocationService::GetInstance().Get())) {
                std::lock_guard<std::mutex> lock(mutex_);
                snapshot_ = {location, reading, true, false};
                fetched_at_ = esp_timer_get_time();
            }
            next_fetch = esp_timer_get_time() + (success ? 900LL : 300LL) * 1000000;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

}  // namespace uta
