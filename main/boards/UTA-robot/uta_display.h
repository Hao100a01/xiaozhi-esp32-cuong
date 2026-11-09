#pragma once

#include <esp_heap_caps.h>
#include <src/misc/cache/instance/lv_image_cache.h>
#include <cstring>
#include "device_state.h"
#include "display/lcd_display.h"
#include "uta_emotions.h"
#include "uta_spectrum.h"

namespace uta {
struct MediaSnapshot;
}

// Lớp hiển thị riêng của UTA: không đưa hành vi board vào màn hình dùng chung.
class UtaDisplay : public SpiLcdDisplay {
public:
    using SpiLcdDisplay::SpiLcdDisplay;
    ~UtaDisplay() override;

    void SetupUI() override;
    void RestoreRotation(bool st7789);
    bool SetRotation(int degrees);
    int Rotation() const { return rotation_.load(); }
    bool SetEmotion(const char* emotion) override;
    void ShowVisualScene(const char* scene, const char* text, int duration_ms = 6000) override;
    void UpdateStandby(DeviceState state, bool media_active, bool wifi_connected);

    void UpdateVideo(const uint16_t* pixels, int width, int height) {
        if (!IsSetupUICalled() || !Lock(20))
            return;
        if (!pixels) {
            if (video_image_)
                lv_obj_add_flag(video_image_, LV_OBJ_FLAG_HIDDEN);
            Unlock();
            return;
        }
        if (width <= 0 || height <= 0 || width > 240 || height > 240) {
            Unlock();
            return;
        }
        if (!video_pixels_)
            video_pixels_ = static_cast<uint16_t*>(
                heap_caps_malloc(240 * 240 * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!video_pixels_) {
            Unlock();
            return;
        }
        if (!video_image_)
            video_image_ = lv_image_create(lv_display_get_layer_top(display_));
        lv_image_cache_drop(&video_descriptor_);
        std::memcpy(video_pixels_, pixels, width * height * sizeof(uint16_t));
        video_descriptor_.header.magic = LV_IMAGE_HEADER_MAGIC;
        video_descriptor_.header.cf = LV_COLOR_FORMAT_RGB565;
        video_descriptor_.header.w = width;
        video_descriptor_.header.h = height;
        video_descriptor_.header.stride = width * 2;
        video_descriptor_.data_size = width * height * 2;
        video_descriptor_.data = reinterpret_cast<const uint8_t*>(video_pixels_);
        lv_image_set_src(video_image_, &video_descriptor_);
        lv_obj_center(video_image_);
        lv_obj_remove_flag(video_image_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_invalidate(video_image_);
        Unlock();
    }

    void UpdateSpectrum(const uta::Spectrum& values, const uta::MediaSnapshot* media);

private:
    std::atomic<int> rotation_{0};
    bool st7789_square_ = false;
    void ApplyRotationLocked(int degrees);
    void SetupEyesLocked();
    bool UpdateEyesLocked(DeviceState state, bool visible, int64_t now);
    void ReleaseEyesLocked();
    lv_obj_t* eyes_screen_ = nullptr;
    lv_obj_t* eyes_image_ = nullptr;
    lv_obj_t* eyes_status_ = nullptr;
    lv_obj_t* eyes_subtitle_ = nullptr;
    std::unique_ptr<uint8_t, decltype(&heap_caps_free)> eyes_gif_data_{nullptr, heap_caps_free};
    std::unique_ptr<LvglGif> eyes_controller_;
    std::atomic<uta::EyeEmotion> requested_eye_{uta::EyeEmotion::Neutral};
    std::atomic<bool> use_base_emotion_{false};
    std::atomic<int64_t> expression_until_{0};
    uta::EyeEmotion loaded_eye_ = uta::EyeEmotion::Count;
    uta::EyeEmotion failed_eye_ = uta::EyeEmotion::Count;
    int64_t eye_retry_at_ = 0;
    lv_obj_t* boot_screen_ = nullptr;
    lv_obj_t* idle_screen_ = nullptr;
    lv_obj_t* clock_label_ = nullptr;
    lv_obj_t* date_label_ = nullptr;
    lv_obj_t* place_label_ = nullptr;
    lv_obj_t* weather_label_ = nullptr;
    lv_obj_t* temperature_label_ = nullptr;
    lv_obj_t* wifi_image_ = nullptr;
    lv_obj_t* wifi_text_ = nullptr;
    lv_obj_t* weather_image_ = nullptr;
    int64_t boot_until_ = 0;
    int64_t content_at_ = 0;
    lv_obj_t* spectrum_ = nullptr;
    lv_obj_t* record_highlight_ = nullptr;
    lv_obj_t* media_kind_ = nullptr;
    lv_obj_t* media_title_ = nullptr;
    lv_obj_t* media_artist_ = nullptr;
    lv_obj_t* media_progress_ = nullptr;
    lv_obj_t* media_progress_dot_ = nullptr;
    lv_obj_t* media_elapsed_ = nullptr;
    lv_obj_t* media_duration_ = nullptr;
    unsigned record_angle_ = 0;
    lv_obj_t* video_image_ = nullptr;
    lv_obj_t* scene_screen_ = nullptr;
    lv_obj_t* scene_icon_ = nullptr;
    lv_obj_t* scene_title_ = nullptr;
    lv_obj_t* scene_detail_ = nullptr;
    lv_obj_t* scene_temperature_ = nullptr;
    std::array<lv_obj_t*, 8> scene_particles_{};
    int64_t scene_until_ = 0;
    bool scene_rain_ = false;
    uint8_t scene_phase_ = 0;
    uint16_t* video_pixels_ = nullptr;
    lv_image_dsc_t video_descriptor_{};
    std::array<lv_obj_t*, uta::kSpectrumBands> bars_{};
    std::array<int, uta::kSpectrumBands> bar_heights_{};
};
