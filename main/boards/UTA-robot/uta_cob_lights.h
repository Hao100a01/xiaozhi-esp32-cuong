#pragma once

#include <esp_log.h>
#include <led_strip.h>

#include <array>

#include "uta_pwm_bank.h"
#include "uta_settings.h"

namespace uta {
class CobLights {
public:
    explicit CobLights(const Configuration& config) : config_(config) {}

    void Start() {
        const auto& pins = config_.cob_pins;
        const bool invert = config_.rgb_common_anode;
        pwm_.Start({pins[kCobRed], pins[kCobGreen], pins[kCobBlue], pins[kCobEdison]},
                   1, kPeriod, {0, 0, 0, 0}, {invert, invert, invert, false});
        if (pins[kCobStrip] < 0)
            return;
        led_strip_config_t strip_config{};
        strip_config.strip_gpio_num = pins[kCobStrip];
        strip_config.max_leds = kPixelCount;
        strip_config.led_model = LED_MODEL_WS2812;
        strip_config.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
        led_strip_rmt_config_t rmt_config{};
        rmt_config.resolution_hz = 10000000;
        auto result = led_strip_new_rmt_device(&strip_config, &rmt_config, &strip_);
        if (result == ESP_OK)
            result = led_strip_clear(strip_);
        if (result != ESP_OK) {
            ESP_LOGW(kTag, "Không khởi tạo được WS2812B: %s", esp_err_to_name(result));
            if (strip_)
                led_strip_del(strip_);
            strip_ = nullptr;
        }
    }

    bool Available(size_t light) const {
        switch (light) {
        case kRgbLight:
            return config_.cob_pins[kCobRed] >= 0;
        case kEdisonLight:
            return config_.cob_pins[kCobEdison] >= 0;
        case kStripLight:
            return strip_ != nullptr;
        default:
            return false;
        }
    }

    void Apply(size_t light, unsigned color) {
        if (!Available(light))
            return;
        if (light == kEdisonLight) {
            pwm_.SetPulse(kCobEdison, color * kPeriod / 255);
            return;
        }
        const unsigned r = (color >> 16) & 255;
        const unsigned g = (color >> 8) & 255;
        const unsigned b = color & 255;
        if (light == kRgbLight) {
            pwm_.SetPulse(kCobRed, r * kPeriod / 255);
            pwm_.SetPulse(kCobGreen, g * kPeriod / 255);
            pwm_.SetPulse(kCobBlue, b * kPeriod / 255);
            return;
        }
        for (size_t pixel = 0; pixel < kPixelCount; ++pixel)
            led_strip_set_pixel(strip_, pixel, r, g, b);
        if (const auto result = led_strip_refresh(strip_); result != ESP_OK)
            ESP_LOGW(kTag, "Không cập nhật được WS2812B: %s", esp_err_to_name(result));
    }

    void ApplyStripFrame(const std::array<unsigned, 8>& colors) {
        if (!strip_)
            return;
        for (size_t pixel = 0; pixel < colors.size(); ++pixel) {
            const unsigned color = colors[pixel];
            led_strip_set_pixel(strip_, pixel, (color >> 16) & 255, (color >> 8) & 255,
                                color & 255);
        }
        if (const auto result = led_strip_refresh(strip_); result != ESP_OK)
            ESP_LOGW(kTag, "Không cập nhật được hiệu ứng WS2812B: %s", esp_err_to_name(result));
    }

private:
    static constexpr const char* kTag = "UTA_Cob";
    static constexpr int kPeriod = 1000;
    static constexpr size_t kPixelCount = 8;
    const Configuration& config_;
    PwmBank pwm_;
    led_strip_handle_t strip_ = nullptr;
};
}  // namespace uta
