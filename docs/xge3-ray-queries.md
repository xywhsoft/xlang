# 3D ray queries

The `xge3d` source package now directly declares three CPU geometry functions.
It requires a matching native SDK with `XGE_ENABLE_3D=1`; no graphics context,
window or engine loop is needed by these queries.

| Language | Native | Result |
| --- | --- | --- |
| `rayAabb(ray, box, maximum)` | `xge3dRayAabb` | `(c_int status, c_float distance)` |
| `raySphere(ray, sphere, maximum)` | `xge3dRaySphere` | `(c_int status, c_float distance)` |
| `rayTriangle(ray, vertices, maximum)` | `xge3dRayTriangle` | `(c_int status, Xge3dHit hit)` |

The first result is the actual native status. `XGE_OK` means hit,
`XGE_ERROR_NOT_FOUND` means miss, and `XGE_ERROR_INVALID_ARGUMENT` identifies
invalid geometry. These are explicit status-returning queries, not boolean or
exception conversions. Check the status before using the second result. A miss
or invalid input clears the native output. Native directions are normalized;
distances use world units. The maximum must be positive and finite. Starting
inside a box or sphere returns a hit at distance zero. Triangles are double-sided.

`c_ref` supplies readonly input values for the single native call. The triangle
input is exactly `Xge3dVec3[3]`, using the existing fixed-array projection; no
temporary heap array, count guess, C forwarding function or new compiler rule
was added. Input value copies, tuple copies, code/call bookkeeping and native
geometry work remain real costs. No CPU equivalence is claimed.

The new beginner lesson, `lib/bindings/xge/three_d/examples/ray_queries.xl`,
constructs a ray, box and sphere, checks the two-unit hit distances, and verifies
a short ray misses. It uses public modules and no test helper or resource class.
The separate qualification fixture obtains a fixed vertex array from C and
exercises all three queries.

One general compiler gap remains: a standalone `Vec3[3] vertices` declaration
passes semantic analysis but lacks a verified default initializer in XLIR.
The original Windows/Linux failed lesson logs and its exact source witness are
retained in the receipt. Existing initialized fixed arrays, such as native
outputs or record array fields, work with the triangle query. The default
initialization rule is awaiting a decision; this limitation is not hidden by a
product-specific triangle constructor or a wrapper layer.

The final gates use actual Native b44 default-plus-3D hosted/static SDKs and
the qualified complete O2 compiler/XRT4641 products on Windows and Linux:

- 399 Windows and 400 Linux source/generator checks, including the three exact
  reviewed physical signatures, stale-declaration rejection and repeated replay.
- All 5573 complete units in the nine selected declaration files are preserved.
- 27 commands per platform: native C O0/O2 oracles in both linkage modes,
  exact generated C, C11/C++11/C++17 headers, JIT, dynamic/static executables,
  import tables and the beginner lesson in all three language modes.
- Twenty-six geometry scenarios test normalized/zero directions, misses,
  inclusive distance limits, inside origins, nonfinite inputs, invalid bounds,
  degenerate/reversed triangles and all independent hit-record fields.
- Equal warm loops perform 3000 native queries each with zero XRT allocation,
  free or reallocation operations and unchanged live counts/bytes. Setup and
  callable preparation are outside these intervals; OS heap activity and CPU
  throughput are not measured.

The immutable evidence index is
`dev/release-materials/provenance/xge3-ray-queries-20261117.json`.
The final source witness differs from the source-gate witness only in the
corrected lesson and its README; production declarations and generator inputs
are byte-identical. Other candidate working files are excluded. No native or
compiler implementation was rebuilt for these declarations. Normal `release/`
was not installed, and no GPU, network or security test ran. Remaining 3D scene,
resource and full integration work is still open.
