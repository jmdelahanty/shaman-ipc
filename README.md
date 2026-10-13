# shaman-ipc

The SHAMAN v2 wire format: the shared-memory structs, constants and protocol helpers that
Orange (writer) and Citrus (reader) must agree on byte for byte. Header only:
`include/shaman_ipc/shaman_v2_wire.h`, namespace `shaman_v2`.

Only the wire format lives here. Orange's queue writer and Citrus's reader stay in their own
repositories and include this header.

## Use

Pin a release as a git submodule and add the subdirectory:

```cmake
add_subdirectory(third_party/shaman-ipc)
target_link_libraries(<target> PRIVATE shaman_ipc::shaman_ipc)
```

```cpp
#include <shaman_ipc/shaman_v2_wire.h>
```

## Rules

- Every struct's size and every field's byte offset are `static_assert`ed. A change that moves
  anything fails to compile until those are updated, which is a layout change: bump
  `kSchemaVersion`, tag a new major release, and record it in agent-contracts.
- Renaming a field without moving it is a minor release (same `kSchemaVersion`).
- Field meanings, especially the frame ids, are defined in agent-contracts
  `citrus-orange/frame_identity_v1_2026-10-12.md`.
- `tests/layout_test.cpp` pins the helpers both sides compute (queue name, FNV-1a,
  index lock path). Run it: `cmake -B build && cmake --build build && ctest --test-dir build`.

## Reading the ring

The queue is a latest-state stream, not a log. It holds `kQueueSize` (64) slots; the writer
owns `head`, the reader owns `tail`, and a push into a full ring fails (the writer counts it)
rather than overwriting, so a ring nobody reads holds the oldest 63 unread slots. The writer keeps
publishing while no reader is attached, cannot see a reader attach, and resets the ring only
when a new writer starts (a new `producer_generation`). It never discards slots on a reader's
behalf, because a reset would race a reader that is still draining. Every reader therefore:

1. **Discards the backlog when it opens.** Move `tail` to the current `head` before the first
   read. Whatever is in the ring at that moment was written for an earlier reader or session.
   A slot pushed between the two loads is skipped too, which costs one update at open.
2. **Drops slots from another recording.** When the reader's session is bound to a recording,
   it keeps only slots whose `recording_identity_token` is that recording's token. This
   covers a backlog that outlives the open (a source shared with an earlier session), a writer
   restart, and a binding change. A reader with no bound recording keeps everything.
3. **Treats a new `producer_generation` as a new stream.** Frame and sequence counters restart
   with it; a change is not a gap or a regression.

Citrus implements this in `ShamanV2LiveStateReader::Open` and `Arena::AdmitsBoundRecording`
(Citrus PR #34, 2026-10-13).

## Updates per frame

A frame produces up to three updates, all carrying the same `state_stream_frame_id`: the base
update when the frame arrives, the detection update, and the pose update. A later update for a
frame supersedes an earlier one, and the first update of a newer frame supersedes all of them.
A reader that wants one row per frame keeps the latest update of the current frame and emits it
when the next frame starts.

`detection_status` and `pose_status` say where each stage stands for that frame:

| `pose_status` | Meaning |
| --- | --- |
| `kDisabled` | The camera has no pose stage. |
| `kNotRequested` | The camera has a pose stage, but this frame is not queued for pose (detection not run on it, or the frame was not handed to pose). |
| `kPending` | The frame is queued for pose; a pose update follows. |
| `kPoses`, `kNoResult`, `kFailed` | The pose stage's result for the frame. |

A pending state that is superseded by the next frame before its result arrives was never
completed; readers must not wait for it. Orange writes these values from commit 014137b
(2026-10-13); before that it wrote `kDisabled` on every base and detection update.

## Why plain structs

Orange writes fixed-layout slots straight into shared memory and Citrus reads them in place,
with no encoding or decoding per frame. Serialization formats such as Protocol Buffers add an
encode/decode step per message. Zero-copy schema formats (FlatBuffers, Cap'n Proto) would add a
code generator for evolution this layout does not need often.

## History

Extracted on 2026-10-12 from Orange `src/shaman_v2.h` at 52a840e (layout revision 4, the same
offsets as Citrus's copy `src/ipc/shaman/shaman_v2.h`, checked field by field).
