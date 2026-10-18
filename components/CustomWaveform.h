#pragma once

// The waveform half of Caustic's custom waveform editor: 660 drawn points and
// the conversion that turns them into an oscillator table.
//
// Recovered from UICustomWaveformDialog's constructor, SetRawValues,
// GetRawValues, FitToScreen and GetWaveform(short*, unsigned int, float) in
// libcaustic.so (ARMv7). See components/CustomWaveform.md. The dialog's
// drawing, touch handling and editing tools are not part of this class.

#include <array>
#include <cstdint>

using uint = unsigned int;

class CustomWaveform {
public:
    // Points across the editor, one per screen column.
    static constexpr int kPointCount = 660;

    // The editor's drawing area in the UI's fixed coordinates, written by the
    // constructor: x from 12 to 672 (+0x324, +0x32C), y from 50 to 365
    // (+0x328, +0x330). GetWaveform and FitToScreen use the two y values.
    static constexpr float kLeft = 12.0f;
    static constexpr float kTop = 50.0f;
    static constexpr float kRight = 672.0f;
    static constexpr float kBottom = 365.0f;

    // Every point at height 0, as the constructor leaves them.
    CustomWaveform();

    // SetRawValues: each height from a short, then FitToScreen. Any non-zero
    // value marks the waveform as drawn (+0x31C).
    void setRawValues(const std::int16_t *values);
    // GetRawValues: each height truncated to a short.
    void getRawValues(std::int16_t *values) const;

    // FitToScreen: if the tallest point is at or beyond half the drawing
    // area's height, scale every point down so it just fits.
    void fitToScreen();

    // GetWaveform(short*, unsigned int, float): resample the points to
    // numSamples, normalise to the tallest point, scale by level, remove the
    // mean and convert to Q15.
    void getWaveform(std::int16_t *output, uint numSamples, float level) const;

    float height(int point) const { return heights_[static_cast<std::size_t>(point)]; }
    bool isDrawn() const { return drawn_; }

private:
    // +0x374 + 8i; the x at +0x370 + 8i is the point's index.
    std::array<float, kPointCount> heights_{};
    bool drawn_ = false; // +0x31C
};
