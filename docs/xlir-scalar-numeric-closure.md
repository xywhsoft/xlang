# Scalar numeric and C value-place closure

The XGE binding fixtures exposed accepted language operations that did not have
a complete XLIR lowering path. `float32` binary arithmetic was missing from a
numeric ABI predicate; compound arithmetic admitted `float` but omitted
`float32`, `c_float` and `c_double`. Typed Array/List compound reads also lacked
the exact scalar projection used by ordinary reads. Narrow language integer
bitwise compounds were missing from their existing normalization path.

The correction uses the existing typed operator, checked projection and
destination transactions. Cgen gains no semantic inference or special XGE
wrapper. Local floating operations retain their declared width, including IEEE
rounding, signed zero, infinity and NaN. Exact exported signatures remain `float`
or `double` as declared. Native C integer overflow/division/shift policy is a
separate pending language decision and is not changed in this batch.

Named C aggregate places now use the same transaction for ordinary and compound
assignment. Keys are evaluated and checked once, the previous leaf is read
before the RHS, and a successful RHS commits into the latest root value.
Reentrant changes to unrelated fields survive. An invalid index prevents RHS
evaluation; a throwing RHS commits no leaf update. This uses typed value
reconstruction and bounds proofs, without exporting an interior reference.
Read bounds errors and assignment errors have distinct canonical XLIR effects.

The source gate `tools/check_xlir_scalar_numeric_closure.py` exercises local,
scripted field/property, typed Array/List, C record and fixed-array targets;
single evaluation; reentrant writes; throwing RHS; out-of-bounds behavior;
exact C callers; O0/O2 generated C; JIT; three public-header standards; and four
negative declarations/operations. A separate sealed-IR executable checks 42
mutations and 84 empty C/header refusals at each optimization level. Windows
and Linux source preflights pass all 19 commands each.

Source validation rebuilt the two frontend units with the updated aggregate
include; it did not rebuild shipping fixed objects, VFS or embedded compilers.
Evidence is pinned in
`dev/release-materials/provenance/xlir-scalar-numeric-preflight-20261112.json`.
Complete matched O2 builds are a separate acceptance step.

Typed collection reads retain their existing owned-value projection and cleanup
bookkeeping; C aggregate transactions retain ordinary value copies and stack
temporaries. These checks establish semantics and ABI, not a total CPU or
zero-overhead performance claim.
