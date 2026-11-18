# 3D models and scene instances

`xge3d.loadModel(path)` loads glTF 2.0 or GLB through the XGE resource
loader. The memory overloads take `bytes` or `Buffer`. Supply
`baseUri` as the model's logical filename when it uses relative resources.
The native loader copies the encoded memory input. With a URI provider
that supplies a custom destructor, the managed loader copies the data and
releases the provider payload before parsing so the model can outlive it.

Pass a `DecodeBudget` to cap the XRT allocations charged to decoding.
The quota keeps counting retained allocations until their final owners
release them. The default allows unlimited decoding. `memoryInfo()`
reports the retained CPU size and estimated GPU payload. `resourceInfo()`
reports the geometry and texture payloads and the current model reference
count.

`model.info()` reports source counts. `nodeName(index)` returns a
copied optional name. `material(index)` returns an independent native
material reference when present. `skinJointNode(skin, joint)` uses the
native `SIZE_MAX` sentinel for an unavailable joint.

`scene.instantiate(model)` publishes the selected glTF scene and returns
its root. An optional parent places the instance in an existing hierarchy.
`scene.instanceNode(root, sourceIndex)` returns a status and a
generation-checked node. The status is `XGE_ERROR_NOT_FOUND` when the
source node is outside the selected scene or has been removed. A failed
synchronous instantiation removes its staging nodes.

The scene holds the instantiated assets. A node moved outside its original
instance root continues to hold its mesh and material. The graphical
resources stay on the graphics creator thread, and engine shutdown
returns Busy while any managed asset is still owned. `model.share()`
and `model.close()` manage independent references to the model itself.

Both Windows and Linux verified file and memory loading for glTF and GLB,
including relative buffers, and read pixels back from a detached mesh
after closing its model. In 64 hosted JIT iterations, metadata queries
performed zero XRT heap operations. Warm instance creation and removal
matched the C loop at six allocations and frees per iteration. Loading
and closing the memory fixture added three allocations per iteration
over C. Two reserve the language owner and one materializes the base URI
as a C string. Injecting failure at all 38 allocation points of that
load returned checked errors with the graphics owner count intact.

[Model example](../lib/bindings/xge/three_d/examples/model.xl)

## Asynchronous loading

`xge3d.createModelLoader()` creates an owning loader with one CPU worker.
Submit a URI with `request()`, inspect its current state with `status()`
and call `pump(uploadBudget)` on the graphics thread to make GPU progress.
The upload budget sets hard byte and operation limits. The time limit is
soft and checked between graphics calls. Zero operations pause GPU work.
Once the request is ready, `take()` consumes its handle and transfers an
independent `Model3D`. Failed jobs expose their native error in `status()`.

Use `setDecodeBudget()` while the loader is empty. The worker charges
decoding and retained data to that budget. `memoryInfo(request)` is
available from CPU-ready through ready. Default input accepts local files
and `file://` URIs. To use registered providers, set
`XGE3D_LOADER_RESOURCE_PROVIDERS` in the descriptor. Those callbacks run
on the CPU worker, so their capture and any dependencies must be safe on
that thread. Legacy provider buffers with a custom release callback are
copied and released on the worker before decoding.

`cancel()` immediately prevents publication. Keep pumping to release
partially uploaded GPU resources. `release()` also invalidates the request
handle. Closing the loader cancels remaining jobs and joins the worker,
which can wait for a file or provider read in progress. Close from the
creating thread. Engine shutdown reports Busy while the loader or its
transferred graphics resources remain live.

In 1,024 warm iterations on Windows and Linux, checking a CPU-ready
request and pumping with a zero budget caused zero XRT allocations,
frees or reallocations, matching the handwritten C loop. This measures
the polling path. Creating a loader starts a worker, submission copies
its URI, and taking a model establishes ownership of its native assets.

[Asynchronous model example](../lib/bindings/xge/three_d/examples/model_async.xl)

## Incremental instantiation

`scene.beginInstantiate(model, parent)` retains the scene and model on
the creating thread. Call `step(workBudget)` until it returns 1. A zero
node budget pauses the work. The time limit is soft and checked between
node, primitive, hierarchy and skin operations.

Before `take()`, the stage is absent from scene enumeration, parent
children, queries and rendering. A successful `take()` returns the
root. If the destination parent was deleted, publication fails. If
allocation for publication fails, the fully built stage remains
available for retry. `close()` discards any unpublished stage and
releases both owners. Keep a scene reference to use the returned node.

Each native node has one pointer for tracking stage membership. The
builder keeps a temporary pointer list and a source-index map.
Publication appends the node pointers to the scene, updates the dense
indices and invalidates the spatial index. It is linear in the number
of published nodes and does not copy mesh or texture data. The synchronous
`instantiate()` path continues to write directly into the scene.

On Windows and Linux, 1,024 paused calls performed zero XRT heap
operations in both xlang and C. Creating, stepping, publishing and then
removing the example instance took nine XRT allocations in xlang
and seven in C per iteration. The two extra allocations hold the
language builder view.

[Incremental model example](../lib/bindings/xge/three_d/examples/model_instance.xl)

## Animation and skinning

Use `model.clip(index)` for a clip in a loaded model, or `xge3d.loadClip()`
for a separate motion file. A clip owns its source model. `clip.info()`
returns the optional name, duration in seconds, track count and source node
count. An optional decode budget also covers the retained track storage.

`scene.animator(root)` retains the scene and its instantiated model.
`play()` keeps the clip alive, including clips still needed by a crossfade.
Animation uses the creator thread. If the instance is removed, `update()`
reports a checked error. Release the animator and any surviving clips
before shutting down the engine.

Playback supports eight layers, fades, global and per-layer pause,
override and additive blending, queued events and root motion.
`playMapped()` accepts an optional target-node mask and explicit source
and target indices. Native code copies the mask and bindings. Default
playback matches clips from another model by unique node names. Use
`bone()` for a socket node and `pose()` / `setPose()` to inspect or
override the current pose. `nextEvent()` returns `XGE_ERROR_NOT_FOUND`
when the queue is empty.

`bindSkin()` accepts an ordered joint array and optional inverse bind
matrices. Leaving the matrices empty selects identity. `skinMatrix()`
reads an individual matrix, and `skinPaletteBytes()` takes an independent
copy of the consecutive native matrices. `prepareSkin()` preuploads a
palette within the supplied GPU budget. `renderer.debugSkeleton()`
draws the current joint hierarchy.

Across 1,024 warm update and playback-query iterations per platform,
xlang and handwritten C performed zero XRT heap operations. Replacing
an unmasked clip used two native allocations per call in both versions.
Explicit mask, binding and joint arrays incur temporary projection
storage before the native copy.

[Animation example](../lib/bindings/xge/three_d/examples/animation.xl)

## Terrain and collision

`terrainOptions()` supplies native defaults. Set width, depth and the
format of the input samples before calling `createTerrain()`. It accepts
a `bytes` or `Buffer` of native-endian U16 or F32 heights. A zero row
stride selects tightly packed samples. The constructor checks that the
last complete row fits before reading and copies the input into native
terrain storage.

Use `sample()` and `raycast()` in terrain-local coordinates with an explicit
LOD. `scene.instantiate(terrain)` creates the chunk nodes and assigns their
LODs. Both an instance and a `chunkMesh()` result can outlive the terrain.
The generated meshes are immutable so their geometry stays in sync with
the terrain's height grid.

Use `shapeSettings()` for sphere, box and capsule settings, or pass a
`Mesh3D` alongside the mesh shape kind to `scene.setCollider()`. The
scene retains that mesh, and `colliderMesh()` acquires a separate reference.
Collision uses its own layer mask and optional excluded subtree. Render
visibility does not affect collision queries. `sweep()` and
`collisionRaycast()` return the native status and a hit. `moveShape()`
returns a proposed displacement, recovery, remainder and contact normals.
The caller applies any resulting movement.

`queryAabb()` and `overlap()` reuse a `createNodeResults()` buffer.
`contacts()` uses a `createContactResults()` buffer. After a
`BUFFER_TOO_SMALL` result, `info().written` is the number of accessible
entries and `info().required` is the capacity needed for all matches.
With zero capacity the query counts only. The next query or successful
resize replaces the buffer's entries. `nodeAt()` and `contactAt()` return
independent value records.

On Windows and Linux, 1,024 iterations of terrain sampling, overlap and
reading a result from a preallocated buffer performed zero XRT heap
operations in both xlang and handwritten C. Terrain creation copies the
height grid, and query buffer creation or capacity growth allocates storage.

[Terrain example](../lib/bindings/xge/three_d/examples/terrain.xl) ·
[Collision example](../lib/bindings/xge/three_d/examples/collision.xl)
