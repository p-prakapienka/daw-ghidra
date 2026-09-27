# LFO — reconstruction notes

Source: `libcaustic.so`, ARMv7. SubSynth builds three `LFO` objects: two it
exposes as LFO 1 and LFO 2, and one it uses as the default modulation source
that every input points at until an LFO is aimed there.

```
LFO::LFO()                             0x80ad8   136 bytes
LFO::OnParamModified()                 0x80a74   100
LFO::SetBPM(float)                     0x80b60     8
LFO::GenerateInternal(unsigned int)    0x807e0   660
```

## Layout

From the constructor:

| Offset | Meaning | Default |
| --- | --- | --- |
| 0x04 | rate, a float truncated to a division index | 1.0 |
| 0x08 | depth, Q24 | `0x00FFFFFF` (1.0) |
| 0x0C | output block, `new int[128]` | — |
| 0x10 | song position in samples, written each block | 0 |
| 0x14 | period in samples | computed |
| 0x18 | position offset, zeroed by `OnParamModified` | 0 |
| 0x1C | waveform | 0 |
| 0x20 | tempo in BPM | 120.0 |
| 0x24 | written `0x00FFFFFF`, not read by `GenerateInternal` | |
| 0x28 | phase offset, Q24 | 0 |
| 0x30 | smoothed value for sawtooth and square | 0 |
| 0x34 | last unsmoothed value | 0 |
| 0x38 | smoothing coefficient, Q24 | `0x00FD70A3` (0.99) |

The 128-int output block is why blocks never exceed 128 samples.

## Rate

`OnParamModified` truncates the rate to an index and reads a table of cycles per
beat at `0x270104`:

```
0.0625  0.125  0.25  0.5  0.75  1  1.5  2  3  4  6  8  16
```

Indices above 12 use 1.0. The period is `(60 / bpm) / division * 44100`,
truncated to an unsigned integer. The LFO is **always tempo-synced**; there is no
free-running rate in hertz. At the default rate of 1.0 — index 1, an eighth of a
cycle per beat — and 120 BPM, one cycle lasts eight beats.

## Phase

`GenerateInternal` does not carry phase between blocks. It recomputes it:

```
offset    = (this[0x18] + this[0x10]) % period
increment = (int)((4 / period) * 16777215)
phase     = this[0x28] + q24mul((int)((offset / period) * 16777215), 0x03FFFFFC)
```

and `SubSynth::Process` writes the song position into `0x10` of both LFOs
before each block. So the LFO is **phase-locked to the transport**: generating
the same position twice gives the same output, notes do not retrigger it, and
seeking the song moves it. One cycle is four units of phase (`0x03FFFFFC` in
Q24).

## Waveforms

The value at `0x1C` selects the shape. Each loop adds the increment first, wraps
by subtracting `0x03FFFFFC`, then shapes:

| Value | Shape | Wrap | Formula |
| --- | --- | --- | --- |
| 0 | sine | `x > 2` | `x * (2 - |x|)` — a parabolic approximation |
| 1 | triangle | `x >= 1` | `|2 - |x|| - 1` |
| 2 | sawtooth, falling | `x > 2` | `-x / 2`, then smoothed |
| 3 | square | `x >= 4` | `+1` for `x` in `[1, 3]`, else `-1`, then smoothed |
| `0x07FFFFFF` | off | | writes zeros |
| other | | | returns without writing |

Everything is multiplied by the depth at the end. The sine starts at zero
rising; the triangle starts at the top.

The sawtooth and square pass through a one-pole smoother,
`state = target + 0.99 * (state - target)`, which softens their jumps. It also
makes the sawtooth lag its ramp by about 99 samples and never quite reach the
rail at the reset — part of the sound, not a defect.

The off value is the sentinel the oscillator tests for at `+0x1C` of a
modulation source: seeing it, the oscillator skips its octave and semitone
arithmetic. That is how the default source works — an LFO switched off.

## Targets

`SubSynth::ConnectLFOs` first points every modulation input at the default
source, then aims each LFO at one input according to its target, stored just
after the LFO object. LFO 1 is wired first and LFO 2 second, so if both choose
the same input LFO 2 wins.

| Target | Label | Input |
| --- | --- | --- |
| 0 | ------ | none |
| 1 | Osc 1 | oscillator 1 vibrato, `+0x00` |
| 2 | Osc 2 | oscillator 2 vibrato, `+0x00` |
| 3 | Osc 1+2 | both oscillators, `+0x00` |
| 4 | Phase | oscillator 2 phase, `+0x08` |
| 5 | Cutoff | every voice filter, `+0x80C` |
| 6 | Volume | SubSynth `+0x3764` |
| 7 | Octave | oscillator 2 octave, `+0x10` — and sets the LFO's phase offset to half a cycle |
| 8 | Semis | oscillator 2 semitone, `+0x14` |
| 9 | Osc1 Mod | oscillator 1 FM depth, `+0x18` |

The labels are adjacent strings in `.rodata` at `0x265f64`, in this order.
Every target except 0 also resets the LFO's phase offset — to zero, or to half
a cycle for Octave, so an octave-switching LFO starts mid-cycle.

Only oscillator 2 can be phase-, octave- or semitone-modulated by an LFO, and
only oscillator 1 has its FM depth modulated. The wiring itself belongs to the
SubSynth step; `LFO::Target` records it as data.

## Not established

What reads `+0x24`; and the Volume input at SubSynth `+0x3764`, which will come
out of `ApplyVCA` in the voice step.
