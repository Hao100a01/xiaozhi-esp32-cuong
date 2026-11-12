#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "device_state.h"

namespace uta {

constexpr bool CanPreviewEyeTrick(DeviceState state) {
    return state == kDeviceStateIdle || state == kDeviceStateListening ||
           state == kDeviceStateSpeaking;
}

// Each clip starts and ends at the neutral pose. Intermediate poses are eased.
struct EyeTrickPose {
    int8_t x = 0;
    int8_t y = 0;
    int8_t spread = 0;
    int8_t width_delta = 0;
    int8_t height_delta = 0;
    uint8_t left_open = 100;
    uint8_t right_open = 100;
};

struct EyeTrick {
    std::string_view id;
    const char* label;
    uint16_t step_ms;
    uint8_t pose_count;
    EyeTrickPose poses[5];
};

inline constexpr EyeTrick kEyeTricks[] = {
    {"peek_left", "Liếc trái", 240, 2,
     {{-27, 0, 0, 0, 0, 65, 100}, {-20, -9, 0, 0, 0, 100, 100}}},
    {"peek_right", "Liếc phải", 240, 2,
     {{27, 0, 0, 0, 0, 100, 65}, {20, -9, 0, 0, 0, 100, 100}}},
    {"look_up", "Ngước lên", 220, 2,
     {{0, -26, 0, -5, 12, 100, 100}, {7, -22, 0, -5, 12, 100, 100}}},
    {"look_down", "Cúi nhìn", 260, 1,
     {{0, 24, 0, 6, -12, 90, 90}}},
    {"scan", "Quét trái phải", 235, 3,
     {{-27, 0, 0, 0, 0, 100, 100}, {0, -8, 0, 0, 0, 100, 100}, {27, 0, 0, 0, 0, 100, 100}}},
    {"orbit", "Đảo mắt vòng tròn", 220, 5,
     {{0, -23, 0, 0, 0, 100, 100}, {22, 0, 0, 0, 0, 100, 100}, {0, 23, 0, 0, 0, 100, 100}, {-22, 0, 0, 0, 0, 100, 100}, {0, -23, 0, 0, 0, 100, 100}}},
    {"cross", "Mắt lé", 250, 2,
     {{0, 0, -18, -14, 0, 100, 100}, {0, 5, -20, -18, 10, 100, 100}}},
    {"wide", "Mắt tách xa", 250, 2,
     {{0, 0, 21, -12, 0, 100, 100}, {0, -7, 17, -8, 0, 100, 100}}},
    {"wink_left", "Nháy mắt trái", 175, 2,
     {{0, 0, 0, 0, 0, 0, 100}, {0, -5, 0, 0, 0, 20, 100}}},
    {"wink_right", "Nháy mắt phải", 175, 2,
     {{0, 0, 0, 0, 0, 100, 0}, {0, -5, 0, 0, 0, 100, 20}}},
    {"double_blink", "Chớp đôi", 125, 3,
     {{0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 100, 100}, {0, 0, 0, 0, 0, 0, 0}}},
    {"alternating", "Chớp luân phiên", 165, 3,
     {{0, 0, 0, 0, 0, 0, 100}, {0, 0, 0, 0, 0, 100, 0}, {0, 0, 0, 0, 0, 0, 100}}},
    {"sleep", "Ngủ gật", 380, 3,
     {{0, 10, 0, 5, 0, 45, 45}, {0, 19, 0, 8, 0, 6, 6}, {0, 22, 0, 8, 0, 0, 0}}},
    {"wake", "Giật mình thức giấc", 220, 3,
     {{0, 15, 0, 0, 0, 3, 3}, {0, -19, 0, 11, 20, 100, 100}, {0, -5, 0, -5, 10, 100, 100}}},
    {"bounce", "Nhảy tưng tưng", 180, 4,
     {{0, 11, 0, 9, -22, 100, 100}, {0, -24, 0, -7, 12, 100, 100}, {0, 12, 0, 9, -24, 100, 100}, {0, -14, 0, -6, 8, 100, 100}}},
    {"stretch", "Vươn vai", 310, 3,
     {{0, -4, 0, -15, 25, 100, 100}, {0, -8, 0, -18, 31, 100, 100}, {0, 8, 0, 13, -22, 100, 100}}},
    {"squish", "Ép dẹt", 240, 3,
     {{0, 7, 0, 16, -34, 100, 100}, {0, 13, 0, 19, -52, 100, 100}, {0, -5, 0, -9, 14, 100, 100}}},
    {"shake", "Lắc ngang", 140, 4,
     {{-19, 0, 0, -5, -8, 100, 100}, {19, 0, 0, -5, -8, 100, 100}, {-16, 0, 0, -5, -8, 100, 100}, {13, 0, 0, -5, -8, 100, 100}}},
    {"nod", "Gật gù", 200, 4,
     {{0, -15, 0, 0, 4, 100, 100}, {0, 18, 0, 4, -12, 88, 88}, {0, -11, 0, 0, 3, 100, 100}, {0, 13, 0, 4, -10, 90, 90}}},
    {"suspicious", "Nheo mắt nghi ngờ", 360, 2,
     {{-9, 2, 0, 7, 0, 35, 75}, {10, 3, 0, 7, 0, 75, 35}}},
    {"surprise", "Tròn mắt ngạc nhiên", 270, 2,
     {{0, -8, 3, 12, 24, 100, 100}, {0, -8, 3, 12, 24, 100, 100}}},
    {"laugh", "Cười rung mắt", 150, 4,
     {{0, 8, 0, 14, -28, 65, 65}, {0, -4, 0, 14, -28, 70, 70}, {0, 10, 0, 14, -28, 60, 60}, {0, -3, 0, 14, -28, 70, 70}}},
    {"dizzy", "Choáng váng", 190, 4,
     {{16, -12, 3, -8, 5, 45, 100}, {-17, 14, -4, -5, 8, 100, 45}, {16, 11, 3, -8, 5, 45, 100}, {-13, -12, -4, -5, 8, 100, 45}}},
    {"heartbeat", "Nhịp tim", 170, 4,
     {{0, 0, 2, 10, 16, 100, 100}, {0, 0, 0, -4, -7, 100, 100}, {0, 0, 3, 14, 21, 100, 100}, {0, 0, 0, -4, -7, 100, 100}}},
};

inline constexpr std::size_t kEyeTrickCount = sizeof(kEyeTricks) / sizeof(kEyeTricks[0]);

}  // namespace uta
