# Workflow shared native foundation

XGE commit `95acd2747543808fec323f5f51248675ed02211c` implements the approved
native sharing rule for Workflow. Each view owns an actual reference, and the
Workflow owns an independent reference to a shared Graph. An independently
acquired Graph survives Workflow close. Workflow load retains both identities.

The native model now supplies creator-thread/activity checks for all 30
existing int operations, the ownership graph and checked finalization/physical
completion protocols. Counts retain zero and find queries retain -1. Cleanup
reports whether the reference was consumed separately from a user Drop error,
preserves the first error and retains the metadata until all native/collector
references are gone. Legacy unique C construction remains available.

Windows and Linux each passed 33 C11 O0/O2 build/run/header commands against the
complete qualified compiler's sole XRT4641 implementation. These include real
aliases, independent Graph survival, reload, concurrent wrong-thread rejection,
collector tail references, allocation failure prefixes, cleanup errors,
Graph shared/Busy regressions, and original Workflow schema/XSON scenarios
using both unique and shared origins. The widget unit still compiles, and
C11/C++11/C++17 public headers pass. Exact reports are indexed in
`dev/release-materials/provenance/xui-workflow-shared-20261117.json`.

Creating a shared Workflow uses four XRT allocations: original Workflow and
Graph storage, plus one native lifetime allocation per model. Share does not
copy model data or allocate. A warmed 1000-count loop observes no XRT alloc,
free or realloc calls. Creator checks, native RC/activity and ownership-domain
bookkeeping still have CPU cost; no raw-C CPU equivalence is claimed.

This is a native foundation checkpoint. It does not publish the language model
candidate or install a new SDK/runtime/IDE. Workflow canvas owning slots,
associated Graph mutation safety during schema callbacks, owned schema callback
groups, cross-module/cycle qualification and final matched delivery remain.
The independent-default-configuration/native-Clone choice and the ordinary
Workflow schema/default Clone/Drop reentry policy are still pending.

The broader pending-design list also includes claimed callback admission during
finalization, an owning Engine controller and stable parent-owned record views.
Earlier checkpoint files that list only those three do not represent the whole
remaining decision set. Historical reports retain their original bytes and
continue to qualify only their recorded sources. No network/security test ran.
