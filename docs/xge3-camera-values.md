# 3D matrix and camera values

`xge3d.transformMatrix`, `perspectiveCamera` and `orthographicCamera` directly
declare the corresponding XGE native functions. The compiler takes the exact
transform address or owns the complete output record, checks native status and
returns an independent value. No handwritten forwarding implementation or
boxed operation dispatcher is added.

Use an SDK built with `XGE_ENABLE_3D=1`. The CPU-only example is
`lib/bindings/xge/three_d/examples/camera_values.xl`; run it with
`xl run camera_values.xl` and the matched packages in `XLANG_PACKAGE_PATH`.
It verifies scaling, translation, perspective and orthographic matrices without
creating a window or GPU context. Plain record construction zero-initializes
fields; set the identity quaternion and scale explicitly as in the example.

Windows r29 and Linux r21 complete O2 compiler cohorts each pass 21 checks:
real JIT, dynamic and static executables for the fixture and example, independent
C++17 value ABI, and C11/C++11/C++17 exported header validation. Tests also
verify quaternion normalization, inline-array copy isolation, invalid projection
arguments and checked native failures. Static executables import neither the
XGE host DLL/shared library nor compiler/runtime DLLs.

The candidate native SDK uses XGE revision `1ba93e6b3` and the already qualified
canonical xlang XRT snapshot. Both SDK builders enforce the XRT fingerprint;
they do not embed another XRT implementation. The original whole-tree audit
failed because two inherited Harfbuzz dependency/cache receipts changed under
`build/`. Its report is preserved. A separate read-only audit confirms every
frozen file outside `build/` is unchanged; it records that exclusion explicitly.

Equivalent native C and language loops execute 1000 constructor sets with zero
XRT allocations, reallocations or frees. This does not measure CPU throughput,
stack copies or the cost of scalar conversions, value transfer and failure
checks. GPU rendering, models/scenes, owned 3D resources and shared-library
package initialization remain separate unfinished capabilities. The immutable
evidence index is `xge-three-d-values-20261112.json` under
`dev/release-materials/provenance/`. These SDKs have not been installed into the
normal release/IDE delivery.
