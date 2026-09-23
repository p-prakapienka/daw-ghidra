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
    // The value the engine stores at offset 0x20 and switches on. Numbering is
    // the jump table's, so it matches the preset data.
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

    // The value stored at offset 0x24 by SetModulationMode. Mode 0 is the
    // main path, which already includes frequency modulation from the input
    // buffer. Modes 1 and 2 take separate paths not yet reconstructed; any
    // other value makes the engine advance the phase and write nothing.
    enum class ModulationMode : int {
        Standard = 0,
        Alternate1 = 1,
        Alternate2 = 2,
    };

    // Every shared table holds one cycle in this many entries.
    static constexpr uint kTableSize = 4096;

    // Phase carries 12 fractional bits below the 12-bit table index.
    static constexpr uint kFractionBits = 12;
    static constexpr uint kPhaseMask = (kTableSize << kFractionBits) - 1;

    // The rate the engine hardcodes, here and in the ADSR.
    static constexpr float kSampleRate = 44100.0f;

    // Converts Q12 hertz to a phase increment: 4096 / 44100, a literal in
    // GenerateSignalLQ's pool.
    static constexpr float kPitchToIncrement = 4096.0f / 44100.0f;

    // Noise is 22050 Q24 ints, half a second.
    static constexpr uint kNoiseTableSize = 22050;

    // An HQ table holds this many band-limited variants of one cycle,
    // interleaved so that all bands of one phase step sit together.
    static constexpr uint kBandCount = 8;

    // Highest harmonic kept in each band, lowest pitch first.
    static const int kBandHarmonics[kBandCount];

    // Output is Q24: the interpolated Q15 table value shifted left by nine and
    // clamped to this.
    static constexpr int kOutputFullScale = 0xFFFE00;

    // Vibrato input scales the pitch by up to this fraction either way.
    static constexpr float kVibratoDepth = 0.1f;

    // Octave and semitone inputs span this many units at full scale, and are
    // floored to whole steps.
    static constexpr float kOctaveRange = 4.0f;
    static constexpr float kSemitoneRange = 12.0f;

    // Modulation sources. The engine reaches each through a pointer on the
    // oscillator whose target holds a per-sample Q24 array at +0x0C. A null
    // pointer here stands for the engine's default source.
    struct Modulation {
        const int *vibrato = nullptr;   // osc +0x00: pitch, +/-10%
        const int *phase = nullptr;     // osc +0x08: phase offset in table units << 12
        const int *octave = nullptr;    // osc +0x10: whole octaves, +/-4
        const int *semitones = nullptr; // osc +0x14: whole semitones, +/-12
        const int *fmDepth = nullptr;   // osc +0x18: scales the FM amount by 1 + value

        // Frequency modulation: the engine's fifth argument and sixth
        // argument. input is another oscillator's Q24 output, amount is Q24.
        const int *fmInput = nullptr;
        int fmAmount = 0;
    };

    Oscillator();

    void setType(Type type);
    Type type() const { return type_; }

    void setModulationMode(ModulationMode mode) { modulationMode_ = mode; }
    ModulationMode modulationMode() const { return modulationMode_; }

    // Frequency multiplier, the engine's field at offset 0x04. SubSynth writes
    // oscillator 2's pitch offset here.
    void setPitchRatio(float ratio) { pitchRatio_ = ratio; }
    float pitchRatio() const { return pitchRatio_; }

    // Fixed phase offset in table entries, the engine's field at 0x0C.
    void setPhaseOffset(int entries) { phaseOffset_ = entries; }

    // Render one block for a voice, as GenerateSignalLQ does. The phase lives
    // in the control voltage slot for oscillatorIndex, so two oscillators can
    // share one voice. bend is the machine's pitch-bend ratio.
    void generate(ControlVoltage &voltage, int oscillatorIndex, int *output, uint numSamples,
                  const Modulation &modulation, float bend = 1.0f);

    // Standalone use without a voice: set a frequency and render.
    void setFrequency(float hertz);
    void resetPhase();
    uint phase() const { return standalone_.phase[0]; }
    void generate(int *output, uint numSamples);

    // The shared tables, built once and reused. Exposed for tests.
    static const short *table(Type type);
    static const int *noiseTable();

    // Band-limited table for a type, or nullptr when the type has none.
    static const short *bandLimitedTable(Type type);

    // Band for a frequency, widest first. Derived, not read: see the notes.
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

    // Voice state used by the standalone generate overload.
    ControlVoltage standalone_ = ControlVoltage::makeDefault();
};
