#pragma once

#include <atomic>
#include <expected>
#include <functional>
#include <mutex>
#include <optional>
#include <string>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "uta_settings.h"
#include "uta_spectrum.h"

namespace uta {

using MediaResult = std::expected<void, std::string>;

struct MediaSnapshot {
    std::string type;
    std::string title;
    std::string status;
    bool playing = false;
    bool preparing = false;
    bool paused = false;
    bool replay_available = false;
    std::string artist;
    int elapsed_seconds = 0;
    int duration_seconds = 0;
};

class MediaPlayer {
public:
    explicit MediaPlayer(SettingsStore& settings);
    bool Start();
    MediaResult PlaySong(const std::string& song, const std::string& artist);
    MediaResult PlayRadio(const std::string& url, const std::string& name);
    MediaResult PlayVideo(const std::string& url);
    MediaResult Replay();
    void SetVideoSink(std::function<void(const uint16_t*, int, int)> sink) {
        video_sink_ = std::move(sink);
    }
    bool IsVideo() const { return video_playing_.load(); }
    void Stop();
    MediaResult Pause(bool paused);
    bool IsPlaying() const { return playing_.load(); }
    bool IsPaused() const { return paused_.load(); }
    std::string Status();
    MediaSnapshot Snapshot();
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
    std::optional<Request> last_request_;
    std::string status_ = "Đã dừng";
    std::atomic<uint32_t> request_id_{0};
    std::atomic<bool> playing_{false};
    std::atomic<bool> paused_{false};
    uint32_t preparing_id_ = 0;
    TaskHandle_t task_ = nullptr;
    Spectrum spectrum_{};
    int64_t spectrum_at_ = 0;
    int64_t queued_pcm_us_ = 0;
    int duration_seconds_ = 0;
    std::string song_title_;
    std::atomic<bool> video_playing_{false};
    std::function<void(const uint16_t*, int, int)> video_sink_;

    MediaResult Submit(bool song, const std::string& value, const std::string& label,
                       bool video = false);
    bool Cancelled(uint32_t id);
    void SetStatus(uint32_t id, const std::string& text);
    void Run();
    std::expected<std::string, std::string> SearchSong(const Request& request,
                                                       const Configuration& config,
                                                       int& duration_seconds, std::string& title);
    bool Stream(const std::string& url, uint32_t id, bool radio, std::string& error,
                bool& retryable, bool* delivered = nullptr);
    bool PushPcm(const int16_t* pcm, size_t count, int rate, uint32_t id, uint32_t generation,
                 bool pause_between_blocks);
};

}  // namespace uta
