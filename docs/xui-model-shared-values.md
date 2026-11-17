# Native model Shared value foundation

The native model APIs now provide explicit identity-sharing storage and read
operations in addition to the unchanged DeepClone APIs. Graph configuration
set/get and command-set, Workflow type schema registration and variable default
registration each have a `Shared` counterpart.

This is the foundation for the approved default data-snapshot behavior: a
language boundary can create one independent snapshot, then hand that snapshot
to a retaining native store. The native store does not have to DeepClone the
new data graph a second time. These native APIs are advanced identity-sharing
operations, not a substitute for the language snapshot boundary.

Shared commands deliberately keep independent old/new history snapshots.
Changing the initial input can change current state, while undo/redo retains
its original data. Workflow descriptor metadata and strings are still copied;
only the schema/default xvalue is shared. Returned Graph values hold their own
native reference and can outlive the Graph.

`tools/check_xui_model_shared_sdk.py` links O0/O2 CPU clients against actual
fresh hosted/static O2 SDKs and the same compiler XRT. It checks reference
identity, ordinary Clone isolation, lifetime, checked Drop, history, allocation
failure and imports. A separate source-unit gate covers all 99 integer model
Busy entries and the original schema/XSON/widget/header regressions. Warm
Shared setters/getters use no XRT heap operations over 1000 iterations; CPU
reference counts and admission checks remain.

The existing `xl6ForeignDataSnapshotV1` converter handles native language
containers, but alone is not the complete safe model boundary: XRT's data-copy
fallback retains opaque handles, and that entry does not supply the compiler's
public-class field plan. Reuse the verified public-data projection machinery
for field visibility, without getters. A complete independent-data boundary
also needs to reject opaque resources/callables without a declared data
provider before publication, rather than silently retaining their identity.
Avoid a second validation traversal by doing this in the same graph copy.

The generic `@c_data_snapshot` declaration rule was subsequently approved on
2026-11-17 and remains to be implemented. The safe public language model API
and the final SDK/compiler/IDE/help delivery have not been promoted by this batch.
Evidence is indexed by
`dev/release-materials/provenance/xui-model-shared-values-20261117.json`.
