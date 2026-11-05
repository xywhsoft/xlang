# Semantic finalization continuations

The native Context cleanup contract permits accepted semantic work to wait for an external owner without consuming the Context. A production WebView delegate is one example. A synchronous `bool Finalize` cannot represent this state: `false` is an actual finalizer failure, while `true` would incorrectly authorize Clear. Neither preparation BUSY nor physical Finish BUSY permits hiding arbitrary earlier semantic progress.

## Independent native capability

Authoritative XRT commit `7cecb354dd67aba47eb47cf9c118aea2b0195498` adds `xrtownershipfinalizationv1`, an exact 40-byte tagged immutable descriptor on x64. V1 adapter, preparation and 48-byte FinishV2 layouts remain unchanged.

`CanStep(data, token)` is a pure frozen query. It does not allocate, alter diagnostics or references, invoke semantic code, or promise that the semantic duty is complete. `Step(data, token, &complete)` executes outside Freeze/mutation with real snapshot pins and claims retained:

| Return | complete | Meaning |
|---|---|---|
| true | false | Durable progress or waiting; owner retained |
| true | true | Semantic duty completed |
| false | false | Actual failure; owner and continuation retained |
| false | true | Completed with failure; never repeat the duty |

A provider preserves its own completed subduties across wait, failure, plan abort and replan. Native ownership transfers are still exact. A wait never invents a reference, consumes an owner, or substitutes a permanent external root.

## Resident plan and driver

The typed catalog registers capabilities by exact adapter pointer. Unknown size/tag, missing callbacks, descriptor replacement, or conflicting capabilities fail closed before Count/Trace. Exact duplicate pointers deduplicate. V8 is the explicit plan constructor. V1 through V7 retain their original contracts.

`xl6OwnershipPlanFinalizeStepV1(plan, &semanticFailed)` returns progress separately from failure. `STEP_WAITING` permits accepted semantic progress. `STEP_READY` only ends the current finalize turn: `TryCommit` still takes a fresh whole graph and routes resurrection or new preparation instead of Clear. The old `FinalizeV2` maps WAIT conservatively to FAILED; it cannot claim the old side-effect-free BUSY meaning.

Each turn is bounded by its initial participant count. Rebuild supplies all old identities as anchors in their exact prior order; admission verifies identity and every immutable capability before publication. New nodes append and do not enter the current turn. Every attempted semantic duty is followed by a fresh graph before another new duty or Clear. Known transient read BUSY may resume only an already-started, still held and claimed continuation with a positive CanStep. It does not authorize unknown nodes, Count/Trace or a new duty. Resurrection restores quarantine and prevents Clear.

An incomplete failure latches an explicit-retry requirement. Automatic collection and iterations within the same timeout do not replay it. `xl6OwnershipPlanRetryFinalizationV1` acknowledges one checked retry without running user code. JIT Drain acknowledges existing FINALIZE plans once at entry; static Retire/Drain does the corresponding phase-specific acknowledgement. Committed Finish/Destroy and terminal legacy plans are not passed to Retry. Completed failures remain completed. Drivers retain the first independent failed bit and diagnostic, including NULL, across later snapshots, Commit, Finish and Drop; later OOM/cleanup cannot replace it.

`xl6StaticModuleCreateV4` accepts the typed finalization query explicitly without changing the older deployment contract layouts. Generated executable entry uses V4; legacy static constructors keep their existing meaning.

## ForeignView participation

Only explicitly marked roots use the new immutable second adapter/preparation identity. Context opts in through the cold `CONTINUABLE_FINALIZE` descriptor flag, together with scope-owning cleanup and independently reported failures. A legacy plan receiving this second adapter fails before invoking semantic work.

A continuation calls the existing privileged native destructor. Pending work retains the actual plan owner/claim/code lease. It does not enqueue a new external root merely to represent waiting, and it does not globally drain other foreign objects. Once native physical observation reports consumption, Step balances only that same root and its physically invalidated borrowed children. Child traversal holds a real temporary pin and rescans the actual directory after each Drop; it does not carry an unpinned next pointer across reentry. Queue bookkeeping is removed without invoking another native destructor. A fresh graph follows before Clear.

Existing explicit Close queues keep their ownership semantics. Child language values remain ordinary borrowed views of the actual Context root; they are not independent native owners. Native publishers still require the documented stop/join and explicit host retirement boundary before a shared library may unload.

## Cost and validation boundary

This adds no new wrapper, allocation or branch to ordinary native methods. The new flag is cold policy metadata. The catalog capability is 40 static bytes plus a cold pointer-array entry. Compiler-evaluated x64 plan sizes change from 48 to 64 bytes per plan node and from 96 to 104 bytes per plan record. Catalog changes from 1128 to 1160 bytes (its policy view from 528 to 544); ForeignView remains 464 bytes and its descriptor remains 136 bytes. V1 adapter64, FinishV2 48 and finalization40 stay distinct exact types. Waiting polls rebuild the real graph and therefore have existing snapshot allocation/scanning costs; they are not a zero-cost operation. No performance timing is inferred from O0 compiler-overlay correctness tests.

Focused gates cover the four completion/failure outcomes, first NULL and real diagnostics, ambient isolation, explicit retry, snapshot OOM, immutable identity/legacy refusal, bounded new peers, resurrection, known-read BUSY, thread eligibility and reentry; existing preparation/Finish/Drop/real Value plans remain separate regressions. A separate ForeignView test holds two Context queues and verifies that only the selected Context and its actual invalidated children are detached. Static V4 tests include waiting and first-NULL propagation across Finish failure; old static Finish/legacy-error modes verify retry acknowledgement does not introduce STATE errors.

The real DLL gate compiles the exact production WebView translation unit with three test-only RC hooks and no browser/backend/window initialization. It destroys the compiler, requests module/global cleanup, observes a successful wait with DLL/carrier alive at two data refs, releases the extra ref without draining, then uses a checked host retry to consume Context once and unload the DLL. Static fake tests do not substitute for this dynamic-loader proof. Focused development overlays do not certify a complete compiler rebuild, embedded VFS, final SDK staging, or release binaries; those require the final matched cohort.
