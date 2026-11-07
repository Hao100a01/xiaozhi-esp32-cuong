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

inline constexpr char kVov1HlsUrl[] = "https://audio-lss.vov.vn/live/vov1.m3u8";
inline constexpr std::array<RadioStation, 9> kStations{{
    {"vov1", "VOV 1 — Thời sự", kVov1HlsUrl},
    {"vov2", "VOV 2 — Văn hóa và giáo dục", "https://audio-lss.vov.vn/live/vov2.m3u8"},
    {"vov3", "VOV 3 — Âm nhạc và giải trí", "https://audio-lss.vov.vn/live/vov3.m3u8"},
    {"vov4", "VOV4", "https://audio-lss.vov.vn/live/vov4.m3u8"},
    {"vov4_dbscl", "VOV4 ĐBSCL",
     "https://str.vov.gov.vn/vovlive/vov4.DBCuuLong.sdp_aac/playlist.m3u8"},
    {"vov5", "VOV 5 tiếng Việt",
     "https://str.vov.gov.vn/vovlive/vov1vov5Vietnamese.sdp_aac/playlist.m3u8"},
    {"vov_mekong", "VOV Giao thông Mê Kông",
     "https://str.vov.gov.vn/vovlive/vovgtMeKong.sdp_aac/playlist.m3u8"},
    {"vov_gt_hcm", "VOV Giao thông TP.HCM",
     "https://play.vovgiaothong.vn/live/gthcm3/playlist.m3u8"},
    {"vov_sonata", "VOV Sonata", "https://audio-lss.vov.vn/live/sonata.m3u8"},
}};

inline const RadioStation* FindRadioStation(std::string_view query) {
    const auto lower_ascii = [](std::string_view text) {
        std::string result(text);
        for (char& c : result) {
            if (c >= 'A' && c <= 'Z') {
                c += 'a' - 'A';
            }
        }
        return result;
    };
    const auto key = lower_ascii(query);
    std::string_view id = key;
    if (key == "vov 1" || key == "vov" || key == "vov mộc" || key == "vov mốc" ||
        key == "vov mốt" || key == "vov mậu" || key == "vov máu" || key == "vov một" ||
        key == "vov mút" || key == "vov mót" || key == "vov mục") {
        id = "vov1";
    } else if (key == "mê kông" || key == "mekong" || key == "vov mekong" ||
               key == "vov mekong fm" || key == "giao thông mê kông") {
        id = "vov_mekong";
    } else if (key == "vov giao thông tphcm" || key == "vov giao thông tp. hồ chí minh" ||
               key == "giao thông tphcm") {
        id = "vov_gt_hcm";
    } else if (key == "vov 4") {
        id = "vov4";
    } else if (key == "vov 5" || key == "vov5 tiếng việt") {
        id = "vov5";
    } else if (key == "sonata" || key == "sanata" || key == "vov sanata") {
        id = "vov_sonata";
    }
    for (const auto& station : kStations) {
        const auto name = lower_ascii(station.name);
        if (id == station.id || key == name || key == name.substr(0, name.find(" — "))) {
            return &station;
        }
    }
    return nullptr;
}

}  // namespace uta
