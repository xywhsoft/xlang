#include "xui_internal.h"

#include <string.h>

#define XUI_LIST_VIEW_DEFAULT_ITEM_HEIGHT 24.0f
#define XUI_LIST_VIEW_DEFAULT_WIDTH 180.0f
#define XUI_LIST_VIEW_DEFAULT_ROWS 6

typedef struct xui_list_view_data_t {
	xui_widget pFrame;
	xui_widget pViewport;
	char** arrItems;
	int* arrEnabled;
	int* arrSelected;
	int* arrExternalSelected;
	xui_font pFont;
	xui_list_view_select_proc onSelect;
	void* pSelectUser;
	xui_list_view_item_proc onRenderItem;
	void* pRenderItemUser;
	int iItemCount;
	int iExternalSelectedCount;
	int iSelectionMode;
	int iSelected;
	int iHover;
	int iFocus;
	int iAnchor;
	int iChangeCount;
	int iSelectCount;
	int bNotifyRepeatSelect;
	float fItemHeight;
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
	uint32_t iTrackColor;
	uint32_t iThumbColor;
	uint32_t iScrollbarHoverColor;
	uint32_t iScrollbarActiveColor;
	uint32_t iScrollbarFocusColor;
	uint32_t iScrollbarDisabledColor;
} xui_list_view_data_t;

static xui_list_view_data_t* __xuiListViewGetData(xui_widget pWidget);

static int __xuiListViewFloatValid(float fValue)
{
	return (fValue == fValue) && (fValue >= 0.0f) && (fValue <= XUI_LAYOUT_UNBOUNDED);
}

static int __xuiListViewDescValid(const xui_list_view_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) {
		return 0;
	}
	if ( (pDesc->iItemCount < 0) || (pDesc->iSelectionCount < 0) ) {
		return 0;
	}
	if ( ((pDesc->fItemHeight != 0.0f) && !__xuiListViewFloatValid(pDesc->fItemHeight)) ||
	     ((pDesc->fPadding != 0.0f) && !__xuiListViewFloatValid(pDesc->fPadding)) ||
	     ((pDesc->fBorderWidth != 0.0f) && !__xuiListViewFloatValid(pDesc->fBorderWidth)) ) {
		return 0;
	}
	return 1;
}

static int __xuiListViewSelectionModeValid(int iMode)
{
	return (iMode == XUI_SELECTION_SINGLE) ||
	       (iMode == XUI_SELECTION_MULTI) ||
	       (iMode == XUI_SELECTION_RANGE);
}

static int __xuiListViewScrollbarModeValid(int iMode)
{
	return (iMode == XUI_SCROLLBAR_MODE_FULL) || (iMode == XUI_SCROLLBAR_MODE_COMPACT);
}

static int __xuiListViewAlpha(uint32_t iColor)
{
	return (int)(iColor & 0xffu);
}

static float __xuiListViewMaxFloat(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static float __xuiListViewMinFloat(float fA, float fB)
{
	return (fA < fB) ? fA : fB;
}

static const char* __xuiListViewText(const char* sText)
{
	return (sText != NULL) ? sText : "";
}

static char* __xuiListViewDupText(const char* sText)
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

static void __xuiListViewFreeItems(char** arrItems, int iCount)
{
	int i;

	if ( arrItems == NULL ) {
		return;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( arrItems[i] != NULL ) {
			xrtFree(arrItems[i]);
		}
	}
	xrtFree(arrItems);
}

static void __xuiListViewDefaults(xui_list_view_data_t* pData)
{
	memset(pData, 0, sizeof(*pData));
	pData->iSelectionMode = XUI_SELECTION_SINGLE;
	pData->iSelected = -1;
	pData->iHover = -1;
	pData->iFocus = -1;
	pData->iAnchor = -1;
	pData->fItemHeight = XUI_LIST_VIEW_DEFAULT_ITEM_HEIGHT;
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
	pData->iTrackColor = XUI_COLOR_RGBA(224, 234, 244, 255);
	pData->iThumbColor = XUI_COLOR_RGBA(126, 161, 196, 245);
	pData->iScrollbarHoverColor = XUI_COLOR_RGBA(76, 136, 204, 250);
	pData->iScrollbarActiveColor = XUI_COLOR_RGBA(47, 128, 237, 255);
	pData->iScrollbarFocusColor = XUI_COLOR_RGBA(47, 128, 237, 180);
	pData->iScrollbarDisabledColor = XUI_COLOR_RGBA(181, 190, 204, 135);
}

static void __xuiListViewApplyDesc(xui_list_view_data_t* pData, const xui_list_view_desc_t* pDesc)
{
	if ( (pData == NULL) || (pDesc == NULL) ) {
		return;
	}
	pData->pFont = pDesc->pFont;
	if ( pDesc->fItemHeight > 0.0f ) pData->fItemHeight = pDesc->fItemHeight;
	if ( pDesc->fPadding > 0.0f ) pData->fPadding = pDesc->fPadding;
	if ( pDesc->fBorderWidth > 0.0f ) pData->fBorderWidth = pDesc->fBorderWidth;
	if ( __xuiListViewSelectionModeValid(pDesc->iSelectionMode) ) pData->iSelectionMode = pDesc->iSelectionMode;
	if ( pDesc->iSelected >= 0 ) pData->iSelected = pDesc->iSelected;
	pData->bNotifyRepeatSelect = pDesc->bNotifyRepeatSelect ? 1 : 0;
	if ( __xuiListViewAlpha(pDesc->iBackgroundColor) != 0 ) pData->iBackgroundColor = pDesc->iBackgroundColor;
	if ( __xuiListViewAlpha(pDesc->iBorderColor) != 0 ) pData->iBorderColor = pDesc->iBorderColor;
	if ( __xuiListViewAlpha(pDesc->iFocusColor) != 0 ) pData->iFocusColor = pDesc->iFocusColor;
	if ( pDesc->iRowColor != 0 ) pData->iRowColor = pDesc->iRowColor;
	if ( __xuiListViewAlpha(pDesc->iHoverColor) != 0 ) pData->iHoverColor = pDesc->iHoverColor;
	if ( __xuiListViewAlpha(pDesc->iSelectedColor) != 0 ) pData->iSelectedColor = pDesc->iSelectedColor;
	if ( __xuiListViewAlpha(pDesc->iTextColor) != 0 ) pData->iTextColor = pDesc->iTextColor;
	if ( __xuiListViewAlpha(pDesc->iDisabledTextColor) != 0 ) pData->iDisabledTextColor = pDesc->iDisabledTextColor;
	if ( __xuiListViewAlpha(pDesc->iTrackColor) != 0 ) pData->iTrackColor = pDesc->iTrackColor;
	if ( __xuiListViewAlpha(pDesc->iThumbColor) != 0 ) pData->iThumbColor = pDesc->iThumbColor;
	if ( __xuiListViewAlpha(pDesc->iScrollbarHoverColor) != 0 ) pData->iScrollbarHoverColor = pDesc->iScrollbarHoverColor;
	if ( __xuiListViewAlpha(pDesc->iScrollbarActiveColor) != 0 ) pData->iScrollbarActiveColor = pDesc->iScrollbarActiveColor;
	if ( __xuiListViewAlpha(pDesc->iScrollbarFocusColor) != 0 ) pData->iScrollbarFocusColor = pDesc->iScrollbarFocusColor;
	if ( __xuiListViewAlpha(pDesc->iScrollbarDisabledColor) != 0 ) pData->iScrollbarDisabledColor = pDesc->iScrollbarDisabledColor;
}

static int __xuiListViewStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
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

static int __xuiListViewStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
{
	xui_style_property_t tProperty;
	int iRet;

	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_FLOAT) && __xuiListViewFloatValid(tProperty.tValue.fFloat) ) {
		*pValue = tProperty.tValue.fFloat;
		return 1;
	}
	return 0;
}

static xui_font __xuiListViewStyleFont(xui_widget pWidget, xui_font pBaseFont)
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

static void __xuiListViewResolve(xui_widget pWidget, xui_list_view_data_t* pData, xui_list_view_data_t* pResolved)
{
	*pResolved = *pData;
	pResolved->pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	pResolved->pFont = __xuiListViewStyleFont(pWidget, pResolved->pFont);
	(void)__xuiListViewStyleColor(pWidget, "listview.background.color", &pResolved->iBackgroundColor);
	(void)__xuiListViewStyleColor(pWidget, "listview.border.color", &pResolved->iBorderColor);
	(void)__xuiListViewStyleColor(pWidget, "listview.focus.color", &pResolved->iFocusColor);
	(void)__xuiListViewStyleColor(pWidget, "listview.row.color", &pResolved->iRowColor);
	(void)__xuiListViewStyleColor(pWidget, "listview.row.hover_color", &pResolved->iHoverColor);
	(void)__xuiListViewStyleColor(pWidget, "listview.row.selected_color", &pResolved->iSelectedColor);
	(void)__xuiListViewStyleColor(pWidget, "listview.text.color", &pResolved->iTextColor);
	(void)__xuiListViewStyleColor(pWidget, "listview.text.disabled_color", &pResolved->iDisabledTextColor);
	(void)__xuiListViewStyleFloat(pWidget, "listview.item.height", &pResolved->fItemHeight);
	(void)__xuiListViewStyleFloat(pWidget, "listview.padding", &pResolved->fPadding);
	(void)__xuiListViewStyleFloat(pWidget, "listview.border.width", &pResolved->fBorderWidth);
	if ( pResolved->fItemHeight < 1.0f ) {
		pResolved->fItemHeight = XUI_LIST_VIEW_DEFAULT_ITEM_HEIGHT;
	}
}

static int* __xuiListViewSelectionArray(xui_list_view_data_t* pData, int* pCount)
{
	int* arrSelection;
	int iCount;

	if ( pCount != NULL ) {
		*pCount = 0;
	}
	if ( pData == NULL ) {
		return NULL;
	}
	arrSelection = pData->arrSelected;
	iCount = pData->iItemCount;
	if ( (pData->arrExternalSelected != NULL) && (pData->iExternalSelectedCount > 0) ) {
		arrSelection = pData->arrExternalSelected;
		iCount = pData->iExternalSelectedCount;
		if ( iCount > pData->iItemCount ) {
			iCount = pData->iItemCount;
		}
	}
	if ( pCount != NULL ) {
		*pCount = iCount;
	}
	return arrSelection;
}

static int __xuiListViewItemEnabledData(const xui_list_view_data_t* pData, int iIndex)
{
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) {
		return 0;
	}
	return (pData->arrEnabled == NULL) || pData->arrEnabled[iIndex];
}

static int __xuiListViewItemSelectedData(xui_list_view_data_t* pData, int iIndex)
{
	int* arrSelection;
	int iCount;

	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) {
		return 0;
	}
	if ( pData->iSelectionMode == XUI_SELECTION_SINGLE ) {
		return pData->iSelected == iIndex;
	}
	arrSelection = __xuiListViewSelectionArray(pData, &iCount);
	return (arrSelection != NULL) && (iIndex < iCount) && arrSelection[iIndex];
}

static void __xuiListViewClearSelectionArray(xui_list_view_data_t* pData)
{
	int* arrSelection;
	int iCount;

	arrSelection = __xuiListViewSelectionArray(pData, &iCount);
	if ( (arrSelection != NULL) && (iCount > 0) ) {
		memset(arrSelection, 0, (size_t)iCount * sizeof(int));
	}
}

static void __xuiListViewSetSelectionFlag(xui_list_view_data_t* pData, int iIndex, int bSelected)
{
	int* arrSelection;
	int iCount;

	arrSelection = __xuiListViewSelectionArray(pData, &iCount);
	if ( (arrSelection != NULL) && (iIndex >= 0) && (iIndex < iCount) ) {
		arrSelection[iIndex] = bSelected ? 1 : 0;
	}
}

static int __xuiListViewFirstSelected(xui_list_view_data_t* pData)
{
	int* arrSelection;
	int iCount;
	int i;

	if ( pData == NULL ) {
		return -1;
	}
	if ( pData->iSelectionMode == XUI_SELECTION_SINGLE ) {
		return __xuiListViewItemEnabledData(pData, pData->iSelected) ? pData->iSelected : -1;
	}
	arrSelection = __xuiListViewSelectionArray(pData, &iCount);
	if ( arrSelection == NULL ) {
		return -1;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( arrSelection[i] && __xuiListViewItemEnabledData(pData, i) ) {
			return i;
		}
	}
	return -1;
}

static void __xuiListViewSyncSingleSelection(xui_list_view_data_t* pData)
{
	__xuiListViewClearSelectionArray(pData);
	if ( __xuiListViewItemEnabledData(pData, pData->iSelected) ) {
		__xuiListViewSetSelectionFlag(pData, pData->iSelected, 1);
	} else {
		pData->iSelected = -1;
	}
}

static int __xuiListViewFindEnabled(const xui_list_view_data_t* pData, int iStart, int iStep)
{
	int i;

	if ( (pData == NULL) || (iStep == 0) ) {
		return -1;
	}
	for ( i = iStart; (i >= 0) && (i < pData->iItemCount); i += iStep ) {
		if ( __xuiListViewItemEnabledData(pData, i) ) {
			return i;
		}
	}
	return -1;
}

static int __xuiListViewFindNearestEnabled(const xui_list_view_data_t* pData, int iIndex, int iStep)
{
	int iResult;

	if ( pData == NULL ) {
		return -1;
	}
	if ( iIndex < 0 ) iIndex = 0;
	if ( iIndex >= pData->iItemCount ) iIndex = pData->iItemCount - 1;
	iResult = __xuiListViewFindEnabled(pData, iIndex, iStep);
	if ( iResult >= 0 ) {
		return iResult;
	}
	return __xuiListViewFindEnabled(pData, iIndex, -iStep);
}

static xui_rect_t __xuiListViewFrameRect(xui_widget pWidget, const xui_list_view_data_t* pData)
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

static int __xuiListViewApplyFrameStyle(xui_widget pWidget, xui_list_view_data_t* pData)
{
	int iRet;

	(void)pWidget;
	if ( (pData == NULL) || (pData->pFrame == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = xuiScrollFrameSetScrollbarMode(pData->pFrame, xuiScrollFrameGetScrollbarMode(pData->pFrame));
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameSetScrollbarPolicy(pData->pFrame, XUI_SCROLLBAR_POLICY_HIDDEN, XUI_SCROLLBAR_POLICY_AUTO);
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameSetWheelAxis(pData->pFrame, XUI_WHEEL_AXIS_VERTICAL);
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameSetWheelStep(pData->pFrame, pData->fItemHeight);
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameSetContentDragEnabled(pData->pFrame, 0);
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameSetMetrics(pData->pFrame, 8.0f, 18.0f, 0.0f);
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameSetColors(pData->pFrame, pData->iTrackColor, pData->iThumbColor, pData->iScrollbarHoverColor, pData->iScrollbarActiveColor, pData->iScrollbarFocusColor, pData->iScrollbarDisabledColor);
	return iRet;
}

static int __xuiListViewUpdateContentSize(xui_widget pWidget, xui_list_view_data_t* pData)
{
	xui_rect_t tFrame;
	float fHeight;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pFrame == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	tFrame = __xuiListViewFrameRect(pWidget, pData);
	fHeight = pData->fItemHeight * (float)pData->iItemCount;
	iRet = xuiScrollFrameSetContentSize(pData->pFrame, __xuiListViewMaxFloat(0.0f, tFrame.fW), __xuiListViewMaxFloat(0.0f, fHeight));
	if ( iRet != XUI_OK ) return iRet;
	return xuiScrollFrameSetWheelStep(pData->pFrame, pData->fItemHeight);
}

static int __xuiListViewInvalidateRows(xui_widget pWidget, xui_list_view_data_t* pData)
{
	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->pViewport != NULL ) {
		(void)xuiWidgetInvalidate(pData->pViewport, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiListViewInvalidateViewportRow(xui_list_view_data_t* pData, int iIndex)
{
	xui_rect_t tViewport;
	xui_rect_t tRow;
	float fOffsetY;
	float fTop;
	float fBottom;

	if ( (pData == NULL) || (pData->pViewport == NULL) || (pData->fItemHeight <= 0.0f) ) {
		return XUI_OK;
	}
	if ( (iIndex < 0) || (iIndex >= pData->iItemCount) ) {
		return XUI_OK;
	}
	tViewport = xuiWidgetGetRect(pData->pViewport);
	if ( (tViewport.fW <= 0.0f) || (tViewport.fH <= 0.0f) ) {
		return XUI_OK;
	}
	fOffsetY = 0.0f;
	(void)xuiScrollFrameGetOffset(pData->pFrame, NULL, &fOffsetY);
	fTop = (float)iIndex * pData->fItemHeight - fOffsetY;
	fBottom = fTop + pData->fItemHeight;
	if ( (fBottom <= 0.0f) || (fTop >= tViewport.fH) ) {
		return XUI_OK;
	}
	tRow.fX = 0.0f;
	tRow.fY = __xuiListViewMaxFloat(0.0f, fTop);
	tRow.fW = tViewport.fW;
	tRow.fH = __xuiListViewMinFloat(tViewport.fH, fBottom) - tRow.fY;
	if ( tRow.fH <= 0.0f ) {
		return XUI_OK;
	}
	return xuiWidgetInvalidateRect(pData->pViewport, tRow, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiListViewInvalidateHoverRows(xui_widget pWidget, xui_list_view_data_t* pData, int iOldHover, int iNewHover)
{
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->pViewport == NULL ) {
		return __xuiListViewInvalidateRows(pWidget, pData);
	}
	iRet = __xuiListViewInvalidateViewportRow(pData, iOldHover);
	if ( iRet == XUI_OK ) {
		iRet = __xuiListViewInvalidateViewportRow(pData, iNewHover);
	}
	return iRet;
}

static uint32_t __xuiListViewState(xui_widget pWidget)
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

static int __xuiListViewSyncState(xui_widget pWidget)
{
	return xuiWidgetSetStateId(pWidget, __xuiListViewState(pWidget));
}

static void __xuiListViewNotifySelect(xui_widget pWidget, xui_list_view_data_t* pData, int iIndex)
{
	if ( (pWidget == NULL) || (pData == NULL) || (iIndex < 0) ) {
		return;
	}
	pData->iSelectCount++;
	if ( pData->onSelect != NULL ) {
		pData->onSelect(pWidget, iIndex, pData->pSelectUser);
	}
}

static int __xuiListViewSetSelectedInternal(xui_widget pWidget, xui_list_view_data_t* pData, int iIndex, int bNotify, int bRepeat)
{
	int iOld;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( !__xuiListViewItemEnabledData(pData, iIndex) ) {
		iIndex = -1;
	}
	iOld = pData->iSelected;
	if ( (iOld == iIndex) && !(bNotify && (bRepeat || pData->bNotifyRepeatSelect)) ) {
		return XUI_OK;
	}
	pData->iSelected = iIndex;
	pData->iFocus = iIndex;
	if ( iIndex >= 0 ) {
		pData->iAnchor = iIndex;
	}
	if ( pData->iSelectionMode == XUI_SELECTION_SINGLE || iIndex < 0 ) {
		__xuiListViewSyncSingleSelection(pData);
	} else {
		__xuiListViewSetSelectionFlag(pData, iIndex, 1);
	}
	if ( iOld != iIndex ) {
		pData->iChangeCount++;
	}
	(void)xuiListViewEnsureVisible(pWidget, iIndex);
	(void)__xuiListViewInvalidateRows(pWidget, pData);
	if ( bNotify && ((iOld != iIndex) || bRepeat || pData->bNotifyRepeatSelect) ) {
		__xuiListViewNotifySelect(pWidget, pData, iIndex);
	}
	return XUI_OK;
}

static int __xuiListViewSelectRange(xui_widget pWidget, xui_list_view_data_t* pData, int iFrom, int iTo, int bNotify)
{
	int iStart;
	int iEnd;
	int i;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( !__xuiListViewItemEnabledData(pData, iTo) ) {
		return XUI_OK;
	}
	if ( !__xuiListViewItemEnabledData(pData, iFrom) ) {
		iFrom = iTo;
	}
	iStart = (iFrom < iTo) ? iFrom : iTo;
	iEnd = (iFrom < iTo) ? iTo : iFrom;
	__xuiListViewClearSelectionArray(pData);
	for ( i = iStart; i <= iEnd; i++ ) {
		if ( __xuiListViewItemEnabledData(pData, i) ) {
			__xuiListViewSetSelectionFlag(pData, i, 1);
		}
	}
	pData->iSelected = iTo;
	pData->iFocus = iTo;
	pData->iChangeCount++;
	(void)xuiListViewEnsureVisible(pWidget, iTo);
	(void)__xuiListViewInvalidateRows(pWidget, pData);
	if ( bNotify ) {
		__xuiListViewNotifySelect(pWidget, pData, iTo);
	}
	return XUI_OK;
}

static int __xuiListViewSelectIndex(xui_widget pWidget, xui_list_view_data_t* pData, int iIndex, uint32_t iModifiers, int bNotify)
{
	int bCtrl;
	int bShift;
	int bSelected;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->iSelectionMode == XUI_SELECTION_SINGLE ) {
		return __xuiListViewSetSelectedInternal(pWidget, pData, iIndex, bNotify, 0);
	}
	if ( !__xuiListViewItemEnabledData(pData, iIndex) ) {
		return XUI_OK;
	}
	bCtrl = ((iModifiers & XUI_MOD_CTRL) != 0);
	bShift = ((iModifiers & XUI_MOD_SHIFT) != 0);
	if ( (pData->iSelectionMode == XUI_SELECTION_RANGE) && bShift && (pData->iAnchor >= 0) ) {
		return __xuiListViewSelectRange(pWidget, pData, pData->iAnchor, iIndex, bNotify);
	}
	if ( bCtrl ) {
		bSelected = __xuiListViewItemSelectedData(pData, iIndex);
		__xuiListViewSetSelectionFlag(pData, iIndex, !bSelected);
		pData->iSelected = iIndex;
		pData->iFocus = iIndex;
		pData->iAnchor = iIndex;
	} else {
		__xuiListViewClearSelectionArray(pData);
		__xuiListViewSetSelectionFlag(pData, iIndex, 1);
		pData->iSelected = iIndex;
		pData->iFocus = iIndex;
		pData->iAnchor = iIndex;
	}
	pData->iChangeCount++;
	(void)xuiListViewEnsureVisible(pWidget, iIndex);
	(void)__xuiListViewInvalidateRows(pWidget, pData);
	if ( bNotify ) {
		__xuiListViewNotifySelect(pWidget, pData, iIndex);
	}
	return XUI_OK;
}

static xui_rect_t __xuiListViewViewportRectLocal(xui_widget pWidget, const xui_list_view_data_t* pData)
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

static int __xuiListViewPointInRect(xui_rect_t tRect, float fX, float fY)
{
	return (fX >= tRect.fX) && (fY >= tRect.fY) &&
	       (fX < (tRect.fX + tRect.fW)) && (fY < (tRect.fY + tRect.fH));
}

static int __xuiListViewHitWorld(xui_widget pWidget, xui_list_view_data_t* pData, float fX, float fY)
{
	xui_rect_t tViewportWorld;
	float fOffsetY;
	float fLocalY;
	int iIndex;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pViewport == NULL) || (pData->fItemHeight <= 0.0f) ) {
		return -1;
	}
	tViewportWorld = xuiWidgetGetWorldRect(pData->pViewport);
	if ( !__xuiListViewPointInRect(tViewportWorld, fX, fY) ) {
		return -1;
	}
	fOffsetY = 0.0f;
	(void)xuiScrollFrameGetOffset(pData->pFrame, NULL, &fOffsetY);
	fLocalY = fY - tViewportWorld.fY + fOffsetY;
	if ( fLocalY < 0.0f ) {
		return -1;
	}
	iIndex = (int)(fLocalY / pData->fItemHeight);
	if ( (iIndex < 0) || (iIndex >= pData->iItemCount) ) {
		return -1;
	}
	return iIndex;
}

static int __xuiListViewSetHover(xui_widget pWidget, xui_list_view_data_t* pData, int iHover)
{
	int iOldHover;

	if ( (pData == NULL) || (pData->iHover == iHover) ) {
		return XUI_OK;
	}
	iOldHover = pData->iHover;
	pData->iHover = iHover;
	return __xuiListViewInvalidateHoverRows(pWidget, pData, iOldHover, iHover);
}

static int __xuiListViewPointerMove(xui_widget pWidget, xui_list_view_data_t* pData, const xui_event_t* pEvent)
{
	int iHover;

	if ( (pEvent == NULL) || (pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) ) {
		return XUI_OK;
	}
	iHover = __xuiListViewHitWorld(pWidget, pData, pEvent->fX, pEvent->fY);
	return __xuiListViewSetHover(pWidget, pData, iHover);
}

static int __xuiListViewPointerDown(xui_widget pWidget, xui_list_view_data_t* pData, const xui_event_t* pEvent)
{
	int iIndex;

	if ( (pEvent == NULL) || (pEvent->iButton != XUI_POINTER_BUTTON_LEFT) || (pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) ) {
		return XUI_OK;
	}
	if ( !xuiWidgetGetEnabled(pWidget) ) {
		return XUI_OK;
	}
	iIndex = __xuiListViewHitWorld(pWidget, pData, pEvent->fX, pEvent->fY);
	if ( iIndex < 0 ) {
		return XUI_OK;
	}
	(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
	if ( !__xuiListViewItemEnabledData(pData, iIndex) ) {
		return XUI_EVENT_DISPATCH_STOP;
	}
	(void)__xuiListViewSetHover(pWidget, pData, iIndex);
	(void)__xuiListViewSelectIndex(pWidget, pData, iIndex, pEvent->iModifiers, 1);
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiListViewMoveKeyboard(xui_widget pWidget, xui_list_view_data_t* pData, int iTarget, uint32_t iModifiers)
{
	if ( !__xuiListViewItemEnabledData(pData, iTarget) ) {
		return XUI_OK;
	}
	if ( (pData->iSelectionMode == XUI_SELECTION_RANGE) &&
	     ((iModifiers & XUI_MOD_SHIFT) != 0) &&
	     (pData->iAnchor >= 0) ) {
		return __xuiListViewSelectRange(pWidget, pData, pData->iAnchor, iTarget, 1);
	}
	return __xuiListViewSelectIndex(pWidget, pData, iTarget, 0, 1);
}

static int __xuiListViewKeyDown(xui_widget pWidget, xui_list_view_data_t* pData, const xui_event_t* pEvent)
{
	xui_rect_t tViewport;
	int iCurrent;
	int iTarget;
	int iRows;

	if ( (pEvent == NULL) || (pEvent->iPhase == XUI_EVENT_PHASE_BUBBLE) || !xuiWidgetGetEnabled(pWidget) ) {
		return XUI_OK;
	}
	iCurrent = __xuiListViewItemEnabledData(pData, pData->iSelected) ? pData->iSelected : __xuiListViewFirstSelected(pData);
	switch ( pEvent->iKey ) {
	case XUI_KEY_UP:
		iTarget = (iCurrent >= 0) ? __xuiListViewFindEnabled(pData, iCurrent - 1, -1) : __xuiListViewFindEnabled(pData, 0, 1);
		if ( iTarget >= 0 ) {
			(void)__xuiListViewMoveKeyboard(pWidget, pData, iTarget, pEvent->iModifiers);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_KEY_DOWN:
		iTarget = (iCurrent >= 0) ? __xuiListViewFindEnabled(pData, iCurrent + 1, 1) : __xuiListViewFindEnabled(pData, 0, 1);
		if ( iTarget >= 0 ) {
			(void)__xuiListViewMoveKeyboard(pWidget, pData, iTarget, pEvent->iModifiers);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_KEY_PAGE_UP:
		tViewport = xuiListViewGetViewportRect(pWidget);
		iRows = (pData->fItemHeight > 0.0f) ? (int)(tViewport.fH / pData->fItemHeight) : 1;
		if ( iRows < 1 ) iRows = 1;
		iTarget = (iCurrent >= 0) ? __xuiListViewFindNearestEnabled(pData, iCurrent - iRows, -1) : __xuiListViewFindEnabled(pData, 0, 1);
		if ( iTarget >= 0 ) {
			(void)__xuiListViewMoveKeyboard(pWidget, pData, iTarget, pEvent->iModifiers);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_KEY_PAGE_DOWN:
		tViewport = xuiListViewGetViewportRect(pWidget);
		iRows = (pData->fItemHeight > 0.0f) ? (int)(tViewport.fH / pData->fItemHeight) : 1;
		if ( iRows < 1 ) iRows = 1;
		iTarget = (iCurrent >= 0) ? __xuiListViewFindNearestEnabled(pData, iCurrent + iRows, 1) : __xuiListViewFindEnabled(pData, 0, 1);
		if ( iTarget >= 0 ) {
			(void)__xuiListViewMoveKeyboard(pWidget, pData, iTarget, pEvent->iModifiers);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_KEY_HOME:
		iTarget = __xuiListViewFindEnabled(pData, 0, 1);
		if ( iTarget >= 0 ) {
			(void)__xuiListViewMoveKeyboard(pWidget, pData, iTarget, pEvent->iModifiers);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_KEY_END:
		iTarget = __xuiListViewFindEnabled(pData, pData->iItemCount - 1, -1);
		if ( iTarget >= 0 ) {
			(void)__xuiListViewMoveKeyboard(pWidget, pData, iTarget, pEvent->iModifiers);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_KEY_ENTER:
	case XUI_KEY_SPACE:
		if ( __xuiListViewItemEnabledData(pData, pData->iSelected) ) {
			(void)__xuiListViewSetSelectedInternal(pWidget, pData, pData->iSelected, 1, 1);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiListViewEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_list_view_data_t* pData;

	(void)pUser;
	if ( (pWidget == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiListViewGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_MOVE:
		return __xuiListViewPointerMove(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_LEAVE:
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		return __xuiListViewSetHover(pWidget, pData, -1);
	case XUI_EVENT_POINTER_DOWN:
		return __xuiListViewPointerDown(pWidget, pData, pEvent);
	case XUI_EVENT_KEY_DOWN:
		return __xuiListViewKeyDown(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_WHEEL:
		if ( pEvent->iPhase != XUI_EVENT_PHASE_CAPTURE ) {
			(void)__xuiListViewSetHover(pWidget, pData, -1);
		}
		return XUI_OK;
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
	case XUI_EVENT_ENABLED_CHANGED:
	case XUI_EVENT_VISIBLE_CHANGED:
		if ( pEvent->iType == XUI_EVENT_ENABLED_CHANGED && pData->pFrame != NULL ) {
			(void)xuiWidgetSetEnabled(pData->pFrame, xuiWidgetGetEnabled(pWidget));
		}
		(void)__xuiListViewSyncState(pWidget);
		return __xuiListViewInvalidateRows(pWidget, pData);
	case XUI_EVENT_BOUNDS_CHANGED:
		(void)__xuiListViewUpdateContentSize(pWidget, pData);
		return XUI_OK;
	default:
		break;
	}
	return XUI_OK;
}

static void __xuiListViewFrameChanged(xui_widget pFrame, float fOffsetX, float fOffsetY, void* pUser)
{
	xui_widget pWidget;
	xui_list_view_data_t* pData;

	(void)pFrame;
	(void)fOffsetX;
	(void)fOffsetY;
	pWidget = (xui_widget)pUser;
	pData = __xuiListViewGetData(pWidget);
	if ( pData == NULL ) {
		return;
	}
	pData->iHover = -1;
	pData->iChangeCount++;
	(void)__xuiListViewInvalidateRows(pWidget, pData);
}

static int __xuiListViewContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_list_view_data_t* pData;
	xui_list_view_data_t tResolved;
	float fRows;

	(void)tConstraint;
	pData = (xui_list_view_data_t*)pUser;
	if ( (pWidget == NULL) || (pData == NULL) || (pSize == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiListViewResolve(pWidget, pData, &tResolved);
	fRows = (float)((pData->iItemCount > 0 && pData->iItemCount < XUI_LIST_VIEW_DEFAULT_ROWS) ? pData->iItemCount : XUI_LIST_VIEW_DEFAULT_ROWS);
	if ( fRows < 3.0f ) {
		fRows = 3.0f;
	}
	pSize->fX = xuiInternalSnapSize(XUI_LIST_VIEW_DEFAULT_WIDTH);
	pSize->fY = xuiInternalSnapSize((tResolved.fItemHeight * fRows) + tResolved.fBorderWidth * 2.0f);
	return XUI_OK;
}

static int __xuiListViewArrange(xui_widget pWidget, xui_rect_t tContentRect, void* pUser)
{
	xui_list_view_data_t* pData;
	xui_list_view_data_t tResolved;
	xui_rect_t tFrame;
	int iRet;

	(void)tContentRect;
	pData = (xui_list_view_data_t*)pUser;
	if ( (pWidget == NULL) || (pData == NULL) || (pData->pFrame == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiListViewResolve(pWidget, pData, &tResolved);
	pData->fItemHeight = tResolved.fItemHeight;
	pData->fPadding = tResolved.fPadding;
	pData->fBorderWidth = tResolved.fBorderWidth;
	tFrame = __xuiListViewFrameRect(pWidget, pData);
	iRet = __xuiListViewUpdateContentSize(pWidget, pData);
	if ( iRet == XUI_OK ) iRet = xuiWidgetArrange(pData->pFrame, tFrame);
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameLayout(pData->pFrame);
	return iRet;
}

static int __xuiListViewDrawRectFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( __xuiListViewAlpha(iColor) == 0 ) {
		return XUI_OK;
	}
	return pProxy->drawRectFill(pProxy, pDraw, tRect, iColor);
}

static int __xuiListViewDrawRectStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	if ( (fWidth <= 0.0f) || (__xuiListViewAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	return pProxy->drawRectStroke(pProxy, pDraw, tRect, fWidth, iColor);
}

static int __xuiListViewCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_list_view_data_t* pData;
	xui_list_view_data_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tRect;
	int bFocused;
	int iRet;

	(void)iStateId;
	(void)pUser;
	if ( (pWidget == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiListViewGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	__xuiListViewResolve(pWidget, pData, &tResolved);
	tRect = xuiWidgetGetRect(pWidget);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	tRect = xuiInternalSnapRect(tRect);
	iRet = __xuiListViewDrawRectFill(pProxy, pDraw, tRect, tResolved.iBackgroundColor);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiListViewDrawRectStroke(pProxy, pDraw, tRect, tResolved.fBorderWidth, tResolved.iBorderColor);
	if ( iRet != XUI_OK ) return iRet;
	bFocused = ((xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget) && xuiWidgetGetEnabled(pWidget)) ? 1 : 0;
	if ( bFocused ) {
		iRet = __xuiListViewDrawRectStroke(pProxy, pDraw, tRect, tResolved.fBorderWidth, tResolved.iFocusColor);
	}
	return iRet;
}

static int __xuiListViewViewportRender(xui_widget pViewport, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_widget pWidget;
	xui_list_view_data_t* pData;
	xui_list_view_data_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tRect;
	xui_rect_t tRow;
	xui_rect_t tFill;
	xui_rect_t tText;
	float fOffsetY;
	float fViewportW;
	float fViewportH;
	float fY;
	int iStart;
	int iEnd;
	int i;
	int iState;
	int iHandled;
	int iRet;
	uint32_t iTextColor;

	(void)iStateId;
	pWidget = (xui_widget)pUser;
	pData = __xuiListViewGetData(pWidget);
	if ( (pViewport == NULL) || (pData == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	__xuiListViewResolve(pWidget, pData, &tResolved);
	tRect = xuiWidgetGetRect(pViewport);
	fViewportW = __xuiListViewMaxFloat(0.0f, tRect.fW);
	fViewportH = __xuiListViewMaxFloat(0.0f, tRect.fH);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	tRect = xuiInternalSnapRect(tRect);
	iRet = pProxy->drawRectFill(pProxy, pDraw, tRect, tResolved.iBackgroundColor);
	if ( iRet != XUI_OK ) return iRet;
	if ( (pData->iItemCount <= 0) || (tResolved.fItemHeight <= 0.0f) ) {
		return XUI_OK;
	}
	fOffsetY = 0.0f;
	(void)xuiScrollFrameGetOffset(pData->pFrame, NULL, &fOffsetY);
	iStart = (int)(fOffsetY / tResolved.fItemHeight);
	if ( iStart < 0 ) iStart = 0;
	iEnd = (int)((fOffsetY + fViewportH) / tResolved.fItemHeight) + 2;
	if ( iEnd > pData->iItemCount ) iEnd = pData->iItemCount;
	for ( i = iStart; i < iEnd; i++ ) {
		fY = ((float)i * tResolved.fItemHeight) - fOffsetY;
		tRow = xuiInternalSnapRect((xui_rect_t){0.0f, fY, fViewportW, tResolved.fItemHeight});
		iState = 0;
		if ( __xuiListViewItemSelectedData(pData, i) ) iState |= XUI_LIST_ITEM_SELECTED;
		if ( pData->iHover == i ) iState |= XUI_LIST_ITEM_HOVER;
		if ( !__xuiListViewItemEnabledData(pData, i) || !xuiWidgetGetEnabled(pWidget) ) iState |= XUI_LIST_ITEM_DISABLED;
		if ( pData->iFocus == i ) iState |= XUI_LIST_ITEM_FOCUS;
		if ( pData->onRenderItem != NULL ) {
			iHandled = pData->onRenderItem(pWidget, i, pDraw, tRow, iState, pData->pRenderItemUser);
			if ( iHandled < 0 ) return iHandled;
			if ( iHandled ) continue;
		}
		if ( (iState & XUI_LIST_ITEM_SELECTED) != 0 ) {
			tFill = xuiInternalSnapRect((xui_rect_t){tRow.fX + 3.0f, tRow.fY + 2.0f, __xuiListViewMaxFloat(1.0f, tRow.fW - 6.0f), __xuiListViewMaxFloat(1.0f, tRow.fH - 4.0f)});
			iRet = __xuiListViewDrawRectFill(pProxy, pDraw, tFill, tResolved.iSelectedColor);
			if ( iRet != XUI_OK ) return iRet;
		} else if ( (iState & XUI_LIST_ITEM_HOVER) != 0 ) {
			tFill = xuiInternalSnapRect((xui_rect_t){tRow.fX + 3.0f, tRow.fY + 2.0f, __xuiListViewMaxFloat(1.0f, tRow.fW - 6.0f), __xuiListViewMaxFloat(1.0f, tRow.fH - 4.0f)});
			iRet = __xuiListViewDrawRectFill(pProxy, pDraw, tFill, tResolved.iHoverColor);
			if ( iRet != XUI_OK ) return iRet;
		} else if ( __xuiListViewAlpha(tResolved.iRowColor) != 0 ) {
			iRet = pProxy->drawRectFill(pProxy, pDraw, tRow, tResolved.iRowColor);
			if ( iRet != XUI_OK ) return iRet;
		}
		if ( ((iState & XUI_LIST_ITEM_FOCUS) != 0) &&
		     (xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget) &&
		     ((iState & XUI_LIST_ITEM_DISABLED) == 0) ) {
			tFill = xuiInternalSnapRect((xui_rect_t){tRow.fX + 3.0f, tRow.fY + 2.0f, __xuiListViewMaxFloat(1.0f, tRow.fW - 6.0f), __xuiListViewMaxFloat(1.0f, tRow.fH - 4.0f)});
			iRet = __xuiListViewDrawRectStroke(pProxy, pDraw, tFill, 1.0f, tResolved.iFocusColor);
			if ( iRet != XUI_OK ) return iRet;
		}
		iTextColor = ((iState & XUI_LIST_ITEM_DISABLED) != 0) ? tResolved.iDisabledTextColor : tResolved.iTextColor;
		if ( (iState & XUI_LIST_ITEM_SELECTED) != 0 ) {
			iTextColor = XUI_COLOR_RGBA(255, 255, 255, 255);
		}
		if ( (tResolved.pFont != NULL) && (__xuiListViewAlpha(iTextColor) != 0) ) {
			tText = xuiInternalSnapRect((xui_rect_t){tRow.fX + tResolved.fPadding, tRow.fY, __xuiListViewMaxFloat(1.0f, tRow.fW - tResolved.fPadding * 2.0f), tRow.fH});
			iRet = pProxy->drawText(pProxy, pDraw, tResolved.pFont, __xuiListViewText(pData->arrItems[i]), tText, iTextColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	return XUI_OK;
}

static void __xuiListViewDefaultLayout(xui_layout_t* pLayout)
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

static void __xuiListViewDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiListViewInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiListViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiListViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiListViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_WHEEL, __xuiListViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiListViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiListViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiListViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiListViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BOUNDS_CHANGED, __xuiListViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_ENABLED_CHANGED, __xuiListViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_VISIBLE_CHANGED, __xuiListViewEvent, NULL);
	return iRet;
}

static int __xuiListViewInitViewport(xui_widget pWidget, xui_list_view_data_t* pData)
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
	return xuiWidgetSetCacheRenderCallback(pData->pViewport, __xuiListViewViewportRender, pWidget);
}

static int __xuiListViewCreateFrame(xui_widget pWidget, xui_list_view_data_t* pData, const xui_list_view_desc_t* pDesc)
{
	xui_scroll_frame_desc_t tFrameDesc;
	int iRet;

	memset(&tFrameDesc, 0, sizeof(tFrameDesc));
	tFrameDesc.iSize = sizeof(tFrameDesc);
	tFrameDesc.iPolicyX = XUI_SCROLLBAR_POLICY_HIDDEN;
	tFrameDesc.iPolicyY = XUI_SCROLLBAR_POLICY_AUTO;
	tFrameDesc.iScrollbarMode = (pDesc != NULL && pDesc->iScrollbarMode != XUI_SCROLLBAR_MODE_FULL && __xuiListViewScrollbarModeValid(pDesc->iScrollbarMode)) ? pDesc->iScrollbarMode : XUI_SCROLLBAR_MODE_COMPACT;
	tFrameDesc.iWheelAxis = XUI_WHEEL_AXIS_VERTICAL;
	tFrameDesc.iCornerMode = XUI_SCROLL_FRAME_CORNER_NONE;
	tFrameDesc.bContentDragEnabled = 0;
	tFrameDesc.fScrollbarSize = 8.0f;
	tFrameDesc.fMinThumbSize = 18.0f;
	tFrameDesc.fWheelStep = pData->fItemHeight;
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
	iRet = xuiScrollFrameSetChange(pData->pFrame, __xuiListViewFrameChanged, pWidget);
	if ( iRet == XUI_OK ) iRet = __xuiListViewInitViewport(pWidget, pData);
	if ( iRet == XUI_OK ) iRet = __xuiListViewApplyFrameStyle(pWidget, pData);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pData->pFrame);
		pData->pFrame = NULL;
		pData->pViewport = NULL;
		return iRet;
	}
	return XUI_OK;
}

static int __xuiListViewInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_list_view_data_t* pData;
	const xui_list_view_desc_t* pDesc;
	int iRet;

	(void)pUser;
	pData = (xui_list_view_data_t*)pTypeData;
	pDesc = (const xui_list_view_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiListViewDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiListViewDefaults(pData);
	__xuiListViewApplyDesc(pData, pDesc);
	(void)xuiWidgetSetLayoutType(pWidget, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetFlowMode(pWidget, XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_CLIP);
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	(void)xuiWidgetSetPadding(pWidget, (xui_thickness_t){0.0f, 0.0f, 0.0f, 0.0f});
	iRet = __xuiListViewCreateFrame(pWidget, pData, pDesc);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiListViewInitEvents(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	if ( pDesc != NULL ) {
		if ( (pDesc->arrSelected != NULL) && (pDesc->iSelectionCount > 0) ) {
			pData->arrExternalSelected = pDesc->arrSelected;
			pData->iExternalSelectedCount = pDesc->iSelectionCount;
		}
		iRet = xuiListViewSetItems(pWidget, pDesc->arrItems, pDesc->iItemCount);
		if ( iRet != XUI_OK ) return iRet;
		iRet = xuiListViewSetEnabledItems(pWidget, pDesc->arrEnabled, pDesc->iItemCount);
		if ( iRet != XUI_OK ) return iRet;
		if ( pDesc->iSelected >= 0 ) {
			iRet = xuiListViewSetSelected(pWidget, pDesc->iSelected);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	(void)__xuiListViewSyncState(pWidget);
	return XUI_OK;
}

static void __xuiListViewDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_list_view_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_list_view_data_t*)pTypeData;
	if ( pData != NULL ) {
		__xuiListViewFreeItems(pData->arrItems, pData->iItemCount);
		if ( pData->arrEnabled != NULL ) xrtFree(pData->arrEnabled);
		if ( pData->arrSelected != NULL ) xrtFree(pData->arrSelected);
		memset(pData, 0, sizeof(*pData));
	}
}

static void __xuiListViewRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
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

static void __xuiListViewRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;
	uint32_t iLayoutDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	iLayoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiListViewRegisterStyleProperty(pContext, pType, "listview.background.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiListViewRegisterStyleProperty(pContext, pType, "listview.border.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiListViewRegisterStyleProperty(pContext, pType, "listview.focus.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiListViewRegisterStyleProperty(pContext, pType, "listview.row.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiListViewRegisterStyleProperty(pContext, pType, "listview.row.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiListViewRegisterStyleProperty(pContext, pType, "listview.row.selected_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiListViewRegisterStyleProperty(pContext, pType, "listview.text.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiListViewRegisterStyleProperty(pContext, pType, "listview.text.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiListViewRegisterStyleProperty(pContext, pType, "listview.item.height", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiListViewRegisterStyleProperty(pContext, pType, "listview.padding", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiListViewRegisterStyleProperty(pContext, pType, "listview.border.width", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiListViewRegisterStyleProperty(pContext, pType, "font.name", XUI_STYLE_VALUE_STRING, iLayoutDirty, XUI_STYLE_PROPERTY_INHERITED);
}

static xui_list_view_data_t* __xuiListViewGetData(xui_widget pWidget)
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
	pType = xuiWidgetFindType(pContext, "listview");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_list_view_data_t*)xuiWidgetGetTypeData(pWidget);
}

XUI_API xui_widget_type xuiListViewGetType(xui_context pContext)
{
	xui_widget_type pType;
	xui_widget_type_desc_t tDesc;
	xui_layout_t tLayout;
	xui_cache_policy_t tPolicy;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "listview");
	if ( pType != NULL ) {
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "listview";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_list_view_data_t);
	tDesc.onInit = __xuiListViewInit;
	tDesc.onDestroy = __xuiListViewDestroy;
	tDesc.onContentMeasure = __xuiListViewContentMeasure;
	tDesc.onLayoutArrange = __xuiListViewArrange;
	tDesc.onCacheRender = __xuiListViewCacheRender;
	__xuiListViewDefaultLayout(&tLayout);
	__xuiListViewDefaultCachePolicy(&tPolicy);
	tDesc.tLayout = tLayout;
	tDesc.tCachePolicy = tPolicy;
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	__xuiListViewRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiListViewCreate(xui_context pContext, xui_widget* ppWidget, const xui_list_view_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( !xuiInternalContextIsValid(pContext) || (ppWidget == NULL) || !__xuiListViewDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiListViewGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiListViewSetSelect(xui_widget pWidget, xui_list_view_select_proc onSelect, void* pUser)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onSelect = onSelect;
	pData->pSelectUser = pUser;
	return XUI_OK;
}

XUI_API int xuiListViewSetItemRenderer(xui_widget pWidget, xui_list_view_item_proc onRender, void* pUser)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onRenderItem = onRender;
	pData->pRenderItemUser = pUser;
	return __xuiListViewInvalidateRows(pWidget, pData);
}

XUI_API int xuiListViewSetItems(xui_widget pWidget, const char** arrItems, int iCount)
{
	xui_list_view_data_t* pData;
	char** arrNewItems;
	int* arrNewEnabled;
	int* arrNewSelected;
	int i;
	int iRet;

	pData = __xuiListViewGetData(pWidget);
	if ( (pData == NULL) || (iCount < 0) || ((iCount > 0) && (arrItems == NULL)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	arrNewItems = NULL;
	arrNewEnabled = NULL;
	arrNewSelected = NULL;
	if ( iCount > 0 ) {
		arrNewItems = (char**)xrtMalloc((size_t)iCount * sizeof(char*));
		arrNewEnabled = (int*)xrtMalloc((size_t)iCount * sizeof(int));
		arrNewSelected = (int*)xrtMalloc((size_t)iCount * sizeof(int));
		if ( (arrNewItems == NULL) || (arrNewEnabled == NULL) || (arrNewSelected == NULL) ) {
			if ( arrNewItems != NULL ) xrtFree(arrNewItems);
			if ( arrNewEnabled != NULL ) xrtFree(arrNewEnabled);
			if ( arrNewSelected != NULL ) xrtFree(arrNewSelected);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		memset(arrNewItems, 0, (size_t)iCount * sizeof(char*));
		memset(arrNewSelected, 0, (size_t)iCount * sizeof(int));
		for ( i = 0; i < iCount; i++ ) {
			arrNewEnabled[i] = 1;
			arrNewItems[i] = __xuiListViewDupText(arrItems[i]);
			if ( arrNewItems[i] == NULL ) {
				__xuiListViewFreeItems(arrNewItems, iCount);
				xrtFree(arrNewEnabled);
				xrtFree(arrNewSelected);
				return XUI_ERROR_OUT_OF_MEMORY;
			}
		}
	}
	__xuiListViewFreeItems(pData->arrItems, pData->iItemCount);
	if ( pData->arrEnabled != NULL ) xrtFree(pData->arrEnabled);
	if ( pData->arrSelected != NULL ) xrtFree(pData->arrSelected);
	pData->arrItems = arrNewItems;
	pData->arrEnabled = arrNewEnabled;
	pData->arrSelected = arrNewSelected;
	pData->iItemCount = iCount;
	pData->iHover = -1;
	if ( !__xuiListViewItemEnabledData(pData, pData->iSelected) ) {
		pData->iSelected = -1;
	}
	pData->iFocus = pData->iSelected;
	pData->iAnchor = pData->iSelected;
	if ( pData->iSelectionMode == XUI_SELECTION_SINGLE ) {
		__xuiListViewSyncSingleSelection(pData);
	} else if ( __xuiListViewItemEnabledData(pData, pData->iSelected) ) {
		__xuiListViewSetSelectionFlag(pData, pData->iSelected, 1);
	} else {
		pData->iSelected = __xuiListViewFirstSelected(pData);
	}
	iRet = __xuiListViewUpdateContentSize(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	pData->iChangeCount++;
	if ( pData->pViewport != NULL ) {
		(void)xuiWidgetInvalidate(pData->pViewport, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiListViewSetEnabledItems(xui_widget pWidget, const int* arrEnabled, int iCount)
{
	xui_list_view_data_t* pData;
	int i;

	pData = __xuiListViewGetData(pWidget);
	if ( (pData == NULL) || (iCount < 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < pData->iItemCount; i++ ) {
		pData->arrEnabled[i] = (arrEnabled == NULL || i >= iCount || arrEnabled[i]) ? 1 : 0;
		if ( !pData->arrEnabled[i] ) {
			__xuiListViewSetSelectionFlag(pData, i, 0);
		}
	}
	if ( !__xuiListViewItemEnabledData(pData, pData->iSelected) ) {
		pData->iSelected = __xuiListViewFirstSelected(pData);
	}
	if ( pData->iSelectionMode == XUI_SELECTION_SINGLE ) {
		__xuiListViewSyncSingleSelection(pData);
	}
	pData->iFocus = pData->iSelected;
	pData->iAnchor = pData->iSelected;
	pData->iChangeCount++;
	return __xuiListViewInvalidateRows(pWidget, pData);
}

XUI_API int xuiListViewGetItemCount(xui_widget pWidget)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	return (pData != NULL) ? pData->iItemCount : 0;
}

XUI_API const char* xuiListViewGetItemText(xui_widget pWidget, int iIndex)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return NULL;
	return __xuiListViewText(pData->arrItems[iIndex]);
}

XUI_API int xuiListViewIsItemEnabled(xui_widget pWidget, int iIndex)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	return __xuiListViewItemEnabledData(pData, iIndex);
}

XUI_API int xuiListViewSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pFont = pFont;
	return __xuiListViewInvalidateRows(pWidget, pData);
}

XUI_API xui_font xuiListViewGetFont(xui_widget pWidget)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API int xuiListViewSetItemHeight(xui_widget pWidget, float fHeight)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	int iRet;
	if ( (pData == NULL) || (fHeight <= 0.0f) || !__xuiListViewFloatValid(fHeight) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fItemHeight = fHeight;
	iRet = __xuiListViewUpdateContentSize(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiListViewGetItemHeight(xui_widget pWidget)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	return (pData != NULL) ? pData->fItemHeight : 0.0f;
}

XUI_API int xuiListViewSetMetrics(xui_widget pWidget, float fItemHeight, float fPadding, float fBorderWidth)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	if ( (pData == NULL) ||
	     ((fItemHeight != 0.0f) && !__xuiListViewFloatValid(fItemHeight)) ||
	     ((fPadding != 0.0f) && !__xuiListViewFloatValid(fPadding)) ||
	     ((fBorderWidth != 0.0f) && !__xuiListViewFloatValid(fBorderWidth)) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( fItemHeight > 0.0f ) pData->fItemHeight = fItemHeight;
	if ( fPadding > 0.0f ) pData->fPadding = fPadding;
	if ( fBorderWidth >= 0.0f ) pData->fBorderWidth = fBorderWidth;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiListViewGetMetrics(xui_widget pWidget, float* pItemHeight, float* pPadding, float* pBorderWidth)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pItemHeight != NULL ) *pItemHeight = pData->fItemHeight;
	if ( pPadding != NULL ) *pPadding = pData->fPadding;
	if ( pBorderWidth != NULL ) *pBorderWidth = pData->fBorderWidth;
	return XUI_OK;
}

XUI_API int xuiListViewSetSelected(xui_widget pWidget, int iIndex)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiListViewSetSelectedInternal(pWidget, pData, iIndex, 0, 0);
}

XUI_API int xuiListViewGetSelected(xui_widget pWidget)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	return (pData != NULL) ? pData->iSelected : -1;
}

XUI_API int xuiListViewSetSelectionMode(xui_widget pWidget, int iMode)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	int iSelected;
	if ( (pData == NULL) || !__xuiListViewSelectionModeValid(iMode) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iSelectionMode == iMode ) return XUI_OK;
	iSelected = (pData->iSelectionMode == XUI_SELECTION_SINGLE) ? pData->iSelected : __xuiListViewFirstSelected(pData);
	pData->iSelectionMode = iMode;
	if ( iMode == XUI_SELECTION_SINGLE ) {
		pData->iSelected = iSelected;
		__xuiListViewSyncSingleSelection(pData);
	} else if ( __xuiListViewItemEnabledData(pData, pData->iSelected) ) {
		__xuiListViewSetSelectionFlag(pData, pData->iSelected, 1);
	}
	pData->iChangeCount++;
	return __xuiListViewInvalidateRows(pWidget, pData);
}

XUI_API int xuiListViewGetSelectionMode(xui_widget pWidget)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	return (pData != NULL) ? pData->iSelectionMode : XUI_SELECTION_SINGLE;
}

XUI_API int xuiListViewSetSelectionBuffer(xui_widget pWidget, int* arrSelected, int iCount)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	int i;
	int* arrOld;
	int iOldCount;
	if ( (pData == NULL) || (iCount < 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	arrOld = __xuiListViewSelectionArray(pData, &iOldCount);
	pData->arrExternalSelected = arrSelected;
	pData->iExternalSelectedCount = (arrSelected != NULL) ? iCount : 0;
	if ( arrSelected != NULL ) {
		for ( i = 0; i < iCount; i++ ) {
			arrSelected[i] = (arrOld != NULL && i < iOldCount && arrOld[i]) ? 1 : 0;
		}
	}
	if ( pData->iSelectionMode == XUI_SELECTION_SINGLE ) {
		__xuiListViewSyncSingleSelection(pData);
	}
	return __xuiListViewInvalidateRows(pWidget, pData);
}

XUI_API int xuiListViewClearSelection(xui_widget pWidget)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiListViewClearSelectionArray(pData);
	pData->iSelected = -1;
	pData->iFocus = -1;
	pData->iAnchor = -1;
	pData->iChangeCount++;
	return __xuiListViewInvalidateRows(pWidget, pData);
}

XUI_API int xuiListViewSetItemSelected(xui_widget pWidget, int iIndex, int bSelected)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !__xuiListViewItemEnabledData(pData, iIndex) ) return XUI_OK;
	if ( pData->iSelectionMode == XUI_SELECTION_SINGLE ) {
		return __xuiListViewSetSelectedInternal(pWidget, pData, bSelected ? iIndex : ((pData->iSelected == iIndex) ? -1 : pData->iSelected), 0, 0);
	}
	__xuiListViewSetSelectionFlag(pData, iIndex, bSelected);
	pData->iSelected = bSelected ? iIndex : __xuiListViewFirstSelected(pData);
	pData->iFocus = pData->iSelected;
	pData->iAnchor = pData->iSelected;
	pData->iChangeCount++;
	return __xuiListViewInvalidateRows(pWidget, pData);
}

XUI_API int xuiListViewIsItemSelected(xui_widget pWidget, int iIndex)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	return __xuiListViewItemSelectedData(pData, iIndex);
}

XUI_API int xuiListViewSetNotifyRepeatSelect(xui_widget pWidget, int bNotify)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bNotifyRepeatSelect = bNotify ? 1 : 0;
	return XUI_OK;
}

XUI_API int xuiListViewGetNotifyRepeatSelect(xui_widget pWidget)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	return (pData != NULL) ? pData->bNotifyRepeatSelect : 0;
}

XUI_API int xuiListViewSetScroll(xui_widget pWidget, float fOffsetY)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	if ( (pData == NULL) || (pData->pFrame == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameSetOffset(pData->pFrame, 0.0f, fOffsetY);
}

XUI_API float xuiListViewGetScroll(xui_widget pWidget)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	float fOffsetY;
	fOffsetY = 0.0f;
	if ( (pData != NULL) && (pData->pFrame != NULL) ) {
		(void)xuiScrollFrameGetOffset(pData->pFrame, NULL, &fOffsetY);
	}
	return fOffsetY;
}

XUI_API int xuiListViewEnsureVisible(xui_widget pWidget, int iIndex)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	xui_rect_t tRect;
	if ( (pData == NULL) || (pData->pFrame == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( (iIndex < 0) || (iIndex >= pData->iItemCount) ) return XUI_OK;
	tRect.fX = 0.0f;
	tRect.fY = (float)iIndex * pData->fItemHeight;
	tRect.fW = 1.0f;
	tRect.fH = pData->fItemHeight;
	return xuiScrollFrameEnsureRectVisible(pData->pFrame, tRect);
}

XUI_API int xuiListViewSetScrollbarMode(xui_widget pWidget, int iMode)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	if ( (pData == NULL) || (pData->pFrame == NULL) || !__xuiListViewScrollbarModeValid(iMode) ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameSetScrollbarMode(pData->pFrame, iMode);
}

XUI_API int xuiListViewGetScrollbarMode(xui_widget pWidget)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	return (pData != NULL && pData->pFrame != NULL) ? xuiScrollFrameGetScrollbarMode(pData->pFrame) : XUI_SCROLLBAR_MODE_COMPACT;
}

XUI_API int xuiListViewSetColors(xui_widget pWidget, uint32_t iBackground, uint32_t iBorder, uint32_t iFocus, uint32_t iRow, uint32_t iHover, uint32_t iSelected, uint32_t iText, uint32_t iDisabledText)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iBackgroundColor = iBackground;
	pData->iBorderColor = iBorder;
	pData->iFocusColor = iFocus;
	pData->iRowColor = iRow;
	pData->iHoverColor = iHover;
	pData->iSelectedColor = iSelected;
	pData->iTextColor = iText;
	pData->iDisabledTextColor = iDisabledText;
	return __xuiListViewInvalidateRows(pWidget, pData);
}

XUI_API int xuiListViewGetColors(xui_widget pWidget, uint32_t* pBackground, uint32_t* pBorder, uint32_t* pFocus, uint32_t* pRow, uint32_t* pHover, uint32_t* pSelected, uint32_t* pText, uint32_t* pDisabledText)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
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

XUI_API int xuiListViewSetScrollbarColors(xui_widget pWidget, uint32_t iTrack, uint32_t iThumb, uint32_t iHover, uint32_t iActive, uint32_t iFocus, uint32_t iDisabled)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
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
	return __xuiListViewInvalidateRows(pWidget, pData);
}

XUI_API int xuiListViewGetScrollbarColors(xui_widget pWidget, uint32_t* pTrack, uint32_t* pThumb, uint32_t* pHover, uint32_t* pActive, uint32_t* pFocus, uint32_t* pDisabled)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pTrack != NULL ) *pTrack = pData->iTrackColor;
	if ( pThumb != NULL ) *pThumb = pData->iThumbColor;
	if ( pHover != NULL ) *pHover = pData->iScrollbarHoverColor;
	if ( pActive != NULL ) *pActive = pData->iScrollbarActiveColor;
	if ( pFocus != NULL ) *pFocus = pData->iScrollbarFocusColor;
	if ( pDisabled != NULL ) *pDisabled = pData->iScrollbarDisabledColor;
	return XUI_OK;
}

XUI_API xui_widget xuiListViewGetFrameWidget(xui_widget pWidget)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	return (pData != NULL) ? pData->pFrame : NULL;
}

XUI_API xui_widget xuiListViewGetViewportWidget(xui_widget pWidget)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	return (pData != NULL) ? pData->pViewport : NULL;
}

XUI_API xui_scroll_model_t* xuiListViewGetModel(xui_widget pWidget)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	return (pData != NULL && pData->pFrame != NULL) ? xuiScrollFrameGetModel(pData->pFrame) : NULL;
}

XUI_API xui_rect_t xuiListViewGetViewportRect(xui_widget pWidget)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	return __xuiListViewViewportRectLocal(pWidget, pData);
}

XUI_API xui_rect_t xuiListViewGetItemRect(xui_widget pWidget, int iIndex)
{
	xui_list_view_data_t* pData;
	xui_rect_t tViewport;
	float fOffsetY;

	pData = __xuiListViewGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) {
		return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	}
	tViewport = xuiListViewGetViewportRect(pWidget);
	fOffsetY = 0.0f;
	(void)xuiScrollFrameGetOffset(pData->pFrame, NULL, &fOffsetY);
	return (xui_rect_t){tViewport.fX, tViewport.fY + (float)iIndex * pData->fItemHeight - fOffsetY, tViewport.fW, pData->fItemHeight};
}

XUI_API int xuiListViewGetItemAt(xui_widget pWidget, float fX, float fY)
{
	xui_list_view_data_t* pData;
	xui_rect_t tViewport;
	float fOffsetY;
	float fLocalY;
	int iIndex;

	pData = __xuiListViewGetData(pWidget);
	if ( (pData == NULL) || (pData->fItemHeight <= 0.0f) ) return -1;
	tViewport = xuiListViewGetViewportRect(pWidget);
	if ( !__xuiListViewPointInRect(tViewport, fX, fY) ) return -1;
	fOffsetY = 0.0f;
	(void)xuiScrollFrameGetOffset(pData->pFrame, NULL, &fOffsetY);
	fLocalY = fY - tViewport.fY + fOffsetY;
	iIndex = (int)(fLocalY / pData->fItemHeight);
	return (iIndex >= 0 && iIndex < pData->iItemCount) ? iIndex : -1;
}

XUI_API int xuiListViewGetHoverIndex(xui_widget pWidget)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	return (pData != NULL) ? pData->iHover : -1;
}

XUI_API int xuiListViewGetFocusIndex(xui_widget pWidget)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	return (pData != NULL) ? pData->iFocus : -1;
}

XUI_API int xuiListViewGetSelectCount(xui_widget pWidget)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	return (pData != NULL) ? pData->iSelectCount : 0;
}

XUI_API int xuiListViewGetChangeCount(xui_widget pWidget)
{
	xui_list_view_data_t* pData = __xuiListViewGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}
