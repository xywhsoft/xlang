# XGE/XUI native storage review witnesses

The declaration generator checks explicit reviews before emitting implicit
spans. A matching C signature does not establish whether a pointer escapes.
The review catalog is `xge3-xui3-api-map-storage.json`.

`storage_review_match` checks the reviewed entry function and optional
`dependencies`. A dependency contains a portable repository-relative
`sourceFile` and a SHA-256 over either the exact bytes or LF-normalized text.
Missing, changed, malformed, duplicate,
absolute, parent-relative and external symlink/junction paths are rejected.
Both free-function span generation and callable-field annotation use this
checker. Non-strict inventory records a changed review and removes any prior
matching storage contract; strict generation fails.

These fingerprints detect changes to manually reviewed code. They do not
infer ownership, discover every transitive dependency, prove ABI correctness,
or certify executable behavior. Seventeen legacy callable-field reviews still
check the entry body only. Their historical `sourceHeaderSha256` remains
provenance metadata.

## Full strict regeneration at XGE bf81cd9c

Forty-six records required review against this revision. Historical witnesses
resolved to six native files. The XGE header adds clipboard declarations, and
the XUI header adds find-scope and provider declarations. The related types
and existing callable signatures are preserved. The CodeEdit source changes
find-scope attachment and command registration. The search source changes
range lookup; MarkAllRegex still uses the unchanged, capacity-checked error
writer. The text-draw dependencies changed only in the clipboard paths.

The two changed entry bodies are the caller-buffer clipboard reader and its
XUI proxy. The platform readers consume the destination synchronously and
limit writes to its supplied capacity. The proxy preserves byte counts and
maps the missing-format and short-buffer error codes to XUI. The refreshed
proxy review pins the platform-reader implementation and both public headers.

The source gate checks all 146 current records and locks every listed
dependency through the end of the run. Repeated generation was byte-identical
on each platform. All nine selected files preserve their
reviewed declarations, and the four package manifests remain byte-identical.
Matching hosted and static packages were staged outside the release tree.

[Validation evidence](../dev/release-materials/provenance/xge-storage-regeneration-20261122.json)

## Reviewed range drawing paths

The `xui_proxy_t.textDraw` review now covers the current RANGE branch. A missing
`drawTextShapeRangeSpans` review is also recorded. Each pins fourteen files,
including the public header, normalization, proxy range/paint helpers, glyph
run/shaping implementation and render command submission. Those files match
the existing qualified Windows/Linux Flow-config SDK source cohorts and the
current native repository. Unrelated dirty native text geometry files remain
outside this update.

The inspected paths have the following storage behavior:

- Text/context/language and span inputs are read in the synchronous call.
  Shaping clears temporary source cursors before publishing a retained run.
- A supplied Shape is borrowed for the call. Otherwise RANGE creates a local
  retained-paint Shape and releases it on completion/failure paths.
- Range painting borrows the glyph/caret arrays, copies spans to stack or
  temporary storage, restores clip state and frees the temporary storage.
- Render submission copies draw records and acquires real texture references;
  queued records do not retain input text/span arrays or the temporary Shape.

Existing declaration scopes keep the Font, Surface, DrawContext and optional
Shape admitted through the native call. This batch changes tooling/review
data only. It adds no language adapter, native implementation, runtime copies,
or runtime bookkeeping. It does not qualify every renderer error path.

## Replay and package manifests

The generator previously rewrote package manifests from obsolete seeds,
dropping separately maintained modules such as particles, SVG and font
resources. It now preserves existing source order and package metadata,
appends missing generated units, and rejects incompatible package identities,
ABI/kind or malformed source lists. A replay with all required units already
listed leaves the manifest bytes unchanged.

Run the source gate outside the working tree, using an exact native source
cohort and Python with `pycparser` installed:

```powershell
python -B tools/check_xge3_storage_reviews.py --root D:/GIT/x-lang `
  --native-root F:/path/to/qualified-native-source --out F:/path/to/new-output
```

The gate exercises stale/malformed reviews and changed helper files, strict
and non-strict annotation, manifest preservation, each real dependency
mutation, two complete strict generator replays, and the Page block. It pins
input bytes and verifies that the repository inputs did not change. Windows
may lack permission to create symlinks; that individual case is reported as
skipped and is exercised on Linux instead.

Passing this source gate **does not make the generated package deliverable**.
The initial isolated replay differed in ten binding source files, including
checked title/provider APIs, callback views, snapshot records and explicit
constants. The fixed OBB output work below addressed one early discrepancy.
Current preservation results are recorded in the strict regeneration section.

## Fixed output cardinality

`xgeShapeExGetOBB` and `xgeShapeExSceneGetOBB` both take a plain
`xge_vec2_t*` but synchronously write four contiguous corners. The generator
previously inferred a single `Vec2`, which could underallocate a native output.
Current library declarations already correctly use `Vec2[4]`.

The `fixedOutputEntries` catalog now records parameter index, element C type,
count, full native declaration, function body and implementation/header pins.
Generation requires these two established four-corner contracts before it
writes sources. Missing, changed or scalarized evidence fails; cardinality is
never guessed from `pPoints4` spelling. The regenerated complete `shapes.xl`
matches the existing correct file after UTF-8 line-ending normalization.
The gate records raw byte differences separately; Linux emits LF while an
existing Windows checkout may contain CRLF. Native dependency fingerprints
and unchanged package manifests still compare exact bytes.

The source gate checks both valid contracts and malformed/missing signature,
count, body, dependency and element metadata. The existing real-SDK OBB gate
also exercises all four corners, typed bound and protocol calls, retained
scene children and the readable bounds lesson through JIT/dynamic/static AOT
on both operating systems. Its emitted-C check requires contiguous `.Elements`
output; static import checks reject runtime/host DLL dependencies.

## Shared-model source synchronization

Four stale storage pins were reviewed against the immutable 09d Native SDK
source. `FontLoadMemory` still copies input into Face-owned bytes; the fallback
path builds that owned replacement and publishes it under the native spin lock.
Both reviews now pin the whole Font implementation, including the copy helper,
instead of relying only on their caller bodies. Root and frozen Font sources
are byte-identical after line-ending normalization.

The two text-drawing reviews depended on an older `xui.h`. A parsed comparison
preserves all 3,637 existing function declarations, 596 type shapes and 1,353
constants; only fourteen FlowGraph ownership functions are added. Their header
dependency pins now match the exact SDK. This is a source review, not a new
rendering or runtime performance measurement. Evidence is indexed by
`dev/release-materials/provenance/xge-storage-rereview-09d-20261116.json`.
