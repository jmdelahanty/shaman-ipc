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

## Why plain structs

Orange writes fixed-layout slots straight into shared memory and Citrus reads them in place,
with no encoding or decoding per frame. Serialization formats such as Protocol Buffers add an
encode/decode step per message. Zero-copy schema formats (FlatBuffers, Cap'n Proto) would add a
code generator for evolution this layout does not need often.

## History

Extracted on 2026-10-12 from Orange `src/shaman_v2.h` at 52a840e (layout revision 4, the same
offsets as Citrus's copy `src/ipc/shaman/shaman_v2.h`, checked field by field).
