#include "main/boards/UTA-robot/uta_media_response.h"

#include <cassert>
#include <string>

static_assert(!uta::ValidMediaText(""));
static_assert(!uta::ValidMediaText("   "));
static_assert(!uta::ValidMediaText("bai\nhat"));
static_assert(uta::ValidMediaText("Bài hát tiếng Việt"));
static_assert(uta::ValidMediaText("", true));

int main() {
    assert(uta::TrimMediaText("  Bài hát  ") == "Bài hát");
    assert(uta::ValidMediaText(std::string(160, 'a')));
    assert(!uta::ValidMediaText(std::string(161, 'a')));
    assert(uta::ValidMusicServer("https://server/api"));
    assert(!uta::ValidMusicServer("https://server/api?key=secret"));
    assert(!uta::ValidMusicServer("https://server/api#fragment"));
    assert(uta::LegacyMusicProxy("https://server/mp3/proxy/stream?url=a%2Fb", "https://server") ==
           "https://server/proxy/stream?url=a%2Fb");
    assert(uta::LegacyMusicProxy("https://other/mp3/proxy/stream?url=x", "https://server").empty());

    uta::HttpRange range;
    assert(uta::ParseMovieRange("bytes 0-99/1000", 0, 0, 100, range));
    assert(range.first == 0 && range.last == 99 && range.total == 1000);
    assert(uta::ParseMovieRange("bytes 100-999/1000", 100, 1000, 0, range));
    assert(!uta::ParseMovieRange("bytes 0-99/1000", 1, 0, 100, range));
    assert(!uta::ParseMovieRange("bytes 0-1000/1000", 0, 0, 0, range));
    assert(!uta::ParseMovieRange("bytes 0-99/1000", 0, 999, 100, range));
    assert(!uta::ParseMovieRange("bytes 0-99/1000", 0, 0, 99, range));
    assert(!uta::ParseMovieRange("bytes 10-9/1000", 10, 0, 0, range));
    assert(!uta::ParseMovieRange("bytes 0-bad/1000", 0, 0, 0, range));
    assert(!uta::ParseMovieRange("bytes 0-99/*", 0, 0, 0, range));
    assert(!uta::ParseMovieRange("bytes 0-99/1000junk", 0, 0, 0, range));
    assert(!uta::ParseMovieRange("bytes 0-99/4294967296", 0, 0, 0, range));
    assert(!uta::ParseMovieRange("bytes -1-99/1000", 0, 0, 0, range));

    auto song = uta::ParseSongResponse("https://server/api", R"({"audio_url":"/a.mp3"})");
    assert(song && *song == "https://server/a.mp3");
    int duration = 0;
    std::string title;
    song = uta::ParseSongResponse(
        "https://server",
        R"({"audio_url":"/a.mp3","duration":351,"title":"SÓNG GIÓ | ICM x JACK | OFFICIAL MUSIC VIDEO"})",
        &duration, &title);
    assert(song && duration == 351 && title == "SÓNG GIÓ | ICM x JACK | OFFICIAL MUSIC VIDEO");
    song = uta::ParseSongResponse(
        "https://server", R"({"audio_url":"/a.mp3","duration":-1,"duration_seconds":180})",
        &duration, &title);
    assert(song && duration == 180 && title.empty());
    song = uta::ParseSongResponse("https://server/api", R"({"audio_url":"a.mp4?token=123"})");
    assert(song && *song == "https://server/api/a.mp4?token=123");
    song = uta::ParseSongResponse("https://server", R"({"audio_url":"https://cdn/a.mp3"})");
    assert(song && *song == "https://cdn/a.mp3");
    for (const auto* invalid :
         {"", "[]", "null", "{}", R"({"audio_url":null})", R"({"audio_url":""})",
          R"({"audio_url":"file:///a.mp3"})", R"({"audio_url":"https://cdn/a.mp3\u0000evil"})",
          R"({"audio_url":"/a.mp3"} trailing)", "[[[[[0]]]]]"}) {
        assert(!uta::ParseSongResponse("https://server", invalid));
    }
    assert(!uta::ParseSongResponse("https://server", std::string(8193, ' ')));
    std::string embedded = R"({"audio_url":"/a.mp3"})";
    embedded.push_back('\0');
    embedded += "garbage";
    assert(!uta::ParseSongResponse("https://server", embedded));
}
