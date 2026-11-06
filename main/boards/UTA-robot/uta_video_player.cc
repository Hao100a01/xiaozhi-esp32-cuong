#include "uta_video_player.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstring>
#include <memory>
#include <new>

#include <esp_audio_dec.h>
#include <esp_extractor.h>
#include <esp_extractor_defaults.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <esp_video_dec.h>
#include <esp_video_dec_default.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <impl/esp_aac_dec.h>

#include "board.h"
#include "esp_ae_rate_cvt.h"
#include "http.h"
#include "uta_h264.h"
#include "uta_helpers.h"
#include "uta_media_policy.h"

namespace uta {
namespace {
constexpr size_t kFrameCapacity = 256 * 256;

// HTTP có thể tìm tới vị trí bất kỳ để đọc bảng chỉ mục MP4, không tải cả tệp.
class MovieInput {
public:
    MovieInput(std::string url, const VideoCallbacks& callbacks)
        : url_(std::move(url)), callbacks_(callbacks) {}
    ~MovieInput() {
        if (http_)
            http_->Close();
    }

    bool Seek(uint32_t position) {
        if (open_ && http_ && position == position_ && position <= range_end_)
            return true;
        open_ = false;
        if (http_)
            http_->Close();
        for (unsigned redirects = 0; redirects < 4 && IsHttpUrl(url_); ++redirects) {
            if (callbacks_.cancelled())
                return false;
            http_ = Board::GetInstance().GetNetwork()->CreateHttp(0);
            if (!http_)
                return false;
            http_->SetTimeout(4000);
            http_->SetHeader("Accept-Encoding", "identity");
            http_->SetHeader("Range", "bytes=" + std::to_string(position) + "-");
            if (!http_->Open("GET", url_))
                return false;
            const auto code = http_->GetStatusCode();
            if (!code)
                return false;
            if (*code == 301 || *code == 302 || *code == 303 || *code == 307 || *code == 308) {
                url_ = ResolveRedirect(url_, http_->GetResponseHeader("location"));
                http_->Close();
                continue;
            }
            if (*code == 206) {
                HttpRange range;
                if (!ParseMovieRange(http_->GetResponseHeader("content-range"), position, size_,
                                     http_->GetBodyLength(), range))
                    return false;
                size_ = range.total;
                range_end_ = range.last;
            } else if (*code == 200 && position == 0) {
                const auto size = http_->GetBodyLength();
                if (!size || size > UINT32_MAX || (size_ && size_ != size))
                    return false;
                size_ = size;
                range_end_ = size_ - 1;
            } else
                return false;
            position_ = position;
            open_ = true;
            return true;
        }
        return false;
    }
    int Read(void* destination, uint32_t size) {
        if (!open_ || !http_ || callbacks_.cancelled() || position_ > size_)
            return -1;
        size = std::min(size, size_ - position_);
        if (!size)
            return 0;
        if (position_ > range_end_ && !Seek(position_))
            return -1;
        size = std::min(size, range_end_ - position_ + 1);
        auto count = http_->Read(static_cast<char*>(destination), size);
        if (!count || *count <= 0 || static_cast<uint32_t>(*count) > size)
            return -1;
        position_ += *count;
        return *count;
    }
    uint32_t Size() const { return size_; }

private:
    std::string url_;
    const VideoCallbacks& callbacks_;
    std::unique_ptr<Http> http_;
    uint32_t position_ = 0, size_ = 0;
    uint32_t range_end_ = 0;
    bool open_ = false;
};

struct MovieSession {
    esp_extractor_handle_t extractor = nullptr;
    esp_video_dec_handle_t video = nullptr;
    esp_audio_dec_handle_t audio = nullptr;
    esp_ae_rate_cvt_handle_t converter = nullptr;
    // Cấp phát một lần trong PSRAM, giới hạn kích thước đầu vào/đầu ra.
    alignas(32) std::array<uint8_t, kFrameCapacity * 3 / 2> decoded_video{};
    alignas(32) std::array<uint16_t, 240 * 240> rgb{};
    alignas(32) std::array<uint8_t, 128 * 1024> encoded_video{};
    std::array<int16_t, 8192> decoded_audio{};
    std::array<int16_t, 8192> mono{};
    std::array<int16_t, 24576 + 128> resampled{};
    int source_rate = 0;
    ~MovieSession() {
        if (converter)
            esp_ae_rate_cvt_close(converter);
        if (audio)
            esp_audio_dec_close(audio);
        if (video)
            esp_video_dec_close(video);
        if (extractor)
            esp_extractor_close(extractor);
    }
};

// Chuyển YUV420 của bộ giải mã H.264 sang RGB565 cho LVGL.
bool ConvertVideo(MovieSession& session, const esp_video_dec_out_frame_t& output, bool h264,
                  int width, int height, int stride, int plane_height) {
    if (width <= 0 || height <= 0 || width > 240 || height > 240 || stride < width ||
        plane_height < height || stride > 256 || plane_height > 256)
        return false;
    if (!h264) {
        if (output.decoded_size < static_cast<size_t>(stride * height * 2))
            return false;
        for (int y = 0; y < height; ++y)
            std::memcpy(session.rgb.data() + y * width, output.data + y * stride * 2, width * 2);
        return true;
    }
    if ((stride & 1) || (plane_height & 1))
        return false;
    const size_t plane = stride * plane_height;
    if (output.decoded_size < plane * 3 / 2)
        return false;
    const auto* u = output.data + plane;
    const auto* v = u + plane / 4;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int luminance = std::max(0, output.data[y * stride + x] - 16);
            const int cb = u[(y / 2) * (stride / 2) + x / 2] - 128;
            const int cr = v[(y / 2) * (stride / 2) + x / 2] - 128;
            const int red = std::clamp((298 * luminance + 409 * cr + 128) >> 8, 0, 255);
            const int green =
                std::clamp((298 * luminance - 100 * cb - 208 * cr + 128) >> 8, 0, 255);
            const int blue = std::clamp((298 * luminance + 516 * cb + 128) >> 8, 0, 255);
            session.rgb[y * width + x] = ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3);
        }
    }
    return true;
}

bool DecodeAudio(MovieSession& session, const esp_extractor_frame_info_t& frame,
                 const VideoCallbacks& callbacks, bool& emitted) {
    esp_audio_dec_in_raw_t input{};
    input.buffer = frame.frame_buffer;
    input.len = frame.frame_size;
    while (input.len && !callbacks.cancelled()) {
        esp_audio_dec_out_frame_t output{};
        output.buffer = reinterpret_cast<uint8_t*>(session.decoded_audio.data());
        output.len = session.decoded_audio.size() * sizeof(int16_t);
        input.consumed = 0;
        if (esp_audio_dec_process(session.audio, &input, &output) != ESP_AUDIO_ERR_OK ||
            input.consumed > input.len || !input.consumed || output.decoded_size > output.len)
            return false;
        input.buffer += input.consumed;
        input.len -= input.consumed;
        if (!output.decoded_size)
            continue;
        esp_audio_dec_info_t info{};
        if (esp_audio_dec_get_info(session.audio, &info) != ESP_AUDIO_ERR_OK ||
            info.bits_per_sample != 16 || info.channel < 1 || info.channel > 2 ||
            info.sample_rate < 8000 || info.sample_rate > 48000 ||
            output.decoded_size % (sizeof(int16_t) * info.channel))
            return false;
        uint32_t samples = output.decoded_size / sizeof(int16_t) / info.channel;
        for (size_t i = 0; i < samples; ++i)
            session.mono[i] = info.channel == 1
                                  ? session.decoded_audio[i]
                                  : (static_cast<int32_t>(session.decoded_audio[2 * i]) +
                                     session.decoded_audio[2 * i + 1]) /
                                        2;
        const int16_t* pcm = session.mono.data();
        if (static_cast<int>(info.sample_rate) != callbacks.output_rate) {
            if (session.source_rate != static_cast<int>(info.sample_rate)) {
                if (session.converter)
                    esp_ae_rate_cvt_close(session.converter);
                session.converter = nullptr;
                esp_ae_rate_cvt_cfg_t config{};
                config.src_rate = info.sample_rate;
                config.dest_rate = callbacks.output_rate;
                config.channel = 1;
                config.bits_per_sample = 16;
                config.complexity = 2;
                config.perf_type = ESP_AE_RATE_CVT_PERF_TYPE_SPEED;
                if (esp_ae_rate_cvt_open(&config, &session.converter) != ESP_AE_ERR_OK)
                    return false;
                session.source_rate = info.sample_rate;
            }
            uint32_t capacity = 0;
            esp_ae_rate_cvt_get_max_out_sample_num(session.converter, samples, &capacity);
            if (capacity > session.resampled.size())
                return false;
            const uint32_t source_samples = samples;
            samples = capacity;
            if (esp_ae_rate_cvt_process(
                    session.converter, reinterpret_cast<esp_ae_sample_t>(session.mono.data()),
                    source_samples, reinterpret_cast<esp_ae_sample_t>(session.resampled.data()),
                    &samples) != ESP_AE_ERR_OK)
                return false;
            pcm = session.resampled.data();
        }
        if (!callbacks.audio(pcm, samples))
            return false;
        emitted = true;
    }
    return !callbacks.cancelled();
}
}  // namespace

bool PlayMp4(const std::string& url, const VideoCallbacks& callbacks) {
    if (!IsHttpUrl(url) || !callbacks.cancelled || !callbacks.paused || !callbacks.audio ||
        !callbacks.video || !callbacks.error || callbacks.output_rate < 8000 ||
        callbacks.output_rate > 48000)
        return false;
    MovieInput input(url, callbacks);
    if (!input.Seek(0)) {
        callbacks.error(
            "Không mở được MP4; cần URL trực tiếp, kích thước tệp và HTTP tải từng phần");
        return false;
    }
    auto* memory =
        heap_caps_aligned_alloc(32, sizeof(MovieSession), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!memory) {
        callbacks.error("Không đủ bộ nhớ cho video");
        return false;
    }
    const auto destroy = [](MovieSession* p) {
        p->~MovieSession();
        heap_caps_free(p);
    };
    std::unique_ptr<MovieSession, decltype(destroy)> session(new (memory) MovieSession, destroy);
    // Đăng ký toàn cục một lần, không lặp lại khi chuyển video.
    static const bool registered = esp_extractor_register_default() == ESP_EXTRACTOR_ERR_OK &&
                                   esp_video_dec_register_default() == ESP_VC_ERR_OK;
    if (!registered) {
        callbacks.error("Không đăng ký được bộ giải mã MP4");
        return false;
    }
    esp_extractor_config_t extract{};
    extract.type = ESP_EXTRACTOR_TYPE_MP4;
    extract.extract_mask = ESP_EXTRACT_MASK_AV;
    extract.in_ctx = &input;
    extract.out_pool_size = 256 * 1024;
    extract.out_align = 32;
    extract.in_read_cb = [](void* data, uint32_t size, void* ctx) {
        return static_cast<MovieInput*>(ctx)->Read(data, size);
    };
    extract.in_seek_cb = [](uint32_t position, void* ctx) {
        return static_cast<MovieInput*>(ctx)->Seek(position) ? 0 : -1;
    };
    extract.in_size_cb = [](void* ctx) { return static_cast<MovieInput*>(ctx)->Size(); };
    if (esp_extractor_open(&extract, &session->extractor) != ESP_EXTRACTOR_ERR_OK ||
        esp_extractor_parse_stream(session->extractor) != ESP_EXTRACTOR_ERR_OK) {
        callbacks.error("Không đọc được cấu trúc MP4 hoặc máy chủ không hỗ trợ tìm vị trí tệp");
        return false;
    }
    esp_extractor_stream_info_t audio{}, video{};
    if (esp_extractor_get_stream_info(session->extractor, ESP_EXTRACTOR_STREAM_TYPE_AUDIO, 0,
                                      &audio) != ESP_EXTRACTOR_ERR_OK ||
        esp_extractor_get_stream_info(session->extractor, ESP_EXTRACTOR_STREAM_TYPE_VIDEO, 0,
                                      &video) != ESP_EXTRACTOR_ERR_OK) {
        callbacks.error("MP4 phải có cả hình và tiếng");
        return false;
    }
    const bool h264 = video.video_info.format == ESP_EXTRACTOR_VIDEO_FORMAT_H264;
    if ((!h264 && video.video_info.format != ESP_EXTRACTOR_VIDEO_FORMAT_MJPEG) ||
        !video.video_info.width || !video.video_info.height || video.video_info.width > 240 ||
        video.video_info.height > 240 ||
        !std::isfinite(static_cast<double>(video.video_info.fps)) || video.video_info.fps <= 0 ||
        video.video_info.fps > 15 ||
        (audio.audio_info.format != ESP_EXTRACTOR_AUDIO_FORMAT_AAC &&
         audio.audio_info.format != ESP_EXTRACTOR_AUDIO_FORMAT_MP3) ||
        audio.audio_info.channel < 1 || audio.audio_info.channel > 2 ||
        audio.audio_info.sample_rate < 8000 || audio.audio_info.sample_rate > 48000) {
        callbacks.error("MP4 cần H.264 Baseline/MJPEG tối đa 240×240, 15 hình/giây và AAC-LC/MP3");
        return false;
    }
    esp_video_dec_cfg_t video_config{};
    video_config.codec_type = h264 ? ESP_VIDEO_CODEC_TYPE_H264 : ESP_VIDEO_CODEC_TYPE_MJPEG;
    video_config.out_fmt =
        h264 ? ESP_VIDEO_CODEC_PIXEL_FMT_YUV420P : ESP_VIDEO_CODEC_PIXEL_FMT_RGB565_LE;
    // Chuẩn hóa Annex B và avcC trước khi đưa dữ liệu vào decoder.
    if (esp_video_dec_open(&video_config, &session->video) != ESP_VC_ERR_OK) {
        callbacks.error("Không mở được bộ giải mã video");
        return false;
    }
    esp_aac_dec_cfg_t aac{};
    aac.sample_rate = audio.audio_info.sample_rate;
    aac.channel = audio.audio_info.channel;
    aac.bits_per_sample = 16;
    aac.no_adts_header = true;
    esp_audio_dec_cfg_t audio_config{};
    audio_config.type = audio.audio_info.format == ESP_EXTRACTOR_AUDIO_FORMAT_AAC
                            ? ESP_AUDIO_TYPE_AAC
                            : ESP_AUDIO_TYPE_MP3;
    if (audio_config.type == ESP_AUDIO_TYPE_AAC) {
        // Chỉ nhận AAC-LC; tránh giải mã sai cấu hình HE-AAC hoặc cấu hình mở rộng.
        if (!audio.spec_info || audio.spec_info_len < 2 || (audio.spec_info[0] >> 3) != 2) {
            callbacks.error("Âm thanh MP4 phải là AAC-LC hoặc MP3");
            return false;
        }
        audio_config.cfg = &aac;
        audio_config.cfg_sz = sizeof(aac);
    }
    if (esp_audio_dec_open(&audio_config, &session->audio) != ESP_AUDIO_ERR_OK) {
        callbacks.error("Không mở được bộ giải mã âm thanh MP4");
        return false;
    }
    int64_t started = 0, paused_at = 0;
    unsigned empty_frames = 0;
    bool audio_played = false, video_played = false;
    bool audio_eos = false, video_eos = false;
    while (!callbacks.cancelled()) {
        if (callbacks.paused()) {
            if (!paused_at)
                paused_at = esp_timer_get_time();
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        if (paused_at) {
            if (started)
                started += esp_timer_get_time() - paused_at;
            paused_at = 0;
        }
        esp_extractor_frame_info_t frame{};
        const auto result = esp_extractor_read_frame(session->extractor, &frame);
        if (result == ESP_EXTRACTOR_ERR_EOS)
            return audio_played && video_played;
        if (result != ESP_EXTRACTOR_ERR_OK) {
            callbacks.error("Lỗi đọc khung MP4; kiểm tra mạng, kích thước khung và codec");
            return false;
        }
        // Luôn trả khung trước khi đóng bộ tách, kể cả lỗi hoặc hủy phát.
        const auto release = [&session](esp_extractor_frame_info_t* p) {
            esp_extractor_release_frame(session->extractor, p);
        };
        std::unique_ptr<esp_extractor_frame_info_t, decltype(release)> guard(&frame, release);
        if (!frame.frame_buffer || !frame.frame_size) {
            if (++empty_frames > 16) {
                callbacks.error("Bộ tách MP4 không trả dữ liệu khung hợp lệ");
                return false;
            }
            if (EXTRACTOR_IS_EOS(frame.frame_flag) && frame.stream_idx == 0) {
                audio_eos |= frame.stream_type == ESP_EXTRACTOR_STREAM_TYPE_AUDIO;
                video_eos |= frame.stream_type == ESP_EXTRACTOR_STREAM_TYPE_VIDEO;
                // Một track hết trước không được cắt phần còn lại của track kia.
                if ((audio_eos && video_eos) || frame.stream_type == ESP_EXTRACTOR_STREAM_TYPE_NONE)
                    return audio_played && video_played;
            }
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }
        empty_frames = 0;
        if (frame.stream_idx != 0)
            continue;
        if (frame.pts > 3600000) {
            callbacks.error("Video vượt giới hạn một giờ");
            return false;
        }
        if (!started)
            started = esp_timer_get_time() - static_cast<int64_t>(frame.pts) * 1000;
        // Theo mốc PTS; chừa 60 ms cho hàng đợi âm thanh, bỏ hình trễ sau giải mã.
        int64_t target = started + static_cast<int64_t>(frame.pts) * 1000 -
                         (frame.stream_type == ESP_EXTRACTOR_STREAM_TYPE_AUDIO ? 60000 : 0);
        while (!callbacks.cancelled() && (callbacks.paused() || esp_timer_get_time() < target)) {
            if (callbacks.paused()) {
                const int64_t begin = esp_timer_get_time();
                while (callbacks.paused() && !callbacks.cancelled())
                    vTaskDelay(pdMS_TO_TICKS(10));
                const int64_t delay = esp_timer_get_time() - begin;
                started += delay;
                target += delay;
                // Mốc hiện tại cũng phải dời theo thời gian tạm dừng.
                paused_at = 0;
                continue;
            }
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        if (callbacks.cancelled())
            return false;
        if (frame.stream_type == ESP_EXTRACTOR_STREAM_TYPE_AUDIO) {
            if (!DecodeAudio(*session, frame, callbacks, audio_played)) {
                callbacks.error("Lỗi giải mã âm thanh MP4");
                return false;
            }
        } else if (frame.stream_type == ESP_EXTRACTOR_STREAM_TYPE_VIDEO) {
            if (frame.frame_size > 128 * 1024) {
                callbacks.error("Khung video vượt giới hạn 128 KiB");
                return false;
            }
            esp_video_dec_in_frame_t encoded{};
            encoded.data = frame.frame_buffer;
            encoded.size = frame.frame_size;
            encoded.pts = frame.pts;
            if (h264) {
                size_t count = 0;
                if (!H264AnnexB(frame.frame_buffer, frame.frame_size, video.spec_info,
                                video.spec_info_len, !video_played, session->encoded_video.data(),
                                session->encoded_video.size(), count)) {
                    callbacks.error("Khung H.264 không hợp lệ, không phải Baseline hoặc quá lớn");
                    return false;
                }
                encoded.data = session->encoded_video.data();
                encoded.size = count;
            }
            esp_video_dec_out_frame_t decoded{};
            decoded.data = session->decoded_video.data();
            decoded.size = session->decoded_video.size();
            // MJPEG RGB565 có thể cần 115200 byte, lớn hơn bộ đệm YUV cho một khung 240².
            if (!h264) {
                decoded.data = reinterpret_cast<uint8_t*>(session->rgb.data());
                decoded.size = session->rgb.size() * 2;
            }
            if (esp_video_dec_process(session->video, &encoded, &decoded) != ESP_VC_ERR_OK ||
                decoded.decoded_size > decoded.size) {
                callbacks.error(
                    "Lỗi giải mã hình MP4; dùng H.264 Baseline hoặc MJPEG độ phân giải nhỏ");
                return false;
            }
            if (!decoded.decoded_size)
                continue;
            esp_video_codec_frame_info_t info{};
            if (esp_video_dec_get_frame_info(session->video, &info) != ESP_VC_ERR_OK ||
                info.res.width != video.video_info.width ||
                info.res.height != video.video_info.height ||
                (!h264 && decoded.decoded_size < info.res.width * info.res.height * 2)) {
                callbacks.error("Kích thước khung video thay đổi hoặc không hợp lệ");
                return false;
            }
            if (h264 && !ConvertVideo(*session, decoded, true, info.res.width, info.res.height,
                                      info.res.width, info.res.height))
                return false;
            if (callbacks.cancelled())
                return false;
            if (!callbacks.paused() &&
                esp_timer_get_time() - (started + static_cast<int64_t>(frame.pts) * 1000) < 180000)
                callbacks.video(session->rgb.data(), info.res.width, info.res.height);
            video_played = true;
        }
    }
    return false;
}
}  // namespace uta
