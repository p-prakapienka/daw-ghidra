#include "Oscillator.h"

#include <cmath>
#include <cstdlib>
#include <mutex>
#include <vector>

namespace {

// The generators take a frequency and a level. Passing the rate divided by the
// table size puts exactly one cycle in the table, which is what the engine does.
constexpr float kTableFrequency = 44100.0f / 4096.0f; // 10.7666
constexpr float kFullScale = 32767.0f;

// Levels the engine passes per waveform. The square is quieter because it
// carries more energy than a sine at the same peak.
constexpr float kSineLevel = 0.3f;
constexpr float kTriangleLevel = 0.3f;
constexpr float kSawtoothLevel = 0.3f;
constexpr float kSquareLevel = 0.2f;
constexpr float kNoiseLevel = 0.3f;

std::vector<short> gSine;
std::vector<short> gTriangle;
std::vector<short> gSawtooth;
std::vector<short> gSquare;
std::vector<short> gNoise;
std::once_flag gTablesBuilt;

// buf[i] = sin(i * frequency * 2pi / 44100) * level * 32767
void sinewave(short *buffer, uint length, float frequency, float level) {
    const double amplitude = static_cast<double>(level * kFullScale);
    for (uint index = 0; index < length; ++index) {
        const float phase = static_cast<float>(index) * frequency * 6.2831854820251465f
                            / Oscillator::kSampleRate;
        buffer[index] = static_cast<short>(std::sin(static_cast<double>(phase)) * amplitude);
    }
}

// A bare ramp from -1 to 1. Not band limited; the HQ table exists for that.
void sawtooth(short *buffer, uint length, float level) {
    const float amplitude = level * kFullScale;
    for (uint index = 0; index < length; ++index) {
        const float value =
            2.0f * static_cast<float>(index) / static_cast<float>(length) - 1.0f;
        buffer[index] = static_cast<short>(value * amplitude);
    }
}

// Alternates sign every half period, where the period is the rate over the
// requested frequency.
void squarewave(short *buffer, uint length, float frequency, float level) {
    const float amplitude = level * kFullScale;
    const int halfPeriod = static_cast<int>((Oscillator::kSampleRate / frequency) * 0.5f);

    float sign = -1.0f;
    int countdown = halfPeriod;
    for (uint index = 0; index < length; ++index) {
        buffer[index] = static_cast<short>(sign * amplitude);
        if (--countdown <= 0) {
            countdown = halfPeriod;
            sign = -sign;
        }
    }
}

// Walks up and down between the level bounds at a constant rate.
void trianglewave(short *buffer, uint length, float frequency, float level) {
    const float amplitude = level * kFullScale;
    const auto halfPeriod = static_cast<float>(
        static_cast<int>((Oscillator::kSampleRate / frequency) * 0.5f));
    const float step = (level / halfPeriod) * 2.0f;

    float value = 0.0f;
    bool rising = true;
    for (uint index = 0; index < length; ++index) {
        buffer[index] = static_cast<short>((value / level) * amplitude);
        value += rising ? step : -step;
        if (value >= level) {
            value = level;
            rising = false;
        } else if (value <= -level) {
            value = -level;
            rising = true;
        }
    }
}

void whitenoise(short *buffer, uint length, float level) {
    const float amplitude = level * kFullScale;
    // Deterministic so tests and renders repeat. The engine's own source of
    // randomness is not recoverable from the binary.
    unsigned int state = 0x13579BDFu;
    for (uint index = 0; index < length; ++index) {
        state = state * 1664525u + 1013904223u;
        const float value =
            static_cast<float>(static_cast<int>(state >> 8) % 2001 - 1000) / 1000.0f;
        buffer[index] = static_cast<short>(value * amplitude);
    }
}

} // namespace

void Oscillator::buildTables() {
    std::call_once(gTablesBuilt, [] {
        gSine.resize(kTableSize);
        gTriangle.resize(kTableSize);
        gSawtooth.resize(kTableSize);
        gSquare.resize(kTableSize);
        gNoise.resize(kNoiseTableSize);

        sinewave(gSine.data(), kTableSize, kTableFrequency, kSineLevel);
        trianglewave(gTriangle.data(), kTableSize, kTableFrequency, kTriangleLevel);
        sawtooth(gSawtooth.data(), kTableSize, kSawtoothLevel);
        squarewave(gSquare.data(), kTableSize, kTableFrequency, kSquareLevel);
        whitenoise(gNoise.data(), kNoiseTableSize, kNoiseLevel);
    });
}

const short *Oscillator::table(Type type) {
    buildTables();
    switch (type) {
    case Type::Sine:
        return gSine.data();
    case Type::Triangle:
        return gTriangle.data();
    case Type::Sawtooth:
    case Type::SawtoothHQ:
        return gSawtooth.data();
    case Type::Square:
    case Type::SquareHQ:
        return gSquare.data();
    default:
        return nullptr;
    }
}

const short *Oscillator::noiseTable() {
    buildTables();
    return gNoise.data();
}

Oscillator::Oscillator() {
    setType(Type::Sine);
}

void Oscillator::setType(Type type) {
    type_ = type;
    table_ = table(type);
}

void Oscillator::setFrequency(float hertz) {
    // One cycle spans kTableSize index steps, each carrying kFractionBits of
    // fraction, so a full cycle is 2^24 phase units.
    const float cycle = static_cast<float>(kTableSize << kFractionBits);
    const float increment = hertz * cycle / kSampleRate;
    phaseIncrement_ = static_cast<uint>(increment < 0.0f ? 0.0f : increment);
}

void Oscillator::generate(int *buffer, uint numSamples) {
    if (type_ == Type::Noise) {
        const short *noise = noiseTable();
        for (uint index = 0; index < numSamples; ++index) {
            buffer[index] = static_cast<int>(static_cast<float>(noise[noiseIndex_]) * level_);
            noiseIndex_ = (noiseIndex_ + 1) % kNoiseTableSize;
        }
        return;
    }

    if (table_ == nullptr) {
        for (uint index = 0; index < numSamples; ++index) {
            buffer[index] = 0;
        }
        return;
    }

    for (uint index = 0; index < numSamples; ++index) {
        // Index and its successor, both wrapped into the table.
        const uint position = (phase_ >> kFractionBits) & (kTableSize - 1);
        const uint next = (position + 1) & (kTableSize - 1);
        const int fraction = static_cast<int>(phase_ & ((1u << kFractionBits) - 1));

        const int current = table_[position];
        const int difference = table_[next] - current;
        const int interpolated = current + ((difference * fraction) >> kFractionBits);

        buffer[index] = static_cast<int>(static_cast<float>(interpolated) * level_);
        phase_ = (phase_ + phaseIncrement_) & kPhaseMask;
    }
}
