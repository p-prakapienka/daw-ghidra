# SubSynth controls

This file is the control-id table: which saved id is which UI control, which
machine field holds that control, and what `OnChildModified` does with its
value. It is the map a loaded `SubSynthArchive` needs before it can configure
a `SubSynth`. The `SubSynthControls` component implements it; see
"The component" at the end.

Recovered from `libcaustic.so` (ARMv7):

| Function | Address | Size |
|---|---|---|
| `SubSynth::SubSynth(UIControl*)` | `0x9fd6c` | 12632 |
| `SubSynth::OnChildModified(UIControl*, UIControlMessage)` | `0x9ea64` | 4864 |
| `SubSynth::OnSequencerControlEvent(SequencerControlEvent const&)` | `0x9dfe0` | 712 |
| `SubSynth::GetControlIDFromName(char const*)` | `0x1a9e1c` | 1088 |
| `SubSynth::GetPitchBendControlID()` | `0x98b14` | 8 |
| `UIControlCollection::AddControl(UIControl*, int, char const*)` | `0x16d61c` | 92 |
| `UIControlCollection::GetControl(int)` | `0x16d410` | 12 |
| `UIControlCollection::Serialize(Archive&)` | `0x16d6b4` | 1148 |
| `UIKnob::GetValue` / `SetValue` / `GetSerializeValue` / `GetMinMaxValues` | `0x16ff88` / `0x17030c` / `0x16ffa0` / `0x16ff74` | |
| `UISlider::GetValue` / `SetValue` / `GetMinMaxValues` | `0x171f6c` / `0x17221c` / `0x171f74` | |
| `UISelector::GetValue` / `SetValue` / `GetMinMaxValues` | `0x16db54` / `0x16db68` / `0x16dbc8` | |
| `UIValueContainer::GetValue` / `SetValue` / `GetMinMaxValues` | `0x98a24` / `0x9930c` / `0x98a10` | |

## How the table was read

The constructor makes 44 `AddControl(control, id, name)` calls, between
`0xa0670` and `0xa2cf0`. The constructor was run under an ARM emulator
(unicorn) with these parts executed for real:

- the UI classes' own constructors and setters;
- `AddControl`.

Every other call returned at once. Logging each `AddControl` call and every
store into `+0xC1E0`…`+0xC300` gives the id, the label, the control's class
and the machine field it lands in. The same run also gives each control's
range, curve and constructor default, read from the control objects
afterwards.

Every id and field matches `GetControlIDFromName`. The comparisons in that
function were replayed one by one, each forced to match. It is a chain of
`strcmp` against 30 names; anything else returns −1.

The effects come from reading `OnChildModified` branch by branch. Its literal
pools were resolved by hand: the capstone listing shows them as instructions.

## Ids, saving and loading

- **Explicit ids.** `AddControl` stores the control at
  `collection + 0x3C + 4·id` (128 slots) and the id at the control's `+0x1C`.
- **Automatic ids.** An id of −1 is replaced by the next free slot, one past
  the highest id so far.
- **The save flag.** The byte at `collection + 0x23C + id` records whether the
  id was explicit.
- **Saving.** `UIControlCollection::Serialize` saves only the controls with
  that flag set, each as a `CTRL` sub-chunk holding `GetSerializeValue()`.
- **Loading.** Loading calls `GetControl(id)->SetSerializeValue(value)`, which
  is `SetValue` for every class SubSynth uses. `UIClickRect` raises its
  `+0x5B` around the call, but SubSynth saves none.
- **The notification.** Every `SetValue` used here ends by calling vtable slot
  2 of the control's parent (`+0x24`), when its notify byte is set. The
  parent is the machine's collection at `+0x08`, and that slot is the thunk
  to `SubSynth::OnChildModified` (`0x9fd64`). The notify byte is at `+0xEF`
  on a knob, `+0x2EF` on a selector and `+0x38` on a value container. So
  loading a song drives the engine through the same branches as turning the
  knobs.
- **Automatic ids reach the engine too.** The controls with automatic ids
  (32–45) are never saved. Sequencer automation still reaches them through
  `GetControl`, and that is how id 32, the pitch bend, is recorded:
  `GetPitchBendControlID()` returns `0x20`.
- **Empty slots.** Slots 9 and 30 stay empty.

## Values by control class

| Class | Stored value | `GetValue` | `SetValue` | Saved |
|---|---|---|---|---|
| `UIKnob` | position `+0x48` | position, or position² when the curve byte `+0x28` is 1 | clamps to `[+0x3C, +0x40]`; with `+0x28` = 2 stores `floorf(v + 0.5)` | position (`GetSerializeValue` reads `+0x48`) |
| `UISlider` | `+0x3C` | `+0x3C` | clamps to `[+0x40, +0x44]`; rounds when `+0xC4` = 1 (neither SubSynth slider) | `GetValue` |
| `UITextSelector`, `UIImageSelector` | index `+0x2E8` | index as a float | `(int)v` clamped to `[0, count − 1]`, count at `+0x2E4` | `GetValue` |
| `UIValueContainer` | `+0x30` | `+0x30` | stores unclamped | `GetValue` |

So the saved value of a knob is its position, while the engine sees
`GetValue`. For the square-curve knobs (curve 1), which are the envelope
times, the engine gets the position squared.

`GetMinMaxValues` returns the knob or slider range. A selector returns 0 and
its count, not count − 1. A value container returns `+0x28`/`+0x2C`.

## Saved controls

Column notes:

- **Name** is the string `GetControlIDFromName` matches.
- **Label** is the third `AddControl` argument, `—` where it is empty.
- **Default** is the constructor's `SetValue` argument after the control has
  clamped and rounded it, or the knob constructor's own value where the
  constructor sets none.

| Id | Name | Label | Field | Class | Range, curve | Default | Branch | Effect |
|---|---|---|---|---|---|---|---|---|
| 0 | — | — | `+0xC1F8` | `UIFileSelector` | — | — | `0x9ef18` | preset browser: picking a file calls `LoadPreset` |
| 1 | `osc1_waveform` | — | `+0xC200` | `UIValueContainer` | 0..8 | 0 | `0x9f724` | oscillator 1 (`+0x2F08`) `SetOscillatorType((int)v)`; see "Waveforms" |
| 2 | `osc_mix` | `Osc\|Mix` | `+0xC228` | knob | 0..1, linear | 0.5 | `0x9fa50` | `SetOscillatorMix(v)` |
| 3 | `osc_bend` | `Bend\|Amount` | `+0xC208` | knob | −0.99..0.99, linear, deadzone | 0 | `0x9f7a4` | pitch sweep (`SubSynthVoice.md`) |
| 4 | `osc2_waveform` | — | `+0xC20C` | `UIValueContainer` | 0..9 | 0 | `0x9f86c` | `+0xC2F7 = (v ≠ 0)`; oscillator 2 (`+0x2F40`) `SetOscillatorType(v − 1)` |
| 5 | `osc2_phase` | `Osc2\|Phase` | `+0xC214` | knob | −0.5..0.5, linear | 0 | `0x9f9d4` | `+0x2F4C` (oscillator 2 `+0x0C`) `= (int)(v · 4096)` |
| 6 | `osc2_octave` | `Osc2\|Octave` | `+0xC218` | knob | −4..4, integer | 0 | `0x9f3d8` | oscillator 2 pitch, see below |
| 7 | `osc2_semis` | `Osc2\|Semis` | `+0xC21C` | knob | −12..12, integer | 0 | `0x9f3d8` | oscillator 2 pitch |
| 8 | `osc2_cents` | `Osc2\|Cents` | `+0xC220` | knob | −50..50, linear, deadzone | 0 | `0x9f3d8` | oscillator 2 pitch; oscillator 1 too in cents mode 1 |
| 10 | `lfo1_waveform` | — | `+0xC234` | `UIImageSelector`, 4 | 0..3 | 0 | `0x9f5a4` | LFO 1 (`+0x36E4`) `+0x1C = (int)v`, `OnParamModified` |
| 11 | `lfo1_target` | — | `+0xC238` | `UITextSelector`, 10 | 0..9 | 0 | `0x9f5d8` | `SetLFO1Target((int)v)` |
| 12 | `lfo1_rate` | `LFO1\|Rate` | `+0xC23C` | knob | 0..12, integer | 6 | `0x9f620` | LFO 1 `+0x04 = v`, `OnParamModified` |
| 13 | `lfo1_depth` | `LFO1\|Depth` | `+0xC240` | knob | 0..1, linear | 0 | `0x9f64c` | LFO 1 `+0x08 = (int)(v · 16777215)`, `OnParamModified` |
| 14 | `lfo2_target` | — | `+0xC244` | `UITextSelector`, 10 | 0..9 | 0 | `0x9f688` | `SetLFO2Target((int)v)` |
| 15 | `lfo2_rate` | `LFO2\|Rate` | `+0xC248` | knob | 0..12, integer | 6 | `0x9f6b4` | LFO 2 (`+0x3724`) `+0x04 = v`, `OnParamModified` |
| 16 | `lfo2_depth` | `LFO2\|Depth` | `+0xC24C` | knob | 0..1, linear | 0 | `0x9f6e0` | LFO 2 `+0x08 = (int)(v · 16777215)`, `OnParamModified` |
| 17 | `filter_cutoff` | `Filter\|Cutoff` | `+0xC250` | `UISlider` | 0.001..1, linear | 1.0 | `0x9efb8` | every voice filter `+0x838 = v`, then the coefficients; see below |
| 18 | `filter_resonance` | `Filter\|Res` | `+0xC254` | `UISlider` | 0..0.9, linear | 0 | `0x9f0f0` | every voice filter `+0x83C = v`, then the coefficients |
| 19 | `filter_type` | — | `+0xC258` | `UITextSelector`, 7 | 0..6 | 0 | `0x9fb20` | mode and invert flag of every voice filter; see below |
| 20 | `filter_kbtrack` | `Filter\|KBTrack` | `+0xC25C` | knob | 0..1, linear | 0 | none | read in `PlayChannel` (`SubSynth.md`) |
| 21 | `filter_attack` | `Filter\|Attack` | `+0xC260` | knob | 0..1.75, square | 0 | `0x9f8fc` | filter ADSRs (`+0xBBA8`) `+0x04 = GetValue` |
| 22 | `filter_decay` | `Filter\|Decay` | `+0xC264` | knob | 0..1.75, square | 0 | `0x9f944` | filter ADSRs `+0x08` |
| 23 | `filter_sustain` | `Filter\|Sustain` | `+0xC268` | knob | 0..1, linear | 1.0 | `0x9f98c` | filter ADSRs `+0x0C` |
| 24 | `filter_release` | `Filter\|Release` | `+0xC26C` | knob | 0..1.75, square | 1.75 | `0x9fa08` | filter ADSRs `+0x10` |
| 25 | `volume_attack` | `Volume\|Attack` | `+0xC270` | knob | 0..1.75, square | 0 | `0x9f2a0` | amplitude ADSRs (`+0x3068`) `+0x04 = GetValue` |
| 26 | `volume_decay` | `Volume\|Decay` | `+0xC274` | knob | 0..1.75, square | 0 | `0x9f2e8` | amplitude ADSRs `+0x08` |
| 27 | `volume_sustain` | `Volume\|Sustain` | `+0xC278` | knob | 0..1, linear | 1.0 | `0x9f4d8` | amplitude ADSRs `+0x0C` |
| 28 | `volume_release` | `Volume\|Release` | `+0xC27C` | knob | 0..1.75, square | 0 | `0x9f520` | amplitude ADSRs `+0x10` |
| 29 | `volume_out` | `Out\|Volume` | `+0xC290` | knob | 0..2, linear | 1.0 | `0x9f568` | `+0xC1E8 = v · v` |
| 31 | `osc_modulation` | `Mod\|Amount` | `+0xC22C` | knob | 0..1, linear | 0 | `0x9fb00` | `+0x2F90 = (int)(v · 16777215)` |

Every ADSR branch loops over all sixteen envelopes of its bank (stride
`0x64`) and calls `ADSR::OnParamModified` on each.

The LFO-target items, in order, are:
`------`, `Osc 1`, `Osc 2`, `Osc 1+2`, `Phase`, `Cutoff`, `Volume`,
`Octave`, `Semis`, `Osc1 Mod`. They are the `LFO::Target` values in
`LFO.md`. The filter-type items are `------`, `LowPass`, `HighPass`,
`BandPass`, `Inv. LP`, `Inv. HP`, `Inv. BP`.

No control sets LFO 2's waveform. Only LFO 1 has a waveform control.

### Two defaults that replace what the constructor writes directly

- The constructor sets each filter envelope's release to 2.0 (`0xa0314`…).
  Later it calls `SetValue(2.0)` on the release knob, which clamps the
  position to 1.75, and `OnChildModified` then gives the filter envelopes
  `1.75² = 3.0625` s.
- The amplitude sustain is written as `16777215.0`, and the sustain knob
  gets `SetValue(16777215.0)`, which clamps to 1.0. So the odd value in the
  amplitude envelopes is overwritten with 1.0 by the same route.

Both happen because `OnChildModified` runs during construction. **This is
now read, not derived.** Running the whole constructor under the emulator
reached the `OnChildModified` thunk 23 times, once per default `SetValue`.
It left the filter envelopes at 3.0625 s and the amplitude sustain at 1.0.

## Controls with automatic ids (not saved)

| Id | Field | Class | Branch | Effect |
|---|---|---|---|---|
| 32 | `+0xC1F0` | `UIValueContainer`, 0..1, default 0.5 | `0x9ed34` | pitch bend, below. `GetControlIDFromName("pitch_bend")` and `GetPitchBendControlID()` both give 32 |
| 33 | `+0xC230` | `UIClickRect` | `0x9f848` | `SetOscillator1ModulationMode(mode + 1)`, wrapping above 2 to 0 |
| 34 | `+0xC224` | `UIClickRect` | `0x9f700` | `SetOscillator2CentsMode(mode + 1)`, wrapping above 1 to 0 |
| 35 | `+0xC204` | `UIImageSelector`, 6 | `0x9f1f0`, `0x9f330` | steps id 1: `(int)(v + 1) ≥ 9 ? 0 : v + 1`. Message 1 sets id 1 to 7 and opens the dialog at `+0xC2A4` |
| 36 | `+0xC210` | `UIImageSelector`, 7 | `0x9f7f0`, `0x9f368` | steps id 4: `(int)(v + 1) > 9 ? 0 : v + 1`, setting `+0xC2F7` when it does not wrap. Message 1 sets id 4 to 9 and opens the dialog at `+0xC2A8` |
| 37 | `+0xC294` | `UIVUMeter` | none | |
| 38 | `+0xC280` | `UIImageSelector`, 1 | `0x9fc98` | `SetPolyphony(+0x2F80 − 1)` |
| 39 | `+0xC284` | `UIImageSelector`, 1 | `0x9fcb4` | `SetPolyphony(+0x2F80 + 1)` |
| 40 | `+0xC288` | `UIImageSelector`, 2 | `0x9fb84` | `v ≠ 0`: `MixerDispatch::OnMachineSolo(this)`, then `OnMachineMute(this)`; `v = 0`: `OnMachineMute(this)` only |
| 41 | `+0xC28C` | `UIImageSelector`, 2 | `0x9fc7c` | `MixerDispatch::OnMachineSolo(this)` |
| 42 | `+0xC29C` | `UIClickRect` | `0x9fbfc` | `+0xC2F4 = 1`, opens the soft keyboard at `+0xC2A0` with the current preset name |
| 43 | — | `UIKeyboard` | none | the on-screen keyboard |
| 44 | `+0xC298` | `UILED` | none | |
| 45 | `+0xC1F4` | `UIClickRect` | `0x9f25c` | message 1: `+0xC2F4 = 0`, opens the soft keyboard |

The custom-waveform values (7 for oscillator 1, 9 for oscillator 2) match
the custom types 7 and 8 in `Oscillator.md`, because oscillator 2's type is
its value − 1.

The soft keyboard (`+0xC2A0`) and the file dialog (`+0xC2AC`) are not in
the collection. They still have branches (message 2), which save the preset
through `SavePreset` and refresh the browser.

## The branches with formulas

### Oscillator 2 pitch (`0x9f3d8`, ids 6, 7, 8)

One branch serves all three knobs and reads all three:

```
ratio  = powf(2, octave · 12 / 12)
ratio *= powf(2, semis / 12)
ratio *= powf(2, cents / 1200 · 12 / 12)
+0x2F44 = ratio                                  ; oscillator 2 +0x04
if control is the cents knob and +0x2F84 == 1:   ; cents mode
    +0x2F0C = powf(2, −cents / 1200 · 12 / 12)   ; oscillator 1 +0x04
```

The `· 12 / 12` pairs are in the code. They are exact for the integer octave
and can move the cents term by one rounding step.

Changing the octave or the semitones in cents mode 1 does not refresh
oscillator 1. `SetOscillator2CentsMode` does, from the cents knob.

### Waveforms (`0x9f724`, `0x9f86c`)

After setting the oscillator type, each branch writes a picture index into
its step selector. Notification is off (`+0x2EF` cleared, then set again),
so writing it does not loop back into `OnChildModified`.

- oscillator 1: `v > 4 ? v − 2 : v > 2 ? v − 1 : v`, into `+0xC204`;
- oscillator 2: `v > 5 ? v − 2 : v > 3 ? v − 1 : v`, into `+0xC210`.

The selectors have 6 and 7 pictures, so this index only drives the
display. The waveform itself is the value at `+0xC200`/`+0xC20C`, which a
value container stores unclamped. A saved value outside 0..8 or 0..9 goes
straight to `SetOscillatorType`.

### Filter cutoff and resonance (`0x9efb8`, `0x9f0f0`)

For each of the sixteen voice filters (`+0x3768`, stride `0x844`), the
branch stores the new value as the filter's base (`+0x838` or `+0x83C`) and
recomputes the coefficients from both bases. The arithmetic matches the end
of `FixedPointSVFilter::FixedPointSVFilter`:

```
ci = c < 0.05 ? 12 : c > 1 ? 255 : (int)(c · 255)
(ri, t) = r < 0 ? (0, −0.0) : r > 1 ? (255, −0.75) : ((int)(r · 255), −0.75 · r)
+0x82C = cutoffTable[ci];  +0x830 = dampingTable[ri]
+0x818 = (int)(((+0x841 ? 1.0 : 1.25) + t) · 16777215)
```

The base is `+0x841 ? 1.0 : 1.25`, the same as the filter's constructor and
`ProcessCV`. SubSynth constructs its filters with the flag set, so its input
gain runs from 1.0 down to 0.325 at the 0.9 resonance limit.
The filter component had the two bases swapped until the fix that followed
this table (see the correction in `src/dsp/FixedPointSVFilter.md`).

### Filter type (`0x9fb20`)

```
t = (int)v
invert = t > 3
mode   = invert ? t − 3 : t
for each voice filter: +0x08 = mode; vtable slot 2(); +0x840 = invert
```

Slot 2 is `Machine::OnParamModified`, a bare `bx lr`. `+0x840` is the
filter's invert-envelope flag, which is what the `Inv.` items select.

### Pitch bend (`0x9ed34`, id 32)

```
v == 0.5: +0xC1EC = 1.0
otherwise: +0xC1EC = powf(2, 2 · (v − 0.5) · g_nPitchBendSemis / 12)
```

The code computes the two sides of 0.5 separately, but both reduce to that
expression. `g_nPitchBendSemis` is the `int` at `.data 0x343908`, reached
through GOT slot `0x30d2d4`. Its initial value is 1. Its writers were not
traced.

### Output volume (`0x9f568`, id 29)

`+0xC1E8 = GetValue() · GetValue()`, two calls on the same knob. The voice
level target is `(int)(+0xC1E8 · velocity · 16777215)`
(`SubSynthVoice.md`), so the knob's 0..2 gives a gain of 0..4, with 1.0 at
the default position.

### The legacy envelope-time fix-up, by id

`Serialize` (`0x9d3e8` onwards), for version ≤ 2.1.0, does
`SetValue(sqrt(sqrt(GetValue())))` on ids 21, 22, 24, 25, 26 and 28. With
the square curve, a stored value x reads back as x², so the result is the
position √x, and the engine time is x again:

```
old file: x seconds → position x → GetValue x² → ⁴√(x²) = √x → position √x → GetValue x
```

Old files stored the time itself. The fix-up turns it into the position
that reproduces that time on the current square-curve knob.

The collection is loaded before the fix-up, and loading is a `SetValue`, so
the stored x has already been clamped to the knob's 1.75 when the fix-up
reads it. Old times above 1.75 s therefore come back as 1.75 s. **Correction:**
an earlier version of this note gave the limit as 1.75² = 3.0625 s, which is
the limit for current files.

The waveform renumbering in `SubSynthArchive.md` applies to ids 1 and 4.

## Sequencer and MIDI events (`OnSequencerControlEvent`)

**Plain automation** (bit `0x40` of event byte `+0x0B` clear):

- The id is the low 30 bits of `event[+0x08]`, sign-extended.
- `GetControl(id)` resolves it, so automatic ids work too.
- If the control's byte `+0x07` is clear, it gets `SetValue(event[+0x10])`.

Automation values are therefore in control units, the same as `CCOL`. For a
knob that is the position, not the engine value.

**MIDI** (bit `0x40` set): the high byte of the id selects the path.

- `0x0E` sets the bend control (`+0xC1F0`) to the event value. The
  control's `+0x07` is set for the duration of the call.
- `0x0B` looks the CC number up in the `MIDIMachineMapping` at `+0xCF0`
  (0x30-byte entries, at most 128 matches). For each mapped id:
  - `0x400`: `SetOscillator2CentsMode(v > 0.5)`;
  - `0x401`: `+0x2F04 = (v ≥ 0.5)`; writing 0 calls
    `ReleaseSustainedNotes`;
  - `0x1E`: `SetOscillator1ModulationMode(v < 0.333 ? 0 : v > 0.666 ? 2 : 1)`;
  - anything else: `GetControl(id)`, skipped if its `+0x07` is set unless
    the event's bit `0x40` and the machine's `+0xCD4` are both set. The
    control then gets `SetValue(min + (max − min) · v)`, from its
    `GetMinMaxValues`, with `+0x07` set during the call. `+0x07` is cleared
    afterwards unless `+0xCD4` is set.

So `0x400`, `0x401` and `0x1E` are **mapping targets, not controls**: no
control has those ids, and plain automation addressed to them finds nothing.

While `+0xCD4` is set, every `OnChildModified` call first records the change
with `Sequenceable::AddControlKey(this + 0x2C4, control, −1)`.

## `OnChildModified` messages

- **Message 0** is a value change and runs the dispatch above.
- **Message 1** is handled for the controls at `+0xC204`, `+0xC210` and
  `+0xC1F4` only.
- **Messages 2 and 3** go to the preset dialogs, and to the automation-clear
  path for the control at `+0x2C8`.

What the user does to raise messages 1–3 is not established.

## Not established

- The writers of `g_nPitchBendSemis`.
- What the control byte `+0x07` means beyond "skip plain automation". The
  MIDI path sets it.
- The value `UIFileSelector` (id 0) saves, and what `+0xC2F4` selects in the
  preset dialogs.
- `CreateDefaultMapping`, the default CC → id table.

## The component

`SubSynthControls` wraps a `SubSynth` and keeps one stored value per id, as
the UI controls do:

- `set(id, value)` is the control's `SetValue` (clamp, truncate or round by
  class and curve) followed by the branch above. Ids it does not model
  return false and change nothing.
- `position(id)` is what would be saved; `value(id)` is what the branch reads.
- `setModulationMode` and `setCentsMode` are `SetOscillator1ModulationMode`
  and `SetOscillator2CentsMode`. Cents mode 0 resets oscillator 1's ratio to
  1.0; any other mode sets `powf(2, −cents / 1200 · 12 / 12)`.
- `applyAutomation` and `applyMidiTarget` are the two event paths.
  Both ignore the control byte `+0x07`, whose meaning is not established.
- `load(archive)` follows `Serialize`'s order: every control in the
  collection, the two custom waveforms, then `SetPolyphony`, the modulation
  mode and the cents mode, then the time fix-up and the waveform
  renumbering.
- `resetToDefaults()` sets every modelled control to its default through
  `set`, as the constructor does.

Not modelled:

- the preset browser (id 0) and the UI-only automatic ids (33–45);
- the preset name and path and the keyboard octave.

A waveform value outside the oscillator's types leaves the playing type
alone, as `SetOscillatorType`'s jump table does.

What the branches need from the other components:

- `FixedPointSVFilter::UpdateCoefficients`, the coefficient update the cutoff
  and resonance branches run inline in the engine. They write `m_fCutoff` or
  `m_fResonance`, then update from both. The filter-type branch writes
  `m_nMode`, calls `OnParamModified` (vtable slot 2, a no-op), then writes
  `m_bInvertEnvelope`, in that order;
- read accessors on `ADSR`, `LFO` and `Oscillator`,
  for the tests.
