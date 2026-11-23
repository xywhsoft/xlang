# FlowGraph / Workflow canvas construction and retirement

`Context.flowGraphWidget()` and `Context.workflowWidget()` create empty native
canvases with their native-owned default models. The declarations call
`xuiFlowGraphWidgetCreate` / `xuiWorkflowWidgetCreate` directly, passing NULL as
the optional descriptor. The existing factory rollback and Context-owned view
contracts handle failed construction, thread checks and expired language views.
No new compiler rule or product-specific C forwarding layer was added.

The previous `flowGraphWidgetNative` / `workflowWidgetNative` names are replaced
by those two names. Advanced C interop can still pass the declared nullable
native descriptor pointer.

The model overloads accept `FlowGraph?` or `Workflow?` and eight named colors.
Pass `model=null` to create an owned default model with custom colors. The
compiler builds the native descriptor on the stack and calls the shared
constructor directly. The canvas takes its own reference during initialization
and retires it through checked widget cleanup if a later stage fails.

```xl
auto graph = xui.createFlowGraph()
graph.addNode("start", title="Start")
auto canvas = ui.flowGraphWidget(graph, backgroundColor=0xEDF5FCFF)
graph.close()
```

Use `FlowGraphWidget.setGraph()` or `WorkflowWidget.setWorkflow()` to install
an existing managed model. The canvas takes its own reference, so the caller
can close its reference after installation. `getGraph()` and `getWorkflow()`
return independent references that survive canvas closure. `Workflow.graph()`
also returns an independent graph reference.

The current Windows and Linux SDKs passed the ownership and diagnostics tests
in JIT, dynamic AOT and static AOT. See the
[current verification record](../dev/release-materials/provenance/xui-flow-workflow-audit-20261123.json).

The shared constructors passed native O0/O2 and language JIT/dynamic/static
AOT on Windows and Linux. Both constructor tests cover reference rollback at
48 allocation-failure prefixes and use the actual canvas render path to check
the background color. In a warmed comparison of 200 pairs of creations and
closes, C used 1,200 XRT allocations and xlang used 2,400 on each platform.
The language path adds three allocations per widget in that workload. The
models retain their identity and their nodes and configuration are shared.
[Shared constructor verification](../dev/release-materials/provenance/xui-flow-canvas-create-20261123.json)

The short examples are [flow_canvas.xl](../lib/bindings/xui/examples/flow_canvas.xl)
and [workflow_canvas.xl](../lib/bindings/xui/examples/workflow_canvas.xl). Each
creates a label and a canvas; a right click updates the label with coordinates.
Their shared window helper supplies only the normal window/event-loop setup.
They demonstrate construction and events, not a complete node editor.

## Native ownership fix

The preceding model Busy guard exposed an owner-slot defect. A configuration
Clone/Drop callback could replace a canvas model or close its widget/Context.
The model's void destroy refused Busy, but its owning widget could still discard
the model pointer. A real predecessor SDK reproduction demonstrates the orphan
on both Windows and Linux; the final regression exercises the corrected SDKs.

Both native canvas types now use XUI's existing pure destroy-readiness hook.
Logical close invalidates views immediately; physical retirement preserves the
model owner until its configuration callback returns. A checked Context close
reports deferred with `consumed=false` in that interval. Raw native callers must
retain the Context owner and retry after the call; normal Context call scopes
provide the deferred cleanup tail.

Model replacement holds an existing Context call scope and a per-canvas mutation
flag through cancellation and retired-value Drop callbacks. Same-model reentry
returns Busy; independent models remain usable. Pointer getters report Busy
without replacing an earlier XRT error. Hover/hit outputs and constructor output
canaries remain unchanged on Busy. A Workflow's internal canvas also rejects
public independent rebinding: its private setter requires the actual composite
owner. Its owning WorkflowWidget remains the model-replacement authority.

## Qualification and measured cost

The final gate runs against matched real hosted/static native libraries and
complete O2 compiler cohorts on Windows and Linux:

- 94 checks per platform: six native suites at O0/O2 in both linkage modes,
  JIT/dynamic/static language fixtures, C/C++ headers, imports and both examples.
- 150 Windows / 151 Linux strict storage-review and generator checks. Full
  normalized generation still differs in `xui/src/managed.xl`; that broad
  candidate is not copied over the maintained binding.
- The new native lifetime regression covers owning/borrowed model slots,
  replacement, current/input Busy, output preservation, independent peers,
  owner-close retry, close during retired-value Drop, 64 allocation-failure
  prefixes and heap balance. The language test covers 128 construction prefixes,
  bound/protocol factories, callback-view expiry and thread/Context boundaries.
- Each example executes for two frames in all three language modes, producing
  six 800x600 RGBA readbacks per platform. For each example, the three hashes
  match within that platform. Cross-platform pixels are not asserted identical.

For 1,000 warm hover queries consuming the node index, both language and native
C measure zero XRT allocations, frees and reallocations in each language mode.
These counts exclude setup and field-validation assertions. They do not measure
CPU equivalence or establish zero cost for cold construction or callbacks.

The exact compiled private widget layouts remain 872 bytes (FlowGraphWidget)
and 40 bytes (WorkflowWidget) on the tested x64 configurations: the new flags
occupy prior padding. Readiness checks, mutation flags and successful setter
call scopes still add CPU/ownership bookkeeping. There is no added config copy
or global lock in this fix.

Public headers remain unchanged in this batch. Native host candidates compile
two units and reuse 162; static candidates compile three objects (including the
output-specific contract) and reuse 163. Their qualified parents are the prior
complete O2 164/166-object builds. Reuse dependencies, object bytes, complete
source snapshots, gate inputs and artifact hashes are independently verified
on the actual operating systems. This is a qualified delta, not a new full
native rebuild. The pinned XRT is the qualified `41bc2457…` header; it does not
establish that the latest upstream XRT working tree has been synchronized.

## Remaining integration boundaries at the initial canvas qualification

Standalone safe Graph/Workflow model objects, shared model references,
long-lived model aliases, implicit reference-cycle retirement and safe owning
model transfer are still incomplete. In particular, a legacy setter can commit
a borrowed input before a later logical-close status is reported; the native
regression retains the caller's input owner. A language transfer API must not
infer consumption solely from that legacy status.

General Workflow schema/variable value callbacks, validation and dynamic-port
callback contracts are outside this graph-configuration qualification. The
follow-up same-Workflow Busy proposal is pending a user decision.

SDKs/packages, source snapshots, raw logs and rendered evidence remain external
under `F:/xlang-validation/xge-full-20261110`. The small
`dev/release-materials/provenance/xui-flow-canvas-20261116.json` receipt indexes
their hashes. This batch does not install the final release/IDE/help catalog or
complete the full XGE/XUI/Document integration goal.
