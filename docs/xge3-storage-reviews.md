# XGE/XUI native storage review witnesses

The declaration generator checks explicit reviews before emitting implicit
spans. A matching C signature does not establish whether a pointer escapes.
The review catalog is `xge3-xui3-api-map-storage.json`.

`storage_review_match` checks the reviewed entry function and optional
`dependencies`. A dependency contains a portable repository-relative
`sourceFile` and a byte-exact SHA-256. Missing, changed, malformed, duplicate,
absolute, parent-relative and external symlink/junction paths are rejected.
Both free-function span generation and callable-field annotation use this
checker. Non-strict inventory records a changed review and removes any prior
matching storage contract; strict generation fails.

These fingerprints detect changes to manually reviewed code. They do not
infer ownership, discover every transitive dependency, prove ABI correctness,
or certify executable behavior. Eighteen legacy callable-field reviews still
check the entry body only; their historical `sourceHeaderSha256` is metadata,
not an enforced current-header assertion. They have not been silently refreshed.

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
The initial isolated replay differed in ten binding source files. The fixed
OBB output follow-up below removes that regression; nine files still differ,
including checked title/provider APIs, callback views, snapshot records and
explicit constants. Those candidates remain external;
none is copied over the existing bindings or into `release/`. The next step
is to preserve and qualify these independently reviewed contracts in the
generator before any regenerated package can replace the current library.

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
