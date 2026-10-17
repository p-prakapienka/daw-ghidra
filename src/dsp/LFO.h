#pragma once

// LFO: Caustic's tempo-synced low-frequency oscillator.
//
// Recovered from LFO::LFO (0x80ad8), ~LFO (0x80790), OnParamModified
// (0x80a74), SetBPM (0x80b60) and GenerateInternal (0x807e0) in
// libcaustic.so (ARMv7), and from the way SubSynth::Process and
// SubSynth::ConnectLFOs drive it. See dsp/LFO.md for the evidence.
//
// Ported to the original's structure. LFO is a root class whose vtable holds
// the destructor and GenerateInternal. Its members are public because
// SubSynth writes the rate, depth, waveform, phase offset and song position
// directly. Their names are ours, in the engine's Hungarian style; the
// offsets and types are the engine's.
//
// The LFO is phase-locked to the song position: every block its phase is
// recomputed from the transport, not accumulated. Its output is a block of
// Q24 values the oscillators and the filter read as a modulation source.

#include <cstdint>

using uint = unsigned int;

class LFO {
public:
    // The waveform at +0x1C. kOff is the sentinel SubSynth's default
    // modulation source carries: GenerateInternal writes zeros, and the
    // oscillator skips its octave and semitone work when it sees it. Any
    // other value writes nothing at all.
    enum { kSine = 0, kTriangle = 1, kSawtooth = 2, kSquare = 3, kOff = 0x07FFFFFF };

    // SubSynth's LFO targets, numbered as the engine stores them (SubSynth
    // +0x3720 for LFO 1, +0x3760 for LFO 2). The comment on each is the input
    // ConnectLFOs points at the LFO. In the engine these belong to SubSynth;
    // they move there with its port.
    enum class Target : int {
        None = 0,
        Oscillator1Pitch = 1,      // oscillator 1 +0x00
        Oscillator2Pitch = 2,      // oscillator 2 +0x00
        BothPitch = 3,             // both oscillators +0x00
        Oscillator2Phase = 4,      // oscillator 2 +0x08
        Cutoff = 5,                // every voice filter +0x80C
        Volume = 6,                // SubSynth +0x3764
        Oscillator2Octave = 7,     // oscillator 2 +0x10, LFO phase offset set to half a cycle
        Oscillator2Semitones = 8,  // oscillator 2 +0x14
        Oscillator1Modulation = 9, // oscillator 1 +0x18, the FM depth
    };

    // The output block is 128 ints, allocated in the constructor; the engine
    // never asks GenerateInternal for more.
    static constexpr uint kBlockSize = 128;

    // One cycle spans four units of phase in Q24; half a cycle is the phase
    // offset ConnectLFOs applies for the octave target.
    static constexpr std::int32_t kCycle = 0x03FFFFFC;
    static constexpr std::int32_t kHalfCycle = 0x01FFFFFC;

    // Full scale for depth and output.
    static constexpr std::int32_t kUnity = 0x00FFFFFF;

    // Cycles per beat, indexed by the truncated rate (.rodata 0x270104).
    static const float kDivisions[13];

    LFO();
    virtual ~LFO();

    // Fill m_pnOutput with nSamples values for the song position in
    // m_nSongPosition.
    virtual void GenerateInternal(unsigned int nSamples);

    // Recompute the period from the rate and the tempo.
    void OnParamModified();
    void SetBPM(float fBPM);

    float m_fRate = 1.0f;              // +0x04: truncated to a division index
    int m_nDepth = kUnity;             // +0x08: Q24
    int *m_pnOutput;                   // +0x0C: new int[128]
    unsigned int m_nSongPosition = 0;  // +0x10: written before each block
    unsigned int m_nPeriod = 1;        // +0x14: samples per cycle
    unsigned int m_nPositionOffset = 0; // +0x18: zeroed by OnParamModified
    int m_nWaveform = kSine;           // +0x1C
    float m_fBPM = 120.0f;             // +0x20
    int m_n24 = kUnity;                // +0x24: written, never read here
    int m_nPhaseOffset = 0;            // +0x28: Q24
    int m_n2C = 0;                     // +0x2C: zeroed, never read here
    int m_nSmoothed = 0;               // +0x30: sawtooth and square smoothing state
    int m_nRaw = 0;                    // +0x34: last unsmoothed value
    int m_nSmoothing = 0x00FD70A3;     // +0x38: 0.99 in Q24

    // The engine never copies an LFO; the block it owns makes copying unsafe.
    LFO(const LFO &) = delete;
    LFO &operator=(const LFO &) = delete;
};

#if UINTPTR_MAX == 0xFFFFFFFF
static_assert(sizeof(LFO) == 0x3C, "LFO is 0x3C bytes");
#endif
