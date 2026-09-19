#include <iostream>
#include <string>
#include <vector>

#include <AAFilter.h>

#include "components/ADSR.h"

namespace {

const char *curveName(ADSR::Curve curve) {
    switch (curve) {
    case ADSR::Curve::Logarithmic: return "logarithmic";
    case ADSR::Curve::Linear: return "linear";
    case ADSR::Curve::Exponential: return "exponential";
    }
    return "unknown";
}

// Runs one note and prints the envelope as rows of characters.
void plotEnvelope(ADSR::Curve curve) {
    ADSR envelope;
    envelope.setAttackSeconds(0.05f);
    envelope.setDecaySeconds(0.10f);
    envelope.setSustainLevel(0.5f);
    envelope.setReleaseSeconds(0.10f);
    envelope.setAttackCurve(curve);
    envelope.setDecayCurve(curve);
    envelope.setReleaseCurve(curve);
    envelope.OnParamModified();

    const unsigned int held = envelope.attackSamples() + envelope.decaySamples() + 2000;
    const unsigned int released = envelope.releaseSamples() + 2000;

    std::vector<short> values(held + released);
    envelope.GenerateValues(0, 1, values.data(), held);
    envelope.GenerateValues(0, 0, values.data() + held, released);

    std::cout << curveName(curve) << "  attack=" << envelope.attackSamples()
              << " decay=" << envelope.decaySamples()
              << " release=" << envelope.releaseSamples()
              << " sustain=" << envelope.sustainQ15() << '\n';

    const auto step = static_cast<unsigned int>(values.size() / 64);
    for (unsigned int index = 0; index < values.size(); index += step) {
        std::cout << std::string(static_cast<std::size_t>(values[index]) / 512, '#') << '\n';
    }
    std::cout << '\n';
}

} // namespace

int main() {
    std::cout << "Rev-caustic component bench\n\n";

    auto filter = soundtouch::AAFilter(8);
    filter.setCutoffFreq(0.5);

    plotEnvelope(ADSR::Curve::Linear);
    plotEnvelope(ADSR::Curve::Logarithmic);
    plotEnvelope(ADSR::Curve::Exponential);

    return 0;
}
