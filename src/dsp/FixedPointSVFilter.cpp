#include "dsp/FixedPointSVFilter.h"

#include <cmath>

using fixedpointsvfilter::Mul24;
using fixedpointsvfilter::ToInt;

FixedPointSVFilter::FixedPointSVFilter(bool bExtendedResonance)
    : m_pEnvelope(&m_DefaultEnvelope), m_bExtendedResonance(bExtendedResonance) {
    // The tables, from the pool at 0x9497c: the cutoff rises exponentially
    // towards unity at the top, the damping falls as the resonance rises.
    const float fDampingBase = bExtendedResonance ? 1.5f : 0.0f;
    for (int i = 0; i < 256; i++) {
        const float x = static_cast<float>(i) * 0.00390625f;
        m_anCutoffTable[i] = ToInt(powf(0.5f, (1.0f - (0.36f + x * 0.64f)) * 8.0f) * 16777215.0f);
        m_anDampingTable[i] = ToInt(powf(0.5f, fDampingBase + x * 4.0f) * 16777215.0f);
    }

    UpdateCoefficients(m_fCutoff, m_fResonance);
}

namespace {

// One sample through the integrators. Both states leak by the same
// 1 - f*q feedback, which is where this departs from a textbook Chamberlin
// filter.
inline void Integrate(int nInput, int nFeedback, int nCutoff, int nGain, int &nBand, int &nLow) {
    nBand = Mul24(nFeedback, nBand) - Mul24(nCutoff, nLow) + Mul24(nInput, nGain);
    nLow = Mul24(nFeedback, nLow) + Mul24(nBand, nCutoff);
}

} // namespace

void FixedPointSVFilter::ProcessCV(ControlVoltage *pCV, int *pBuffer, unsigned int nSamples,
                                   int nChannels) {
    // The cutoff base and the voice's key tracking are combined once per call.
    const float fTrackedCutoff = m_fCutoff * pCV->filterCutoffScale;
    const bool bStereo = nChannels == 2;

    int nFeedback = 0xFFFFFF - Mul24(m_nDamping, m_nCutoff);
    int nGain = Mul24(m_nCutoff, m_nInputGain);
    int nBandL = m_anBand[0];
    int nBandR = m_anBand[1];
    int nLowL = m_anLow[0];
    int nLowR = m_anLow[1];

    // A control block: one envelope value, then fresh coefficients.
    const auto ControlBlock = [&](unsigned int nOffset) {
        // The engine leaves this uninitialised; the default envelope writes
        // nothing, so it is zeroed here instead of read uninitialised.
        short sEnvelope = 0;
        const bool bGate = (pCV->flags & ControlVoltage::kGateBit) != 0;
        const unsigned int nPosition =
            static_cast<unsigned int>(bGate ? pCV->envelopePosition : pCV->releasePosition);
        m_pEnvelope->GenerateValues(nPosition + nOffset, bGate ? 1 : 0, &sEnvelope, 1);

        float fEnvelope = static_cast<float>(sEnvelope) / 32767.0f;
        if (m_bInvertEnvelope) {
            fEnvelope = 1.0f - fEnvelope;
        }
        const float fLift =
            1.0f + static_cast<float>(m_pModulation->m_pnOutput[nOffset]) * (1.0f / 16777215.0f);

        UpdateCoefficients(fEnvelope * (fLift * fTrackedCutoff), fEnvelope * m_fResonance);
        nFeedback = 0xFFFFFF - Mul24(m_nDamping, m_nCutoff);
        nGain = Mul24(m_nCutoff, m_nInputGain);
    };

    // The three taps are written out as separate loops, one per mode, each
    // starting with a control block and refreshing every 16 samples.
    switch (m_nMode) {
    case 1:
        for (unsigned int n = 0, nBlock = 16; n < nSamples; n++) {
            if (++nBlock > 15) {
                ControlBlock(n);
                nBlock = 0;
            }
            Integrate(pBuffer[0], nFeedback, m_nCutoff, nGain, nBandL, nLowL);
            pBuffer[0] = nLowL;
            if (bStereo) {
                Integrate(pBuffer[1], nFeedback, m_nCutoff, nGain, nBandR, nLowR);
                pBuffer[1] = nLowR;
                pBuffer += 2;
            } else {
                pBuffer += 1;
            }
        }
        break;
    case 2:
        for (unsigned int n = 0, nBlock = 16; n < nSamples; n++) {
            if (++nBlock > 15) {
                ControlBlock(n);
                nBlock = 0;
            }
            const int nInputL = pBuffer[0];
            Integrate(nInputL, nFeedback, m_nCutoff, nGain, nBandL, nLowL);
            pBuffer[0] = nInputL - nLowL;
            if (bStereo) {
                const int nInputR = pBuffer[1];
                Integrate(nInputR, nFeedback, m_nCutoff, nGain, nBandR, nLowR);
                pBuffer[1] = nInputR - nLowR;
                pBuffer += 2;
            } else {
                pBuffer += 1;
            }
        }
        break;
    case 3:
        for (unsigned int n = 0, nBlock = 16; n < nSamples; n++) {
            if (++nBlock > 15) {
                ControlBlock(n);
                nBlock = 0;
            }
            // The band tap is trimmed to 0.9 (0xE66665).
            Integrate(pBuffer[0], nFeedback, m_nCutoff, nGain, nBandL, nLowL);
            pBuffer[0] = Mul24(nBandL - nLowL, 0xE66665);
            if (bStereo) {
                Integrate(pBuffer[1], nFeedback, m_nCutoff, nGain, nBandR, nLowR);
                pBuffer[1] = Mul24(nBandR - nLowR, 0xE66665);
                pBuffer += 2;
            } else {
                pBuffer += 1;
            }
        }
        break;
    default:
        break;
    }

    m_anBand[0] = nBandL;
    m_anBand[1] = nBandR;
    m_anLow[0] = nLowL;
    m_anLow[1] = nLowR;
}
