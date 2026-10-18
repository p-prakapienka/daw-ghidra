#include "CustomWaveform.h"

#include <cmath>
#include <vector>

namespace {

// vcvt.s32.f32: truncates, clamps to the int range, NaN to zero.
int toInt(float value) {
    if (value != value) {
        return 0;
    }
    if (value >= 2147483648.0f) {
        return 0x7FFFFFFF;
    }
    if (value <= -2147483648.0f) {
        return -0x7FFFFFFF - 1;
    }
    return static_cast<int>(value);
}

// The peak search both functions share: vcmpe |y|, peak; vmovpl, so a value
// equal to the peak (or a NaN) also replaces it.
float tallest(const std::array<float, CustomWaveform::kPointCount> &heights) {
    float peak = 0.0f;
    for (const float height : heights) {
        const float magnitude = std::fabs(height);
        if (!(magnitude < peak)) {
            peak = magnitude;
        }
    }
    return peak;
}

} // namespace

CustomWaveform::CustomWaveform() = default;

void CustomWaveform::setRawValues(const std::int16_t *values) {
    for (int point = 0; point < kPointCount; ++point) {
        const std::int16_t value = values[point];
        heights_[static_cast<std::size_t>(point)] = static_cast<float>(value);
        if (value != 0) {
            drawn_ = true;
        }
    }
    fitToScreen();
}

void CustomWaveform::getRawValues(std::int16_t *values) const {
    for (int point = 0; point < kPointCount; ++point) {
        values[point] = static_cast<std::int16_t>(toInt(heights_[static_cast<std::size_t>(point)]));
    }
}

void CustomWaveform::fitToScreen() {
    const float half = (kBottom - kTop) * 0.5f;
    const float peak = tallest(heights_);
    if (half > peak) {
        return;
    }
    const float scale = half / peak;
    for (float &height : heights_) {
        height = height * scale;
    }
}

void CustomWaveform::getWaveform(std::int16_t *output, uint numSamples, float level) const {
    std::vector<float> scratch(numSamples);
    const float count = static_cast<float>(numSamples);
    const float step = 660.0f / count;

    const float peak = tallest(heights_);
    const float gain = peak != 0.0f ? ((kTop + kBottom) * 0.5f) / peak : 1.0f;

    if (numSamples == 0) {
        return;
    }

    constexpr int kLast = kPointCount - 1;
    for (uint sample = 0; sample < numSamples; ++sample) {
        const float position = static_cast<float>(static_cast<int>(sample)) * step;
        const int index = toInt(position + 0.5f);
        const float fraction = position - static_cast<float>(index);
        int first = index >= kLast ? kLast : index;
        int second = index + 1 >= kLast ? kLast : index + 1;
        first = first < 0 ? 0 : first;
        second = second < 0 ? 0 : second;

        const float from = heights_[static_cast<std::size_t>(first)];
        float value = from + fraction * (heights_[static_cast<std::size_t>(second)] - from);
        value = value / ((kTop + kBottom) * 0.5f);
        value = value * gain;
        value = value * level;
        scratch[sample] = value;
    }

    const float inverse = 1.0f / count;
    float mean = 0.0f;
    for (const float value : scratch) {
        mean = mean + value * inverse;
    }
    for (uint sample = 0; sample < numSamples; ++sample) {
        output[sample] = static_cast<std::int16_t>(toInt((scratch[sample] - mean) * 32767.0f));
    }
}
