#pragma once

// SubSynth's control collection: the id table, the controls' own value
// handling, and the OnChildModified branches that turn a control value into
// engine state.
//
// Recovered from SubSynth::SubSynth (the AddControl calls),
// SubSynth::OnChildModified, SubSynth::OnSequencerControlEvent,
// SubSynth::GetControlIDFromName, SubSynth::SetOscillator1ModulationMode,
// SubSynth::SetOscillator2CentsMode, SubSynth::Serialize and the SetValue /
// GetValue / GetMinMaxValues methods of UIKnob, UISlider, UISelector and
// UIValueContainer in libcaustic.so (ARMv7). See
// src/machines/subsynth/SubSynthControls.md.
//
// The engine keeps the value in the UI control and the branch reads it back
// with GetValue, so this class keeps one stored value per id and applies the
// branch after every set. The UI-only controls (preset browser, mute, solo,
// polyphony and waveform step buttons, VU meter, keyboard, LED) are not
// modelled.

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

class SubSynth;
class SubSynthArchive;

class SubSynthControls {
public:
    // How the control stores and reports a value.
    enum class Kind {
        FileSelector,   // id 0, the preset browser
        Knob,           // UIKnob
        Slider,         // UISlider
        Selector,       // UITextSelector / UIImageSelector
        ValueContainer, // UIValueContainer, stores unclamped
    };

    // UIKnob's +0x28: what SetValue and GetValue do to the position.
    enum class Curve {
        Linear,  // 0
        Square,  // 1: GetValue returns position²
        Integer, // 2: SetValue stores floorf(v + 0.5)
    };

    struct Control {
        int id;
        std::string_view name;  // GetControlIDFromName, empty for id 0
        std::string_view label; // the AddControl label, empty where none
        std::uint32_t field;    // SubSynth offset holding the control pointer
        Kind kind;
        float minimum; // GetMinMaxValues
        float maximum; // for a selector, the item count
        Curve curve;
        float defaultValue; // after the control's own clamp and rounding
        bool saved;         // explicit id, written to CCOL
    };

    // Ids 0-8, 10-29 and 31 are saved; 32 is the pitch bend, the first id
    // AddControl assigned itself. Slots 9 and 30 are empty.
    static constexpr int kPitchBendId = 32;
    static constexpr int kMaxId = 45;

    // OnSequencerControlEvent's MIDI-mapping targets that are not controls.
    static constexpr int kCentsModeTarget = 0x400;
    static constexpr int kSustainTarget = 0x401;
    static constexpr int kModulationModeTarget = 0x1E;

    // The ids whose times the version <= 2.1.0 fix-up rewrites.
    static constexpr std::array<int, 6> kLegacyTimeIds{21, 22, 24, 25, 26, 28};

    static const Control *find(int id);
    static const std::array<Control, 31> &table();

    // GetControlIDFromName: -1 for anything not in its 30 names.
    static int idFromName(std::string_view name);

    // Positions start at the defaults; nothing is pushed to the machine.
    explicit SubSynthControls(SubSynth &synth);

    // UIControl::SetValue followed by the OnChildModified branch. Returns
    // false, changing nothing, for an id this class does not model.
    bool set(int id, float value);

    // Every modelled control set to its default through set(), as the
    // engine's constructor does: each default SetValue there reaches
    // OnChildModified.
    void resetToDefaults();

    // What the control stores and saves (GetSerializeValue).
    float position(int id) const;
    // What the branch reads (GetValue): position² for a square knob.
    float value(int id) const;

    // SetOscillator1ModulationMode and SetOscillator2CentsMode.
    void setModulationMode(int mode);
    void setCentsMode(int mode);
    int modulationMode() const { return modulationMode_; }
    int centsMode() const { return centsMode_; }

    // OnSequencerControlEvent, plain automation: SetValue with the raw
    // control value, in the same units CCOL stores.
    bool applyAutomation(int id, float value) { return set(id, value); }

    // OnSequencerControlEvent, a mapped MIDI CC with value 0..1: the three
    // in-place targets, or the control's range from GetMinMaxValues.
    bool applyMidiTarget(int id, float value);

    // SubSynth::Serialize's load path, in its order: the collection, the
    // custom waveforms, then polyphony and the two modes, then the legacy
    // fix-ups. Names, the preset path and the keyboard octave are UI state and
    // are not applied.
    void load(const SubSynthArchive &archive);

    // g_nPitchBendSemis, .data 0x343908. Its writers are not traced; the
    // initial value is 1.
    int pitchBendSemitones = 1;

private:
    void applyEffect(int id);
    void updateOscillator2Pitch(bool fromCents);
    void updateBend();

    SubSynth &synth_;
    std::array<float, kMaxId + 1> positions_{};
    int modulationMode_ = 0; // +0x2F88
    int centsMode_ = 0;      // +0x2F84
};
