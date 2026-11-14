# XUI geometry queries

Twenty-seven existing native functions now have direct safe methods. They
produce ordinary values and keep the original native parameter order and
float/double/integer precision. No native library change or per-method C
forwarder is involved. The matched SDK retains XGE commit `c22608d6`.

| Class | Methods | Returned values in native order |
| --- | --- | --- |
| Button | getIconLayout | placement, icon size, gap |
| CheckCard | getMetrics | border, checked border, corner size, focus width |
| ScrollBar | getMetrics | thickness, minimum thumb size, button size |
| Slider | getMetrics | track size, knob size, track radius |
| RangeSlider | getMetrics | track size, knob size |
| Carousel | getMetrics | arrow size, indicator size/gap/bottom |
| VirtualJoystick | getMetrics | radius, knob size, dead zone |
| SplitLayout | getDividerMetrics | layout size, visual size, hit size |
| Window | getChrome | title bar, border, resize grip, button size |
| ScrollFrame | getContentSize, getOffset, getMetrics | width/height; x/y; scrollbar/thumb/button sizes |
| ScrollView | getContentSize, getViewportHint, getOffset, getScrollbarPolicy, getMetrics, getColors, getButtonColors, getCornerColors | width/height; hint width/height; x/y; x/y policies; scrollbar/thumb/button sizes; six colors; button/icon colors; corner/grip colors |
| Canvas | getViewportHint, getOffset | hint width/height; x/y |
| Popup | getScroll, getScrollbarStyle, getMetrics | x/y; mode/size; padding/border/shadow |
| Chart | dataToPixel, pixelToData | Vec2 with native float coordinates; two native double data coordinates |

For example:

```xl
auto track,knob,radius=slider.getMetrics()
auto pixel=chart.dataToPixel(25,10)
auto x,y=chart.pixelToData(pixel.fX,pixel.fY)
```

`tools/xui_geometry_contracts.py` contains the reviewed declaration source and
an idempotent targeted synchronizer. The full binding generator uses that same
source. It preserves other declarations and never guesses a status contract
from a function name. Generated public C/C++ headers assert the actual native
function signatures, including the mixed integer/float and double outputs.

These producers return XUI_OK on success. A nonzero native status is checked
before result publication; assignment to a previous scalar tuple or Vec2 keeps
the previous complete value on failure. Chart rejects non-finite input through
its original native implementation. Widget/Context lifetime, creation-thread
access and callback/code ownership continue to use the existing generic gates.

Several other Chart queries return an ordinary 0/1 state together with pointer
outputs: view range, brush range, animation and optional series settings. Their
return value must remain visible. They are **not** classified as zero-success
status functions by this batch; their remaining projection work stays in the
full integration plan.

The readable [geometry_queries.xl](../lib/bindings/xui/examples/geometry_queries.xl)
lesson shows slider dimensions and a real line chart. It explicitly selects a
numeric horizontal axis, assigns the Context's font and displays the coordinate
round trip with one decimal place. Display formatting does not change native
calculation precision. Slider range is explicitly 0–100. Fault injection,
thread/lifetime assertions and measurement loops stay in the separate fixture.
The catalog has 81 independent examples; this lesson does not change the 57
shared gallery scenes.

Each direct query calls its original native function once. Direct unpacking
uses C-local scalar/record payloads of 8–24 bytes, without arrays, text scans or
an added native allocation. The generic lifetime/thread admission, pinning,
code activity and error boundary still cost work. The 1000-call Slider query
comparison has zero XRT allocations, frees and reallocations in both language
and handwritten native C. This does not measure CPU equivalence, backend/module
startup, compilation/loading or GPU throughput, and does not establish whole
binding zero overhead.

An unchanged Slider lesson is also built with the same complete compiler and
identical native static archive against the previous and updated declarations.
Its file length and aggregate size of all text sections change by zero bytes
on both OSes. This is evidence for that selected program's unused declarations,
not a universal size or speed guarantee.

The qualified receipt is
`dev/release-materials/provenance/xui-geometry-queries-20261114.json`. It separates
CPU fixture acceptance from the final revised lesson, pins unchanged native
bytes and records package equivalence for reusing the CPU results. JIT and
independent dynamic/static AOT, public C11/C++11/C++17 headers, failed output
assignment, allocation budgets and thread/owner checks are covered on Windows
and Linux. Real lesson windows use the same pinned font; Linux uses local
Xvfb/software OpenGL. Final runtime/SDK/IDE installation and the complete
XGE/XUI/Document integration remain outstanding.
