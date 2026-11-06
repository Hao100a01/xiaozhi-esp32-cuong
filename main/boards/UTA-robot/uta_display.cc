#include "uta_display.h"

#include <cmath>
#include <cstdio>
#include <ctime>

#include <esp_timer.h>
#include <nvs.h>

#include "uta_ui_assets.h"
#include "uta_weather.h"

namespace {
constexpr char kRotationNamespace[] = "uta_robot";
constexpr char kRotationKey[] = "screen_rotation";
constexpr lv_display_rotation_t kRotations[] = {
    LV_DISPLAY_ROTATION_0, LV_DISPLAY_ROTATION_90, LV_DISPLAY_ROTATION_180,
    LV_DISPLAY_ROTATION_270,
};

bool ValidRotation(int degrees) {
    return degrees >= 0 && degrees <= 270 && degrees % 90 == 0;
}

void Visible(lv_obj_t* object, bool visible) {
    if (visible)
        lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t* Panel(const lv_image_dsc_t* background) {
    auto* panel = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, 240, 240);
    lv_obj_center(panel);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_CLICKABLE);
    auto* image = lv_image_create(panel);
    lv_image_set_src(image, background);
    lv_obj_set_pos(image, 0, 0);
    return panel;
}

lv_obj_t* Label(lv_obj_t* panel, const char* text, int x, int y, int width, uint32_t color,
                const lv_font_t* font = &uta::uta_font_small) {
    auto* label = lv_label_create(panel);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    lv_obj_set_height(label, font->line_height);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_label_set_text(label, text);
    return label;
}

lv_obj_t* Icon(lv_obj_t* panel, unsigned index, int x, int y, int size) {
    auto* image = lv_image_create(panel);
    lv_image_set_src(image, uta::UiIcon(index));
    lv_image_set_pivot(image, 0, 0);
    lv_image_set_scale(image, size * 256 / 32);
    lv_obj_set_pos(image, x, y);
    return image;
}

bool IsVisible(lv_obj_t* object) { return object && !lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN); }
}  // namespace

void UtaDisplay::RestoreRotation() {
    nvs_handle_t handle;
    if (nvs_open(kRotationNamespace, NVS_READONLY, &handle) != ESP_OK)
        return;
    int32_t degrees = 0;
    const auto result = nvs_get_i32(handle, kRotationKey, &degrees);
    nvs_close(handle);
    if (result == ESP_OK && ValidRotation(degrees) && display_ && Lock()) {
        lv_display_set_rotation(display_, kRotations[degrees / 90]);
        rotation_.store(degrees);
        Unlock();
    }
}

bool UtaDisplay::SetRotation(int degrees) {
    if (!ValidRotation(degrees) || !display_ || !Lock(100))
        return false;

    bool saved = degrees == Rotation();
    if (!saved) {
        nvs_handle_t handle;
        if (nvs_open(kRotationNamespace, NVS_READWRITE, &handle) == ESP_OK) {
            saved = nvs_set_i32(handle, kRotationKey, degrees) == ESP_OK &&
                    nvs_commit(handle) == ESP_OK;
            nvs_close(handle);
        }
        if (saved) {
            lv_display_set_rotation(display_, kRotations[degrees / 90]);
            rotation_.store(degrees);
        }
    }
    Unlock();
    return saved;
}

void UtaDisplay::SetupUI() {
    // Bảo toàn giao diện hội thoại và biểu cảm của LcdDisplay.
    if (IsSetupUICalled())
        return;
    SpiLcdDisplay::SetupUI();
    DisplayLockGuard lock(this);
    SetupEyesLocked();
    idle_screen_ = Panel(uta::NeonImage());
    Label(idle_screen_, "ATFUTA", 16, 16, 90, 0x98b3c8);
    wifi_image_ = Icon(idle_screen_, uta::kWifiOfflineIcon, 171, 14, 18);
    wifi_text_ = Label(idle_screen_, "Offline", 191, 16, 48, 0x8b9baa);
    clock_label_ = Label(idle_screen_, "--:--", 13, 59, 214, 0x6ee8ef, &uta::uta_font_clock);
    date_label_ = Label(idle_screen_, "Đang đồng bộ giờ", 16, 129, 208, 0x9db0c2);
    auto* divider = lv_obj_create(idle_screen_);
    lv_obj_remove_style_all(divider);
    lv_obj_set_pos(divider, 16, 158);
    lv_obj_set_size(divider, 208, 1);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(divider, lv_color_hex(0x233848), 0);
    Icon(idle_screen_, uta::kLocationIcon, 16, 176, 15);
    place_label_ = Label(idle_screen_, "Chưa có vị trí", 36, 176, 188, 0xeef8ff);
    weather_image_ = Icon(idle_screen_, 0, 15, 199, 26);
    weather_label_ = Label(idle_screen_, "Chưa cập nhật", 47, 205, 125, 0x9db0c2);
    temperature_label_ =
        Label(idle_screen_, "--°", 172, 193, 66, 0xeef8ff, &uta::uta_font_temperature);
    lv_obj_set_style_text_align(temperature_label_, LV_TEXT_ALIGN_RIGHT, 0);
    Visible(idle_screen_, false);

    boot_screen_ = Panel(uta::BootImage());
    auto* text = Label(boot_screen_, "ĐANG KHỞI ĐỘNG", 16, 184, 208, 0x30383c);
    lv_obj_set_style_text_align(text, LV_TEXT_ALIGN_CENTER, 0);
    auto* line = lv_obj_create(boot_screen_);
    lv_obj_remove_style_all(line);
    lv_obj_set_pos(line, 88, 216);
    lv_obj_set_size(line, 64, 2);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(line, lv_color_hex(0x30383c), 0);
    boot_until_ = esp_timer_get_time() + 2200000;
}

void UtaDisplay::UpdateStandby(DeviceState state, bool media_active, bool wifi_connected) {
    if (!IsSetupUICalled())
        return;
    const int64_t now = esp_timer_get_time();
    const bool idle = state == kDeviceStateIdle && !media_active;
    const bool refresh = now >= content_at_;
    uta::WeatherSnapshot weather;
    if (refresh)
        weather = uta::WeatherService::GetInstance().Get();
    if (!Lock(10))
        return;
    if (!idle_screen_ || !boot_screen_) {
        Unlock();
        return;
    }
    const bool boot =
        now < boot_until_ && (state == kDeviceStateStarting || state == kDeviceStateIdle ||
                              state == kDeviceStateConnecting);
    Visible(boot_screen_, boot);
    // Thông báo, ảnh xem trước và cảnh báo vẫn phải nhìn thấy khi đang rảnh.
    const bool has_message = chat_message_label_ && lv_label_get_text(chat_message_label_)[0];
    const bool overlay_allowed = !boot && !IsVisible(notification_label_) &&
                                 !IsVisible(low_battery_popup_) && !IsVisible(preview_image_);
    const bool eye_context =
        uta::IsEyeConversationState(state) || (idle && now < expression_until_.load());
    const bool eyes_visible = UpdateEyesLocked(state, eye_context && overlay_allowed, now);
    const bool show = idle && overlay_allowed && !has_message && !eyes_visible;
    Visible(idle_screen_, show);
    if (refresh) {
        content_at_ = now + 1000000;
        const time_t current_time = std::time(nullptr);
        tm local{};
        if (localtime_r(&current_time, &local) && local.tm_year >= 125) {
            char clock[8];
            char date[64];
            static constexpr const char* weekdays[]{"Chủ Nhật", "Thứ Hai", "Thứ Ba", "Thứ Tư",
                                                    "Thứ Năm",  "Thứ Sáu", "Thứ Bảy"};
            std::snprintf(clock, sizeof(clock), "%02d:%02d", local.tm_hour, local.tm_min);
            std::snprintf(date, sizeof(date), "%s · %02d/%02d/%04d", weekdays[local.tm_wday],
                          local.tm_mday, local.tm_mon + 1, local.tm_year + 1900);
            lv_label_set_text(clock_label_, clock);
            lv_label_set_text(date_label_, date);
        } else {
            lv_label_set_text(clock_label_, "--:--");
            lv_label_set_text(date_label_, "Đang đồng bộ giờ");
        }
        lv_image_set_src(wifi_image_, uta::UiIcon(wifi_connected ? uta::kWifiConnectedIcon
                                                                 : uta::kWifiOfflineIcon));
        lv_label_set_text(wifi_text_, wifi_connected ? "Wi-Fi" : "Offline");
        lv_obj_set_style_text_color(wifi_text_, lv_color_hex(wifi_connected ? 0x64dfc5 : 0x8b9baa),
                                    0);
        lv_label_set_text(place_label_, weather.location.available ? weather.location.name.c_str()
                                                                   : "Chưa có vị trí");
        if (weather.available) {
            const auto condition = uta::DescribeWeather(weather.reading.code, weather.reading.day);
            char temperature[16];
            std::snprintf(temperature, sizeof(temperature), "%d°",
                          static_cast<int>(std::lround(weather.reading.temperature)));
            lv_label_set_text(temperature_label_, temperature);
            lv_label_set_text(weather_label_,
                              weather.stale ? "Dữ liệu cũ" : condition.description.data());
            lv_image_set_src(weather_image_, uta::UiIcon(static_cast<unsigned>(condition.icon)));
        } else {
            lv_label_set_text(temperature_label_, "--°");
            lv_label_set_text(weather_label_, "Chưa cập nhật");
            lv_image_set_src(weather_image_, uta::UiIcon(0));
        }
    }
    Unlock();
}
