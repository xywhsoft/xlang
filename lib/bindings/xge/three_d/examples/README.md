# 3D values

Use the matched SDK with `XGE_ENABLE_3D=1`. These examples need no graphics
context, window, model file or network service.

| File | Observable result |
| --- | --- |
| `camera_values.xl` | Checks transform matrices and perspective/orthographic cameras. |
| `screen_ray.xl` | Preserves a 0.25-unit offset near a world origin of 10^12 and checks the centre screen ray points along negative Z. |
| `ray_queries.xl` | Tests a ray against a box and sphere; a short ray returns NOT_FOUND with cleared output. |
| `particles.xl` | Emits into an eight-particle CPU capacity, reports four dropped particles, then drains to zero. Requires `XGE3D_ENABLE_PARTICLES=1`. |
| `scene_hierarchy.xl` | Combines parent/child transforms, looks up a named node and keeps a shared scene usable after closing its first view. |

```text
xl run screen_ray.xl
xl build screen_ray.xl -o screen_ray.exe
```

`relativePosition` subtracts the double-precision origin before converting to
a float vector. `screenRay` takes framebuffer pixel coordinates from the
top-left; its origin lies on the near plane. Invalid extents, singular cameras
and unrepresentable relative coordinates raise checked errors. Values are
independent copies and need no explicit close. This does not start a renderer
or demonstrate the still separate scene/resource integration.

`rayAabb`, `raySphere` and `rayTriangle` return `(status, output)`. Check the
status against `xge.XGE_OK`, `xge.XGE_ERROR_NOT_FOUND` or an error code before
using the output. These APIs preserve native status codes; invalid geometry
returns `XGE_ERROR_INVALID_ARGUMENT`. Native queries normalize directions;
distances and the positive finite maximum are in world units. Starting inside
a volume returns distance zero. Triangles use exactly three fixed C-record
values, with no temporary heap array. All inputs remain independent values.
The triangle query accepts an existing fixed array, such as an array field or
native output. Standalone fixed-array declarations currently lack a verified
default initializer; that general compiler contract remains under discussion.
The separate qualification fixture obtains a fixed array from C and tests all
three queries; the beginner lesson does not depend on that fixture.
