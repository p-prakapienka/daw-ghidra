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
    // Size and stride come from the per-voice loop in SubSynth's constructor.
    static constexpr std::size_t kSize = 0x60;

    // Pitch fields hold frequency in hertz multiplied by this, so they are
    // Q12 fixed-point hertz stored as floats.
    static constexpr float kPitchScale = 4096.0f;

    // Keyboard tracking pivots here: a note at this frequency leaves the
    // filter cutoff unchanged whatever the tracking amount.
    static constexpr float kTrackingPivotHertz = 500.0f;

    // 0x00: flag byte. Bit 0 is voice-active, cleared by ProcessChannel once
    // the envelope reports done. Bit 1 is the gate, set on note-on and tested
    // by the filter to choose which envelope position to use. Bit 2 is copied
    // from the key event's flag word on note-on.
    static constexpr std::uint8_t kActiveBit = 0x01;
    static constexpr std::uint8_t kGateBit = 0x02;

    std::uint8_t flags;
    std::uint8_t pad01[3];

    // 0x04: samples since note-on. Zeroed on note-on, advanced by every
    // processed block.
    std::int32_t samplesSinceNoteOn;

    // 0x08: the envelope position while the gate is open. Advanced by every
    // block; the filter passes it to the envelope as the position argument.
    std::int32_t envelopePosition;

    // 0x0C: the envelope position after note-off. Advanced only while the
    // gate is closed, and passed to IsDone to decide when the voice stops.
    std::int32_t releasePosition;

    // 0x10, 0x14: zeroed on note-on unless the note is held legato.
    std::int32_t field10;
    std::int32_t field14;
    std::int32_t field18;

    // 0x1C: pointer, read only by the standard-quality oscillator.
    const void *field1C;

    // 0x20: the note frequency in Q12 hertz as an unsigned integer.
    std::uint32_t frequencyQ12;

    // 0x24, 0x28, 0x2C: pitch in Q12 hertz. Without glide all three are set
    // to the new note. With glide the current value is left to slide towards
    // the target, which is why the oscillator reads 0x28 twice per call.
    float glideStartPitch;
    float pitch;
    float targetPitch;

    // 0x30: glide length in samples. Zero means no glide, and the oscillator
    // takes a different path when it is non-zero. Not a pointer.
    std::uint32_t glideSamples;

    std::int32_t field34;

    // 0x38: the one integer the constructor sets to something other than zero.
    std::int32_t field38;

    // 0x3C: the key event's note identifier.
    std::int32_t noteId;

    // 0x40, 0x44: zeroed on note-on.
    float field40;
    float field44;

    // 0x48, 0x4C: both copied from the same machine-level float on note-on.
    float field48;
    float field4C;

    // 0x50: copied from a second machine-level float on note-on; read several
    // times per call by the oscillator.
    float field50;

    // 0x54: the note frequency in hertz.
    float frequencyHertz;

    // 0x58: filter keyboard tracking, folded into the filter's cutoff base once
    // per control block. 1.0 at the pivot frequency or with tracking off.
    float filterCutoffScale;

    // 0x5C: a float copied from the key event on note-on. Defaults to 1.0,
    // which fits velocity, but that reading is a hypothesis.
    float field5C;

    // Defaults exactly as SubSynth's constructor leaves them.
    static ControlVoltage makeDefault();

    // Reproduce SubSynth::PlayChannel's note-on writes for a note with no
    // glide. keyboardTracking is the machine's filter tracking amount, and
    // eventValue is the float the engine copies from the key event.
    void noteOn(float frequencyHz, int note, float keyboardTracking, float eventValue);

    // Retarget the pitch fields for a glide of the given length, leaving the
    // current pitch where it is so it can slide.
    void beginGlide(float targetFrequencyHz, std::uint32_t lengthSamples);

    // The keyboard tracking curve on its own, for tests and for hosts that
    // want to drive the filter without a full note-on.
    static float trackingScale(float frequencyHz, float keyboardTracking);

    bool isActive() const { return (flags & kActiveBit) != 0; }
    bool isGateOpen() const { return (flags & kGateBit) != 0; }

    // Close the gate. Release time starts counting from the next block.
    void noteOff() { flags = static_cast<std::uint8_t>(flags & ~kGateBit); }

    // Reproduce the end of SubSynth::ProcessChannel: both note counters move
    // every block, the release counter only while the gate is closed.
    void advance(std::uint32_t numSamples);

    // The position the filter hands to its envelope for this block.
    std::int32_t envelopePositionForGate() const {
        return isGateOpen() ? envelopePosition : releasePosition;
    }
};

// The engine is 32-bit, so its pointers are four bytes and the struct is
// exactly 0x60. This mirror exists to hold the recovered offsets in code that a
// compiler checks, on any host. The usable struct above keeps native pointers
// and therefore a different size.
namespace controlVoltageLayout {

struct Engine {
    std::uint8_t flags;
    std::uint8_t pad01[3];
    std::int32_t samplesSinceNoteOn;
    std::int32_t envelopePosition;
    std::int32_t releasePosition;
    std::int32_t field10;
    std::int32_t field14;
    std::int32_t field18;
    std::uint32_t field1C;
    std::uint32_t frequencyQ12;
    float glideStartPitch;
    float pitch;
    float targetPitch;
    std::uint32_t glideSamples;
    std::int32_t field34;
    std::int32_t field38;
    std::int32_t noteId;
    float field40;
    float field44;
    float field48;
    float field4C;
    float field50;
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
static_assert(offsetof(Engine, glideStartPitch) == 0x24, "");
static_assert(offsetof(Engine, pitch) == 0x28, "");
static_assert(offsetof(Engine, targetPitch) == 0x2C, "");
static_assert(offsetof(Engine, glideSamples) == 0x30, "");
static_assert(offsetof(Engine, field38) == 0x38, "");
static_assert(offsetof(Engine, noteId) == 0x3C, "");
static_assert(offsetof(Engine, field48) == 0x48, "");
static_assert(offsetof(Engine, field50) == 0x50, "");
static_assert(offsetof(Engine, frequencyHertz) == 0x54, "");
static_assert(offsetof(Engine, filterCutoffScale) == 0x58, "");
static_assert(offsetof(Engine, field5C) == 0x5C, "");

} // namespace controlVoltageLayout
