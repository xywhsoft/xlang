# SVG integration: native prerequisite batch

The authoritative `XGE_ENABLE_SVG` block currently has 68 public functions.
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

## Confirmed fixed-output gap

The existing Shape and ShapeScene `getOBB` declarations both return one Vec2,
although their native functions write four. With the complete Windows r17
compiler the generated C contains:

```c
xge_vec2_t native_raw = {0};
int native_status = xgeShapeExGetOBB(shape, tolerance, &native_raw);
```

Only C generation was inspected; this unsafe program was not executed. Exact
compiler/binding/output hashes are retained in the preflight index. The normal
native pointer signature alone cannot prove the declared storage extent.

An isolated probe replacing the return type with the existing inline value type
`Vec2[4]` is rejected by current semantic checking. Complete the general fixed
C-array output contract across semantic analysis, sealed XLIR, verification and
C/header generation, then migrate both old OBB declarations and SVG OBB. Generated
C must provide four contiguous native elements and pass their address directly.
Use the existing language fixed-array value semantics; do not create an XGE
forwarding function, reinterpret an undersized value or allocate a dynamic Array
for this fixed native output. Include direct/bound/protocol paths, strict headers,
damaged-IR rejection and actual Windows/Linux JIT/dynamic/static AOT acceptance.

Native prerequisites do not pass that compiler gate, safe paint invalidation,
automatic module-global resource/code lifetime, complete SVG or final delivery.
