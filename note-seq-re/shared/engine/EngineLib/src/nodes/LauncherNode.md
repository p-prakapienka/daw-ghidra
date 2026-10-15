# LauncherNode: reconstruction notes

Source: `ABLSwiftBridge.framework` (Ableton Note), arm64. `LauncherNode<Content>` is one track's clip launcher. It holds the track's clip slots, a pending trigger and the playing state, and it decides on the audio thread when a clip starts and stops. The binary has only the `midi_player::Content` instantiation.

```
LauncherNode<midi_player::Content>::launchTimeIfDue(transport::Segment const&) const    0x3a7b9c   168 bytes
LauncherNode<midi_player::Content>::clipStopTimeIfDue(transport::Segment const&) const  0x3a7ae4   184
launcher::Clip<midi_player::Content>::operator=(Clip&&)                                0x372ff4
launcher::Clip<midi_player::Content>::~Clip()                                          0x3a8efc
```

Done so far: `launchTimeIfDue`, `clipStopTimeIfDue`, and the data layout they read. The rest of the node is still stubs.

## Layout

### `LauncherNode<Content>` (at least 0xe0 bytes)

| Offset | Member | Notes |
| --- | --- | --- |
| 0x00 | `mClipSlots`: `launcher::detail::ClipSlotsHolder<Content>` | name ours; could also be a base class |
| 0x78 | `moPendingTrigger`: `optional<PendingTrigger>`, engaged at 0xb0 | name from the assertion `moPendingTrigger.has_value()` |
| 0xb8 | `mPlayingState`: `launcher::PlayingState` | name ours |
| 0xd8 | `mIsRecording` (bool) | name ours; with `PendingTrigger::kind == 1` it makes a launch record |

### `launcher::detail::ClipSlotsHolder<Content>` (0x78 bytes)

| Offset | Member |
| --- | --- |
| 0x00 | `realtime::SharedPtr<vector<ClipSlot<Content>> const>`. The vector pointer sits in the second word (0x08), and the reference count at +0x18 of the block in the first word |
| 0x10 | `optional<TemporaryClip>`: `slotIndex` at 0x10, `Clip` at 0x18, engaged at 0x70 |

Every reader looks up a clip the same way, inlined. Here that lookup is `clipAt(slotIndex)` (our name):
1. If the temporary clip is engaged and stands in for that slot, use it.
2. Otherwise use the slot's clip, provided the pointer is set, the index is within the vector, and the slot is filled.

### `launcher::Clip<Content>` (0x58) and `ClipSlot<Content>` (0x60)

| Offset | Member |
| --- | --- |
| 0x00 | shared pointer, presumably the content |
| 0x10 | shared pointer, purpose unknown |
| 0x20 | `transport::ClipRegion` (start, optional loop, end; 0x28 bytes) |
| 0x48 | shared pointer, purpose unknown |

A `ClipSlot` is an optional `Clip`, with the engaged flag at 0x58.

### `PendingTrigger` (0x38) and `launcher::PlayingState` (0x20)

| Offset | PendingTrigger | PlayingState |
| --- | --- | --- |
| 0x00 | `quantum` (beats; 0 means at once) | `sessionClipStartTime` |
| 0x08 | `oClipSlot` (index, engaged at 0x10) | `oPlayingClipSlot` (index, engaged at 0x10); empty means stopped |
| 0x18 | `kind` (int32; 1 records) | unknown, always copied along |
| 0x20 | unknown, passed to `ensureFilledSlot` | |
| 0x28 | `oLaunchTime` (engaged at 0x30) | |

The `PlayingState` names follow the model's `FPlayingState` (`mSessionClipStartTime`, `mPlayingClipSlot`). The `PendingTrigger` field names are ours.

## Behaviour

- **`launchTimeIfDue`:** with no pending trigger, nothing is due. Otherwise the launch time is `roundUpToMultiple(segment.beatTimeStart, quantum)`, and it's due if it falls before `segment.beatTimeEnd`.
  - A zero quantum fires at the segment start.
  - The quantum's sign is ignored.
  - The grid is the song's beat time, so it doesn't depend on the playing clip.
- **`clipStopTimeIfDue`:** with no playing slot, an empty slot, or a looping clip, nothing is due. Otherwise:
  - If the clip's start lies after the segment start (the song time jumped back), it stops at the segment start.
  - Otherwise it stops at `start + region.end - region.start`, in that order, if that falls before the segment end. A stop time earlier than the segment start becomes the segment start.

## `math::roundUpToMultiple`

The real function is in `shared/base/MathLib/include/ableton/math/MathUtils.ipp` and is inlined here, so its name is ours.

1. It returns the value unchanged for a zero quantum, then takes the quantum's absolute value.
2. A shared helper asserts `quantum > 0.0` (line 24), so only a NaN quantum can fail it. It then asserts `std::abs(quants) < kExactIntRangeMaxForDouble` (2^53, line 27) and returns `std::round(value / quantum)`. The named `roundToNearestMultiple` @0x920530 begins with the same two assertions.
3. The result is `lower = quantum * n`, stepped up to `quantum * (n + 1)` when `lower < value`.

This isn't always bit-identical to `std::ceil(value / quantum) * quantum` (about 2% of a random sample differ), so the rounding has to be reproduced exactly.
