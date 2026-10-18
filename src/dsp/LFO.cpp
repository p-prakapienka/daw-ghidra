#include "dsp/LFO.h"

#include <cstdlib>
#include <cstring>

const float LFO::kDivisions[13] = {
    0.0625f, 0.125f, 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f, 16.0f,
};

namespace {

inline int Mul24(int a, int b) {
    return static_cast<int>((static_cast<long long>(a) * static_cast<long long>(b)) >> 24);
}

// vcvt.s32.f32 and vcvt.u32.f32: truncate, clamp to the type's range, NaN
// to zero.
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

inline unsigned int ToUnsigned(float f) {
    if (!(f > 0.0f)) {
        return 0;
    }
    if (f >= 4294967296.0f) {
        return 0xFFFFFFFFu;
    }
    return static_cast<unsigned int>(f);
}

} // namespace

LFO::LFO() {
    // new int[128] without an initialiser in the engine. The default
    // modulation source is generated once before anything reads it, and the
    // other two LFOs before every block, so the zeroing here is never seen.
    m_pnOutput = new int[kBlockSize]();
    OnParamModified();
}

// The engine frees the block with scalar operator delete (0x807ac).
LFO::~LFO() {
    delete[] m_pnOutput;
    m_pnOutput = nullptr;
}

void LFO::SetBPM(float fBPM) {
    m_fBPM = fBPM;
    OnParamModified();
}

void LFO::OnParamModified() {
    m_nPositionOffset = 0;
    const int nIndex = ToInt(m_fRate);
    const float fDivision = static_cast<unsigned int>(nIndex) > 12 ? 1.0f : kDivisions[nIndex];
    m_n24 = kUnity;
    // (60 / bpm) seconds per beat, over cycles per beat, in samples.
    m_nPeriod = ToUnsigned(60.0f / m_fBPM / fDivision * 44100.0f);
}

void LFO::GenerateInternal(unsigned int nSamples) {
    const float fPeriod = static_cast<float>(m_nPeriod);
    // __aeabi_uidivmod; the engine would divide by zero for a zero period.
    const unsigned int nOffset =
        m_nPeriod != 0 ? (m_nPositionOffset + m_nSongPosition) % m_nPeriod : 0;
    const int nIncrement = ToInt(4.0f / fPeriod * 16777215.0f);
    const int nStart = ToInt(static_cast<float>(nOffset) / fPeriod * 16777215.0f);
    int nPhase = m_nPhaseOffset + Mul24(nStart, kCycle);
    int *pOut = m_pnOutput;

    switch (m_nWaveform) {
    case kSine:
        // Parabolic sine: x * (2 - |x|) over (-2, 2].
        for (unsigned int n = 0; n < nSamples; n++) {
            nPhase += nIncrement;
            if (nPhase > 0x01FFFFFE) {
                nPhase -= kCycle;
            }
            *pOut++ = Mul24(m_nDepth, Mul24(nPhase, 0x01FFFFFE - std::abs(nPhase)));
        }
        break;
    case kTriangle:
        // |2 - |x|| - 1 over [-3, 1).
        for (unsigned int n = 0; n < nSamples; n++) {
            nPhase += nIncrement;
            if (nPhase >= 0x01000000) {
                nPhase -= kCycle;
            }
            *pOut++ = Mul24(m_nDepth, std::abs(0x01FFFFFE - std::abs(nPhase)) - kUnity);
        }
        break;
    case kSawtooth:
        // Falling ramp -x/2 over (-2, 2], smoothed: s = t + 0.99 * (s - t).
        for (unsigned int n = 0; n < nSamples; n++) {
            nPhase += nIncrement;
            if (nPhase > 0x01FFFFFE) {
                nPhase -= kCycle;
            }
            m_nRaw = (-nPhase) >> 1;
            m_nSmoothed = m_nRaw + Mul24(m_nSmoothing, m_nSmoothed - m_nRaw);
            *pOut++ = Mul24(m_nDepth, m_nSmoothed);
        }
        break;
    case kSquare:
        // High for x in [1, 3] of a [0, 4) cycle, smoothed.
        for (unsigned int n = 0; n < nSamples; n++) {
            nPhase += nIncrement;
            if (nPhase >= kCycle + 1) {
                nPhase -= kCycle;
            }
            m_nRaw = static_cast<unsigned int>(nPhase - kUnity) <= 0x01FFFFFEu ? kUnity : -kUnity;
            m_nSmoothed = m_nRaw + Mul24(m_nSmoothing, m_nSmoothed - m_nRaw);
            *pOut++ = Mul24(m_nDepth, m_nSmoothed);
        }
        break;
    case kOff:
        std::memset(m_pnOutput, 0, nSamples * sizeof(int));
        break;
    default:
        break;
    }
}
