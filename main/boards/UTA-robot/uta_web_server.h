#pragma once

#include <functional>

#include <esp_http_server.h>
#include <cJSON.h>

#include "uta_media_player.h"
#include "uta_settings.h"

namespace uta {

struct WebActions {
    std::function<bool(const std::string&, int)> robot;
    std::function<bool(int, int)> servo;
    std::function<bool(int)> cob_available;
    std::function<bool(int, int)> cob;
    std::function<bool(const std::string&)> cob_effect;
    std::function<int()> tof_mm;
    std::function<int()> ultrasonic_mm;
    std::function<bool(int, int)> extra_servo;
    std::function<void(int)> volume;
    std::function<void(int)> brightness;
    std::function<int()> rotation;
    std::function<bool(int)> set_rotation;
    std::function<void()> restart;
    std::function<void()> wifi;
};

class WebServer {
public:
    WebServer(SettingsStore& settings, MediaPlayer& media, const Configuration& active,
              WebActions actions);
    bool Start();

private:
    SettingsStore& settings_;
    MediaPlayer& media_;
    Configuration active_;
    WebActions actions_;
    httpd_handle_t server_ = nullptr;

    static esp_err_t Dispatch(httpd_req_t* request);
    esp_err_t Handle(httpd_req_t* request);
    esp_err_t Status(httpd_req_t* request);
    esp_err_t Save(httpd_req_t* request, const cJSON* body);
    esp_err_t Command(httpd_req_t* request, const cJSON* body);
};

}  // namespace uta
