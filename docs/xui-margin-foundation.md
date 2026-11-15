# Margin model native prerequisite

XGE commit `488d0126` adds the unique model owner and native scope foundation
needed before safe CodeMargin callbacks can retain language closures. CodeEdit
claims its model; the public borrowed pointer has no destroy right. Checked
destruction separates failure from consumption, watches expire during final
cleanup, and caller-owned scopes keep model storage alive across native tails.

Closing CodeEdit while a model scope is active consumes its model at the
Context tail, then returns INVALID_STATE from the model scope end. Context
close returning DEFERRED has not consumed its owner: end the scope and retry
Context close after quiescence. The native tests exercise both paths, nested
and copied scopes, wrong threads, quiescent tracing, invalidator failure and
130 CodeEdit constructor allocation budgets per O0/O2 hosted/static mode.

The first full O2 SDK builds 164 hosted and 166 static units per platform.
A packing-only successor rebuilds one hosted unit/reuses 163, and rebuilds
two static units/reuses 164. The model shrinks from its initial 64-byte candidate
to 56 requested bytes; the former unprotected model used 16. Creation still
allocates one object and the native scope uses 72 caller-owned bytes. Thread,
mutation-domain, Context and cleanup checks have costs. Prepared 1000-scope
native loops have no heap operations; no CPU equivalence claim is made.

Matched hosted/static packages are staged separately against the new SDK and
complete O2 compiler r40/r33. Existing CodeEdit/selection/theme and counted-text
clients pass C/C++ headers, JIT and independent dynamic/static AOT. Text reads
retain NUL, copy OOM keeps old values, and snapshots survive Context close.
The text client needs its existing separate CPU provider C source; the initial
gate omitted it and failed symbol resolution on both platforms. Those reports
remain historical; the corrected command explicitly supplies the provider.

This batch does not expose Margin as a callback-free language leaf. Native
owned render/event registration, independent invocation/layout snapshots and
their ownership edges remain necessary. Writable output arrays and internal
owner aliases retain their pending contracts. The declaration inventory now
contains 5201 native functions and 3831 language-declared function identities;
the eight new native support functions do not imply eight completed language
methods or a completion percentage.

See `dev/release-materials/provenance/xui-margin-foundation-20261115.json` and
upstream `docs/code-margin-lifetime.md` for exact evidence and costs. Unrelated
native pixel geometry edits remain outside the snapshots and commits. No
shipping release/IDE installation, version change, push, device, graphics or
network qualification is claimed.
