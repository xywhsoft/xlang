# CodeEdit margins

`editor.margins()` observes the live margin model and keeps the editor's
Context alive. Closing the editor invalidates the view. Closing the view
detaches its observer. The editor and installed callbacks remain active.
Changing a margin's size or visibility schedules layout and rendering.

`editor.setMarginRenderCallback(id, callback)` and
`setMarginEventCallback(id, callback)` store the closures on the editor.
Pass `null` to clear a role. The callback may use its DrawContext through
the existing Proxy draw methods during the invocation.
See [the custom margin lesson](../lib/bindings/xui/examples/code_edit_margin.xl).

`measureLayout(viewport)` returns the visible margin count and text rectangle.
`findHit(viewport,x,y,lineHeight,scrollY=0)` returns a pointer-free
`CodeMarginHit`. The id is zero when the point misses. Invalid geometry still
raises a checked error. Both queries also work with owned callbacks installed.
Use `getState` and `getRect` to inspect individual margins.

`captureLayout(viewport, limit=-1)` captures the visible margins in display
order. Use a nonnegative limit for a prefix. `layout.getState()` always
returns the full visible count and text rectangle, including with a limit of
zero. `layout.entries()` returns a read-only sequence of numeric states and
rectangles. It holds its own reference to the captured storage, so you can
close the layout and continue reading the sequence after the model changes or
the editor closes. The snapshot contains only numeric geometry.
For only the count and text rectangle, use the allocation-free `measureLayout()`.
Native capture validates the full layout, then writes the selected entries into
one allocation. Reading entries from the snapshot shares this storage. In a
1,000-iteration capture, state read, traversal and release comparison, C used
1,000 XRT allocations and frees and xlang used 8,000. Traversing the same
already-acquired language sequence 1,000 times used zero allocations, frees
and reallocations. These measurements count heap operations.

Windows and Linux acceptance exercises real render and pointer callbacks, with
JIT and standalone dynamic/static builds. The same CPU-rendering workload
performs three XRT allocations and frees per frame with either callback.
See [the current acceptance record](../dev/release-materials/provenance/xui-codeedit-margin-20261120.json)
for timings and SDK hashes.

## Standalone model

`xui.createCodeMargin()` returns one unique, watched owner. `CodeMarginModel`
declares native clear/defaults/add/remove/width/visibility/query methods directly,
with native scopes, a real callback ownership graph and checked consumption.
`add(id,kind,width,flags=VISIBLE)` constructs one zeroed native descriptor on
the generated C stack. There is no per-method C forwarding wrapper.

`getState(index)` returns the pointer-free `CodeMarginState`; `getTotalWidth`
and `getRect` use checked native outputs. Invalid geometry/indices and native
allocation errors leave language output assignments unchanged. GetCount is
admitted by the model scope before using the native count-or-zero function.
Unknown or hidden ids keep the native unsupported checked outcome in getRect;
hit-test's normal no-hit result is not silently mapped to an exception API.
`setRenderCallback` and `setEventCallback` hold independent closures and can be
cleared with null. Native replacement or clear can commit before a consuming
callback Drop reports failure. Explicit close invalidates further use.

The render callback receives a callback-period Widget, margin id, line,
callback-period DrawContext and numeric Rect. The event callback receives
Widget, id, line, event type and Rect. Long-lived data must be copied; saving a
callback view does not extend its invocation. The binding uses the existing
generic owned-callback, record, stack-pack, scope and native-view contracts.
It changes no compiler or language rule.

The matched native SDK owns per-role callback slots, rereads admission after
reentrant Hold, detaches before Drop, and pins a whole CodeEdit layout plus
each invocation. The native tests exercise real rendering and input, including
clear during a frame and closure of the editor. Built-in margins retain direct
drawing. Legacy pointer-bearing Get/Layout refuse owned callbacks rather than
export a closure without its owner or conflate the two user pointers.

Native storage measured on x64: model64 requested bytes, slot72, one allocation
each; cached layout entry72 and caller frame96 (including model scope72). The previous
lifetime foundation used model56 and layout entry64. Prepared native frame
loops allocate nothing and make no external Hold/Drop calls, while private
reference changes, graph/Context/checked admission and callback bookkeeping
remain. Numeric language/C loops are measured separately. These results do
not imply CPU equivalence or a zero-cost ownership layer. Frame preparation
copies M layout entries and looks up their lazy slots, O(M*S) in the worst
case for S registered role slots; an invocation uses the cached node directly.
The packed input descriptor uses 48 stack bytes on x64. The one-allocation
slot measurement covers native storage. Language closure creation and
registration are outside the timed render workload.

`examples/code_margin.xl` is a window-free lesson for the standalone model.
Its language fixture covers direct packing, numeric queries, output preservation,
callback publication, registration OOM, cycle cleanup and closed-handle rejection.
The pointer-bearing C layout array remains available through explicit C interop.

See [the standalone model acceptance record](../dev/release-materials/provenance/xui-margin-owned-20261115.json)
for the earlier builds, allocation counts and callback-slot tests.

## Built-in line numbers and change markers

`CodeEdit.setLineNumberLayout()` sets automatic width, horizontal padding on
each side, and alignment. The defaults are automatic width, 2 pixels and
right alignment. `getLineNumberLayout()` returns those three values as a tuple.

`getLineChange(line)` reads the marker for a zero-based document line. A
deletion marker appears on the surviving line. `clearLineChanges()` clears
the markers while preserving the document text and dirty state. The four
methods call the corresponding XUI functions directly.

The native editor discards saved or cleared markers by resetting their active
count. A clean line query and an explicit clear take constant time. Tracking
the next edit reuses the table's capacity and initializes the active rows.
Insertions and deletions may move the affected suffix of the table.

The [line margin example](../lib/bindings/xui/examples/code_edit_line_margin.xl)
sets the layout and shows the marker created by inserting a line.
