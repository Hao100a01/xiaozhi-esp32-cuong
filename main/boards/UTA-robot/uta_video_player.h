#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace uta {
struct VideoCallbacks {
    int output_rate = 24000;
    std::function<bool()> cancelled;
    std::function<bool()> paused;
    std::function<bool(const int16_t*, size_t)> audio;
    std::function<void(const uint16_t*, int, int)> video;
    std::function<void(const std::string&)> error;
};

// Chạy trên tác vụ media; không tạo thêm bộ ghi I2S hoặc vòng lặp chính.
bool PlayMp4(const std::string& url, const VideoCallbacks& callbacks);
}  // namespace uta
