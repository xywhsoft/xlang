# ShapeScene mask compositing

`ShapeScene.compositeMask(method, blend, bounds, draw, tolerance, parent, screenSpace)`
uses the scene as a mask for custom content. The callback receives the matrix
for the temporary render target after cropping to the source, mask and current
clip. Use `transform.value` with `Shape.drawPxEx` or
`ShapeScene.drawPxEx` to draw into that target. The logical transform view
expires when the callback returns. The copied matrix value can be kept.
`screenSpace` defaults to 1. Pass 0 to apply the current camera first.

An empty intersection can complete without invoking the callback. While a
callback is active, native code pins the mask scene and rejects changes to its
structure with Busy. Balanced nested passes are allowed. If the callback
leaves a pass active, the renderer ends that pass before restoring the
original render target. A normal callback return in this case reports a
checked state error. An exception in the callback keeps its original error.
The pass and clip are restored before the error is delivered.

The declaration calls `xgeShapeExMaskCompositeScene` directly. The compiler
uses its existing synchronous callback thunk and callback lifetime check.
Reading the transform copies six floats. The mask scene uses a reference
acquire/release pair and a structural guard. Native compositing still uses
temporary render targets, submissions, and extra drawing and compositing
passes. Target reuse is the existing renderer behavior.

Run [shape_mask.xl](../lib/bindings/xge/examples/shape_mask.xl) with a graphics
backend. It draws orange through a circular mask and closes after 90 frames.

The Windows and Linux OpenGL tests read inside and outside pixels and check
that callback errors and an abandoned nested pass leave the outer pass and clip
usable. The Win32 and Linux build gates run the same fixture in JIT, dynamic AOT
and static AOT. The Linux runs use Xvfb and software OpenGL.

In a 1,000-call native comparison with the same clipped-out bounds and mask,
both builds made 2,000 allocations and 2,000 frees on each platform. The bounds
work still runs on that path. There were no draw callbacks or reallocations.
See the [build and runtime evidence](../dev/release-materials/provenance/xge-shape-mask-20261120.json).
