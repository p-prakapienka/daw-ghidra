#include "machines/subsynth/SubSynthVoice.h"

#include <algorithm>

namespace {

// The engine's smull followed by a 24-bit shift of the 64-bit product.
int multiply24(int a, int b) {
    return static_cast<int>((static_cast<std::int64_t>(a) * b) >> 24);
}

// Unity in Q24 as the engine adds it to a modulation value: 0x1000000 - 1.
constexpr int kOne24 = 0x00FFFFFF;

// Aim one LFO at its target, the body of one ConnectLFOs switch.
void aim(SubSynthRouting &routing, LFO &lfo, LFO::Target target) {
    switch (target) {
    case LFO::Target::Oscillator1Pitch: routing.oscillator1Vibrato = &lfo; break;
    case LFO::Target::Oscillator2Pitch: routing.oscillator2Vibrato = &lfo; break;
    case LFO::Target::BothPitch:
        routing.oscillator1Vibrato = &lfo;
        routing.oscillator2Vibrato = &lfo;
        break;
    case LFO::Target::Oscillator2Phase: routing.oscillator2Phase = &lfo; break;
    case LFO::Target::Cutoff: routing.cutoff = &lfo; break;
    case LFO::Target::Volume: routing.volume = &lfo; break;
    case LFO::Target::Oscillator2Octave: routing.oscillator2Octave = &lfo; break;
    case LFO::Target::Oscillator2Semitones: routing.oscillator2Semitones = &lfo; break;
    case LFO::Target::Oscillator1Modulation: routing.oscillator1FMDepth = &lfo; break;
    default:
        // Target 0 and anything above 9 leave the inputs and the LFO's phase
        // offset alone.
        return;
    }
    lfo.m_nPhaseOffset = target == LFO::Target::Oscillator2Octave ? LFO::kHalfCycle : 0;
}

} // namespace

SubSynthRouting SubSynthRouting::connect(LFO &lfo1, LFO::Target target1, LFO &lfo2,
                                         LFO::Target target2) {
    SubSynthRouting routing;
    aim(routing, lfo1, target1);
    aim(routing, lfo2, target2);
    return routing;
}

SubSynthVoice::Shared::Shared() {
    defaultSource.m_nWaveform = LFO::kOff;
    defaultSource.OnParamModified();
    defaultSource.m_nDepth = 0;
    defaultSource.OnParamModified();
    defaultSource.GenerateInternal(LFO::kBlockSize);
    connectOscillators();
}

void SubSynthVoice::Shared::connectOscillators() {
    LFO *const def = &defaultSource;
    const auto pick = [def](LFO *source) { return source != nullptr ? source : def; };
    oscillator1.m_pVibrato = pick(routing.oscillator1Vibrato);
    oscillator1.m_pPhase = def;
    oscillator1.m_pOctave = def;
    oscillator1.m_pSemitones = def;
    oscillator1.m_pFMDepth = pick(routing.oscillator1FMDepth);
    oscillator2.m_pVibrato = pick(routing.oscillator2Vibrato);
    oscillator2.m_pPhase = pick(routing.oscillator2Phase);
    oscillator2.m_pOctave = pick(routing.oscillator2Octave);
    oscillator2.m_pSemitones = pick(routing.oscillator2Semitones);
    oscillator2.m_pFMDepth = def;
}

void SubSynthVoice::Shared::setMix(float mix) {
    mixQ24 = static_cast<std::int32_t>(mix * kQ24Scale);
}

void SubSynthVoice::Shared::setModulationAmount(float amount) {
    modulationAmountQ24 = static_cast<std::int32_t>(amount * kQ24Scale);
}

void SubSynthVoice::Shared::setOscillator2Waveform(int controlValue) {
    oscillator2Enabled = controlValue != 0;
    // The engine calls SetOscillatorType(value - 1) even for 0: -1 falls
    // outside its switch, so only the stored type changes.
    oscillator2.SetOscillatorType(static_cast<Oscillator::OscillatorType>(controlValue - 1));
}

std::int32_t SubSynthVoice::levelTarget(float voiceGain, float velocity) {
    return static_cast<std::int32_t>(voiceGain * velocity * kQ24Scale);
}

void SubSynthVoice::crossfade(const int *from, int *into, std::int32_t mixQ24, uint numSamples) {
    for (uint index = 0; index < numSamples; ++index) {
        into[index] += multiply24(from[index] - into[index], mixQ24);
    }
}

void SubSynthVoice::applyAmplitudeEnvelope(int *buffer, uint numSamples) {
    const bool gate = (voltage.m_nFlags & ControlVoltage::kGateBit) != 0;
    const auto position =
        static_cast<uint>(gate ? voltage.m_nEnvelopePosition : voltage.m_nReleasePosition);
    amplitudeEnvelope.GenerateValues(position, gate ? 1u : 0u, envelopeBuffer_.data(), numSamples);
    for (uint index = 0; index < numSamples; ++index) {
        buffer[index] = multiply24(buffer[index], envelopeBuffer_[index] << 9);
    }
}

void SubSynthVoice::process(Shared &shared, int *output, uint numSamples) {
    if (oscillator1Buffer_.size() < numSamples) {
        oscillator1Buffer_.resize(numSamples);
        oscillator2Buffer_.resize(numSamples);
        envelopeBuffer_.resize(numSamples);
    }
    int *mix = oscillator1Buffer_.data();
    int *second = oscillator2Buffer_.data();

    // The target is recomputed every block; the smoother below chases it.
    level.target = levelTarget(shared.voiceGain, voltage.m_fVelocity);

    // Oscillator 2 first, into its own buffer. Its modulator is its own
    // output buffer at amount 0, which is how ProcessChannel calls it.
    if (shared.oscillator2Enabled) {
        shared.oscillator2.GenerateSignal(&voltage, 1, second, numSamples, second, 0, shared.bend);
    } else {
        std::fill(second, second + numSamples, 0);
    }

    // Oscillator 1 takes oscillator 2 as its modulator. The modulation mode
    // set on oscillator 1 decides whether that is FM, PM or AM.
    shared.oscillator1.GenerateSignal(&voltage, 0, mix, numSamples, second,
                                      multiply24(shared.modulationAmountQ24, kModulationAmountScale),
                                      shared.bend);

    if (shared.oscillator2Enabled) {
        crossfade(second, mix, shared.mixQ24, numSamples);
    }

    applyAmplitudeEnvelope(mix, numSamples);

    filter.m_pModulation =
        shared.routing.cutoff != nullptr ? shared.routing.cutoff : &shared.defaultSource;
    filter.ProcessCV(&voltage, mix, numSamples, 1);

    // Level smoothing, volume modulation and accumulation, per sample. The
    // default volume source contributes zero, so the factor is 0xFFFFFF, not
    // exactly one.
    const int *volume = (shared.routing.volume != nullptr ? shared.routing.volume
                                                          : &shared.defaultSource)->m_pnOutput;
    for (uint index = 0; index < numSamples; ++index) {
        level.current = level.target + multiply24(level.current - level.target, level.smoothing);
        const int lift = volume[index] + kOne24;
        output[index] += multiply24(mix[index], multiply24(level.current, lift));
    }

    // Both note counters move every block, the release counter only while
    // the gate is closed (0x9cae4-0x9cb18).
    voltage.m_nSamplesSinceNoteOn += static_cast<std::int32_t>(numSamples);
    voltage.m_nEnvelopePosition += static_cast<std::int32_t>(numSamples);
    if ((voltage.m_nFlags & ControlVoltage::kGateBit) == 0) {
        voltage.m_nReleasePosition += static_cast<std::int32_t>(numSamples);
    }

    // The amplitude envelope decides when the voice ends. Ending it also
    // drops the filter's integrator state.
    if (amplitudeEnvelope.IsDone(static_cast<uint>(voltage.m_nReleasePosition))) {
        voltage.m_nFlags = static_cast<std::uint8_t>(voltage.m_nFlags & ~ControlVoltage::kActiveBit);
        filter.m_anBand[0] = 0;
        filter.m_anBand[1] = 0;
        filter.m_anLow[0] = 0;
        filter.m_anLow[1] = 0;
    }
}
