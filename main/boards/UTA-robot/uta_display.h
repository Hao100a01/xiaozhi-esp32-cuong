#pragma once

#include <esp_heap_caps.h>
#include <src/misc/cache/instance/lv_image_cache.h>
#include <cstring>
#include "device_state.h"
#include "display/lcd_display.h"
#include "uta_emotions.h"
#include "uta_spectrum.h"

// Lớp hiển thị riêng của UTA: không đưa hành vi board vào màn hình dùng chung.
class UtaDisplay : public SpiLcdDisplay {
public:
    using SpiLcdDisplay::SpiLcdDisplay;
    ~UtaDisplay() override;

    void SetupUI() override;
    void RestoreRotation();
    bool SetRotation(int degrees);
    int Rotation() const { return rotation_.load(); }
    bool SetEmotion(const char* emotion) override;
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
            video_image_ = lv_image_create(lv_layer_top());
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

    void UpdateSpectrum(const uta::Spectrum& values, bool visible) {
        // Không để việc vẽ chặn tác vụ điều khiển khi LVGL đang bận.
        if (!IsSetupUICalled() || !Lock(10))
            return;
        if (!spectrum_ && visible) {
            spectrum_ = lv_obj_create(lv_layer_top());
            lv_obj_set_size(spectrum_, 224, 72);
            lv_obj_align(spectrum_, LV_ALIGN_BOTTOM_MID, 0, -4);
            lv_obj_remove_flag(spectrum_, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_pad_all(spectrum_, 4, 0);
            lv_obj_set_style_border_width(spectrum_, 0, 0);
            lv_obj_set_style_bg_color(spectrum_, lv_color_hex(0x102030), 0);
            for (size_t i = 0; i < bars_.size(); ++i) {
                bars_[i] = lv_obj_create(spectrum_);
                lv_obj_remove_flag(bars_[i], LV_OBJ_FLAG_SCROLLABLE);
                lv_obj_set_style_border_width(bars_[i], 0, 0);
                lv_obj_set_style_radius(bars_[i], 2, 0);
                lv_obj_set_style_bg_color(bars_[i], lv_color_hex(0x35d6a0), 0);
            }
        }
        if (spectrum_) {
            if (visible)
                lv_obj_remove_flag(spectrum_, LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(spectrum_, LV_OBJ_FLAG_HIDDEN);
            if (visible) {
                for (size_t i = 0; i < bars_.size(); ++i) {
                    lv_obj_set_size(bars_[i], 10, std::max(2, values[i] * 60 / 100));
                    lv_obj_align(bars_[i], LV_ALIGN_BOTTOM_LEFT, i * 13, 0);
                }
            }
        }
        Unlock();
    }

private:
    std::atomic<int> rotation_{0};
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
    lv_obj_t* video_image_ = nullptr;
    uint16_t* video_pixels_ = nullptr;
    lv_image_dsc_t video_descriptor_{};
    std::array<lv_obj_t*, uta::kSpectrumBands> bars_{};
};
