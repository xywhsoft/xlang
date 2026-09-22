#include "xlayout.h"

#include "xrt.h"

#include <string.h>

#define XLAYOUT_DIRTY_MEASURE 0x01u
#define XLAYOUT_DIRTY_ARRANGE 0x02u
#define XLAYOUT_SLOT_NONE UINT32_MAX

typedef struct xlayout_node_internal xlayout_node_internal_t;

typedef struct {
	xlayout_node_internal_t* node;
	uint32_t generation;
	uint32_t next_free;
} xlayout_slot_t;

typedef struct {
	xlayout_node_internal_t* node;
	xlayout_measure_t measure;
	xlayout_rect_t rect;
	float main_size;
	float cross_size;
	float margin_main_before;
	float margin_main_after;
	float margin_cross_before;
	float margin_cross_after;
	float grow;
	float shrink;
	uint32_t fragment_index;
	xlayout_fragment_spec_t fragment;
	bool is_fragment;
} xlayout_work_item_t;

struct xlayout_node_internal {
	xlayout_context_t* context;
	xlayout_node_internal_t* parent;
	xlayout_node_internal_t* first_child;
	xlayout_node_internal_t* last_child;
	xlayout_node_internal_t* previous;
	xlayout_node_internal_t* next;
	uint32_t child_count;
	uint32_t slot;
	uint32_t generation;
	xlayout_role_t role;
	xlayout_style_t style;
	xlayout_track_t* columns;
	xlayout_track_t* rows;
	uint32_t column_count;
	uint32_t row_count;
	xlayout_measure_fn measure_callback;
	xlayout_arrange_children_fn arrange_children_callback;
	void* arrange_children_user_data;
	xlayout_fragment_count_fn fragment_count_callback;
	xlayout_fragment_measure_fn fragment_measure_callback;
	void* user_data;
	xlayout_constraints_t cached_constraints;
	xlayout_measure_t cached_measure;
	xlayout_measure_t intrinsic_measure;
	xlayout_result_t result;
	uint32_t dirty;
	uint32_t fragment_start;
	uint32_t fragment_epoch;
};

struct xlayout_context {
	xpool* node_pool;
	xlayout_slot_t* slots;
	uint32_t slot_count;
	uint32_t slot_capacity;
	uint32_t free_slot;
	uint32_t live_nodes;
	xlayout_work_item_t* items;
	uint32_t item_count;
	uint32_t item_capacity;
	float* scalars;
	uint32_t scalar_count;
	uint32_t scalar_capacity;
	xlayout_fragment_t* fragments;
	uint32_t fragment_count;
	uint32_t fragment_capacity;
	uint32_t arrange_epoch;
	xlayout_node_internal_t* active_arrange_parent;
	uint64_t measure_calls;
	uint64_t measure_cache_hits;
};

static float xl_min(float a, float b)
{
	return a < b ? a : b;
}

static float xl_max(float a, float b)
{
	return a > b ? a : b;
}

static float xl_clamp(float value, float low, float high)
{
	return xl_min(xl_max(value, low), high);
}

static bool xl_bounded(float value)
{
	return value < XLAYOUT_UNBOUNDED * 0.5f;
}

static float xl_nonnegative(float value)
{
	return value > 0.0f ? value : 0.0f;
}

static bool xl_float_equal(float a, float b)
{
	float delta = a - b;
	return delta < 0.0f ? -delta < 0.0001f : delta < 0.0001f;
}

static xlayout_constraints_t xl_normalize_constraints(xlayout_constraints_t value)
{
	value.min_width = xl_nonnegative(value.min_width);
	value.min_height = xl_nonnegative(value.min_height);
	value.max_width = value.max_width < value.min_width ? value.min_width : value.max_width;
	value.max_height = value.max_height < value.min_height ? value.min_height : value.max_height;
	return value;
}

static bool xl_constraints_equal(const xlayout_constraints_t* left, const xlayout_constraints_t* right)
{
	return xl_float_equal(left->min_width, right->min_width)
		&& xl_float_equal(left->max_width, right->max_width)
		&& xl_float_equal(left->min_height, right->min_height)
		&& xl_float_equal(left->max_height, right->max_height);
}

static xlayout_node_t xl_handle(const xlayout_node_internal_t* node)
{
	if ( node == NULL ) {
		return XLAYOUT_NODE_INVALID;
	}
	return ((uint64_t)node->generation << 32) | (uint64_t)(node->slot + 1u);
}

static xlayout_node_internal_t* xl_resolve(const xlayout_context_t* context, xlayout_node_t handle)
{
	uint32_t encoded_slot;
	uint32_t slot;
	uint32_t generation;
	if ( context == NULL || handle == XLAYOUT_NODE_INVALID ) {
		return NULL;
	}
	encoded_slot = (uint32_t)(handle & 0xffffffffu);
	if ( encoded_slot == 0 ) {
		return NULL;
	}
	slot = encoded_slot - 1u;
	generation = (uint32_t)(handle >> 32);
	if ( slot >= context->slot_count || context->slots[slot].node == NULL
		|| context->slots[slot].generation != generation ) {
		return NULL;
	}
	return context->slots[slot].node;
}

static bool xl_reserve_slots(xlayout_context_t* context, uint32_t required)
{
	xlayout_slot_t* slots;
	uint32_t capacity;
	if ( required <= context->slot_capacity ) {
		return true;
	}
	capacity = context->slot_capacity ? context->slot_capacity : 64u;
	while ( capacity < required ) {
		capacity = capacity < 0x40000000u ? capacity * 2u : required;
	}
	slots = (xlayout_slot_t*)xrtRealloc(context->slots, sizeof(*slots) * capacity);
	if ( slots == NULL ) {
		return false;
	}
	memset(&slots[context->slot_capacity], 0, sizeof(*slots) * (capacity - context->slot_capacity));
	context->slots = slots;
	context->slot_capacity = capacity;
	return true;
}

static bool xl_reserve_items(xlayout_context_t* context, uint32_t required)
{
	xlayout_work_item_t* items;
	uint32_t capacity;
	if ( required <= context->item_capacity ) {
		return true;
	}
	capacity = context->item_capacity ? context->item_capacity : 64u;
	while ( capacity < required ) {
		capacity = capacity < 0x40000000u ? capacity * 2u : required;
	}
	items = (xlayout_work_item_t*)xrtRealloc(context->items, sizeof(*items) * capacity);
	if ( items == NULL ) {
		return false;
	}
	context->items = items;
	context->item_capacity = capacity;
	return true;
}

static bool xl_reserve_scalars(xlayout_context_t* context, uint32_t required)
{
	float* scalars;
	uint32_t capacity;
	if ( required <= context->scalar_capacity ) {
		return true;
	}
	capacity = context->scalar_capacity ? context->scalar_capacity : 128u;
	while ( capacity < required ) {
		capacity = capacity < 0x40000000u ? capacity * 2u : required;
	}
	scalars = (float*)xrtRealloc(context->scalars, sizeof(*scalars) * capacity);
	if ( scalars == NULL ) {
		return false;
	}
	context->scalars = scalars;
	context->scalar_capacity = capacity;
	return true;
}

static bool xl_reserve_fragments(xlayout_context_t* context, uint32_t required)
{
	xlayout_fragment_t* fragments;
	uint32_t capacity;
	if ( required <= context->fragment_capacity ) {
		return true;
	}
	capacity = context->fragment_capacity ? context->fragment_capacity : 64u;
	while ( capacity < required ) {
		capacity = capacity < 0x40000000u ? capacity * 2u : required;
	}
	fragments = (xlayout_fragment_t*)xrtRealloc(context->fragments, sizeof(*fragments) * capacity);
	if ( fragments == NULL ) {
		return false;
	}
	context->fragments = fragments;
	context->fragment_capacity = capacity;
	return true;
}

static float xl_horizontal_edges(xlayout_edges_t edges)
{
	return edges.left + edges.right;
}

static float xl_vertical_edges(xlayout_edges_t edges)
{
	return edges.top + edges.bottom;
}

static float xl_main_size(xlayout_axis_t axis, float width, float height)
{
	return axis == XLAYOUT_HORIZONTAL ? width : height;
}

static float xl_cross_size(xlayout_axis_t axis, float width, float height)
{
	return axis == XLAYOUT_HORIZONTAL ? height : width;
}

static float xl_main_before(xlayout_axis_t axis, xlayout_edges_t edges)
{
	return axis == XLAYOUT_HORIZONTAL ? edges.left : edges.top;
}

static float xl_main_after(xlayout_axis_t axis, xlayout_edges_t edges)
{
	return axis == XLAYOUT_HORIZONTAL ? edges.right : edges.bottom;
}

static float xl_cross_before(xlayout_axis_t axis, xlayout_edges_t edges)
{
	return axis == XLAYOUT_HORIZONTAL ? edges.top : edges.left;
}

static float xl_cross_after(xlayout_axis_t axis, xlayout_edges_t edges)
{
	return axis == XLAYOUT_HORIZONTAL ? edges.bottom : edges.right;
}

static xlayout_rect_t xl_content_rect(const xlayout_node_internal_t* node, xlayout_rect_t rect)
{
	xlayout_edges_t padding = node->style.container.padding;
	rect.x += padding.left;
	rect.y += padding.top;
	rect.width = xl_nonnegative(rect.width - padding.left - padding.right);
	rect.height = xl_nonnegative(rect.height - padding.top - padding.bottom);
	return rect;
}

static float xl_resolve_length(xlayout_length_t length, float natural, float available)
{
	switch ( length.kind ) {
	case XLAYOUT_LENGTH_FIXED:
		return xl_nonnegative(length.value);
	case XLAYOUT_LENGTH_PERCENT:
		return xl_bounded(available) ? xl_nonnegative(available * length.value) : natural;
	case XLAYOUT_LENGTH_FILL:
		return xl_bounded(available) ? available : natural;
	case XLAYOUT_LENGTH_AUTO:
	default:
		return natural;
	}
}

static float xl_style_max(float value)
{
	return value < 0.0f ? XLAYOUT_UNBOUNDED : value;
}

static void xl_apply_node_size(
	const xlayout_node_internal_t* node,
	const xlayout_constraints_t* constraints,
	xlayout_measure_t* measure)
{
	float min_width = xl_max(constraints->min_width, xl_nonnegative(node->style.size.min_width));
	float min_height = xl_max(constraints->min_height, xl_nonnegative(node->style.size.min_height));
	float max_width = xl_min(constraints->max_width, xl_style_max(node->style.size.max_width));
	float max_height = xl_min(constraints->max_height, xl_style_max(node->style.size.max_height));
	measure->width = xl_resolve_length(node->style.size.width, measure->width, constraints->max_width);
	measure->height = xl_resolve_length(node->style.size.height, measure->height, constraints->max_height);
	measure->width = xl_clamp(measure->width, min_width, xl_max(min_width, max_width));
	measure->height = xl_clamp(measure->height, min_height, xl_max(min_height, max_height));
	if ( measure->baseline < 0.0f ) {
		measure->baseline = measure->height;
	}
	measure->baseline = xl_clamp(measure->baseline, 0.0f, measure->height);
}

static void xl_invalidate_measure_node(xlayout_node_internal_t* node)
{
	while ( node != NULL ) {
		node->dirty |= XLAYOUT_DIRTY_MEASURE | XLAYOUT_DIRTY_ARRANGE;
		node = node->parent;
	}
}

static void xl_invalidate_arrange_node(xlayout_node_internal_t* node)
{
	while ( node != NULL ) {
		node->dirty |= XLAYOUT_DIRTY_ARRANGE;
		node = node->parent;
	}
}

static bool xl_measure_node(
	xlayout_context_t* context,
	xlayout_node_internal_t* node,
	xlayout_constraints_t constraints,
	xlayout_measure_t* output);

static xlayout_align_t xl_item_alignment(
	const xlayout_node_internal_t* parent,
	const xlayout_node_internal_t* child);

static xlayout_constraints_t xl_child_constraints(
	const xlayout_node_internal_t* parent,
	xlayout_constraints_t constraints,
	const xlayout_node_internal_t* child)
{
	xlayout_edges_t padding = parent->style.container.padding;
	xlayout_edges_t margin = child->style.item.margin;
	float horizontal = xl_horizontal_edges(padding) + xl_horizontal_edges(margin);
	float vertical = xl_vertical_edges(padding) + xl_vertical_edges(margin);
	constraints.min_width = 0.0f;
	constraints.min_height = 0.0f;
	constraints.max_width = xl_bounded(constraints.max_width)
		? xl_nonnegative(constraints.max_width - horizontal) : XLAYOUT_UNBOUNDED;
	constraints.max_height = xl_bounded(constraints.max_height)
		? xl_nonnegative(constraints.max_height - vertical) : XLAYOUT_UNBOUNDED;
	return constraints;
}

static xlayout_constraints_t xl_content_constraints(
	const xlayout_node_internal_t* node,
	xlayout_constraints_t constraints)
{
	float horizontal = xl_horizontal_edges(node->style.container.padding);
	float vertical = xl_vertical_edges(node->style.container.padding);
	constraints.min_width = xl_nonnegative(constraints.min_width - horizontal);
	constraints.min_height = xl_nonnegative(constraints.min_height - vertical);
	constraints.max_width = xl_bounded(constraints.max_width)
		? xl_nonnegative(constraints.max_width - horizontal) : XLAYOUT_UNBOUNDED;
	constraints.max_height = xl_bounded(constraints.max_height)
		? xl_nonnegative(constraints.max_height - vertical) : XLAYOUT_UNBOUNDED;
	return xl_normalize_constraints(constraints);
}

static bool xl_measure_own_content(
	xlayout_context_t* context,
	xlayout_node_internal_t* node,
	xlayout_constraints_t constraints,
	xlayout_measure_t* output)
{
	xlayout_edges_t padding = node->style.container.padding;
	memset(output, 0, sizeof(*output));
	output->baseline = -1.0f;
	if ( node->measure_callback != NULL ) {
		constraints = xl_content_constraints(node, constraints);
		if ( !node->measure_callback(context, xl_handle(node), &constraints, node->user_data, output) ) return false;
	}
	output->width = xl_nonnegative(output->width) + xl_horizontal_edges(padding);
	output->height = xl_nonnegative(output->height) + xl_vertical_edges(padding);
	if ( output->baseline < 0.0f ) output->baseline = output->height;
	else output->baseline = xl_clamp(output->baseline + padding.top, 0.0f, output->height);
	return true;
}

static bool xl_measure_stack(
	xlayout_context_t* context,
	xlayout_node_internal_t* node,
	xlayout_constraints_t constraints,
	xlayout_measure_t* output)
{
	xlayout_axis_t axis = node->style.container.axis;
	xlayout_node_internal_t* child;
	float main = 0.0f;
	float cross = 0.0f;
	float gap = axis == XLAYOUT_HORIZONTAL ? node->style.container.column_gap : node->style.container.row_gap;
	float baseline_ascent = 0.0f;
	float baseline_descent = 0.0f;
	uint32_t count = 0;
	for ( child = node->first_child; child != NULL; child = child->next ) {
		xlayout_measure_t measured;
		xlayout_constraints_t child_constraints;
		if ( !child->style.visible || child->style.item.position == XLAYOUT_POSITION_ABSOLUTE ) {
			continue;
		}
		child_constraints = xl_child_constraints(node, constraints, child);
		if ( axis == XLAYOUT_HORIZONTAL ) {
			child_constraints.max_width = XLAYOUT_UNBOUNDED;
		} else {
			child_constraints.max_height = XLAYOUT_UNBOUNDED;
		}
		if ( !xl_measure_node(context, child, child_constraints, &measured) ) {
			return false;
		}
		main += xl_main_size(axis, measured.width, measured.height)
			+ xl_main_before(axis, child->style.item.margin)
			+ xl_main_after(axis, child->style.item.margin);
		cross = xl_max(cross, xl_cross_size(axis, measured.width, measured.height)
			+ xl_cross_before(axis, child->style.item.margin)
			+ xl_cross_after(axis, child->style.item.margin));
		if ( axis == XLAYOUT_HORIZONTAL && xl_item_alignment(node, child) == XLAYOUT_ALIGN_BASELINE ) {
			baseline_ascent = xl_max(baseline_ascent, measured.baseline + child->style.item.margin.top);
			baseline_descent = xl_max(baseline_descent, measured.height - measured.baseline + child->style.item.margin.bottom);
		}
		count++;
	}
	if ( axis == XLAYOUT_HORIZONTAL ) cross = xl_max(cross, baseline_ascent + baseline_descent);
	if ( count > 1 ) {
		main += gap * (float)(count - 1u);
	}
	if ( axis == XLAYOUT_HORIZONTAL ) {
		output->width = main + xl_horizontal_edges(node->style.container.padding);
		output->height = cross + xl_vertical_edges(node->style.container.padding);
	} else {
		output->width = cross + xl_horizontal_edges(node->style.container.padding);
		output->height = main + xl_vertical_edges(node->style.container.padding);
	}
	output->baseline = output->height;
	return true;
}

static bool xl_measure_layer(
	xlayout_context_t* context,
	xlayout_node_internal_t* node,
	xlayout_constraints_t constraints,
	xlayout_measure_t* output)
{
	xlayout_node_internal_t* child;
	float width = 0.0f;
	float height = 0.0f;
	for ( child = node->first_child; child != NULL; child = child->next ) {
		xlayout_measure_t measured;
		xlayout_constraints_t child_constraints;
		if ( !child->style.visible ) {
			continue;
		}
		child_constraints = xl_child_constraints(node, constraints, child);
		if ( !xl_measure_node(context, child, child_constraints, &measured) ) {
			return false;
		}
		width = xl_max(width, child->style.item.x + child->style.item.margin.left
			+ measured.width + child->style.item.margin.right);
		height = xl_max(height, child->style.item.y + child->style.item.margin.top
			+ measured.height + child->style.item.margin.bottom);
	}
	output->width = width + xl_horizontal_edges(node->style.container.padding);
	output->height = height + xl_vertical_edges(node->style.container.padding);
	output->baseline = output->height;
	return true;
}

static bool xl_measure_dock(
	xlayout_context_t* context,
	xlayout_node_internal_t* node,
	xlayout_constraints_t constraints,
	xlayout_measure_t* output)
{
	xlayout_node_internal_t* child;
	float width = 0.0f;
	float height = 0.0f;
	float gap = node->style.container.column_gap;
	uint32_t consumed = 0u;
	for ( child = node->first_child; child != NULL; child = child->next ) {
		xlayout_measure_t measured;
		xlayout_constraints_t child_constraints;
		float outer_width;
		float outer_height;
		if ( !child->style.visible || child->style.item.position == XLAYOUT_POSITION_ABSOLUTE ) continue;
		child_constraints = xl_child_constraints(node, constraints, child);
		if ( !xl_measure_node(context, child, child_constraints, &measured) ) return false;
		outer_width = measured.width + xl_horizontal_edges(child->style.item.margin);
		outer_height = measured.height + xl_vertical_edges(child->style.item.margin);
		if ( child->style.item.dock == XLAYOUT_DOCK_LEFT || child->style.item.dock == XLAYOUT_DOCK_RIGHT ) {
			width += (consumed ? gap : 0.0f) + outer_width;
			height = xl_max(height, outer_height);
			consumed++;
		} else if ( child->style.item.dock == XLAYOUT_DOCK_TOP || child->style.item.dock == XLAYOUT_DOCK_BOTTOM ) {
			height += (consumed ? gap : 0.0f) + outer_height;
			width = xl_max(width, outer_width);
			consumed++;
		} else {
			width = xl_max(width, outer_width);
			height = xl_max(height, outer_height);
		}
	}
	output->width = width + xl_horizontal_edges(node->style.container.padding);
	output->height = height + xl_vertical_edges(node->style.container.padding);
	output->baseline = output->height;
	return true;
}

static const xlayout_track_t* xl_track_at(const xlayout_node_internal_t* node, bool columns, uint32_t index)
{
	static const xlayout_track_t automatic = { XLAYOUT_TRACK_AUTO, 0.0f, 0.0f, XLAYOUT_UNBOUNDED, 0.0f };
	if ( columns ) {
		return index < node->column_count ? &node->columns[index] : &automatic;
	}
	return index < node->row_count ? &node->rows[index] : &automatic;
}

static float xl_track_limit(const xlayout_track_t* track, float value)
{
	float maximum = track->max_size < 0.0f ? XLAYOUT_UNBOUNDED : track->max_size;
	return xl_clamp(value, xl_nonnegative(track->min_size), maximum);
}

static void xl_grow_tracks(
	xlayout_node_internal_t* node,
	bool columns,
	float* sizes,
	uint32_t start,
	uint32_t count,
	float amount,
	int mode)
{
	while ( amount > 0.0001f ) {
		float active_weight = 0.0f;
		float consumed = 0.0f;
		uint32_t index;
		for ( index = 0; index < count; ++index ) {
			const xlayout_track_t* track = xl_track_at(node, columns, start + index);
			float maximum = track->max_size < 0.0f ? XLAYOUT_UNBOUNDED : track->max_size;
			bool eligible = mode == 0 ? track->kind != XLAYOUT_TRACK_FIXED
				: mode == 1 ? (track->kind == XLAYOUT_TRACK_FRACTION || track->kind == XLAYOUT_TRACK_MINMAX)
				: (track->kind == XLAYOUT_TRACK_AUTO || track->kind == XLAYOUT_TRACK_MINMAX);
			if ( eligible
				&& sizes[start + index] + 0.0001f < maximum ) {
				active_weight += mode == 1 && track->value > 0.0f ? track->value : 1.0f;
			}
		}
		if ( active_weight <= 0.0f ) break;
		for ( index = 0; index < count; ++index ) {
			const xlayout_track_t* track = xl_track_at(node, columns, start + index);
			float maximum = track->max_size < 0.0f ? XLAYOUT_UNBOUNDED : track->max_size;
			bool eligible = mode == 0 ? track->kind != XLAYOUT_TRACK_FIXED
				: mode == 1 ? (track->kind == XLAYOUT_TRACK_FRACTION || track->kind == XLAYOUT_TRACK_MINMAX)
				: (track->kind == XLAYOUT_TRACK_AUTO || track->kind == XLAYOUT_TRACK_MINMAX);
			float weight;
			float growth;
			if ( !eligible
				|| sizes[start + index] + 0.0001f >= maximum ) continue;
			weight = mode == 1 && track->value > 0.0f ? track->value : 1.0f;
			growth = amount * weight / active_weight;
			if ( growth > maximum - sizes[start + index] ) growth = maximum - sizes[start + index];
			if ( growth > 0.0f ) {
				sizes[start + index] += growth;
				consumed += growth;
			}
		}
		if ( consumed <= 0.0001f ) break;
		amount -= consumed;
	}
}

static float xl_shrink_tracks(
	xlayout_node_internal_t* node,
	bool columns,
	float* sizes,
	uint32_t count,
	float deficit)
{
	float total_consumed = 0.0f;
	while ( deficit > 0.0001f ) {
		float active_weight = 0.0f;
		float consumed = 0.0f;
		uint32_t index;
		for ( index = 0; index < count; ++index ) {
			const xlayout_track_t* track = xl_track_at(node, columns, index);
			float minimum = xl_nonnegative(track->min_size);
			if ( track->shrink > 0.0f && sizes[index] > minimum + 0.0001f ) {
				active_weight += track->shrink * sizes[index];
			}
		}
		if ( active_weight <= 0.0f ) break;
		for ( index = 0; index < count; ++index ) {
			const xlayout_track_t* track = xl_track_at(node, columns, index);
			float minimum = xl_nonnegative(track->min_size);
			float reduction;
			if ( track->shrink <= 0.0f || sizes[index] <= minimum + 0.0001f ) continue;
			reduction = deficit * track->shrink * sizes[index] / active_weight;
			if ( reduction > sizes[index] - minimum ) reduction = sizes[index] - minimum;
			if ( reduction > 0.0f ) {
				sizes[index] -= reduction;
				consumed += reduction;
			}
		}
		if ( consumed <= 0.0001f ) break;
		deficit -= consumed;
		total_consumed += consumed;
	}
	return total_consumed;
}

static void xl_compute_tracks(
	xlayout_node_internal_t* node,
	bool columns,
	float available,
	float cross_available,
	float gap,
	float* sizes,
	uint32_t count)
{
	xlayout_node_internal_t* child;
	float total = count > 1u ? gap * (float)(count - 1u) : 0.0f;
	float percent_basis = XLAYOUT_UNBOUNDED;
	uint32_t index;
	if ( xl_bounded(available) ) {
		percent_basis = xl_nonnegative(available - total);
		for ( index = 0; index < count; ++index ) {
			const xlayout_track_t* track = xl_track_at(node, columns, index);
			if ( track->kind == XLAYOUT_TRACK_FIXED ) {
				percent_basis = xl_nonnegative(percent_basis - xl_track_limit(track, track->value));
			} else if ( track->kind == XLAYOUT_TRACK_CROSS ) {
				float cross_size = xl_bounded(cross_available)
					? cross_available * xl_nonnegative(track->value) : track->min_size;
				percent_basis = xl_nonnegative(percent_basis - xl_track_limit(track, cross_size));
			}
		}
	}
	for ( index = 0; index < count; ++index ) {
		const xlayout_track_t* track = xl_track_at(node, columns, index);
		switch ( track->kind ) {
		case XLAYOUT_TRACK_FIXED:
			sizes[index] = xl_track_limit(track, track->value);
			break;
		case XLAYOUT_TRACK_MINMAX:
			sizes[index] = xl_nonnegative(track->min_size);
			break;
		case XLAYOUT_TRACK_FRACTION:
			sizes[index] = xl_nonnegative(track->min_size);
			break;
		case XLAYOUT_TRACK_PERCENT:
			sizes[index] = xl_bounded(percent_basis)
				? xl_track_limit(track, percent_basis * xl_clamp(track->value, 0.0f, 1.0f))
				: xl_nonnegative(track->min_size);
			break;
		case XLAYOUT_TRACK_CROSS:
			sizes[index] = xl_track_limit(track, xl_bounded(cross_available)
				? cross_available * xl_nonnegative(track->value) : track->min_size);
			break;
		case XLAYOUT_TRACK_AUTO:
		default:
			sizes[index] = xl_nonnegative(track->min_size);
			break;
		}
	}
	for ( child = node->first_child; child != NULL; child = child->next ) {
		uint32_t start;
		uint32_t span;
		float desired;
		float current = 0.0f;
		float deficit;
		if ( !child->style.visible || child->style.item.position == XLAYOUT_POSITION_ABSOLUTE ) {
			continue;
		}
		start = columns ? child->style.item.column : child->style.item.row;
		span = columns ? child->style.item.column_span : child->style.item.row_span;
		span = span == 0u ? 1u : span;
		if ( start >= count ) {
			continue;
		}
		if ( span > count - start ) {
			span = count - start;
		}
		desired = columns
			? child->cached_measure.width + xl_horizontal_edges(child->style.item.margin)
			: child->cached_measure.height + xl_vertical_edges(child->style.item.margin);
		for ( index = 0; index < span; ++index ) {
			current += sizes[start + index];
		}
		current += gap * (float)(span - 1u);
		deficit = desired - current;
		if ( deficit > 0.0f ) {
			xl_grow_tracks(node, columns, sizes, start, span, deficit, xl_bounded(available) ? 2 : 0);
		}
	}
	for ( index = 0; index < count; ++index ) {
		total += sizes[index];
	}
	if ( xl_bounded(available) && total > available ) {
		total -= xl_shrink_tracks(node, columns, sizes, count, total - available);
	}
	if ( xl_bounded(available) && available > total ) {
		xl_grow_tracks(node, columns, sizes, 0u, count, available - total, 1);
	}
}

static void xl_track_dimensions(const xlayout_node_internal_t* node, uint32_t* columns, uint32_t* rows)
{
	xlayout_node_internal_t* child;
	*columns = node->column_count;
	*rows = node->row_count;
	for ( child = node->first_child; child != NULL; child = child->next ) {
		uint32_t column_end = child->style.item.column + (child->style.item.column_span ? child->style.item.column_span : 1u);
		uint32_t row_end = child->style.item.row + (child->style.item.row_span ? child->style.item.row_span : 1u);
		*columns = *columns < column_end ? column_end : *columns;
		*rows = *rows < row_end ? row_end : *rows;
	}
	*columns = *columns ? *columns : 1u;
	*rows = *rows ? *rows : 1u;
}

static bool xl_measure_track(
	xlayout_context_t* context,
	xlayout_node_internal_t* node,
	xlayout_constraints_t constraints,
	xlayout_measure_t* output)
{
	xlayout_node_internal_t* child;
	uint32_t columns;
	uint32_t rows;
	uint32_t scalar_mark;
	float* column_sizes;
	float* row_sizes;
	float width = 0.0f;
	float height = 0.0f;
	uint32_t index;
	for ( child = node->first_child; child != NULL; child = child->next ) {
		xlayout_constraints_t child_constraints;
		if ( !child->style.visible || child->style.item.position == XLAYOUT_POSITION_ABSOLUTE ) {
			continue;
		}
		child_constraints = xl_child_constraints(node, constraints, child);
		if ( !xl_measure_node(context, child, child_constraints, &child->cached_measure) ) {
			return false;
		}
	}
	xl_track_dimensions(node, &columns, &rows);
	scalar_mark = context->scalar_count;
	if ( !xl_reserve_scalars(context, scalar_mark + columns + rows) ) {
		return false;
	}
	context->scalar_count = scalar_mark + columns + rows;
	column_sizes = &context->scalars[scalar_mark];
	row_sizes = &context->scalars[scalar_mark + columns];
	xl_compute_tracks(node, true, XLAYOUT_UNBOUNDED, constraints.max_height, node->style.container.column_gap, column_sizes, columns);
	xl_compute_tracks(node, false, XLAYOUT_UNBOUNDED, constraints.max_width, node->style.container.row_gap, row_sizes, rows);
	for ( index = 0; index < columns; ++index ) {
		width += column_sizes[index];
	}
	for ( index = 0; index < rows; ++index ) {
		height += row_sizes[index];
	}
	if ( columns > 1u ) {
		width += node->style.container.column_gap * (float)(columns - 1u);
	}
	if ( rows > 1u ) {
		height += node->style.container.row_gap * (float)(rows - 1u);
	}
	context->scalar_count = scalar_mark;
	output->width = width + xl_horizontal_edges(node->style.container.padding);
	output->height = height + xl_vertical_edges(node->style.container.padding);
	output->baseline = output->height;
	return true;
}

static bool xl_get_flow_fragment(
	xlayout_context_t* context,
	xlayout_node_internal_t* child,
	uint32_t index,
	float available,
	xlayout_fragment_spec_t* fragment)
{
	memset(fragment, 0, sizeof(*fragment));
	fragment->baseline = -1.0f;
	if ( child->fragment_measure_callback != NULL ) {
		if ( !child->fragment_measure_callback(context, xl_handle(child), index, available, child->user_data, fragment) ) {
			return false;
		}
		fragment->width = xl_nonnegative(fragment->width);
		fragment->height = xl_nonnegative(fragment->height);
		if ( fragment->baseline < 0.0f ) {
			fragment->baseline = fragment->height;
		}
		return true;
	}
	fragment->width = child->cached_measure.width;
	fragment->height = child->cached_measure.height;
	fragment->baseline = child->cached_measure.baseline;
	return true;
}

static bool xl_measure_flow(
	xlayout_context_t* context,
	xlayout_node_internal_t* node,
	xlayout_constraints_t constraints,
	xlayout_measure_t* output)
{
	xlayout_axis_t axis = node->style.container.axis;
	xlayout_node_internal_t* child;
	float inline_limit;
	float line_main = 0.0f;
	float line_cross = 0.0f;
	float maximum_main = 0.0f;
	float total_cross = 0.0f;
	float main_gap = axis == XLAYOUT_HORIZONTAL ? node->style.container.column_gap : node->style.container.row_gap;
	float line_gap = axis == XLAYOUT_HORIZONTAL ? node->style.container.row_gap : node->style.container.column_gap;
	bool line_has_items = false;
	inline_limit = axis == XLAYOUT_HORIZONTAL
		? constraints.max_width - xl_horizontal_edges(node->style.container.padding)
		: constraints.max_height - xl_vertical_edges(node->style.container.padding);
	inline_limit = xl_bounded(inline_limit) ? xl_nonnegative(inline_limit) : XLAYOUT_UNBOUNDED;
	for ( child = node->first_child; child != NULL; child = child->next ) {
		uint32_t fragment_count;
		uint32_t index;
		xlayout_constraints_t child_constraints;
		float child_main_before;
		float child_main_after;
		float child_cross_before;
		float child_cross_after;
		if ( !child->style.visible || child->style.item.position == XLAYOUT_POSITION_ABSOLUTE ) {
			continue;
		}
		child_constraints = xl_child_constraints(node, constraints, child);
		if ( !xl_measure_node(context, child, child_constraints, &child->cached_measure) ) {
			return false;
		}
		fragment_count = child->fragment_count_callback != NULL
			? child->fragment_count_callback(context, xl_handle(child), inline_limit, child->user_data) : 1u;
		child_main_before = xl_main_before(axis, child->style.item.margin);
		child_main_after = xl_main_after(axis, child->style.item.margin);
		child_cross_before = xl_cross_before(axis, child->style.item.margin);
		child_cross_after = xl_cross_after(axis, child->style.item.margin);
		for ( index = 0; index < fragment_count; ++index ) {
			xlayout_fragment_spec_t fragment;
			float margin_main_before = index == 0u ? child_main_before : 0.0f;
			float margin_main_after = index + 1u == fragment_count ? child_main_after : 0.0f;
			float remaining = xl_bounded(inline_limit) ? xl_nonnegative(inline_limit - line_main
				- (line_has_items ? main_gap : 0.0f) - margin_main_before - margin_main_after) : XLAYOUT_UNBOUNDED;
			float fragment_main;
			float fragment_cross;
			float outer_main;
			float outer_cross;
			if ( !xl_get_flow_fragment(context, child, index, remaining, &fragment) ) {
				return false;
			}
			if ( index == 0u && child->style.item.break_before ) fragment.flags |= XLAYOUT_FRAGMENT_BREAK_BEFORE;
			if ( index + 1u == fragment_count && child->style.item.break_after ) fragment.flags |= XLAYOUT_FRAGMENT_BREAK_AFTER;
			fragment_main = xl_main_size(axis, fragment.width, fragment.height);
			fragment_cross = xl_cross_size(axis, fragment.width, fragment.height);
			if ( (fragment.flags & XLAYOUT_FRAGMENT_BREAK_BEFORE) != 0 && line_has_items ) {
				maximum_main = xl_max(maximum_main, line_main);
				total_cross += (total_cross > 0.0f ? line_gap : 0.0f) + line_cross;
				line_main = line_cross = 0.0f;
				line_has_items = false;
				remaining = xl_bounded(inline_limit)
					? xl_nonnegative(inline_limit - margin_main_before - margin_main_after) : XLAYOUT_UNBOUNDED;
				if ( !xl_get_flow_fragment(context, child, index, remaining, &fragment) ) {
					return false;
				}
				if ( index == 0u && child->style.item.break_before ) fragment.flags |= XLAYOUT_FRAGMENT_BREAK_BEFORE;
				if ( index + 1u == fragment_count && child->style.item.break_after ) fragment.flags |= XLAYOUT_FRAGMENT_BREAK_AFTER;
				fragment_main = xl_main_size(axis, fragment.width, fragment.height);
				fragment_cross = xl_cross_size(axis, fragment.width, fragment.height);
			}
			outer_main = margin_main_before + fragment_main + margin_main_after;
			outer_cross = child_cross_before + fragment_cross + child_cross_after;
			if ( line_has_items && xl_bounded(inline_limit) && line_main + main_gap + outer_main > inline_limit ) {
				maximum_main = xl_max(maximum_main, line_main);
				total_cross += (total_cross > 0.0f ? line_gap : 0.0f) + line_cross;
				line_main = line_cross = 0.0f;
				line_has_items = false;
				remaining = xl_nonnegative(inline_limit - margin_main_before - margin_main_after);
				if ( !xl_get_flow_fragment(context, child, index, remaining, &fragment) ) {
					return false;
				}
				if ( index == 0u && child->style.item.break_before ) fragment.flags |= XLAYOUT_FRAGMENT_BREAK_BEFORE;
				if ( index + 1u == fragment_count && child->style.item.break_after ) fragment.flags |= XLAYOUT_FRAGMENT_BREAK_AFTER;
				fragment_main = xl_main_size(axis, fragment.width, fragment.height);
				fragment_cross = xl_cross_size(axis, fragment.width, fragment.height);
				outer_main = margin_main_before + fragment_main + margin_main_after;
				outer_cross = child_cross_before + fragment_cross + child_cross_after;
			}
			line_main += (line_has_items ? main_gap : 0.0f) + outer_main;
			line_cross = xl_max(line_cross, outer_cross);
			line_has_items = true;
			if ( (fragment.flags & XLAYOUT_FRAGMENT_BREAK_AFTER) != 0 ) {
				maximum_main = xl_max(maximum_main, line_main);
				total_cross += (total_cross > 0.0f ? line_gap : 0.0f) + line_cross;
				line_main = line_cross = 0.0f;
				line_has_items = false;
			}
		}
	}
	if ( line_has_items ) {
		maximum_main = xl_max(maximum_main, line_main);
		total_cross += (total_cross > 0.0f ? line_gap : 0.0f) + line_cross;
	}
	if ( axis == XLAYOUT_HORIZONTAL ) {
		output->width = maximum_main + xl_horizontal_edges(node->style.container.padding);
		output->height = total_cross + xl_vertical_edges(node->style.container.padding);
	} else {
		output->width = total_cross + xl_horizontal_edges(node->style.container.padding);
		output->height = maximum_main + xl_vertical_edges(node->style.container.padding);
	}
	output->baseline = output->height;
	return true;
}

static bool xl_measure_node(
	xlayout_context_t* context,
	xlayout_node_internal_t* node,
	xlayout_constraints_t constraints,
	xlayout_measure_t* output)
{
	xlayout_measure_t measured = { 0.0f, 0.0f, -1.0f };
	constraints = xl_normalize_constraints(constraints);
	context->measure_calls++;
	if ( (node->dirty & XLAYOUT_DIRTY_MEASURE) == 0
		&& xl_constraints_equal(&node->cached_constraints, &constraints) ) {
		context->measure_cache_hits++;
		*output = node->cached_measure;
		return true;
	}
	if ( !node->style.visible ) {
		measured.baseline = 0.0f;
	} else if ( node->role == XLAYOUT_ROLE_LEAF ) {
		if ( !xl_measure_own_content(context, node, constraints, &measured) ) return false;
	} else {
		xlayout_measure_t own;
		switch ( node->style.container.format ) {
		case XLAYOUT_FORMAT_LAYER:
			if ( !xl_measure_layer(context, node, constraints, &measured) ) return false;
			break;
		case XLAYOUT_FORMAT_TRACK:
			if ( !xl_measure_track(context, node, constraints, &measured) ) return false;
			break;
		case XLAYOUT_FORMAT_FLOW:
			if ( !xl_measure_flow(context, node, constraints, &measured) ) return false;
			break;
		case XLAYOUT_FORMAT_DOCK:
			if ( !xl_measure_dock(context, node, constraints, &measured) ) return false;
			break;
		case XLAYOUT_FORMAT_STACK:
		default:
			if ( !xl_measure_stack(context, node, constraints, &measured) ) return false;
			break;
		}
		if ( !xl_measure_own_content(context, node, constraints, &own) ) return false;
		measured.width = xl_max(measured.width, own.width);
		measured.height = xl_max(measured.height, own.height);
		if ( own.baseline >= 0.0f && own.height >= measured.height ) measured.baseline = own.baseline;
	}
	if ( node->role == XLAYOUT_ROLE_CONTAINER ) {
		xlayout_node_internal_t* child;
		for ( child = node->first_child; child != NULL; child = child->next ) {
			xlayout_measure_t absolute_measure;
			if ( child->style.visible && child->style.item.position == XLAYOUT_POSITION_ABSOLUTE ) {
				xlayout_constraints_t child_constraints = xl_child_constraints(node, constraints, child);
				if ( !xl_measure_node(context, child, child_constraints, &absolute_measure) ) return false;
			}
		}
	}
	node->intrinsic_measure = measured;
	xl_apply_node_size(node, &constraints, &measured);
	node->cached_constraints = constraints;
	node->cached_measure = measured;
	node->dirty &= ~XLAYOUT_DIRTY_MEASURE;
	*output = measured;
	return true;
}

static xlayout_align_t xl_item_alignment(const xlayout_node_internal_t* parent, const xlayout_node_internal_t* child)
{
	return child->style.item.align_self == XLAYOUT_ALIGN_AUTO
		? parent->style.container.align_items : child->style.item.align_self;
}

static xlayout_align_t xl_item_justification(const xlayout_node_internal_t* parent, const xlayout_node_internal_t* child)
{
	return child->style.item.justify_self == XLAYOUT_ALIGN_AUTO
		? parent->style.container.justify_items : child->style.item.justify_self;
}

static void xl_set_axis_rect(
	xlayout_axis_t axis,
	xlayout_rect_t content,
	float main_position,
	float cross_position,
	float main_size,
	float cross_size,
	xlayout_rect_t* output)
{
	if ( axis == XLAYOUT_HORIZONTAL ) {
		output->x = content.x + main_position;
		output->y = content.y + cross_position;
		output->width = main_size;
		output->height = cross_size;
	} else {
		output->x = content.x + cross_position;
		output->y = content.y + main_position;
		output->width = cross_size;
		output->height = main_size;
	}
}

static void xl_justify(
	xlayout_justify_t justify,
	float free_space,
	uint32_t count,
	float base_gap,
	float* start,
	float* gap)
{
	*start = 0.0f;
	*gap = base_gap;
	if ( free_space <= 0.0f ) {
		return;
	}
	switch ( justify ) {
	case XLAYOUT_JUSTIFY_CENTER:
		*start = free_space * 0.5f;
		break;
	case XLAYOUT_JUSTIFY_END:
		*start = free_space;
		break;
	case XLAYOUT_JUSTIFY_SPACE_BETWEEN:
		if ( count > 1u ) *gap += free_space / (float)(count - 1u);
		break;
	case XLAYOUT_JUSTIFY_SPACE_AROUND:
		if ( count > 0u ) {
			*gap += free_space / (float)count;
			*start = free_space / (float)count * 0.5f;
		}
		break;
	case XLAYOUT_JUSTIFY_SPACE_EVENLY:
		*gap += free_space / (float)(count + 1u);
		*start = free_space / (float)(count + 1u);
		break;
	case XLAYOUT_JUSTIFY_START:
	default:
		break;
	}
}

static bool xl_arrange_node(xlayout_context_t* context, xlayout_node_internal_t* node, xlayout_rect_t rect);

static bool xl_arrange_absolute_children(xlayout_context_t* context, xlayout_node_internal_t* node, xlayout_rect_t content)
{
	xlayout_node_internal_t* child;
	for ( child = node->first_child; child != NULL; child = child->next ) {
		xlayout_rect_t child_rect;
		xlayout_measure_t measure;
		if ( !child->style.visible || child->style.item.position != XLAYOUT_POSITION_ABSOLUTE ) {
			continue;
		}
		measure = child->cached_measure;
		child_rect.x = content.x + child->style.item.x + child->style.item.margin.left;
		child_rect.y = content.y + child->style.item.y + child->style.item.margin.top;
		child_rect.width = xl_resolve_length(child->style.size.width, measure.width, content.width);
		child_rect.height = xl_resolve_length(child->style.size.height, measure.height, content.height);
		child_rect.width = xl_nonnegative(child_rect.width);
		child_rect.height = xl_nonnegative(child_rect.height);
		if ( !xl_arrange_node(context, child, child_rect) ) {
			return false;
		}
	}
	return true;
}

static bool xl_arrange_stack(xlayout_context_t* context, xlayout_node_internal_t* node, xlayout_rect_t content)
{
	xlayout_axis_t axis = node->style.container.axis;
	xlayout_node_internal_t* child;
	uint32_t mark = context->item_count;
	uint32_t count = 0;
	uint32_t index;
	float available_main = xl_main_size(axis, content.width, content.height);
	float available_cross = xl_cross_size(axis, content.width, content.height);
	float base_gap = axis == XLAYOUT_HORIZONTAL ? node->style.container.column_gap : node->style.container.row_gap;
	float total = 0.0f;
	float total_grow = 0.0f;
	float total_shrink = 0.0f;
	float free_space;
	float position;
	float gap;
	float baseline = 0.0f;
	for ( child = node->first_child; child != NULL; child = child->next ) {
		if ( child->style.visible && child->style.item.position == XLAYOUT_POSITION_FLOW ) count++;
	}
	if ( !xl_reserve_items(context, mark + count) ) {
		return false;
	}
	context->item_count = mark + count;
	count = 0;
	for ( child = node->first_child; child != NULL; child = child->next ) {
		xlayout_work_item_t* item;
		if ( !child->style.visible || child->style.item.position != XLAYOUT_POSITION_FLOW ) continue;
		item = &context->items[mark + count++];
		memset(item, 0, sizeof(*item));
		item->node = child;
		item->measure = child->cached_measure;
		item->main_size = xl_main_size(axis, item->measure.width, item->measure.height);
		item->cross_size = xl_cross_size(axis, item->measure.width, item->measure.height);
		item->margin_main_before = xl_main_before(axis, child->style.item.margin);
		item->margin_main_after = xl_main_after(axis, child->style.item.margin);
		item->margin_cross_before = xl_cross_before(axis, child->style.item.margin);
		item->margin_cross_after = xl_cross_after(axis, child->style.item.margin);
		item->grow = xl_nonnegative(child->style.item.grow);
		item->shrink = xl_nonnegative(child->style.item.shrink);
		total += item->main_size + item->margin_main_before + item->margin_main_after;
		total_grow += item->grow;
		total_shrink += item->shrink * item->main_size;
	}
	/* Stable insertion sort keeps equal order values in tree order. */
	for ( index = 1; index < count; ++index ) {
		xlayout_work_item_t value = context->items[mark + index];
		uint32_t at = index;
		while ( at > 0u && context->items[mark + at - 1u].node->style.item.order > value.node->style.item.order ) {
			context->items[mark + at] = context->items[mark + at - 1u];
			at--;
		}
		context->items[mark + at] = value;
	}
	if ( count > 1u ) total += base_gap * (float)(count - 1u);
	if ( axis == XLAYOUT_HORIZONTAL ) {
		for ( index = 0; index < count; ++index ) {
			xlayout_work_item_t* item = &context->items[mark + index];
			if ( xl_item_alignment(node, item->node) == XLAYOUT_ALIGN_BASELINE ) {
				baseline = xl_max(baseline, item->margin_cross_before + item->measure.baseline);
			}
		}
	}
	free_space = available_main - total;
	if ( free_space > 0.0f && total_grow > 0.0f ) {
		for ( index = 0; index < count; ++index ) {
			xlayout_work_item_t* item = &context->items[mark + index];
			item->main_size += free_space * item->grow / total_grow;
		}
		total = available_main;
		free_space = 0.0f;
	} else if ( free_space < 0.0f && total_shrink > 0.0f ) {
		float deficit = -free_space;
		for ( index = 0; index < count; ++index ) {
			xlayout_work_item_t* item = &context->items[mark + index];
			float share = deficit * item->shrink * item->main_size / total_shrink;
			item->main_size = xl_nonnegative(item->main_size - share);
		}
		free_space = 0.0f;
	}
	xl_justify(node->style.container.justify_content, free_space, count, base_gap, &position, &gap);
	for ( index = 0; index < count; ++index ) {
		uint32_t source_index = node->style.container.reverse ? count - index - 1u : index;
		xlayout_work_item_t* item = &context->items[mark + source_index];
		xlayout_align_t align = xl_item_alignment(node, item->node);
		float cross_available = xl_nonnegative(available_cross - item->margin_cross_before - item->margin_cross_after);
		float cross_size = align == XLAYOUT_ALIGN_STRETCH ? cross_available : xl_min(item->cross_size, cross_available);
		float cross_position = item->margin_cross_before;
		if ( align == XLAYOUT_ALIGN_BASELINE && axis == XLAYOUT_HORIZONTAL ) {
			cross_position = baseline - item->measure.baseline;
		} else if ( align == XLAYOUT_ALIGN_CENTER ) {
			cross_position += (cross_available - cross_size) * 0.5f;
		} else if ( align == XLAYOUT_ALIGN_END ) {
			cross_position += cross_available - cross_size;
		}
		position += item->margin_main_before;
		xl_set_axis_rect(axis, content, position, cross_position, item->main_size, cross_size, &item->rect);
		position += item->main_size + item->margin_main_after + (index + 1u < count ? gap : 0.0f);
	}
	for ( index = 0; index < count; ++index ) {
		xlayout_work_item_t item = context->items[mark + index];
		if ( !xl_arrange_node(context, item.node, item.rect) ) {
			context->item_count = mark;
			return false;
		}
	}
	context->item_count = mark;
	return xl_arrange_absolute_children(context, node, content);
}

static bool xl_arrange_layer(xlayout_context_t* context, xlayout_node_internal_t* node, xlayout_rect_t content)
{
	xlayout_node_internal_t* child;
	for ( child = node->first_child; child != NULL; child = child->next ) {
		xlayout_measure_t measured;
		xlayout_rect_t rect;
		xlayout_align_t align;
		xlayout_align_t justify;
		float available_width;
		float available_height;
		if ( !child->style.visible ) continue;
		measured = child->cached_measure;
		available_width = xl_nonnegative(content.width - xl_horizontal_edges(child->style.item.margin) - child->style.item.x);
		available_height = xl_nonnegative(content.height - xl_vertical_edges(child->style.item.margin) - child->style.item.y);
		align = xl_item_alignment(node, child);
		justify = xl_item_justification(node, child);
		rect.width = justify == XLAYOUT_ALIGN_STRETCH ? available_width
			: xl_nonnegative(xl_resolve_length(child->style.size.width, measured.width, available_width));
		rect.height = align == XLAYOUT_ALIGN_STRETCH ? available_height
			: xl_nonnegative(xl_resolve_length(child->style.size.height, measured.height, available_height));
		rect.x = content.x + child->style.item.x + child->style.item.margin.left;
		rect.y = content.y + child->style.item.y + child->style.item.margin.top;
		if ( justify == XLAYOUT_ALIGN_CENTER ) {
			rect.x += (available_width - rect.width) * 0.5f;
		} else if ( justify == XLAYOUT_ALIGN_END ) {
			rect.x += available_width - rect.width;
		}
		if ( align == XLAYOUT_ALIGN_CENTER ) {
			rect.y += (available_height - rect.height) * 0.5f;
		} else if ( align == XLAYOUT_ALIGN_END ) {
			rect.y += available_height - rect.height;
		}
		if ( !xl_arrange_node(context, child, rect) ) return false;
	}
	return true;
}

static bool xl_arrange_dock(xlayout_context_t* context, xlayout_node_internal_t* node, xlayout_rect_t content)
{
	xlayout_node_internal_t* child;
	xlayout_rect_t remaining = content;
	float gap = node->style.container.column_gap;
	for ( child = node->first_child; child != NULL; child = child->next ) {
		xlayout_edges_t margin;
		xlayout_measure_t measured;
		xlayout_rect_t rect;
		float outer;
		if ( !child->style.visible || child->style.item.position == XLAYOUT_POSITION_ABSOLUTE ) continue;
		margin = child->style.item.margin;
		measured = child->cached_measure;
		if ( child->style.item.dock == XLAYOUT_DOCK_LEFT || child->style.item.dock == XLAYOUT_DOCK_RIGHT ) {
			outer = xl_min(remaining.width, measured.width + xl_horizontal_edges(margin));
			rect.x = child->style.item.dock == XLAYOUT_DOCK_LEFT
				? remaining.x + margin.left : remaining.x + remaining.width - outer + margin.left;
			rect.y = remaining.y + margin.top;
			rect.width = xl_nonnegative(outer - xl_horizontal_edges(margin));
			rect.height = xl_nonnegative(remaining.height - xl_vertical_edges(margin));
			if ( child->style.item.dock == XLAYOUT_DOCK_LEFT ) remaining.x += outer + gap;
			remaining.width = xl_nonnegative(remaining.width - outer - gap);
		} else if ( child->style.item.dock == XLAYOUT_DOCK_TOP || child->style.item.dock == XLAYOUT_DOCK_BOTTOM ) {
			outer = xl_min(remaining.height, measured.height + xl_vertical_edges(margin));
			rect.x = remaining.x + margin.left;
			rect.y = child->style.item.dock == XLAYOUT_DOCK_TOP
				? remaining.y + margin.top : remaining.y + remaining.height - outer + margin.top;
			rect.width = xl_nonnegative(remaining.width - xl_horizontal_edges(margin));
			rect.height = xl_nonnegative(outer - xl_vertical_edges(margin));
			if ( child->style.item.dock == XLAYOUT_DOCK_TOP ) remaining.y += outer + gap;
			remaining.height = xl_nonnegative(remaining.height - outer - gap);
		} else {
			rect.x = remaining.x + margin.left;
			rect.y = remaining.y + margin.top;
			rect.width = xl_nonnegative(remaining.width - xl_horizontal_edges(margin));
			rect.height = xl_nonnegative(remaining.height - xl_vertical_edges(margin));
		}
		if ( !xl_arrange_node(context, child, rect) ) return false;
	}
	return xl_arrange_absolute_children(context, node, content);
}

static bool xl_arrange_track(xlayout_context_t* context, xlayout_node_internal_t* node, xlayout_rect_t content)
{
	uint32_t columns;
	uint32_t rows;
	uint32_t scalar_mark = context->scalar_count;
	uint32_t item_mark = context->item_count;
	uint32_t item_count = 0;
	float* column_sizes;
	float* row_sizes;
	float* column_offsets;
	float* row_offsets;
	xlayout_node_internal_t* child;
	uint32_t index;
	xl_track_dimensions(node, &columns, &rows);
	if ( !xl_reserve_scalars(context, scalar_mark + (columns + rows) * 2u) ) return false;
	context->scalar_count = scalar_mark + (columns + rows) * 2u;
	column_sizes = &context->scalars[scalar_mark];
	row_sizes = &context->scalars[scalar_mark + columns];
	column_offsets = &context->scalars[scalar_mark + columns + rows];
	row_offsets = &context->scalars[scalar_mark + columns * 2u + rows];
	xl_compute_tracks(node, true, content.width, content.height, node->style.container.column_gap, column_sizes, columns);
	xl_compute_tracks(node, false, content.height, content.width, node->style.container.row_gap, row_sizes, rows);
	column_offsets[0] = 0.0f;
	for ( index = 1; index < columns; ++index ) {
		column_offsets[index] = column_offsets[index - 1u] + column_sizes[index - 1u] + node->style.container.column_gap;
	}
	row_offsets[0] = 0.0f;
	for ( index = 1; index < rows; ++index ) {
		row_offsets[index] = row_offsets[index - 1u] + row_sizes[index - 1u] + node->style.container.row_gap;
	}
	for ( child = node->first_child; child != NULL; child = child->next ) {
		if ( child->style.visible && child->style.item.position == XLAYOUT_POSITION_FLOW ) item_count++;
	}
	if ( !xl_reserve_items(context, item_mark + item_count) ) {
		context->scalar_count = scalar_mark;
		return false;
	}
	context->item_count = item_mark + item_count;
	item_count = 0;
	for ( child = node->first_child; child != NULL; child = child->next ) {
		uint32_t column;
		uint32_t row;
		uint32_t column_span;
		uint32_t row_span;
		float cell_width = 0.0f;
		float cell_height = 0.0f;
		float available_width;
		float available_height;
		xlayout_align_t align;
		xlayout_align_t justify;
		xlayout_work_item_t* item;
		if ( !child->style.visible || child->style.item.position != XLAYOUT_POSITION_FLOW ) continue;
		column = child->style.item.column < columns ? child->style.item.column : columns - 1u;
		row = child->style.item.row < rows ? child->style.item.row : rows - 1u;
		column_span = child->style.item.column_span ? child->style.item.column_span : 1u;
		row_span = child->style.item.row_span ? child->style.item.row_span : 1u;
		if ( column_span > columns - column ) column_span = columns - column;
		if ( row_span > rows - row ) row_span = rows - row;
		for ( index = 0; index < column_span; ++index ) cell_width += column_sizes[column + index];
		for ( index = 0; index < row_span; ++index ) cell_height += row_sizes[row + index];
		cell_width += node->style.container.column_gap * (float)(column_span - 1u);
		cell_height += node->style.container.row_gap * (float)(row_span - 1u);
		available_width = xl_nonnegative(cell_width - xl_horizontal_edges(child->style.item.margin));
		available_height = xl_nonnegative(cell_height - xl_vertical_edges(child->style.item.margin));
		align = xl_item_alignment(node, child);
		justify = xl_item_justification(node, child);
		item = &context->items[item_mark + item_count++];
		memset(item, 0, sizeof(*item));
		item->node = child;
		item->rect.x = content.x + column_offsets[column] + child->style.item.margin.left;
		item->rect.y = content.y + row_offsets[row] + child->style.item.margin.top;
		item->rect.width = justify == XLAYOUT_ALIGN_STRETCH ? available_width : xl_min(child->cached_measure.width, available_width);
		item->rect.height = align == XLAYOUT_ALIGN_STRETCH ? available_height : xl_min(child->cached_measure.height, available_height);
		if ( justify == XLAYOUT_ALIGN_CENTER ) {
			item->rect.x += (available_width - item->rect.width) * 0.5f;
		} else if ( justify == XLAYOUT_ALIGN_END ) {
			item->rect.x += available_width - item->rect.width;
		}
		if ( align == XLAYOUT_ALIGN_CENTER ) {
			item->rect.y += (available_height - item->rect.height) * 0.5f;
		} else if ( align == XLAYOUT_ALIGN_END ) {
			item->rect.y += available_height - item->rect.height;
		}
	}
	context->scalar_count = scalar_mark;
	for ( index = 0; index < item_count; ++index ) {
		xlayout_work_item_t item = context->items[item_mark + index];
		if ( !xl_arrange_node(context, item.node, item.rect) ) {
			context->item_count = item_mark;
			return false;
		}
	}
	context->item_count = item_mark;
	return xl_arrange_absolute_children(context, node, content);
}

static bool xl_append_fragment_result(
	xlayout_context_t* context,
	xlayout_node_internal_t* source,
	uint32_t index,
	const xlayout_fragment_spec_t* spec,
	xlayout_rect_t rect)
{
	xlayout_fragment_t* result;
	if ( !xl_reserve_fragments(context, context->fragment_count + 1u) ) return false;
	if ( source->fragment_epoch != context->arrange_epoch ) {
		source->fragment_epoch = context->arrange_epoch;
		source->fragment_start = context->fragment_count;
		source->result.fragment_count = 0;
		source->result.rect = rect;
	} else {
		float right = xl_max(source->result.rect.x + source->result.rect.width, rect.x + rect.width);
		float bottom = xl_max(source->result.rect.y + source->result.rect.height, rect.y + rect.height);
		source->result.rect.x = xl_min(source->result.rect.x, rect.x);
		source->result.rect.y = xl_min(source->result.rect.y, rect.y);
		source->result.rect.width = right - source->result.rect.x;
		source->result.rect.height = bottom - source->result.rect.y;
	}
	result = &context->fragments[context->fragment_count++];
	result->source = xl_handle(source);
	result->index = index;
	result->tag = spec->tag;
	result->rect = rect;
	result->baseline = spec->baseline;
	result->flags = spec->flags;
	source->result.fragment_count++;
	return true;
}

static bool xl_finish_flow_line(
	xlayout_context_t* context,
	xlayout_node_internal_t* node,
	xlayout_rect_t content,
	xlayout_axis_t axis,
	uint32_t start,
	uint32_t count,
	float line_position,
	float line_main,
	float line_cross)
{
	float available_main = xl_main_size(axis, content.width, content.height);
	float free_space = available_main - line_main;
	float position;
	float gap;
	uint32_t index;
	float base_gap = axis == XLAYOUT_HORIZONTAL ? node->style.container.column_gap : node->style.container.row_gap;
	float baseline = 0.0f;
	for ( index = 0; index < count; ++index ) {
		xlayout_work_item_t* item = &context->items[start + index];
		if ( xl_item_alignment(node, item->node) == XLAYOUT_ALIGN_BASELINE ) {
			float item_baseline = item->is_fragment ? item->fragment.baseline : item->measure.baseline;
			baseline = xl_max(baseline, item->margin_cross_before + item_baseline);
		}
	}
	xl_justify(node->style.container.justify_content, free_space, count, base_gap, &position, &gap);
	for ( index = 0; index < count; ++index ) {
		xlayout_work_item_t* item = &context->items[start + index];
		xlayout_align_t align = xl_item_alignment(node, item->node);
		float cross_size = item->cross_size;
		float outer_cross = item->margin_cross_before + cross_size + item->margin_cross_after;
		float cross_position = line_position + item->margin_cross_before;
		if ( align == XLAYOUT_ALIGN_STRETCH ) {
			cross_size = xl_nonnegative(line_cross - item->margin_cross_before - item->margin_cross_after);
		} else if ( align == XLAYOUT_ALIGN_BASELINE ) {
			float item_baseline = item->is_fragment ? item->fragment.baseline : item->measure.baseline;
			cross_position = line_position + baseline - item_baseline;
		} else if ( align == XLAYOUT_ALIGN_CENTER ) {
			cross_position += (line_cross - outer_cross) * 0.5f;
		} else if ( align == XLAYOUT_ALIGN_END ) {
			cross_position += line_cross - outer_cross;
		}
		xl_set_axis_rect(axis, content, position + item->margin_main_before, cross_position,
			item->main_size, cross_size, &item->rect);
		position += item->margin_main_before + item->main_size + item->margin_main_after
			+ (index + 1u < count ? gap : 0.0f);
	}
	for ( index = 0; index < count; ++index ) {
		xlayout_work_item_t item = context->items[start + index];
		if ( item.is_fragment ) {
			if ( !xl_append_fragment_result(context, item.node, item.fragment_index, &item.fragment, item.rect) ) return false;
		} else if ( !xl_arrange_node(context, item.node, item.rect) ) {
			return false;
		}
	}
	return true;
}

static bool xl_arrange_flow(xlayout_context_t* context, xlayout_node_internal_t* node, xlayout_rect_t content)
{
	xlayout_axis_t axis = node->style.container.axis;
	xlayout_node_internal_t* child;
	uint32_t mark = context->item_count;
	uint32_t line_count = 0;
	float inline_limit = xl_main_size(axis, content.width, content.height);
	float line_main = 0.0f;
	float line_cross = 0.0f;
	float line_position = 0.0f;
	float main_gap = axis == XLAYOUT_HORIZONTAL ? node->style.container.column_gap : node->style.container.row_gap;
	float line_gap = axis == XLAYOUT_HORIZONTAL ? node->style.container.row_gap : node->style.container.column_gap;
	for ( child = node->first_child; child != NULL; child = child->next ) {
		uint32_t fragment_count;
		uint32_t index;
		float child_main_before;
		float child_main_after;
		float child_cross_before;
		float child_cross_after;
		if ( !child->style.visible || child->style.item.position == XLAYOUT_POSITION_ABSOLUTE ) continue;
		fragment_count = child->fragment_count_callback != NULL
			? child->fragment_count_callback(context, xl_handle(child), inline_limit, child->user_data) : 1u;
		child_main_before = xl_main_before(axis, child->style.item.margin);
		child_main_after = xl_main_after(axis, child->style.item.margin);
		child_cross_before = xl_cross_before(axis, child->style.item.margin);
		child_cross_after = xl_cross_after(axis, child->style.item.margin);
		for ( index = 0; index < fragment_count; ++index ) {
			xlayout_fragment_spec_t fragment;
			float margin_main_before = index == 0u ? child_main_before : 0.0f;
			float margin_main_after = index + 1u == fragment_count ? child_main_after : 0.0f;
			float remaining = xl_nonnegative(inline_limit - line_main - (line_count ? main_gap : 0.0f)
				- margin_main_before - margin_main_after);
			float fragment_main;
			float fragment_cross;
			float outer_main;
			float outer_cross;
			xlayout_work_item_t* item;
			if ( !xl_get_flow_fragment(context, child, index, remaining, &fragment) ) goto fail;
			if ( index == 0u && child->style.item.break_before ) fragment.flags |= XLAYOUT_FRAGMENT_BREAK_BEFORE;
			if ( index + 1u == fragment_count && child->style.item.break_after ) fragment.flags |= XLAYOUT_FRAGMENT_BREAK_AFTER;
			fragment_main = xl_main_size(axis, fragment.width, fragment.height);
			fragment_cross = xl_cross_size(axis, fragment.width, fragment.height);
			outer_main = margin_main_before + fragment_main + margin_main_after;
			outer_cross = child_cross_before + fragment_cross + child_cross_after;
			if ( ((fragment.flags & XLAYOUT_FRAGMENT_BREAK_BEFORE) != 0 && line_count)
				|| (line_count && line_main + main_gap + outer_main > inline_limit) ) {
				if ( !xl_finish_flow_line(context, node, content, axis, mark, line_count, line_position, line_main, line_cross) ) goto fail;
				line_position += line_cross + line_gap;
				context->item_count = mark;
				line_count = 0;
				line_main = line_cross = 0.0f;
				remaining = xl_nonnegative(inline_limit - margin_main_before - margin_main_after);
				if ( !xl_get_flow_fragment(context, child, index, remaining, &fragment) ) goto fail;
				if ( index == 0u && child->style.item.break_before ) fragment.flags |= XLAYOUT_FRAGMENT_BREAK_BEFORE;
				if ( index + 1u == fragment_count && child->style.item.break_after ) fragment.flags |= XLAYOUT_FRAGMENT_BREAK_AFTER;
				fragment_main = xl_main_size(axis, fragment.width, fragment.height);
				fragment_cross = xl_cross_size(axis, fragment.width, fragment.height);
				outer_main = margin_main_before + fragment_main + margin_main_after;
				outer_cross = child_cross_before + fragment_cross + child_cross_after;
			}
			if ( !xl_reserve_items(context, mark + line_count + 1u) ) goto fail;
			context->item_count = mark + line_count + 1u;
			item = &context->items[mark + line_count++];
			memset(item, 0, sizeof(*item));
			item->node = child;
			item->measure = child->cached_measure;
			item->main_size = fragment_main;
			item->cross_size = fragment_cross;
			item->margin_main_before = margin_main_before;
			item->margin_main_after = margin_main_after;
			item->margin_cross_before = child_cross_before;
			item->margin_cross_after = child_cross_after;
			item->is_fragment = child->fragment_measure_callback != NULL;
			item->fragment_index = index;
			item->fragment = fragment;
			line_main += (line_count > 1u ? main_gap : 0.0f) + outer_main;
			line_cross = xl_max(line_cross, outer_cross);
			if ( (fragment.flags & XLAYOUT_FRAGMENT_BREAK_AFTER) != 0 ) {
				if ( !xl_finish_flow_line(context, node, content, axis, mark, line_count, line_position, line_main, line_cross) ) goto fail;
				line_position += line_cross + line_gap;
				context->item_count = mark;
				line_count = 0;
				line_main = line_cross = 0.0f;
			}
		}
	}
	if ( line_count && !xl_finish_flow_line(context, node, content, axis, mark, line_count, line_position, line_main, line_cross) ) goto fail;
	context->item_count = mark;
	return xl_arrange_absolute_children(context, node, content);
fail:
	context->item_count = mark;
	return false;
}

static bool xl_arrange_node(xlayout_context_t* context, xlayout_node_internal_t* node, xlayout_rect_t rect)
{
	xlayout_rect_t content;
	if ( !node->style.visible ) {
		memset(&node->result, 0, sizeof(node->result));
		node->dirty &= ~XLAYOUT_DIRTY_ARRANGE;
		return true;
	}
	rect.width = xl_nonnegative(rect.width);
	rect.height = xl_nonnegative(rect.height);
	node->result.rect = rect;
	node->result.baseline = node->cached_measure.baseline;
	node->result.fragment_count = 0;
	content = xl_content_rect(node, rect);
	node->result.content_size.width = xl_max(content.width,
		xl_nonnegative(node->intrinsic_measure.width - xl_horizontal_edges(node->style.container.padding)));
	node->result.content_size.height = xl_max(content.height,
		xl_nonnegative(node->intrinsic_measure.height - xl_vertical_edges(node->style.container.padding)));
	node->result.clipped = (node->style.container.overflow_x != XLAYOUT_OVERFLOW_VISIBLE
		&& node->result.content_size.width > content.width)
		|| (node->style.container.overflow_y != XLAYOUT_OVERFLOW_VISIBLE
		&& node->result.content_size.height > content.height);
	if ( node->role == XLAYOUT_ROLE_CONTAINER ) {
		bool arranged;
		if ( node->arrange_children_callback != NULL ) {
			xlayout_node_internal_t* previous = context->active_arrange_parent;
			context->active_arrange_parent = node;
			arranged = node->arrange_children_callback(context, xl_handle(node), content,
				node->arrange_children_user_data);
			context->active_arrange_parent = previous;
		} else switch ( node->style.container.format ) {
		case XLAYOUT_FORMAT_LAYER:
			arranged = xl_arrange_layer(context, node, content);
			break;
		case XLAYOUT_FORMAT_TRACK:
			arranged = xl_arrange_track(context, node, content);
			break;
		case XLAYOUT_FORMAT_FLOW:
			arranged = xl_arrange_flow(context, node, content);
			break;
		case XLAYOUT_FORMAT_DOCK:
			arranged = xl_arrange_dock(context, node, content);
			break;
		case XLAYOUT_FORMAT_STACK:
		default:
			arranged = xl_arrange_stack(context, node, content);
			break;
		}
		if ( !arranged ) return false;
	}
	node->dirty &= ~XLAYOUT_DIRTY_ARRANGE;
	return true;
}

static void xl_detach(xlayout_node_internal_t* node)
{
	xlayout_node_internal_t* parent = node->parent;
	if ( parent == NULL ) return;
	if ( node->previous != NULL ) node->previous->next = node->next;
	else parent->first_child = node->next;
	if ( node->next != NULL ) node->next->previous = node->previous;
	else parent->last_child = node->previous;
	parent->child_count--;
	node->parent = NULL;
	node->previous = NULL;
	node->next = NULL;
	xl_invalidate_measure_node(parent);
}

static void xl_destroy_node(xlayout_context_t* context, xlayout_node_internal_t* node)
{
	uint32_t slot;
	while ( node->first_child != NULL ) {
		xl_destroy_node(context, node->first_child);
	}
	xl_detach(node);
	if ( node->columns != NULL ) xrtFree(node->columns);
	if ( node->rows != NULL ) xrtFree(node->rows);
	slot = node->slot;
	context->slots[slot].node = NULL;
	context->slots[slot].generation++;
	if ( context->slots[slot].generation == 0u ) context->slots[slot].generation = 1u;
	context->slots[slot].next_free = context->free_slot;
	context->free_slot = slot;
	context->live_nodes--;
	(void)xrtPoolFree(context->node_pool, node);
}

xlayout_context_t* xLayoutContextCreate(const xlayout_context_config_t* config)
{
	xlayout_context_t* context = (xlayout_context_t*)xrtMalloc(sizeof(*context));
	uint32_t nodes = config != NULL && config->initial_node_capacity ? config->initial_node_capacity : 64u;
	uint32_t workspace = config != NULL && config->initial_workspace_capacity ? config->initial_workspace_capacity : 128u;
	if ( context == NULL ) return NULL;
	memset(context, 0, sizeof(*context));
	context->free_slot = XLAYOUT_SLOT_NONE;
	context->arrange_epoch = 1u;
	context->node_pool = xrtPoolCreate(sizeof(xlayout_node_internal_t));
	if ( context->node_pool == NULL || !xl_reserve_slots(context, nodes)
		|| !xl_reserve_items(context, workspace) || !xl_reserve_scalars(context, workspace) ) {
		xLayoutContextDestroy(context);
		return NULL;
	}
	return context;
}

void xLayoutContextDestroy(xlayout_context_t* context)
{
	uint32_t index;
	if ( context == NULL ) return;
	for ( index = 0; index < context->slot_count; ++index ) {
		xlayout_node_internal_t* node = context->slots != NULL ? context->slots[index].node : NULL;
		if ( node != NULL ) {
			if ( node->columns != NULL ) xrtFree(node->columns);
			if ( node->rows != NULL ) xrtFree(node->rows);
		}
	}
	if ( context->node_pool != NULL ) xrtPoolDestroy(context->node_pool);
	if ( context->slots != NULL ) xrtFree(context->slots);
	if ( context->items != NULL ) xrtFree(context->items);
	if ( context->scalars != NULL ) xrtFree(context->scalars);
	if ( context->fragments != NULL ) xrtFree(context->fragments);
	xrtFree(context);
}

void xLayoutContextGetStats(const xlayout_context_t* context, xlayout_stats_t* output)
{
	if ( output == NULL ) return;
	memset(output, 0, sizeof(*output));
	if ( context == NULL ) return;
	output->live_nodes = context->live_nodes;
	output->slot_capacity = context->slot_capacity;
	output->workspace_capacity = context->item_capacity;
	output->measure_calls = context->measure_calls;
	output->measure_cache_hits = context->measure_cache_hits;
}

xlayout_style_t xLayoutStyleDefault(void)
{
	xlayout_style_t style;
	memset(&style, 0, sizeof(style));
	style.size.width.kind = XLAYOUT_LENGTH_AUTO;
	style.size.height.kind = XLAYOUT_LENGTH_AUTO;
	style.size.max_width = XLAYOUT_AUTO;
	style.size.max_height = XLAYOUT_AUTO;
	style.item.align_self = XLAYOUT_ALIGN_AUTO;
	style.item.justify_self = XLAYOUT_ALIGN_AUTO;
	style.item.shrink = 1.0f;
	style.item.column_span = 1u;
	style.item.row_span = 1u;
	style.container.format = XLAYOUT_FORMAT_STACK;
	style.container.axis = XLAYOUT_HORIZONTAL;
	style.container.align_items = XLAYOUT_ALIGN_STRETCH;
	style.container.justify_items = XLAYOUT_ALIGN_STRETCH;
	style.container.justify_content = XLAYOUT_JUSTIFY_START;
	style.container.overflow_x = XLAYOUT_OVERFLOW_VISIBLE;
	style.container.overflow_y = XLAYOUT_OVERFLOW_VISIBLE;
	style.visible = true;
	return style;
}

xlayout_constraints_t xLayoutConstraints(float max_width, float max_height)
{
	xlayout_constraints_t constraints = { 0.0f, max_width, 0.0f, max_height };
	return xl_normalize_constraints(constraints);
}

xlayout_constraints_t xLayoutConstraintsTight(float width, float height)
{
	xlayout_constraints_t constraints = { width, width, height, height };
	return xl_normalize_constraints(constraints);
}

xlayout_track_t xLayoutTrackFixed(float size)
{
	xlayout_track_t track = { XLAYOUT_TRACK_FIXED, xl_nonnegative(size), 0.0f, XLAYOUT_UNBOUNDED, 0.0f };
	return track;
}

xlayout_track_t xLayoutTrackAuto(void)
{
	xlayout_track_t track = { XLAYOUT_TRACK_AUTO, 0.0f, 0.0f, XLAYOUT_UNBOUNDED, 0.0f };
	return track;
}

xlayout_track_t xLayoutTrackFraction(float weight)
{
	xlayout_track_t track = { XLAYOUT_TRACK_FRACTION, weight > 0.0f ? weight : 1.0f, 0.0f, XLAYOUT_UNBOUNDED, 0.0f };
	return track;
}

xlayout_track_t xLayoutTrackMinMax(float min_size, float max_size, float weight)
{
	xlayout_track_t track = { XLAYOUT_TRACK_MINMAX, weight > 0.0f ? weight : 1.0f, xl_nonnegative(min_size), max_size, 0.0f };
	if ( track.max_size >= 0.0f && track.max_size < track.min_size ) track.max_size = track.min_size;
	return track;
}

xlayout_track_t xLayoutTrackPercent(float ratio)
{
	xlayout_track_t track = { XLAYOUT_TRACK_PERCENT, xl_clamp(ratio, 0.0f, 1.0f), 0.0f, XLAYOUT_UNBOUNDED, 0.0f };
	return track;
}

xlayout_track_t xLayoutTrackCross(float ratio, float min_size, float max_size)
{
	xlayout_track_t track = { XLAYOUT_TRACK_CROSS, xl_nonnegative(ratio),
		xl_nonnegative(min_size), xl_style_max(max_size), 0.0f };
	if ( track.max_size < track.min_size ) track.max_size = track.min_size;
	return track;
}

xlayout_node_t xLayoutNodeCreate(xlayout_context_t* context, xlayout_role_t role)
{
	xlayout_node_internal_t* node;
	uint32_t slot;
	if ( context == NULL ) return XLAYOUT_NODE_INVALID;
	if ( context->free_slot != XLAYOUT_SLOT_NONE ) {
		slot = context->free_slot;
		context->free_slot = context->slots[slot].next_free;
	} else {
		if ( !xl_reserve_slots(context, context->slot_count + 1u) ) return XLAYOUT_NODE_INVALID;
		slot = context->slot_count++;
		if ( context->slots[slot].generation == 0u ) context->slots[slot].generation = 1u;
	}
	node = (xlayout_node_internal_t*)xrtPoolAlloc(context->node_pool);
	if ( node == NULL ) {
		context->slots[slot].next_free = context->free_slot;
		context->free_slot = slot;
		return XLAYOUT_NODE_INVALID;
	}
	memset(node, 0, sizeof(*node));
	node->context = context;
	node->slot = slot;
	node->generation = context->slots[slot].generation;
	node->role = role;
	node->style = xLayoutStyleDefault();
	node->dirty = XLAYOUT_DIRTY_MEASURE | XLAYOUT_DIRTY_ARRANGE;
	node->cached_measure.baseline = -1.0f;
	context->slots[slot].node = node;
	context->live_nodes++;
	return xl_handle(node);
}

void xLayoutNodeDestroy(xlayout_context_t* context, xlayout_node_t handle)
{
	xlayout_node_internal_t* node = xl_resolve(context, handle);
	if ( node != NULL ) xl_destroy_node(context, node);
}

bool xLayoutNodeIsValid(const xlayout_context_t* context, xlayout_node_t node)
{
	return xl_resolve(context, node) != NULL;
}

bool xLayoutNodeInsertBefore(xlayout_context_t* context, xlayout_node_t parent_handle, xlayout_node_t child_handle, xlayout_node_t before_handle)
{
	xlayout_node_internal_t* parent = xl_resolve(context, parent_handle);
	xlayout_node_internal_t* child = xl_resolve(context, child_handle);
	xlayout_node_internal_t* before = before_handle == XLAYOUT_NODE_INVALID ? NULL : xl_resolve(context, before_handle);
	xlayout_node_internal_t* cursor;
	if ( parent == NULL || child == NULL || parent == child || parent->role != XLAYOUT_ROLE_CONTAINER ) return false;
	if ( before != NULL && before->parent != parent ) return false;
	for ( cursor = parent; cursor != NULL; cursor = cursor->parent ) {
		if ( cursor == child ) return false;
	}
	xl_detach(child);
	child->parent = parent;
	if ( before == NULL ) {
		child->previous = parent->last_child;
		if ( parent->last_child != NULL ) parent->last_child->next = child;
		else parent->first_child = child;
		parent->last_child = child;
	} else {
		child->next = before;
		child->previous = before->previous;
		if ( before->previous != NULL ) before->previous->next = child;
		else parent->first_child = child;
		before->previous = child;
	}
	parent->child_count++;
	xl_invalidate_measure_node(parent);
	return true;
}

bool xLayoutNodeAppend(xlayout_context_t* context, xlayout_node_t parent, xlayout_node_t child)
{
	return xLayoutNodeInsertBefore(context, parent, child, XLAYOUT_NODE_INVALID);
}

bool xLayoutNodeRemove(xlayout_context_t* context, xlayout_node_t child_handle)
{
	xlayout_node_internal_t* child = xl_resolve(context, child_handle);
	if ( child == NULL || child->parent == NULL ) return false;
	xl_detach(child);
	return true;
}

xlayout_node_t xLayoutNodeParent(const xlayout_context_t* context, xlayout_node_t node)
{
	xlayout_node_internal_t* resolved = xl_resolve(context, node);
	return resolved != NULL ? xl_handle(resolved->parent) : XLAYOUT_NODE_INVALID;
}

xlayout_node_t xLayoutNodeFirstChild(const xlayout_context_t* context, xlayout_node_t node)
{
	xlayout_node_internal_t* resolved = xl_resolve(context, node);
	return resolved != NULL ? xl_handle(resolved->first_child) : XLAYOUT_NODE_INVALID;
}

xlayout_node_t xLayoutNodeNextSibling(const xlayout_context_t* context, xlayout_node_t node)
{
	xlayout_node_internal_t* resolved = xl_resolve(context, node);
	return resolved != NULL ? xl_handle(resolved->next) : XLAYOUT_NODE_INVALID;
}

bool xLayoutNodeSetRole(xlayout_context_t* context, xlayout_node_t handle, xlayout_role_t role)
{
	xlayout_node_internal_t* node = xl_resolve(context, handle);
	if ( node == NULL || (role == XLAYOUT_ROLE_LEAF && node->child_count != 0u) ) return false;
	if ( node->role != role ) {
		node->role = role;
		xl_invalidate_measure_node(node);
	}
	return true;
}

bool xLayoutNodeSetStyle(xlayout_context_t* context, xlayout_node_t handle, const xlayout_style_t* style)
{
	xlayout_node_internal_t* node = xl_resolve(context, handle);
	xlayout_style_t normalized;
	if ( node == NULL || style == NULL ) return false;
	normalized = *style;
	if ( normalized.item.column_span == 0u ) normalized.item.column_span = 1u;
	if ( normalized.item.row_span == 0u ) normalized.item.row_span = 1u;
	if ( memcmp(&node->style, &normalized, sizeof(normalized)) == 0 ) return true;
	node->style = normalized;
	xl_invalidate_measure_node(node);
	return true;
}

bool xLayoutNodeGetStyle(const xlayout_context_t* context, xlayout_node_t handle, xlayout_style_t* output)
{
	xlayout_node_internal_t* node = xl_resolve(context, handle);
	if ( node == NULL || output == NULL ) return false;
	*output = node->style;
	return true;
}

bool xLayoutNodeSetMeasure(xlayout_context_t* context, xlayout_node_t handle, xlayout_measure_fn measure, void* user_data)
{
	xlayout_node_internal_t* node = xl_resolve(context, handle);
	if ( node == NULL ) return false;
	if ( node->measure_callback == measure && node->user_data == user_data ) return true;
	node->measure_callback = measure;
	node->user_data = user_data;
	xl_invalidate_measure_node(node);
	return true;
}

bool xLayoutNodeSetArrangeChildren(xlayout_context_t* context, xlayout_node_t handle,
	xlayout_arrange_children_fn arrange, void* user_data)
{
	xlayout_node_internal_t* node = xl_resolve(context, handle);
	if ( node == NULL ) return false;
	if ( node->arrange_children_callback == arrange && node->arrange_children_user_data == user_data ) return true;
	node->arrange_children_callback = arrange;
	node->arrange_children_user_data = user_data;
	xl_invalidate_arrange_node(node);
	return true;
}

bool xLayoutNodeSetFragmentSource(xlayout_context_t* context, xlayout_node_t handle, xlayout_fragment_count_fn count, xlayout_fragment_measure_fn measure)
{
	xlayout_node_internal_t* node = xl_resolve(context, handle);
	if ( node == NULL || (count == NULL) != (measure == NULL) ) return false;
	node->fragment_count_callback = count;
	node->fragment_measure_callback = measure;
	xl_invalidate_measure_node(node);
	return true;
}

bool xLayoutNodeSetUserData(xlayout_context_t* context, xlayout_node_t handle, void* user_data)
{
	xlayout_node_internal_t* node = xl_resolve(context, handle);
	if ( node == NULL ) return false;
	node->user_data = user_data;
	return true;
}

void* xLayoutNodeGetUserData(const xlayout_context_t* context, xlayout_node_t handle)
{
	xlayout_node_internal_t* node = xl_resolve(context, handle);
	return node != NULL ? node->user_data : NULL;
}

static bool xl_set_tracks(xlayout_context_t* context, xlayout_node_t handle, const xlayout_track_t* tracks, uint32_t count, bool columns)
{
	xlayout_node_internal_t* node = xl_resolve(context, handle);
	xlayout_track_t* copy = NULL;
	xlayout_track_t* current;
	uint32_t current_count;
	if ( node == NULL || (count != 0u && tracks == NULL) ) return false;
	current = columns ? node->columns : node->rows;
	current_count = columns ? node->column_count : node->row_count;
	if ( current_count == count && (count == 0u || memcmp(current, tracks, sizeof(*tracks) * count) == 0) ) return true;
	if ( count != 0u ) {
		copy = (xlayout_track_t*)xrtMalloc(sizeof(*copy) * count);
		if ( copy == NULL ) return false;
		memcpy(copy, tracks, sizeof(*copy) * count);
	}
	if ( columns ) {
		if ( node->columns != NULL ) xrtFree(node->columns);
		node->columns = copy;
		node->column_count = count;
	} else {
		if ( node->rows != NULL ) xrtFree(node->rows);
		node->rows = copy;
		node->row_count = count;
	}
	xl_invalidate_measure_node(node);
	return true;
}

bool xLayoutNodeSetColumns(xlayout_context_t* context, xlayout_node_t node, const xlayout_track_t* tracks, uint32_t count)
{
	return xl_set_tracks(context, node, tracks, count, true);
}

bool xLayoutNodeSetRows(xlayout_context_t* context, xlayout_node_t node, const xlayout_track_t* tracks, uint32_t count)
{
	return xl_set_tracks(context, node, tracks, count, false);
}

void xLayoutNodeInvalidateMeasure(xlayout_context_t* context, xlayout_node_t handle)
{
	xlayout_node_internal_t* node = xl_resolve(context, handle);
	if ( node != NULL ) xl_invalidate_measure_node(node);
}

void xLayoutNodeInvalidateArrange(xlayout_context_t* context, xlayout_node_t handle)
{
	xlayout_node_internal_t* node = xl_resolve(context, handle);
	if ( node != NULL ) xl_invalidate_arrange_node(node);
}

bool xLayoutMeasure(xlayout_context_t* context, xlayout_node_t handle, const xlayout_constraints_t* constraints, xlayout_measure_t* output)
{
	xlayout_node_internal_t* node = xl_resolve(context, handle);
	xlayout_measure_t measured;
	if ( node == NULL || constraints == NULL ) return false;
	if ( !xl_measure_node(context, node, *constraints, &measured) ) return false;
	if ( output != NULL ) *output = measured;
	return true;
}

bool xLayoutArrange(xlayout_context_t* context, xlayout_node_t handle, xlayout_rect_t rect)
{
	xlayout_node_internal_t* node = xl_resolve(context, handle);
	xlayout_constraints_t constraints;
	xlayout_measure_t measured;
	if ( node == NULL ) return false;
	constraints = xLayoutConstraints(rect.width, rect.height);
	if ( !xl_measure_node(context, node, constraints, &measured) ) return false;
	context->fragment_count = 0;
	context->arrange_epoch++;
	if ( context->arrange_epoch == 0u ) context->arrange_epoch = 1u;
	return xl_arrange_node(context, node, rect);
}

bool xLayoutArrangeChild(xlayout_context_t* context, xlayout_node_t parent_handle,
	xlayout_node_t child_handle, xlayout_rect_t rect)
{
	xlayout_node_internal_t* parent = xl_resolve(context, parent_handle);
	xlayout_node_internal_t* child = xl_resolve(context, child_handle);
	if ( parent == NULL || child == NULL || context->active_arrange_parent != parent
		|| child->parent != parent ) return false;
	return xl_arrange_node(context, child, rect);
}

bool xLayoutRun(xlayout_context_t* context, xlayout_node_t node, const xlayout_constraints_t* constraints, xlayout_rect_t rect, xlayout_measure_t* measured)
{
	if ( !xLayoutMeasure(context, node, constraints, measured) ) return false;
	return xLayoutArrange(context, node, rect);
}

bool xLayoutNodeGetResult(const xlayout_context_t* context, xlayout_node_t handle, xlayout_result_t* output)
{
	xlayout_node_internal_t* node = xl_resolve(context, handle);
	if ( node == NULL || output == NULL ) return false;
	*output = node->result;
	if ( node->fragment_epoch != context->arrange_epoch ) output->fragment_count = 0u;
	return true;
}

uint32_t xLayoutNodeFragmentCount(const xlayout_context_t* context, xlayout_node_t handle)
{
	xlayout_node_internal_t* node = xl_resolve(context, handle);
	return node != NULL && node->fragment_epoch == context->arrange_epoch ? node->result.fragment_count : 0u;
}

bool xLayoutNodeGetFragment(const xlayout_context_t* context, xlayout_node_t handle, uint32_t index, xlayout_fragment_t* output)
{
	xlayout_node_internal_t* node = xl_resolve(context, handle);
	if ( node == NULL || output == NULL || node->fragment_epoch != context->arrange_epoch
		|| index >= node->result.fragment_count ) return false;
	*output = context->fragments[node->fragment_start + index];
	return true;
}
