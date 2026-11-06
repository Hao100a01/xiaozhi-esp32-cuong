#include "uta_emotion_assets.h"

#include <array>
#include <cstring>

#include "assets.h"

namespace uta {

bool LoadEyeGif(EyeEmotion emotion, lv_image_dsc_t& image) {
    static constexpr std::array<const char*, static_cast<unsigned>(EyeEmotion::Count)> names{{
#define UTA_EYE(id, stem, file) "uta_eye_" file,
#include "uta_emotion_catalog.inc"
#undef UTA_EYE
    }};
    const auto index = static_cast<unsigned>(emotion);
    void* data = nullptr;
    size_t size = 0;
    if (index >= names.size() || !Assets::GetInstance().GetAssetData(names[index], data, size) ||
        size < 14 || size > 256 * 1024 ||
        (std::memcmp(data, "GIF89a", 6) != 0 && std::memcmp(data, "GIF87a", 6) != 0))
        return false;
    const auto* bytes = static_cast<const uint8_t*>(data);
    if (bytes[6] != 240 || bytes[7] != 0 || bytes[8] != 240 || bytes[9] != 0)
        return false;

    image = {};
    image.header.magic = LV_IMAGE_HEADER_MAGIC;
    image.header.cf = LV_COLOR_FORMAT_RAW_ALPHA;
    image.header.w = 240;
    image.header.h = 240;
    image.data = bytes;
    image.data_size = size;
    return true;
}

}  // namespace uta
