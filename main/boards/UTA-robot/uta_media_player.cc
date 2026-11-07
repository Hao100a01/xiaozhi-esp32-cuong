#include "uta_media_player.h"
#include "uta_media_policy.h"
#include "uta_media_response.h"
#include "uta_hls.h"
#include "uta_video_player.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <new>
#include <vector>

#include <esp_audio_dec_default.h>
#include <esp_audio_simple_dec.h>
#include <esp_audio_simple_dec_default.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <cJSON.h>
#include <impl/esp_aac_dec.h>
#include <psa/crypto.h>

#include "application.h"
#include "audio_service.h"
#include "board.h"
#include "display.h"
#include "esp_ae_rate_cvt.h"
#include "http.h"
#include "system_info.h"

namespace uta {
namespace {
constexpr const char* kTag = "UTA_Nhac";
constexpr size_t kInputCapacity = 16384;
constexpr size_t kPcmSamples = 8192;

void AddMusicHeaders(Http& http, const std::string& secret) {
    // Tương thích cách ký yêu cầu /stream_pcm của repo Việt Nam.
    std::string mac = SystemInfo::GetMacAddress();
    std::string chip = mac;
    std::erase(chip, ':');
    std::string timestamp = std::to_string(esp_timer_get_time() / 1000000);
    std::string message = mac + ":" + chip + ":" + timestamp + ":" + secret;
    std::array<uint8_t, 32> digest{};
    size_t length = 0;
    if (psa_hash_compute(PSA_ALG_SHA_256, reinterpret_cast<const uint8_t*>(message.data()),
                         message.size(), digest.data(), digest.size(), &length) != PSA_SUCCESS ||
        length != 32) {
        return;
    }
    constexpr char hex[] = "0123456789ABCDEF";
    std::string key;
    for (size_t i = 0; i < 16; ++i) {
        key += hex[digest[i] >> 4];
        key += hex[digest[i] & 15];
    }
    http.SetHeader("X-MAC-Address", mac);
    http.SetHeader("X-Chip-ID", chip);
    http.SetHeader("X-Timestamp", timestamp);
    http.SetHeader("X-Dynamic-Key", key);
}

std::unique_ptr<Http> OpenHttp(std::string url, const Configuration& config, bool music,
                               const std::function<bool()>& cancelled, std::string& error,
                                bool* retryable = nullptr, std::string* final_url = nullptr) {
    for (unsigned redirects = 0; redirects <= 3 && IsHttpUrl(url); ++redirects) {
        if (cancelled())
            return nullptr;
        auto http = Board::GetInstance().GetNetwork()->CreateHttp(0);
        if (!http)
            return nullptr;
        http->SetTimeout(music ? 12000 : 8000);
        http->SetHeader("User-Agent", "ESP32-Music-Player/1.0");
        http->SetHeader("Accept-Encoding", "identity");
        http->SetHeader("Icy-MetaData", "0");
        // Không chuyển thông tin ký yêu cầu sang máy chủ nhạc/đài khác nguồn.
        if (music && UrlOrigin(url) == UrlOrigin(config.music_server)) {
            AddMusicHeaders(*http, config.music_key);
            http->SetHeader("Cache-Control", "no-cache");
        }
        if (!http->Open("GET", url)) {
            if (retryable)
                *retryable = true;
            error = "Không kết nối được nguồn phát; kiểm tra mạng và địa chỉ máy chủ";
            return nullptr;
        }
        auto status = http->GetStatusCode();
        if (status && (*status == 200 || *status == 206)) {
            if (final_url)
                *final_url = url;
            return http;
        }
        const auto location = http->GetResponseHeader("location");
        http->Close();
        if (!status || (*status != 301 && *status != 302 && *status != 303 && *status != 307 &&
                        *status != 308)) {
            if (retryable && !status)
                *retryable = true;
            error = status ? "Nguồn phát trả HTTP " + std::to_string(*status)
                           : "Không đọc được phản hồi HTTP của nguồn phát";
            return nullptr;
        }
        url = ResolveRedirect(url, location);
    }
    error = "Nguồn phát chuyển hướng quá nhiều hoặc trả địa chỉ không hợp lệ";
    return nullptr;
}

class HlsReader {
public:
    HlsReader(const Configuration& config, std::function<bool()> cancelled,
              const std::atomic<bool>& paused, std::string& error, bool& retryable)
        : config_(config), cancelled_(std::move(cancelled)), paused_(paused),
          error_(error), retryable_(retryable) {}

    ~HlsReader() {
        if (segment_)
            segment_->Close();
    }

    bool Open(std::unique_ptr<Http> http, std::string url) {
        // Master VOV trả URL CDN có chữ ký phiên, cần đọc lại khi reconnect.
        for (unsigned depth = 0; depth < 4; ++depth) {
            if (!Load(std::move(http), url))
                return false;
            if (playlist_.variant.empty()) {
                playlist_url_ = url;
                return true;
            }
            url = playlist_.variant;
            http = OpenHttp(url, config_, false, cancelled_, error_, &retryable_, &url);
            if (!http)
                return false;
        }
        error_ = "Playlist radio HLS lồng quá nhiều tầng";
        return false;
    }

    int Read(char* output, size_t capacity) {
        size_t written = 0;
        auto deadline = esp_timer_get_time() + 30000000;
        while (written < capacity && !cancelled_()) {
            if (paused_) {
                if (written)
                    break;
                vTaskDelay(pdMS_TO_TICKS(20));
                deadline = esp_timer_get_time() + 30000000;
                continue;
            }
            if (esp_timer_get_time() > deadline) {
                error_ = "Radio HLS không gửi âm thanh AAC trong 30 giây";
                retryable_ = true;
                return -1;
            }
            if (!payload_.empty()) {
                const auto bytes = std::min(capacity - written, payload_.size());
                std::memcpy(output + written, payload_.data(), bytes);
                payload_ = payload_.subspan(bytes);
                written += bytes;
                continue;
            }
            if (!segment_) {
                if (written)
                    break;
                const int next = NextSegment();
                if (next <= 0)
                    return next;
                if (next == 2) {
                    deadline = esp_timer_get_time() + 30000000;
                    continue;
                }
            }
            auto count = segment_->Read(reinterpret_cast<char*>(packet_.data() + packet_used_),
                                        packet_.size() - packet_used_);
            if (!count || *count < 0 || static_cast<size_t>(*count) > packet_.size() - packet_used_) {
                error_ = "Không đọc được đoạn radio HLS";
                retryable_ = true;
                return -1;
            }
            if (*count == 0) {
                segment_->Close();
                segment_.reset();
                if (packet_used_ || segment_bytes_ == 0) {
                    error_ = "Đoạn radio MPEG-TS kết thúc thiếu dữ liệu";
                    retryable_ = true;
                    return -1;
                }
                continue;
            }
            packet_used_ += *count;
            segment_bytes_ += *count;
            if (segment_bytes_ > 512 * 1024) {
                error_ = "Đoạn radio HLS vượt giới hạn 512 KiB";
                return -1;
            }
            if (packet_used_ == packet_.size()) {
                auto payload = demux_.Packet(packet_);
                if (!payload) {
                    error_ = payload.error();
                    retryable_ = true;
                    return -1;
                }
                payload_ = *payload;
                packet_used_ = 0;
            }
        }
        return cancelled_() ? -1 : static_cast<int>(written);
    }

private:
    const Configuration& config_;
    std::function<bool()> cancelled_;
    const std::atomic<bool>& paused_;
    std::string& error_;
    bool& retryable_;
    std::string playlist_url_;
    HlsPlaylist playlist_;
    std::unique_ptr<Http> segment_;
    HlsTsAudio demux_;
    std::array<uint8_t, 188> packet_{};
    std::span<const uint8_t> payload_;
    size_t packet_used_ = 0, segment_bytes_ = 0;
    uint64_t next_sequence_ = 0;
    bool started_ = false;
    int64_t last_progress_ = esp_timer_get_time();

    bool Load(std::unique_ptr<Http> http, const std::string& url) {
        std::string body;
        std::array<char, 1024> buffer{};
        const auto deadline = esp_timer_get_time() + 15000000;
        while (!cancelled_() && esp_timer_get_time() < deadline) {
            auto count = http->Read(buffer.data(), buffer.size());
            if (!count || *count < 0) {
                http->Close();
                error_ = "Kết nối bị ngắt khi đọc playlist HLS";
                retryable_ = true;
                return false;
            }
            if (static_cast<size_t>(*count) > buffer.size() || body.size() + *count > 65536) {
                http->Close();
                error_ = "Playlist HLS vượt giới hạn dữ liệu";
                return false;
            }
            if (*count == 0) {
                http->Close();
                auto parsed = ParseHlsPlaylist(url, body);
                if (!parsed) {
                    error_ = parsed.error();
                    return false;
                }
                playlist_ = std::move(*parsed);
                return true;
            }
            body.append(buffer.data(), *count);
        }
        http->Close();
        error_ = "Playlist radio HLS phản hồi quá chậm hoặc đã hủy";
        retryable_ = true;
        return false;
    }

    int NextSegment() {
        // Thời hạn tính theo tiến triển, có nhường task và kiểm tra hủy.
        while (!cancelled_()) {
            if (paused_) {
                last_progress_ = esp_timer_get_time();
                return 2;
            }
            for (const auto& item : playlist_.segments) {
                if (started_ && item.sequence < next_sequence_)
                    continue;
                if (started_ && (item.discontinuity || item.sequence != next_sequence_)) {
                    error_ = "Đài HLS đổi mốc âm thanh, cần kết nối lại";
                    retryable_ = true;
                    return -1;
                }
                segment_ = OpenHttp(item.url, config_, false, cancelled_, error_, &retryable_);
                if (!segment_) {
                    // URL phiên/đoạn có thể hết hạn. Run() sẽ mở lại từ master,
                    // vẫn giữ giới hạn hai lần reconnect của radio.
                    retryable_ = true;
                    return -1;
                }
                next_sequence_ = item.sequence + 1;
                started_ = true;
                segment_bytes_ = 0;
                last_progress_ = esp_timer_get_time();
                return 1;
            }
            if (playlist_.ended)
                return 0;
            const auto wait_ms = std::min(playlist_.target_seconds * 500U, 2000U);
            for (unsigned waited = 0; waited < wait_ms && !cancelled_(); waited += 20)
                vTaskDelay(pdMS_TO_TICKS(20));
            std::string url = playlist_url_;
            auto http = OpenHttp(url, config_, false, cancelled_, error_, &retryable_, &url);
            if (!http) {
                // Playlist CDN cũng có thể hết phiên; thử lấy phiên mới từ URL gốc.
                retryable_ = true;
                return -1;
            }
            if (!Load(std::move(http), url))
                return -1;
            playlist_url_ = url;
            if (!playlist_.variant.empty()) {
                error_ = "Playlist radio đổi sang master khi đang phát";
                return -1;
            }
            if (esp_timer_get_time() - last_progress_ > 30000000) {
                error_ = "Đài HLS không có đoạn âm thanh mới trong 30 giây";
                retryable_ = true;
                return -1;
            }
        }
        return -1;
    }
};

struct DecoderSession {
    std::array<uint8_t, kInputCapacity> input{};
    std::array<int16_t, kPcmSamples> decoded{};
    std::array<int16_t, kPcmSamples> mono{};
    std::array<int16_t, kPcmSamples * 3 + 128> converted{};
    esp_audio_simple_dec_handle_t decoder = nullptr;
    esp_ae_rate_cvt_handle_t converter = nullptr;
    esp_aac_dec_cfg_t aac{};
    int source_rate = 0;

    ~DecoderSession() {
        if (converter) {
            esp_ae_rate_cvt_close(converter);
        }
        if (decoder) {
            esp_audio_simple_dec_close(decoder);
        }
    }
};
}  // namespace

MediaPlayer::MediaPlayer(SettingsStore& settings) : settings_(settings) {}

bool MediaPlayer::Start() {
    if (task_) {
        return true;
    }
    if (psa_crypto_init() != PSA_SUCCESS) {
        return false;
    }
    return xTaskCreate([](void* context) { static_cast<MediaPlayer*>(context)->Run(); }, "uta_nhac",
                       8192, this, 2, &task_) == pdPASS;
}

MediaResult MediaPlayer::Submit(bool song, const std::string& value, const std::string& label,
                                bool video) {
    if (!task_)
        return std::unexpected("Tác vụ phát media chưa sẵn sàng");
    Request request;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (preparing_id_)
            return std::unexpected("Đang chuẩn bị một nguồn phát; đợi hoàn tất hoặc dừng trước");
        request = Request{++request_id_, song, value, label, video};
        preparing_id_ = request.id;
        last_request_ = request;
        queued_pcm_us_ = 0;
        duration_seconds_ = 0;
        song_title_.clear();
        spectrum_ = {};
        spectrum_at_ = 0;
        pending_.reset();
        status_ = "Đang chuẩn bị phát";
        playing_ = true;
        paused_ = false;
    }
    Application::GetInstance().PrepareLocalPlayback([this, request](bool ready) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (request.id != request_id_)
            return;
        preparing_id_ = 0;
        if (!ready) {
            playing_ = false;
            status_ = "Chưa phát được khi đang khởi động hoặc cấu hình mạng";
            return;
        }
        // Một yêu cầu chờ duy nhất; mọi I/O chạy trên tác vụ phát nhạc.
        pending_ = request;
        xTaskNotifyGive(task_);
    });
    return {};
}

MediaResult MediaPlayer::PlaySong(const std::string& song, const std::string& artist) {
    if (!ValidMediaText(song) || !ValidMediaText(artist, true))
        return std::unexpected(
            "Tên bài hát phải có nội dung, tối đa 160 byte; ca sĩ tối đa 160 byte");
    if (!ValidMusicServer(settings_.Get().music_server))
        return std::unexpected("Chưa cài đúng URL máy chủ nhạc trên trang điều khiển :8080");
    return Submit(true, TrimMediaText(song), TrimMediaText(artist));
}

MediaResult MediaPlayer::PlayRadio(const std::string& url, const std::string& name) {
    if (!IsHttpUrl(url))
        return std::unexpected(
            "Radio cần URL MP3/AAC hoặc HLS MPEG-TS/AAC qua HTTP/HTTPS, tối đa 2048 byte");
    if (!ValidMediaText(name, true))
        return std::unexpected("Tên đài không hợp lệ hoặc dài quá 160 byte");
    const auto title = TrimMediaText(name);
    if (url == "https://stream.vovmedia.vn/vov-1" ||
        url == "http://stream.vovmedia.vn/vov-1")
        return Submit(false, kVov1HlsUrl, title.empty() ? "VOV 1" : title);
    return Submit(false, url, title.empty() ? "Radio Internet" : title);
}

MediaResult MediaPlayer::PlayVideo(const std::string& url) {
    if (!video_sink_)
        return std::unexpected("Thiết bị chưa có đầu ra video");
    if (!IsHttpUrl(url))
        return std::unexpected("MP4 cần URL tệp trực tiếp qua HTTP/HTTPS, tối đa 2048 byte");
    return Submit(false, url, "Video MP4", true);
}

MediaResult MediaPlayer::Replay() {
    std::optional<Request> request;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        request = last_request_;
    }
    if (!request)
        return std::unexpected("Chưa có nguồn media nào để phát lại");
    if (request->song)
        return PlaySong(request->value, request->label);
    return request->video ? PlayVideo(request->value) : PlayRadio(request->value, request->label);
}

void MediaPlayer::Stop() {
    uint32_t id;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        id = ++request_id_;
        pending_.reset();
        preparing_id_ = 0;
        status_ = "Đã dừng";
        playing_ = false;
        paused_ = false;
    }
    Application::GetInstance().Schedule([this, id]() {
        auto& app = Application::GetInstance();
        if (request_id_.load() == id && app.GetDeviceState() == kDeviceStateIdle) {
            app.GetAudioService().ResetDecoder();
        }
    });
}

MediaResult MediaPlayer::Pause(bool paused) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (Application::GetInstance().GetDeviceState() != kDeviceStateIdle)
        return std::unexpected(
            "Hội thoại đã ngắt nguồn media. Có thể phát lại nguồn cũ từ đầu sau khi kết thúc lệnh");
    if (!playing_ || preparing_id_)
        return std::unexpected(
            "Không có nguồn đang phát sẵn sàng. Nếu hội thoại đã dừng nguồn cũ, hãy phát lại từ "
            "đầu");
    paused_ = paused;
    return {};
}

std::string MediaPlayer::Status() {
    std::lock_guard<std::mutex> lock(mutex_);
    return paused_ && playing_ ? "Đang tạm dừng" : status_;
}

MediaSnapshot MediaPlayer::Snapshot() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string type = "none", title;
    if (last_request_) {
        type = last_request_->video ? "mp4" : last_request_->song ? "music" : "radio";
        if (video_playing_)
            type = "mp4";
        title = last_request_->song ? last_request_->value : last_request_->label;
        if (last_request_->song && !song_title_.empty())
            title = song_title_;
    }
    return {type,
            title,
            paused_ && playing_ ? "Đang tạm dừng" : status_,
            playing_.load(),
            preparing_id_ != 0,
            paused_.load(),
            last_request_.has_value(),
            last_request_ && last_request_->song ? last_request_->label : "",
            static_cast<int>(queued_pcm_us_ / 1000000),
            duration_seconds_};
}

Spectrum MediaPlayer::GetSpectrum() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!playing_ || paused_ || esp_timer_get_time() - spectrum_at_ > 350000)
        return {};
    return spectrum_;
}

bool MediaPlayer::Cancelled(uint32_t id) {
    return id != request_id_.load() ||
           Application::GetInstance().GetDeviceState() != kDeviceStateIdle;
}

void MediaPlayer::SetStatus(uint32_t id, const std::string& text) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (id == request_id_) {
        status_ = text;
    }
}

std::expected<std::string, std::string> MediaPlayer::SearchSong(const Request& request,
                                                                const Configuration& config,
                                                                int& duration_seconds,
                                                                std::string& title) {
    if (!ValidMusicServer(config.music_server))
        return std::unexpected("Chưa cài đúng địa chỉ máy chủ nhạc trên trang điều khiển");
    std::string base = config.music_server;
    while (base.ends_with('/')) {
        base.pop_back();
    }
    const std::string url = base + "/stream_pcm?song=" + UrlEncode(request.value) +
                            "&artist=" + UrlEncode(request.label);
    std::string error;
    auto http =
        OpenHttp(url, config, true, [this, id = request.id]() { return Cancelled(id); }, error);
    if (!http)
        return std::unexpected(error.empty() ? "Không mở được dịch vụ tìm nhạc" : error);
    std::array<char, 1024> buffer{};
    std::string response;
    const auto deadline = esp_timer_get_time() + 15000000;
    bool complete = false;
    while (!Cancelled(request.id) && esp_timer_get_time() < deadline) {
        auto size = http->Read(buffer.data(), buffer.size());
        if (!size || *size < 0 || static_cast<size_t>(*size) > buffer.size() ||
            response.size() + *size > 8192) {
            http->Close();
            return std::unexpected("Phản hồi tìm nhạc bị lỗi hoặc vượt quá 8192 byte");
        }
        if (*size == 0) {
            complete = true;
            break;
        }
        response.append(buffer.data(), *size);
    }
    http->Close();
    if (Cancelled(request.id)) {
        return std::unexpected("Đã hủy tìm nhạc để ưu tiên hội thoại hoặc lệnh mới");
    }
    if (!complete)
        return std::unexpected("Máy chủ tìm nhạc phản hồi quá chậm");
    return ParseSongResponse(base, response, &duration_seconds, &title);
}

bool MediaPlayer::Stream(const std::string& url, uint32_t id, bool radio, std::string& error,
                         bool& retryable, bool* delivered) {
    retryable = false;
    auto& audio = Application::GetInstance().GetAudioService();
    const uint32_t generation = audio.GetPlaybackGeneration();
    const int output_rate = Board::GetInstance().GetAudioCodec()->output_sample_rate();
    const auto config = settings_.Get();
    std::string final_url;
    auto http = OpenHttp(
        url, config, !radio, [this, id]() { return Cancelled(id); }, error, &retryable, &final_url);
    if (!http)
        return false;
    auto content_type = http->GetResponseHeader("content-type");
    std::transform(content_type.begin(), content_type.end(), content_type.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const auto path = final_url.substr(0, final_url.find_first_of("?#"));
    const bool hls_source = radio &&
                            (content_type.find("mpegurl") != std::string::npos ||
                             path.ends_with(".m3u8"));
    if (content_type.find("text/html") != std::string::npos ||
        (!hls_source && content_type.find("mpegurl") != std::string::npos) ||
        content_type.find("scpls") != std::string::npos) {
        http->Close();
        error = "Nguồn là trang web hoặc playlist; cần luồng MP3/AAC trực tiếp";
        return false;
    }
    if (output_rate < 8000 || output_rate > 48000) {
        http->Close();
        error = "Tần số đầu ra âm thanh không được hỗ trợ";
        return false;
    }
    void* memory = heap_caps_malloc(sizeof(DecoderSession), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!memory) {
        http->Close();
        error = "Không đủ bộ nhớ để giải mã nhạc/radio";
        return false;
    }
    const auto destroy = [](DecoderSession* value) {
        value->~DecoderSession();
        heap_caps_free(value);
    };
    std::unique_ptr<DecoderSession, decltype(destroy)> session(new (memory) DecoderSession,
                                                               destroy);
    std::unique_ptr<HlsReader> hls;
    if (hls_source) {
        hls = std::make_unique<HlsReader>(
            config, [this, id]() { return Cancelled(id); }, paused_, error, retryable);
        if (!hls->Open(std::move(http), final_url))
            return false;
        ESP_LOGI(kTag, "Radio HLS: đọc playlist và tách AAC từ MPEG-TS");
    }
    size_t used = 0;
    bool eof = false;
    bool need_input = true;
    bool played = false;
    unsigned no_progress = 0;
    while (!Cancelled(id) && audio.GetPlaybackGeneration() == generation) {
        if (paused_) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        if (!eof && (need_input || used < 4096)) {
            if (used == session->input.size()) {
                break;
            }
            auto* buffer = reinterpret_cast<char*>(session->input.data() + used);
            const auto capacity = std::min<size_t>(4096, session->input.size() - used);
            int count;
            if (hls) {
                count = hls->Read(buffer, capacity);
            } else {
                auto received = http->Read(buffer, capacity);
                count = received ? *received : -1;
            }
            if (count < 0 || static_cast<size_t>(count) > capacity) {
                if (!hls) {
                    error = used || played ? "Luồng âm thanh bị ngắt khi đang đọc dữ liệu"
                                          : "Nguồn phát không gửi dữ liệu âm thanh trong thời gian chờ";
                    retryable = true;
                }
                break;
            }
            eof = count == 0;
            if (eof && radio && !hls) {
                error = "Luồng radio trực tiếp đã đóng kết nối";
                retryable = true;
            }
            used += count;
            need_input = false;
        }
        if (used == 0 && eof) {
            if (http)
                http->Close();
            return played && (!radio || hls);
        }
        // Nạp trước dữ liệu có giới hạn để tránh hụt tiếng ngay sau khung đầu tiên.
        if (!session->decoder && !eof && used < 8192) {
            need_input = true;
            continue;
        }
        if (!session->decoder) {
            if (used < 4) {
                if (eof) {
                    break;
                }
                need_input = true;
                continue;
            }
            // AAC ADTS phải được nhận trước MP3 vì cả hai cùng bắt đầu bằng 0xff.
            const bool adts = session->input[0] == 0xff && (session->input[1] & 0xf6) == 0xf0;
            const bool mp3 = std::memcmp(session->input.data(), "ID3", 3) == 0 ||
                             (session->input[0] == 0xff && !adts);
            esp_audio_simple_dec_cfg_t config{};
            config.dec_type = adts || (radio && !mp3 && !url.ends_with(".mp3"))
                                  ? ESP_AUDIO_SIMPLE_DEC_TYPE_AAC
                                  : ESP_AUDIO_SIMPLE_DEC_TYPE_MP3;
            if (config.dec_type == ESP_AUDIO_SIMPLE_DEC_TYPE_AAC) {
                session->aac.bits_per_sample = 16;
                session->aac.aac_plus_enable = true;
                config.dec_cfg = &session->aac;
                config.cfg_size = sizeof(session->aac);
            }
            if (esp_audio_simple_dec_open(&config, &session->decoder) != ESP_AUDIO_ERR_OK) {
                break;
            }
        }
        esp_audio_simple_dec_raw_t raw{};
        raw.buffer = session->input.data();
        raw.len = used;
        raw.eos = eof;
        raw.frame_recover = ESP_AUDIO_SIMPLE_DEC_RECOVERY_NONE;
        esp_audio_simple_dec_out_t out{};
        out.buffer = reinterpret_cast<uint8_t*>(session->decoded.data());
        out.len = session->decoded.size() * sizeof(int16_t);
        const auto result = esp_audio_simple_dec_process(session->decoder, &raw, &out);
        if (raw.consumed > used || out.decoded_size > out.len) {
            break;
        }
        used -= raw.consumed;
        std::memmove(session->input.data(), session->input.data() + raw.consumed, used);
        if (result == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
            break;
        }
        if (result == ESP_AUDIO_ERR_DATA_LACK) {
            if (eof) {
                break;
            }
            need_input = true;
            continue;
        }
        if (result != ESP_AUDIO_ERR_OK && result != ESP_AUDIO_ERR_CONTINUE) {
            break;
        }
        if (raw.consumed == 0 && out.decoded_size == 0) {
            if (++no_progress > 8) {
                break;
            }
            need_input = true;
            continue;
        }
        no_progress = 0;
        if (out.decoded_size == 0) {
            continue;
        }
        esp_audio_simple_dec_info_t info{};
        if (esp_audio_simple_dec_get_info(session->decoder, &info) != ESP_AUDIO_ERR_OK ||
            info.bits_per_sample != 16 || info.channel < 1 || info.channel > 2 ||
            info.sample_rate < 8000 || info.sample_rate > 48000 ||
            out.decoded_size % (sizeof(int16_t) * info.channel) != 0) {
            break;
        }
        const size_t frames = out.decoded_size / sizeof(int16_t) / info.channel;
        for (size_t i = 0; i < frames; ++i) {
            session->mono[i] = info.channel == 1 ? session->decoded[i]
                                                 : (static_cast<int32_t>(session->decoded[2 * i]) +
                                                    session->decoded[2 * i + 1]) /
                                                       2;
        }
        const int16_t* pcm = session->mono.data();
        uint32_t samples = frames;
        if (info.sample_rate != output_rate) {
            if (session->source_rate != info.sample_rate) {
                if (session->converter) {
                    esp_ae_rate_cvt_close(session->converter);
                    session->converter = nullptr;
                }
                esp_ae_rate_cvt_cfg_t config{};
                config.src_rate = info.sample_rate;
                config.dest_rate = output_rate;
                config.channel = 1;
                config.bits_per_sample = 16;
                config.complexity = 2;
                config.perf_type = ESP_AE_RATE_CVT_PERF_TYPE_SPEED;
                if (esp_ae_rate_cvt_open(&config, &session->converter) != ESP_AE_ERR_OK) {
                    break;
                }
                session->source_rate = info.sample_rate;
            }
            uint32_t capacity = 0;
            esp_ae_rate_cvt_get_max_out_sample_num(session->converter, frames, &capacity);
            if (capacity > session->converted.size()) {
                break;
            }
            samples = capacity;
            if (esp_ae_rate_cvt_process(
                    session->converter, reinterpret_cast<esp_ae_sample_t>(session->mono.data()),
                    frames, reinterpret_cast<esp_ae_sample_t>(session->converted.data()),
                    &samples) != ESP_AE_ERR_OK) {
                break;
            }
            pcm = session->converted.data();
        }
        if (!samples)
            continue;
        // Giới hạn cập nhật phổ ở 10 lần/giây; không lấy dữ liệu từ mic.
        const int64_t now = esp_timer_get_time();
        if (now - spectrum_at_ >= 100000) {
            const auto values = AnalyzeSpectrum(pcm, samples, output_rate);
            std::lock_guard<std::mutex> lock(mutex_);
            if (id == request_id_) {
                spectrum_ = values;
                spectrum_at_ = now;
            }
        }
        if (!PushPcm(pcm, samples, output_rate, id, generation, true)) {
            error = "Đầu ra âm thanh bị hủy hoặc không nhận thêm dữ liệu";
            break;
        }
        played = true;
        if (delivered)
            *delivered = true;
    }
    if (http)
        http->Close();
    if (error.empty())
        error = "Không giải mã được nguồn MP3/AAC; kiểm tra định dạng và luồng trực tiếp";
    return false;
}

bool MediaPlayer::PushPcm(const int16_t* pcm, size_t count, int rate, uint32_t id,
                          uint32_t generation, bool pause_between_blocks) {
    if (!pcm || rate < 8000 || rate > 48000)
        return false;
    auto& audio = Application::GetInstance().GetAudioService();
    size_t offset = 0;
    while (offset < count && !Cancelled(id)) {
        const size_t chunk = std::min<size_t>(rate / 50, count - offset);
        std::vector<int16_t> block(pcm + offset, pcm + offset + chunk);
        int64_t deadline = esp_timer_get_time() + 2000000;
        for (;;) {
            if (Cancelled(id) || audio.GetPlaybackGeneration() != generation)
                return false;
            if (pause_between_blocks && paused_) {
                vTaskDelay(pdMS_TO_TICKS(20));
                deadline = esp_timer_get_time() + 2000000;
                continue;
            }
            // Push chỉ move dữ liệu khi thành công; giữ cùng buffer khi queue đầy.
            if (audio.PushPcmToPlaybackQueue(std::move(block), generation))
                break;
            if (esp_timer_get_time() >= deadline)
                return false;
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        offset += chunk;
    }
    if (pause_between_blocks && offset > 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (id == request_id_)
            queued_pcm_us_ += static_cast<int64_t>(offset) * 1000000 / rate;
    }
    return offset == count && !Cancelled(id);
}

void MediaPlayer::Run() {
    // Đăng ký một lần; không gỡ decoder dùng chung khi chuyển bài.
    esp_audio_dec_register_default();
    esp_audio_simple_dec_register_default();
    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(200));
        std::optional<Request> request;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            request.swap(pending_);
        }
        if (!request) {
            continue;
        }
        if (Cancelled(request->id)) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (request->id == request_id_) {
                playing_ = false;
                paused_ = false;
                status_ = "Đã dừng để ưu tiên hội thoại";
            }
            continue;
        }
        const auto config = settings_.Get();
        std::string url = request->value;
        bool ok = false;
        std::string movie_error;
        if (request->song) {
            int duration = 0;
            std::string title;
            auto found = SearchSong(*request, config, duration, title);
            if (found) {
                url = std::move(*found);
                std::lock_guard<std::mutex> lock(mutex_);
                if (request->id == request_id_) {
                    duration_seconds_ = duration;
                    song_title_ = std::move(title);
                }
            } else {
                url.clear();
                movie_error = found.error();
            }
        }
        if (!url.empty() && !Cancelled(request->id)) {
            SetStatus(request->id,
                      "Đang phát: " + (request->song ? request->value : request->label));
            if (request->video || IsMp4Url(url)) {
                if (!video_sink_) {
                    movie_error = "Nguồn nhạc trả MP4 nhưng thiết bị chưa có đầu ra video";
                } else {
                    auto& audio = Application::GetInstance().GetAudioService();
                    const auto generation = audio.GetPlaybackGeneration();
                    VideoCallbacks callbacks;
                    callbacks.output_rate =
                        Board::GetInstance().GetAudioCodec()->output_sample_rate();
                    callbacks.cancelled = [this, id = request->id, generation, &audio]() {
                        return Cancelled(id) || audio.GetPlaybackGeneration() != generation;
                    };
                    callbacks.paused = [this]() { return paused_.load(); };
                    callbacks.video = [this, id = request->id](const uint16_t* pixels, int width,
                                                               int height) {
                        if (!Cancelled(id))
                            video_sink_(pixels, width, height);
                    };
                    callbacks.error = [&movie_error](const std::string& text) {
                        movie_error = text;
                    };
                    callbacks.audio = [this, id = request->id, generation,
                                       rate = callbacks.output_rate](const int16_t* pcm,
                                                                     size_t count) {
                        // Hoàn tất khung hiện tại; bộ phát MP4 dời PTS khi tạm dừng.
                        return PushPcm(pcm, count, rate, id, generation, false);
                    };
                    video_playing_ = true;
                    ok = PlayMp4(url, callbacks);
                    video_playing_ = false;
                    video_sink_(nullptr, 0, 0);
                }
            } else {
                bool retryable = false;
                bool delivered = false;
                ok = Stream(url, request->id, !request->song, movie_error, retryable, &delivered);
                if (request->song && !ok && !delivered && !Cancelled(request->id)) {
                    const auto legacy = LegacyMusicProxy(url, config.music_server);
                    if (!legacy.empty()) {
                        ESP_LOGI(kTag, "Thử đường dẫn proxy nhạc tương thích bản cũ");
                        ok = Stream(legacy, request->id, false, movie_error, retryable);
                    }
                }
                // Radio thử lại tối đa hai lần khi mất kết nối hoặc cần phiên HLS mới.
                for (unsigned retry = 0;
                     !request->song && !ok && retryable && retry < 2 && !Cancelled(request->id);
                     ++retry) {
                    SetStatus(request->id, "Đang kết nối lại đài Internet");
                    vTaskDelay(pdMS_TO_TICKS(250 * (retry + 1)));
                    movie_error.clear();
                    ok = Stream(url, request->id, true, movie_error, retryable);
                }
            }
        }
        // Đợi đuôi PCM đã xếp hàng, không báo phát xong trước khi loa nhận hết.
        if (ok) {
            auto& audio = Application::GetInstance().GetAudioService();
            const auto deadline = esp_timer_get_time() + 2000000;
            while (!Cancelled(request->id) && !audio.IsPlaybackIdle() &&
                   esp_timer_get_time() < deadline)
                vTaskDelay(pdMS_TO_TICKS(10));
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (request->id == request_id_) {
                playing_ = false;
                paused_ = false;
                status_ = Cancelled(request->id) ? "Đã dừng để ưu tiên hội thoại"
                          : ok                   ? "Đã phát xong"
                          : movie_error.empty()  ? "Không phát được; kiểm tra mạng và máy chủ"
                                                 : movie_error;
            }
        }
        if (!ok && !Cancelled(request->id) && !movie_error.empty())
            ESP_LOGW(kTag, "%s", movie_error.c_str());
        ESP_LOGI(kTag, "Đã kết thúc lượt phát");
    }
}

}  // namespace uta
