#include "dsp/WaveTables.h"

// The bands start at roughly C1 and double with every octave (.rodata
// 0x15c48c); the fraction is how far the frequency has climbed from its
// band's start towards the next start, clamped to 0..1.
int WaveTables::FrequencyToWaveTable(float fFrequency, float *pfFraction) {
    int nTable;
    float fFraction;
    if (fFrequency >= 2093.0f) {
        nTable = 6;
        fFraction = (fFrequency - 2093.0f) / 2093.0f;
    } else if (fFrequency >= 1046.0f) {
        nTable = 5;
        fFraction = (fFrequency - 1046.0f) / 1046.0f;
    } else if (fFrequency >= 523.0f) {
        nTable = 4;
        fFraction = (fFrequency - 523.0f) / 523.0f;
    } else if (fFrequency >= 261.0f) {
        nTable = 3;
        fFraction = (fFrequency - 261.0f) / 261.0f;
    } else if (fFrequency >= 130.0f) {
        nTable = 2;
        fFraction = (fFrequency - 130.0f) / 130.0f;
    } else if (fFrequency >= 65.0f) {
        nTable = 1;
        fFraction = (fFrequency - 65.0f) / 65.0f;
    } else {
        nTable = 0;
        fFraction = (fFrequency - 33.0f) / 33.0f;
    }
    if (fFraction < 0.0f) {
        fFraction = 0.0f;
    } else if (fFraction > 1.0f) {
        fFraction = 1.0f;
    }
    *pfFraction = fFraction;
    return nTable;
}
