// The reconstruction's side of the differential checks. Reads one scenario
// per line on stdin and prints the results the Python side compares against
// the original library. Floats travel as 32-bit hex words so both sides see
// the same bits.
//
//   ctor <flag>
//   process <flag> <mode> <cutoff> <res> <invert> <attack> <decay> <sustain>
//           <release> <hertz> <tracking> <advance> <gate> <samples> <channels>
//           <modulation>
//
// Build from the repository root (one line):
//   g++ -std=c++20 -O2 -ffp-contract=off -I. components/*.cpp
//       tools/differential/driver.cpp -o build/differential-driver

#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "components/ADSR.h"
#include "components/ControlVoltage.h"
#include "components/StateVariableFilter.h"

namespace {

float bitsToFloat(std::uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

std::uint32_t readWord(std::istream &in) {
    std::string token;
    in >> token;
    return static_cast<std::uint32_t>(std::stoul(token, nullptr, 16));
}

void printInts(const char *label, const int *values, std::size_t count) {
    std::cout << label;
    for (std::size_t index = 0; index < count; ++index) {
        std::cout << ' ' << values[index];
    }
    std::cout << '\n';
}

// The test signal both sides feed the filter, one value per buffer slot.
int inputSample(std::size_t index) {
    return (static_cast<int>((index * 37) % 512) - 256) << 15;
}

// The per-sample modulation both sides use: none, or a rising ramp.
int modulationSample(int kind, std::size_t index) {
    return kind == 0 ? 0 : static_cast<int>(index) * 65536 - 0x400000;
}

void construct(std::istream &in) {
    const bool flag = readWord(in) != 0;
    const StateVariableFilter filter(flag);
    std::vector<int> cutoff(StateVariableFilter::kTableSize);
    std::vector<int> damping(StateVariableFilter::kTableSize);
    for (int index = 0; index < StateVariableFilter::kTableSize; ++index) {
        cutoff[index] = filter.getCutoffTableEntry(index);
        damping[index] = filter.getDampingTableEntry(index);
    }
    printInts("cutoff", cutoff.data(), cutoff.size());
    printInts("damping", damping.data(), damping.size());
    const int state[] = {filter.getInputGain(), filter.getCutoffCoefficient(),
                         filter.getDampingCoefficient()};
    printInts("state", state, 3);
}

void process(std::istream &in) {
    const bool flag = readWord(in) != 0;
    const auto mode = static_cast<StateVariableFilter::Mode>(readWord(in));
    const float cutoff = bitsToFloat(readWord(in));
    const float resonance = bitsToFloat(readWord(in));
    const bool invert = readWord(in) != 0;
    const float attack = bitsToFloat(readWord(in));
    const float decay = bitsToFloat(readWord(in));
    const float sustain = bitsToFloat(readWord(in));
    const float release = bitsToFloat(readWord(in));
    const float hertz = bitsToFloat(readWord(in));
    const float tracking = bitsToFloat(readWord(in));
    const std::uint32_t advance = readWord(in);
    const bool gate = readWord(in) != 0;
    const std::uint32_t samples = readWord(in);
    const int channels = static_cast<int>(readWord(in));
    const int modulationKind = static_cast<int>(readWord(in));

    ADSR envelope;
    envelope.setAttackSeconds(attack);
    envelope.setDecaySeconds(decay);
    envelope.setSustainLevel(sustain);
    envelope.setReleaseSeconds(release);
    envelope.OnParamModified();

    ControlVoltage voltage = ControlVoltage::makeDefault();
    voltage.noteOn(hertz, 60, tracking, 1.0f);
    voltage.advance(advance);
    if (!gate) {
        voltage.noteOff();
    }

    std::vector<int> modulation(samples);
    for (std::size_t index = 0; index < modulation.size(); ++index) {
        modulation[index] = modulationSample(modulationKind, index);
    }

    StateVariableFilter filter(flag);
    filter.setMode(mode);
    filter.setCutoffBase(cutoff);
    filter.setResonanceBase(resonance);
    filter.setInvertEnvelope(invert);

    const std::size_t slots = static_cast<std::size_t>(samples) * (channels == 2 ? 2 : 1);
    std::vector<int> buffer(slots);
    for (std::size_t index = 0; index < slots; ++index) {
        buffer[index] = inputSample(index);
    }

    // The engine's 32-bit layout, not the host struct: on a 64-bit host the
    // pointer at 0x1C is eight bytes and shifts everything after it.
    controlVoltageLayout::Engine engine{};
    engine.flags = voltage.flags;
    engine.samplesSinceNoteOn = voltage.samplesSinceNoteOn;
    engine.envelopePosition = voltage.envelopePosition;
    engine.releasePosition = voltage.releasePosition;
    engine.phase[0] = voltage.phase[0];
    engine.phase[1] = voltage.phase[1];
    engine.field18 = voltage.field18;
    engine.frequencyQ12 = voltage.frequencyQ12;
    engine.glideStartPitch = voltage.glideStartPitch;
    engine.targetPitch = voltage.targetPitch;
    engine.currentPitch = voltage.currentPitch;
    engine.glideSamples = voltage.glideSamples;
    engine.field34 = voltage.field34;
    engine.field38 = voltage.field38;
    engine.noteId = voltage.noteId;
    engine.field40 = voltage.field40;
    engine.field44 = voltage.field44;
    engine.pitchSweep[0] = voltage.pitchSweep[0];
    engine.pitchSweep[1] = voltage.pitchSweep[1];
    engine.pitchSweepDecay = voltage.pitchSweepDecay;
    engine.frequencyHertz = voltage.frequencyHertz;
    engine.filterCutoffScale = voltage.filterCutoffScale;
    engine.velocity = voltage.velocity;

    std::cout << "cv";
    const auto *bytes = reinterpret_cast<const unsigned char *>(&engine);
    for (std::size_t index = 0; index < sizeof engine; ++index) {
        std::cout << ' ' << static_cast<int>(bytes[index]);
    }
    std::cout << '\n';

    filter.processVoice({&voltage, &envelope, modulationKind == 0 ? nullptr : modulation.data()},
                        buffer.data(), samples, channels);

    printInts("out", buffer.data(), buffer.size());
    const int state[] = {filter.getInputGain(), filter.getCutoffCoefficient(),
                         filter.getDampingCoefficient()};
    printInts("state", state, 3);
}

} // namespace

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string command;
        in >> command;
        if (command == "ctor") {
            construct(in);
        } else if (command == "process") {
            process(in);
        }
        std::cout << "end" << std::endl;
    }
    return 0;
}
