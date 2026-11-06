#include "uta_ui_assets.h"

#include <array>

extern const uint8_t uta_boot_start[] asm("_binary_uta_boot_rgb565_start");
extern const uint8_t uta_neon_start[] asm("_binary_uta_neon_rgb565_start");
extern const uint8_t uta_icons_start[] asm("_binary_uta_icons_argb8888_start");

namespace uta {
namespace {
lv_image_dsc_t Image(const uint8_t* bytes, unsigned width, unsigned height,
                     lv_color_format_t format, unsigned bytes_per_pixel) {
    lv_image_dsc_t image{};
    image.header.magic = LV_IMAGE_HEADER_MAGIC;
    image.header.cf = format;
    image.header.w = width;
    image.header.h = height;
    image.header.stride = width * bytes_per_pixel;
    image.data_size = width * height * bytes_per_pixel;
    image.data = bytes;
    return image;
}
}  // namespace

#include "assets/uta_font_clock.inc"
#include "assets/uta_font_small.inc"
#include "assets/uta_font_temperature.inc"

const lv_image_dsc_t* BootImage() {
    static const auto image = Image(uta_boot_start, 240, 240, LV_COLOR_FORMAT_RGB565, 2);
    return &image;
}

const lv_image_dsc_t* NeonImage() {
    static const auto image = Image(uta_neon_start, 240, 240, LV_COLOR_FORMAT_RGB565, 2);
    return &image;
}

const lv_image_dsc_t* UiIcon(unsigned index) {
    static const auto icons = []() {
        std::array<lv_image_dsc_t, 12> result{};
        for (unsigned i = 0; i < result.size(); ++i)
            result[i] =
                Image(uta_icons_start + i * 32 * 32 * 4, 32, 32, LV_COLOR_FORMAT_ARGB8888, 4);
        return result;
    }();
    return &icons[index < icons.size() ? index : 0];
}

}  // namespace uta
