#pragma once

// WaveTables: band-limited table selection shared by Caustic's synths.
//
// Only FrequencyToWaveTable (0x15c398) is reconstructed: it is what
// Oscillator::GenerateSignalHQ uses. The class's own tables
// (s_pas*Tables, Init, Deinit) serve other machines and are not here.
// Ported to the original's structure. See dsp/Oscillator.md.

class WaveTables {
public:
    // The lower of the two bands an HQ oscillator blends at this frequency,
    // 0..6, and in *pfFraction how far towards the next band it is, 0..1.
    static int FrequencyToWaveTable(float fFrequency, float *pfFraction);
};
