#pragma once

#include <cstdlib>

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_check.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "uta_settings.h"

namespace uta {
// Chỉ tác vụ điều khiển board ghi các đầu ra này; kênh 0 dành riêng đèn nền.
class Motion {
public:
    explicit Motion(const Configuration& config) : config_(config) {}

    void Start() {
        if (config_.motor_driver) {
            Timer(LEDC_TIMER_1, 20000, LEDC_TIMER_10_BIT);
            if (config_.motor_driver == 1) {
                for (auto index : {kMotorL1, kMotorL2, kMotorR1, kMotorR2}) {
                    const auto pin = Pin(index);
                    ESP_ERROR_CHECK(gpio_set_level(pin, 0));
                    ESP_ERROR_CHECK(gpio_set_direction(pin, GPIO_MODE_OUTPUT));
                }
                Channel(kMotorLEn, LEDC_CHANNEL_1, LEDC_TIMER_1);
                Channel(kMotorREn, LEDC_CHANNEL_2, LEDC_TIMER_1);
            } else {
                Channel(kMotorL1, LEDC_CHANNEL_1, LEDC_TIMER_1);
                Channel(kMotorL2, LEDC_CHANNEL_2, LEDC_TIMER_1);
                Channel(kMotorR1, LEDC_CHANNEL_3, LEDC_TIMER_1);
                Channel(kMotorR2, LEDC_CHANNEL_4, LEDC_TIMER_1);
            }
            Stop();
        }
        if (config_.servos_enabled) {
            Timer(LEDC_TIMER_2, 50, LEDC_TIMER_14_BIT);
            Channel(kServoHead, LEDC_CHANNEL_5, LEDC_TIMER_2);
            Channel(kServoNeck, LEDC_CHANNEL_6, LEDC_TIMER_2);
            Servo(0, config_.servo_home[0]);
            Servo(1, config_.servo_home[1]);
        }
    }

    void Stop() {
        if (!config_.motor_driver)
            return;
        Duty(LEDC_CHANNEL_1, 0);
        Duty(LEDC_CHANNEL_2, 0);
        if (config_.motor_driver == 2) {
            Duty(LEDC_CHANNEL_3, 0);
            Duty(LEDC_CHANNEL_4, 0);
        } else {
            for (auto index : {kMotorL1, kMotorL2, kMotorR1, kMotorR2})
                gpio_set_level(Pin(index), 0);
        }
    }

    void Drive(int left, int right) {
        if (!config_.motor_driver)
            return;
        Stop();
        // Ngắt công suất trước khi đổi chiều; chạy ngoài tác vụ âm thanh/chính.
        vTaskDelay(pdMS_TO_TICKS(10));
        if (config_.reverse_left)
            left = -left;
        if (config_.reverse_right)
            right = -right;
        Motor(true, std::clamp(left, -100, 100));
        Motor(false, std::clamp(right, -100, 100));
    }

    void Servo(size_t axis, int angle) {
        if (!config_.servos_enabled || axis >= 2)
            return;
        angle = std::clamp(angle, config_.servo_min[axis], config_.servo_max[axis]);
        // Servo 180 độ, 50 Hz, xung 500–2500 µs; giới hạn góc tránh chạm cơ khí.
        const unsigned pulse = 500 + angle * 2000 / 180;
        Duty(axis == 0 ? LEDC_CHANNEL_5 : LEDC_CHANNEL_6, pulse * 16383 / 20000);
    }

private:
    const Configuration& config_;
    gpio_num_t Pin(PinIndex index) const {
        return static_cast<gpio_num_t>(config_.gpio.pins[index]);
    }
    static void Timer(ledc_timer_t timer, unsigned frequency, ledc_timer_bit_t bits) {
        ledc_timer_config_t config{};
        config.speed_mode = LEDC_LOW_SPEED_MODE;
        config.timer_num = timer;
        config.freq_hz = frequency;
        config.duty_resolution = bits;
        config.clk_cfg = LEDC_AUTO_CLK;
        ESP_ERROR_CHECK(ledc_timer_config(&config));
    }
    void Channel(PinIndex pin, ledc_channel_t channel, ledc_timer_t timer) {
        ledc_channel_config_t config{};
        config.gpio_num = Pin(pin);
        config.channel = channel;
        config.timer_sel = timer;
        config.speed_mode = LEDC_LOW_SPEED_MODE;
        ESP_ERROR_CHECK(ledc_channel_config(&config));
    }
    static void Duty(ledc_channel_t channel, unsigned duty) {
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty));
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, channel));
    }
    void Motor(bool left, int speed) {
        const unsigned duty = std::abs(speed) * 1023 / 100;
        if (config_.motor_driver == 1) {
            gpio_set_level(Pin(left ? kMotorL1 : kMotorR1), speed > 0);
            gpio_set_level(Pin(left ? kMotorL2 : kMotorR2), speed < 0);
            Duty(left ? LEDC_CHANNEL_1 : LEDC_CHANNEL_2, duty);
        } else {
            Duty(left ? LEDC_CHANNEL_1 : LEDC_CHANNEL_3, speed > 0 ? duty : 0);
            Duty(left ? LEDC_CHANNEL_2 : LEDC_CHANNEL_4, speed < 0 ? duty : 0);
        }
    }
};
}  // namespace uta
