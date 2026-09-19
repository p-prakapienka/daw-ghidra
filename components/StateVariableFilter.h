#pragma once

// Reconstructed Caustic state-variable filter.
//
// Recovered from FixedPointSVFilter::FixedPointSVFilter(bool) and
// FixedPointSVFilter::ProcessCV in libcaustic.so (ARMv7). This is the filter
// SubSynth instantiates per voice; MultiFilter is a separate effect machine.
// See components/StateVariableFilter.md for the evidence.
//
// Everything runs in Q24 fixed point, the same domain the ADSR uses.

using uint = unsigned int;

class StateVariableFilter {
public:
    // The value the engine keeps at offset 0x08. Anything outside 1..3 leaves
    // the buffer untouched.
    enum class Mode : int {
        Bypass = 0,
        LowPass = 1,
        HighPass = 2,
        BandPass = 3,
    };

    // Coefficient tables hold 256 entries each.
    static constexpr int kTableSize = 256;

    // One in the Q24 domain, and the largest representable value.
    static constexpr int kOne24 = 0x1000000;
    static constexpr int kMax24 = 0xFFFFFF;

    // Coefficients are refreshed once per block of this many samples.
    static constexpr uint kControlBlockSamples = 16;

    // Cutoff below this fraction collapses to the first table entry.
    static constexpr float kMinimumCutoff = 0.05f;

    // extendedResonance selects the damping table with the deeper range.
    // SubSynth passes true.
    explicit StateVariableFilter(bool extendedResonance);

    void setMode(Mode mode) { mode_ = mode; }
    Mode mode() const { return mode_; }

    // Normalised 0 .. 1 controls. Both index the tables built in the
    // constructor, so the mapping is the engine's, not a fresh one.
    void setCutoff(float cutoff);
    void setResonance(float resonance);

    // Applied to the input before it enters the integrators.
    void setInputGain(float gain);

    // Filter a block in place. Samples are Q24. numChannels of 2 means the
    // buffer is interleaved stereo and both channels are filtered with a
    // shared coefficient pair; any other value filters one channel.
    void process(int *buffer, uint numSamples, int numChannels);

    // Drop the integrator state without touching the coefficients.
    void reset();

    // Resolved coefficients, for tests and for plotting the response.
    int cutoffCoefficient() const { return cutoffCoefficient_; }
    int dampingCoefficient() const { return dampingCoefficient_; }
    int cutoffTableEntry(int index) const { return cutoffTable_[index]; }
    int dampingTableEntry(int index) const { return dampingTable_[index]; }

private:
    struct Channel {
        int low = 0;
        int band = 0;
    };

    // Q24 multiply: the engine's smull followed by a 24-bit shift of the
    // 64-bit product.
    static int multiply(int a, int b);

    int processSample(Channel &channel, int input, int feedback, int gain) const;

    // Built once in the constructor and never modified.
    int cutoffTable_[kTableSize] = {};
    int dampingTable_[kTableSize] = {};

    Mode mode_ = Mode::Bypass;

    int cutoffCoefficient_ = 0;
    int dampingCoefficient_ = 0;
    int inputGain_ = kMax24;

    Channel left_;
    Channel right_;
};
