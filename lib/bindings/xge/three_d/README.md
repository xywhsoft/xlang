# XGE 3D declarations

Use a matching _xge_native SDK with XGE_ENABLE_3D=1. Current safe CPU value
entries cover defaults, transform/perspective/orthographic values, large-world
relative positions and screen rays. Native POD names retain the Xge3d prefix.
The full scene/model/GPU/resource and query families remain in development.

createDecodeBudget(uint64 bytes) creates one real shared XRT quota.
DecodeBudget.info() returns the native sized statistics record;
setLimit(bytes) rejects zero and a limit below live charges. share() creates
an independent native owner. Closing one view invalidates its ordinary
language aliases, while a shared view and charged native blocks remain valid.
close() is a checked void operation, consistent with other owned resources.

See [the CPU-only example](examples/decode_budget.xl). It needs no window, GPU
or model file. The quota counts live XRT allocations inside bounded native
loads; it is not process or GPU memory measurement. Model/loader attachment in
the safe language API is still pending. No raw pointer is used as a placeholder
for those missing APIs.

Business methods declare the original native functions directly. The cold
policy only releases the one transferred native owner. This CPU leaf has no
language callback edges or native CallBegin/CallEnd pair; the resident root's
existing rule defers an active close until its generic C scopes have returned.
No scope-only policy flag or new runtime semantics is required.

Both platforms pass JIT and independent dynamic/static case/example programs,
C11/C++11/C++17 headers, real live-charge/independent-share/active-close checks
and 64 managed publication OOM budgets. 1000 info/setLimit rounds (2000 calls)
in language and C use no XRT heap operations under zero-allocation budgets.
Creation still allocates a native backing object and managed controls/leases;
language share creates another control/lease. The backing allocator's native
creation failure is not injected by the managed publication test.

No universal zero-overhead claim is made: checked receiver/error scopes and
ownership bookkeeping remain; the current generated info call also copies
the 48-byte stack result to its SSA value before field extraction. Native lock
work remains in both paths. General POD output-copy optimization, language
thread transfer, model/GPU integration, package DLL initialization and final
matched delivery are separate remaining work.
