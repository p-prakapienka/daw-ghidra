#pragma once

// Reconstructed Caustic wavetable oscillator, standard quality path.
//
// Recovered from GenerateWavetables, the sinewave/sawtooth/squarewave/
// trianglewave/whitenoise generators, Oscillator::SetOscillatorType and
// Oscillator::GenerateSignalLQ in libcaustic.so (ARMv7). See
// components/Oscillator.md for the evidence.
//
// Includes the band-limited HQ tables. The modulation modes and everything
// reached through ControlVoltage remain a separate pass.

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

    // Every shared table holds one cycle in this many entries.
    static constexpr uint kTableSize = 4096;

    // Phase carries 12 fractional bits below the 12-bit table index.
    static constexpr uint kFractionBits = 12;
    static constexpr uint kPhaseMask = (kTableSize << kFractionBits) - 1;

    // The rate the engine hardcodes, here and in the ADSR.
    static constexpr float kSampleRate = 44100.0f;

    // Noise is a half second of ints rather than one cycle of shorts.
    static constexpr uint kNoiseTableSize = 22050;

    // An HQ table holds this many band-limited variants of one cycle,
    // interleaved so that all bands of one phase step sit together.
    static constexpr uint kBandCount = 8;

    // Highest harmonic kept in each band, lowest pitch first.
    static const int kBandHarmonics[kBandCount];

    Oscillator();

    void setType(Type type);
    Type type() const { return type_; }

    // Oscillator frequency in Hz. Converted to a phase increment against the
    // engine's fixed rate.
    void setFrequency(float hertz);

    // Linear gain applied to every sample, the engine's field at offset 0x04.
    void setLevel(float level) { level_ = level; }

    void resetPhase() { phase_ = 0; }
    uint phase() const { return phase_; }

    // Append numSamples to the buffer. Samples are the engine's Q24-ish ints,
    // the same domain the filter consumes.
    void generate(int *buffer, uint numSamples);

    // The shared tables, built once and reused. Exposed for tests.
    static const short *table(Type type);
    static const int *noiseTable();

    // Band-limited table for a type, or nullptr when the type has none.
    static const short *bandLimitedTable(Type type);

    // Band the engine would read at this frequency, widest first.
    static uint bandForFrequency(float hertz);
    uint band() const { return band_; }

private:
    static void buildTables();

    Type type_ = Type::Sine;
    const short *table_ = nullptr;
    const short *bandTable_ = nullptr;
    uint band_ = 0;
    float level_ = 1.0f;
    uint phase_ = 0;
    uint phaseIncrement_ = 0;
    uint noiseIndex_ = 0;
};
