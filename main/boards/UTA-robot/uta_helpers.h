#pragma once

#include <array>
#include <cctype>
#include <string>
#include <string_view>

namespace uta {

enum PinIndex {
    kLcdMosi,
    kLcdClock,
    kLcdDc,
    kLcdCs,
    kBacklight,
    kLcdReset,
    kTouchMiso,
    kTouchCs,
    kTouchIrq,
    kChassisTx,
    kChassisRx,
    kMicWs,
    kMicSck,
    kMicDin,
    kSpkDout,
    kSpkBclk,
    kSpkLrck,
    kMotorL1,
    kMotorL2,
    kMotorR1,
    kMotorR2,
    kServoHead,
    kServoNeck,
    kPinCount
};

inline constexpr std::array<const char*, kPinCount> kPinKeys{
    "d_mosi",     "d_clk",       "d_dc",      "d_cs",      "d_blk",     "d_rst",     "cam_ung_miso",
    "cam_ung_cs", "cam_ung_irq", "robot_tx",  "robot_rx",  "mic_ws",    "mic_sck",   "mic_din",
    "spk_dout",   "spk_bclk",    "spk_lrck",  "dc_trai_1", "dc_trai_2", "dc_phai_1", "dc_phai_2",
    "servo_dau", "servo_co"};
inline constexpr std::array<const char*, kPinCount> kPinLabels{
    "Dữ liệu màn hình", "Xung màn hình",       "Lệnh màn hình",    "Chọn màn hình",
    "Đèn nền",          "Đặt lại màn hình",    "Dữ liệu cảm ứng",  "Chọn cảm ứng",
    "Ngắt cảm ứng",     "Truyền tới đế robot", "Nhận từ đế robot", "Chọn kênh mic",
    "Xung mic",         "Dữ liệu mic",         "Dữ liệu loa",      "Xung loa",
    "Chọn kênh loa",    "Động cơ trái IN1",    "Động cơ trái IN2", "Động cơ phải IN1",
    "Động cơ phải IN2", "Servo đầu",          "Servo cổ"};

struct PinSettings {
    std::array<int, kPinCount> pins{40, 39, 42, 2,  1, 41, 11, 12, 13, 38, 48, 4, 5,
                                    6,  7,  15, 16, 8, 9,  10, 14, 21, 43};
};

inline constexpr bool AllowedGpio(int pin) {
    return (pin >= 0 && pin <= 21) || (pin >= 38 && pin <= 48);
}

inline constexpr std::string ValidatePins(const PinSettings& config) {
    for (size_t i = 0; i < config.pins.size(); ++i) {
        const int pin = config.pins[i];
        if (pin == -1 && (i == kLcdReset || i == kTouchIrq)) {
            continue;
        }
        // Giữ chân khởi động, USB và bộ nhớ Flash/PSRAM của N16R8.
        if (!AllowedGpio(pin)) {
            return std::string(kPinLabels[i]) + ": chân bị giữ lại cho phần cứng khác";
        }
        for (size_t j = 0; j < i; ++j) {
            if (config.pins[j] == pin) {
                return std::string(kPinLabels[i]) + ": trùng chân với " + kPinLabels[j];
            }
        }
    }
    return {};
}

inline constexpr bool IsHttpUrl(std::string_view url) {
    if (url.size() > 2048 || (url.substr(0, 7) != "http://" && url.substr(0, 8) != "https://")) {
        return false;
    }
    const auto start = url.find("://") + 3;
    const auto end = url.find_first_of("/?#", start);
    const auto host = url.substr(start, end == std::string_view::npos ? end : end - start);
    if (host.empty() || host.find('@') != std::string_view::npos) {
        return false;
    }
    for (unsigned char c : url) {
        if (c <= 32 || c == 127 || c == '\\') {
            return false;
        }
    }
    return true;
}

inline bool IsMp4Url(std::string_view url) {
    const auto end = url.find_first_of("?#");
    auto path = url.substr(0, end);
    if (path.size() < 4)
        return false;
    std::string suffix(path.substr(path.size() - 4));
    for (auto& c : suffix)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return suffix == ".mp4";
}

inline constexpr std::string UrlEncode(std::string_view input) {
    constexpr char digits[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(input.size() * 3);
    for (unsigned char c : input) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            result += static_cast<char>(c);
        } else {
            result += '%';
            result += digits[c >> 4];
            result += digits[c & 15];
        }
    }
    return result;
}

inline constexpr std::string ResolveUrl(const std::string& base, const std::string& path) {
    if (IsHttpUrl(path)) {
        return path;
    }
    if (path.empty() || path.starts_with("//") || path.find("://") != std::string::npos) {
        return {};
    }
    const auto authority_end = base.find('/', base.find("://") + 3);
    std::string url;
    if (path.front() == '/') {
        url = base.substr(0, authority_end);
    } else {
        url = base;
        if (!url.ends_with('/'))
            url += '/';
    }
    for (char c : path)
        url += c;
    if (!IsHttpUrl(url))
        return {};
    return url;
}

inline constexpr std::string_view UrlOrigin(std::string_view url) {
    const auto start = url.find("://");
    if (start == std::string_view::npos)
        return {};
    return url.substr(0, url.find_first_of("/?#", start + 3));
}

inline constexpr std::string ResolveRedirect(const std::string& current,
                                             const std::string& location) {
    if (location.starts_with("//")) {
        auto url = current.substr(0, current.find(':'));
        url += ':';
        for (char c : location)
            url += c;
        if (!IsHttpUrl(url))
            return {};
        return url;
    }
    const auto clean = current.substr(0, current.find_first_of("?#"));
    const auto slash = clean.rfind('/');
    const auto base = slash < UrlOrigin(clean).size() ? std::string(UrlOrigin(clean))
                                                      : clean.substr(0, slash + 1);
    return ResolveUrl(base, location);
}

struct RadioStation {
    const char* id;
    const char* name;
    const char* url;
};

// Giữ danh sách đài từ repo Việt Nam; địa chỉ có thể đổi ngoài firmware.
inline constexpr std::array<RadioStation, 7> kStations{{
    {"vov1", "VOV 1 — Thời sự", "https://stream.vovmedia.vn/vov-1"},
    {"vov2", "VOV 2 — Văn hóa và giáo dục", "https://stream.vovmedia.vn/vov-2"},
    {"vov3", "VOV 3 — Âm nhạc và giải trí", "https://stream.vovmedia.vn/vov-3"},
    {"vov5", "VOV 5 — Đối ngoại", "https://stream.vovmedia.vn/vov5"},
    {"vov_gt_hn", "VOV Giao thông Hà Nội", "https://stream.vovmedia.vn/vovgt-hn"},
    {"vov_gt_hcm", "VOV Giao thông TP. Hồ Chí Minh", "https://stream.vovmedia.vn/vovgt-hcm"},
    {"vov_tieng_anh", "VOV — Kênh tiếng Anh", "https://stream.vovmedia.vn/vov247"},
}};

}  // namespace uta
