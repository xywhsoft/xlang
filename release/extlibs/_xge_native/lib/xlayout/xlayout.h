#ifndef XLAYOUT_H
#define XLAYOUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef XLAYOUT_API
#define XLAYOUT_API
#endif

#define XLAYOUT_NODE_INVALID ((xlayout_node_t)0)
#define XLAYOUT_AUTO (-1.0f)
#define XLAYOUT_UNBOUNDED (3.402823466e+38F)

typedef uint64_t xlayout_node_t;
typedef struct xlayout_context xlayout_context_t;

typedef struct {
	float width;
	float height;
} xlayout_size_t;

typedef struct {
	float x;
	float y;
	float width;
	float height;
} xlayout_rect_t;

typedef struct {
	float left;
	float top;
	float right;
	float bottom;
} xlayout_edges_t;

typedef struct {
	float min_width;
	float max_width;
	float min_height;
	float max_height;
} xlayout_constraints_t;

typedef struct {
	float width;
	float height;
	float baseline;
} xlayout_measure_t;

typedef enum {
	XLAYOUT_ROLE_LEAF = 0,
	XLAYOUT_ROLE_CONTAINER = 1
} xlayout_role_t;

typedef enum {
	XLAYOUT_FORMAT_STACK = 0,
	XLAYOUT_FORMAT_LAYER = 1,
	XLAYOUT_FORMAT_TRACK = 2,
	XLAYOUT_FORMAT_FLOW = 3,
	XLAYOUT_FORMAT_DOCK = 4
} xlayout_format_t;

typedef enum {
	XLAYOUT_DOCK_FILL = 0,
	XLAYOUT_DOCK_LEFT = 1,
	XLAYOUT_DOCK_TOP = 2,
	XLAYOUT_DOCK_RIGHT = 3,
	XLAYOUT_DOCK_BOTTOM = 4
} xlayout_dock_t;

typedef enum {
	XLAYOUT_HORIZONTAL = 0,
	XLAYOUT_VERTICAL = 1
} xlayout_axis_t;

typedef enum {
	XLAYOUT_ALIGN_AUTO = 0,
	XLAYOUT_ALIGN_START = 1,
	XLAYOUT_ALIGN_CENTER = 2,
	XLAYOUT_ALIGN_END = 3,
	XLAYOUT_ALIGN_STRETCH = 4,
	XLAYOUT_ALIGN_BASELINE = 5
} xlayout_align_t;

typedef enum {
	XLAYOUT_JUSTIFY_START = 0,
	XLAYOUT_JUSTIFY_CENTER = 1,
	XLAYOUT_JUSTIFY_END = 2,
	XLAYOUT_JUSTIFY_SPACE_BETWEEN = 3,
	XLAYOUT_JUSTIFY_SPACE_AROUND = 4,
	XLAYOUT_JUSTIFY_SPACE_EVENLY = 5
} xlayout_justify_t;

typedef enum {
	XLAYOUT_LENGTH_AUTO = 0,
	XLAYOUT_LENGTH_FIXED = 1,
	XLAYOUT_LENGTH_PERCENT = 2,
	XLAYOUT_LENGTH_FILL = 3
} xlayout_length_kind_t;

typedef struct {
	xlayout_length_kind_t kind;
	float value;
} xlayout_length_t;

typedef enum {
	XLAYOUT_POSITION_FLOW = 0,
	XLAYOUT_POSITION_ABSOLUTE = 1
} xlayout_position_t;

typedef enum {
	XLAYOUT_OVERFLOW_VISIBLE = 0,
	XLAYOUT_OVERFLOW_CLIP = 1,
	XLAYOUT_OVERFLOW_SCROLL = 2
} xlayout_overflow_t;

typedef enum {
	XLAYOUT_TRACK_FIXED = 0,
	XLAYOUT_TRACK_AUTO = 1,
	XLAYOUT_TRACK_FRACTION = 2,
	XLAYOUT_TRACK_MINMAX = 3,
	XLAYOUT_TRACK_PERCENT = 4,
	XLAYOUT_TRACK_CROSS = 5
} xlayout_track_kind_t;

typedef struct {
	xlayout_track_kind_t kind;
	float value;
	float min_size;
	float max_size;
	float shrink;
} xlayout_track_t;

typedef enum {
	XLAYOUT_FRAGMENT_NONE = 0,
	XLAYOUT_FRAGMENT_BREAK_BEFORE = 1u << 0,
	XLAYOUT_FRAGMENT_BREAK_AFTER = 1u << 1
} xlayout_fragment_flags_t;

typedef struct {
	float width;
	float height;
	float baseline;
	uint32_t flags;
	uintptr_t tag;
} xlayout_fragment_spec_t;

typedef struct {
	xlayout_node_t source;
	uint32_t index;
	uintptr_t tag;
	xlayout_rect_t rect;
	float baseline;
	uint32_t flags;
} xlayout_fragment_t;

typedef struct {
	xlayout_length_t width;
	xlayout_length_t height;
	float min_width;
	float max_width;
	float min_height;
	float max_height;
} xlayout_size_style_t;

typedef struct {
	xlayout_edges_t margin;
	xlayout_position_t position;
	xlayout_dock_t dock;
	xlayout_align_t align_self;
	xlayout_align_t justify_self;
	float grow;
	float shrink;
	float x;
	float y;
	uint32_t column;
	uint32_t row;
	uint32_t column_span;
	uint32_t row_span;
	int32_t order;
	bool break_before;
	bool break_after;
} xlayout_item_style_t;

typedef struct {
	xlayout_format_t format;
	xlayout_axis_t axis;
	xlayout_align_t align_items;
	xlayout_align_t justify_items;
	xlayout_justify_t justify_content;
	xlayout_overflow_t overflow_x;
	xlayout_overflow_t overflow_y;
	xlayout_edges_t padding;
	float column_gap;
	float row_gap;
	bool reverse;
} xlayout_container_style_t;

typedef struct {
	xlayout_size_style_t size;
	xlayout_item_style_t item;
	xlayout_container_style_t container;
	bool visible;
} xlayout_style_t;

typedef struct {
	xlayout_rect_t rect;
	xlayout_size_t content_size;
	float baseline;
	uint32_t fragment_count;
	bool clipped;
} xlayout_result_t;

typedef struct {
	uint32_t initial_node_capacity;
	uint32_t initial_workspace_capacity;
} xlayout_context_config_t;

typedef struct {
	uint32_t live_nodes;
	uint32_t slot_capacity;
	uint32_t workspace_capacity;
	uint64_t measure_calls;
	uint64_t measure_cache_hits;
} xlayout_stats_t;

typedef bool (*xlayout_measure_fn)(
	xlayout_context_t* context,
	xlayout_node_t node,
	const xlayout_constraints_t* constraints,
	void* user_data,
	xlayout_measure_t* output);

typedef bool (*xlayout_arrange_children_fn)(
	xlayout_context_t* context,
	xlayout_node_t node,
	xlayout_rect_t content_rect,
	void* user_data);

typedef uint32_t (*xlayout_fragment_count_fn)(
	xlayout_context_t* context,
	xlayout_node_t node,
	float max_inline_size,
	void* user_data);

typedef bool (*xlayout_fragment_measure_fn)(
	xlayout_context_t* context,
	xlayout_node_t node,
	uint32_t fragment_index,
	float available_inline_size,
	void* user_data,
	xlayout_fragment_spec_t* output);

XLAYOUT_API xlayout_context_t* xLayoutContextCreate(const xlayout_context_config_t* config);
XLAYOUT_API void xLayoutContextDestroy(xlayout_context_t* context);
XLAYOUT_API void xLayoutContextGetStats(const xlayout_context_t* context, xlayout_stats_t* output);

XLAYOUT_API xlayout_style_t xLayoutStyleDefault(void);
XLAYOUT_API xlayout_constraints_t xLayoutConstraints(float max_width, float max_height);
XLAYOUT_API xlayout_constraints_t xLayoutConstraintsTight(float width, float height);
XLAYOUT_API xlayout_track_t xLayoutTrackFixed(float size);
XLAYOUT_API xlayout_track_t xLayoutTrackAuto(void);
XLAYOUT_API xlayout_track_t xLayoutTrackFraction(float weight);
XLAYOUT_API xlayout_track_t xLayoutTrackMinMax(float min_size, float max_size, float weight);
XLAYOUT_API xlayout_track_t xLayoutTrackPercent(float ratio);
XLAYOUT_API xlayout_track_t xLayoutTrackCross(float ratio, float min_size, float max_size);

XLAYOUT_API xlayout_node_t xLayoutNodeCreate(xlayout_context_t* context, xlayout_role_t role);
XLAYOUT_API void xLayoutNodeDestroy(xlayout_context_t* context, xlayout_node_t node);
XLAYOUT_API bool xLayoutNodeIsValid(const xlayout_context_t* context, xlayout_node_t node);
XLAYOUT_API bool xLayoutNodeAppend(xlayout_context_t* context, xlayout_node_t parent, xlayout_node_t child);
XLAYOUT_API bool xLayoutNodeInsertBefore(xlayout_context_t* context, xlayout_node_t parent, xlayout_node_t child, xlayout_node_t before);
XLAYOUT_API bool xLayoutNodeRemove(xlayout_context_t* context, xlayout_node_t child);
XLAYOUT_API xlayout_node_t xLayoutNodeParent(const xlayout_context_t* context, xlayout_node_t node);
XLAYOUT_API xlayout_node_t xLayoutNodeFirstChild(const xlayout_context_t* context, xlayout_node_t node);
XLAYOUT_API xlayout_node_t xLayoutNodeNextSibling(const xlayout_context_t* context, xlayout_node_t node);

XLAYOUT_API bool xLayoutNodeSetRole(xlayout_context_t* context, xlayout_node_t node, xlayout_role_t role);
XLAYOUT_API bool xLayoutNodeSetStyle(xlayout_context_t* context, xlayout_node_t node, const xlayout_style_t* style);
XLAYOUT_API bool xLayoutNodeGetStyle(const xlayout_context_t* context, xlayout_node_t node, xlayout_style_t* output);
XLAYOUT_API bool xLayoutNodeSetMeasure(xlayout_context_t* context, xlayout_node_t node, xlayout_measure_fn measure, void* user_data);
XLAYOUT_API bool xLayoutNodeSetArrangeChildren(xlayout_context_t* context, xlayout_node_t node, xlayout_arrange_children_fn arrange, void* user_data);
XLAYOUT_API bool xLayoutNodeSetFragmentSource(xlayout_context_t* context, xlayout_node_t node, xlayout_fragment_count_fn count, xlayout_fragment_measure_fn measure);
XLAYOUT_API bool xLayoutNodeSetUserData(xlayout_context_t* context, xlayout_node_t node, void* user_data);
XLAYOUT_API void* xLayoutNodeGetUserData(const xlayout_context_t* context, xlayout_node_t node);
XLAYOUT_API bool xLayoutNodeSetColumns(xlayout_context_t* context, xlayout_node_t node, const xlayout_track_t* tracks, uint32_t count);
XLAYOUT_API bool xLayoutNodeSetRows(xlayout_context_t* context, xlayout_node_t node, const xlayout_track_t* tracks, uint32_t count);

XLAYOUT_API void xLayoutNodeInvalidateMeasure(xlayout_context_t* context, xlayout_node_t node);
XLAYOUT_API void xLayoutNodeInvalidateArrange(xlayout_context_t* context, xlayout_node_t node);
XLAYOUT_API bool xLayoutMeasure(xlayout_context_t* context, xlayout_node_t node, const xlayout_constraints_t* constraints, xlayout_measure_t* output);
XLAYOUT_API bool xLayoutArrange(xlayout_context_t* context, xlayout_node_t node, xlayout_rect_t rect);
XLAYOUT_API bool xLayoutArrangeChild(xlayout_context_t* context, xlayout_node_t parent, xlayout_node_t child, xlayout_rect_t rect);
XLAYOUT_API bool xLayoutRun(xlayout_context_t* context, xlayout_node_t node, const xlayout_constraints_t* constraints, xlayout_rect_t rect, xlayout_measure_t* measured);
XLAYOUT_API bool xLayoutNodeGetResult(const xlayout_context_t* context, xlayout_node_t node, xlayout_result_t* output);
XLAYOUT_API uint32_t xLayoutNodeFragmentCount(const xlayout_context_t* context, xlayout_node_t node);
XLAYOUT_API bool xLayoutNodeGetFragment(const xlayout_context_t* context, xlayout_node_t node, uint32_t index, xlayout_fragment_t* output);

#ifdef __cplusplus
}
#endif

#endif
