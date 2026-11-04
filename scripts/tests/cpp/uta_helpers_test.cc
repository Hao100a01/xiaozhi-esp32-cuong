#include "main/boards/UTA-robot/uta_helpers.h"

// Các kiểm tra chạy lúc biên dịch, không cần phần cứng hoặc thư viện ESP-IDF.
static_assert(uta::ValidatePins(uta::PinSettings{}).empty());
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
    pins.pins[uta::kLcdClock] = 19;  // Không chiếm chân USB.
    return !uta::ValidatePins(pins).empty();
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
