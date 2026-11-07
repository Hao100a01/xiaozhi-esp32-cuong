#include "uta_display.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <new>

#include <esp_log.h>
#include <esp_timer.h>

#include "uta_emotion_assets.h"
#include "uta_ui_assets.h"

namespace {
void Hide(lv_obj_t* object) {
    if (object)
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
}

void SetTextIfChanged(lv_obj_t* label, const char* text) {
    if (std::strcmp(lv_label_get_text(label), text) != 0)
        lv_label_set_text(label, text);
}

const char* ConversationCaption(DeviceState state) {
    switch (state) {
        case kDeviceStateListening:
            return "Đang lắng nghe";
        case kDeviceStateConnecting:
            return "Đang kết nối";
        case kDeviceStateSpeaking:
            return "Đang trả lời";
        case kDeviceStateNotifying:
            return "Đang thông báo";
        default:
            return "ATFUTA";
    }
}
}  // namespace

UtaDisplay::~UtaDisplay() {
    DisplayLockGuard lock(this);
    ReleaseEyesLocked();
}

bool UtaDisplay::SetEmotion(const char* emotion) {
    if (!emotion || !IsSetupUICalled())
        return false;
    if (const auto eye = uta::FindEyeEmotion(emotion)) {
        // Chỉ gửi yêu cầu nhỏ từ event loop; tác vụ điều khiển tải GIF dưới khóa LVGL.
        requested_eye_.store(*eye);
        expression_until_.store(*eye == uta::EyeEmotion::Neutral ? 0
                                                                 : esp_timer_get_time() + 5000000);
        use_base_emotion_.store(false);
        return true;
    }
    // Giữ các biểu tượng chuyên dụng: gear, cảnh báo, nâng cấp và tài nguyên khác.
    // Đánh dấu trước khi hàm gốc nhả khóa để tác vụ điều khiển không xóa GIF mới.
    const bool previous_mode = use_base_emotion_.exchange(true);
    const bool accepted = SpiLcdDisplay::SetEmotion(emotion);
    if (!accepted)
        use_base_emotion_.store(previous_mode);
    return accepted;
}

void UtaDisplay::SetupEyesLocked() {
    eyes_screen_ = lv_obj_create(lv_display_get_layer_top(display_));
    lv_obj_remove_style_all(eyes_screen_);
    lv_obj_set_size(eyes_screen_, 240, 240);
    lv_obj_center(eyes_screen_);
    lv_obj_remove_flag(eyes_screen_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(eyes_screen_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(eyes_screen_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(eyes_screen_, LV_OPA_COVER, 0);
    eyes_image_ = lv_image_create(eyes_screen_);
    lv_obj_center(eyes_image_);
    auto label = [this](int y, uint32_t color) {
        auto* object = lv_label_create(eyes_screen_);
        lv_obj_set_pos(object, 16, y);
        lv_obj_set_size(object, 208, uta::uta_font_small.line_height);
        lv_obj_set_style_text_font(object, &uta::uta_font_small, 0);
        lv_obj_set_style_text_color(object, lv_color_hex(color), 0);
        lv_obj_set_style_text_align(object, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(object, LV_LABEL_LONG_DOT);
        lv_label_set_text(object, "");
        return object;
    };
    eyes_status_ = label(10, 0x98b3c8);
    eyes_subtitle_ = label(210, 0xeef8ff);
    Hide(eyes_screen_);
}

void UtaDisplay::ReleaseEyesLocked() {
    if (eyes_controller_) {
        eyes_controller_->Pause();
        eyes_controller_->SetFrameCallback({});
        if (const auto* image = eyes_controller_->image_dsc())
            lv_image_cache_drop(image);
        if (eyes_image_)
            lv_image_set_src(eyes_image_, nullptr);
        eyes_controller_.reset();
    }
    eyes_gif_data_.reset();
    loaded_eye_ = uta::EyeEmotion::Count;
}

bool UtaDisplay::UpdateEyesLocked(DeviceState state, bool visible, int64_t now) {
    if (!eyes_screen_)
        return false;
    if (!visible || use_base_emotion_.load()) {
        if (eyes_controller_ && eyes_controller_->IsPlaying())
            eyes_controller_->Pause();
        Hide(eyes_screen_);
        return false;
    }
    const auto emotion = uta::ResolveEyeEmotion(requested_eye_.load(), state);
    if (emotion == failed_eye_ && now < eye_retry_at_) {
        Hide(eyes_screen_);
        return false;
    }
    // Fallback có thể vừa tạo GIF mới dù decoder mắt cũ vẫn được giữ để resume.
    if (gif_controller_) {
        gif_controller_->Pause();
        if (const auto* image = gif_controller_->image_dsc())
            lv_image_cache_drop(image);
        lv_image_set_src(emoji_image_, nullptr);
        gif_controller_.reset();
    }
    if (!eyes_controller_ || loaded_eye_ != emotion) {
        ReleaseEyesLocked();
        lv_image_dsc_t asset{};
        if (uta::LoadEyeGif(emotion, asset)) {
            eyes_gif_data_.reset(static_cast<uint8_t*>(
                heap_caps_malloc(asset.data_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
            if (eyes_gif_data_) {
                std::memcpy(eyes_gif_data_.get(), asset.data, asset.data_size);
                asset.data = eyes_gif_data_.get();
                eyes_controller_.reset(new (std::nothrow) LvglGif(&asset));
            }
        }
        if (eyes_controller_ && eyes_controller_->IsLoaded()) {
            eyes_controller_->SetLoopCount(0);
            eyes_controller_->SetFrameCallback([this]() {
                lv_image_set_src(eyes_image_, eyes_controller_->image_dsc());
                lv_obj_invalidate(eyes_image_);
            });
            lv_image_set_src(eyes_image_, eyes_controller_->image_dsc());
            eyes_controller_->Start();
        }
        if (!eyes_controller_ || !eyes_controller_->IsLoaded() || !eyes_controller_->IsPlaying()) {
            ReleaseEyesLocked();
            failed_eye_ = emotion;
            eye_retry_at_ = now + 30000000;
            Hide(eyes_screen_);
            ESP_LOGW("UTA_Emotion", "Chưa tải được GIF mắt; giữ giao diện gốc");
            return false;
        }
        loaded_eye_ = emotion;
        failed_eye_ = uta::EyeEmotion::Count;
    } else if (!eyes_controller_->IsPlaying()) {
        eyes_controller_->Resume();
    }
    SetTextIfChanged(eyes_status_, ConversationCaption(state));
    const char* text = chat_message_label_ ? lv_label_get_text(chat_message_label_) : "";
    std::array<char, 193> subtitle{};
    const size_t length = std::strlen(text);
    size_t count = std::min(length, subtitle.size() - 4);
    // Không cắt ở giữa một ký tự UTF-8; giới hạn bộ nhớ cho phụ đề dài từ mạng.
    while (count && (static_cast<unsigned char>(text[count]) & 0xc0) == 0x80)
        --count;
    std::memcpy(subtitle.data(), text, count);
    if (count < length)
        std::memcpy(subtitle.data() + count, "...", 3);
    SetTextIfChanged(eyes_subtitle_, subtitle.data());
    lv_obj_remove_flag(eyes_screen_, LV_OBJ_FLAG_HIDDEN);
    return true;
}
