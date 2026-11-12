# 3D values

Use the matched SDK with `XGE_ENABLE_3D=1`. These examples need no graphics
context, window, model file or network service.

| File | Observable result |
| --- | --- |
| `camera_values.xl` | Checks transform matrices and perspective/orthographic cameras. |
| `screen_ray.xl` | Preserves a 0.25-unit offset near a world origin of 10^12 and checks the centre screen ray points along negative Z. |

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
