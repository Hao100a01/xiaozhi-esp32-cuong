#pragma once

#include <optional>
#include <string_view>

#include "device_state.h"

namespace uta {

enum class EyeEmotion : unsigned {
    Neutral,
    Listening,
    Happy,
    Joy,
    Laugh,
    Sad,
    SadSoft,
    Angry,
    AngrySoft,
    Surprised,
    Sleepy,
    Blink,
    Wink,
    Curious,
    Thinking,
    Confused,
    Distressed,
    Embarrassed,
    Skeptical,
    Silent,
    Squint,
    Count
};

// Chấp nhận tên emotion từ server và tên GIF gốc, không phụ thuộc thư mục Desktop.
constexpr std::optional<EyeEmotion> FindEyeEmotion(std::string_view name) {
    if (name.ends_with(".gif"))
        name.remove_suffix(4);
#define UTA_EYE(id, stem, file) \
    if (name == stem)           \
        return EyeEmotion::id;
#include "uta_emotion_catalog.inc"
#undef UTA_EYE
    struct Alias {
        std::string_view name;
        EyeEmotion emotion;
    };
    constexpr Alias aliases[]{{"neutral", EyeEmotion::Neutral},
                              {"listening", EyeEmotion::Listening},
                              {"happy", EyeEmotion::Happy},
                              {"joy", EyeEmotion::Joy},
                              {"excited", EyeEmotion::Joy},
                              {"laughing", EyeEmotion::Laugh},
                              {"funny", EyeEmotion::Laugh},
                              {"sad", EyeEmotion::Sad},
                              {"crying", EyeEmotion::SadSoft},
                              {"angry", EyeEmotion::Angry},
                              {"furious", EyeEmotion::Angry},
                              {"annoyed", EyeEmotion::AngrySoft},
                              {"surprised", EyeEmotion::Surprised},
                              {"shocked", EyeEmotion::Surprised},
                              {"sleepy", EyeEmotion::Sleepy},
                              {"tired", EyeEmotion::Sleepy},
                              {"blinking", EyeEmotion::Blink},
                              {"winking", EyeEmotion::Wink},
                              {"silly", EyeEmotion::Wink},
                              {"curious", EyeEmotion::Curious},
                              {"question", EyeEmotion::Curious},
                              {"thinking", EyeEmotion::Thinking},
                              {"confused", EyeEmotion::Confused},
                              {"dizzy", EyeEmotion::Confused},
                              {"scared", EyeEmotion::Distressed},
                              {"afraid", EyeEmotion::Distressed},
                              {"embarrassed", EyeEmotion::Embarrassed},
                              {"nervous", EyeEmotion::Embarrassed},
                              {"skeptical", EyeEmotion::Skeptical},
                              {"confident", EyeEmotion::Skeptical},
                              {"cool", EyeEmotion::Squint},
                              {"relaxed", EyeEmotion::Squint},
                              {"silent", EyeEmotion::Silent},
                              {"loving", EyeEmotion::Joy},
                              {"kissy", EyeEmotion::Wink},
                              {"delicious", EyeEmotion::Happy}};
    for (const auto& alias : aliases) {
        if (name == alias.name)
            return alias.emotion;
    }
    return std::nullopt;
}

constexpr bool IsEyeConversationState(DeviceState state) {
    return state == kDeviceStateListening || state == kDeviceStateConnecting ||
           state == kDeviceStateSpeaking || state == kDeviceStateNotifying;
}

constexpr EyeEmotion ResolveEyeEmotion(EyeEmotion requested, DeviceState state) {
    if (requested != EyeEmotion::Neutral)
        return requested;
    if (state == kDeviceStateListening)
        return EyeEmotion::Listening;
    if (state == kDeviceStateConnecting)
        return EyeEmotion::Thinking;
    return EyeEmotion::Neutral;
}

}  // namespace uta
