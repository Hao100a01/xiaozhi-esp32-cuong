#pragma once

#include <string>
#include <string_view>

namespace uta {
enum class ReplyGesture { None, Nod, Shake };

inline ReplyGesture ReplyGestureFor(std::string_view sentence) {
    std::string word;
    bool nod = false;
    bool previous_chinh = false;
    for (size_t i = 0; i <= sentence.size();) {
        const auto byte = i < sentence.size() ? static_cast<unsigned char>(sentence[i]) : 0;
        // Dấu câu Unicode thường gặp, gồm dấu ngoặc kép, gạch ngang và dấu ba chấm.
        const bool punctuation = i + 2 < sentence.size() &&
                                 sentence.substr(i, 2) == "\xe2\x80";
        if (!punctuation && (byte >= 128 || (byte >= 'a' && byte <= 'z') ||
                            (byte >= 'A' && byte <= 'Z') || (byte >= '0' && byte <= '9') ||
                            byte == '_')) {
            if (byte == 0xc3 && i + 1 < sentence.size()) {
                const auto accent = static_cast<unsigned char>(sentence[i + 1]);
                word += sentence[i++];
                word += static_cast<char>(accent >= 0x80 && accent <= 0x9e && accent != 0x97
                                              ? accent + 0x20
                                              : accent);
                ++i;
            } else {
                word += static_cast<char>(byte >= 'A' && byte <= 'Z' ? byte + ('a' - 'A') : byte);
                ++i;
            }
            continue;
        }
        if (!word.empty()) {
            if (word == "không")
                return ReplyGesture::Shake;
            nod = nod || word == "có" || word == "đúng" || word == "Đúng" ||
                  (previous_chinh && word == "xác");
            previous_chinh = word == "chính";
            word.clear();
        }
        if (punctuation || (byte != ' ' && byte != '\t' && byte != '\n' && byte != '\r'))
            previous_chinh = false;
        i += punctuation ? 3 : 1;
    }
    return nod ? ReplyGesture::Nod : ReplyGesture::None;
}
}  // namespace uta
