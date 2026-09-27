#include "SubSynthVoice.h"

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
    const int *source = lfo.output();
    switch (target) {
    case LFO::Target::Oscillator1Pitch: routing.oscillator1.vibrato = source; break;
    case LFO::Target::Oscillator2Pitch: routing.oscillator2.vibrato = source; break;
    case LFO::Target::BothPitch:
        routing.oscillator1.vibrato = source;
        routing.oscillator2.vibrato = source;
        break;
    case LFO::Target::Oscillator2Phase: routing.oscillator2.phase = source; break;
    case LFO::Target::Cutoff: routing.cutoff = source; break;
    case LFO::Target::Volume: routing.volume = source; break;
    case LFO::Target::Oscillator2Octave: routing.oscillator2.octave = source; break;
    case LFO::Target::Oscillator2Semitones: routing.oscillator2.semitones = source; break;
    case LFO::Target::Oscillator1Modulation: routing.oscillator1.fmDepth = source; break;
    default:
        // Target 0 and anything above 9 leave the inputs and the LFO's phase
        // offset alone.
        return;
    }
    lfo.setPhaseOffset(target == LFO::Target::Oscillator2Octave ? LFO::kHalfCycle : 0);
}

} // namespace

SubSynthRouting SubSynthRouting::connect(LFO &lfo1, LFO::Target target1, LFO &lfo2,
                                         LFO::Target target2) {
    SubSynthRouting routing;
    aim(routing, lfo1, target1);
    aim(routing, lfo2, target2);
    return routing;
}

void SubSynthVoice::Shared::setMix(float mix) {
    mixQ24 = static_cast<std::int32_t>(mix * kQ24Scale);
}

void SubSynthVoice::Shared::setModulationAmount(float amount) {
    modulationAmountQ24 = static_cast<std::int32_t>(amount * kQ24Scale);
}

void SubSynthVoice::Shared::setOscillator2Waveform(int controlValue) {
    oscillator2Enabled = controlValue != 0;
    // The engine calls SetOscillatorType(value - 1) even for 0; -1 falls
    // outside its switch and only the stored type changes. Oscillator 2 is
    // not rendered while disabled, so skipping the call is equivalent.
    if (oscillator2Enabled) {
        oscillator2.setType(static_cast<Oscillator::Type>(controlValue - 1));
    }
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
    const uint gate = voltage.isGateOpen() ? 1u : 0u;
    const auto position = static_cast<uint>(voltage.envelopePositionForGate());
    amplitudeEnvelope.GenerateValues(position, gate, envelopeBuffer_.data(), numSamples);
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
    level.target = levelTarget(shared.voiceGain, voltage.velocity);

    // Oscillator 2 first, into its own buffer, with no modulator of its own.
    if (shared.oscillator2Enabled) {
        Oscillator::Modulation modulation = shared.routing.oscillator2;
        modulation.fmInput = nullptr;
        modulation.fmAmount = 0;
        shared.oscillator2.generate(voltage, 1, second, numSamples, modulation, shared.bend);
    } else {
        std::fill(second, second + numSamples, 0);
    }

    // Oscillator 1 takes oscillator 2 as its modulator. The modulation mode
    // set on oscillator 1 decides whether that is FM, PM or AM.
    Oscillator::Modulation modulation = shared.routing.oscillator1;
    modulation.fmInput = second;
    modulation.fmAmount = multiply24(shared.modulationAmountQ24, kModulationAmountScale);
    shared.oscillator1.generate(voltage, 0, mix, numSamples, modulation, shared.bend);

    if (shared.oscillator2Enabled) {
        crossfade(second, mix, shared.mixQ24, numSamples);
    }

    applyAmplitudeEnvelope(mix, numSamples);

    StateVariableFilter::VoiceInputs inputs;
    inputs.voltage = &voltage;
    inputs.envelope = &filterEnvelope;
    inputs.modulation = shared.routing.cutoff;
    filter.processVoice(inputs, mix, numSamples, 1);

    // Level smoothing, volume modulation and accumulation, per sample. The
    // default volume source contributes zero, so the factor is 0xFFFFFF, not
    // exactly one.
    const int *volume = shared.routing.volume;
    for (uint index = 0; index < numSamples; ++index) {
        level.current = level.target + multiply24(level.current - level.target, level.smoothing);
        const int lift = (volume != nullptr ? volume[index] : 0) + kOne24;
        output[index] += multiply24(mix[index], multiply24(level.current, lift));
    }

    voltage.advance(numSamples);

    // The amplitude envelope decides when the voice ends. Ending it also
    // drops the filter's integrator state.
    if (amplitudeEnvelope.IsDone(static_cast<uint>(voltage.releasePosition))) {
        voltage.flags = static_cast<std::uint8_t>(voltage.flags & ~ControlVoltage::kActiveBit);
        filter.reset();
    }
}
