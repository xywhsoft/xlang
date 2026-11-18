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
