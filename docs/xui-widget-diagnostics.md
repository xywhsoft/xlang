# XUI widget, render tree and Dock diagnostics

All three methods return independent value snapshots. Rectangles and flags
remain usable after a later layout, render-tree update, or widget close.
Use the layout and render-tree methods before sampling on-screen geometry.

| Method | Result order |
| --- | --- |
| `Widget.debugInfo()` | Depth, child count, visible, enabled, focusable, tab stop, focus scope, hit-test visible, drag enabled, drop enabled, IME mode, layer, z-index, dirty flags, event mask, subtree event mask, local rect, world rect, subtree dirty flags |
| `Context.getRenderNode(index)` | World rect, paint rect, layer, z-index, state ID, cache policy, flags |
| `DockPanel.getWindowInfo(window)` | Window ID, pane ID, state, region, previous region, previous side, previous tab index, closable, dockable, movable, resizable, resize edges, minimum width, minimum height, icon source rect, window rect, client rect, tab rect, auto-hide rect |

Render-node indexes are zero-based and refer to the current render-tree order.
Window IDs come from `addWindow()`. An invalid index or ID raises a checked
error. The Widget query uses the normal creator-thread and expiry checks.
Flags and state IDs retain their native numeric types.

The C records also carry transient object pointers. Use `Widget.getParent()`,
`Context.hitTest()` and the DockPanel's client, button and icon accessors to
obtain watched or retained resources. `DockPanel.getWindowTitle()` returns
the title as an independent string. The native size fields are supplied by
the bindings. Direct access to the render node's associated widget and the
Dock window's host widget still needs a watched record-field output.

Each query directly calls its XUI function once. The first render-node read
after a tree change can rebuild the native index and allocate. Value
projection uses stack records and still performs the normal checked call and
lifetime checks. See the [console example](../lib/bindings/xui/examples/dock_queries.xl).
