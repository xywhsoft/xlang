# Large world positions and screen rays

`xge3d.relativePosition(origin, globalPosition)` subtracts the two native double
vectors before producing a float vector. It preserves local offsets in a large
world; finite inputs whose difference cannot be represented are checked errors.
`xge3d.screenRay(camera, x, y, width, height)` uses framebuffer coordinates
from the top left and returns the native near-plane origin and normalized
direction. Invalid extents and singular camera matrices are checked errors.

Both are direct declarations using the existing exact C records. The camera is
passed by address; output is prepared on the stack and published only after
success. Failed calls preserve an existing assigned language value. No C
forwarding implementation, boxed operation dispatcher or heap output is added.
Use an SDK with `XGE_ENABLE_3D=1`. The standalone
`lib/bindings/xge/three_d/examples/screen_ray.xl` example needs no window or GPU.

Windows full r31 passes 21 commands: fixture/example JIT, independent dynamic
and static programs, real C++17 archive ABI comparison, three public header
modes and executable import checks. Large coordinates, projection, corner
direction, invalid extents, finite overflow and singular matrices are exercised.
C and language loops each execute 1000 pairs with zero XRT heap operations.
Value transfer, stack storage, scalar conversions and checked status still cost
work; CPU throughput is not equated. The receipt is
`dev/release-materials/provenance/xge-spatial-values-windows-20261113.json`.
Linux, GPU/stateful 3D, package shared-library initialization and final delivery
remain separate qualification work.
