#include "machines/subsynth/SubSynthControls.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "machines/subsynth/SubSynth.h"
#include "machines/subsynth/SubSynthArchive.h"

namespace {

using Kind = SubSynthControls::Kind;
using Curve = SubSynthControls::Curve;

// The constructor's AddControl calls, by id. Ranges, curves and defaults are
// the control objects' state after construction; see SubSynthControls.md.
constexpr std::array<SubSynthControls::Control, 31> kControls{{
    {0, "", "", 0xC1F8, Kind::FileSelector, 0.0f, 0.0f, Curve::Linear, 0.0f, true},
    {1, "osc1_waveform", "", 0xC200, Kind::ValueContainer, 0.0f, 8.0f, Curve::Linear, 0.0f, true},
    {2, "osc_mix", "Osc|Mix", 0xC228, Kind::Knob, 0.0f, 1.0f, Curve::Linear, 0.5f, true},
    {3, "osc_bend", "Bend|Amount", 0xC208, Kind::Knob, -0.99f, 0.99f, Curve::Linear, 0.0f, true},
    {4, "osc2_waveform", "", 0xC20C, Kind::ValueContainer, 0.0f, 9.0f, Curve::Linear, 0.0f, true},
    {5, "osc2_phase", "Osc2|Phase", 0xC214, Kind::Knob, -0.5f, 0.5f, Curve::Linear, 0.0f, true},
    {6, "osc2_octave", "Osc2|Octave", 0xC218, Kind::Knob, -4.0f, 4.0f, Curve::Integer, 0.0f, true},
    {7, "osc2_semis", "Osc2|Semis", 0xC21C, Kind::Knob, -12.0f, 12.0f, Curve::Integer, 0.0f, true},
    {8, "osc2_cents", "Osc2|Cents", 0xC220, Kind::Knob, -50.0f, 50.0f, Curve::Linear, 0.0f, true},
    {10, "lfo1_waveform", "", 0xC234, Kind::Selector, 0.0f, 4.0f, Curve::Linear, 0.0f, true},
    {11, "lfo1_target", "", 0xC238, Kind::Selector, 0.0f, 10.0f, Curve::Linear, 0.0f, true},
    {12, "lfo1_rate", "LFO1|Rate", 0xC23C, Kind::Knob, 0.0f, 12.0f, Curve::Integer, 6.0f, true},
    {13, "lfo1_depth", "LFO1|Depth", 0xC240, Kind::Knob, 0.0f, 1.0f, Curve::Linear, 0.0f, true},
    {14, "lfo2_target", "", 0xC244, Kind::Selector, 0.0f, 10.0f, Curve::Linear, 0.0f, true},
    {15, "lfo2_rate", "LFO2|Rate", 0xC248, Kind::Knob, 0.0f, 12.0f, Curve::Integer, 6.0f, true},
    {16, "lfo2_depth", "LFO2|Depth", 0xC24C, Kind::Knob, 0.0f, 1.0f, Curve::Linear, 0.0f, true},
    {17, "filter_cutoff", "Filter|Cutoff", 0xC250, Kind::Slider, 0.001f, 1.0f, Curve::Linear, 1.0f, true},
    {18, "filter_resonance", "Filter|Res", 0xC254, Kind::Slider, 0.0f, 0.9f, Curve::Linear, 0.0f, true},
    {19, "filter_type", "", 0xC258, Kind::Selector, 0.0f, 7.0f, Curve::Linear, 0.0f, true},
    {20, "filter_kbtrack", "Filter|KBTrack", 0xC25C, Kind::Knob, 0.0f, 1.0f, Curve::Linear, 0.0f, true},
    {21, "filter_attack", "Filter|Attack", 0xC260, Kind::Knob, 0.0f, 1.75f, Curve::Square, 0.0f, true},
    {22, "filter_decay", "Filter|Decay", 0xC264, Kind::Knob, 0.0f, 1.75f, Curve::Square, 0.0f, true},
    {23, "filter_sustain", "Filter|Sustain", 0xC268, Kind::Knob, 0.0f, 1.0f, Curve::Linear, 1.0f, true},
    {24, "filter_release", "Filter|Release", 0xC26C, Kind::Knob, 0.0f, 1.75f, Curve::Square, 1.75f, true},
    {25, "volume_attack", "Volume|Attack", 0xC270, Kind::Knob, 0.0f, 1.75f, Curve::Square, 0.0f, true},
    {26, "volume_decay", "Volume|Decay", 0xC274, Kind::Knob, 0.0f, 1.75f, Curve::Square, 0.0f, true},
    {27, "volume_sustain", "Volume|Sustain", 0xC278, Kind::Knob, 0.0f, 1.0f, Curve::Linear, 1.0f, true},
    {28, "volume_release", "Volume|Release", 0xC27C, Kind::Knob, 0.0f, 1.75f, Curve::Square, 0.0f, true},
    {29, "volume_out", "Out|Volume", 0xC290, Kind::Knob, 0.0f, 2.0f, Curve::Linear, 1.0f, true},
    {31, "osc_modulation", "Mod|Amount", 0xC22C, Kind::Knob, 0.0f, 1.0f, Curve::Linear, 0.0f, true},
    {32, "pitch_bend", "", 0xC1F0, Kind::ValueContainer, 0.0f, 1.0f, Curve::Linear, 0.5f, false},
}};

// The literals OnSequencerControlEvent compares a mapped value against.
constexpr float kModulationModeLow = 0.333f;  // 0x3EAA7EFA
constexpr float kModulationModeHigh = 0.666f; // 0x3F2A7EFA

// vcvt.s32.f32: truncate toward zero, clamped to the int range, with NaN as zero.
int toInt(float value) {
    if (std::isnan(value)) {
        return 0;
    }
    if (value >= 2147483648.0f) {
        return std::numeric_limits<int>::max();
    }
    if (value <= -2147483648.0f) {
        return std::numeric_limits<int>::min();
    }
    return static_cast<int>(value);
}

// UIKnob and UISlider: below the minimum gives the minimum, anything not at
// or below the maximum (NaN included) gives the maximum.
float clampToRange(float value, float minimum, float maximum) {
    if (minimum > value) {
        return minimum;
    }
    return value <= maximum ? value : maximum;
}

// The "· 12 / 12" pairs are in the engine's arithmetic.
float centsRatio(float cents) { return std::pow(2.0f, cents / 1200.0f * 12.0f / 12.0f); }

} // namespace

const std::array<SubSynthControls::Control, 31> &SubSynthControls::table() { return kControls; }

const SubSynthControls::Control *SubSynthControls::find(int id) {
    for (const Control &control : kControls) {
        if (control.id == id) {
            return &control;
        }
    }
    return nullptr;
}

int SubSynthControls::idFromName(std::string_view name) {
    for (const Control &control : kControls) {
        if (!control.name.empty() && control.name == name) {
            return control.id;
        }
    }
    return -1;
}

SubSynthControls::SubSynthControls(SubSynth &synth) : synth_(synth) {
    for (const Control &control : kControls) {
        positions_[static_cast<std::size_t>(control.id)] = control.defaultValue;
    }
}

bool SubSynthControls::set(int id, float value) {
    const Control *control = find(id);
    if (control == nullptr) {
        return false;
    }

    float stored = value;
    switch (control->kind) {
    case Kind::Knob:
        stored = clampToRange(value, control->minimum, control->maximum);
        if (control->curve == Curve::Integer) {
            stored = std::floor(stored + 0.5f);
        }
        break;
    case Kind::Slider:
        stored = clampToRange(value, control->minimum, control->maximum);
        break;
    case Kind::Selector: {
        int index = toInt(value);
        const int count = static_cast<int>(control->maximum);
        if (index < 0) {
            index = 0;
        } else if (index >= count) {
            index = count - 1;
        }
        stored = static_cast<float>(index);
        break;
    }
    case Kind::FileSelector:
    case Kind::ValueContainer:
        break;
    }

    positions_[static_cast<std::size_t>(id)] = stored;
    applyEffect(id);
    return true;
}

void SubSynthControls::resetToDefaults() {
    for (const Control &control : kControls) {
        set(control.id, control.defaultValue);
    }
}

float SubSynthControls::position(int id) const {
    if (id < 0 || id > kMaxId) {
        return 0.0f;
    }
    return positions_[static_cast<std::size_t>(id)];
}

float SubSynthControls::value(int id) const {
    const float stored = position(id);
    const Control *control = find(id);
    if (control != nullptr && control->kind == Kind::Knob && control->curve == Curve::Square) {
        return stored * stored;
    }
    return stored;
}

void SubSynthControls::setModulationMode(int mode) {
    modulationMode_ = mode;
    synth_.shared.oscillator1.setModulationMode(static_cast<Oscillator::ModulationMode>(mode));
}

void SubSynthControls::setCentsMode(int mode) {
    centsMode_ = mode;
    synth_.shared.oscillator1.setPitchRatio(mode == 0 ? 1.0f : centsRatio(-value(8)));
}

bool SubSynthControls::applyMidiTarget(int id, float value) {
    switch (id) {
    case kCentsModeTarget:
        setCentsMode(value > 0.5f ? 1 : 0);
        return true;
    case kSustainTarget:
        synth_.setSustain(value >= 0.5f);
        return true;
    case kModulationModeTarget:
        setModulationMode(value < kModulationModeLow ? 0 : value > kModulationModeHigh ? 2 : 1);
        return true;
    default:
        break;
    }

    const Control *control = find(id);
    if (control == nullptr || control->kind == Kind::FileSelector) {
        return false;
    }
    // GetMinMaxValues: a selector reports 0 and its item count.
    const float minimum = control->kind == Kind::Selector ? 0.0f : control->minimum;
    const float maximum = control->maximum;
    return set(id, minimum + (maximum - minimum) * value);
}

void SubSynthControls::load(const SubSynthArchive &archive) {
    for (const SubSynthArchive::Control &control : archive.controls) {
        if (control.id <= static_cast<std::uint32_t>(kMaxId)) {
            set(static_cast<int>(control.id), control.value);
        }
    }

    // The custom waveforms, each through SetRawValues and GetWaveform. Before
    // 3.0 there is no second waveform and the engine clears only the first
    // 660 bytes of the buffer still holding the first one, so editor 2 gets
    // 330 zeros followed by the second half of waveform 1 (0x9d8d0).
    if (archive.customWaveform1) {
        synth_.customWaveforms[0].setRawValues(archive.customWaveform1->data());
        synth_.refreshCustomWavetable(0);
        SubSynthArchive::Waveform second = *archive.customWaveform1;
        if (archive.customWaveform2) {
            second = *archive.customWaveform2;
        } else {
            std::fill(second.begin(), second.begin() + second.size() / 2, std::int16_t{0});
        }
        synth_.customWaveforms[1].setRawValues(second.data());
        synth_.refreshCustomWavetable(1);
    }

    synth_.setPolyphony(archive.polyphony);
    setModulationMode(archive.modulationMode);
    setCentsMode(archive.centsMode);

    // Version <= 2.1.0 stored the times themselves. GetValue squares the
    // position, so the fourth root gives back the position whose square is
    // the stored time.
    if (archive.needsQuarticTimeFixup()) {
        for (const int id : kLegacyTimeIds) {
            set(id, std::sqrt(std::sqrt(value(id))));
        }
    }

    // Before 3.0 there were no HQ waveforms (types 3 and 5).
    if (archive.needsWaveformRenumbering()) {
        float oscillator1 = value(1);
        if (oscillator1 >= 4.0f) {
            oscillator1 += 2.0f;
        } else if (oscillator1 >= 3.0f) {
            oscillator1 += 1.0f;
        }
        set(1, oscillator1);

        float oscillator2 = value(4);
        if (oscillator2 >= 5.0f) {
            oscillator2 += 2.0f;
        } else if (oscillator2 >= 4.0f) {
            oscillator2 += 1.0f;
        }
        set(4, oscillator2);
    }
}

void SubSynthControls::applyEffect(int id) {
    SubSynthVoice::Shared &shared = synth_.shared;
    const float v = value(id);

    switch (id) {
    case 1: {
        // SetOscillatorType stores any value but only switches tables for
        // 0..8; outside that the playing table is kept.
        const int type = toInt(v);
        if (type >= 0 && type <= 8) {
            shared.oscillator1.setType(static_cast<Oscillator::Type>(type));
        }
        break;
    }
    case 2:
        shared.setMix(v);
        break;
    case 3:
        synth_.setPitchSweep(v);
        break;
    case 4: {
        const int controlValue = toInt(v);
        if (controlValue >= 0 && controlValue <= 9) {
            shared.setOscillator2Waveform(controlValue);
        } else {
            shared.oscillator2Enabled = true;
        }
        break;
    }
    case 5:
        shared.oscillator2.setPhaseOffset(static_cast<int>(v * 4096.0f));
        break;
    case 6:
    case 7:
        updateOscillator2Pitch(false);
        break;
    case 8:
        updateOscillator2Pitch(true);
        break;
    case 10:
        synth_.lfo1.m_nWaveform = toInt(v);
        synth_.lfo1.OnParamModified();
        break;
    case 11:
        synth_.setLFOTargets(static_cast<LFO::Target>(toInt(v)), synth_.lfo2Target());
        break;
    case 12:
        synth_.lfo1.m_fRate = v;
        synth_.lfo1.OnParamModified();
        break;
    case 13:
        synth_.lfo1.m_nDepth = toInt(v * 16777215.0f);
        synth_.lfo1.OnParamModified();
        break;
    case 14:
        synth_.setLFOTargets(synth_.lfo1Target(), static_cast<LFO::Target>(toInt(v)));
        break;
    case 15:
        synth_.lfo2.m_fRate = v;
        synth_.lfo2.OnParamModified();
        break;
    case 16:
        synth_.lfo2.m_nDepth = toInt(v * 16777215.0f);
        synth_.lfo2.OnParamModified();
        break;
    case 17:
        for (SubSynthVoice &voice : synth_.voices) {
            voice.filter.m_fCutoff = v;
            voice.filter.UpdateCoefficients(voice.filter.m_fCutoff, voice.filter.m_fResonance);
        }
        break;
    case 18:
        for (SubSynthVoice &voice : synth_.voices) {
            voice.filter.m_fResonance = v;
            voice.filter.UpdateCoefficients(voice.filter.m_fCutoff, voice.filter.m_fResonance);
        }
        break;
    case 19: {
        const int type = toInt(v);
        const bool invert = type > 3;
        const int mode = invert ? type - 3 : type;
        for (SubSynthVoice &voice : synth_.voices) {
            voice.filter.m_nMode = mode;
            voice.filter.OnParamModified();
            voice.filter.m_bInvertEnvelope = invert;
        }
        break;
    }
    case 20:
        synth_.keyboardTracking = v;
        break;
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
    case 26:
    case 27:
    case 28:
        for (SubSynthVoice &voice : synth_.voices) {
            ADSR &envelope = id <= 24 ? voice.filterEnvelope : voice.amplitudeEnvelope;
            switch ((id - 21) % 4) {
            case 0:
                envelope.m_fAttack = v;
                break;
            case 1:
                envelope.m_fDecay = v;
                break;
            case 2:
                envelope.m_fSustain = v;
                break;
            default:
                envelope.m_fRelease = v;
                break;
            }
            envelope.OnParamModified();
        }
        break;
    case 29:
        // GetValue twice on the same knob.
        shared.voiceGain = v * v;
        break;
    case 31:
        shared.setModulationAmount(v);
        break;
    case kPitchBendId:
        updateBend();
        break;
    default:
        // Id 0, the preset browser, loads a preset file: not modelled.
        break;
    }
}

void SubSynthControls::updateOscillator2Pitch(bool fromCents) {
    float ratio = std::pow(2.0f, value(6) * 12.0f / 12.0f);
    ratio *= std::pow(2.0f, value(7) / 12.0f);
    ratio *= centsRatio(value(8));
    synth_.shared.oscillator2.setPitchRatio(ratio);

    // Only the cents knob refreshes oscillator 1, and only in cents mode 1.
    if (fromCents && centsMode_ == 1) {
        synth_.shared.oscillator1.setPitchRatio(centsRatio(-value(8)));
    }
}

void SubSynthControls::updateBend() {
    const float v = value(kPitchBendId);
    const float semitones = static_cast<float>(pitchBendSemitones);

    float offset;
    if (v == 0.5f) {
        synth_.shared.bend = 1.0f;
        return;
    }
    if (v > 0.5f) {
        offset = (v - 0.5f) * semitones;
    } else if (v < 0.5f) {
        offset = -((0.5f - v) * semitones);
    } else {
        return; // NaN: the engine leaves the bend alone
    }
    synth_.shared.bend = std::pow(2.0f, (offset + offset) / 12.0f);
}
