# 3D renderer controls and diagnostics

`setOptions()` copies the numeric options returned by
`threeDRendererOptionsDefault()`. Set the render and shadow layer masks,
maximum lights and profiling flag before rendering. `resetOptions()` restores
native defaults. `profile()` returns the latest counters and CPU timings as
an independent value. GPU timings are optional and can refer to a completed
measurement from an earlier frame. Check `gpu_available` and `gpu_sample`.

`prepare(shadowResolution,budget)` can warm the shadow atlas. Its returned
statistics distinguish completed work, new operations and a budget that
allows no progress. The native API counts a driver allocation as one
operation. Repeating a prepared resolution reuses the atlas.

FXAA, color grading and tone mapping, bloom, and height fog are available
with the corresponding native SDK features. The settings records come from
the `threeD*Default()` functions. The set methods preserve native validation,
and the reset methods pass the native null input. Active bloom or color
processing requires a color clear when rendering a frame. `bloomInfo()`
reports the live levels and storage, and resetting bloom releases its
textures.

`debugLine()`, `debugBox()` and `debugAxes()` enqueue persistent geometry
in renderer coordinates. `clearDebug()` empties that queue. The line and
box colors use linear RGBA. Match the scene origin when supplying
coordinates, and draw 2D UI after the 3D frame. The
[debug example](../lib/bindings/xge/three_d/examples/debug_draw.xl)
draws one line and prints the native CPU timing.

The renderer fixture runs on Windows and Linux in JIT, hosted AOT and
static AOT. It checks debug pixels with GPU readback, including their removal
after clearing the queue. In each platform's hosted JIT run, 64 repeated
profile queries and 64 equivalent C queries produced equal totals and no
XRT allocations, frees or reallocations after warm-up.
