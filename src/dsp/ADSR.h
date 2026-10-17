#pragma once

// ADSR: Caustic's attack-decay-sustain-release envelope, Q15 output from a
// 24-bit ramp.
//
// Recovered from ADSR::ADSR (0x806fc), OnParamModified (0x80628),
// SetMinSamples (0x80618), IsDone (0x7e528) and GenerateValues (0x7f708) in
// libcaustic.so (ARMv7). See dsp/ADSR.md for the evidence behind every
// constant.
//
// Ported to the original's structure. The members are public because
// SubSynth writes the parameters directly and then calls OnParamModified.
// Their names are ours, in the engine's Hungarian style; the offsets and
// types are the engine's. GenerateValues is one function, written as the
// engine's is: one loop per curve per segment, because the curves are not
// handled alike.

#include <cstdint>

#include "dsp/Envelope.h"

using uint = unsigned int;
using ushort = unsigned short;

class ADSR : public Envelope {
public:
    // Segment shapes, stored as plain ints (+0x3C, +0x40, +0x44).
    enum { kLogarithmic = 0, kLinear = 1, kExponential = 2 };

    ADSR();

    // Recompute the segment lengths and the Q15 sustain from the parameters:
    // seconds at 44100 Hz, clamped to the minimum and to 220500 samples.
    void OnParamModified();

    // Lower bounds for the attack, decay and release segments, in samples.
    void SetMinSamples(unsigned int nAttack, unsigned int nDecay, unsigned int nRelease);

    // True once the release segment and a 0x800-sample tail have elapsed.
    bool IsDone(unsigned int nPosition) override;

    // nPosition is the sample offset since the last gate change; nGate is
    // non-zero while the note is held, zero runs the release.
    void GenerateValues(unsigned int nPosition, unsigned int nGate, short *pOutput,
                        unsigned int nSamples) override;

    float m_fAttack = 0.0f;                 // +0x04: seconds
    float m_fDecay = 0.0f;                  // +0x08: seconds
    float m_fSustain = 1.0f;                // +0x0C: 0 .. 1
    float m_fRelease = 0.0f;                // +0x10: seconds
    unsigned int m_nAttackSamples = 0;      // +0x14
    unsigned int m_nDecaySamples = 0;       // +0x18
    unsigned int m_nReleaseSamples = 0;     // +0x1C
    unsigned int m_nNoteAttackSamples = 0;  // +0x20: latched at note-on
    unsigned int m_nNoteDecaySamples = 0;   // +0x24: latched at note-on
    unsigned int m_nNoteReleaseSamples = 0; // +0x28: latched at note-off
    unsigned int m_nMinAttackSamples = 80;  // +0x2C
    unsigned int m_nMinDecaySamples = 80;   // +0x30
    unsigned int m_nMinReleaseSamples = 80; // +0x34
    unsigned int m_nLastPosition = 0;       // +0x38: for retrigger detection
    int m_nAttackCurve = kLinear;           // +0x3C
    int m_nDecayCurve = kLinear;            // +0x40
    int m_nReleaseCurve = kLinear;          // +0x44
    int m_nAttackStep = 0;                  // +0x48: ramp step, 2^24 / length
    int m_nDecayStep = 0;                   // +0x4C
    int m_nReleaseStep = 0;                 // +0x50
    short m_sSustain = 0;                   // +0x54: Q15, from OnParamModified
    short m_sLast = 0;                      // +0x56: last value written
    short m_sAttackStart = 0;               // +0x58: level the attack rises from
    short m_sPrevious = 0;                  // +0x5A: level for the retrigger fade
    short m_sNoteSustain = 0;               // +0x5C: sustain latched at note-on
    short m_sReleaseStart = 0;              // +0x5E: level latched at note-off
    bool m_bHeld = false;                   // +0x60
    bool m_bStartFromCurrent = false;       // +0x61: see dsp/ADSR.md
    bool m_bIdle = true;                    // +0x62
};

#if UINTPTR_MAX == 0xFFFFFFFF
static_assert(sizeof(ADSR) == 0x64, "ADSR is 0x64 bytes");
#endif
