# Source job admission and physical quiescence (M7b5o)

This stage prepares another participant for the future IDE source transaction
host. It does not activate standard LSP rename/applyEdit or grant permission to
write sources. The template and recovery workers now belong to the explicit,
zero-initialized `xide_app.sourceJobs` registry. Session maintenance, recovery
windows and template windows pass that same registry; the service constructors
have no anonymous fallback.

## Ownership and admission

The registry is intrusive owner metadata, not another scheduler, allocator or
thread owner. Serialize its APIs on the UI owner. Each entry borrows its real
Win32 worker HANDLE; the enclosing job/service and registry must outlive that
entry. A constructor begins admission before allocation, path preparation and
callbacks, links its entry, binds the actual thread, then ends admission. Every
failure prefix balances admission and retires its entry. Destruction waits for
the real worker, unlinks, then closes the HANDLE.

Synchronous result publication also holds an admission scope. In particular,
document callbacks invoked while a template opens an editor cannot acquire a
source-job lease halfway through publication. Nested synchronous admissions are
allowed; suspension is refused until all such scopes finish.

`Suspend` passively closes new admission and returns an exact, monotonically
increasing token. Tokens do not wrap or cross initialized owner lifetimes.
`State` performs zero-time waits on the registered thread handles. An immutable
result's Ready/Done flag does not prove physical thread exit. Removing an entry
under a lease invalidates its restoration baseline, independently of whether
the remaining workers are quiet. Stale tokens clear status outputs and cannot
resume another lease. `Resume` is explicit and may abandon a pending wait; the
caller must settle any writes, rollback and recovery before reopening admission.

Suspension does not cancel or join workers, drain queued results or resume
automatically. Cancellation and teardown remain available. A quiet status covers
only these registered jobs, not every IDE source reader or the complete
request/source/project/configuration/consent policy.

## User interface behavior

Template polling checks both result publication and physical exit before
destroying a job. A completed result remains pending during suspension. Recovery
result taking and UI publication behave the same way. New template/recovery work
and session maintenance do not start while admission is held; relevant controls
reflect that state without making either C or xlang editors read-only.

Cancelling an already generated but not yet published template result revokes
automatic opening and project/workspace association. Files already generated
remain on disk. Reopening admission does not restore the cancelled consent.

## Validation and evidence

All 125 GUI production units were rebuilt together because `xide_app` changed.
The tests use the exact matched XGE SDK and the previously accepted native
language service. No SDK or server units were rebuilt in this stage. Private
aliases in the root runtime protect the last working delivery and prevent the
new GUI from loading the older default XGE DLL; all aliases are removed after
validation.

The focused tests exercise real template writes and recovery reads, worker
trampolines retained after the real procedure returns, passive holds, pending
results, cancellation, reentrant constructors/publication, allocation/thread
failure, token exhaustion, membership changes and teardown. The actual GUI
checks include C and xlang typing, deletion, undo, Unicode/IME and click focus.
Seven deliberate guard-removal variants must fail assertions. Broader validation
includes 55 GUI suites, nine existing template/recovery/session suites and
bounded actual GUI startup for C and xlang.

The broad GUI helper is frozen to the accepted matching XGE producer's header
and previously accepted helper C bytes. A run that consumed a concurrently
modified external helper was rejected, and its raw evidence is retained. The
legacy damaged-preferences test now asserts an owned default object and preserves
the original corrupt bytes and warning; the previous GUI also reproduced the
obsolete null-object expectation.

Suspend and rejected construction make no direct owner-side allocations. This
is a narrow measured property, not a whole-process allocation or latency claim.
There is no new per-keystroke registry scan; status polling scans registered
workers, and control refreshes still do metadata work on hold transitions. This
stage does not qualify Linux, sanitizers, whole-goal latency, memory use or
longevity. The full host, other reader policies, recovery UI and standard LSP
rename/applyEdit activation remain separate work.

The committed material index is
`dev/release-materials/provenance/lsp-source-job-lease-20261112.json`. Immutable
source/build receipts, failed attempts, exact dependencies, screenshots and
large logs live in its indexed external bundles. The default runtime, SDK and
delivery index are preserved; this stage performs no online publication.

The final whitespace check removed one trailing space in the template job's
registry field. Recompiling that exact source produced a byte-identical production
object; the include-based focused fixture also produced byte-identical
preprocessor output. Both original validation and this final refinement are
retained and bound, without changing the immutable full-build snapshot.
