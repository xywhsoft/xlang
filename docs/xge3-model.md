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

## Texture inputs

`xge3d.createTexture(image)` takes a straight-alpha RGBA8 `Image`.
Load encoded images with `xge.XGE_IMAGE_STRAIGHT_ALPHA`. The original
image supplies the pixels and row stride during the call. The native texture
copies the pixels into its own storage, so the image may be closed immediately.
For raw inputs, `createTexture(width, height, pixels)` accepts either
`bytes` or `Buffer` and checks that the full image fits. A zero-initialized
sampler selects the native defaults. Pass a sampler and sRGB flag to
configure either form.

[Material example](../lib/bindings/xge/three_d/examples/material.xl)

## Complete material updates

`materialSettingsDefault()` returns a value with the native material factors
and five texture mappings, in `XGE3D_MAP_*` order. `material.settings()` takes
a copy of the current factors and mappings. Use `mapTexture(slot)` to obtain an
independent texture reference for a slot.

`createMaterial(settings, baseColor=texture)` installs the supplied state
at creation. `material.setState(settings, ...)` replaces all factors,
mappings and texture slots together. Omitted texture arguments clear their
slots. A rejected change preserves the old state. Use `setMap()` to change
just one slot. Keep texture inputs valid through the call; successful
materials retain their own references.

`material.snapshot()` captures the factors, all five mappings and all five
texture references together. The captured settings and slot assignments
remain available after the source material changes or closes. Each
`snapshot.mapTexture(slot)` returns a separately owned texture, or `null`
for an empty slot. The underlying pixels remain shared with other holders
of that texture. Use the snapshot on its creator thread and close it before
shutting down the engine. [Material snapshot example](../lib/bindings/xge/three_d/examples/material_snapshot.xl)

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
`setLayer(layer, clip)` immediately replaces a layer using the native
defaults unless time, speed, weight or looping are supplied. Omit the clip
to clear the layer and cancel its transition. `layerTime()` reads the native float
time directly. An empty layer or invalid index returns zero, while
`playback()` supplies checked validation and the active flag.
`playMapped()` accepts an optional target-node mask and explicit source
and target indices. With zero fade it uses the same immediate replacement
operation as `setLayer()`. Native code copies the mask and bindings. Default
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

## Local and provider model caching

`createModelCache()` creates an explicit cache bound to its creating thread.
Local paths use absolute lexical keys. Resource providers use exact URI keys
when enabled. The cache retains an optional decode quota, so the caller can
close its original quota view after creating the cache.

`load()` returns an owned reference to a cached model. `request()` creates
an individual subscription and coalesces active jobs for the same key.
Call `pump()` with a native upload budget and use `status()` to observe
progress. `take()` consumes a ready subscription and returns the model.
`cancel()` affects the specified subscription and stops the shared job when
its last subscriber cancels. Use `release()` for subscriptions you do not
take, including failed or cancelled ones.

The cache retains its own model references. `trim()` evicts unused entries
to reach the soft limits. `clear()` evicts all unused entries. Live model
owners and subscriptions can keep the cache above its targets. A model
reference obtained by `load()` or `take()` remains valid after cache closure.
The cache publishes immutable shared materials, geometry and textures.
Assign an independently created material to a scene node to customize an
instance. Closing a cache with outstanding work cancels it and joins the
worker, which can wait for an in-flight provider call.

In 128 warm iterations of `load()`, `info()` and `close()`, xlang incurred
three more XRT allocations and frees per iteration than equivalent C.
The Windows counts were 8 versus 5 per iteration and the Linux counts
were 6 versus 3. Both paths normalize the path and retain the same
cached model on a hit. The measurement counts heap operations.

[Model cache example](../lib/bindings/xge/three_d/examples/model_cache.xl)

## Declarative scene loading

`Scene.loadColumns()` loads a hierarchy from typed columns. Parent indices
must point to an earlier row. `sceneRootParent()` places a row immediately
under the returned group root, and `sceneNodeTransform()` supplies identity.
Nullable names, meshes, materials and model URIs allow plain grouping rows.
The hidden flag controls initial visibility.

A row may name a model URI or use a preloaded `Model3D` in the optional
resolved-model column. The latter lets the application apply its own lookup
or loading policy. Passing a model cache shares URI results with the
application's other loads. Without a cache, URIs use the managed loader.
The resulting scene retains its assets independently of these inputs.

Resolution completes before node construction. If resolution or construction
fails, the call leaves no partial hierarchy. Successful cache fills may remain
available after a later row fails. The compiler projects one native record
array for the row data, with read pins and per-element resource scopes. An
explicit resolved-model column adds a temporary native pointer array and
resource scopes. Native node names are copied into the scene.

On both Windows and Linux, 128 warm iterations that loaded and destroyed
a two-row hierarchy used 14 XRT allocations and frees per xlang iteration
and 7 in equivalent C. The rows had names and transforms, with empty asset
columns. The measurement counts heap operations for the entire loop.

[Scene loading example](../lib/bindings/xge/three_d/examples/scene_load.xl)

## Model retirement

Call `model.beginRetirement()` on the graphics thread while its context is
active. Additional native model references, such as a live scene instance or
cache entry, cause a checked Busy error. Once retirement begins, the token
holds its own reference and ordinary model queries fail. Close the original
view so the token can release the final model reference.

Call `step()` with the operation count and optional soft microsecond budget
until it returns 1. A zero operation count only checks progress. Begin checks
the model's uploaded mesh and texture contexts before it allocates a token.
Each step works on the original graphics context. `info()` reports cumulative
progress. Close the token on the creator thread. Closing it before completion
leaves the final owner responsible for synchronous cleanup.

[Model retirement example](../lib/bindings/xge/three_d/examples/model_retirement.xl)

## Camera orientation

`viewLookAt()` takes an eye and target relative to the scene origin.
`viewLookAtGlobal()` accepts a double-precision origin, eye and target.
It subtracts the origin before checking range and converting to floats.
Assign either result to `camera.view` to preserve the projection and clip
planes. `screenRay()` uses the resulting view and projection together,
and returns a ray in the scene's origin-relative coordinates.

Coincident eye and target, a parallel up direction and unrepresentable
coordinates produce checked errors. A failed call leaves the previous
camera value intact. Each binding calls the native matrix producer once.

On Windows and Linux, 1,024 iterations constructing both views produced
zero XRT heap allocations, reallocations or frees in xlang and handwritten
C. The xlang API returns a 64-byte C matrix value for assignment to the
camera. The C comparison writes directly into the camera's view field.

[Camera example](../lib/bindings/xge/three_d/examples/look_at.xl)

## Read-only mesh geometry

`mesh.geometry()` retains the current CPU geometry. Read all vertex fields
with `vertices()`, and use `indexBits` to select `indices16()` or `indices32()`.
For a non-indexed mesh, either index sequence is empty and `indexBits` is 0.
Requesting the wrong width on an indexed mesh raises a checked error.

Existing sequences keep their original data after mesh updates. They can
outlive both the geometry handle and the graphics engine on the creator
thread. The native owner retains the existing buffers without copying
them. Each vertex read copies one C record by value.

On Windows and Linux, acquiring the geometry and vertex sequence used
eight XRT allocations in xlang. Acquiring the geometry and vertex pointer
directly in C used one. After setup, 1,024 traversals of the same three
vertices used no XRT heap allocations, reallocations or frees in either
case.

[Read-only mesh example](../lib/bindings/xge/three_d/examples/mesh_geometry.xl)

## Reusable billboard batches

`createBillboardBatch(geometry)` validates and copies up to 65,536 rows
into an immutable CPU batch. It can be used before graphics initialization.
Read its rows through `batch.geometry()` or submit the batch with one
texture for all rows or a texture array. The selected texture count must
match the batch row count. A null texture uses white.

The renderer retains the textures and copies the commands on successful
submission. A failed submission leaves the existing commands intact. An
empty batch clears the queue. Custom batches skip repeated geometry
validation. Mixed textures still require projecting and pinning each
selected texture for the native call.

In 256 warmed submissions of three rows on Windows and Linux, xlang used
two XRT allocations and two frees per mixed-texture batch. Submitting
both arrays used three of each. Submitting a batch with a single texture
choice, or using the equivalent direct C calls, used no XRT heap
operations after warm-up. The counts cover submission, before rendering.

[Custom billboard example](../lib/bindings/xge/three_d/examples/billboard_batch.xl)

Use `renderer.setParticleBillboards(particles, blend, texture)` to submit
live particles directly from the simulation. It generates commands in the
renderer’s reusable staging buffer and retains the optional shared texture
on publication. Empty particles clear the submission. Later simulation
updates leave the submitted rows intact until the next successful call.
Use `particles.collectBillboards()` to keep or inspect an independent batch.
[Particle rendering example](../lib/bindings/xge/three_d/examples/billboards.xl)

## Scene node data

`scene.setUserData(node, value)` stores an independent data snapshot.
It includes public stored fields of supported classes. Passing null
clears the attachment. Handles, pointers and cycles produce checked
errors. A failed write keeps the previous attachment.

`scene.userData(node)` returns an independent `var` or null. The returned
data stays valid after deleting the node or closing the scene. An
ordinary clone starts with no attachment. `XGE3D_CLONE_USER_DATA` keeps
the attachment on the clone through a reference to the private snapshot.
Replacing or clearing either node's data leaves the other node's
attachment intact.

The native API keeps the borrowed C pointer slot and the owned XRT value
slot separate. Managed scenes trace all occupied value slots, including
slots awaiting physical destruction after a budgeted delete. The native
setter copies plain XRT data. The language setter first projects the
input to public data, then calls that same setter.

For 128 warmed operations on a one-key object, writing from xlang took
eight XRT allocations and frees per operation. Writing plain data directly
from C took four. Reading a private snapshot took four on both paths.
These counts were identical on Windows and Linux.

[Scene node data example](../lib/bindings/xge/three_d/examples/node_user_data.xl)
