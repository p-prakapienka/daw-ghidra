#include "Oscillator.h"

#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <mutex>
#include <vector>

namespace {

struct StateScale {
    static constexpr float kInverseOne24 = 5.960464477539063e-08f; // 2^-24
};

// The generators take a frequency and a level. Passing the rate divided by the
// table size puts exactly one cycle in the table, which is what the engine does.
constexpr float kTableFrequency = 44100.0f / 4096.0f; // 10.7666
constexpr float kFullScale = 32767.0f;

// Levels the engine passes per waveform. The square is quieter because it
// carries more energy than a sine at the same peak.
constexpr float kSineLevel = 0.3f;
constexpr float kTriangleLevel = 0.4f;
constexpr float kSawtoothLevel = 0.3f;
constexpr float kSquareLevel = 0.2f;
constexpr float kNoiseLevel = 0.4f;

std::vector<short> gSine;
std::vector<short> gTriangle;
std::vector<short> gSawtooth;
std::vector<short> gSquare;
std::vector<short> gHQSawtooth;
std::vector<short> gHQSquare;
std::vector<int> gNoise;
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

// ASwhitenoise(int*, unsigned int, float): a fixed-seed add/xor generator, not
// lrand48. The seeds are the MD5 initial constants. Output is Q24:
// (int)(a * level * 2^-31 * 16777215) for each successive value of a.
void whitenoise(int *buffer, uint length, float level) {
    const float scale = level * 4.656612873077393e-10f; // level / 2^31
    std::uint32_t a = 0xEFCDAB89u;
    std::uint32_t b = 0x67452301u;
    for (uint index = 0; index < length; ++index) {
        const auto value = static_cast<float>(static_cast<std::int32_t>(a));
        b ^= a;
        a += b;
        buffer[index] = static_cast<int>(value * scale * 16777215.0f);
    }
}

// Each HQ table is eight band-limited cycles, built once at GenerateWavetables.
// A band is a literal sum of sines at amplitude 1/h up to kBandHarmonics[band]
// — not an FFT. Playback never sums sines; it reads the baked table.
// The engine writes the bands interleaved: band b of phase step i lives at
// table[i * kBandCount + b].
void buildBandLimited(std::vector<short> &table, int harmonicStep, float level) {
    table.assign(Oscillator::kTableSize * Oscillator::kBandCount, 0);

    std::vector<float> sineScratch(Oscillator::kTableSize);
    for (uint index = 0; index < Oscillator::kTableSize; ++index) {
        sineScratch[index] = std::sin(2.0f * 3.14159265358979f * static_cast<float>(index)
                                      / static_cast<float>(Oscillator::kTableSize));
    }

    std::vector<float> accumulator(Oscillator::kTableSize);

    for (uint band = 0; band < Oscillator::kBandCount; ++band) {
        const int limit = Oscillator::kBandHarmonics[band];
        std::fill(accumulator.begin(), accumulator.end(), 0.0f);

        for (int harmonic = 1; harmonic < limit; harmonic += harmonicStep) {
            const float amplitude = 1.0f / static_cast<float>(harmonic);
            uint position = 0;
            for (uint index = 0; index < Oscillator::kTableSize; ++index) {
                accumulator[index] += amplitude * sineScratch[position % Oscillator::kTableSize];
                position += static_cast<uint>(harmonic);
            }
        }

        float peak = 0.0f;
        for (const float value : accumulator) {
            peak = std::max(peak, value);
        }
        if (peak <= 0.0f) {
            peak = 1.0f;
        }

        for (uint index = 0; index < Oscillator::kTableSize; ++index) {
            const float normalised = -accumulator[index] / peak;
            table[index * Oscillator::kBandCount + band] =
                static_cast<short>(normalised * level * kFullScale);
        }
    }
}

} // namespace

const int Oscillator::kBandHarmonics[Oscillator::kBandCount] = {512, 337, 169, 84, 42, 21, 10, 6};

void Oscillator::buildTables() {
    std::call_once(gTablesBuilt, [] {
        gSine.resize(kTableSize);
        gTriangle.resize(kTableSize);
        gSawtooth.resize(kTableSize);
        gSquare.resize(kTableSize);
        gNoise.resize(kNoiseTableSize);
        gHQSawtooth.resize(kTableSize * kBandCount);
        gHQSquare.resize(kTableSize * kBandCount);

        sinewave(gSine.data(), kTableSize, kTableFrequency, kSineLevel);
        trianglewave(gTriangle.data(), kTableSize, kTableFrequency, kTriangleLevel);
        sawtooth(gSawtooth.data(), kTableSize, kSawtoothLevel);
        squarewave(gSquare.data(), kTableSize, kTableFrequency, kSquareLevel);
        whitenoise(gNoise.data(), kNoiseTableSize, kNoiseLevel);

        // Sawtooth keeps every harmonic; the square keeps the odd ones.
        buildBandLimited(gHQSawtooth, 1, kSawtoothLevel);
        buildBandLimited(gHQSquare, 2, kSquareLevel);
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

const int *Oscillator::noiseTable() {
    buildTables();
    return gNoise.data();
}

const short *Oscillator::bandLimitedTable(Type type) {
    buildTables();
    switch (type) {
    case Type::SawtoothHQ:
        return gHQSawtooth.data();
    case Type::SquareHQ:
        return gHQSquare.data();
    default:
        return nullptr;
    }
}

// Derived, not read from the engine: pick the widest band whose top harmonic
// still fits below Nyquist at this pitch.
uint Oscillator::bandForFrequency(float hertz) {
    if (!(hertz > 0.0f)) {
        return 0;
    }
    const float nyquist = kSampleRate * 0.5f;
    for (uint band = 0; band < kBandCount; ++band) {
        if (static_cast<float>(kBandHarmonics[band]) * hertz <= nyquist) {
            return band;
        }
    }
    return kBandCount - 1;
}

Oscillator::Oscillator() {
    setType(Type::Sine);
}

void Oscillator::setType(Type type) {
    type_ = type;
    table_ = table(type);
    bandTable_ = bandLimitedTable(type);
}

int Oscillator::readTable(uint index, int fraction) const {
    const uint position = index & (kTableSize - 1);
    const uint next = (index + 1) & (kTableSize - 1);

    int current;
    int following;
    if (bandTable_ != nullptr) {
        current = bandTable_[position * kBandCount + band_];
        following = bandTable_[next * kBandCount + band_];
    } else {
        current = table_[position];
        following = table_[next];
    }

    // The difference saturates at the top before it is scaled, as the engine
    // does with cmp #0x8000 / movwge #0x7fff.
    int difference = following - current;
    if (difference >= 0x8000) {
        difference = 0x7FFF;
    } else if (difference < -0x7FFF) {
        difference = -0x7FFF;
    }

    // fraction is the low 12 phase bits widened to Q15.
    const int value = current + ((difference * fraction) >> 15);

    // Q15 to Q24 with the engine's clamp.
    if (value >= 0x8000) {
        return kOutputFullScale;
    }
    if (value < -0x7FFF) {
        return -kOutputFullScale;
    }
    return value << 9;
}

void Oscillator::generateNoise(int *output, uint numSamples) {
    // The engine copies consecutive noise values straight to the output and
    // wraps the position by (position + n) mod (22050 - n), so the next block's
    // copy of n values always fits. Pitch and modulation do not apply.
    const int *noise = noiseTable();
    for (uint index = 0; index < numSamples; ++index) {
        output[index] = noise[(noisePosition_ + index) % kNoiseTableSize];
    }
    if (numSamples < kNoiseTableSize) {
        noisePosition_ = (noisePosition_ + numSamples) % (kNoiseTableSize - numSamples);
    }
}

void Oscillator::generate(ControlVoltage &voltage, int oscillatorIndex, int *output,
                          uint numSamples, const Modulation &modulation, float bend) {
    // Glide is resolved once per block, before anything else: linear in
    // Q12 hertz from the start to the target over glideSamples, measured from
    // note-on. Reaching the end clears the glide.
    float pitch = voltage.targetPitch;
    if (voltage.glideSamples != 0) {
        float progress = static_cast<float>(voltage.samplesSinceNoteOn)
                         / static_cast<float>(voltage.glideSamples);
        if (progress >= 1.0f) {
            progress = 1.0f;
            voltage.glideSamples = 0;
        }
        voltage.currentPitch =
            voltage.glideStartPitch + progress * (voltage.targetPitch - voltage.glideStartPitch);
        pitch = voltage.currentPitch;
    }

    if (type_ == Type::Noise) {
        generateNoise(output, numSamples);
        return;
    }

    const float base = pitch * bend * pitchRatio_ * kPitchToIncrement;
    band_ = bandForFrequency(pitch * bend * pitchRatio_ / ControlVoltage::kPitchScale);

    std::uint32_t &phase = voltage.phase[oscillatorIndex];
    float &sweep = voltage.pitchSweep[oscillatorIndex];

    if (modulationMode_ != ModulationMode::Standard || (table_ == nullptr && bandTable_ == nullptr)) {
        // Modes 1 and 2 are not reconstructed; other values write nothing in
        // the engine either. Keep the phase moving so the voice stays coherent.
        for (uint index = 0; index < numSamples; ++index) {
            output[index] = 0;
        }
        phase = (phase + static_cast<std::uint32_t>(base) * numSamples) & kPhaseMask;
        return;
    }

    // Octave and semitone offsets are floored to whole steps. The engine
    // recomputes them only when either source is connected, which a null
    // pointer here stands for.
    float stepRatio = 1.0f;

    for (uint index = 0; index < numSamples; ++index) {
        if (modulation.octave != nullptr || modulation.semitones != nullptr) {
            const float octaves =
                modulation.octave != nullptr
                    ? std::floor(static_cast<float>(modulation.octave[index])
                                 * StateScale::kInverseOne24 * kOctaveRange)
                    : 0.0f;
            const float semitones =
                modulation.semitones != nullptr
                    ? std::floor(static_cast<float>(modulation.semitones[index])
                                 * StateScale::kInverseOne24 * kSemitoneRange)
                    : 0.0f;
            stepRatio = std::pow(2.0f, (semitones + octaves * 12.0f) / 12.0f);
        }

        const float vibrato =
            1.0f + (modulation.vibrato != nullptr
                        ? static_cast<float>(modulation.vibrato[index]) * StateScale::kInverseOne24
                              * kVibratoDepth
                        : 0.0f);

        float fm = 1.0f;
        if (modulation.fmInput != nullptr) {
            const int depthLift =
                modulation.fmDepth != nullptr ? modulation.fmDepth[index] : 0;
            const auto depth = static_cast<int>(
                (static_cast<std::int64_t>(depthLift + 0xFFFFFF) * modulation.fmAmount) >> 24);
            const auto drive = static_cast<int>(
                (static_cast<std::int64_t>(modulation.fmInput[index]) * depth) >> 24);
            fm = 1.0f + static_cast<float>(drive) * StateScale::kInverseOne24;
        }

        const int phaseShift =
            modulation.phase != nullptr ? (modulation.phase[index] >> kFractionBits) : 0;
        const uint tableIndex = (phase >> kFractionBits) + static_cast<uint>(phaseOffset_ + phaseShift);
        const auto fraction = static_cast<int>((phase & ((1u << kFractionBits) - 1)) << 3);

        output[index] = readTable(tableIndex, fraction);

        // The increment is recomputed in float every sample and truncated.
        const float increment = (1.0f + sweep) * base * vibrato * fm * stepRatio;
        phase += static_cast<std::uint32_t>(static_cast<std::int32_t>(increment));

        sweep *= voltage.pitchSweepDecay;
    }

    phase &= kPhaseMask;
}

void Oscillator::setFrequency(float hertz) {
    const float scaled = hertz * ControlVoltage::kPitchScale;
    standalone_.targetPitch = scaled;
    standalone_.currentPitch = scaled;
    standalone_.glideStartPitch = scaled;
    standalone_.glideSamples = 0;
}

void Oscillator::resetPhase() {
    standalone_.phase[0] = 0;
    noisePosition_ = 0;
}

void Oscillator::generate(int *output, uint numSamples) {
    generate(standalone_, 0, output, numSamples, Modulation{}, 1.0f);
}
