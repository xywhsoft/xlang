# Shared FlowGraph model

This module exposes the native FlowGraph as a directly declared owned
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

`getNodeConfig` uses native Shared retrieval and the generic
[`@c_data_snapshot_result(0)`](foreign-result-snapshot.md) contract. It makes
one independent strict PUBLIC data snapshot while the native owning result
stays alive. Editing this result does not change the model. Unknown resources
are checked-rejected, getters never execute, and allocation/conversion failure
discards the result without changing the stored owner. It does not DeepClone
before the projection. Native Clone reads remain explicitly named.

The earlier 2026-11-17 input/model batch had 73 declarations naming 68 native
functions. Whole
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

This model module is not installed in `release/`. Workflow, remaining model
ownership and real widget integration
cases, and final SDK/compiler/IDE/help delivery remain active work. Small
evidence is indexed by
`dev/release-materials/provenance/xui-flow-graph-inputs-20261117.json`.

The 2026-11-18 result integration has 74 declarations naming the same 68
native functions. Both complete O2 compiler cohorts passed the updated
public-field and source/result isolation cases, default-read OOM, whole
declaration C/C++ proofs, JIT, hosted/static EXE/shared modules and physical
unload. The updated readable example passed both linkages. These are 102
effective language/example checks and 819 source checks, with 33,556 files
independently rechecked. Default-read projection had a five-position complete
allocation failure prefix in each execution mode.

The Linux validation volume became read-only when its Windows backing drive
filled. Ubuntu was moved to F and restored to writable operation; 15,825
frozen compiler inputs and all compiler products were reverified. The
interrupted result group was rerun in a fresh output directory. Audit found
one old static shared artifact and two logs had not persisted. Three targeted
replay commands regenerated them; the shared image exactly matches its
originally recorded SHA-256. The damaged bytes and historical report remain
at their recorded paths, with explicit replacement evidence. This environment
recovery does not change the source or skip a qualification boundary.

The successor receipt is
`dev/release-materials/provenance/xui-flow-graph-results-20261118.json`.
Workflow, advanced model cycles/widget cases, other open contracts and the
final matched SDK/compiler/IDE/help installation remain separate work.
