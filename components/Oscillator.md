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

`10.7666` is `44100 / 4096`, so exactly one cycle lands in the table. The level
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

## Derived rather than read

The phase increment for a given frequency is **not** taken from the engine's
float setup path, which is entangled with the `ControlVoltage` modulation this
component does not model. It is derived from the index arithmetic above: one
cycle is `4096 << 12` units, so `increment = 2^24 * hertz / 44100`. That is the
only mapping consistent with the table geometry, but it is a derivation, not a
reading.

The noise generator is also not the engine's. `whitenoise` takes a `float*` and
its source of randomness is not recoverable from the binary, so this component
uses a deterministic generator at the same level. The waveform is different; the
character and amplitude are not.

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

Playback still has to choose which of the eight copies to read. The rule in
this component — widest band whose top harmonic is still ≤ Nyquist — is
**derived, not read**. The engine has a picker in the `GenerateSignalHQ`
prologue, mixed with state this component does not model. The tables, the
caps, and the interleaved layout were read from the binary; the function that
maps a pitch onto a band index was not.

## Derived rather than read

Two things in this component are derivations, marked here so they are not
mistaken for readings.

The phase increment for a frequency comes from the index arithmetic — a cycle
is `4096 << 12` units, so `increment = 2^24 * hertz / 44100` — not from the
engine's float setup path, which is entangled with `ControlVoltage`.

The band chosen for a pitch is likewise derived: the widest band whose top
harmonic still fits below Nyquist. The engine computes its index somewhere in
the `GenerateSignalHQ` prologue, from state this component does not model. The
selection rule here is the one the harmonic limits imply, and it behaves
correctly, but it has not been read out of the binary.

The noise source is a local deterministic generator, because the engine's
randomness cannot be recovered from a binary. Only its length and level match.

## Not in this pass

The modulation modes selected by `SetModulationMode`, the custom wavetables at
type 7 and 8, and everything reached through `ControlVoltage` — pitch
modulation, sync, the per-voice fields.

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
