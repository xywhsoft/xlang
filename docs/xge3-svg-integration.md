# SVG integration: native prerequisite batch

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

## Remaining public surface

| Native capability | Language design / acceptance still required |
| --- | --- |
| Create/Destroy/AddRef/RefCount/Clear/Clone | Owning Svg with the existing checked view/code lifetime protocol; no per-method business wrappers. |
| Load/LoadMemory/LoadCached/CacheInvalidate/CacheClear | Exact input lengths and path validation, checked errors and full cache threading/identity review. The native global cache is caller-serialized; independent language views must not silently permit races on one cached document. |
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
