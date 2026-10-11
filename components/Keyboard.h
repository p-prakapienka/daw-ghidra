#pragma once

// Note-number to frequency conversion, as UIKeyboard::GetFrequencyFromNoteId
// does it in libcaustic.so (ARMv7). See components/SubSynth.md.

class Keyboard {
public:
    // The table holds one entry per note id.
    static constexpr int kNoteCount = 128;

    // The note the tuning reference sits on, a literal in UIKeyboard's
    // constructor.
    static constexpr int kReferenceNote = 69;

    // g_fConcertA's initial value in .data.
    static constexpr float kConcertA = 440.0f;

    // 440 * 2^((note - 69) / 12), computed once in float with powf. The
    // engine indexes its table without a bounds check; this clamps to the
    // table instead.
    static float frequencyFromNoteId(int note);
};
