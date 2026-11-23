# Inventory hit and visible-range queries

`InventoryGrid.toValue()` returns an independent snapshot in the native
`xui.inventorygrid.v1` format. It includes the layout, colors, interaction
state, gamepad settings and slot metadata. Icons and animations contribute
metadata only. The result remains valid after the widget or context closes.
Native export failures are checked. The binding adopts the native value tree
directly, with no second serialization or deep copy.

`InventoryGrid.hitTest(worldX,worldY)` calls `xuiInventoryGridHitTest` once and
returns a copied `InventoryHit`. Its input coordinates are in world space.
The returned `fX`, `fY` and `tRect` are local to the InventoryGrid. To inspect a
known slot, add the Widget's world origin to a point in `getSlotRect(slot)`.
This differs from TimeLineView's local-coordinate hit-test input.

Native hit parts distinguish the slot, icon, count and hotkey regions. These
are geometric regions even when a slot has no displayed count/icon text. A
point in slot spacing or outside the viewport succeeds with
`XUI_INVENTORY_HIT_NONE`, slot -1 and a zero rectangle. The record still contains
the local input point. NaN and either infinity are invalid and fail checked
without replacing an old result. An empty grid also returns an ordinary miss.

`getVisibleRange()` calls `xuiInventoryGridGetVisibleRange` once and returns the
complete `InventoryVisibleRange`: first/last slot, row and column; counts;
total rows/columns; and native paint-slot count. It resolves the current
geometry and scroll state. `getLastPaintRange()` instead returns the previously
painted range, so it can remain empty before any rendering. A first/last slot
pair can span hidden columns on intermediate rows; use `iSlotCount`, rather
than assuming `last-first+1` is the visible count. Partial rows/columns retain
the native intersection rules. An empty grid reports count zero and -1 bounds.

Both records already have exact public numeric declarations. The native
producers fill their `iSize`; no pointer-bearing record or output-array rule is
needed. The binding uses existing checked stack outputs, Widget/Context
protection and creator-thread admission. Independent record copies can be
changed without altering native state. A closed or foreign-thread Widget
fails checked and leaves a previous complete result unchanged.

The window-free lesson
[`inventory_queries.xl`](../lib/bindings/xui/examples/inventory_queries.xl)
creates 40 slots in four columns. With matched packages installed, run
`xl run inventory_queries.xl`: it prints visible slots 0–11, hit slot 0, then
slots 28–39 after scrolling the last slot into view. It uses no graphics
window, system-font discovery or network service. The existing
`inventorygrid.xl` remains the interactive selection lesson.

Use `setMetrics` to explicitly select zero layout padding. The native
`setLayout` descriptor treats a zero padding field as a default. This matters
to precise geometry examples; the fixture uses the same explicit metrics as
the lesson instead of altering native defaults.

`tools/check_xui_inventory_queries.py` checks all hit parts with a nonzero
Widget world origin, local output geometry, spacing/outside misses, invalid
coordinates, visible versus painted ranges, scroll changes, empty results,
independent copies, thread/close refusal and old-result preservation. It
checks C11/C++11/C++17 public headers, JIT/dynamic/static execution and independent
executable imports, and builds/runs the lesson on Windows and Linux.

For 1000 prepared rounds of visible-range, slot-hit and miss queries, both
language and handwritten C have zero XRT allocation/free/reallocation. The
language loop also succeeds under a zero allocation budget. The output
temporaries are 36 bytes for the hit and 52 bytes for the range on x64; checked
commit may copy them. Geometry synchronization, row counting, scalar/status/
thread checks, Widget/Context scopes and error bookkeeping remain work. Cold
setup, formatting and rendering are excluded; this is no CPU equivalence or
constant-time guarantee. No native implementation or compiler rule changed.

Pointer-bearing slot records, query/output arrays, animation ownership and
return-plus-output projections keep their separate integration status. Final
matching SDK/compiler/runtime/IDE installation remains in the full goal.

## Slot descriptors

`InventoryGrid.setSlot()` publishes one slot's item settings, two text fields,
icon and animation style as one native update. The owning Context retains an
icon from its Proxy. An incompatible icon or invalid field rejects the full
update. The native C caller's opaque animation object stays attached during
descriptor edits.

`getSlot()` returns the settings, label, hotkey, optional icon, icon rectangle
and tint, then animation flags, scale and tint. It copies the text and gives
the caller an independent icon reference. That reference remains usable after
clearing the slot or closing the grid. If any output conversion fails, the
caller keeps the previous complete result.

Each binding calls one native descriptor function. In the 2,000-call warmed
samples without an icon, xlang's setter allocated two temporary C strings
per call. The getter allocated eight times per call, including the two text
copies and the resource-result reservation. The handwritten C getter
allocated twice per call to copy the same strings.

[Slot descriptor example](../lib/bindings/xui/examples/inventory_content.xl) ·
[Validation and measurements](../dev/release-materials/provenance/xui-inventory-slot-descriptor-20261123.json)

## Per-slot animation

`setSlotAnimation(index, renderer, flags, scale, tint)` installs a renderer
for one slot. It takes precedence over the grid animation renderer and uses
the same drawing callback signature. The callback can capture application
animation state and draw through the callback's DrawContext. Use
`setSlotAnimationStyle()` to adjust the style without replacing the callback.
`getSlotAnimation()` reads whether the slot has its own renderer and returns
its flags, scale and tint in the same call.

Passing `null` to `setSlotAnimation()` clears the slot animation. Use
`setSlotAnimationStyle()` to let the grid renderer draw that slot again.
Clearing a slot, shrinking it out of the grid or closing the grid releases its
registration. An invocation keeps the callback and slot snapshot alive until
it returns. An exception in the renderer reaches the checked render call.

The native renderer index is allocated when the first painter is attached.
Capacity follows peak concurrent registrations until the grid closes.
Lookup uses binary search, and each dispatch pins the existing registration
without another owner Hold. The callback's Widget, slot and DrawContext views
are valid for that invocation. The grid keeps independent registrations after
content, icon and descriptor edits.

In 250 warmed paints driven by the same native CPU render loop, the C and
xlang callbacks produced identical allocation counts on both platforms.
Driving style and render from xlang on every iteration added one allocation
per iteration in this sample. The callbacks only incremented a counter.

[Per-slot animation example](../lib/bindings/xui/examples/inventory_paint.xl) ·
[Validation and measurements](../dev/release-materials/provenance/xui-inventory-slot-painters-20261123.json)
