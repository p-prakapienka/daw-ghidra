#pragma once

// Reconstructed Caustic wavetable oscillator.
//
// Recovered from GenerateWavetables, the waveform generators,
// Oscillator::SetOscillatorType, Oscillator::SetModulationMode and
// Oscillator::GenerateSignal{LQ,HQ} in libcaustic.so (ARMv7). See
// components/Oscillator.md for the evidence.
//
// The pitch path, glide, phase storage, pitch sweep, the five modulation
// inputs and frequency modulation are the engine's. Modulation modes 1 and 2
// and the custom wavetables are not yet implemented.

#include <cstdint>

#include "ControlVoltage.h"

using uint = unsigned int;

class Oscillator {
public:
    enum class Type : int {
        Sine = 0,
        Triangle = 1,
        Sawtooth = 2,
        SawtoothHQ = 3,
        Square = 4,
        SquareHQ = 5,
        Noise = 6,
        Custom1 = 7,
        Custom2 = 8,
    };

    enum class ModulationMode : int {
        Standard = 0,
        Alternate1 = 1,
        Alternate2 = 2,
    };

    static constexpr uint kTableSize = 4096;
    static constexpr uint kFractionBits = 12;
    static constexpr uint kPhaseMask = (kTableSize << kFractionBits) - 1;
    static constexpr float kSampleRate = 44100.0f;
    static constexpr float kPitchToIncrement = 4096.0f / 44100.0f;
    static constexpr uint kNoiseTableSize = 22050;
    static constexpr uint kBandCount = 8;
    static const int kBandHarmonics[kBandCount];
    static constexpr int kOutputFullScale = 0xFFFE00;
    static constexpr float kVibratoDepth = 0.1f;
    static constexpr float kOctaveRange = 4.0f;
    static constexpr float kSemitoneRange = 12.0f;

    struct Modulation {
        const int *vibrato = nullptr;
        const int *phase = nullptr;
        const int *octave = nullptr;
        const int *semitones = nullptr;
        const int *fmDepth = nullptr;
        const int *fmInput = nullptr;
        int fmAmount = 0;
    };

    Oscillator();

    void setType(Type type);
    Type type() const { return type_; }

    void setModulationMode(ModulationMode mode) { modulationMode_ = mode; }
    ModulationMode modulationMode() const { return modulationMode_; }

    void setPitchRatio(float ratio) { pitchRatio_ = ratio; }
    float pitchRatio() const { return pitchRatio_; }

    void setPhaseOffset(int entries) { phaseOffset_ = entries; }

    void generate(ControlVoltage &voltage, int oscillatorIndex, int *output, uint numSamples,
                  const Modulation &modulation, float bend = 1.0f);

    void setFrequency(float hertz);
    void resetPhase();
    uint phase() const { return standalone_.phase[0]; }
    void generate(int *output, uint numSamples);

    static const short *table(Type type);
    static const int *noiseTable();
    static const short *bandLimitedTable(Type type);
    static uint bandForFrequency(float hertz);
    uint band() const { return band_; }

private:
    static void buildTables();
    int readTable(uint index, int fraction) const;
    void generateNoise(int *output, uint numSamples);

    Type type_ = Type::Sine;
    ModulationMode modulationMode_ = ModulationMode::Standard;
    const short *table_ = nullptr;
    const short *bandTable_ = nullptr;
    uint band_ = 0;
    float pitchRatio_ = 1.0f;
    int phaseOffset_ = 0;
    uint noisePosition_ = 0;
    ControlVoltage standalone_ = ControlVoltage::makeDefault();
};
