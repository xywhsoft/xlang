# Inspecting XUI widgets by position

`Context.inspectWidgetAt(x, y, flags=0)` uses the same stacking, clipping and
hit flags as `Context.hitTest`. Coordinates are in the Context's UI space.
Finite fractional coordinates truncate toward zero, as in the native point
inspector. Zero flags use the native default, which skips disabled and hidden
widgets.

A hit returns the selected widget, its optional parent, the local and world
rectangles, then these facts in order:

- Depth and child count.
- Visible, enabled, focusable, tab stop, focus scope and hit-test visibility.
- Drag enabled, drop enabled, IME mode, layer and z-index.
- Dirty flags, subtree dirty flags, event mask and subtree event mask.

A miss returns null for both handles and zero for every fact. Layout and
stacking failures and invalid coordinates raise a checked error. The returned
widget and parent are observed Context-owned views. Either becomes invalid
when the corresponding native widget is closed. The diagnostic facts are
independent values and can still be read afterward.

The declaration makes one call to the checked XUI inspector. It may need to
run layout and build an invalidated stacking cache. The native inspector uses
stack-allocated lifetime links until deferred cleanup completes. The language
uses its normal owner-view admission, which can allocate on the first view.
The [window-free example](../lib/bindings/xui/examples/widget_inspect.xl) uses
the depth and world width to report the selected widget.
