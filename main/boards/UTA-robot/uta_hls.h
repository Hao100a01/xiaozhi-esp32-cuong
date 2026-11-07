#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "uta_helpers.h"

namespace uta {

struct HlsSegment {
    uint64_t sequence = 0;
    std::string url;
    bool discontinuity = false;
};

struct HlsPlaylist {
    std::string variant;
    std::vector<HlsSegment> segments;
    unsigned target_seconds = 2;
    bool ended = false;
};

inline constexpr bool HlsNumber(std::string_view text, uint64_t& value) {
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

inline constexpr std::expected<HlsPlaylist, std::string> ParseHlsPlaylist(const std::string& url,
                                                               std::string_view text) {
    if (text.size() > 65536 || text.find('\0') != std::string_view::npos)
        return std::unexpected("Playlist HLS quá lớn hoặc không hợp lệ");
    if (text.starts_with("\xef\xbb\xbf"))
        text.remove_prefix(3);
    HlsPlaylist result;
    uint64_t sequence = 0;
    bool header = false, variant = false, segment = false, discontinuity = false;
    while (!text.empty()) {
        const auto end = text.find('\n');
        auto line = text.substr(0, end);
        text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
        if (line.ends_with('\r'))
            line.remove_suffix(1);
        if (line.empty())
            continue;
        if (!header) {
            if (line != "#EXTM3U")
                return std::unexpected("Nguồn radio không phải playlist HLS");
            header = true;
            continue;
        }
        if (line.starts_with("#EXT-X-STREAM-INF:")) {
            variant = true;
        } else if (line.starts_with("#EXT-X-MEDIA-SEQUENCE:")) {
            if (!result.segments.empty() || !HlsNumber(line.substr(22), sequence))
                return std::unexpected("Số thứ tự HLS không hợp lệ");
        } else if (line.starts_with("#EXT-X-TARGETDURATION:")) {
            uint64_t seconds = 0;
            if (!HlsNumber(line.substr(22), seconds) || seconds < 1 || seconds > 60)
                return std::unexpected("Thời lượng đoạn HLS không được hỗ trợ");
            result.target_seconds = static_cast<unsigned>(seconds);
        } else if (line.starts_with("#EXT-X-KEY:")) {
            if (line != "#EXT-X-KEY:METHOD=NONE")
                return std::unexpected("Radio HLS có mã hóa chưa được hỗ trợ");
        } else if (line.starts_with("#EXT-X-MAP:") || line.starts_with("#EXT-X-BYTERANGE:") ||
                   line.starts_with("#EXT-X-GAP") || line.starts_with("#EXT-X-SKIP:")) {
            return std::unexpected("Radio HLS cần MPEG-TS đầy đủ, không có đoạn bị bỏ qua");
        } else if (line == "#EXT-X-DISCONTINUITY") {
            discontinuity = true;
        } else if (line == "#EXT-X-ENDLIST") {
            result.ended = true;
        } else if (line.starts_with("#EXTINF:")) {
            segment = true;
        } else if (line.front() != '#') {
            const auto resolved = ResolveRedirect(url, std::string(line));
            if (resolved.empty())
                return std::unexpected("HLS chứa địa chỉ không hợp lệ");
            if (variant) {
                // Chọn rendition đầu tiên. Không lưu URL phiên CDN vào NVS.
                if (result.variant.empty())
                    result.variant = resolved;
                variant = false;
            } else if (segment) {
                if (result.segments.size() >= 128 || sequence == UINT64_MAX)
                    return std::unexpected("Playlist HLS vượt giới hạn");
                result.segments.push_back({sequence++, resolved, discontinuity});
                discontinuity = false;
                segment = false;
            } else {
                return std::unexpected("Đoạn HLS thiếu thông tin EXTINF");
            }
        }
    }
    if (!header || variant || segment ||
        (result.variant.empty() && result.segments.empty()) ||
        (!result.variant.empty() && !result.segments.empty()))
        return std::unexpected("Playlist HLS chưa có nội dung hợp lệ");
    return result;
}

// Tách AAC ADTS từ gói TS 188 byte. Không giữ cả đoạn TS trong bộ nhớ.
class HlsTsAudio {
public:
    constexpr std::expected<std::span<const uint8_t>, std::string> Packet(
        std::span<const uint8_t> packet) {
        if (packet.size() != 188 || packet[0] != 0x47 || (packet[1] & 0x80) ||
            (packet[3] & 0xc0))
            return std::unexpected("Gói MPEG-TS radio bị lỗi hoặc có mã hóa");
        const unsigned pid = (packet[1] & 31) * 256 + packet[2];
        const bool start = packet[1] & 0x40;
        const unsigned control = (packet[3] >> 4) & 3;
        if (control == 0)
            return std::unexpected("Gói MPEG-TS thiếu thông tin payload");
        size_t offset = 4;
        if (control & 2) {
            offset += 1 + packet[4];
            if (offset > packet.size())
                return std::unexpected("Adaptation field MPEG-TS không hợp lệ");
        }
        if (!(control & 1) || offset == packet.size())
            return std::span<const uint8_t>{};
        auto payload = packet.subspan(offset);
        if (pid == 0 || pid == pmt_pid_) {
            auto section = Section(payload, start, pid);
            if (!section)
                return std::unexpected(section.error());
            if (section->empty())
                return std::span<const uint8_t>{};
            auto table = *section;
            if (pid == 0 && table[0] == 0) {
                for (size_t i = 8; i + 4 <= table.size() - 4; i += 4) {
                    if (table[i] || table[i + 1]) {
                        pmt_pid_ = ((table[i + 2] & 31) << 8) | table[i + 3];
                        break;
                    }
                }
            } else if (pid == pmt_pid_ && table[0] == 2 && table.size() >= 16) {
                size_t i = 12 + ((table[10] & 15) << 8) + table[11];
                while (i + 5 <= table.size() - 4) {
                    if (table[i] == 0x0f) {
                        audio_pid_ = ((table[i + 1] & 31) << 8) | table[i + 2];
                        break;
                    }
                    i += 5 + ((table[i + 3] & 15) << 8) + table[i + 4];
                }
            }
            return std::span<const uint8_t>{};
        }
        if (pid != audio_pid_)
            return std::span<const uint8_t>{};
        const unsigned counter = packet[3] & 15;
        if (last_counter_ >= 0) {
            if (counter == static_cast<unsigned>(last_counter_))
                return std::span<const uint8_t>{};
            if (counter != static_cast<unsigned>((last_counter_ + 1) & 15))
                return std::unexpected("Mất gói âm thanh MPEG-TS");
        }
        last_counter_ = counter;
        if (start) {
            header_used_ = 0;
            header_size_ = 9;
            pes_remaining_ = 0;
        }
        if (header_size_ == 0)
            return std::span<const uint8_t>{};
        while (header_used_ < header_size_ && !payload.empty()) {
            header_[header_used_++] = payload.front();
            payload = payload.subspan(1);
            if (header_used_ == 9) {
                if (header_[0] || header_[1] || header_[2] != 1 ||
                    header_[3] < 0xc0 || header_[3] > 0xdf || (header_[6] & 0xc0) != 0x80)
                    return std::unexpected("PES radio không phải âm thanh AAC hợp lệ");
                header_size_ = 9 + header_[8];
                const unsigned length = (header_[4] << 8) | header_[5];
                if (length && length < 3U + header_[8])
                    return std::unexpected("Độ dài PES radio không hợp lệ");
                pes_remaining_ = length ? length - 3 - header_[8] : -1;
            }
        }
        if (header_used_ < header_size_)
            return std::span<const uint8_t>{};
        if (pes_remaining_ >= 0) {
            payload = payload.first(std::min(payload.size(), static_cast<size_t>(pes_remaining_)));
            pes_remaining_ -= payload.size();
        }
        return payload;
    }

private:
    unsigned pmt_pid_ = 8192;
    unsigned audio_pid_ = 8192;
    int last_counter_ = -1;
    std::array<uint8_t, 264> header_{};
    size_t header_used_ = 0, header_size_ = 0;
    int pes_remaining_ = 0;
    std::array<uint8_t, 1024> section_{};
    size_t section_used_ = 0;
    unsigned section_pid_ = 8192;

    constexpr std::expected<std::span<const uint8_t>, std::string> Section(
        std::span<const uint8_t> payload, bool start, unsigned pid) {
        if (start) {
            const size_t pointer = payload.front();
            if (pointer + 1 >= payload.size())
                return std::unexpected("Bảng MPEG-TS radio không hợp lệ");
            payload = payload.subspan(pointer + 1);
            section_used_ = 0;
            section_pid_ = pid;
        } else if (section_used_ == 0 || section_pid_ != pid) {
            return std::span<const uint8_t>{};
        }
        const size_t bytes = std::min(payload.size(), section_.size() - section_used_);
        std::copy_n(payload.data(), bytes, section_.data() + section_used_);
        section_used_ += bytes;
        if (section_used_ < 3)
            return std::span<const uint8_t>{};
        const size_t length = 3 + ((section_[1] & 15) << 8) + section_[2];
        if (length < 12 || length > section_.size())
            return std::unexpected("Bảng MPEG-TS vượt giới hạn");
        if (section_used_ < length)
            return std::span<const uint8_t>{};
        section_used_ = 0;
        uint32_t crc = 0xffffffff;
        for (size_t i = 0; i < length; ++i) {
            crc ^= static_cast<uint32_t>(section_[i]) << 24;
            for (unsigned bit = 0; bit < 8; ++bit)
                crc = (crc << 1) ^ ((crc & 0x80000000) ? 0x04c11db7 : 0);
        }
        if (crc)
            return std::unexpected("CRC bảng MPEG-TS radio không hợp lệ");
        return std::span<const uint8_t>(section_.data(), length);
    }
};

}  // namespace uta
