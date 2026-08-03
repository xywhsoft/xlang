#include "xui_internal.h"

#include <math.h>
#include <string.h>

#define XUI_FLOW_NODE_BUCKET_SIZE 256.0f
#define XUI_FLOW_GRID_ZOOM_BUCKET_SCALE 100.0f

typedef struct xui_flow_graph_drag_node_t {
	char* sId;
	float fStartX;
	float fStartY;
} xui_flow_graph_drag_node_t;

typedef struct xui_flow_graph_grid_line_t {
	float fPos;
	int bVertical;
} xui_flow_graph_grid_line_t;

typedef struct xui_flow_graph_node_card_state_t {
	int iNode;
	int iDiagCount;
	uint32_t iFill;
	uint32_t iBorder;
	int bHasRunBadge;
	char sRunBadge[8];
	uint32_t iRunBadgeColor;
	uint32_t iRunBadgeTextColor;
} xui_flow_graph_node_card_state_t;

typedef struct xui_flow_graph_edge_layer_state_t {
	int iEdge;
	float fX0;
	float fY0;
	float fX1;
	float fY1;
	int iRoutePointCount;
	float arrRouteX[32];
	float arrRouteY[32];
	uint32_t iColor;
} xui_flow_graph_edge_layer_state_t;

typedef struct xui_flow_graph_route_t {
	int iCount;
	float arrX[32];
	float arrY[32];
} xui_flow_graph_route_t;

typedef struct xui_flow_graph_node_bucket_t {
	xarray_struct arrNodes;
} xui_flow_graph_node_bucket_t;

typedef struct xui_flow_graph_edge_bucket_t {
	xarray_struct arrEdges;
} xui_flow_graph_edge_bucket_t;

typedef struct xui_flow_graph_widget_data_t {
	xui_flow_graph pGraph;
	int bOwnGraph;
	uint32_t iBackgroundColor;
	uint32_t iGridColor;
	uint32_t iNodeColor;
	uint32_t iSelectedNodeColor;
	uint32_t iNodeBorderColor;
	uint32_t iPortColor;
	uint32_t iEdgeColor;
	uint32_t iSelectedEdgeColor;
	uint32_t iLastGraphRevision;
	int bDraggingNode;
	int bDraggingConnection;
	int bDraggingMarquee;
	xui_flow_graph_drag_node_t* arrDragNodes;
	int iDragNodeCount;
	char* sDragNodeId;
	char* sDragPortId;
	int iDragPortKind;
	int iDragPortDirection;
	uint32_t iMarqueeModifiers;
	float fDragStartX;
	float fDragStartY;
	float fDragCurrentX;
	float fDragCurrentY;
	int bDragConnectionValid;
	int iHoverType;
	int iHoverNode;
	int iHoverPort;
	int iHoverEdge;
	xarray_struct arrNodeBuckets;
	xarray_struct arrVisibleNodes;
	uint32_t iNodeBucketRevision;
	xui_rect_t tNodeBucketContent;
	int iNodeBucketCols;
	int iNodeBucketRows;
	xarray_struct arrEdgeBuckets;
	xarray_struct arrVisibleEdges;
	uint32_t iEdgeBucketRevision;
	xui_rect_t tEdgeBucketContent;
	int iEdgeBucketCols;
	int iEdgeBucketRows;
	xarray_struct arrGridLines;
	xui_rect_t tGridCacheContent;
	int iGridZoomBucket;
	float fGridPanX;
	float fGridPanY;
	float fGridStep;
	xarray_struct arrNodeCardStates;
	uint32_t iNodeCardRevision;
	int iNodeCardHoverType;
	int iNodeCardHoverNode;
	int iNodeCardEnabled;
	xarray_struct arrEdgeLayerStates;
	uint32_t iEdgeLayerRevision;
	xui_rect_t tEdgeLayerContent;
	int iEdgeLayerHoverType;
	int iEdgeLayerHoverEdge;
} xui_flow_graph_widget_data_t;

static int __xuiFlowGraphWidgetDescValid(const xui_flow_graph_desc_t* pDesc)
{
	return (pDesc == NULL) || (pDesc->iSize == 0u) || (pDesc->iSize >= sizeof(*pDesc));
}

static xui_flow_graph_widget_data_t* __xuiFlowGraphWidgetGetData(xui_widget pWidget)
{
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return NULL;
	}
	pType = xuiFlowGraphGetType(xuiWidgetGetContext(pWidget));
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_flow_graph_widget_data_t*)xuiWidgetGetTypeData(pWidget);
}

static void __xuiFlowGraphWidgetPointToLocal(xui_widget pWidget, float fX, float fY, float* pLocalX, float* pLocalY)
{
	xui_rect_t tWorld;

	tWorld = xuiWidgetGetWorldRect(pWidget);
	if ( pLocalX != NULL ) *pLocalX = fX - tWorld.fX;
	if ( pLocalY != NULL ) *pLocalY = fY - tWorld.fY;
}

static int __xuiFlowGraphWidgetEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser);
static void __xuiFlowGraphWidgetPortCenter(xui_flow_graph pGraph, int iNode, int iPort, xui_rect_t tContent, float* pX, float* pY);

static int __xuiFlowGraphWidgetSetHover(xui_widget pWidget, xui_flow_graph_widget_data_t* pData, const xui_flow_hit_t* pHit)
{
	int iType;
	int iNode;
	int iPort;
	int iEdge;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iType = (pHit != NULL) ? pHit->iType : XUI_FLOW_HIT_NONE;
	iNode = (pHit != NULL) ? pHit->iNode : -1;
	iPort = (pHit != NULL) ? pHit->iPort : -1;
	iEdge = (pHit != NULL) ? pHit->iEdge : -1;
	if ( pData->iHoverType == iType && pData->iHoverNode == iNode && pData->iHoverPort == iPort && pData->iHoverEdge == iEdge ) {
		return XUI_OK;
	}
	pData->iHoverType = iType;
	pData->iHoverNode = iNode;
	pData->iHoverPort = iPort;
	pData->iHoverEdge = iEdge;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static uint32_t __xuiFlowGraphWidgetColor(uint32_t iColor, uint32_t iFallback)
{
	return (iColor != 0u) ? iColor : iFallback;
}

static int __xuiFlowGraphWidgetStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
{
	xui_style_property_t tProperty;
	int iRet;

	if ( pColor == NULL ) {
		return 0;
	}
	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_COLOR) ) {
		*pColor = tProperty.tValue.iColor;
		return 1;
	}
	return 0;
}

static void __xuiFlowGraphWidgetSaveColors(const xui_flow_graph_widget_data_t* pData, uint32_t arrColors[8])
{
	if ( (pData == NULL) || (arrColors == NULL) ) {
		return;
	}
	arrColors[0] = pData->iBackgroundColor;
	arrColors[1] = pData->iGridColor;
	arrColors[2] = pData->iNodeColor;
	arrColors[3] = pData->iSelectedNodeColor;
	arrColors[4] = pData->iNodeBorderColor;
	arrColors[5] = pData->iPortColor;
	arrColors[6] = pData->iEdgeColor;
	arrColors[7] = pData->iSelectedEdgeColor;
}

static void __xuiFlowGraphWidgetRestoreColors(xui_flow_graph_widget_data_t* pData, const uint32_t arrColors[8])
{
	if ( (pData == NULL) || (arrColors == NULL) ) {
		return;
	}
	pData->iBackgroundColor = arrColors[0];
	pData->iGridColor = arrColors[1];
	pData->iNodeColor = arrColors[2];
	pData->iSelectedNodeColor = arrColors[3];
	pData->iNodeBorderColor = arrColors[4];
	pData->iPortColor = arrColors[5];
	pData->iEdgeColor = arrColors[6];
	pData->iSelectedEdgeColor = arrColors[7];
}

static void __xuiFlowGraphWidgetApplyStyleColors(xui_widget pWidget, xui_flow_graph_widget_data_t* pData)
{
	if ( pData == NULL ) {
		return;
	}
	(void)__xuiFlowGraphWidgetStyleColor(pWidget, "flowgraph.background.color", &pData->iBackgroundColor);
	(void)__xuiFlowGraphWidgetStyleColor(pWidget, "flowgraph.grid.color", &pData->iGridColor);
	(void)__xuiFlowGraphWidgetStyleColor(pWidget, "flowgraph.node.color", &pData->iNodeColor);
	(void)__xuiFlowGraphWidgetStyleColor(pWidget, "flowgraph.node.selected_color", &pData->iSelectedNodeColor);
	(void)__xuiFlowGraphWidgetStyleColor(pWidget, "flowgraph.node.border_color", &pData->iNodeBorderColor);
	(void)__xuiFlowGraphWidgetStyleColor(pWidget, "flowgraph.port.color", &pData->iPortColor);
	(void)__xuiFlowGraphWidgetStyleColor(pWidget, "flowgraph.edge.color", &pData->iEdgeColor);
	(void)__xuiFlowGraphWidgetStyleColor(pWidget, "flowgraph.edge.selected_color", &pData->iSelectedEdgeColor);
}

static uint32_t __xuiFlowGraphWidgetNodeRunColor(int iRunState, uint32_t iFallback)
{
	switch ( iRunState ) {
	case XUI_WORKFLOW_NODE_RUN_QUEUED: return XUI_COLOR_RGBA(245, 180, 55, 255);
	case XUI_WORKFLOW_NODE_RUN_RUNNING: return XUI_COLOR_RGBA(42, 134, 230, 255);
	case XUI_WORKFLOW_NODE_RUN_SUCCESS: return XUI_COLOR_RGBA(30, 160, 95, 255);
	case XUI_WORKFLOW_NODE_RUN_FAILED: return XUI_COLOR_RGBA(214, 72, 72, 255);
	case XUI_WORKFLOW_NODE_RUN_SKIPPED: return XUI_COLOR_RGBA(140, 150, 164, 210);
	case XUI_WORKFLOW_NODE_RUN_WARNING: return XUI_COLOR_RGBA(225, 140, 36, 255);
	default: return iFallback;
	}
}

static uint32_t __xuiFlowGraphWidgetEdgeRunColor(int iRunState, uint32_t iFallback)
{
	switch ( iRunState ) {
	case XUI_WORKFLOW_EDGE_RUN_ACTIVE: return XUI_COLOR_RGBA(42, 134, 230, 245);
	case XUI_WORKFLOW_EDGE_RUN_TAKEN: return XUI_COLOR_RGBA(30, 160, 95, 245);
	case XUI_WORKFLOW_EDGE_RUN_SKIPPED: return XUI_COLOR_RGBA(140, 150, 164, 180);
	case XUI_WORKFLOW_EDGE_RUN_INVALID: return XUI_COLOR_RGBA(214, 72, 72, 230);
	default: return iFallback;
	}
}

static char* __xuiFlowGraphWidgetCopyString(const char* sText)
{
	char* sCopy;
	size_t iSize;

	if ( sText == NULL ) {
		return NULL;
	}
	iSize = strlen(sText);
	sCopy = (char*)xrtMalloc(iSize + 1u);
	if ( sCopy == NULL ) {
		return NULL;
	}
	memcpy(sCopy, sText, iSize + 1u);
	return sCopy;
}

static int __xuiFlowGraphWidgetRectNear(xui_rect_t tA, xui_rect_t tB)
{
	return (fabsf(tA.fX - tB.fX) < 0.01f) && (fabsf(tA.fY - tB.fY) < 0.01f) &&
	       (fabsf(tA.fW - tB.fW) < 0.01f) && (fabsf(tA.fH - tB.fH) < 0.01f);
}

static void __xuiFlowGraphWidgetClearNodeBuckets(xui_flow_graph_widget_data_t* pData)
{
	xui_flow_graph_node_bucket_t* pBucket;
	uint32_t i;

	if ( pData == NULL ) {
		return;
	}
	for ( i = 1u; i <= pData->arrNodeBuckets.Count; ++i ) {
		pBucket = (xui_flow_graph_node_bucket_t*)xrtArrayGet_Unsafe(&pData->arrNodeBuckets, i);
		xrtArrayUnit(&pBucket->arrNodes);
	}
	xrtArrayUnit(&pData->arrNodeBuckets);
	xrtArrayInit(&pData->arrNodeBuckets, sizeof(xui_flow_graph_node_bucket_t), XRT_OBJMODE_LOCAL);
	xrtArrayUnit(&pData->arrVisibleNodes);
	xrtArrayInit(&pData->arrVisibleNodes, sizeof(int), XRT_OBJMODE_LOCAL);
	pData->iNodeBucketRevision = 0u;
	pData->iNodeBucketCols = 0;
	pData->iNodeBucketRows = 0;
	memset(&pData->tNodeBucketContent, 0, sizeof(pData->tNodeBucketContent));
}

static void __xuiFlowGraphWidgetClearEdgeBuckets(xui_flow_graph_widget_data_t* pData)
{
	xui_flow_graph_edge_bucket_t* pBucket;
	uint32_t i;

	if ( pData == NULL ) {
		return;
	}
	for ( i = 1u; i <= pData->arrEdgeBuckets.Count; ++i ) {
		pBucket = (xui_flow_graph_edge_bucket_t*)xrtArrayGet_Unsafe(&pData->arrEdgeBuckets, i);
		xrtArrayUnit(&pBucket->arrEdges);
	}
	xrtArrayUnit(&pData->arrEdgeBuckets);
	xrtArrayInit(&pData->arrEdgeBuckets, sizeof(xui_flow_graph_edge_bucket_t), XRT_OBJMODE_LOCAL);
	xrtArrayUnit(&pData->arrVisibleEdges);
	xrtArrayInit(&pData->arrVisibleEdges, sizeof(int), XRT_OBJMODE_LOCAL);
	pData->iEdgeBucketRevision = 0u;
	pData->iEdgeBucketCols = 0;
	pData->iEdgeBucketRows = 0;
	memset(&pData->tEdgeBucketContent, 0, sizeof(pData->tEdgeBucketContent));
}

static void __xuiFlowGraphWidgetClearGridCache(xui_flow_graph_widget_data_t* pData)
{
	if ( pData == NULL ) {
		return;
	}
	xrtArrayUnit(&pData->arrGridLines);
	xrtArrayInit(&pData->arrGridLines, sizeof(xui_flow_graph_grid_line_t), XRT_OBJMODE_LOCAL);
	pData->iGridZoomBucket = 0;
	pData->fGridPanX = 0.0f;
	pData->fGridPanY = 0.0f;
	pData->fGridStep = 0.0f;
	memset(&pData->tGridCacheContent, 0, sizeof(pData->tGridCacheContent));
}

static void __xuiFlowGraphWidgetClearNodeCardStates(xui_flow_graph_widget_data_t* pData)
{
	if ( pData == NULL ) {
		return;
	}
	xrtArrayUnit(&pData->arrNodeCardStates);
	xrtArrayInit(&pData->arrNodeCardStates, sizeof(xui_flow_graph_node_card_state_t), XRT_OBJMODE_LOCAL);
	pData->iNodeCardRevision = 0u;
	pData->iNodeCardHoverType = XUI_FLOW_HIT_NONE;
	pData->iNodeCardHoverNode = -1;
	pData->iNodeCardEnabled = 0;
}

static void __xuiFlowGraphWidgetClearEdgeLayerStates(xui_flow_graph_widget_data_t* pData)
{
	if ( pData == NULL ) {
		return;
	}
	xrtArrayUnit(&pData->arrEdgeLayerStates);
	xrtArrayInit(&pData->arrEdgeLayerStates, sizeof(xui_flow_graph_edge_layer_state_t), XRT_OBJMODE_LOCAL);
	pData->iEdgeLayerRevision = 0u;
	pData->iEdgeLayerHoverType = XUI_FLOW_HIT_NONE;
	pData->iEdgeLayerHoverEdge = -1;
	memset(&pData->tEdgeLayerContent, 0, sizeof(pData->tEdgeLayerContent));
}

static int __xuiFlowGraphWidgetAppendInt(xarray pArray, int iValue)
{
	int* pItem;
	uint32_t iPos;

	if ( pArray == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iPos = xrtArrayAppend(pArray, 1u);
	if ( iPos == 0u ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pItem = (int*)xrtArrayGet_Unsafe(pArray, iPos);
	*pItem = iValue;
	return XUI_OK;
}

static int __xuiFlowGraphWidgetAppendGridLine(xarray pArray, float fPos, int bVertical)
{
	xui_flow_graph_grid_line_t* pLine;
	uint32_t iPos;

	if ( pArray == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iPos = xrtArrayAppend(pArray, 1u);
	if ( iPos == 0u ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pLine = (xui_flow_graph_grid_line_t*)xrtArrayGet_Unsafe(pArray, iPos);
	pLine->fPos = fPos;
	pLine->bVertical = bVertical ? 1 : 0;
	return XUI_OK;
}

static xui_rect_t __xuiFlowGraphWidgetInflateRect(xui_rect_t tRect, float fAmount)
{
	tRect.fX -= fAmount;
	tRect.fY -= fAmount;
	tRect.fW += fAmount * 2.0f;
	tRect.fH += fAmount * 2.0f;
	return tRect;
}

static void __xuiFlowGraphWidgetClearDrag(xui_flow_graph_widget_data_t* pData)
{
	int i;

	if ( pData == NULL ) {
		return;
	}
	if ( pData->arrDragNodes != NULL ) {
		for ( i = 0; i < pData->iDragNodeCount; ++i ) {
			xrtFree(pData->arrDragNodes[i].sId);
		}
		xrtFree(pData->arrDragNodes);
	}
	pData->arrDragNodes = NULL;
	pData->iDragNodeCount = 0;
	xrtFree(pData->sDragNodeId);
	xrtFree(pData->sDragPortId);
	pData->sDragNodeId = NULL;
	pData->sDragPortId = NULL;
	pData->bDraggingNode = 0;
	pData->bDraggingConnection = 0;
	pData->bDraggingMarquee = 0;
	pData->iDragPortKind = 0;
	pData->iDragPortDirection = 0;
	pData->iMarqueeModifiers = 0u;
	pData->fDragStartX = 0.0f;
	pData->fDragStartY = 0.0f;
	pData->fDragCurrentX = 0.0f;
	pData->fDragCurrentY = 0.0f;
	pData->bDragConnectionValid = 0;
}

static int __xuiFlowGraphWidgetZoomBucket(float fZoom)
{
	if ( fZoom <= 0.001f ) {
		fZoom = 0.001f;
	}
	return (int)floorf(fZoom * XUI_FLOW_GRID_ZOOM_BUCKET_SCALE + 0.5f);
}

static int __xuiFlowGraphWidgetRebuildGridCache(xui_flow_graph_widget_data_t* pData, xui_rect_t tRect, const xui_flow_viewport_t* pViewport)
{
	float fStep;
	float fStartX;
	float fStartY;
	float fX;
	float fY;
	int iZoomBucket;
	int iRet;

	if ( (pData == NULL) || (pViewport == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XUI_OK;
	}
	iZoomBucket = __xuiFlowGraphWidgetZoomBucket(pViewport->fZoom);
	fStep = 32.0f * ((pViewport->fZoom > 0.0f) ? pViewport->fZoom : 1.0f);
	if ( fStep < 8.0f ) {
		fStep = 8.0f;
	}
	fStartX = tRect.fX + fmodf(pViewport->fPanX * pViewport->fZoom, fStep);
	fStartY = tRect.fY + fmodf(pViewport->fPanY * pViewport->fZoom, fStep);
	while ( fStartX > tRect.fX ) fStartX -= fStep;
	while ( fStartY > tRect.fY ) fStartY -= fStep;
	if ( pData->iGridZoomBucket == iZoomBucket &&
	     __xuiFlowGraphWidgetRectNear(pData->tGridCacheContent, tRect) &&
	     fabsf(pData->fGridPanX - fStartX) < 0.01f &&
	     fabsf(pData->fGridPanY - fStartY) < 0.01f &&
	     fabsf(pData->fGridStep - fStep) < 0.01f &&
	     pData->arrGridLines.Count > 0u ) {
		return XUI_OK;
	}
	__xuiFlowGraphWidgetClearGridCache(pData);
	pData->iGridZoomBucket = iZoomBucket;
	pData->tGridCacheContent = tRect;
	pData->fGridPanX = fStartX;
	pData->fGridPanY = fStartY;
	pData->fGridStep = fStep;
	for ( fX = fStartX; fX < tRect.fX + tRect.fW; fX += fStep ) {
		iRet = __xuiFlowGraphWidgetAppendGridLine(&pData->arrGridLines, fX, 1);
		if ( iRet != XUI_OK ) return iRet;
	}
	for ( fY = fStartY; fY < tRect.fY + tRect.fH; fY += fStep ) {
		iRet = __xuiFlowGraphWidgetAppendGridLine(&pData->arrGridLines, fY, 0);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiFlowGraphWidgetDrawGrid(xui_proxy pProxy, xui_draw_context pDraw, xui_flow_graph_widget_data_t* pData, xui_rect_t tRect, const xui_flow_viewport_t* pViewport, uint32_t iColor)
{
	xui_flow_graph_grid_line_t* pLine;
	uint32_t i;
	int iRet;

	if ( (pProxy->drawRectFill == NULL) || (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XUI_OK;
	}
	iRet = __xuiFlowGraphWidgetRebuildGridCache(pData, tRect, pViewport);
	if ( iRet != XUI_OK ) return iRet;
	for ( i = 1u; i <= pData->arrGridLines.Count; ++i ) {
		pLine = (xui_flow_graph_grid_line_t*)xrtArrayGet_Unsafe(&pData->arrGridLines, i);
		if ( pLine->bVertical ) {
			iRet = pProxy->drawRectFill(pProxy, pDraw, (xui_rect_t){pLine->fPos, tRect.fY, 1.0f, tRect.fH}, iColor);
		} else {
			iRet = pProxy->drawRectFill(pProxy, pDraw, (xui_rect_t){tRect.fX, pLine->fPos, tRect.fW, 1.0f}, iColor);
		}
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static xui_rect_t __xuiFlowGraphWidgetNodeRect(xui_flow_graph pGraph, const xui_flow_node_info_t* pNode, xui_rect_t tContent)
{
	xui_rect_t tRect;
	float fX;
	float fY;

	xuiFlowGraphWorldToScreen(pGraph, pNode->fX, pNode->fY, &fX, &fY);
	tRect.fX = tContent.fX + fX;
	tRect.fY = tContent.fY + fY;
	tRect.fW = pNode->fW;
	tRect.fH = pNode->fH;
	if ( tRect.fW <= 0.0f ) tRect.fW = 160.0f;
	if ( tRect.fH <= 0.0f ) tRect.fH = 88.0f;
	return tRect;
}

static int __xuiFlowGraphWidgetPointInRect(xui_rect_t tRect, float fX, float fY)
{
	return (fX >= tRect.fX) && (fY >= tRect.fY) && (fX <= tRect.fX + tRect.fW) && (fY <= tRect.fY + tRect.fH);
}

static xui_rect_t __xuiFlowGraphWidgetRectFromPoints(float fX0, float fY0, float fX1, float fY1)
{
	xui_rect_t tRect;

	tRect.fX = (fX0 < fX1) ? fX0 : fX1;
	tRect.fY = (fY0 < fY1) ? fY0 : fY1;
	tRect.fW = fabsf(fX1 - fX0);
	tRect.fH = fabsf(fY1 - fY0);
	return tRect;
}

static int __xuiFlowGraphWidgetRouteStyle(int iRouteStyle)
{
	if ( iRouteStyle == XUI_FLOW_ROUTE_STRAIGHT || iRouteStyle == XUI_FLOW_ROUTE_ORTHOGONAL || iRouteStyle == XUI_FLOW_ROUTE_BEZIER ) {
		return iRouteStyle;
	}
	return XUI_FLOW_ROUTE_ORTHOGONAL;
}

static void __xuiFlowGraphWidgetRouteAppend(xui_flow_graph_route_t* pRoute, float fX, float fY)
{
	int iLast;

	if ( (pRoute == NULL) || (pRoute->iCount >= (int)(sizeof(pRoute->arrX) / sizeof(pRoute->arrX[0]))) ) {
		return;
	}
	iLast = pRoute->iCount - 1;
	if ( iLast >= 0 && fabsf(pRoute->arrX[iLast] - fX) < 0.01f && fabsf(pRoute->arrY[iLast] - fY) < 0.01f ) {
		return;
	}
	pRoute->arrX[pRoute->iCount] = fX;
	pRoute->arrY[pRoute->iCount] = fY;
	pRoute->iCount++;
}

static float __xuiFlowGraphWidgetRouteDefaultOffset(float fOffset)
{
	return (fabsf(fOffset) > 0.001f) ? fabsf(fOffset) : 32.0f;
}

static void __xuiFlowGraphWidgetBuildRoute(xui_flow_graph_route_t* pRoute, float fX0, float fY0, float fX1, float fY1, int iRouteStyle, float fRouteBias, float fSourceOffset, float fTargetOffset)
{
	float fDir;
	float fMidX;
	float fDeltaX;
	float fT;
	float fU;
	float fCX0;
	float fCX1;
	float fX;
	float fY;
	int i;

	if ( pRoute == NULL ) {
		return;
	}
	memset(pRoute, 0, sizeof(*pRoute));
	iRouteStyle = __xuiFlowGraphWidgetRouteStyle(iRouteStyle);
	if ( iRouteStyle == XUI_FLOW_ROUTE_STRAIGHT ) {
		__xuiFlowGraphWidgetRouteAppend(pRoute, fX0, fY0);
		__xuiFlowGraphWidgetRouteAppend(pRoute, fX1, fY1);
		return;
	}
	fDir = (fX1 >= fX0) ? 1.0f : -1.0f;
	fSourceOffset = __xuiFlowGraphWidgetRouteDefaultOffset(fSourceOffset);
	fTargetOffset = __xuiFlowGraphWidgetRouteDefaultOffset(fTargetOffset);
	if ( fRouteBias <= 0.001f ) fRouteBias = 0.5f;
	if ( fRouteBias > 0.999f ) fRouteBias = 0.999f;
	if ( iRouteStyle == XUI_FLOW_ROUTE_BEZIER ) {
		fCX0 = fX0 + fDir * fSourceOffset;
		fCX1 = fX1 - fDir * fTargetOffset;
		for ( i = 0; i <= 24; ++i ) {
			fT = (float)i / 24.0f;
			fU = 1.0f - fT;
			fX = fU * fU * fU * fX0 + 3.0f * fU * fU * fT * fCX0 + 3.0f * fU * fT * fT * fCX1 + fT * fT * fT * fX1;
			fY = fU * fU * fU * fY0 + 3.0f * fU * fU * fT * fY0 + 3.0f * fU * fT * fT * fY1 + fT * fT * fT * fY1;
			__xuiFlowGraphWidgetRouteAppend(pRoute, fX, fY);
		}
		return;
	}
	fDeltaX = fX1 - fX0;
	if ( fabsf(fDeltaX) >= (fSourceOffset + fTargetOffset + 8.0f) ) {
		fMidX = fX0 + fDeltaX * fRouteBias;
	} else {
		fMidX = fX0 + fDir * fSourceOffset;
	}
	__xuiFlowGraphWidgetRouteAppend(pRoute, fX0, fY0);
	__xuiFlowGraphWidgetRouteAppend(pRoute, fMidX, fY0);
	__xuiFlowGraphWidgetRouteAppend(pRoute, fMidX, fY1);
	__xuiFlowGraphWidgetRouteAppend(pRoute, fX1, fY1);
}

static xui_rect_t __xuiFlowGraphWidgetRouteBounds(const xui_flow_graph_route_t* pRoute)
{
	xui_rect_t tRect;
	float fMinX;
	float fMinY;
	float fMaxX;
	float fMaxY;
	int i;

	memset(&tRect, 0, sizeof(tRect));
	if ( (pRoute == NULL) || (pRoute->iCount <= 0) ) {
		return tRect;
	}
	fMinX = pRoute->arrX[0];
	fMaxX = pRoute->arrX[0];
	fMinY = pRoute->arrY[0];
	fMaxY = pRoute->arrY[0];
	for ( i = 1; i < pRoute->iCount; ++i ) {
		if ( pRoute->arrX[i] < fMinX ) fMinX = pRoute->arrX[i];
		if ( pRoute->arrX[i] > fMaxX ) fMaxX = pRoute->arrX[i];
		if ( pRoute->arrY[i] < fMinY ) fMinY = pRoute->arrY[i];
		if ( pRoute->arrY[i] > fMaxY ) fMaxY = pRoute->arrY[i];
	}
	tRect.fX = fMinX;
	tRect.fY = fMinY;
	tRect.fW = fMaxX - fMinX;
	tRect.fH = fMaxY - fMinY;
	return tRect;
}

static int __xuiFlowGraphWidgetRectsIntersect(xui_rect_t tA, xui_rect_t tB)
{
	return (tA.fX <= tB.fX + tB.fW) && (tA.fX + tA.fW >= tB.fX) &&
	       (tA.fY <= tB.fY + tB.fH) && (tA.fY + tA.fH >= tB.fY);
}

static int __xuiFlowGraphWidgetClampInt(int iValue, int iMin, int iMax)
{
	if ( iValue < iMin ) return iMin;
	if ( iValue > iMax ) return iMax;
	return iValue;
}

static int __xuiFlowGraphWidgetBucketIndex(const xui_flow_graph_widget_data_t* pData, float fX, float fY)
{
	int iCol;
	int iRow;

	if ( (pData == NULL) || (pData->iNodeBucketCols <= 0) || (pData->iNodeBucketRows <= 0) ) {
		return -1;
	}
	iCol = (int)floorf((fX - pData->tNodeBucketContent.fX) / XUI_FLOW_NODE_BUCKET_SIZE);
	iRow = (int)floorf((fY - pData->tNodeBucketContent.fY) / XUI_FLOW_NODE_BUCKET_SIZE);
	iCol = __xuiFlowGraphWidgetClampInt(iCol, 0, pData->iNodeBucketCols - 1);
	iRow = __xuiFlowGraphWidgetClampInt(iRow, 0, pData->iNodeBucketRows - 1);
	return iRow * pData->iNodeBucketCols + iCol;
}

static int __xuiFlowGraphWidgetEdgeBucketIndex(const xui_flow_graph_widget_data_t* pData, float fX, float fY)
{
	int iCol;
	int iRow;

	if ( (pData == NULL) || (pData->iEdgeBucketCols <= 0) || (pData->iEdgeBucketRows <= 0) ) {
		return -1;
	}
	iCol = (int)floorf((fX - pData->tEdgeBucketContent.fX) / XUI_FLOW_NODE_BUCKET_SIZE);
	iRow = (int)floorf((fY - pData->tEdgeBucketContent.fY) / XUI_FLOW_NODE_BUCKET_SIZE);
	iCol = __xuiFlowGraphWidgetClampInt(iCol, 0, pData->iEdgeBucketCols - 1);
	iRow = __xuiFlowGraphWidgetClampInt(iRow, 0, pData->iEdgeBucketRows - 1);
	return iRow * pData->iEdgeBucketCols + iCol;
}

static int __xuiFlowGraphWidgetRebuildNodeBuckets(xui_flow_graph_widget_data_t* pData, xui_rect_t tContent)
{
	xui_flow_graph_node_bucket_t* pBucket;
	xui_flow_node_info_t tNode;
	xui_rect_t tNodeRect;
	int iNodeCount;
	int iBucketCount;
	int iNode;
	int iCol0;
	int iCol1;
	int iRow0;
	int iRow1;
	int iCol;
	int iRow;
	int iRet;
	uint32_t iPos;

	if ( (pData == NULL) || (pData->pGraph == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->iNodeBucketRevision == xuiFlowGraphGetRevision(pData->pGraph) &&
	     __xuiFlowGraphWidgetRectNear(pData->tNodeBucketContent, tContent) &&
	     pData->arrNodeBuckets.Count > 0u ) {
		return XUI_OK;
	}
	__xuiFlowGraphWidgetClearNodeBuckets(pData);
	pData->tNodeBucketContent = tContent;
	pData->iNodeBucketRevision = xuiFlowGraphGetRevision(pData->pGraph);
	pData->iNodeBucketCols = (int)ceilf(tContent.fW / XUI_FLOW_NODE_BUCKET_SIZE);
	pData->iNodeBucketRows = (int)ceilf(tContent.fH / XUI_FLOW_NODE_BUCKET_SIZE);
	if ( pData->iNodeBucketCols < 1 ) pData->iNodeBucketCols = 1;
	if ( pData->iNodeBucketRows < 1 ) pData->iNodeBucketRows = 1;
	iBucketCount = pData->iNodeBucketCols * pData->iNodeBucketRows;
	iPos = xrtArrayAppend(&pData->arrNodeBuckets, (uint32_t)iBucketCount);
	if ( iPos == 0u ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	for ( iNode = 0; iNode < iBucketCount; ++iNode ) {
		pBucket = (xui_flow_graph_node_bucket_t*)xrtArrayGet_Unsafe(&pData->arrNodeBuckets, (uint32_t)iNode + 1u);
		memset(pBucket, 0, sizeof(*pBucket));
		xrtArrayInit(&pBucket->arrNodes, sizeof(int), XRT_OBJMODE_LOCAL);
	}
	iNodeCount = xuiFlowGraphGetNodeCount(pData->pGraph);
	for ( iNode = 0; iNode < iNodeCount; ++iNode ) {
		if ( xuiFlowGraphGetNode(pData->pGraph, iNode, &tNode) != XUI_OK ) {
			continue;
		}
		tNodeRect = __xuiFlowGraphWidgetInflateRect(__xuiFlowGraphWidgetNodeRect(pData->pGraph, &tNode, tContent), 8.0f);
		if ( !__xuiFlowGraphWidgetRectsIntersect(tNodeRect, tContent) ) {
			continue;
		}
		iRet = __xuiFlowGraphWidgetAppendInt(&pData->arrVisibleNodes, iNode);
		if ( iRet != XUI_OK ) return iRet;
		iCol0 = __xuiFlowGraphWidgetClampInt((int)floorf((tNodeRect.fX - tContent.fX) / XUI_FLOW_NODE_BUCKET_SIZE), 0, pData->iNodeBucketCols - 1);
		iCol1 = __xuiFlowGraphWidgetClampInt((int)floorf((tNodeRect.fX + tNodeRect.fW - tContent.fX) / XUI_FLOW_NODE_BUCKET_SIZE), 0, pData->iNodeBucketCols - 1);
		iRow0 = __xuiFlowGraphWidgetClampInt((int)floorf((tNodeRect.fY - tContent.fY) / XUI_FLOW_NODE_BUCKET_SIZE), 0, pData->iNodeBucketRows - 1);
		iRow1 = __xuiFlowGraphWidgetClampInt((int)floorf((tNodeRect.fY + tNodeRect.fH - tContent.fY) / XUI_FLOW_NODE_BUCKET_SIZE), 0, pData->iNodeBucketRows - 1);
		for ( iRow = iRow0; iRow <= iRow1; ++iRow ) {
			for ( iCol = iCol0; iCol <= iCol1; ++iCol ) {
				pBucket = (xui_flow_graph_node_bucket_t*)xrtArrayGet_Unsafe(&pData->arrNodeBuckets, (uint32_t)(iRow * pData->iNodeBucketCols + iCol) + 1u);
				iRet = __xuiFlowGraphWidgetAppendInt(&pBucket->arrNodes, iNode);
				if ( iRet != XUI_OK ) return iRet;
			}
		}
	}
	return XUI_OK;
}

static int __xuiFlowGraphWidgetRebuildEdgeBuckets(xui_flow_graph_widget_data_t* pData, xui_rect_t tContent)
{
	xui_flow_graph_edge_bucket_t* pBucket;
	xui_flow_edge_info_t tEdge;
	xui_flow_graph_route_t tRoute;
	xui_rect_t tEdgeRect;
	float fX0;
	float fY0;
	float fX1;
	float fY1;
	int iEdgeCount;
	int iBucketCount;
	int iEdge;
	int iCol0;
	int iCol1;
	int iRow0;
	int iRow1;
	int iCol;
	int iRow;
	int iRet;
	uint32_t iPos;

	if ( (pData == NULL) || (pData->pGraph == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->iEdgeBucketRevision == xuiFlowGraphGetRevision(pData->pGraph) &&
	     __xuiFlowGraphWidgetRectNear(pData->tEdgeBucketContent, tContent) &&
	     pData->arrEdgeBuckets.Count > 0u ) {
		return XUI_OK;
	}
	__xuiFlowGraphWidgetClearEdgeBuckets(pData);
	pData->tEdgeBucketContent = tContent;
	pData->iEdgeBucketRevision = xuiFlowGraphGetRevision(pData->pGraph);
	pData->iEdgeBucketCols = (int)ceilf(tContent.fW / XUI_FLOW_NODE_BUCKET_SIZE);
	pData->iEdgeBucketRows = (int)ceilf(tContent.fH / XUI_FLOW_NODE_BUCKET_SIZE);
	if ( pData->iEdgeBucketCols < 1 ) pData->iEdgeBucketCols = 1;
	if ( pData->iEdgeBucketRows < 1 ) pData->iEdgeBucketRows = 1;
	iBucketCount = pData->iEdgeBucketCols * pData->iEdgeBucketRows;
	iPos = xrtArrayAppend(&pData->arrEdgeBuckets, (uint32_t)iBucketCount);
	if ( iPos == 0u ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	for ( iEdge = 0; iEdge < iBucketCount; ++iEdge ) {
		pBucket = (xui_flow_graph_edge_bucket_t*)xrtArrayGet_Unsafe(&pData->arrEdgeBuckets, (uint32_t)iEdge + 1u);
		memset(pBucket, 0, sizeof(*pBucket));
		xrtArrayInit(&pBucket->arrEdges, sizeof(int), XRT_OBJMODE_LOCAL);
	}
	iEdgeCount = xuiFlowGraphGetEdgeCount(pData->pGraph);
	for ( iEdge = 0; iEdge < iEdgeCount; ++iEdge ) {
		if ( xuiFlowGraphGetEdge(pData->pGraph, iEdge, &tEdge) != XUI_OK ) {
			continue;
		}
		__xuiFlowGraphWidgetPortCenter(pData->pGraph, tEdge.iFromNode, tEdge.iFromPort, tContent, &fX0, &fY0);
		__xuiFlowGraphWidgetPortCenter(pData->pGraph, tEdge.iToNode, tEdge.iToPort, tContent, &fX1, &fY1);
		__xuiFlowGraphWidgetBuildRoute(&tRoute, fX0, fY0, fX1, fY1, tEdge.iRouteStyle, tEdge.fRouteBias, tEdge.fRouteSourceOffset, tEdge.fRouteTargetOffset);
		tEdgeRect = __xuiFlowGraphWidgetInflateRect(__xuiFlowGraphWidgetRouteBounds(&tRoute), 8.0f);
		if ( !__xuiFlowGraphWidgetRectsIntersect(tEdgeRect, tContent) ) {
			continue;
		}
		iRet = __xuiFlowGraphWidgetAppendInt(&pData->arrVisibleEdges, iEdge);
		if ( iRet != XUI_OK ) return iRet;
		iCol0 = __xuiFlowGraphWidgetClampInt((int)floorf((tEdgeRect.fX - tContent.fX) / XUI_FLOW_NODE_BUCKET_SIZE), 0, pData->iEdgeBucketCols - 1);
		iCol1 = __xuiFlowGraphWidgetClampInt((int)floorf((tEdgeRect.fX + tEdgeRect.fW - tContent.fX) / XUI_FLOW_NODE_BUCKET_SIZE), 0, pData->iEdgeBucketCols - 1);
		iRow0 = __xuiFlowGraphWidgetClampInt((int)floorf((tEdgeRect.fY - tContent.fY) / XUI_FLOW_NODE_BUCKET_SIZE), 0, pData->iEdgeBucketRows - 1);
		iRow1 = __xuiFlowGraphWidgetClampInt((int)floorf((tEdgeRect.fY + tEdgeRect.fH - tContent.fY) / XUI_FLOW_NODE_BUCKET_SIZE), 0, pData->iEdgeBucketRows - 1);
		for ( iRow = iRow0; iRow <= iRow1; ++iRow ) {
			for ( iCol = iCol0; iCol <= iCol1; ++iCol ) {
				pBucket = (xui_flow_graph_edge_bucket_t*)xrtArrayGet_Unsafe(&pData->arrEdgeBuckets, (uint32_t)(iRow * pData->iEdgeBucketCols + iCol) + 1u);
				iRet = __xuiFlowGraphWidgetAppendInt(&pBucket->arrEdges, iEdge);
				if ( iRet != XUI_OK ) return iRet;
			}
		}
	}
	return XUI_OK;
}

static int __xuiFlowGraphWidgetRebuildNodeCardStates(xui_flow_graph_widget_data_t* pData, int bEnabled)
{
	xui_flow_graph_node_card_state_t* pState;
	xui_flow_node_info_t tNode;
	int* pNodeIndex;
	int i;
	int iNode;
	int iDiagCount;
	uint32_t iRevision;
	uint32_t iPos;
	uint32_t iFill;
	uint32_t iBorder;

	if ( (pData == NULL) || (pData->pGraph == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRevision = xuiFlowGraphGetRevision(pData->pGraph);
	if ( pData->iNodeCardRevision == iRevision &&
	     pData->iNodeCardHoverType == pData->iHoverType &&
	     pData->iNodeCardHoverNode == pData->iHoverNode &&
	     pData->iNodeCardEnabled == (bEnabled ? 1 : 0) &&
	     pData->arrNodeCardStates.Count == pData->arrVisibleNodes.Count ) {
		return XUI_OK;
	}
	__xuiFlowGraphWidgetClearNodeCardStates(pData);
	pData->iNodeCardRevision = iRevision;
	pData->iNodeCardHoverType = pData->iHoverType;
	pData->iNodeCardHoverNode = pData->iHoverNode;
	pData->iNodeCardEnabled = bEnabled ? 1 : 0;
	for ( i = 1; i <= (int)pData->arrVisibleNodes.Count; ++i ) {
		pNodeIndex = (int*)xrtArrayGet_Unsafe(&pData->arrVisibleNodes, (uint32_t)i);
		if ( pNodeIndex == NULL ) {
			continue;
		}
		iNode = *pNodeIndex;
		if ( xuiFlowGraphGetNode(pData->pGraph, iNode, &tNode) != XUI_OK ) {
			continue;
		}
		iPos = xrtArrayAppend(&pData->arrNodeCardStates, 1u);
		if ( iPos == 0u ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		pState = (xui_flow_graph_node_card_state_t*)xrtArrayGet_Unsafe(&pData->arrNodeCardStates, iPos);
		memset(pState, 0, sizeof(*pState));
		pState->iNode = iNode;
		iFill = xuiFlowGraphIsNodeSelected(pData->pGraph, tNode.sId) ? pData->iSelectedNodeColor : pData->iNodeColor;
		if ( pData->iHoverType == XUI_FLOW_HIT_NODE && pData->iHoverNode == iNode ) {
			iFill = XUI_COLOR_RGBA(238, 247, 255, 255);
		}
		if ( tNode.iRunState == XUI_WORKFLOW_NODE_RUN_RUNNING ) {
			iFill = XUI_COLOR_RGBA(232, 244, 255, 255);
		} else if ( tNode.iRunState == XUI_WORKFLOW_NODE_RUN_SUCCESS ) {
			iFill = XUI_COLOR_RGBA(232, 250, 240, 255);
		} else if ( tNode.iRunState == XUI_WORKFLOW_NODE_RUN_FAILED ) {
			iFill = XUI_COLOR_RGBA(255, 238, 238, 255);
		} else if ( tNode.iRunState == XUI_WORKFLOW_NODE_RUN_WARNING ) {
			iFill = XUI_COLOR_RGBA(255, 246, 232, 255);
		}
		iDiagCount = xuiFlowGraphGetNodeDiagnosticCount(pData->pGraph, tNode.sId);
		if ( iDiagCount > 0 ) {
			iBorder = XUI_COLOR_RGBA(214, 72, 72, 240);
		} else {
			iBorder = __xuiFlowGraphWidgetNodeRunColor(tNode.iRunState, bEnabled ? pData->iNodeBorderColor : XUI_COLOR_RGBA(140, 150, 164, 120));
		}
		if ( !bEnabled ) {
			iFill = XUI_COLOR_RGBA(246, 248, 251, 190);
		}
		pState->iFill = iFill;
		pState->iBorder = iBorder;
		pState->iDiagCount = iDiagCount;
		pState->iRunBadgeColor = XUI_COLOR_RGBA(92, 105, 121, 235);
		pState->iRunBadgeTextColor = XUI_COLOR_RGBA(255, 255, 255, 255);
		if ( tNode.iRunState == XUI_WORKFLOW_NODE_RUN_SUCCESS ) {
			pState->bHasRunBadge = 1;
			pState->iRunBadgeColor = XUI_COLOR_RGBA(30, 160, 95, 235);
			snprintf(pState->sRunBadge, sizeof(pState->sRunBadge), "OK");
		} else if ( tNode.iRunState == XUI_WORKFLOW_NODE_RUN_FAILED ) {
			pState->bHasRunBadge = 1;
			pState->iRunBadgeColor = XUI_COLOR_RGBA(214, 72, 72, 235);
			snprintf(pState->sRunBadge, sizeof(pState->sRunBadge), "!");
		} else if ( tNode.iRunState == XUI_WORKFLOW_NODE_RUN_SKIPPED ) {
			pState->bHasRunBadge = 1;
			pState->iRunBadgeColor = XUI_COLOR_RGBA(130, 140, 154, 220);
			snprintf(pState->sRunBadge, sizeof(pState->sRunBadge), "--");
		} else if ( tNode.iRunState == XUI_WORKFLOW_NODE_RUN_WARNING ) {
			pState->bHasRunBadge = 1;
			pState->iRunBadgeColor = XUI_COLOR_RGBA(232, 160, 36, 235);
			pState->iRunBadgeTextColor = XUI_COLOR_RGBA(30, 36, 44, 255);
			snprintf(pState->sRunBadge, sizeof(pState->sRunBadge), "!");
		}
	}
	return XUI_OK;
}

static int __xuiFlowGraphWidgetRebuildEdgeLayerStates(xui_flow_graph_widget_data_t* pData, xui_rect_t tContent)
{
	xui_flow_graph_edge_layer_state_t* pState;
	xui_flow_edge_info_t tEdge;
	int* pEdgeIndex;
	int i;
	int iEdge;
	uint32_t iRevision;
	uint32_t iColor;
	uint32_t iPos;

	if ( (pData == NULL) || (pData->pGraph == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRevision = xuiFlowGraphGetRevision(pData->pGraph);
	if ( pData->iEdgeLayerRevision == iRevision &&
	     __xuiFlowGraphWidgetRectNear(pData->tEdgeLayerContent, tContent) &&
	     pData->iEdgeLayerHoverType == pData->iHoverType &&
	     pData->iEdgeLayerHoverEdge == pData->iHoverEdge &&
	     pData->arrEdgeLayerStates.Count == pData->arrVisibleEdges.Count ) {
		return XUI_OK;
	}
	__xuiFlowGraphWidgetClearEdgeLayerStates(pData);
	pData->iEdgeLayerRevision = iRevision;
	pData->tEdgeLayerContent = tContent;
	pData->iEdgeLayerHoverType = pData->iHoverType;
	pData->iEdgeLayerHoverEdge = pData->iHoverEdge;
	for ( i = 1; i <= (int)pData->arrVisibleEdges.Count; ++i ) {
		pEdgeIndex = (int*)xrtArrayGet_Unsafe(&pData->arrVisibleEdges, (uint32_t)i);
		if ( pEdgeIndex == NULL ) {
			continue;
		}
		iEdge = *pEdgeIndex;
		if ( xuiFlowGraphGetEdge(pData->pGraph, iEdge, &tEdge) != XUI_OK ) {
			continue;
		}
		iPos = xrtArrayAppend(&pData->arrEdgeLayerStates, 1u);
		if ( iPos == 0u ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		pState = (xui_flow_graph_edge_layer_state_t*)xrtArrayGet_Unsafe(&pData->arrEdgeLayerStates, iPos);
		memset(pState, 0, sizeof(*pState));
		pState->iEdge = iEdge;
		__xuiFlowGraphWidgetPortCenter(pData->pGraph, tEdge.iFromNode, tEdge.iFromPort, tContent, &pState->fX0, &pState->fY0);
		__xuiFlowGraphWidgetPortCenter(pData->pGraph, tEdge.iToNode, tEdge.iToPort, tContent, &pState->fX1, &pState->fY1);
		{
			xui_flow_graph_route_t tRoute;
			int iPoint;

			__xuiFlowGraphWidgetBuildRoute(&tRoute, pState->fX0, pState->fY0, pState->fX1, pState->fY1, tEdge.iRouteStyle, tEdge.fRouteBias, tEdge.fRouteSourceOffset, tEdge.fRouteTargetOffset);
			pState->iRoutePointCount = tRoute.iCount;
			for ( iPoint = 0; iPoint < tRoute.iCount; ++iPoint ) {
				pState->arrRouteX[iPoint] = tRoute.arrX[iPoint];
				pState->arrRouteY[iPoint] = tRoute.arrY[iPoint];
			}
		}
		iColor = xuiFlowGraphIsEdgeSelected(pData->pGraph, tEdge.sId) ? pData->iSelectedEdgeColor : pData->iEdgeColor;
		iColor = __xuiFlowGraphWidgetEdgeRunColor(tEdge.iRunState, iColor);
		if ( pData->iHoverType == XUI_FLOW_HIT_EDGE && pData->iHoverEdge == iEdge ) {
			iColor = XUI_COLOR_RGBA(18, 102, 208, 255);
		}
		if ( tEdge.bInvalid || xuiFlowGraphGetEdgeDiagnosticCount(pData->pGraph, tEdge.sId) > 0 ) {
			iColor = XUI_COLOR_RGBA(214, 72, 72, 230);
		}
		pState->iColor = iColor;
	}
	return XUI_OK;
}

static float __xuiFlowGraphWidgetDistanceToSegment(float fPX, float fPY, float fX0, float fY0, float fX1, float fY1)
{
	float fDX;
	float fDY;
	float fLen2;
	float fT;
	float fX;
	float fY;

	fDX = fX1 - fX0;
	fDY = fY1 - fY0;
	fLen2 = fDX * fDX + fDY * fDY;
	if ( fLen2 <= 0.0001f ) {
		fDX = fPX - fX0;
		fDY = fPY - fY0;
		return sqrtf(fDX * fDX + fDY * fDY);
	}
	fT = ((fPX - fX0) * fDX + (fPY - fY0) * fDY) / fLen2;
	if ( fT < 0.0f ) fT = 0.0f;
	if ( fT > 1.0f ) fT = 1.0f;
	fX = fX0 + fT * fDX;
	fY = fY0 + fT * fDY;
	fDX = fPX - fX;
	fDY = fPY - fY;
	return sqrtf(fDX * fDX + fDY * fDY);
}

static float __xuiFlowGraphWidgetDistanceToRoute(float fPX, float fPY, const xui_flow_graph_route_t* pRoute)
{
	float fBest;
	float fDistance;
	int i;

	if ( (pRoute == NULL) || (pRoute->iCount <= 0) ) {
		return 1000000.0f;
	}
	if ( pRoute->iCount == 1 ) {
		return __xuiFlowGraphWidgetDistanceToSegment(fPX, fPY, pRoute->arrX[0], pRoute->arrY[0], pRoute->arrX[0], pRoute->arrY[0]);
	}
	fBest = 1000000.0f;
	for ( i = 1; i < pRoute->iCount; ++i ) {
		fDistance = __xuiFlowGraphWidgetDistanceToSegment(fPX, fPY, pRoute->arrX[i - 1], pRoute->arrY[i - 1], pRoute->arrX[i], pRoute->arrY[i]);
		if ( fDistance < fBest ) {
			fBest = fDistance;
		}
	}
	return fBest;
}

static void __xuiFlowGraphWidgetPortCenter(xui_flow_graph pGraph, int iNode, int iPort, xui_rect_t tContent, float* pX, float* pY)
{
	xui_flow_node_info_t tNode;
	xui_flow_port_info_t tPort;
	xui_rect_t tNodeRect;
	int iCount;

	*pX = tContent.fX;
	*pY = tContent.fY;
	if ( xuiFlowGraphGetNode(pGraph, iNode, &tNode) != XUI_OK ) {
		return;
	}
	tNodeRect = __xuiFlowGraphWidgetNodeRect(pGraph, &tNode, tContent);
	iCount = xuiFlowGraphGetNodePortCount(pGraph, iNode);
	if ( iCount <= 0 ) iCount = 1;
	*pY = tNodeRect.fY + 28.0f + ((float)iPort + 1.0f) * ((tNodeRect.fH - 36.0f) / ((float)iCount + 1.0f));
	*pX = tNodeRect.fX;
	if ( xuiFlowGraphGetPort(pGraph, iNode, iPort, &tPort) == XUI_OK && tPort.iDirection == XUI_FLOW_PORT_OUTPUT ) {
		*pX = tNodeRect.fX + tNodeRect.fW;
	}
}

static void __xuiFlowGraphWidgetSetHit(xui_flow_hit_t* pHit, int iType, int iNode, int iPort, int iEdge, xui_rect_t tRect, float fX, float fY)
{
	if ( pHit == NULL ) {
		return;
	}
	memset(pHit, 0, sizeof(*pHit));
	pHit->iSize = sizeof(*pHit);
	pHit->iType = iType;
	pHit->iNode = iNode;
	pHit->iPort = iPort;
	pHit->iEdge = iEdge;
	pHit->tRect = tRect;
	pHit->fX = fX;
	pHit->fY = fY;
}

static int __xuiFlowGraphWidgetFindPortById(xui_flow_graph pGraph, const char* sNodeId, const char* sPortId, int* pNode, int* pPort)
{
	xui_flow_port_info_t tPort;
	int iNode;
	int i;
	int iCount;

	if ( (pGraph == NULL) || (sNodeId == NULL) || (sPortId == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iNode = xuiFlowGraphFindNode(pGraph, sNodeId);
	if ( iNode < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iCount = xuiFlowGraphGetNodePortCount(pGraph, iNode);
	for ( i = 0; i < iCount; ++i ) {
		if ( xuiFlowGraphGetPort(pGraph, iNode, i, &tPort) != XUI_OK ) {
			continue;
		}
		if ( (tPort.sId != NULL) && (strcmp(tPort.sId, sPortId) == 0) ) {
			if ( pNode != NULL ) *pNode = iNode;
			if ( pPort != NULL ) *pPort = i;
			return XUI_OK;
		}
	}
	return XUI_ERROR_INVALID_ARGUMENT;
}

static int __xuiFlowGraphWidgetConnectionTargetValid(xui_flow_graph_widget_data_t* pData, const xui_flow_hit_t* pHit)
{
	xui_flow_port_info_t tPort;

	if ( (pData == NULL) || (pHit == NULL) || !pData->bDraggingConnection || (pHit->iType != XUI_FLOW_HIT_PORT) || (pHit->iNode < 0) || (pHit->iPort < 0) ) {
		return 0;
	}
	if ( xuiFlowGraphGetPort(pData->pGraph, pHit->iNode, pHit->iPort, &tPort) != XUI_OK ) {
		return 0;
	}
	return ((tPort.iKind == pData->iDragPortKind) && (tPort.iDirection != pData->iDragPortDirection)) ? 1 : 0;
}

static int __xuiFlowGraphWidgetDrawEdges(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tContent, xui_flow_graph_widget_data_t* pData)
{
	xui_flow_graph_edge_layer_state_t* pState;
	int i;
	int iPoint;
	int iRet;

	if ( pProxy->drawLine == NULL ) {
		return XUI_OK;
	}
	iRet = __xuiFlowGraphWidgetRebuildEdgeLayerStates(pData, tContent);
	if ( iRet != XUI_OK ) return iRet;
	for ( i = 1; i <= (int)pData->arrEdgeLayerStates.Count; ++i ) {
		pState = (xui_flow_graph_edge_layer_state_t*)xrtArrayGet_Unsafe(&pData->arrEdgeLayerStates, (uint32_t)i);
		if ( pState == NULL ) {
			continue;
		}
		for ( iPoint = 1; iPoint < pState->iRoutePointCount; ++iPoint ) {
			iRet = pProxy->drawLine(pProxy, pDraw, pState->arrRouteX[iPoint - 1], pState->arrRouteY[iPoint - 1], pState->arrRouteX[iPoint], pState->arrRouteY[iPoint], 2.0f, pState->iColor);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	return XUI_OK;
}

static int __xuiFlowGraphWidgetDrawConnectionPreview(xui_proxy pProxy, xui_draw_context pDraw, xui_flow_graph pGraph, xui_rect_t tContent, const xui_flow_graph_widget_data_t* pData)
{
	xui_flow_graph_route_t tRoute;
	float fX0;
	float fY0;
	uint32_t iColor;
	int iNode;
	int iPort;
	int i;
	int iRet;

	if ( (pProxy->drawLine == NULL) || (pData == NULL) || !pData->bDraggingConnection || (pData->sDragNodeId == NULL) || (pData->sDragPortId == NULL) ) {
		return XUI_OK;
	}
	if ( __xuiFlowGraphWidgetFindPortById(pGraph, pData->sDragNodeId, pData->sDragPortId, &iNode, &iPort) != XUI_OK ) {
		return XUI_OK;
	}
	__xuiFlowGraphWidgetPortCenter(pGraph, iNode, iPort, tContent, &fX0, &fY0);
	iColor = pData->bDragConnectionValid ? XUI_COLOR_RGBA(30, 160, 95, 230) : XUI_COLOR_RGBA(214, 72, 72, 210);
	__xuiFlowGraphWidgetBuildRoute(&tRoute, fX0, fY0, pData->fDragCurrentX, pData->fDragCurrentY, XUI_FLOW_ROUTE_AUTO, 0.5f, 0.0f, 0.0f);
	for ( i = 1; i < tRoute.iCount; ++i ) {
		iRet = pProxy->drawLine(pProxy, pDraw, tRoute.arrX[i - 1], tRoute.arrY[i - 1], tRoute.arrX[i], tRoute.arrY[i], 2.0f, iColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiFlowGraphWidgetDrawMarquee(xui_proxy pProxy, xui_draw_context pDraw, const xui_flow_graph_widget_data_t* pData)
{
	xui_rect_t tRect;
	int iRet;

	if ( (pData == NULL) || !pData->bDraggingMarquee ) {
		return XUI_OK;
	}
	tRect = __xuiFlowGraphWidgetRectFromPoints(pData->fDragStartX, pData->fDragStartY, pData->fDragCurrentX, pData->fDragCurrentY);
	if ( (tRect.fW < 1.0f) || (tRect.fH < 1.0f) ) {
		return XUI_OK;
	}
	if ( pProxy->drawRectFill != NULL ) {
		iRet = pProxy->drawRectFill(pProxy, pDraw, tRect, XUI_COLOR_RGBA(42, 134, 230, 38));
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pProxy->drawRectStroke != NULL ) {
		return pProxy->drawRectStroke(pProxy, pDraw, tRect, 1.0f, XUI_COLOR_RGBA(26, 115, 232, 210));
	}
	return XUI_OK;
}

static int __xuiFlowGraphWidgetDrawNodes(xui_widget pWidget, xui_proxy pProxy, xui_draw_context pDraw, xui_flow_graph pGraph, xui_rect_t tContent, xui_flow_graph_widget_data_t* pData)
{
	xui_flow_node_info_t tNode;
	xui_flow_graph_node_card_state_t* pState;
	xui_rect_t tNodeRect;
	xui_rect_t tTitleRect;
	xui_rect_t tSummaryRect;
	xui_rect_t tBadgeRect;
	xui_font pFont;
	float fPortX;
	float fPortY;
	uint32_t iPort;
	uint32_t iText;
	char sBadge[16];
	int bEnabled;
	int i;
	int j;
	int iNode;
	int iPortCount;
	int iRet;

	if ( (pProxy->drawRectFill == NULL) || (pProxy->drawRectStroke == NULL) ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	pFont = xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	bEnabled = xuiWidgetGetEnabled(pWidget);
	iPort = bEnabled ? pData->iPortColor : XUI_COLOR_RGBA(140, 150, 164, 180);
	iText = bEnabled ? XUI_COLOR_RGBA(26, 36, 52, 255) : XUI_COLOR_RGBA(108, 118, 132, 180);
	iRet = __xuiFlowGraphWidgetRebuildNodeCardStates(pData, bEnabled);
	if ( iRet != XUI_OK ) return iRet;
	for ( i = 1; i <= (int)pData->arrNodeCardStates.Count; ++i ) {
		pState = (xui_flow_graph_node_card_state_t*)xrtArrayGet_Unsafe(&pData->arrNodeCardStates, (uint32_t)i);
		if ( pState == NULL ) {
			continue;
		}
		iNode = pState->iNode;
		if ( xuiFlowGraphGetNode(pGraph, iNode, &tNode) != XUI_OK ) {
			continue;
		}
		tNodeRect = __xuiFlowGraphWidgetNodeRect(pGraph, &tNode, tContent);
		if ( (tNodeRect.fX + tNodeRect.fW < tContent.fX) || (tNodeRect.fY + tNodeRect.fH < tContent.fY) ||
		     (tNodeRect.fX > tContent.fX + tContent.fW) || (tNodeRect.fY > tContent.fY + tContent.fH) ) {
			continue;
		}
		iRet = pProxy->drawRectFill(pProxy, pDraw, tNodeRect, pState->iFill);
		if ( iRet != XUI_OK ) return iRet;
		iRet = pProxy->drawRectStroke(pProxy, pDraw, tNodeRect, 1.0f, pState->iBorder);
		if ( iRet != XUI_OK ) return iRet;
		if ( pState->iDiagCount > 0 ) {
			tBadgeRect.fX = tNodeRect.fX + tNodeRect.fW - 30.0f;
			tBadgeRect.fY = tNodeRect.fY + 8.0f;
			tBadgeRect.fW = 20.0f;
			tBadgeRect.fH = 16.0f;
			snprintf(sBadge, sizeof(sBadge), "%d", pState->iDiagCount);
			iRet = pProxy->drawRectFill(pProxy, pDraw, tBadgeRect, XUI_COLOR_RGBA(214, 72, 72, 235));
			if ( iRet != XUI_OK ) return iRet;
			if ( pProxy->drawText != NULL && pFont != NULL ) {
				iRet = pProxy->drawText(pProxy, pDraw, pFont, sBadge, tBadgeRect, XUI_COLOR_RGBA(255, 255, 255, 255), XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
				if ( iRet != XUI_OK ) return iRet;
			}
		}
		if ( pState->bHasRunBadge ) {
			tBadgeRect.fX = tNodeRect.fX + tNodeRect.fW - ((pState->iDiagCount > 0) ? 56.0f : 30.0f);
			tBadgeRect.fY = tNodeRect.fY + 8.0f;
			tBadgeRect.fW = 20.0f;
			tBadgeRect.fH = 16.0f;
			iRet = pProxy->drawRectFill(pProxy, pDraw, tBadgeRect, pState->iRunBadgeColor);
			if ( iRet != XUI_OK ) return iRet;
			if ( pProxy->drawText != NULL && pFont != NULL ) {
				iRet = pProxy->drawText(pProxy, pDraw, pFont, pState->sRunBadge, tBadgeRect, pState->iRunBadgeTextColor, XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
				if ( iRet != XUI_OK ) return iRet;
			}
		}
		if ( (pProxy->drawText != NULL) && (pFont != NULL) && (tNode.sTitle != NULL) ) {
			tTitleRect.fX = tNodeRect.fX + 12.0f;
			tTitleRect.fY = tNodeRect.fY + 8.0f;
			tTitleRect.fW = tNodeRect.fW - 24.0f;
			tTitleRect.fH = 22.0f;
			iRet = pProxy->drawText(pProxy, pDraw, pFont, tNode.sTitle, tTitleRect, iText, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
			if ( iRet != XUI_OK ) return iRet;
		}
		if ( (pProxy->drawText != NULL) && (pFont != NULL) && (tNode.sSummary != NULL) && (tNode.sSummary[0] != 0) ) {
			tSummaryRect.fX = tNodeRect.fX + 12.0f;
			tSummaryRect.fY = tNodeRect.fY + 32.0f;
			tSummaryRect.fW = tNodeRect.fW - 24.0f;
			tSummaryRect.fH = 18.0f;
			iRet = pProxy->drawText(pProxy, pDraw, pFont, tNode.sSummary, tSummaryRect, XUI_COLOR_RGBA(82, 96, 112, 230), XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
			if ( iRet != XUI_OK ) return iRet;
		}
		if ( (pProxy->drawText != NULL) && (pFont != NULL) && (tNode.sRunPreview != NULL) && (tNode.sRunPreview[0] != 0) ) {
			tSummaryRect.fX = tNodeRect.fX + 12.0f;
			tSummaryRect.fY = tNodeRect.fY + tNodeRect.fH - 24.0f;
			tSummaryRect.fW = tNodeRect.fW - 24.0f;
			tSummaryRect.fH = 16.0f;
			iRet = pProxy->drawText(pProxy, pDraw, pFont, tNode.sRunPreview, tSummaryRect, __xuiFlowGraphWidgetNodeRunColor(tNode.iRunState, XUI_COLOR_RGBA(82, 96, 112, 230)), XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
			if ( iRet != XUI_OK ) return iRet;
		}
		iPortCount = xuiFlowGraphGetNodePortCount(pGraph, iNode);
		for ( j = 0; j < iPortCount; ++j ) {
			__xuiFlowGraphWidgetPortCenter(pGraph, iNode, j, tContent, &fPortX, &fPortY);
			if ( pData->iHoverType == XUI_FLOW_HIT_PORT && pData->iHoverNode == iNode && pData->iHoverPort == j ) {
				iPort = XUI_COLOR_RGBA(18, 102, 208, 255);
			} else {
				iPort = bEnabled ? pData->iPortColor : XUI_COLOR_RGBA(140, 150, 164, 180);
			}
			if ( pProxy->drawCircleFill != NULL ) {
				iRet = pProxy->drawCircleFill(pProxy, pDraw, fPortX, fPortY, 4.0f, iPort);
			} else {
				iRet = pProxy->drawRectFill(pProxy, pDraw, (xui_rect_t){fPortX - 3.0f, fPortY - 3.0f, 6.0f, 6.0f}, iPort);
			}
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	return XUI_OK;
}

static int __xuiFlowGraphWidgetCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_flow_graph_widget_data_t* pData;
	xui_proxy pProxy;
	xui_rect_t tContent;
	xui_flow_viewport_t tViewport;
	uint32_t arrSavedColors[8];
	int iRet;

	(void)iStateId;
	(void)pUser;
	if ( (pWidget == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiFlowGraphWidgetGetData(pWidget);
	if ( (pData == NULL) || (pData->pGraph == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( (pProxy == NULL) || (pProxy->drawRectFill == NULL) ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	__xuiFlowGraphWidgetSaveColors(pData, arrSavedColors);
	__xuiFlowGraphWidgetApplyStyleColors(pWidget, pData);
	tContent = xuiWidgetGetContentRect(pWidget);
	iRet = pProxy->drawRectFill(pProxy, pDraw, tContent, pData->iBackgroundColor);
	if ( iRet != XUI_OK ) goto cleanup;
	memset(&tViewport, 0, sizeof(tViewport));
	tViewport.iSize = sizeof(tViewport);
	(void)xuiFlowGraphGetViewport(pData->pGraph, &tViewport);
	iRet = __xuiFlowGraphWidgetRebuildNodeBuckets(pData, tContent);
	if ( iRet != XUI_OK ) goto cleanup;
	iRet = __xuiFlowGraphWidgetRebuildEdgeBuckets(pData, tContent);
	if ( iRet != XUI_OK ) goto cleanup;
	iRet = __xuiFlowGraphWidgetDrawGrid(pProxy, pDraw, pData, tContent, &tViewport, pData->iGridColor);
	if ( iRet != XUI_OK ) goto cleanup;
	iRet = __xuiFlowGraphWidgetDrawEdges(pProxy, pDraw, tContent, pData);
	if ( iRet != XUI_OK ) goto cleanup;
	iRet = __xuiFlowGraphWidgetDrawNodes(pWidget, pProxy, pDraw, pData->pGraph, tContent, pData);
	if ( iRet != XUI_OK ) goto cleanup;
	iRet = __xuiFlowGraphWidgetDrawConnectionPreview(pProxy, pDraw, pData->pGraph, tContent, pData);
	if ( iRet != XUI_OK ) goto cleanup;
	iRet = __xuiFlowGraphWidgetDrawMarquee(pProxy, pDraw, pData);
cleanup:
	__xuiFlowGraphWidgetRestoreColors(pData, arrSavedColors);
	return iRet;
}

static int __xuiFlowGraphWidgetInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_flow_graph_widget_data_t* pData;
	const xui_flow_graph_desc_t* pDesc;
	int iRet;

	(void)pUser;
	if ( (pWidget == NULL) || (pTypeData == NULL) || !__xuiFlowGraphWidgetDescValid((const xui_flow_graph_desc_t*)pCreateData) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = (xui_flow_graph_widget_data_t*)pTypeData;
	pDesc = (const xui_flow_graph_desc_t*)pCreateData;
	memset(pData, 0, sizeof(*pData));
	xrtArrayInit(&pData->arrNodeBuckets, sizeof(xui_flow_graph_node_bucket_t), XRT_OBJMODE_LOCAL);
	xrtArrayInit(&pData->arrVisibleNodes, sizeof(int), XRT_OBJMODE_LOCAL);
	xrtArrayInit(&pData->arrEdgeBuckets, sizeof(xui_flow_graph_edge_bucket_t), XRT_OBJMODE_LOCAL);
	xrtArrayInit(&pData->arrVisibleEdges, sizeof(int), XRT_OBJMODE_LOCAL);
	xrtArrayInit(&pData->arrGridLines, sizeof(xui_flow_graph_grid_line_t), XRT_OBJMODE_LOCAL);
	xrtArrayInit(&pData->arrNodeCardStates, sizeof(xui_flow_graph_node_card_state_t), XRT_OBJMODE_LOCAL);
	xrtArrayInit(&pData->arrEdgeLayerStates, sizeof(xui_flow_graph_edge_layer_state_t), XRT_OBJMODE_LOCAL);
	pData->iBackgroundColor = XUI_COLOR_RGBA(246, 248, 252, 255);
	pData->iGridColor = XUI_COLOR_RGBA(214, 220, 230, 120);
	pData->iNodeColor = XUI_COLOR_RGBA(255, 255, 255, 255);
	pData->iSelectedNodeColor = XUI_COLOR_RGBA(226, 242, 255, 255);
	pData->iNodeBorderColor = XUI_COLOR_RGBA(74, 93, 118, 180);
	pData->iPortColor = XUI_COLOR_RGBA(42, 134, 230, 255);
	pData->iEdgeColor = XUI_COLOR_RGBA(86, 108, 134, 220);
	pData->iSelectedEdgeColor = XUI_COLOR_RGBA(26, 115, 232, 255);
	if ( pDesc != NULL ) {
		pData->iBackgroundColor = __xuiFlowGraphWidgetColor(pDesc->iBackgroundColor, pData->iBackgroundColor);
		pData->iGridColor = __xuiFlowGraphWidgetColor(pDesc->iGridColor, pData->iGridColor);
		pData->iNodeColor = __xuiFlowGraphWidgetColor(pDesc->iNodeColor, pData->iNodeColor);
		pData->iSelectedNodeColor = __xuiFlowGraphWidgetColor(pDesc->iSelectedNodeColor, pData->iSelectedNodeColor);
		pData->iNodeBorderColor = __xuiFlowGraphWidgetColor(pDesc->iNodeBorderColor, pData->iNodeBorderColor);
		pData->iPortColor = __xuiFlowGraphWidgetColor(pDesc->iPortColor, pData->iPortColor);
		pData->iEdgeColor = __xuiFlowGraphWidgetColor(pDesc->iEdgeColor, pData->iEdgeColor);
		pData->iSelectedEdgeColor = __xuiFlowGraphWidgetColor(pDesc->iSelectedEdgeColor, pData->iSelectedEdgeColor);
		pData->pGraph = pDesc->pGraph;
		pData->bOwnGraph = pDesc->bOwnGraph;
	}
	if ( pData->pGraph == NULL ) {
		iRet = xuiFlowGraphCreate(&pData->pGraph);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		pData->bOwnGraph = 1;
	}
	pData->iHoverType = XUI_FLOW_HIT_NONE;
	pData->iHoverNode = -1;
	pData->iHoverPort = -1;
	pData->iHoverEdge = -1;
	pData->iLastGraphRevision = xuiFlowGraphGetRevision(pData->pGraph);
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	(void)xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiFlowGraphWidgetEvent, NULL);
	(void)xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiFlowGraphWidgetEvent, NULL);
	(void)xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiFlowGraphWidgetEvent, NULL);
	(void)xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiFlowGraphWidgetEvent, NULL);
	(void)xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiFlowGraphWidgetEvent, NULL);
	return XUI_OK;
}

static int __xuiFlowGraphWidgetUpdate(xui_widget pWidget, float fDelta, void* pUser)
{
	xui_flow_graph_widget_data_t* pData;
	uint32_t iRevision;

	(void)fDelta;
	(void)pUser;
	pData = __xuiFlowGraphWidgetGetData(pWidget);
	if ( (pData == NULL) || (pData->pGraph == NULL) ) {
		return XUI_OK;
	}
	iRevision = xuiFlowGraphGetRevision(pData->pGraph);
	if ( iRevision != pData->iLastGraphRevision ) {
		pData->iLastGraphRevision = iRevision;
		return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return XUI_OK;
}

static void __xuiFlowGraphWidgetDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_flow_graph_widget_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_flow_graph_widget_data_t*)pTypeData;
	if ( pData != NULL ) {
		__xuiFlowGraphWidgetClearDrag(pData);
		__xuiFlowGraphWidgetClearNodeBuckets(pData);
		__xuiFlowGraphWidgetClearEdgeBuckets(pData);
		__xuiFlowGraphWidgetClearGridCache(pData);
		__xuiFlowGraphWidgetClearNodeCardStates(pData);
		__xuiFlowGraphWidgetClearEdgeLayerStates(pData);
		if ( pData->bOwnGraph && (pData->pGraph != NULL) ) {
			xuiFlowGraphDestroy(pData->pGraph);
		}
		memset(pData, 0, sizeof(*pData));
	}
}

static void __xuiFlowGraphWidgetDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_MANUAL;
	pLayout->iWidthMode = XUI_SIZE_FILL;
	pLayout->iHeightMode = XUI_SIZE_FILL;
	pLayout->iFlowMode = XUI_FLOW_BLOCK;
	pLayout->iDock = XUI_DOCK_FILL;
	pLayout->iOverflow = XUI_OVERFLOW_CLIP;
	pLayout->iAlignX = XUI_ALIGN_STRETCH;
	pLayout->iAlignY = XUI_ALIGN_STRETCH;
	pLayout->iTableRowSpan = 1;
	pLayout->iTableColumnSpan = 1;
	pLayout->iGridColumnCount = 1;
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fShrink = 1.0f;
}

static void __xuiFlowGraphWidgetDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE | XUI_CACHE_UPDATE_ALL_STATES;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static void __xuiFlowGraphWidgetRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
{
	xui_style_property_info_t tInfo;

	if ( xuiStyleFindProperty(pContext, sName) != 0 ) {
		return;
	}
	memset(&tInfo, 0, sizeof(tInfo));
	tInfo.iSize = sizeof(tInfo);
	tInfo.sName = sName;
	tInfo.iValueType = iValueType;
	tInfo.iDirtyFlags = iDirtyFlags;
	tInfo.iFlags = iFlags;
	tInfo.pWidgetType = pType;
	(void)xuiStyleRegisterProperty(pContext, &tInfo, NULL);
}

static void __xuiFlowGraphWidgetRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_STYLE | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiFlowGraphWidgetRegisterStyleProperty(pContext, pType, "flowgraph.background.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiFlowGraphWidgetRegisterStyleProperty(pContext, pType, "flowgraph.grid.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiFlowGraphWidgetRegisterStyleProperty(pContext, pType, "flowgraph.node.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiFlowGraphWidgetRegisterStyleProperty(pContext, pType, "flowgraph.node.selected_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiFlowGraphWidgetRegisterStyleProperty(pContext, pType, "flowgraph.node.border_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiFlowGraphWidgetRegisterStyleProperty(pContext, pType, "flowgraph.port.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiFlowGraphWidgetRegisterStyleProperty(pContext, pType, "flowgraph.edge.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiFlowGraphWidgetRegisterStyleProperty(pContext, pType, "flowgraph.edge.selected_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
}

XUI_API xui_widget_type xuiFlowGraphGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "flowgraph");
	if ( pType != NULL ) {
		__xuiFlowGraphWidgetRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "flowgraph";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_flow_graph_widget_data_t);
	tDesc.onInit = __xuiFlowGraphWidgetInit;
	tDesc.onDestroy = __xuiFlowGraphWidgetDestroy;
	tDesc.onCacheRender = __xuiFlowGraphWidgetCacheRender;
	tDesc.onUpdate = __xuiFlowGraphWidgetUpdate;
	__xuiFlowGraphWidgetDefaultLayout(&tDesc.tLayout);
	__xuiFlowGraphWidgetDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet == XUI_OK ) {
		__xuiFlowGraphWidgetRegisterStyleProperties(pContext, pType);
	}
	return (iRet == XUI_OK) ? pType : NULL;
}

XUI_API int xuiFlowGraphWidgetCreate(xui_context pContext, xui_widget* ppWidget, const xui_flow_graph_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiFlowGraphWidgetDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiFlowGraphGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API xui_flow_graph xuiFlowGraphWidgetGetGraph(xui_widget pWidget)
{
	xui_flow_graph_widget_data_t* pData;

	pData = __xuiFlowGraphWidgetGetData(pWidget);
	return (pData != NULL) ? pData->pGraph : NULL;
}

XUI_API int xuiFlowGraphWidgetSetGraph(xui_widget pWidget, xui_flow_graph pGraph, int bOwnGraph)
{
	xui_flow_graph_widget_data_t* pData;

	pData = __xuiFlowGraphWidgetGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiFlowGraphWidgetClearDrag(pData);
	__xuiFlowGraphWidgetClearNodeBuckets(pData);
	__xuiFlowGraphWidgetClearEdgeBuckets(pData);
	__xuiFlowGraphWidgetClearGridCache(pData);
	__xuiFlowGraphWidgetClearNodeCardStates(pData);
	__xuiFlowGraphWidgetClearEdgeLayerStates(pData);
	if ( pData->bOwnGraph && (pData->pGraph != NULL) && (pData->pGraph != pGraph) ) {
		xuiFlowGraphDestroy(pData->pGraph);
	}
	pData->pGraph = pGraph;
	pData->bOwnGraph = bOwnGraph ? 1 : 0;
	pData->iHoverType = XUI_FLOW_HIT_NONE;
	pData->iHoverNode = -1;
	pData->iHoverPort = -1;
	pData->iHoverEdge = -1;
	pData->iLastGraphRevision = xuiFlowGraphGetRevision(pGraph);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiFlowGraphWidgetHitTestLocal(xui_widget pWidget, float fX, float fY, xui_flow_hit_t* pHit)
{
	xui_flow_graph_widget_data_t* pData;
	xui_flow_graph pGraph;
	xui_flow_node_info_t tNode;
	xui_flow_edge_info_t tEdge;
	xui_flow_graph_route_t tRoute;
	xui_rect_t tContent;
	xui_rect_t tNodeRect;
	xui_rect_t tPortRect;
	xui_flow_graph_node_bucket_t* pBucket;
	xui_flow_graph_edge_bucket_t* pEdgeBucket;
	int* pNodeIndex;
	int* pEdgeIndex;
	float fPortX;
	float fPortY;
	float fEdgeX0;
	float fEdgeY0;
	float fEdgeX1;
	float fEdgeY1;
	float fDistance;
	int i;
	int j;
	int iBucket;
	int iEdgeBucket;
	int iPortCount;
	int iRet;

	if ( (pHit == NULL) || !xuiInternalWidgetIsValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiFlowGraphWidgetSetHit(pHit, XUI_FLOW_HIT_NONE, -1, -1, -1, (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f}, fX, fY);
	pData = __xuiFlowGraphWidgetGetData(pWidget);
	if ( (pData == NULL) || (pData->pGraph == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pGraph = pData->pGraph;
	tContent = xuiWidgetGetContentRect(pWidget);
	if ( !__xuiFlowGraphWidgetPointInRect(tContent, fX, fY) ) {
		return XUI_OK;
	}

	iRet = __xuiFlowGraphWidgetRebuildNodeBuckets(pData, tContent);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iBucket = __xuiFlowGraphWidgetBucketIndex(pData, fX, fY);
	if ( iBucket >= 0 && (uint32_t)iBucket < pData->arrNodeBuckets.Count ) {
		pBucket = (xui_flow_graph_node_bucket_t*)xrtArrayGet_Unsafe(&pData->arrNodeBuckets, (uint32_t)iBucket + 1u);
		for ( i = (int)pBucket->arrNodes.Count; i >= 1; --i ) {
			pNodeIndex = (int*)xrtArrayGet_Unsafe(&pBucket->arrNodes, (uint32_t)i);
			if ( pNodeIndex == NULL ) {
				continue;
			}
			if ( xuiFlowGraphGetNode(pGraph, *pNodeIndex, &tNode) != XUI_OK ) {
				continue;
			}
			tNodeRect = __xuiFlowGraphWidgetNodeRect(pGraph, &tNode, tContent);
			iPortCount = xuiFlowGraphGetNodePortCount(pGraph, *pNodeIndex);
			for ( j = 0; j < iPortCount; ++j ) {
				__xuiFlowGraphWidgetPortCenter(pGraph, *pNodeIndex, j, tContent, &fPortX, &fPortY);
				tPortRect = (xui_rect_t){fPortX - 7.0f, fPortY - 7.0f, 14.0f, 14.0f};
				if ( __xuiFlowGraphWidgetPointInRect(tPortRect, fX, fY) ) {
					__xuiFlowGraphWidgetSetHit(pHit, XUI_FLOW_HIT_PORT, *pNodeIndex, j, -1, tPortRect, fX, fY);
					return XUI_OK;
				}
			}
			if ( __xuiFlowGraphWidgetPointInRect(tNodeRect, fX, fY) ) {
				__xuiFlowGraphWidgetSetHit(pHit, XUI_FLOW_HIT_NODE, *pNodeIndex, -1, -1, tNodeRect, fX, fY);
				return XUI_OK;
			}
		}
	}

	iRet = __xuiFlowGraphWidgetRebuildEdgeBuckets(pData, tContent);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iEdgeBucket = __xuiFlowGraphWidgetEdgeBucketIndex(pData, fX, fY);
	if ( iEdgeBucket >= 0 && (uint32_t)iEdgeBucket < pData->arrEdgeBuckets.Count ) {
		pEdgeBucket = (xui_flow_graph_edge_bucket_t*)xrtArrayGet_Unsafe(&pData->arrEdgeBuckets, (uint32_t)iEdgeBucket + 1u);
		for ( i = (int)pEdgeBucket->arrEdges.Count; i >= 1; --i ) {
			pEdgeIndex = (int*)xrtArrayGet_Unsafe(&pEdgeBucket->arrEdges, (uint32_t)i);
			if ( pEdgeIndex == NULL ) {
				continue;
			}
			if ( xuiFlowGraphGetEdge(pGraph, *pEdgeIndex, &tEdge) != XUI_OK ) {
				continue;
			}
			__xuiFlowGraphWidgetPortCenter(pGraph, tEdge.iFromNode, tEdge.iFromPort, tContent, &fEdgeX0, &fEdgeY0);
			__xuiFlowGraphWidgetPortCenter(pGraph, tEdge.iToNode, tEdge.iToPort, tContent, &fEdgeX1, &fEdgeY1);
			__xuiFlowGraphWidgetBuildRoute(&tRoute, fEdgeX0, fEdgeY0, fEdgeX1, fEdgeY1, tEdge.iRouteStyle, tEdge.fRouteBias, tEdge.fRouteSourceOffset, tEdge.fRouteTargetOffset);
			fDistance = __xuiFlowGraphWidgetDistanceToRoute(fX, fY, &tRoute);
			if ( fDistance <= 6.0f ) {
				tPortRect = __xuiFlowGraphWidgetRouteBounds(&tRoute);
				__xuiFlowGraphWidgetSetHit(pHit, XUI_FLOW_HIT_EDGE, -1, -1, *pEdgeIndex, tPortRect, fX, fY);
				return XUI_OK;
			}
		}
	}

	__xuiFlowGraphWidgetSetHit(pHit, XUI_FLOW_HIT_BACKGROUND, -1, -1, -1, tContent, fX, fY);
	return XUI_OK;
}

XUI_API int xuiFlowGraphWidgetHitTest(xui_widget pWidget, float fX, float fY, xui_flow_hit_t* pHit)
{
	float fLocalX;
	float fLocalY;

	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiFlowGraphWidgetPointToLocal(pWidget, fX, fY, &fLocalX, &fLocalY);
	return __xuiFlowGraphWidgetHitTestLocal(pWidget, fLocalX, fLocalY, pHit);
}

XUI_API int xuiFlowGraphWidgetGetHoverHit(xui_widget pWidget, xui_flow_hit_t* pHit)
{
	xui_flow_graph_widget_data_t* pData;

	if ( pHit == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiFlowGraphWidgetGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(pHit, 0, sizeof(*pHit));
	pHit->iSize = sizeof(*pHit);
	pHit->iType = pData->iHoverType;
	pHit->iNode = pData->iHoverNode;
	pHit->iPort = pData->iHoverPort;
	pHit->iEdge = pData->iHoverEdge;
	return XUI_OK;
}

static int __xuiFlowGraphWidgetSelectAt(xui_widget pWidget, float fX, float fY, uint32_t iModifiers, xui_flow_hit_t* pHit)
{
	xui_flow_graph_widget_data_t* pData;
	xui_flow_hit_t tHit;
	xui_flow_node_info_t tNode;
	xui_flow_edge_info_t tEdge;
	int bToggle;
	int bSelected;
	int iRet;

	pData = __xuiFlowGraphWidgetGetData(pWidget);
	if ( (pData == NULL) || (pData->pGraph == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(&tHit, 0, sizeof(tHit));
	tHit.iSize = sizeof(tHit);
	iRet = xuiFlowGraphWidgetHitTest(pWidget, fX, fY, &tHit);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	bToggle = ((iModifiers & (XUI_MOD_CTRL | XUI_MOD_SHIFT)) != 0u) ? 1 : 0;
	if ( !bToggle ) {
		iRet = xuiFlowGraphClearSelection(pData->pGraph);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	if ( tHit.iType == XUI_FLOW_HIT_PORT || tHit.iType == XUI_FLOW_HIT_NODE ) {
		iRet = xuiFlowGraphGetNode(pData->pGraph, tHit.iNode, &tNode);
		if ( iRet == XUI_OK ) {
			bSelected = bToggle ? !xuiFlowGraphIsNodeSelected(pData->pGraph, tNode.sId) : 1;
			iRet = xuiFlowGraphSelectNode(pData->pGraph, tNode.sId, bSelected);
		}
	} else if ( tHit.iType == XUI_FLOW_HIT_EDGE ) {
		iRet = xuiFlowGraphGetEdge(pData->pGraph, tHit.iEdge, &tEdge);
		if ( iRet == XUI_OK ) {
			bSelected = bToggle ? !xuiFlowGraphIsEdgeSelected(pData->pGraph, tEdge.sId) : 1;
			iRet = xuiFlowGraphSelectEdge(pData->pGraph, tEdge.sId, bSelected);
		}
	}
	if ( pHit != NULL ) {
		*pHit = tHit;
	}
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiFlowGraphWidgetStartNodeDrag(xui_widget pWidget, xui_flow_graph_widget_data_t* pData, const xui_flow_hit_t* pHit)
{
	xui_flow_graph_drag_node_t* arrNodes;
	xui_flow_node_info_t tNode;
	int i;
	int iCount;
	int iSelectedCount;
	int iDragCount;
	int iRet;

	if ( (pData == NULL) || (pData->pGraph == NULL) || (pHit == NULL) || (pHit->iNode < 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = xuiFlowGraphGetNode(pData->pGraph, pHit->iNode, &tNode);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	__xuiFlowGraphWidgetClearDrag(pData);
	iSelectedCount = xuiFlowGraphGetSelectedNodeCount(pData->pGraph);
	iDragCount = (xuiFlowGraphIsNodeSelected(pData->pGraph, tNode.sId) && (iSelectedCount > 1)) ? iSelectedCount : 1;
	arrNodes = (xui_flow_graph_drag_node_t*)xrtMalloc(sizeof(*arrNodes) * (size_t)iDragCount);
	if ( arrNodes == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	memset(arrNodes, 0, sizeof(*arrNodes) * (size_t)iDragCount);
	if ( iDragCount == 1 ) {
		arrNodes[0].sId = __xuiFlowGraphWidgetCopyString(tNode.sId);
		arrNodes[0].fStartX = tNode.fX;
		arrNodes[0].fStartY = tNode.fY;
		if ( arrNodes[0].sId == NULL ) {
			xrtFree(arrNodes);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
	} else {
		iCount = xuiFlowGraphGetNodeCount(pData->pGraph);
		iDragCount = 0;
		for ( i = 0; i < iCount; ++i ) {
			if ( xuiFlowGraphGetNode(pData->pGraph, i, &tNode) != XUI_OK ) {
				continue;
			}
			if ( !xuiFlowGraphIsNodeSelected(pData->pGraph, tNode.sId) ) {
				continue;
			}
			arrNodes[iDragCount].sId = __xuiFlowGraphWidgetCopyString(tNode.sId);
			if ( arrNodes[iDragCount].sId == NULL ) {
				pData->arrDragNodes = arrNodes;
				pData->iDragNodeCount = iDragCount;
				__xuiFlowGraphWidgetClearDrag(pData);
				return XUI_ERROR_OUT_OF_MEMORY;
			}
			arrNodes[iDragCount].fStartX = tNode.fX;
			arrNodes[iDragCount].fStartY = tNode.fY;
			iDragCount++;
		}
	}
	pData->arrDragNodes = arrNodes;
	pData->iDragNodeCount = iDragCount;
	pData->bDraggingNode = 1;
	pData->fDragStartX = pHit->fX;
	pData->fDragStartY = pHit->fY;
	return xuiSetPointerCapture(xuiWidgetGetContext(pWidget), pWidget);
}

static int __xuiFlowGraphWidgetDragNode(xui_widget pWidget, xui_flow_graph_widget_data_t* pData, float fX, float fY)
{
	xui_flow_viewport_t tViewport;
	float fDeltaX;
	float fDeltaY;
	float fZoom;
	int i;
	int iRet;

	if ( (pData == NULL) || !pData->bDraggingNode || (pData->arrDragNodes == NULL) || (pData->iDragNodeCount <= 0) || (pData->pGraph == NULL) ) {
		return XUI_OK;
	}
	memset(&tViewport, 0, sizeof(tViewport));
	tViewport.iSize = sizeof(tViewport);
	(void)xuiFlowGraphGetViewport(pData->pGraph, &tViewport);
	fZoom = (tViewport.fZoom > 0.001f) ? tViewport.fZoom : 1.0f;
	fDeltaX = (fX - pData->fDragStartX) / fZoom;
	fDeltaY = (fY - pData->fDragStartY) / fZoom;
	for ( i = 0; i < pData->iDragNodeCount; ++i ) {
		iRet = xuiFlowGraphSetNodePosition(pData->pGraph, pData->arrDragNodes[i].sId,
			pData->arrDragNodes[i].fStartX + fDeltaX,
			pData->arrDragNodes[i].fStartY + fDeltaY);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiFlowGraphWidgetFinishNodeDrag(xui_widget pWidget, xui_flow_graph_widget_data_t* pData)
{
	xui_flow_move_node_record_t* pRecords;
	xui_flow_node_info_t tNode;
	int i;
	int iNode;
	int iRecordCount;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || !pData->bDraggingNode || (pData->pGraph == NULL) ) {
		return XUI_OK;
	}
	pRecords = (xui_flow_move_node_record_t*)xrtMalloc(sizeof(*pRecords) * (size_t)pData->iDragNodeCount);
	if ( pRecords == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	memset(pRecords, 0, sizeof(*pRecords) * (size_t)pData->iDragNodeCount);
	iRecordCount = 0;
	for ( i = 0; i < pData->iDragNodeCount; ++i ) {
		if ( pData->arrDragNodes[i].sId == NULL ) {
			continue;
		}
		iNode = xuiFlowGraphFindNode(pData->pGraph, pData->arrDragNodes[i].sId);
		if ( iNode < 0 ) {
			continue;
		}
		memset(&tNode, 0, sizeof(tNode));
		tNode.iSize = sizeof(tNode);
		iRet = xuiFlowGraphGetNode(pData->pGraph, iNode, &tNode);
		if ( iRet != XUI_OK ) {
			xrtFree(pRecords);
			return iRet;
		}
		pRecords[iRecordCount].iSize = sizeof(pRecords[iRecordCount]);
		pRecords[iRecordCount].sId = pData->arrDragNodes[i].sId;
		pRecords[iRecordCount].fOldX = pData->arrDragNodes[i].fStartX;
		pRecords[iRecordCount].fOldY = pData->arrDragNodes[i].fStartY;
		pRecords[iRecordCount].fNewX = tNode.fX;
		pRecords[iRecordCount].fNewY = tNode.fY;
		iRecordCount++;
	}
	iRet = XUI_OK;
	if ( iRecordCount > 0 ) {
		iRet = xuiFlowGraphCommandRecordMoveNodes(pData->pGraph, pRecords, iRecordCount);
	}
	xrtFree(pRecords);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiFlowGraphWidgetStartMarquee(xui_widget pWidget, xui_flow_graph_widget_data_t* pData, float fX, float fY, uint32_t iModifiers)
{
	if ( (pWidget == NULL) || (pData == NULL) || (pData->pGraph == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiFlowGraphWidgetClearDrag(pData);
	pData->bDraggingMarquee = 1;
	pData->iMarqueeModifiers = iModifiers;
	pData->fDragStartX = fX;
	pData->fDragStartY = fY;
	pData->fDragCurrentX = fX;
	pData->fDragCurrentY = fY;
	return xuiSetPointerCapture(xuiWidgetGetContext(pWidget), pWidget);
}

static int __xuiFlowGraphWidgetUpdateMarquee(xui_widget pWidget, xui_flow_graph_widget_data_t* pData, float fX, float fY)
{
	if ( (pWidget == NULL) || (pData == NULL) || !pData->bDraggingMarquee ) {
		return XUI_OK;
	}
	pData->fDragCurrentX = fX;
	pData->fDragCurrentY = fY;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiFlowGraphWidgetFinishMarquee(xui_widget pWidget, xui_flow_graph_widget_data_t* pData)
{
	xui_flow_node_info_t tNode;
	xui_rect_t tContent;
	xui_rect_t tMarquee;
	xui_rect_t tNodeRect;
	int bToggle;
	int i;
	int iCount;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pGraph == NULL) || !pData->bDraggingMarquee ) {
		return XUI_OK;
	}
	tMarquee = __xuiFlowGraphWidgetRectFromPoints(pData->fDragStartX, pData->fDragStartY, pData->fDragCurrentX, pData->fDragCurrentY);
	if ( (tMarquee.fW < 2.0f) || (tMarquee.fH < 2.0f) ) {
		return XUI_OK;
	}
	bToggle = ((pData->iMarqueeModifiers & XUI_MOD_SHIFT) != 0u) ? 1 : 0;
	if ( (pData->iMarqueeModifiers & (XUI_MOD_CTRL | XUI_MOD_SHIFT)) == 0u ) {
		iRet = xuiFlowGraphClearSelection(pData->pGraph);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	tContent = xuiWidgetGetContentRect(pWidget);
	iCount = xuiFlowGraphGetNodeCount(pData->pGraph);
	for ( i = 0; i < iCount; ++i ) {
		if ( xuiFlowGraphGetNode(pData->pGraph, i, &tNode) != XUI_OK ) {
			continue;
		}
		tNodeRect = __xuiFlowGraphWidgetNodeRect(pData->pGraph, &tNode, tContent);
		if ( !__xuiFlowGraphWidgetRectsIntersect(tMarquee, tNodeRect) ) {
			continue;
		}
		iRet = xuiFlowGraphSelectNode(pData->pGraph, tNode.sId, bToggle ? !xuiFlowGraphIsNodeSelected(pData->pGraph, tNode.sId) : 1);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiFlowGraphWidgetStartConnectionDrag(xui_widget pWidget, xui_flow_graph_widget_data_t* pData, const xui_flow_hit_t* pHit)
{
	xui_flow_node_info_t tNode;
	xui_flow_port_info_t tPort;
	int iRet;

	if ( (pData == NULL) || (pData->pGraph == NULL) || (pHit == NULL) || (pHit->iNode < 0) || (pHit->iPort < 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = xuiFlowGraphGetNode(pData->pGraph, pHit->iNode, &tNode);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iRet = xuiFlowGraphGetPort(pData->pGraph, pHit->iNode, pHit->iPort, &tPort);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	__xuiFlowGraphWidgetClearDrag(pData);
	pData->sDragNodeId = __xuiFlowGraphWidgetCopyString(tNode.sId);
	pData->sDragPortId = __xuiFlowGraphWidgetCopyString(tPort.sId);
	if ( (pData->sDragNodeId == NULL) || (pData->sDragPortId == NULL) ) {
		__xuiFlowGraphWidgetClearDrag(pData);
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pData->bDraggingConnection = 1;
	pData->iDragPortKind = tPort.iKind;
	pData->iDragPortDirection = tPort.iDirection;
	pData->fDragStartX = pHit->fX;
	pData->fDragStartY = pHit->fY;
	pData->fDragCurrentX = pHit->fX;
	pData->fDragCurrentY = pHit->fY;
	pData->bDragConnectionValid = 0;
	return xuiSetPointerCapture(xuiWidgetGetContext(pWidget), pWidget);
}

static int __xuiFlowGraphWidgetUpdateConnectionPreview(xui_widget pWidget, xui_flow_graph_widget_data_t* pData, float fX, float fY)
{
	xui_flow_hit_t tHit;
	int iRet;

	if ( (pData == NULL) || !pData->bDraggingConnection || (pData->pGraph == NULL) ) {
		return XUI_OK;
	}
	pData->fDragCurrentX = fX;
	pData->fDragCurrentY = fY;
	memset(&tHit, 0, sizeof(tHit));
	tHit.iSize = sizeof(tHit);
	iRet = __xuiFlowGraphWidgetHitTestLocal(pWidget, fX, fY, &tHit);
	if ( iRet != XUI_OK ) {
		pData->bDragConnectionValid = 0;
		return iRet;
	}
	pData->bDragConnectionValid = __xuiFlowGraphWidgetConnectionTargetValid(pData, &tHit);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiFlowGraphWidgetFinishConnectionDrag(xui_widget pWidget, xui_flow_graph_widget_data_t* pData, float fX, float fY)
{
	xui_flow_hit_t tHit;
	xui_flow_node_info_t tNode;
	xui_flow_port_info_t tPort;
	xui_flow_edge_desc_t tEdge;
	char sEdgeId[64];
	int iRet;

	if ( (pData == NULL) || !pData->bDraggingConnection || (pData->pGraph == NULL) || (pData->sDragNodeId == NULL) || (pData->sDragPortId == NULL) ) {
		return XUI_OK;
	}
	memset(&tHit, 0, sizeof(tHit));
	tHit.iSize = sizeof(tHit);
	iRet = __xuiFlowGraphWidgetHitTestLocal(pWidget, fX, fY, &tHit);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( (tHit.iType != XUI_FLOW_HIT_PORT) || (tHit.iNode < 0) || (tHit.iPort < 0) ) {
		return XUI_OK;
	}
	if ( !__xuiFlowGraphWidgetConnectionTargetValid(pData, &tHit) ) {
		return XUI_OK;
	}
	iRet = xuiFlowGraphGetNode(pData->pGraph, tHit.iNode, &tNode);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iRet = xuiFlowGraphGetPort(pData->pGraph, tHit.iNode, tHit.iPort, &tPort);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iRet = xuiFlowGraphMakeEdgeId(pData->pGraph, sEdgeId, (int)sizeof(sEdgeId));
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	memset(&tEdge, 0, sizeof(tEdge));
	tEdge.iSize = sizeof(tEdge);
	tEdge.sId = sEdgeId;
	tEdge.iKind = pData->iDragPortKind;
	if ( pData->iDragPortDirection == XUI_FLOW_PORT_OUTPUT ) {
		tEdge.sFromNode = pData->sDragNodeId;
		tEdge.sFromPort = pData->sDragPortId;
		tEdge.sToNode = tNode.sId;
		tEdge.sToPort = tPort.sId;
	} else {
		tEdge.sFromNode = tNode.sId;
		tEdge.sFromPort = tPort.sId;
		tEdge.sToNode = pData->sDragNodeId;
		tEdge.sToPort = pData->sDragPortId;
	}
	iRet = xuiFlowGraphAddEdge(pData->pGraph, &tEdge, NULL);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiFlowGraphWidgetDeleteSelection(xui_widget pWidget, xui_flow_graph_widget_data_t* pData)
{
	int iHasSelection;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pGraph == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iHasSelection = (xuiFlowGraphGetSelectedNodeCount(pData->pGraph) > 0 || xuiFlowGraphGetSelectedEdgeCount(pData->pGraph) > 0);
	if ( !iHasSelection ) {
		return XUI_OK;
	}
	__xuiFlowGraphWidgetClearDrag(pData);
	iRet = xuiFlowGraphDeleteSelection(pData->pGraph);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiFlowGraphWidgetCancelDrag(xui_widget pWidget, xui_flow_graph_widget_data_t* pData)
{
	int i;
	int iRet;
	int bHadDrag;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pGraph == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	bHadDrag = (pData->bDraggingNode || pData->bDraggingConnection || pData->bDraggingMarquee) ? 1 : 0;
	if ( pData->bDraggingNode ) {
		for ( i = 0; i < pData->iDragNodeCount; ++i ) {
			if ( pData->arrDragNodes[i].sId == NULL ) {
				continue;
			}
			iRet = xuiFlowGraphSetNodePosition(pData->pGraph, pData->arrDragNodes[i].sId, pData->arrDragNodes[i].fStartX, pData->arrDragNodes[i].fStartY);
			if ( iRet != XUI_OK ) {
				return iRet;
			}
		}
	}
	__xuiFlowGraphWidgetClearDrag(pData);
	(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
	if ( bHadDrag ) {
		return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return XUI_OK;
}

static int __xuiFlowGraphWidgetEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_flow_graph_widget_data_t* pData;
	xui_flow_hit_t tHit;
	xui_flow_node_info_t tNode;
	float fLocalX;
	float fLocalY;
	uint32_t iStop;
	int bPreserveSelection;
	int iRet;

	(void)pUser;
	if ( (pWidget == NULL) || (pEvent == NULL) ) {
		return 0;
	}
	pData = __xuiFlowGraphWidgetGetData(pWidget);
	if ( (pData == NULL) || (pData->pGraph == NULL) || !xuiWidgetGetEnabled(pWidget) ) {
		return 0;
	}
	if ( pEvent->iType == XUI_EVENT_KEY_DOWN ) {
		if ( pEvent->iKey == XUI_KEY_ESCAPE ) {
			iRet = __xuiFlowGraphWidgetCancelDrag(pWidget, pData);
			return (iRet == XUI_OK) ? XUI_EVENT_DISPATCH_STOP : 0;
		}
		if ( (pEvent->iKey == XUI_KEY_DELETE) || (pEvent->iKey == XUI_KEY_BACKSPACE) ) {
			iRet = __xuiFlowGraphWidgetDeleteSelection(pWidget, pData);
			return (iRet == XUI_OK) ? XUI_EVENT_DISPATCH_STOP : 0;
		}
		return 0;
	}
	if ( pEvent->iType == XUI_EVENT_POINTER_DOWN ) {
		if ( pEvent->iButton != XUI_POINTER_BUTTON_LEFT ) {
			return 0;
		}
		__xuiFlowGraphWidgetPointToLocal(pWidget, pEvent->fX, pEvent->fY, &fLocalX, &fLocalY);
		(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
		memset(&tHit, 0, sizeof(tHit));
		tHit.iSize = sizeof(tHit);
		iRet = __xuiFlowGraphWidgetHitTestLocal(pWidget, fLocalX, fLocalY, &tHit);
		if ( iRet != XUI_OK ) {
			return 0;
		}
		bPreserveSelection = 0;
		if ( ((pEvent->iModifiers & (XUI_MOD_CTRL | XUI_MOD_SHIFT)) == 0u) &&
		     ((tHit.iType == XUI_FLOW_HIT_NODE) || (tHit.iType == XUI_FLOW_HIT_PORT)) &&
		     (xuiFlowGraphGetSelectedNodeCount(pData->pGraph) > 1) &&
		     (xuiFlowGraphGetNode(pData->pGraph, tHit.iNode, &tNode) == XUI_OK) &&
		     xuiFlowGraphIsNodeSelected(pData->pGraph, tNode.sId) ) {
			bPreserveSelection = 1;
		}
		if ( !bPreserveSelection ) {
			iRet = __xuiFlowGraphWidgetSelectAt(pWidget, pEvent->fX, pEvent->fY, pEvent->iModifiers, &tHit);
			if ( iRet != XUI_OK ) {
				return 0;
			}
		}
		if ( ((pEvent->iModifiers & (XUI_MOD_CTRL | XUI_MOD_SHIFT)) == 0u) && (tHit.iType == XUI_FLOW_HIT_PORT) ) {
			(void)__xuiFlowGraphWidgetStartConnectionDrag(pWidget, pData, &tHit);
		} else if ( ((pEvent->iModifiers & (XUI_MOD_CTRL | XUI_MOD_SHIFT)) == 0u) && (tHit.iType == XUI_FLOW_HIT_NODE) ) {
			(void)__xuiFlowGraphWidgetStartNodeDrag(pWidget, pData, &tHit);
		} else if ( tHit.iType == XUI_FLOW_HIT_BACKGROUND ) {
			(void)__xuiFlowGraphWidgetStartMarquee(pWidget, pData, fLocalX, fLocalY, pEvent->iModifiers);
		}
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( pEvent->iType == XUI_EVENT_POINTER_MOVE ) {
		__xuiFlowGraphWidgetPointToLocal(pWidget, pEvent->fX, pEvent->fY, &fLocalX, &fLocalY);
		if ( pData->bDraggingMarquee ) {
			(void)__xuiFlowGraphWidgetUpdateMarquee(pWidget, pData, fLocalX, fLocalY);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( pData->bDraggingConnection ) {
			(void)__xuiFlowGraphWidgetUpdateConnectionPreview(pWidget, pData, fLocalX, fLocalY);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( !pData->bDraggingNode ) {
			memset(&tHit, 0, sizeof(tHit));
			tHit.iSize = sizeof(tHit);
			if ( __xuiFlowGraphWidgetHitTestLocal(pWidget, fLocalX, fLocalY, &tHit) == XUI_OK ) {
				(void)__xuiFlowGraphWidgetSetHover(pWidget, pData, &tHit);
			}
			return 0;
		}
		iRet = __xuiFlowGraphWidgetDragNode(pWidget, pData, fLocalX, fLocalY);
		return (iRet == XUI_OK) ? XUI_EVENT_DISPATCH_STOP : 0;
	}
	if ( pEvent->iType == XUI_EVENT_POINTER_UP ) {
		if ( pData->bDraggingMarquee ) {
			iStop = XUI_EVENT_DISPATCH_STOP;
			(void)__xuiFlowGraphWidgetFinishMarquee(pWidget, pData);
			__xuiFlowGraphWidgetClearDrag(pData);
			(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
			return iStop;
		}
		if ( pData->bDraggingConnection ) {
			iStop = XUI_EVENT_DISPATCH_STOP;
			__xuiFlowGraphWidgetPointToLocal(pWidget, pEvent->fX, pEvent->fY, &fLocalX, &fLocalY);
			(void)__xuiFlowGraphWidgetFinishConnectionDrag(pWidget, pData, fLocalX, fLocalY);
			__xuiFlowGraphWidgetClearDrag(pData);
			(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
			return iStop;
		}
		if ( !pData->bDraggingNode ) {
			return 0;
		}
		iStop = XUI_EVENT_DISPATCH_STOP;
		(void)__xuiFlowGraphWidgetFinishNodeDrag(pWidget, pData);
		__xuiFlowGraphWidgetClearDrag(pData);
		(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
		return iStop;
	}
	if ( pEvent->iType == XUI_EVENT_POINTER_CAPTURE_LOST ) {
		__xuiFlowGraphWidgetClearDrag(pData);
		return XUI_EVENT_DISPATCH_STOP;
	}
	return 0;
}

XUI_API int xuiFlowGraphWidgetSelectAt(xui_widget pWidget, float fX, float fY, xui_flow_hit_t* pHit)
{
	return __xuiFlowGraphWidgetSelectAt(pWidget, fX, fY, 0u, pHit);
}
