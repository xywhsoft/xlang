#include "xui_internal.h"

#include <math.h>
#include <string.h>

static xlayout_align_t __xuiLayoutAlign(int align)
{
	switch ( align ) {
	case XUI_ALIGN_CENTER: return XLAYOUT_ALIGN_CENTER;
	case XUI_ALIGN_END: return XLAYOUT_ALIGN_END;
	case XUI_ALIGN_STRETCH: return XLAYOUT_ALIGN_STRETCH;
	case XUI_ALIGN_START:
	default: return XLAYOUT_ALIGN_START;
	}
}

static xlayout_overflow_t __xuiLayoutOverflow(int overflow)
{
	return overflow == XUI_OVERFLOW_VISIBLE ? XLAYOUT_OVERFLOW_VISIBLE : XLAYOUT_OVERFLOW_CLIP;
}

static xlayout_length_t __xuiLayoutLength(int mode, float preferred, float current)
{
	xlayout_length_t length;
	length.value = 0.0f;
	if ( mode == XUI_SIZE_CONTENT ) {
		length.kind = XLAYOUT_LENGTH_AUTO;
	} else if ( mode == XUI_SIZE_FILL ) {
		length.kind = XLAYOUT_LENGTH_FILL;
	} else if ( preferred > 0.0f ) {
		length.kind = XLAYOUT_LENGTH_FIXED;
		length.value = preferred;
	} else if ( current > 0.0f ) {
		length.kind = XLAYOUT_LENGTH_FIXED;
		length.value = current;
	} else {
		length.kind = XLAYOUT_LENGTH_AUTO;
	}
	return length;
}

static bool __xuiLayoutMeasureCallback(
	xlayout_context_t* context,
	xlayout_node_t node,
	const xlayout_constraints_t* constraints,
	void* user_data,
	xlayout_measure_t* output)
{
	xui_widget widget = (xui_widget)user_data;
	xui_vec2_t constraint;
	xui_vec2_t content = { 0.0f, 0.0f };
	int result;
	(void)context;
	(void)node;
	if ( !xuiInternalWidgetIsValid(widget) ) return false;
	constraint.fX = constraints->max_width < XLAYOUT_UNBOUNDED ? constraints->max_width : XUI_LAYOUT_UNBOUNDED;
	constraint.fY = constraints->max_height < XLAYOUT_UNBOUNDED ? constraints->max_height : XUI_LAYOUT_UNBOUNDED;
	if ( widget->onContentMeasure != NULL ) {
		result = widget->onContentMeasure(widget, constraint, &content, widget->pContentMeasureUser);
		if ( result != XUI_OK ) {
			xuiInternalReportError(widget->pContext, widget, result, XUI_ERROR_STAGE_LAYOUT, 1,
				"layout.measure", "Content measurement failed; zero content size was used.");
			content.fX = 0.0f;
			content.fY = 0.0f;
		} else if ( !isfinite(content.fX) || !isfinite(content.fY) ||
		            (content.fX < 0.0f) || (content.fY < 0.0f) ) {
			xuiInternalReportError(widget->pContext, widget, XUI_ERROR_INVALID_ARGUMENT,
				XUI_ERROR_STAGE_LAYOUT, 1, "layout.measure",
				"Content measurement returned an invalid size; invalid axes were clamped to zero.");
			if ( !isfinite(content.fX) || (content.fX < 0.0f) ) content.fX = 0.0f;
			if ( !isfinite(content.fY) || (content.fY < 0.0f) ) content.fY = 0.0f;
		}
	}
	output->width = content.fX;
	output->height = content.fY;
	output->baseline = widget->tLayout.fBaseline > 0.0f ? widget->tLayout.fBaseline : output->height;
	return true;
}

static bool __xuiLayoutArrangeChildrenCallback(
	xlayout_context_t* context,
	xlayout_node_t node,
	xlayout_rect_t content_rect,
	void* user_data)
{
	xui_widget widget = (xui_widget)user_data;
	xlayout_result_t parent_result;
	xui_rect_t local_content;
	xui_rect_t parent_pixels;
	xui_rect_t content_pixels;
	int result;
	if ( !xuiInternalWidgetIsValid(widget) || widget->onLayoutChildren == NULL
		|| !xLayoutNodeGetResult(context, node, &parent_result) ) return false;
	parent_pixels = xuiInternalRectFromFloatNearest(parent_result.rect.x,
		parent_result.rect.y, parent_result.rect.width, parent_result.rect.height);
	content_pixels = xuiInternalRectFromFloatNearest(content_rect.x,
		content_rect.y, content_rect.width, content_rect.height);
	local_content.fX = content_pixels.fX - parent_pixels.fX;
	local_content.fY = content_pixels.fY - parent_pixels.fY;
	local_content.fW = content_pixels.fW;
	local_content.fH = content_pixels.fH;
	result = widget->onLayoutChildren(widget, local_content, widget->pLayoutChildrenUser);
	if ( result != XUI_OK ) {
		xuiInternalReportError(widget->pContext, widget, result, XUI_ERROR_STAGE_LAYOUT, 1,
			"layout.children", "Custom child arrangement failed; existing child results were preserved.");
	}
	return true;
}

static xlayout_track_t __xuiLayoutTrack(xui_table_track_t track)
{
	xlayout_track_t result;
	float maximum = track.fMax < XUI_LAYOUT_UNBOUNDED ? track.fMax : XLAYOUT_UNBOUNDED;
	if ( track.iSizeMode == XUI_SIZE_FIXED ) {
		result = xLayoutTrackFixed(track.fValue);
		result.min_size = track.fMin;
		result.max_size = maximum;
	} else if ( track.iSizeMode == XUI_SIZE_FILL ) {
		result = xLayoutTrackFraction(track.fWeight);
		result.min_size = track.fMin;
		result.max_size = maximum;
	} else if ( track.iSizeMode == XUI_TRACK_CROSS ) {
		result = xLayoutTrackCross(track.fValue, track.fMin, maximum);
	} else {
		result = xLayoutTrackAuto();
		result.min_size = track.fMin;
		result.max_size = maximum;
	}
	result.shrink = track.fShrink;
	return result;
}

static xlayout_track_t* __xuiLayoutTrackScratch(xui_context context, uint32_t count)
{
	xlayout_track_t* scratch;
	uint32_t capacity;
	if ( count == 0u ) return NULL;
	if ( count <= context->iLayoutTrackScratchCapacity ) return context->pLayoutTrackScratch;
	capacity = context->iLayoutTrackScratchCapacity > 0u ? context->iLayoutTrackScratchCapacity : 8u;
	while ( capacity < count ) {
		if ( capacity > UINT32_MAX / 2u ) {
			capacity = count;
			break;
		}
		capacity *= 2u;
	}
	scratch = (xlayout_track_t*)xrtRealloc(context->pLayoutTrackScratch, sizeof(*scratch) * (size_t)capacity);
	if ( scratch == NULL ) return NULL;
	context->pLayoutTrackScratch = scratch;
	context->iLayoutTrackScratchCapacity = capacity;
	return scratch;
}

static void __xuiLayoutMapContainer(xui_widget widget, xlayout_style_t* style)
{
	switch ( widget->tLayout.iLayoutType ) {
	case XUI_LAYOUT_ROW:
		style->container.format = XLAYOUT_FORMAT_STACK;
		style->container.axis = XLAYOUT_HORIZONTAL;
		break;
	case XUI_LAYOUT_COLUMN:
		style->container.format = XLAYOUT_FORMAT_STACK;
		style->container.axis = XLAYOUT_VERTICAL;
		break;
	case XUI_LAYOUT_FLOW:
		style->container.format = XLAYOUT_FORMAT_FLOW;
		style->container.axis = XLAYOUT_HORIZONTAL;
		break;
	case XUI_LAYOUT_DOCK:
		style->container.format = XLAYOUT_FORMAT_DOCK;
		break;
	case XUI_LAYOUT_TABLE:
	case XUI_LAYOUT_GRID:
		style->container.format = XLAYOUT_FORMAT_TRACK;
		break;
	case XUI_LAYOUT_OVERLAY:
	case XUI_LAYOUT_MANUAL:
	default:
		style->container.format = XLAYOUT_FORMAT_LAYER;
		break;
	}
	style->container.padding.left = widget->tLayout.tPadding.fLeft;
	style->container.padding.top = widget->tLayout.tPadding.fTop;
	style->container.padding.right = widget->tLayout.tPadding.fRight;
	style->container.padding.bottom = widget->tLayout.tPadding.fBottom;
	style->container.column_gap = widget->tLayout.fGap;
	style->container.row_gap = widget->tLayout.fGap;
	style->container.overflow_x = __xuiLayoutOverflow(widget->tLayout.iOverflow);
	style->container.overflow_y = __xuiLayoutOverflow(widget->tLayout.iOverflow);
	style->container.align_items = XLAYOUT_ALIGN_STRETCH;
	style->container.justify_items = XLAYOUT_ALIGN_STRETCH;
}

static void __xuiLayoutMapItem(
	xui_widget widget,
	xui_widget parent,
	uint32_t visible_index,
	xlayout_style_t* style)
{
	style->item.margin.left = widget->tLayout.tMargin.fLeft;
	style->item.margin.top = widget->tLayout.tMargin.fTop;
	style->item.margin.right = widget->tLayout.tMargin.fRight;
	style->item.margin.bottom = widget->tLayout.tMargin.fBottom;
	style->item.grow = widget->tLayout.fGrow;
	style->item.shrink = widget->tLayout.fShrink;
	style->item.justify_self = widget->tLayout.iWidthMode == XUI_SIZE_FILL
		? XLAYOUT_ALIGN_STRETCH : __xuiLayoutAlign(widget->tLayout.iAlignX);
	style->item.align_self = widget->tLayout.iHeightMode == XUI_SIZE_FILL
		? XLAYOUT_ALIGN_STRETCH : __xuiLayoutAlign(widget->tLayout.iAlignY);
	style->item.column = (uint32_t)widget->tLayout.iTableColumn;
	style->item.row = (uint32_t)widget->tLayout.iTableRow;
	style->item.column_span = (uint32_t)widget->tLayout.iTableColumnSpan;
	style->item.row_span = (uint32_t)widget->tLayout.iTableRowSpan;
	style->item.order = widget->tLayout.iZIndex;
	switch ( widget->tLayout.iDock ) {
	case XUI_DOCK_LEFT: style->item.dock = XLAYOUT_DOCK_LEFT; break;
	case XUI_DOCK_TOP: style->item.dock = XLAYOUT_DOCK_TOP; break;
	case XUI_DOCK_RIGHT: style->item.dock = XLAYOUT_DOCK_RIGHT; break;
	case XUI_DOCK_BOTTOM: style->item.dock = XLAYOUT_DOCK_BOTTOM; break;
	case XUI_DOCK_FILL:
	default: style->item.dock = XLAYOUT_DOCK_FILL; break;
	}
	if ( parent == NULL ) return;
	if ( parent->tLayout.iLayoutType == XUI_LAYOUT_ROW ) {
		style->item.align_self = widget->tLayout.iHeightMode == XUI_SIZE_FILL
			? XLAYOUT_ALIGN_STRETCH : __xuiLayoutAlign(widget->tLayout.iAlignY);
		if ( widget->tLayout.iWidthMode == XUI_SIZE_FILL && style->item.grow <= 0.0f ) style->item.grow = 1.0f;
	} else if ( parent->tLayout.iLayoutType == XUI_LAYOUT_COLUMN ) {
		style->item.align_self = widget->tLayout.iWidthMode == XUI_SIZE_FILL
			? XLAYOUT_ALIGN_STRETCH : __xuiLayoutAlign(widget->tLayout.iAlignX);
		if ( widget->tLayout.iHeightMode == XUI_SIZE_FILL && style->item.grow <= 0.0f ) style->item.grow = 1.0f;
	} else if ( parent->tLayout.iLayoutType == XUI_LAYOUT_MANUAL ) {
		style->item.x = widget->tRect.fX;
		style->item.y = widget->tRect.fY;
		style->item.justify_self = XLAYOUT_ALIGN_START;
		style->item.align_self = XLAYOUT_ALIGN_START;
		style->size.width.kind = XLAYOUT_LENGTH_FIXED;
		style->size.width.value = widget->tRect.fW;
		style->size.height.kind = XLAYOUT_LENGTH_FIXED;
		style->size.height.value = widget->tRect.fH;
	} else if ( parent->tLayout.iLayoutType == XUI_LAYOUT_OVERLAY ) {
		if ( widget->tLayout.iFlowMode == XUI_FLOW_ABSOLUTE ) {
			style->item.position = XLAYOUT_POSITION_ABSOLUTE;
			style->item.x = widget->tRect.fX;
			style->item.y = widget->tRect.fY;
			style->item.justify_self = XLAYOUT_ALIGN_START;
			style->item.align_self = XLAYOUT_ALIGN_START;
			style->size.width.kind = XLAYOUT_LENGTH_FIXED;
			style->size.width.value = widget->tRect.fW;
			style->size.height.kind = XLAYOUT_LENGTH_FIXED;
			style->size.height.value = widget->tRect.fH;
		} else {
			style->item.x = 0.0f;
			style->item.y = 0.0f;
		}
	} else if ( parent->tLayout.iLayoutType == XUI_LAYOUT_FLOW ) {
		style->item.position = (widget->tLayout.iFlowMode == XUI_FLOW_NONE || widget->tLayout.iFlowMode == XUI_FLOW_ABSOLUTE)
			? XLAYOUT_POSITION_ABSOLUTE : XLAYOUT_POSITION_FLOW;
		style->item.x = widget->tRect.fX;
		style->item.y = widget->tRect.fY;
		style->item.break_before = widget->tLayout.iFlowMode == XUI_FLOW_BLOCK;
		style->item.break_after = widget->tLayout.iFlowMode == XUI_FLOW_BLOCK;
	} else if ( parent->tLayout.iLayoutType == XUI_LAYOUT_GRID ) {
		uint32_t columns = parent->tLayout.iGridColumnCount > 0 ? (uint32_t)parent->tLayout.iGridColumnCount : 1u;
		style->item.column = visible_index % columns;
		style->item.row = visible_index / columns;
		style->item.column_span = 1u;
		style->item.row_span = 1u;
	}
}

static int __xuiLayoutSyncTracks(xui_widget widget, uint32_t visible_children)
{
	xlayout_context_t* context = widget->pContext->pLayoutContext;
	xlayout_track_t* columns = NULL;
	xlayout_track_t* rows = NULL;
	xlayout_track_t* scratch = NULL;
	uint32_t column_count = 0u;
	uint32_t row_count = 0u;
	uint32_t index;
	int result = XUI_OK;
	if ( widget->tLayout.iLayoutType == XUI_LAYOUT_TABLE ) {
		column_count = widget->iTableColumnCount > 0 ? (uint32_t)widget->iTableColumnCount : 0u;
		row_count = widget->iTableRowCount > 0 ? (uint32_t)widget->iTableRowCount : 0u;
		scratch = __xuiLayoutTrackScratch(widget->pContext, column_count + row_count);
		if ( column_count + row_count > 0u && scratch == NULL ) result = XUI_ERROR_OUT_OF_MEMORY;
		columns = scratch;
		rows = scratch != NULL ? scratch + column_count : NULL;
		if ( result == XUI_OK ) {
			for ( index = 0; index < column_count; ++index ) columns[index] = __xuiLayoutTrack(widget->pTableColumns[index]);
			for ( index = 0; index < row_count; ++index ) rows[index] = __xuiLayoutTrack(widget->pTableRows[index]);
		}
	} else if ( widget->tLayout.iLayoutType == XUI_LAYOUT_GRID ) {
		column_count = widget->tLayout.iGridColumnCount > 0 ? (uint32_t)widget->tLayout.iGridColumnCount : 1u;
		if ( visible_children > 0u && column_count > visible_children ) column_count = visible_children;
		row_count = visible_children > 0u ? (visible_children + column_count - 1u) / column_count : 1u;
		scratch = __xuiLayoutTrackScratch(widget->pContext, column_count + row_count);
		if ( scratch == NULL ) result = XUI_ERROR_OUT_OF_MEMORY;
		columns = scratch;
		rows = scratch != NULL ? scratch + column_count : NULL;
		if ( result == XUI_OK ) {
			for ( index = 0; index < column_count; ++index ) columns[index] = widget->tLayout.fGridItemWidth > 0.0f
				? xLayoutTrackFixed(widget->tLayout.fGridItemWidth) : xLayoutTrackAuto();
			for ( index = 0; index < row_count; ++index ) rows[index] = widget->tLayout.fGridItemHeight > 0.0f
				? xLayoutTrackFixed(widget->tLayout.fGridItemHeight) : xLayoutTrackAuto();
		}
	}
	if ( result == XUI_OK && (!xLayoutNodeSetColumns(context, widget->iLayoutNode, columns, column_count)
		|| !xLayoutNodeSetRows(context, widget->iLayoutNode, rows, row_count)) ) result = XUI_ERROR_OUT_OF_MEMORY;
	return result;
}

static int __xuiLayoutSyncNode(xui_widget widget, xui_widget parent, uint32_t visible_index)
{
	xlayout_context_t* context;
	xlayout_style_t style;
	xui_widget child;
	uint32_t child_visible_count = 0u;
	uint32_t child_visible_index = 0u;
	uint32_t parent_version = parent != NULL ? parent->iLayoutVersion : 0u;
	int sync_self;
	int result;
	if ( !xuiInternalWidgetIsValid(widget) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( widget->onLayoutPrepare != NULL ) {
		result = widget->onLayoutPrepare(widget, widget->pLayoutPrepareUser);
		if ( result != XUI_OK ) {
			xuiInternalReportError(widget->pContext, widget, result, XUI_ERROR_STAGE_LAYOUT, 1,
				"layout.prepare", "Layout preparation failed; the current widget properties were used.");
		}
	}
	sync_self = widget->iLayoutSyncedVersion != widget->iLayoutVersion
		|| widget->iLayoutSyncedParentVersion != parent_version
		|| (widget->iDirtyFlags & XUI_WIDGET_DIRTY_LAYOUT) != 0u;
	if ( !sync_self && (widget->iSubtreeDirtyFlags & XUI_WIDGET_DIRTY_LAYOUT) == 0u ) return XUI_OK;
	context = widget->pContext->pLayoutContext;
	for ( child = widget->pFirstChild; child != NULL; child = child->pNextSibling ) {
		if ( child->bVisible ) child_visible_count++;
	}
	if ( sync_self ) {
		style = xLayoutStyleDefault();
		style.visible = widget->bVisible != 0;
		style.size.width = __xuiLayoutLength(widget->tLayout.iWidthMode, widget->tLayout.fPreferredWidth, widget->tRect.fW);
		style.size.height = __xuiLayoutLength(widget->tLayout.iHeightMode, widget->tLayout.fPreferredHeight, widget->tRect.fH);
		style.size.min_width = widget->tLayout.fMinWidth;
		style.size.min_height = widget->tLayout.fMinHeight;
		style.size.max_width = widget->tLayout.fMaxWidth < XUI_LAYOUT_UNBOUNDED ? widget->tLayout.fMaxWidth : XLAYOUT_AUTO;
		style.size.max_height = widget->tLayout.fMaxHeight < XUI_LAYOUT_UNBOUNDED ? widget->tLayout.fMaxHeight : XLAYOUT_AUTO;
		__xuiLayoutMapContainer(widget, &style);
		__xuiLayoutMapItem(widget, parent, visible_index, &style);
		if ( !xLayoutNodeSetRole(context, widget->iLayoutNode,
			widget->iChildCount > 0 ? XLAYOUT_ROLE_CONTAINER : XLAYOUT_ROLE_LEAF)
			|| !xLayoutNodeSetStyle(context, widget->iLayoutNode, &style)
			|| !xLayoutNodeSetMeasure(context, widget->iLayoutNode, __xuiLayoutMeasureCallback, widget)
			|| !xLayoutNodeSetArrangeChildren(context, widget->iLayoutNode,
				widget->onLayoutChildren != NULL ? __xuiLayoutArrangeChildrenCallback : NULL, widget) ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		result = __xuiLayoutSyncTracks(widget, child_visible_count);
		if ( result != XUI_OK ) return result;
		widget->iLayoutSyncedVersion = widget->iLayoutVersion;
		widget->iLayoutSyncedParentVersion = parent_version;
	}
	for ( child = widget->pFirstChild; child != NULL; child = child->pNextSibling ) {
		result = __xuiLayoutSyncNode(child, widget, child_visible_index);
		if ( result != XUI_OK ) return result;
		if ( child->bVisible ) child_visible_index++;
	}
	return XUI_OK;
}

static xui_rect_t __xuiLayoutLocalRect(xui_widget widget, xlayout_rect_t rect, int bSubtreeRoot)
{
	xui_rect_t result;
	xui_rect_t parent_pixels;

	result = xuiInternalRectFromFloatNearest(rect.x, rect.y, rect.width, rect.height);
	if ( !bSubtreeRoot && widget->pParent != NULL ) {
		xlayout_result_t parent_result;
		if ( xLayoutNodeGetResult(widget->pContext->pLayoutContext, widget->pParent->iLayoutNode, &parent_result) ) {
			parent_pixels = xuiInternalRectFromFloatNearest(parent_result.rect.x,
				parent_result.rect.y, parent_result.rect.width, parent_result.rect.height);
			result.fX -= parent_pixels.fX;
			result.fY -= parent_pixels.fY;
		}
	}
	return result;
}

static int __xuiLayoutApply(xui_widget widget, int bSubtreeRoot);

static void __xuiLayoutClearDirtyTree(xui_widget widget)
{
	xui_widget child;
	widget->iDirtyFlags &= ~XUI_WIDGET_DIRTY_LAYOUT;
	widget->iSubtreeDirtyFlags &= ~XUI_WIDGET_DIRTY_LAYOUT;
	for ( child = widget->pFirstChild; child != NULL; child = child->pNextSibling ) {
		__xuiLayoutClearDirtyTree(child);
	}
}

static void __xuiLayoutRefreshSubtreeDirty(xui_widget widget)
{
	xui_widget child;
	uint32_t flags = widget->iDirtyFlags & XUI_WIDGET_DIRTY_LAYOUT;
	for ( child = widget->pFirstChild; child != NULL; child = child->pNextSibling ) {
		flags |= child->iSubtreeDirtyFlags & XUI_WIDGET_DIRTY_LAYOUT;
	}
	widget->iSubtreeDirtyFlags = (widget->iSubtreeDirtyFlags & ~XUI_WIDGET_DIRTY_LAYOUT) | flags;
}

static int __xuiLayoutApply(xui_widget widget, int bSubtreeRoot)
{
	xlayout_result_t layout_result;
	xui_widget child;
	xui_rect_t content;
	int result;
	if ( !widget->bVisible ) {
		__xuiLayoutClearDirtyTree(widget);
		return XUI_OK;
	}
	if ( !xLayoutNodeGetResult(widget->pContext->pLayoutContext, widget->iLayoutNode, &layout_result) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	widget->tRect = __xuiLayoutLocalRect(widget, layout_result.rect, bSubtreeRoot);
	widget->tMeasuredSize.fX = xuiInternalSnapSize(layout_result.rect.width);
	widget->tMeasuredSize.fY = xuiInternalSnapSize(layout_result.rect.height);
	widget->bMeasureValid = 1;
	widget->bArrangeValid = 1;
	/* New invalidations raised by descendants or Complete belong to the next pass. */
	widget->iDirtyFlags &= ~XUI_WIDGET_DIRTY_LAYOUT;
	content = xuiWidgetGetContentRect(widget);
	for ( child = widget->pFirstChild; child != NULL; child = child->pNextSibling ) {
		result = __xuiLayoutApply(child, 0);
		if ( result != XUI_OK ) return result;
	}
	if ( widget->onLayoutComplete != NULL ) {
		result = widget->onLayoutComplete(widget, content, widget->pLayoutCompleteUser);
		if ( result != XUI_OK ) {
			xuiInternalReportError(widget->pContext, widget, result, XUI_ERROR_STAGE_LAYOUT, 1,
				"layout.complete", "Layout completion failed; the computed layout was preserved.");
		}
	}
	__xuiLayoutRefreshSubtreeDirty(widget);
	return XUI_OK;
}

int xuiInternalLayoutCreateWidget(xui_widget widget)
{
	if ( !xuiInternalWidgetIsValid(widget) || widget->pContext->pLayoutContext == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	widget->iLayoutNode = xLayoutNodeCreate(widget->pContext->pLayoutContext, XLAYOUT_ROLE_LEAF);
	return widget->iLayoutNode != XLAYOUT_NODE_INVALID ? XUI_OK : XUI_ERROR_OUT_OF_MEMORY;
}

void xuiInternalLayoutDestroyWidget(xui_widget widget)
{
	if ( widget == NULL || widget->pContext == NULL || widget->pContext->pLayoutContext == NULL
		|| widget->iLayoutNode == XLAYOUT_NODE_INVALID ) return;
	xLayoutNodeDestroy(widget->pContext->pLayoutContext, widget->iLayoutNode);
	widget->iLayoutNode = XLAYOUT_NODE_INVALID;
}

int xuiInternalLayoutAttach(xui_widget parent, xui_widget child, xui_widget before)
{
	xlayout_context_t* context;
	if ( !xuiInternalWidgetIsValid(parent) || !xuiInternalWidgetIsValid(child) ) return XUI_ERROR_INVALID_ARGUMENT;
	context = parent->pContext->pLayoutContext;
	if ( !xLayoutNodeSetRole(context, parent->iLayoutNode, XLAYOUT_ROLE_CONTAINER)
		|| !xLayoutNodeInsertBefore(context, parent->iLayoutNode, child->iLayoutNode,
			before != NULL ? before->iLayoutNode : XLAYOUT_NODE_INVALID) ) return XUI_ERROR_OUT_OF_MEMORY;
	return XUI_OK;
}

void xuiInternalLayoutDetach(xui_widget widget)
{
	xui_widget parent;
	if ( !xuiInternalWidgetIsValid(widget) || widget->pContext->pLayoutContext == NULL ) return;
	parent = widget->pParent;
	(void)xLayoutNodeRemove(widget->pContext->pLayoutContext, widget->iLayoutNode);
	if ( parent != NULL && parent->iChildCount <= 1 ) {
		(void)xLayoutNodeSetRole(parent->pContext->pLayoutContext, parent->iLayoutNode, XLAYOUT_ROLE_LEAF);
	}
}

void xuiInternalLayoutInvalidate(xui_widget widget, int measure)
{
	if ( !xuiInternalWidgetIsValid(widget) || widget->pContext->pLayoutContext == NULL ) return;
	if ( measure ) xLayoutNodeInvalidateMeasure(widget->pContext->pLayoutContext, widget->iLayoutNode);
	else xLayoutNodeInvalidateArrange(widget->pContext->pLayoutContext, widget->iLayoutNode);
}

int xuiInternalLayoutMeasure(xui_widget widget, xui_vec2_t constraint, xui_vec2_t* measured)
{
	xlayout_constraints_t constraints;
	xlayout_measure_t result;
	int sync_result;
	if ( !xuiInternalWidgetIsValid(widget) || measured == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	sync_result = __xuiLayoutSyncNode(widget, NULL, 0u);
	if ( sync_result != XUI_OK ) return sync_result;
	constraints = xLayoutConstraints(constraint.fX, constraint.fY);
	if ( !xLayoutMeasure(widget->pContext->pLayoutContext, widget->iLayoutNode, &constraints, &result) ) return XUI_ERROR_OUT_OF_MEMORY;
	measured->fX = xuiInternalSnapSize(result.width);
	measured->fY = xuiInternalSnapSize(result.height);
	widget->tMeasuredSize = *measured;
	widget->tMeasureConstraint = constraint;
	widget->bMeasureValid = 1;
	return XUI_OK;
}

int xuiInternalLayoutArrange(xui_widget widget, xui_rect_t rect)
{
	int result;
	if ( !xuiInternalWidgetIsValid(widget) ) return XUI_ERROR_INVALID_ARGUMENT;
	result = __xuiLayoutSyncNode(widget, NULL, 0u);
	if ( result != XUI_OK ) return result;
	if ( !xLayoutArrange(widget->pContext->pLayoutContext, widget->iLayoutNode,
		(xlayout_rect_t){ rect.fX, rect.fY, rect.fW, rect.fH }) ) return XUI_ERROR_OUT_OF_MEMORY;
	return __xuiLayoutApply(widget, 1);
}

int xuiLayoutArrangeChild(xui_widget parent, xui_widget child, xui_rect_t rect)
{
	xlayout_result_t parent_result;
	xlayout_rect_t global_rect;
	if ( !xuiInternalWidgetIsValid(parent) || !xuiInternalWidgetIsValid(child)
		|| parent->pContext != child->pContext || child->pParent != parent
		|| rect.fW < 0.0f || rect.fH < 0.0f ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !xLayoutNodeGetResult(parent->pContext->pLayoutContext, parent->iLayoutNode, &parent_result) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	global_rect.x = parent_result.rect.x + rect.fX;
	global_rect.y = parent_result.rect.y + rect.fY;
	global_rect.width = rect.fW;
	global_rect.height = rect.fH;
	return xLayoutArrangeChild(parent->pContext->pLayoutContext, parent->iLayoutNode,
		child->iLayoutNode, global_rect) ? XUI_OK : XUI_ERROR_INVALID_ARGUMENT;
}
