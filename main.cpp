#include <iostream>
#include <string>
#include <vector>

#include <AAFilter.h>

#include "dsp/ADSR.h"

namespace {

const char *curveName(int curve) {
    switch (curve) {
    case ADSR::kLogarithmic: return "logarithmic";
    case ADSR::kLinear: return "linear";
    case ADSR::kExponential: return "exponential";
    }
    return "unknown";
}

// Runs one note and prints the envelope as rows of characters.
void plotEnvelope(int curve) {
    ADSR envelope;
    envelope.m_fAttack = 0.05f;
    envelope.m_fDecay = 0.10f;
    envelope.m_fSustain = 0.5f;
    envelope.m_fRelease = 0.10f;
    envelope.m_nAttackCurve = curve;
    envelope.m_nDecayCurve = curve;
    envelope.m_nReleaseCurve = curve;
    envelope.OnParamModified();

    const unsigned int held = envelope.m_nAttackSamples + envelope.m_nDecaySamples + 2000;
    const unsigned int released = envelope.m_nReleaseSamples + 2000;

    std::vector<short> values(held + released);
    envelope.GenerateValues(0, 1, values.data(), held);
    envelope.GenerateValues(0, 0, values.data() + held, released);

    std::cout << curveName(curve) << "  attack=" << envelope.m_nAttackSamples
              << " decay=" << envelope.m_nDecaySamples
              << " release=" << envelope.m_nReleaseSamples
              << " sustain=" << envelope.m_sSustain << '\n';

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

    plotEnvelope(ADSR::kLinear);
    plotEnvelope(ADSR::kLogarithmic);
    plotEnvelope(ADSR::kExponential);

    return 0;
}
