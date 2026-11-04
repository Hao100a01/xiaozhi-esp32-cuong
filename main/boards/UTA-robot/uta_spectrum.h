#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace uta {
inline constexpr size_t kSpectrumBands = 16;
using Spectrum = std::array<uint8_t, kSpectrumBands>;

// Đo năng lượng ở 16 tần số bằng Goertzel, cửa sổ Hann, không cấp phát bộ nhớ.
// Đầu vào là PCM mono đã đổi về tần số loa; kết quả 0..100 theo mức dBFS.
inline Spectrum AnalyzeSpectrum(const int16_t* pcm, size_t count, int rate) {
    Spectrum result{};
    count = std::min<size_t>(count, 1024);
    if (!pcm || count < 32 || rate <= 0)
        return result;
    constexpr float pi = 3.14159265358979323846f;
    const float window_cos = std::cos(2.0f * pi / (count - 1));
    const float window_sin = std::sin(2.0f * pi / (count - 1));
    for (size_t band = 0; band < kSpectrumBands; ++band) {
        const float frequency = 80.0f * std::pow(100.0f, static_cast<float>(band) / 15.0f);
        if (frequency >= rate * 0.48f)
            continue;
        const float coefficient = 2.0f * std::cos(2.0f * pi * frequency / rate);
        float previous = 0, before_previous = 0;
        float phase_cos = 1, phase_sin = 0;
        for (size_t i = 0; i < count; ++i) {
            const float window = 0.5f - 0.5f * phase_cos;
            const float next_cos = phase_cos * window_cos - phase_sin * window_sin;
            phase_sin = phase_sin * window_cos + phase_cos * window_sin;
            phase_cos = next_cos;
            const float next =
                pcm[i] / 32768.0f * window + coefficient * previous - before_previous;
            before_previous = previous;
            previous = next;
        }
        const float power = std::max(0.0f, previous * previous + before_previous * before_previous -
                                               coefficient * previous * before_previous);
        const float amplitude = 4.0f * std::sqrt(power) / count;
        const float db = 20.0f * std::log10(std::max(amplitude, 0.000001f));
        result[band] =
            static_cast<uint8_t>(std::clamp((db + 60.0f) * 100.0f / 60.0f, 0.0f, 100.0f));
    }
    return result;
}
}  // namespace uta
