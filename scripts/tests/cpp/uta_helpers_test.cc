#include "main/boards/UTA-robot/uta_helpers.h"
#include "main/boards/UTA-robot/uta_location_json.h"
#include "main/boards/UTA-robot/uta_provinces.h"

static_assert(uta::ValidProvinceCode(""));
static_assert(uta::FindProvince("79")->name == "TP. Hồ Chí Minh");
static_assert(uta::FindProvince("01")->name == "Hà Nội");
static_assert(!uta::ValidProvinceCode("1"));
static_assert(!uta::ValidProvinceCode("99"));
static_assert(!uta::ValidProvinceCode("79<script>"));
static_assert(uta::LocationJsonDepthSafe(R"({"province":"79"})"));
static_assert(uta::LocationJsonDepthSafe(R"({"city":"[{\\\"text"})"));
static_assert(!uta::LocationJsonDepthSafe("[[[[[0]]]]]"));
static_assert(!uta::LocationJsonDepthSafe("{\"city\":\"unfinished"));
static_assert(!uta::LocationJsonDepthSafe("}{"));
static_assert([]() {
    for (size_t i = 0; i < uta::kProvinces.size(); ++i) {
        const auto& province = uta::kProvinces[i];
        if (province.code.size() != 2 || province.name.empty() || province.latitude < 8 ||
            province.latitude > 24 || province.longitude < 102 || province.longitude > 110)
            return false;
        for (size_t j = i + 1; j < uta::kProvinces.size(); ++j) {
            if (province.code == uta::kProvinces[j].code)
                return false;
        }
    }
    return true;
}());

// Các kiểm tra chạy lúc biên dịch, không cần phần cứng hoặc thư viện ESP-IDF.
static_assert(uta::ValidatePins(uta::PinSettings{}).empty());
static_assert(uta::AllowedGpio(0) && uta::AllowedGpio(3) && uta::AllowedGpio(20));
static_assert(uta::AllowedGpio(43) && uta::AllowedGpio(44) && uta::AllowedGpio(45) &&
              uta::AllowedGpio(46) && uta::AllowedGpio(48));
static_assert(!uta::AllowedGpio(35) && !uta::AllowedGpio(36) && !uta::AllowedGpio(37));
static_assert([]() {
    auto pins = uta::PinSettings{};
    pins.pins[uta::kMicDin] = pins.pins[uta::kLcdMosi];
    return !uta::ValidatePins(pins).empty();
}());
static_assert([]() {
    auto pins = uta::PinSettings{};
    pins.pins[uta::kChassisTx] = 35;  // Dành cho bộ nhớ PSRAM.
    return !uta::ValidatePins(pins).empty();
}());
static_assert([]() {
    auto pins = uta::PinSettings{};
    pins.pins[uta::kTouchCs] = pins.pins[uta::kBacklight];
    return !uta::ValidatePins(pins).empty();
}());
static_assert([]() {
    auto pins = uta::PinSettings{};
    pins.pins[uta::kLcdClock] = 19;  // USB, được phép với cảnh báo trên web.
    return uta::ValidatePins(pins).empty();
}());
static_assert([]() {
    auto pins = uta::PinSettings{};
    pins.pins[uta::kChassisTx] = -1;
    return !uta::ValidatePins(pins).empty();
}());
static_assert([]() {
    auto pins = uta::PinSettings{};
    pins.pins[uta::kChassisTx] = 48;
    pins.pins[uta::kChassisRx] = 38;
    pins.pins[uta::kTouchIrq] = -1;
    pins.pins[uta::kLcdReset] = 13;
    return uta::ValidatePins(pins).empty();
}());
static_assert(uta::IsHttpUrl("https://stream.vovmedia.vn/vov-1"));
static_assert(uta::IsHttpUrl("http://192.168.1.20:8000/nhac"));
static_assert(!uta::IsHttpUrl("https://"));
static_assert(!uta::IsHttpUrl("file:///etc/passwd"));
static_assert(!uta::IsHttpUrl("https://nguoi:matkhau@maychu/nhac"));
static_assert(!uta::IsHttpUrl("https://maychu/\r\nX-Test: 1"));
static_assert(!uta::IsHttpUrl("https://maychu\\nhac"));
static_assert(uta::UrlEncode("bài hát & ca sĩ") == "b%C3%A0i%20h%C3%A1t%20%26%20ca%20s%C4%A9");
static_assert(uta::ResolveUrl("https://maychu/api", "/mp3/bai.mp3?ten=a%20b") ==
              "https://maychu/mp3/bai.mp3?ten=a%20b");
static_assert(uta::ResolveUrl("https://maychu/api", "bai.mp3") == "https://maychu/api/bai.mp3");
static_assert(uta::ResolveUrl("https://maychu", "//maykhac/bai.mp3").empty());
static_assert(uta::UrlOrigin("https://maychu:8080/a?q=1") == "https://maychu:8080");
static_assert(uta::ResolveRedirect("https://maychu/a/b.mp3", "c.mp3") == "https://maychu/a/c.mp3");
static_assert(uta::ResolveRedirect("https://maychu/a/b.mp3", "/c.mp3") == "https://maychu/c.mp3");
static_assert(uta::ResolveRedirect("https://maychu/a", "//dai/nhac") == "https://dai/nhac");
