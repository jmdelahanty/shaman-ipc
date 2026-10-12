# Changelog

## v4.0.0 (2026-10-12)

- Extracted from Orange 52a840e. Layout revision 4 (`kSchemaVersion` 4,
  `sizeof(Slot)` 35368, `sizeof(SharedQueue)` 2263640).
- Slot fields `state_frame_id` and `source_frame_id` are named `state_stream_frame_id` and
  `source_stream_frame_id` (names only; same offsets).
- Field offsets asserted for Keypoint, Object, Slot and SharedQueue.
