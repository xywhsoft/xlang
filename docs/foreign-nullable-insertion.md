# Contextual null insertion

Array/Set insertion consumes an owned erased value after applying the declared
element type. A literal `null` has no standalone C storage type. An all-null
conditional also needs its destination to choose a physical representation.
Rejecting either source because its original C ABI is unsupported prevents
otherwise valid `array<Menu?>.push(null)` and `set<Menu?>.add(null)` calls.

The compiler keeps the original intrinsic identity, receiver/operand/result
checks, element compatibility and Set key recipe checks. The final lowering
check now validates the actual materialized xvalue carrier. An expression with
no standalone C storage uses the existing conditional lowering with the
boundary's target type. Its condition executes once; only the selected branch
is evaluated. A failing condition never reaches the append operation.

The open `var` domain also admits canonical null explicitly. This uses the
module's runtime null TypeId, builtin kind, empty parameter list and unsupported
standalone ABI; other domains still require their existing boxing recipes.
Nullable types keep their exact base/subtype rules. This grants no numeric
widening, nullable unwrapping or conversion of null into a nonnullable element.

There is no new runtime helper, XGE forwarding function, native ABI or public
language rule. Generated insertion still uses `xl6ArrayValueAppendTakeV1` and
`xl6SetValueAddTakeV1`. Existing boxing, owner transfers, hash/key operations and
container growth retain their costs; this is not a zero-allocation or CPU
equivalence claim.

The focused tests cover literal/all-null/typed-null expressions, condition
evaluation and failure, foreign owner/refcount views, numeric/string/bytes/var
domains, and Set duplicate/null-versus-empty behavior. The owned CPU fixture
returns resource-bearing containers across function boundaries, clears holders,
checks invalidation after Context close, and calibrates every allocation budget
for a sequence of individual null appends. Each failing append preserves its
previous element count; earlier successful appends are not rolled back.
Complete function retirement must restore exact heap/resource counters.

`tools/check_foreign_nullable_insert.py` requires a complete O2 compiler with
TCC, XRT and generated VFS resources. It checks C/C++ headers, JIT and dynamic/
static AOT, negative semantic admission, the existing nullable-container matrix,
and embedded compilation with recovery after an error. Partial frontends are
diagnostic evidence only. The old complete-cohort nested executable is retained
as an expected-failure control. Final qualification is recorded separately in
`dev/release-materials/provenance/foreign-nullable-insertion-20261115.json`.

The matched native SDK is reused. This change does not settle the pending
output-record string, alias, found/partial-result or other foreign lifetime
contracts, nor does it establish full XGE/XUI/Document completion or delivery.
