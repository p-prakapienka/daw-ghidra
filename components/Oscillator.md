# Oscillator — reconstruction notes, standard quality path

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
trianglewave(table, 4096, 10.7666f, 0.3f)
```

`10.7666` is `44100 / 4096`, so exactly one cycle lands in the table. The level
is multiplied by `32767`, giving peaks of 9830 for everything except the square
at 6553.

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

## Not in this pass

`GenerateSignalHQ` and the band-limited tables, the modulation modes selected by
`SetModulationMode`, the custom wavetables, and everything reached through
`ControlVoltage` — pitch modulation, sync, the per-voice fields. Those are the
second patch.

## Measured

```
sine      [0]=0  [1024]=9830  [2048]=0  [3072]=-9830
sawtooth  [0]=-9830  [2048]=0  [4095]=9825
square    [0]=-6553  [2047]=-6553  [2048]=6553
triangle  [0]=0  [1024]=9830  [2048]=9  [3072]=-9820
```

A 441 Hz sine gives 440 rising zero crossings in one second, peak 9830, and an
RMS to peak ratio of 0.707.
