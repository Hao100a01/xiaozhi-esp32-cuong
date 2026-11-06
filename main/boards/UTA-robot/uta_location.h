#pragma once

#include <mutex>
#include <string>

#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace uta {

struct Location {
    std::string name;
    double latitude = 0;
    double longitude = 0;
    bool available = false;
    bool from_ip = false;
};

// Dữ liệu dùng chung cho trang cấu hình và màn hình chờ; chỉ tác vụ riêng gọi mạng.
class LocationService {
public:
    static LocationService& GetInstance();
    bool Start();
    bool SetProvince(const std::string& code);
    std::string ProvinceCode();
    Location Get();
    static bool ParseIpResponse(const cJSON* json, Location& result);

private:
    LocationService();
    void Run();
    bool LookupIp(Location& result);
    std::mutex mutex_;
    std::string province_code_;
    Location location_;
    TaskHandle_t task_ = nullptr;
    unsigned revision_ = 0;
};

}  // namespace uta
