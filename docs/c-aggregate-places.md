# C aggregate value assignment

Declared C records and fixed C arrays are values. A field assignment such as
`model.items[i].vertices[j].x = value` updates the named root `model`; it must
not silently modify a discarded temporary.

Typed XLIR records a complete aggregate place through exact record fields and
fixed-array indexes. Ordinary assignment evaluates the RHS first, then each
index once from left to right. Every index is checked before evaluating the
next index. Lowering reloads the named root after these evaluations, rebuilds
the selected path, and publishes it once. If an index callback modified a
different field of that same root, the final update preserves that change.
An exception before publication leaves the target leaf unchanged; explicit
side effects of RHS/index code are retained.

Array bounds facts describe an exact fixed-array type and evaluated index.
They remain valid for a reloaded value of that same type. Verification requires
the successful bounds branch to dominate every consumer; an unchecked or
false-edge path cannot use the fact. Fixed extents are at most `UINT32_MAX`.
Unsigned keys beyond `INT64_MAX` normalize to negative bounds carriers and are
rejected rather than narrowing into a valid index.

The lowering plan exists only during compilation. It emits typed aggregate
get/set, fixed-array get/set and storage operations; it adds no heap object or
borrow/reference API to the language. Intermediate value copies and the normal
storage publication contract remain visible to C optimization. This is not a
claim that all aggregate assignments have identical cost to handwritten C.

Assignment through a temporary is rejected. Mutable closure capture of a C
record remains an explicit unsupported-cell diagnostic, covered by a negative
fixture; this change does not silently invent capture/reference semantics.
See `test/cases/foreign_aggregate_place.xl` and
`tools/check_foreign_aggregate_place.py` for the two-platform CPU gate.
