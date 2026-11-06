#include "main/boards/UTA-robot/uta_weather_data.h"

#include <cassert>
#include <limits>
#include <memory>

namespace {
using JsonPtr = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
constexpr const char* kValid =
    R"({"current_units":{"temperature_2m":"°C","time":"unixtime"},"current":{"temperature_2m":28.5,"weather_code":2,"is_day":1,"time":1790000000}})";
}  // namespace

int main() {
    uta::WeatherReading reading;
    JsonPtr root(cJSON_Parse(kValid), cJSON_Delete);
    assert(uta::ParseWeather(root.get(), reading));
    assert(reading.temperature == 28.5 && reading.code == 2 && reading.day);
    assert(uta::DescribeWeather(0, false).icon == uta::WeatherIcon::Moon);
    assert(uta::DescribeWeather(65, true).icon == uta::WeatherIcon::Rain);
    assert(uta::DescribeWeather(99, true).icon == uta::WeatherIcon::Storm);
    assert(uta::DescribeWeather(4, true).icon == uta::WeatherIcon::Unknown);
    assert(!uta::ParseWeather(nullptr, reading));

    auto* current = cJSON_GetObjectItemCaseSensitive(root.get(), "current");
    for (const char* field : {"temperature_2m", "weather_code", "is_day", "time"}) {
        JsonPtr copy(cJSON_Duplicate(root.get(), true), cJSON_Delete);
        auto* object = cJSON_GetObjectItemCaseSensitive(copy.get(), "current");
        cJSON_ReplaceItemInObjectCaseSensitive(object, field, cJSON_CreateNull());
        assert(!uta::ParseWeather(copy.get(), reading));
    }
    cJSON_SetNumberValue(cJSON_GetObjectItem(current, "temperature_2m"), -5.5);
    assert(uta::ParseWeather(root.get(), reading));
    cJSON_SetNumberValue(cJSON_GetObjectItem(current, "temperature_2m"), 100);
    assert(!uta::ParseWeather(root.get(), reading));
    cJSON_SetNumberValue(cJSON_GetObjectItem(current, "temperature_2m"), 20);
    for (double invalid : {4., 100., 2.5}) {
        cJSON_SetNumberValue(cJSON_GetObjectItem(current, "weather_code"), invalid);
        assert(!uta::ParseWeather(root.get(), reading));
    }
    cJSON_SetNumberValue(cJSON_GetObjectItem(current, "weather_code"), 0);
    cJSON_SetNumberValue(cJSON_GetObjectItem(current, "is_day"), 2);
    assert(!uta::ParseWeather(root.get(), reading));
    cJSON_SetNumberValue(cJSON_GetObjectItem(current, "is_day"), 0);
    assert(uta::ParseWeather(root.get(), reading) && !reading.day);
    cJSON_SetNumberValue(cJSON_GetObjectItem(current, "time"), 0);
    assert(!uta::ParseWeather(root.get(), reading));
    cJSON_SetNumberValue(cJSON_GetObjectItem(current, "time"), 1790000000);
    auto* units = cJSON_GetObjectItem(root.get(), "current_units");
    cJSON_ReplaceItemInObjectCaseSensitive(units, "temperature_2m", cJSON_CreateString("°F"));
    assert(!uta::ParseWeather(root.get(), reading));
}
