#pragma once

// Reconstructed Caustic LFO.
//
// Recovered from LFO::LFO, LFO::OnParamModified, LFO::SetBPM and
// LFO::GenerateInternal in libcaustic.so (ARMv7), and from the way
// SubSynth::Process and SubSynth::ConnectLFOs drive it. See
// components/LFO.md for the evidence.
//
// The LFO is tempo-synced and phase-locked to the song position: every block
// its phase is recomputed from the transport, not accumulated, and it is not
// retriggered by notes. Output is a Q24 block the oscillators and the filter
// read as a modulation source.

#include <cstdint>

using uint = unsigned int;

class LFO {
public:
    // The value at offset 0x1C. Off is the sentinel the engine's default
    // modulation source carries: it writes zeros, and the oscillator skips
    // its octave and semitone work when it sees it. Any other value writes
    // nothing at all.
    enum class Waveform : std::int32_t {
        Sine = 0,
        Triangle = 1,
        Sawtooth = 2,
        Square = 3,
        Off = 0x07FFFFFF,
    };

    // SubSynth's LFO targets, numbered as the engine stores them. The comment
    // on each is the input ConnectLFOs points at the LFO.
    enum class Target : int {
        None = 0,
        Oscillator1Pitch = 1,   // oscillator 1 +0x00
        Oscillator2Pitch = 2,   // oscillator 2 +0x00
        BothPitch = 3,          // both oscillators +0x00
        Oscillator2Phase = 4,   // oscillator 2 +0x08
        Cutoff = 5,             // every voice filter +0x80C
        Volume = 6,             // SubSynth +0x3764
        Oscillator2Octave = 7,  // oscillator 2 +0x10, LFO phase offset set to half a cycle
        Oscillator2Semitones = 8, // oscillator 2 +0x14
        Oscillator1Modulation = 9, // oscillator 1 +0x18, the FM depth
    };

    // The output block is 128 ints, allocated in the constructor, so the
    // engine never processes more than this many samples at once.
    static constexpr uint kBlockSize = 128;

    // Rate indexes this table of cycles per beat. Indices above 12 use 1.0.
    static constexpr int kDivisionCount = 13;
    static const float kDivisions[kDivisionCount];

    // One LFO cycle spans four units of phase in Q24.
    static constexpr std::int32_t kCycle = 0x03FFFFFC;

    // Half a cycle, the phase offset ConnectLFOs applies for the octave target.
    static constexpr std::int32_t kHalfCycle = 0x01FFFFFC;

    // Full scale for depth and output.
    static constexpr std::int32_t kUnity = 0x00FFFFFF;

    // One-pole smoothing coefficient for the sawtooth and square, 0.99 in Q24.
    static constexpr std::int32_t kDefaultSmoothing = 0x00FD70A3;

    LFO();

    // Rate is stored as a float and truncated to a division index.
    void setRate(float rate) { rate_ = rate; }
    void setDepth(float depth) { depth_ = static_cast<std::int32_t>(depth * kUnity); }
    void setDepthQ24(std::int32_t depth) { depth_ = depth; }
    void setWaveform(Waveform waveform) { waveform_ = waveform; }
    void setPhaseOffset(std::int32_t offset) { phaseOffset_ = offset; }

    // Stores the tempo and recomputes the period, as SetBPM does.
    void SetBPM(float bpm);

    // Recompute the period from rate and tempo, and restart the offset
    // counter. Call after changing the rate.
    void OnParamModified();

    // Fill the output block for this song position, as SubSynth::Process does
    // by writing the position to +0x10 and calling GenerateInternal.
    void generate(uint songPosition, uint numSamples);

    const std::int32_t *output() const { return output_; }
    uint periodSamples() const { return period_; }
    Waveform waveform() const { return waveform_; }

private:
    static std::int32_t multiply(std::int32_t a, std::int32_t b);

    std::int32_t smooth(std::int32_t target);

    float rate_ = 1.0f;                      // 0x04
    std::int32_t depth_ = kUnity;            // 0x08
    std::int32_t output_[kBlockSize] = {};   // 0x0C, heap-allocated in the engine
    uint position_ = 0;                      // 0x10, the song position
    uint period_ = 1;                        // 0x14
    uint positionOffset_ = 0;                // 0x18, zeroed by OnParamModified
    Waveform waveform_ = Waveform::Sine;     // 0x1C
    float bpm_ = 120.0f;                     // 0x20
    std::int32_t phaseOffset_ = 0;           // 0x28
    std::int32_t smoothed_ = 0;              // 0x30
    std::int32_t raw_ = 0;                   // 0x34
    std::int32_t smoothing_ = kDefaultSmoothing; // 0x38
};
