#include "xui_internal.h"

#include <string.h>

#define XUI_TREE_VIEW_DEFAULT_ITEM_HEIGHT 24.0f
#define XUI_TREE_VIEW_DEFAULT_WIDTH 240.0f
#define XUI_TREE_VIEW_DEFAULT_ROWS 7
#define XUI_TREE_VIEW_ACTIVE_ROW 0
#define XUI_TREE_VIEW_ACTIVE_EXPANDER 1
#define XUI_TREE_VIEW_ACTIVE_CHECK 2

typedef struct xui_tree_view_data_t {
	xui_widget pFrame;
	xui_widget pViewport;
	xui_tree_view_node_t* arrNodes;
	int* arrVisible;
	int iNodeCount;
	int iNodeCapacity;
	int iVisibleCount;
	int iVisibleCapacity;
	xui_font pFont;
	xui_tree_view_select_proc onSelect;
	void* pSelectUser;
	xui_tree_view_count_proc onCount;
	xui_tree_view_node_proc onNode;
	void* pAdapterUser;
	xui_tree_view_item_proc onRenderItem;
	void* pRenderItemUser;
	int iSelectedId;
	int iHoverVisible;
	int iFocusVisible;
	int iActiveVisible;
	int iActivePart;
	int iChangeCount;
	int iSelectCount;
	float fItemHeight;
	float fIndent;
	float fPadding;
	float fBorderWidth;
	uint32_t iBackgroundColor;
	uint32_t iBorderColor;
	uint32_t iFocusColor;
	uint32_t iRowColor;
	uint32_t iHoverColor;
	uint32_t iSelectedColor;
	uint32_t iTextColor;
	uint32_t iDisabledTextColor;
	uint32_t iExpanderColor;
	uint32_t iIconColor;
	uint32_t iCheckColor;
	uint32_t iTrackColor;
	uint32_t iThumbColor;
	uint32_t iScrollbarHoverColor;
	uint32_t iScrollbarActiveColor;
	uint32_t iScrollbarFocusColor;
	uint32_t iScrollbarDisabledColor;
} xui_tree_view_data_t;

static xui_tree_view_data_t* __xuiTreeViewGetData(xui_widget pWidget);

static int __xuiTreeViewFloatValid(float fValue)
{
	return (fValue == fValue) && (fValue >= 0.0f) && (fValue <= XUI_LAYOUT_UNBOUNDED);
}

static int __xuiTreeViewDescValid(const xui_tree_view_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) {
		return 0;
	}
	if ( pDesc->iNodeCount < 0 ) {
		return 0;
	}
	if ( ((pDesc->fItemHeight != 0.0f) && !__xuiTreeViewFloatValid(pDesc->fItemHeight)) ||
	     ((pDesc->fIndent != 0.0f) && !__xuiTreeViewFloatValid(pDesc->fIndent)) ||
	     ((pDesc->fPadding != 0.0f) && !__xuiTreeViewFloatValid(pDesc->fPadding)) ||
	     ((pDesc->fBorderWidth != 0.0f) && !__xuiTreeViewFloatValid(pDesc->fBorderWidth)) ) {
		return 0;
	}
	return 1;
}

static int __xuiTreeViewScrollbarModeValid(int iMode)
{
	return (iMode == XUI_SCROLLBAR_MODE_FULL) || (iMode == XUI_SCROLLBAR_MODE_COMPACT);
}

static int __xuiTreeViewAlpha(uint32_t iColor)
{
	return (int)(iColor & 0xffu);
}

static float __xuiTreeViewMaxFloat(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static float __xuiTreeViewMinFloat(float fA, float fB)
{
	return (fA < fB) ? fA : fB;
}

static const char* __xuiTreeViewText(const char* sText)
{
	return (sText != NULL) ? sText : "";
}

static char* __xuiTreeViewDupText(const char* sText)
{
	char* sCopy;
	size_t iLen;

	if ( sText == NULL ) {
		sText = "";
	}
	iLen = strlen(sText);
	sCopy = (char*)xrtMalloc(iLen + 1u);
	if ( sCopy == NULL ) {
		return NULL;
	}
	memcpy(sCopy, sText, iLen + 1u);
	return sCopy;
}

static void __xuiTreeViewFreeNodeText(xui_tree_view_node_t* pNode)
{
	if ( (pNode != NULL) && (pNode->sText != NULL) ) {
		xrtFree((void*)pNode->sText);
		pNode->sText = NULL;
	}
}

static void __xuiTreeViewClearNodeStorage(xui_tree_view_data_t* pData)
{
	int i;

	if ( pData == NULL ) {
		return;
	}
	for ( i = 0; i < pData->iNodeCount; i++ ) {
		__xuiTreeViewFreeNodeText(&pData->arrNodes[i]);
	}
	if ( pData->arrNodes != NULL ) {
		xrtFree(pData->arrNodes);
	}
	if ( pData->arrVisible != NULL ) {
		xrtFree(pData->arrVisible);
	}
	pData->arrNodes = NULL;
	pData->arrVisible = NULL;
	pData->iNodeCount = 0;
	pData->iNodeCapacity = 0;
	pData->iVisibleCount = 0;
	pData->iVisibleCapacity = 0;
}

static void __xuiTreeViewDefaults(xui_tree_view_data_t* pData)
{
	memset(pData, 0, sizeof(*pData));
	pData->iSelectedId = -1;
	pData->iHoverVisible = -1;
	pData->iFocusVisible = -1;
	pData->iActiveVisible = -1;
	pData->iActivePart = XUI_TREE_VIEW_ACTIVE_ROW;
	pData->fItemHeight = XUI_TREE_VIEW_DEFAULT_ITEM_HEIGHT;
	pData->fIndent = 18.0f;
	pData->fPadding = 8.0f;
	pData->fBorderWidth = 1.0f;
	pData->iBackgroundColor = XUI_COLOR_RGBA(248, 252, 255, 255);
	pData->iBorderColor = XUI_COLOR_RGBA(132, 174, 214, 255);
	pData->iFocusColor = XUI_COLOR_RGBA(47, 128, 237, 255);
	pData->iRowColor = XUI_COLOR_RGBA(255, 255, 255, 0);
	pData->iHoverColor = XUI_COLOR_RGBA(231, 243, 253, 255);
	pData->iSelectedColor = XUI_COLOR_RGBA(47, 128, 237, 255);
	pData->iTextColor = XUI_COLOR_RGBA(31, 50, 73, 255);
	pData->iDisabledTextColor = XUI_COLOR_RGBA(132, 146, 162, 210);
	pData->iExpanderColor = XUI_COLOR_RGBA(61, 91, 125, 240);
	pData->iIconColor = XUI_COLOR_RGBA(98, 139, 181, 240);
	pData->iCheckColor = XUI_COLOR_RGBA(47, 128, 237, 255);
	pData->iTrackColor = XUI_COLOR_RGBA(224, 234, 244, 255);
	pData->iThumbColor = XUI_COLOR_RGBA(126, 161, 196, 245);
	pData->iScrollbarHoverColor = XUI_COLOR_RGBA(76, 136, 204, 250);
	pData->iScrollbarActiveColor = XUI_COLOR_RGBA(47, 128, 237, 255);
	pData->iScrollbarFocusColor = XUI_COLOR_RGBA(47, 128, 237, 180);
	pData->iScrollbarDisabledColor = XUI_COLOR_RGBA(181, 190, 204, 135);
}

static void __xuiTreeViewApplyDesc(xui_tree_view_data_t* pData, const xui_tree_view_desc_t* pDesc)
{
	if ( (pData == NULL) || (pDesc == NULL) ) {
		return;
	}
	pData->pFont = pDesc->pFont;
	if ( pDesc->fItemHeight > 0.0f ) pData->fItemHeight = pDesc->fItemHeight;
	if ( pDesc->fIndent > 0.0f ) pData->fIndent = pDesc->fIndent;
	if ( pDesc->fPadding > 0.0f ) pData->fPadding = pDesc->fPadding;
	if ( pDesc->fBorderWidth > 0.0f ) pData->fBorderWidth = pDesc->fBorderWidth;
	if ( pDesc->iSelectedId >= 0 ) pData->iSelectedId = pDesc->iSelectedId;
	if ( __xuiTreeViewAlpha(pDesc->iBackgroundColor) != 0 ) pData->iBackgroundColor = pDesc->iBackgroundColor;
	if ( __xuiTreeViewAlpha(pDesc->iBorderColor) != 0 ) pData->iBorderColor = pDesc->iBorderColor;
	if ( __xuiTreeViewAlpha(pDesc->iFocusColor) != 0 ) pData->iFocusColor = pDesc->iFocusColor;
	if ( pDesc->iRowColor != 0 ) pData->iRowColor = pDesc->iRowColor;
	if ( __xuiTreeViewAlpha(pDesc->iHoverColor) != 0 ) pData->iHoverColor = pDesc->iHoverColor;
	if ( __xuiTreeViewAlpha(pDesc->iSelectedColor) != 0 ) pData->iSelectedColor = pDesc->iSelectedColor;
	if ( __xuiTreeViewAlpha(pDesc->iTextColor) != 0 ) pData->iTextColor = pDesc->iTextColor;
	if ( __xuiTreeViewAlpha(pDesc->iDisabledTextColor) != 0 ) pData->iDisabledTextColor = pDesc->iDisabledTextColor;
	if ( __xuiTreeViewAlpha(pDesc->iExpanderColor) != 0 ) pData->iExpanderColor = pDesc->iExpanderColor;
	if ( __xuiTreeViewAlpha(pDesc->iIconColor) != 0 ) pData->iIconColor = pDesc->iIconColor;
	if ( __xuiTreeViewAlpha(pDesc->iCheckColor) != 0 ) pData->iCheckColor = pDesc->iCheckColor;
	if ( __xuiTreeViewAlpha(pDesc->iTrackColor) != 0 ) pData->iTrackColor = pDesc->iTrackColor;
	if ( __xuiTreeViewAlpha(pDesc->iThumbColor) != 0 ) pData->iThumbColor = pDesc->iThumbColor;
	if ( __xuiTreeViewAlpha(pDesc->iScrollbarHoverColor) != 0 ) pData->iScrollbarHoverColor = pDesc->iScrollbarHoverColor;
	if ( __xuiTreeViewAlpha(pDesc->iScrollbarActiveColor) != 0 ) pData->iScrollbarActiveColor = pDesc->iScrollbarActiveColor;
	if ( __xuiTreeViewAlpha(pDesc->iScrollbarFocusColor) != 0 ) pData->iScrollbarFocusColor = pDesc->iScrollbarFocusColor;
	if ( __xuiTreeViewAlpha(pDesc->iScrollbarDisabledColor) != 0 ) pData->iScrollbarDisabledColor = pDesc->iScrollbarDisabledColor;
}

static int __xuiTreeViewStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
{
	xui_style_property_t tProperty;
	int iRet;

	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_COLOR) ) {
		*pColor = tProperty.tValue.iColor;
		return 1;
	}
	return 0;
}

static int __xuiTreeViewStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
{
	xui_style_property_t tProperty;
	int iRet;

	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_FLOAT) && __xuiTreeViewFloatValid(tProperty.tValue.fFloat) ) {
		*pValue = tProperty.tValue.fFloat;
		return 1;
	}
	return 0;
}

static xui_font __xuiTreeViewStyleFont(xui_widget pWidget, xui_font pBaseFont)
{
	xui_style_property_t tProperty;
	xui_font pFont;
	int iRet;

	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, "font.name", &tProperty);
	if ( (iRet == XUI_OK) &&
	     (tProperty.tValue.iType == XUI_STYLE_VALUE_STRING) &&
	     (tProperty.tValue.sText != NULL) ) {
		pFont = xuiFindFont(xuiWidgetGetContext(pWidget), tProperty.tValue.sText);
		if ( pFont != NULL ) {
			return pFont;
		}
	}
	return pBaseFont;
}

static void __xuiTreeViewResolve(xui_widget pWidget, xui_tree_view_data_t* pData, xui_tree_view_data_t* pResolved)
{
	*pResolved = *pData;
	pResolved->pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	pResolved->pFont = __xuiTreeViewStyleFont(pWidget, pResolved->pFont);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.background.color", &pResolved->iBackgroundColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.border.color", &pResolved->iBorderColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.focus.color", &pResolved->iFocusColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.row.color", &pResolved->iRowColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.row.hover_color", &pResolved->iHoverColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.row.selected_color", &pResolved->iSelectedColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.text.color", &pResolved->iTextColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.text.disabled_color", &pResolved->iDisabledTextColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.expander.color", &pResolved->iExpanderColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.icon.color", &pResolved->iIconColor);
	(void)__xuiTreeViewStyleColor(pWidget, "treeview.check.color", &pResolved->iCheckColor);
	(void)__xuiTreeViewStyleFloat(pWidget, "treeview.item.height", &pResolved->fItemHeight);
	(void)__xuiTreeViewStyleFloat(pWidget, "treeview.indent", &pResolved->fIndent);
	(void)__xuiTreeViewStyleFloat(pWidget, "treeview.padding", &pResolved->fPadding);
	(void)__xuiTreeViewStyleFloat(pWidget, "treeview.border.width", &pResolved->fBorderWidth);
	if ( pResolved->fItemHeight < 1.0f ) {
		pResolved->fItemHeight = XUI_TREE_VIEW_DEFAULT_ITEM_HEIGHT;
	}
}

static int __xuiTreeViewEnsureNodeCapacity(xui_tree_view_data_t* pData, int iCount)
{
	xui_tree_view_node_t* pNewNodes;
	int iNewCapacity;

	if ( pData == NULL || iCount < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( iCount <= pData->iNodeCapacity ) {
		return XUI_OK;
	}
	iNewCapacity = (pData->iNodeCapacity > 0) ? pData->iNodeCapacity * 2 : 16;
	while ( iNewCapacity < iCount ) {
		iNewCapacity *= 2;
	}
	pNewNodes = (xui_tree_view_node_t*)xrtRealloc(pData->arrNodes, (size_t)iNewCapacity * sizeof(*pNewNodes));
	if ( pNewNodes == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pData->arrNodes = pNewNodes;
	pData->iNodeCapacity = iNewCapacity;
	return XUI_OK;
}

static int __xuiTreeViewEnsureVisibleCapacity(xui_tree_view_data_t* pData, int iCount)
{
	int* pNewVisible;
	int iNewCapacity;

	if ( pData == NULL || iCount < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( iCount <= pData->iVisibleCapacity ) {
		return XUI_OK;
	}
	iNewCapacity = (pData->iVisibleCapacity > 0) ? pData->iVisibleCapacity * 2 : 16;
	while ( iNewCapacity < iCount ) {
		iNewCapacity *= 2;
	}
	pNewVisible = (int*)xrtRealloc(pData->arrVisible, (size_t)iNewCapacity * sizeof(*pNewVisible));
	if ( pNewVisible == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pData->arrVisible = pNewVisible;
	pData->iVisibleCapacity = iNewCapacity;
	return XUI_OK;
}

static int __xuiTreeViewFindNodeData(const xui_tree_view_data_t* pData, int iNodeId)
{
	int i;

	if ( (pData == NULL) || (iNodeId < 0) ) {
		return -1;
	}
	for ( i = 0; i < pData->iNodeCount; i++ ) {
		if ( pData->arrNodes[i].iId == iNodeId ) {
			return i;
		}
	}
	return -1;
}

static int __xuiTreeViewVisibleIndexData(const xui_tree_view_data_t* pData, int iNodeId)
{
	int i;

	if ( (pData == NULL) || (iNodeId < 0) ) {
		return -1;
	}
	for ( i = 0; i < pData->iVisibleCount; i++ ) {
		if ( pData->arrNodes[pData->arrVisible[i]].iId == iNodeId ) {
			return i;
		}
	}
	return -1;
}

static int __xuiTreeViewNodeEnabledData(const xui_tree_view_data_t* pData, int iNode)
{
	if ( (pData == NULL) || (iNode < 0) || (iNode >= pData->iNodeCount) ) {
		return 0;
	}
	return pData->arrNodes[iNode].bEnabled != 0;
}

static int __xuiTreeViewAppendNodeData(xui_tree_view_data_t* pData, const xui_tree_view_node_t* pSource, int bDefaultEnabled)
{
	xui_tree_view_node_t* pNode;
	int iRet;

	if ( (pData == NULL) || (pSource == NULL) || (pSource->iId < 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( __xuiTreeViewFindNodeData(pData, pSource->iId) >= 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pSource->iParent >= 0) && (__xuiTreeViewFindNodeData(pData, pSource->iParent) < 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiTreeViewEnsureNodeCapacity(pData, pData->iNodeCount + 1);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pNode = &pData->arrNodes[pData->iNodeCount];
	memset(pNode, 0, sizeof(*pNode));
	*pNode = *pSource;
	pNode->sText = __xuiTreeViewDupText(pSource->sText);
	if ( pNode->sText == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	(void)bDefaultEnabled;
	pNode->bEnabled = pSource->bEnabled ? 1 : 0;
	pNode->bExpanded = pSource->bExpanded ? 1 : 0;
	pNode->bHasChildren = 0;
	pNode->bIconReserved = pSource->bIconReserved ? 1 : 0;
	pNode->bCheckReserved = pSource->bCheckReserved ? 1 : 0;
	pNode->bChecked = pSource->bChecked ? 1 : 0;
	pData->iNodeCount++;
	return XUI_OK;
}

static int __xuiTreeViewUpdateContentSize(xui_widget pWidget, xui_tree_view_data_t* pData)
{
	xui_rect_t tFrame;
	float fHeight;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pFrame == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	tFrame = xuiWidgetGetContentRect(pWidget);
	if ( pData->fBorderWidth > 0.0f ) {
		tFrame = xuiInternalInsetRect(tFrame, pData->fBorderWidth);
	}
	if ( tFrame.fW < 0.0f ) tFrame.fW = 0.0f;
	if ( tFrame.fH < 0.0f ) tFrame.fH = 0.0f;
	fHeight = pData->fItemHeight * (float)pData->iVisibleCount;
	iRet = xuiScrollFrameSetContentSize(pData->pFrame, __xuiTreeViewMaxFloat(0.0f, tFrame.fW), __xuiTreeViewMaxFloat(0.0f, fHeight));
	if ( iRet != XUI_OK ) return iRet;
	return xuiScrollFrameSetWheelStep(pData->pFrame, pData->fItemHeight * 3.0f);
}

static int __xuiTreeViewInvalidateRows(xui_widget pWidget, xui_tree_view_data_t* pData)
{
	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->pViewport != NULL ) {
		(void)xuiWidgetInvalidate(pData->pViewport, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiTreeViewInvalidateViewportRow(xui_tree_view_data_t* pData, int iVisible)
{
	xui_rect_t tViewport;
	xui_rect_t tRow;
	float fOffsetY;
	float fTop;
	float fBottom;

	if ( (pData == NULL) || (pData->pViewport == NULL) || (pData->fItemHeight <= 0.0f) ) {
		return XUI_OK;
	}
	if ( (iVisible < 0) || (iVisible >= pData->iVisibleCount) ) {
		return XUI_OK;
	}
	tViewport = xuiWidgetGetRect(pData->pViewport);
	if ( (tViewport.fW <= 0.0f) || (tViewport.fH <= 0.0f) ) {
		return XUI_OK;
	}
	fOffsetY = 0.0f;
	(void)xuiScrollFrameGetOffset(pData->pFrame, NULL, &fOffsetY);
	fTop = (float)iVisible * pData->fItemHeight - fOffsetY;
	fBottom = fTop + pData->fItemHeight;
	if ( (fBottom <= 0.0f) || (fTop >= tViewport.fH) ) {
		return XUI_OK;
	}
	tRow.fX = 0.0f;
	tRow.fY = __xuiTreeViewMaxFloat(0.0f, fTop);
	tRow.fW = tViewport.fW;
	tRow.fH = __xuiTreeViewMinFloat(tViewport.fH, fBottom) - tRow.fY;
	if ( tRow.fH <= 0.0f ) {
		return XUI_OK;
	}
	return xuiWidgetInvalidateRect(pData->pViewport, tRow, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiTreeViewInvalidateHoverRows(xui_widget pWidget, xui_tree_view_data_t* pData, int iOldHover, int iNewHover)
{
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->pViewport == NULL ) {
		return __xuiTreeViewInvalidateRows(pWidget, pData);
	}
	iRet = __xuiTreeViewInvalidateViewportRow(pData, iOldHover);
	if ( iRet == XUI_OK ) {
		iRet = __xuiTreeViewInvalidateViewportRow(pData, iNewHover);
	}
	return iRet;
}

static int __xuiTreeViewAppendVisibleNode(xui_tree_view_data_t* pData, int iNode, int iDepth)
{
	int i;
	int iRet;
	int iId;

	if ( (pData == NULL) || (iNode < 0) || (iNode >= pData->iNodeCount) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiTreeViewEnsureVisibleCapacity(pData, pData->iVisibleCount + 1);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pData->arrNodes[iNode].iDepth = iDepth;
	pData->arrVisible[pData->iVisibleCount++] = iNode;
	if ( pData->arrNodes[iNode].bExpanded == 0 ) {
		return XUI_OK;
	}
	iId = pData->arrNodes[iNode].iId;
	for ( i = 0; i < pData->iNodeCount; i++ ) {
		if ( pData->arrNodes[i].iParent == iId ) {
			iRet = __xuiTreeViewAppendVisibleNode(pData, i, iDepth + 1);
			if ( iRet != XUI_OK ) {
				return iRet;
			}
		}
	}
	return XUI_OK;
}

static void __xuiTreeViewRecomputeChildren(xui_tree_view_data_t* pData)
{
	int i;
	int iParent;

	if ( pData == NULL ) {
		return;
	}
	for ( i = 0; i < pData->iNodeCount; i++ ) {
		pData->arrNodes[i].bHasChildren = 0;
	}
	for ( i = 0; i < pData->iNodeCount; i++ ) {
		iParent = __xuiTreeViewFindNodeData(pData, pData->arrNodes[i].iParent);
		if ( iParent >= 0 ) {
			pData->arrNodes[iParent].bHasChildren = 1;
		}
	}
}

static int __xuiTreeViewRebuildVisible(xui_widget pWidget, xui_tree_view_data_t* pData, int iFallbackNodeId)
{
	int i;
	int iVisible;
	int iNode;
	int iRet;

	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->iVisibleCount = 0;
	pData->iHoverVisible = -1;
	__xuiTreeViewRecomputeChildren(pData);
	for ( i = 0; i < pData->iNodeCount; i++ ) {
		if ( pData->arrNodes[i].iParent < 0 ) {
			iRet = __xuiTreeViewAppendVisibleNode(pData, i, 0);
			if ( iRet != XUI_OK ) {
				return iRet;
			}
		}
	}
	iVisible = __xuiTreeViewVisibleIndexData(pData, pData->iSelectedId);
	iNode = (iVisible >= 0) ? pData->arrVisible[iVisible] : -1;
	if ( (iVisible < 0) || !__xuiTreeViewNodeEnabledData(pData, iNode) ) {
		iVisible = __xuiTreeViewVisibleIndexData(pData, iFallbackNodeId);
		iNode = (iVisible >= 0) ? pData->arrVisible[iVisible] : -1;
		if ( (iVisible >= 0) && __xuiTreeViewNodeEnabledData(pData, iNode) ) {
			pData->iSelectedId = iFallbackNodeId;
		} else {
			pData->iSelectedId = -1;
			iVisible = -1;
		}
	}
	pData->iFocusVisible = iVisible;
	if ( pWidget != NULL && pData->pFrame != NULL ) {
		(void)__xuiTreeViewUpdateContentSize(pWidget, pData);
		(void)__xuiTreeViewInvalidateRows(pWidget, pData);
	}
	return XUI_OK;
}

static uint32_t __xuiTreeViewState(xui_widget pWidget)
{
	uint32_t iState;

	iState = xuiWidgetGetInputState(pWidget);
	if ( !xuiWidgetGetEnabled(pWidget) ) {
		iState |= XUI_WIDGET_STATE_DISABLED;
	}
	if ( xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget ) {
		iState |= XUI_WIDGET_STATE_FOCUS;
	}
	return iState;
}

static int __xuiTreeViewSyncState(xui_widget pWidget)
{
	return xuiWidgetSetStateId(pWidget, __xuiTreeViewState(pWidget));
}

static void __xuiTreeViewNotifySelect(xui_widget pWidget, xui_tree_view_data_t* pData, int iNodeId)
{
	if ( (pWidget == NULL) || (pData == NULL) || (iNodeId < 0) ) {
		return;
	}
	pData->iSelectCount++;
	if ( pData->onSelect != NULL ) {
		pData->onSelect(pWidget, iNodeId, pData->pSelectUser);
	}
}

static int __xuiTreeViewEnsureVisibleIndex(xui_widget pWidget, xui_tree_view_data_t* pData, int iVisible)
{
	xui_rect_t tRect;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pFrame == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (iVisible < 0) || (iVisible >= pData->iVisibleCount) ) {
		return XUI_OK;
	}
	tRect.fX = 0.0f;
	tRect.fY = (float)iVisible * pData->fItemHeight;
	tRect.fW = 1.0f;
	tRect.fH = pData->fItemHeight;
	return xuiScrollFrameEnsureRectVisible(pData->pFrame, tRect);
}

static int __xuiTreeViewSetSelectedVisible(xui_widget pWidget, xui_tree_view_data_t* pData, int iVisible, int bNotify)
{
	int iNode;
	int iNodeId;
	int iOld;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (iVisible < 0) || (iVisible >= pData->iVisibleCount) ) {
		iNodeId = -1;
	} else {
		iNode = pData->arrVisible[iVisible];
		if ( !__xuiTreeViewNodeEnabledData(pData, iNode) ) {
			iNodeId = -1;
			iVisible = -1;
		} else {
			iNodeId = pData->arrNodes[iNode].iId;
		}
	}
	iOld = pData->iSelectedId;
	pData->iSelectedId = iNodeId;
	pData->iFocusVisible = iVisible;
	if ( iOld != iNodeId ) {
		pData->iChangeCount++;
	}
	if ( iVisible >= 0 ) {
		(void)__xuiTreeViewEnsureVisibleIndex(pWidget, pData, iVisible);
	}
	(void)__xuiTreeViewInvalidateRows(pWidget, pData);
	if ( bNotify && iNodeId >= 0 ) {
		__xuiTreeViewNotifySelect(pWidget, pData, iNodeId);
	}
	return XUI_OK;
}

static int __xuiTreeViewFindEnabledVisible(const xui_tree_view_data_t* pData, int iStart, int iStep)
{
	int i;
	int iNode;

	if ( (pData == NULL) || (iStep == 0) ) {
		return -1;
	}
	for ( i = iStart; (i >= 0) && (i < pData->iVisibleCount); i += iStep ) {
		iNode = pData->arrVisible[i];
		if ( __xuiTreeViewNodeEnabledData(pData, iNode) ) {
			return i;
		}
	}
	return -1;
}

static int __xuiTreeViewFindNearestEnabledVisible(const xui_tree_view_data_t* pData, int iIndex, int iStep)
{
	int iResult;

	if ( pData == NULL ) {
		return -1;
	}
	if ( iIndex < 0 ) iIndex = 0;
	if ( iIndex >= pData->iVisibleCount ) iIndex = pData->iVisibleCount - 1;
	iResult = __xuiTreeViewFindEnabledVisible(pData, iIndex, iStep);
	if ( iResult >= 0 ) {
		return iResult;
	}
	return __xuiTreeViewFindEnabledVisible(pData, iIndex, -iStep);
}

static xui_rect_t __xuiTreeViewFrameRect(xui_widget pWidget, const xui_tree_view_data_t* pData)
{
	xui_rect_t tRect;
	float fInset;

	tRect = xuiWidgetGetContentRect(pWidget);
	fInset = (pData != NULL && pData->fBorderWidth > 0.0f) ? pData->fBorderWidth : 0.0f;
	if ( fInset > 0.0f ) {
		tRect = xuiInternalInsetRect(tRect, fInset);
	}
	if ( tRect.fW < 0.0f ) tRect.fW = 0.0f;
	if ( tRect.fH < 0.0f ) tRect.fH = 0.0f;
	return xuiInternalSnapRect(tRect);
}

static xui_rect_t __xuiTreeViewViewportRectLocal(xui_widget pWidget, const xui_tree_view_data_t* pData)
{
	xui_rect_t tFrame;
	xui_rect_t tViewport;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pFrame == NULL) ) {
		return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	}
	(void)pWidget;
	tFrame = xuiWidgetGetRect(pData->pFrame);
	tViewport = xuiScrollFrameGetViewportRect(pData->pFrame);
	tViewport.fX += tFrame.fX;
	tViewport.fY += tFrame.fY;
	return tViewport;
}

static int __xuiTreeViewPointInRect(xui_rect_t tRect, float fX, float fY)
{
	return (fX >= tRect.fX) && (fY >= tRect.fY) &&
	       (fX < (tRect.fX + tRect.fW)) && (fY < (tRect.fY + tRect.fH));
}

static int __xuiTreeViewHitWorld(xui_widget pWidget, xui_tree_view_data_t* pData, float fX, float fY)
{
	xui_rect_t tViewportWorld;
	float fOffsetY;
	float fLocalY;
	int iVisible;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pViewport == NULL) || (pData->fItemHeight <= 0.0f) ) {
		return -1;
	}
	tViewportWorld = xuiWidgetGetWorldRect(pData->pViewport);
	if ( !__xuiTreeViewPointInRect(tViewportWorld, fX, fY) ) {
		return -1;
	}
	fOffsetY = 0.0f;
	(void)xuiScrollFrameGetOffset(pData->pFrame, NULL, &fOffsetY);
	fLocalY = fY - tViewportWorld.fY + fOffsetY;
	if ( fLocalY < 0.0f ) {
		return -1;
	}
	iVisible = (int)(fLocalY / pData->fItemHeight);
	if ( (iVisible < 0) || (iVisible >= pData->iVisibleCount) ) {
		return -1;
	}
	return iVisible;
}

static int __xuiTreeViewSetHover(xui_widget pWidget, xui_tree_view_data_t* pData, int iHover)
{
	int iOldHover;

	if ( (pData == NULL) || (pData->iHoverVisible == iHover) ) {
		return XUI_OK;
	}
	iOldHover = pData->iHoverVisible;
	pData->iHoverVisible = iHover;
	return __xuiTreeViewInvalidateHoverRows(pWidget, pData, iOldHover, iHover);
}

static xui_rect_t __xuiTreeViewExpanderRect(const xui_tree_view_data_t* pData, const xui_tree_view_node_t* pNode, xui_rect_t tRow)
{
	float fSize;
	float fX;
	float fY;

	fSize = 14.0f;
	fX = tRow.fX + pData->fPadding + (float)pNode->iDepth * pData->fIndent;
	fY = tRow.fY + (tRow.fH - fSize) * 0.5f;
	return xuiInternalSnapRect((xui_rect_t){fX, fY, fSize, fSize});
}

static xui_rect_t __xuiTreeViewCheckRect(const xui_tree_view_data_t* pData, const xui_tree_view_node_t* pNode, xui_rect_t tRow)
{
	xui_rect_t tExpander;
	float fSize;
	float fX;
	float fY;

	tExpander = __xuiTreeViewExpanderRect(pData, pNode, tRow);
	fSize = 12.0f;
	fX = tExpander.fX + tExpander.fW + 5.0f;
	fY = tRow.fY + (tRow.fH - fSize) * 0.5f;
	return xuiInternalSnapRect((xui_rect_t){fX, fY, fSize, fSize});
}

static int __xuiTreeViewHitExpander(xui_widget pWidget, xui_tree_view_data_t* pData, int iVisible, float fX, float fY)
{
	xui_rect_t tRow;
	xui_rect_t tWorld;
	xui_tree_view_node_t* pNode;
	float fX0;
	float fW;

	if ( (pData == NULL) || (iVisible < 0) || (iVisible >= pData->iVisibleCount) ) {
		return 0;
	}
	pNode = &pData->arrNodes[pData->arrVisible[iVisible]];
	if ( pNode->bHasChildren == 0 ) {
		return 0;
	}
	tRow = xuiTreeViewGetItemRect(pWidget, iVisible);
	tWorld = xuiWidgetGetWorldRect(pWidget);
	tRow.fX += tWorld.fX;
	tRow.fY += tWorld.fY;
	fX0 = tRow.fX + pData->fPadding + (float)pNode->iDepth * pData->fIndent - 2.0f;
	fW = __xuiTreeViewMaxFloat(22.0f, pData->fIndent + 4.0f);
	return (fX >= fX0) && (fX < fX0 + fW) && (fY >= tRow.fY) && (fY < tRow.fY + tRow.fH);
}

static int __xuiTreeViewHitCheck(xui_widget pWidget, xui_tree_view_data_t* pData, int iVisible, float fX, float fY)
{
	xui_rect_t tRow;
	xui_rect_t tCheck;
	xui_rect_t tWorld;
	xui_tree_view_node_t* pNode;

	if ( (pData == NULL) || (iVisible < 0) || (iVisible >= pData->iVisibleCount) ) {
		return 0;
	}
	pNode = &pData->arrNodes[pData->arrVisible[iVisible]];
	if ( pNode->bCheckReserved == 0 ) {
		return 0;
	}
	tRow = xuiTreeViewGetItemRect(pWidget, iVisible);
	tWorld = xuiWidgetGetWorldRect(pWidget);
	tRow.fX += tWorld.fX;
	tRow.fY += tWorld.fY;
	tCheck = __xuiTreeViewCheckRect(pData, pNode, tRow);
	tCheck.fX -= 4.0f;
	tCheck.fW += 8.0f;
	tCheck.fY = tRow.fY;
	tCheck.fH = tRow.fH;
	return __xuiTreeViewPointInRect(tCheck, fX, fY);
}

static int __xuiTreeViewApplyFrameStyle(xui_widget pWidget, xui_tree_view_data_t* pData)
{
	int iRet;

	(void)pWidget;
	if ( (pData == NULL) || (pData->pFrame == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = xuiScrollFrameSetScrollbarMode(pData->pFrame, xuiScrollFrameGetScrollbarMode(pData->pFrame));
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameSetScrollbarPolicy(pData->pFrame, XUI_SCROLLBAR_POLICY_HIDDEN, XUI_SCROLLBAR_POLICY_AUTO);
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameSetWheelAxis(pData->pFrame, XUI_WHEEL_AXIS_VERTICAL);
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameSetWheelStep(pData->pFrame, pData->fItemHeight * 3.0f);
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameSetContentDragEnabled(pData->pFrame, 0);
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameSetMetrics(pData->pFrame, 8.0f, 18.0f, 0.0f);
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameSetColors(pData->pFrame, pData->iTrackColor, pData->iThumbColor, pData->iScrollbarHoverColor, pData->iScrollbarActiveColor, pData->iScrollbarFocusColor, pData->iScrollbarDisabledColor);
	return iRet;
}

static int __xuiTreeViewPointerMove(xui_widget pWidget, xui_tree_view_data_t* pData, const xui_event_t* pEvent)
{
	int iHover;

	if ( (pEvent == NULL) || (pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) ) {
		return XUI_OK;
	}
	iHover = __xuiTreeViewHitWorld(pWidget, pData, pEvent->fX, pEvent->fY);
	return __xuiTreeViewSetHover(pWidget, pData, iHover);
}

static int __xuiTreeViewPointerDown(xui_widget pWidget, xui_tree_view_data_t* pData, const xui_event_t* pEvent)
{
	int iVisible;
	int iNode;

	if ( (pEvent == NULL) || (pEvent->iButton != XUI_POINTER_BUTTON_LEFT) || (pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) ) {
		return XUI_OK;
	}
	if ( !xuiWidgetGetEnabled(pWidget) ) {
		return XUI_OK;
	}
	iVisible = __xuiTreeViewHitWorld(pWidget, pData, pEvent->fX, pEvent->fY);
	if ( iVisible < 0 ) {
		return XUI_OK;
	}
	(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
	iNode = pData->arrVisible[iVisible];
	if ( !__xuiTreeViewNodeEnabledData(pData, iNode) ) {
		return XUI_EVENT_DISPATCH_STOP;
	}
	pData->iActiveVisible = iVisible;
	pData->iActivePart = XUI_TREE_VIEW_ACTIVE_ROW;
	if ( __xuiTreeViewHitExpander(pWidget, pData, iVisible, pEvent->fX, pEvent->fY) ) {
		pData->iActivePart = XUI_TREE_VIEW_ACTIVE_EXPANDER;
	} else if ( __xuiTreeViewHitCheck(pWidget, pData, iVisible, pEvent->fX, pEvent->fY) ) {
		pData->iActivePart = XUI_TREE_VIEW_ACTIVE_CHECK;
	}
	pData->iFocusVisible = iVisible;
	(void)__xuiTreeViewSetHover(pWidget, pData, iVisible);
	(void)xuiSetPointerCapture(xuiWidgetGetContext(pWidget), pWidget);
	(void)__xuiTreeViewInvalidateRows(pWidget, pData);
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiTreeViewPointerUp(xui_widget pWidget, xui_tree_view_data_t* pData, const xui_event_t* pEvent)
{
	xui_context pContext;
	int iVisible;
	int iNode;
	int iRet;

	if ( (pEvent == NULL) || (pEvent->iButton != XUI_POINTER_BUTTON_LEFT) || (pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) ) {
		return XUI_OK;
	}
	pContext = xuiWidgetGetContext(pWidget);
	if ( (pData->iActiveVisible < 0) || (xuiGetPointerCapture(pContext) != pWidget) ) {
		return XUI_OK;
	}
	iRet = XUI_OK;
	iVisible = __xuiTreeViewHitWorld(pWidget, pData, pEvent->fX, pEvent->fY);
	if ( iVisible == pData->iActiveVisible ) {
		iNode = pData->arrVisible[iVisible];
		if ( __xuiTreeViewNodeEnabledData(pData, iNode) ) {
			if ( (pData->iActivePart == XUI_TREE_VIEW_ACTIVE_EXPANDER) && (pData->arrNodes[iNode].bHasChildren != 0) ) {
				pData->arrNodes[iNode].bExpanded = !pData->arrNodes[iNode].bExpanded;
				pData->iChangeCount++;
				iRet = __xuiTreeViewRebuildVisible(pWidget, pData, pData->arrNodes[iNode].bExpanded ? -1 : pData->arrNodes[iNode].iId);
			} else if ( (pData->iActivePart == XUI_TREE_VIEW_ACTIVE_CHECK) && (pData->arrNodes[iNode].bCheckReserved != 0) ) {
				pData->arrNodes[iNode].bChecked = !pData->arrNodes[iNode].bChecked;
				pData->iChangeCount++;
				iRet = __xuiTreeViewInvalidateRows(pWidget, pData);
			} else {
				iRet = __xuiTreeViewSetSelectedVisible(pWidget, pData, iVisible, 1);
			}
		}
	}
	pData->iActiveVisible = -1;
	pData->iActivePart = XUI_TREE_VIEW_ACTIVE_ROW;
	(void)xuiReleasePointerCapture(pContext, pWidget);
	(void)__xuiTreeViewInvalidateRows(pWidget, pData);
	return (iRet == XUI_OK) ? (int)XUI_EVENT_DISPATCH_STOP : iRet;
}

static int __xuiTreeViewVisibleRows(xui_widget pWidget, xui_tree_view_data_t* pData)
{
	xui_rect_t tViewport;
	int iRows;

	tViewport = xuiTreeViewGetViewportRect(pWidget);
	iRows = (pData->fItemHeight > 0.0f) ? (int)(tViewport.fH / pData->fItemHeight) : 1;
	return (iRows > 0) ? iRows : 1;
}

static int __xuiTreeViewKeyDown(xui_widget pWidget, xui_tree_view_data_t* pData, const xui_event_t* pEvent)
{
	int iCurrent;
	int iTarget;
	int iNode;
	int iParentVisible;
	int iRows;

	if ( (pEvent == NULL) || (pEvent->iPhase == XUI_EVENT_PHASE_BUBBLE) || !xuiWidgetGetEnabled(pWidget) || (pData->iVisibleCount <= 0) ) {
		return XUI_OK;
	}
	iCurrent = __xuiTreeViewVisibleIndexData(pData, pData->iSelectedId);
	if ( iCurrent < 0 ) {
		iCurrent = pData->iFocusVisible;
	}
	switch ( pEvent->iKey ) {
	case XUI_KEY_UP:
		iTarget = (iCurrent >= 0) ? __xuiTreeViewFindEnabledVisible(pData, iCurrent - 1, -1) : __xuiTreeViewFindEnabledVisible(pData, 0, 1);
		if ( iTarget >= 0 ) {
			(void)__xuiTreeViewSetSelectedVisible(pWidget, pData, iTarget, 1);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_KEY_DOWN:
		iTarget = (iCurrent >= 0) ? __xuiTreeViewFindEnabledVisible(pData, iCurrent + 1, 1) : __xuiTreeViewFindEnabledVisible(pData, 0, 1);
		if ( iTarget >= 0 ) {
			(void)__xuiTreeViewSetSelectedVisible(pWidget, pData, iTarget, 1);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_KEY_PAGE_UP:
		iRows = __xuiTreeViewVisibleRows(pWidget, pData);
		iTarget = (iCurrent >= 0) ? __xuiTreeViewFindNearestEnabledVisible(pData, iCurrent - iRows, -1) : __xuiTreeViewFindEnabledVisible(pData, 0, 1);
		if ( iTarget >= 0 ) {
			(void)__xuiTreeViewSetSelectedVisible(pWidget, pData, iTarget, 1);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_KEY_PAGE_DOWN:
		iRows = __xuiTreeViewVisibleRows(pWidget, pData);
		iTarget = (iCurrent >= 0) ? __xuiTreeViewFindNearestEnabledVisible(pData, iCurrent + iRows, 1) : __xuiTreeViewFindEnabledVisible(pData, 0, 1);
		if ( iTarget >= 0 ) {
			(void)__xuiTreeViewSetSelectedVisible(pWidget, pData, iTarget, 1);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_KEY_HOME:
		iTarget = __xuiTreeViewFindEnabledVisible(pData, 0, 1);
		if ( iTarget >= 0 ) {
			(void)__xuiTreeViewSetSelectedVisible(pWidget, pData, iTarget, 1);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_KEY_END:
		iTarget = __xuiTreeViewFindEnabledVisible(pData, pData->iVisibleCount - 1, -1);
		if ( iTarget >= 0 ) {
			(void)__xuiTreeViewSetSelectedVisible(pWidget, pData, iTarget, 1);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_KEY_RIGHT:
		if ( iCurrent >= 0 ) {
			iNode = pData->arrVisible[iCurrent];
			if ( (pData->arrNodes[iNode].bHasChildren != 0) && (pData->arrNodes[iNode].bExpanded == 0) ) {
				pData->arrNodes[iNode].bExpanded = 1;
				pData->iChangeCount++;
				(void)__xuiTreeViewRebuildVisible(pWidget, pData, -1);
			} else if ( pData->arrNodes[iNode].bHasChildren != 0 ) {
				iTarget = __xuiTreeViewFindEnabledVisible(pData, iCurrent + 1, 1);
				if ( iTarget >= 0 ) {
					(void)__xuiTreeViewSetSelectedVisible(pWidget, pData, iTarget, 1);
				}
			}
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_KEY_LEFT:
		if ( iCurrent >= 0 ) {
			iNode = pData->arrVisible[iCurrent];
			if ( (pData->arrNodes[iNode].bHasChildren != 0) && (pData->arrNodes[iNode].bExpanded != 0) ) {
				pData->arrNodes[iNode].bExpanded = 0;
				pData->iChangeCount++;
				(void)__xuiTreeViewRebuildVisible(pWidget, pData, pData->arrNodes[iNode].iId);
			} else if ( pData->arrNodes[iNode].iParent >= 0 ) {
				iParentVisible = __xuiTreeViewVisibleIndexData(pData, pData->arrNodes[iNode].iParent);
				if ( iParentVisible >= 0 ) {
					(void)__xuiTreeViewSetSelectedVisible(pWidget, pData, iParentVisible, 1);
				}
			}
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_KEY_ENTER:
	case XUI_KEY_SPACE:
		if ( pData->iSelectedId >= 0 ) {
			__xuiTreeViewNotifySelect(pWidget, pData, pData->iSelectedId);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiTreeViewEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_tree_view_data_t* pData;

	(void)pUser;
	if ( (pWidget == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiTreeViewGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_MOVE:
		return __xuiTreeViewPointerMove(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_LEAVE:
		return __xuiTreeViewSetHover(pWidget, pData, -1);
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		pData->iActiveVisible = -1;
		pData->iActivePart = XUI_TREE_VIEW_ACTIVE_ROW;
		return __xuiTreeViewSetHover(pWidget, pData, -1);
	case XUI_EVENT_POINTER_DOWN:
		return __xuiTreeViewPointerDown(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_UP:
		return __xuiTreeViewPointerUp(pWidget, pData, pEvent);
	case XUI_EVENT_KEY_DOWN:
		return __xuiTreeViewKeyDown(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_WHEEL:
		if ( pEvent->iPhase != XUI_EVENT_PHASE_CAPTURE ) {
			(void)__xuiTreeViewSetHover(pWidget, pData, -1);
		}
		return XUI_OK;
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
	case XUI_EVENT_ENABLED_CHANGED:
	case XUI_EVENT_VISIBLE_CHANGED:
		if ( pEvent->iType == XUI_EVENT_ENABLED_CHANGED && pData->pFrame != NULL ) {
			(void)xuiWidgetSetEnabled(pData->pFrame, xuiWidgetGetEnabled(pWidget));
		}
		(void)__xuiTreeViewSyncState(pWidget);
		return __xuiTreeViewInvalidateRows(pWidget, pData);
	case XUI_EVENT_BOUNDS_CHANGED:
		(void)__xuiTreeViewUpdateContentSize(pWidget, pData);
		return XUI_OK;
	default:
		break;
	}
	return XUI_OK;
}

static void __xuiTreeViewFrameChanged(xui_widget pFrame, float fOffsetX, float fOffsetY, void* pUser)
{
	xui_widget pWidget;
	xui_tree_view_data_t* pData;

	(void)pFrame;
	(void)fOffsetX;
	(void)fOffsetY;
	pWidget = (xui_widget)pUser;
	pData = __xuiTreeViewGetData(pWidget);
	if ( pData == NULL ) {
		return;
	}
	pData->iHoverVisible = -1;
	pData->iChangeCount++;
	(void)__xuiTreeViewInvalidateRows(pWidget, pData);
}

static int __xuiTreeViewContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_tree_view_data_t* pData;
	xui_tree_view_data_t tResolved;
	float fRows;

	(void)tConstraint;
	pData = (xui_tree_view_data_t*)pUser;
	if ( (pWidget == NULL) || (pData == NULL) || (pSize == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiTreeViewResolve(pWidget, pData, &tResolved);
	fRows = (float)((pData->iVisibleCount > 0 && pData->iVisibleCount < XUI_TREE_VIEW_DEFAULT_ROWS) ? pData->iVisibleCount : XUI_TREE_VIEW_DEFAULT_ROWS);
	if ( fRows < 3.0f ) {
		fRows = 3.0f;
	}
	pSize->fX = xuiInternalSnapSize(XUI_TREE_VIEW_DEFAULT_WIDTH);
	pSize->fY = xuiInternalSnapSize((tResolved.fItemHeight * fRows) + tResolved.fBorderWidth * 2.0f);
	return XUI_OK;
}

static int __xuiTreeViewArrange(xui_widget pWidget, xui_rect_t tContentRect, void* pUser)
{
	xui_tree_view_data_t* pData;
	xui_tree_view_data_t tResolved;
	xui_rect_t tFrame;
	int iRet;

	(void)tContentRect;
	pData = (xui_tree_view_data_t*)pUser;
	if ( (pWidget == NULL) || (pData == NULL) || (pData->pFrame == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiTreeViewResolve(pWidget, pData, &tResolved);
	pData->fItemHeight = tResolved.fItemHeight;
	pData->fIndent = tResolved.fIndent;
	pData->fPadding = tResolved.fPadding;
	pData->fBorderWidth = tResolved.fBorderWidth;
	tFrame = __xuiTreeViewFrameRect(pWidget, pData);
	iRet = __xuiTreeViewUpdateContentSize(pWidget, pData);
	if ( iRet == XUI_OK ) iRet = xuiWidgetArrange(pData->pFrame, tFrame);
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameLayout(pData->pFrame);
	return iRet;
}

static int __xuiTreeViewDrawRectFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( __xuiTreeViewAlpha(iColor) == 0 ) {
		return XUI_OK;
	}
	return pProxy->drawRectFill(pProxy, pDraw, tRect, iColor);
}

static int __xuiTreeViewDrawRectStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	if ( (fWidth <= 0.0f) || (__xuiTreeViewAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	return pProxy->drawRectStroke(pProxy, pDraw, tRect, fWidth, iColor);
}

static int __xuiTreeViewCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_tree_view_data_t* pData;
	xui_tree_view_data_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tRect;
	int bFocused;
	int iRet;

	(void)iStateId;
	(void)pUser;
	if ( (pWidget == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiTreeViewGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	__xuiTreeViewResolve(pWidget, pData, &tResolved);
	tRect = xuiWidgetGetRect(pWidget);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	tRect = xuiInternalSnapRect(tRect);
	iRet = __xuiTreeViewDrawRectFill(pProxy, pDraw, tRect, tResolved.iBackgroundColor);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTreeViewDrawRectStroke(pProxy, pDraw, tRect, tResolved.fBorderWidth, tResolved.iBorderColor);
	if ( iRet != XUI_OK ) return iRet;
	bFocused = ((xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget) && xuiWidgetGetEnabled(pWidget)) ? 1 : 0;
	if ( bFocused ) {
		iRet = __xuiTreeViewDrawRectStroke(pProxy, pDraw, tRect, tResolved.fBorderWidth, tResolved.iFocusColor);
	}
	return iRet;
}

static int __xuiTreeViewDrawExpander(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, int bExpanded, uint32_t iColor)
{
	xui_vec2_t tA;
	xui_vec2_t tB;
	xui_vec2_t tC;
	float fCX;
	float fCY;

	if ( (pProxy->drawTriangleFill == NULL) || (__xuiTreeViewAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	fCX = tRect.fX + tRect.fW * 0.5f;
	fCY = tRect.fY + tRect.fH * 0.5f;
	if ( bExpanded ) {
		tA = (xui_vec2_t){fCX - 5.0f, fCY - 2.5f};
		tB = (xui_vec2_t){fCX + 5.0f, fCY - 2.5f};
		tC = (xui_vec2_t){fCX, fCY + 4.0f};
	} else {
		tA = (xui_vec2_t){fCX - 2.5f, fCY - 5.0f};
		tB = (xui_vec2_t){fCX - 2.5f, fCY + 5.0f};
		tC = (xui_vec2_t){fCX + 4.5f, fCY};
	}
	return pProxy->drawTriangleFill(pProxy, pDraw, tA, tB, tC, iColor);
}

static uint32_t __xuiTreeViewExpanderHotspotColor(int iState, int bActiveExpander)
{
	if ( (iState & XUI_TREE_ITEM_DISABLED) != 0 ) {
		return XUI_COLOR_RGBA(224, 232, 242, 130);
	}
	if ( (iState & XUI_TREE_ITEM_SELECTED) != 0 ) {
		return bActiveExpander ? XUI_COLOR_RGBA(13, 62, 132, 190) : XUI_COLOR_RGBA(18, 86, 178, 145);
	}
	if ( bActiveExpander ) {
		return XUI_COLOR_RGBA(47, 128, 237, 52);
	}
	if ( (iState & XUI_TREE_ITEM_HOVER) != 0 ) {
		return XUI_COLOR_RGBA(47, 128, 237, 32);
	}
	return XUI_COLOR_RGBA(205, 225, 246, 165);
}

static uint32_t __xuiTreeViewExpanderGlyphColor(const xui_tree_view_data_t* pResolved, int iState)
{
	if ( pResolved == NULL ) {
		return 0;
	}
	if ( (iState & XUI_TREE_ITEM_DISABLED) != 0 ) {
		return pResolved->iDisabledTextColor;
	}
	if ( (iState & XUI_TREE_ITEM_SELECTED) != 0 ) {
		return XUI_COLOR_RGBA(255, 255, 255, 255);
	}
	return pResolved->iExpanderColor;
}

static int __xuiTreeViewDrawCheck(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, int bChecked, int bDisabled, uint32_t iColor, uint32_t iDisabledColor)
{
	uint32_t iBorder;
	uint32_t iFill;
	int iRet;

	iBorder = bDisabled ? iDisabledColor : iColor;
	iFill = bChecked ? iColor : XUI_COLOR_RGBA(255, 255, 255, 255);
	iRet = __xuiTreeViewDrawRectFill(pProxy, pDraw, tRect, iFill);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTreeViewDrawRectStroke(pProxy, pDraw, tRect, 1.0f, iBorder);
	if ( iRet != XUI_OK ) return iRet;
	if ( bChecked && (pProxy->drawLine != NULL) ) {
		iRet = pProxy->drawLine(pProxy, pDraw, tRect.fX + 3.0f, tRect.fY + 6.0f, tRect.fX + 5.2f, tRect.fY + 8.2f, 1.7f, XUI_COLOR_RGBA(255, 255, 255, 255));
		if ( iRet != XUI_OK ) return iRet;
		iRet = pProxy->drawLine(pProxy, pDraw, tRect.fX + 5.0f, tRect.fY + 8.1f, tRect.fX + 9.3f, tRect.fY + 3.8f, 1.7f, XUI_COLOR_RGBA(255, 255, 255, 255));
	}
	return iRet;
}

static int __xuiTreeViewDrawIcon(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, int bFolder, uint32_t iColor)
{
	xui_rect_t tBody;
	xui_rect_t tTab;
	int iRet;

	if ( __xuiTreeViewAlpha(iColor) == 0 ) {
		return XUI_OK;
	}
	if ( bFolder ) {
		tTab = xuiInternalSnapRect((xui_rect_t){tRect.fX + 1.0f, tRect.fY + 3.0f, tRect.fW * 0.42f, 3.0f});
		tBody = xuiInternalSnapRect((xui_rect_t){tRect.fX + 1.0f, tRect.fY + 5.0f, tRect.fW - 2.0f, tRect.fH - 6.0f});
		iRet = __xuiTreeViewDrawRectFill(pProxy, pDraw, tTab, XUI_COLOR_RGBA(171, 201, 231, 230));
		if ( iRet != XUI_OK ) return iRet;
		iRet = __xuiTreeViewDrawRectFill(pProxy, pDraw, tBody, XUI_COLOR_RGBA(206, 226, 246, 235));
		if ( iRet != XUI_OK ) return iRet;
		return __xuiTreeViewDrawRectStroke(pProxy, pDraw, tBody, 1.0f, iColor);
	}
	tBody = xuiInternalSnapRect((xui_rect_t){tRect.fX + 3.0f, tRect.fY + 2.0f, tRect.fW - 6.0f, tRect.fH - 4.0f});
	iRet = __xuiTreeViewDrawRectFill(pProxy, pDraw, tBody, XUI_COLOR_RGBA(255, 255, 255, 230));
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTreeViewDrawRectStroke(pProxy, pDraw, tBody, 1.0f, iColor);
	if ( iRet != XUI_OK ) return iRet;
	if ( pProxy->drawLine != NULL ) {
		iRet = pProxy->drawLine(pProxy, pDraw, tBody.fX + 3.0f, tBody.fY + 5.0f, tBody.fX + tBody.fW - 3.0f, tBody.fY + 5.0f, 1.0f, XUI_COLOR_RGBA(164, 190, 217, 210));
	}
	return iRet;
}

static int __xuiTreeViewViewportRender(xui_widget pViewport, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_widget pWidget;
	xui_tree_view_data_t* pData;
	xui_tree_view_data_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tRect;
	xui_rect_t tRow;
	xui_rect_t tFill;
	xui_rect_t tText;
	xui_rect_t tExpander;
	xui_rect_t tCheck;
	xui_rect_t tIcon;
	xui_tree_view_node_t* pNode;
	float fOffsetY;
	float fViewportW;
	float fViewportH;
	float fY;
	float fTextX;
	int iStart;
	int iEnd;
	int i;
	int iNode;
	int iState;
	int iHandled;
	int iRet;
	int bActiveExpander;
	uint32_t iTextColor;
	uint32_t iExpanderColor;

	(void)iStateId;
	pWidget = (xui_widget)pUser;
	pData = __xuiTreeViewGetData(pWidget);
	if ( (pViewport == NULL) || (pData == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	__xuiTreeViewResolve(pWidget, pData, &tResolved);
	tRect = xuiWidgetGetRect(pViewport);
	fViewportW = __xuiTreeViewMaxFloat(0.0f, tRect.fW);
	fViewportH = __xuiTreeViewMaxFloat(0.0f, tRect.fH);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	tRect = xuiInternalSnapRect(tRect);
	iRet = pProxy->drawRectFill(pProxy, pDraw, tRect, tResolved.iBackgroundColor);
	if ( iRet != XUI_OK ) return iRet;
	if ( (pData->iVisibleCount <= 0) || (tResolved.fItemHeight <= 0.0f) ) {
		return XUI_OK;
	}
	fOffsetY = 0.0f;
	(void)xuiScrollFrameGetOffset(pData->pFrame, NULL, &fOffsetY);
	iStart = (int)(fOffsetY / tResolved.fItemHeight);
	if ( iStart < 0 ) iStart = 0;
	iEnd = (int)((fOffsetY + fViewportH) / tResolved.fItemHeight) + 2;
	if ( iEnd > pData->iVisibleCount ) iEnd = pData->iVisibleCount;
	for ( i = iStart; i < iEnd; i++ ) {
		iNode = pData->arrVisible[i];
		pNode = &pData->arrNodes[iNode];
		fY = ((float)i * tResolved.fItemHeight) - fOffsetY;
		tRow = xuiInternalSnapRect((xui_rect_t){0.0f, fY, fViewportW, tResolved.fItemHeight});
		iState = 0;
		if ( pData->iSelectedId == pNode->iId ) iState |= XUI_TREE_ITEM_SELECTED;
		if ( pData->iHoverVisible == i ) iState |= XUI_TREE_ITEM_HOVER;
		if ( !__xuiTreeViewNodeEnabledData(pData, iNode) || !xuiWidgetGetEnabled(pWidget) ) iState |= XUI_TREE_ITEM_DISABLED;
		if ( pData->iFocusVisible == i ) iState |= XUI_TREE_ITEM_FOCUS;
		if ( pNode->bExpanded ) iState |= XUI_TREE_ITEM_EXPANDED;
		if ( pNode->bHasChildren ) iState |= XUI_TREE_ITEM_HAS_CHILDREN;
		if ( pNode->bChecked ) iState |= XUI_TREE_ITEM_CHECKED;
		if ( pData->onRenderItem != NULL ) {
			iHandled = pData->onRenderItem(pWidget, pNode->iId, i, pNode, pDraw, tRow, iState, pData->pRenderItemUser);
			if ( iHandled < 0 ) return iHandled;
			if ( iHandled ) continue;
		}
		if ( (iState & XUI_TREE_ITEM_SELECTED) != 0 ) {
			tFill = xuiInternalSnapRect((xui_rect_t){tRow.fX + 3.0f, tRow.fY + 2.0f, __xuiTreeViewMaxFloat(1.0f, tRow.fW - 6.0f), __xuiTreeViewMaxFloat(1.0f, tRow.fH - 4.0f)});
			iRet = __xuiTreeViewDrawRectFill(pProxy, pDraw, tFill, tResolved.iSelectedColor);
			if ( iRet != XUI_OK ) return iRet;
		} else if ( (iState & XUI_TREE_ITEM_HOVER) != 0 ) {
			tFill = xuiInternalSnapRect((xui_rect_t){tRow.fX + 3.0f, tRow.fY + 2.0f, __xuiTreeViewMaxFloat(1.0f, tRow.fW - 6.0f), __xuiTreeViewMaxFloat(1.0f, tRow.fH - 4.0f)});
			iRet = __xuiTreeViewDrawRectFill(pProxy, pDraw, tFill, tResolved.iHoverColor);
			if ( iRet != XUI_OK ) return iRet;
		} else if ( __xuiTreeViewAlpha(tResolved.iRowColor) != 0 ) {
			iRet = pProxy->drawRectFill(pProxy, pDraw, tRow, tResolved.iRowColor);
			if ( iRet != XUI_OK ) return iRet;
		}
		if ( ((iState & XUI_TREE_ITEM_FOCUS) != 0) &&
		     (xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget) &&
		     ((iState & XUI_TREE_ITEM_DISABLED) == 0) ) {
			tFill = xuiInternalSnapRect((xui_rect_t){tRow.fX + 3.0f, tRow.fY + 2.0f, __xuiTreeViewMaxFloat(1.0f, tRow.fW - 6.0f), __xuiTreeViewMaxFloat(1.0f, tRow.fH - 4.0f)});
			iRet = __xuiTreeViewDrawRectStroke(pProxy, pDraw, tFill, 1.0f, tResolved.iFocusColor);
			if ( iRet != XUI_OK ) return iRet;
		}
		tExpander = __xuiTreeViewExpanderRect(&tResolved, pNode, tRow);
		if ( pNode->bHasChildren ) {
			bActiveExpander = (pData->iActiveVisible == i) && (pData->iActivePart == XUI_TREE_VIEW_ACTIVE_EXPANDER);
			tFill = xuiInternalSnapRect((xui_rect_t){tExpander.fX - 1.0f, tExpander.fY - 1.0f, tExpander.fW + 2.0f, tExpander.fH + 2.0f});
			iRet = __xuiTreeViewDrawRectFill(pProxy, pDraw, tFill, __xuiTreeViewExpanderHotspotColor(iState, bActiveExpander));
			if ( iRet != XUI_OK ) return iRet;
			iExpanderColor = __xuiTreeViewExpanderGlyphColor(&tResolved, iState);
			iRet = __xuiTreeViewDrawExpander(pProxy, pDraw, tExpander, pNode->bExpanded, iExpanderColor);
			if ( iRet != XUI_OK ) return iRet;
		}
		fTextX = tExpander.fX + tExpander.fW + 5.0f;
		if ( pNode->bCheckReserved ) {
			tCheck = __xuiTreeViewCheckRect(&tResolved, pNode, tRow);
			iRet = __xuiTreeViewDrawCheck(pProxy, pDraw, tCheck, pNode->bChecked, (iState & XUI_TREE_ITEM_DISABLED) != 0, tResolved.iCheckColor, tResolved.iDisabledTextColor);
			if ( iRet != XUI_OK ) return iRet;
			fTextX = tCheck.fX + tCheck.fW + 6.0f;
		}
		if ( pNode->bIconReserved ) {
			tIcon = xuiInternalSnapRect((xui_rect_t){fTextX, tRow.fY + (tRow.fH - 14.0f) * 0.5f, 15.0f, 14.0f});
			iRet = __xuiTreeViewDrawIcon(pProxy, pDraw, tIcon, pNode->bHasChildren, ((iState & XUI_TREE_ITEM_DISABLED) != 0) ? tResolved.iDisabledTextColor : tResolved.iIconColor);
			if ( iRet != XUI_OK ) return iRet;
			fTextX = tIcon.fX + tIcon.fW + 6.0f;
		}
		iTextColor = ((iState & XUI_TREE_ITEM_DISABLED) != 0) ? tResolved.iDisabledTextColor : tResolved.iTextColor;
		if ( (iState & XUI_TREE_ITEM_SELECTED) != 0 ) {
			iTextColor = XUI_COLOR_RGBA(255, 255, 255, 255);
		}
		if ( (tResolved.pFont != NULL) && (__xuiTreeViewAlpha(iTextColor) != 0) ) {
			tText = xuiInternalSnapRect((xui_rect_t){fTextX, tRow.fY, __xuiTreeViewMaxFloat(1.0f, tRow.fW - fTextX - tResolved.fPadding), tRow.fH});
			iRet = pProxy->drawText(pProxy, pDraw, tResolved.pFont, __xuiTreeViewText(pNode->sText), tText, iTextColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	return XUI_OK;
}

static void __xuiTreeViewDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_MANUAL;
	pLayout->iWidthMode = XUI_SIZE_CONTENT;
	pLayout->iHeightMode = XUI_SIZE_CONTENT;
	pLayout->iFlowMode = XUI_FLOW_BLOCK;
	pLayout->iDock = XUI_DOCK_FILL;
	pLayout->iOverflow = XUI_OVERFLOW_CLIP;
	pLayout->iAlignX = XUI_ALIGN_START;
	pLayout->iAlignY = XUI_ALIGN_START;
	pLayout->iTableRowSpan = 1;
	pLayout->iTableColumnSpan = 1;
	pLayout->iGridColumnCount = 1;
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fShrink = 1.0f;
}

static void __xuiTreeViewDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiTreeViewInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiTreeViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiTreeViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiTreeViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiTreeViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_WHEEL, __xuiTreeViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiTreeViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiTreeViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiTreeViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiTreeViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BOUNDS_CHANGED, __xuiTreeViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_ENABLED_CHANGED, __xuiTreeViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_VISIBLE_CHANGED, __xuiTreeViewEvent, NULL);
	return iRet;
}

static int __xuiTreeViewInitViewport(xui_widget pWidget, xui_tree_view_data_t* pData)
{
	xui_cache_policy_t tPolicy;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pViewport == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(&tPolicy, 0, sizeof(tPolicy));
	tPolicy.iSize = sizeof(tPolicy);
	tPolicy.iPolicy = XUI_CACHE_POLICY_SELF;
	tPolicy.iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	tPolicy.iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	(void)xuiWidgetSetLayoutType(pData->pViewport, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetFlowMode(pData->pViewport, XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetFocusable(pData->pViewport, 0);
	(void)xuiWidgetSetTabStop(pData->pViewport, 0);
	(void)xuiWidgetSetCachePolicy(pData->pViewport, &tPolicy);
	return xuiWidgetSetCacheRenderCallback(pData->pViewport, __xuiTreeViewViewportRender, pWidget);
}

static int __xuiTreeViewCreateFrame(xui_widget pWidget, xui_tree_view_data_t* pData, const xui_tree_view_desc_t* pDesc)
{
	xui_scroll_frame_desc_t tFrameDesc;
	int iRet;

	memset(&tFrameDesc, 0, sizeof(tFrameDesc));
	tFrameDesc.iSize = sizeof(tFrameDesc);
	tFrameDesc.iPolicyX = XUI_SCROLLBAR_POLICY_HIDDEN;
	tFrameDesc.iPolicyY = XUI_SCROLLBAR_POLICY_AUTO;
	tFrameDesc.iScrollbarMode = (pDesc != NULL && pDesc->iScrollbarMode != XUI_SCROLLBAR_MODE_FULL && __xuiTreeViewScrollbarModeValid(pDesc->iScrollbarMode)) ? pDesc->iScrollbarMode : XUI_SCROLLBAR_MODE_COMPACT;
	tFrameDesc.iWheelAxis = XUI_WHEEL_AXIS_VERTICAL;
	tFrameDesc.iCornerMode = XUI_SCROLL_FRAME_CORNER_NONE;
	tFrameDesc.bContentDragEnabled = 0;
	tFrameDesc.fScrollbarSize = 8.0f;
	tFrameDesc.fMinThumbSize = 18.0f;
	tFrameDesc.fWheelStep = pData->fItemHeight * 3.0f;
	tFrameDesc.iTrackColor = pData->iTrackColor;
	tFrameDesc.iThumbColor = pData->iThumbColor;
	tFrameDesc.iHoverColor = pData->iScrollbarHoverColor;
	tFrameDesc.iActiveColor = pData->iScrollbarActiveColor;
	tFrameDesc.iFocusColor = pData->iScrollbarFocusColor;
	tFrameDesc.iDisabledColor = pData->iScrollbarDisabledColor;
	iRet = xuiScrollFrameCreate(xuiWidgetGetContext(pWidget), &pData->pFrame, &tFrameDesc);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiWidgetAddChild(pWidget, pData->pFrame);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pData->pFrame);
		pData->pFrame = NULL;
		return iRet;
	}
	(void)xuiWidgetSetFlowMode(pData->pFrame, XUI_FLOW_ABSOLUTE);
	pData->pViewport = xuiScrollFrameGetViewportWidget(pData->pFrame);
	if ( pData->pViewport == NULL ) {
		xuiWidgetDestroy(pData->pFrame);
		pData->pFrame = NULL;
		return XUI_ERROR_NOT_INITIALIZED;
	}
	iRet = xuiScrollFrameSetChange(pData->pFrame, __xuiTreeViewFrameChanged, pWidget);
	if ( iRet == XUI_OK ) iRet = __xuiTreeViewInitViewport(pWidget, pData);
	if ( iRet == XUI_OK ) iRet = __xuiTreeViewApplyFrameStyle(pWidget, pData);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pData->pFrame);
		pData->pFrame = NULL;
		pData->pViewport = NULL;
		return iRet;
	}
	return XUI_OK;
}

static int __xuiTreeViewInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_tree_view_data_t* pData;
	const xui_tree_view_desc_t* pDesc;
	int iRet;

	(void)pUser;
	pData = (xui_tree_view_data_t*)pTypeData;
	pDesc = (const xui_tree_view_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiTreeViewDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiTreeViewDefaults(pData);
	__xuiTreeViewApplyDesc(pData, pDesc);
	(void)xuiWidgetSetLayoutType(pWidget, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetFlowMode(pWidget, XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_CLIP);
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	(void)xuiWidgetSetPadding(pWidget, (xui_thickness_t){0.0f, 0.0f, 0.0f, 0.0f});
	iRet = __xuiTreeViewCreateFrame(pWidget, pData, pDesc);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTreeViewInitEvents(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	if ( (pDesc != NULL) && (pDesc->arrNodes != NULL) && (pDesc->iNodeCount > 0) ) {
		iRet = xuiTreeViewSetNodes(pWidget, pDesc->arrNodes, pDesc->iNodeCount);
		if ( iRet != XUI_OK ) return iRet;
		if ( pDesc->iSelectedId >= 0 ) {
			iRet = xuiTreeViewSetSelected(pWidget, pDesc->iSelectedId);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	(void)__xuiTreeViewSyncState(pWidget);
	return XUI_OK;
}

static void __xuiTreeViewDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_tree_view_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_tree_view_data_t*)pTypeData;
	if ( pData != NULL ) {
		__xuiTreeViewClearNodeStorage(pData);
		memset(pData, 0, sizeof(*pData));
	}
}

static void __xuiTreeViewRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
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

static void __xuiTreeViewRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;
	uint32_t iLayoutDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	iLayoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.background.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.border.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.focus.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.row.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.row.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.row.selected_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.text.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.text.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.expander.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.icon.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.check.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.item.height", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.indent", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.padding", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "treeview.border.width", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiTreeViewRegisterStyleProperty(pContext, pType, "font.name", XUI_STYLE_VALUE_STRING, iLayoutDirty, XUI_STYLE_PROPERTY_INHERITED);
}

static xui_tree_view_data_t* __xuiTreeViewGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return NULL;
	}
	pContext = xuiWidgetGetContext(pWidget);
	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "treeview");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_tree_view_data_t*)xuiWidgetGetTypeData(pWidget);
}

XUI_API xui_widget_type xuiTreeViewGetType(xui_context pContext)
{
	xui_widget_type pType;
	xui_widget_type_desc_t tDesc;
	xui_layout_t tLayout;
	xui_cache_policy_t tPolicy;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "treeview");
	if ( pType != NULL ) {
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "treeview";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_tree_view_data_t);
	tDesc.onInit = __xuiTreeViewInit;
	tDesc.onDestroy = __xuiTreeViewDestroy;
	tDesc.onContentMeasure = __xuiTreeViewContentMeasure;
	tDesc.onLayoutArrange = __xuiTreeViewArrange;
	tDesc.onCacheRender = __xuiTreeViewCacheRender;
	__xuiTreeViewDefaultLayout(&tLayout);
	__xuiTreeViewDefaultCachePolicy(&tPolicy);
	tDesc.tLayout = tLayout;
	tDesc.tCachePolicy = tPolicy;
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	__xuiTreeViewRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiTreeViewCreate(xui_context pContext, xui_widget* ppWidget, const xui_tree_view_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( !xuiInternalContextIsValid(pContext) || (ppWidget == NULL) || !__xuiTreeViewDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiTreeViewGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiTreeViewSetSelect(xui_widget pWidget, xui_tree_view_select_proc onSelect, void* pUser)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onSelect = onSelect;
	pData->pSelectUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTreeViewSetItemRenderer(xui_widget pWidget, xui_tree_view_item_proc onRender, void* pUser)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onRenderItem = onRender;
	pData->pRenderItemUser = pUser;
	return __xuiTreeViewInvalidateRows(pWidget, pData);
}

XUI_API int xuiTreeViewSetAdapter(xui_widget pWidget, xui_tree_view_count_proc onCount, xui_tree_view_node_proc onNode, void* pUser)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onCount = onCount;
	pData->onNode = onNode;
	pData->pAdapterUser = pUser;
	return xuiTreeViewRefreshAdapter(pWidget);
}

XUI_API int xuiTreeViewRefreshAdapter(xui_widget pWidget)
{
	xui_tree_view_data_t* pData;
	xui_tree_view_node_t tNode;
	int iCount;
	int i;
	int iSelectedId;
	float fScroll;
	int iRet;

	pData = __xuiTreeViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( (pData->onCount == NULL) || (pData->onNode == NULL) ) {
		return XUI_OK;
	}
	iCount = pData->onCount(pWidget, pData->pAdapterUser);
	if ( iCount < 0 ) {
		iCount = 0;
	}
	iSelectedId = pData->iSelectedId;
	fScroll = xuiTreeViewGetScroll(pWidget);
	for ( i = 0; i < pData->iNodeCount; i++ ) {
		__xuiTreeViewFreeNodeText(&pData->arrNodes[i]);
	}
	pData->iNodeCount = 0;
	pData->iVisibleCount = 0;
	pData->iHoverVisible = -1;
	pData->iFocusVisible = -1;
	pData->iSelectedId = iSelectedId;
	for ( i = 0; i < iCount; i++ ) {
		memset(&tNode, 0, sizeof(tNode));
		tNode.iId = -1;
		tNode.iParent = -1;
		tNode.sText = "";
		tNode.bEnabled = 1;
		tNode.bIconReserved = 1;
		if ( pData->onNode(pWidget, i, &tNode, pData->pAdapterUser) != XUI_OK ) {
			continue;
		}
		(void)__xuiTreeViewAppendNodeData(pData, &tNode, 1);
	}
	iRet = __xuiTreeViewRebuildVisible(pWidget, pData, -1);
	if ( iRet != XUI_OK ) return iRet;
	return xuiTreeViewSetScroll(pWidget, fScroll);
}

XUI_API int xuiTreeViewClear(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	int i;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < pData->iNodeCount; i++ ) {
		__xuiTreeViewFreeNodeText(&pData->arrNodes[i]);
	}
	pData->iNodeCount = 0;
	pData->iVisibleCount = 0;
	pData->iSelectedId = -1;
	pData->iHoverVisible = -1;
	pData->iFocusVisible = -1;
	pData->iActiveVisible = -1;
	(void)xuiTreeViewSetScroll(pWidget, 0.0f);
	(void)__xuiTreeViewUpdateContentSize(pWidget, pData);
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTreeViewSetNodes(xui_widget pWidget, const xui_tree_view_node_t* pNodes, int iCount)
{
	xui_tree_view_data_t* pData;
	int i;
	int iRet;

	pData = __xuiTreeViewGetData(pWidget);
	if ( (pData == NULL) || (iCount < 0) || ((iCount > 0) && (pNodes == NULL)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = xuiTreeViewClear(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	for ( i = 0; i < iCount; i++ ) {
		iRet = __xuiTreeViewAppendNodeData(pData, &pNodes[i], 0);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	iRet = __xuiTreeViewRebuildVisible(pWidget, pData, -1);
	if ( iRet != XUI_OK ) return iRet;
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTreeViewAddNode(xui_widget pWidget, int iId, int iParentId, const char* sText)
{
	xui_tree_view_data_t* pData;
	xui_tree_view_node_t tNode;
	int iRet;

	pData = __xuiTreeViewGetData(pWidget);
	if ( (pData == NULL) || (iId < 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&tNode, 0, sizeof(tNode));
	tNode.iId = iId;
	tNode.iParent = iParentId;
	tNode.sText = sText;
	tNode.bEnabled = 1;
	tNode.bIconReserved = 1;
	iRet = __xuiTreeViewAppendNodeData(pData, &tNode, 1);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTreeViewRebuildVisible(pWidget, pData, -1);
	if ( iRet != XUI_OK ) return iRet;
	pData->iChangeCount++;
	return XUI_OK;
}

XUI_API int xuiTreeViewGetNodeCount(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return (pData != NULL) ? pData->iNodeCount : 0;
}

XUI_API const xui_tree_view_node_t* xuiTreeViewGetNode(xui_widget pWidget, int iIndex)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iNodeCount) ) return NULL;
	return &pData->arrNodes[iIndex];
}

XUI_API const xui_tree_view_node_t* xuiTreeViewGetNodeById(xui_widget pWidget, int iNodeId)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	int iNode = __xuiTreeViewFindNodeData(pData, iNodeId);
	return (iNode >= 0) ? &pData->arrNodes[iNode] : NULL;
}

XUI_API int xuiTreeViewFindNode(xui_widget pWidget, int iNodeId)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return __xuiTreeViewFindNodeData(pData, iNodeId);
}

XUI_API int xuiTreeViewSetNodeExpanded(xui_widget pWidget, int iNodeId, int bExpanded)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	int iNode;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iNode = __xuiTreeViewFindNodeData(pData, iNodeId);
	if ( iNode < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrNodes[iNode].bExpanded = bExpanded ? 1 : 0;
	pData->iChangeCount++;
	return __xuiTreeViewRebuildVisible(pWidget, pData, bExpanded ? -1 : iNodeId);
}

XUI_API int xuiTreeViewGetNodeExpanded(xui_widget pWidget, int iNodeId)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	int iNode = __xuiTreeViewFindNodeData(pData, iNodeId);
	return (iNode >= 0) ? pData->arrNodes[iNode].bExpanded : 0;
}

XUI_API int xuiTreeViewToggleNode(xui_widget pWidget, int iNodeId)
{
	return xuiTreeViewSetNodeExpanded(pWidget, iNodeId, !xuiTreeViewGetNodeExpanded(pWidget, iNodeId));
}

XUI_API int xuiTreeViewSetNodeEnabled(xui_widget pWidget, int iNodeId, int bEnabled)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	int iNode;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iNode = __xuiTreeViewFindNodeData(pData, iNodeId);
	if ( iNode < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrNodes[iNode].bEnabled = bEnabled ? 1 : 0;
	pData->iChangeCount++;
	return __xuiTreeViewRebuildVisible(pWidget, pData, -1);
}

XUI_API int xuiTreeViewGetNodeEnabled(xui_widget pWidget, int iNodeId)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	int iNode = __xuiTreeViewFindNodeData(pData, iNodeId);
	return (iNode >= 0) ? pData->arrNodes[iNode].bEnabled : 0;
}

XUI_API int xuiTreeViewSetNodeChecked(xui_widget pWidget, int iNodeId, int bChecked)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	int iNode;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iNode = __xuiTreeViewFindNodeData(pData, iNodeId);
	if ( iNode < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrNodes[iNode].bCheckReserved = 1;
	pData->arrNodes[iNode].bChecked = bChecked ? 1 : 0;
	pData->iChangeCount++;
	return __xuiTreeViewInvalidateRows(pWidget, pData);
}

XUI_API int xuiTreeViewGetNodeChecked(xui_widget pWidget, int iNodeId)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	int iNode = __xuiTreeViewFindNodeData(pData, iNodeId);
	return (iNode >= 0) ? pData->arrNodes[iNode].bChecked : 0;
}

XUI_API int xuiTreeViewSetNodeDecorations(xui_widget pWidget, int iNodeId, int bIcon, int bCheck)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	int iNode;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iNode = __xuiTreeViewFindNodeData(pData, iNodeId);
	if ( iNode < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrNodes[iNode].bIconReserved = bIcon ? 1 : 0;
	pData->arrNodes[iNode].bCheckReserved = bCheck ? 1 : 0;
	pData->iChangeCount++;
	return __xuiTreeViewInvalidateRows(pWidget, pData);
}

XUI_API int xuiTreeViewSetSelected(xui_widget pWidget, int iNodeId)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	int iVisible;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iVisible = __xuiTreeViewVisibleIndexData(pData, iNodeId);
	return __xuiTreeViewSetSelectedVisible(pWidget, pData, iVisible, 0);
}

XUI_API int xuiTreeViewGetSelected(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return (pData != NULL) ? pData->iSelectedId : -1;
}

XUI_API int xuiTreeViewGetVisibleCount(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return (pData != NULL) ? pData->iVisibleCount : 0;
}

XUI_API int xuiTreeViewGetVisibleNodeId(xui_widget pWidget, int iVisible)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	if ( (pData == NULL) || (iVisible < 0) || (iVisible >= pData->iVisibleCount) ) return -1;
	return pData->arrNodes[pData->arrVisible[iVisible]].iId;
}

XUI_API const xui_tree_view_node_t* xuiTreeViewGetVisibleNode(xui_widget pWidget, int iVisible)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	if ( (pData == NULL) || (iVisible < 0) || (iVisible >= pData->iVisibleCount) ) return NULL;
	return &pData->arrNodes[pData->arrVisible[iVisible]];
}

XUI_API int xuiTreeViewGetVisibleIndexOfId(xui_widget pWidget, int iNodeId)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return __xuiTreeViewVisibleIndexData(pData, iNodeId);
}

XUI_API int xuiTreeViewSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pFont = pFont;
	return __xuiTreeViewInvalidateRows(pWidget, pData);
}

XUI_API xui_font xuiTreeViewGetFont(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API int xuiTreeViewSetMetrics(xui_widget pWidget, float fItemHeight, float fIndent, float fPadding, float fBorderWidth)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	if ( (pData == NULL) ||
	     ((fItemHeight != 0.0f) && !__xuiTreeViewFloatValid(fItemHeight)) ||
	     ((fIndent != 0.0f) && !__xuiTreeViewFloatValid(fIndent)) ||
	     ((fPadding != 0.0f) && !__xuiTreeViewFloatValid(fPadding)) ||
	     ((fBorderWidth != 0.0f) && !__xuiTreeViewFloatValid(fBorderWidth)) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( fItemHeight > 0.0f ) pData->fItemHeight = fItemHeight;
	if ( fIndent > 0.0f ) pData->fIndent = fIndent;
	if ( fPadding > 0.0f ) pData->fPadding = fPadding;
	if ( fBorderWidth >= 0.0f ) pData->fBorderWidth = fBorderWidth;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTreeViewGetMetrics(xui_widget pWidget, float* pItemHeight, float* pIndent, float* pPadding, float* pBorderWidth)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pItemHeight != NULL ) *pItemHeight = pData->fItemHeight;
	if ( pIndent != NULL ) *pIndent = pData->fIndent;
	if ( pPadding != NULL ) *pPadding = pData->fPadding;
	if ( pBorderWidth != NULL ) *pBorderWidth = pData->fBorderWidth;
	return XUI_OK;
}

XUI_API int xuiTreeViewSetScroll(xui_widget pWidget, float fOffsetY)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	if ( (pData == NULL) || (pData->pFrame == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameSetOffset(pData->pFrame, 0.0f, fOffsetY);
}

XUI_API float xuiTreeViewGetScroll(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	float fOffsetY;
	fOffsetY = 0.0f;
	if ( (pData != NULL) && (pData->pFrame != NULL) ) {
		(void)xuiScrollFrameGetOffset(pData->pFrame, NULL, &fOffsetY);
	}
	return fOffsetY;
}

XUI_API int xuiTreeViewEnsureVisible(xui_widget pWidget, int iNodeId)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	int iVisible;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iVisible = __xuiTreeViewVisibleIndexData(pData, iNodeId);
	return __xuiTreeViewEnsureVisibleIndex(pWidget, pData, iVisible);
}

XUI_API int xuiTreeViewSetScrollbarMode(xui_widget pWidget, int iMode)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	if ( (pData == NULL) || (pData->pFrame == NULL) || !__xuiTreeViewScrollbarModeValid(iMode) ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameSetScrollbarMode(pData->pFrame, iMode);
}

XUI_API int xuiTreeViewGetScrollbarMode(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return (pData != NULL && pData->pFrame != NULL) ? xuiScrollFrameGetScrollbarMode(pData->pFrame) : XUI_SCROLLBAR_MODE_COMPACT;
}

XUI_API int xuiTreeViewSetColors(xui_widget pWidget, uint32_t iBackground, uint32_t iBorder, uint32_t iFocus, uint32_t iRow, uint32_t iHover, uint32_t iSelected, uint32_t iText, uint32_t iDisabledText)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iBackgroundColor = iBackground;
	pData->iBorderColor = iBorder;
	pData->iFocusColor = iFocus;
	pData->iRowColor = iRow;
	pData->iHoverColor = iHover;
	pData->iSelectedColor = iSelected;
	pData->iTextColor = iText;
	pData->iDisabledTextColor = iDisabledText;
	return __xuiTreeViewInvalidateRows(pWidget, pData);
}

XUI_API int xuiTreeViewGetColors(xui_widget pWidget, uint32_t* pBackground, uint32_t* pBorder, uint32_t* pFocus, uint32_t* pRow, uint32_t* pHover, uint32_t* pSelected, uint32_t* pText, uint32_t* pDisabledText)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pBackground != NULL ) *pBackground = pData->iBackgroundColor;
	if ( pBorder != NULL ) *pBorder = pData->iBorderColor;
	if ( pFocus != NULL ) *pFocus = pData->iFocusColor;
	if ( pRow != NULL ) *pRow = pData->iRowColor;
	if ( pHover != NULL ) *pHover = pData->iHoverColor;
	if ( pSelected != NULL ) *pSelected = pData->iSelectedColor;
	if ( pText != NULL ) *pText = pData->iTextColor;
	if ( pDisabledText != NULL ) *pDisabledText = pData->iDisabledTextColor;
	return XUI_OK;
}

XUI_API int xuiTreeViewSetDecorationColors(xui_widget pWidget, uint32_t iExpander, uint32_t iIcon, uint32_t iCheck)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iExpanderColor = iExpander;
	pData->iIconColor = iIcon;
	pData->iCheckColor = iCheck;
	return __xuiTreeViewInvalidateRows(pWidget, pData);
}

XUI_API int xuiTreeViewGetDecorationColors(xui_widget pWidget, uint32_t* pExpander, uint32_t* pIcon, uint32_t* pCheck)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pExpander != NULL ) *pExpander = pData->iExpanderColor;
	if ( pIcon != NULL ) *pIcon = pData->iIconColor;
	if ( pCheck != NULL ) *pCheck = pData->iCheckColor;
	return XUI_OK;
}

XUI_API int xuiTreeViewSetScrollbarColors(xui_widget pWidget, uint32_t iTrack, uint32_t iThumb, uint32_t iHover, uint32_t iActive, uint32_t iFocus, uint32_t iDisabled)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	int iRet;
	if ( (pData == NULL) || (pData->pFrame == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iTrackColor = iTrack;
	pData->iThumbColor = iThumb;
	pData->iScrollbarHoverColor = iHover;
	pData->iScrollbarActiveColor = iActive;
	pData->iScrollbarFocusColor = iFocus;
	pData->iScrollbarDisabledColor = iDisabled;
	iRet = xuiScrollFrameSetColors(pData->pFrame, iTrack, iThumb, iHover, iActive, iFocus, iDisabled);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiTreeViewInvalidateRows(pWidget, pData);
}

XUI_API int xuiTreeViewGetScrollbarColors(xui_widget pWidget, uint32_t* pTrack, uint32_t* pThumb, uint32_t* pHover, uint32_t* pActive, uint32_t* pFocus, uint32_t* pDisabled)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pTrack != NULL ) *pTrack = pData->iTrackColor;
	if ( pThumb != NULL ) *pThumb = pData->iThumbColor;
	if ( pHover != NULL ) *pHover = pData->iScrollbarHoverColor;
	if ( pActive != NULL ) *pActive = pData->iScrollbarActiveColor;
	if ( pFocus != NULL ) *pFocus = pData->iScrollbarFocusColor;
	if ( pDisabled != NULL ) *pDisabled = pData->iScrollbarDisabledColor;
	return XUI_OK;
}

XUI_API xui_widget xuiTreeViewGetFrameWidget(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return (pData != NULL) ? pData->pFrame : NULL;
}

XUI_API xui_widget xuiTreeViewGetViewportWidget(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return (pData != NULL) ? pData->pViewport : NULL;
}

XUI_API xui_scroll_model_t* xuiTreeViewGetModel(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return (pData != NULL && pData->pFrame != NULL) ? xuiScrollFrameGetModel(pData->pFrame) : NULL;
}

XUI_API xui_rect_t xuiTreeViewGetViewportRect(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return __xuiTreeViewViewportRectLocal(pWidget, pData);
}

XUI_API xui_rect_t xuiTreeViewGetItemRect(xui_widget pWidget, int iVisible)
{
	xui_tree_view_data_t* pData;
	xui_rect_t tViewport;
	float fOffsetY;

	pData = __xuiTreeViewGetData(pWidget);
	if ( (pData == NULL) || (iVisible < 0) || (iVisible >= pData->iVisibleCount) ) {
		return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	}
	tViewport = xuiTreeViewGetViewportRect(pWidget);
	fOffsetY = 0.0f;
	(void)xuiScrollFrameGetOffset(pData->pFrame, NULL, &fOffsetY);
	return (xui_rect_t){tViewport.fX, tViewport.fY + (float)iVisible * pData->fItemHeight - fOffsetY, tViewport.fW, pData->fItemHeight};
}

XUI_API int xuiTreeViewGetItemAt(xui_widget pWidget, float fX, float fY)
{
	xui_tree_view_data_t* pData;
	xui_rect_t tViewport;
	float fOffsetY;
	float fLocalY;
	int iVisible;

	pData = __xuiTreeViewGetData(pWidget);
	if ( (pData == NULL) || (pData->fItemHeight <= 0.0f) ) return -1;
	tViewport = xuiTreeViewGetViewportRect(pWidget);
	if ( !__xuiTreeViewPointInRect(tViewport, fX, fY) ) return -1;
	fOffsetY = 0.0f;
	(void)xuiScrollFrameGetOffset(pData->pFrame, NULL, &fOffsetY);
	fLocalY = fY - tViewport.fY + fOffsetY;
	iVisible = (int)(fLocalY / pData->fItemHeight);
	return (iVisible >= 0 && iVisible < pData->iVisibleCount) ? iVisible : -1;
}

XUI_API int xuiTreeViewGetHoverIndex(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return (pData != NULL) ? pData->iHoverVisible : -1;
}

XUI_API int xuiTreeViewGetFocusIndex(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return (pData != NULL) ? pData->iFocusVisible : -1;
}

XUI_API int xuiTreeViewGetSelectCount(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return (pData != NULL) ? pData->iSelectCount : 0;
}

XUI_API int xuiTreeViewGetChangeCount(xui_widget pWidget)
{
	xui_tree_view_data_t* pData = __xuiTreeViewGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}
