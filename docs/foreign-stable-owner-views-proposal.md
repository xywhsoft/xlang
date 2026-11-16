# Stable parent-owned native records: pending contract

This proposal is not implemented. It records a reproduced limitation, rather
than weakening the current admission rules or installing an invalid binding.
The parent XGE backend implementation pins immutable descriptor/name storage
until its final real reference is released (`xge_backend_owned.inl`, Describe).

## Reproduction and current checks

Direct `xgePlatformBackendDescribe` / `xgeGraphicsBackendDescribe` declarations
can already use `c_output(1,0)` and `c_result("owner_view",0)`. Exact const native
types compile. Runtime exposure fails checked before publishing the view:

- `viewDescriptorValid` requires Destroy and, for every child, Watch/Unwatch.
- `xl6ForeignViewBorrowChildV1` also requires a native root CallBegin scope.
- Backend roots are independently refcounted, creator-thread resources. Their
  descriptor is embedded stable storage, with no separate destructor/watch.

The Windows actual full-compiler probe fails at Describe, after successful
backend construction. Candidate bytes, source hashes and negative logs are
preserved in the external `backend-describe-proposal-r1` and
`backend-describe-win-preflight-r1` through `r3` cohorts. Public bindings were
restored to their previously qualified committed versions. A fake watch or
no-op native destructor would misrepresent the native contract.

## Proposed runtime contract

Add a closed descriptor flag for **stable parent-owned storage**, usable only
on a child with a declared exact owning root. Existing watched children retain
all their current requirements. Existing language `owner_view` output mapping
and nominal owner checks remain; no per-method C forwarding query is needed.

1. The producer guarantees that the child's address remains valid until the
   real parent payload is destroyed. The child has no independent native free,
   replacement or destruction notification. Const descriptor fields expose
   only read-only metadata in the XGE binding.
2. A child control holds the real root control/code dependency and contributes
   its actual existing graph edge. It introduces no duplicate native owner,
   synthetic native watch, guessed leaf graph or hidden language reference.
3. Native result admission still requires the creator thread, a successful
   active root call, exact owner type, nonclosing/live root and stable family.
   A refcounted root without native CallBegin is eligible only for this
   explicitly opted-in mode; ordinary watched children remain unchanged.
4. Cold publication can allocate the ordinary child/directory controls.
   Existing live exposure reuses them. Failure publishes nothing and rolls
   back all provisional ownership/directory/code state.
5. Explicit parent close invalidates these children before they can be used
   again, even while another independent native share remains. Losing the
   parent's language variable alone leaves a held child usable. Parent and
   child access retain the existing checked thread/close rules.
6. Child close/release only detaches its view. It must never free the embedded
   record or call a fake destructor. Root cleanup still owns the only native
   release, may fail checked, and cannot lose its duty during graph retirement.
7. Root invalidation and finalization explicitly clear/detach stable children
   without reading their former native fields. Shared callbacks/code remain
   traced through the true root; claimed-cycle finalization is a separate,
   already pending semantic decision.
8. Reject unknown/mixed flags, root use, native child Watch/Destroy/CallBegin
   hooks in this mode, mismatched roots, closing parents and stale admission.
   This flag is an audited native-provider promise, not arbitrary-pointer
   validation or proof that a third-party C implementation is immutable.

The exact constant/annotation spelling must be finalized with implementation;
this proposal authorizes no new syntax or runtime semantics by itself.

## Acceptance required before publishing the binding

Exercise both watched and stable children with actual runtime source, generated
C ABI proofs, Windows/Linux full matched compilers, JIT and dynamic/static AOT.
Cover default parent release with a saved child, explicit close and independent
share, callback/native-call close reentry, wrong thread, cold OOM publication,
string snapshot OOM, exact heap balance, graph trace/retirement and code image
unload. Never replace the still-failing claimed-callback or global Engine gates
with these acyclic tests. Measure cold exposure, repeated scalar reads and text
snapshot copies separately against the same native workload.
