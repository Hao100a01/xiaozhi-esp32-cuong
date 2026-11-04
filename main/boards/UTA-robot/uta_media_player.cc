#include "uta_media_player.h"
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

using JsonPtr = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;

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

std::unique_ptr<Http> OpenHttp(std::string url, const Configuration& config, bool music) {
    for (unsigned redirects = 0; redirects <= 3 && IsHttpUrl(url); ++redirects) {
        auto http = Board::GetInstance().GetNetwork()->CreateHttp(0);
        if (!http)
            return nullptr;
        http->SetTimeout(4000);
        http->SetHeader("Accept-Encoding", "identity");
        http->SetHeader("Icy-MetaData", "0");
        // Không chuyển thông tin ký yêu cầu sang máy chủ nhạc/đài khác nguồn.
        if (music && UrlOrigin(url) == UrlOrigin(config.music_server)) {
            AddMusicHeaders(*http, config.music_key);
        }
        if (!http->Open("GET", url))
            return nullptr;
        auto status = http->GetStatusCode();
        if (status && *status >= 200 && *status < 300)
            return http;
        const auto location = http->GetResponseHeader("location");
        http->Close();
        if (!status || (*status != 301 && *status != 302 && *status != 303 && *status != 307 &&
                        *status != 308))
            return nullptr;
        url = ResolveRedirect(url, location);
    }
    return nullptr;
}

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

bool MediaPlayer::Submit(bool song, const std::string& value, const std::string& label,
                         bool video) {
    if (!task_ || value.empty() || value.size() > 512 || label.size() > 160) {
        return false;
    }
    if (preparing_.exchange(true)) {
        return false;
    }
    Request request;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        request = Request{++request_id_, song, value, label, video};
        pending_.reset();
        status_ = "Đang chuẩn bị phát";
        playing_ = true;
        paused_ = false;
    }
    Application::GetInstance().PrepareLocalPlayback([this, request](bool ready) {
        std::lock_guard<std::mutex> lock(mutex_);
        preparing_ = false;
        if (request.id != request_id_)
            return;
        if (!ready) {
            playing_ = false;
            status_ = "Chưa phát được khi đang khởi động hoặc cấu hình mạng";
            return;
        }
        // Một yêu cầu chờ duy nhất; mọi I/O chạy trên tác vụ phát nhạc.
        pending_ = request;
        xTaskNotifyGive(task_);
    });
    return true;
}

bool MediaPlayer::PlaySong(const std::string& song, const std::string& artist) {
    return song.size() <= 160 && Submit(true, song, artist);
}

bool MediaPlayer::PlayRadio(const std::string& url, const std::string& name) {
    return IsHttpUrl(url) && Submit(false, url, name);
}

bool MediaPlayer::PlayVideo(const std::string& url) {
    return video_sink_ && IsHttpUrl(url) && Submit(false, url, "Video MP4", true);
}

void MediaPlayer::Stop() {
    uint32_t id;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        id = ++request_id_;
        pending_.reset();
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

void MediaPlayer::Pause(bool paused) { paused_ = paused; }

std::string MediaPlayer::Status() {
    std::lock_guard<std::mutex> lock(mutex_);
    return paused_ && playing_ ? "Đang tạm dừng" : status_;
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

std::string MediaPlayer::SearchSong(const Request& request, const Configuration& config) {
    if (!IsHttpUrl(config.music_server)) {
        SetStatus(request.id, "Chưa cài địa chỉ máy chủ nhạc trên trang điều khiển");
        return {};
    }
    std::string base = config.music_server;
    while (base.ends_with('/')) {
        base.pop_back();
    }
    const std::string url = base + "/stream_pcm?song=" + UrlEncode(request.value) +
                            "&artist=" + UrlEncode(request.label);
    auto http = OpenHttp(url, config, true);
    if (!http)
        return {};
    std::array<char, 1024> buffer{};
    std::string response;
    while (!Cancelled(request.id)) {
        auto size = http->Read(buffer.data(), buffer.size());
        if (!size || *size < 0 || static_cast<size_t>(*size) > buffer.size() ||
            response.size() + *size > 8192) {
            http->Close();
            return {};
        }
        if (*size == 0) {
            break;
        }
        response.append(buffer.data(), *size);
    }
    http->Close();
    if (Cancelled(request.id)) {
        return {};
    }
    JsonPtr root(cJSON_Parse(response.c_str()), cJSON_Delete);
    const auto* audio = cJSON_GetObjectItemCaseSensitive(root.get(), "audio_url");
    if (!cJSON_IsString(audio) || !audio->valuestring) {
        return {};
    }
    return ResolveUrl(base, audio->valuestring);
}

bool MediaPlayer::Stream(const std::string& url, uint32_t id, bool radio) {
    auto& audio = Application::GetInstance().GetAudioService();
    const uint32_t generation = audio.GetPlaybackGeneration();
    const int output_rate = Board::GetInstance().GetAudioCodec()->output_sample_rate();
    auto http = OpenHttp(url, settings_.Get(), !radio);
    if (!http)
        return false;
    void* memory = heap_caps_malloc(sizeof(DecoderSession), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!memory) {
        http->Close();
        return false;
    }
    const auto destroy = [](DecoderSession* value) {
        value->~DecoderSession();
        heap_caps_free(value);
    };
    std::unique_ptr<DecoderSession, decltype(destroy)> session(new (memory) DecoderSession,
                                                               destroy);
    size_t used = 0;
    bool eof = false;
    bool need_input = true;
    bool played = false;
    unsigned no_progress = 0;
    while (!Cancelled(id)) {
        if (paused_) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        if (!eof && (need_input || used < 4096)) {
            if (used == session->input.size()) {
                break;
            }
            auto count = http->Read(reinterpret_cast<char*>(session->input.data() + used),
                                    std::min<size_t>(4096, session->input.size() - used));
            if (!count || *count < 0 ||
                static_cast<size_t>(*count) >
                    std::min<size_t>(4096, session->input.size() - used)) {
                break;
            }
            eof = *count == 0;
            used += *count;
            need_input = false;
        }
        if (used == 0 && eof) {
            http->Close();
            return played;
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
        size_t offset = 0;
        while (offset < samples && !Cancelled(id)) {
            const size_t chunk = std::min<size_t>(output_rate / 50, samples - offset);
            std::vector<int16_t> block(pcm + offset, pcm + offset + chunk);
            if (!audio.PushPcmToPlaybackQueue(std::move(block), generation)) {
                if (audio.GetPlaybackGeneration() != generation) {
                    http->Close();
                    return false;
                }
                continue;
            }
            offset += chunk;
            played = true;
        }
    }
    http->Close();
    return false;
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
        std::string url = request->song ? SearchSong(*request, config) : request->value;
        bool ok = false;
        std::string movie_error;
        if (!url.empty() && !Cancelled(request->id)) {
            SetStatus(request->id,
                      "Đang phát: " + (request->song ? request->value : request->label));
            if (request->video || IsMp4Url(url)) {
                auto& audio = Application::GetInstance().GetAudioService();
                const auto generation = audio.GetPlaybackGeneration();
                VideoCallbacks callbacks;
                callbacks.output_rate = Board::GetInstance().GetAudioCodec()->output_sample_rate();
                callbacks.cancelled = [this, id = request->id, generation, &audio]() {
                    return Cancelled(id) || audio.GetPlaybackGeneration() != generation;
                };
                callbacks.paused = [this]() { return paused_.load(); };
                callbacks.video = video_sink_;
                callbacks.error = [&movie_error](const std::string& text) { movie_error = text; };
                callbacks.audio = [this, id = request->id, generation, &audio,
                                   rate = callbacks.output_rate](const int16_t* pcm, size_t count) {
                    size_t offset = 0;
                    while (offset < count && !Cancelled(id)) {
                        // Hoàn tất khung hiện tại; bộ phát video xử lý tạm dừng và dời mốc PTS.
                        const size_t chunk = std::min<size_t>(rate / 50, count - offset);
                        std::vector<int16_t> block(pcm + offset, pcm + offset + chunk);
                        if (audio.PushPcmToPlaybackQueue(std::move(block), generation))
                            offset += chunk;
                        else if (audio.GetPlaybackGeneration() != generation)
                            return false;
                    }
                    return !Cancelled(id);
                };
                video_playing_ = true;
                ok = PlayMp4(url, callbacks);
                video_playing_ = false;
                video_sink_(nullptr, 0, 0);
            } else {
                ok = Stream(url, request->id, !request->song);
            }
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
        ESP_LOGI(kTag, "Đã kết thúc lượt phát");
    }
}

}  // namespace uta
