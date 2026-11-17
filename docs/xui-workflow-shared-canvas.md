# Workflow canvas reference handoff

XGE commit `b44cdfa716e455fda2de8c02fa6b9216fee7f78f` extends the approved
sharing rule to Workflow canvases. `GetWorkflowOwned` obtains a real independent
reference; `SetWorkflowShared` gives the canvas its own reference. External
Workflow and Graph views survive canvas and Context close. Each actual canvas
reference is traced once, while the internal Graph canvas borrows its model.

A replacement can commit before old-model cleanup reports an error. The native
handoff tracks actual Graph publication, retains an unconsumed old reference in
one traced slot, and retries it before another replacement or destruction.
Destruction preparation also stops the internal canvas borrowing its Graph
before checked current-model release. Refused release preserves the owner and
prevents physical destruction. The earliest error is retained even if later
cleanup also fails or requests logical widget retirement.

Windows/Linux source-unit gates passed 62 build/run/header commands with the
matched compiler XRT4641 and frozen CPU proxy. Cases include aliases, owning
edges, promotion failures, 32 handoff allocation budgets (two actual failure
prefixes), real Drop failures, and explicitly injected unconsumed/current
release and post-publication invalidate failures. The latter injected results
test native result contracts; they are not real OS/scheduler failures. Original
Workflow interaction/widget and Graph shared/widget scenarios still pass.

Both native hosted and static libraries have also been fully rebuilt on both
platforms for the default profile with 3D enabled. The baseline audit matches
3799 tracked files and pins two existing build-time executable inputs; it
overlays only the 17 committed changes from the qualified native parent. It
copies source bytes rather than linking writable build caches to old evidence.
All frozen source inputs remain unchanged after the builds.

Twenty additional commands use these actual new libraries, without replacing
production source units: C11 O0/O2 build/run and export/symbol checks for the
hosted and static products on both platforms. Hosted tests bind to the complete
qualified compiler's sole XRT. Static tests retain the linker-only injection
cases; hosted tests preserve ordinary assertions and omit those injections.
Exact build, source and product hashes are indexed in
`dev/release-materials/provenance/xui-workflow-shared-canvas-20261117.json`.

There is one extra pointer slot per native canvas. First promotion adds the
Workflow/Graph lifetime metadata; warmed get/release and idempotent-set loops
observe no XRT allocation, free or reallocation calls. Native reference,
quiescence and call-scope checks still cost CPU. Raw-C CPU equivalence is not
claimed.

A fresh declaration inventory reads the same frozen native headers: 5294
functions and 991 types, with 3966 function names present in the current
language sources. These counts include lifecycle/support APIs, disabled
configurations and replacements, and include current candidate source files.
They are not a completion percentage or runtime acceptance. The inventory does
not qualify any missing name as an intentional omission without further review.

This completes the native canvas ownership batch and its default/3D SDK builds.
Safe Workflow language declarations, owned schema callbacks, pending default
configuration/reentry decisions, associated Graph mutation safety, persistent
module/cycle qualification, other required configurations and final matched
compiler/SDK/IDE/help/example installation remain. No normal release tree was
installed, no network/security test ran, and the long-running goal remains active.
