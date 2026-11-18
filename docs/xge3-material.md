# 3D materials, textures and shader hooks

Start with `xge3d.materialDefault()`, edit the PBR factors and pass them to
`createMaterial(params)`. Set `unlit` for a material whose base color is
independent of the scene lighting. `params()` returns the current factors
as a value. `setParams()` validates and replaces the factors while
preserving the maps and the shader.

`setMap(slot, texture)` sets a texture with identity coordinates. Pass a
`TextureMapping` to choose the UV channel, offset, scale and rotation.
Call `setMap(slot)` to clear it. A target's sampled texture can be used
for base color or emission. The renderer checks for feedback if a frame
would sample the target it is writing to.

Use `Texture3D.update(x, y, width, height, rgba, stride)` to replace
straight-alpha RGBA8 pixels. A zero stride selects tightly packed rows.
The byte span must reach through the last row of the rectangle. Both
`bytes` and `Buffer` are accepted. Target-backed textures are read-only.
`prepare(budget)` supports incremental upload and reports whether the
texture is ready.

`createShader(vertex, surface, boundsMargin)` compiles custom GLSL hooks
on the active graphics context. Omitted hooks use the native identity
functions. A vertex hook that moves geometry should supply a local-space
bound for the displacement. `setShader(shader, uniforms)` copies four
`vec4` uniforms from a `c_float[16]` into the material. Pass `null` as
the shader to use the renderer's default program.

A scene node owns its material. The material owns each assigned texture
and shader. `setMaterialTree(root, material)` applies to mesh nodes
under the selected root. `scene.material(node)`, `mapTexture(slot)` and
`share()` acquire independent native references. The lookup methods
return `null` for empty slots. Each owner can close separately on the
graphics creator thread. The engine remains Busy until the outstanding
graphics owners are released.

Both Windows and Linux validated the shader and the updated texture with
pixel readback in JIT and AOT. In 64 repeated hosted JIT iterations,
setting factors, setting a map, updating existing CPU pixels and reading
factors produced zero XRT heap operations, as did the equivalent C loop.
Acquiring and closing a separate map texture view used two XRT allocations
per iteration in xlang and zero in C. Material creation rolled back at
each injected allocation failure with the engine's owner count intact.

[Textured triangle example](../lib/bindings/xge/three_d/examples/material.xl)
