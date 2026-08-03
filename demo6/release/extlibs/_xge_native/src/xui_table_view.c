#include "xui_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define XUI_TABLE_VIEW_DEFAULT_COLUMN_WIDTH 80.0f
#define XUI_TABLE_VIEW_DEFAULT_ROW_HEIGHT 20.0f
#define XUI_TABLE_VIEW_DEFAULT_HEADER_HEIGHT 24.0f
#define XUI_TABLE_VIEW_DEFAULT_WIDTH 420.0f
#define XUI_TABLE_VIEW_DEFAULT_ROWS 8

typedef struct xui_table_view_data_t {
	xui_widget pFrame;
	xui_widget pViewport;
	xui_table_view_column_t arrColumns[XUI_TABLE_VIEW_COLUMN_CAPACITY];
	xui_table_view_row_t* arrRows;
	int iColumnCount;
	int iRowCount;
	int iRowStateCount;
	xui_font pFont;
	xui_table_view_count_proc onCount;
	xui_table_view_cell_proc onCell;
	void* pAdapterUser;
	xui_table_view_sort_proc onSort;
	void* pSortUser;
	xui_table_view_select_proc onSelect;
	void* pSelectUser;
	xui_table_view_column_resize_proc onColumnResize;
	void* pColumnResizeUser;
	xui_table_view_hover_proc onHover;
	void* pHoverUser;
	xui_table_view_merge_proc onMerge;
	void* pMergeUser;
	xui_table_view_header_renderer_proc onHeaderRender;
	void* pHeaderRenderUser;
	xui_table_view_cell_renderer_proc onCellRender;
	void* pCellRenderUser;
	int iSelectionMode;
	int iSelectedRow;
	int iSelectedColumn;
	int iHoverRow;
	int iHoverColumn;
	int iFocusRow;
	int iFocusColumn;
	int iActiveRow;
	int iActiveColumn;
	int iFirstVisible;
	int iPaintVisibleCount;
	int iSortColumn;
	int bSortDescending;
	int iResizeColumn;
	float fDragX;
	float fDragWidth;
	float fDefaultColumnWidth;
	float fDefaultRowHeight;
	float fHeaderHeight;
	float fBorderWidth;
	int iChangeCount;
	int iSelectCount;
	int iSortCount;
	int iHoverCount;
	int iColumnResizeCount;
	uint32_t iBackgroundColor;
	uint32_t iHeaderColor;
	uint32_t iHeaderTextColor;
	uint32_t iRowColor;
	uint32_t iAltRowColor;
	uint32_t iHoverColor;
	uint32_t iSelectedColor;
	uint32_t iDisabledColor;
	uint32_t iGridColor;
	uint32_t iTextColor;
	uint32_t iDisabledTextColor;
	uint32_t iFocusRingColor;
	uint32_t iBarColor;
	uint32_t iThumbColor;
	uint32_t iScrollbarHoverColor;
	uint32_t iScrollbarActiveColor;
	uint32_t iScrollbarFocusColor;
	uint32_t iScrollbarDisabledColor;
} xui_table_view_data_t;

static xui_table_view_data_t* __xuiTableViewGetData(xui_widget pWidget);
static float __xuiTableViewRowHeight(const xui_table_view_data_t* pData, int iRow);
static float __xuiTableViewRowTop(const xui_table_view_data_t* pData, int iRow);
static void __xuiTableViewGetCell(xui_widget pWidget, xui_table_view_data_t* pData, int iRow, int iColumn, xui_table_view_cell_t* pCell);
static xui_rect_t __xuiTableViewCellContentRectData(xui_table_view_data_t* pData, int iRow, int iColumn, int iRowSpan, int iColSpan);

static int __xuiTableViewFloatValid(float fValue)
{
	return (fValue == fValue) && (fValue >= 0.0f) && (fValue <= XUI_LAYOUT_UNBOUNDED);
}

static int __xuiTableViewAlpha(uint32_t iColor)
{
	return (int)(iColor & 0xffu);
}

static uint32_t __xuiTableViewColorWithAlpha(uint32_t iColor, int iAlpha)
{
	if ( iAlpha < 0 ) iAlpha = 0;
	if ( iAlpha > 255 ) iAlpha = 255;
	return (iColor & 0xffffff00u) | (uint32_t)iAlpha;
}

static float __xuiTableViewMaxFloat(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static float __xuiTableViewMinFloat(float fA, float fB)
{
	return (fA < fB) ? fA : fB;
}

static const char* __xuiTableViewText(const char* sText)
{
	return (sText != NULL) ? sText : "";
}

static int __xuiTableViewScrollbarModeValid(int iMode)
{
	return (iMode == XUI_SCROLLBAR_MODE_FULL) || (iMode == XUI_SCROLLBAR_MODE_COMPACT);
}

static int __xuiTableViewSelectionModeValid(int iMode)
{
	return (iMode == XUI_TABLE_VIEW_SELECTION_CELL) || (iMode == XUI_TABLE_VIEW_SELECTION_ROW);
}

static int __xuiTableViewDescValid(const xui_table_view_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) {
		return 0;
	}
	if ( (pDesc->iColumnCount < 0) || (pDesc->iRowCount < 0) ||
	     (pDesc->iColumnCount > XUI_TABLE_VIEW_COLUMN_CAPACITY) ) {
		return 0;
	}
	if ( ((pDesc->fDefaultColumnWidth != 0.0f) && !__xuiTableViewFloatValid(pDesc->fDefaultColumnWidth)) ||
	     ((pDesc->fDefaultRowHeight != 0.0f) && !__xuiTableViewFloatValid(pDesc->fDefaultRowHeight)) ||
	     ((pDesc->fHeaderHeight != 0.0f) && !__xuiTableViewFloatValid(pDesc->fHeaderHeight)) ) {
		return 0;
	}
	return 1;
}

static void __xuiTableViewDefaults(xui_table_view_data_t* pData)
{
	memset(pData, 0, sizeof(*pData));
	pData->iSelectionMode = XUI_TABLE_VIEW_SELECTION_CELL;
	pData->iSelectedRow = -1;
	pData->iSelectedColumn = -1;
	pData->iHoverRow = -1;
	pData->iHoverColumn = -1;
	pData->iFocusRow = -1;
	pData->iFocusColumn = -1;
	pData->iActiveRow = -1;
	pData->iActiveColumn = -1;
	pData->iSortColumn = -1;
	pData->iResizeColumn = -1;
	pData->fDefaultColumnWidth = XUI_TABLE_VIEW_DEFAULT_COLUMN_WIDTH;
	pData->fDefaultRowHeight = XUI_TABLE_VIEW_DEFAULT_ROW_HEIGHT;
	pData->fHeaderHeight = XUI_TABLE_VIEW_DEFAULT_HEADER_HEIGHT;
	pData->fBorderWidth = 1.0f;
	pData->iBackgroundColor = XUI_COLOR_RGBA(248, 252, 255, 255);
	pData->iHeaderColor = XUI_COLOR_RGBA(232, 241, 250, 255);
	pData->iHeaderTextColor = XUI_COLOR_RGBA(48, 68, 91, 255);
	pData->iRowColor = XUI_COLOR_RGBA(255, 255, 255, 255);
	pData->iAltRowColor = XUI_COLOR_RGBA(249, 252, 255, 255);
	pData->iHoverColor = XUI_COLOR_RGBA(231, 243, 253, 255);
	pData->iSelectedColor = XUI_COLOR_RGBA(47, 128, 237, 255);
	pData->iDisabledColor = XUI_COLOR_RGBA(238, 244, 250, 255);
	pData->iGridColor = XUI_COLOR_RGBA(206, 221, 235, 255);
	pData->iTextColor = XUI_COLOR_RGBA(31, 50, 73, 255);
	pData->iDisabledTextColor = XUI_COLOR_RGBA(132, 146, 162, 210);
	pData->iFocusRingColor = XUI_COLOR_RGBA(47, 128, 237, 255);
	pData->iBarColor = XUI_COLOR_RGBA(224, 234, 244, 255);
	pData->iThumbColor = XUI_COLOR_RGBA(126, 161, 196, 245);
	pData->iScrollbarHoverColor = XUI_COLOR_RGBA(76, 136, 204, 250);
	pData->iScrollbarActiveColor = XUI_COLOR_RGBA(47, 128, 237, 255);
	pData->iScrollbarFocusColor = XUI_COLOR_RGBA(47, 128, 237, 180);
	pData->iScrollbarDisabledColor = XUI_COLOR_RGBA(181, 190, 204, 135);
}

static void __xuiTableViewApplyDesc(xui_table_view_data_t* pData, const xui_table_view_desc_t* pDesc)
{
	if ( (pData == NULL) || (pDesc == NULL) ) {
		return;
	}
	pData->pFont = pDesc->pFont;
	pData->onCount = pDesc->onCount;
	pData->onCell = pDesc->onCell;
	pData->pAdapterUser = pDesc->pAdapterUser;
	if ( pDesc->fDefaultColumnWidth > 0.0f ) pData->fDefaultColumnWidth = pDesc->fDefaultColumnWidth;
	if ( pDesc->fDefaultRowHeight > 0.0f ) pData->fDefaultRowHeight = pDesc->fDefaultRowHeight;
	if ( pDesc->fHeaderHeight > 0.0f ) pData->fHeaderHeight = pDesc->fHeaderHeight;
	if ( __xuiTableViewSelectionModeValid(pDesc->iSelectionMode) ) pData->iSelectionMode = pDesc->iSelectionMode;
	if ( __xuiTableViewAlpha(pDesc->tColors.iBackgroundColor) != 0 ) pData->iBackgroundColor = pDesc->tColors.iBackgroundColor;
	if ( __xuiTableViewAlpha(pDesc->tColors.iHeaderColor) != 0 ) pData->iHeaderColor = pDesc->tColors.iHeaderColor;
	if ( __xuiTableViewAlpha(pDesc->tColors.iHeaderTextColor) != 0 ) pData->iHeaderTextColor = pDesc->tColors.iHeaderTextColor;
	if ( __xuiTableViewAlpha(pDesc->tColors.iRowColor) != 0 ) pData->iRowColor = pDesc->tColors.iRowColor;
	if ( __xuiTableViewAlpha(pDesc->tColors.iAltRowColor) != 0 ) pData->iAltRowColor = pDesc->tColors.iAltRowColor;
	if ( __xuiTableViewAlpha(pDesc->tColors.iHoverColor) != 0 ) pData->iHoverColor = pDesc->tColors.iHoverColor;
	if ( __xuiTableViewAlpha(pDesc->tColors.iSelectedColor) != 0 ) pData->iSelectedColor = pDesc->tColors.iSelectedColor;
	if ( __xuiTableViewAlpha(pDesc->tColors.iDisabledColor) != 0 ) pData->iDisabledColor = pDesc->tColors.iDisabledColor;
	if ( __xuiTableViewAlpha(pDesc->tColors.iGridColor) != 0 ) pData->iGridColor = pDesc->tColors.iGridColor;
	if ( __xuiTableViewAlpha(pDesc->tColors.iTextColor) != 0 ) pData->iTextColor = pDesc->tColors.iTextColor;
	if ( __xuiTableViewAlpha(pDesc->tColors.iDisabledTextColor) != 0 ) pData->iDisabledTextColor = pDesc->tColors.iDisabledTextColor;
	if ( __xuiTableViewAlpha(pDesc->tColors.iFocusRingColor) != 0 ) pData->iFocusRingColor = pDesc->tColors.iFocusRingColor;
	if ( __xuiTableViewAlpha(pDesc->tColors.iBarColor) != 0 ) pData->iBarColor = pDesc->tColors.iBarColor;
	if ( __xuiTableViewAlpha(pDesc->tColors.iThumbColor) != 0 ) pData->iThumbColor = pDesc->tColors.iThumbColor;
	if ( __xuiTableViewAlpha(pDesc->iTrackColor) != 0 ) pData->iBarColor = pDesc->iTrackColor;
	if ( __xuiTableViewAlpha(pDesc->iThumbColor) != 0 ) pData->iThumbColor = pDesc->iThumbColor;
	if ( __xuiTableViewAlpha(pDesc->iScrollbarHoverColor) != 0 ) pData->iScrollbarHoverColor = pDesc->iScrollbarHoverColor;
	if ( __xuiTableViewAlpha(pDesc->iScrollbarActiveColor) != 0 ) pData->iScrollbarActiveColor = pDesc->iScrollbarActiveColor;
	if ( __xuiTableViewAlpha(pDesc->iScrollbarFocusColor) != 0 ) pData->iScrollbarFocusColor = pDesc->iScrollbarFocusColor;
	if ( __xuiTableViewAlpha(pDesc->iScrollbarDisabledColor) != 0 ) pData->iScrollbarDisabledColor = pDesc->iScrollbarDisabledColor;
}

static int __xuiTableViewStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
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

static int __xuiTableViewStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
{
	xui_style_property_t tProperty;
	int iRet;

	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_FLOAT) ) {
		*pValue = tProperty.tValue.fFloat;
		return 1;
	}
	return 0;
}

static xui_font __xuiTableViewStyleFont(xui_widget pWidget, xui_font pBaseFont)
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

static void __xuiTableViewResolve(xui_widget pWidget, const xui_table_view_data_t* pData, xui_table_view_data_t* pOut)
{
	if ( (pWidget == NULL) || (pData == NULL) || (pOut == NULL) ) {
		return;
	}
	*pOut = *pData;
	pOut->pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	pOut->pFont = __xuiTableViewStyleFont(pWidget, pOut->pFont);
	(void)__xuiTableViewStyleColor(pWidget, "tableview.background.color", &pOut->iBackgroundColor);
	(void)__xuiTableViewStyleColor(pWidget, "tableview.header.color", &pOut->iHeaderColor);
	(void)__xuiTableViewStyleColor(pWidget, "tableview.header.text_color", &pOut->iHeaderTextColor);
	(void)__xuiTableViewStyleColor(pWidget, "tableview.row.color", &pOut->iRowColor);
	(void)__xuiTableViewStyleColor(pWidget, "tableview.row.alt_color", &pOut->iAltRowColor);
	(void)__xuiTableViewStyleColor(pWidget, "tableview.row.hover_color", &pOut->iHoverColor);
	(void)__xuiTableViewStyleColor(pWidget, "tableview.row.selected_color", &pOut->iSelectedColor);
	(void)__xuiTableViewStyleColor(pWidget, "tableview.row.disabled_color", &pOut->iDisabledColor);
	(void)__xuiTableViewStyleColor(pWidget, "tableview.grid.color", &pOut->iGridColor);
	(void)__xuiTableViewStyleColor(pWidget, "tableview.text.color", &pOut->iTextColor);
	(void)__xuiTableViewStyleColor(pWidget, "tableview.text.disabled_color", &pOut->iDisabledTextColor);
	(void)__xuiTableViewStyleColor(pWidget, "tableview.focus.color", &pOut->iFocusRingColor);
	(void)__xuiTableViewStyleFloat(pWidget, "tableview.default.column_width", &pOut->fDefaultColumnWidth);
	(void)__xuiTableViewStyleFloat(pWidget, "tableview.default.row_height", &pOut->fDefaultRowHeight);
	(void)__xuiTableViewStyleFloat(pWidget, "tableview.header.height", &pOut->fHeaderHeight);
	(void)__xuiTableViewStyleFloat(pWidget, "tableview.border.width", &pOut->fBorderWidth);
	if ( pOut->fDefaultColumnWidth <= 0.0f ) pOut->fDefaultColumnWidth = XUI_TABLE_VIEW_DEFAULT_COLUMN_WIDTH;
	if ( pOut->fDefaultRowHeight <= 0.0f ) pOut->fDefaultRowHeight = XUI_TABLE_VIEW_DEFAULT_ROW_HEIGHT;
	if ( pOut->fHeaderHeight < 0.0f ) pOut->fHeaderHeight = 0.0f;
	if ( pOut->fBorderWidth < 0.0f ) pOut->fBorderWidth = 0.0f;
}

static int __xuiTableViewInvalidate(xui_widget pWidget, xui_table_view_data_t* pData, uint32_t iFlags)
{
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = xuiWidgetInvalidate(pWidget, iFlags);
	if ( (iRet == XUI_OK) && (pData->pViewport != NULL) ) {
		iRet = xuiWidgetInvalidate(pData->pViewport, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return iRet;
}

static int __xuiTableViewInvalidateViewportRect(xui_table_view_data_t* pData, xui_rect_t tRect)
{
	xui_rect_t tViewport;
	float fRight;
	float fBottom;

	if ( (pData == NULL) || (pData->pViewport == NULL) ) {
		return XUI_OK;
	}
	tViewport = xuiWidgetGetRect(pData->pViewport);
	if ( (tViewport.fW <= 0.0f) || (tViewport.fH <= 0.0f) || (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XUI_OK;
	}
	fRight = __xuiTableViewMinFloat(tViewport.fW, tRect.fX + tRect.fW);
	fBottom = __xuiTableViewMinFloat(tViewport.fH, tRect.fY + tRect.fH);
	tRect.fX = __xuiTableViewMaxFloat(0.0f, tRect.fX);
	tRect.fY = __xuiTableViewMaxFloat(0.0f, tRect.fY);
	tRect.fW = fRight - tRect.fX;
	tRect.fH = fBottom - tRect.fY;
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XUI_OK;
	}
	return xuiWidgetInvalidateRect(pData->pViewport, tRect, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiTableViewInvalidateHoverCell(xui_widget pWidget, xui_table_view_data_t* pData, int iRow, int iColumn)
{
	xui_table_view_cell_t tCell;
	xui_rect_t tViewport;
	xui_rect_t tContent;
	xui_rect_t tLocal;
	float fOffsetX;
	float fOffsetY;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pViewport == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (iRow < 0) || (iRow >= pData->iRowCount) ) {
		return XUI_OK;
	}
	tViewport = xuiWidgetGetRect(pData->pViewport);
	fOffsetX = 0.0f;
	fOffsetY = 0.0f;
	(void)xuiScrollFrameGetOffset(pData->pFrame, &fOffsetX, &fOffsetY);
	if ( pData->iSelectionMode == XUI_TABLE_VIEW_SELECTION_ROW ) {
		tContent.fX = 0.0f;
		tContent.fY = __xuiTableViewRowTop(pData, iRow) - fOffsetY;
		tContent.fW = tViewport.fW;
		tContent.fH = __xuiTableViewRowHeight(pData, iRow);
		return __xuiTableViewInvalidateViewportRect(pData, tContent);
	}
	if ( (iColumn < 0) || (iColumn >= pData->iColumnCount) ) {
		return XUI_OK;
	}
	__xuiTableViewGetCell(pWidget, pData, iRow, iColumn, &tCell);
	tContent = __xuiTableViewCellContentRectData(pData, iRow, iColumn, tCell.iRowSpan, tCell.iColSpan);
	tLocal.fX = tContent.fX - fOffsetX;
	tLocal.fY = tContent.fY - fOffsetY;
	tLocal.fW = tContent.fW;
	tLocal.fH = tContent.fH;
	return __xuiTableViewInvalidateViewportRect(pData, tLocal);
}

static int __xuiTableViewInvalidateHoverCells(xui_widget pWidget, xui_table_view_data_t* pData, int iOldRow, int iOldColumn, int iNewRow, int iNewColumn)
{
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->pViewport == NULL ) {
		return __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	iRet = __xuiTableViewInvalidateHoverCell(pWidget, pData, iOldRow, iOldColumn);
	if ( iRet == XUI_OK ) {
		iRet = __xuiTableViewInvalidateHoverCell(pWidget, pData, iNewRow, iNewColumn);
	}
	return iRet;
}

static int __xuiTableViewSyncRowCount(xui_widget pWidget, xui_table_view_data_t* pData)
{
	int iCount;

	if ( pData == NULL ) {
		return 0;
	}
	if ( pData->onCount != NULL ) {
		iCount = pData->onCount(pWidget, pData->pAdapterUser);
		if ( iCount < 0 ) iCount = 0;
		pData->iRowCount = iCount;
	} else {
		pData->iRowCount = pData->iRowStateCount;
	}
	if ( pData->iSelectedRow >= pData->iRowCount ) {
		pData->iSelectedRow = -1;
		pData->iSelectedColumn = -1;
	}
	if ( pData->iHoverRow >= pData->iRowCount ) {
		pData->iHoverRow = -1;
		pData->iHoverColumn = -1;
	}
	if ( pData->iFocusRow >= pData->iRowCount ) {
		pData->iFocusRow = -1;
		pData->iFocusColumn = -1;
	}
	return pData->iRowCount;
}

static float __xuiTableViewColumnWidth(const xui_table_view_data_t* pData, int iColumn)
{
	float fWidth;

	if ( (pData == NULL) || (iColumn < 0) || (iColumn >= pData->iColumnCount) ) {
		return 0.0f;
	}
	if ( pData->arrColumns[iColumn].bVisible == 0 ) {
		return 0.0f;
	}
	fWidth = pData->arrColumns[iColumn].fWidth;
	if ( fWidth <= 0.0f ) fWidth = pData->fDefaultColumnWidth;
	if ( fWidth <= 0.0f ) fWidth = XUI_TABLE_VIEW_DEFAULT_COLUMN_WIDTH;
	return fWidth;
}

static float __xuiTableViewRowHeight(const xui_table_view_data_t* pData, int iRow)
{
	float fHeight;

	if ( pData == NULL ) {
		return 0.0f;
	}
	fHeight = 0.0f;
	if ( (pData->arrRows != NULL) && (iRow >= 0) && (iRow < pData->iRowStateCount) ) {
		fHeight = pData->arrRows[iRow].fHeight;
	}
	if ( fHeight <= 0.0f ) fHeight = pData->fDefaultRowHeight;
	if ( fHeight <= 0.0f ) fHeight = XUI_TABLE_VIEW_DEFAULT_ROW_HEIGHT;
	return fHeight;
}

static int __xuiTableViewRowDisabled(const xui_table_view_data_t* pData, int iRow)
{
	if ( (pData != NULL) && (pData->arrRows != NULL) && (iRow >= 0) && (iRow < pData->iRowStateCount) ) {
		return pData->arrRows[iRow].bDisabled != 0;
	}
	return 0;
}

static int __xuiTableViewRowSelected(const xui_table_view_data_t* pData, int iRow)
{
	if ( pData == NULL ) {
		return 0;
	}
	if ( (pData->iSelectionMode == XUI_TABLE_VIEW_SELECTION_ROW) && (pData->iSelectedRow == iRow) ) {
		return 1;
	}
	if ( (pData->arrRows != NULL) && (iRow >= 0) && (iRow < pData->iRowStateCount) ) {
		return pData->arrRows[iRow].bSelected != 0;
	}
	return 0;
}

static float __xuiTableViewContentWidth(const xui_table_view_data_t* pData)
{
	float fW;
	int i;

	fW = 0.0f;
	if ( pData != NULL ) {
		for ( i = 0; i < pData->iColumnCount; i++ ) {
			fW += __xuiTableViewColumnWidth(pData, i);
		}
	}
	return fW;
}

static float __xuiTableViewRowTop(const xui_table_view_data_t* pData, int iRow)
{
	float fY;
	int i;

	fY = 0.0f;
	if ( pData == NULL ) {
		return 0.0f;
	}
	for ( i = 0; (i < iRow) && (i < pData->iRowCount); i++ ) {
		fY += __xuiTableViewRowHeight(pData, i);
	}
	return fY;
}

static float __xuiTableViewContentHeight(xui_widget pWidget, xui_table_view_data_t* pData)
{
	float fH;
	int i;
	int iCount;

	fH = 0.0f;
	iCount = __xuiTableViewSyncRowCount(pWidget, pData);
	for ( i = 0; i < iCount; i++ ) {
		fH += __xuiTableViewRowHeight(pData, i);
	}
	return fH;
}

static float __xuiTableViewColumnLeft(const xui_table_view_data_t* pData, int iColumn)
{
	float fX;
	int i;

	fX = 0.0f;
	if ( pData == NULL ) {
		return 0.0f;
	}
	for ( i = 0; (i < iColumn) && (i < pData->iColumnCount); i++ ) {
		fX += __xuiTableViewColumnWidth(pData, i);
	}
	return fX;
}

static int __xuiTableViewColumnAtContentX(const xui_table_view_data_t* pData, float fContentX)
{
	float fX;
	float fW;
	int i;

	if ( pData == NULL ) {
		return -1;
	}
	fX = 0.0f;
	for ( i = 0; i < pData->iColumnCount; i++ ) {
		fW = __xuiTableViewColumnWidth(pData, i);
		if ( fW <= 0.0f ) {
			continue;
		}
		if ( (fContentX >= fX) && (fContentX < fX + fW) ) {
			return i;
		}
		fX += fW;
	}
	return -1;
}

static int __xuiTableViewRowAtContentY(const xui_table_view_data_t* pData, float fContentY)
{
	float fY;
	float fH;
	int i;

	if ( pData == NULL ) {
		return -1;
	}
	fY = 0.0f;
	for ( i = 0; i < pData->iRowCount; i++ ) {
		fH = __xuiTableViewRowHeight(pData, i);
		if ( (fContentY >= fY) && (fContentY < fY + fH) ) {
			return i;
		}
		fY += fH;
	}
	return -1;
}

static int __xuiTableViewNextVisibleColumn(const xui_table_view_data_t* pData, int iColumn, int iStep)
{
	int i;

	if ( (pData == NULL) || (iStep == 0) ) {
		return -1;
	}
	i = iColumn;
	while ( (i >= 0) && (i < pData->iColumnCount) ) {
		if ( __xuiTableViewColumnWidth(pData, i) > 0.0f ) {
			return i;
		}
		i += iStep;
	}
	return -1;
}

static int __xuiTableViewFirstVisibleColumn(const xui_table_view_data_t* pData)
{
	return __xuiTableViewNextVisibleColumn(pData, 0, 1);
}

static int __xuiTableViewLastVisibleColumn(const xui_table_view_data_t* pData)
{
	return (pData != NULL) ? __xuiTableViewNextVisibleColumn(pData, pData->iColumnCount - 1, -1) : -1;
}

static void __xuiTableViewDefaultCell(const xui_table_view_data_t* pData, int iColumn, xui_table_view_cell_t* pCell)
{
	if ( pCell == NULL ) {
		return;
	}
	memset(pCell, 0, sizeof(*pCell));
	pCell->iRowSpan = 1;
	pCell->iColSpan = 1;
	pCell->iType = ((pData != NULL) && (iColumn >= 0) && (iColumn < pData->iColumnCount)) ? pData->arrColumns[iColumn].iType : XUI_TABLE_CELL_TYPE_TEXT;
}

static void __xuiTableViewGetCell(xui_widget pWidget, xui_table_view_data_t* pData, int iRow, int iColumn, xui_table_view_cell_t* pCell)
{
	int iRowSpan;
	int iColSpan;

	__xuiTableViewDefaultCell(pData, iColumn, pCell);
	if ( (pData == NULL) || (pCell == NULL) || (iRow < 0) || (iColumn < 0) ) {
		return;
	}
	if ( pData->onCell != NULL ) {
		(void)pData->onCell(pWidget, iRow, iColumn, pCell, pData->pAdapterUser);
	}
	if ( pData->onMerge != NULL ) {
		iRowSpan = pCell->iRowSpan;
		iColSpan = pCell->iColSpan;
		if ( pData->onMerge(pWidget, iRow, iColumn, &iRowSpan, &iColSpan, pData->pMergeUser) != 0 ) {
			pCell->iRowSpan = iRowSpan;
			pCell->iColSpan = iColSpan;
		}
	}
	if ( pCell->iRowSpan <= 0 ) pCell->iRowSpan = 1;
	if ( pCell->iColSpan <= 0 ) pCell->iColSpan = 1;
	if ( iRow + pCell->iRowSpan > pData->iRowCount ) pCell->iRowSpan = pData->iRowCount - iRow;
	if ( iColumn + pCell->iColSpan > pData->iColumnCount ) pCell->iColSpan = pData->iColumnCount - iColumn;
	if ( pCell->iRowSpan <= 0 ) pCell->iRowSpan = 1;
	if ( pCell->iColSpan <= 0 ) pCell->iColSpan = 1;
}

static int __xuiTableViewMergeOwner(xui_widget pWidget, xui_table_view_data_t* pData, int iRow, int iColumn, int* pOwnerRow, int* pOwnerColumn)
{
	xui_table_view_cell_t tCell;
	int r;
	int c;

	if ( pOwnerRow != NULL ) *pOwnerRow = iRow;
	if ( pOwnerColumn != NULL ) *pOwnerColumn = iColumn;
	if ( (pData == NULL) || (iRow < 0) || (iColumn < 0) ) {
		return 0;
	}
	for ( r = 0; r <= iRow; r++ ) {
		for ( c = 0; c <= iColumn; c++ ) {
			__xuiTableViewGetCell(pWidget, pData, r, c, &tCell);
			if ( (tCell.iRowSpan <= 1) && (tCell.iColSpan <= 1) ) {
				continue;
			}
			if ( (iRow >= r) && (iRow < r + tCell.iRowSpan) && (iColumn >= c) && (iColumn < c + tCell.iColSpan) ) {
				if ( pOwnerRow != NULL ) *pOwnerRow = r;
				if ( pOwnerColumn != NULL ) *pOwnerColumn = c;
				return (r != iRow) || (c != iColumn);
			}
		}
	}
	return 0;
}

static float __xuiTableViewSpanWidth(const xui_table_view_data_t* pData, int iColumn, int iColSpan)
{
	float fW;
	int i;

	fW = 0.0f;
	if ( iColSpan <= 0 ) iColSpan = 1;
	for ( i = 0; (i < iColSpan) && (iColumn + i < pData->iColumnCount); i++ ) {
		fW += __xuiTableViewColumnWidth(pData, iColumn + i);
	}
	return fW;
}

static float __xuiTableViewSpanHeight(const xui_table_view_data_t* pData, int iRow, int iRowSpan)
{
	float fH;
	int i;

	fH = 0.0f;
	if ( iRowSpan <= 0 ) iRowSpan = 1;
	for ( i = 0; (i < iRowSpan) && (iRow + i < pData->iRowCount); i++ ) {
		fH += __xuiTableViewRowHeight(pData, iRow + i);
	}
	return fH;
}

static xui_rect_t __xuiTableViewCellContentRectData(xui_table_view_data_t* pData, int iRow, int iColumn, int iRowSpan, int iColSpan)
{
	xui_rect_t tRect;

	memset(&tRect, 0, sizeof(tRect));
	if ( pData == NULL ) {
		return tRect;
	}
	tRect.fX = __xuiTableViewColumnLeft(pData, iColumn);
	tRect.fY = __xuiTableViewRowTop(pData, iRow);
	tRect.fW = __xuiTableViewSpanWidth(pData, iColumn, iColSpan);
	tRect.fH = __xuiTableViewSpanHeight(pData, iRow, iRowSpan);
	return tRect;
}

static int __xuiTableViewRectIntersects(xui_rect_t tA, xui_rect_t tB)
{
	return (tA.fX < tB.fX + tB.fW) && (tA.fX + tA.fW > tB.fX) &&
	       (tA.fY < tB.fY + tB.fH) && (tA.fY + tA.fH > tB.fY);
}

static int __xuiTableViewPointInRect(xui_rect_t tRect, float fX, float fY)
{
	return (fX >= tRect.fX) && (fY >= tRect.fY) &&
	       (fX < tRect.fX + tRect.fW) && (fY < tRect.fY + tRect.fH);
}

static xui_rect_t __xuiTableViewFrameRect(xui_widget pWidget, const xui_table_view_data_t* pData)
{
	xui_rect_t tRect;
	xui_rect_t tFrame;
	float fBorder;

	tRect = xuiWidgetGetRect(pWidget);
	fBorder = (pData != NULL) ? pData->fBorderWidth : 1.0f;
	tFrame.fX = fBorder;
	tFrame.fY = fBorder + ((pData != NULL) ? pData->fHeaderHeight : XUI_TABLE_VIEW_DEFAULT_HEADER_HEIGHT);
	tFrame.fW = tRect.fW - fBorder * 2.0f;
	tFrame.fH = tRect.fH - fBorder * 2.0f - ((pData != NULL) ? pData->fHeaderHeight : XUI_TABLE_VIEW_DEFAULT_HEADER_HEIGHT);
	if ( tFrame.fW < 0.0f ) tFrame.fW = 0.0f;
	if ( tFrame.fH < 0.0f ) tFrame.fH = 0.0f;
	return tFrame;
}

static xui_rect_t __xuiTableViewViewportRectLocal(xui_widget pWidget, xui_table_view_data_t* pData)
{
	xui_rect_t tFrame;
	xui_rect_t tViewport;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pFrame == NULL) ) {
		return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	}
	tFrame = xuiWidgetGetRect(pData->pFrame);
	tViewport = xuiScrollFrameGetViewportRect(pData->pFrame);
	tViewport.fX += tFrame.fX;
	tViewport.fY += tFrame.fY;
	return tViewport;
}

static xui_rect_t __xuiTableViewHeaderRect(xui_widget pWidget, xui_table_view_data_t* pData)
{
	xui_rect_t tViewport;
	xui_rect_t tRect;
	float fBorder;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	}
	tViewport = __xuiTableViewViewportRectLocal(pWidget, pData);
	tRect = xuiWidgetGetRect(pWidget);
	fBorder = pData->fBorderWidth;
	if ( tViewport.fW <= 0.0f ) {
		tViewport.fX = fBorder;
		tViewport.fW = __xuiTableViewMaxFloat(0.0f, tRect.fW - fBorder * 2.0f);
	}
	tViewport.fY = fBorder;
	tViewport.fH = pData->fHeaderHeight;
	return tViewport;
}

static int __xuiTableViewUpdateContentSize(xui_widget pWidget, xui_table_view_data_t* pData)
{
	xui_rect_t tFrame;
	float fWidth;
	float fHeight;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pFrame == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	tFrame = __xuiTableViewFrameRect(pWidget, pData);
	fWidth = __xuiTableViewContentWidth(pData);
	fHeight = __xuiTableViewContentHeight(pWidget, pData);
	iRet = xuiScrollFrameSetContentSize(pData->pFrame, __xuiTableViewMaxFloat(0.0f, fWidth), __xuiTableViewMaxFloat(0.0f, fHeight));
	if ( iRet != XUI_OK ) return iRet;
	(void)tFrame;
	return xuiScrollFrameSetWheelStep(pData->pFrame, pData->fDefaultRowHeight * 3.0f);
}

static void __xuiTableViewMakeTextSummary(const char* sText, char* sBuffer, int iCapacity)
{
	int i;

	if ( (sBuffer == NULL) || (iCapacity <= 0) ) {
		return;
	}
	sBuffer[0] = 0;
	if ( sText == NULL ) {
		return;
	}
	for ( i = 0; (i < iCapacity - 1) && (sText[i] != 0) && (sText[i] != '\r') && (sText[i] != '\n'); i++ ) {
		sBuffer[i] = sText[i];
	}
	sBuffer[i] = 0;
}

static const char* __xuiTableViewCellText(xui_widget pWidget, xui_table_view_data_t* pData, int iRow, int iColumn, const xui_table_view_cell_t* pCell, char* sBuffer, int iCapacity)
{
	xui_table_view_format_proc onFormat;
	void* pUser;

	if ( (pCell == NULL) || (sBuffer == NULL) || (iCapacity <= 0) ) {
		return "";
	}
	sBuffer[0] = 0;
	if ( (pCell->sText != NULL) && (pCell->sText[0] != 0) ) {
		if ( pCell->iType == XUI_TABLE_CELL_TYPE_TEXTAREA ) {
			__xuiTableViewMakeTextSummary(pCell->sText, sBuffer, iCapacity);
			return sBuffer;
		}
		return pCell->sText;
	}
	onFormat = pCell->onFormat;
	pUser = pCell->pFormatUser;
	if ( (onFormat == NULL) && (pData != NULL) && (iColumn >= 0) && (iColumn < pData->iColumnCount) ) {
		onFormat = pData->arrColumns[iColumn].onFormat;
		pUser = pData->arrColumns[iColumn].pFormatUser;
	}
	if ( onFormat != NULL ) {
		if ( onFormat(pWidget, iRow, iColumn, pCell, sBuffer, iCapacity, pUser) != 0 ) {
			sBuffer[iCapacity - 1] = 0;
			return sBuffer;
		}
		sBuffer[0] = 0;
	}
	return "";
}

static int __xuiTableViewTextEqualsIgnoreCase(const char* sA, const char* sB)
{
	unsigned char a;
	unsigned char b;

	if ( (sA == NULL) || (sB == NULL) ) {
		return 0;
	}
	while ( (*sA != 0) && (*sB != 0) ) {
		a = (unsigned char)*sA++;
		b = (unsigned char)*sB++;
		if ( tolower(a) != tolower(b) ) {
			return 0;
		}
	}
	return (*sA == 0) && (*sB == 0);
}

static int __xuiTableViewBoolFromText(const char* sText)
{
	if ( sText == NULL ) {
		return 0;
	}
	return (strcmp(sText, "1") == 0) ||
	       __xuiTableViewTextEqualsIgnoreCase(sText, "true") ||
	       __xuiTableViewTextEqualsIgnoreCase(sText, "yes") ||
	       __xuiTableViewTextEqualsIgnoreCase(sText, "on") ||
	       __xuiTableViewTextEqualsIgnoreCase(sText, "enabled") ||
	       __xuiTableViewTextEqualsIgnoreCase(sText, "checked");
}

static int __xuiTableViewHexNibble(char ch)
{
	if ( (ch >= '0') && (ch <= '9') ) return ch - '0';
	if ( (ch >= 'a') && (ch <= 'f') ) return ch - 'a' + 10;
	if ( (ch >= 'A') && (ch <= 'F') ) return ch - 'A' + 10;
	return -1;
}

static int __xuiTableViewParseColor(const char* sText, uint32_t* pColor)
{
	const char* s;
	unsigned int arr[4];
	int iLen;
	int i;
	int n0;
	int n1;

	if ( (sText == NULL) || (pColor == NULL) ) {
		return 0;
	}
	s = (sText[0] == '#') ? (sText + 1) : sText;
	iLen = (int)strlen(s);
	if ( (iLen != 6) && (iLen != 8) ) {
		return 0;
	}
	for ( i = 0; i < iLen / 2; i++ ) {
		n0 = __xuiTableViewHexNibble(s[i * 2]);
		n1 = __xuiTableViewHexNibble(s[i * 2 + 1]);
		if ( (n0 < 0) || (n1 < 0) ) {
			return 0;
		}
		arr[i] = (unsigned int)((n0 << 4) | n1);
	}
	if ( iLen == 6 ) {
		arr[3] = 255u;
	}
	*pColor = XUI_COLOR_RGBA(arr[0], arr[1], arr[2], arr[3]);
	return 1;
}

static int __xuiTableViewDrawFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( __xuiTableViewAlpha(iColor) == 0 ) {
		return XUI_OK;
	}
	return pProxy->drawRectFill(pProxy, pDraw, xuiInternalSnapRect(tRect), iColor);
}

static int __xuiTableViewDrawStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	if ( (fWidth <= 0.0f) || (__xuiTableViewAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	return pProxy->drawRectStroke(pProxy, pDraw, xuiInternalSnapRect(tRect), fWidth, iColor);
}

static int __xuiTableViewDrawRectFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( __xuiTableViewAlpha(iColor) == 0 ) {
		return XUI_OK;
	}
	return __xuiTableViewDrawFill(pProxy, pDraw, tRect, iColor);
}

static int __xuiTableViewDrawRectStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	if ( (fWidth <= 0.0f) || (__xuiTableViewAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	return __xuiTableViewDrawStroke(pProxy, pDraw, tRect, fWidth, iColor);
}

static int __xuiTableViewDrawChecker(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect)
{
	xui_rect_t tCell;
	uint32_t iA;
	uint32_t iB;
	float fSize;
	float x;
	float y;
	int ix;
	int iy;

	if ( (pProxy == NULL) || (pProxy->drawRectFill == NULL) ) {
		return XUI_OK;
	}
	iA = XUI_COLOR_RGBA(236, 241, 247, 255);
	iB = XUI_COLOR_RGBA(255, 255, 255, 255);
	fSize = 4.0f;
	iy = 0;
	for ( y = tRect.fY; y < tRect.fY + tRect.fH; y += fSize, iy++ ) {
		ix = 0;
		for ( x = tRect.fX; x < tRect.fX + tRect.fW; x += fSize, ix++ ) {
			tCell = (xui_rect_t){x, y, __xuiTableViewMinFloat(fSize, tRect.fX + tRect.fW - x), __xuiTableViewMinFloat(fSize, tRect.fY + tRect.fH - y)};
			(void)pProxy->drawRectFill(pProxy, pDraw, xuiInternalSnapRect(tCell), (((ix + iy) & 1) != 0) ? iA : iB);
		}
	}
	return XUI_OK;
}

static int __xuiTableViewDrawCheck(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, int bChecked, int bDisabled, uint32_t iAccent, uint32_t iDisabledColor)
{
	uint32_t iBorder;
	uint32_t iFill;
	int iRet;

	iBorder = bDisabled ? iDisabledColor : iAccent;
	iFill = bChecked ? iAccent : XUI_COLOR_RGBA(255, 255, 255, 255);
	iRet = __xuiTableViewDrawRectFill(pProxy, pDraw, tRect, iFill);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTableViewDrawRectStroke(pProxy, pDraw, tRect, 1.0f, iBorder);
	if ( iRet != XUI_OK ) return iRet;
	if ( bChecked && (pProxy->drawLine != NULL) ) {
		iRet = pProxy->drawLine(pProxy, pDraw, tRect.fX + 3.0f, tRect.fY + 6.0f, tRect.fX + 5.2f, tRect.fY + 8.2f, 1.7f, XUI_COLOR_RGBA(255, 255, 255, 255));
		if ( iRet != XUI_OK ) return iRet;
		iRet = pProxy->drawLine(pProxy, pDraw, tRect.fX + 5.0f, tRect.fY + 8.1f, tRect.fX + 9.3f, tRect.fY + 3.8f, 1.7f, XUI_COLOR_RGBA(255, 255, 255, 255));
	}
	return iRet;
}

static int __xuiTableViewDrawSortMark(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, int bDescending, uint32_t iColor)
{
	xui_vec2_t tA;
	xui_vec2_t tB;
	xui_vec2_t tC;
	float fCX;
	float fCY;

	if ( (pProxy->drawTriangleFill == NULL) || (__xuiTableViewAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	fCX = tRect.fX + tRect.fW - 11.0f;
	fCY = tRect.fY + tRect.fH * 0.5f;
	if ( bDescending ) {
		tA = (xui_vec2_t){fCX - 4.0f, fCY - 2.5f};
		tB = (xui_vec2_t){fCX + 4.0f, fCY - 2.5f};
		tC = (xui_vec2_t){fCX, fCY + 3.5f};
	} else {
		tA = (xui_vec2_t){fCX - 4.0f, fCY + 2.5f};
		tB = (xui_vec2_t){fCX + 4.0f, fCY + 2.5f};
		tC = (xui_vec2_t){fCX, fCY - 3.5f};
	}
	return pProxy->drawTriangleFill(pProxy, pDraw, tA, tB, tC, iColor);
}

static int __xuiTableViewDrawCellContent(xui_widget pWidget, xui_table_view_data_t* pData, const xui_table_view_data_t* pResolved, xui_proxy pProxy, xui_draw_context pDraw, int iRow, int iColumn, const xui_table_view_cell_t* pCell, xui_rect_t tCell, int iState)
{
	xui_rect_t tText;
	xui_rect_t tBox;
	xui_rect_t tButton;
	const char* sText;
	char sBuffer[256];
	uint32_t iText;
	uint32_t iColor;
	uint32_t iGrid;
	int iFlags;
	int iRet;

	iText = ((iState & XUI_TABLE_CELL_DISABLED) != 0) ? pResolved->iDisabledTextColor : pResolved->iTextColor;
	if ( (iState & XUI_TABLE_CELL_SELECTED) != 0 ) {
		iText = XUI_COLOR_RGBA(255, 255, 255, 255);
	}
	if ( pCell->bHasStyle && (__xuiTableViewAlpha(pCell->iTextColor) != 0) ) {
		iText = pCell->iTextColor;
	}
	if ( (iColumn >= 0) && (iColumn < pData->iColumnCount) && pData->arrColumns[iColumn].bHasStyle && (__xuiTableViewAlpha(pData->arrColumns[iColumn].iTextColor) != 0) ) {
		iText = pData->arrColumns[iColumn].iTextColor;
	}
	iFlags = XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP;
	if ( (iColumn >= 0) && (iColumn < pData->iColumnCount) ) {
		if ( pData->arrColumns[iColumn].iAlign == XUI_TEXT_ALIGN_CENTER ) {
			iFlags = XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP;
		} else if ( pData->arrColumns[iColumn].iAlign == XUI_TEXT_ALIGN_RIGHT ) {
			iFlags = XUI_TEXT_ALIGN_RIGHT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP;
		}
	}
	sText = __xuiTableViewCellText(pWidget, pData, iRow, iColumn, pCell, sBuffer, (int)sizeof(sBuffer));
	tText = (xui_rect_t){tCell.fX + 8.0f, tCell.fY + 1.0f, __xuiTableViewMaxFloat(1.0f, tCell.fW - 16.0f), __xuiTableViewMaxFloat(1.0f, tCell.fH - 2.0f)};
	if ( pCell->iType == XUI_TABLE_CELL_TYPE_BOOL ) {
		tBox = xuiInternalSnapRect((xui_rect_t){tCell.fX + 8.0f, tCell.fY + (tCell.fH - 12.0f) * 0.5f, 12.0f, 12.0f});
		iRet = __xuiTableViewDrawCheck(pProxy, pDraw, tBox, __xuiTableViewBoolFromText(sText), (iState & XUI_TABLE_CELL_DISABLED) != 0, pResolved->iSelectedColor, pResolved->iDisabledTextColor);
		if ( iRet != XUI_OK ) return iRet;
		tText.fX = tBox.fX + tBox.fW + 6.0f;
		tText.fW = __xuiTableViewMaxFloat(1.0f, tCell.fX + tCell.fW - tText.fX - 7.0f);
		iFlags = XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP;
	} else if ( pCell->iType == XUI_TABLE_CELL_TYPE_COLOR ) {
		tBox = xuiInternalSnapRect((xui_rect_t){tCell.fX + 8.0f, tCell.fY + (tCell.fH - 14.0f) * 0.5f, 24.0f, 14.0f});
		iColor = XUI_COLOR_RGBA(120, 160, 200, 255);
		(void)__xuiTableViewParseColor(sText, &iColor);
		iGrid = pResolved->iGridColor;
		if ( __xuiTableViewAlpha(iColor) < 255 ) {
			iRet = __xuiTableViewDrawChecker(pProxy, pDraw, tBox);
			if ( iRet != XUI_OK ) return iRet;
		}
		iRet = __xuiTableViewDrawRectFill(pProxy, pDraw, tBox, iColor);
		if ( iRet != XUI_OK ) return iRet;
		iRet = __xuiTableViewDrawRectStroke(pProxy, pDraw, tBox, 1.0f, iGrid);
		if ( iRet != XUI_OK ) return iRet;
		tText.fX = tBox.fX + tBox.fW + 6.0f;
		tText.fW = __xuiTableViewMaxFloat(1.0f, tCell.fX + tCell.fW - tText.fX - 7.0f);
		iFlags = XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP;
	} else if ( (pCell->iType == XUI_TABLE_CELL_TYPE_PICKER) || (pCell->iType == XUI_TABLE_CELL_TYPE_FILE) || (pCell->iType == XUI_TABLE_CELL_TYPE_IMAGE) ) {
		tButton = xuiInternalSnapRect((xui_rect_t){tCell.fX + tCell.fW - 25.0f, tCell.fY + 2.0f, 21.0f, __xuiTableViewMaxFloat(1.0f, tCell.fH - 4.0f)});
		tText.fW = __xuiTableViewMaxFloat(1.0f, tButton.fX - tText.fX - 4.0f);
		iRet = __xuiTableViewDrawRectFill(pProxy, pDraw, tButton, __xuiTableViewColorWithAlpha(pResolved->iHeaderColor, 220));
		if ( iRet != XUI_OK ) return iRet;
		iRet = __xuiTableViewDrawRectStroke(pProxy, pDraw, tButton, 1.0f, __xuiTableViewColorWithAlpha(pResolved->iGridColor, 220));
		if ( iRet != XUI_OK ) return iRet;
		if ( (pResolved->pFont != NULL) && (__xuiTableViewAlpha(iText) != 0) ) {
			iRet = pProxy->drawText(pProxy, pDraw, pResolved->pFont, "...", tButton, iText, XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	if ( (pResolved->pFont != NULL) && (__xuiTableViewAlpha(iText) != 0) && (tText.fW > 0.0f) ) {
		iRet = pProxy->drawText(pProxy, pDraw, pResolved->pFont, __xuiTableViewText(sText), xuiInternalSnapRect(tText), iText, iFlags);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiTableViewCellDisabled(xui_widget pWidget, xui_table_view_data_t* pData, int iRow, int iColumn)
{
	xui_table_view_cell_t tCell;

	if ( __xuiTableViewRowDisabled(pData, iRow) ) {
		return 1;
	}
	__xuiTableViewGetCell(pWidget, pData, iRow, iColumn, &tCell);
	return tCell.bDisabled != 0;
}

static int __xuiTableViewFindEnabledRow(xui_widget pWidget, xui_table_view_data_t* pData, int iStart, int iStep)
{
	int i;

	(void)pWidget;
	if ( (pData == NULL) || (iStep == 0) ) {
		return -1;
	}
	i = iStart;
	while ( (i >= 0) && (i < pData->iRowCount) ) {
		if ( !__xuiTableViewRowDisabled(pData, i) ) {
			return i;
		}
		i += iStep;
	}
	return -1;
}

static int __xuiTableViewFindSelectableCell(xui_widget pWidget, xui_table_view_data_t* pData, int iRow, int iColumn, int iRowStep, int iColumnStep, int* pOutRow, int* pOutColumn)
{
	int r;
	int c;

	if ( pOutRow != NULL ) *pOutRow = -1;
	if ( pOutColumn != NULL ) *pOutColumn = -1;
	if ( (pData == NULL) || (pData->iRowCount <= 0) || (pData->iColumnCount <= 0) ) {
		return 0;
	}
	r = iRow;
	c = iColumn;
	while ( (r >= 0) && (r < pData->iRowCount) && (c >= 0) && (c < pData->iColumnCount) ) {
		if ( (__xuiTableViewColumnWidth(pData, c) > 0.0f) && !__xuiTableViewCellDisabled(pWidget, pData, r, c) ) {
			if ( pOutRow != NULL ) *pOutRow = r;
			if ( pOutColumn != NULL ) *pOutColumn = c;
			return 1;
		}
		if ( iRowStep != 0 ) {
			r += iRowStep;
		} else {
			c += iColumnStep;
		}
	}
	return 0;
}

static int __xuiTableViewSetHover(xui_widget pWidget, xui_table_view_data_t* pData, int iRow, int iColumn)
{
	int iOldRow;
	int iOldColumn;

	if ( pData == NULL ) {
		return XUI_OK;
	}
	if ( (pData->iHoverRow == iRow) && (pData->iHoverColumn == iColumn) ) {
		return XUI_OK;
	}
	iOldRow = pData->iHoverRow;
	iOldColumn = pData->iHoverColumn;
	pData->iHoverRow = iRow;
	pData->iHoverColumn = iColumn;
	pData->iHoverCount++;
	if ( pData->onHover != NULL ) {
		pData->onHover(pWidget, iRow, iColumn, pData->iSelectionMode, pData->pHoverUser);
	}
	return __xuiTableViewInvalidateHoverCells(pWidget, pData, iOldRow, iOldColumn, iRow, iColumn);
}

static int __xuiTableViewEnsureVisibleInternal(xui_widget pWidget, xui_table_view_data_t* pData, int iRow, int iColumn)
{
	xui_table_view_cell_t tCell;
	xui_rect_t tRect;
	int iOwnerRow;
	int iOwnerColumn;

	if ( (pData == NULL) || (pData->pFrame == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (iRow < 0) || (iRow >= pData->iRowCount) || (iColumn < 0) || (iColumn >= pData->iColumnCount) ) {
		return XUI_OK;
	}
	(void)__xuiTableViewMergeOwner(pWidget, pData, iRow, iColumn, &iOwnerRow, &iOwnerColumn);
	__xuiTableViewGetCell(pWidget, pData, iOwnerRow, iOwnerColumn, &tCell);
	tRect = __xuiTableViewCellContentRectData(pData, iOwnerRow, iOwnerColumn, tCell.iRowSpan, tCell.iColSpan);
	return xuiScrollFrameEnsureRectVisible(pData->pFrame, tRect);
}

static void __xuiTableViewNotifySelect(xui_widget pWidget, xui_table_view_data_t* pData)
{
	if ( pData == NULL ) {
		return;
	}
	pData->iSelectCount++;
	if ( pData->onSelect != NULL ) {
		pData->onSelect(pWidget, pData->iSelectedRow, pData->iSelectedColumn, pData->iSelectionMode, pData->pSelectUser);
	}
}

static int __xuiTableViewSelect(xui_widget pWidget, xui_table_view_data_t* pData, int iRow, int iColumn, int bNotify)
{
	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	(void)__xuiTableViewSyncRowCount(pWidget, pData);
	if ( (iRow < 0) || (iRow >= pData->iRowCount) ) {
		iRow = -1;
		iColumn = -1;
	}
	if ( iRow >= 0 ) {
		if ( pData->iSelectionMode == XUI_TABLE_VIEW_SELECTION_ROW ) {
			iColumn = -1;
			if ( __xuiTableViewRowDisabled(pData, iRow) ) {
				return XUI_OK;
			}
		} else {
			if ( (iColumn < 0) || (iColumn >= pData->iColumnCount) || __xuiTableViewCellDisabled(pWidget, pData, iRow, iColumn) ) {
				return XUI_OK;
			}
		}
	}
	if ( (pData->iSelectedRow == iRow) && (pData->iSelectedColumn == iColumn) ) {
		if ( bNotify ) {
			__xuiTableViewNotifySelect(pWidget, pData);
		}
		return XUI_OK;
	}
	pData->iSelectedRow = iRow;
	pData->iSelectedColumn = iColumn;
	pData->iFocusRow = iRow;
	pData->iFocusColumn = iColumn;
	if ( pData->iSelectionMode == XUI_TABLE_VIEW_SELECTION_ROW ) {
		pData->iFocusColumn = -1;
	}
	pData->iChangeCount++;
	if ( iRow >= 0 ) {
		(void)__xuiTableViewEnsureVisibleInternal(pWidget, pData, iRow, (iColumn >= 0) ? iColumn : __xuiTableViewFirstVisibleColumn(pData));
	}
	if ( bNotify ) {
		__xuiTableViewNotifySelect(pWidget, pData);
	}
	return __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiTableViewHitCellWorld(xui_widget pWidget, xui_table_view_data_t* pData, float fX, float fY, int* pRow, int* pColumn)
{
	xui_rect_t tViewportWorld;
	float fOffsetX;
	float fOffsetY;
	float fContentX;
	float fContentY;
	int iRow;
	int iColumn;
	int iOwnerRow;
	int iOwnerColumn;

	if ( pRow != NULL ) *pRow = -1;
	if ( pColumn != NULL ) *pColumn = -1;
	if ( (pWidget == NULL) || (pData == NULL) || (pData->pViewport == NULL) ) {
		return 0;
	}
	(void)__xuiTableViewSyncRowCount(pWidget, pData);
	tViewportWorld = xuiWidgetGetWorldRect(pData->pViewport);
	if ( !__xuiTableViewPointInRect(tViewportWorld, fX, fY) ) {
		return 0;
	}
	fOffsetX = 0.0f;
	fOffsetY = 0.0f;
	(void)xuiScrollFrameGetOffset(pData->pFrame, &fOffsetX, &fOffsetY);
	fContentX = fX - tViewportWorld.fX + fOffsetX;
	fContentY = fY - tViewportWorld.fY + fOffsetY;
	iColumn = __xuiTableViewColumnAtContentX(pData, fContentX);
	iRow = __xuiTableViewRowAtContentY(pData, fContentY);
	if ( (iRow < 0) || (iColumn < 0) ) {
		return 0;
	}
	(void)__xuiTableViewMergeOwner(pWidget, pData, iRow, iColumn, &iOwnerRow, &iOwnerColumn);
	if ( pRow != NULL ) *pRow = iOwnerRow;
	if ( pColumn != NULL ) *pColumn = iOwnerColumn;
	return 1;
}

static int __xuiTableViewHeaderColumnAtWorld(xui_widget pWidget, xui_table_view_data_t* pData, float fX, float fY)
{
	xui_rect_t tHeader;
	xui_rect_t tWorld;
	float fOffsetX;
	float fContentX;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return -1;
	}
	tWorld = xuiWidgetGetWorldRect(pWidget);
	tHeader = __xuiTableViewHeaderRect(pWidget, pData);
	tHeader.fX += tWorld.fX;
	tHeader.fY += tWorld.fY;
	if ( !__xuiTableViewPointInRect(tHeader, fX, fY) ) {
		return -1;
	}
	fOffsetX = 0.0f;
	(void)xuiScrollFrameGetOffset(pData->pFrame, &fOffsetX, NULL);
	fContentX = fX - tHeader.fX + fOffsetX;
	return __xuiTableViewColumnAtContentX(pData, fContentX);
}

static int __xuiTableViewHeaderResizeAtWorld(xui_widget pWidget, xui_table_view_data_t* pData, float fX, float fY)
{
	xui_rect_t tHeader;
	xui_rect_t tWorld;
	float fOffsetX;
	float fLeft;
	float fRight;
	int i;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return -1;
	}
	tWorld = xuiWidgetGetWorldRect(pWidget);
	tHeader = __xuiTableViewHeaderRect(pWidget, pData);
	tHeader.fX += tWorld.fX;
	tHeader.fY += tWorld.fY;
	if ( !__xuiTableViewPointInRect(tHeader, fX, fY) ) {
		return -1;
	}
	fOffsetX = 0.0f;
	(void)xuiScrollFrameGetOffset(pData->pFrame, &fOffsetX, NULL);
	for ( i = 0; i < pData->iColumnCount; i++ ) {
		if ( (pData->arrColumns[i].bVisible == 0) || (pData->arrColumns[i].bResizable == 0) ) {
			continue;
		}
		fLeft = tHeader.fX + __xuiTableViewColumnLeft(pData, i) - fOffsetX;
		fRight = fLeft + __xuiTableViewColumnWidth(pData, i);
		if ( (fX >= fRight - 4.0f) && (fX <= fRight + 4.0f) ) {
			return i;
		}
	}
	return -1;
}

static int __xuiTableViewApplyFrameStyle(xui_widget pWidget, xui_table_view_data_t* pData)
{
	int iRet;

	(void)pWidget;
	if ( (pData == NULL) || (pData->pFrame == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = xuiScrollFrameSetScrollbarPolicy(pData->pFrame, XUI_SCROLLBAR_POLICY_AUTO, XUI_SCROLLBAR_POLICY_AUTO);
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameSetWheelAxis(pData->pFrame, XUI_WHEEL_AXIS_VERTICAL);
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameSetWheelStep(pData->pFrame, pData->fDefaultRowHeight * 3.0f);
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameSetContentDragEnabled(pData->pFrame, 0);
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameSetCornerMode(pData->pFrame, XUI_SCROLL_FRAME_CORNER_AUTO);
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameSetMetrics(pData->pFrame, 8.0f, 18.0f, 0.0f);
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameSetColors(pData->pFrame, pData->iBarColor, pData->iThumbColor, pData->iScrollbarHoverColor, pData->iScrollbarActiveColor, pData->iScrollbarFocusColor, pData->iScrollbarDisabledColor);
	return iRet;
}

static int __xuiTableViewPointerMove(xui_widget pWidget, xui_table_view_data_t* pData, const xui_event_t* pEvent)
{
	float fDelta;
	float fWidth;
	float fMin;
	float fMax;
	int iRow;
	int iColumn;

	if ( (pEvent == NULL) || (pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) ) {
		return XUI_OK;
	}
	if ( (pData->iResizeColumn >= 0) && (xuiGetPointerCapture(xuiWidgetGetContext(pWidget)) == pWidget) ) {
		fDelta = pEvent->fX - pData->fDragX;
		fWidth = pData->fDragWidth + fDelta;
		fMin = pData->arrColumns[pData->iResizeColumn].fMinWidth;
		fMax = pData->arrColumns[pData->iResizeColumn].fMaxWidth;
		if ( fMin <= 0.0f ) fMin = 30.0f;
		if ( fWidth < fMin ) fWidth = fMin;
		if ( (fMax > 0.0f) && (fWidth > fMax) ) fWidth = fMax;
		if ( pData->arrColumns[pData->iResizeColumn].fWidth != fWidth ) {
			pData->arrColumns[pData->iResizeColumn].fWidth = fWidth;
			pData->iColumnResizeCount++;
			pData->iChangeCount++;
			(void)__xuiTableViewUpdateContentSize(pWidget, pData);
			if ( pData->onColumnResize != NULL ) {
				pData->onColumnResize(pWidget, pData->iResizeColumn, fWidth, pData->pColumnResizeUser);
			}
			(void)__xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		}
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( __xuiTableViewHitCellWorld(pWidget, pData, pEvent->fX, pEvent->fY, &iRow, &iColumn) ) {
		return __xuiTableViewSetHover(pWidget, pData, iRow, iColumn);
	}
	return __xuiTableViewSetHover(pWidget, pData, -1, -1);
}

static int __xuiTableViewPointerDown(xui_widget pWidget, xui_table_view_data_t* pData, const xui_event_t* pEvent)
{
	int iRow;
	int iColumn;
	int iResize;
	int iHeader;

	if ( (pEvent == NULL) || (pEvent->iButton != XUI_POINTER_BUTTON_LEFT) || (pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) ) {
		return XUI_OK;
	}
	if ( !xuiWidgetGetEnabled(pWidget) ) {
		return XUI_OK;
	}
	iResize = __xuiTableViewHeaderResizeAtWorld(pWidget, pData, pEvent->fX, pEvent->fY);
	if ( iResize >= 0 ) {
		(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
		pData->iResizeColumn = iResize;
		pData->fDragX = pEvent->fX;
		pData->fDragWidth = __xuiTableViewColumnWidth(pData, iResize);
		(void)xuiSetPointerCapture(xuiWidgetGetContext(pWidget), pWidget);
		return XUI_EVENT_DISPATCH_STOP;
	}
	iHeader = __xuiTableViewHeaderColumnAtWorld(pWidget, pData, pEvent->fX, pEvent->fY);
	if ( iHeader >= 0 ) {
		(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
		if ( pData->iSortColumn == iHeader ) {
			pData->bSortDescending = !pData->bSortDescending;
		} else {
			pData->iSortColumn = iHeader;
			pData->bSortDescending = 0;
		}
		pData->iSortCount++;
		pData->iChangeCount++;
		if ( pData->onSort != NULL ) {
			pData->onSort(pWidget, pData->iSortColumn, pData->bSortDescending, pData->pSortUser);
		}
		(void)__xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( __xuiTableViewHitCellWorld(pWidget, pData, pEvent->fX, pEvent->fY, &iRow, &iColumn) ) {
		(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
		pData->iActiveRow = iRow;
		pData->iActiveColumn = iColumn;
		(void)__xuiTableViewSetHover(pWidget, pData, iRow, iColumn);
		(void)__xuiTableViewSelect(pWidget, pData, iRow, iColumn, 1);
		return XUI_EVENT_DISPATCH_STOP;
	}
	return XUI_OK;
}

static int __xuiTableViewPointerUp(xui_widget pWidget, xui_table_view_data_t* pData, const xui_event_t* pEvent)
{
	if ( (pEvent == NULL) || (pEvent->iButton != XUI_POINTER_BUTTON_LEFT) || (pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) ) {
		return XUI_OK;
	}
	if ( (pData->iResizeColumn >= 0) && (xuiGetPointerCapture(xuiWidgetGetContext(pWidget)) == pWidget) ) {
		pData->iResizeColumn = -1;
		(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
		(void)__xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		return XUI_EVENT_DISPATCH_STOP;
	}
	return XUI_OK;
}

static int __xuiTableViewVisibleRows(xui_widget pWidget, xui_table_view_data_t* pData)
{
	xui_rect_t tViewport;
	int iRows;

	tViewport = __xuiTableViewViewportRectLocal(pWidget, pData);
	iRows = (pData->fDefaultRowHeight > 0.0f) ? (int)(tViewport.fH / pData->fDefaultRowHeight) : 1;
	return (iRows > 0) ? iRows : 1;
}

static int __xuiTableViewMoveKeyboardCell(xui_widget pWidget, xui_table_view_data_t* pData, int iRow, int iColumn, int iRowStep, int iColumnStep)
{
	int iTargetRow;
	int iTargetColumn;

	if ( __xuiTableViewFindSelectableCell(pWidget, pData, iRow, iColumn, iRowStep, iColumnStep, &iTargetRow, &iTargetColumn) ) {
		(void)__xuiTableViewSelect(pWidget, pData, iTargetRow, iTargetColumn, 1);
		return XUI_EVENT_DISPATCH_STOP;
	}
	return XUI_OK;
}

static int __xuiTableViewKeyDown(xui_widget pWidget, xui_table_view_data_t* pData, const xui_event_t* pEvent)
{
	int iCurrentRow;
	int iCurrentColumn;
	int iTarget;
	int iRows;

	if ( (pEvent == NULL) || (pEvent->iPhase == XUI_EVENT_PHASE_BUBBLE) || !xuiWidgetGetEnabled(pWidget) || (pData->iRowCount <= 0) ) {
		return XUI_OK;
	}
	iCurrentRow = (pData->iSelectedRow >= 0) ? pData->iSelectedRow : pData->iFocusRow;
	iCurrentColumn = (pData->iSelectedColumn >= 0) ? pData->iSelectedColumn : pData->iFocusColumn;
	if ( iCurrentColumn < 0 ) {
		iCurrentColumn = __xuiTableViewFirstVisibleColumn(pData);
	}
	if ( pData->iSelectionMode == XUI_TABLE_VIEW_SELECTION_ROW ) {
		switch ( pEvent->iKey ) {
		case XUI_KEY_UP:
			iTarget = (iCurrentRow >= 0) ? __xuiTableViewFindEnabledRow(pWidget, pData, iCurrentRow - 1, -1) : __xuiTableViewFindEnabledRow(pWidget, pData, 0, 1);
			if ( iTarget >= 0 ) { (void)__xuiTableViewSelect(pWidget, pData, iTarget, -1, 1); return XUI_EVENT_DISPATCH_STOP; }
			break;
		case XUI_KEY_DOWN:
			iTarget = (iCurrentRow >= 0) ? __xuiTableViewFindEnabledRow(pWidget, pData, iCurrentRow + 1, 1) : __xuiTableViewFindEnabledRow(pWidget, pData, 0, 1);
			if ( iTarget >= 0 ) { (void)__xuiTableViewSelect(pWidget, pData, iTarget, -1, 1); return XUI_EVENT_DISPATCH_STOP; }
			break;
		case XUI_KEY_PAGE_UP:
			iRows = __xuiTableViewVisibleRows(pWidget, pData);
			iTarget = (iCurrentRow >= 0) ? __xuiTableViewFindEnabledRow(pWidget, pData, iCurrentRow - iRows, -1) : __xuiTableViewFindEnabledRow(pWidget, pData, 0, 1);
			if ( iTarget >= 0 ) { (void)__xuiTableViewSelect(pWidget, pData, iTarget, -1, 1); return XUI_EVENT_DISPATCH_STOP; }
			break;
		case XUI_KEY_PAGE_DOWN:
			iRows = __xuiTableViewVisibleRows(pWidget, pData);
			iTarget = (iCurrentRow >= 0) ? __xuiTableViewFindEnabledRow(pWidget, pData, iCurrentRow + iRows, 1) : __xuiTableViewFindEnabledRow(pWidget, pData, 0, 1);
			if ( iTarget >= 0 ) { (void)__xuiTableViewSelect(pWidget, pData, iTarget, -1, 1); return XUI_EVENT_DISPATCH_STOP; }
			break;
		case XUI_KEY_HOME:
			iTarget = __xuiTableViewFindEnabledRow(pWidget, pData, 0, 1);
			if ( iTarget >= 0 ) { (void)__xuiTableViewSelect(pWidget, pData, iTarget, -1, 1); return XUI_EVENT_DISPATCH_STOP; }
			break;
		case XUI_KEY_END:
			iTarget = __xuiTableViewFindEnabledRow(pWidget, pData, pData->iRowCount - 1, -1);
			if ( iTarget >= 0 ) { (void)__xuiTableViewSelect(pWidget, pData, iTarget, -1, 1); return XUI_EVENT_DISPATCH_STOP; }
			break;
		case XUI_KEY_ENTER:
		case XUI_KEY_SPACE:
			if ( pData->iSelectedRow >= 0 ) { __xuiTableViewNotifySelect(pWidget, pData); return XUI_EVENT_DISPATCH_STOP; }
			break;
		default:
			break;
		}
		return XUI_OK;
	}
	switch ( pEvent->iKey ) {
	case XUI_KEY_UP:
		return __xuiTableViewMoveKeyboardCell(pWidget, pData, (iCurrentRow >= 0) ? iCurrentRow - 1 : 0, iCurrentColumn, -1, 0);
	case XUI_KEY_DOWN:
		return __xuiTableViewMoveKeyboardCell(pWidget, pData, (iCurrentRow >= 0) ? iCurrentRow + 1 : 0, iCurrentColumn, 1, 0);
	case XUI_KEY_LEFT:
		return __xuiTableViewMoveKeyboardCell(pWidget, pData, (iCurrentRow >= 0) ? iCurrentRow : 0, iCurrentColumn - 1, 0, -1);
	case XUI_KEY_RIGHT:
		return __xuiTableViewMoveKeyboardCell(pWidget, pData, (iCurrentRow >= 0) ? iCurrentRow : 0, iCurrentColumn + 1, 0, 1);
	case XUI_KEY_PAGE_UP:
		iRows = __xuiTableViewVisibleRows(pWidget, pData);
		return __xuiTableViewMoveKeyboardCell(pWidget, pData, (iCurrentRow >= 0) ? iCurrentRow - iRows : 0, iCurrentColumn, -1, 0);
	case XUI_KEY_PAGE_DOWN:
		iRows = __xuiTableViewVisibleRows(pWidget, pData);
		return __xuiTableViewMoveKeyboardCell(pWidget, pData, (iCurrentRow >= 0) ? iCurrentRow + iRows : 0, iCurrentColumn, 1, 0);
	case XUI_KEY_HOME:
		if ( (pEvent->iModifiers & XUI_MOD_CTRL) != 0 ) return __xuiTableViewMoveKeyboardCell(pWidget, pData, 0, __xuiTableViewFirstVisibleColumn(pData), 1, 0);
		return __xuiTableViewMoveKeyboardCell(pWidget, pData, (iCurrentRow >= 0) ? iCurrentRow : 0, __xuiTableViewFirstVisibleColumn(pData), 0, 1);
	case XUI_KEY_END:
		if ( (pEvent->iModifiers & XUI_MOD_CTRL) != 0 ) return __xuiTableViewMoveKeyboardCell(pWidget, pData, pData->iRowCount - 1, __xuiTableViewLastVisibleColumn(pData), -1, 0);
		return __xuiTableViewMoveKeyboardCell(pWidget, pData, (iCurrentRow >= 0) ? iCurrentRow : 0, __xuiTableViewLastVisibleColumn(pData), 0, -1);
	case XUI_KEY_ENTER:
	case XUI_KEY_SPACE:
		if ( pData->iSelectedRow >= 0 ) { __xuiTableViewNotifySelect(pWidget, pData); return XUI_EVENT_DISPATCH_STOP; }
		break;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiTableViewEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_table_view_data_t* pData;

	(void)pUser;
	if ( (pWidget == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_MOVE:
		return __xuiTableViewPointerMove(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_LEAVE:
		if ( pData->iResizeColumn < 0 ) return __xuiTableViewSetHover(pWidget, pData, -1, -1);
		return XUI_OK;
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		pData->iResizeColumn = -1;
		pData->iActiveRow = -1;
		pData->iActiveColumn = -1;
		return __xuiTableViewSetHover(pWidget, pData, -1, -1);
	case XUI_EVENT_POINTER_DOWN:
		return __xuiTableViewPointerDown(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_UP:
		return __xuiTableViewPointerUp(pWidget, pData, pEvent);
	case XUI_EVENT_KEY_DOWN:
		return __xuiTableViewKeyDown(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_WHEEL:
		if ( pEvent->iPhase != XUI_EVENT_PHASE_CAPTURE ) {
			(void)__xuiTableViewSetHover(pWidget, pData, -1, -1);
		}
		return XUI_OK;
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
	case XUI_EVENT_ENABLED_CHANGED:
	case XUI_EVENT_VISIBLE_CHANGED:
		if ( pEvent->iType == XUI_EVENT_ENABLED_CHANGED && pData->pFrame != NULL ) {
			(void)xuiWidgetSetEnabled(pData->pFrame, xuiWidgetGetEnabled(pWidget));
		}
		return __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	case XUI_EVENT_BOUNDS_CHANGED:
		(void)__xuiTableViewUpdateContentSize(pWidget, pData);
		return XUI_OK;
	default:
		break;
	}
	return XUI_OK;
}

static void __xuiTableViewFrameChanged(xui_widget pFrame, float fOffsetX, float fOffsetY, void* pUser)
{
	xui_widget pWidget;
	xui_table_view_data_t* pData;

	(void)pFrame;
	(void)fOffsetX;
	(void)fOffsetY;
	pWidget = (xui_widget)pUser;
	pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) {
		return;
	}
	pData->iHoverRow = -1;
	pData->iHoverColumn = -1;
	pData->iChangeCount++;
	(void)__xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiTableViewContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_table_view_data_t* pData;
	xui_table_view_data_t tResolved;
	float fRows;

	(void)tConstraint;
	pData = (xui_table_view_data_t*)pUser;
	if ( (pWidget == NULL) || (pData == NULL) || (pSize == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiTableViewResolve(pWidget, pData, &tResolved);
	fRows = (float)((pData->iRowCount > 0 && pData->iRowCount < XUI_TABLE_VIEW_DEFAULT_ROWS) ? pData->iRowCount : XUI_TABLE_VIEW_DEFAULT_ROWS);
	if ( fRows < 3.0f ) fRows = 3.0f;
	pSize->fX = xuiInternalSnapSize(XUI_TABLE_VIEW_DEFAULT_WIDTH);
	pSize->fY = xuiInternalSnapSize(tResolved.fHeaderHeight + tResolved.fDefaultRowHeight * fRows + tResolved.fBorderWidth * 2.0f);
	return XUI_OK;
}

static int __xuiTableViewArrange(xui_widget pWidget, xui_rect_t tContentRect, void* pUser)
{
	xui_table_view_data_t* pData;
	xui_table_view_data_t tResolved;
	xui_rect_t tFrame;
	int iRet;

	(void)tContentRect;
	pData = (xui_table_view_data_t*)pUser;
	if ( (pWidget == NULL) || (pData == NULL) || (pData->pFrame == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiTableViewResolve(pWidget, pData, &tResolved);
	pData->pFont = tResolved.pFont;
	pData->fDefaultColumnWidth = tResolved.fDefaultColumnWidth;
	pData->fDefaultRowHeight = tResolved.fDefaultRowHeight;
	pData->fHeaderHeight = tResolved.fHeaderHeight;
	pData->fBorderWidth = tResolved.fBorderWidth;
	tFrame = __xuiTableViewFrameRect(pWidget, pData);
	iRet = __xuiTableViewUpdateContentSize(pWidget, pData);
	if ( iRet == XUI_OK ) iRet = xuiWidgetArrange(pData->pFrame, tFrame);
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameLayout(pData->pFrame);
	return iRet;
}

static int __xuiTableViewCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_table_view_data_t* pData;
	xui_table_view_data_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tRect;
	xui_rect_t tHeader;
	xui_rect_t tCell;
	xui_rect_t tText;
	xui_rect_t tVisible;
	float fOffsetX;
	float fX;
	float fW;
	int i;
	int iState;
	int iHandled;
	int iRet;
	uint32_t iBorder;

	(void)iStateId;
	(void)pUser;
	if ( (pWidget == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	__xuiTableViewResolve(pWidget, pData, &tResolved);
	tRect = xuiWidgetGetRect(pWidget);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	tRect = xuiInternalSnapRect(tRect);
	iRet = __xuiTableViewDrawFill(pProxy, pDraw, tRect, tResolved.iBackgroundColor);
	if ( iRet != XUI_OK ) return iRet;
	tHeader = __xuiTableViewHeaderRect(pWidget, &tResolved);
	if ( tHeader.fH > 0.0f ) {
		iRet = __xuiTableViewDrawFill(pProxy, pDraw, tHeader, tResolved.iHeaderColor);
		if ( iRet != XUI_OK ) return iRet;
		fOffsetX = 0.0f;
		(void)xuiScrollFrameGetOffset(pData->pFrame, &fOffsetX, NULL);
		for ( i = 0; i < pData->iColumnCount; i++ ) {
			fW = __xuiTableViewColumnWidth(pData, i);
			if ( fW <= 0.0f ) continue;
			fX = tHeader.fX + __xuiTableViewColumnLeft(pData, i) - fOffsetX;
			tCell = xuiInternalSnapRect((xui_rect_t){fX, tHeader.fY, fW, tHeader.fH});
			pData->arrColumns[i].tRect = tCell;
			tVisible = tCell;
			if ( tVisible.fX < tHeader.fX ) {
				tVisible.fW -= (tHeader.fX - tVisible.fX);
				tVisible.fX = tHeader.fX;
			}
			if ( tVisible.fX + tVisible.fW > tHeader.fX + tHeader.fW ) {
				tVisible.fW = tHeader.fX + tHeader.fW - tVisible.fX;
			}
			if ( tVisible.fW <= 0.0f ) continue;
			iState = XUI_TABLE_CELL_HEADER;
			if ( i == pData->iSortColumn ) iState |= XUI_TABLE_CELL_FOCUS;
			if ( pData->onHeaderRender != NULL ) {
				iHandled = pData->onHeaderRender(pWidget, i, &pData->arrColumns[i], pDraw, tVisible, iState, pData->pHeaderRenderUser);
				if ( iHandled < 0 ) return iHandled;
				if ( iHandled ) continue;
			}
			if ( i == pData->iSortColumn ) {
				iRet = __xuiTableViewDrawFill(pProxy, pDraw, tVisible, __xuiTableViewColorWithAlpha(tResolved.iSelectedColor, 32));
				if ( iRet != XUI_OK ) return iRet;
			}
			if ( pProxy->drawLine != NULL ) {
				iRet = pProxy->drawLine(pProxy, pDraw, tCell.fX + tCell.fW - 0.5f, tHeader.fY + 4.0f, tCell.fX + tCell.fW - 0.5f, tHeader.fY + tHeader.fH - 4.0f, 1.0f, __xuiTableViewColorWithAlpha(tResolved.iGridColor, 210));
				if ( iRet != XUI_OK ) return iRet;
			}
			if ( tResolved.pFont != NULL ) {
				tText = (xui_rect_t){tVisible.fX + 8.0f, tVisible.fY, __xuiTableViewMaxFloat(1.0f, tVisible.fW - 18.0f), tVisible.fH};
				iRet = pProxy->drawText(pProxy, pDraw, tResolved.pFont, __xuiTableViewText(pData->arrColumns[i].sTitle), xuiInternalSnapRect(tText), tResolved.iHeaderTextColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
				if ( iRet != XUI_OK ) return iRet;
			}
			if ( i == pData->iSortColumn ) {
				iRet = __xuiTableViewDrawSortMark(pProxy, pDraw, tVisible, pData->bSortDescending, tResolved.iHeaderTextColor);
				if ( iRet != XUI_OK ) return iRet;
			}
		}
		if ( pProxy->drawLine != NULL ) {
			iRet = pProxy->drawLine(pProxy, pDraw, tHeader.fX, tHeader.fY + tHeader.fH - 0.5f, tHeader.fX + tHeader.fW, tHeader.fY + tHeader.fH - 0.5f, 1.0f, tResolved.iGridColor);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	iBorder = ((xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget) && xuiWidgetGetEnabled(pWidget)) ? tResolved.iFocusRingColor : tResolved.iGridColor;
	return __xuiTableViewDrawStroke(pProxy, pDraw, tRect, tResolved.fBorderWidth, iBorder);
}

static int __xuiTableViewViewportRender(xui_widget pViewport, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_widget pWidget;
	xui_table_view_data_t* pData;
	xui_table_view_data_t tResolved;
	xui_proxy pProxy;
	xui_table_view_cell_t tCellData;
	xui_rect_t tRect;
	xui_rect_t tViewportContent;
	xui_rect_t tCellContent;
	xui_rect_t tCell;
	xui_rect_t tFocus;
	float fOffsetX;
	float fOffsetY;
	float fViewportW;
	float fViewportH;
	float fRowTop;
	float fRowHeight;
	int iStart;
	int iEnd;
	int i;
	int j;
	int iOwnerRow;
	int iOwnerColumn;
	int iState;
	int iHandled;
	int iRet;
	uint32_t iBackground;
	uint32_t iGrid;

	(void)iStateId;
	pWidget = (xui_widget)pUser;
	pData = __xuiTableViewGetData(pWidget);
	if ( (pViewport == NULL) || (pData == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	__xuiTableViewResolve(pWidget, pData, &tResolved);
	tRect = xuiWidgetGetRect(pViewport);
	fViewportW = __xuiTableViewMaxFloat(0.0f, tRect.fW);
	fViewportH = __xuiTableViewMaxFloat(0.0f, tRect.fH);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	tRect = xuiInternalSnapRect(tRect);
	iRet = __xuiTableViewDrawFill(pProxy, pDraw, tRect, tResolved.iBackgroundColor);
	if ( iRet != XUI_OK ) return iRet;
	(void)__xuiTableViewSyncRowCount(pWidget, pData);
	if ( (pData->iRowCount <= 0) || (pData->iColumnCount <= 0) ) {
		pData->iFirstVisible = 0;
		pData->iPaintVisibleCount = 0;
		return XUI_OK;
	}
	fOffsetX = 0.0f;
	fOffsetY = 0.0f;
	(void)xuiScrollFrameGetOffset(pData->pFrame, &fOffsetX, &fOffsetY);
	tViewportContent = (xui_rect_t){fOffsetX, fOffsetY, fViewportW, fViewportH};
	iStart = 0;
	fRowTop = 0.0f;
	for ( i = 0; i < pData->iRowCount; i++ ) {
		fRowHeight = __xuiTableViewRowHeight(pData, i);
		if ( fRowTop + fRowHeight > fOffsetY ) {
			iStart = i;
			break;
		}
		fRowTop += fRowHeight;
	}
	iEnd = iStart;
	while ( (iEnd < pData->iRowCount) && (fRowTop < fOffsetY + fViewportH + pData->fDefaultRowHeight) ) {
		fRowTop += __xuiTableViewRowHeight(pData, iEnd);
		iEnd++;
	}
	pData->iFirstVisible = iStart;
	pData->iPaintVisibleCount = (iEnd > iStart) ? (iEnd - iStart) : 0;
	for ( i = iStart; i < iEnd; i++ ) {
		for ( j = 0; j < pData->iColumnCount; j++ ) {
			if ( __xuiTableViewColumnWidth(pData, j) <= 0.0f ) {
				continue;
			}
			if ( __xuiTableViewMergeOwner(pWidget, pData, i, j, &iOwnerRow, &iOwnerColumn) ) {
				continue;
			}
			__xuiTableViewGetCell(pWidget, pData, i, j, &tCellData);
			tCellContent = __xuiTableViewCellContentRectData(pData, i, j, tCellData.iRowSpan, tCellData.iColSpan);
			if ( !__xuiTableViewRectIntersects(tCellContent, tViewportContent) ) {
				continue;
			}
			tCell = xuiInternalSnapRect((xui_rect_t){tCellContent.fX - fOffsetX, tCellContent.fY - fOffsetY, tCellContent.fW, tCellContent.fH});
			iState = 0;
			if ( (pData->iSelectionMode == XUI_TABLE_VIEW_SELECTION_ROW && pData->iSelectedRow == i) ||
			     (pData->iSelectionMode == XUI_TABLE_VIEW_SELECTION_CELL && pData->iSelectedRow == i && pData->iSelectedColumn == j) ||
			     __xuiTableViewRowSelected(pData, i) ) {
				iState |= XUI_TABLE_CELL_SELECTED;
			}
			if ( pData->iHoverRow == i && (pData->iSelectionMode == XUI_TABLE_VIEW_SELECTION_ROW || pData->iHoverColumn == j) ) iState |= XUI_TABLE_CELL_HOVER;
			if ( __xuiTableViewRowDisabled(pData, i) || tCellData.bDisabled || !xuiWidgetGetEnabled(pWidget) ) iState |= XUI_TABLE_CELL_DISABLED;
			if ( pData->iFocusRow == i && (pData->iSelectionMode == XUI_TABLE_VIEW_SELECTION_ROW || pData->iFocusColumn == j) ) iState |= XUI_TABLE_CELL_FOCUS;
			if ( tCellData.bEditing ) iState |= XUI_TABLE_CELL_EDITING;
			if ( tCellData.bInvalid ) iState |= XUI_TABLE_CELL_INVALID;
			if ( tCellData.bDirty ) iState |= XUI_TABLE_CELL_DIRTY;
			if ( (tCellData.iRowSpan > 1) || (tCellData.iColSpan > 1) ) iState |= XUI_TABLE_CELL_MERGED;
			iBackground = (i & 1) ? tResolved.iAltRowColor : tResolved.iRowColor;
			if ( __xuiTableViewRowDisabled(pData, i) ) iBackground = tResolved.iDisabledColor;
			if ( pData->arrRows != NULL && i < pData->iRowStateCount && pData->arrRows[i].bHasStyle && __xuiTableViewAlpha(pData->arrRows[i].iBackgroundColor) != 0 ) iBackground = pData->arrRows[i].iBackgroundColor;
			if ( pData->arrColumns[j].bHasStyle && __xuiTableViewAlpha(pData->arrColumns[j].iBackgroundColor) != 0 ) iBackground = pData->arrColumns[j].iBackgroundColor;
			if ( tCellData.bHasStyle && __xuiTableViewAlpha(tCellData.iBackgroundColor) != 0 ) iBackground = tCellData.iBackgroundColor;
			if ( (iState & XUI_TABLE_CELL_SELECTED) != 0 ) {
				iBackground = tResolved.iSelectedColor;
			} else if ( (iState & XUI_TABLE_CELL_HOVER) != 0 ) {
				iBackground = tResolved.iHoverColor;
			}
			iRet = __xuiTableViewDrawFill(pProxy, pDraw, tCell, iBackground);
			if ( iRet != XUI_OK ) return iRet;
			iGrid = tResolved.iGridColor;
			if ( pData->arrRows != NULL && i < pData->iRowStateCount && pData->arrRows[i].bHasStyle && __xuiTableViewAlpha(pData->arrRows[i].iGridColor) != 0 ) iGrid = pData->arrRows[i].iGridColor;
			if ( pData->arrColumns[j].bHasStyle && __xuiTableViewAlpha(pData->arrColumns[j].iGridColor) != 0 ) iGrid = pData->arrColumns[j].iGridColor;
			if ( tCellData.bHasStyle && __xuiTableViewAlpha(tCellData.iGridColor) != 0 ) iGrid = tCellData.iGridColor;
			if ( tCellData.onRender != NULL ) {
				iHandled = tCellData.onRender(pWidget, i, j, &tCellData, pDraw, tCell, iState, tCellData.pRenderUser);
				if ( iHandled < 0 ) return iHandled;
				if ( iHandled ) {
					continue;
				}
			}
			if ( pData->arrColumns[j].onRender != NULL ) {
				iHandled = pData->arrColumns[j].onRender(pWidget, i, j, &tCellData, pDraw, tCell, iState, pData->arrColumns[j].pRenderUser);
				if ( iHandled < 0 ) return iHandled;
				if ( iHandled ) {
					continue;
				}
			}
			if ( pData->onCellRender != NULL ) {
				iHandled = pData->onCellRender(pWidget, i, j, &tCellData, pDraw, tCell, iState, pData->pCellRenderUser);
				if ( iHandled < 0 ) return iHandled;
				if ( iHandled ) {
					continue;
				}
			}
			iRet = __xuiTableViewDrawCellContent(pWidget, pData, &tResolved, pProxy, pDraw, i, j, &tCellData, tCell, iState);
			if ( iRet != XUI_OK ) return iRet;
			if ( pProxy->drawLine != NULL ) {
				iRet = pProxy->drawLine(pProxy, pDraw, tCell.fX, tCell.fY + tCell.fH - 0.5f, tCell.fX + tCell.fW, tCell.fY + tCell.fH - 0.5f, 1.0f, iGrid);
				if ( iRet != XUI_OK ) return iRet;
				iRet = pProxy->drawLine(pProxy, pDraw, tCell.fX + tCell.fW - 0.5f, tCell.fY, tCell.fX + tCell.fW - 0.5f, tCell.fY + tCell.fH, 1.0f, iGrid);
				if ( iRet != XUI_OK ) return iRet;
			}
			if ( (iState & XUI_TABLE_CELL_INVALID) != 0 ) {
				iRet = __xuiTableViewDrawFill(pProxy, pDraw, (xui_rect_t){tCell.fX, tCell.fY, 3.0f, tCell.fH}, XUI_COLOR_RGBA(218, 82, 82, 255));
				if ( iRet != XUI_OK ) return iRet;
			}
			if ( (iState & XUI_TABLE_CELL_DIRTY) != 0 && pProxy->drawTriangleFill != NULL ) {
				(void)pProxy->drawTriangleFill(pProxy, pDraw,
					(xui_vec2_t){tCell.fX + tCell.fW - 8.0f, tCell.fY},
					(xui_vec2_t){tCell.fX + tCell.fW, tCell.fY},
					(xui_vec2_t){tCell.fX + tCell.fW, tCell.fY + 8.0f},
					XUI_COLOR_RGBA(245, 158, 11, 255));
			}
			if ( (iState & XUI_TABLE_CELL_EDITING) != 0 ) {
				iRet = __xuiTableViewDrawStroke(pProxy, pDraw, (xui_rect_t){tCell.fX + 1.0f, tCell.fY + 1.0f, __xuiTableViewMaxFloat(1.0f, tCell.fW - 2.0f), __xuiTableViewMaxFloat(1.0f, tCell.fH - 2.0f)}, 1.0f, XUI_COLOR_RGBA(34, 160, 112, 255));
				if ( iRet != XUI_OK ) return iRet;
			}
			if ( ((iState & XUI_TABLE_CELL_FOCUS) != 0) &&
			     (xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget) &&
			     ((iState & XUI_TABLE_CELL_DISABLED) == 0) ) {
				tFocus = (xui_rect_t){tCell.fX + 1.0f, tCell.fY + 1.0f, __xuiTableViewMaxFloat(1.0f, tCell.fW - 2.0f), __xuiTableViewMaxFloat(1.0f, tCell.fH - 2.0f)};
				iRet = __xuiTableViewDrawStroke(pProxy, pDraw, tFocus, 1.0f, tResolved.iFocusRingColor);
				if ( iRet != XUI_OK ) return iRet;
			}
		}
	}
	return XUI_OK;
}

static int __xuiTableViewTooltipResolve(xui_context pContext, xui_widget pTooltipWidget, xui_tooltip_desc_t* pDesc, void* pUser)
{
	xui_widget pWidget;
	xui_table_view_data_t* pData;
	xui_table_view_cell_t tCell;
	int iRow;
	int iColumn;

	(void)pTooltipWidget;
	pWidget = (xui_widget)pUser;
	pData = __xuiTableViewGetData(pWidget);
	if ( (pContext == NULL) || (pData == NULL) || (pDesc == NULL) ) {
		return 0;
	}
	if ( !__xuiTableViewHitCellWorld(pWidget, pData, pContext->fTooltipMouseX, pContext->fTooltipMouseY, &iRow, &iColumn) ) {
		return 0;
	}
	__xuiTableViewGetCell(pWidget, pData, iRow, iColumn, &tCell);
	if ( (tCell.sTooltip == NULL) || (tCell.sTooltip[0] == 0) ) {
		return 0;
	}
	memset(pDesc, 0, sizeof(*pDesc));
	pDesc->iSize = sizeof(*pDesc);
	pDesc->iType = XUI_TOOLTIP_TEXT;
	pDesc->sText = tCell.sTooltip;
	pDesc->iAnchor = XUI_TOOLTIP_ANCHOR_CURSOR;
	pDesc->fOffsetX = 12.0f;
	pDesc->fOffsetY = 18.0f;
	pDesc->fDelay = 0.35f;
	pDesc->bFollowCursor = 1;
	return 1;
}

static void __xuiTableViewDefaultLayout(xui_layout_t* pLayout)
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

static void __xuiTableViewDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE | XUI_CACHE_UPDATE_ALL_STATES;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiTableViewInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiTableViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiTableViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiTableViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiTableViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_WHEEL, __xuiTableViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiTableViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiTableViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiTableViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiTableViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BOUNDS_CHANGED, __xuiTableViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_ENABLED_CHANGED, __xuiTableViewEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_VISIBLE_CHANGED, __xuiTableViewEvent, NULL);
	return iRet;
}

static int __xuiTableViewInitViewport(xui_widget pWidget, xui_table_view_data_t* pData)
{
	xui_cache_policy_t tPolicy;
	int iRet;

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
	iRet = xuiWidgetSetCacheRenderCallback(pData->pViewport, __xuiTableViewViewportRender, pWidget);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetTooltipResolver(pData->pViewport, __xuiTableViewTooltipResolve, pWidget);
	return iRet;
}

static int __xuiTableViewCreateFrame(xui_widget pWidget, xui_table_view_data_t* pData, const xui_table_view_desc_t* pDesc)
{
	xui_scroll_frame_desc_t tFrameDesc;
	int iRet;

	memset(&tFrameDesc, 0, sizeof(tFrameDesc));
	tFrameDesc.iSize = sizeof(tFrameDesc);
	tFrameDesc.iPolicyX = XUI_SCROLLBAR_POLICY_AUTO;
	tFrameDesc.iPolicyY = XUI_SCROLLBAR_POLICY_AUTO;
	tFrameDesc.iScrollbarMode = XUI_SCROLLBAR_MODE_COMPACT;
	tFrameDesc.iWheelAxis = XUI_WHEEL_AXIS_VERTICAL;
	tFrameDesc.iCornerMode = XUI_SCROLL_FRAME_CORNER_AUTO;
	tFrameDesc.bContentDragEnabled = 0;
	tFrameDesc.fScrollbarSize = 8.0f;
	tFrameDesc.fMinThumbSize = 18.0f;
	tFrameDesc.fWheelStep = pData->fDefaultRowHeight * 3.0f;
	tFrameDesc.iTrackColor = pData->iBarColor;
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
	iRet = xuiScrollFrameSetChange(pData->pFrame, __xuiTableViewFrameChanged, pWidget);
	if ( iRet == XUI_OK ) iRet = __xuiTableViewInitViewport(pWidget, pData);
	if ( iRet == XUI_OK ) iRet = __xuiTableViewApplyFrameStyle(pWidget, pData);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pData->pFrame);
		pData->pFrame = NULL;
		pData->pViewport = NULL;
		return iRet;
	}
	return XUI_OK;
}

static int __xuiTableViewInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_table_view_data_t* pData;
	const xui_table_view_desc_t* pDesc;
	int iRet;

	(void)pUser;
	pData = (xui_table_view_data_t*)pTypeData;
	pDesc = (const xui_table_view_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiTableViewDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiTableViewDefaults(pData);
	__xuiTableViewApplyDesc(pData, pDesc);
	(void)xuiWidgetSetLayoutType(pWidget, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetFlowMode(pWidget, XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_CLIP);
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	(void)xuiWidgetSetPadding(pWidget, (xui_thickness_t){0.0f, 0.0f, 0.0f, 0.0f});
	iRet = __xuiTableViewCreateFrame(pWidget, pData, pDesc);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTableViewInitEvents(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	if ( pDesc != NULL ) {
		iRet = xuiTableViewSetColumns(pWidget, pDesc->arrColumns, pDesc->iColumnCount);
		if ( iRet != XUI_OK ) return iRet;
		iRet = xuiTableViewSetRows(pWidget, pDesc->arrRows, pDesc->iRowCount);
		if ( iRet != XUI_OK ) return iRet;
		if ( pDesc->onCount != NULL || pDesc->onCell != NULL ) {
			iRet = xuiTableViewSetAdapter(pWidget, pDesc->onCount, pDesc->onCell, pDesc->pAdapterUser);
			if ( iRet != XUI_OK ) return iRet;
		}
		if ( (pDesc->iSelectedRow > 0) ||
		     (pDesc->iSelectedColumn > 0) ||
		     ((pDesc->iSelectionMode == XUI_TABLE_VIEW_SELECTION_ROW) && (pDesc->iSelectedRow >= 0)) ) {
			if ( pData->iSelectionMode == XUI_TABLE_VIEW_SELECTION_ROW ) {
				iRet = xuiTableViewSetSelectedRow(pWidget, pDesc->iSelectedRow);
			} else {
				iRet = xuiTableViewSetSelectedCell(pWidget, pDesc->iSelectedRow, pDesc->iSelectedColumn);
			}
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	return __xuiTableViewUpdateContentSize(pWidget, pData);
}

static void __xuiTableViewDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_table_view_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_table_view_data_t*)pTypeData;
	if ( pData != NULL ) {
		if ( pData->arrRows != NULL ) {
			xrtFree(pData->arrRows);
		}
		memset(pData, 0, sizeof(*pData));
	}
}

static void __xuiTableViewRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
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

static void __xuiTableViewRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;
	uint32_t iLayoutDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	iLayoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.background.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.header.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.header.text_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.row.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.row.alt_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.row.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.row.selected_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.row.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.grid.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.text.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.text.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.focus.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.default.column_width", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.default.row_height", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.header.height", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.border.width", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "font.name", XUI_STYLE_VALUE_STRING, iLayoutDirty, XUI_STYLE_PROPERTY_INHERITED);
}

static xui_table_view_data_t* __xuiTableViewGetData(xui_widget pWidget)
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
	pType = xuiWidgetFindType(pContext, "tableview");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_table_view_data_t*)xuiWidgetGetTypeData(pWidget);
}

XUI_API xui_widget_type xuiTableViewGetType(xui_context pContext)
{
	xui_widget_type pType;
	xui_widget_type_desc_t tDesc;
	xui_layout_t tLayout;
	xui_cache_policy_t tPolicy;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "tableview");
	if ( pType != NULL ) {
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "tableview";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_table_view_data_t);
	tDesc.onInit = __xuiTableViewInit;
	tDesc.onDestroy = __xuiTableViewDestroy;
	tDesc.onContentMeasure = __xuiTableViewContentMeasure;
	tDesc.onLayoutArrange = __xuiTableViewArrange;
	tDesc.onCacheRender = __xuiTableViewCacheRender;
	__xuiTableViewDefaultLayout(&tLayout);
	__xuiTableViewDefaultCachePolicy(&tPolicy);
	tDesc.tLayout = tLayout;
	tDesc.tCachePolicy = tPolicy;
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	__xuiTableViewRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiTableViewCreate(xui_context pContext, xui_widget* ppWidget, const xui_table_view_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( !xuiInternalContextIsValid(pContext) || (ppWidget == NULL) || !__xuiTableViewDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiTableViewGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiTableViewSetColumns(xui_widget pWidget, const xui_table_view_column_t* arrColumns, int iCount)
{
	xui_table_view_data_t* pData;
	int i;
	int iRet;

	pData = __xuiTableViewGetData(pWidget);
	if ( (pData == NULL) || (iCount < 0) || (iCount > XUI_TABLE_VIEW_COLUMN_CAPACITY) || ((iCount > 0) && (arrColumns == NULL)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(pData->arrColumns, 0, sizeof(pData->arrColumns));
	pData->iColumnCount = iCount;
	for ( i = 0; i < iCount; i++ ) {
		pData->arrColumns[i] = arrColumns[i];
		if ( pData->arrColumns[i].fWidth <= 0.0f ) pData->arrColumns[i].fWidth = pData->fDefaultColumnWidth;
		if ( pData->arrColumns[i].fMinWidth <= 0.0f ) pData->arrColumns[i].fMinWidth = 30.0f;
		if ( pData->arrColumns[i].bVisibleSet == 0 ) pData->arrColumns[i].bVisible = 1;
		if ( pData->arrColumns[i].bResizableSet == 0 ) pData->arrColumns[i].bResizable = 1;
	}
	if ( pData->iSelectedColumn >= iCount ) pData->iSelectedColumn = -1;
	if ( pData->iFocusColumn >= iCount ) pData->iFocusColumn = -1;
	iRet = __xuiTableViewUpdateContentSize(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	pData->iChangeCount++;
	return __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTableViewSetRows(xui_widget pWidget, const xui_table_view_row_t* arrRows, int iCount)
{
	xui_table_view_data_t* pData;
	xui_table_view_row_t* arrNewRows;
	int iRet;

	pData = __xuiTableViewGetData(pWidget);
	if ( (pData == NULL) || (iCount < 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	arrNewRows = NULL;
	if ( iCount > 0 ) {
		arrNewRows = (xui_table_view_row_t*)xrtMalloc((size_t)iCount * sizeof(xui_table_view_row_t));
		if ( arrNewRows == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		if ( arrRows != NULL ) {
			memcpy(arrNewRows, arrRows, (size_t)iCount * sizeof(xui_table_view_row_t));
		} else {
			memset(arrNewRows, 0, (size_t)iCount * sizeof(xui_table_view_row_t));
		}
	}
	if ( pData->arrRows != NULL ) {
		xrtFree(pData->arrRows);
	}
	pData->arrRows = arrNewRows;
	pData->iRowStateCount = iCount;
	if ( pData->onCount == NULL ) {
		pData->iRowCount = iCount;
	}
	iRet = __xuiTableViewUpdateContentSize(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	pData->iChangeCount++;
	return __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTableViewSetAdapter(xui_widget pWidget, xui_table_view_count_proc onCount, xui_table_view_cell_proc onCell, void* pUser)
{
	xui_table_view_data_t* pData;
	int iRet;

	pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onCount = onCount;
	pData->onCell = onCell;
	pData->pAdapterUser = pUser;
	iRet = __xuiTableViewUpdateContentSize(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	pData->iChangeCount++;
	return __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTableViewRefreshAdapter(xui_widget pWidget)
{
	return xuiTableViewRefresh(pWidget);
}

XUI_API int xuiTableViewSetSort(xui_widget pWidget, xui_table_view_sort_proc onSort, void* pUser)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onSort = onSort;
	pData->pSortUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTableViewSetSelect(xui_widget pWidget, xui_table_view_select_proc onSelect, void* pUser)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onSelect = onSelect;
	pData->pSelectUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTableViewSetColumnResize(xui_widget pWidget, xui_table_view_column_resize_proc onResize, void* pUser)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onColumnResize = onResize;
	pData->pColumnResizeUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTableViewSetHover(xui_widget pWidget, xui_table_view_hover_proc onHover, void* pUser)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onHover = onHover;
	pData->pHoverUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTableViewSetMergeProvider(xui_widget pWidget, xui_table_view_merge_proc onMerge, void* pUser)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onMerge = onMerge;
	pData->pMergeUser = pUser;
	return __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTableViewSetHeaderRenderer(xui_widget pWidget, xui_table_view_header_renderer_proc onRender, void* pUser)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onHeaderRender = onRender;
	pData->pHeaderRenderUser = pUser;
	return __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTableViewSetCellRenderer(xui_widget pWidget, xui_table_view_cell_renderer_proc onRender, void* pUser)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onCellRender = onRender;
	pData->pCellRenderUser = pUser;
	return __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTableViewSetColumnFormatter(xui_widget pWidget, int iColumn, xui_table_view_format_proc onFormat, void* pUser)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( (pData == NULL) || (iColumn < 0) || (iColumn >= pData->iColumnCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrColumns[iColumn].onFormat = onFormat;
	pData->arrColumns[iColumn].pFormatUser = pUser;
	return __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTableViewSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pFont = pFont;
	return __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_font xuiTableViewGetFont(xui_widget pWidget)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API int xuiTableViewSetDefaultMetrics(xui_widget pWidget, float fColumnWidth, float fRowHeight, float fHeaderHeight)
{
	xui_table_view_data_t* pData;
	int iRet;

	pData = __xuiTableViewGetData(pWidget);
	if ( (pData == NULL) ||
	     ((fColumnWidth != 0.0f) && ((fColumnWidth <= 0.0f) || !__xuiTableViewFloatValid(fColumnWidth))) ||
	     ((fRowHeight != 0.0f) && ((fRowHeight <= 0.0f) || !__xuiTableViewFloatValid(fRowHeight))) ||
	     ((fHeaderHeight != 0.0f) && !__xuiTableViewFloatValid(fHeaderHeight)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( fColumnWidth > 0.0f ) pData->fDefaultColumnWidth = fColumnWidth;
	if ( fRowHeight > 0.0f ) pData->fDefaultRowHeight = fRowHeight;
	if ( fHeaderHeight >= 0.0f ) pData->fHeaderHeight = fHeaderHeight;
	iRet = __xuiTableViewUpdateContentSize(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTableViewGetDefaultMetrics(xui_widget pWidget, float* pColumnWidth, float* pRowHeight, float* pHeaderHeight)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pColumnWidth != NULL ) *pColumnWidth = pData->fDefaultColumnWidth;
	if ( pRowHeight != NULL ) *pRowHeight = pData->fDefaultRowHeight;
	if ( pHeaderHeight != NULL ) *pHeaderHeight = pData->fHeaderHeight;
	return XUI_OK;
}

XUI_API int xuiTableViewSetSelectionMode(xui_widget pWidget, int iMode)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( (pData == NULL) || !__xuiTableViewSelectionModeValid(iMode) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iSelectionMode = iMode;
	if ( iMode == XUI_TABLE_VIEW_SELECTION_ROW ) {
		pData->iSelectedColumn = -1;
		pData->iFocusColumn = -1;
	}
	return __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTableViewGetSelectionMode(xui_widget pWidget)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	return (pData != NULL) ? pData->iSelectionMode : XUI_TABLE_VIEW_SELECTION_CELL;
}

XUI_API int xuiTableViewSetSelectedRow(xui_widget pWidget, int iRow)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iSelectionMode = XUI_TABLE_VIEW_SELECTION_ROW;
	return __xuiTableViewSelect(pWidget, pData, iRow, -1, 0);
}

XUI_API int xuiTableViewGetSelectedRow(xui_widget pWidget)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	return (pData != NULL) ? pData->iSelectedRow : -1;
}

XUI_API int xuiTableViewSetSelectedCell(xui_widget pWidget, int iRow, int iColumn)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iSelectionMode = XUI_TABLE_VIEW_SELECTION_CELL;
	return __xuiTableViewSelect(pWidget, pData, iRow, iColumn, 0);
}

XUI_API int xuiTableViewGetSelectedCell(xui_widget pWidget, int* pRow, int* pColumn)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pRow != NULL ) *pRow = pData->iSelectedRow;
	if ( pColumn != NULL ) *pColumn = pData->iSelectedColumn;
	return XUI_OK;
}

XUI_API int xuiTableViewSetOffset(xui_widget pWidget, float fOffsetX, float fOffsetY)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( (pData == NULL) || (pData->pFrame == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameSetOffset(pData->pFrame, fOffsetX, fOffsetY);
}

XUI_API int xuiTableViewGetOffset(xui_widget pWidget, float* pOffsetX, float* pOffsetY)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( (pData == NULL) || (pData->pFrame == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameGetOffset(pData->pFrame, pOffsetX, pOffsetY);
}

XUI_API int xuiTableViewSetScrollbarMode(xui_widget pWidget, int iMode)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( (pData == NULL) || (pData->pFrame == NULL) || !__xuiTableViewScrollbarModeValid(iMode) ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameSetScrollbarMode(pData->pFrame, iMode);
}

XUI_API int xuiTableViewGetScrollbarMode(xui_widget pWidget)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	return (pData != NULL && pData->pFrame != NULL) ? xuiScrollFrameGetScrollbarMode(pData->pFrame) : XUI_SCROLLBAR_MODE_COMPACT;
}

XUI_API int xuiTableViewGetColumnCount(xui_widget pWidget)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	return (pData != NULL) ? pData->iColumnCount : 0;
}

XUI_API const xui_table_view_column_t* xuiTableViewGetColumn(xui_widget pWidget, int iColumn)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( (pData == NULL) || (iColumn < 0) || (iColumn >= pData->iColumnCount) ) return NULL;
	return &pData->arrColumns[iColumn];
}

XUI_API float xuiTableViewGetColumnWidth(xui_widget pWidget, int iColumn)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	return __xuiTableViewColumnWidth(pData, iColumn);
}

XUI_API int xuiTableViewSetColumnWidth(xui_widget pWidget, int iColumn, float fWidth)
{
	xui_table_view_data_t* pData;
	float fMin;
	float fMax;
	int iRet;

	pData = __xuiTableViewGetData(pWidget);
	if ( (pData == NULL) || (iColumn < 0) || (iColumn >= pData->iColumnCount) || (fWidth <= 0.0f) || !__xuiTableViewFloatValid(fWidth) ) return XUI_ERROR_INVALID_ARGUMENT;
	fMin = pData->arrColumns[iColumn].fMinWidth;
	fMax = pData->arrColumns[iColumn].fMaxWidth;
	if ( fMin <= 0.0f ) fMin = 30.0f;
	if ( fWidth < fMin ) fWidth = fMin;
	if ( (fMax > 0.0f) && (fWidth > fMax) ) fWidth = fMax;
	pData->arrColumns[iColumn].fWidth = fWidth;
	iRet = __xuiTableViewUpdateContentSize(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	pData->iChangeCount++;
	return __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTableViewGetRowCount(xui_widget pWidget)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	return __xuiTableViewSyncRowCount(pWidget, pData);
}

XUI_API int xuiTableViewGetFirstVisible(xui_widget pWidget)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	return (pData != NULL) ? pData->iFirstVisible : 0;
}

XUI_API int xuiTableViewGetPaintVisibleCount(xui_widget pWidget)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	return (pData != NULL) ? pData->iPaintVisibleCount : 0;
}

XUI_API int xuiTableViewGetActiveCell(xui_widget pWidget, int* pRow, int* pColumn)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pRow != NULL ) *pRow = pData->iActiveRow;
	if ( pColumn != NULL ) *pColumn = pData->iActiveColumn;
	return XUI_OK;
}

XUI_API int xuiTableViewGetHoverCell(xui_widget pWidget, int* pRow, int* pColumn)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pRow != NULL ) *pRow = pData->iHoverRow;
	if ( pColumn != NULL ) *pColumn = pData->iHoverColumn;
	return XUI_OK;
}

XUI_API int xuiTableViewGetCellContentRect(xui_widget pWidget, int iRow, int iColumn, xui_rect_t* pRect)
{
	xui_table_view_data_t* pData;
	xui_table_view_cell_t tCell;
	int iOwnerRow;
	int iOwnerColumn;

	if ( pRect != NULL ) memset(pRect, 0, sizeof(*pRect));
	pData = __xuiTableViewGetData(pWidget);
	if ( (pData == NULL) || (pRect == NULL) || (iRow < 0) || (iColumn < 0) || (iRow >= __xuiTableViewSyncRowCount(pWidget, pData)) || (iColumn >= pData->iColumnCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	(void)__xuiTableViewMergeOwner(pWidget, pData, iRow, iColumn, &iOwnerRow, &iOwnerColumn);
	__xuiTableViewGetCell(pWidget, pData, iOwnerRow, iOwnerColumn, &tCell);
	*pRect = __xuiTableViewCellContentRectData(pData, iOwnerRow, iOwnerColumn, tCell.iRowSpan, tCell.iColSpan);
	return XUI_OK;
}

XUI_API int xuiTableViewGetCellRect(xui_widget pWidget, int iRow, int iColumn, xui_rect_t* pRect)
{
	xui_table_view_data_t* pData;
	xui_rect_t tContent;
	xui_rect_t tViewport;
	float fOffsetX;
	float fOffsetY;
	int iRet;

	if ( pRect != NULL ) memset(pRect, 0, sizeof(*pRect));
	pData = __xuiTableViewGetData(pWidget);
	if ( (pData == NULL) || (pRect == NULL) || (pData->pFrame == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = xuiTableViewGetCellContentRect(pWidget, iRow, iColumn, &tContent);
	if ( iRet != XUI_OK ) return iRet;
	tViewport = __xuiTableViewViewportRectLocal(pWidget, pData);
	fOffsetX = 0.0f;
	fOffsetY = 0.0f;
	(void)xuiScrollFrameGetOffset(pData->pFrame, &fOffsetX, &fOffsetY);
	*pRect = (xui_rect_t){tViewport.fX + tContent.fX - fOffsetX, tViewport.fY + tContent.fY - fOffsetY, tContent.fW, tContent.fH};
	return XUI_OK;
}

XUI_API xui_widget xuiTableViewGetFrameWidget(xui_widget pWidget)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	return (pData != NULL) ? pData->pFrame : NULL;
}

XUI_API xui_widget xuiTableViewGetViewportWidget(xui_widget pWidget)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	return (pData != NULL) ? pData->pViewport : NULL;
}

XUI_API xui_widget xuiTableViewGetBodyWidget(xui_widget pWidget)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	return (pData != NULL) ? pData->pFrame : NULL;
}

XUI_API xui_scroll_model_t* xuiTableViewGetModel(xui_widget pWidget)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	return (pData != NULL && pData->pFrame != NULL) ? xuiScrollFrameGetModel(pData->pFrame) : NULL;
}

XUI_API xui_rect_t xuiTableViewGetViewportRect(xui_widget pWidget)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	return __xuiTableViewViewportRectLocal(pWidget, pData);
}

XUI_API int xuiTableViewGetItemAt(xui_widget pWidget, float fX, float fY, int* pRow, int* pColumn)
{
	xui_table_view_data_t* pData;
	xui_rect_t tViewport;
	float fOffsetX;
	float fOffsetY;
	float fContentX;
	float fContentY;
	int iRow;
	int iColumn;
	int iOwnerRow;
	int iOwnerColumn;

	if ( pRow != NULL ) *pRow = -1;
	if ( pColumn != NULL ) *pColumn = -1;
	pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	(void)__xuiTableViewSyncRowCount(pWidget, pData);
	tViewport = __xuiTableViewViewportRectLocal(pWidget, pData);
	if ( !__xuiTableViewPointInRect(tViewport, fX, fY) ) return XUI_OK;
	fOffsetX = 0.0f;
	fOffsetY = 0.0f;
	(void)xuiScrollFrameGetOffset(pData->pFrame, &fOffsetX, &fOffsetY);
	fContentX = fX - tViewport.fX + fOffsetX;
	fContentY = fY - tViewport.fY + fOffsetY;
	iColumn = __xuiTableViewColumnAtContentX(pData, fContentX);
	iRow = __xuiTableViewRowAtContentY(pData, fContentY);
	if ( (iRow < 0) || (iColumn < 0) ) return XUI_OK;
	(void)__xuiTableViewMergeOwner(pWidget, pData, iRow, iColumn, &iOwnerRow, &iOwnerColumn);
	if ( pRow != NULL ) *pRow = iOwnerRow;
	if ( pColumn != NULL ) *pColumn = iOwnerColumn;
	return XUI_OK;
}

XUI_API int xuiTableViewSetColors(xui_widget pWidget, uint32_t iBackground, uint32_t iHeader, uint32_t iRow, uint32_t iSelected, uint32_t iGrid, uint32_t iText)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iBackgroundColor = iBackground;
	pData->iHeaderColor = iHeader;
	pData->iHeaderTextColor = iText;
	pData->iRowColor = iRow;
	pData->iAltRowColor = XUI_COLOR_RGBA(249, 252, 255, 255);
	pData->iHoverColor = XUI_COLOR_RGBA(231, 243, 253, 255);
	pData->iSelectedColor = iSelected;
	pData->iGridColor = iGrid;
	pData->iTextColor = iText;
	return __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTableViewSetColorStyle(xui_widget pWidget, const xui_table_view_colors_t* pColors)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( (pData == NULL) || (pColors == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iBackgroundColor = pColors->iBackgroundColor;
	pData->iHeaderColor = pColors->iHeaderColor;
	pData->iHeaderTextColor = pColors->iHeaderTextColor;
	pData->iRowColor = pColors->iRowColor;
	pData->iAltRowColor = pColors->iAltRowColor;
	pData->iHoverColor = pColors->iHoverColor;
	pData->iSelectedColor = pColors->iSelectedColor;
	pData->iDisabledColor = pColors->iDisabledColor;
	pData->iGridColor = pColors->iGridColor;
	pData->iTextColor = pColors->iTextColor;
	pData->iDisabledTextColor = pColors->iDisabledTextColor;
	pData->iFocusRingColor = pColors->iFocusRingColor;
	pData->iBarColor = pColors->iBarColor;
	pData->iThumbColor = pColors->iThumbColor;
	if ( pData->pFrame != NULL ) {
		(void)xuiScrollFrameSetColors(pData->pFrame,
			pData->iBarColor,
			pData->iThumbColor,
			pData->iScrollbarHoverColor,
			pData->iScrollbarActiveColor,
			pData->iScrollbarFocusColor,
			pData->iScrollbarDisabledColor);
	}
	return __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTableViewGetColors(xui_widget pWidget, xui_table_view_colors_t* pColors)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( (pData == NULL) || (pColors == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(pColors, 0, sizeof(*pColors));
	pColors->iBackgroundColor = pData->iBackgroundColor;
	pColors->iHeaderColor = pData->iHeaderColor;
	pColors->iHeaderTextColor = pData->iHeaderTextColor;
	pColors->iRowColor = pData->iRowColor;
	pColors->iAltRowColor = pData->iAltRowColor;
	pColors->iHoverColor = pData->iHoverColor;
	pColors->iSelectedColor = pData->iSelectedColor;
	pColors->iDisabledColor = pData->iDisabledColor;
	pColors->iGridColor = pData->iGridColor;
	pColors->iTextColor = pData->iTextColor;
	pColors->iDisabledTextColor = pData->iDisabledTextColor;
	pColors->iFocusRingColor = pData->iFocusRingColor;
	pColors->iBarColor = pData->iBarColor;
	pColors->iThumbColor = pData->iThumbColor;
	return XUI_OK;
}

XUI_API int xuiTableViewSetDisabledTextColor(xui_widget pWidget, uint32_t iColor)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iDisabledTextColor = iColor;
	return __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTableViewSetScrollbarColors(xui_widget pWidget, uint32_t iTrack, uint32_t iThumb, uint32_t iHover, uint32_t iActive, uint32_t iFocus, uint32_t iDisabled)
{
	xui_table_view_data_t* pData;
	int iRet;

	pData = __xuiTableViewGetData(pWidget);
	if ( (pData == NULL) || (pData->pFrame == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iBarColor = iTrack;
	pData->iThumbColor = iThumb;
	pData->iScrollbarHoverColor = iHover;
	pData->iScrollbarActiveColor = iActive;
	pData->iScrollbarFocusColor = iFocus;
	pData->iScrollbarDisabledColor = iDisabled;
	iRet = xuiScrollFrameSetColors(pData->pFrame, iTrack, iThumb, iHover, iActive, iFocus, iDisabled);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTableViewGetScrollbarColors(xui_widget pWidget, uint32_t* pTrack, uint32_t* pThumb, uint32_t* pHover, uint32_t* pActive, uint32_t* pFocus, uint32_t* pDisabled)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pTrack != NULL ) *pTrack = pData->iBarColor;
	if ( pThumb != NULL ) *pThumb = pData->iThumbColor;
	if ( pHover != NULL ) *pHover = pData->iScrollbarHoverColor;
	if ( pActive != NULL ) *pActive = pData->iScrollbarActiveColor;
	if ( pFocus != NULL ) *pFocus = pData->iScrollbarFocusColor;
	if ( pDisabled != NULL ) *pDisabled = pData->iScrollbarDisabledColor;
	return XUI_OK;
}

XUI_API int xuiTableViewRefresh(xui_widget pWidget)
{
	xui_table_view_data_t* pData;
	int iRet;

	pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiTableViewUpdateContentSize(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	pData->iChangeCount++;
	return __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTableViewEnsureCellVisible(xui_widget pWidget, int iRow, int iColumn)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiTableViewEnsureVisibleInternal(pWidget, pData, iRow, iColumn);
}

XUI_API int xuiTableViewGetSortColumn(xui_widget pWidget, int* pDescending)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return -1;
	if ( pDescending != NULL ) *pDescending = pData->bSortDescending;
	return pData->iSortColumn;
}

XUI_API int xuiTableViewGetSelectCount(xui_widget pWidget)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	return (pData != NULL) ? pData->iSelectCount : 0;
}

XUI_API int xuiTableViewGetSortCount(xui_widget pWidget)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	return (pData != NULL) ? pData->iSortCount : 0;
}

XUI_API int xuiTableViewGetHoverCount(xui_widget pWidget)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	return (pData != NULL) ? pData->iHoverCount : 0;
}

XUI_API int xuiTableViewGetColumnResizeCount(xui_widget pWidget)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	return (pData != NULL) ? pData->iColumnResizeCount : 0;
}

XUI_API int xuiTableViewGetChangeCount(xui_widget pWidget)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}
