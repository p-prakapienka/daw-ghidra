# LauncherNode: reconstruction notes

Source: `ABLSwiftBridge.framework` (Ableton Note), arm64. `LauncherNode<Content>` is one track's clip launcher. It holds the track's clip slots, a pending trigger and the playing state. On the audio thread it decides when a clip starts and stops, and it tells the rest of the track which clip plays. The binary has only the `midi_player::Content` instantiation.

```
LauncherNode<midi_player::Content>::launchTimeIfDue(transport::Segment const&) const    0x3a7b9c   168 bytes
LauncherNode<midi_player::Content>::clipStopTimeIfDue(transport::Segment const&) const  0x3a7ae4   184
LauncherNode<midi_player::Content>::setState(...)                                      0x3a6ec8  1724
LauncherNode<midi_player::Content>::play(...)                                          0x3a69fc    52
LauncherNode<midi_player::Content>::processCommand(..., Trigger const&)                0x3a6a30   560
LauncherNode<midi_player::Content>::processCommand(..., PlayingState const&)           0x3a7584    20
LauncherNode<midi_player::Content>::processCommand(..., RestartPlayingClip)            0x3a7598    76
LauncherNode<midi_player::Content>::processCommand(..., StartRecording)                0x3a75e4    20
LauncherNode<midi_player::Content>::processCommand(..., TriggerRecording const&)       0x3a75f8   300
LauncherNode<midi_player::Content>::processCommand(..., StopRecording)                 0x3a7724    32
LauncherNode<midi_player::Content>::launchPendingClip(AudioContext const&)             0x3a7c44   272
LauncherNode<midi_player::Content>::finishPlayingClip(AudioContext const&)             0x3a7d54    68
LauncherNode<midi_player::Content>::adjustPlayingClipStartIfNeeded(AudioContext const&) 0x3a7d98  212
launcher::detail::ClipSlotsHolder<midi_player::Content>::ensureFilledSlot(unsigned long, Beats)  0x3a6c60  616
launcher::operator<<(std::ostream&, launcher::ChangeOrigin)                            0x3a807c     4
```

**Done:**
- The timing functions: `launchTimeIfDue` and `clipStopTimeIfDue`.
- The state transition, `setState`, and everything that goes through it: `play`, all six `processCommand` overloads, `launchPendingClip`, `finishPlayingClip` and `adjustPlayingClipStartIfNeeded`.
- `ensureFilledSlot`.

**Still stubs:** `processClipSlots`, `processPendingPlaybackOperations` and the node's `operator()`.

## Layout

### `LauncherNode<Content>` (at least 0xe0 bytes)

| Offset | Member | Notes |
| --- | --- | --- |
| 0x00 | `mClipSlots`: `launcher::detail::ClipSlotsHolder<Content>` | name ours; could also be a base class |
| 0x78 | `moPendingTrigger`: `optional<PendingTrigger>`, engaged at 0xb0 | name from the assertion `moPendingTrigger.has_value()` |
| 0xb8 | `mPlayingState`: `launcher::PlayingState` | name ours |
| 0xd8 | `mIsRecording` (bool) | name ours |

### `launcher::detail::ClipSlotsHolder<Content>` (0x78 bytes)

| Offset | Member |
| --- | --- |
| 0x00 | `realtime::SharedPtr<vector<ClipSlot<Content>> const>`. The vector pointer sits in the second word (0x08), and the reference count at +0x18 of the block in the first word |
| 0x10 | `optional<TemporaryClip>`: `slotIndex` at 0x10, `Clip` at 0x18, engaged at 0x70 |

Every reader looks up a clip the same way, inlined. Here that lookup is `clipAt(slotIndex)` (our name):
1. If the temporary clip is engaged and stands in for that slot, use it.
2. Otherwise use the slot's clip, provided the pointer is set, the index is within the vector, and the slot is filled.

### `launcher::Clip<Content>` (0x58) and `ClipSlot<Content>` (0x60)

Each pointer feeds one of the node's outputs, which gives its type. The member names are ours.

| Offset | Member | Output |
| --- | --- | --- |
| 0x00 | `SharedPtr<Content const> pContent` | content (0x70) |
| 0x10 | `SharedPtr<ClipAutomation const> pAutomation` | automation (0x78) |
| 0x20 | `transport::ClipRegion region` (start, optional loop, end; 0x28 bytes) | clip frame (0x60) |
| 0x48 | `SharedPtr<time::Groove const> pGroove` | groove (0x68) |

- **`ClipAutomation`:** our alias for the type the automation port spells out: `unordered_map<ModelId<DeviceIdTag>, SharedPtr<flat_map<ModelId<ParameterIdTag>, SharedPtr<vector<Event<float, Beats>> const>> const>>`. That's per device, per parameter, a breakpoint list in clip beats.
- **Implicit members:** `Clip` has no user-declared special members. The binary's `~Clip` @0x3a8efc and `operator=(Clip&&)` @0x372ff4 are compiler-generated; the move assignment moves member by member and is emitted in the `SyncClipSlots` code. The same goes for `~TemporaryClip` @0x3a8e4c.
- **`ClipSlot`:** an optional `Clip`, with the engaged flag at 0x58.

### `PendingTrigger` (0x38) and `launcher::PlayingState` (0x20)

| Offset | PendingTrigger | PlayingState |
| --- | --- | --- |
| 0x00 | `quantum` (beats; 0 means at once) | `sessionClipStartTime` |
| 0x08 | `oClipSlot` (index, engaged at 0x10); empty means stop | `oPlayingClipSlot` (index, engaged at 0x10); empty means stopped |
| 0x18 | `kind` (int32): 0 launch, 1 record | `unknown18`, always copied along |
| 0x20 | `newClipDuration` (beats) | |
| 0x28 | `oLaunchTime` (engaged at 0x30) | |

- **Names:** the `PlayingState` names follow the model's `FPlayingState` (`mSessionClipStartTime`, `mPlayingClipSlot`). `newClipDuration` is the parameter name in `ensureFilledSlot`'s assertion. The other `PendingTrigger` names are ours.
- **`unknown18`:** `FPlayingState` has a third member, `mMode` (`FPlayingState::Mode`, with a `kClipSlot` value). This word may be its engine counterpart. Every launcher function copies it unchanged, and only `processCommand(PlayingState)` brings in a new value.

### Commands (`launcher::` structs, names ours)

| Command | Layout |
| --- | --- |
| `Trigger` (0x30) | `quantum` 0x00, `oClipSlot` 0x08 (engaged 0x10), `newClipDuration` 0x18, `oLaunchTime` 0x20 (engaged 0x28) |
| `TriggerRecording` (0x18) | `quantum` 0x00, `clipSlot` 0x08 (not optional), `newClipDuration` 0x10 |
| `RestartPlayingClip`, `StartRecording`, `StopRecording` | empty, passed by value |
| `PlayingState` | as above |

The node's command input is `EventBuffer<variant<Trigger, PlayingState, RestartPlayingClip, StartRecording, TriggerRecording, StopRecording>>` (from `operator()`'s signature).

### `AudioContext` and the outputs

`AudioContext` starts with the current `transport::Segment` (0x60 bytes). The node's outputs follow, in this order:

| Offset | Output |
| --- | --- |
| 0x60 | `CvBuffer<optional<ClipTimeNode::PlayingClipFrame>>`: the playing clip's start and region |
| 0x68 | `CvBuffer<SharedPtr<time::Groove const>>` |
| 0x70 | `CvBuffer<SharedPtr<midi_player::Content const>>` |
| 0x78 | `CvBuffer<SharedPtr<ClipAutomation const>>` |
| 0x80 | `EventBuffer<launcher::State>` |

`operator()`'s signature names these types. `processPendingPlaybackOperations` takes the context by value and narrows its segment as it goes.

- **`ClipTimeNode::PlayingClipFrame` (0x30):** the session clip start time at 0x00, then the clip's `ClipRegion` at 0x08.
- **`launcher::State` (0x48), the change report:**

  | Offset | Member (names ours) |
  | --- | --- |
  | 0x00 | `optional<PendingLaunch> oPendingLaunch`, engaged at 0x18: the pending slot (optional) and `willRecord` at 0x10 |
  | 0x20 | `PlayingState playingState` |
  | 0x40 | `isRecording` |
  | 0x41 | `isPlayingTemporaryClip` |
  | 0x44 | `ChangeOrigin origin` (int32) |

### `launcher::ChangeOrigin`

An int-sized enum. `operator<<` prints the number. The values come from the callers; the names are ours.

| Value | Name | Caller |
| --- | --- | --- |
| 0 | `kCommand` | `processCommand`, `play` |
| 1 | `kLaunch` | `launchPendingClip` |
| 2 | `kPlayback` | `finishPlayingClip`, `adjustPlayingClipStartIfNeeded` |
| 3 | `kRestart` | `processCommand(RestartPlayingClip)` |
| 4 | `kClipSlots` | `processClipSlots` |

## Behaviour

### Timing

- **`launchTimeIfDue`:** with no pending trigger, nothing is due. Otherwise the launch time is `roundUpToMultiple(segment.beatTimeStart, quantum)`, and it's due if it falls before `segment.beatTimeEnd`.
  - A zero quantum fires at the segment start.
  - The quantum's sign is ignored.
  - The grid is the song's beat time, so it doesn't depend on the playing clip.
- **`clipStopTimeIfDue`:** with no playing slot, an empty slot, or a looping clip, nothing is due. Otherwise:
  - If the clip's start lies after the segment start (the song time jumped back), it stops at the segment start.
  - Otherwise it stops at `start + region.end - region.start`, in that order, if that falls before the segment end. A stop time earlier than the segment start becomes the segment start.

### The state transition: `setState`

Every change of the launcher's state goes through `setState(context, origin, oPendingTrigger, playingState, isRecording)`:

1. **Store:** it stores all three values. The arguments may be the node's own members: `play` passes `moPendingTrigger`, and `StartRecording` passes `mPlayingState`.
2. **Look up the clip:** it looks up the playing slot's clip. A slot without a clip counts as stopped, so the slot index is dropped.
3. **Send the playing clip:** all at the segment's first frame (`segment.frameOffsetStart`), it sends:
   - the clip frame (`{sessionClipStartTime, region}`), or an empty optional;
   - then the groove, content and automation pointers, or null.
4. **Send the change report:** a `launcher::State`.
   - **`willRecord`:** set when recording is on, or when the pending trigger is a record trigger.
   - **`isPlayingTemporaryClip`:** set when the temporary clip stands in for the playing slot.

`play(context, origin, playingState, recordingChange)` is `setState` with the pending trigger unchanged. It sets recording to `recordingChange == kStart || mIsRecording`, so `play` can start recording but never stops it. `RecordingChange` (names ours) only has the value 1 tested. `StopRecording` calls `setState` directly.

### Commands

| Command | Effect |
| --- | --- |
| `PlayingState` | `play` with it as given (kCommand) |
| `RestartPlayingClip` | while a slot plays: `play` it again from the segment start (kRestart) |
| `StartRecording` | `play` what plays now, starting recording |
| `StopRecording` | a pending record trigger becomes a plain launch; `setState` with recording off |
| `TriggerRecording` | see below |
| `Trigger` | see below |

- **`Trigger`:**
  - **Queue or fire now:** a non-zero quantum queues the trigger while the transport plays or while the slot holds a clip. Otherwise it takes effect now. So a stopped transport launches a clip on the grid once it starts, while an empty slot (a stop) acts at once.
  - **Kind:** the queued trigger keeps the `kind` of the one it replaces, so re-triggering during a pending record launch still records.
  - **Firing now:** this works like `launchPendingClip` below, with origin `kCommand`.
- **`TriggerRecording`:**
  1. If the slot already plays, recording starts at once.
  2. Otherwise, with a zero quantum, the slot is filled if it's empty (`ensureFilledSlot`) and plays from the segment start, recording.
  3. Otherwise a record trigger is queued (`kind` 1, no fixed launch time). Recording itself stays as it is.

### Playback operations

- **`launchPendingClip`:** asserts `moPendingTrigger.has_value()` (line 374), then clears the trigger.
  - **Start time:** the trigger's fixed launch time if it has one, otherwise the segment start.
  - **Empty slot:** when there's a slot and either recording is on or the trigger is a record trigger, `ensureFilledSlot` gives the slot a temporary clip.
  - **Play:** it then plays with origin `kLaunch`, starting recording for a record trigger.
- **`finishPlayingClip`:** plays nothing from the segment start (`kPlayback`).
- **`adjustPlayingClipStartIfNeeded`:** it acts only on a looping playing clip whose start lies after the segment start (the song time jumped back). The start moves back by whole loops, then the clip plays again (`kPlayback`):

  ```cpp
  start -= loopLength * std::ceil((start - segmentStart) / loopLength);
  ```

  The new start is at or before the segment start, and the loop phase is unchanged. The instruction is `frintp`, round toward +∞, so it's `ceil`; Ghidra's pseudocode shows a truncating integer cast, which is wrong. The original multiplies and subtracts in separate instructions, with no fused multiply-subtract, so the `.cpp` splits it into two statements.

### `ensureFilledSlot`

Defined in `LauncherNode.hpp`; line 259 asserts `newClipDuration >= 0.0_beats`.
- **When it does nothing:** a zero duration, or a slot that `clipAt` already finds, whether through the slot or the temporary clip.
- **Otherwise:** the temporary clip becomes an empty clip for that slot:
  - region `{start 0, loop [0, newClipDuration], end newClipDuration}`;
  - no content, automation or groove.
- **Replacing:** a temporary clip for another slot is replaced. When one already exists, the original assigns its slot index and move-assigns the clip; otherwise it constructs one in place.

## `math::roundUpToMultiple`

The real function is in `shared/base/MathLib/include/ableton/math/MathUtils.ipp` and is inlined here, so its name is ours.

1. It returns the value unchanged for a zero quantum, then takes the quantum's absolute value.
2. A shared helper asserts `quantum > 0.0` (line 24), so only a NaN quantum can fail it. It then asserts `std::abs(quants) < kExactIntRangeMaxForDouble` (2^53, line 27) and returns `std::round(value / quantum)`. The named `roundToNearestMultiple` @0x920530 begins with the same two assertions.
3. The result is `lower = quantum * n`, stepped up to `quantum * (n + 1)` when `lower < value`.

This isn't always bit-identical to `std::ceil(value / quantum) * quantum` (about 2% of a random sample differ), so the rounding has to be reproduced exactly.

## Calls in the binary

- **`setState` through a stub:** callers reach `setState` through the import stub at 0x11f16a4. That stub is a weak-bound GOT entry (import 10406, empty name). Ghidra's pseudocode names the call after an unrelated identical-code-folded symbol, `WavetableDeviceOscillator<Oscillator2>::setUnisonVoiceCount`.
- **Inlined `play`:** `play` itself has no callers. Every `processCommand` overload and `launchPendingClip` inline it.

## Open points

- **`unknown18` in `PlayingState`:** possibly the mode (see above).
- **Port buffers:** the real `CvBuffer::emplace_back` throws `bad_variant_access` unless the buffer holds events (variant index at +0x28), and it drops a value equal to the last one. The stand-ins in `Stubs.hpp` record every event.
- **Lead for `launcher::State`:** `MidiClipsRecorder::onRtStateChange` reads a `RealtimeRecordingState` whose assertions mention `isDurationPending`, `isRecording` and `rtState.oClipFrame`. It may be built from `launcher::State` and could give the real member names.
