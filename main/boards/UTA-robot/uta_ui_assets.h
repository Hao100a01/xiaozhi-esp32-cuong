#pragma once

#include <lvgl.h>

namespace uta {

extern const lv_font_t uta_font_small;
extern const lv_font_t uta_font_clock;
extern const lv_font_t uta_font_temperature;
const lv_image_dsc_t* BootImage();
const lv_image_dsc_t* NeonImage();
const lv_image_dsc_t* UiIcon(unsigned index);
inline constexpr unsigned kWifiConnectedIcon = 9;
inline constexpr unsigned kWifiOfflineIcon = 10;
inline constexpr unsigned kLocationIcon = 11;

}  // namespace uta
