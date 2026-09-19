#include "StateVariableFilter.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

// Constants from the constructor's pool at 0x9497c .. 0x9498c.
constexpr float kTableStep = 0.00390625f;     // 1 / 256
constexpr float kCutoffOffset = 0.36f;
constexpr float kCutoffSpan = 0.64f;
constexpr float kCutoffExponent = 8.0f;
constexpr float kDampingExponent = 4.0f;
constexpr float kExtendedDampingBase = 1.5f;
constexpr float kQ24Scale = 16777215.0f;

// Fixed trim applied to the band-pass tap, loaded as an immediate at 0x95140.
constexpr int kBandPassTrim = 0xE66665; // 0.9 in Q24

} // namespace

StateVariableFilter::StateVariableFilter(bool extendedResonance) {
    const float dampingBase = extendedResonance ? kExtendedDampingBase : 0.0f;

    for (int index = 0; index < kTableSize; ++index) {
        const float position = static_cast<float>(index) * kTableStep;

        // Cutoff rises exponentially towards unity at the top of the table.
        const float cutoff = 1.0f - (kCutoffOffset + position * kCutoffSpan);
        cutoffTable_[index] =
            static_cast<int>(std::pow(0.5f, kCutoffExponent * cutoff) * kQ24Scale);

        // Damping falls as resonance rises, so a high index is a sharp filter.
        dampingTable_[index] = static_cast<int>(
            std::pow(0.5f, dampingBase + position * kDampingExponent) * kQ24Scale);
    }

    cutoffCoefficient_ = cutoffTable_[0];
    dampingCoefficient_ = dampingTable_[0];
}

int StateVariableFilter::multiply(int a, int b) {
    const auto product = static_cast<std::int64_t>(a) * static_cast<std::int64_t>(b);
    return static_cast<int>(product >> 24);
}

void StateVariableFilter::setCutoff(float cutoff) {
    // Below the floor the engine indexes the first entry rather than
    // extrapolating the curve.
    if (!(cutoff >= kMinimumCutoff)) {
        cutoffCoefficient_ = cutoffTable_[0];
        return;
    }

    const float clamped = std::min(cutoff, 1.0f);
    const int index = std::clamp(static_cast<int>(clamped * (kTableSize - 1)), 0, kTableSize - 1);
    cutoffCoefficient_ = cutoffTable_[index];
}

void StateVariableFilter::setResonance(float resonance) {
    const float clamped = std::clamp(resonance, 0.0f, 1.0f);
    const int index = std::clamp(static_cast<int>(clamped * (kTableSize - 1)), 0, kTableSize - 1);
    dampingCoefficient_ = dampingTable_[index];
}

void StateVariableFilter::setInputGain(float gain) {
    inputGain_ = static_cast<int>(std::clamp(gain, 0.0f, 1.0f) * kQ24Scale);
}

void StateVariableFilter::reset() {
    left_ = Channel{};
    right_ = Channel{};
}

// One integrator pass. Both states are leaky by the same 1 - f*q factor, which
// is where this departs from a textbook Chamberlin filter.
int StateVariableFilter::processSample(Channel &channel, int input, int feedback, int gain) const {
    const int drive = multiply(input, gain);

    channel.band = multiply(feedback, channel.band) - multiply(cutoffCoefficient_, channel.low)
                   + drive;
    channel.low = multiply(feedback, channel.low) + multiply(cutoffCoefficient_, channel.band);

    switch (mode_) {
    case Mode::LowPass:
        return channel.low;
    case Mode::HighPass:
        return input - channel.low;
    case Mode::BandPass:
        return multiply(channel.band - channel.low, kBandPassTrim);
    case Mode::Bypass:
    default:
        return input;
    }
}

void StateVariableFilter::process(int *buffer, uint numSamples, int numChannels) {
    if (mode_ == Mode::Bypass) {
        return;
    }

    // The prologue precomputes both of these once per call, so a coefficient
    // change mid-block cannot be heard until the next call.
    const int feedback = kMax24 - multiply(dampingCoefficient_, cutoffCoefficient_);
    const int gain = multiply(cutoffCoefficient_, inputGain_);

    const bool stereo = numChannels == 2;

    for (uint index = 0; index < numSamples; ++index) {
        buffer[0] = processSample(left_, buffer[0], feedback, gain);

        if (stereo) {
            buffer[1] = processSample(right_, buffer[1], feedback, gain);
            buffer += 2;
        } else {
            buffer += 1;
        }
    }
}
