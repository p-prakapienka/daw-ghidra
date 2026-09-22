# ControlVoltage — reconstruction notes

`ControlVoltage` exports no methods, so nothing about it can be read directly.
Its layout was recovered from two sides: every function that takes one, and the
block in `SubSynth::SubSynth` that initialises one per voice.

## Readers

```
FixedPointSVFilter::ProcessCV(ControlVoltage*, int*, uint, int)
FloatSVFilter::ProcessCV(ControlVoltage*, int*, uint, int)
Oscillator::GenerateSignalLQ(ControlVoltage*, int, int*, uint, int*, int, float)
Oscillator::GenerateSignalHQ(...)
SuperOscillator::Trigger(ControlVoltage*)
SuperOscillator::GenerateStereoSignal(ControlVoltage*, int*, uint, float)
```

Disassembling each and following the register the pointer arrives in gives the
set of offsets that are read, and whether each is read as a float, an integer or
a pointer.

One thing worth stating plainly: in `SubSynth::ProcessChannel` the oscillator
and the filter are handed **the same pointer** — both calls load it from
`[sp, #0x24]`. So there is one block per voice shared by its oscillators and its
filter, not one per subsystem.

## Size and defaults

The constructor initialises these blocks in a loop with a **stride of 0x60**, so
the struct is 96 bytes. In the same loop the ADSR objects stride by 0x64.

Every field the loop touches:

| Offset | Width | Default | Read by |
| --- | --- | --- | --- |
| 0x00 | byte | low three bits cleared | `FloatSVFilter` |
| 0x04 | float | 0.0 | both oscillator paths, `SuperOscillator` |
| 0x08 | int | 0 | — |
| 0x0C | pointer | null | oscillator every sample; filter once per block |
| 0x10, 0x14, 0x18 | int | 0 | — |
| 0x1C | pointer | — | standard-quality oscillator |
| 0x20 | int | 0 | — |
| 0x24 | float | 0.0 | both oscillator paths |
| 0x28 | float | 0.0 | both oscillator paths, twice per call |
| 0x2C | float | 0.0 | both oscillator paths |
| 0x30 | pointer | null | oscillators; `SuperOscillator` in both entry points |
| 0x34 | int | 0 | — |
| 0x38 | int | **1** | — |
| 0x3C | int | 0 | — |
| 0x40, 0x44, 0x48, 0x4C | float | 0.0 | — |
| 0x50 | float | 0.0 | oscillator, several times per call |
| 0x54 | float | 0.0 | `SuperOscillator::Trigger` |
| 0x58 | float | **1.0** | filter, folded into its cutoff base |
| 0x5C | float | **1.0** | — |

The two defaults that are not zero are worth noticing. `0x38` is the only
integer set to one, and the pair at `0x58`/`0x5C` default to unity — which is
what a scale factor looks like when a voice has not modulated anything yet.

The float constants were confirmed from the constructor: `1.0` is a `vmov.f32`
immediate, and `0.0` is the literal at `0xa03f0`. The same two registers
initialise the voice's ADSR as attack 0, decay 0, sustain 1, release 2.0, which
is a sane default envelope and corroborates the reading.

## What is named, and what is not

Only two fields have enough evidence to name:

- **0x0C** is a pointer to a per-sample array of ints. The filter loads it once
  per control block and indexes it by the sample offset; the oscillator reads it
  on every sample. It is the per-sample modulation buffer.
- **0x58** is multiplied into the filter's cutoff base once per control block.
  `StateVariableFilter::setCutoffWithVoltage` applies it, and at the default of
  1.0 the cutoff is unchanged.

**0x28** is named `pitch` on weaker evidence: both oscillator paths read it
twice per call and multiply it by the caller's float argument, which is where
pitch would enter. Treat the name as a hypothesis.

Everything else keeps its offset as its name. Guessing at roles here would be
cheap and would mislead whoever wires the next component.

## Host layout versus engine layout

The engine is 32-bit, so its pointers are four bytes and the block is exactly
0x60. A host struct with native pointers is a different size on a 64-bit build,
so the header carries both: a usable `ControlVoltage` with real pointers, and a
`controlVoltageLayout::Engine` mirror with 32-bit handles whose `static_assert`s
pin every recovered offset. The mirror is compiled, not commented, so the
offsets cannot drift silently.

## Still unmapped

The writers. This pass establishes the shape, the defaults and who reads what,
but not what puts values into most fields during playback. `SubSynth::PlayChannel`
writes to a few of these offsets and is the obvious next thing to read, along
with the 16-sample control block in the filter that consumes 0x0C and 0x58
together.
