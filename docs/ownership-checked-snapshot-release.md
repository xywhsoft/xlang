# Checked ownership snapshot release

`xrtownershipfinishv2` extends the canonical V1 completion prefix with an exact
ABI tag and `DropChecked`. It does not change the V1 size or any existing XRT
function signature. A provider publishes `&descriptor.V1` with the V2 size;
readers accept only the exact V1 size or exact V2 size, tag and non-null hook.
No V2 tail is read for a V1 descriptor. Unknown descriptors are refused before
ownership Count/Trace or lifecycle work.

The hook consumes exactly one real Hold reference, even when it returns false.
False reports failed physical cleanup, including failure without an allocated
diagnostic. The plan calls either DropChecked or legacy Drop once. It restores
an aborted claim before releasing pins and executes releases outside Freeze.
Descriptor code stays pinned through the entire physical release batch.

`xl6OwnershipPlanTryDestroy` returns READY once the plan is consumed. A later
cleanup failure cannot turn that into a retryable ERROR pointing to freed
storage. Instead, V2 drop failures join the enclosing checked error scope; if
there is no checked caller, the resident failure mailbox retains the failed
batch for the next safe boundary or explicit host retrieval. The first failure
has priority even when its diagnostic is NULL. An earlier caller diagnostic is
preserved. BUSY leaves the plan and all pins intact. Failed pinRoot creation
also consumes its acquired pin using the same versioned hook and reports an
explicit ERROR, preserving the earlier infrastructure failure.

V1-only drop batches keep their existing void callback behavior and do not open
the extra error scope. V2 batches add one checked scope, version dispatch and
diagnostic observation during retirement. These are cold lifecycle costs, not
per-render-call C wrappers. This does not establish zero total binding overhead.

CPU-only acceptance rebuilds six lifecycle components and reuses the verified
R31 XRT object, whose embedded checked-error core is unchanged. Windows and
Linux both pass O0/O2: guard-page V1 reading, malformed V2 rejection, committed
and aborted last-reference failure, failed construction after pinning, BUSY
retry, NULL first error, later cleanup errors, caller diagnostic priority,
mailbox delivery, and allocator balance. Six earlier ownership/continuation/tail
programs also pass. This focused gate does not certify a new full compiler,
native UI engine, GPU behavior, or delivery package.

Run `tools/check_ownership_checked_drop.py --help` for the pinned parent object,
SDK and evidence arguments. Recorded input hashes and exact commands are in the
provenance report; historical reports and the delivery index remain unchanged.
