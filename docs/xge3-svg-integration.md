# SVG integration: native prerequisite batches

At native prerequisite revision `5c4c30b4`, the authoritative `XGE_ENABLE_SVG`
block has 68 public functions.
Only the two legacy cache namespace declarations are present in the xlang
package; there is no complete safe Svg/SvgPaint API. This ledger does not mark
that family complete or qualify a release installation.

## Native lifetime prerequisite

The SVG prerequisite fixes are indexed in
`dev/release-materials/provenance/xge-svg-preflight-20261111.json` and the sibling
XGE repository's `docs/svg-lifetime-contract.md`.

Destroy/Clear share embedded-font cleanup. Active loading/traversal retain the
document, including provider return tails. Reentrant content replacement and
structural mask/clip replacement return Busy; nested read traversal and opacity
changes remain usable. Invalid/overflowing references are rejected, including
cache/embedded-image acquisition rollback. Picture now exposes its actual child
tree. UBSan additionally found a signed Base64 accumulator shift on a valid
project-owned embedded font; unsigned accumulation removes that undefined
behavior without adding a second decoder or changing the allocation bound.

Windows/Linux O0/O2 private aggregate tests, actual dynamic/static products and
strict C11/C++11/C++17 clients pass. Linux aggregate ASan/UBSan passes separately;
the remaining archive objects and host XRT are uninstrumented. Seven old issues
have bounded assertion controls, and the old signed shift has a valid-input
UBSan reproduction. Clone/load allocation sweeps keep a live source/empty
document and require exact heap equality, including error counters, after every
prefix. No GPU, audio device, window or network is used.

The native private document adds 8 requested bytes on x64 (9,408 to 9,416).
Reference/state guards add no owner-record allocation or lock. Existing parser,
file/provider, font and clone work remains, and no language overhead or timing
claim follows from these native checks. Public function signatures are unchanged;
only the aggregate is rebuilt against the original matched full r56 cohort.
Current-mainline XUI integration and final delivery still need their own gates.

## Native paint observation prerequisite

XGE `1811dd9c` adds `xgeSvgPaintLifetimeWatch`/`LifetimeUnwatch`, bringing the
native SVG function count to 70. Clear, reload and destruction detach observers
before notifying them and before resetting/freeing paint storage. Self/peer
removal is safe; Clear pins the document while an invalidator may release the
last external owner. Accepted replacement invalidates old paints even if a
later allocation fails. Invalid arguments preserve the old generation.

Full Windows/Linux O2 host/static builds compile all 146 native translation
units each. Authoritative aggregate O0/O2 checks, actual host/static C11/C++11/
C++17 clients, existing embedded-font sweeps and Linux aggregate ASan/UBSan pass
149 command rows, indexed in `xge-svg-watches-20261111.json`. The remaining native
archive and host XRT are uninstrumented in the sanitizer gate. The private
document adds 8 bytes (9,416 to 9,424); one caller-owned observer uses 32 bytes on
x64. Register/unregister are O(1), with no heap allocation or lock; notification
is O(observers), and Clear adds a native reference acquire/release pair. Existing
resource and language lifetime work remains. No timing/total-zero-cost claim or
release installation follows from this native prerequisite.

The complete r18 compiler still rejects a status/output borrowed owner-view
query without a rollback helper. The same declaration with a rollback helper
passes its control. This is a general interop gap: native SVG paint lookup does
not transfer a new native owner to destroy on admission failure. Complete that
borrowed-output contract rather than invent a no-op destroy helper or wrap every
SVG query. The C-generation-only probes never execute an unsafe program.

The approved public cache design is an explicit `SvgCache` object. It and its
returned documents follow the creator-thread rule; independent threads use
independent caches. Entries within one cache intentionally share documents.
Clearing/closing a cache releases its references while externally held documents
remain usable. This replaces unsafe process-global sharing in the safe language
surface. Its native prerequisite is implemented below; the safe language object
and its complete ownership/renderer policy still await their own gates.

## Native explicit cache prerequisite

XGE `05bc4768` adds `xgeSvgCacheCreate`/`Destroy`/`Load`/`Remove`/`Reset`, raising
the native SVG family to 75 functions. Shared internal algorithms serve both
explicit caches and legacy global calls. Active operations pin their cache
through provider/invalidation tails and reject recursive operations before
allocation; removing/clearing/closing a cache preserves independent document
owners. Legacy global Clear now detaches its old list before notifications and
preserves new entries published during reentry. The global API remains
caller-serialized; it is not a cross-thread safe language cache.

`xge-svg-cache-20261111.json` pins 198 command rows over complete Windows/Linux
O2 host/static products, aggregate O0/O2 cache/watch/lifetime regressions, actual
C/C++ clients, independent cache threads and Linux aggregate ASan/UBSan. Two
old-code assertion controls stop before unsafe reentry. Each 96-prefix allocation
sweep has 26 failures and 70 successes with exact heap equality. A cache requests
24 bytes and one creation allocation; hits allocate nothing. URI scans,
O(entries) lookup, parser/document/entry allocations on misses and reference/
state checks remain. Language-view costs have not yet been measured.

The full-tree inventory had copied two generated HarfBuzz `.d`/`.json` files.
Separate proofs preserve the original failed inventory reports and freezes,
classify only those generated outputs, and verify every other byte unchanged.
Both native builds had succeeded; no rebuild/source change was needed for the
inventory correction. Native/source material remains external. No release/IDE
was installed, and no complete language SVG/renderer/lifetime gate is claimed.

## Remaining public surface

| Native capability | Language design / acceptance still required |
| --- | --- |
| Create/Destroy/AddRef/RefCount/Clear/Clone | Owning Svg with the existing checked view/code lifetime protocol; no per-method business wrappers. |
| Load/LoadMemory/LoadCached/CacheInvalidate/CacheClear | Exact input lengths/path validation and checked errors. The native explicit cache is ready; bind the approved creator-thread SvgCache object and preserve independent document owners. The old native global cache must not silently become cross-thread language views. |
| Size/Origin/ViewBox/Aspect/Viewport | Direct typed scalar/record and multiple output projections. Preserve ordinary not-found/not-set distinctions. |
| Paint lookup/owner/parent/children/traversal | Owner-bounded SvgPaint views with native invalidation before reload/Clear/destruction; existing generic observation/callback machinery, not unowned point handles. |
| Paint identity/name/transform/opacity/visibility/blend | Direct calls and copied text output, with checked owner/thread/expiration admission. |
| Mask/clip/Shape getters and setters | Actual retained native references, output ownership and correct partial failure cleanup; avoid hiding unowned pointers in plain records. |
| Bounds/OBB/intersection/contains | Exact records and a four-Vec2 inline output. A single Vec2 is insufficient. |
| Draw/DrawPx | Same native renderer/context/thread contract and real pixel acceptance. |
| Rasterize URI/memory | Writable output capacity, stride/extent/overflow checks and controlled failure; no arbitrary byte string masquerading as mutable storage. |
| Texture load / Ex / memory / memory Ex | Existing stable owning Texture and creation flags, validated data extents, renderer ownership and real pixels. |

## Fixed-output migration

The old Shape and ShapeScene `getOBB` declarations returned one Vec2, although
their native functions write four. The complete Windows r17 compiler generated:

```c
xge_vec2_t native_raw = {0};
int native_status = xgeShapeExGetOBB(shape, tolerance, &native_raw);
```

Only C generation was inspected; this unsafe program was not executed. Exact
compiler/binding/output hashes are retained in the preflight index. The normal
native pointer signature alone cannot prove the declared storage extent.

An isolated probe replacing the return type with the existing `Vec2[4]` was
also rejected by that compiler. The general fixed C-array output contract now
covers semantic analysis, sealed XLIR verification and C/header generation.
Both old declarations return `Vec2[4]`; Cgen reserves four inline points and
passes `native_raw.Elements` directly. No XGE forwarding function, undersized
reinterpretation, Buffer or dynamic Array is introduced. SVG OBB will use the
same contract when its safe paint surface is admitted.

`foreign-fixed-output-20261111.json` indexes complete Windows r18/Linux r10 O2
compiler builds, including TinyCC, XRT, VFS and the embedded compiler. On each
platform, 36 generic checks and 14 real XGE checks pass, plus 41 Windows and
40 Linux readonly sequence regression checks: 181 command rows in total.
The extra Windows row inspects PE imports. They include direct/bound/
protocol/native-field calls, mixed outputs, status-failure isolation, all four
rectangle corners, actual dynamic/static products, nested JIT/AOT compilers,
strict C11/C++11/C++17 headers, an actual C++ shared-library client and rejected
ABI declarations. The separate O0 frontend damage executable rejects 54 sealed
IR mutations before C/header bytes; it is explicitly not a full compiler build.

A 1,000-call hot loop adds no XRT heap allocation/free/reallocation. Complete
inline zero initialization, ordinary value copies, callable preparation and
managed receiver/error/code-lifetime work remain. There is no timing comparison
or total-zero-overhead claim. C pointer type proof cannot prove cardinality;
the declaration author must still match the documented native output count.
The runnable `xge/examples/shape_bounds.xl` demonstrates the fixed result without
opening a window or initializing a graphics context.

This closes the fixed-output compiler/binding gate. Safe paint invalidation,
cache ownership/concurrency, retained outputs, actual SVG rendering, automatic
module-global resource/code lifetime, complete SVG and final matched delivery
remain separate gates. No release/IDE installation is qualified by this batch.

## Borrowed pointer and C-string output prerequisite

The generic compiler now distinguishes physical output storage from logical
result admission. `c_output` plus `c_result("owner_view",source)` borrows an
existing native root/child without pretending to construct it or requiring a
destructor rollback. An unsuccessful producer's output can be unspecified and
is never observed. Owning factories still require their original rollback.
This supports the native SVG paint/owner/parent/child pointer-output pattern.

`c_output_view(slot,"cstring")` maps `const char**` to an independent owned
string, using the existing terminating-text admission while input scopes remain
active. It supports nullable/empty values, getters and mixed/converted tuples;
a failed later copy releases earlier copies without changing prior assignments.
It cannot preserve text after an embedded NUL; counted `xstrview*` outputs retain
their separate length-aware contract. Neither feature adds a SVG wrapper.

`foreign-output-ownership-20261111.json` indexes complete Windows r19/Linux r11
O2 builds with TinyCC/XRT/VFS and the embedded compiler, and 188 command checks.
They exercise JIT, independent static binaries, strict C/C++ generated headers,
actual C++ shared-library clients, embedded JIT/AOT compilers, negative ABI and
semantic declarations, and existing managed/retained/counted/fixed-output paths.
Separate pinned O0 frontend executables reject 172 damaged-IR variants before
any C/header bytes. Their scope is explicitly different from the full builds.

Borrowed cold admission still allocates observation/control records when needed.
The 1,000-call repeated-exposure loop adds no XRT heap operation. For C-string
outputs, 200 result materializations/drops allocate and free exactly 200 times,
with no reallocations, plus a NUL scan and copy each. Existing ordinary string
comparison can add operand copies: an earlier comparison-containing loop used
600 allocations for the same 200 producer calls. The isolated counter does not
claim to remove that compiler cost or establish total zero overhead. Repeated
64-budget string/tuple failure sweeps return to the exact warmed heap baseline.

This closes only these output-contract prerequisites. A bounded native CPU
control also confirmed that SVG font-cache growth moves an externally retained
Font record; it stops before stale-pointer access. Native stable font storage,
font/raster GPU-cache engine ownership, optional clamped byte-prefix mapping,
safe complete SVG declarations/callbacks, actual rendering and final matched
delivery remain open. The approved explicit cache keeps shared documents within
one creator-thread cache and independent caches across threads. No release/IDE
installation or complete SVG acceptance is claimed here.

## Stable native cache storage and real GPU acceptance

XGE `beefc241` fixes moving cached Font records and gives cached raster/atlas
textures real native GPU ownership. GlyphRuns retain stable Fonts and Faces
after document Clear/destruction. CPU-only font storage survives Init/Unit and
does not itself reserve the renderer. Cached/queued GPU textures do reserve it.
Font statistics are atomic and survive engine-state resets. Embedded-face OOM
keeps source bytes for retry; text loading/bounds/drawing propagate allocation
errors instead of silently changing fonts or returning zero advance.

`xge-svg-font-cache-20261112.json` indexes 331 passing command rows from complete
Windows/Linux O2 host/static and Linux EGL cohorts, aggregate O0/O2/debug,
actual C/C++ clients, allocation sweeps, retained resources and regressions.
Windows OpenGL and headless software EGL read back the same 64 red and 33 white
pixels. Aggregate tests also queue a public draw of the actual cached Texture,
then drop the last document before flushing. Four bounded old-code assertion
controls reproduce moving records and swallowed OOM; no stale pointer access
is executed. Linux sanitizer instrumentation covers the aggregate, not the
remaining native archive or host XRT.

Stable font storage adds one allocation/free per cached Font. Full occupied
cache storage requests 240 rather than 232 bytes per font on x64; unused entry
capacity shrinks. CPU-only raster storage shrinks by 40 bytes; materialized GPU
storage adds a separate 48-byte Texture record. Owner/thread checks and atomic
statistics remain real native costs. A 1,000-hit loop adds no XRT heap operation;
no language timing or total-zero-overhead claim follows.

Independent-thread tests use independent embedded Faces. Shared mutable global
fallback Faces still need caller serialization; safe language fallback/thread
admission must be qualified. This native prerequisite does not add the complete
Svg/SvgPaint/SvgCache declarations, clamped byte-prefix mapping or final matched
delivery. No release/IDE files were installed.

## Clamped input and output-region prerequisites

`3958b1858` adds the general adjacent `int64` byte limit to `c_span`: `-1`
selects the complete input, nonnegative values clamp to the real byte length,
and other negatives reject before native entry. The direct call still has one
pointer/count pair. Declaration facts remain 88 bytes; only sparse parameter
rows add four bytes on x64. The preflight records 113 command checks and 130
damaged-IR rejections. The later complete Windows r20 O2/TCC/XRT/VFS build
passes 120 checks, including its embedded JIT and independently built compiler
host. Linux r12 subsequently passes the same 120 checks. The combined
`foreign-span-prefix-20261112.json` pins 240 checks and verifies both compiled
source overrides against `3958b1858`. These basic-prefix cohorts do not qualify
the later region-composition change.

`46a609352` composes that input with a fixed output pixel region in the shared
physical-signature walk. The adjacent logical limit is consumed before mapping
the region pointer, dimensions or stride; pointer/count members cannot become
unrelated scalar selectors. No SVG-specific forwarding function is added.
`foreign-span-region-prefix-preflight-20261112.json` records 17 O0 frontend
checks and 55 damaged-IR rejections. Strided output preserves padding and length;
bad prefixes/capacity/stride preserve the destination and do not call native.
Field/bound/protocol paths, exact C/C++ headers and a real shared-library client
pass; 200 warmed field calls succeed with a zero XRT allocation budget.

The composition still needs integrated O2/embedded-compiler qualification.
Safe Svg/SvgPaint/SvgCache declarations, callback/owner scopes, fallback/thread
admission and final matched delivery remain separate gates. No release/IDE
installation or full SVG completion is claimed by these compiler prerequisites.

## Optional limits after required arguments

SVG memory/raster interfaces need optional `size=-1` after required dimensions,
outputs and stride. The general `c_span` mapping now accepts a distinct limit
at any explicit argument index, including before its input. Several inputs can
share one limit, clamped independently; the physical signature omits that limit
only once. This is compile-time mapping rather than a new SVG forwarding body.

`foreign-span-limit-positions-preflight-20261112.json` records 149 passing O0
frontend command checks, 96 damaged-IR rejections and a bounded old-compiler
rejection control. JIT, static and actual C++ dynamic clients cover leading,
trailing, shared and interleaved limits, including output regions and method
receivers. A warm 4,000-native-call loop changes no XRT heap counters. Metadata
sizes remain 88-byte facts and 696-byte sparse rows. Complete O2/Linux/embedded
acceptance remains for the integrated cohort; no release/IDE files are changed.

Explicit creator-thread `SvgCache` remains the approved public model. A separate
root-owned, non-independently-destructible node-view lifecycle proposal is still
pending confirmation. The old resident runtime rejects it before attaching a
native observer. That proposal is not implemented by the span changes above.

## Shared fallback Face prerequisite qualified

XGE `952eb0f9` closes the shared-Face race: atomic Face references, immutable
outline-list publication and protected fallback snapshots/replacement. A
snapshot holds the selected Face while the Font is constructed outside the
global slot lock. Init keeps its reset behavior and releases the old fallback;
independent document Fonts continue to use their retained Face. Mutable Fonts,
families and engine objects keep their existing caller/thread contracts.

`xge-shared-font-faces-20261112.json` pins complete Windows/Linux O2 host/static
builds (584 translation units) and 255 passing command checks. Linux TSan also
instruments canonical XRT allocator/locks/atomics; remaining archive objects
are uninstrumented. The old implementation reproduces the reference race in
the same instrumented environment. Nonempty outline, fallback replacement and
acquisition OOM paths retain/retry/clean correctly. Existing SVG ownership,
cache/reset and C/C++ public clients pass; Linux verifies 12,915 input-file
occurrences after acceptance.

Font source bytes remain shared. Linux x64 Face storage shrinks 280 to 272
requested bytes; 1,000 warm outline hits perform no XRT heap operation. Two
atomic pointer loads per hit, atomic Face references and a cold fallback slot
lock/temporary reference are real native costs. This is not an end-to-end speed
or zero-overhead claim. Complete language declarations, confirmed node-view
semantics, integrated compiler gates and matched final delivery remain open.
