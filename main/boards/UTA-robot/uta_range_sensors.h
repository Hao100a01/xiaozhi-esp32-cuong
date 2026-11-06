#pragma once

#include <atomic>
#include <array>

#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <driver/rmt_rx.h>
#include <esp_log.h>
#include <esp_rom_sys.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern "C" {
#include <vl53l0x.h>
}

#include "uta_settings.h"

namespace uta {
class RangeSensors {
public:
    explicit RangeSensors(const Configuration& config) : config_(config) {}

    void Start(const std::array<i2c_master_bus_handle_t, 2>& oled_buses) {
        if (config_.range_pins[kTofSda] >= 0) {
            const int shared = TofSharedOled(config_);
            if (shared >= 0)
                tof_bus_ = oled_buses[shared];
            if (xTaskCreate([](void* arg) { static_cast<RangeSensors*>(arg)->RunTof(); },
                            "uta_tof", 4096, this, 2, nullptr) != pdPASS)
                ESP_LOGW(kTag, "Không tạo được tác vụ ToF");
        }
        if (config_.range_pins[kUltrasonicTrig] >= 0 &&
            xTaskCreate([](void* arg) { static_cast<RangeSensors*>(arg)->RunUltrasonic(); },
                        "uta_hcsr04", 3072, this, 2, nullptr) != pdPASS)
            ESP_LOGW(kTag, "Không tạo được tác vụ HC-SR04");
    }

    int TofMillimeters() const { return tof_mm_.load(); }
    int UltrasonicMillimeters() const { return ultrasonic_mm_.load(); }

private:
    static constexpr const char* kTag = "UTA_Range";
    const Configuration& config_;
    i2c_master_bus_handle_t tof_bus_ = nullptr;
    std::atomic<int> tof_mm_{-1};
    std::atomic<int> ultrasonic_mm_{-1};

    esp_err_t InitTof(vl53l0x_handle_t& sensor) {
        const auto& pins = config_.range_pins;
        if (pins[kTofShutdown] >= 0) {
            const auto pin = static_cast<gpio_num_t>(pins[kTofShutdown]);
            gpio_set_level(pin, 0);
            gpio_set_direction(pin, GPIO_MODE_OUTPUT_OD);
            vTaskDelay(pdMS_TO_TICKS(20));
            gpio_set_level(pin, 1);  // Release XSHUT to the module's pull-up.
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        if (!tof_bus_) {
            if (TofSharedOled(config_) >= 0)
                return ESP_ERR_INVALID_STATE;
            i2c_master_bus_config_t bus_config{};
            bus_config.i2c_port = IsOled(config_.primary_lcd.driver) ||
                                           IsOled(config_.secondary_lcd.driver) ? 1 : 0;
            bus_config.sda_io_num = static_cast<gpio_num_t>(pins[kTofSda]);
            bus_config.scl_io_num = static_cast<gpio_num_t>(pins[kTofScl]);
            bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
            bus_config.glitch_ignore_cnt = 7;
            bus_config.flags.enable_internal_pullup = true;
            const auto result = i2c_new_master_bus(&bus_config, &tof_bus_);
            if (result != ESP_OK)
                return result;
        }
        auto result = vl53l0x_create(&sensor, tof_bus_);
        if (result == ESP_OK)
            result = vl53l0x_init(sensor);
        if (result == ESP_OK) {
            vl53l0x_ref_calibration_t calibration{};
            result = vl53l0x_perform_ref_calibration(sensor, &calibration);
        }
        if (result == ESP_OK)
            result = vl53l0x_set_mode(sensor, VL53L0X_MODE_SINGLE);
        return result;
    }

    void RunTof() {
        vl53l0x_handle_t sensor = nullptr;
        if (const auto result = InitTof(sensor); result == ESP_OK) {
            for (;;) {
                vl53l0x_data_t sample{};
                const auto measured = vl53l0x_single_measure(sensor, &sample);
                tof_mm_.store(measured == ESP_OK && sample.valid ? sample.distance_mm : -1);
                vTaskDelay(pdMS_TO_TICKS(250));
            }
        } else {
            ESP_LOGW(kTag, "Không khởi tạo được VL53L0X: %s", esp_err_to_name(result));
            if (sensor)
                vl53l0x_destroy(sensor);
            if (tof_bus_ && TofSharedOled(config_) < 0)
                i2c_del_master_bus(tof_bus_);
        }
        vTaskDelete(nullptr);
    }

    static bool EchoReceived(rmt_channel_handle_t, const rmt_rx_done_event_data_t* data,
                             void* task) {
        BaseType_t wake = pdFALSE;
        xTaskNotifyFromISR(static_cast<TaskHandle_t>(task), data->num_symbols,
                           eSetValueWithOverwrite, &wake);
        return wake == pdTRUE;
    }

    void RunUltrasonic() {
        const auto trigger = static_cast<gpio_num_t>(config_.range_pins[kUltrasonicTrig]);
        gpio_set_level(trigger, 0);
        gpio_set_direction(trigger, GPIO_MODE_OUTPUT);
        rmt_rx_channel_config_t channel_config{};
        channel_config.gpio_num = static_cast<gpio_num_t>(config_.range_pins[kUltrasonicEcho]);
        channel_config.clk_src = RMT_CLK_SRC_DEFAULT;
        channel_config.resolution_hz = 1000000;
        channel_config.mem_block_symbols = 48;
        rmt_channel_handle_t channel = nullptr;
        auto result = rmt_new_rx_channel(&channel_config, &channel);
        rmt_rx_event_callbacks_t callbacks{};
        callbacks.on_recv_done = EchoReceived;
        if (result == ESP_OK)
            result = rmt_rx_register_event_callbacks(channel, &callbacks, xTaskGetCurrentTaskHandle());
        if (result == ESP_OK)
            result = rmt_enable(channel);
        if (result != ESP_OK) {
            ESP_LOGW(kTag, "Không khởi tạo được HC-SR04: %s", esp_err_to_name(result));
            if (channel)
                rmt_del_channel(channel);
            vTaskDelete(nullptr);
            return;
        }

        std::array<rmt_symbol_word_t, 48> symbols{};
        rmt_receive_config_t receive_config{};
        receive_config.signal_range_min_ns = 2000;
        receive_config.signal_range_max_ns = 28000000;
        for (;;) {
            int millimeters = -1;
            uint32_t count = 0;
            if (rmt_receive(channel, symbols.data(), sizeof(symbols), &receive_config) == ESP_OK) {
                gpio_set_level(trigger, 1);
                esp_rom_delay_us(12);
                gpio_set_level(trigger, 0);
                if (xTaskNotifyWait(0, UINT32_MAX, &count, pdMS_TO_TICKS(80)) == pdTRUE) {
                    for (size_t i = 0; i < std::min<size_t>(count, symbols.size()); ++i) {
                        const auto& symbol = symbols[i];
                        const unsigned pulse = symbol.level0 ? symbol.duration0 :
                                               symbol.level1 ? symbol.duration1 : 0;
                        if (pulse >= 116 && pulse <= 23324) {
                            millimeters = (pulse * 343 + 1000) / 2000;
                            break;
                        }
                    }
                } else {
                    rmt_disable(channel);
                    xTaskNotifyStateClear(nullptr);
                    rmt_enable(channel);
                }
            }
            ultrasonic_mm_.store(millimeters);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
};
}  // namespace uta
