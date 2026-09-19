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
samples = clamp((uint)(seconds * 44100.0f), minSamples, 220500)
sustainQ15 = (short)(sustain * 32767.0f)
```

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

- The second argument to `GenerateValues` is the gate: non-zero holds the note,
  zero runs the release. The first argument is the sample offset since the last
  gate change.
- Segment lengths, the step sizes, and the sustain level are latched when a note
  starts and when it is released (offsets `0x20`, `0x24`, `0x28`, `0x48`, `0x4C`,
  `0x50`, `0x5C`, `0x5E`). Editing a parameter mid-note therefore cannot change
  the segment already in flight.
- The first `min(attackSamples, 32)` samples of an attack crossfade from
  whatever level the previous note left behind, using a linear blend
  `(position / fadeLength) * 32767`. This is what stops a retrigger clicking.
  The float division in that blend is in the original code as well.
- The release multiplies its shaped ramp by the level captured at note-off, so
  releasing during attack or decay starts from wherever the envelope was.

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
| 0x62 | envelope is idle |

## What this reconstruction does not claim

The implementation in `ADSR.cpp` is a readable reconstruction, not a proven
bit-exact port. The shapes, constants, clamps, latching and retrigger fade all
match what the disassembly does, and the measured output is correct at the
segment boundaries:

```
curve      attack midpoint   attack end   decay end   release midpoint
Linear          16374          32749        16383          8191
Exponential      8182          32731        16383          4095
Logarithmic     25195          32525        16383         12601
```

with `sustain = 0.5` giving `16383` exactly. But the original interleaves its
loops in ways a compiler chose, and a sample-for-sample comparison against the
engine has not been run. The most likely place for a one-sample difference is
the release, where the original decrements the ramp before emitting in the
linear case and after it in the other two.
