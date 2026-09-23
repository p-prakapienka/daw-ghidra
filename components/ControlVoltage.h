#pragma once

#include <cstddef>
#include <cstdint>

// Reconstructed Caustic per-voice modulation block.
//
// ControlVoltage exports no methods, so its layout was recovered from the
// places that read it — FixedPointSVFilter::ProcessCV, FloatSVFilter::ProcessCV,
// Oscillator::GenerateSignal{LQ,HQ}, SuperOscillator::Trigger and
// GenerateStereoSignal — from the block in SubSynth::SubSynth that initialises
// one per voice, from SubSynth::PlayChannel, which fills it on note-on, and
// from the end of SubSynth::ProcessChannel, which advances it per block.
// See components/ControlVoltage.md.
//
// Field names follow the evidence. Where a field's role is established it is
// named; where only its offset, width and default are known it keeps its
// offset as its name rather than a guess.

struct ControlVoltage {
    static constexpr std::size_t kSize = 0x60;
    static constexpr float kPitchScale = 4096.0f;
    static constexpr float kTrackingPivotHertz = 500.0f;
    static constexpr std::uint8_t kActiveBit = 0x01;
    static constexpr std::uint8_t kGateBit = 0x02;

    std::uint8_t flags;
    std::uint8_t pad01[3];
    std::int32_t samplesSinceNoteOn;
    std::int32_t envelopePosition;
    std::int32_t releasePosition;
    std::uint32_t phase[2];
    std::int32_t field18;
    const void *field1C;
    std::uint32_t frequencyQ12;
    float glideStartPitch;
    float targetPitch;
    float currentPitch;
    std::uint32_t glideSamples;
    std::int32_t field34;
    std::int32_t field38;
    std::int32_t noteId;
    float field40;
    float field44;
    float pitchSweep[2];
    float pitchSweepDecay;
    float frequencyHertz;
    float filterCutoffScale;
    float field5C;

    static ControlVoltage makeDefault();
    void noteOn(float frequencyHz, int note, float keyboardTracking, float eventValue,
                float sweep = 0.0f, float sweepDecay = 1.0f);
    void beginGlide(float targetFrequencyHz, std::uint32_t lengthSamples);
    static float trackingScale(float frequencyHz, float keyboardTracking);
    bool isActive() const { return (flags & kActiveBit) != 0; }
    bool isGateOpen() const { return (flags & kGateBit) != 0; }
    void noteOff() { flags = static_cast<std::uint8_t>(flags & ~kGateBit); }
    void advance(std::uint32_t numSamples);
    std::int32_t envelopePositionForGate() const {
        return isGateOpen() ? envelopePosition : releasePosition;
    }
};

namespace controlVoltageLayout {

struct Engine {
    std::uint8_t flags;
    std::uint8_t pad01[3];
    std::int32_t samplesSinceNoteOn;
    std::int32_t envelopePosition;
    std::int32_t releasePosition;
    std::uint32_t phase[2];
    std::int32_t field18;
    std::uint32_t field1C;
    std::uint32_t frequencyQ12;
    float glideStartPitch;
    float targetPitch;
    float currentPitch;
    std::uint32_t glideSamples;
    std::int32_t field34;
    std::int32_t field38;
    std::int32_t noteId;
    float field40;
    float field44;
    float pitchSweep[2];
    float pitchSweepDecay;
    float frequencyHertz;
    float filterCutoffScale;
    float field5C;
};

static_assert(sizeof(Engine) == ControlVoltage::kSize,
              "the engine block is 0x60 bytes, the per-voice stride in SubSynth");
static_assert(offsetof(Engine, samplesSinceNoteOn) == 0x04, "");
static_assert(offsetof(Engine, envelopePosition) == 0x08, "");
static_assert(offsetof(Engine, releasePosition) == 0x0C, "");
static_assert(offsetof(Engine, field1C) == 0x1C, "");
static_assert(offsetof(Engine, frequencyQ12) == 0x20, "");
static_assert(offsetof(Engine, phase) == 0x10, "");
static_assert(offsetof(Engine, glideStartPitch) == 0x24, "");
static_assert(offsetof(Engine, targetPitch) == 0x28, "");
static_assert(offsetof(Engine, currentPitch) == 0x2C, "");
static_assert(offsetof(Engine, glideSamples) == 0x30, "");
static_assert(offsetof(Engine, field38) == 0x38, "");
static_assert(offsetof(Engine, noteId) == 0x3C, "");
static_assert(offsetof(Engine, pitchSweep) == 0x48, "");
static_assert(offsetof(Engine, pitchSweepDecay) == 0x50, "");
static_assert(offsetof(Engine, frequencyHertz) == 0x54, "");
static_assert(offsetof(Engine, filterCutoffScale) == 0x58, "");
static_assert(offsetof(Engine, field5C) == 0x5C, "");

} // namespace controlVoltageLayout
