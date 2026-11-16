# Shared FlowGraph native foundation

The approved native sharing protocol is implemented in the XGE repository:

- `d2a65458`: FlowGraph actual shared references, ownership graph and activity.
- `719d61a2`: independent canvas references and checked model handoff.

`xuiFlowGraphCreateOwned` creates a shared model; `ShareOwned` obtains another
real reference. Closing a view releases only that reference. The last native
reference destroys the model. `ReleaseOwnerChecked` separates consumption from
callback cleanup failure, so a consumed failed release must not be retried.
Creator-thread checks precede reads of mutable graph state. Data replacement
keeps the same shared identity, including during concurrent rejected calls.

`xuiFlowGraphWidgetGetGraphOwned` returns an independent reference, promoting an
owning native slot once. It does not promote an unowned legacy borrowed model.
`SetGraphShared` gives the canvas its own reference without consuming the caller's.
Owning canvas/Workflow graph slots trace exactly one graph edge. Configuration
and undo/redo edges are traced by the actual model owner, without duplication.

Both Windows and Linux passed 33 O0/O2 source-unit build/run/header checks and
eight static-archive canvas build/run checks. These include real aliases,
independent close, wrong-thread rejection concurrent with creator mutations,
same-model configuration callback Busy, collector finalization with outstanding
references, OOM, consuming cleanup errors, canvas retirement during old-model
Drop, original model/schema/XSON/canvas cases and DataObject regressions. They
use the sole matched XRT implementation and frozen test proxy. Evidence is
indexed in `dev/release-materials/provenance/xui-flow-shared-20261116.json`.

These gates pin XRT `6305aa54`. During evidence review the upstream advanced to
`09d543a3`, adding optional WebSocket TLS-ticket observation. That later snapshot
has passed six isolated generation/maturity checks. All 3481 existing function
declarations compare unchanged and one function is added. This is preparation,
not runtime qualification; it is not claimed by the existing build reports.
The next matched SDK/compiler synchronization must account for this addition.

Sharing adds native reference/activity and ownership-domain bookkeeping. There
is one owner-pointer field and one metadata allocation when a raw owning model
is promoted. Share does not copy model arrays or configurations. A warmed
1000-count-call test observes zero XRT allocation/reallocation; CPU equivalence
to raw native C has not been established.

These are native CPU gates. They do not establish a new matched SDK/host image,
complete Workflow shared ownership, safe xlang model classes/domains, language
JIT/AOT model qualification, GPU/desktop performance or installed IDE/runtime
delivery. Those integration steps remain in the active long-running task.
