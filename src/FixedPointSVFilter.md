# FixedPointSVFilter — reconstruction notes

Source: `libcaustic.so`, ARMv7. SubSynth's per-voice filter. `MultiFilter` is a
separate rack effect and is not this class; `FloatSVFilter` is the same design
in floating point.

```
FixedPointSVFilter::FixedPointSVFilter(bool)                  0x94778    560 bytes
FixedPointSVFilter::ProcessCV(ControlVoltage*, int*, uint, int) 0x949a8  2728 bytes
```

`SubSynth::SubSynth` constructs it with `true`, and `SubSynth::ProcessChannel`
calls `ProcessCV` per voice.

## The port

This is the first class in `src/`, the tree that follows the original's
structure: class names, signatures, member order and control flow. It
replaces `components/StateVariableFilter`, which reproduced the same
behaviour through an API of our own.

| From the library | In `src/` |
|---|---|
| typeinfo: `FixedPointSVFilter` → `Filter` → `Machine` | `FixedPointSVFilter.h`, `Filter.h`, `Machine.h` |
| `Machine`'s weak inlines and vtable order | `Machine.h`, with `m_pInput` at `+0x04` |
| `Envelope`, the vtable with only `IsDone` and `GenerateValues` | `Envelope.h`; `ADSR` now derives from it |
| `FixedPointSVFilter(bool)`, `ProcessCV(ControlVoltage*, int*, unsigned int, int)` | the same signatures |

- **Member names.** The names are ours, in the engine's Hungarian style
  (`m_n…`, `m_f…`, `m_an…`); only the offsets and types are the engine's.
- **Public fields.** The members are public because `SubSynth` writes them
  directly.
- **Size check.** On a 32-bit build, `static_assert` checks the object is
  `0x844` bytes.
- **`UpdateCoefficients`** is the coefficient update the constructor ends
  with. Each `ProcessCV` control block and both `SubSynth::OnChildModified`
  filter branches repeat it. The library has no out-of-line function for it;
  each of those places has the code inline. So here it is an inline member.
- **`ProcessCV`** follows the original's shape. It loads the coefficients and
  states, dispatches on the mode, and runs one loop per tap, each refreshing
  the coefficients every 16 samples. It writes the states back on every path,
  including the modes it does nothing for.

## Coefficient tables

The constructor builds two 256-entry Q24 tables with `powf`, from the literals
at `0x9497c`–`0x9498c` (`0.36`, `0.64`, `1/256`, `16777215`):

```
x = i / 256
cutoff[i]  = 0.5^(8 * (1 - (0.36 + 0.64x))) * 16777215   ; 482443 .. 16546238
damping[i] = 0.5^(base + 4x)               * 16777215    ; base = 1.5 or 0
```

The constructor's `bool` selects that base. SubSynth passes `true`, so its
damping runs 0.354 down to 0.022; with `false` it starts at unity.

## Layout

| Offset | Member | Meaning |
| --- | --- | --- |
| 0x00 | vtable | `FixedPointSVFilter` |
| 0x04 | `m_pInput` | `Machine`'s input, 0 |
| 0x08 | `m_nMode` | 1 low-pass, 2 high-pass, 3 band-pass, anything else does nothing |
| 0x0C | `m_anCutoffTable` | cutoff table, 256 ints |
| 0x40C | `m_anDampingTable` | damping table, 256 ints |
| 0x80C | `m_pModulation` | the cutoff's modulation source, an LFO (0 until `ConnectLFOs`) |
| 0x810 | `m_DefaultEnvelope` | an embedded `Envelope`: always done, writes nothing |
| 0x814 | `m_pEnvelope` | starts at `+0x810`; SubSynth points it at the voice's filter ADSR |
| 0x818 | `m_nInputGain` | input gain, Q24 |
| 0x81C, 0x820 | `m_anBand` | band integrator, first and second channel |
| 0x824, 0x828 | `m_anLow` | low integrator, first and second channel |
| 0x82C | `m_nCutoff` | resolved cutoff coefficient |
| 0x830 | `m_nDamping` | resolved damping coefficient |
| 0x834 | `m_n834` | zeroed by the constructor, not read by `ProcessCV` |
| 0x838, 0x83C | `m_fCutoff`, `m_fResonance` | cutoff and resonance, as floats |
| 0x840 | `m_bInvertEnvelope` | invert the envelope |
| 0x841 | `m_bExtendedResonance` | the constructor's flag |

**Correction:** an earlier version of this table had the states as "low and
band, first channel" at `0x81C`/`0x820` and the same for the second channel
at `0x824`/`0x828`. The loop at `0x94aa0` reads `0x81C` and `0x824` for the
first channel: `0x81C` is updated as the band integrator and `0x824` as the
low one. The second channel's band and low are at `0x820` and `0x828`.

The object is `0x844` bytes, so every voice carries its own 2 KB copy of two
identical tables.

## Per-sample core

Both coefficients are combined once per call:

```
feedback = 0xFFFFFF - q24mul(damping, cutoff)     ; 1 - f*q
gain     = q24mul(cutoff, inputGain)
```

and then, per sample and per channel:

```
band = q24mul(feedback, band) - q24mul(cutoff, low) + q24mul(input, gain)
low  = q24mul(feedback, low)  + q24mul(cutoff, band)
```

Every multiply is `smull` followed by a 24-bit shift of the 64-bit product.

Note that **both** integrators are damped by the same `1 - f*q` factor. A
textbook Chamberlin filter damps only the band-pass path, so this one leaks at
low frequencies — visible as the roll-off below the corner in the low-pass
response.

The three outputs are taps off the same pair of states:

| Mode | Output | Evidence |
| --- | --- | --- |
| 1, low-pass | `low` | `str sl, [fp]` at 0x94b38 |
| 2, high-pass | `input - low` | `rsb r3, sl, ip` at 0x94e78, with `ip` reloaded from the buffer |
| 3, band-pass | `q24mul(band - low, 0xE66665)` | 0x951a0; the trim is an immediate, 0.9 in Q24 |

The mode names are the three adjacent strings `LowPass`, `HighPass`, `BandPass`
in `.rodata` at `0x265cf0`, and SubSynth exposes a `Filter Mode` control.

## Stereo

`ProcessCV`'s fourth argument is the channel count. The prologue computes
`channels != 2` and the loop uses it to decide whether to filter a second
interleaved sample: two channels advance the pointer by 8 bytes, anything else
by 4. Both channels share one coefficient pair and keep separate state.

## The control block

`ProcessCV` runs its per-sample loop in groups of sixteen and recomputes the
coefficients at the top of each group (`0x94c18`–`0x94d6c`). With the control
voltage mapped, the whole block reads:

```
gate     = cv.flags bit 1
position = (gate ? cv[0x08] : cv[0x0C]) + sampleOffset
envelope->GenerateValues(position, gate, &value, 1)      ; vtable slot 1
e = value / 32767
if (this[0x840]) e = 1 - e                                ; invert

mod  = (*this[0x80C])[0x0C][sampleOffset]                  ; per-sample Q24 array
lift = 1 + mod * 0x33800001                                ; 1 / 16777215
cutoff    = e * lift * (this[0x838] * cv[0x58])           ; base * keyboard tracking
resonance = e * this[0x83C]                               ; the envelope scales this too

cutoffIndex = cutoff < 0.05 ? 12 : cutoff > 1 ? 255 : (int)(cutoff * 255)
resIndex, gainTerm =
    resonance < 0 ? (0, 0)
  : resonance > 1 ? (255, -0.75)
  :                 ((int)(resonance * 255), -0.75 * resonance)

this[0x82C] = cutoffTable[cutoffIndex]
this[0x830] = dampingTable[resIndex]
this[0x818] = ((this[0x841] ? 1.0 : 1.25) + gainTerm) * 16777215   ; input gain
feedback = 0xFFFFFF - q24mul(damping, cutoff)
drive    = q24mul(cutoff, inputGain)
```

Three things in there were wrong or missing in the earlier notes.

The cutoff floor does not select entry 0. Below 0.05 the index is set to
**12**, which is what `0.05 * 255` truncates to anyway — the floor is simply
"behave as 0.05", not a special table entry.

The **input gain is not a free parameter**. It is derived from resonance:
`(1.0 - 0.75 * resonance)` for the extended variant SubSynth uses, or
`(1.25 - 0.75 * resonance)` otherwise, times full scale. For SubSynth the
drive therefore falls from 1.0 at zero resonance to 0.325 at its slider's
0.9 limit (0.25 at 1.0), which is what keeps the resonant peak in range.
The port has no way to set the gain directly; it is always the engine's.

**Correction:** an earlier version of this note and of the component had the
two bases the other way round. Every site loads 1.25 and replaces it with 1.0
when the flag byte is non-zero:

- the constructor at `0x94914`–`0x94938`: `cmp r1, #0`, `vmov s12, #1.25`,
  `vmov s13, #1.0`, `vmoveq s13, s12`;
- `ProcessCV` at `0x94d04`–`0x94d44`, the same shape;
- both `SubSynth::OnChildModified` filter branches (`0x9effc`, `0x9f134`):
  `vmov #1.25`, then `vmovne` from `s16` = 1.0.

So SubSynth's filters were driven 0.25 of full scale too hard.

The constructor ends the same way (`0x948a4`–`0x9494c`). It sets the cutoff
base to 1.0 and the resonance base to 0.0, then resolves them without an
envelope: cutoff entry 255, damping entry 0, and the input gain for zero
resonance. The port does the same.

And the envelope scales **both** cutoff and resonance. That is unusual, but
`vmul.f32 s15, s15, s12` at `0x94ca8` is unambiguous: the same `e` that scaled
the cutoff multiplies the resonance base.

The envelope is called through a vtable whose slot 0 is `IsDone` and slot 1 is
`GenerateValues` — the ADSR's layout. The filter only ever calls slot 1. The
`IsDone` call at the end of `ProcessChannel` goes to the voice's *amplitude*
envelope (SubSynth `+0x3068 + 0x64·voice`), not to this one. The filter's
`+0x814` defaults to an embedded null envelope at `+0x810` and SubSynth points
it at the voice's filter ADSR (SubSynth `+0xBBA8 + 0x64·voice`). When the voice
ends, `ProcessChannel` zeroes `+0x81C`–`+0x828`, the four integrator states;
`SubSynthVoice` writes the four members the same way.

The per-sample array at `+0x80C` is a modulation source, not the control
voltage. `SubSynth::ConnectLFOs` points every modulation input — the two
oscillators' fields and each voice filter's `+0x80C` — at a default source, then
rewires one input per LFO according to the LFO target. So `+0x80C` is an LFO
output when an LFO targets the filter, and the default source otherwise.
That default source is an LFO at SubSynth `+0x36A8` that nothing ever runs,
so its output stays zero. `SubSynthVoice::Shared::defaultSource` plays that
part here, and the filter always has a source.

**Correction:** the modulation scale is the literal `0x33800001`
(`0x94dcc`), the float nearest `1 / 16777215`, as in the oscillator. The
earlier component used exactly `2^-24`. The two differ by one ulp, which can
move the cutoff index across a table step.

`ProcessCV` dispatches on the mode first (`0x94a10`–`0x94a4c`). Any mode other
than 1–3 writes the four integrator states back unchanged and returns, so no
control block runs: the buffer, the input gain and both coefficients are left
as they were.

## Checked against the original

Both checks compare against the 3.2.0 (beta 7) armv7 library, bit for bit:

- **The constructor:** both tables and the resolved state, for both flag
  values.
- **`ProcessCV`, 81 scenarios:** every mode, plus mode 0 and an unknown mode;
  cutoff and resonance in and past their ranges; inversion; gate open and
  closed; key tracking; mono and stereo; modulation. The original runs with
  its own `ADSR`.
- **Inside SubSynth,** the 28 whole-machine scenarios in `SubSynthVoice.md`.
