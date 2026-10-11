#include "components/SubSynthArchive.h"

#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;

void putTag(Bytes &bytes, const char *tag) { bytes.insert(bytes.end(), tag, tag + 4); }

void putU32(Bytes &bytes, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        bytes.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

void putF32(Bytes &bytes, float value) {
    std::uint32_t raw = 0;
    std::memcpy(&raw, &value, sizeof raw);
    putU32(bytes, raw);
}

void putControls(Bytes &bytes, const std::vector<SubSynthArchive::Control> &controls) {
    putTag(bytes, "CCOL");
    putU32(bytes, static_cast<std::uint32_t>(controls.size() * 16));
    for (const auto &control : controls) {
        putTag(bytes, "CTRL");
        putU32(bytes, 8);
        putU32(bytes, control.id);
        putF32(bytes, control.value);
    }
}

void putName(Bytes &bytes, const std::string &name) {
    std::string padded = name;
    padded.resize(SubSynthArchive::kPresetNameLength, '\0');
    bytes.insert(bytes.end(), padded.begin(), padded.end());
}

Bytes wrap(const Bytes &body) {
    Bytes block;
    putTag(block, "SSYN");
    putU32(block, static_cast<std::uint32_t>(body.size()));
    block.insert(block.end(), body.begin(), body.end());
    return block;
}

SubSynthArchive sample() {
    SubSynthArchive archive;
    archive.controls = {{0, 0.25f}, {7, 1.0f}, {42, -0.5f}};
    archive.presetName = "Warm Lead";
    archive.presetPath = "/sdcard/caustic/presets/subsynth/warm.subsynth";
    archive.keyboardOctave = 4;
    SubSynthArchive::Waveform wave{};
    for (std::size_t index = 0; index < wave.size(); ++index) {
        wave[index] = static_cast<std::int16_t>(index * 10 - 3000);
    }
    archive.customWaveform1 = wave;
    archive.customWaveform2 = SubSynthArchive::Waveform{};
    archive.polyphony = 6;
    archive.modulationMode = 2;
    archive.centsMode = 1;
    archive.patterns = {1, 2, 3, 4, 5};
    return archive;
}

} // namespace

TEST(SubSynthArchive, WritesTheCurrentLayout) {
    const Bytes bytes = sample().write(SubSynthArchive::Context::Song);

    EXPECT_EQ(std::string(bytes.begin(), bytes.begin() + 4), "SSYN");
    // Version 3.1 little-endian: the 0x0000, 0x01, 0x03 other readers see.
    EXPECT_EQ(bytes[8], 0x00);
    EXPECT_EQ(bytes[9], 0x00);
    EXPECT_EQ(bytes[10], 0x01);
    EXPECT_EQ(bytes[11], 0x03);
    EXPECT_EQ(std::string(bytes.begin() + 12, bytes.begin() + 16), "CCOL");
    EXPECT_EQ(std::string(bytes.begin() + 20, bytes.begin() + 24), "CTRL");

    const std::uint32_t length = bytes[4] | bytes[5] << 8 | bytes[6] << 16 | bytes[7] << 24;
    EXPECT_EQ(length, bytes.size() - 8);
}

TEST(SubSynthArchive, RoundTripsASong) {
    const SubSynthArchive original = sample();
    const Bytes bytes = original.write(SubSynthArchive::Context::Song);

    std::size_t consumed = 0;
    std::string error;
    const auto read =
        SubSynthArchive::read(bytes, SubSynthArchive::Context::Song, 0x00020000, &error, &consumed);
    ASSERT_TRUE(read.has_value()) << error;

    EXPECT_EQ(consumed, bytes.size());
    EXPECT_EQ(read->version, SubSynthArchive::kCurrentVersion);
    ASSERT_EQ(read->controls.size(), 3u);
    EXPECT_EQ(read->controls[2].id, 42u);
    EXPECT_FLOAT_EQ(read->controls[2].value, -0.5f);
    EXPECT_EQ(read->presetName, "Warm Lead");
    EXPECT_EQ(read->presetPath, original.presetPath);
    EXPECT_EQ(read->keyboardOctave, 4);
    EXPECT_EQ(read->customWaveform1, original.customWaveform1);
    EXPECT_EQ(read->polyphony, 6);
    EXPECT_EQ(read->modulationMode, 2);
    EXPECT_EQ(read->centsMode, 1);
    EXPECT_EQ(read->patterns, original.patterns);
    EXPECT_FALSE(read->needsQuarticTimeFixup());
    EXPECT_FALSE(read->needsWaveformRenumbering());
}

TEST(SubSynthArchive, PresetsCarryNoPathAndNoPatterns) {
    const Bytes song = sample().write(SubSynthArchive::Context::Song);
    const Bytes preset = sample().write(SubSynthArchive::Context::Preset);
    EXPECT_LT(preset.size(), song.size());

    const auto read = SubSynthArchive::read(preset, SubSynthArchive::Context::Preset);
    ASSERT_TRUE(read.has_value());
    EXPECT_TRUE(read->presetPath.empty());
    EXPECT_TRUE(read->patterns.empty());
    EXPECT_EQ(read->keyboardOctave, 4);
    EXPECT_EQ(read->polyphony, 6);
}

TEST(SubSynthArchive, OldSongsCarryNoPatterns) {
    const Bytes bytes = sample().write(SubSynthArchive::Context::Song);
    const auto read = SubSynthArchive::read(bytes, SubSynthArchive::Context::Song, 0x00010000);
    ASSERT_TRUE(read.has_value());
    EXPECT_TRUE(read->patterns.empty());
}

TEST(SubSynthArchive, ReadsAVersionOneBlock) {
    // 1.0: controls and the name only; polyphony is implied.
    Bytes body;
    putU32(body, 0x00010000);
    putControls(body, {{3, 0.5f}});
    putName(body, "Old");

    const auto read = SubSynthArchive::read(wrap(body), SubSynthArchive::Context::Preset);
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(read->polyphony, 8);
    EXPECT_FALSE(read->customWaveform1.has_value());
    EXPECT_FALSE(read->keyboardOctave.has_value());
    EXPECT_TRUE(read->needsQuarticTimeFixup());
    EXPECT_TRUE(read->needsWaveformRenumbering());
}

TEST(SubSynthArchive, TruncatesVersionOneFloatWaveforms) {
    Bytes body;
    putU32(body, 0x00010100);
    putControls(body, {});
    putName(body, "Float");
    for (std::size_t index = 0; index < SubSynthArchive::kCustomWaveformPoints; ++index) {
        putF32(body, index == 0 ? 123.9f : -45.7f);
    }

    const auto read = SubSynthArchive::read(wrap(body), SubSynthArchive::Context::Preset);
    ASSERT_TRUE(read.has_value());
    ASSERT_TRUE(read->customWaveform1.has_value());
    EXPECT_EQ((*read->customWaveform1)[0], 123);
    EXPECT_EQ((*read->customWaveform1)[1], -45);
    EXPECT_FALSE(read->customWaveform2.has_value());
    EXPECT_EQ(read->polyphony, 8);
}

TEST(SubSynthArchive, ReadsAVersionTwoBlock) {
    Bytes body;
    putU32(body, 0x00020000);
    putControls(body, {});
    putName(body, "Two");
    for (std::size_t index = 0; index < SubSynthArchive::kCustomWaveformPoints; ++index) {
        body.push_back(0x10);
        body.push_back(0x00);
    }
    putU32(body, 3);

    const auto read = SubSynthArchive::read(wrap(body), SubSynthArchive::Context::Preset);
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ((*read->customWaveform1)[5], 16);
    EXPECT_EQ(read->polyphony, 3);
    EXPECT_EQ(read->modulationMode, 0);
    EXPECT_TRUE(read->needsWaveformRenumbering());
}

TEST(SubSynthArchive, RejectsOtherBlocksAndTruncation) {
    Bytes other = wrap({0, 0, 0, 0});
    other[0] = 'P';
    std::string error;
    EXPECT_FALSE(SubSynthArchive::read(other, SubSynthArchive::Context::Song, 0, &error).has_value());
    EXPECT_FALSE(error.empty());

    Bytes bytes = sample().write(SubSynthArchive::Context::Song);
    bytes.resize(bytes.size() - 10);
    EXPECT_FALSE(SubSynthArchive::read(bytes, SubSynthArchive::Context::Song).has_value());
}

TEST(SubSynthArchive, SkipsEntriesThatAreNotControls) {
    Bytes body;
    putU32(body, 0x00010000);
    Bytes collection;
    putTag(collection, "CTRL");
    putU32(collection, 8);
    putU32(collection, 1);
    putF32(collection, 0.75f);
    putTag(collection, "XXXX");
    putU32(collection, 4);
    putU32(collection, 0);
    putTag(body, "CCOL");
    putU32(body, static_cast<std::uint32_t>(collection.size()));
    body.insert(body.end(), collection.begin(), collection.end());
    putName(body, "Mixed");

    const auto read = SubSynthArchive::read(wrap(body), SubSynthArchive::Context::Preset);
    ASSERT_TRUE(read.has_value());
    ASSERT_EQ(read->controls.size(), 1u);
    EXPECT_FLOAT_EQ(read->controls[0].value, 0.75f);
}
