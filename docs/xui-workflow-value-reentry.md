# Workflow schema/default callbacks

The native Workflow layer now applies the existing same-Graph Busy protocol to
type schemas, variable defaults and configuration Clone/Drop. Calls on the
same Workflow and associated Graph fail before touching additional arguments
or outputs. Independent models remain usable. Temporary import models protect
the target's callback identity, and finalization keeps the associated Graph
alive through all table Drops.

The native regression covers every integer model entry, preserved outputs,
shared Graph aliases during Workflow destruction, default replacement inside
ObjectSetTake, failed Clone and allocation-failure rollback. It also keeps the
earlier ownership, schema and XSON tests. The two O0/O2 source-unit gates use
one matching full compiler XRT and reproduce the original bug on the preceding
native sources.

`tools/check_xui_workflow_value_sdk.py` separately verifies actual fresh O2
hosted/static SDKs. Its O0/O2 CPU clients link only the real SDK library and the
same compiler-owned XRT. They do not replace SDK production units. Imports
distinguish hosted and static native linkage. The SDK builds and current
fixtures are pinned independently; historical failed attempts remain intact.

No model field or allocation is added for the guard. Callback depth updates
and helper calls still cost CPU work. The language's default independent data
snapshot policy is approved but is not implemented by this native guard.
Advanced native Clone behavior is preserved. The safe public model API and
final compiler/SDK/IDE/help delivery remain open.

Evidence is indexed by
`dev/release-materials/provenance/xui-workflow-value-reentry-20261117.json`.
