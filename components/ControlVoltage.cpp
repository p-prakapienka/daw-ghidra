#include "ControlVoltage.h"

#include <cmath>

ControlVoltage ControlVoltage::makeDefault() {
    ControlVoltage voltage{};

    // The constructor zeroes every integer and pointer except 0x38, zeroes
    // every float except the pair at 0x58, and clears the low three flag bits.
    voltage.flags = 0;
    voltage.field38 = 1;
    voltage.filterCutoffScale = 1.0f;
    voltage.field5C = 1.0f;

    return voltage;
}

float ControlVoltage::trackingScale(float frequencyHz, float keyboardTracking) {
    // PlayChannel: 1 + tracking * (sqrt(frequency / 500) - 1). The square root
    // means an octave up opens the cutoff by a factor of sqrt(2), not 2.
    const float ratio = std::sqrt(frequencyHz / kTrackingPivotHertz);
    return 1.0f + keyboardTracking * (ratio - 1.0f);
}

void ControlVoltage::noteOn(float frequencyHz, int note, float keyboardTracking,
                            float eventValue) {
    const float scaled = frequencyHz * kPitchScale;

    flags = static_cast<std::uint8_t>(flags | 0x03);
    noteId = note;
    frequencyHertz = frequencyHz;
    frequencyQ12 = static_cast<std::uint32_t>(scaled);
    field5C = eventValue;
    filterCutoffScale = trackingScale(frequencyHz, keyboardTracking);

    // Runtime state the oscillator and glide own is cleared for the new note.
    field04 = 0.0f;
    field08 = 0;
    modulation = nullptr;
    field10 = 0;
    field14 = 0;
    field40 = 0.0f;
    field44 = 0.0f;

    // No glide: the three pitch fields agree and the glide length is zero.
    glideStartPitch = scaled;
    pitch = scaled;
    targetPitch = scaled;
    glideSamples = 0;
}

void ControlVoltage::beginGlide(float targetFrequencyHz, std::uint32_t lengthSamples) {
    // The engine leaves 0x28 alone here so the oscillator slides from where
    // the previous note left it.
    glideStartPitch = pitch;
    targetPitch = targetFrequencyHz * kPitchScale;
    frequencyQ12 = static_cast<std::uint32_t>(targetPitch);
    frequencyHertz = targetFrequencyHz;
    glideSamples = lengthSamples;
}
