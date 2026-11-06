#pragma once

#include <string_view>

namespace uta {

// Chặn JSON lồng sâu trước khi gọi cJSON để bảo vệ stack tác vụ mạng.
constexpr bool LocationJsonDepthSafe(std::string_view body) {
    unsigned depth = 0;
    bool quoted = false;
    bool escaped = false;
    for (const char byte : body) {
        if (quoted) {
            if (escaped)
                escaped = false;
            else if (byte == '\\')
                escaped = true;
            else if (byte == '"')
                quoted = false;
        } else if (byte == '"') {
            quoted = true;
        } else if (byte == '{' || byte == '[') {
            if (++depth > 4)
                return false;
        } else if (byte == '}' || byte == ']') {
            if (depth == 0)
                return false;
            --depth;
        }
    }
    // Loại cú pháp ngoặc không khớp còn lại do cJSON kiểm tra.
    return depth == 0 && !quoted;
}

}  // namespace uta
