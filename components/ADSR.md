# ADSR — reconstruction notes

Source: `libcaustic.so`, ARMv7 Android build. The library is stripped of
`.symtab` but keeps its dynamic symbols, so the five methods below are named and
sized exactly. Everything here was read out of the disassembly; nothing is
guessed unless it says so.

```
ADSR::ADSR()                                          0x806fc   148 bytes
ADSR::OnParamModified()                               0x80628   212 bytes
ADSR::SetMinSamples(uint, uint, uint)                 0x80618    16 bytes
ADSR::IsDone(uint)                                    0x7e528    24 bytes
ADSR::GenerateValues(uint, uint, short*, uint)        0x7f708  3428 bytes
```

Sibling classes `AREnvelope` and `DecayEnvelope` have the same five-method
shape and are cut-down versions of this one.

## Parameters

`OnParamModified` converts seconds to samples and clamps:

```
samples = clamp((uint)vcvt(seconds * 44100.0f), minSamples, 220500)
sustainQ15 = (short)vcvt(sustain * 32767.0f)
```

`vcvt` is `vcvt.s32.f32`: it truncates and clamps at both ends, so an
oversized sustain becomes `0x7FFFFFFF` and keeps `0xFFFF`. SubSynth's
amplitude envelopes are built with a sustain of 16777215.0, so their
`sustainQ15` is −1 until the Sustain control's default replaces it. A plain
C++ cast would be undefined there, and gives `INT_MIN` on x86 (low half 0),
so the component clamps explicitly.

- `44100.0f` and `32767.0f` are literals in the function's constant pool at
  `0x806f4` and `0x806f8`. The sample rate is **hardcoded**; the envelope does
  not know the real output rate.
- `220500` is `0x35D54`, loaded by `movw`/`movt` before each range check. It is
  exactly five seconds at 44100.
- The minimums come from `SetMinSamples` and default to `0x50` (80 samples).
- A negative time is converted by `vcvt.s32.f32` and stored in an unsigned
  field, so it wraps past the upper bound and ends up clamped to 220500 rather
  than to the minimum. The reconstruction keeps that behaviour.
- Both a too-short and a too-long value are replaced, the first by the minimum
  and the second by the maximum. The register holding the bound is reloaded
  between the two comparisons, which is easy to misread as clamping both ends to
  the minimum.

`IsDone(position)` is `releaseSamples + 0x800 < position` — a 2048-sample tail
after the release finishes.

## Envelope domain

Segments are generated as a 24-bit ramp. The ramp constant is `0xFFFE00`, not
`0x1000000`: it starts one 512th below full scale so the first shaped value
cannot overflow Q15. Output samples are Q15, `0 .. 32767`, written as `short`.

Falling segments floor at zero with `v & ~(v >> 15)`, the branchless form of
`max(v, 0)`.

## Segment shapes

Each of attack, decay and release stores a shape as a plain int, at offsets
`0x3C`, `0x40` and `0x44`. The constructor sets all three to `1`. Given a 24-bit
ramp `r`:

| Value | Shape | Formula |
| --- | --- | --- |
| 1 | Linear | `s = (r << 7) >> 16` |
| 2 | Exponential | `s = (r << 7) >> 16; y = (s * s * 2) >> 16` |
| 0 | Logarithmic | `s = (r << 6) >> 16; y = 2 * ((((s * 22936) >> 15) << 15) / min(s + 6717, 0x7FFF))` |

The logarithmic case is the one worth care. In ARM it reads:

```
ubfx   r1, r4, #0xa, #0x10      ; s = (ramp << 6) >> 16
movw   ip, #0x5998              ; 22936, about 0.6999 in Q15
smulbb r3, r1, ip               ; s * 22936
add    r1, r1, #0x1a00
add    r1, r1, #0x3d            ; s + 6717
cmp    r1, r2                   ; clamp the divisor at 0x7FFF
movge  r1, r2
asr    r0, r3, #0xf
lsl    r0, r0, #0xf             ; (product >> 15) << 15
bl     __aeabi_idiv
```

**The `lsl r0, r0, #0xf` matters.** The existing decompiled draft in this repo
simplified the line to `(s * 22936 / 32768) / divisor`, which drops the shift
back up and makes the whole curve evaluate to zero or one. With the shift, the
hyperbola reaches about 0.993 of full scale at the top of the ramp.

## Note handling

`GenerateValues(position, gate, out, n)` takes the sample offset since the last
gate change and the gate. `GenerateValues` is one function of 3428 bytes whose
loops are written out once per curve, and the curves are **not** handled
alike. `ADSR.cpp` follows that shape: one loop per curve per segment.

### Starting a note

With the gate set:

- **Retrigger.** If the byte at `0x61` is clear and the position is below the
  last one seen (`0x38`), the note restarts from zero (`0x7fca8`).
- **New note.** Otherwise, if the note is not already held (`0x60`), a new
  note starts (`0x8002c`). With `0x61` set it rises from the last level
  written (`0x56`), over a span of `0x7FFF − level`; with it clear, from zero
  over `0x7FFF`.
- **What a start latches:**
  - the attack and decay lengths (`0x20`, `0x24`);
  - the previous note's level for the fade (`0x5A`);
  - the start level (`0x58`);
  - the sustain (`0x5C`);
  - both steps, as `(int)(16777216.0f / (float)length)` in float,
    clamping at both ends.

  **Correction:** the steps used to be integer `0x1000000 / length`.

The byte at `0x61` is cleared by the constructor. SubSynth sets it to 1 on
every amplitude envelope (`SubSynth::SubSynth`, `0xa048c`), and
`SetPolyphony` copies `+0xC2F9` into it on every filter envelope (`0x9ccdc`).
So amplitude envelopes never restart from zero: a note starts from wherever
the voice is.

### Attack

While `position < attackLength`, the first `min(attackLength, 32)` samples
fade in with the weight `(int)((float)position / (float)fadeLength * 32767)`.
The curves differ in what they fade from:

- **Linear and exponential** fade from the sample they wrote last, so the
  fade is recursive: `out = prev + (w * clamp16(shaped − prev) >> 15)`, with
  `prev` updated each sample and stored back to `0x56`.
- **Logarithmic** first copies `0x5A` into `0x56` and fades from that fixed
  level. Its shaped value is offset and then doubled with 16-bit wrap:
  `clamp16` of the offset value maps ≥ 0x8000 to −2 and < −0x7FFF to 2.

**Correction:** the fade used to blend from one fixed level for every curve.

After the fade:

| Curve | Value |
|---|---|
| Linear | `start + ((short)span * min(ramp >> 9, 0x7FFF) >> 15)`. The ramp is clamped, not wrapped to 16 bits as before. |
| Exponential | `start + sbfx15(s2 * span)`, where `s2 = (s * s >> 15) & 0xFFFF` and `s = (ramp >> 9) & 0xFFFF` |
| Logarithmic | `start + sbfx15((log(s) << 1) * span)`, where `s = (ramp >> 10) & 0xFFFF` |

Every value is clamped to ±0x7FFF.

### Decay and sustain

- **Span:** the decay's span uses the **current** sustain (`0x54`), while its
  floor uses the sustain latched at note-on (`0x5C`).
- **Plateau:** samples past the decay are filled with the **current**
  sustain, so a sustain edit shows up mid-note.
- **Correction:** both used the latched sustain before.
- **Ramp:** the decay ramp is `0xFFFE00 − step · (position − attackLength)`,
  taken as a signed 16-bit value and floored at zero before the curve is
  applied.

### Release

On the first gate-clear block after a held note, the release latches:

- its length (`0x28`);
- the level it starts from, the last sample written (`0x5E`);
- its step, computed the same way as the attack's.

It writes `max(0, sbfx15(shaped * level))` until the length runs out, then
zeros, and goes idle. While idle the block is cleared with `memset` and
nothing else happens.

### Empty blocks

The engine has no early return for `n = 0`. The latching still runs, and its
closing store reads the halfword before the buffer as the "last sample". This
component runs the latching too, but keeps the last level unchanged instead of
reading outside the buffer.

## Checked against the original

`GenerateValues`, `IsDone` and the resolved lengths are identical to the
3.2.0 (beta 7) armv7 library in 128 scenarios:

- **eight parameter sets:** every curve, mixed curves, SubSynth's minimums
  (10, 20, 200), zero-length segments, the 3.0625 s maximum times, and
  out-of-range values;
- **eight timelines:** held then released, released early, a retrigger in
  each segment, odd block sizes, never held, and a jump in position;
- **both values of `0x61`.**

## Known member offsets

| Offset | Meaning |
| --- | --- |
| 0x04, 0x08, 0x0C, 0x10 | attack, decay seconds; sustain 0..1; release seconds |
| 0x14, 0x18, 0x1C | resolved attack, decay, release lengths in samples |
| 0x20, 0x24, 0x28 | lengths latched for the note in flight |
| 0x2C, 0x30, 0x34 | minimum lengths, default 0x50 |
| 0x38 | last position seen, used to detect a retrigger |
| 0x3C, 0x40, 0x44 | attack, decay, release shape |
| 0x48, 0x4C, 0x50 | attack, decay, release step in the 24-bit domain |
| 0x54 | sustain as Q15 |
| 0x56 | last value written |
| 0x58 | level the attack starts from |
| 0x5A | level of the previous note, for the retrigger fade |
| 0x5C | sustain latched for the note in flight |
| 0x5E | level captured at note-off |
| 0x60 | note is held |
| 0x61 | a new note starts from the current level, and backward positions do not retrigger |
| 0x62 | envelope is idle |
