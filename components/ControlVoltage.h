#pragma once

#include <cstddef>
#include <cstdint>

// Reconstructed Caustic per-voice modulation block.
//
// ControlVoltage exports no methods, so its layout was recovered from the
// places that read it — FixedPointSVFilter::ProcessCV, FloatSVFilter::ProcessCV,
// Oscillator::GenerateSignal{LQ,HQ}, SuperOscillator::Trigger and
// GenerateStereoSignal — and from the block in SubSynth::SubSynth that
// initialises one per voice. See components/ControlVoltage.md.
//
// Field names follow the evidence. Where a field's role is established it is
// named; where only its offset, width and default are known it keeps its
// offset as its name rather than a guess.

struct ControlVoltage {
    // Size and stride come from the per-voice loop in SubSynth's constructor.
    static constexpr std::size_t kSize = 0x60;

    // 0x00: flag byte. The constructor clears bits 0, 1 and 2 and leaves the
    // rest of the byte alone, so other bits are set elsewhere.
    std::uint8_t flags;
    std::uint8_t pad01[3];

    // 0x04: read by both oscillator paths and by SuperOscillator.
    float field04;

    std::int32_t field08;

    // 0x0C on the device: pointer to a per-sample modulation array, indexed by
    // the sample offset within the block. The filter dereferences it once per
    // control block; the oscillator reads it on every sample.
    const std::int32_t *modulation;

    std::int32_t field10;
    std::int32_t field14;
    std::int32_t field18;

    // 0x1C: pointer, read only by the standard-quality oscillator.
    const void *field1C;

    std::int32_t field20;

    // 0x24, 0x28, 0x2C: floats read by both oscillator paths. 0x28 is read
    // twice per call and is multiplied by the caller's float argument, so it
    // carries pitch.
    float field24;
    float pitch;
    float field2C;

    // 0x30: pointer. Non-zero sends the oscillator down a different path, and
    // SuperOscillator reads it in both Trigger and GenerateStereoSignal.
    const void *field30;

    std::int32_t field34;

    // 0x38: the one integer the constructor sets to something other than zero.
    std::int32_t field38;

    std::int32_t field3C;

    float field40;
    float field44;
    float field48;
    float field4C;

    // 0x50: read several times per call by the oscillator.
    float field50;

    // 0x54: read by SuperOscillator::Trigger.
    float field54;

    // 0x58: multiplied into the filter's cutoff base once per control block.
    // Defaults to 1.0, so an untouched voice leaves the cutoff alone.
    float filterCutoffScale;

    // 0x5C: initialised to 1.0 alongside 0x58 and not read by anything
    // reconstructed so far.
    float field5C;

    // Defaults exactly as SubSynth's constructor leaves them.
    static ControlVoltage makeDefault();
};

// The engine is 32-bit, so its pointers are four bytes and the struct is
// exactly 0x60. This mirror exists to hold the recovered offsets in code that a
// compiler checks, on any host. The usable struct above keeps native pointers
// and therefore a different size.
namespace controlVoltageLayout {

struct Engine {
    std::uint8_t flags;
    std::uint8_t pad01[3];
    float field04;
    std::int32_t field08;
    std::uint32_t modulation;
    std::int32_t field10;
    std::int32_t field14;
    std::int32_t field18;
    std::uint32_t field1C;
    std::int32_t field20;
    float field24;
    float pitch;
    float field2C;
    std::uint32_t field30;
    std::int32_t field34;
    std::int32_t field38;
    std::int32_t field3C;
    float field40;
    float field44;
    float field48;
    float field4C;
    float field50;
    float field54;
    float filterCutoffScale;
    float field5C;
};

static_assert(sizeof(Engine) == ControlVoltage::kSize,
              "the engine block is 0x60 bytes, the per-voice stride in SubSynth");
static_assert(offsetof(Engine, field04) == 0x04, "");
static_assert(offsetof(Engine, modulation) == 0x0C, "");
static_assert(offsetof(Engine, field1C) == 0x1C, "");
static_assert(offsetof(Engine, field24) == 0x24, "");
static_assert(offsetof(Engine, pitch) == 0x28, "");
static_assert(offsetof(Engine, field2C) == 0x2C, "");
static_assert(offsetof(Engine, field30) == 0x30, "");
static_assert(offsetof(Engine, field38) == 0x38, "");
static_assert(offsetof(Engine, field50) == 0x50, "");
static_assert(offsetof(Engine, field54) == 0x54, "");
static_assert(offsetof(Engine, filterCutoffScale) == 0x58, "");
static_assert(offsetof(Engine, field5C) == 0x5C, "");

} // namespace controlVoltageLayout
