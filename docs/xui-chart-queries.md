# Chart state queries

These methods directly call the matching XUI function once. Their first result
preserves its native 0/1 state rather than treating it as a zero-success status.

| Method | Results in order |
| --- | --- |
| `getViewRange()` | manual flag, min X, max X, min Y, max Y |
| `getBrushRange()` | active flag, min X, max X, min Y, max Y |
| `getAnimation()` | active flag, enabled flag, duration, progress |
| `getSeriesAreaFill(series)` | enabled flag, color |
| `getSeriesValueRadius(series)` | enabled flag, minimum size, maximum size |
| `getSeriesValueColor(series)` | enabled flag, minimum color, maximum color |

Flags are `c_int`, ranges are `c_double`, animation and radius values are
`c_float`, and colors are `c_uint`. The native animation duration unit is
seconds. Disabling or clearing an option can retain its configured numeric
values. Inspect the flag before using those values as an enabled setting.

```text
chart.setViewRange(0, 3, 0, 8)
auto manual, minX, maxX, minY, maxY = chart.getViewRange()
assert manual == 1 and minX == 0 and maxX == 3 and minY == 0 and maxY == 8
```

Negative native returns produce a checked error and publish none of the tuple.
An unsupported series type for value radius/color is therefore an error. XUI's
area-fill query instead returns 0 for an invalid or non-line series and may
leave the color output untouched. Its language result is `(0, 0)` because the
local output storage starts at zero. This deliberately preserves XUI behavior.
Closing the widget or its Context invalidates queries through every alias;
normal Widget creation-thread admission remains in effect.

No per-method C wrappers, arrays, text scans or extra native calls are added.
The direct path has C-local tuple storage, normal lifetime/code admission and
an error boundary. The qualified CPU comparison uses the same native SDK and
6,000 queries per language/native interval with zero XRT heap operations;
it does not claim equal CPU cost or qualify a GPU backend.

`test/cases/foreign_xui_chart_queries.xl` compares every query to handwritten C,
including inactive states, normalized brush ranges, animation progress,
preserved style values, failed tuple assignments, bound/protocol/dynamic calls
and closed-widget rejection. `tools/check_xui_chart_queries.py` runs actual
complete-compiler JIT, dynamic/static independent executables, C/C++ headers and
O0/O2 native host/static oracles using a deterministic CPU drawing backend.
The visual [Chart lesson](../lib/bindings/xui/examples/chart.xl) shows the manual
range alongside its four plotted points. See the receipt in
`dev/release-materials/provenance/xui-chart-queries-20261117.json` for the actual
qualification scope; this batch does not install the runtime or IDE.
