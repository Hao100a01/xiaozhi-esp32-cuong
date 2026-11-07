#pragma once

#include <cstring>
#include <expected>
#include <memory>
#include <string>

#include <cJSON.h>

#include "uta_location_json.h"
#include "uta_media_policy.h"

namespace uta {

inline std::expected<std::string, std::string> ParseSongResponse(const std::string& base,
                                                                 const std::string& response,
                                                                 int* duration_seconds = nullptr,
                                                                 std::string* title = nullptr) {
    if (response.size() > 8192 || response.find('\0') != std::string::npos ||
        response.find("\\u0000") != std::string::npos || !LocationJsonDepthSafe(response))
        return std::unexpected("Máy chủ tìm nhạc trả JSON không hợp lệ hoặc quá lớn");
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(
        cJSON_ParseWithLengthOpts(response.c_str(), response.size() + 1, nullptr, true),
        cJSON_Delete);
    const auto* audio = cJSON_GetObjectItemCaseSensitive(root.get(), "audio_url");
    if (!cJSON_IsObject(root.get()) || !cJSON_IsString(audio) || !audio->valuestring ||
        std::strlen(audio->valuestring) > kMediaUrlLimit)
        return std::unexpected("Không tìm thấy audio_url hợp lệ trong phản hồi máy chủ nhạc");
    auto url = ResolveUrl(base, audio->valuestring);
    if (url.empty())
        return std::unexpected("Máy chủ nhạc trả địa chỉ audio_url không hợp lệ");
    if (duration_seconds) {
        const auto read_duration = [&](const char* field) {
            const auto* duration = cJSON_GetObjectItemCaseSensitive(root.get(), field);
            return cJSON_IsNumber(duration) && duration->valuedouble >= 1 &&
                           duration->valuedouble <= 86400
                       ? static_cast<int>(duration->valuedouble)
                       : 0;
        };
        *duration_seconds = read_duration("duration");
        if (*duration_seconds == 0)
            *duration_seconds = read_duration("duration_seconds");
    }
    if (title) {
        const auto* name = cJSON_GetObjectItemCaseSensitive(root.get(), "title");
        *title = cJSON_IsString(name) && name->valuestring && ValidMediaText(name->valuestring)
                     ? TrimMediaText(name->valuestring)
                     : std::string{};
    }
    return url;
}

}  // namespace uta
