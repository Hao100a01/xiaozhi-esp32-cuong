#include "main/boards/UTA-robot/uta_emotions.h"

using uta::EyeEmotion;
using uta::FindEyeEmotion;
using uta::IsEyeConversationState;
using uta::ResolveEyeEmotion;

static_assert(static_cast<unsigned>(EyeEmotion::Count) == 21);
static_assert(FindEyeEmotion("neutral") == EyeEmotion::Neutral);
static_assert(FindEyeEmotion("binhthuong.gif") == EyeEmotion::Listening);
static_assert(FindEyeEmotion("buon2") == EyeEmotion::SadSoft);
static_assert(FindEyeEmotion("thinking") == EyeEmotion::Thinking);
static_assert(!FindEyeEmotion("gear"));  // Giữ fallback biểu tượng chức năng.
static_assert(!FindEyeEmotion("unknown"));
static_assert(!FindEyeEmotion(""));
static_assert(ResolveEyeEmotion(EyeEmotion::Neutral, kDeviceStateListening) ==
              EyeEmotion::Listening);
static_assert(ResolveEyeEmotion(EyeEmotion::Neutral, kDeviceStateConnecting) ==
              EyeEmotion::Thinking);
static_assert(ResolveEyeEmotion(EyeEmotion::Sad, kDeviceStateListening) == EyeEmotion::Sad);
static_assert(ResolveEyeEmotion(EyeEmotion::Sad, kDeviceStateSpeaking) == EyeEmotion::Sad);
static_assert(ResolveEyeEmotion(EyeEmotion::Neutral, kDeviceStateSpeaking) == EyeEmotion::Neutral);
static_assert(IsEyeConversationState(kDeviceStateListening));
static_assert(IsEyeConversationState(kDeviceStateSpeaking));
static_assert(!IsEyeConversationState(kDeviceStateIdle));
static_assert(!IsEyeConversationState(kDeviceStateWifiConfiguring));
static_assert(!IsEyeConversationState(kDeviceStateUpgrading));
static_assert(!IsEyeConversationState(kDeviceStateFatalError));
