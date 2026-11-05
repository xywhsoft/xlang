# DataObject and atomic multi-format clipboard: outstanding contract

No DataObject/clipboard implementation is added by this assessment. The desired
operation is a meaningful native transaction, not a loop of language wrappers:
prepare one complete multi-format snapshot, then call `clipboardSetItems` once.
A failure before the commit must call the backend zero times. Tests can use an
in-memory backend without touching the OS clipboard.

## Current source contract

`D:/GIT/xge/src/xui/core/xui_drag_drop.c` owns the complete implementation:

- Lines 12-27: a slot holds copied format/data or raw `onRead/onFree/pUser`; the
  object has plain `int iRefCount`, slot array, count and capacity.
- `xuiDataObjectAddRef/Release`: ordinary non-atomic RC, with no creator-thread
  check, native ownership graph, watch or checked retirement protocol.
- `xuiDataObjectSet`: copies immediate bytes before publishing the slot.
- `__xuiDataSlotPrepare`: an existing slot runs its old `onFree` before resetting
  its fields. That user callback can reenter the same object.
- `SetProvider`: installs raw callbacks/userdata with no Owned Hold/Drop or
  language closure/code edge.
- `FormatAt`: returns a borrowed pointer into mutable object storage.
- `Get`: calls a raw lazy provider directly, without a snapshot/read carrier or
  protecting the slot against provider replacement.

The public declarations in `include/xui.h` describe copy/read/capacity behavior,
but do not grant cross-thread use, arbitrary reentrant mutation, or a language
callback lifetime. Based on these implementation facts, a C caller must arrange
serialization and provider/object lifetime; that inference is not a new safe
language contract. Existing raw callers remain C interoperability.

## Why a local snapshotDepth patch is insufficient

Holding one object RC and rejecting Set during snapshot would address one
reentrant slot replacement. It does not make the plain RC thread-safe, protect
provider code/userdata, expose actual graph edges, establish creator affinity,
report cleanup failure independently from consumption, or settle raw borrowed
format pointers. A language DataObject class cannot be labeled a leaf or caller
view to bypass those questions.

The whole data-object lifetime needs a deliberate native contract before safe
language exposure. Immediate copied bytes are straightforward; lazy providers
need exact typed Owned callbacks and real ownership/code edges. Concurrency,
final semantic cleanup, native graph admission and lifetime of drag sessions
holding the object must be audited together. Reuse approved native ownership and
checked-retirement mechanisms instead of inventing a clipboard-only queue.

## Proposed transaction after that prerequisite

A native `xuiProxyClipboardSetDataObject(proxy,data)` can acquire genuine source
and provider lifetime, validate every format, materialize every immediate/lazy
payload into independent snapshot storage, verify all length/overflow boundaries,
and invoke `proxy->clipboardSetItems` exactly once. Lazy required-size queries
and writes can legitimately disagree; capacity growth/truncation must reject the
transaction rather than publish a partial clipboard. Backend input is borrowed
for the single call unless its explicit native contract says otherwise.

Copies and allocations must be measured honestly: descriptor storage, format
strings and payload bytes form a real snapshot; lazy providers usually need a
size query plus a write call. No zero-copy claim or exact allocation count is
made before that implementation exists. The safe nested descriptor-array
projection decision and the complete DataObject contract should be resolved
before this is marked delivered.
