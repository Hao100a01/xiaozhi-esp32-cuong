#pragma once

#include <cstddef>
#include <cstdint>

#include <lvgl.h>

#include "mimo/MimoEyes.h"
#include "uta_emotions.h"

namespace uta {

// All entry points run under the display lock or inside the LVGL task.
class MimoEyes {
public:
    explicit MimoEyes(lv_obj_t* parent);
    ~MimoEyes();
    MimoEyes(const MimoEyes&) = delete;
    MimoEyes& operator=(const MimoEyes&) = delete;

    bool IsAvailable() const { return timer_ != nullptr; }
    void SetVisible(bool visible);
    void SetEmotion(EyeEmotion emotion);
    void SetActivityState(DeviceState state) { activity_ = state; }
    bool PlayTrick(std::size_t index);
    bool IsTrickPreviewPlaying() const { return playing_ && preview_; }

private:
    void DrawFrame(uint32_t now);
    bool SampleTrick(uint32_t now, mimo::Pose& pose, mimo::Settings& settings);
    void ScheduleTrick(uint32_t now);

    lv_obj_t* canvas_ = nullptr;
    lv_timer_t* timer_ = nullptr;
    lv_draw_buf_t buffer_{};
    void* pixels_ = nullptr;
    mimo::EyeEngine engine_;
    EyeEmotion emotion_ = EyeEmotion::Neutral;
    DeviceState activity_ = kDeviceStateIdle;
    uint32_t frame_at_ = 0;
    uint32_t trick_at_ = 0;
    uint32_t next_trick_at_ = 0;
    std::size_t trick_index_ = 0;
    bool visible_ = false;
    bool playing_ = false;
    bool preview_ = false;
};

}  // namespace uta
