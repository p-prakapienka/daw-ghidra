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

| Offset | Meaning |
| --- | --- |
| 0x08 | mode: 1 low-pass, 2 high-pass, 3 band-pass, anything else passthrough |
| 0x0C | cutoff table, 256 ints |
| 0x40C | damping table, 256 ints |
| 0x818 | input gain |
| 0x81C, 0x820 | low and band state, first channel |
| 0x824, 0x828 | low and band state, second channel |
| 0x82C | resolved cutoff coefficient |
| 0x830 | resolved damping coefficient |
| 0x838, 0x83C | cutoff and resonance base, as floats |
| 0x840 | invert the envelope |
| 0x841 | the constructor's extended-resonance flag |

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
lift = 1 + mod * 2^-24
cutoff    = e * lift * (this[0x838] * cv[0x58])           ; base * keyboard tracking
resonance = e * this[0x83C]                               ; the envelope scales this too

cutoffIndex = cutoff < 0.05 ? 12 : cutoff > 1 ? 255 : (int)(cutoff * 255)
resIndex, gainTerm =
    resonance < 0 ? (0, 0)
  : resonance > 1 ? (255, -0.75)
  :                 ((int)(resonance * 255), -0.75 * resonance)

this[0x82C] = cutoffTable[cutoffIndex]
this[0x830] = dampingTable[resIndex]
this[0x818] = ((this[0x841] ? 1.25 : 1.0) + gainTerm) * 16777215   ; input gain
feedback = 0xFFFFFF - q24mul(damping, cutoff)
drive    = q24mul(cutoff, inputGain)
```

Three things in there were wrong or missing in the earlier notes.

The cutoff floor does not select entry 0. Below 0.05 the index is set to
**12**, which is what `0.05 * 255` truncates to anyway — the floor is simply
"behave as 0.05", not a special table entry.

The **input gain is not a free parameter**. It is derived from resonance:
`(1.25 - 0.75 * resonance)` for the extended variant SubSynth uses, or
`(1.0 - 0.75 * resonance)` otherwise, times full scale. As resonance rises the
drive drops from 1.25 to 0.5, which is what keeps the resonant peak in range.
`setInputGain` remains for hosts that want to override it, but `updateControlBlock`
sets it the engine's way.

And the envelope scales **both** cutoff and resonance. That is unusual, but
`vmul.f32 s15, s15, s12` at `0x94ca8` is unambiguous: the same `e` that scaled
the cutoff multiplies the resonance base.

The envelope is called through a vtable whose slot 0 is `IsDone` and slot 1 is
`GenerateValues` — the ADSR's layout — and `ProcessChannel` calls slot 0 with
`cv[0x0C]` after every block to decide whether the voice is finished. The
filter's `+0x814` defaults to an embedded null envelope at `+0x810` and
SubSynth points it at the voice's filter ADSR.

The per-sample array at `+0x80C` is a modulation source, not the control
voltage. `SubSynth::ConnectLFOs` points every modulation input — the two
oscillators' fields and each voice filter's `+0x80C` — at a default source, then
rewires one input per LFO according to the LFO target. So `+0x80C` is an LFO
output when an LFO targets the filter, and the default source otherwise.

`updateControlBlock` and `processVoice` implement all of this. The remaining
untested claim is that the default source reads as zeros; this component treats
a null modulation pointer that way.

## Measured response

Cutoff 0.5, resonance 0.2, RMS of a filtered sine relative to full scale:

```
           100 Hz   500 Hz   2 kHz    8 kHz
low-pass   0.683    0.801    0.378    0.018
high-pass  0.027    0.132    1.033    0.714
band-pass  0.492    0.637    0.585    0.095
```

The reconstruction is behaviourally faithful, not proven bit-exact: it has not
been compared sample-for-sample against the engine.
