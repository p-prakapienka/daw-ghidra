#include "LFO.h"

#include <cstdlib>

// .rodata at 0x270104.
const float LFO::kDivisions[LFO::kDivisionCount] = {
    0.0625f, 0.125f, 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f, 16.0f,
};

namespace {

constexpr float kSecondsPerMinute = 60.0f;
constexpr float kSampleRate = 44100.0f;
constexpr float kScale = 16777215.0f;
constexpr std::int32_t kTwo = 0x01FFFFFE;
constexpr std::int32_t kOne = 0x01000000;

} // namespace

LFO::LFO() {
    OnParamModified();
}

std::int32_t LFO::multiply(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>((static_cast<std::int64_t>(a) * b) >> 24);
}

void LFO::SetBPM(float bpm) {
    bpm_ = bpm;
    OnParamModified();
}

void LFO::OnParamModified() {
    positionOffset_ = 0;

    const int index = static_cast<int>(rate_);
    const float division =
        (index >= 0 && index < kDivisionCount) ? kDivisions[index] : 1.0f;

    // (60 / bpm) seconds per beat, divided by cycles per beat, in samples.
    const float period = ((kSecondsPerMinute / bpm_) / division) * kSampleRate;
    period_ = static_cast<uint>(period);
    if (period_ == 0) {
        period_ = 1;
    }
}

// One-pole smoothing towards the target, used by the sawtooth and square to
// soften their jumps: state = target + 0.99 * (state - target).
std::int32_t LFO::smooth(std::int32_t target) {
    raw_ = target;
    smoothed_ = target + multiply(smoothing_, smoothed_ - target);
    return smoothed_;
}

void LFO::generate(uint songPosition, uint numSamples) {
    position_ = songPosition;
    if (numSamples > kBlockSize) {
        numSamples = kBlockSize;
    }

    if (waveform_ == Waveform::Off) {
        for (uint index = 0; index < numSamples; ++index) {
            output_[index] = 0;
        }
        return;
    }

    // The phase is recomputed from the song position every block rather than
    // carried over, which locks the LFO to the transport.
    const float period = static_cast<float>(period_);
    const uint offset = (positionOffset_ + position_) % period_;
    const auto increment = static_cast<std::int32_t>((4.0f / period) * kScale);
    const auto start = static_cast<std::int32_t>((static_cast<float>(offset) / period) * kScale);
    std::int32_t phase = phaseOffset_ + multiply(start, kCycle);

    switch (waveform_) {
    case Waveform::Sine:
        // Parabolic sine: x * (2 - |x|) over (-2, 2].
        for (uint index = 0; index < numSamples; ++index) {
            phase += increment;
            if (phase > kTwo) {
                phase -= kCycle;
            }
            const std::int32_t shape = multiply(phase, kTwo - std::abs(phase));
            output_[index] = multiply(depth_, shape);
        }
        break;

    case Waveform::Triangle:
        // |2 - |x|| - 1 over [-3, 1).
        for (uint index = 0; index < numSamples; ++index) {
            phase += increment;
            if (phase >= kOne) {
                phase -= kCycle;
            }
            const std::int32_t shape = std::abs(kTwo - std::abs(phase)) - kUnity;
            output_[index] = multiply(depth_, shape);
        }
        break;

    case Waveform::Sawtooth:
        // Falling ramp -x/2 over (-2, 2], smoothed.
        for (uint index = 0; index < numSamples; ++index) {
            phase += increment;
            if (phase > kTwo) {
                phase -= kCycle;
            }
            output_[index] = multiply(depth_, smooth((-phase) >> 1));
        }
        break;

    case Waveform::Square:
        // High for x in [1, 3] of a [0, 4) cycle, smoothed.
        for (uint index = 0; index < numSamples; ++index) {
            phase += increment;
            if (phase >= kCycle + 1) {
                phase -= kCycle;
            }
            const auto shifted = static_cast<std::uint32_t>(phase - kUnity);
            const std::int32_t target =
                shifted <= static_cast<std::uint32_t>(kTwo) ? kUnity : -kUnity;
            output_[index] = multiply(depth_, smooth(target));
        }
        break;

    default:
        // The engine returns without touching the block.
        break;
    }
}
