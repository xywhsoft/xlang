# Shared FlowGraph model

This candidate exposes the native FlowGraph as a directly declared owned
class. Every view owns one actual native reference; `share()` obtains another,
and `close()` releases only the current view. An independent shared view can
outlive it. Ordinary methods use direct native calls with checked creator
thread, view, error and resource admission. They add no per-method forwarding
implementation. Native activity/ownership bookkeeping still applies.

The declarations cover nodes, ports, edges, dynamic port rebuilding, position
and bounds, summaries and run state, history/revisions, selection, coordinates,
generated IDs and diagnostics. Returned record strings are copied before the
call scope ends, preserve null/empty distinctions, and outlive model edits or
close. Column-array projection creates a checked temporary C record array,
with O(n) conversion and lifetime protection. Generated IDs write a caller
Buffer whose initialized length must be at least 64 bytes.

`setNodeConfig` and `commandSetNodeConfig` use
[`@c_data_snapshot(1)`](foreign-data-snapshot.md) and native Shared storage.
They perform one strict PUBLIC input projection before native entry: public
stored fields are copied, getters never execute, and private/opaque resource
storage is excluded or checked-rejected. Source changes after a successful
call cannot change the stored snapshot. The command additionally preserves
the native independent old/new history snapshots; those history copies remain.

Advanced `setNodeConfigClone`, `getNodeConfigClone` and
`commandSetNodeConfigClone` keep native Clone behavior. The corresponding
explicit Shared operations preserve reference identity. A Shared read owns
its returned value and can outlive the Graph. Native/shared values can retain
language resources and code; these advanced ownership graphs require their
own qualification and are not covered by a pure-data input test.

There is currently no default `getNodeConfig` declaration. The generic owned
result strict-data projection was separately approved on 2026-11-17 and is
being implemented and qualified before wiring that default read. A native
Clone read remains explicitly named.

The source has 73 declarations naming 68 native functions. Whole
declaration emission, C11 code/header and C++11/C++17 headers passed. Source
storage review and deterministic generation passed 409/410 checks on Windows/
Linux. Both complete O2 compiler cohorts passed JIT, hosted/static EXE and
shared module retirement/unload for behavior, factory/output/history OOM,
public-field projection, opaque refusal and the 11/27-position pure-data write
failure prefixes. The readable top-level example passed JIT and both AOT
linkages. These total 102 language/example commands; independent verification
rechecked 33,519 source, SDK, compiler, log and product files. These counts do
not establish whole API behavioral or lifetime coverage.

Windows standalone qualification exposed a TinyCC automatic-stack defect:
completed C-block cleanup locals accumulated into a 1,125,824-byte frame,
exceeding the normal 1 MiB executable reserve. A diagnostic copy with a larger
reserve confirmed the cause; that change is not a shipping fix. The separate
[scope-slot reuse fix](tcc-scope-stack.md), complete O2 compiler/runtime/VFS
rebuilds and actual model replays passed. Windows executable and shared
products retain the normal 1 MiB PE reserve. The Windows Python shared host
uses the matched native DLL directory in its loader PATH.

This input/model foundation is not installed in `release/`. Default result
snapshots, Workflow, remaining model ownership and real widget integration
cases, and final SDK/compiler/IDE/help delivery remain active work. Small
evidence is indexed by
`dev/release-materials/provenance/xui-flow-graph-inputs-20261117.json`.
