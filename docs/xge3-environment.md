# 3D environments and configurable frames

`createEnvironment(edge, rgba)` takes six square, tightly packed
straight-alpha RGBA8 faces. Pass `bytes` or `Buffer`, with exactly
`edge * edge * 4 * 6` bytes. The image overload takes six decoded
straight-alpha `Image` objects and accepts source row strides. In both
overloads the face order is +X, -X, +Y, -Y, +Z, -Z and `srgb` defaults to 1.
The engine copies the pixels at creation, so the inputs can be released
before rendering.

`setIBL()` installs precomputed linear RGBA8 irradiance, a prefiltered
specular cube and a BRDF lookup image. For prefiltered input, pack the six
faces of the largest mip first, then the six faces of each smaller mip down
through 1×1. The largest edge must be a power of two. The BRDF lookup uses R
for scale and G for bias. `setIBLImages()` takes the same mip order as a flat
array of decoded images. Replacement preserves the previous lighting if
validation or allocation fails. `clearIBL()` removes lighting and preserves
the sky. Uploaded lighting is changed on its active graphics context.

Start with `frameOptionsDefault()` and pass it to
`renderer.renderFrame(scene, camera, options, target, environment)`.
The target and environment are optional. Set `ibl_intensity` above zero
after installing IBL. To use linear fog or shadows, set the corresponding
`enable_fog` or `enable_shadows` field and edit the supplied defaults.
The same call configures exposure, clear flags, culling and instancing.
The engine reports Unsupported if the chosen frame needs a feature
omitted from its build.

`Environment3D.share()` acquires an independent native reference.
The engine keeps the graphics lease until the last reference is released.
Rendering, mutation and checked release use the graphics creator thread.
The environment is held for the duration of a frame call.

Windows and Linux tests read back the sky, indirect lighting and fog,
exercise the shadow pass and verify that cube and BRDF uploads restore the
host's pixel-unpack state. In hosted JIT, 64 warm frames made zero XRT heap
operations on both platforms, matching the equivalent C calls. Relative to
C, creating an environment from packed bytes added two XRT allocations per
call for the owning view. The image-array path added two further temporary
allocations for projecting and pinning the image pointers.

[Sky cube example](../lib/bindings/xge/three_d/examples/environment.xl)
