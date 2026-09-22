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

## Note-on: `SubSynth::PlayChannel`

`PlayChannel(SequencerKeyEvent const&, int channel, bool)` at `0x9bff0` is the
writer. The voice block sits at `SubSynth + channel * 0x60 + 0xCF0`, and the
control voltage at `+0xCFC` inside it, so every store below is given relative
to the control voltage.

The note frequency comes from `UIKeyboard::GetFrequencyFromNoteId`, which is a
plain table lookup — `table[noteId]` — into a float array in `.bss`. The code
that fills that table was not located, so this component takes hertz directly
rather than guess at the tuning.

| Offset | Written as | Evidence |
| --- | --- | --- |
| 0x00 | `flags \|= 3`; bit 2 copied from the key event | `orr r2, r2, #3` at 0x9c188; `bfi sb, r1, #2, #1` |
| 0x04 | `0` | `str r0, [r3, #0xd00]` |
| 0x08 | `0` | `str r2, [r3, #0xd04]` |
| 0x0C | `0` | `str r0, [r3, #0xd08]` |
| 0x10, 0x14 | `0` unless legato | conditional stores at 0x9c170 |
| 0x20 | `(uint)(hertz * 4096)` | `vmul` by the 4096.0 literal, `vcvt.u32` |
| 0x24, 0x28, 0x2C | `hertz * 4096` as floats | second `GetFrequencyFromNoteId`, same literal |
| 0x30 | glide length in samples, or 0 | `1.0 / samplePeriod` from a global, times the glide time |
| 0x3C | note id | `str r7, [sl, #0xd38]` with r7 from `event + 0x18` |
| 0x40, 0x44 | `0.0` | |
| 0x48, 0x4C | one machine-level float | both from `SubSynth + 0x3054` |
| 0x50 | another machine-level float | from `SubSynth + 0x3058` |
| 0x54 | hertz | first `GetFrequencyFromNoteId` result stored raw |
| 0x58 | `1 + tracking * (sqrt(hertz / 500) - 1)` | `vdiv` by the 500.0 literal, `vsqrt`, `vsub` 1.0, `vmla` |
| 0x5C | a float from `event + 0x34` | `str r2, [sb, #8]` |

Two of these settle earlier questions. **0x30 is not a pointer.** It is the
glide length in samples, and the "non-zero sends the oscillator down another
path" observation is simply the oscillator checking whether a glide is in
progress. And **0x58 is filter keyboard tracking**, with a square-root law and
a 500 Hz pivot: a note two octaves above 500 Hz opens the cutoff by 2×, one
two octaves below closes it to 0.5×, and at 500 Hz the tracking amount does
nothing at all. The tracking amount itself arrives through a virtual call on a
control object, which fits the `Filter Kb track` knob in SubSynth's control
list.

The three pitch fields are the glide mechanism. Without glide all three are
written equal. With glide (the branch at 0x9c220, entered when the previous
note is still sounding) the current pitch at 0x28 is left alone so the
oscillator can slide it towards the target — which is why the oscillator reads
0x28 twice per call.

## What is named, and what is not

Named from the writer and the readers together: `modulation` (0x0C),
`frequencyQ12` (0x20), `glideStartPitch` / `pitch` / `targetPitch` (0x24–0x2C),
`glideSamples` (0x30), `noteId` (0x3C), `frequencyHertz` (0x54) and
`filterCutoffScale` (0x58).

Still offset-named: 0x04, 0x08, 0x10–0x1C, 0x34, 0x38, 0x40–0x50, and 0x5C.
Several of those are zeroed on note-on and then owned by the oscillator, so
their roles will come out of the oscillator's pitch path. 0x5C is copied from
the key event and defaults to 1.0, which fits velocity; the header says so and
calls it a hypothesis.

## Host layout versus engine layout

The engine is 32-bit, so its pointers are four bytes and the block is exactly
0x60. A host struct with native pointers is a different size on a 64-bit build,
so the header carries both: a usable `ControlVoltage` with real pointers, and a
`controlVoltageLayout::Engine` mirror with 32-bit handles whose `static_assert`s
pin every recovered offset. The mirror is compiled, not commented, so the
offsets cannot drift silently.

## Still unmapped

The note table behind `GetFrequencyFromNoteId`; the two machine-level floats
copied to 0x48/0x4C and 0x50; what attaches the per-sample buffer at 0x0C
after note-on (the constructor and note-on both leave it null, so an LFO or
envelope stage sets it later); and the note-off path in `DoKeyOff`. The
oscillator's pitch prologue will now read in terms of these names.
