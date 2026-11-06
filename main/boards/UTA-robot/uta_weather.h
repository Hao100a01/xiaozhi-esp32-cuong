#pragma once

#include <mutex>

#include "uta_location.h"
#include "uta_weather_data.h"

namespace uta {

struct WeatherSnapshot {
    Location location;
    WeatherReading reading;
    bool available = false;
    bool stale = true;
};

class WeatherService {
public:
    static WeatherService& GetInstance();
    bool Start();
    WeatherSnapshot Get();

private:
    void Run();
    bool Fetch(const Location& location, WeatherReading& reading);
    std::mutex mutex_;
    WeatherSnapshot snapshot_;
    int64_t fetched_at_ = 0;
    TaskHandle_t task_ = nullptr;
};

}  // namespace uta
