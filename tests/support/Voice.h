#pragma once

// Test helpers that put a ControlVoltage into the states SubSynth puts it in.
// In the engine this is code inside SubSynth::PlayChannel (note-on) and at the
// end of SubSynth::ProcessChannel (the per-block advance); these reproduce
// it for tests that drive an oscillator or a filter on their own.

#include <cmath>
#include <cstdint>

#include "dsp/ControlVoltage.h"

// PlayChannel's key tracking: 1 + tracking * (sqrt(hz / 500) - 1).
inline float trackingScale(float frequencyHz, float keyboardTracking) {
    return 1.0f + keyboardTracking * (std::sqrt(frequencyHz / 500.0f) - 1.0f);
}

// PlayChannel for a note with no glide, no slide and no stealing.
inline void noteOn(ControlVoltage &cv, float frequencyHz, int note, float keyboardTracking,
                   float velocity, float sweep = 0.0f, float sweepDecay = 1.0f) {
    const float pitch = frequencyHz * ControlVoltage::kPitchScale;
    cv.m_nFlags = static_cast<std::uint8_t>(cv.m_nFlags | ControlVoltage::kActiveBit
                                            | ControlVoltage::kGateBit);
    cv.m_nNoteId = note;
    cv.m_fFrequency = frequencyHz;
    cv.m_nFrequency = static_cast<std::uint32_t>(pitch);
    cv.m_fVelocity = velocity;
    cv.m_fFilterCutoffScale = trackingScale(frequencyHz, keyboardTracking);
    cv.m_nSamplesSinceNoteOn = 0;
    cv.m_nEnvelopePosition = 0;
    cv.m_nReleasePosition = 0;
    cv.m_anPhase[0] = 0;
    cv.m_anPhase[1] = 0;
    cv.m_f40 = 0.0f;
    cv.m_f44 = 0.0f;
    cv.m_afPitchSweep[0] = sweep;
    cv.m_afPitchSweep[1] = sweep;
    cv.m_fPitchSweepDecay = sweepDecay;
    cv.m_fGlideStart = pitch;
    cv.m_fTargetPitch = pitch;
    cv.m_fCurrentPitch = pitch;
    cv.m_nGlideSamples = 0;
}

// PlayChannel's glide branch: start from the current pitch, aim at the new note.
inline void beginGlide(ControlVoltage &cv, float targetFrequencyHz, std::uint32_t lengthSamples) {
    cv.m_fGlideStart = cv.m_fCurrentPitch;
    cv.m_fTargetPitch = targetFrequencyHz * ControlVoltage::kPitchScale;
    cv.m_nSamplesSinceNoteOn = 0;
    cv.m_nFrequency = static_cast<std::uint32_t>(cv.m_fTargetPitch);
    cv.m_fFrequency = targetFrequencyHz;
    cv.m_nGlideSamples = lengthSamples;
}

inline bool isGateOpen(const ControlVoltage &cv) {
    return (cv.m_nFlags & ControlVoltage::kGateBit) != 0;
}

inline void noteOff(ControlVoltage &cv) {
    cv.m_nFlags = static_cast<std::uint8_t>(cv.m_nFlags & ~ControlVoltage::kGateBit);
}

// The end of ProcessChannel: both note counters move every block, the
// release counter only while the gate is closed.
inline void advance(ControlVoltage &cv, std::uint32_t numSamples) {
    const auto count = static_cast<std::int32_t>(numSamples);
    cv.m_nSamplesSinceNoteOn += count;
    cv.m_nEnvelopePosition += count;
    if (!isGateOpen(cv)) {
        cv.m_nReleasePosition += count;
    }
}
