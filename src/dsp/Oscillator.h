#pragma once

// Oscillator: SubSynth's wavetable oscillator. Standard quality reads one
// 4096-entry table; HQ crossfades between band-limited copies of a cycle.
//
// Recovered from Oscillator::Oscillator (0x8f974), ~Oscillator (0x8fabc),
// SetOscillatorType (0x8dd40), SetModulationMode (0x8de24), GenerateSignal
// (0x8f024), GenerateSignalLQ (0x8de2c) and GenerateSignalHQ (0x8e60c), and
// from GenerateWavetables (0x8f54c) and its generators, in libcaustic.so
// (ARMv7). See dsp/Oscillator.md.
//
// Ported to the original's structure. Oscillator has no virtual functions.
// The members are public because SubSynth writes them directly: the pitch
// ratio and phase offset from its controls, and the five modulation sources
// from ConnectLFOs. Their names are ours, in the engine's Hungarian style;
// the offsets and types are the engine's. The shared tables are static
// members with the engine's own names, reference-counted by the constructor
// and destructor.

#include <cstdint>

#include "dsp/ControlVoltage.h"
#include "dsp/LFO.h"

class Oscillator {
public:
    // Oscillator::OscillatorType, the value at +0x20. Numbered as the engine
    // stores it, which is also what presets hold.
    enum OscillatorType {
        kSine = 0,
        kTriangle = 1,
        kSawtooth = 2,
        kSawtoothHQ = 3,
        kSquare = 4,
        kSquareHQ = 5,
        kNoise = 6,
        kCustom1 = 7,
        kCustom2 = 8,
    };

    // Oscillator::ModulationMode, the value at +0x24: what the modulator
    // product, input * amount * (1 + depth), drives. Any other value makes
    // GenerateSignal store the phase back and return.
    enum ModulationMode {
        kFrequencyModulation = 0, // scales the phase increment by 1 + product
        kPhaseModulation = 1,     // offsets the table index by the product
        kAmplitudeModulation = 2, // scales the output sample by 1 + product
    };

    static constexpr unsigned int kTableSize = 4096;      // entries per cycle
    static constexpr unsigned int kBandCount = 8;         // bands in an HQ table
    static constexpr unsigned int kNoiseTableSize = 22050; // Q24 ints, half a second

    Oscillator();
    ~Oscillator();

    void SetOscillatorType(OscillatorType nType);
    void SetModulationMode(ModulationMode nMode) { m_nModulationMode = nMode; }

    // Render nSamples for the voice in pCV, with the phase in slot nIndex.
    // pFMInput is the modulator (another oscillator's output) and nFMAmount
    // its amount, Q24; fBend is the machine's pitch-bend ratio. Dispatches on
    // m_bHQ.
    void GenerateSignal(ControlVoltage *pCV, int nIndex, int *pOutput, unsigned int nSamples,
                        int *pFMInput, int nFMAmount, float fBend);
    void GenerateSignalLQ(ControlVoltage *pCV, int nIndex, int *pOutput, unsigned int nSamples,
                          int *pFMInput, int nFMAmount, float fBend);
    void GenerateSignalHQ(ControlVoltage *pCV, int nIndex, int *pOutput, unsigned int nSamples,
                          int *pFMInput, int nFMAmount, float fBend);

    LFO *m_pVibrato = nullptr;          // +0x00: pitch, +/-10%
    float m_fPitchRatio = 1.0f;         // +0x04: frequency multiplier
    LFO *m_pPhase = nullptr;            // +0x08: phase offset in table units << 12
    int m_nPhaseOffset = 0;             // +0x0C: fixed phase offset in table entries
    LFO *m_pOctave = nullptr;           // +0x10: whole octaves, +/-4
    LFO *m_pSemitones = nullptr;        // +0x14: whole semitones, +/-12
    LFO *m_pFMDepth = nullptr;          // +0x18: scales the FM amount by 1 + value
    unsigned int m_nNoisePosition = 0;  // +0x1C
    OscillatorType m_nType = kSine;     // +0x20
    ModulationMode m_nModulationMode = kFrequencyModulation; // +0x24: not set by the constructor
    const short *m_psTable = nullptr;   // +0x28: the table playing
    const short *m_psCustomTable1 = nullptr; // +0x2C: type 7's table, set by SubSynth
    const short *m_psCustomTable2 = nullptr; // +0x30: type 8's table
    bool m_bHQ = false;                 // +0x34

    // The shared tables, built by GenerateWavetables for the first
    // oscillator and freed by DestroyWavetables after the last.
    static short *m_psSinewaveTable;
    static short *m_psTrianglewaveTable;
    static short *m_psSawtoothTable;
    static short *m_psSquarewaveTable;
    static short *m_psHQSawtoothTable;   // 8 bands, interleaved
    static short *m_psHQSquarewaveTable; // 8 bands, interleaved
    static int *m_pasWhitenoiseTable;
    static int m_nWaveTableRefCount;

    Oscillator(const Oscillator &) = delete;
    Oscillator &operator=(const Oscillator &) = delete;
};

#if UINTPTR_MAX == 0xFFFFFFFF
static_assert(sizeof(Oscillator) == 0x38, "Oscillator is 0x38 bytes");
#endif

// The table generators GenerateWavetables calls, with the engine's
// signatures. frequency puts one cycle in the table; level is the peak.
void GenerateWavetables();
void DestroyWavetables();
void sinewave(short *pBuffer, unsigned int nLength, float fFrequency, float fLevel);
void trianglewave(short *pBuffer, unsigned int nLength, float fFrequency, float fLevel);
void sawtooth(short *pBuffer, unsigned int nLength, float fFrequency, float fLevel);
void squarewave(short *pBuffer, unsigned int nLength, float fFrequency, float fLevel);
void ASwhitenoise(int *pBuffer, unsigned int nLength, float fLevel);
