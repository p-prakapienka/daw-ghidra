# SubSynth voice

Recovered from `libcaustic.so` (ARMv7):

| Function | Address | Size |
|---|---|---|
| `SubSynth::ProcessChannel(int, int*, unsigned)` | `0x9c8c0` | 756 |
| `SubSynth::ApplyVCA(int, int*, unsigned)` | `0x9c810` | 176 |
| `Mixer::MixCrossfade(int*, int*, int, unsigned)` | `0x82248` | 72 |
| `SubSynth::ConnectLFOs()` | `0x9c524` | 724 |
| `SubSynth::Process(int*, unsigned)` | `0xa2ec4` | 344 |
| `SubSynth::SetPolyphony(int)` | `0x9cc48` | 388 |

Defaults come from `SubSynth::SubSynth` and control scaling from
`SubSynth::OnChildModified`.

## `ProcessChannel`, annotated

`r0` is the machine, `r1` the voice, `r2` the output, `r3` the sample count.
`cv` is the voice's control voltage at `+0xCFC + 0x60·v`.

```
level[v].target = (int)(this[0xC1E8] * cv[0x5C] * 16777215.0f)

if this[0xC2F7]:                              ; oscillator 2 enabled
    osc2(+0x2F40).GenerateSignal(cv, 1, buf2, n, fmIn=buf2, amount=0, bend)
else:
    memset(buf2, 0, n*4)

osc1(+0x2F08).GenerateSignal(cv, 0, buf1, n,
                             fmIn=buf2, amount=q24(this[0x2F90], 0x027FFFFD), bend)

if this[0xC2F7]:
    Mixer::MixCrossfade(buf2, buf1, this[0x2F8C], n)   ; buf1 += (buf2-buf1)·mix

ApplyVCA(v, buf1, n)
filter[v](+0x3768 + 0x844·v).ProcessCV(cv, buf1, n, 1)

for i in 0..n:
    level.current = level.target + q24(level.current - level.target, level.smoothing)
    out[i] += q24(buf1[i], q24(level.current, volume[i] + 0xFFFFFF))

cv[0x04] += n; cv[0x08] += n; cv[0x0C] += n·(1 - gate)
if ampEnvelope[v].IsDone(cv[0x0C]):           ; vtable slot 0
    cv.flags &= ~1                            ; voice inactive
    filter[v][0x81C..0x828] = 0               ; integrators
```

`bend` is the float at `+0xC1EC`; `buf1`, `buf2` and the envelope buffer are the
pointers at `+0x305C`, `+0x3060` and `+0x3064`, shared by every voice.
`volume` is the per-sample array at `+0x0C` of the source `+0x3764` points at.

Things the pseudocode makes plain:

- **Oscillator 2 is rendered first and is oscillator 1's modulator.** Its own
  call passes its output buffer as the modulator input with amount 0, which
  contributes nothing. The component passes no modulator instead.
- **"Osc1 Mod Amount" is scaled by 2.5** (`0x027FFFFD`) before it reaches the
  oscillator. What the product does depends on oscillator 1's modulation mode;
  see `components/Oscillator.md`.
- **Switching oscillator 2 off also removes the modulator**, because the
  input buffer is zeroed rather than skipped.
- **The mix is a crossfade**, not a sum: 0 is oscillator 1 alone, 1 is
  oscillator 2 alone. It only runs while oscillator 2 is enabled.
- **SubSynth is mono.** Every voice adds into one buffer, and
  `SubSynth::Process` hands that buffer to `UIVUMeter::Process(out, n, 1)`.
- **The volume factor is `value + 0xFFFFFF`**, so with the default source
  the output is scaled by 0xFFFFFF/2²⁴, not exactly one.

## `ApplyVCA`

```
position = gate ? cv[0x08] : cv[0x0C]
ampEnvelope[v](+0x3068 + 0x64·v).GenerateValues(position, gate, envBuf, n)   ; slot 1
buf[i] = q24(buf[i], envBuf[i] << 9)
```

The gate argument is the gate bit itself; `r2` still holds it when the call is
made, because `r2`'s original value (the buffer) was moved to `r7` first.

## The level smoother

Sixteen `{current, target, coefficient}` triples at `+0x2F94 + 12·v`.

- The constructor writes coefficient `0x00FFBE76` (0.999) and zeroes the rest.
- `SetPolyphony` overwrites every coefficient with `0x00E66665` (0.9).
  `ResetMachine` calls `SetPolyphony(4)`, so a running machine uses 0.9, a
  time constant of about ten samples: a de-click, not an audible glide.
- Nothing on the note-on path writes `current`, so a voice reused for a new
  note moves from its previous level to the new velocity.
- The multiply floors, so the smoother settles 9 units below a full-scale
  target rather than on it.

The target is `(int)(voiceGain · velocity · 16777215)`. `voiceGain` is the
float at `+0xC1E8`, set to 1.0 in the constructor. A scan of every function
in the library for that offset found no other writer.

## Routing

`ConnectLFOs` first points every input at the default source `+0x36A8`: both
oscillators' five source pointers (`+0x2F08`…`+0x2F20`, `+0x2F40`…`+0x2F58`),
the Volume input `+0x3764`, and all sixteen filters' `+0x80C` (the loop runs
`+0x3F74` to `+0x3F74 + 16·0x844`). It then switches on LFO 1's target at
`+0x3720` and LFO 2's at `+0x3760`, each through a nine-entry jump table
indexed by `target − 1`. The target list is in `components/LFO.md`.

Two details of the switch:

- It writes the LFO's phase offset (`+0x370C` / `+0x374C`) only for targets
  1 to 9. Target 0 leaves the previous offset in place.
- The default case of LFO 1's switch falls through to LFO 2's. LFO 2's cases
  return directly.

## Around the voice: `SubSynth::Process`

1. Call vtable slot 3 of the object at `+0xC2B0` with `(out, n)`. This is the
   pattern editor, which is what advances the sequencer.
2. Run `ProcessPendingNotes`.
3. Compute the song position from a global double and store it in both LFOs'
   position fields (`+0x36F4`, `+0x3734`).
4. Run `GenerateInternal(n)` on LFO 1, then LFO 2.
5. For each voice below the polyphony at `+0x2F80` whose active bit is set,
   run `ProcessChannel`.
6. Tail-call `UIVUMeter::Process` on the meter at `+0xC294`.

The loop stops at the polyphony, not at sixteen.

## Block size

`buf1` and `buf2` are each `new int[128]` (0x200 bytes) and the envelope
buffer is 0x120 bytes (144 shorts). The LFO output is also 128 entries. So the
engine never processes more than 128 samples in one call. The component
resizes its own buffers, but routed LFO outputs still cap a block at 128.

## Controls read along the way

These are `OnChildModified` branches, matched by the UI control pointer each
one compares:

| Control pointer | Effect |
|---|---|
| `+0xC20C` | Osc2 waveform: `this[0xC2F7] = (value != 0)`, then `SetOscillatorType(value − 1)` |
| `+0xC210` | Steps `+0xC20C` by one. At 9 or below it sets `0xC2F7`; above 9 it wraps to a literal |
| `+0xC228` | `SetOscillatorMix(value)`: `this[0x2F8C] = (int)(value · 16777215)` |
| `+0xC22C` | `this[0x2F90] = (int)(value · 16777215)`, the modulation amount |
| `+0xC230` | Cycles the modulation mode: `(this[0x2F88] + 1)`, wrapping above 2 to 0 |
| `+0xC208` | Pitch sweep: `+0x3054 = −v` and `+0x3058 = 0x3F7FE5C9` (≈0.9996) when `v > 0`; `+0x3054 = −2v` and `+0x3058 = 0x3F7FEDFA` (≈0.99972) otherwise |
| `+0xC260`, `+0xC264`, `+0xC268`, `+0xC26C` | Write attack, decay, sustain and release (ADSR `+0x04`…`+0x10`) into all sixteen filter ADSRs at `+0xBBA8`, calling `OnParamModified` on each. The loop ends at `+0xC1E8`, where the bank ends |

`SetOscillatorType(−1)` is out of range for its jump table. It stores the type
and returns without changing the table.

Defaults in the constructor: oscillator 2 off (`0xC2F7 = 0`), mix 0.5,
modulation amount 0, bend 1.0, voice gain 1.0.

## `SetPolyphony`

- 0 selects one voice.
- 2 selects one voice if the byte at `+0xC2F9` is set, otherwise two.
- Other values at or below 0 select one voice.
- Positive values are clamped to 8.

It stores the count at `+0x2F80` and sets `+0xC2F8` to whether the count is 1.
It clears `+0xC2F9`. For all sixteen slots it:

- clears flag bits 0–2;
- zeroes the counters, both phases, `frequencyQ12`, the pitch and glide fields,
  `noteId`, `0x34`, `0x40`, `0x44`, both sweeps and the sweep decay at `0x50`,
  and `frequencyHertz`;
- sets `field38` to 1 and `filterCutoffScale` and `velocity` to 1.0;
- writes a byte into each filter ADSR at `+0x61`;
- sets the level coefficient to 0.9.

It then notifies the pattern editor. None of this changes the voice DSP;
`SubSynth::setPolyphony` implements it, and `components/SubSynth.md` covers
what `+0xC2F8` and `+0xC2F9` gate.

## Not established

- The label of the control at `+0xC208`, which drives the pitch sweep.
- What the ADSR byte at `+0x61` does. The ADSR component does not read one.
- Whether anything writes `+0xC1E8` through a pointer that the offset scan
  cannot see.
