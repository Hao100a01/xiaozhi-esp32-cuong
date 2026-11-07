#include "main/boards/UTA-robot/uta_hls.h"

constexpr bool CheckPlaylists() {
    auto master = uta::ParseHlsPlaylist(
        "https://radio/live/vov1.m3u8",
        "#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=72000\n"
        "https://cdn/live/vov1.m3u8?session=example\n");
    if (!master || master->variant != "https://cdn/live/vov1.m3u8?session=example")
        return false;
    auto media = uta::ParseHlsPlaylist(
        master->variant,
        "#EXTM3U\r\n#EXT-X-TARGETDURATION:2\r\n#EXT-X-MEDIA-SEQUENCE:8913\r\n"
        "#EXTINF:1.984,\r\nvov1/a.ts?session=example\r\n"
        "#EXT-X-DISCONTINUITY\r\n#EXTINF:1.984,\r\nvov1/b.ts\r\n"
        "#EXT-X-ENDLIST\r\n");
    if (!media || media->segments.size() != 2 || media->target_seconds != 2 ||
        !media->ended || media->segments[0].sequence != 8913 ||
        media->segments[0].url != "https://cdn/live/vov1/a.ts?session=example" ||
        !media->segments[1].discontinuity)
        return false;
    for (auto invalid : {
             "<html>radio</html>", "#EXTM3U\n", "#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=1\n",
             "#EXTM3U\n#EXTINF:2,\nfile:///a.ts\n",
             "#EXTM3U\n#EXT-X-MEDIA-SEQUENCE:-1\n#EXTINF:2,\na.ts\n",
             "#EXTM3U\n#EXT-X-TARGETDURATION:0\n#EXTINF:2,\na.ts\n",
             "#EXTM3U\n#EXT-X-KEY:METHOD=AES-128,URI=\"key\"\n#EXTINF:2,\na.ts\n",
             "#EXTM3U\n#EXT-X-MAP:URI=\"init\"\n#EXTINF:2,\na.ts\n",
             "#EXTM3U\n#EXT-X-BYTERANGE:20@0\n#EXTINF:2,\na.ts\n",
             "#EXTM3U\n#EXT-X-SKIP:SKIPPED-SEGMENTS=1\n#EXTINF:2,\na.ts\n"}) {
        if (uta::ParseHlsPlaylist("https://radio/live.m3u8", invalid))
            return false;
    }
    return !uta::ParseHlsPlaylist("https://radio/live.m3u8", std::string(65537, 'x'));
}

constexpr std::array<uint8_t, 188> TsPacket(unsigned pid, bool start, unsigned counter,
                                          std::span<const uint8_t> payload) {
    std::array<uint8_t, 188> packet{};
    packet.fill(0xff);
    packet[0] = 0x47;
    packet[1] = (pid >> 8) | (start ? 0x40 : 0);
    packet[2] = pid & 255;
    packet[3] = 0x30 | counter;
    packet[4] = 183 - payload.size();
    if (packet[4])
        packet[5] = 0;
    std::copy(payload.begin(), payload.end(), packet.end() - payload.size());
    return packet;
}

constexpr bool CheckTs() {
    uta::HlsTsAudio demux;
    // PAT/PMT table bytes from the VOV1 transport stream, including pointer and CRC.
    constexpr uint8_t pat[] = {0, 0, 0xb0, 0x0d, 0, 1, 0xc1, 0, 0,
                               0, 1, 0xef, 0xff, 0x36, 0x90, 0xe2, 0x3d};
    constexpr uint8_t pmt[] = {0, 2, 0xb0, 0x12, 0, 1, 0xc1, 0, 0, 0xe1, 1, 0xf0, 0,
                               0x0f, 0xe1, 1, 0xf0, 0, 0xec, 0xe2, 0xb0, 0x94};
    auto packet = TsPacket(0, true, 0, pat);
    auto result = demux.Packet(packet);
    if (!result || !result->empty())
        return false;
    packet = TsPacket(4095, true, 0, pmt);
    result = demux.Packet(packet);
    if (!result || !result->empty())
        return false;
    constexpr uint8_t pes[] = {0, 0, 1, 0xc0, 0, 11, 0x80, 0, 0,
                               0xff, 0xf1, 0x50, 0x80, 0, 0xff, 0xfc, 0x12};
    packet = TsPacket(257, true, 0, std::span(pes).first(5));
    result = demux.Packet(packet);
    if (!result || !result->empty())
        return false;
    packet = TsPacket(257, false, 1, std::span(pes).subspan(5));
    result = demux.Packet(packet);
    if (!result || result->size() != 8 || (*result)[0] != 0xff || (*result)[1] != 0xf1)
        return false;
    // Duplicate payload must not be fed into the AAC decoder a second time.
    result = demux.Packet(packet);
    if (!result || !result->empty())
        return false;
    packet = TsPacket(257, true, 3, pes);
    if (demux.Packet(packet))
        return false;
    packet[0] = 0;
    if (demux.Packet(packet))
        return false;
    packet = TsPacket(257, true, 2, pes);
    packet[3] |= 0x80;
    return !demux.Packet(packet);
}

// C++23 constant evaluation runs these checks even with the cross compiler's
// syntax-only mode, without producing or flashing a firmware image.
static_assert(CheckPlaylists());
static_assert(CheckTs());

int main() {}
