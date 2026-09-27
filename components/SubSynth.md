# SubSynth machine

Recovered from `libcaustic.so` (ARMv7):

| Function | Address | Size |
|---|---|---|
| `SubSynth::PlayNote(SequencerKeyEvent const&)` | `0x9c2e8` | 240 |
| `SubSynth::PlayChannel(SequencerKeyEvent const&, int, bool)` | `0x9bff0` | 760 |
| `SubSynth::StopNote` | `0x9c3d8` | 104 |
| `SubSynth::StopAllNotes` | `0x9c440` | 72 |
| `SubSynth::AddPendingNote` | `0x9dae8` | 172 |
| `SubSynth::ProcessPendingNotes` | `0x9e2b0` | 300 |
| `SubSynth::OnSequencerKeyEvent` | `0x9dd00` | 64 |
| `SubSynth::SlavePlayNote` / `SlaveStopNote` | `0x9db94` / `0x9dcc4` | 60 / 60 |
| `SubSynth::PreviewKeyOn` / `PreviewKeyOff` | `0x9dbd0` / `0x9ddc0` | 232 / 300 |
| `SubSynth::DoKeyOff` | `0x9dd48` | 120 |
| `SubSynth::ReleaseSustainedNotes` | `0x9def8` | 80 |
| `SubSynth::OnMIDIFocusLost` | `0x9df48` | 152 |
| `SubSynth::SetPolyphony(int)` | `0x9cc48` | 388 |
| `SubSynth::Process(int*, unsigned)` | `0xa2ec4` | 344 |
| `UIKeyboard::GetFrequencyFromNoteId(int)` | `0x16ff38` | 24 |

## Event flow

```
sequencer / pattern editor ─► OnSequencerKeyEvent ─┬─ type 0 ─► SlavePlayNote ─┐
                                                   ├─ type 1 ─► SlaveStopNote ─┤
                                                   └─ other ─► patternEditor vtable +0xD4
live keyboard ─► PreviewKeyOn ─► (scale filter) ─► SlavePlayNote              │
               PreviewKeyOff ─► (scale filter) ─► sustain list or DoKeyOff ─► SlaveStopNote
                                                                               ▼
                                                                  AddPendingNote (mutex)
Process ─► ProcessPendingNotes: type 0 ─► PlayNote ─► PlayChannel
                                type 1 ─► StopNote
```

Nothing plays a note directly. Every path queues, and the queue drains at the
top of `Process`, so note changes land on block boundaries.

`SlavePlayNote`/`SlaveStopNote` also light or clear the note on the
on-screen keyboard and in the pattern editor (`[+0xC2B0] + note + 0x99F4`).
`PreviewKeyOn` and `DoKeyOff` additionally record into the sequencer or the
pattern editor when the transport's flags at `+0xEC0`–`+0xEC3` say so. Events
with flag bit 2 set go through `Scale::FilterKeyEvent` first. None of that is
modelled.

## The queue

- 64 events of 0x38 bytes at `+0x12FC`, right after the sixteen control
  voltages. The count is at `+0x20FC`.
- A pthread mutex is at `+0xC2B4`, with a byte at `+0xC2B8` set while it is
  held.
- `AddPendingNote` tests `count > 63` **before** locking and drops the event.
- `ProcessPendingNotes` clears `+0xC2FA`, then handles each entry by type.
  Type 0 calls `PlayNote`. Type 1 calls `StopNote` and sets `+0xC2FA`. Any
  other type is skipped.
- Each consumed entry is reset to the empty event described below, and the
  count goes to zero.

## `SequencerKeyEvent`

| Offset | Reset value | Read by |
|---|---|---|
| 0x00 | −1 | — |
| 0x04 | −1 | the type switch: 0 on, 1 off |
| 0x08 | 1 000 000.0 | the glide computation (start) |
| 0x0C | 0 | the glide computation (duration) |
| 0x18 | 0 (u16) | note id: every note path |
| 0x1A | 0 (u16) | bit 0 slide, bit 2 live input |
| 0x34 | 1.0 | velocity, into `cv[0x5C]` |

0x10, 0x14, 0x1C (u16), 0x20 (u8) and 0x24–0x30 are zeroed by the reset and
read by nothing SubSynth does.

## `PlayNote`

1. For all sixteen slots: if the gate is open, `cv.noteId` equals the event's
   note and the sustain byte `+0x2F04` is clear, return. A held note is not
   played twice unless the pedal is down.
2. Take the first slot below the polyphony whose active bit is clear, and call
   `PlayChannel(event, v, false)`.
3. Otherwise find the slot with the greatest `samplesSinceNoteOn`, as an
   unsigned `movhi` comparison starting from 0, and call
   `PlayChannel(event, v, true)`. If every slot reads 0, which happens when
   all of them were started in the same queue batch, the note is dropped.

## `PlayChannel`

`slide = mono ? (event.flags & 1) : 0`, where mono is `+0xC2F8`.

Always:

- `noteId`, `frequencyHertz`, `frequencyQ12` and `velocity`;
- `filterCutoffScale = 1 + tracking·(√(hz/500) − 1)`, with tracking read from
  the control at `+0xC25C`;
- flag bit 2 from the event;
- `samplesSinceNoteOn` and `releasePosition` zeroed;
- both sweeps from `+0x3054`, the sweep decay from `+0x3058`;
- `0x40` and `0x44` zeroed.

Then:

```
if +0xC2F9:  retrigger = !(+0xC2FA && stealing)
else:        retrigger = !slide
if retrigger:
    envelopePosition = 0
    if !slide && !stealing: phase[0] = phase[1] = 0
flags |= active | gate
targetPitch = hz · 4096
if !mono: return                               ; glide fields untouched
if slide:
    glideStartPitch = currentPitch
    beats = (event.start > last.start)
          ? max(1/12, last.start + last.duration − event.start)
          : 1/12
    glideSamples = (uint)(beats · (float)(uint)(1.0 / beatsPerSample))
else:
    currentPitch = glideStartPitch = targetPitch; glideSamples = 0
last = event                                   ; 0x38 bytes to +0xC2BC
```

Consequences:

- **A slide glides for as long as the two notes overlap**, with a floor of a
  twelfth of a beat. A slide that starts at or before the previous note's
  start always takes the floor.
- **A monophonic line is phase-continuous.** Its only voice is always busy,
  so every note after the first steals it, and a stolen voice keeps its
  phase. The envelope still restarts unless the note slides.
- **`frequencyHertz` is looked up three times** (for `0x54`, `0x20` and
  `0x28`); the results are identical.

`ControlVoltage::noteOn` is equivalent to the mono, non-sliding, non-stealing
case.

## Note frequencies

`GetFrequencyFromNoteId` indexes a 128-float table at `.bss` `0x349cc8`
without a bounds check. `UIKeyboard::UIKeyboard` fills it once, behind a
static flag, at `0x16ede0`:

```
for n in 0..127: table[n] = powf(2, (n − 69.0) / 12.0) * g_fConcertA
```

`g_fConcertA` is `.data` `0x3438f4`, initialised to 440.0 and reached through
the GOT. That settles the note table that had been left open.

## Release, sustain and focus

- `StopNote` looks only below the polyphony. For each gated slot with the
  note it zeroes `releasePosition` and clears the gate.
- `StopAllNotes` does the same for all sixteen slots, whatever the note,
  then resets the on-screen keyboard. `ShutUp` calls it and then pattern
  editor vtable `+0xD8`.
- The sustain byte `+0x2F04` is written by control `0x401` in
  `OnSequencerControlEvent`: set at 0.5 and above. Writing 0 calls
  `ReleaseSustainedNotes`.
- `PreviewKeyOff` stores the event in a 64-entry list at `+0x2100` (count
  `+0x2F00`) when the pedal is down and the list is not full, skipping notes
  already listed. Otherwise it calls `DoKeyOff`.
- `ReleaseSustainedNotes` runs `DoKeyOff` on each stored event and empties
  the list.
- `OnMIDIFocusLost` closes the gate and zeroes the release position of every
  slot with both the gate and flag bit 2 set. It then clears the pedal and
  runs `ReleaseSustainedNotes`.

## `SetPolyphony`

| Argument | Voices | Mono (`+0xC2F8`) | Clears `+0xC2F9` |
|---|---|---|---|
| 0 | 1 | yes | yes |
| 2 | `+0xC2F9 ? 1 : 2` | `+0xC2F9` | yes |
| < 0 | 1 | yes | no |
| other | min(n, 8) | n == 1 | yes |

The per-slot reset is in `components/SubSynthVoice.md`. `SupportsGlide()`
returns `+0xC2F8`, and `Draw` switches its polyphony text on `+0xC2F9`.

Nothing in the library writes a non-zero value to `+0xC2F9`. The constructor
and `SetPolyphony` clear it, and `Serialize` saves `polyphony − [+0xC2F9]`.
So its branches in `SetPolyphony` and `PlayChannel`, and the `+0xC2FA` flag
they consult, are dead in this build. It looks like a legato mode that was
removed from the UI but not from the code.

Callers:

- the polyphony `−`/`+` buttons (controls `+0xC280`/`+0xC284`) pass
  `current ∓ 1`;
- the OSC handler parses `poly`, clamps it to 0..16 and passes it on;
- `ResetMachine` passes 4;
- `Serialize` passes 8 for old files, or the saved value.

## `Process`

```
position = (uint)((double)(uint)(1.0 / T[0xE20]) * beats)
patternEditor(+0xC2B0)->vtable[3](out, n)      ; drives the sequencer
ProcessPendingNotes()
lfo1.position = lfo2.position = position
lfo1.GenerateInternal(n); lfo2.GenerateInternal(n)
for v < polyphony: if cv[v].active: ProcessChannel(v, out, n)
UIVUMeter::Process(+0xC294, out, n, 1)
```

`T` is the `OutputPanel` global. `beats` is its double at `+0xE50` while the
flag at `+0xEC0` is set and at `+0xE70` otherwise, whenever `+0xEC1` is set;
with `+0xEC1` clear it is `+0xE88`. They look like song and pattern
positions, but that has not been traced further.

`OutputPanel::UpdateBPM` writes `T[0xE20] = bpm / 60.0 / 44100.0` as a
double, from the float at `+0xDF8`: beats per sample. The truncation to
`uint` samples per beat happens both here and in the glide length. The
transport's default tempo is not traced. The component starts at 120, the
LFO constructor's default.

`OnBPMChanged` passes the tempo to both LFOs' `SetBPM`. `SetLFO1Target` and
`SetLFO2Target` store to `+0x3720`/`+0x3760` and tail-call `ConnectLFOs`.

## Controls read along the way

- `SetOscillator2CentsMode(mode)` stores the mode at `+0x2F84`. Mode 0 sets
  oscillator 1's pitch ratio (`+0x2F0C`) to 1.0. Any other mode sets it to
  `powf(2, −cents / 1200)`, with cents read from the control at `+0xC220`. So
  cents mode detunes oscillator 1 down by the cents amount, rather than
  moving oscillator 2.
- `SetOscillator2PitchOffset(ratio)` writes oscillator 2's `+0x04`.
- `SetOscillator2PhaseOffset(value)` writes `(int)(value · 4096)` table
  entries to oscillator 2's `+0x0C`.
- `OnSequencerControlEvent` handles three IDs itself:
  - `0x400` is cents mode (value > 0.5);
  - `0x401` is the sustain pedal;
  - `0x1E` is the modulation mode, using the 0.333/0.666 thresholds in
    `components/Oscillator.md`.

  Every other ID is forwarded to the matching UI control.

## Machine layout additions

| Offset | Contents |
|---|---|
| `+0x12FC` | pending queue, 64 × 0x38 |
| `+0x20FC` | pending count |
| `+0x2100` | sustained note-offs, 64 × 0x38 |
| `+0x2F00` | sustained count |
| `+0x2F04` | sustain pedal |
| `+0x2F80` | polyphony |
| `+0x2F84` | oscillator 2 cents mode |
| `+0x2F88` | oscillator 1 modulation mode |
| `+0xC2B4` | pending-queue mutex, `+0xC2B8` its held flag |
| `+0xC2BC` | last monophonic event, 0x38 bytes |
| `+0xC2F4`, `+0xC2F5` | bytes gating `SavePreset` and `LoadPreset` calls in `OnChildModified`'s file-dialog branches; roles not pinned |
| `+0xC2F6` | set while `LoadPreset`/`SavePreset` run; `Serialize` then skips the preset path and the patterns |
| `+0xC2F8` | monophonic |
| `+0xC2F9`, `+0xC2FA` | the dead legato pair |

## Not established

- What `OutputPanel`'s three position doubles are exactly, and its default
  tempo.
- The labels of the controls at `+0xC280`/`+0xC284` beyond their `∓1`
  behaviour.
