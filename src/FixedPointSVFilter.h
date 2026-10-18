#pragma once

// FixedPointSVFilter: SubSynth's per-voice state-variable filter, a
// Chamberlin-style design in Q24 fixed point with low-, high- and band-pass
// taps and an envelope- and LFO-driven cutoff.
//
// Recovered from FixedPointSVFilter::FixedPointSVFilter(bool) (0x94778) and
// FixedPointSVFilter::ProcessCV(ControlVoltage*, int*, unsigned int, int)
// (0x949a8) in libcaustic.so (ARMv7). See src/FixedPointSVFilter.md.
//
// The members are public because SubSynth writes them directly. Their names
// are ours, in the engine's Hungarian style; the offsets are the engine's.

#include "../components/ControlVoltage.h"
#include "../components/LFO.h"
#include "Envelope.h"
#include "Filter.h"

class FixedPointSVFilter : public Filter {
public:
    // bExtendedResonance selects the damping table with the deeper range and
    // an input gain that starts at 1.0 instead of 1.25. SubSynth passes true.
    explicit FixedPointSVFilter(bool bExtendedResonance);
    virtual ~FixedPointSVFilter() {}

    // Filters nSamples samples in place, one channel or two interleaved
    // (nChannels == 2). Every 16 samples it reads one envelope value at the
    // voice's position and refreshes the coefficients from the cutoff and
    // resonance, the voice's key tracking and the modulation source. Any mode
    // other than 1-3 returns without touching anything.
    void ProcessCV(ControlVoltage *pCV, int *pBuffer, unsigned int nSamples, int nChannels);

    // The coefficient update that ends the constructor, every control block
    // in ProcessCV, and SubSynth's cutoff and resonance handlers. The engine
    // has no out-of-line function for it: each of those places has the code
    // inline.
    inline void UpdateCoefficients(float fCutoff, float fResonance);

    int m_nMode = 0;                 // +0x08: 0 none, 1 low-, 2 high-, 3 band-pass
    int m_anCutoffTable[256];        // +0x0C
    int m_anDampingTable[256];       // +0x40C
    LFO *m_pModulation = nullptr;    // +0x80C: the cutoff's modulation source
    Envelope m_DefaultEnvelope;      // +0x810
    Envelope *m_pEnvelope;           // +0x814: starts at m_DefaultEnvelope
    int m_nInputGain = 0;            // +0x818, Q24
    int m_anBand[2] = {0, 0};        // +0x81C: band integrator, left and right
    int m_anLow[2] = {0, 0};         // +0x824: low integrator, left and right
    int m_nCutoff = 0;               // +0x82C: cutoff coefficient, Q24
    int m_nDamping = 0;              // +0x830: damping coefficient, Q24
    int m_n834 = 0;                  // +0x834: zeroed, not read by ProcessCV
    float m_fCutoff = 1.0f;          // +0x838: 0 .. 1
    float m_fResonance = 0.0f;       // +0x83C: 0 .. 1
    bool m_bInvertEnvelope = false;  // +0x840: the "Inv." filter types
    bool m_bExtendedResonance;       // +0x841
};

#if UINTPTR_MAX == 0xFFFFFFFF
static_assert(sizeof(FixedPointSVFilter) == 0x844, "FixedPointSVFilter is 0x844 bytes");
#endif

namespace fixedpointsvfilter {

// The engine's Q24 product: 64-bit multiply, keep bits 24..55.
inline int Mul24(int a, int b) {
    return static_cast<int>((static_cast<long long>(a) * static_cast<long long>(b)) >> 24);
}

// vcvt.s32.f32: truncates, clamps to the int range, NaN to zero.
inline int ToInt(float f) {
    if (f != f) {
        return 0;
    }
    if (f >= 2147483648.0f) {
        return 0x7FFFFFFF;
    }
    if (f <= -2147483648.0f) {
        return -0x7FFFFFFF - 1;
    }
    return static_cast<int>(f);
}

} // namespace fixedpointsvfilter

inline void FixedPointSVFilter::UpdateCoefficients(float fCutoff, float fResonance) {
    using fixedpointsvfilter::ToInt;

    int nCutoffIndex;
    if (fCutoff < 0.05f) {
        nCutoffIndex = 12;
    } else if (fCutoff > 1.0f) {
        nCutoffIndex = 255;
    } else {
        nCutoffIndex = ToInt(fCutoff * 255.0f);
    }

    int nResonanceIndex;
    float fGainTerm;
    if (fResonance < 0.0f) {
        nResonanceIndex = 0;
        fGainTerm = -0.0f;
    } else if (fResonance > 1.0f) {
        nResonanceIndex = 255;
        fGainTerm = -0.75f;
    } else {
        nResonanceIndex = ToInt(fResonance * 255.0f);
        fGainTerm = fResonance * -0.75f;
    }

    m_nDamping = m_anDampingTable[nResonanceIndex];
    m_nCutoff = m_anCutoffTable[nCutoffIndex];
    m_nInputGain = ToInt(((m_bExtendedResonance ? 1.0f : 1.25f) + fGainTerm) * 16777215.0f);
}
