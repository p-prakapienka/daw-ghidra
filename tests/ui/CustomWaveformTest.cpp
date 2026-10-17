#include "ui/CustomWaveform.h"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>

namespace {

std::array<std::int16_t, CustomWaveform::kPointCount> sine(int amplitude) {
    std::array<std::int16_t, CustomWaveform::kPointCount> raw{};
    for (int point = 0; point < CustomWaveform::kPointCount; ++point) {
        raw[static_cast<std::size_t>(point)] = static_cast<std::int16_t>(
            amplitude * std::sin(2.0 * 3.141592653589793 * point / CustomWaveform::kPointCount));
    }
    return raw;
}

} // namespace

TEST(CustomWaveform, StartsFlatAndSilent) {
    const CustomWaveform waveform;
    EXPECT_FALSE(waveform.isDrawn());
    std::array<std::int16_t, 64> table{};
    table.fill(1);
    waveform.getWaveform(table.data(), 64, 0.3f);
    for (const std::int16_t value : table) {
        EXPECT_EQ(value, 0);
    }
}

TEST(CustomWaveform, FitsTallShapesIntoHalfTheArea) {
    CustomWaveform waveform;
    const auto raw = sine(300);
    waveform.setRawValues(raw.data());
    EXPECT_TRUE(waveform.isDrawn());
    float peak = 0.0f;
    for (int point = 0; point < CustomWaveform::kPointCount; ++point) {
        peak = std::fmax(peak, std::fabs(waveform.height(point)));
    }
    EXPECT_FLOAT_EQ(peak, 157.5f);

    // A shape that already fits is left alone.
    const auto small = sine(100);
    waveform.setRawValues(small.data());
    std::array<std::int16_t, CustomWaveform::kPointCount> back{};
    waveform.getRawValues(back.data());
    EXPECT_EQ(back, small);
}

TEST(CustomWaveform, NormalisesToThePeakAndRemovesTheMean) {
    CustomWaveform waveform;
    std::array<std::int16_t, CustomWaveform::kPointCount> raw{};
    raw.fill(40);
    raw[100] = 80;
    waveform.setRawValues(raw.data());
    std::array<std::int16_t, 4096> table{};
    waveform.getWaveform(table.data(), 4096, 0.3f);
    long long sum = 0;
    int top = -32768;
    for (const std::int16_t value : table) {
        sum += value;
        top = std::max<int>(top, value);
    }
    EXPECT_LT(std::llabs(sum), 4096);  // the mean is gone, up to truncation
    // Interpolating from the nearest point extrapolates past the spike, so
    // the table peaks above (1 - mean) * level * 32767.
    EXPECT_GT(top, static_cast<int>((1.0f - 0.5f) * 0.3f * 32767.0f));
}
