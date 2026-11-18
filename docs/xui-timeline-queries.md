# Timeline hit records and offsets

`TimeLineView.hitTest(x,y)` returns an independent `TimelineHit` value through
the existing checked native-output contract. It directly calls
`xuiTimeLineViewHitTest` once. Its coordinates are local to the timeline Widget,
not world coordinates. The record contains hit type, layer index, stable layer
ID, frame index, span ID and a local rectangle. The rectangle is the native hit
rectangle; for a span hit it is the selected frame cell, not the entire span.

The native hit type distinguishes the corner, ruler, playhead, layer controls,
frame, selected frame, span and scrollbars. A normal miss succeeds with
`XUI_TIMELINE_HIT_NONE`, all four index/ID fields set to -1 and a zero rectangle.
It does not throw or return null. NaN, infinity and coordinates beyond the
native layout bound are invalid and throw without replacing an old result.
Layer indexes and layer IDs are distinct; do not interchange them when layers
are reordered.

`TimeLineView.getOffset()` returns `(c_float,c_float)` from the two output slots
of one `xuiTimeLineViewGetOffset` call. It reads the actual native scroll-frame
offset after native clamping. Use the existing `setOffset` and
`ensureFrameVisible` operations to change it. The hit test already incorporates
that offset when converting local positions to content frame/layer indexes.

Both calls retain the ordinary Widget/Context protection and creator-thread
checks. Closed or foreign-thread views fail checked. The output temporary is
committed only after the native status and scope complete successfully. A
copied hit has no pointers, callbacks or owner edges, and changing it does not
change the timeline.

The window-free lesson
[`timeline_queries.xl`](../lib/bindings/xui/examples/timeline_queries.xl) creates
one Animation layer and Move span, establishes explicit geometry and prints
frame 10, `Inside span: true` and zero initial offsets. With matched packages
installed, run it with `xl run timeline_queries.xl`. It needs no window, font
discovery or network service. The existing `timeline.xl` remains the visual
layers/span lesson.

`tools/check_xui_timeline_queries.py` tests a scrolled timeline against the real
native implementation. It checks span/selection/frame/ruler/playhead/corner/
layer/miss results, supplied local rectangles, independent copies, four invalid
coordinate forms, output preservation, thread and close refusal, public
C11/C++11/C++17 headers and JIT/dynamic/static programs. It also builds and runs
the lesson and checks independent executable imports. AOT has no compiler/TCC
DLL dependency; static executables have no XGE host import.

For 1000 prepared rounds of offset, span and miss queries, language and
handwritten C both perform zero XRT allocation/free/reallocation. The language
loop also runs under a zero allocation budget. The x64 output temporary is
36 bytes for `TimelineHit` and 8 bytes for the offset pair; successful commit
may copy those values. Status/scalar/thread/epoch checks, Context scopes and
native geometry/layer/span/selection searches remain work. Native hit testing
is not a constant-time claim. Setup, formatting and GUI rendering are outside
the measured loop; equal heap counts do not establish CPU equivalence.

The hit and offset binding reuses qualified O2 compiler and SDK cohorts.
Layer, frame, and span snapshots are documented in
[Timeline and MessageList state](xui-timeline-message-state.md).
