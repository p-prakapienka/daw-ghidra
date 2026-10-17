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
| 0x0C | int | 0 | filter once per block (the release position) |
| 0x10, 0x14, 0x18 | int | 0 | — |
| 0x1C | int | — | none of the known readers |
| 0x20 | int | 0 | — |
| 0x24 | float | 0.0 | both oscillator paths |
| 0x28 | float | 0.0 | both oscillator paths, twice per call |
| 0x2C | float | 0.0 | both oscillator paths |
| 0x30 | uint | 0 | oscillators; `SuperOscillator` in both entry points (the glide length) |
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
| 0x04 | `0` | `str r0, [r3, #0xd00]` — samples since note-on |
| 0x08 | `0` | `str r2, [r3, #0xd04]` — envelope position while gated |
| 0x0C | `0` | `str r0, [r3, #0xd08]` — envelope position after note-off |
| 0x10, 0x14 | `0` unless legato | conditional stores at 0x9c170 — phase of oscillator 0 and 1 |
| 0x20 | `(uint)(hertz * 4096)` | `vmul` by the 4096.0 literal, `vcvt.u32` |
| 0x24, 0x28, 0x2C | `hertz * 4096` as floats | second `GetFrequencyFromNoteId`, same literal |
| 0x30 | glide length in samples, or 0 | `1.0 / samplePeriod` from a global, times the glide time |
| 0x3C | note id | `str r7, [sl, #0xd38]` with r7 from `event + 0x18` |
| 0x40, 0x44 | `0.0` | |
| 0x48, 0x4C | one machine-level float | both from `SubSynth + 0x3054` — pitch sweep per oscillator |
| 0x50 | another machine-level float | from `SubSynth + 0x3058` — the sweep's per-sample decay |
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

The three pitch fields are the glide mechanism: 0x24 start, 0x28 target, 0x2C
current. Without glide all three are written equal. With glide (the branch at
0x9c220, entered when the previous note is still sounding) the start takes the
current value from 0x2C and the target takes the new note; the oscillator then
moves 0x2C once per block. An earlier version of these notes had 0x28 and 0x2C
the other way round.

## Per block: the end of `SubSynth::ProcessChannel`

After a voice's oscillators and filter have run, `ProcessChannel`
(`0x9cae4`–`0x9cb6c`) advances the counters and checks for the end of the note:

```
cv[0x04] += numSamples
cv[0x08] += numSamples
cv[0x0C] += numSamples * (gate ? 0 : 1)          ; release time only
if (envelope->IsDone(cv[0x0C])) {                ; vtable slot 0
    cv.flags bit 0 = 0                           ; voice inactive
    filter state at 0x81C..0x828 = 0             ; silence the filter
}
```

So 0x0C is **not** a modulation pointer, as the previous notes had it. It is the
release position: the number of samples since the gate closed, and exactly the
argument the ADSR's `IsDone` and release path expect. 0x08 is the matching
position while the gate is open, and the filter picks between them by the gate
bit when it asks the envelope for a value. 0x04 advances identically to 0x08
and is read by the oscillator; the two likely diverge on legato, which this
component has not reached.

The flag byte is therefore: bit 0 voice active, bit 1 gate open, bit 2 copied
from the key event.

## The port

`ControlVoltage` is ported to the original's structure (`dsp/ControlVoltage.h`).
It has no methods and no typeinfo, so in the engine it is a plain struct, and
here it is too:

- **Fields only,** with the engine's offsets and types, and defaults equal to
  those of SubSynth's constructor loop.
- **Names.** They are ours, in the engine's Hungarian style:

  | Offset | Member |
  |---|---|
  | 0x00 | `m_nFlags` |
  | 0x04 | `m_nSamplesSinceNoteOn` |
  | 0x08 | `m_nEnvelopePosition` |
  | 0x0C | `m_nReleasePosition` |
  | 0x10 | `m_anPhase[2]` |
  | 0x20 | `m_nFrequency` |
  | 0x24 | `m_fGlideStart` |
  | 0x28 | `m_fTargetPitch` |
  | 0x2C | `m_fCurrentPitch` |
  | 0x30 | `m_nGlideSamples` |
  | 0x3C | `m_nNoteId` |
  | 0x48 | `m_afPitchSweep[2]` |
  | 0x50 | `m_fPitchSweepDecay` |
  | 0x54 | `m_fFrequency` |
  | 0x58 | `m_fFilterCutoffScale` |
  | 0x5C | `m_fVelocity` |

  0x18, 0x1C, 0x34, 0x38, 0x40 and 0x44 keep their offsets as names
  (`m_n18`…`m_f44`).
- **No helper methods.** The old component's methods are gone, because the
  engine has that code elsewhere:
  - the note-on writes and key tracking are inline in `SubSynth::PlayChannel`;
  - the per-block advance is at the end of `ProcessChannel` (`SubSynthVoice`
    here);
  - gate tests are bit tests where they are used.

  `tests/support/Voice.h` reproduces note-on, glide, note-off and the
  advance for tests that drive an oscillator or a filter on their own.

**Correction: `0x1C` is not a pointer.** An earlier reading took
`ldr r2, [r1, #0x1c]` at `0x8de74` in `GenerateSignalLQ` as a read of the
control voltage. There, `r1` is the oscillator's octave modulation source
loaded from `[r8, #0x10]`, an LFO, and the load is that LFO's waveform,
compared with the default source's `0x07FFFFFF`. None of the known readers
touch the control voltage's `0x1C`.

So the block holds no pointers, and the struct is the engine's 0x60 bytes on
every host, 64-bit included. The separate `controlVoltageLayout::Engine`
mirror is gone: the struct itself carries the `static_assert`s on every
offset.

## Still unmapped

The note table behind `GetFrequencyFromNoteId`; the two machine-level floats
copied to 0x48/0x4C and 0x50; the note-off path, which reaches the gate bit
through `SlaveStopNote` rather than directly in `DoKeyOff`; and the legato
handling that keeps 0x10/0x14 when a note is held. The
oscillator's pitch prologue will now read in terms of these names.
