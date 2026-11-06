#pragma once

#include <algorithm>
#include <array>

#include <driver/mcpwm_prelude.h>
#include <esp_check.h>

namespace uta {
class PwmBank {
public:
    void Start(const std::array<int, 4>& pins, int group, int period,
               const std::array<int, 4>& initial, const std::array<bool, 4>& inverted = {}) {
        period_ = period;
        if (std::all_of(pins.begin(), pins.end(), [](int pin) { return pin < 0; }))
            return;
        mcpwm_timer_config_t timer_config{};
        timer_config.group_id = group;
        timer_config.clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT;
        timer_config.resolution_hz = 1000000;
        timer_config.count_mode = MCPWM_TIMER_COUNT_MODE_UP;
        timer_config.period_ticks = period;
        mcpwm_timer_handle_t timer = nullptr;
        ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));
        for (size_t base = 0; base < pins.size(); base += 2) {
            if (pins[base] < 0 && pins[base + 1] < 0)
                continue;
            mcpwm_operator_config_t operator_config{};
            operator_config.group_id = timer_config.group_id;
            mcpwm_oper_handle_t oper = nullptr;
            ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper));
            ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, timer));
            for (size_t axis = base; axis < base + 2; ++axis) {
                if (pins[axis] < 0)
                    continue;
                mcpwm_comparator_config_t compare_config{};
                compare_config.flags.update_cmp_on_tez = true;
                ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &compare_config, &comparators_[axis]));
                mcpwm_generator_config_t generator_config{};
                generator_config.gen_gpio_num = pins[axis];
                generator_config.flags.invert_pwm = inverted[axis];
                auto& generator = generators_[axis];
                ESP_ERROR_CHECK(mcpwm_new_generator(oper, &generator_config, &generator));
                ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(
                    generator, MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                                            MCPWM_TIMER_EVENT_EMPTY,
                                                            MCPWM_GEN_ACTION_HIGH)));
                ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(
                    generator, MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,
                                                              comparators_[axis],
                                                              MCPWM_GEN_ACTION_LOW)));
                SetPulse(axis, initial[axis]);
            }
        }
        ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
        ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));
    }

    void SetPulse(size_t axis, int ticks) {
        if (axis >= generators_.size() || !generators_[axis])
            return;
        const bool constant = ticks <= 0 || ticks >= period_;
        if (!constant)
            ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparators_[axis], ticks));
        ESP_ERROR_CHECK(mcpwm_generator_set_force_level(
            generators_[axis], constant ? (ticks >= period_ ? 1 : 0) : -1, true));
    }

private:
    int period_ = 0;
    std::array<mcpwm_cmpr_handle_t, 4> comparators_{};
    std::array<mcpwm_gen_handle_t, 4> generators_{};
};
}  // namespace uta
