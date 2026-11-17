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

The example covers the owned data model. Retained dynamic-port and validation
callbacks and checked canvas-to-model access have their own pending integration.
