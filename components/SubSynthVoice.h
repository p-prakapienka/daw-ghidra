#pragma once

// Reconstructed SubSynth voice.
//
// Recovered from SubSynth::ProcessChannel, SubSynth::ApplyVCA,
// Mixer::MixCrossfade and SubSynth::ConnectLFOs in libcaustic.so (ARMv7), with
// defaults from SubSynth::SubSynth and SubSynth::SetPolyphony and control
// scaling from SubSynth::OnChildModified. See components/SubSynthVoice.md.
//
// One voice renders one note: two oscillators, crossfaded, through the
// amplitude envelope, through the voice's filter, then scaled by a smoothed
// velocity level and the volume modulation and added to a mono output.

#include <cstdint>
#include <vector>

#include "ADSR.h"
#include "ControlVoltage.h"
#include "LFO.h"
#include "Oscillator.h"
#include "StateVariableFilter.h"

using uint = unsigned int;

// Where the machine's modulation sources point: SubSynth's oscillator source
// pointers, every voice filter's +0x80C, and the volume source at +0x3764.
// A null pointer stands for the engine's default source, an LFO switched off.
struct SubSynthRouting {
    // Vibrato, phase, octave, semitone and FM-depth sources per oscillator.
    // The modulator and amount fields are filled in by the voice.
    Oscillator::Modulation oscillator1;
    Oscillator::Modulation oscillator2;

    // Per-sample Q24 cutoff modulation, shared by every voice filter.
    const int *cutoff = nullptr;

    // Per-sample Q24 volume modulation: the output is scaled by 1 + value.
    const int *volume = nullptr;

    // Reproduce SubSynth::ConnectLFOs: reset every input to the default
    // source, then aim one input per LFO at its target, LFO 1 first, so LFO 2
    // wins a shared target. A target in 1 .. 9 also sets that LFO's phase
    // offset: half a cycle for the octave target, zero for the others.
    static SubSynthRouting connect(LFO &lfo1, LFO::Target target1, LFO &lfo2,
                                   LFO::Target target2);
};

class SubSynthVoice {
public:
    // SubSynth keeps sixteen voice slots, of which SetPolyphony enables at
    // most eight.
    static constexpr uint kVoiceSlots = 16;
    static constexpr uint kMaxPolyphony = 8;

    // The engine's scratch buffers hold this many samples, and so do the LFO
    // outputs, so it never renders a longer block. process() accepts longer
    // blocks as long as no routed LFO output is shorter.
    static constexpr uint kMaxBlockSamples = 128;

    // Oscillator 1's modulator amount is the "Osc1 Mod Amount" control times
    // this, 2.5 in Q24.
    static constexpr std::int32_t kModulationAmountScale = 0x027FFFFD;

    // The per-sample coefficient of the output level smoother. The
    // constructor writes the first; SetPolyphony, which ResetMachine calls,
    // overwrites it with the second, so a running machine uses 0.9.
    static constexpr std::int32_t kConstructorLevelSmoothing = 0x00FFBE76; // 0.999
    static constexpr std::int32_t kLevelSmoothing = 0x00E66665;            // 0.9

    // Floats become Q24 through this factor: the literal 0x4B7FFFFF.
    static constexpr float kQ24Scale = 16777215.0f;

    // Machine-level state every voice reads. SubSynth owns one of each.
    struct Shared {
        Oscillator oscillator1; // SubSynth +0x2F08
        Oscillator oscillator2; // SubSynth +0x2F40

        // +0xC2F7. Oscillator 2 is neither rendered nor mixed while this is
        // false, so oscillator 1's modulator input is silence.
        bool oscillator2Enabled = false;

        // +0x2F8C: the crossfade from oscillator 1 (0) to oscillator 2 (1),
        // Q24. The constructor sets 0.5.
        std::int32_t mixQ24 = static_cast<std::int32_t>(0.5f * kQ24Scale);

        // +0x2F90: "Osc1 Mod Amount" as Q24, before the 2.5 scale.
        std::int32_t modulationAmountQ24 = 0;

        // +0xC1EC: the pitch-bend ratio, 2^(semitones / 12).
        float bend = 1.0f;

        // +0xC1E8: scales every voice's velocity into its level target. The
        // constructor sets 1.0 and nothing found writes it afterwards.
        float voiceGain = 1.0f;

        SubSynthRouting routing;

        // The control handlers: SetOscillatorMix and the "Osc2 Waveform" and
        // "Osc1 Mod Amount" branches of OnChildModified.
        void setMix(float mix);
        void setModulationAmount(float amount);

        // 0 switches oscillator 2 off; any other value enables it with type
        // value - 1.
        void setOscillator2Waveform(int controlValue);
    };

    // The level smoother, SubSynth +0x2F94 + 12 * voice. Not reset between
    // notes, so a reused voice glides from its previous level.
    struct Level {
        std::int32_t current = 0;
        std::int32_t target = 0;
        std::int32_t smoothing = kConstructorLevelSmoothing;
    };

    // The per-voice state ProcessChannel reads. SubSynth keeps each in its
    // own array indexed by voice: the control voltage at +0xCFC, the
    // amplitude envelope at +0x3068, the filter at +0x3768 and the filter
    // envelope at +0xBBA8.
    ControlVoltage voltage = ControlVoltage::makeDefault();
    ADSR amplitudeEnvelope;
    ADSR filterEnvelope;
    StateVariableFilter filter{true};
    Level level;

    bool isActive() const { return voltage.isActive(); }

    // Render one block and add it to output, as ProcessChannel does. The
    // caller skips voices that are not active, as SubSynth::Process does.
    void process(Shared &shared, int *output, uint numSamples);

    // The level target ProcessChannel computes at the top of every block.
    static std::int32_t levelTarget(float voiceGain, float velocity);

    // Mixer::MixCrossfade: into[i] += (from[i] - into[i]) * mix, in Q24.
    static void crossfade(const int *from, int *into, std::int32_t mixQ24, uint numSamples);

    // The last block's intermediate buffers, for tests: oscillator 1 after
    // the crossfade, and oscillator 2.
    const std::vector<int> &mixBuffer() const { return oscillator1Buffer_; }
    const std::vector<int> &oscillator2Buffer() const { return oscillator2Buffer_; }

private:
    // SubSynth::ApplyVCA: the amplitude envelope, Q15 widened to Q24.
    void applyAmplitudeEnvelope(int *buffer, uint numSamples);

    // SubSynth +0x305C, +0x3060 and +0x3064. The engine shares one set across
    // voices; each voice owning its own gives the same result.
    std::vector<int> oscillator1Buffer_;
    std::vector<int> oscillator2Buffer_;
    std::vector<short> envelopeBuffer_;
};
