#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <string>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "uta_settings.h"
#include "uta_spectrum.h"

namespace uta {

class MediaPlayer {
public:
    explicit MediaPlayer(SettingsStore& settings);
    bool Start();
    bool PlaySong(const std::string& song, const std::string& artist);
    bool PlayRadio(const std::string& url, const std::string& name);
    bool PlayVideo(const std::string& url);
    void SetVideoSink(std::function<void(const uint16_t*, int, int)> sink) {
        video_sink_ = std::move(sink);
    }
    bool IsVideo() const { return video_playing_.load(); }
    void Stop();
    void Pause(bool paused);
    bool IsPlaying() const { return playing_.load(); }
    bool IsPaused() const { return paused_.load(); }
    std::string Status();
    Spectrum GetSpectrum();

private:
    struct Request {
        uint32_t id;
        bool song;
        std::string value;
        std::string label;
        bool video = false;
    };
    SettingsStore& settings_;
    std::mutex mutex_;
    std::optional<Request> pending_;
    std::string status_ = "Đã dừng";
    std::atomic<uint32_t> request_id_{0};
    std::atomic<bool> playing_{false};
    std::atomic<bool> paused_{false};
    std::atomic<bool> preparing_{false};
    TaskHandle_t task_ = nullptr;
    Spectrum spectrum_{};
    int64_t spectrum_at_ = 0;
    std::atomic<bool> video_playing_{false};
    std::function<void(const uint16_t*, int, int)> video_sink_;

    bool Submit(bool song, const std::string& value, const std::string& label, bool video = false);
    bool Cancelled(uint32_t id);
    void SetStatus(uint32_t id, const std::string& text);
    void Run();
    std::string SearchSong(const Request& request, const Configuration& config);
    bool Stream(const std::string& url, uint32_t id, bool radio);
};

}  // namespace uta
