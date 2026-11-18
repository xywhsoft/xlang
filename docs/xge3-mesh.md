# 3D meshes and scene nodes

Create a triangle mesh with `xge3d.createMesh(vertices)`. Pass an
`array<c_ushort>` as `indices` for 16-bit indices. Use
`createMesh32(vertices, -1, indices)` with an `array<c_uint>` for larger
indices. Positions and normals are in mesh-local coordinates. The native
mesh copies input data and validates the triangle count, finite vertex
fields and index bounds. The size arguments select prefixes, with `-1`
using the complete array.

`scene.setMesh(node, mesh)` gives the node a native reference.
`scene.mesh(node)` returns an independently owned view or `null` for an
empty node. `mesh.share()` acquires another native reference. Each can
outlive the original language view. `scene.setLods(node, meshes, distances)`
owns one reference for each configured level, with up to three increasing
positive thresholds. Clearing or replacing the primary mesh releases the
old levels.

`mesh.update()` and `mesh.update32()` replace the data on the same mesh.
Every owner observes the update, and scene bounds are recomputed. Failure
preserves the previous geometry and ownership. `bounds()`, `info()`,
`vertexAt()` and `indexAt()` copy out a record or a scalar. Mesh access is
confined to the graphics creator thread. `prepare(budget)` can upload a
CPU mesh incrementally once the graphics context is available. Read
`completed` to decide whether to continue on a later frame.

A live mesh reserves the engine through its last native reference.
Release all views and detach the mesh from nodes and LODs before retrying a
checked engine close that returned Busy.

In Windows and Linux hosted JIT runs, 64 vertex reads through xlang and 64
equivalent C reads produced equal values and zero XRT heap operations.
Creating and closing 64 identical three-vertex indexed meshes used 384
allocations through xlang and 192 through C, with matching frees and zero
reallocations. The binding projects vertices into one temporary array per
call and reserves an owned language view. The scalar index array is
read-pinned in place. Injecting failure at each of the six creation
allocations returned a checked error without leaving a new native owner.

[Render a triangle](../lib/bindings/xge/three_d/examples/mesh.xl) to see
scene attachment, rendering and resource release in one frame.
