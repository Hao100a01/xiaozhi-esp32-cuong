#pragma once

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>

#include "uta_helpers.h"

namespace uta {

inline constexpr size_t kMediaUrlLimit = 2048;
inline constexpr size_t kMediaTitleLimit = 160;

inline std::string TrimMediaText(std::string_view text) {
    while (!text.empty() && text.front() == ' ')
        text.remove_prefix(1);
    while (!text.empty() && text.back() == ' ')
        text.remove_suffix(1);
    return std::string(text);
}

constexpr bool ValidMediaText(std::string_view text, bool allow_empty = false) {
    if (text.size() > kMediaTitleLimit)
        return false;
    bool visible = false;
    for (const unsigned char byte : text) {
        if (byte < 32 || byte == 127)
            return false;
        visible |= byte != ' ';
    }
    return visible || allow_empty;
}

inline bool ValidMusicServer(std::string_view url) {
    return IsHttpUrl(url) && url.find_first_of("?#") == std::string_view::npos;
}

inline std::string LegacyMusicProxy(std::string_view url, std::string_view server) {
    if (!IsHttpUrl(url) || UrlOrigin(url) != UrlOrigin(server))
        return {};
    const auto origin = UrlOrigin(url);
    if (!url.substr(origin.size()).starts_with("/mp3/proxy/stream?"))
        return {};
    return std::string(origin) + std::string(url.substr(origin.size() + 4));
}

struct HttpRange {
    uint32_t first = 0;
    uint32_t last = 0;
    uint32_t total = 0;
};

inline bool ParseMovieRange(std::string_view text, uint32_t expected, uint32_t known_size,
                            size_t body_length, HttpRange& result) {
    if (!text.starts_with("bytes "))
        return false;
    text.remove_prefix(6);
    const auto dash = text.find('-');
    const auto slash = text.find('/');
    if (dash == std::string_view::npos || slash == std::string_view::npos || dash >= slash)
        return false;
    const auto number = [](std::string_view part, uint32_t& value) {
        const auto parsed = std::from_chars(part.data(), part.data() + part.size(), value);
        return !part.empty() && parsed.ec == std::errc{} && parsed.ptr == part.data() + part.size();
    };
    HttpRange range;
    if (!number(text.substr(0, dash), range.first) ||
        !number(text.substr(dash + 1, slash - dash - 1), range.last) ||
        !number(text.substr(slash + 1), range.total) || range.first != expected ||
        range.last < range.first || range.last >= range.total ||
        (known_size && range.total != known_size) ||
        (body_length && body_length != uint64_t{range.last} - range.first + 1))
        return false;
    result = range;
    return true;
}

}  // namespace uta
