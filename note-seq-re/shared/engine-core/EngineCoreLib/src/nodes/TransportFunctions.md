# transport functions: reconstruction notes

Source: `ABLSwiftBridge.framework` (Ableton Note), arm64. These are free functions in `ableton::engine::transport`. Nodes use them to convert between a segment's frames and beats, and between song time and clip time.

```
transport::beatTimeToFrameOffset(Segment const&, Beats)    0x10a3ec   112 bytes
transport::frameOffsetToBeatTime(Segment const&, Frames)   0x10a45c   112
transport::cropSegment(Segment const&, Frames, Frames)     0x10a4cc   256
transport::songTimeToClipTime(Beats, Beats, ClipRegion const&)  0x10a5cc  76
```

Calls out to `nextafter` (stub `0x11f2acc`) and `fmod` (stub `0x11f27c0`).

## Segment layout (0x60 bytes)

| Offset | Field | Evidence |
| --- | --- | --- |
| 0x00 | `frameOffsetStart` | asserts; all three functions |
| 0x08 | `frameOffsetEnd` | asserts; all three functions |
| 0x10 | `beatTimeStart` | asserts; `Setter<…, &Segment::beatTimeStart>` |
| 0x18 | `beatTimeEnd` | asserts |
| 0x20 | `isPlaying` (bool) | `Setter<bool, &Segment::isPlaying>`; tested by `cropSegment` |
| 0x28 | `oLoop` (`optional<Loop>`: start, end, engaged at 0x38) | `Setter<…, &Segment::oLoop>`; copied field by field in `cropSegment` |
| 0x40 | `tempo` (inferred) | `Setter<…, &Segment::tempo>`; the only named field without a confirmed slot |
| 0x48 | frames per beat (our name) | multiplier in `beatTimeToFrameOffset` |
| 0x50 | beats per frame (our name) | multiplier in `frameOffsetToBeatTime` and `cropSegment` |
| 0x58 | `grooveAmount` (float) | `Setter<float, &Segment::grooveAmount>`; `cropSegment` copies 0x4c–0x5b |

`ClipRegion` (0x28 bytes) is `start` at 0x00, then an `optional<Loop>` at 0x08 (start, end, engaged at 0x18), then `end` at 0x20. `songTimeToClipTime` reads only the first two; `end` is placed by `launcher::Clip` and matches the model's `FClipRegion` member order (`mStart`, `mLoop`, `mEnd`).

## Behaviour

- **`beatTimeToFrameOffset`:** `min(nextafter(frameOffsetEnd, -inf), frameOffsetStart + (beat - beatTimeStart) * framesPerBeat)`. The result always stays inside `[start, end)`. It asserts `beatTimeStart <= beat < beatTimeEnd`.
- **`frameOffsetToBeatTime`:** the inverse, clamped to just below `beatTimeEnd`. It asserts `frameOffsetStart <= frame < frameOffsetEnd`.
- **`cropSegment`:** copies the segment and sets the new frame range. Only while `isPlaying` does it recompute both beat times, using an inlined helper (asserts at lines 23–24). That helper differs from `frameOffsetToBeatTime` in two ways:
  - It accepts `frame == frameOffsetEnd` and returns `beatTimeEnd` exactly.
  - It clamps to `beatTimeEnd` itself, not to `nextafter`.

  A stopped segment keeps its beat times.
- **`songTimeToClipTime`:** `clipTime = song - launch + region.start`. If the clip loops and `clipTime >= loop.end`, then `clipTime = loop.start + fmod(clipTime - loop.start, loop.end - loop.start)`. A clip time before `loop.start` (an intro) isn't wrapped.

## Open points

- The real `Beats`/`Frames` are strong types (`datatype::Distance`). Here they're plain `double`s.
- The source lines suggest a fixed order: the helper sits near the top (around line 20), followed by `beatTimeToFrameOffset` (47), `frameOffsetToBeatTime` (60) and `cropSegment` (73). The `.cpp` keeps that order, with the helper first.
