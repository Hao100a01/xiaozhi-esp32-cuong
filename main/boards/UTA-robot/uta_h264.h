#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace uta {
// Chuyển NAL dạng độ dài trong MP4 thành Annex B; kiểm tra mọi độ dài trước khi sao chép.
inline bool H264AnnexB(const uint8_t* frame, size_t size, const uint8_t* extra, size_t extra_size,
                       bool first, uint8_t* output, size_t capacity, size_t& used) {
    used = 0;
    if (!frame || !size || !output)
        return false;
    auto append = [&](const uint8_t* data, size_t count, bool prefix) {
        const size_t needed = count + (prefix ? 4 : 0);
        if (used > capacity || needed > capacity - used)
            return false;
        if (prefix) {
            output[used++] = 0;
            output[used++] = 0;
            output[used++] = 0;
            output[used++] = 1;
        }
        std::memcpy(output + used, data, count);
        used += count;
        return true;
    };
    unsigned length_bytes = 4;
    if (extra && extra_size >= 7 && extra[0] == 1) {
        if (extra[1] != 66)
            return false;  // Chỉ H.264 Baseline, không nhận B-frame.
        length_bytes = (extra[4] & 3) + 1;
        size_t position = 6;
        unsigned units = extra[5] & 31;
        for (unsigned group = 0; group < 2; ++group) {
            for (unsigned i = 0; i < units; ++i) {
                if (position + 2 > extra_size)
                    return false;
                const size_t count =
                    (static_cast<size_t>(extra[position]) << 8) | extra[position + 1];
                position += 2;
                if (!count || count > extra_size - position)
                    return false;
                if (first && !append(extra + position, count, true))
                    return false;
                position += count;
            }
            if (group == 0) {
                if (position >= extra_size)
                    return false;
                units = extra[position++];
            }
        }
    } else if (first && extra && extra_size >= 4 && extra[0] == 0 && extra[1] == 0 &&
               (extra[2] == 1 || (extra[2] == 0 && extra[3] == 1))) {
        if (!append(extra, extra_size, false))
            return false;
    }
    if (size >= 4 && frame[0] == 0 && frame[1] == 0 &&
        (frame[2] == 1 || (frame[2] == 0 && frame[3] == 1)))
        return append(frame, size, false);
    size_t position = 0;
    while (position < size) {
        if (size - position < length_bytes)
            return false;
        size_t count = 0;
        for (unsigned i = 0; i < length_bytes; ++i)
            count = (count << 8) | frame[position++];
        if (!count || count > size - position || !append(frame + position, count, true))
            return false;
        position += count;
    }
    return true;
}
}  // namespace uta
