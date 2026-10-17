#include "dsp/Oscillator.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "dsp/WaveTables.h"

short *Oscillator::m_psSinewaveTable = nullptr;
short *Oscillator::m_psTrianglewaveTable = nullptr;
short *Oscillator::m_psSawtoothTable = nullptr;
short *Oscillator::m_psSquarewaveTable = nullptr;
short *Oscillator::m_psHQSawtoothTable = nullptr;
short *Oscillator::m_psHQSquarewaveTable = nullptr;
int *Oscillator::m_pasWhitenoiseTable = nullptr;
int Oscillator::m_nWaveTableRefCount = 0;

namespace {

// 1 / 16777215, the Q24 full scale as GenerateSignalLQ and GenerateSignalHQ
// load it: 0x33800001, one ulp above 2^-24 (0x8e1e8).
constexpr float kInverseQ24 = 1.0f / 16777215.0f;

// Q12 hertz to a phase increment: 4096 / 44100 (0x8ea2c).
constexpr float kPitchToIncrement = 4096.0f / 44100.0f;

// The frequency GenerateWavetables passes, the literal 10.7666f
// (0x412C43FE), close to but not exactly 44100 / 4096.
constexpr float kTableFrequency = 10.7666f;

// Highest harmonic kept in each HQ band, widest band first.
constexpr int kBandHarmonics[Oscillator::kBandCount] = {512, 337, 169, 84, 42, 21, 10, 6};

inline int Mul24(int a, int b) {
    return static_cast<int>((static_cast<long long>(a) * static_cast<long long>(b)) >> 24);
}

// The engine's 16-bit clamp: at or above 0x8000 gives 0x7FFF, below
// -0x7FFF gives -0x7FFF.
inline int Clamp16(int n) {
    if (n >= 0x8000) {
        return 0x7FFF;
    }
    if (n < -0x7FFF) {
        return -0x7FFF;
    }
    return n;
}

// Q15 to Q24 output with the engine's clamp.
inline int ToOutput(int n) {
    if (n >= 0x8000) {
        return 0xFFFE00;
    }
    if (n < -0x7FFF) {
        return -0xFFFE00;
    }
    return n << 9;
}

// One HQ table: eight band-limited cycles, each a literal sum of sines at
// amplitude 1/h up to the band's harmonic limit, interleaved so band b of
// step i is at table[i * 8 + b].
void BuildBandLimited(short *psTable, int nHarmonicStep, float fLevel) {
    static float afSine[Oscillator::kTableSize];
    static float afSum[Oscillator::kTableSize];
    for (unsigned int i = 0; i < Oscillator::kTableSize; i++) {
        afSine[i] = std::sin(2.0f * 3.14159265358979f * static_cast<float>(i)
                             / static_cast<float>(Oscillator::kTableSize));
    }
    for (unsigned int nBand = 0; nBand < Oscillator::kBandCount; nBand++) {
        std::memset(afSum, 0, sizeof afSum);
        for (int h = 1; h < kBandHarmonics[nBand]; h += nHarmonicStep) {
            const float fAmplitude = 1.0f / static_cast<float>(h);
            unsigned int nPosition = 0;
            for (unsigned int i = 0; i < Oscillator::kTableSize; i++) {
                afSum[i] += fAmplitude * afSine[nPosition % Oscillator::kTableSize];
                nPosition += static_cast<unsigned int>(h);
            }
        }
        float fPeak = 0.0f;
        for (const float f : afSum) {
            fPeak = f > fPeak ? f : fPeak;
        }
        if (fPeak <= 0.0f) {
            fPeak = 1.0f;
        }
        for (unsigned int i = 0; i < Oscillator::kTableSize; i++) {
            psTable[i * Oscillator::kBandCount + nBand] =
                static_cast<short>(-afSum[i] / fPeak * fLevel * 32767.0f);
        }
    }
}

} // namespace

// buf[i] = sin(i * frequency * 2pi / 44100) * level * 32767, in double.
void sinewave(short *pBuffer, unsigned int nLength, float fFrequency, float fLevel) {
    const double dAmplitude = static_cast<double>(fLevel * 32767.0f);
    for (unsigned int i = 0; i < nLength; i++) {
        const float fPhase = static_cast<float>(i) * fFrequency * 6.2831854820251465f / 44100.0f;
        pBuffer[i] = static_cast<short>(std::sin(static_cast<double>(fPhase)) * dAmplitude);
    }
}

// Walks up and down between the level bounds at a constant rate.
void trianglewave(short *pBuffer, unsigned int nLength, float fFrequency, float fLevel) {
    const float fAmplitude = fLevel * 32767.0f;
    const auto fHalfPeriod = static_cast<float>(static_cast<int>((44100.0f / fFrequency) * 0.5f));
    const float fStep = (fLevel / fHalfPeriod) * 2.0f;
    float fValue = 0.0f;
    bool bRising = true;
    for (unsigned int i = 0; i < nLength; i++) {
        pBuffer[i] = static_cast<short>((fValue / fLevel) * fAmplitude);
        fValue += bRising ? fStep : -fStep;
        if (fValue >= fLevel) {
            fValue = fLevel;
            bRising = false;
        } else if (fValue <= -fLevel) {
            fValue = -fLevel;
            bRising = true;
        }
    }
}

// A bare ramp from -1 to 1 over the buffer; the frequency is not used.
void sawtooth(short *pBuffer, unsigned int nLength, float /*fFrequency*/, float fLevel) {
    const float fAmplitude = fLevel * 32767.0f;
    for (unsigned int i = 0; i < nLength; i++) {
        const float fValue = 2.0f * static_cast<float>(i) / static_cast<float>(nLength) - 1.0f;
        pBuffer[i] = static_cast<short>(fValue * fAmplitude);
    }
}

// Alternates sign every half period, the rate over the frequency.
void squarewave(short *pBuffer, unsigned int nLength, float fFrequency, float fLevel) {
    const float fAmplitude = fLevel * 32767.0f;
    const int nHalfPeriod = static_cast<int>((44100.0f / fFrequency) * 0.5f);
    float fSign = -1.0f;
    int nCountdown = nHalfPeriod;
    for (unsigned int i = 0; i < nLength; i++) {
        pBuffer[i] = static_cast<short>(fSign * fAmplitude);
        if (--nCountdown <= 0) {
            nCountdown = nHalfPeriod;
            fSign = -fSign;
        }
    }
}

// A fixed-seed add/xor generator seeded with the MD5 initial constants, not
// lrand48. Output is Q24: (int)(a * level * 2^-31 * 16777215).
void ASwhitenoise(int *pBuffer, unsigned int nLength, float fLevel) {
    const float fScale = fLevel * 4.656612873077393e-10f;
    std::uint32_t a = 0xEFCDAB89u;
    std::uint32_t b = 0x67452301u;
    for (unsigned int i = 0; i < nLength; i++) {
        const auto fValue = static_cast<float>(static_cast<std::int32_t>(a));
        b ^= a;
        a += b;
        pBuffer[i] = static_cast<int>(fValue * fScale * 16777215.0f);
    }
}

void GenerateWavetables() {
    Oscillator::m_psSquarewaveTable = new short[Oscillator::kTableSize];
    Oscillator::m_psSinewaveTable = new short[Oscillator::kTableSize];
    Oscillator::m_psSawtoothTable = new short[Oscillator::kTableSize];
    Oscillator::m_psTrianglewaveTable = new short[Oscillator::kTableSize];
    Oscillator::m_pasWhitenoiseTable = new int[Oscillator::kNoiseTableSize];
    Oscillator::m_psHQSawtoothTable = new short[Oscillator::kTableSize * Oscillator::kBandCount];
    Oscillator::m_psHQSquarewaveTable = new short[Oscillator::kTableSize * Oscillator::kBandCount];

    squarewave(Oscillator::m_psSquarewaveTable, Oscillator::kTableSize, kTableFrequency, 0.2f);
    sinewave(Oscillator::m_psSinewaveTable, Oscillator::kTableSize, kTableFrequency, 0.3f);
    sawtooth(Oscillator::m_psSawtoothTable, Oscillator::kTableSize, kTableFrequency, 0.3f);
    trianglewave(Oscillator::m_psTrianglewaveTable, Oscillator::kTableSize, kTableFrequency, 0.4f);
    ASwhitenoise(Oscillator::m_pasWhitenoiseTable, Oscillator::kNoiseTableSize, 0.4f);

    // The sawtooth keeps every harmonic, the square the odd ones.
    BuildBandLimited(Oscillator::m_psHQSawtoothTable, 1, 0.3f);
    BuildBandLimited(Oscillator::m_psHQSquarewaveTable, 2, 0.2f);
}

void DestroyWavetables() {
    delete[] Oscillator::m_psSquarewaveTable;
    delete[] Oscillator::m_psSinewaveTable;
    delete[] Oscillator::m_psSawtoothTable;
    delete[] Oscillator::m_psTrianglewaveTable;
    delete[] Oscillator::m_pasWhitenoiseTable;
    delete[] Oscillator::m_psHQSawtoothTable;
    delete[] Oscillator::m_psHQSquarewaveTable;
    Oscillator::m_psSquarewaveTable = nullptr;
    Oscillator::m_psSinewaveTable = nullptr;
    Oscillator::m_psSawtoothTable = nullptr;
    Oscillator::m_psTrianglewaveTable = nullptr;
    Oscillator::m_pasWhitenoiseTable = nullptr;
    Oscillator::m_psHQSawtoothTable = nullptr;
    Oscillator::m_psHQSquarewaveTable = nullptr;
}

Oscillator::Oscillator() {
    if (m_nWaveTableRefCount == 0) {
        GenerateWavetables();
    }
    m_nWaveTableRefCount++;
    SetOscillatorType(kSine);
}

Oscillator::~Oscillator() {
    if (--m_nWaveTableRefCount == 0) {
        DestroyWavetables();
    }
}

// The type is stored whatever it is; only 0..8 (compared unsigned) choose a
// table. Noise keeps the table it had.
void Oscillator::SetOscillatorType(OscillatorType nType) {
    m_nType = nType;
    switch (static_cast<unsigned int>(nType)) {
    case kSine:
        m_bHQ = false;
        m_psTable = m_psSinewaveTable;
        break;
    case kTriangle:
        m_bHQ = false;
        m_psTable = m_psTrianglewaveTable;
        break;
    case kSawtooth:
        m_bHQ = false;
        m_psTable = m_psSawtoothTable;
        break;
    case kSawtoothHQ:
        m_bHQ = true;
        m_psTable = m_psHQSawtoothTable;
        break;
    case kSquare:
        m_bHQ = false;
        m_psTable = m_psSquarewaveTable;
        break;
    case kSquareHQ:
        m_bHQ = true;
        m_psTable = m_psHQSquarewaveTable;
        break;
    case kNoise:
        m_bHQ = false;
        break;
    case kCustom1:
        m_bHQ = false;
        m_psTable = m_psCustomTable1;
        break;
    case kCustom2:
        m_bHQ = false;
        m_psTable = m_psCustomTable2;
        break;
    default:
        break;
    }
}

void Oscillator::GenerateSignal(ControlVoltage *pCV, int nIndex, int *pOutput,
                                unsigned int nSamples, int *pFMInput, int nFMAmount, float fBend) {
    if (m_bHQ) {
        GenerateSignalHQ(pCV, nIndex, pOutput, nSamples, pFMInput, nFMAmount, fBend);
    } else {
        GenerateSignalLQ(pCV, nIndex, pOutput, nSamples, pFMInput, nFMAmount, fBend);
    }
}

namespace {

// Glide is resolved once per block, before anything else: linear in Q12
// hertz from the start to the target over m_nGlideSamples, measured from
// note-on. Reaching the end clears the glide.
inline float ResolvePitch(ControlVoltage *pCV) {
    if (pCV->m_nGlideSamples == 0) {
        return pCV->m_fTargetPitch;
    }
    float fProgress = static_cast<float>(pCV->m_nSamplesSinceNoteOn)
                      / static_cast<float>(pCV->m_nGlideSamples);
    if (fProgress >= 1.0f) {
        fProgress = 1.0f;
        pCV->m_nGlideSamples = 0;
    }
    pCV->m_fCurrentPitch = pCV->m_fGlideStart + fProgress * (pCV->m_fTargetPitch - pCV->m_fGlideStart);
    return pCV->m_fCurrentPitch;
}

// The per-sample parts every mode loop shares.
struct SampleContext {
    const Oscillator *pOsc;
    int *pFMInput;
    int nFMAmount;
    bool bSteps;
    float fStepRatio;

    // Octave and semitone inputs, floored to whole steps. Recomputed only
    // when either source is not the default (its waveform is not kOff).
    void UpdateSteps(unsigned int n) {
        if (bSteps) {
            const float fOctaves = std::floor(
                static_cast<float>(pOsc->m_pOctave->m_pnOutput[n]) * kInverseQ24 * 4.0f);
            const float fSemitones = std::floor(
                static_cast<float>(pOsc->m_pSemitones->m_pnOutput[n]) * kInverseQ24 * 12.0f);
            fStepRatio = std::pow(2.0f, (fSemitones + fOctaves * 12.0f) / 12.0f);
        }
    }

    float Vibrato(unsigned int n) const {
        return 1.0f + static_cast<float>(pOsc->m_pVibrato->m_pnOutput[n]) * kInverseQ24 * 0.1f;
    }

    // The modulator product, input * (amount * (1 + depth)), both Q24.
    int Drive(unsigned int n) const {
        return Mul24(pFMInput[n], Mul24(pOsc->m_pFMDepth->m_pnOutput[n] + 0xFFFFFF, nFMAmount));
    }

    unsigned int TableIndex(std::uint32_t nPhase, unsigned int n) const {
        return (nPhase >> 12)
               + static_cast<unsigned int>(pOsc->m_nPhaseOffset + (pOsc->m_pPhase->m_pnOutput[n] >> 12));
    }
};

inline int ReadLQ(const short *psTable, unsigned int nIndex, int nFraction) {
    const int nCurrent = psTable[nIndex & 0xFFF];
    const int nFollowing = psTable[(nIndex + 1) & 0xFFF];
    return ToOutput(nCurrent + ((Clamp16(nFollowing - nCurrent) * nFraction) >> 15));
}

// Band b and band b + 1 at the same phase, each clamped, blended by the
// band weight.
inline int ReadHQ(const short *psTable, unsigned int nIndex, int nFraction, unsigned int nBand,
                  int nWeight) {
    const unsigned int nPosition = nIndex & 0xFFF;
    const unsigned int nNext = (nIndex + 1) & 0xFFF;
    const auto Interpolate = [&](unsigned int b) {
        const int nCurrent = psTable[nPosition * Oscillator::kBandCount + b];
        const int nFollowing = psTable[nNext * Oscillator::kBandCount + b];
        return Clamp16(nCurrent + ((Clamp16(nFollowing - nCurrent) * nFraction) >> 15));
    };
    const int nLower = Interpolate(nBand);
    const int nUpper = Interpolate(nBand + 1);
    return ToOutput(nLower + ((nWeight * Clamp16(nUpper - nLower)) >> 15));
}

// The three mode loops, as the engine writes them out per mode. Read is the
// table read, LQ or HQ.
template <typename Read>
void RunModes(Oscillator *pOsc, ControlVoltage *pCV, int nIndex, int *pOutput,
              unsigned int nSamples, SampleContext &ctx, float fBase, Read read) {
    std::uint32_t &nPhase = pCV->m_anPhase[nIndex];
    float &fSweep = pCV->m_afPitchSweep[nIndex];
    const auto Advance = [&](float fFM, unsigned int n) {
        // Recomputed in float every sample and truncated.
        const float fIncrement = (1.0f + fSweep) * fBase * ctx.Vibrato(n) * fFM * ctx.fStepRatio;
        nPhase += static_cast<std::uint32_t>(static_cast<std::int32_t>(fIncrement));
        fSweep *= pCV->m_fPitchSweepDecay;
    };
    const auto Fraction = [&]() { return static_cast<int>((nPhase & 0xFFFu) << 3); };

    switch (pOsc->m_nModulationMode) {
    case Oscillator::kFrequencyModulation:
        for (unsigned int n = 0; n < nSamples; n++) {
            ctx.UpdateSteps(n);
            const int nDrive = ctx.Drive(n);
            pOutput[n] = read(ctx.TableIndex(nPhase, n), Fraction());
            Advance(1.0f + static_cast<float>(nDrive) * kInverseQ24, n);
        }
        break;
    case Oscillator::kPhaseModulation:
        // The product moves the table index only; the fraction still comes
        // from the unmodulated phase.
        for (unsigned int n = 0; n < nSamples; n++) {
            ctx.UpdateSteps(n);
            const int nDrive = ctx.Drive(n);
            pOutput[n] = read(ctx.TableIndex(nPhase + static_cast<std::uint32_t>(nDrive), n), Fraction());
            Advance(1.0f, n);
        }
        break;
    case Oscillator::kAmplitudeModulation:
        for (unsigned int n = 0; n < nSamples; n++) {
            ctx.UpdateSteps(n);
            const int nDrive = ctx.Drive(n);
            pOutput[n] = Mul24(read(ctx.TableIndex(nPhase, n), Fraction()), nDrive + 0xFFFFFF);
            Advance(1.0f, n);
        }
        break;
    default:
        break;
    }
    nPhase &= 0xFFFFFF;
}

} // namespace

void Oscillator::GenerateSignalLQ(ControlVoltage *pCV, int nIndex, int *pOutput,
                                  unsigned int nSamples, int *pFMInput, int nFMAmount, float fBend) {
    const float fPitch = ResolvePitch(pCV);

    // Noise ignores pitch and modulation: consecutive values straight from
    // the table, the position wrapped by (position + n) mod (22050 - n) so the
    // next block's copy always fits.
    if (m_nType == kNoise) {
        if (m_nNoisePosition + nSamples <= kNoiseTableSize) {
            std::memcpy(pOutput, m_pasWhitenoiseTable + m_nNoisePosition, nSamples * sizeof(int));
        } else {
            // A block longer than the one before it can run past the table;
            // the engine's memcpy would read beyond it. Wrap instead.
            for (unsigned int n = 0; n < nSamples; n++) {
                pOutput[n] = m_pasWhitenoiseTable[(m_nNoisePosition + n) % kNoiseTableSize];
            }
        }
        if (nSamples < kNoiseTableSize) {
            m_nNoisePosition = (nSamples + m_nNoisePosition) % (kNoiseTableSize - nSamples);
        }
        return;
    }

    const float fBase = fPitch * fBend * m_fPitchRatio * kPitchToIncrement;
    SampleContext ctx{this, pFMInput, nFMAmount,
                      m_pOctave->m_nWaveform != LFO::kOff || m_pSemitones->m_nWaveform != LFO::kOff,
                      1.0f};
    const short *psTable = m_psTable;
    RunModes(this, pCV, nIndex, pOutput, nSamples, ctx, fBase,
             [psTable](unsigned int nTableIndex, int nFraction) {
                 return ReadLQ(psTable, nTableIndex, nFraction);
             });
}

void Oscillator::GenerateSignalHQ(ControlVoltage *pCV, int nIndex, int *pOutput,
                                  unsigned int nSamples, int *pFMInput, int nFMAmount, float fBend) {
    const float fPitch = ResolvePitch(pCV);
    const float fBase = fPitch * fBend * m_fPitchRatio * kPitchToIncrement;

    // The band pair for this block, from the increment turned back into
    // hertz: base / (4096 / 44100) * (1 / 4096), in that order.
    float fFraction = 0.0f;
    const auto nBand = static_cast<unsigned int>(
        WaveTables::FrequencyToWaveTable(fBase / kPitchToIncrement * (1.0f / 4096.0f), &fFraction));
    const int nWeight = static_cast<std::int16_t>(static_cast<int>(fFraction * 16383.0f));

    SampleContext ctx{this, pFMInput, nFMAmount,
                      m_pOctave->m_nWaveform != LFO::kOff || m_pSemitones->m_nWaveform != LFO::kOff,
                      1.0f};
    const short *psTable = m_psTable;
    RunModes(this, pCV, nIndex, pOutput, nSamples, ctx, fBase,
             [psTable, nBand, nWeight](unsigned int nTableIndex, int nFraction) {
                 return ReadHQ(psTable, nTableIndex, nFraction, nBand, nWeight);
             });
}
