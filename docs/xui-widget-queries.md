# Widget measurement and terminal cell snapshots

`Widget.measureContent(constraint)` calls `xuiWidgetMeasureContent` and returns
the widget's own clamped content size. `Widget.measure(constraint)` calls
`xuiWidgetMeasure` and returns its layout measurement, including the native
layout policy and child participation. Both take an exact `Vec2` constraint
and return an independent `Vec2`; measurement can update native caches. Neither
method arranges the widget or replaces the existing layout system.

The existing owned content-measure callback receives a `CallbackVec2` output
view valid only during that callback. It can fill the size; storing the view
does not extend its validity. Callback failure, invalid constraints, and a
widget closing during measurement are checked errors. Failed result admission
preserves the caller's previous value; native cache/other side effects are not
promised to roll back. Existing creator-thread and owner-expiry checks apply.

`Terminal.getCell(column,row)` calls `xuiTerminalGetCell` once and returns the
complete pointer-free `TerminalCell` value: size, codepoint, resolved foreground
and background colors, flags, style, width, link ID, combining count, reserved
bytes and sixteen combining codepoint slots. It is a 96-byte native record on
the qualified x64 platforms. Wide characters retain the native two-cell layout;
the continuation cell has width zero. The link ID is an identifier, not a URL
or a resource handle. Cell reads do not implement terminal search or sessions.

Changing a returned record, including its arrays, does not change the terminal.
The independent snapshot remains usable after the Context closes. Invalid
coordinates are checked failures and do not overwrite an existing destination.
The generic fixed-array record and direct-output contracts provide this mapping;
there is no new compiler/runtime rule or XGE forwarding function.

Native output is written into independent result storage. Record/value copies,
native layout/cache and terminal style resolution, ownership/error scopes and
callback epoch checks still have costs. Prepared count-free measurements on a
plain widget and a terminal cell read are compared with the same C workload;
heap counts alone do not establish equal CPU cost. Text shaping and custom
measurement callbacks can allocate independently of these prepared queries.

`examples/widget_measure.xl` demonstrates using a label's measured size.
`examples/terminal_cells.xl` displays a combining character and its cell data;
it starts no shell or child process. The separate query fixture covers full
array copying, wide cells, failed output publication, callback expiry/errors
and self-close, foreign-thread rejection, closed bound calls and heap costs.

Windows and Linux pass the matched full O2 compiler/SDK gate: each has 41
functional, native O0/O2 hosted/static, C11/C++11/C++17 header and language
JIT/dynamic/static steps. Both real-window lessons render two frames in every
mode; each lesson's same-platform readbacks match, and the images were inspected.
Linux uses Xvfb/Mesa software OpenGL, without claiming physical-driver coverage.

A separate 32-prefix cold text-layout measurement gate passes all three language
modes on both platforms (5 command steps each). It exercises real native text
shaping/cache allocation rather than assuming a plain widget query allocates:
8 budgets fail, 24 succeed, old destinations survive failures, and resources
close successfully. Prepared queries remain a separate zero-heap comparison.
Unused-declaration trimming and exact evidence hashes are recorded separately
in `dev/release-materials/provenance/xui-widget-queries-20261115.json`.

The trim control removes only these three query declarations and TerminalCell
from otherwise identical packages. Each platform passes 8 trim commands;
executable bytes, text and BSS deltas are all zero. The batch totals 54 actual
acceptance commands per platform (108 combined), excluding failed diagnostic
attempts, staging, inventory and evidence rehashes. Source/SDK/compiler reuse
does not imply final release installation or completion of the entire XGE goal.
