# Workflow data model

Create a model with `xui.createWorkflow()`. It is usable from a terminal
without creating a window. A successful `share()` takes a separate native
reference. `graph()` returns its own reference to the FlowGraph, which can
outlive the Workflow that supplied it. Call `close()` on each view when
explicit checked cleanup is needed. Model access follows the creator thread.

Use `createWorkflowSchema()` and `addWorkflowSchemaField()` to describe node
configuration. `registerNodeType()` accepts typed columns for the input and
output ports, plus the schema. Each set of columns uses one common size.
Pass `-1` to require all column lengths to match. Nonnegative sizes select a
prefix up to the shortest column. Use empty columns for a type with no ports.

The default configuration and variable write methods snapshot supported public
data so changes to the original language value cannot modify model state.
`setNodeConfig()` validates and applies configuration, dynamic ports and summary
together. The old state remains intact if preparation fails. The native
`Clone` family preserves the original XUI deep clone behavior. The explicit
`Shared` family retains the original value identity, so aliases can observe
one another's changes.

`getNodeConfig()` and `getVariable()` return independent data snapshots.
`getVariable()` returns the variable ID, optional title, optional type,
optional scope and default value, in that order. `getVariableShared()` and
`getNodeConfigShared()` keep real XRT references, which remain valid after
the model is closed. Text results are independently owned. A failure during
result conversion releases the entire tuple and reports a checked error.

The [workflow model example](../lib/bindings/xui/examples/workflow_model.xl)
creates a type with ports, configures a node and reads a variable both ways.
The type declaration calls native registration once. It temporarily allocates
one native record array per nonempty port group and prepares the columns in
O(n) time. The default value and schema snapshots add graph traversal and
storage. The native model still copies port metadata and validates it.

Use `WorkflowWidget.setWorkflow(model)` to give a canvas a separate reference
to the model. `getWorkflow()` returns an independent view, including for the
canvas's default model. Both the returned view and previously acquired views
can outlive the canvas and context. A native failure while retiring the old
reference may be reported after the new model is already installed.

Use `registerNodeTypeWithCallbacks()` to generate ports from a node's
configuration or provide additional validation diagnostics. Its first
arguments follow `registerNodeType()`. The two optional callbacks receive
a `WorkflowCallbackNode` and a typed output writer. The Workflow retains
the callback group and its captured values for the lifetime of that node
type. Return 0 on success. A thrown error is propagated by the model
operation that invoked the callback.

`node.config()` returns an independent data snapshot. The node view and
writer expire when the callback returns. Use the data snapshot or copy the
node's `id` and `kind` when you need them afterward. Calls on the same
Workflow and its associated FlowGraph report Busy while a callback runs.

Each writer takes a typed row and copies its strings during `append()`.
Dynamic ports are marked automatically. A diagnostic with no node ID is
associated with the current node. An append failure rejects that callback's
entire result, even if the callback catches the error. Configuration changes
keep the previous configuration and ports when dynamic output preparation
fails. See the [callbacks example](../lib/bindings/xui/examples/workflow_callbacks.xl).

Registration calls the native entry once. A callback group allocates a shared
environment. Dispatch enters the compiler-generated callback directly. A
configuration read incurs a snapshot traversal. Writers use temporary row
storage and copy text, and the Graph copies port metadata into its own
storage. The diagnostic batch transfers its existing strings to the Graph.

`getNodeLibraryItem(index)` returns the type ID, version, optional title,
category, description and icon, followed by the fixed input and output
counts. `getSelectedNode()` returns the first selected node in model
order and reports a checked error when there is no selection. Its fields
match `FlowGraph.getNode()`.

Use `setNodeRunState()` and `setEdgeRunState()` to update execution
highlighting. Their getters return the native state and optional preview.
The copied preview distinguishes an empty string from null and can outlive
the model. `writeVariableId(buffer)` writes a NUL-terminated identifier in
a caller-sized Buffer. A 16-byte writable Buffer fits every supported ID.
An insufficient Buffer raises a checked error without advancing the sequence.

`toValue()` exports the XSON model as independent data. The native producer
builds one envelope and retains its configuration and variable defaults
until the language takes a whole-result snapshot. Repeated payload references
stay shared within that snapshot. `loadValue()` takes one input snapshot,
then the native loader retains its payloads. Invalid input leaves the existing
graph and variables intact. A successful load keeps all existing graph views
and the registered node types.

`toValueShared()` and `loadValueShared()` are for callers working directly
with XRT wire values. For example, a configuration returned by
`getNodeConfigShared()` can be stored as an explicitly shared variable default.
The `Clone` methods follow the original XRT deep-copy paths.
`saveXsonFileClone()` and `loadXsonFile()` use the native XSON file format.

`validateConfig()` returns the number of configuration issues.
`validateConfigReport(type, config)` returns all issues in schema order.
Each record contains `path`, `code`, and `message`. The report owns its
data and remains readable after closing the Workflow. Omit `config` to
check which required fields are missing. An allocation failure rejects
the entire report.
