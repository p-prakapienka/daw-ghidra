# CustomWaveform — reconstruction notes

The waveform side of `UICustomWaveformDialog`, Caustic's custom waveform editor.
SubSynth has two of these dialogs (`+0xC2A4`, `+0xC2A8`). Each holds 660 drawn
points. `SubSynth::RefreshCustomWavetable` turns one into the 4096-short table
that oscillator type 7 or 8 plays.

```
UICustomWaveformDialog::UICustomWaveformDialog(...)            0x176110  3084 bytes
UICustomWaveformDialog::SetRawValues(short*)                   0x177820    72
UICustomWaveformDialog::GetRawValues(short*)                   0x1770c8    52
UICustomWaveformDialog::FitToScreen()                          0x1770fc   128
UICustomWaveformDialog::GetWaveform(short*, unsigned int, float) 0x177868 416
SubSynth::RefreshCustomWavetable(int)                          0x992c4     60
```

## Points and the drawing area

The constructor writes the drawing area in the UI's fixed coordinates:

| Offset | Value | Meaning |
|---|---|---|
| `+0x324` | 12.0 | left |
| `+0x328` | 50.0 | top |
| `+0x32C` | 672.0 | right |
| `+0x330` | 365.0 | bottom |

These are literals, not screen measurements, so the conversion below does not
depend on the device.

It then lays out 660 points, 8 bytes apart from `+0x370`:

- **x**, at `+0x370 + 8i`, is the index `i`;
- **height**, at `+0x374 + 8i`, starts at 0.

A second array of 660 pairs at `+0x1810` holds 0.078125 and 0.09765625.
`GetWaveform` does not read it.

## SetRawValues, GetRawValues, FitToScreen

- **`SetRawValues`** sets each height to `(float)(short)raw[i]`. Any non-zero
  value sets the byte at `+0x31C`, which marks the waveform as drawn. It then
  tail-calls `FitToScreen`.
- **`FitToScreen`**:
  - finds the tallest point, `peak = max |height|`;
  - returns if half the area's height, `(365 − 50) / 2 = 157.5`, is greater
    than `peak`;
  - otherwise multiplies every height by `157.5 / peak`.
- **`GetRawValues`** writes each height back as a short, truncated, which is
  what `Serialize` saves.
- **The peak search.** Both peak searches use `vcmpe |h|, peak; vmovpl`, so a
  value equal to the running peak replaces it.

## GetWaveform

```
step  = 660 / (float)n
peak  = max |height|
gain  = peak != 0 ? ((top + bottom) * 0.5) / peak : 1
for i in 0 .. n-1:
    x     = (float)i * step
    k     = (int)(x + 0.5)                        ; vcvt, truncating
    f     = x - (float)k                          ; −0.5 .. 0.5
    a, b  = height[clamp(k, 0, 659)], height[clamp(k + 1, 0, 659)]
    v[i]  = (a + f * (b − a)) / ((top + bottom) * 0.5) * gain * level
mean  = Σ v[i] * (1 / (float)n)                   ; accumulated in that order
out[i] = (short)(int)((v[i] − mean) * 32767)
```

Mathematically the result is `height / peak * level`, with the mean removed.

- **The centre cancels.** `(top + bottom) / 2 = 207.5` divides out against the
  gain, but both operations are in the code and shape the rounding.
- **Interpolation is not standard.** It runs from the *nearest* point, so the
  fraction is negative half of the time and the curve is extrapolated backwards
  from it.
- **Inside SubSynth.** `RefreshCustomWavetable` calls
  `GetWaveform(table, 4096, 0.3)`: editor 0 into `+0x2F78`, editor 1 into
  `+0x2F7C`, any other index nothing. The constructor clears both tables, and
  both oscillators point their custom slots at them.

## Checked against the original

`SetRawValues`, `GetWaveform`, `GetRawValues` and the drawn flag are
identical to the 3.2.0 (beta 7) armv7 library in 32 scenarios. Eight shapes
were each converted at four table sizes and levels: 4096 and 660 at 0.3, 333
at 1.0, and 1 at 0.3. The shapes:

- flat, a small sine, a sine tall enough to be fitted, and a saw;
- an offset wave, a single spike, full-range noise, and square steps at the
  fit limit.

The same tables played through SubSynth are covered in `SubSynthVoice.md`.

## Not part of this class

Drawing, touch editing, `FilterCurve`, `SmoothEnds` and `AddNoise`, which change
the points in the editor.
