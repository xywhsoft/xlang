# Borrowed current-module code capability

`@c_code_owner(slot)` inserts `const XL6_CodeOwner*` at one physical C argument slot without adding a language parameter. The generated argument is the bound module's code owner, or NULL for an explicitly process-resident static module. It is not an address-based module guess or a global runtime lookup.

The pointer is borrowed for the call. A cold native adapter that exports a callback or OS-visible object must acquire its scoped code lease during that call and retain the accepted lease for every exported lifetime. The lease is released only after native cleanup returns. A missing owner cannot silently stand in for a dynamic owner. This contract alone does not qualify any COM, clipboard or drag operation.

The parser, semantic checker and sealed IR enforce unique physical slots, complete signatures and collisions with receivers, packed inputs, spans, callbacks and output slots. Generated C/C++ checks prove the exact header signature. Direct calls, native function-pointer fields, bound methods and protocol witnesses all consume the same physical plan.

Sparse fixed inputs distinguish typed NULL from module code owner. Unknown kinds and a noncanonical owner pointer spelling are rejected before emitting C or headers. The node facts stay 88 bytes; each sparse fixed-input row adds a 4-byte kind to the previous typed-NULL row. Ordinary declarations have no additional row or generated work.

The focused O0/O2 acceptance runs the actual static and dynamically bound module paths, keeps a lease past module-unit cleanup, and checks final release. Windows and Linux each passed 29 new process steps and 44 existing nested-pack/NULL steps. It is a frontend overlay over the recorded complete source parent, not a full shipping rebuild. Malformed IR is tested before output publication. A late Linux fixture-root correction changes test inputs only; the earlier failed harness result is retained.

Run `python tools/check_foreign_module_code_owner.py --help` for the frozen-build and overlay arguments. The acceptance receipt records exact compiler inputs and reports.
