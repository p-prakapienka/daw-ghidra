#pragma once

// The SubSynth block of a Caustic song or preset file.
//
// Recovered from SubSynth::Serialize, UIControlCollection::Serialize and
// Archive::Serialize in libcaustic.so (ARMv7). The same function reads and
// writes, switching on the archive's mode, so the layout below is the one it
// writes and every older layout it still reads. See
// src/machines/subsynth/SubSynthArchive.md.
//
// Control values are kept as the raw id/value pairs of the control
// collection. SubSynthControls::load turns them into engine state.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

class SubSynthArchive {
public:
    static constexpr std::array<char, 4> kTag{'S', 'S', 'Y', 'N'};
    static constexpr std::array<char, 4> kControlCollectionTag{'C', 'C', 'O', 'L'};
    static constexpr std::array<char, 4> kControlTag{'C', 'T', 'R', 'L'};

    // What SubSynth::Serialize writes: 3.1.0.0, stored little-endian, so
    // readers that split it see 0x0000, 0x01, 0x03.
    static constexpr std::uint32_t kCurrentVersion = 0x03010000;

    // Versions at which fields appeared, from the comparisons in Serialize.
    static constexpr std::uint32_t kCustomWaveformVersion = 0x00010100;   // > 0x000100FF
    static constexpr std::uint32_t kShortWaveformVersion = 0x00020000;    // shorts, polyphony
    static constexpr std::uint32_t kQuarticTimesVersion = 0x00020101;     // > 0x00020100
    static constexpr std::uint32_t kThirdGenerationVersion = 0x03000000;  // waveform 2, modes
    static constexpr std::uint32_t kPresetPathVersion = 0x03010000;       // > 0x0300FFFF

    // A song's own version gates the pattern block, with its top bit masked.
    static constexpr std::uint32_t kSongPatternVersion = 0x00020000;

    static constexpr std::size_t kPresetNameLength = 32;
    static constexpr std::size_t kCustomWaveformPoints = 660;

    // Loading a song versus loading a preset. SubSynth sets +0xC2F6 while a
    // preset is read or written; then there is no preset path and no
    // patterns.
    enum class Context { Song, Preset };

    struct Control {
        std::uint32_t id = 0;
        float value = 0.0f;
    };

    using Waveform = std::array<std::int16_t, kCustomWaveformPoints>;

    std::uint32_t version = kCurrentVersion;
    std::vector<Control> controls;
    std::string presetName;

    // Only in songs, and only from 3.1.
    std::string presetPath;

    // From 3.1, in songs and presets.
    std::optional<std::int32_t> keyboardOctave;

    // The two custom waveform editors' raw points. Absent before 1.1;
    // waveform 2 is absent before 3.0, where the engine zeroes it.
    std::optional<Waveform> customWaveform1;
    std::optional<Waveform> customWaveform2;

    // Stored from 2.0. Older blocks imply 8, which the engine then sets.
    std::int32_t polyphony = 8;

    // Oscillator 1 modulation mode and oscillator 2 cents mode, from 3.0.
    // Older blocks imply 0.
    std::int32_t modulationMode = 0;
    std::int32_t centsMode = 0;

    // The pattern editor's own block, kept opaque: whatever follows inside
    // the SubSynth block's length.
    std::vector<std::uint8_t> patterns;

    // Parse a block starting at its tag. songVersion is the enclosing
    // song's version; it only matters in the Song context. Returns nullopt,
    // and sets error when given, if the block is not a SubSynth block or is
    // truncated. consumed receives the block's total size.
    static std::optional<SubSynthArchive> read(std::span<const std::uint8_t> bytes, Context context,
                                               std::uint32_t songVersion = kSongPatternVersion,
                                               std::string *error = nullptr,
                                               std::size_t *consumed = nullptr);

    // Write the block in the current layout, whatever version it was read
    // from.
    std::vector<std::uint8_t> write(Context context) const;

    // Before 2.1.1 three ADSR times on each envelope were stored linearly;
    // the engine takes the fourth root on load. Before 3.0 the waveform
    // selectors had no HQ entries and are renumbered. The archive only
    // reports them; SubSynthControls::load applies them by control id.
    bool needsQuarticTimeFixup() const { return version < kQuarticTimesVersion; }
    bool needsWaveformRenumbering() const { return version < kThirdGenerationVersion; }
};
