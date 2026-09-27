#include "SubSynthArchive.h"

#include <algorithm>
#include <cstring>

namespace {

class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

    std::size_t position() const { return position_; }
    std::size_t remaining() const { return bytes_.size() - position_; }

    bool tag(const std::array<char, 4> &expected) {
        if (remaining() < 4) {
            return false;
        }
        const bool match = std::memcmp(bytes_.data() + position_, expected.data(), 4) == 0;
        position_ += 4;
        return match;
    }

    bool u32(std::uint32_t &value) {
        if (remaining() < 4) {
            return false;
        }
        value = static_cast<std::uint32_t>(bytes_[position_])
                | static_cast<std::uint32_t>(bytes_[position_ + 1]) << 8
                | static_cast<std::uint32_t>(bytes_[position_ + 2]) << 16
                | static_cast<std::uint32_t>(bytes_[position_ + 3]) << 24;
        position_ += 4;
        return true;
    }

    bool i32(std::int32_t &value) {
        std::uint32_t raw = 0;
        if (!u32(raw)) {
            return false;
        }
        value = static_cast<std::int32_t>(raw);
        return true;
    }

    bool f32(float &value) {
        std::uint32_t raw = 0;
        if (!u32(raw)) {
            return false;
        }
        std::memcpy(&value, &raw, sizeof value);
        return true;
    }

    bool i16(std::int16_t &value) {
        if (remaining() < 2) {
            return false;
        }
        value = static_cast<std::int16_t>(bytes_[position_] | bytes_[position_ + 1] << 8);
        position_ += 2;
        return true;
    }

    bool bytes(std::size_t count, std::span<const std::uint8_t> &out) {
        if (remaining() < count) {
            return false;
        }
        out = bytes_.subspan(position_, count);
        position_ += count;
        return true;
    }

private:
    std::span<const std::uint8_t> bytes_;
    std::size_t position_ = 0;
};

class Writer {
public:
    void tag(const std::array<char, 4> &value) {
        bytes_.insert(bytes_.end(), value.begin(), value.end());
    }
    void u32(std::uint32_t value) {
        for (int shift = 0; shift < 32; shift += 8) {
            bytes_.push_back(static_cast<std::uint8_t>(value >> shift));
        }
    }
    void i32(std::int32_t value) { u32(static_cast<std::uint32_t>(value)); }
    void f32(float value) {
        std::uint32_t raw = 0;
        std::memcpy(&raw, &value, sizeof raw);
        u32(raw);
    }
    void i16(std::int16_t value) {
        const auto raw = static_cast<std::uint16_t>(value);
        bytes_.push_back(static_cast<std::uint8_t>(raw));
        bytes_.push_back(static_cast<std::uint8_t>(raw >> 8));
    }
    void raw(const void *data, std::size_t count) {
        const auto *begin = static_cast<const std::uint8_t *>(data);
        bytes_.insert(bytes_.end(), begin, begin + count);
    }
    std::size_t size() const { return bytes_.size(); }
    void patchU32(std::size_t at, std::uint32_t value) {
        for (int index = 0; index < 4; ++index) {
            bytes_[at + static_cast<std::size_t>(index)] = static_cast<std::uint8_t>(value >> (8 * index));
        }
    }
    std::vector<std::uint8_t> take() { return std::move(bytes_); }

private:
    std::vector<std::uint8_t> bytes_;
};

bool fail(std::string *error, const char *message) {
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

// UIControlCollection::Serialize: "CCOL", a length, then one "CTRL" chunk per
// saved control, each with its own length (8) and an id/value pair.
bool readControls(Reader &reader, std::vector<SubSynthArchive::Control> &controls, std::string *error) {
    if (!reader.tag(SubSynthArchive::kControlCollectionTag)) {
        return fail(error, "missing CCOL control collection");
    }
    std::uint32_t length = 0;
    std::span<const std::uint8_t> body;
    if (!reader.u32(length) || !reader.bytes(length, body)) {
        return fail(error, "truncated control collection");
    }

    Reader entries(body);
    while (entries.remaining() >= 8) {
        const bool isControl = entries.tag(SubSynthArchive::kControlTag);
        std::uint32_t entryLength = 0;
        std::span<const std::uint8_t> entry;
        if (!entries.u32(entryLength) || !entries.bytes(entryLength, entry)) {
            return fail(error, "truncated control entry");
        }
        if (!isControl || entryLength < 8) {
            continue;
        }
        Reader pair(entry);
        SubSynthArchive::Control control;
        pair.u32(control.id);
        pair.f32(control.value);
        controls.push_back(control);
    }
    return true;
}

bool readWaveformShorts(Reader &reader, SubSynthArchive::Waveform &waveform) {
    for (std::int16_t &point : waveform) {
        if (!reader.i16(point)) {
            return false;
        }
    }
    return true;
}

} // namespace

std::optional<SubSynthArchive> SubSynthArchive::read(std::span<const std::uint8_t> bytes, Context context,
                                                     std::uint32_t songVersion, std::string *error,
                                                     std::size_t *consumed) {
    Reader outer(bytes);
    if (!outer.tag(kTag)) {
        fail(error, "not a SubSynth block");
        return std::nullopt;
    }
    std::uint32_t length = 0;
    std::span<const std::uint8_t> body;
    if (!outer.u32(length) || !outer.bytes(length, body)) {
        fail(error, "truncated SubSynth block");
        return std::nullopt;
    }
    if (consumed != nullptr) {
        *consumed = outer.position();
    }

    Reader reader(body);
    SubSynthArchive archive;
    if (!reader.u32(archive.version)) {
        fail(error, "missing version");
        return std::nullopt;
    }
    if (!readControls(reader, archive.controls, error)) {
        return std::nullopt;
    }

    std::span<const std::uint8_t> name;
    if (!reader.bytes(kPresetNameLength, name)) {
        fail(error, "truncated preset name");
        return std::nullopt;
    }
    const auto end = std::find(name.begin(), name.end(), std::uint8_t{0});
    archive.presetName.assign(name.begin(), end);

    if (archive.version >= kPresetPathVersion) {
        if (context == Context::Song) {
            std::uint32_t pathLength = 0;
            std::span<const std::uint8_t> path;
            if (!reader.u32(pathLength) || !reader.bytes(pathLength, path)) {
                fail(error, "truncated preset path");
                return std::nullopt;
            }
            archive.presetPath.assign(path.begin(), path.end());
        }
        std::int32_t octave = 0;
        if (!reader.i32(octave)) {
            fail(error, "missing keyboard octave");
            return std::nullopt;
        }
        archive.keyboardOctave = octave;
    }

    if (archive.version >= kCustomWaveformVersion) {
        Waveform first{};
        if (archive.version < kShortWaveformVersion) {
            // 1.x stored the points as floats; the engine truncates each to
            // a short.
            for (std::int16_t &point : first) {
                float value = 0.0f;
                if (!reader.f32(value)) {
                    fail(error, "truncated custom waveform");
                    return std::nullopt;
                }
                point = static_cast<std::int16_t>(static_cast<std::int32_t>(value));
            }
        } else if (!readWaveformShorts(reader, first)) {
            fail(error, "truncated custom waveform");
            return std::nullopt;
        }
        archive.customWaveform1 = first;

        if (archive.version >= kThirdGenerationVersion) {
            Waveform second{};
            if (!readWaveformShorts(reader, second)) {
                fail(error, "truncated second custom waveform");
                return std::nullopt;
            }
            archive.customWaveform2 = second;
        }

        if (archive.version >= kShortWaveformVersion && !reader.i32(archive.polyphony)) {
            fail(error, "missing polyphony");
            return std::nullopt;
        }
    }

    if (archive.version >= kThirdGenerationVersion) {
        if (!reader.i32(archive.modulationMode) || !reader.i32(archive.centsMode)) {
            fail(error, "missing oscillator modes");
            return std::nullopt;
        }
    }

    // The pattern editor serialises itself last, in songs from 2.0 on.
    if (context == Context::Song && (songVersion & 0x7FFFFFFFu) >= kSongPatternVersion) {
        std::span<const std::uint8_t> rest;
        reader.bytes(reader.remaining(), rest);
        archive.patterns.assign(rest.begin(), rest.end());
    }

    return archive;
}

std::vector<std::uint8_t> SubSynthArchive::write(Context context) const {
    Writer writer;
    writer.tag(kTag);
    const std::size_t lengthAt = writer.size();
    writer.u32(0);

    writer.u32(kCurrentVersion);

    writer.tag(kControlCollectionTag);
    writer.u32(static_cast<std::uint32_t>(controls.size() * 16));
    for (const Control &control : controls) {
        writer.tag(kControlTag);
        writer.u32(8);
        writer.u32(control.id);
        writer.f32(control.value);
    }

    std::array<char, kPresetNameLength> name{};
    std::copy_n(presetName.begin(), std::min(presetName.size(), kPresetNameLength - 1), name.begin());
    writer.raw(name.data(), name.size());

    if (context == Context::Song) {
        writer.u32(static_cast<std::uint32_t>(presetPath.size()));
        writer.raw(presetPath.data(), presetPath.size());
    }
    writer.i32(keyboardOctave.value_or(0));

    const Waveform silent{};
    for (const std::int16_t point : customWaveform1.value_or(silent)) {
        writer.i16(point);
    }
    for (const std::int16_t point : customWaveform2.value_or(silent)) {
        writer.i16(point);
    }
    writer.i32(polyphony);
    writer.i32(modulationMode);
    writer.i32(centsMode);

    if (context == Context::Song) {
        writer.raw(patterns.data(), patterns.size());
    }

    writer.patchU32(lengthAt, static_cast<std::uint32_t>(writer.size() - lengthAt - 4));
    return writer.take();
}
