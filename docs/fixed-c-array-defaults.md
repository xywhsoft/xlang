# Fixed C-array default values

Fixed C arrays now have an explicit default-zero initializer contract. For
example, `c_int[3] counts` starts with three zero values, and
`Xge3dVec3[3] triangle` starts with three zero vectors. This applies to global
and local values, aliases, and fixed-array fields in external C records.
An ordinary language class can contain such an external record; its record
fields use the same C zero values. Direct fixed-array fields in ordinary
language classes remain unsupported and are rejected.

The semantic definite-assignment check admits these defaults. Typed IR records
`c-array-zero`, distinct from scalar, record and tagged-union initialization.
Lowering emits the existing verified aggregate-zero instruction for an exact
trivial C-array carrier. Both the IR and Cgen verifiers check that carrier's
type, ABI, extent, element parameter and ownership. The generated C uses its
ordinary zero-valued wrapper; it does not allocate a language container or call
a runtime factory. Existing initializer enum numbers are unchanged.

Managed elements, invalid extents, self-referential initializers and unrelated
unassigned local scalars gain no new permission. Copies remain independent
values, and accesses retain their existing checked bounds. Arrays saved across
`await`, `yield`, returned by value or captured in a closure keep the same
value semantics.

`tools/check_foreign_fixed_array_zero.py` exercises the actual complete O2
compiler, TCC, XRT, VFS and precompiled-resource products on Windows and Linux.
It checks typed/lowered plans, C11/C++11/C++17 public headers, JIT, standalone
EXEs, a shared library followed by checked retirement and unload, four negative
cases and six adjacent regressions. The manual
`lib/bindings/xge/three_d/examples/ray_queries.xl` lesson now fills a native
three-vector array and calls the existing triangle query directly. Its hosted
and static AOT forms use matching XGE SDKs; import inspection checks that the
static executable needs neither compiler DLLs nor the XGE host DLL.

Each platform has 46 successful outer commands, including four expected
rejections. The core fixture verifies 13 initializer plans and observes zero
XRT heap allocation/reallocation/live-object changes over 1000 loop-local
array declarations. It deliberately does not claim zero CPU work or measure
whole-program stack usage; constructing a zero array still writes its values.

An adjacent pre-existing test compared a default `time` with `#00:00:00#`.
Both the preceding full compiler and this build show that the former is
`time(0)` (0001-01-01), while the literal denotes 1970-01-01. The implicit and
explicit constructor tests now compare with `time(0)`; time semantics were
not changed. The first gate also omitted the existing array regression's
separately named C provider; the final gate supplies and pins that provider.
Earlier failed gate reports are retained as historical evidence.

The small receipt in
`dev/release-materials/provenance/fixed-c-array-defaults-20261117.json` pins
the exact source, builds, gates and independent verification. Large evidence
stays in the external validation tree. This is a compiler/XGE-lesson batch;
the final matched release/IDE delivery and the remaining full XGE integration
work remain open.
