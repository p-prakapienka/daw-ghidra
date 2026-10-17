#pragma once

// ControlVoltage: the per-voice block a SubSynth voice's oscillators and filter
// share: gate and counters, oscillator phases, pitch and glide, pitch sweep,
// key tracking and velocity.
//
// ControlVoltage exports no methods and has no typeinfo: in the engine it is
// a plain struct. Its layout was recovered from the functions that take one
// (FixedPointSVFilter::ProcessCV, FloatSVFilter::ProcessCV,
// Oscillator::GenerateSignal{LQ,HQ}, SuperOscillator), from the block in
// SubSynth::SubSynth that initialises one per voice, from
// SubSynth::PlayChannel, which fills it on note-on, and from the end of
// SubSynth::ProcessChannel, which advances it per block. See
// dsp/ControlVoltage.md.
//
// Ported to the original's structure: fields only, with the engine's offsets
// and types. The code that writes them lives where the engine has it, in
// SubSynth. The defaults below are those of SubSynth's constructor loop. The
// names are ours, in the engine's Hungarian style; fields whose role is not
// established keep their offset as their name. There are no pointers, so the
// block is 0x60 bytes on every host.

#include <cstddef>
#include <cstdint>

struct ControlVoltage {
    // Flag bits at +0x00: bit 0 voice active (cleared by ProcessChannel when
    // the amplitude envelope is done), bit 1 gate (set on note-on, tested by
    // the filter), bit 2 copied from the key event's live-input flag.
    static constexpr std::uint8_t kActiveBit = 0x01;
    static constexpr std::uint8_t kGateBit = 0x02;
    static constexpr std::uint8_t kLiveInputBit = 0x04;

    // Pitch fields are hertz times this: Q12 hertz stored as floats.
    static constexpr float kPitchScale = 4096.0f;

    std::uint8_t m_nFlags = 0;            // +0x00
    std::uint8_t m_anPad01[3] = {0, 0, 0};
    std::int32_t m_nSamplesSinceNoteOn = 0; // +0x04: zeroed on note-on, advanced every block
    std::int32_t m_nEnvelopePosition = 0; // +0x08: envelope position while gated
    std::int32_t m_nReleasePosition = 0;  // +0x0C: envelope position after note-off
    std::uint32_t m_anPhase[2] = {0, 0};  // +0x10: phase of oscillator 0 and 1
    std::int32_t m_n18 = 0;               // +0x18
    std::int32_t m_n1C = 0;               // +0x1C: read by none of the known readers
    std::uint32_t m_nFrequency = 0;       // +0x20: note frequency, Q12 hertz
    float m_fGlideStart = 0.0f;           // +0x24: glide start pitch, Q12 hertz
    float m_fTargetPitch = 0.0f;          // +0x28: glide target pitch
    float m_fCurrentPitch = 0.0f;         // +0x2C: current pitch, moved by the oscillator
    std::uint32_t m_nGlideSamples = 0;    // +0x30: glide length; 0 when no glide
    std::int32_t m_n34 = 0;               // +0x34
    std::int32_t m_n38 = 1;               // +0x38: the one integer set to 1
    std::int32_t m_nNoteId = 0;           // +0x3C: the key event's note
    float m_f40 = 0.0f;                   // +0x40: zeroed on note-on
    float m_f44 = 0.0f;                   // +0x44: zeroed on note-on
    float m_afPitchSweep[2] = {0.0f, 0.0f}; // +0x48: per-oscillator pitch sweep
    float m_fPitchSweepDecay = 0.0f;      // +0x50: per-sample decay of both sweeps
    float m_fFrequency = 0.0f;            // +0x54: note frequency in hertz
    float m_fFilterCutoffScale = 1.0f;    // +0x58: filter key tracking
    float m_fVelocity = 1.0f;             // +0x5C: note velocity, the voice's level target
};

static_assert(sizeof(ControlVoltage) == 0x60, "the per-voice stride in SubSynth");
static_assert(offsetof(ControlVoltage, m_nSamplesSinceNoteOn) == 0x04, "");
static_assert(offsetof(ControlVoltage, m_nEnvelopePosition) == 0x08, "");
static_assert(offsetof(ControlVoltage, m_nReleasePosition) == 0x0C, "");
static_assert(offsetof(ControlVoltage, m_anPhase) == 0x10, "");
static_assert(offsetof(ControlVoltage, m_n1C) == 0x1C, "");
static_assert(offsetof(ControlVoltage, m_nFrequency) == 0x20, "");
static_assert(offsetof(ControlVoltage, m_fGlideStart) == 0x24, "");
static_assert(offsetof(ControlVoltage, m_fTargetPitch) == 0x28, "");
static_assert(offsetof(ControlVoltage, m_fCurrentPitch) == 0x2C, "");
static_assert(offsetof(ControlVoltage, m_nGlideSamples) == 0x30, "");
static_assert(offsetof(ControlVoltage, m_n38) == 0x38, "");
static_assert(offsetof(ControlVoltage, m_nNoteId) == 0x3C, "");
static_assert(offsetof(ControlVoltage, m_afPitchSweep) == 0x48, "");
static_assert(offsetof(ControlVoltage, m_fPitchSweepDecay) == 0x50, "");
static_assert(offsetof(ControlVoltage, m_fFrequency) == 0x54, "");
static_assert(offsetof(ControlVoltage, m_fFilterCutoffScale) == 0x58, "");
static_assert(offsetof(ControlVoltage, m_fVelocity) == 0x5C, "");
