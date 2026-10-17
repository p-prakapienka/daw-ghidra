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

| Offset | Member | Meaning | Default |
| --- | --- | --- | --- |
| 0x00 | vtable | `~LFO` (complete, deleting), `GenerateInternal` | |
| 0x04 | `m_fRate` | rate, a float truncated to a division index | 1.0 |
| 0x08 | `m_nDepth` | depth, Q24 | `0x00FFFFFF` (1.0) |
| 0x0C | `m_pnOutput` | output block, `new int[128]` | — |
| 0x10 | `m_nSongPosition` | song position in samples, written each block | 0 |
| 0x14 | `m_nPeriod` | period in samples | computed |
| 0x18 | `m_nPositionOffset` | position offset, zeroed by `OnParamModified` | 0 |
| 0x1C | `m_nWaveform` | waveform | 0 |
| 0x20 | `m_fBPM` | tempo in BPM | 120.0 |
| 0x24 | `m_n24` | written `0x00FFFFFF` by the constructor and `OnParamModified`, not read by `GenerateInternal` | |
| 0x28 | `m_nPhaseOffset` | phase offset, Q24 | 0 |
| 0x2C | `m_n2C` | zeroed by the constructor, not read by `GenerateInternal` | 0 |
| 0x30 | `m_nSmoothed` | smoothed value for sawtooth and square | 0 |
| 0x34 | `m_nRaw` | last unsmoothed value | 0 |
| 0x38 | `m_nSmoothing` | smoothing coefficient, Q24 | `0x00FD70A3` (0.99) |

The object is `0x3C` bytes. In SubSynth the three LFOs sit at `+0x36A8`,
`+0x36E4` and `+0x3724`, with LFO 1's target at `+0x3720` and LFO 2's at
`+0x3760` right after each.

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
only oscillator 1 has its FM depth modulated. `SubSynthRouting::connect` in
`src/machines/subsynth/SubSynthVoice.h` performs the wiring; `LFO::Target` records the
numbering.

## The port

`LFO` is ported to the original's structure (`dsp/LFO.h`). It is a root
class (its typeinfo has no base), whose vtable holds the two destructors and
`GenerateInternal`:

- **Methods.** `LFO()`, `~LFO()`, `GenerateInternal(unsigned int)`,
  `OnParamModified()` and `SetBPM(float)` have the engine's signatures.
- **Members.** They are public at the engine's offsets and types, `0x3C`
  bytes on a 32-bit build, checked by `static_assert` and with `g++ -m32`.
  Their names are ours.
- **How SubSynth drives it.** It writes the rate, depth, waveform and phase
  offset and then calls `OnParamModified`. Before each block it writes
  `m_nSongPosition` and calls `GenerateInternal`.
- **The output block.** It is `new int[128]`, as in the engine. The engine
  leaves it uninitialised and frees it with scalar `delete` (`0x807ac`).
  Here it is zeroed and freed with `delete[]`; the zeroing is never visible,
  because every LFO generates a block before anything reads it.
- **`GenerateInternal` takes the engine's contract.** It no longer clamps
  the block to 128 samples (the engine does not), and it guards the modulo
  against a zero period, where the engine would divide by zero.

**Correction:** `OnParamModified` stores the period as `vcvt.u32.f32` gives
it, which is 0 for an absurd tempo. The earlier component replaced 0 with 1.

The default modulation source is SubSynth's third LFO. Its constructor
(`0xa0494`–`0xa04d0`) sets `m_nWaveform` to `0x07FFFFFF` and calls
`OnParamModified`, sets `m_nDepth` to 0 and calls it again, then runs
`GenerateInternal(128)`. The "off" waveform writes zeros. `SubSynth` does
the same with `SubSynthVoice::Shared::defaultSource`.

## Checked against the original

`OnParamModified`, `SetBPM` and `GenerateInternal` are identical to the
3.2.0 (beta 7) armv7 library in 100 scenarios. Each compared the period and
every output block. They covered:

- all four waveforms at rates 0, 3, 6 and 12;
- half and zero depth, a phase offset, a fractional rate, and a rate past the
  division table and below zero;
- two other tempos and an unknown waveform;
- four runs of blocks each: contiguous 128-sample blocks, 16-sample blocks, a
  jump in song position, and an odd start with 100-sample blocks.

No change was needed.

## Not established

What reads `+0x24` and `+0x2C`.

The Volume input at SubSynth `+0x3764` turned out not to involve `ApplyVCA`:
`ProcessChannel`'s output loop scales every sample by `1 + value`, after the
level smoother. See `src/machines/subsynth/SubSynthVoice.md`.
