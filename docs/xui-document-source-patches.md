# Document source-patch preparation

`Document.prepareSource` and `prepareSourceBytes` start a source-patch candidate;
`continueSource` and `continueSourceBytes` extend the current candidate. They
call the real `xuiDocumentPrepareSource` / `xuiDocumentPrepareContinueSource`
once, with no product-specific C forwarding function. Native validation,
revision, cancellation and creator-thread rules remain authoritative.

```xl
auto candidate=document.prepareSource(starts=[uint64(0)],ends=[uint64(3)],text=["new"])
auto next=document.continueSource(candidate,[uint64(4)],[uint64(7)],["text"])
candidate.close()
next.run()
auto changes=document.preparePublish(next)
changes.close()
next.close()
```

Starts and ends are byte offsets in the candidate's current source. The three
columns use the common array projection rules: `size=-1` requires equal lengths,
nonnegative size selects a bounded common prefix, and other negatives fail
before native entry. The initial descriptor exposes domain (default 2), base
revision, origin and group before the named columns. Their defaults allow
`prepareSource(starts=...,ends=...,text=...)`; an empty call has zero patches.
The Bytes forms still supply native UTF-8 source, not arbitrary binary source.

Projection retains the actual array storage and original text/bytes pointers
through the synchronous call, sets every patch's `iSize`, and uses actual byte
lengths. It neither scans for NUL nor copies text. Native code copies accepted
source before return, so callers may subsequently change the input arrays.
Temporary C records and read pins add work: see
[array projection costs](foreign-array-projections.md). This batch makes no
raw-C CPU-time or whole-program zero-overhead claim.

A successful continuation supersedes its previous candidate. Closing that
old handle does not cancel the new candidate. Invalid input or allocation
failure must leave the current candidate usable; cancellation, stale candidates,
expired handles and wrong-thread language access fail checked. `run` prepares
the candidate and `preparePublish` commits the accumulated edits as one undo
step. The readable window-free lesson is
`lib/bindings/xui/examples/document_source_patches.xl`.

The CPU-only gates are:

- `tools/check_foreign_array_slices.py`: exact bytes/lengths/sizeof, retained
  pointer identity, string/bytes overloads, pack composition, callback read pins,
  failed native/callback cleanup, 32 language allocation budgets, warmed
  allocation comparison, and invalid declaration/native-header rejection.
- `tools/check_xui_document_source_patches.py`: actual complete O2 compiler,
  JIT/dynamic/static AOT, C11/C++11/C++17 public headers and import tables; both
  the lesson and boundary script. The latter covers all four methods, retained
  source, options, stale/invalid/cancelled/expired cases, thread admission,
  metadata, undo/redo and 32 language allocation budgets.
- `tools/check_xui_document_source_native.py`: independent native C oracle,
  hosted/static SDK at O0/O2, source-copy/candidate/metadata/undo rules and real
  custom-allocator failure prefixes with retry and balanced payloads. The raw
  diagnostic allocator is not exposed as language Owned provenance. Language
  XRT budgets do not intercept all native CRT allocations.

Windows full r42 and Linux full r35 pass 164 focused process checks in total,
in addition to both complete O2 compiler builds. The dated receipt
`foreign-array-slices-document-source-20261115.json` records exact
tool/source/compiler/SDK/package/log hashes.
This is incremental source/package qualification. Remaining XGE/XUI/Document
coverage and the matched release/IDE/help installation remain active; this
batch does not install binaries, change current-delivery, bump a version or push.
