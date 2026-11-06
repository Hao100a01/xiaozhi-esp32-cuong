#include "uta_media_mcp.h"

#include <memory>

#include "mcp_server.h"
#include "uta_media_player.h"
#include "uta_media_policy.h"

namespace uta {
namespace {
using JsonPtr = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;

ToolResult Accepted(MediaResult result, const char* text) {
    if (!result)
        return std::unexpected(std::move(result.error()));
    return std::string(text);
}

ToolResult Status(MediaPlayer& media) {
    const auto status = media.Snapshot();
    JsonPtr root(cJSON_CreateObject(), cJSON_Delete);
    if (!root || !cJSON_AddStringToObject(root.get(), "loai", status.type.c_str()) ||
        !cJSON_AddStringToObject(root.get(), "ten", status.title.c_str()) ||
        !cJSON_AddStringToObject(root.get(), "trang_thai", status.status.c_str()) ||
        !cJSON_AddBoolToObject(root.get(), "dang_phat", status.playing) ||
        !cJSON_AddBoolToObject(root.get(), "dang_chuan_bi", status.preparing) ||
        !cJSON_AddBoolToObject(root.get(), "tam_dung", status.paused) ||
        !cJSON_AddBoolToObject(root.get(), "co_the_tiep_tuc",
                               status.playing && status.paused && !status.preparing) ||
        !cJSON_AddBoolToObject(root.get(), "co_the_phat_lai", status.replay_available))
        return std::unexpected("Không đủ bộ nhớ đọc trạng thái media");
    return root.release();
}

ToolResult Stations(SettingsStore& settings) {
    JsonPtr root(cJSON_CreateArray(), cJSON_Delete);
    if (!root)
        return std::unexpected("Không đủ bộ nhớ đọc danh sách đài");
    const auto add = [&](const char* id, const char* name) {
        JsonPtr item(cJSON_CreateObject(), cJSON_Delete);
        if (!item || !cJSON_AddStringToObject(item.get(), "ma_dai", id) ||
            !cJSON_AddStringToObject(item.get(), "ten", name) ||
            !cJSON_AddItemToArray(root.get(), item.get()))
            return false;
        item.release();
        return true;
    };
    for (const auto& station : kStations) {
        if (!add(station.id, station.name))
            return std::unexpected("Không đủ bộ nhớ đọc danh sách đài");
    }
    const auto config = settings.Get();
    if (IsHttpUrl(config.radio_url) && !add("tuy_chinh", config.radio_name.c_str()))
        return std::unexpected("Không đủ bộ nhớ đọc danh sách đài");
    return root.release();
}
}  // namespace

void RegisterMediaTools(MediaPlayer& media, SettingsStore& settings) {
    auto& mcp = McpServer::GetInstance();
    mcp.AddTool(
        "self.music.play_song",
        "Tìm bài theo tên/ca sĩ trên máy chủ đã cài. Nhận yêu cầu bất đồng bộ; dùng "
        "self.media.get_status để xem kết quả",
        PropertyList({Property("bai_hat", kPropertyTypeString).SetMaxLength(kMediaTitleLimit),
                      Property("ca_si", kPropertyTypeString, "").SetMaxLength(kMediaTitleLimit)}),
        [&media](const PropertyList& values) -> ToolResult {
            return Accepted(
                media.PlaySong(values["bai_hat"].value<std::string>(),
                               values["ca_si"].value<std::string>()),
                "Đã nhận yêu cầu tìm và phát bài hát; chưa xác nhận nguồn phát thành công");
        });
    mcp.AddTool("self.radio.list_stations",
                "Xem mã và tên các đài radio có sẵn, gồm đài tùy chỉnh đã lưu", PropertyList(),
                [&settings](const PropertyList&) { return Stations(settings); });
    mcp.AddTool("self.radio.play_station",
                "Phát đài theo mã từ self.radio.list_stations; tuy_chinh dùng đài đã lưu",
                PropertyList({Property("ma_dai", kPropertyTypeString).SetMaxLength(40)}),
                [&media, &settings](const PropertyList& values) -> ToolResult {
                    const auto id = TrimMediaText(values["ma_dai"].value<std::string>());
                    if (id == "tuy_chinh") {
                        const auto config = settings.Get();
                        return Accepted(media.PlayRadio(config.radio_url, config.radio_name),
                                        "Đã nhận yêu cầu phát đài tùy chỉnh");
                    }
                    for (const auto& station : kStations) {
                        if (id == station.id)
                            return Accepted(media.PlayRadio(station.url, station.name),
                                            "Đã nhận yêu cầu phát đài");
                    }
                    return std::unexpected("Mã đài không hợp lệ; hãy xem danh sách đài có sẵn");
                });
    mcp.AddTool("self.radio.play_url",
                "Phát URL luồng MP3/AAC radio trực tiếp; không nhận trang web, HLS hoặc playlist",
                PropertyList({Property("url", kPropertyTypeString).SetMaxLength(kMediaUrlLimit),
                              Property("ten", kPropertyTypeString, "Radio Internet")
                                  .SetMaxLength(kMediaTitleLimit)}),
                [&media](const PropertyList& values) -> ToolResult {
                    return Accepted(media.PlayRadio(values["url"].value<std::string>(),
                                                    values["ten"].value<std::string>()),
                                    "Đã nhận yêu cầu phát URL radio");
                });
    mcp.AddTool("self.video.play_mp4",
                "Phát tệp MP4 trực tiếp: H.264 Baseline/MJPEG tối đa 240x240, 15 fps, AAC-LC/MP3, "
                "HTTP Range; không nhận trang xem YouTube",
                PropertyList({Property("url", kPropertyTypeString).SetMaxLength(kMediaUrlLimit)}),
                [&media](const PropertyList& values) -> ToolResult {
                    return Accepted(media.PlayVideo(values["url"].value<std::string>()),
                                    "Đã nhận yêu cầu mở MP4; dùng trạng thái media để xem kết quả");
                });
    mcp.AddTool("self.media.get_status",
                "Xem trạng thái và lỗi của nhạc/radio/MP4; không trả khóa máy chủ hoặc URL đã ký",
                PropertyList(), [&media](const PropertyList&) { return Status(media); });
    mcp.AddTool("self.media.pause",
                "Tạm dừng nguồn nhạc/radio/MP4 còn đang mở; không tự mở lại nguồn đã dừng",
                PropertyList(), [&media](const PropertyList&) -> ToolResult {
                    return Accepted(media.Pause(true), "Đã nhận lệnh tạm dừng media");
                });
    mcp.AddTool("self.media.resume",
                "Tiếp tục nguồn đang tạm dừng. Nếu hội thoại đã dừng nguồn cũ, dùng "
                "self.media.replay để phát lại từ đầu",
                PropertyList(), [&media](const PropertyList&) -> ToolResult {
                    return Accepted(media.Pause(false), "Đã nhận lệnh tiếp tục nguồn đang mở");
                });
    mcp.AddTool("self.media.replay",
                "Mở lại nguồn gần nhất: bài hát/MP4 phát từ đầu, radio nối lại luồng trực tiếp; "
                "không phải tua hoặc tiếp tục từ vị trí cũ",
                PropertyList(), [&media](const PropertyList&) -> ToolResult {
                    return Accepted(
                        media.Replay(),
                        "Đã nhận yêu cầu phát lại: nhạc/MP4 từ đầu, radio ở thời điểm trực tiếp");
                });
    const auto stop = [&media](const PropertyList&) -> ToolResult {
        media.Stop();
        return std::string("Đã dừng nhạc/radio/MP4 và hủy yêu cầu đang chuẩn bị");
    };
    mcp.AddTool("self.media.stop", "Dừng nhạc, radio hoặc MP4, gồm cả yêu cầu đang chuẩn bị",
                PropertyList(), stop);
    // Giữ tên MCP cũ để tương thích các lệnh nhạc đang dùng.
    mcp.AddTool("self.music.stop", "Dừng nhạc/radio/MP4; tương thích tên lệnh cũ", PropertyList(),
                stop);
}

}  // namespace uta
