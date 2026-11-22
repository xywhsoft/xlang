# XGE 3D declarations

Use a matching _xge_native SDK with XGE_ENABLE_3D=1. Current safe CPU value
entries cover defaults, transform/perspective/orthographic values, large-world
relative positions, screen rays and bounded CPU particle simulation. Native POD names retain the Xge3d prefix.
The full scene/model/GPU/resource and query families remain in development.

createDecodeBudget(uint64 bytes) creates one real shared XRT quota.
DecodeBudget.info() returns the native sized statistics record;
setLimit(bytes) rejects zero and a limit below live charges. share() creates
an independent native owner. Closing one view invalidates its ordinary
language aliases, while a shared view and charged native blocks remain valid.
close() is a checked void operation, consistent with other owned resources.

See [the CPU-only example](examples/decode_budget.xl). It needs no window, GPU
or model file. The quota counts live XRT allocations inside bounded native
loads; it is not process or GPU memory measurement. Model/loader attachment in
the safe language API is still pending. No raw pointer is used as a placeholder
for those missing APIs.

Business methods declare the original native functions directly. The cold
policy only releases the one transferred native owner. This CPU leaf has no
language callback edges or native CallBegin/CallEnd pair; the resident root's
existing rule defers an active close until its generic C scopes have returned.
No scope-only policy flag or new runtime semantics is required.

Both platforms pass JIT and independent dynamic/static case/example programs,
C11/C++11/C++17 headers, real live-charge/independent-share/active-close checks
and 64 managed publication OOM budgets. 1000 info/setLimit rounds (2000 calls)
in language and C use no XRT heap operations under zero-allocation budgets.
Creation still allocates a native backing object and managed controls/leases;
language share creates another control/lease. The backing allocator's native
creation failure is not injected by the managed publication test.

No universal zero-overhead claim is made: checked receiver/error scopes and
ownership bookkeeping remain. With the direct-output compiler, info writes
the exact 48-byte record into its independent SSA value; earlier compilers
also copy a raw stack result into that value. Native lock work remains in both
paths. The generic optimization is documented in
[direct output storage](../../../../docs/foreign-direct-output-storage.md).
Language thread transfer, model/GPU integration, package DLL initialization and final
matched delivery are separate remaining work.

For a skinned scene node, `scene.skinPalette(node)` captures the current
mesh-local matrices in joint order. Acquiring a palette makes one native
allocation and copies the matrices into independent CPU storage.
`palette.matrices()` retains that storage and reads the typed records in
place. A sequence stays readable after its palette, scene and engine close.
For a single matrix, use `scene.skinMatrix(node, joint)`. See the procedurally
skinned triangle in [skin_palette.xl](examples/skin_palette.xl).

`animationOptions()` returns the native defaults for time, speed, weight and
looping. Change any field and pass the same record to `Animator.playConfigured()`
on any layer. Optional masks follow the target model's source-node order.
Optional `Xge3dBoneBinding` entries map clip source indices to target indices.
The animator retains the clip and copies the mask and resolved binding data
before returning. Optional `fade` is in seconds. A null clip clears the layer
immediately at zero fade, or stops it after the requested fade.

The options record crosses the C boundary as typed storage. The existing
`playMapped()` method still accepts individual options. Both methods run the
same native playback code. Use [animation.xl](examples/animation.xl) from the
examples directory to load the bundled assets and sample a pose without
opening a window.

`createParticles(threeDParticlesDefault())` creates an independent CPU emitter.
Its `setPosition`, `emit`, `update` and sized `info` methods call the native
functions directly. Closing it invalidates its language aliases; generic call
scopes defer the native free during active calls. This family has no GPU,
callback or other native resource references. Prepared simulation/statistics
loops need no heap operation, while creation allocates native storage plus
language controls. See [particles.xl](examples/particles.xl) and
[the simulation contract](../../../../docs/xge3-particles.md).

Particles require `XGE3D_ENABLE_PARTICLES=1` in the matching SDK.
`collectBillboards()` produces an independent CPU snapshot whose `geometry()`
sequence can outlive both the emitter and the batch handle. Submit the batch
to `Renderer.setBillboards()` and call `render()` with a scene and camera.
The renderer retains an optional `Texture3D` until that queue is replaced
or cleared. See [billboards.xl](examples/billboards.xl) and
[the ownership contract](../../../../docs/xge3-particles.md).

[offscreen.xl](examples/offscreen.xl) shows how to render to a
`RenderTarget3D`, read a pixel, and use the target as a billboard texture.
Pass separate geometry and nullable texture arrays for a mixed-texture queue.
`BillboardBatch.share()` gives a separate handle to the same immutable CPU
geometry. Each handle can close independently. A `geometry()` sequence also
holds its own reference. See [billboard_batch.xl](examples/billboard_batch.xl).

[debug_draw.xl](examples/debug_draw.xl) draws a persistent debug line and
prints the last frame's CPU time. The renderer also exposes
[options, profiling, warmup and configured post-processing](../../../../docs/xge3-renderer-diagnostics.md).

## 3D mesh example

[Render a triangle](examples/mesh.xl) from three vertices and attach it to a
scene node. The [mesh guide](../../../../docs/xge3-mesh.md) covers indexed
geometry, updates, level-of-detail attachments and ownership.

[Render a textured triangle](examples/material.xl) to see how a scene node
holds its material and how that material holds a texture. The
[material guide](../../../../docs/xge3-material.md) covers map coordinates,
texture updates, shader hooks and resource ownership.

[Render a sky cube](examples/environment.xl) with a configurable frame.
The [environment guide](../../../../docs/xge3-environment.md) covers cube
face order, precomputed IBL, shadow settings and linear fog.

[Load and render a glTF model](examples/model.xl). Run the example from the
`examples` directory so it can find its bundled asset. The
[model guide](../../../../docs/xge3-model.md) describes resource loading,
decode budgets and the lifetime of scene instances.
