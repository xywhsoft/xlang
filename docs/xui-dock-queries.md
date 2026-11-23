# Dock pane splitting and drop targets

`DockPanel.dockWindowToPaneSide(window,pane,side,ratio=0)` calls
`xuiDockPanelDockWindowToPaneSide` once and returns its numeric pane output.
The C status is checked before publishing the destination. The native default
ratio is 0.25 and positive ratios are clamped to 0.10–0.90. Invalid windows,
panes, sides and non-finite ratios fail without overwriting a language result.
As in C, a successful no-op can return -1 when no new pane was produced.
The output is an id in this DockPanel, not an independent owning resource.

`findDropTarget(window,x,y)` returns an independent, pointer-free
`DockDropInfo`. Coordinates are local to the panel. `bValid=0` is a successful
miss, including an unknown/non-dockable window or a position outside the
panel. A valid record contains the target pane, region, side and preview rect.
The method queries a potential drop; it does not perform a drop, change the
current drag preview or alter docking state. Both methods retain native
semantics rather than interpret the returned status as a found flag.

The existing `@c_output` declarations supply one scalar output slot or one
40-byte record on the C stack. There are no new C forwarding functions,
boxed results or alternate Dock implementation. Native layout synchronization,
window-tree mutations, checks and generic resource scopes retain their costs;
splitting is not a zero-allocation operation claim. Warm target hit/miss loops
compare language/native heap counters and exercise allocation budget zero.
No CPU timing equivalence is claimed.

The console lesson `lib/bindings/xui/examples/dock_queries.xl` creates two
windows, splits their panes and inspects a potential drop without starting a
graphics window. The dedicated QA fixture covers all four split sides, the
native no-op sentinel, local coordinates on an offset panel, independent
records, misses, C parity, invalid inputs, checked output preservation,
wrong-thread and expired views. Its gate checks C/C++ headers and executes
JIT, dynamic AOT and static AOT on matched SDKs. This is CPU geometry
qualification, not interactive drag rendering or final IDE delivery.

## Window data

Use `DockPanel.setWindowData(window, value)` to attach a data snapshot and
`getWindowData(window)` to read an independent snapshot. A valid window without
data returns `null`. Assigning `null` clears it. Invalid window indices and
failed snapshot conversions report checked errors. Input projection reads
public storage fields and preserves embedded NUL and binary data.

The value stays with its window through docking, floating, hiding and layout
restoration. `saveState()` records layout only. Clearing the panel releases
its window data. Reused indices start empty, and any snapshot already returned
to the caller stays usable. The example `dock_state.xl` assigns an inspector
role and checks it after restoring the layout.

Both methods call the native retained-value API directly. The native table is
allocated lazily, and prepared native values can be replaced or retained for
output without allocating. Language snapshots traverse and copy data on each
call. The existing raw C user pointer has independent storage and ownership.

In the warmed JIT sample, 4,000 integer set/get pairs incurred zero XRT
allocations in both C and xlang on Windows and Linux. The two-field object
sample incurred 52,000 balanced allocations in xlang and zero in the native
retain/read loop. The sample includes result lookup and validation.
