# SubSynth archive block

Recovered from `SubSynth::Serialize(Archive&)` (`0x9d09c`, 2636 bytes),
`UIControlCollection::Serialize(Archive&)` (`0x16d6b4`) and
`Archive::Serialize(void*, unsigned)` (`0x1cb438`).

## `Archive`

`Archive::Serialize(ptr, n)` reads or writes `n` bytes and returns `n`,
depending on the mode word at `+0x08`: mode 0 `fread`s, mode 1 `fwrite`s. On a
short write it clears the byte at `+0x00`. `+0x04` is the `FILE*`, and
`+0x10` is the enclosing song's version. The same `Serialize` function
therefore defines both directions, and every version test in it applies
when loading.

## Layout, current version (3.1)

| Field | Size | Condition |
|---|---|---|
| `"SSYN"` | 4 | |
| length of everything after this field | 4 | written last, via `ftell`/`fseek` |
| version `0x03010000` | 4 | |
| control collection (`CCOL`) | 8 + 16·n | |
| preset name | 32 | |
| preset path length, then its bytes | 4 + len | version ≥ 3.1 and not a preset |
| keyboard octave | 4 | version ≥ 3.1 |
| custom waveform 1 | 660 × i16 | version ≥ 1.1 (660 × f32 before 2.0) |
| custom waveform 2 | 660 × i16 | version ≥ 3.0 |
| polyphony | 4 | version ≥ 2.0 |
| oscillator 1 modulation mode | 4 | version ≥ 3.0 |
| oscillator 2 cents mode | 4 | version ≥ 3.0 |
| pattern editor block | rest | not a preset, and song version (top bit masked) ≥ 2.0 |

"Not a preset" is the byte at SubSynth `+0xC2F6` clear. `LoadPreset` and
`SavePreset` set it around their call.

The version is little-endian, so readers that split it into u16/u8/u8 see
`0x0000, 0x01, 0x03`. DawVert's `proj_caustic.py` does exactly that. Its
SSYN branch reads the same fields in the same order, and names the keyboard
octave `unknown4`.

When loading:

- If the tag is not `SSYN`, the function still reads the length and then
  returns without seeking, so the caller's position is wrong afterwards.
- Every voice slot gets the `SetPolyphony` control-voltage reset before
  anything else is read.
- The name is read into a 32-byte buffer and set as the preset label.
- On save the label's own 32-byte buffer is written as-is, so bytes after
  the terminator may be stale text. The component writes zeros there.
- The path is read into a 0x104-byte global (cleared first) and handed to
  `UIFileBrowserDialog::TryToSetStartDir`.
- The octave goes to `UIKeyboard::SetOctave`.
- Each waveform goes through `UICustomWaveformDialog::SetRawValues`, then
  `GetWaveform(buffer, 4096, 0.3)` into the 4096-short tables at `+0x2F78`
  and `+0x2F7C`. Those are what oscillator types 7 and 8 play. Before 3.0,
  waveform 2's dialog is cleared instead.
- Polyphony goes to `SetPolyphony`, which gets 8 when the field is absent.
- The two modes go to `SetOscillator1ModulationMode` and
  `SetOscillator2CentsMode`, which get 0 when the fields are absent.

## Control collection

```
"CCOL"  u32 length
  "CTRL" u32 8  u32 id  f32 value      ; one 16-byte chunk per control
  ...
```

On save a control is written only when its slot in the collection
(`+0x3C + 4·id`) is non-null and its flag byte at `+0x23C + id` is set. The
chunk length and the collection length are both written after the fact, with
`ftell`/`fseek`.

On load the value goes to `control->vtable[0x38](value)`, then through the
machine's usual control handling.

A chunk that is not tagged `CTRL` is not skipped properly by the engine:
the remaining length is not reduced. This component skips it by its
declared length.

**Each entry is a `CTRL` sub-chunk, not a bare id/value pair.** DawVert
agrees: it iterates IFF chunks inside `CCOL` and reads the id and value from
each.

## Legacy fix-ups on load

These act on the UI controls after the collection has been applied. They are
identified by the machine field that holds the control pointer, with the
control id from `SubSynthControls.md`.

- **Version ≤ 2.1.0** (`≤ 0x00020100`): the controls at `+0xC260`,
  `+0xC264`, `+0xC26C`, `+0xC270`, `+0xC274` and `+0xC27C` (ids 21, 22,
  24, 25, 26, 28) get `SetValue(sqrt(sqrt(GetValue())))`. These are the
  attack, decay and release times of both envelopes; sustain (`+0xC268`,
  `+0xC278`) is left alone.
  - **Correction:** an earlier version of this note said the current UI
    applies a fourth-power curve. It applies a square: these knobs'
    `GetValue` returns the position squared. The fourth root acts on that
    square, so a stored time x becomes the position √x, which reads back as
    x seconds. Old files stored the time itself, and the fix-up keeps it.
    The collection is applied first, though, and the knob clamps the stored
    x to its 1.75 maximum before the fix-up reads it, so old times above
    1.75 s come back as 1.75 s. (An earlier version of this correction gave
    the limit as 3.0625 s.) See `SubSynthControls.md`.
- **Version < 3.0**: the waveform selectors are renumbered, because 3.0
  inserted the two HQ types.
  - Oscillator 1 (`+0xC200`, id 1): v ≥ 4 → v + 2, else v ≥ 3 → v + 1.
  - Oscillator 2 (`+0xC20C`, id 4, where 0 is off): v ≥ 5 → v + 2, else
    v ≥ 4 → v + 1.

`SubSynthArchive` reports both conditions (`needsQuarticTimeFixup`,
`needsWaveformRenumbering`) instead of applying them.

## Not established

- `CreateDefaultMapping`, the default MIDI CC mapping. The control id table
  itself is in `SubSynthControls.md`.
- `UICustomWaveformDialog::GetWaveform`, which turns 660 raw points into a
  4096-sample table. This is the missing piece for oscillator types 7 and 8.
- The pattern editor's block (`SynthPatternEditor`, vtable `+0x80`).
