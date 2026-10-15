# Oscillator — reconstruction notes

Source: `libcaustic.so`, ARMv7. The oscillator SubSynth builds two of.

```
GenerateWavetables()                                     0x8f54c   1064 bytes
sinewave(short*, uint, float, float)                     0x8f34c    136
squarewave(short*, uint, float, float)                   0x8f0e8    304
trianglewave(short*, uint, float, float)                 0x8f218    308
sawtooth(short*, uint, float, float)                     0x8f434
whitenoise(float*, uint, float)
Oscillator::SetOscillatorType(Oscillator::OscillatorType) 0x8dd40    228
Oscillator::GenerateSignal(...)                          0x8f024     64
Oscillator::GenerateSignalLQ(...)                        0x8de2c   2016
Oscillator::GenerateSignalHQ(...)                        0x8e60c   2584
```

The static tables are exported by name, which removes all guesswork about what
each one is: `m_psSinewaveTable`, `m_psTrianglewaveTable`, `m_psSawtoothTable`,
`m_psSquarewaveTable`, `m_psHQSawtoothTable`, `m_psHQSquarewaveTable`,
`m_pasWhitenoiseTable`, and `m_nWaveTableRefCount`.

## Table construction

`GenerateWavetables` allocates four blocks of `0x2000` bytes — 4096 shorts each
— plus `0x15888` bytes for noise, which is 44100 shorts, one second at the
engine's rate. It runs once; the constructor calls it only when the refcount is
zero and then increments it, so every oscillator in the engine shares one set.

Each generator takes a frequency and a level:

```
squarewave(table, 4096, 10.7666f, 0.2f)
sinewave  (table, 4096, 10.7666f, 0.3f)
sawtooth  (table, 4096, 10.7666f, 0.3f)
trianglewave(table, 4096, 10.7666f, 0.4f)
ASwhitenoise(table, 22050, 0.4f)
```

`10.7666` is close to `44100 / 4096`, so one cycle lands in the table. It is the
literal `10.7666f` (`0x412C43FE`, loaded with `movw`/`movt`), not the exact
quotient `10.7666015625` (`0x412C4400`). **Correction:** this component used
the exact quotient, which moved five sine entries by one (entry 1961 is 1308,
not 1307) and every sine output that read them. The square, triangle and
sawtooth tables come out the same either way. The level
is multiplied by `32767`, giving peaks of 9830 for the sine and sawtooth, 6553
for the square, and 13106 for the triangle.

The noise table is separate: `0x15888` bytes is 22050 **ints**, half a second,
written by `ASwhitenoise(int*, unsigned int, float)` at level 0.4 — not shorts,
and not one second.

`sinewave` is `sin(i * frequency * 2π / 44100) * level * 32767`, computed in
double. The other three are **not band limited**: the sawtooth is a bare
`2i/n - 1` ramp, the square flips sign every `(44100 / frequency) / 2` samples,
and the triangle walks up and down at a constant step. The separate HQ tables
exist precisely because these alias.

## Type switch

`SetOscillatorType` is a nine-entry jump table storing a table pointer at
`+0x28` and an HQ flag at `+0x34`:

| Value | Table | HQ |
| --- | --- | --- |
| 0 | `m_psSinewaveTable` | no |
| 1 | `m_psTrianglewaveTable` | no |
| 2 | `m_psSawtoothTable` | no |
| 3 | `m_psHQSawtoothTable` | yes |
| 4 | `m_psSquarewaveTable` | no |
| 5 | `m_psHQSquarewaveTable` | yes |
| 6 | none — noise, handled directly in the generator | no |
| 7 | instance pointer at `+0x2C` | no |
| 8 | instance pointer at `+0x30` | no |

Values 7 and 8 are the two custom wavetables, which matches SubSynth storing two
660-entry custom waveforms per preset. `GenerateSignal` itself is a 64-byte
dispatcher: it reads the HQ flag and tail-calls `GenerateSignalHQ` or
`GenerateSignalLQ`.

## Playback

The inner loop of `GenerateSignalLQ` reads the phase accumulator, takes the
table index and its successor, and interpolates:

```
ubfx  r1, r4, #0, #0xc        ; index      = phase >> 12, masked to 12 bits
add   r4, r4, #1              ; successor
ubfx  r4, r4, #0, #0xc
ldrh  r1, [r0, r1, lsl #1]
ldrsh r0, [r0, r4, lsl #1]
rsb   r4, r1, #0
sxtah r0, r0, r4              ; difference
```

So the phase carries 12 fractional bits below a 12-bit index, and the table is
read with linear interpolation. A full cycle is therefore `2^24` phase units.

## The pitch path

`GenerateSignalLQ`'s signature is
`(ControlVoltage* cv, int index, int* out, uint n, int* fmInput, int fmAmount, float bend)`,
and its prologue computes a base increment once per block:

```
pitch = cv.glideSamples ? glide(cv) : cv[0x28]            ; Q12 hertz
base  = pitch * bend * this[0x04] * (4096 / 44100)       ; literal at 0x8e1e0
```

`4096 / 44100` applied to Q12 hertz gives `2^24 * hertz / 44100` — the formula
the earlier notes had derived from the index arithmetic. It is now read.

`this[0x04]` is **a frequency multiplier, not a level**, as the first patch had
it: `SubSynth::SetOscillator2PitchOffset` writes oscillator 2's `+0x04`. There
is no per-oscillator output level at all; the tables carry theirs.

`bend` is `SubSynth + 0xC1EC`, written by `OnChildModified` as
`powf(2, semitones / 12)` from the `Bend` control.

### Glide

When `cv[0x30]` is non-zero the prologue interpolates, once per block:

```
t = min(1, cv.samplesSinceNoteOn / cv.glideSamples)
cv[0x2C] = cv[0x24] + t * (cv[0x28] - cv[0x24])        ; start + t(target - start)
if (t >= 1) cv.glideSamples = 0
```

So `0x28` is the target and `0x2C` the current pitch (the previous patch had
those two swapped), and the glide is **linear in hertz**, not in pitch: halfway
from 220 to 440 is 330 Hz, not 311.

### Per sample

```
index    = (phase >> 12) + this[0x0C] + (phaseMod[i] >> 12)
fraction = (phase & 0xFFF) << 3                          ; Q15
out[i]   = clampQ24((table[index] + ((table[index+1] - table[index]) * fraction >> 15)) << 9)

vibrato  = 1 + vibratoMod[i] * k * 0.1
fm       = 1 + q24(fmInput[i] * q24(fmAmount * (1 + fmDepthMod[i]))) * k
steps    = 2^((floor(4 * octaveMod) * 12 + floor(12 * semitoneMod)) / 12)
phase   += (int)((1 + cv.sweep[index]) * base * vibrato * fm * steps)
cv.sweep[index] *= cv[0x50]
```

`k` is the literal at `0x8e1e8`, `0x33800001`: the float nearest to
`1 / 16777215`, one ulp above `2^-24`. **Correction:** this component used
exactly `2^-24`. The difference is invisible for vibrato and FM, but it moves
the octave and semitone inputs across `floorf` boundaries. A negative input
that lands exactly on a step comes out one step lower with the original: a
semitone input of `−0x800000` (−0.5) is `floorf(−6.0000004) = −7` semitones,
where `2^-24` gave `−6`. Positive inputs are unaffected.

Several things fall out of that:

- **Output is Q24.** The interpolated Q15 value is shifted left by nine and
  clamped to `±0xFFFE00`. The first patch emitted Q15; that was wrong for every
  consumer downstream.
- **The phase lives in the control voltage**, at `0x10 + 4 * index`, and is
  masked to 24 bits when written back. Two oscillators share a voice by using
  slots 0 and 1.
- **The increment is recomputed in float every sample** and truncated to int.
- **`0x48`/`0x4C` are a per-oscillator pitch sweep** that multiplies the
  increment by `1 + sweep` and decays by `cv[0x50]` each sample. Both start
  from the same machine-level value on note-on.
- **Five modulation inputs**, each a pointer on the oscillator to a source whose
  `+0x0C` is a per-sample Q24 array: vibrato at `+0x00` (±10%), phase at
  `+0x08`, octave at `+0x10` and semitone at `+0x14` (both floored to whole
  steps with `floorf`), and FM depth at `+0x18`. `ConnectLFOs` aims these at a
  default source or an LFO. The octave and semitone ratio is only recomputed
  when either source is not the default, tested by a sentinel `0x07FFFFFF` in
  the source.
- **Frequency modulation is part of mode 0**, driven by the fifth and sixth
  arguments — another oscillator's output and an amount.
- `this[0x0C]` is a fixed phase offset in table entries, which fits the
  `Osc2 Phase` control.

## Modulation modes

`SetModulationMode` stores to `+0x24`; SubSynth drives it from "Osc1 Mod Mode"
(`SetOscillator1ModulationMode` also keeps a copy at SubSynth `+0x2F88`, which
the mode button cycles 0 → 1 → 2 → 0). The prologue branches on it: 0 runs the
path above, 1 jumps to `0x8e3e8`, 2 to `0x8e19c`, and any other value stores
the phase back **unchanged** and returns without writing the output. The HQ
generator branches the same way (`0x8ea40`, `0x8ec9c`) with the same bodies
around its band-limited read.

All three modes compute the same modulator product per sample,

```
product = q24(input[i], q24(1 + depth[i], amount))     ; depth from osc +0x18
```

and differ only in where it goes:

| Mode | Name here | Where the product goes |
|---|---|---|
| 0 | `Frequency` | increment × (1 + product·2⁻²⁴) |
| 1 | `Phase` | table index from `(phase + product) >> 12`; the fraction still comes from the unmodulated phase |
| 2 | `Amplitude` | finished Q24 sample × (product + 0xFFFFFF) >> 24, a 64-bit multiply |

Modes 1 and 2 leave the increment without the FM factor. In SubSynth the
input is oscillator 2's buffer and the amount is "Osc1 Mod Amount" × 2.5; see
`components/SubSynthVoice.md`. With oscillator 2 switched off the input is
zeros, so mode 2 still scales the sample by 0xFFFFFF/2²⁴.

Automation maps the control's 0..1 value to a mode by thresholds: below 0.333
is 0, above 0.666 is 2, otherwise 1 (`OnSequencerControlEvent`, literals at
`0x9e2a0` and `0x9e2a4`).

## Noise

`ASwhitenoise` does not use `lrand48`, though the library imports it. It is a
fixed-seed generator:

```
a = 0xEFCDAB89, b = 0x67452301            ; the MD5 initial constants
out[i] = (int)((int32)a * level * 2^-31 * 16777215)
b ^= a; a += b
```

so the noise table is **exactly reproducible**, and this component now
generates it bit for bit. At playback the engine copies consecutive values
straight to the output — no pitch, no interpolation, no scaling — and wraps the
position by `(position + n) mod (22050 - n)` so the next block's copy always
fits.

## Band-limited tables

The two HQ tables are `0x10000` bytes each, 32768 shorts, which is
`8 * 4096` — eight band-limited variants of one cycle. The harmonic limit per
band is a `.rodata` array at `0x270198`:

```
512, 337, 169, 84, 42, 21, 10, 6
```

A naive saw or square has harmonics that go on forever. At high pitch those
partials sit above Nyquist (22050 Hz at this engine's rate) and fold back as
wrong notes — aliasing. HQ avoids that by baking eight copies of the same
cycle, each poorer in overtones, and playing the richest copy whose leftover
harmonics still fit.

Each copy **is a sum of sines**, and the builder adds them one by one. There is
no FFT and no closed form. For each band, harmonic `h` adds
`(1/h) * sine[(i * h) mod 4096]` into a 4096-float scratch, stepping `h` by 1
for the sawtooth and by 2 for the square, so the square keeps only odd
harmonics. Band 0 of the sawtooth therefore walks `h = 1 .. 511` — hundreds of
sines × 4096 samples. That is cheap only because `GenerateWavetables` runs
once; `GenerateSignalHQ` never sums sines. It reads the baked table.

The band is then normalised against its own peak, **negated**, and scaled by
the same level as the plain table — 0.3 for the sawtooth, 0.2 for the square.

The eight bands are stored **interleaved**, not one after another. The builder
writes with a 16-byte stride, and the inner loop of `GenerateSignalHQ` reads

```
add r0, r4, r0, lsl #3        ; band + index * 8
lsl r0, r0, #1                ; as shorts
ldrh  r4, [r3, r0]
```

so element `b` of phase step `i` is at `table[i * 8 + b]`. All eight bands of
one phase step sit in the same cache line, which is the point.

`GenerateSignal` picks HQ or LQ from the flag at `+0x34`, which
`SetOscillatorType` sets only for types 3 and 5.

### Choosing and blending bands

`GenerateSignalHQ` asks `WaveTables::FrequencyToWaveTable(float, float*)`
(`0x15c398`) once per block. The hertz it passes are computed from the block's
increment as `base / (4096 / 44100) * (1 / 4096)`, in that order. The function
compares against seven thresholds (`.rodata` `0x15c48c`…), roughly the Cs of
octaves 1 to 7:

| Hertz from | Band | Fraction |
|---|---|---|
| 2093 | 6 | (f − 2093) / 2093 |
| 1046 | 5 | (f − 1046) / 1046 |
| 523 | 4 | (f − 523) / 523 |
| 261 | 3 | (f − 261) / 261 |
| 130 | 2 | (f − 130) / 130 |
| 65 | 1 | (f − 65) / 65 |
| below 65 | 0 | (f − 33) / 33 |

The fraction is clamped to 0..1, and the prologue turns it into a Q14 weight,
`(int)(fraction * 16383)`. Each sample then reads **two** bands, the returned
one and the next narrower one, and blends them:

```
lower  = sat16(b[i][band]   + (sat16(b[i+1][band]   - b[i][band])   * frac >> 15))
upper  = sat16(b[i][band+1] + (sat16(b[i+1][band+1] - b[i][band+1]) * frac >> 15))
value  = lower + (weight * sat16(upper - lower) >> 15)
out[i] = clampQ24(value << 9)
```

where `sat16` saturates to ±0x7FFF as the LQ difference does. Band 7 is only
ever the upper half of a blend.

**Correction:** this component used to pick a single band, the widest whose
top harmonic stayed below Nyquist, a rule derived rather than read. The engine
crossfades between neighbouring bands instead, so its HQ output differs from
any single-band read.

## Checked against the original

Every table `GenerateWavetables` builds is identical. So is `GenerateSignal`
for every type except the custom ones (7 and 8) in 189 scenarios. Each
scenario ran three consecutive blocks and covered:

- all three modulation modes;
- pitch ratio, phase offset, glide, pitch sweep in both slots, bend, and very
  low and high notes;
- each modulation input on its own, including FM with depth.

Each block compared the output and the control voltage afterwards, on the
3.2.0 (beta 7) armv7 library.

## Not in this pass

The custom wavetables at types 7 and 8.

## Measured

```
sine      [0]=0  [1024]=9830  [2048]=0  [3072]=-9830
sawtooth  [0]=-9830  [2048]=0  [4095]=9825
square    [0]=-6553  [2047]=-6553  [2048]=6553
triangle  [0]=0  [1024]=13106  [2048]=9  [3072]=-13096
```

A 441 Hz sine gives 440 rising zero crossings in one second, peak 9830, and an
RMS to peak ratio of 0.707.

A 2 kHz sawtooth, measured at frequencies that are not its harmonics:

```
          1400 Hz   3100 Hz
plain        48        36
band limited  0         0
```

and a 440 Hz band-limited square has no second harmonic, with its third at a
third of the fundamental.
