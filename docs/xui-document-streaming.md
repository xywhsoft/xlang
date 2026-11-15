# Document source streaming

Start a candidate with `document.prepareStreamSource(null, utf8="...")`.
The first call can set `baseRevision`, `origin` and `group`. Append to that
candidate with `document.continueStreamSource(previous, "...")`, then `run()`
and `preparePublish()`. Continuation inherits the first options. Only publication
changes the live Document and its history. Superseded candidates stay owned
until released, but cannot be continued or published as current work.

The existing start declaration packs a real `xui_doc_txn_desc_t`. Passing a
non-NULL previous handle through that declaration always supplies both previous
and descriptor, which the native API rejects. The new companion declaration
supplies the exact typed NULL descriptor required by native continuation. It
uses the already implemented const-pack/typed-NULL compatibility rule; the
compiler, IR, runtime and native function were not changed. Both declarations
make one direct call to `xuiDocumentPrepareStreamSource` and use its ordinary
owned output factory. No C forwarding function or operation dispatch is added.

The original unsupported combination remains a checked error. Use the companion
method for continuation, including an empty final chunk. Native failures leave
the previous candidate intact. A successful continuation supersedes it. Native
publication checks the Document identity, current generation, base revision,
parse state and cancellation. The test also checks retention after input-string
replacement, first-origin inheritance, parsing before continuation, stale and
cancelled handles, one-step undo/redo and closed-Document rejection.

The safe language handles keep their existing creator-thread admission. The
separate native fixture retains a real prepare reference and runs parsing on a
worker, then publishes on its Document's owner thread. That native result does
not certify cross-thread access to language ForeignView handles.

The native fixture additionally splits the UTF-8 bytes of `中`, rejects malformed
or prematurely finalized tails without superseding the candidate, and overwrites
input storage after the successful call to prove independent source retention.
The current string binding does not claim a bytes/Buffer input API. A trial
combining those span profiles with NULL projection exposed a separate declaration
composition limit; it was preserved externally and not installed. Bulk source
patch input remains separate: per-record sizeof plus pointer/actual-byte-length
field projection is still needed. Append streaming does not replace patch edits.

Allocation coverage is split explicitly. Thirty-two XRT budgets per language run
exercise factory admission, checked cleanup and preservation of the old candidate
when the language call fails. Native Document payloads normally use CRT malloc,
so these counters cannot prove native payload OOM coverage. A separate raw,
custom-allocator diagnostic injects every native continuation allocation prefix:
16 fail and the next succeeds, with all diagnostic payloads released. It is not
promoted to an Owned language root; default Owned-provenance semantic and worker
cases execute separately. XRT live/invalid/double-free counters are also checked.
This does not claim interception of every CRT allocation in the default host.

Warm 1000-info-query language/C comparisons have zero XRT heap operations under
zero allocation budget. The 72-byte output record is copied from stack storage;
the native implementation reads atomics/immutable source sizes without an
allocator call. The first descriptor is 32 stack bytes; continuation supplies
NULL. Factory/control/code/domain ownership, input lifetime protection, thread
checks and checked error handling remain costs. Native source copying and parsing
allocate. No complete cold-allocation parity or CPU-equivalence claim is made.

`tools/check_xui_document_streaming.py` runs the positive case and the linear,
window-free `lib/bindings/xui/examples/document_streaming.xl` lesson in JIT,
dynamic AOT and static AOT, with C11/C++11/C++17 public headers and import checks.
`check_xui_document_stream_gap.py` records the earlier package's missing safe
continuation as a historical diagnostic: its `productFixed=false` is deliberate
and must not be described as acceptance of the new entry point. Native O0/O2
hosted/static conformance and custom allocator diagnostics are pinned there.
The generator's `--check` also proves the new declaration is reproducible.

The dated provenance receipt is `xui-document-streaming-20261115.json`. This is
incremental source/package qualification. Full XGE/XUI/Document coverage and the
final matched compiler/SDK/IDE/help installation remain active; this batch does
not install release binaries, update current-delivery, change a version or push.
