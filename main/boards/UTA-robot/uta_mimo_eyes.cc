#include "uta_mimo_eyes.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <esp_heap_caps.h>
#include <esp_random.h>

#include "uta_eye_tricks.h"

namespace uta {
namespace {
constexpr int kWidth = 240;
constexpr int kHeight = 144;
constexpr float kScale = 0.6f;
constexpr uint16_t kCyan = 0x375f;

// Mimo emits triangles and lines. Rasterize into LVGL's reusable RGB565 image.
class Canvas {
public:
    explicit Canvas(lv_draw_buf_t& buffer) : buffer_(buffer) {}
    int width() const { return kWidth; }
    int height() const { return kHeight; }

    void fillTriangle(float ax, float ay, float bx, float by, float cx, float cy, uint16_t color) {
        mimo::Point a{ax, ay}, b{bx, by}, c{cx, cy};
        if (a.y > b.y)
            std::swap(a, b);
        if (b.y > c.y)
            std::swap(b, c);
        if (a.y > b.y)
            std::swap(a, b);
        if (a.y == c.y)
            return;
        const auto slope = [](mimo::Point from, mimo::Point to) {
            return to.y == from.y ? 0.0f : (to.x - from.x) / (to.y - from.y);
        };
        const float ac = slope(a, c), ab = slope(a, b), bc = slope(b, c);
        const int first = std::max(0, static_cast<int>(std::ceil(a.y - 0.5f)));
        const int last = std::min(kHeight, static_cast<int>(std::ceil(c.y - 0.5f)));
        for (int y = first; y < last; ++y) {
            const float row = y + 0.5f;
            const float x1 = a.x + (row - a.y) * ac;
            const float x2 = row < b.y ? a.x + (row - a.y) * ab : b.x + (row - b.y) * bc;
            const int left = std::max(0, static_cast<int>(std::ceil(std::min(x1, x2) - 0.5f)));
            const int right =
                std::min(kWidth, static_cast<int>(std::ceil(std::max(x1, x2) - 0.5f)));
            if (left < right)
                std::fill(Row(y) + left, Row(y) + right, color);
        }
    }

    void drawLine(float ax, float ay, float bx, float by, uint16_t color) {
        int x = std::lround(ax), y = std::lround(ay);
        const int end_x = std::lround(bx), end_y = std::lround(by);
        const int dx = std::abs(end_x - x), dy = -std::abs(end_y - y);
        const int sx = x < end_x ? 1 : -1, sy = y < end_y ? 1 : -1;
        int error = dx + dy;
        for (;;) {
            if (x >= 0 && x < kWidth && y >= 0 && y < kHeight)
                Row(y)[x] = color;
            if (x == end_x && y == end_y)
                break;
            const int doubled = 2 * error;
            if (doubled >= dy) {
                error += dy;
                x += sx;
            }
            if (doubled <= dx) {
                error += dx;
                y += sy;
            }
        }
    }

private:
    uint16_t* Row(int y) {
        return reinterpret_cast<uint16_t*>(buffer_.data + y * buffer_.header.stride);
    }

    lv_draw_buf_t& buffer_;
};

mimo::Emotion MimoEmotion(EyeEmotion emotion) {
    using E = EyeEmotion;
    using M = mimo::Emotion;
    switch (emotion) {
        case E::Listening: return M::Focus;
        case E::Happy: return M::Happy;
        case E::Joy: return M::Excited;
        case E::Laugh: return M::Sparkle;
        case E::Sad:
        case E::SadSoft: return M::Sad;
        case E::Angry: return M::Angry;
        case E::AngrySoft: return M::Mischief;
        case E::Surprised: return M::Surprised;
        case E::Sleepy: return M::Sleepy;
        case E::Curious:
        case E::Thinking: return M::Curious;
        case E::Confused: return M::Confused;
        case E::Distressed: return M::Dizzy;
        case E::Embarrassed: return M::Shy;
        case E::Skeptical:
        case E::Squint: return M::Suspicious;
        case E::Silent: return M::Sleep;
        default: return M::Neutral;
    }
}
}  // namespace

MimoEyes::MimoEyes(lv_obj_t* parent) {
#if LV_USE_CANVAS
    constexpr uint32_t kFrameMs = 33;
    const auto stride = lv_draw_buf_width_to_stride(kWidth, LV_COLOR_FORMAT_RGB565);
    const auto bytes = stride * kHeight;
    pixels_ = heap_caps_aligned_alloc(std::max<size_t>(LV_DRAW_BUF_ALIGN, alignof(uint16_t)),
                                     bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!pixels_ ||
        lv_draw_buf_init(&buffer_, kWidth, kHeight, LV_COLOR_FORMAT_RGB565, stride, pixels_,
                         bytes) != LV_RESULT_OK)
        return;
    canvas_ = lv_canvas_create(parent);
    if (!canvas_)
        return;
    std::memset(buffer_.data, 0, bytes);
    lv_canvas_set_draw_buf(canvas_, &buffer_);
    lv_obj_center(canvas_);
    lv_obj_add_flag(canvas_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(canvas_, LV_OBJ_FLAG_CLICKABLE);
    engine_.settings.width = 56;
    engine_.settings.height = 100;
    engine_.settings.gap = 146;
    timer_ = lv_timer_create([](lv_timer_t* timer) {
        auto* eyes = static_cast<MimoEyes*>(lv_timer_get_user_data(timer));
        eyes->DrawFrame(lv_tick_get());
    }, kFrameMs, this);
    if (timer_)
        lv_timer_pause(timer_);
#else
    (void)parent;
#endif
}

MimoEyes::~MimoEyes() {
    if (timer_)
        lv_timer_delete(timer_);
    if (canvas_)
        lv_obj_delete(canvas_);
    heap_caps_free(pixels_);
}

void MimoEyes::SetVisible(bool visible) {
    if (!IsAvailable() || visible_ == visible)
        return;
    visible_ = visible;
    if (visible) {
        frame_at_ = lv_tick_get();
        ScheduleTrick(frame_at_);
        lv_obj_remove_flag(canvas_, LV_OBJ_FLAG_HIDDEN);
        lv_timer_resume(timer_);
        lv_timer_reset(timer_);
        DrawFrame(frame_at_);
    } else {
        lv_timer_pause(timer_);
        lv_obj_add_flag(canvas_, LV_OBJ_FLAG_HIDDEN);
        playing_ = false;
    }
}

void MimoEyes::SetEmotion(EyeEmotion emotion) {
    if (emotion_ == emotion)
        return;
    emotion_ = emotion;
    engine_.setEmotion(MimoEmotion(emotion));
    auto gesture = mimo::Gesture::None;
    if (emotion == EyeEmotion::Blink)
        gesture = mimo::Gesture::Blink;
    else if (emotion == EyeEmotion::Wink)
        gesture = mimo::Gesture::Wink;
    engine_.playGesture(gesture);
}

bool MimoEyes::PlayTrick(std::size_t index) {
    if (!visible_ || !CanPreviewEyeTrick(activity_) || index >= kEyeTrickCount)
        return false;
    trick_index_ = index;
    trick_at_ = lv_tick_get();
    playing_ = preview_ = true;
    return true;
}

void MimoEyes::ScheduleTrick(uint32_t now) {
    next_trick_at_ = now + 12000 + esp_random() % 10000;
}

bool MimoEyes::SampleTrick(uint32_t now, mimo::Pose& pose, mimo::Settings& settings) {
    const bool idle = activity_ == kDeviceStateIdle && emotion_ == EyeEmotion::Neutral;
    if (!idle && !(IsTrickPreviewPlaying() && CanPreviewEyeTrick(activity_))) {
        if (playing_)
            ScheduleTrick(now);
        playing_ = false;
        return false;
    }
    if (!playing_ && now - next_trick_at_ < 0x80000000u) {
        trick_index_ = (trick_index_ + 1 + esp_random() % (kEyeTrickCount - 1)) % kEyeTrickCount;
        trick_at_ = now;
        playing_ = true;
        preview_ = false;
    }
    if (!playing_)
        return false;
    const auto& trick = kEyeTricks[trick_index_];
    const auto elapsed = now - trick_at_;
    const auto segment = elapsed / trick.step_ms;
    if (segment > trick.pose_count) {
        playing_ = false;
        ScheduleTrick(now);
        return false;
    }

    constexpr EyeTrickPose neutral{};
    const auto& from = segment == 0 ? neutral : trick.poses[segment - 1];
    const auto& to = segment == trick.pose_count ? neutral : trick.poses[segment];
    const float t = static_cast<float>(elapsed % trick.step_ms) / trick.step_ms;
    const float eased = t * t * (3 - 2 * t);
    const auto blend = [eased](int a, int b) { return a + (b - a) * eased; };
    pose = {};
    pose.x = blend(from.x, to.x) / kScale;
    pose.y = blend(from.y, to.y) / kScale;
    settings.gap += 2 * blend(from.spread, to.spread) / kScale;
    const float width_scale = 1 + blend(from.width_delta, to.width_delta) / 62.0f;
    const float height_scale = 1 + blend(from.height_delta, to.height_delta) / 78.0f;
    pose.lw = pose.rw = width_scale;
    pose.lh = std::max(0.035f, height_scale * blend(from.left_open, to.left_open) / 100);
    pose.rh = std::max(0.035f, height_scale * blend(from.right_open, to.right_open) / 100);
    return true;
}

void MimoEyes::DrawFrame(uint32_t now) {
    const auto delta = now - frame_at_;
    frame_at_ = now;
    auto settings = engine_.settings;
    mimo::Pose pose;
    if (!SampleTrick(now, pose, settings))
        pose = engine_.update(delta);
    std::memset(buffer_.data, 0, buffer_.header.stride * kHeight);
    Canvas canvas(buffer_);
    mimo::render(canvas, pose, settings, kCyan);
    lv_obj_invalidate(canvas_);
}

}  // namespace uta
