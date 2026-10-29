# xLayout

xLayout is a retained, renderer-independent layout engine written in C. It
depends on XRT for allocation and has no dependency on XGE, XUI, widgets,
fonts, windows, rendering, or input.

Its central rule is:

> A parent decides how its children are arranged. A child only describes how
> it participates in that arrangement.

xLayout can arrange UI controls, text fragments, images, game data, or any
other objects that can report an intrinsic size. XUI is its first major
consumer, not part of the xLayout implementation.

## Source and build

The public API is in `xlayout.h`; the implementation is in `xlayout.c`.

When used as a standalone module, compile both `xlayout.c` and XRT into the
application. When using XGE's DLL build, xLayout is already part of
`xge.dll` and its public symbols are exported.

The module test and benchmark are built with:

```bat
lib\xlayout\test\build_test.bat
lib\xlayout\test\build_benchmark.bat
```

## Ownership and allocation

An `xlayout_context_t` owns every node created in it. A node is represented by
a generation-checked 64-bit handle. Destroyed handles do not become valid when
their storage is reused.

Nodes use XRT's fixed-size memory pool. Slots, tracks, line items, scalar work
arrays, and fragments are owned by the context and grow only when a larger
layout needs them. A stable tree does not allocate per node during repeated
layout.

The caller owns callback user data. xLayout borrows that pointer and never
frees it. Destroying a node destroys its complete layout subtree.

`xlayout_context_config_t` provides initial capacities:

- `initial_node_capacity` reserves the initial handle slot table. Node objects
  remain backed by XRT's self-growing fixed-size pool.
- `initial_workspace_capacity` reserves temporary line-item and scalar track
  workspace. Fragment storage grows on demand and is then reused.

Zero selects the implementation defaults.

## Nodes

A node has one of two roles:

- `XLAYOUT_ROLE_LEAF` obtains its intrinsic size from a measurement callback.
- `XLAYOUT_ROLE_CONTAINER` measures and arranges children using a formatting
  context. It may also have an intrinsic measurement callback.

Use `xLayoutNodeAppend` or `xLayoutNodeInsertBefore` to build the tree.
`xLayoutNodeRemove` detaches a node without destroying it.

Changing a node's style, role, tracks, callback, user data, or tree position
invalidates the required cached work automatically.

## Constraints and measurement

Every measurement receives four constraints:

```c
typedef struct {
    float min_width;
    float max_width;
    float min_height;
    float max_height;
} xlayout_constraints_t;
```

`xLayoutConstraints(max_width, max_height)` creates constraints with zero
minimums. `xLayoutConstraintsTight(width, height)` fixes both dimensions.
Use `XLAYOUT_UNBOUNDED` for an unconstrained maximum.

A leaf callback returns width, height, and baseline. The sizes are intrinsic
content sizes; xLayout applies padding, explicit lengths, and min/max limits.

```c
static bool measure_item(
    xlayout_context_t* context,
    xlayout_node_t node,
    const xlayout_constraints_t* constraints,
    void* user_data,
    xlayout_measure_t* output)
{
    const item_t* item = user_data;
    (void)context;
    (void)node;
    (void)constraints;
    output->width = item->width;
    output->height = item->height;
    output->baseline = item->baseline;
    return true;
}
```

The baseline is measured from the top edge. A negative baseline requests the
default bottom-edge baseline.

## Style model

`xlayout_style_t` separates three concerns:

- `size` describes the node's own width, height, and min/max limits.
- `item` describes how the node participates in its parent.
- `container` describes how the node arranges its children.

This separation is intentional. For example, a child's `grow` value is read by
its parent Stack, while the child's own Stack direction applies only to its
children.

Start from `xLayoutStyleDefault()` and change only the required fields.

### Lengths

- `XLAYOUT_LENGTH_AUTO` uses intrinsic measurement.
- `XLAYOUT_LENGTH_FIXED` uses an absolute size.
- `XLAYOUT_LENGTH_PERCENT` uses a ratio of the available parent size.
- `XLAYOUT_LENGTH_FILL` accepts the available size on that axis.

The `value` for a percent length is a ratio, where `0.5f` means 50 percent.
All resolved lengths are clamped by the node's min/max size.

### Item participation

An item can specify:

- margins;
- flow or absolute positioning;
- Dock side;
- self alignment;
- grow and shrink weights;
- local absolute `x` and `y`;
- Track row, column, and spans;
- stable order;
- Flow breaks before or after the item.

Properties unsupported by the current parent formatting context have no
effect.

## Formatting contexts

### Stack

`XLAYOUT_FORMAT_STACK` arranges children on one axis. Set `axis` to horizontal
or vertical. Stack supports:

- grow and shrink weights;
- min/max sizes;
- margins and gaps;
- start, center, end, stretch, and baseline alignment;
- start, center, end, space-between, space-around, and space-evenly
  justification;
- reverse order;
- absolute children outside normal flow.

Use Stack for rows, columns, toolbars, forms, and ordinary application layout.

### Layer

`XLAYOUT_FORMAT_LAYER` overlays children in one content rectangle. Normal
children are aligned inside the slot; absolute children use their `x` and `y`
coordinates. Layer is suitable for canvases, overlays, badges, and free-form
placement.

### Track

`XLAYOUT_FORMAT_TRACK` arranges children in rows and columns. Install tracks
with `xLayoutNodeSetColumns` and `xLayoutNodeSetRows`.

Track kinds are:

- fixed: absolute size;
- auto: intrinsic child contribution;
- fraction: weighted share of remaining space;
- minmax: weighted share constrained by min and max;
- percent: ratio of available space after fixed tracks and gaps;
- cross: ratio of the container's opposite axis, constrained by min and max.

Cross tracks are useful for square icon, button, avatar, and media slots. A
cross column derives its width from the available row height; a cross row
derives its height from the available column width.

Track `shrink` is explicit. A zero shrink weight preserves the track size when
space is insufficient. Positive weights share the shortage while respecting
`min_size`. If a growing track reaches `max_size`, unused space is
redistributed to other eligible tracks.

Children select a row and column and may span multiple tracks.

### Flow

`XLAYOUT_FORMAT_FLOW` arranges inline items into lines and wraps at the inline
limit. It supports horizontal or vertical flow, item margins, line gaps,
baseline alignment, explicit breaks, absolute items, and multi-fragment
sources.

A multi-fragment source supplies two callbacks:

```c
uint32_t fragment_count(
    xlayout_context_t* context,
    xlayout_node_t node,
    float max_inline_size,
    void* user_data);

bool fragment_measure(
    xlayout_context_t* context,
    xlayout_node_t node,
    uint32_t fragment_index,
    float available_inline_size,
    void* user_data,
    xlayout_fragment_spec_t* output);
```

The measure callback may return a different fragment width for the current
remaining line width. If the fragment wraps, xLayout measures it again with the
fresh line width. The first fragment receives the item's leading margin, the
last receives its trailing margin, and each fragment receives cross-axis
margins.

After layout, query generated fragments with
`xLayoutNodeFragmentCount` and `xLayoutNodeGetFragment`. `tag` is copied from
the fragment specification and can identify a text run, glyph range, inline
object, or application record.

### Dock

`XLAYOUT_FORMAT_DOCK` consumes the current remaining rectangle in child order.
Children may dock left, top, right, bottom, or fill. Margins, gaps, intrinsic
measurement, and min/max size rules still apply.

Dock is a small formatting primitive for application shells. XUI DockPanel
uses Track nodes for its internal pane tree and region solver; window state,
tabs, floating panes, drag targets, and auto-hide behavior remain control
logic.

## Running layout

The complete operation is:

```c
xlayout_measure_t measured;
xlayout_constraints_t constraints = xLayoutConstraints(640.0f, 480.0f);
xlayout_rect_t viewport = { 0.0f, 0.0f, 640.0f, 480.0f };

if (!xLayoutRun(context, root, &constraints, viewport, &measured)) {
    /* Invalid input, callback failure, or allocation failure. */
}
```

`xLayoutMeasure` and `xLayoutArrange` are also exposed separately. Arrange
writes immutable query results. Read a node's assigned rectangle, content size,
baseline, clipping state, and fragment count with `xLayoutNodeGetResult`.

xLayout uses floating-point coordinates and does not snap to pixels. The
consumer owns pixel snapping and rendering policy.

## Invalidation and cache

Measurement is cached by exact constraints. A measure invalidation propagates
to ancestors because their intrinsic size may change. An arrange invalidation
keeps intrinsic measurement but recomputes geometry.

Use:

- `xLayoutNodeInvalidateMeasure` after callback-owned intrinsic data changes;
- `xLayoutNodeInvalidateArrange` after placement-only external state changes.

Public style and tree mutation functions perform their own invalidation.
Calling invalidation explicitly after those functions is unnecessary.

`xLayoutContextGetStats` reports live nodes, slot capacity, workspace capacity,
measurement calls, and measurement cache hits. These counters support tests and
performance diagnostics; they are not part of layout behavior.

## Minimal example

```c
xlayout_context_t* layout = xLayoutContextCreate(NULL);
xlayout_node_t root = xLayoutNodeCreate(layout, XLAYOUT_ROLE_CONTAINER);
xlayout_node_t left = xLayoutNodeCreate(layout, XLAYOUT_ROLE_LEAF);
xlayout_node_t right = xLayoutNodeCreate(layout, XLAYOUT_ROLE_LEAF);
xlayout_style_t root_style = xLayoutStyleDefault();
xlayout_style_t right_style = xLayoutStyleDefault();

root_style.container.format = XLAYOUT_FORMAT_STACK;
root_style.container.axis = XLAYOUT_HORIZONTAL;
root_style.container.column_gap = 8.0f;
xLayoutNodeSetStyle(layout, root, &root_style);

right_style.item.grow = 1.0f;
right_style.item.shrink = 1.0f;
xLayoutNodeSetStyle(layout, right, &right_style);

xLayoutNodeAppend(layout, root, left);
xLayoutNodeAppend(layout, root, right);

xLayoutRun(layout, root,
    &(xlayout_constraints_t){ 0, 640, 0, 480 },
    (xlayout_rect_t){ 0, 0, 640, 480 }, NULL);

xLayoutNodeDestroy(layout, root);
xLayoutContextDestroy(layout);
```

Production leaves should normally install measurement callbacks before the
first run.

## XUI integration boundary

Each XUI widget stores one xLayout handle. XUI maps widget layout properties to
xLayout style, adapts control content measurement to the measurement callback,
and copies arranged rectangles back to widget geometry with XUI pixel snapping.

Controls with dynamic layout structure may install an XUI layout-prepare
callback. SplitLayout uses it to declare pane and divider tracks before solving.
DockPanel uses virtual Track nodes for pane splits and its five outer regions.
Neither control owns a separate geometry solver.

Specialized XUI containers submit direct-child slots through xLayout's arrange-
children protocol. Post-layout callbacks may invalidate newly discovered
constraints; XUI preserves those invalidations and performs a bounded
stabilization loop instead of recursively invoking another control layout.

Calling `xuiWidgetMeasure` or `xuiWidgetArrange` on a subtree treats that widget
as the root of the requested operation. Parent participation rules are applied
only when the parent itself is being solved.

## Performance guidance

- Reuse one context for a related layout forest.
- Set realistic initial capacities for large stable documents.
- Reuse nodes instead of rebuilding the tree every frame.
- Invalidate only the smallest node whose intrinsic data changed.
- Keep measurement callbacks deterministic and allocation-free where possible.
- Use fragment callbacks for text runs instead of creating one node per glyph.

The included benchmark currently arranges 10,000 Flow children for 200 cached
passes and reports time, cache hits, and workspace capacity. Benchmark results
depend on compiler, CPU, and build flags and are not an API guarantee.
