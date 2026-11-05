# Leased native domains and semantic finalization

`XL6_OwnershipDomainV1` keeps its original layout and existing behavior.
`XL6_OwnershipDomainV2` embeds that layout as its first member, followed by an
exact ABI tag and an immutable finalization query. Its prefix size identifies
the complete V2 record. Unknown sizes, tags and missing queries are rejected.

A native domain attached to a ForeignView has no `Load` callback. The view's
real physical and code owners keep that domain, its query and its returned
native metadata alive through the plan's final Drop batch. Closed and cleared
views remain in the physical directory until the last real pin is dropped.
No native policy is registered as a permanent resident object.

The query receives an exact native reference and its admitted adapter. A
successful query returns either the adapter's exact FinalizationV1 descriptor
or NULL when it has no continuation. Failure leaves the output NULL. Queries
run under Freeze and cannot allocate, change references, mutate diagnostics
or invoke language or semantic code. The runtime checks descriptor size, tag,
adapter identity and both callbacks before admitting it.

The catalog combines resident Load policies and V2 queries. ForeignView queries
each distinct leased domain once, regardless of how many roots share it. All
matching descriptors must agree by pointer identity. Malformed metadata,
query failure and conflicting capabilities reject admission; no first-match
priority or legacy-finalizer fallback is used.

Only cold graph admission and lifecycle planning use this extension. Ordinary
native method calls, copies, allocations and generated direct calls gain no
additional work. A semantic finalizer still reports failure separately from
completion: incomplete failures require explicit retry; pure waits can resume;
completed cleanup is never repeated merely because it reported an error.

`tools/check_ownership_leased_finalization.py` checks V1 regressions, typed
NativeSequence cleanup, leased-domain waits and failures, descriptor rejection,
capability conflicts, graph refresh, resurrection, thread affinity and zero live
allocation delta at O0 and O2. It reuses a verified complete source parent and
explicitly identifies the two rebuilt runtime units as a partial runtime gate.
