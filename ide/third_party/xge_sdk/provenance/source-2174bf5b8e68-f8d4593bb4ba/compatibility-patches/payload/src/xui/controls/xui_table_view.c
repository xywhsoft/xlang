#include "xui_config.h"
#if XUI_ENABLE_TABLE_VIEW
#include "xui_internal.h"
#include "xui_callback_hold_internal.h"
#include "xui_table_view_paint.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define XUI_TABLE_VIEW_DEFAULT_COLUMN_WIDTH 80.0f
#define XUI_TABLE_VIEW_DEFAULT_ROW_HEIGHT 20.0f
#define XUI_TABLE_VIEW_DEFAULT_HEADER_HEIGHT 24.0f
#define XUI_TABLE_VIEW_DEFAULT_WIDTH 420.0f
#define XUI_TABLE_VIEW_DEFAULT_ROWS 8

#ifndef XUI_TABLE_VIEW_INDEX_STEP
#define XUI_TABLE_VIEW_INDEX_STEP(kind) ((void)0)
#endif

typedef struct xui_table_view_row_prefix_t {
	double fExplicitHeight;
	int iDefaultCount;
} xui_table_view_row_prefix_t;

typedef struct xui_table_view_merge_band_t {
	int iStart;
	int iEnd;
	int iMerge;
} xui_table_view_merge_band_t;

typedef struct xui_table_view_merge_column_t {
	xui_table_view_merge_band_t* arrBands;
	int iCount;
	int iCapacity;
} xui_table_view_merge_column_t;

typedef struct xui_table_view_merge_t {
	int iRow;
	int iColumn;
	int bPaintable;
	uint64_t iPaintSerial;
} xui_table_view_merge_t;

typedef struct xui_table_view_cell_read_t {
	int iRow;
	int iColumn;
	struct xui_table_view_cell_read_t* pPrevious;
} xui_table_view_cell_read_t;

typedef struct xui_table_view_count_read_t {
	xui_table_view_count_proc onCount;
	void* pUser;
	struct xui_table_view_count_read_t* pPrevious;
} xui_table_view_count_read_t;

typedef struct xui_table_view_cell_lease_t {
    xui_table_view_cell_output_t output;
    xui_callback_snapshot_t provider;
    xui_table_cell_format_t format;
    int failure;
    xui_context context;
    bool owned,reading,textReady;
} xui_table_view_cell_lease_t;

typedef struct xui_table_column_storage_t {
    size_t references;
    size_t bytes;
    xui_table_view_column_t columns[];
} xui_table_column_storage_t;

static int __xuiTableColumnStorageRetain(xui_table_column_storage_t* storage)
{
    if (!storage) return XUI_OK;
    if (storage->references == (size_t)-1) return XUI_ERROR_LIMIT_EXCEEDED;
    ++storage->references;
    return XUI_OK;
}
static void __xuiTableColumnStorageRelease(xui_table_column_storage_t* storage)
{
    if (storage && --storage->references == 0) xrtFree(storage);
}
static int __xuiTableColumnStorageAliases(const xui_table_column_storage_t* storage,
    const xui_table_view_column_t* columns, int count)
{
    if (!storage) return 0;
    uintptr_t start = (uintptr_t)storage;
    for (int i = 0; i < count; ++i) {
        uintptr_t id = (uintptr_t)columns[i].sId;
        uintptr_t title = (uintptr_t)columns[i].sTitle;
        if ((id >= start && id - start < storage->bytes) ||
            (title >= start && title - start < storage->bytes)) return 1;
    }
    return 0;
}

typedef struct xui_table_view_data_t {
	xui_widget pFrame;
	xui_widget pViewport;
	xui_table_view_column_t arrColumns[XUI_TABLE_VIEW_COLUMN_CAPACITY];
    xui_table_column_storage_t* pColumnStorage;
	xui_table_view_row_t* arrRows;
	xui_table_view_row_prefix_t* arrRowPrefix;
	xui_table_view_merge_column_t arrMergeColumns[XUI_TABLE_VIEW_COLUMN_CAPACITY];
	xui_table_view_merge_t* arrMerges;
	int iMergeCount;
	int iMergeCapacity;
	int iMergeIndexedRows;
	int bMergeBuilding;
	uint64_t iMergeGeneration;
	uint64_t iCallbackGeneration;
	xui_table_view_count_read_t* pCountRead;
	xui_table_view_cell_read_t* pCellRead;
	uint64_t iPaintSerial;
	int iColumnCount;
	int iRowCount;
	int iRowStateCount;
	xui_font pFont;
	xui_table_view_count_proc onCount;
	xui_table_view_cell_proc onCell;
	void* pAdapterUser;
    xui_table_view_count_proc onCountOwned;
    xui_table_view_cell_owned_proc onCellOwned;
    void* pCountOwnedUser;
    void* pCellOwnedUser;
    bool bAdapterOwned;
    uint64_t iAdapterPublication;
    char* sResolvedTooltip;
	xui_table_view_sort_proc onSort;
	void* pSortUser;
	xui_table_view_select_proc onSelect;
	void* pSelectUser;
	xui_table_view_column_resize_proc onColumnResize;
	void* pColumnResizeUser;
	xui_table_view_hover_proc onHover;
	void* pHoverUser;
	xui_table_view_context_proc onContext;
	void* pContextUser;
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
	int iActiveHeader;
	uint64_t iActivePointerId;
	int iActivePointerType;
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
	float fBaseDefaultColumnWidth;
	float fBaseDefaultRowHeight;
	float fBaseHeaderHeight;
	float fBaseBorderWidth;
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
	uint32_t iSelectedTextColor;
	uint32_t iCheckBackgroundColor;
	uint32_t iCheckMarkColor;
	uint32_t iCheckerPrimaryColor;
	uint32_t iCheckerSecondaryColor;
	uint32_t iColorFallbackColor;
	uint32_t iInvalidColor;
	uint32_t iDirtyColor;
	uint32_t iEditingColor;
	uint32_t iPickerBackgroundColor;
	uint32_t iPickerBorderColor;
	uint32_t iPreparedStyleHash;
	int bPaintStylePrepared;
	char sAccessibleName[256];
	xui_table_view_paint_adapter_t tPaintAdapter;
} xui_table_view_data_t;

/* Typed native snapshot, retained through the callback's physical return.
 * Scalar arguments and caller-owned hit snapshots precede reentrant Hold. */
static int __xuiOwnedInvoke_xuiTableViewSetMergeProvider(xui_widget widget,xui_table_view_data_t* data,xui_table_view_merge_proc selected,void* user,int iRow,int iColumn,int* pRowSpan,int* pColSpan)
{
    if(!widget->pEventOwners || !widget->pEventOwners->pBindings)return selected(widget,iRow,iColumn,pRowSpan,pColSpan,user);
    xui_callback_snapshot_t snapshot={0};
    int status=xuiInternalCallbackSnapshotBeginReturned(widget,&data->onMerge,&user,&snapshot);
    if(status!=XUI_OK)return status;
    int value=selected(widget,iRow,iColumn,pRowSpan,pColSpan,user);
    status=value<0 ? value:XUI_OK;
    xerror* first=status!=XUI_OK ? xrtTakeError():NULL;
    int ended=xuiInternalCallbackSnapshotEndOutcome(&snapshot,status,first);
    return ended!=XUI_OK ? ended:value;
}


/* Typed native snapshot, retained through the callback's physical return.
 * Scalar arguments and caller-owned hit snapshots precede reentrant Hold. */
static int __xuiOwnedInvoke_xuiTableViewSetContextMenu(xui_widget widget,xui_table_view_data_t* data,xui_table_view_context_proc selected,void* user,int iPart,int iRow,int iColumn,float fX,float fY)
{
    if(!widget->pEventOwners || !widget->pEventOwners->pBindings)return selected(widget,iPart,iRow,iColumn,fX,fY,user);
    xui_callback_snapshot_t snapshot={0};
    int status=xuiInternalCallbackSnapshotBeginReported(widget,&data->onContext,&user,&snapshot);
    if(status!=XUI_OK)return status;
    int value=selected(widget,iPart,iRow,iColumn,fX,fY,user);
    status=value<0 ? value:XUI_OK;
    xerror* first=status!=XUI_OK ? xrtTakeError():NULL;
    int ended=xuiInternalCallbackSnapshotEndOutcome(&snapshot,status,first);
    return ended!=XUI_OK ? ended:value;
}


static xui_table_view_data_t* __xuiTableViewGetData(xui_widget pWidget);
static void __xuiTableViewCancelPointer(xui_widget pWidget,
	xui_table_view_data_t* pData, int bReleaseCapture);
static float __xuiTableViewRowHeight(const xui_table_view_data_t* pData, int iRow);
static float __xuiTableViewRowTop(const xui_table_view_data_t* pData, int iRow);
static void __xuiTableViewLeaseOutcome(int next,int* result,xerror** first);
static int __xuiTableViewLeaseEnd(xui_table_view_cell_lease_t* lease, int result);
static int __xuiTableViewGetCell(xui_widget pWidget, xui_table_view_data_t* pData, int iRow, int iColumn, xui_table_view_cell_t* pCell, xui_table_view_cell_lease_t* pLease);
static xui_rect_t __xuiTableViewCellContentRectData(xui_table_view_data_t* pData, int iRow, int iColumn, int iRowSpan, int iColSpan);
static int __xuiTableViewCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser);
static int __xuiTableViewViewportRender(xui_widget pViewport, xui_draw_context pDraw, uint32_t iStateId, void* pUser);

/* The enclosing operation keeps storage alive; generations reject stale callback results. */
static int __xuiTableViewCallbackCurrent(xui_widget pWidget, const xui_table_view_data_t* pData, uint64_t iGeneration)
{
	return xuiInternalWidgetIsValid(pWidget) &&
		!xuiInternalContextDestroyPending(xuiWidgetGetContext(pWidget)) &&
		pData->iCallbackGeneration == iGeneration;
}

static int __xuiTableViewFloatValid(float fValue)
{
	return (fValue == fValue) && (fValue >= 0.0f) && (fValue <= XUI_LAYOUT_UNBOUNDED);
}

static int __xuiTableViewWidgetAlive(xui_widget pWidget, xui_table_view_data_t* pData)
{
	xui_context pContext;

	if ( !xuiInternalWidgetIsValid(pWidget) ) return 0;
	pContext = xuiWidgetGetContext(pWidget);
	return xuiInternalContextIsValid(pContext) && !xuiInternalContextDestroyPending(pContext) &&
		__xuiTableViewGetData(pWidget) == pData;
}

static int __xuiTableViewColumnDescValid(const xui_table_view_column_t* pColumn)
{
	if ( pColumn == NULL || !__xuiTableViewFloatValid(pColumn->fWidth) ||
	     !__xuiTableViewFloatValid(pColumn->fMinWidth) ||
	     !__xuiTableViewFloatValid(pColumn->fMaxWidth) ) return 0;
	if ( pColumn->fMaxWidth > 0.0f && pColumn->fMinWidth > pColumn->fMaxWidth ) return 0;
	return 1;
}

static int __xuiTableViewRowDescValid(const xui_table_view_row_t* pRow)
{
	return pRow != NULL && __xuiTableViewFloatValid(pRow->fHeight);
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
	     (pDesc->iColumnCount > XUI_TABLE_VIEW_COLUMN_CAPACITY) ||
	     ((pDesc->iColumnCount > 0) && (pDesc->arrColumns == NULL)) ||
	     !__xuiTableViewSelectionModeValid(pDesc->iSelectionMode) ||
	     !__xuiTableViewScrollbarModeValid(pDesc->iScrollbarMode) ) {
		return 0;
	}
	if ( ((pDesc->fDefaultColumnWidth != 0.0f) && !__xuiTableViewFloatValid(pDesc->fDefaultColumnWidth)) ||
	     ((pDesc->fDefaultRowHeight != 0.0f) && !__xuiTableViewFloatValid(pDesc->fDefaultRowHeight)) ||
	     ((pDesc->fHeaderHeight != 0.0f) && !__xuiTableViewFloatValid(pDesc->fHeaderHeight)) ) {
		return 0;
	}
	{
		int i;
		for ( i = 0; i < pDesc->iColumnCount; i++ ) {
			if ( !__xuiTableViewColumnDescValid(&pDesc->arrColumns[i]) ) return 0;
		}
		if ( pDesc->arrRows != NULL ) {
			for ( i = 0; i < pDesc->iRowCount; i++ ) {
				if ( !__xuiTableViewRowDescValid(&pDesc->arrRows[i]) ) return 0;
			}
		}
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
	pData->iActiveHeader = -1;
	pData->iActivePointerId = XUI_POINTER_ID_MOUSE;
	pData->iActivePointerType = XUI_POINTER_TYPE_MOUSE;
	pData->iSortColumn = -1;
	pData->iResizeColumn = -1;
	pData->fDefaultColumnWidth = XUI_TABLE_VIEW_DEFAULT_COLUMN_WIDTH;
	pData->fDefaultRowHeight = XUI_TABLE_VIEW_DEFAULT_ROW_HEIGHT;
	pData->fHeaderHeight = XUI_TABLE_VIEW_DEFAULT_HEADER_HEIGHT;
	pData->fBorderWidth = 1.0f;
	pData->fBaseDefaultColumnWidth = pData->fDefaultColumnWidth;
	pData->fBaseDefaultRowHeight = pData->fDefaultRowHeight;
	pData->fBaseHeaderHeight = pData->fHeaderHeight;
	pData->fBaseBorderWidth = pData->fBorderWidth;
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
	pData->iSelectedTextColor = XUI_COLOR_WHITE;
	pData->iCheckBackgroundColor = XUI_COLOR_WHITE;
	pData->iCheckMarkColor = XUI_COLOR_WHITE;
	pData->iCheckerPrimaryColor = XUI_COLOR_RGBA(236, 241, 247, 255);
	pData->iCheckerSecondaryColor = XUI_COLOR_WHITE;
	pData->iColorFallbackColor = XUI_COLOR_RGBA(120, 160, 200, 255);
	pData->iInvalidColor = XUI_COLOR_RGBA(218, 82, 82, 255);
	pData->iDirtyColor = XUI_COLOR_RGBA(245, 158, 11, 255);
	pData->iEditingColor = XUI_COLOR_RGBA(34, 160, 112, 255);
	pData->iPickerBackgroundColor = XUI_COLOR_RGBA(232, 241, 250, 220);
	pData->iPickerBorderColor = XUI_COLOR_RGBA(206, 221, 235, 220);
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
	if ( pDesc->fDefaultColumnWidth > 0.0f ) pData->fBaseDefaultColumnWidth = pDesc->fDefaultColumnWidth;
	if ( pDesc->fDefaultRowHeight > 0.0f ) pData->fBaseDefaultRowHeight = pDesc->fDefaultRowHeight;
	if ( pDesc->fHeaderHeight > 0.0f ) pData->fBaseHeaderHeight = pDesc->fHeaderHeight;
	pData->fDefaultColumnWidth = pData->fBaseDefaultColumnWidth;
	pData->fDefaultRowHeight = pData->fBaseDefaultRowHeight;
	pData->fHeaderHeight = pData->fBaseHeaderHeight;
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
	if ( (pData == NULL) || (pOut == NULL) ) {
		return;
	}
	*pOut = *pData;
	if ( pWidget == NULL ) return;
	pOut->fDefaultColumnWidth = pData->fBaseDefaultColumnWidth;
	pOut->fDefaultRowHeight = pData->fBaseDefaultRowHeight;
	pOut->fHeaderHeight = pData->fBaseHeaderHeight;
	pOut->fBorderWidth = pData->fBaseBorderWidth;
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
	/* The owner has already resolved its role palette and any child overrides. */
	if ( pData->tPaintAdapter.bHasBackground ) pOut->iBackgroundColor = pData->tPaintAdapter.iBackgroundColor;
	if ( pData->tPaintAdapter.bHasGrid ) pOut->iGridColor = pData->tPaintAdapter.iGridColor;
	pOut->iPickerBackgroundColor = __xuiTableViewColorWithAlpha(pOut->iHeaderColor, (uint8_t)(__xuiTableViewAlpha(pOut->iHeaderColor) * 220 / 255));
	pOut->iPickerBorderColor = __xuiTableViewColorWithAlpha(pOut->iGridColor, (uint8_t)(__xuiTableViewAlpha(pOut->iGridColor) * 220 / 255));
	(void)__xuiTableViewStyleColor(pWidget, "tableview.text.selected_color", &pOut->iSelectedTextColor);
	(void)__xuiTableViewStyleColor(pWidget, "tableview.check.background_color", &pOut->iCheckBackgroundColor);
	(void)__xuiTableViewStyleColor(pWidget, "tableview.check.mark_color", &pOut->iCheckMarkColor);
	(void)__xuiTableViewStyleColor(pWidget, "tableview.checker.primary_color", &pOut->iCheckerPrimaryColor);
	(void)__xuiTableViewStyleColor(pWidget, "tableview.checker.secondary_color", &pOut->iCheckerSecondaryColor);
	(void)__xuiTableViewStyleColor(pWidget, "tableview.color.fallback_color", &pOut->iColorFallbackColor);
	(void)__xuiTableViewStyleColor(pWidget, "tableview.cell.invalid_color", &pOut->iInvalidColor);
	(void)__xuiTableViewStyleColor(pWidget, "tableview.cell.dirty_color", &pOut->iDirtyColor);
	(void)__xuiTableViewStyleColor(pWidget, "tableview.cell.editing_color", &pOut->iEditingColor);
	(void)__xuiTableViewStyleColor(pWidget, "tableview.picker.background_color", &pOut->iPickerBackgroundColor);
	(void)__xuiTableViewStyleColor(pWidget, "tableview.picker.border_color", &pOut->iPickerBorderColor);
	(void)__xuiTableViewStyleColor(pWidget, "tableview.scrollbar.track_color", &pOut->iBarColor);
	(void)__xuiTableViewStyleColor(pWidget, "tableview.scrollbar.thumb_color", &pOut->iThumbColor);
	(void)__xuiTableViewStyleColor(pWidget, "tableview.scrollbar.hover_color", &pOut->iScrollbarHoverColor);
	(void)__xuiTableViewStyleColor(pWidget, "tableview.scrollbar.active_color", &pOut->iScrollbarActiveColor);
	(void)__xuiTableViewStyleColor(pWidget, "tableview.scrollbar.focus_color", &pOut->iScrollbarFocusColor);
	(void)__xuiTableViewStyleColor(pWidget, "tableview.scrollbar.disabled_color", &pOut->iScrollbarDisabledColor);
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

int xuiInternalTableViewSetPaintAdapter(xui_widget pWidget, const xui_table_view_paint_adapter_t* pAdapter)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	xui_table_view_paint_adapter_t tAdapter = {0};
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pAdapter != NULL ) tAdapter = *pAdapter;
	if ( pData->tPaintAdapter.bHasBackground == tAdapter.bHasBackground &&
	     pData->tPaintAdapter.iBackgroundColor == tAdapter.iBackgroundColor &&
	     pData->tPaintAdapter.bHasGrid == tAdapter.bHasGrid &&
	     pData->tPaintAdapter.iGridColor == tAdapter.iGridColor &&
	     pData->tPaintAdapter.bCellColorsPresent == tAdapter.bCellColorsPresent &&
	     pData->tPaintAdapter.onCellBackground == tAdapter.onCellBackground &&
	     pData->tPaintAdapter.onCellDecorations == tAdapter.onCellDecorations &&
	     pData->tPaintAdapter.pUser == tAdapter.pUser ) return XUI_OK;
	pData->tPaintAdapter = tAdapter;
	pData->iCallbackGeneration++;
	return __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
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
    xui_table_view_cell_lease_t lease={0};
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
	if ( __xuiTableViewGetCell(pWidget, pData, iRow, iColumn, &tCell,&lease) != XUI_OK ) return XUI_ERROR_INVALID_STATE;
	tContent = __xuiTableViewCellContentRectData(pData, iRow, iColumn, tCell.iRowSpan, tCell.iColSpan);
	tLocal.fX = tContent.fX - fOffsetX;
	tLocal.fY = tContent.fY - fOffsetY;
	tLocal.fW = tContent.fW;
	tLocal.fH = tContent.fH;
    int ended=__xuiTableViewLeaseEnd(&lease,XUI_OK);
    if(ended!=XUI_OK)return ended;
    if(!__xuiTableViewWidgetAlive(pWidget,pData))return XUI_ERROR_INVALID_STATE;
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

static void __xuiTableViewInvalidateMerges(xui_table_view_data_t* pData)
{
	int i;
	pData->iMergeGeneration++;
	pData->iCallbackGeneration++;
	pData->iMergeIndexedRows = 0;
	pData->iMergeCount = 0;
	for ( i = 0; i < XUI_TABLE_VIEW_COLUMN_CAPACITY; i++ ) {
		pData->arrMergeColumns[i].iCount = 0;
	}
}

static double __xuiTableViewContentWidthFor(const xui_table_view_column_t* arrColumns,
	int iColumnCount, float fDefaultColumnWidth)
{
	double fWidth = 0.0;
	int i;

	if ( arrColumns == NULL || iColumnCount <= 0 ) return 0.0;
	for ( i = 0; i < iColumnCount; i++ ) {
		float fColumnWidth;
		if ( arrColumns[i].bVisible == 0 ) continue;
		fColumnWidth = arrColumns[i].fWidth;
		if ( fColumnWidth <= 0.0f ) fColumnWidth = fDefaultColumnWidth;
		fWidth += (double)fColumnWidth;
	}
	return fWidth;
}

static double __xuiTableViewContentHeightFor(const xui_table_view_row_prefix_t* arrPrefix,
	int iRowStateCount, int iRowCount, float fDefaultRowHeight)
{
	double fHeight = 0.0;
	int iExplicitRows;
	int iDefaultRows;

	if ( iRowCount <= 0 ) return 0.0;
	iExplicitRows = (iRowCount < iRowStateCount) ? iRowCount : iRowStateCount;
	iDefaultRows = iRowCount - iExplicitRows;
	if ( iExplicitRows > 0 && arrPrefix != NULL ) {
		fHeight = arrPrefix[iExplicitRows].fExplicitHeight;
		iDefaultRows += arrPrefix[iExplicitRows].iDefaultCount;
	}
	return fHeight + (double)iDefaultRows * (double)fDefaultRowHeight;
}

static int __xuiTableViewDimensionsValid(const xui_table_view_column_t* arrColumns,
	int iColumnCount, float fDefaultColumnWidth,
	const xui_table_view_row_prefix_t* arrPrefix, int iRowStateCount,
	int iRowCount, float fDefaultRowHeight)
{
	double fWidth;
	double fHeight;

	if ( !__xuiTableViewFloatValid(fDefaultColumnWidth) || fDefaultColumnWidth <= 0.0f ||
	     !__xuiTableViewFloatValid(fDefaultRowHeight) || fDefaultRowHeight <= 0.0f ||
	     iColumnCount < 0 || iRowStateCount < 0 || iRowCount < 0 ) return 0;
	fWidth = __xuiTableViewContentWidthFor(arrColumns, iColumnCount, fDefaultColumnWidth);
	fHeight = __xuiTableViewContentHeightFor(arrPrefix, iRowStateCount, iRowCount, fDefaultRowHeight);
	return fWidth >= 0.0 && fWidth <= (double)XUI_LAYOUT_UNBOUNDED &&
		fHeight >= 0.0 && fHeight <= (double)XUI_LAYOUT_UNBOUNDED;
}

static int __xuiTableViewSyncRowCount(xui_widget pWidget, xui_table_view_data_t* pData)
{
	int iCount;
	uint64_t iGeneration;
	xui_table_view_count_read_t tRead;
	xui_table_view_count_read_t* pRead;

	if ( pData == NULL ) {
		return 0;
	}
	if ( (pData->bAdapterOwned ? pData->onCountOwned:pData->onCount) != NULL ) {
        bool ownedCount=pData->bAdapterOwned;
		xui_table_view_count_proc selected=ownedCount ? pData->onCountOwned:pData->onCount;
        void* selectedUser=pData->bAdapterOwned ? pData->pCountOwnedUser:pData->pAdapterUser;
		/* Recursive reads use the committed count; a newly bound provider may run immediately. */
		for ( pRead = pData->pCountRead; pRead != NULL; pRead = pRead->pPrevious ) {
			if ( pRead->onCount == selected && pRead->pUser == selectedUser ) return pData->iRowCount;
		}
		iGeneration = pData->iCallbackGeneration;
		if ( !__xuiTableViewCallbackCurrent(pWidget, pData, iGeneration) ) return XUI_ERROR_INVALID_STATE;
		tRead = (xui_table_view_count_read_t){selected, selectedUser, pData->pCountRead};
		pData->pCountRead = &tRead;
		xui_callback_snapshot_t snapshot={0};
        int status=pData->bAdapterOwned ? xuiInternalCallbackSnapshotBeginReturned(pWidget,&pData->onCountOwned,&selectedUser,&snapshot):XUI_OK;
        if(status==XUI_OK && !__xuiTableViewCallbackCurrent(pWidget,pData,iGeneration))status=XUI_ERROR_INVALID_STATE;
        iCount=status==XUI_OK ? selected(pWidget,selectedUser):0;
        pData->pCountRead=tRead.pPrevious;
        if(status==XUI_OK && ownedCount && iCount<0)status=iCount;
        xerror* first=status!=XUI_OK ? xrtTakeError():NULL;
        status=xuiInternalCallbackSnapshotEndOutcome(&snapshot,status,first);
        if(status!=XUI_OK)return status;
		pData->pCountRead = tRead.pPrevious;
		if ( !__xuiTableViewCallbackCurrent(pWidget, pData, iGeneration) ) return XUI_ERROR_INVALID_STATE;
		if ( iCount < 0 ) iCount = 0;
	} else {
		iCount = pData->iRowStateCount;
	}
	if ( !__xuiTableViewDimensionsValid(pData->arrColumns, pData->iColumnCount,
	     pData->fDefaultColumnWidth, pData->arrRowPrefix, pData->iRowStateCount,
	     iCount, pData->fDefaultRowHeight) ) return XUI_ERROR_INVALID_STATE;
	if ( pData->iRowCount != iCount ) {
		if ( pData->iActiveRow >= iCount ) __xuiTableViewCancelPointer(pWidget, pData, 1);
		pData->iRowCount = iCount;
		__xuiTableViewInvalidateMerges(pData);
		xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_TREE_CHANGED);
		xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_BOUNDS_CHANGED);
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

	XUI_TABLE_VIEW_INDEX_STEP(ROW_READ);

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

static double __xuiTableViewRowOffset(const xui_table_view_data_t* pData, int iRow)
{
	if ( (pData == NULL) || (iRow <= 0) ) {
		return 0.0;
	}
	if ( iRow > pData->iRowCount ) iRow = pData->iRowCount;
	/* Default-height rows remain parametric, including an adapter-only tail. */
	return __xuiTableViewContentHeightFor(pData->arrRowPrefix, pData->iRowStateCount,
		iRow, (pData->fDefaultRowHeight > 0.0f) ? pData->fDefaultRowHeight : XUI_TABLE_VIEW_DEFAULT_ROW_HEIGHT);
}

static float __xuiTableViewRowTop(const xui_table_view_data_t* pData, int iRow)
{
	return (float)__xuiTableViewRowOffset(pData, iRow);
}

static float __xuiTableViewContentHeight(xui_widget pWidget, xui_table_view_data_t* pData)
{
	int iCount = __xuiTableViewSyncRowCount(pWidget, pData);
	return (iCount < 0) ? -1.0f : __xuiTableViewRowTop(pData, iCount);
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
	int iLow;
	int iHigh;
	int iMid;

	if ( (pData == NULL) || !(fContentY >= 0.0f) ||
	     ((double)fContentY >= __xuiTableViewRowOffset(pData, pData->iRowCount)) ) {
		return -1;
	}
	iLow = 0;
	iHigh = pData->iRowCount;
	while ( iLow < iHigh ) {
		XUI_TABLE_VIEW_INDEX_STEP(ROW_PROBE);
		iMid = iLow + (iHigh - iLow) / 2;
		if ( __xuiTableViewRowOffset(pData, iMid + 1) <= (double)fContentY ) {
			iLow = iMid + 1;
		} else {
			iHigh = iMid;
		}
	}
	return iLow;
}

static int __xuiTableViewRowEndAtContentY(const xui_table_view_data_t* pData, float fContentY)
{
	int iLow = 0;
	int iHigh = pData->iRowCount;
	int iMid;
	while ( iLow < iHigh ) {
		XUI_TABLE_VIEW_INDEX_STEP(ROW_PROBE);
		iMid = iLow + (iHigh - iLow) / 2;
		if ( __xuiTableViewRowOffset(pData, iMid) < (double)fContentY ) iLow = iMid + 1;
		else iHigh = iMid;
	}
	return iLow;
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


static void __xuiTableViewLeaseOutcome(int next,int* result,xerror** first)
{
    xerror* error=xrtTakeError();
    if(next==XUI_OK && !error)return;
    if(*result==XUI_OK){*result=next!=XUI_OK ? next:XUI_ERROR_CALLBACK_FAILED;*first=error;}
    else xrtErrorFree(error);
}
static void __xuiTableViewRecordFailure(xui_context context,int result)
{xuiInternalContextRecordFailure(context,result);}
static int __xuiTableViewLeaseEnd(xui_table_view_cell_lease_t* lease,int result)
{
    xerror* first=result!=XUI_OK ? xrtTakeError():NULL;
    if(lease->format.context){result=xuiInternalTableCellFormatEndOutcome(&lease->format,result,first);first=xrtTakeError();}
    if(lease->reading){lease->reading=false;xuiInternalContextCleanupOutcome(lease->context,xuiInternalTableCellReadEnd(&lease->output),&result,&first);}
    if(lease->owned){lease->owned=false;xuiInternalContextCleanupOutcome(lease->context,xuiTableViewCellOutputDisposeChecked(&lease->output),&result,&first);}
    result=xuiInternalCallbackSnapshotEndOutcome(&lease->provider,result,first);first=xrtTakeError();
    memset(lease,0,sizeof(*lease));
    xrtSetErrorTake(first);return result;
}
static int __xuiTableViewGetCell(xui_widget pWidget,xui_table_view_data_t* pData,int iRow,int iColumn,
    xui_table_view_cell_t* pCell,xui_table_view_cell_lease_t* lease)
{
    xui_table_view_cell_read_t tRead,*pRead;int iRowSpan,iColSpan,iHandled,result=XUI_OK;
    uint64_t generation;
    memset(lease,0,sizeof(*lease));lease->context=xuiWidgetGetContext(pWidget);
    __xuiTableViewDefaultCell(pData,iColumn,pCell);
    if(!pData || !pCell || iRow<0 || iColumn<0)return XUI_ERROR_INVALID_ARGUMENT;
    for(pRead=pData->pCellRead;pRead;pRead=pRead->pPrevious)
        if(pRead->iRow==iRow && pRead->iColumn==iColumn)return XUI_ERROR_INVALID_STATE;
    generation=pData->iCallbackGeneration;
    if(!__xuiTableViewCallbackCurrent(pWidget,pData,generation))return XUI_ERROR_INVALID_STATE;
    tRead=(xui_table_view_cell_read_t){iRow,iColumn,pData->pCellRead};pData->pCellRead=&tRead;
    if(pData->bAdapterOwned){
        lease->owned=true;xuiTableViewCellOutputInit(&lease->output);lease->output.iType=pCell->iType;
        xui_table_view_cell_owned_proc selected=pData->onCellOwned;void* user=pData->pCellOwnedUser;
        if(selected){
            result=xuiInternalCallbackSnapshotBeginReturned(pWidget,&pData->onCellOwned,&user,&lease->provider);
            if(result!=XUI_OK)goto failed;
            if(!__xuiTableViewCallbackCurrent(pWidget,pData,generation)){result=XUI_ERROR_INVALID_STATE;goto failed;}
            result=selected(pWidget,iRow,iColumn,&lease->output,user);
            if(result<0)goto failed;
            if(xrtGetError()){result=XUI_ERROR_CALLBACK_FAILED;goto failed;}
            result=XUI_OK;
        }
        if(!__xuiTableViewCallbackCurrent(pWidget,pData,generation)){result=XUI_ERROR_INVALID_STATE;goto failed;}
        result=xuiInternalTableCellReadBegin(&lease->output);if(result!=XUI_OK)goto failed;
        lease->reading=true;xuiInternalTableCellProject(pCell,&lease->output);
    }else if(pData->onCell){
        (void)pData->onCell(pWidget,iRow,iColumn,pCell,pData->pAdapterUser);
        if(!__xuiTableViewCallbackCurrent(pWidget,pData,generation)){result=XUI_ERROR_INVALID_STATE;goto failed;}
    }
    if(pData->onMerge){
        iRowSpan=pCell->iRowSpan;iColSpan=pCell->iColSpan;
        iHandled=__xuiOwnedInvoke_xuiTableViewSetMergeProvider(pWidget,pData,pData->onMerge,pData->pMergeUser ,iRow,iColumn,&iRowSpan,&iColSpan);
        if(!__xuiTableViewCallbackCurrent(pWidget,pData,generation)){result=XUI_ERROR_INVALID_STATE;goto failed;}
        if(iHandled<0){result=iHandled;goto failed;}
        if(iHandled){pCell->iRowSpan=iRowSpan;pCell->iColSpan=iColSpan;}
    }
    pData->pCellRead=tRead.pPrevious;
    if(pCell->iRowSpan<=0)pCell->iRowSpan=1;
    if(pCell->iColSpan<=0)pCell->iColSpan=1;
    if(pCell->iRowSpan>pData->iRowCount-iRow)pCell->iRowSpan=pData->iRowCount-iRow;
    if(pCell->iColSpan>pData->iColumnCount-iColumn)pCell->iColSpan=pData->iColumnCount-iColumn;
    if(pCell->iRowSpan<=0)pCell->iRowSpan=1;
    if(pCell->iColSpan<=0)pCell->iColSpan=1;
    if(lease->owned){lease->output.iRowSpan=pCell->iRowSpan;lease->output.iColSpan=pCell->iColSpan;}
    return XUI_OK;
failed:
    pData->pCellRead=tRead.pPrevious;memset(pCell,0,sizeof(*pCell));return __xuiTableViewLeaseEnd(lease,result);
}

static int __xuiTableViewMergeBandAt(const xui_table_view_merge_column_t* pColumn, int iRow)
{
	int iLow = 0;
	int iHigh = pColumn->iCount;
	int iMid;
	while ( iLow < iHigh ) {
		XUI_TABLE_VIEW_INDEX_STEP(MERGE_PROBE);
		iMid = iLow + (iHigh - iLow) / 2;
		if ( pColumn->arrBands[iMid].iEnd <= iRow ) iLow = iMid + 1;
		else iHigh = iMid;
	}
	return iLow;
}

static void* __xuiTableViewGrowIndex(void* pItems, int* pCapacity, int iCount, size_t iItemSize)
{
	void* pNew;
	int iCapacity;
	if ( iCount < *pCapacity ) return pItems;
	if ( iCount == INT_MAX ) return NULL;
	iCapacity = (*pCapacity == 0) ? 16 : ((*pCapacity > INT_MAX / 2) ? INT_MAX : *pCapacity * 2);
	if ( (size_t)iCapacity > (size_t)-1 / iItemSize ) return NULL;
	pNew = xrtRealloc(pItems, (size_t)iCapacity * iItemSize);
	if ( pNew != NULL ) *pCapacity = iCapacity;
	return pNew;
}

static int __xuiTableViewIndexMerges(xui_widget pWidget, xui_table_view_data_t* pData, int iLastRow)
{
	xui_table_view_cell_t tCell;
    xui_table_view_cell_lease_t lease={0};
	xui_table_view_merge_column_t* pColumn;
	xui_table_view_merge_band_t* pBands;
	xui_table_view_merge_t* pMerges;
	uint64_t iGeneration;
	int iBand;
	int iStart;
	int iEnd;
	int bPaintable;
	int bContributes;
	int r;
	int c;
	int j;
	int iRet = XUI_OK;

	if ( (pData->onMerge == NULL && pData->onCell == NULL && !pData->bAdapterOwned) || iLastRow < pData->iMergeIndexedRows ) return XUI_OK;
	if ( pData->bMergeBuilding || pData->pCellRead != NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bMergeBuilding = 1;
	iGeneration = pData->iMergeGeneration;
	/* Either callback can supply spans. Discover each row once per data generation;
	 * callers must Refresh after changing adapter spans without a count change. */
	for ( r = pData->iMergeIndexedRows; r <= iLastRow; r++ ) {
		for ( c = 0; c < pData->iColumnCount; c++ ) {
			iRet = __xuiTableViewGetCell(pWidget, pData, r, c, &tCell,&lease);
            iRet=__xuiTableViewLeaseEnd(&lease,iRet);
			if ( iRet != XUI_OK ) goto done;
			if ( iGeneration != pData->iMergeGeneration ) {
				iRet = XUI_ERROR_INVALID_STATE;
				goto done;
			}
			if ( tCell.iRowSpan <= 1 && tCell.iColSpan <= 1 ) continue;
			iEnd = r + tCell.iRowSpan;
			bContributes = 0;
			for ( j = c; j < c + tCell.iColSpan; j++ ) {
				pColumn = &pData->arrMergeColumns[j];
				if ( pColumn->iCount == 0 || pColumn->arrBands[pColumn->iCount - 1].iEnd < iEnd ) bContributes = 1;
			}
			if ( !bContributes ) continue;
			pColumn = &pData->arrMergeColumns[c];
			iBand = __xuiTableViewMergeBandAt(pColumn, r);
			bPaintable = (iBand == pColumn->iCount || pColumn->arrBands[iBand].iStart > r);
			pMerges = (xui_table_view_merge_t*)__xuiTableViewGrowIndex(pData->arrMerges, &pData->iMergeCapacity, pData->iMergeCount, sizeof(*pMerges));
			if ( pMerges == NULL ) { iRet = XUI_ERROR_OUT_OF_MEMORY; goto done; }
			pData->arrMerges = pMerges;
			pMerges[pData->iMergeCount] = (xui_table_view_merge_t){r, c, bPaintable, 0};
			/* Row-major discovery makes only the uncovered tail appendable. Keeping
			 * disjoint bands preserves the original first-owner overlap precedence. */
			for ( j = c; j < c + tCell.iColSpan; j++ ) {
				pColumn = &pData->arrMergeColumns[j];
				iStart = r;
				if ( pColumn->iCount > 0 && pColumn->arrBands[pColumn->iCount - 1].iEnd > iStart ) {
					iStart = pColumn->arrBands[pColumn->iCount - 1].iEnd;
				}
				if ( iStart >= iEnd ) continue;
				pBands = (xui_table_view_merge_band_t*)__xuiTableViewGrowIndex(pColumn->arrBands, &pColumn->iCapacity, pColumn->iCount, sizeof(*pBands));
				if ( pBands == NULL ) { iRet = XUI_ERROR_OUT_OF_MEMORY; goto done; }
				pColumn->arrBands = pBands;
				pBands[pColumn->iCount++] = (xui_table_view_merge_band_t){iStart, iEnd, pData->iMergeCount};
			}
			pData->iMergeCount++;
		}
		pData->iMergeIndexedRows = r + 1;
	}
done:
	pData->bMergeBuilding = 0;
	if ( iRet != XUI_OK && iGeneration == pData->iMergeGeneration ) __xuiTableViewInvalidateMerges(pData);
	return iRet;
}

static int __xuiTableViewMergeOwner(xui_widget pWidget, xui_table_view_data_t* pData, int iRow, int iColumn, int* pOwnerRow, int* pOwnerColumn)
{
	xui_table_view_merge_column_t* pColumn;
	xui_table_view_merge_t* pMerge;
	int iBand;
	int iRet;
	if ( pOwnerRow != NULL ) *pOwnerRow = iRow;
	if ( pOwnerColumn != NULL ) *pOwnerColumn = iColumn;
	if ( (pData == NULL) || (iRow < 0) || (iColumn < 0) ||
	     (iRow >= pData->iRowCount) || (iColumn >= pData->iColumnCount) ||
	     (pData->onMerge == NULL && pData->onCell == NULL && !pData->bAdapterOwned) ) return 0;
	iRet = __xuiTableViewIndexMerges(pWidget, pData, iRow);
	if ( iRet != XUI_OK ) return iRet;
	pColumn = &pData->arrMergeColumns[iColumn];
	iBand = __xuiTableViewMergeBandAt(pColumn, iRow);
	if ( iBand < pColumn->iCount && pColumn->arrBands[iBand].iStart <= iRow ) {
		pMerge = &pData->arrMerges[pColumn->arrBands[iBand].iMerge];
		if ( pOwnerRow != NULL ) *pOwnerRow = pMerge->iRow;
		if ( pOwnerColumn != NULL ) *pOwnerColumn = pMerge->iColumn;
		return pMerge->iRow != iRow || pMerge->iColumn != iColumn;
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
	if ( (pData == NULL) || (iRow < 0) || (iRow >= pData->iRowCount) ) return 0.0f;
	if ( iRowSpan <= 0 ) iRowSpan = 1;
	if ( iRowSpan > pData->iRowCount - iRow ) iRowSpan = pData->iRowCount - iRow;
	return (float)(__xuiTableViewRowOffset(pData, iRow + iRowSpan) - __xuiTableViewRowOffset(pData, iRow));
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

static int __xuiTableViewUpdateContentSizeInternal(xui_widget pWidget,
	xui_table_view_data_t* pData, int bSyncRowCount)
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
	fHeight = bSyncRowCount ? __xuiTableViewContentHeight(pWidget, pData) :
		(float)__xuiTableViewContentHeightFor(pData->arrRowPrefix,
			pData->iRowStateCount, pData->iRowCount, pData->fDefaultRowHeight);
	if ( fHeight < 0.0f ) return XUI_ERROR_INVALID_STATE;
	if ( !__xuiTableViewFloatValid(fWidth) || !__xuiTableViewFloatValid(fHeight) ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = xuiScrollFrameSetContentSize(pData->pFrame, __xuiTableViewMaxFloat(0.0f, fWidth), __xuiTableViewMaxFloat(0.0f, fHeight));
	if ( iRet != XUI_OK ) return iRet;
	(void)tFrame;
	return xuiScrollFrameSetWheelStep(pData->pFrame, pData->fDefaultRowHeight * 3.0f);
}

static int __xuiTableViewUpdateContentSize(xui_widget pWidget, xui_table_view_data_t* pData)
{
	return __xuiTableViewUpdateContentSizeInternal(pWidget, pData, 1);
}

static int __xuiTableViewUpdateContentSizeCommitted(xui_widget pWidget, xui_table_view_data_t* pData)
{
	return __xuiTableViewUpdateContentSizeInternal(pWidget, pData, 0);
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


static const char* __xuiTableViewOwnedText(xui_widget widget,xui_table_view_data_t* data,int row,int column,xui_table_view_cell_lease_t* lease)
{
    if(lease->textReady)return lease->format.text.Data ? lease->format.text.Data:"";
    uint64_t generation=data->iCallbackGeneration;
    lease->failure=xuiInternalTableCellFormatBegin(widget,row,column,&lease->output,&lease->format);
    if(lease->failure==XUI_OK && !__xuiTableViewCallbackCurrent(widget,data,generation))lease->failure=XUI_ERROR_INVALID_STATE;
    if(lease->failure!=XUI_OK){__xuiTableViewRecordFailure(lease->context,lease->failure);return NULL;}
    lease->textReady=true;return lease->format.text.Data ? lease->format.text.Data:"";
}
static int __xuiTableViewOwnedRender(xui_widget widget,xui_table_view_data_t* data,int row,int column,
    xui_table_view_cell_lease_t* lease,xui_draw_context draw,xui_rect_t rect,int state)
{
    const xui_table_view_cell_output_t* source=xuiInternalTableCellSource(&lease->output);
    if(!source || !source->_onRender)return 0;
    xui_table_view_cell_render_owned_proc selected=source->_onRender;void* user=source->_pRenderUser;
    xui_callback_owner_t owner=source->_tRenderOwner;uint64_t generation=data->iCallbackGeneration;
    bool held=false;int result=xuiInternalCallbackHoldIsolated(&owner,&held);
    if(result!=XUI_OK){
        xerror* first=xrtTakeError();
        if(held)__xuiTableViewLeaseOutcome(xuiInternalCallbackOwnerDropOutcome(lease->context,&owner,result,first),&result,&first);
        xrtSetErrorTake(first);return result;
    }
    int handled=0;
    if(!__xuiTableViewCallbackCurrent(widget,data,generation))result=XUI_ERROR_INVALID_STATE;
    else{handled=selected(widget,row,column,&lease->output,draw,rect,state,user);if(handled<0)result=handled;}
    if(!__xuiTableViewCallbackCurrent(widget,data,generation) && result==XUI_OK)result=XUI_ERROR_INVALID_STATE;
    xerror* first=result!=XUI_OK ? xrtTakeError():NULL;
    __xuiTableViewLeaseOutcome(xuiInternalCallbackOwnerDropOutcome(lease->context,&owner,result,first),&result,&first);
    xrtSetErrorTake(first);
    return result!=XUI_OK ? result:handled;
}
static const char* __xuiTableViewCellText(xui_widget pWidget, xui_table_view_data_t* pData, int iRow, int iColumn, const xui_table_view_cell_t* pCell, xui_table_view_cell_lease_t* pLease, char* sBuffer, int iCapacity)
{
	xui_table_view_format_proc onFormat;
	void* pUser;
	uint64_t iGeneration;
	int iHandled;

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
    if(pLease && pLease->owned){
        const xui_table_view_cell_output_t* source=xuiInternalTableCellSource(&pLease->output);
        if(source && source->_onText)return __xuiTableViewOwnedText(pWidget,pData,iRow,iColumn,pLease);
    }
	onFormat = pCell->onFormat;
	pUser = pCell->pFormatUser;
	if ( (onFormat == NULL) && (pData != NULL) && (iColumn >= 0) && (iColumn < pData->iColumnCount) ) {
		onFormat = pData->arrColumns[iColumn].onFormat;
		pUser = pData->arrColumns[iColumn].pFormatUser;
	}
	if ( onFormat != NULL ) {
		iGeneration = pData->iCallbackGeneration;
		iHandled = onFormat(pWidget, iRow, iColumn, pCell, sBuffer, iCapacity, pUser);
		if ( !__xuiTableViewCallbackCurrent(pWidget, pData, iGeneration) ) { sBuffer[0] = 0; return NULL; }
		if ( iHandled != 0 ) {
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

/* Grid edges are integral coverage, with no antialias fringe. Keep the same
 * vertex-color blend path as shape lines, including translucent grid colors. */
static int __xuiTableViewDrawGridFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	static const uint32_t indices[6] = {0, 1, 2, 2, 1, 3};
	xui_mesh_vertex_t vertices[4];
	tRect = xuiInternalSnapRect(tRect);
	if ( tRect.fW <= 0 || tRect.fH <= 0 || __xuiTableViewAlpha(iColor) == 0 ) return XUI_OK;
	if ( pProxy->drawMeshTriangles == NULL ) return __xuiTableViewDrawFill(pProxy, pDraw, tRect, iColor);
	vertices[0] = (xui_mesh_vertex_t){tRect.fX, tRect.fY, iColor};
	vertices[1] = (xui_mesh_vertex_t){tRect.fX + tRect.fW, tRect.fY, iColor};
	vertices[2] = (xui_mesh_vertex_t){tRect.fX, tRect.fY + tRect.fH, iColor};
	vertices[3] = (xui_mesh_vertex_t){tRect.fX + tRect.fW, tRect.fY + tRect.fH, iColor};
	return pProxy->drawMeshTriangles(pProxy, pDraw, vertices, 4, indices, 6, 0);
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

static int __xuiTableViewDrawChecker(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, const xui_table_view_data_t* pResolved)
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
	iA = pResolved->iCheckerPrimaryColor;
	iB = pResolved->iCheckerSecondaryColor;
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

static int __xuiTableViewDrawCheck(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, int bChecked, int bDisabled, uint32_t iAccent, uint32_t iDisabledColor, const xui_table_view_data_t* pResolved)
{
	uint32_t iBorder;
	uint32_t iFill;
	int iRet;

	iBorder = bDisabled ? iDisabledColor : iAccent;
	iFill = bChecked ? iAccent : pResolved->iCheckBackgroundColor;
	iRet = __xuiTableViewDrawRectFill(pProxy, pDraw, tRect, iFill);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTableViewDrawRectStroke(pProxy, pDraw, tRect, 1.0f, iBorder);
	if ( iRet != XUI_OK ) return iRet;
	if ( bChecked && (pProxy->drawLine != NULL) ) {
		iRet = pProxy->drawLine(pProxy, pDraw, tRect.fX + 3.0f, tRect.fY + 6.0f, tRect.fX + 5.2f, tRect.fY + 8.2f, 1.7f, pResolved->iCheckMarkColor);
		if ( iRet != XUI_OK ) return iRet;
		iRet = pProxy->drawLine(pProxy, pDraw, tRect.fX + 5.0f, tRect.fY + 8.1f, tRect.fX + 9.3f, tRect.fY + 3.8f, 1.7f, pResolved->iCheckMarkColor);
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

static int __xuiTableViewDrawCellContent(xui_widget pWidget, xui_table_view_data_t* pData, const xui_table_view_data_t* pResolved, xui_proxy pProxy, xui_draw_context pDraw, int iRow, int iColumn, const xui_table_view_cell_t* pCell, xui_table_view_cell_lease_t* pLease, xui_rect_t tCell, int iState)
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
		iText = pResolved->iSelectedTextColor;
	}
	if ( pCell->bHasStyle && (pResolved->tPaintAdapter.bCellColorsPresent || __xuiTableViewAlpha(pCell->iTextColor) != 0) ) {
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
	sText = __xuiTableViewCellText(pWidget, pData, iRow, iColumn, pCell, pLease, sBuffer, (int)sizeof(sBuffer));
	if ( sText == NULL ) return pLease && pLease->failure!=XUI_OK ? pLease->failure:XUI_ERROR_INVALID_STATE;
	tText = (xui_rect_t){tCell.fX + 8.0f, tCell.fY + 1.0f, __xuiTableViewMaxFloat(1.0f, tCell.fW - 16.0f), __xuiTableViewMaxFloat(1.0f, tCell.fH - 2.0f)};
	if ( pCell->iType == XUI_TABLE_CELL_TYPE_BOOL ) {
		tBox = xuiInternalSnapRect((xui_rect_t){tCell.fX + 8.0f, tCell.fY + (tCell.fH - 12.0f) * 0.5f, 12.0f, 12.0f});
		iRet = __xuiTableViewDrawCheck(pProxy, pDraw, tBox, __xuiTableViewBoolFromText(sText), (iState & XUI_TABLE_CELL_DISABLED) != 0, pResolved->iSelectedColor, pResolved->iDisabledTextColor, pResolved);
		if ( iRet != XUI_OK ) return iRet;
		tText.fX = tBox.fX + tBox.fW + 6.0f;
		tText.fW = __xuiTableViewMaxFloat(1.0f, tCell.fX + tCell.fW - tText.fX - 7.0f);
		iFlags = XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP;
	} else if ( pCell->iType == XUI_TABLE_CELL_TYPE_COLOR ) {
		tBox = xuiInternalSnapRect((xui_rect_t){tCell.fX + 8.0f, tCell.fY + (tCell.fH - 14.0f) * 0.5f, 24.0f, 14.0f});
		iColor = pResolved->iColorFallbackColor;
		(void)__xuiTableViewParseColor(sText, &iColor);
		iGrid = pResolved->iGridColor;
		if ( __xuiTableViewAlpha(iColor) < 255 ) {
			iRet = __xuiTableViewDrawChecker(pProxy, pDraw, tBox, pResolved);
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
		iRet = __xuiTableViewDrawRectFill(pProxy, pDraw, tButton, pResolved->iPickerBackgroundColor);
		if ( iRet != XUI_OK ) return iRet;
		iRet = __xuiTableViewDrawRectStroke(pProxy, pDraw, tButton, 1.0f, pResolved->iPickerBorderColor);
		if ( iRet != XUI_OK ) return iRet;
		if ( (pResolved->pFont != NULL) && (__xuiTableViewAlpha(iText) != 0) ) {
			iRet = pProxy->drawText(pProxy, pDraw, &(xui_text_item_t){.iSize=sizeof(xui_text_item_t), .pFont=pResolved->pFont, .sText="...", .iTextSize=-1, .iFlags=XUI_TEXT_SHAPE_DEFAULT | ((XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP) & XUI_TEXT_RTL ? XUI_TEXT_SHAPE_RTL : 0)}, tButton, iText, XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	if ( (pResolved->pFont != NULL) && (__xuiTableViewAlpha(iText) != 0) && (tText.fW > 0.0f) ) {
		iRet = pProxy->drawText(pProxy, pDraw, &(xui_text_item_t){.iSize=sizeof(xui_text_item_t), .pFont=pResolved->pFont, .sText=__xuiTableViewText(sText), .iTextSize=-1, .iFlags=XUI_TEXT_SHAPE_DEFAULT | ((iFlags) & XUI_TEXT_RTL ? XUI_TEXT_SHAPE_RTL : 0)}, xuiInternalSnapRect(tText), iText, iFlags);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiTableViewCellDisabled(xui_widget pWidget, xui_table_view_data_t* pData, int iRow, int iColumn)
{
	xui_table_view_cell_t tCell;
    xui_table_view_cell_lease_t lease={0};

	if ( __xuiTableViewRowDisabled(pData, iRow) ) {
		return 1;
	}
	if ( __xuiTableViewGetCell(pWidget, pData, iRow, iColumn, &tCell,&lease) != XUI_OK ) return XUI_ERROR_INVALID_STATE;
    int disabled=tCell.bDisabled!=0;
    int ended=__xuiTableViewLeaseEnd(&lease,XUI_OK);
    return ended!=XUI_OK ? ended:disabled;
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
	int iDisabled;

	if ( pOutRow != NULL ) *pOutRow = -1;
	if ( pOutColumn != NULL ) *pOutColumn = -1;
	if ( (pData == NULL) || (pData->iRowCount <= 0) || (pData->iColumnCount <= 0) ) {
		return 0;
	}
	r = iRow;
	c = iColumn;
	while ( (r >= 0) && (r < pData->iRowCount) && (c >= 0) && (c < pData->iColumnCount) ) {
		iDisabled = (__xuiTableViewColumnWidth(pData, c) > 0.0f) ? __xuiTableViewCellDisabled(pWidget, pData, r, c) : 1;
		if ( iDisabled < 0 ) return 0;
		if ( !iDisabled ) {
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
	int iRet;
	xui_table_view_hover_proc onHover;

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
	iRet = __xuiTableViewInvalidateHoverCells(pWidget, pData, iOldRow, iOldColumn, iRow, iColumn);
	if ( iRet != XUI_OK ) {
		pData->iHoverRow = iOldRow;
		pData->iHoverColumn = iOldColumn;
		pData->iHoverCount--;
		return iRet;
	}
	onHover = pData->onHover;
	if ( onHover != NULL ) XUI_INTERNAL_CALLBACK_VOID(pWidget, pData->onHover, xui_table_view_hover_proc, pData->pHoverUser, pWidget, iRow, iColumn, pData->iSelectionMode);
	return XUI_OK;
}

static int __xuiTableViewEnsureVisibleInternal(xui_widget pWidget, xui_table_view_data_t* pData, int iRow, int iColumn)
{
	xui_table_view_cell_t tCell;
    xui_table_view_cell_lease_t lease={0};
	xui_rect_t tRect;
	int iOwnerRow;
	int iOwnerColumn;
	int iRet;

	if ( (pData == NULL) || (pData->pFrame == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( __xuiTableViewSyncRowCount(pWidget, pData) < 0 ) return XUI_ERROR_INVALID_STATE;
	if ( (iRow < 0) || (iRow >= pData->iRowCount) || (iColumn < 0) || (iColumn >= pData->iColumnCount) ) {
		return XUI_OK;
	}
	iRet = __xuiTableViewMergeOwner(pWidget, pData, iRow, iColumn, &iOwnerRow, &iOwnerColumn);
	if ( iRet < 0 ) return iRet;
	iRet = __xuiTableViewGetCell(pWidget, pData, iOwnerRow, iOwnerColumn, &tCell,&lease);
	if ( iRet != XUI_OK ) return iRet;
	tRect = __xuiTableViewCellContentRectData(pData, iOwnerRow, iOwnerColumn, tCell.iRowSpan, tCell.iColSpan);
    int ended=__xuiTableViewLeaseEnd(&lease,XUI_OK);
    if(ended!=XUI_OK)return ended;
    if(!__xuiTableViewWidgetAlive(pWidget,pData))return XUI_ERROR_INVALID_STATE;
	return xuiScrollFrameEnsureRectVisible(pData->pFrame, tRect);
}

static void __xuiTableViewNotifySelect(xui_widget pWidget, xui_table_view_data_t* pData)
{
	if ( pData == NULL ) {
		return;
	}
	pData->iSelectCount++;
	if ( pData->onSelect != NULL ) {
		XUI_INTERNAL_CALLBACK_VOID(pWidget, pData->onSelect, xui_table_view_select_proc, pData->pSelectUser, pWidget, pData->iSelectedRow, pData->iSelectedColumn, pData->iSelectionMode);
	}
}

static int __xuiTableViewSelect(xui_widget pWidget, xui_table_view_data_t* pData, int iRow, int iColumn, int bNotify)
{
	int iOldRow;
	int iOldColumn;
	int iOldFocusRow;
	int iOldFocusColumn;
	int iRet;
	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( __xuiTableViewSyncRowCount(pWidget, pData) < 0 ) return XUI_ERROR_INVALID_STATE;
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
			if ( (iColumn < 0) || (iColumn >= pData->iColumnCount) ) {
				return XUI_OK;
			}
			iRet = __xuiTableViewCellDisabled(pWidget, pData, iRow, iColumn);
			if ( iRet < 0 ) return iRet;
			if ( iRet ) return XUI_OK;
		}
	}
	iOldRow = pData->iSelectedRow;
	iOldColumn = pData->iSelectedColumn;
	iOldFocusRow = pData->iFocusRow;
	iOldFocusColumn = pData->iFocusColumn;
	if ( (pData->iSelectedRow == iRow) && (pData->iSelectedColumn == iColumn) ) {
		if ( bNotify ) {
			__xuiTableViewNotifySelect(pWidget, pData);
		}
		return XUI_OK;
	}
	if ( iRow >= 0 ) {
		iRet = __xuiTableViewEnsureVisibleInternal(pWidget, pData, iRow, (iColumn >= 0) ? iColumn : __xuiTableViewFirstVisibleColumn(pData));
		if ( iRet != XUI_OK ) return iRet;
	}
	pData->iSelectedRow = iRow;
	pData->iSelectedColumn = iColumn;
	pData->iFocusRow = iRow;
	pData->iFocusColumn = iColumn;
	if ( pData->iSelectionMode == XUI_TABLE_VIEW_SELECTION_ROW ) {
		pData->iFocusColumn = -1;
	}
	pData->iChangeCount++;
	iRet = __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( iRet != XUI_OK ) {
		pData->iSelectedRow = iOldRow;
		pData->iSelectedColumn = iOldColumn;
		pData->iFocusRow = iOldFocusRow;
		pData->iFocusColumn = iOldFocusColumn;
		pData->iChangeCount--;
		return iRet;
	}
	if ( iOldRow != iRow || iOldColumn != iColumn ) {
		xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_SELECTION_CHANGED);
	}
	if ( iOldFocusRow != pData->iFocusRow || iOldFocusColumn != pData->iFocusColumn ) {
		xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_FOCUS_CHANGED);
	}
	if ( bNotify ) __xuiTableViewNotifySelect(pWidget, pData);
	return XUI_OK;
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
	int iRet;

	if ( pRow != NULL ) *pRow = -1;
	if ( pColumn != NULL ) *pColumn = -1;
	if ( (pWidget == NULL) || (pData == NULL) || (pData->pViewport == NULL) ) {
		return 0;
	}
	if ( __xuiTableViewSyncRowCount(pWidget, pData) < 0 ) return XUI_ERROR_INVALID_STATE;
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
	iRet = __xuiTableViewMergeOwner(pWidget, pData, iRow, iColumn, &iOwnerRow, &iOwnerColumn);
	if ( iRet < 0 ) return iRet;
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

static int __xuiTableViewQueryCursor(xui_widget pWidget, int iX, int iY, void* pUser)
{
	xui_table_view_data_t* pData = (xui_table_view_data_t*)pUser;

	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_CURSOR_INHERIT;
	if ( !xuiWidgetGetEffectiveEnabled(pWidget) ) return XUI_CURSOR_NOT_ALLOWED;
	return (__xuiTableViewHeaderResizeAtWorld(pWidget, pData, (float)iX, (float)iY) >= 0) ? XUI_CURSOR_RESIZE_EW : XUI_CURSOR_INHERIT;
}

static int __xuiTableViewApplyFrameStyle(xui_widget pWidget, xui_table_view_data_t* pData)
{
	xui_table_view_data_t tResolved;
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
	__xuiTableViewResolve(pWidget, pData, &tResolved);
	if ( iRet == XUI_OK ) iRet = xuiScrollFrameSetColors(pData->pFrame, tResolved.iBarColor, tResolved.iThumbColor, tResolved.iScrollbarHoverColor, tResolved.iScrollbarActiveColor, tResolved.iScrollbarFocusColor, tResolved.iScrollbarDisabledColor);
	return iRet;
}

static int __xuiTableViewHasActivePointer(const xui_table_view_data_t* pData)
{
	return pData != NULL && (pData->iResizeColumn >= 0 || pData->iActiveHeader >= 0 ||
		pData->iActiveRow >= 0);
}

static int __xuiTableViewPointerMatches(const xui_table_view_data_t* pData,
	const xui_event_t* pEvent)
{
	return __xuiTableViewHasActivePointer(pData) && pEvent != NULL &&
		pData->iActivePointerId == pEvent->iPointerId &&
		pData->iActivePointerType == pEvent->iPointerType;
}

static void __xuiTableViewCancelPointer(xui_widget pWidget,
	xui_table_view_data_t* pData, int bReleaseCapture)
{
	xui_context pContext;
	uint64_t iPointerId;
	int iPointerType;

	if ( pWidget == NULL || pData == NULL || !__xuiTableViewHasActivePointer(pData) ) return;
	pContext = xuiWidgetGetContext(pWidget);
	iPointerId = pData->iActivePointerId;
	iPointerType = pData->iActivePointerType;
	pData->iResizeColumn = -1;
	pData->iActiveHeader = -1;
	pData->iActiveRow = -1;
	pData->iActiveColumn = -1;
	pData->iActivePointerId = XUI_POINTER_ID_MOUSE;
	pData->iActivePointerType = XUI_POINTER_TYPE_MOUSE;
	if ( bReleaseCapture && pContext != NULL &&
	     xuiGetPointerCaptureEx(pContext, iPointerId, iPointerType) == pWidget ) {
		(void)xuiReleasePointerCaptureEx(pContext, iPointerId, iPointerType, pWidget);
	}
}

static int __xuiTableViewActivateSort(xui_widget pWidget,
	xui_table_view_data_t* pData, int iColumn)
{
	xui_table_view_sort_proc onSort;
	int iRet;

	if ( iColumn < 0 || iColumn >= pData->iColumnCount ) return XUI_OK;
	if ( pData->iSortColumn == iColumn ) pData->bSortDescending = !pData->bSortDescending;
	else {
		pData->iSortColumn = iColumn;
		pData->bSortDescending = 0;
	}
	pData->iSortCount++;
	pData->iChangeCount++;
	__xuiTableViewInvalidateMerges(pData);
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_NODE_CHANGED);
	iRet = __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( iRet != XUI_OK ) return iRet;
	onSort = pData->onSort;
	if ( onSort != NULL ) XUI_INTERNAL_CALLBACK_VOID(pWidget, pData->onSort, xui_table_view_sort_proc, pData->pSortUser, pWidget, iColumn, pData->bSortDescending);
	return XUI_OK;
}

static int __xuiTableViewPointerMove(xui_widget pWidget, xui_table_view_data_t* pData, const xui_event_t* pEvent)
{
	float fDelta;
	float fWidth;
	float fMin;
	float fMax;
	int iRow;
	int iColumn;
	int iRet;

	if ( (pEvent == NULL) || (pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) ) {
		return XUI_OK;
	}
	if ( __xuiTableViewHasActivePointer(pData) && !__xuiTableViewPointerMatches(pData, pEvent) ) return XUI_OK;
	if ( (pData->iResizeColumn >= 0) &&
	     (xuiGetPointerCaptureEx(xuiWidgetGetContext(pWidget), pEvent->iPointerId,
	      pEvent->iPointerType) == pWidget) ) {
		float fOldWidth;
		xui_table_view_column_resize_proc onColumnResize;
		int iResizeColumn;
		if ( pData->iResizeColumn >= pData->iColumnCount ) {
			__xuiTableViewCancelPointer(pWidget, pData, 1);
			return XUI_EVENT_DISPATCH_STOP;
		}
		fDelta = pEvent->fX - pData->fDragX;
		fWidth = pData->fDragWidth + fDelta;
		fMin = pData->arrColumns[pData->iResizeColumn].fMinWidth;
		fMax = pData->arrColumns[pData->iResizeColumn].fMaxWidth;
		if ( fMin <= 0.0f ) fMin = 30.0f;
		if ( fWidth < fMin ) fWidth = fMin;
		if ( (fMax > 0.0f) && (fWidth > fMax) ) fWidth = fMax;
		if ( pData->arrColumns[pData->iResizeColumn].fWidth != fWidth ) {
			fOldWidth = pData->arrColumns[pData->iResizeColumn].fWidth;
			pData->arrColumns[pData->iResizeColumn].fWidth = fWidth;
			__xuiTableViewInvalidateMerges(pData);
			iRet = __xuiTableViewUpdateContentSizeCommitted(pWidget, pData);
			if ( iRet != XUI_OK ) {
				if ( __xuiTableViewWidgetAlive(pWidget, pData) &&
				     pData->iResizeColumn >= 0 && pData->iResizeColumn < pData->iColumnCount ) {
					pData->arrColumns[pData->iResizeColumn].fWidth = fOldWidth;
					__xuiTableViewInvalidateMerges(pData);
					(void)__xuiTableViewUpdateContentSizeCommitted(pWidget, pData);
				}
				return iRet;
			}
			pData->iColumnResizeCount++;
			pData->iChangeCount++;
			xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_BOUNDS_CHANGED);
			iRet = __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
			if ( iRet != XUI_OK ) return iRet;
			iResizeColumn = pData->iResizeColumn;
			onColumnResize = pData->onColumnResize;
			if ( onColumnResize != NULL ) {
				XUI_INTERNAL_CALLBACK_VOID(pWidget, pData->onColumnResize, xui_table_view_column_resize_proc, pData->pColumnResizeUser, pWidget, iResizeColumn, fWidth);
			}
			return XUI_EVENT_DISPATCH_STOP;
		}
		return XUI_EVENT_DISPATCH_STOP;
	}
	iRet = __xuiTableViewHitCellWorld(pWidget, pData, pEvent->fX, pEvent->fY, &iRow, &iColumn);
	if ( iRet < 0 ) return XUI_EVENT_DISPATCH_STOP;
	if ( iRet ) {
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
	int iRet;

	if ( (pEvent == NULL) || (pEvent->iButton != XUI_POINTER_BUTTON_LEFT) || (pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) ) {
		return XUI_OK;
	}
	if ( !xuiWidgetGetEffectiveEnabled(pWidget) ) {
		return XUI_OK;
	}
	if ( __xuiTableViewHasActivePointer(pData) ) {
		return __xuiTableViewPointerMatches(pData, pEvent) ? XUI_EVENT_DISPATCH_STOP : XUI_OK;
	}
	iResize = __xuiTableViewHeaderResizeAtWorld(pWidget, pData, pEvent->fX, pEvent->fY);
	if ( iResize >= 0 ) {
		iRet = xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
		if ( iRet != XUI_OK || !__xuiTableViewWidgetAlive(pWidget, pData) ) return (iRet == XUI_OK) ? (int)XUI_EVENT_DISPATCH_STOP : iRet;
		iRet = xuiSetPointerCaptureEx(xuiWidgetGetContext(pWidget), pEvent->iPointerId,
			pEvent->iPointerType, pWidget);
		if ( iRet != XUI_OK || !__xuiTableViewWidgetAlive(pWidget, pData) ) return iRet;
		pData->iResizeColumn = iResize;
		pData->iActivePointerId = pEvent->iPointerId;
		pData->iActivePointerType = pEvent->iPointerType;
		pData->fDragX = pEvent->fX;
		pData->fDragWidth = __xuiTableViewColumnWidth(pData, iResize);
		return XUI_EVENT_DISPATCH_STOP;
	}
	iHeader = __xuiTableViewHeaderColumnAtWorld(pWidget, pData, pEvent->fX, pEvent->fY);
	if ( iHeader >= 0 ) {
		iRet = xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
		if ( iRet != XUI_OK || !__xuiTableViewWidgetAlive(pWidget, pData) ) return (iRet == XUI_OK) ? (int)XUI_EVENT_DISPATCH_STOP : iRet;
		iRet = xuiSetPointerCaptureEx(xuiWidgetGetContext(pWidget), pEvent->iPointerId,
			pEvent->iPointerType, pWidget);
		if ( iRet != XUI_OK || !__xuiTableViewWidgetAlive(pWidget, pData) ) return iRet;
		pData->iActiveHeader = iHeader;
		pData->iActivePointerId = pEvent->iPointerId;
		pData->iActivePointerType = pEvent->iPointerType;
		return XUI_EVENT_DISPATCH_STOP;
	}
	iRet = __xuiTableViewHitCellWorld(pWidget, pData, pEvent->fX, pEvent->fY, &iRow, &iColumn);
	if ( iRet < 0 ) return XUI_EVENT_DISPATCH_STOP;
	if ( iRet ) {
		iRet = xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
		if ( iRet != XUI_OK || !__xuiTableViewWidgetAlive(pWidget, pData) ) return (iRet == XUI_OK) ? (int)XUI_EVENT_DISPATCH_STOP : iRet;
		iRet = xuiSetPointerCaptureEx(xuiWidgetGetContext(pWidget), pEvent->iPointerId,
			pEvent->iPointerType, pWidget);
		if ( iRet != XUI_OK || !__xuiTableViewWidgetAlive(pWidget, pData) ) return iRet;
		pData->iActiveRow = iRow;
		pData->iActiveColumn = iColumn;
		pData->iActivePointerId = pEvent->iPointerId;
		pData->iActivePointerType = pEvent->iPointerType;
		iRet = __xuiTableViewSetHover(pWidget, pData, iRow, iColumn);
		if ( iRet != XUI_OK ) {
			if ( __xuiTableViewWidgetAlive(pWidget, pData) ) {
				__xuiTableViewCancelPointer(pWidget, pData, 1);
			}
			return iRet;
		}
		if ( !__xuiTableViewWidgetAlive(pWidget, pData) ) return XUI_EVENT_DISPATCH_STOP;
		iRet = __xuiTableViewSelect(pWidget, pData, iRow, iColumn, 1);
		if ( iRet != XUI_OK && __xuiTableViewWidgetAlive(pWidget, pData) ) __xuiTableViewCancelPointer(pWidget, pData, 1);
		return XUI_EVENT_DISPATCH_STOP;
	}
	return XUI_OK;
}

static int __xuiTableViewPointerUp(xui_widget pWidget, xui_table_view_data_t* pData, const xui_event_t* pEvent)
{
	xui_context pContext;
	int iActiveHeader;
	int iHeader;
	int iRet;

	if ( (pEvent == NULL) || (pEvent->iButton != XUI_POINTER_BUTTON_LEFT) || (pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) ) {
		return XUI_OK;
	}
	if ( !__xuiTableViewPointerMatches(pData, pEvent) ) return XUI_OK;
	pContext = xuiWidgetGetContext(pWidget);
	if ( xuiGetPointerCaptureEx(pContext, pEvent->iPointerId, pEvent->iPointerType) != pWidget ) {
		__xuiTableViewCancelPointer(pWidget, pData, 0);
		return XUI_OK;
	}
	iActiveHeader = pData->iActiveHeader;
	iHeader = (iActiveHeader >= 0) ? __xuiTableViewHeaderColumnAtWorld(pWidget, pData, pEvent->fX, pEvent->fY) : -1;
	__xuiTableViewCancelPointer(pWidget, pData, 1);
	iRet = __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( iRet != XUI_OK ) return iRet;
	if ( iActiveHeader >= 0 && iHeader == iActiveHeader && xuiWidgetGetEffectiveEnabled(pWidget) ) {
		iRet = __xuiTableViewActivateSort(pWidget, pData, iActiveHeader);
		return (iRet == XUI_OK) ? (int)XUI_EVENT_DISPATCH_STOP : iRet;
	}
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiTableViewContextMenu(xui_widget pWidget, xui_table_view_data_t* pData, const xui_event_t* pEvent)
{
	xui_rect_t tAnchor;
	xui_rect_t tLocal;
	xui_rect_t tWorld;
	float fX;
	float fY;
	int iPart = XUI_CONTEXT_TARGET_BACKGROUND;
	int iRow = -1;
	int iColumn = -1;
	int iAnchorColumn;
	int iRet;

	if ( pEvent == NULL || pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE || !xuiWidgetGetEffectiveEnabled(pWidget) ) return XUI_OK;
	tWorld = xuiWidgetGetWorldRect(pWidget);
	tAnchor = tWorld;
	if ( pEvent->iKey == XUI_KEY_CONTEXT_MENU ) {
		iRow = pData->iSelectedRow;
		iColumn = pData->iSelectedColumn;
		if ( iRow >= 0 ) {
			iPart = XUI_CONTEXT_TARGET_ITEM;
			iAnchorColumn = (iColumn >= 0) ? iColumn : __xuiTableViewFirstVisibleColumn(pData);
			if ( iAnchorColumn >= 0 ) {
				iRet = xuiTableViewGetCellRect(pWidget, iRow, iAnchorColumn, &tLocal);
				if ( iRet != XUI_OK ) return XUI_EVENT_DISPATCH_STOP;
				tAnchor = tLocal;
				tAnchor.fX += tWorld.fX;
				tAnchor.fY += tWorld.fY;
			}
		}
	} else {
		iColumn = __xuiTableViewHeaderColumnAtWorld(pWidget, pData, pEvent->fX, pEvent->fY);
		if ( iColumn >= 0 ) {
			iPart = XUI_CONTEXT_TARGET_HEADER;
			iRow = -1;
		} else {
			iRet = __xuiTableViewHitCellWorld(pWidget, pData, pEvent->fX, pEvent->fY, &iRow, &iColumn);
			if ( iRet < 0 ) return XUI_EVENT_DISPATCH_STOP;
			if ( iRet ) iPart = XUI_CONTEXT_TARGET_ITEM;
		}
	}
	xuiInternalContextMenuPoint(pEvent, tAnchor, &fX, &fY);
	iRet = xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
	if ( iRet != XUI_OK || !__xuiTableViewWidgetAlive(pWidget, pData) ) return (iRet == XUI_OK) ? (int)XUI_EVENT_DISPATCH_STOP : iRet;
	if ( iPart == XUI_CONTEXT_TARGET_ITEM ) {
		if ( __xuiTableViewSetHover(pWidget, pData, iRow, iColumn) != XUI_OK ) return XUI_EVENT_DISPATCH_STOP;
		if ( !__xuiTableViewWidgetAlive(pWidget, pData) ) return XUI_EVENT_DISPATCH_STOP;
		if ( pData->iSelectedRow != iRow ||
		     (pData->iSelectionMode == XUI_TABLE_VIEW_SELECTION_CELL && pData->iSelectedColumn != iColumn) ) {
			iRet = __xuiTableViewSelect(pWidget, pData, iRow, iColumn, 1);
			if ( iRet != XUI_OK ) return iRet;
			if ( !__xuiTableViewWidgetAlive(pWidget, pData) ) return XUI_EVENT_DISPATCH_STOP;
		}
	}
	if ( pData->onContext != NULL ) {
		return __xuiOwnedInvoke_xuiTableViewSetContextMenu(pWidget,pData,pData->onContext,pData->pContextUser ,iPart,iRow,iColumn,fX,fY);
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
	int iRet;

	if ( __xuiTableViewFindSelectableCell(pWidget, pData, iRow, iColumn, iRowStep, iColumnStep, &iTargetRow, &iTargetColumn) ) {
		iRet = __xuiTableViewSelect(pWidget, pData, iTargetRow, iTargetColumn, 1);
		return (iRet == XUI_OK) ? (int)XUI_EVENT_DISPATCH_STOP : iRet;
	}
	return XUI_OK;
}

static int __xuiTableViewMoveKeyboardRow(xui_widget pWidget,
	xui_table_view_data_t* pData, int iTarget)
{
	int iRet;
	if ( iTarget < 0 ) return XUI_OK;
	iRet = __xuiTableViewSelect(pWidget, pData, iTarget, -1, 1);
	return (iRet == XUI_OK) ? (int)XUI_EVENT_DISPATCH_STOP : iRet;
}

static int __xuiTableViewKeyDown(xui_widget pWidget, xui_table_view_data_t* pData, const xui_event_t* pEvent)
{
	int iCurrentRow;
	int iCurrentColumn;
	int iTarget;
	int iRows;

	if ( (pEvent == NULL) || (pEvent->iPhase == XUI_EVENT_PHASE_BUBBLE) || !xuiWidgetGetEffectiveEnabled(pWidget) || (pData->iRowCount <= 0) ) {
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
			return __xuiTableViewMoveKeyboardRow(pWidget, pData, iTarget);
		case XUI_KEY_DOWN:
			iTarget = (iCurrentRow >= 0) ? __xuiTableViewFindEnabledRow(pWidget, pData, iCurrentRow + 1, 1) : __xuiTableViewFindEnabledRow(pWidget, pData, 0, 1);
			return __xuiTableViewMoveKeyboardRow(pWidget, pData, iTarget);
		case XUI_KEY_PAGE_UP:
			iRows = __xuiTableViewVisibleRows(pWidget, pData);
			iTarget = (iCurrentRow >= 0) ? __xuiTableViewFindEnabledRow(pWidget, pData, iCurrentRow - iRows, -1) : __xuiTableViewFindEnabledRow(pWidget, pData, 0, 1);
			return __xuiTableViewMoveKeyboardRow(pWidget, pData, iTarget);
		case XUI_KEY_PAGE_DOWN:
			iRows = __xuiTableViewVisibleRows(pWidget, pData);
			iTarget = (iCurrentRow >= 0) ? __xuiTableViewFindEnabledRow(pWidget, pData, iCurrentRow + iRows, 1) : __xuiTableViewFindEnabledRow(pWidget, pData, 0, 1);
			return __xuiTableViewMoveKeyboardRow(pWidget, pData, iTarget);
		case XUI_KEY_HOME:
			iTarget = __xuiTableViewFindEnabledRow(pWidget, pData, 0, 1);
			return __xuiTableViewMoveKeyboardRow(pWidget, pData, iTarget);
		case XUI_KEY_END:
			iTarget = __xuiTableViewFindEnabledRow(pWidget, pData, pData->iRowCount - 1, -1);
			return __xuiTableViewMoveKeyboardRow(pWidget, pData, iTarget);
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
		if ( __xuiTableViewPointerMatches(pData, pEvent) ) __xuiTableViewCancelPointer(pWidget, pData, 0);
		return __xuiTableViewSetHover(pWidget, pData, -1, -1);
	case XUI_EVENT_POINTER_DOWN:
		return __xuiTableViewPointerDown(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_UP:
		return __xuiTableViewPointerUp(pWidget, pData, pEvent);
	case XUI_EVENT_CONTEXT_MENU:
		return __xuiTableViewContextMenu(pWidget, pData, pEvent);
	case XUI_EVENT_KEY_DOWN:
		return __xuiTableViewKeyDown(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_WHEEL:
		if ( pEvent->iPhase != XUI_EVENT_PHASE_CAPTURE ) {
			return __xuiTableViewSetHover(pWidget, pData, -1, -1);
		}
		return XUI_OK;
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
	case XUI_EVENT_ENABLED_CHANGED:
	case XUI_EVENT_VISIBLE_CHANGED:
		if ( pEvent->pTarget != pWidget ) return XUI_OK;
		if ( (pEvent->iType == XUI_EVENT_ENABLED_CHANGED || pEvent->iType == XUI_EVENT_VISIBLE_CHANGED) &&
		     (!xuiWidgetGetEffectiveEnabled(pWidget) || !xuiWidgetGetEffectiveVisible(pWidget)) ) {
			__xuiTableViewCancelPointer(pWidget, pData, 1);
		}
		if ( pEvent->iType == XUI_EVENT_ENABLED_CHANGED && pData->pFrame != NULL ) {
			int iRet = xuiWidgetSetEnabled(pData->pFrame,
				xuiWidgetGetEffectiveEnabled(pWidget));
			if ( iRet != XUI_OK ) return iRet;
		}
		return __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	case XUI_EVENT_BOUNDS_CHANGED:
		if ( pEvent->pTarget != pWidget ) return XUI_OK;
		return __xuiTableViewUpdateContentSize(pWidget, pData);
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
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_BOUNDS_CHANGED);
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
	if ( !__xuiTableViewFloatValid(tResolved.fDefaultColumnWidth) || tResolved.fDefaultColumnWidth <= 0.0f ||
	     !__xuiTableViewFloatValid(tResolved.fDefaultRowHeight) || tResolved.fDefaultRowHeight <= 0.0f ||
	     !__xuiTableViewFloatValid(tResolved.fHeaderHeight) ||
	     !__xuiTableViewFloatValid(tResolved.fBorderWidth) ) return XUI_ERROR_INVALID_ARGUMENT;
	fRows = (float)((pData->iRowCount > 0 && pData->iRowCount < XUI_TABLE_VIEW_DEFAULT_ROWS) ? pData->iRowCount : XUI_TABLE_VIEW_DEFAULT_ROWS);
	if ( fRows < 3.0f ) fRows = 3.0f;
	pSize->fX = xuiInternalSnapSize(XUI_TABLE_VIEW_DEFAULT_WIDTH);
	pSize->fY = xuiInternalSnapSize(tResolved.fHeaderHeight + tResolved.fDefaultRowHeight * fRows + tResolved.fBorderWidth * 2.0f);
	return XUI_OK;
}

static int __xuiTableViewPrepare(xui_widget pWidget, void* pUser)
{
	xui_table_view_data_t* pData;
	xui_table_view_data_t tResolved;
	xui_thickness_t tMargin;
	xui_thickness_t tOldMargin;
	float fOldDefaultColumnWidth;
	float fOldDefaultRowHeight;
	float fOldHeaderHeight;
	float fOldBorderWidth;
	int bMetricsChanged;
	int iRet;

	pData = (xui_table_view_data_t*)pUser;
	if ( (pWidget == NULL) || (pData == NULL) || (pData->pFrame == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiTableViewResolve(pWidget, pData, &tResolved);
	if ( !__xuiTableViewFloatValid(tResolved.fDefaultColumnWidth) || tResolved.fDefaultColumnWidth <= 0.0f ||
	     !__xuiTableViewFloatValid(tResolved.fDefaultRowHeight) || tResolved.fDefaultRowHeight <= 0.0f ||
	     !__xuiTableViewFloatValid(tResolved.fHeaderHeight) ||
	     !__xuiTableViewFloatValid(tResolved.fBorderWidth) ||
	     (double)tResolved.fHeaderHeight + (double)tResolved.fBorderWidth * 2.0 > (double)XUI_LAYOUT_UNBOUNDED ||
	     !__xuiTableViewDimensionsValid(pData->arrColumns, pData->iColumnCount,
	      tResolved.fDefaultColumnWidth, pData->arrRowPrefix, pData->iRowStateCount,
	      pData->iRowCount, tResolved.fDefaultRowHeight) ) return XUI_ERROR_INVALID_ARGUMENT;
	fOldDefaultColumnWidth = pData->fDefaultColumnWidth;
	fOldDefaultRowHeight = pData->fDefaultRowHeight;
	fOldHeaderHeight = pData->fHeaderHeight;
	fOldBorderWidth = pData->fBorderWidth;
	tOldMargin = xuiWidgetGetMargin(pData->pFrame);
	bMetricsChanged = fOldDefaultColumnWidth != tResolved.fDefaultColumnWidth ||
		fOldDefaultRowHeight != tResolved.fDefaultRowHeight ||
		fOldHeaderHeight != tResolved.fHeaderHeight ||
		fOldBorderWidth != tResolved.fBorderWidth;
	pData->fDefaultColumnWidth = tResolved.fDefaultColumnWidth;
	pData->fDefaultRowHeight = tResolved.fDefaultRowHeight;
	pData->fHeaderHeight = tResolved.fHeaderHeight;
	pData->fBorderWidth = tResolved.fBorderWidth;
	tMargin = (xui_thickness_t){
		tResolved.fBorderWidth,
		tResolved.fBorderWidth + tResolved.fHeaderHeight,
		tResolved.fBorderWidth,
		tResolved.fBorderWidth
	};
	iRet = xuiWidgetSetMargin(pData->pFrame, tMargin);
	if ( iRet == XUI_OK ) iRet = __xuiTableViewApplyFrameStyle(pWidget, pData);
	if ( iRet == XUI_OK ) iRet = __xuiTableViewUpdateContentSizeCommitted(pWidget, pData);
	if ( iRet != XUI_OK ) {
		pData->fDefaultColumnWidth = fOldDefaultColumnWidth;
		pData->fDefaultRowHeight = fOldDefaultRowHeight;
		pData->fHeaderHeight = fOldHeaderHeight;
		pData->fBorderWidth = fOldBorderWidth;
		(void)xuiWidgetSetMargin(pData->pFrame, tOldMargin);
		(void)__xuiTableViewUpdateContentSizeCommitted(pWidget, pData);
		return iRet;
	}
	if ( bMetricsChanged ) __xuiTableViewInvalidateMerges(pData);
	return XUI_OK;
}

static int __xuiTableViewLayoutComplete(xui_widget pWidget, xui_rect_t tContentRect, void* pUser)
{
	xui_table_view_data_t* pData;
	int iRet;
	(void)tContentRect;
	pData = (xui_table_view_data_t*)pUser;
	if ( (pWidget == NULL) || (pData == NULL) || (pData->pFrame == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiTableViewUpdateContentSize(pWidget, pData);
	return iRet;
}

static int __xuiTableViewPreparePaint(xui_widget pWidget)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	xui_table_view_data_t tResolved;
	int iRet;
	if ( pData == NULL || pData->pFrame == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->bPaintStylePrepared && pData->iPreparedStyleHash == xuiWidgetGetStyleHash(pWidget) ) return XUI_OK;
	__xuiTableViewResolve(pWidget, pData, &tResolved);
	iRet = xuiScrollFrameSetColors(pData->pFrame, tResolved.iBarColor, tResolved.iThumbColor, tResolved.iScrollbarHoverColor, tResolved.iScrollbarActiveColor, tResolved.iScrollbarFocusColor, tResolved.iScrollbarDisabledColor);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiInternalWidgetInvalidatePaintIfNeeded(pData->pViewport);
	if ( iRet != XUI_OK ) return iRet;
	pData->iPreparedStyleHash = xuiWidgetGetStyleHash(pWidget);
	pData->bPaintStylePrepared = 1;
	return XUI_OK;
}

static int __xuiTableViewCacheRenderOperation(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_table_view_data_t* pData;
	xui_table_view_data_t tResolved;
	xui_table_view_column_t tColumn;
	uint64_t iGeneration;
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
				iGeneration = pData->iCallbackGeneration;
				tColumn = pData->arrColumns[i];
				iHandled = pData->onHeaderRender(pWidget, i, &tColumn, pDraw, tVisible, iState, pData->pHeaderRenderUser);
				if ( !__xuiTableViewCallbackCurrent(pWidget, pData, iGeneration) ) return XUI_OK;
				if ( iHandled < 0 ) return iHandled;
				if ( iHandled ) continue;
			}
			if ( i == pData->iSortColumn ) {
				iRet = __xuiTableViewDrawFill(pProxy, pDraw, tVisible, __xuiTableViewColorWithAlpha(tResolved.iSelectedColor, 32));
				if ( iRet != XUI_OK ) return iRet;
			}
			if ( (pProxy->drawMeshTriangles != NULL || pProxy->drawRectFill != NULL) ) {
				/* Grid dividers are pixel edges, not antialiased shape strokes. */
				iRet = __xuiTableViewDrawGridFill(pProxy, pDraw, (xui_rect_t){tCell.fX + tCell.fW - 1, tHeader.fY + 4, 1, __xuiTableViewMaxFloat(0, tHeader.fH - 8)}, __xuiTableViewColorWithAlpha(tResolved.iGridColor, 210));
				if ( iRet != XUI_OK ) return iRet;
			}
			if ( tResolved.pFont != NULL ) {
				tText = (xui_rect_t){tVisible.fX + 8.0f, tVisible.fY, __xuiTableViewMaxFloat(1.0f, tVisible.fW - 18.0f), tVisible.fH};
				iRet = pProxy->drawText(pProxy, pDraw, &(xui_text_item_t){.iSize=sizeof(xui_text_item_t), .pFont=tResolved.pFont, .sText=__xuiTableViewText(pData->arrColumns[i].sTitle), .iTextSize=-1, .iFlags=XUI_TEXT_SHAPE_DEFAULT | ((XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP) & XUI_TEXT_RTL ? XUI_TEXT_SHAPE_RTL : 0)}, xuiInternalSnapRect(tText), tResolved.iHeaderTextColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
				if ( iRet != XUI_OK ) return iRet;
			}
			if ( i == pData->iSortColumn ) {
				iRet = __xuiTableViewDrawSortMark(pProxy, pDraw, tVisible, pData->bSortDescending, tResolved.iHeaderTextColor);
				if ( iRet != XUI_OK ) return iRet;
			}
		}
		if ( (pProxy->drawMeshTriangles != NULL || pProxy->drawRectFill != NULL) ) {
			iRet = __xuiTableViewDrawGridFill(pProxy, pDraw, (xui_rect_t){tHeader.fX, tHeader.fY + tHeader.fH - 1, tHeader.fW, 1}, tResolved.iGridColor);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	iBorder = ((xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget) && xuiWidgetGetEffectiveEnabled(pWidget)) ? tResolved.iFocusRingColor : tResolved.iGridColor;
	return __xuiTableViewDrawStroke(pProxy, pDraw, tRect, tResolved.fBorderWidth, iBorder);
}

static int __xuiTableViewPaintCellBody(xui_widget pWidget,xui_table_view_data_t* pData,
    const xui_table_view_data_t* pResolved,xui_proxy pProxy,xui_draw_context pDraw,
    int i,int j,float fOffsetX,float fOffsetY,xui_rect_t tViewportContent,uint64_t iCallbackGeneration,
    const xui_table_view_cell_t* pCell,xui_table_view_cell_lease_t* pLease)
{
    xui_table_view_cell_t tCellData=*pCell;
    xui_rect_t tCellContent,tCell,tFocus;
    int iState,iHandled,iRet;
    uint32_t iBackground,iGrid;
	tCellContent = __xuiTableViewCellContentRectData(pData, i, j, tCellData.iRowSpan, tCellData.iColSpan);
	if ( !__xuiTableViewRectIntersects(tCellContent, tViewportContent) ) {
		return XUI_OK;
	}
	tCell = xuiInternalSnapRect((xui_rect_t){tCellContent.fX - fOffsetX, tCellContent.fY - fOffsetY, tCellContent.fW, tCellContent.fH});
	iState = 0;
	if ( (pData->iSelectionMode == XUI_TABLE_VIEW_SELECTION_ROW && pData->iSelectedRow == i) ||
	     (pData->iSelectionMode == XUI_TABLE_VIEW_SELECTION_CELL && pData->iSelectedRow == i && pData->iSelectedColumn == j) ||
	     __xuiTableViewRowSelected(pData, i) ) {
		iState |= XUI_TABLE_CELL_SELECTED;
	}
	if ( pData->iHoverRow == i && (pData->iSelectionMode == XUI_TABLE_VIEW_SELECTION_ROW || pData->iHoverColumn == j) ) iState |= XUI_TABLE_CELL_HOVER;
	if ( __xuiTableViewRowDisabled(pData, i) || tCellData.bDisabled || !xuiWidgetGetEffectiveEnabled(pWidget) ) iState |= XUI_TABLE_CELL_DISABLED;
	if ( pData->iFocusRow == i && (pData->iSelectionMode == XUI_TABLE_VIEW_SELECTION_ROW || pData->iFocusColumn == j) ) iState |= XUI_TABLE_CELL_FOCUS;
	if ( tCellData.bEditing ) iState |= XUI_TABLE_CELL_EDITING;
	if ( tCellData.bInvalid ) iState |= XUI_TABLE_CELL_INVALID;
	if ( tCellData.bDirty ) iState |= XUI_TABLE_CELL_DIRTY;
	if ( (tCellData.iRowSpan > 1) || (tCellData.iColSpan > 1) ) iState |= XUI_TABLE_CELL_MERGED;
	iBackground = (i & 1) ? pResolved->iAltRowColor : pResolved->iRowColor;
	if ( __xuiTableViewRowDisabled(pData, i) ) iBackground = pResolved->iDisabledColor;
	if ( pData->arrRows != NULL && i < pData->iRowStateCount && pData->arrRows[i].bHasStyle && __xuiTableViewAlpha(pData->arrRows[i].iBackgroundColor) != 0 ) iBackground = pData->arrRows[i].iBackgroundColor;
	if ( pData->arrColumns[j].bHasStyle && __xuiTableViewAlpha(pData->arrColumns[j].iBackgroundColor) != 0 ) iBackground = pData->arrColumns[j].iBackgroundColor;
	if ( tCellData.bHasStyle && (pResolved->tPaintAdapter.bCellColorsPresent || __xuiTableViewAlpha(tCellData.iBackgroundColor) != 0) ) iBackground = tCellData.iBackgroundColor;
	if ( (iState & XUI_TABLE_CELL_SELECTED) != 0 ) {
		iBackground = pResolved->iSelectedColor;
	} else if ( (iState & XUI_TABLE_CELL_HOVER) != 0 ) {
		iBackground = pResolved->iHoverColor;
	}
	iHandled = 0;
	if ( pResolved->tPaintAdapter.onCellBackground != NULL ) {
		iHandled = pResolved->tPaintAdapter.onCellBackground(pWidget, i, j, &tCellData, pDraw, tCell, iState, pResolved->tPaintAdapter.pUser);
		if ( !__xuiTableViewCallbackCurrent(pWidget, pData, iCallbackGeneration) ) return pLease->owned ? XUI_ERROR_INVALID_STATE:XUI_OK;
		if ( iHandled < 0 ) return iHandled;
	}
	if ( !iHandled ) {
		iRet = __xuiTableViewDrawFill(pProxy, pDraw, tCell, iBackground);
		if ( iRet != XUI_OK ) return iRet;
	}
	iGrid = pResolved->iGridColor;
	if ( pData->arrRows != NULL && i < pData->iRowStateCount && pData->arrRows[i].bHasStyle && __xuiTableViewAlpha(pData->arrRows[i].iGridColor) != 0 ) iGrid = pData->arrRows[i].iGridColor;
	if ( pData->arrColumns[j].bHasStyle && __xuiTableViewAlpha(pData->arrColumns[j].iGridColor) != 0 ) iGrid = pData->arrColumns[j].iGridColor;
	if ( tCellData.bHasStyle && (pResolved->tPaintAdapter.bCellColorsPresent || __xuiTableViewAlpha(tCellData.iGridColor) != 0) ) iGrid = tCellData.iGridColor;
    if(pLease->owned){
        iHandled=__xuiTableViewOwnedRender(pWidget,pData,i,j,pLease,pDraw,tCell,iState);
        if(iHandled<0)return iHandled;
        if(iHandled)return XUI_OK;
    }
	if ( tCellData.onRender != NULL ) {
		iHandled = tCellData.onRender(pWidget, i, j, &tCellData, pDraw, tCell, iState, tCellData.pRenderUser);
		if ( !__xuiTableViewCallbackCurrent(pWidget, pData, iCallbackGeneration) ) return pLease->owned ? XUI_ERROR_INVALID_STATE:XUI_OK;
		if ( iHandled < 0 ) return iHandled;
		if ( iHandled ) {
			return XUI_OK;
		}
	}
	if ( pData->arrColumns[j].onRender != NULL ) {
		iHandled = pData->arrColumns[j].onRender(pWidget, i, j, &tCellData, pDraw, tCell, iState, pData->arrColumns[j].pRenderUser);
		if ( !__xuiTableViewCallbackCurrent(pWidget, pData, iCallbackGeneration) ) return pLease->owned ? XUI_ERROR_INVALID_STATE:XUI_OK;
		if ( iHandled < 0 ) return iHandled;
		if ( iHandled ) {
			return XUI_OK;
		}
	}
	if ( pData->onCellRender != NULL ) {
		iHandled = pData->onCellRender(pWidget, i, j, &tCellData, pDraw, tCell, iState, pData->pCellRenderUser);
		if ( !__xuiTableViewCallbackCurrent(pWidget, pData, iCallbackGeneration) ) return pLease->owned ? XUI_ERROR_INVALID_STATE:XUI_OK;
		if ( iHandled < 0 ) return iHandled;
		if ( iHandled ) {
			return XUI_OK;
		}
	}
	iRet = __xuiTableViewDrawCellContent(pWidget, pData, pResolved, pProxy, pDraw, i, j, &tCellData, pLease, tCell, iState);
	if ( !__xuiTableViewCallbackCurrent(pWidget, pData, iCallbackGeneration) ) return pLease->owned ? XUI_ERROR_INVALID_STATE:XUI_OK;
	if ( iRet != XUI_OK ) return iRet;
	if ( (pProxy->drawMeshTriangles != NULL || pProxy->drawRectFill != NULL) ) {
		iRet = __xuiTableViewDrawGridFill(pProxy, pDraw, (xui_rect_t){tCell.fX, tCell.fY + tCell.fH - 1, tCell.fW, 1}, iGrid);
		if ( iRet != XUI_OK ) return iRet;
		/* The bottom edge owns the corner: translucent grids blend once. */
		iRet = __xuiTableViewDrawGridFill(pProxy, pDraw, (xui_rect_t){tCell.fX + tCell.fW - 1, tCell.fY, 1, __xuiTableViewMaxFloat(0, tCell.fH - 1)}, iGrid);
		if ( iRet != XUI_OK ) return iRet;
	}
	iHandled = 0;
	if ( pResolved->tPaintAdapter.onCellDecorations != NULL ) {
		iHandled = pResolved->tPaintAdapter.onCellDecorations(pWidget, i, j, &tCellData, pDraw, tCell, iState, pResolved->tPaintAdapter.pUser);
		if ( !__xuiTableViewCallbackCurrent(pWidget, pData, iCallbackGeneration) ) return pLease->owned ? XUI_ERROR_INVALID_STATE:XUI_OK;
		if ( iHandled < 0 ) return iHandled;
	}
	if ( !iHandled && (iState & XUI_TABLE_CELL_INVALID) != 0 ) {
		iRet = __xuiTableViewDrawFill(pProxy, pDraw, (xui_rect_t){tCell.fX, tCell.fY, 3.0f, tCell.fH}, pResolved->iInvalidColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( !iHandled && (iState & XUI_TABLE_CELL_DIRTY) != 0 && pProxy->drawTriangleFill != NULL ) {
		(void)pProxy->drawTriangleFill(pProxy, pDraw,
			(xui_vec2_t){tCell.fX + tCell.fW - 8.0f, tCell.fY},
			(xui_vec2_t){tCell.fX + tCell.fW, tCell.fY},
			(xui_vec2_t){tCell.fX + tCell.fW, tCell.fY + 8.0f},
			pResolved->iDirtyColor);
	}
	if ( (iState & XUI_TABLE_CELL_EDITING) != 0 ) {
		iRet = __xuiTableViewDrawStroke(pProxy, pDraw, (xui_rect_t){tCell.fX + 1.0f, tCell.fY + 1.0f, __xuiTableViewMaxFloat(1.0f, tCell.fW - 2.0f), __xuiTableViewMaxFloat(1.0f, tCell.fH - 2.0f)}, 1.0f, pResolved->iEditingColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( ((iState & XUI_TABLE_CELL_FOCUS) != 0) &&
	     (xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget) &&
	     ((iState & XUI_TABLE_CELL_DISABLED) == 0) ) {
		tFocus = (xui_rect_t){tCell.fX + 1.0f, tCell.fY + 1.0f, __xuiTableViewMaxFloat(1.0f, tCell.fW - 2.0f), __xuiTableViewMaxFloat(1.0f, tCell.fH - 2.0f)};
		iRet = __xuiTableViewDrawStroke(pProxy, pDraw, tFocus, 1.0f, pResolved->iFocusRingColor);
		if ( iRet != XUI_OK ) return iRet;
	}
    return XUI_OK;
}
static int __xuiTableViewPaintCell(xui_widget widget,xui_table_view_data_t* data,
    const xui_table_view_data_t* resolved,xui_proxy proxy,xui_draw_context draw,
    int row,int column,float offsetX,float offsetY,xui_rect_t viewport,uint64_t generation)
{
    xui_table_view_cell_t cell;xui_table_view_cell_lease_t lease={0};
    int result=__xuiTableViewGetCell(widget,data,row,column,&cell,&lease);
    if(result==XUI_OK)result=__xuiTableViewPaintCellBody(widget,data,resolved,proxy,draw,row,column,offsetX,offsetY,viewport,generation,&cell,&lease);
    return __xuiTableViewLeaseEnd(&lease,result);
}

static int __xuiTableViewViewportRenderOperation(xui_widget pViewport, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_widget pWidget;
	xui_table_view_data_t* pData;
	xui_table_view_data_t tResolved;
	xui_proxy pProxy;
	xui_table_view_merge_column_t* pMergeColumn;
	xui_table_view_merge_t* pMerge;
	xui_rect_t tRect;
	xui_rect_t tViewportContent;
	float fOffsetX;
	float fOffsetY;
	float fViewportW;
	float fViewportH;
	float fColumnLeft;
	float fColumnWidth;
	int iStart;
	int iEnd;
	int iCounterEnd;
	int iColumnStart;
	int iColumnEnd;
	int iVisibleRow;
	int iVisibleColumn;
	int arrMergeCursors[XUI_TABLE_VIEW_COLUMN_CAPACITY];
	uint64_t iGeneration;
	uint64_t iCallbackGeneration;
	int i;
	int j;
	int iRet;

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
	if ( __xuiTableViewSyncRowCount(pWidget, pData) < 0 ) return XUI_OK;
	if ( (pData->iRowCount <= 0) || (pData->iColumnCount <= 0) ) {
		pData->iFirstVisible = 0;
		pData->iPaintVisibleCount = 0;
		return XUI_OK;
	}
	fOffsetX = 0.0f;
	fOffsetY = 0.0f;
	(void)xuiScrollFrameGetOffset(pData->pFrame, &fOffsetX, &fOffsetY);
	tViewportContent = (xui_rect_t){fOffsetX, fOffsetY, fViewportW, fViewportH};
	iStart = __xuiTableViewRowAtContentY(pData, fOffsetY);
	if ( iStart < 0 ) iStart = pData->iRowCount;
	iEnd = __xuiTableViewRowEndAtContentY(pData, fOffsetY + fViewportH);
	/* Keep the historical overscan counter without visiting its offscreen cells. */
	iCounterEnd = __xuiTableViewRowEndAtContentY(pData, fOffsetY + fViewportH + pData->fDefaultRowHeight);
	pData->iFirstVisible = iStart;
	pData->iPaintVisibleCount = (iCounterEnd > iStart) ? (iCounterEnd - iStart) : 0;
	iColumnStart = pData->iColumnCount;
	iColumnEnd = 0;
	fColumnLeft = 0.0f;
	for ( j = 0; j < pData->iColumnCount; j++ ) {
		fColumnWidth = __xuiTableViewColumnWidth(pData, j);
		if ( fColumnWidth > 0.0f && fColumnLeft + fColumnWidth > fOffsetX && fColumnLeft < fOffsetX + fViewportW ) {
			if ( iColumnStart == pData->iColumnCount ) iColumnStart = j;
			iColumnEnd = j + 1;
		}
		fColumnLeft += fColumnWidth;
	}
	if ( iStart == iEnd || iColumnStart >= iColumnEnd || fViewportH <= 0.0f ) return XUI_OK;
	iCallbackGeneration = pData->iCallbackGeneration;
	iRet = __xuiTableViewIndexMerges(pWidget, pData, iEnd - 1);
	if ( !__xuiTableViewCallbackCurrent(pWidget, pData, iCallbackGeneration) ) return XUI_OK;
	if ( iRet != XUI_OK ) return iRet;
	iGeneration = pData->iMergeGeneration;
	pData->iPaintSerial++;
	if ( pData->iPaintSerial == 0 ) {
		for ( i = 0; i < pData->iMergeCount; i++ ) pData->arrMerges[i].iPaintSerial = 0;
		pData->iPaintSerial = 1;
	}
	for ( j = iColumnStart; j < iColumnEnd; j++ ) {
		arrMergeCursors[j] = __xuiTableViewMergeBandAt(&pData->arrMergeColumns[j], iStart);
	}
	for ( iVisibleRow = iStart; iVisibleRow < iEnd; iVisibleRow++ ) {
		for ( iVisibleColumn = iColumnStart; iVisibleColumn < iColumnEnd; iVisibleColumn++ ) {
			if ( iGeneration != pData->iMergeGeneration || !__xuiTableViewCallbackCurrent(pWidget, pData, iCallbackGeneration) ) return XUI_OK;
			i = iVisibleRow;
			j = iVisibleColumn;
			if ( __xuiTableViewColumnWidth(pData, j) <= 0.0f ) continue;
			pMergeColumn = &pData->arrMergeColumns[j];
			while ( arrMergeCursors[j] < pMergeColumn->iCount && pMergeColumn->arrBands[arrMergeCursors[j]].iEnd <= i ) {
				XUI_TABLE_VIEW_INDEX_STEP(MERGE_PROBE);
				arrMergeCursors[j]++;
			}
			if ( arrMergeCursors[j] < pMergeColumn->iCount && pMergeColumn->arrBands[arrMergeCursors[j]].iStart <= i ) {
				pMerge = &pData->arrMerges[pMergeColumn->arrBands[arrMergeCursors[j]].iMerge];
				if ( !pMerge->bPaintable || pMerge->iPaintSerial == pData->iPaintSerial ) continue;
				pMerge->iPaintSerial = pData->iPaintSerial;
				i = pMerge->iRow;
				j = pMerge->iColumn;
				if ( __xuiTableViewColumnWidth(pData, j) <= 0.0f ) continue;
			}
            iRet=__xuiTableViewPaintCell(pWidget,pData,&tResolved,pProxy,pDraw,i,j,fOffsetX,fOffsetY,tViewportContent,iCallbackGeneration);
            if(iRet!=XUI_OK)return iRet;
		}
	}
	return XUI_OK;
}

static int __xuiTableViewCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_context pContext = xuiWidgetGetContext(pWidget);
	int iRet;
	xuiInternalOperationEnter(pContext);
	iRet = __xuiTableViewCacheRenderOperation(pWidget, pDraw, iStateId, pUser);
	xuiInternalOperationLeave(pContext);
	return iRet;
}

static int __xuiTableViewViewportRender(xui_widget pViewport, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_context pContext = xuiWidgetGetContext((xui_widget)pUser);
	int iRet;
	xuiInternalOperationEnter(pContext);
	iRet = __xuiTableViewViewportRenderOperation(pViewport, pDraw, iStateId, pUser);
	xuiInternalOperationLeave(pContext);
	return iRet;
}

static int __xuiTableViewTooltipResolve(xui_context pContext, xui_widget pTooltipWidget, xui_tooltip_desc_t* pDesc, void* pUser)
{
	xui_widget pWidget;
	xui_table_view_data_t* pData;
	xui_table_view_cell_t tCell;
    xui_table_view_cell_lease_t lease={0};
	int iRow;
	int iColumn;

	(void)pTooltipWidget;
	pWidget = (xui_widget)pUser;
	pData = __xuiTableViewGetData(pWidget);
	if ( (pContext == NULL) || (pData == NULL) || (pDesc == NULL) ) {
		return 0;
	}
	if ( __xuiTableViewHitCellWorld(pWidget, pData, pContext->fTooltipMouseX, pContext->fTooltipMouseY, &iRow, &iColumn) <= 0 ) {
		return 0;
	}
	if ( __xuiTableViewGetCell(pWidget, pData, iRow, iColumn, &tCell,&lease) != XUI_OK ) return 0;
	if ( (tCell.sTooltip == NULL) || (tCell.sTooltip[0] == 0) ) {
        int ended=__xuiTableViewLeaseEnd(&lease,XUI_OK);return ended==XUI_OK ? 0:ended;
    }
    if(lease.owned){
        xstrview text=xuiTableViewCellOutputGetTooltip(&lease.output);
        char* copy=xrtMalloc(text.Size+1);
        if(!copy)return __xuiTableViewLeaseEnd(&lease,XUI_ERROR_OUT_OF_MEMORY);
        memcpy(copy,text.Data,text.Size);copy[text.Size]=0;
        int ended=__xuiTableViewLeaseEnd(&lease,XUI_OK);
        if(ended!=XUI_OK || !__xuiTableViewWidgetAlive(pWidget,pData)){xrtFree(copy);return ended!=XUI_OK ? ended:XUI_ERROR_INVALID_STATE;}
        xrtFree(pData->sResolvedTooltip);pData->sResolvedTooltip=copy;tCell.sTooltip=copy;
    }else{int ended=__xuiTableViewLeaseEnd(&lease,XUI_OK);if(ended!=XUI_OK)return ended;}
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

static int __xuiTableViewAccessibleCount(xui_widget pWidget, void* pUser)
{
	xui_table_view_data_t* pData = (xui_table_view_data_t*)pUser;

	(void)pWidget;
	if ( pData == NULL || pData->iRowCount <= 0 || pData->iColumnCount <= 0 ) return 1;
	if ( pData->iRowCount > (INT_MAX - 1) / pData->iColumnCount ) return 1;
	return pData->iRowCount * pData->iColumnCount + 1;
}

static int __xuiTableViewAccessibleFocus(xui_widget pWidget,
	uint64_t* pNodeId, void* pUser)
{
	xui_table_view_data_t* pData = (xui_table_view_data_t*)pUser;
	int iColumn;

	if ( pData == NULL || pNodeId == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*pNodeId = 1;
	if ( xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) != pWidget ||
	     pData->iFocusRow < 0 || pData->iFocusRow >= pData->iRowCount ) {
		return XUI_OK;
	}
	iColumn = pData->iSelectionMode == XUI_TABLE_VIEW_SELECTION_CELL ?
		pData->iFocusColumn : __xuiTableViewFirstVisibleColumn(pData);
	if ( iColumn >= 0 && iColumn < pData->iColumnCount ) {
		*pNodeId = (uint64_t)pData->iFocusRow * (uint64_t)pData->iColumnCount +
			(uint64_t)iColumn + 2u;
	}
	return XUI_OK;
}

static int __xuiTableViewAccessibleGetBody(xui_widget pWidget, int iIndex,
	xui_accessible_node_t* pNode, void* pUser, xui_table_view_cell_lease_t* pLease)
{
	xui_table_view_data_t* pData = (xui_table_view_data_t*)pUser;
	xui_table_view_cell_t tCell;
	xui_rect_t tContent;
	xui_rect_t tLocal;
	xui_rect_t tViewport;
	xui_rect_t tWorld;
	const char* sText;
	char sBuffer[256];
	float fOffsetX;
	float fOffsetY;
	int iCellCount;
	int iCell;
	int iRow;
	int iColumn;
	int bDisabled;

	if ( pData == NULL || pNode == NULL || iIndex < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	iCellCount = __xuiTableViewAccessibleCount(pWidget, pData) - 1;
	if ( iIndex > iCellCount ) return XUI_ERROR_INVALID_ARGUMENT;
	tWorld = xuiWidgetGetWorldRect(pWidget);
	if ( iIndex == 0 ) {
		pNode->iId = 1;
		pNode->iRole = XUI_ACCESSIBLE_ROLE_TABLE;
		pNode->sName = xuiWidgetGetAccessibleName(pWidget) != NULL ?
			xuiWidgetGetAccessibleName(pWidget) : "Table";
		pNode->sDescription = xuiWidgetGetAccessibleDescription(pWidget);
		pNode->tBounds = tWorld;
		pNode->iState |= XUI_ACCESSIBLE_STATE_FOCUSABLE | XUI_ACCESSIBLE_STATE_VERTICAL;
		if ( xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget ) {
			pNode->iState |= XUI_ACCESSIBLE_STATE_FOCUSED;
		}
		pNode->iActions = XUI_ACCESSIBLE_ACTION_MASK(XUI_ACCESSIBLE_ACTION_FOCUS);
		pNode->iRowCount = pData->iRowCount;
		pNode->iColumnCount = pData->iColumnCount;
		if ( !xuiWidgetGetEffectiveEnabled(pWidget) ) {
			pNode->iState |= XUI_ACCESSIBLE_STATE_DISABLED;
			pNode->iActions = 0;
		}
		return XUI_OK;
	}
	iCell = iIndex - 1;
	iRow = iCell / pData->iColumnCount;
	iColumn = iCell % pData->iColumnCount;
	if ( iRow < 0 || iRow >= pData->iRowCount ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( __xuiTableViewGetCell(pWidget, pData, iRow, iColumn, &tCell,pLease) != XUI_OK ) {
		return XUI_ERROR_INVALID_STATE;
	}
	sText = __xuiTableViewCellText(pWidget, pData, iRow, iColumn, &tCell, pLease,
		sBuffer, (int)sizeof(sBuffer));
	if ( sText == NULL ) return pLease && pLease->failure!=XUI_OK ? pLease->failure:XUI_ERROR_INVALID_STATE;
	__xuiTableViewMakeTextSummary(sText, pData->sAccessibleName,
		(int)sizeof(pData->sAccessibleName));
	xuiInternalUtf8TrimIncomplete(pData->sAccessibleName);
	if ( pData->sAccessibleName[0] == '\0' ) {
		snprintf(pData->sAccessibleName, sizeof(pData->sAccessibleName),
			"Row %d, column %d", iRow + 1, iColumn + 1);
		pData->sAccessibleName[sizeof(pData->sAccessibleName) - 1u] = '\0';
	}
	pNode->iId = (uint64_t)iCell + 2u;
	pNode->iParentId = 1;
	pNode->iRole = XUI_ACCESSIBLE_ROLE_CELL;
	pNode->sName = pData->sAccessibleName;
	pNode->iRow = iRow;
	pNode->iColumn = iColumn;
	pNode->iRowCount = tCell.iRowSpan;
	pNode->iColumnCount = tCell.iColSpan;
	pNode->iState |= XUI_ACCESSIBLE_STATE_SELECTABLE;
	if ( pData->iSelectedRow == iRow &&
	     (pData->iSelectionMode == XUI_TABLE_VIEW_SELECTION_ROW ||
	      pData->iSelectedColumn == iColumn) ) {
		pNode->iState |= XUI_ACCESSIBLE_STATE_SELECTED;
	}
	if ( pData->iFocusRow == iRow &&
	     ((pData->iSelectionMode == XUI_TABLE_VIEW_SELECTION_CELL && pData->iFocusColumn == iColumn) ||
	      (pData->iSelectionMode == XUI_TABLE_VIEW_SELECTION_ROW &&
	       __xuiTableViewFirstVisibleColumn(pData) == iColumn)) &&
	     xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget ) {
		pNode->iState |= XUI_ACCESSIBLE_STATE_FOCUSED;
	}
	bDisabled = __xuiTableViewRowDisabled(pData, iRow) || tCell.bDisabled ||
		!xuiWidgetGetEffectiveEnabled(pWidget);
	if ( bDisabled ) pNode->iState |= XUI_ACCESSIBLE_STATE_DISABLED;
	if ( tCell.iType == XUI_TABLE_CELL_TYPE_BOOL && __xuiTableViewBoolFromText(sText) ) {
		pNode->iState |= XUI_ACCESSIBLE_STATE_CHECKED;
	}
	tContent = __xuiTableViewCellContentRectData(pData, iRow, iColumn,
		tCell.iRowSpan, tCell.iColSpan);
	tViewport = __xuiTableViewViewportRectLocal(pWidget, pData);
	fOffsetX = 0.0f;
	fOffsetY = 0.0f;
	(void)xuiScrollFrameGetOffset(pData->pFrame, &fOffsetX, &fOffsetY);
	tLocal = (xui_rect_t){tViewport.fX + tContent.fX - fOffsetX,
		tViewport.fY + tContent.fY - fOffsetY, tContent.fW, tContent.fH};
	pNode->tBounds = (xui_rect_t){tWorld.fX + tLocal.fX, tWorld.fY + tLocal.fY,
		tLocal.fW, tLocal.fH};
	if ( pData->arrColumns[iColumn].bVisible == 0 || tLocal.fW <= 0.0f || tLocal.fH <= 0.0f ||
	     tLocal.fX + tLocal.fW <= tViewport.fX || tLocal.fY + tLocal.fH <= tViewport.fY ||
	     tLocal.fX >= tViewport.fX + tViewport.fW || tLocal.fY >= tViewport.fY + tViewport.fH ) {
		pNode->iState |= XUI_ACCESSIBLE_STATE_OFFSCREEN;
	}
	pNode->iActions = XUI_ACCESSIBLE_ACTION_MASK(XUI_ACCESSIBLE_ACTION_FOCUS) |
		XUI_ACCESSIBLE_ACTION_MASK(XUI_ACCESSIBLE_ACTION_SCROLL_INTO_VIEW) |
		XUI_ACCESSIBLE_ACTION_MASK(XUI_ACCESSIBLE_ACTION_SET_SELECTION) |
		XUI_ACCESSIBLE_ACTION_MASK(XUI_ACCESSIBLE_ACTION_ACTIVATE);
	if ( bDisabled ) pNode->iActions = 0;
	return XUI_OK;
}
static int __xuiTableViewAccessibleGet(xui_widget pWidget,int iIndex,xui_accessible_node_t* pNode,void* pUser)
{
    xui_table_view_cell_lease_t lease={0};
    int result=__xuiTableViewAccessibleGetBody(pWidget,iIndex,pNode,pUser,&lease);
    return __xuiTableViewLeaseEnd(&lease,result);
}


static int __xuiTableViewAccessibleAction(xui_widget pWidget, uint64_t iNodeId,
	int iAction, const void* pPayload, void* pUser)
{
	xui_table_view_data_t* pData = (xui_table_view_data_t*)pUser;
	uint64_t iCell;
	int iRow;
	int iColumn;
	int iRet;

	(void)pPayload;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !xuiWidgetGetEffectiveEnabled(pWidget) ) return XUI_ERROR_INVALID_STATE;
	if ( iNodeId == 0 || iNodeId == 1 ) {
		return iAction == XUI_ACCESSIBLE_ACTION_FOCUS ?
			xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget) : XUI_ERROR_UNSUPPORTED;
	}
	if ( pData->iColumnCount <= 0 || iNodeId < 2 ) return XUI_ERROR_NOT_FOUND;
	iCell = iNodeId - 2u;
	if ( iCell >= (uint64_t)pData->iRowCount * (uint64_t)pData->iColumnCount ) {
		return XUI_ERROR_NOT_FOUND;
	}
	iRow = (int)(iCell / (uint64_t)pData->iColumnCount);
	iColumn = (int)(iCell % (uint64_t)pData->iColumnCount);
	iRet = __xuiTableViewCellDisabled(pWidget, pData, iRow, iColumn);
	if ( iRet < 0 ) return iRet;
	if ( iRet ) return XUI_ERROR_INVALID_STATE;
	if ( iAction == XUI_ACCESSIBLE_ACTION_SCROLL_INTO_VIEW ) {
		return xuiTableViewEnsureCellVisible(pWidget, iRow, iColumn);
	}
	if ( iAction != XUI_ACCESSIBLE_ACTION_FOCUS &&
	     iAction != XUI_ACCESSIBLE_ACTION_SET_SELECTION &&
	     iAction != XUI_ACCESSIBLE_ACTION_ACTIVATE ) return XUI_ERROR_UNSUPPORTED;
	iRet = xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
	if ( iRet != XUI_OK || !__xuiTableViewWidgetAlive(pWidget, pData) ) return iRet;
	iRet = __xuiTableViewEnsureVisibleInternal(pWidget, pData, iRow, iColumn);
	if ( iRet != XUI_OK || !__xuiTableViewWidgetAlive(pWidget, pData) ) return iRet;
	return __xuiTableViewSelect(pWidget, pData, iRow,
		(pData->iSelectionMode == XUI_TABLE_VIEW_SELECTION_ROW) ? -1 : iColumn,
		iAction != XUI_ACCESSIBLE_ACTION_FOCUS);
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
	pLayout->iMeasureContainment = XUI_MEASURE_CONTAIN_BOTH;
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
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_CONTEXT_MENU, __xuiTableViewEvent, NULL);
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
	iRet = xuiWidgetSetLayoutType(pData->pViewport, XUI_LAYOUT_MANUAL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetFlowMode(pData->pViewport, XUI_FLOW_ABSOLUTE);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetFocusable(pData->pViewport, 0);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetTabStop(pData->pViewport, 0);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetCachePolicy(pData->pViewport, &tPolicy);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetCacheRenderCallback(pData->pViewport, __xuiTableViewViewportRender, pWidget);
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
	tFrameDesc.iScrollbarMode = (pDesc != NULL && __xuiTableViewScrollbarModeValid(pDesc->iScrollbarMode)) ?
		pDesc->iScrollbarMode : XUI_SCROLLBAR_MODE_COMPACT;
	tFrameDesc.iWheelAxis = XUI_WHEEL_AXIS_VERTICAL;
	tFrameDesc.iCornerMode = XUI_SCROLL_FRAME_CORNER_AUTO;
	tFrameDesc.bContentDragEnabled = 0;
	tFrameDesc.fScrollbarSize = (tFrameDesc.iScrollbarMode == XUI_SCROLLBAR_MODE_FULL) ? 16.0f : 8.0f;
	tFrameDesc.fMinThumbSize = 18.0f;
	tFrameDesc.fWheelStep = pData->fDefaultRowHeight * 3.0f;
	tFrameDesc.iTrackColor = pData->iBarColor;
	tFrameDesc.iThumbColor = pData->iThumbColor;
	tFrameDesc.iHoverColor = pData->iScrollbarHoverColor;
	tFrameDesc.iActiveColor = pData->iScrollbarActiveColor;
	tFrameDesc.iFocusColor = pData->iScrollbarFocusColor;
	tFrameDesc.iDisabledColor = pData->iScrollbarDisabledColor;
	iRet = xuiInternalScrollFrameCreatePart(pWidget,&pData->pFrame,&tFrameDesc);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiInternalWidgetInsertPart(pWidget,pWidget,pData->pFrame,NULL);
	if ( iRet != XUI_OK ) {
		(void)xuiInternalWidgetDestroyPart(pWidget,pData->pFrame);
		pData->pFrame = NULL;
		return iRet;
	}
	iRet = xuiWidgetSetFlowMode(pData->pFrame, XUI_FLOW_BLOCK);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetSizeMode(pData->pFrame, XUI_SIZE_FILL, XUI_SIZE_FILL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetAlign(pData->pFrame, XUI_ALIGN_STRETCH, XUI_ALIGN_STRETCH);
	if ( iRet != XUI_OK ) {
		(void)xuiInternalWidgetDestroyPart(pWidget,pData->pFrame);
		pData->pFrame = NULL;
		return iRet;
	}
	pData->pViewport = xuiScrollFrameGetViewportWidget(pData->pFrame);
	if ( pData->pViewport == NULL ) {
		(void)xuiInternalWidgetDestroyPart(pWidget,pData->pFrame);
		pData->pFrame = NULL;
		return XUI_ERROR_NOT_INITIALIZED;
	}
	iRet = xuiScrollFrameSetChange(pData->pFrame, __xuiTableViewFrameChanged, pWidget);
	if ( iRet == XUI_OK ) iRet = __xuiTableViewInitViewport(pWidget, pData);
	if ( iRet == XUI_OK ) iRet = __xuiTableViewApplyFrameStyle(pWidget, pData);
	if ( iRet != XUI_OK ) {
		(void)xuiInternalWidgetDestroyPart(pWidget,pData->pFrame);
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
	iRet = xuiWidgetSetLayoutType(pWidget, XUI_LAYOUT_OVERLAY);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetFlowMode(pWidget, XUI_FLOW_ABSOLUTE);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_CLIP);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetFocusable(pWidget, 1);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetTabStop(pWidget, 1);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetPadding(pWidget,
		(xui_thickness_t){0.0f, 0.0f, 0.0f, 0.0f});
	if ( iRet == XUI_OK ) iRet = __xuiTableViewCreateFrame(pWidget, pData, pDesc);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiTableViewInitEvents(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiWidgetSetAccessibilityProvider(pWidget,
		__xuiTableViewAccessibleCount, __xuiTableViewAccessibleGet,
		__xuiTableViewAccessibleAction, pData);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiInternalAccessibilitySetFocusResolver(pWidget,
		__xuiTableViewAccessibleFocus);
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
	int i;

	(void)pWidget;
	(void)pUser;
	pData = (xui_table_view_data_t*)pTypeData;
	if ( pData != NULL ) {
        xrtFree(pData->sResolvedTooltip);pData->sResolvedTooltip=NULL;
        __xuiTableColumnStorageRelease(pData->pColumnStorage);pData->pColumnStorage=NULL;
		if ( pData->arrRows != NULL ) {
			xrtFree(pData->arrRows);
		}
		xrtFree(pData->arrRowPrefix);
		xrtFree(pData->arrMerges);
		for ( i = 0; i < XUI_TABLE_VIEW_COLUMN_CAPACITY; i++ ) {
			xrtFree(pData->arrMergeColumns[i].arrBands);
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
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.text.selected_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.check.background_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.check.mark_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.checker.primary_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.checker.secondary_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.color.fallback_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.cell.invalid_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.cell.dirty_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.cell.editing_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.picker.background_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.picker.border_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.scrollbar.track_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.scrollbar.thumb_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.scrollbar.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.scrollbar.active_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.scrollbar.focus_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiTableViewRegisterStyleProperty(pContext, pType, "tableview.scrollbar.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
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
	if ( !xuiInternalContextIsValid(pContext) || xuiInternalContextDestroyPending(pContext) ) {
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
	tDesc.onLayoutPrepare = __xuiTableViewPrepare;
	tDesc.onLayoutComplete = __xuiTableViewLayoutComplete;
	tDesc.onCacheRender = __xuiTableViewCacheRender;
	tDesc.onQueryCursor = __xuiTableViewQueryCursor;
	__xuiTableViewDefaultLayout(&tLayout);
	__xuiTableViewDefaultCachePolicy(&tPolicy);
	tDesc.tLayout = tLayout;
	tDesc.tCachePolicy = tPolicy;
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	__xuiTableViewRegisterStyleProperties(pContext, pType);
	pType->onPreparePaint = __xuiTableViewPreparePaint;
	    static const size_t fontOffsets[]={offsetof(xui_table_view_data_t,pFont)};
    pType->pFontOffsets=fontOffsets;pType->iFontSlotCount=sizeof(fontOffsets)/sizeof(fontOffsets[0]);
    return pType;
}

static int __xuiTableViewCreateWithOwner(xui_context pContext, xui_widget* ppWidget, const xui_table_view_desc_t* pDesc,xui_widget pNativeOwner)
{
	xui_widget_type pType;

	if ( ppWidget == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppWidget = NULL;
	if ( !xuiInternalContextIsValid(pContext) || !__xuiTableViewDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pType = xuiTableViewGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return (pNativeOwner ? xuiInternalWidgetCreatePart(pNativeOwner,pType,ppWidget,pDesc) : xuiWidgetCreateTyped(pContext,pType,ppWidget,pDesc));
}

XUI_API int xuiTableViewCreate(xui_context pContext, xui_widget* ppWidget, const xui_table_view_desc_t* pDesc)
{return __xuiTableViewCreateWithOwner(pContext,ppWidget,pDesc,NULL);}
int xuiInternalTableViewCreatePart(xui_widget pOwner,xui_widget* ppWidget,const xui_table_view_desc_t* pDesc)
{
    if(ppWidget)*ppWidget=NULL;
    if(!xuiInternalWidgetIsValid(pOwner) || !ppWidget)return XUI_ERROR_INVALID_ARGUMENT;
    xui_context context=xuiWidgetGetContext(pOwner);
    xuiInternalOperationEnter(context);
    int status=__xuiTableViewCreateWithOwner(context,ppWidget,pDesc,pOwner);
    xuiInternalOperationLeave(context);
    return status;
}


static int __xuiTableViewSetColumnsOperation(xui_widget pWidget, const xui_table_view_column_t* arrColumns, int iCount, xui_table_column_storage_t* storage)
{
	xui_table_view_data_t* pData;
	xui_table_view_column_t arrNewColumns[XUI_TABLE_VIEW_COLUMN_CAPACITY];
	xui_table_view_column_t arrOldColumns[XUI_TABLE_VIEW_COLUMN_CAPACITY];
	int iOldColumnCount;
	int iOldSelectedColumn;
	int iOldFocusColumn;
	int iOldHoverRow;
	int iOldHoverColumn;
	int iOldSortColumn;
	int bOldSortDescending;
	int i;
	int iRet;

	pData = __xuiTableViewGetData(pWidget);
	if ( (pData == NULL) || (iCount < 0) || (iCount > XUI_TABLE_VIEW_COLUMN_CAPACITY) || ((iCount > 0) && (arrColumns == NULL)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->onCount != NULL && __xuiTableViewSyncRowCount(pWidget, pData) < 0 ) return XUI_ERROR_INVALID_STATE;
    if (!__xuiTableViewWidgetAlive(pWidget,pData)) return XUI_ERROR_INVALID_STATE;
	memset(arrNewColumns, 0, sizeof(arrNewColumns));
	for ( i = 0; i < iCount; i++ ) {
		if ( !__xuiTableViewColumnDescValid(&arrColumns[i]) ) return XUI_ERROR_INVALID_ARGUMENT;
		arrNewColumns[i] = arrColumns[i];
		memset(&arrNewColumns[i].tRect, 0, sizeof(arrNewColumns[i].tRect));
		if ( arrNewColumns[i].fMinWidth <= 0.0f ) arrNewColumns[i].fMinWidth = 30.0f;
		if ( arrNewColumns[i].fMaxWidth > 0.0f && arrNewColumns[i].fMinWidth > arrNewColumns[i].fMaxWidth ) return XUI_ERROR_INVALID_ARGUMENT;
		if ( arrNewColumns[i].fWidth <= 0.0f ) arrNewColumns[i].fWidth = pData->fDefaultColumnWidth;
		if ( arrNewColumns[i].fWidth < arrNewColumns[i].fMinWidth ) arrNewColumns[i].fWidth = arrNewColumns[i].fMinWidth;
		if ( arrNewColumns[i].fMaxWidth > 0.0f && arrNewColumns[i].fWidth > arrNewColumns[i].fMaxWidth ) arrNewColumns[i].fWidth = arrNewColumns[i].fMaxWidth;
		if ( arrNewColumns[i].bVisibleSet == 0 ) arrNewColumns[i].bVisible = 1;
		else arrNewColumns[i].bVisible = arrNewColumns[i].bVisible != 0;
		if ( arrNewColumns[i].bResizableSet == 0 ) arrNewColumns[i].bResizable = 1;
		else arrNewColumns[i].bResizable = arrNewColumns[i].bResizable != 0;
	}
	if ( !__xuiTableViewDimensionsValid(arrNewColumns, iCount, pData->fDefaultColumnWidth,
	     pData->arrRowPrefix, pData->iRowStateCount, pData->iRowCount,
	     pData->fDefaultRowHeight) ) return XUI_ERROR_INVALID_ARGUMENT;
	memcpy(arrOldColumns, pData->arrColumns, sizeof(arrOldColumns));
	for ( i = 0; i < XUI_TABLE_VIEW_COLUMN_CAPACITY; i++ ) {
		memset(&arrOldColumns[i].tRect, 0, sizeof(arrOldColumns[i].tRect));
	}
	if ( pData->iColumnCount == iCount &&
	     memcmp(arrOldColumns, arrNewColumns, sizeof(arrNewColumns)) == 0 ) return XUI_OK;
	memcpy(arrOldColumns, pData->arrColumns, sizeof(arrOldColumns));
	iOldColumnCount = pData->iColumnCount;
	iOldSelectedColumn = pData->iSelectedColumn;
	iOldFocusColumn = pData->iFocusColumn;
	iOldHoverRow = pData->iHoverRow;
	iOldHoverColumn = pData->iHoverColumn;
	iOldSortColumn = pData->iSortColumn;
	bOldSortDescending = pData->bSortDescending;
    xui_table_column_storage_t* oldStorage = pData->pColumnStorage;
    if (!storage && __xuiTableColumnStorageAliases(oldStorage,arrColumns,iCount)) storage = oldStorage;
    if (__xuiTableColumnStorageRetain(oldStorage) != XUI_OK) return XUI_ERROR_LIMIT_EXCEEDED;
    if (__xuiTableColumnStorageRetain(storage) != XUI_OK) {
        __xuiTableColumnStorageRelease(oldStorage);
        return XUI_ERROR_LIMIT_EXCEEDED;
    }
	__xuiTableViewCancelPointer(pWidget, pData, 1);
    if (!__xuiTableViewWidgetAlive(pWidget,pData)) {
        __xuiTableColumnStorageRelease(oldStorage);
        __xuiTableColumnStorageRelease(storage);
        return XUI_ERROR_INVALID_STATE;
    }
    xui_table_column_storage_t* previousStorage = pData->pColumnStorage;
    pData->pColumnStorage = storage; /* transfer the reserved field reference */
    __xuiTableColumnStorageRelease(previousStorage);
	memcpy(pData->arrColumns, arrNewColumns, sizeof(arrNewColumns));
	pData->iColumnCount = iCount;
	if ( pData->iSelectedColumn >= iCount ) pData->iSelectedColumn = -1;
	if ( pData->iFocusColumn >= iCount ) pData->iFocusColumn = -1;
	if ( pData->iHoverColumn >= iCount ) { pData->iHoverRow = -1; pData->iHoverColumn = -1; }
	if ( pData->iSortColumn >= iCount ) { pData->iSortColumn = -1; pData->bSortDescending = 0; }
	__xuiTableViewInvalidateMerges(pData);
	iRet = __xuiTableViewUpdateContentSizeCommitted(pWidget, pData);
    if (!__xuiTableViewWidgetAlive(pWidget,pData)) {
        __xuiTableColumnStorageRelease(oldStorage);
        return iRet == XUI_OK ? XUI_ERROR_INVALID_STATE : iRet;
    }
	if ( iRet != XUI_OK ) {
        previousStorage = pData->pColumnStorage;
        pData->pColumnStorage = oldStorage; /* transfer the rollback snapshot */
        __xuiTableColumnStorageRelease(previousStorage);
		memcpy(pData->arrColumns, arrOldColumns, sizeof(arrOldColumns));
		pData->iColumnCount = iOldColumnCount;
		pData->iSelectedColumn = iOldSelectedColumn;
		pData->iFocusColumn = iOldFocusColumn;
		pData->iHoverRow = iOldHoverRow;
		pData->iHoverColumn = iOldHoverColumn;
		pData->iSortColumn = iOldSortColumn;
		pData->bSortDescending = bOldSortDescending;
		__xuiTableViewInvalidateMerges(pData);
		(void)__xuiTableViewUpdateContentSizeCommitted(pWidget, pData);
		return iRet;
	}
    __xuiTableColumnStorageRelease(oldStorage);
	pData->iChangeCount++;
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_TREE_CHANGED);
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_BOUNDS_CHANGED);
	return __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiTableViewSetRowsOperation(xui_widget pWidget, const xui_table_view_row_t* arrRows, int iCount)
{
	xui_table_view_data_t* pData;
	xui_table_view_row_t* arrNewRows;
	xui_table_view_row_prefix_t* arrPrefix;
	xui_table_view_row_t* arrOldRows;
	xui_table_view_row_prefix_t* arrOldPrefix;
	int iOldRowStateCount;
	int iOldRowCount;
	int iOldSelectedRow;
	int iOldSelectedColumn;
	int iOldHoverRow;
	int iOldHoverColumn;
	int iOldFocusRow;
	int iOldFocusColumn;
	int i;
	int iRet;

	pData = __xuiTableViewGetData(pWidget);
	if ( (pData == NULL) || (iCount < 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->onCount != NULL && __xuiTableViewSyncRowCount(pWidget, pData) < 0 ) return XUI_ERROR_INVALID_STATE;
	arrNewRows = NULL;
	arrPrefix = NULL;
	if ( iCount > 0 ) {
		if ( arrRows != NULL ) {
			for ( i = 0; i < iCount; i++ ) {
				if ( !__xuiTableViewRowDescValid(&arrRows[i]) ) return XUI_ERROR_INVALID_ARGUMENT;
			}
		}
		if ( (size_t)iCount > (size_t)-1 / sizeof(*arrNewRows) ||
		     (size_t)iCount >= (size_t)-1 / sizeof(*arrPrefix) ) return XUI_ERROR_OUT_OF_MEMORY;
		arrNewRows = (xui_table_view_row_t*)xrtMalloc((size_t)iCount * sizeof(xui_table_view_row_t));
		if ( arrNewRows == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		if ( arrRows != NULL ) {
			memcpy(arrNewRows, arrRows, (size_t)iCount * sizeof(xui_table_view_row_t));
		} else {
			memset(arrNewRows, 0, (size_t)iCount * sizeof(xui_table_view_row_t));
		}
		arrPrefix = (xui_table_view_row_prefix_t*)xrtMalloc(((size_t)iCount + 1) * sizeof(*arrPrefix));
		if ( arrPrefix == NULL ) {
			xrtFree(arrNewRows);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		arrPrefix[0] = (xui_table_view_row_prefix_t){0.0, 0};
		for ( i = 0; i < iCount; i++ ) {
			XUI_TABLE_VIEW_INDEX_STEP(ROW_READ);
			arrPrefix[i + 1] = arrPrefix[i];
			if ( arrNewRows[i].fHeight <= 0.0f ) arrPrefix[i + 1].iDefaultCount++;
			else arrPrefix[i + 1].fExplicitHeight += arrNewRows[i].fHeight;
		}
	}
	if ( !__xuiTableViewDimensionsValid(pData->arrColumns, pData->iColumnCount,
	     pData->fDefaultColumnWidth, arrPrefix, iCount,
	     (pData->onCount == NULL) ? iCount : pData->iRowCount,
	     pData->fDefaultRowHeight) ) {
		xrtFree(arrNewRows);
		xrtFree(arrPrefix);
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->iRowStateCount == iCount &&
	     ((iCount == 0) || (pData->arrRows != NULL &&
	      memcmp(pData->arrRows, arrNewRows, (size_t)iCount * sizeof(*arrNewRows)) == 0)) ) {
		xrtFree(arrNewRows);
		xrtFree(arrPrefix);
		return XUI_OK;
	}
	arrOldRows = pData->arrRows;
	arrOldPrefix = pData->arrRowPrefix;
	iOldRowStateCount = pData->iRowStateCount;
	iOldRowCount = pData->iRowCount;
	iOldSelectedRow = pData->iSelectedRow;
	iOldSelectedColumn = pData->iSelectedColumn;
	iOldHoverRow = pData->iHoverRow;
	iOldHoverColumn = pData->iHoverColumn;
	iOldFocusRow = pData->iFocusRow;
	iOldFocusColumn = pData->iFocusColumn;
	__xuiTableViewCancelPointer(pWidget, pData, 1);
	pData->arrRows = arrNewRows;
	pData->arrRowPrefix = arrPrefix;
	pData->iRowStateCount = iCount;
	__xuiTableViewInvalidateMerges(pData);
	if ( pData->onCount == NULL ) {
		pData->iRowCount = iCount;
	}
	if ( pData->iSelectedRow >= pData->iRowCount ) { pData->iSelectedRow = -1; pData->iSelectedColumn = -1; }
	if ( pData->iHoverRow >= pData->iRowCount ) { pData->iHoverRow = -1; pData->iHoverColumn = -1; }
	if ( pData->iFocusRow >= pData->iRowCount ) { pData->iFocusRow = -1; pData->iFocusColumn = -1; }
	iRet = __xuiTableViewUpdateContentSizeCommitted(pWidget, pData);
	if ( iRet != XUI_OK ) {
		pData->arrRows = arrOldRows;
		pData->arrRowPrefix = arrOldPrefix;
		pData->iRowStateCount = iOldRowStateCount;
		pData->iRowCount = iOldRowCount;
		pData->iSelectedRow = iOldSelectedRow;
		pData->iSelectedColumn = iOldSelectedColumn;
		pData->iHoverRow = iOldHoverRow;
		pData->iHoverColumn = iOldHoverColumn;
		pData->iFocusRow = iOldFocusRow;
		pData->iFocusColumn = iOldFocusColumn;
		__xuiTableViewInvalidateMerges(pData);
		(void)__xuiTableViewUpdateContentSizeCommitted(pWidget, pData);
		xrtFree(arrNewRows);
		xrtFree(arrPrefix);
		return iRet;
	}
	xrtFree(arrOldRows);
	xrtFree(arrOldPrefix);
	pData->iChangeCount++;
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_TREE_CHANGED);
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_BOUNDS_CHANGED);
	return __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}


XUI_API int xuiTableViewSetAdapterOwned(xui_widget widget,
    xui_table_view_count_proc count,void* countUser,const xui_callback_owner_t* countOwner,
    xui_table_view_cell_owned_proc cell,void* cellUser,const xui_callback_owner_t* cellOwner)
{
    xui_table_view_data_t* data=__xuiTableViewGetData(widget);
    if(!data)return XUI_ERROR_INVALID_ARGUMENT;
    xui_callback_group_entry_t entries[2]={
        {.pSlot=&data->onCountOwned,.bHasCallback=count!=NULL,.pUser=countUser,.pOwner=countOwner},
        {.pSlot=&data->onCellOwned,.bHasCallback=cell!=NULL,.pUser=cellUser,.pOwner=cellOwner}
    };
    xui_callback_group_update_t update={0};int result=xuiInternalCallbackGroupUpdateBegin(widget,entries,2,&update);
    if(result!=XUI_OK)return result;
    data->onCountOwned=count;data->pCountOwnedUser=countUser;data->onCellOwned=cell;data->pCellOwnedUser=cellUser;
    /* Replacement ends the old raw registration's lifetime. Clearing Owned
     * must not resurrect a borrowed raw userdata pointer from an older mode. */
    data->onCount=NULL;data->onCell=NULL;data->pAdapterUser=NULL;
    data->bAdapterOwned=count!=NULL || cell!=NULL;++data->iAdapterPublication;
    xuiInternalCallbackGroupUpdatePublish(&update);
    __xuiTableViewInvalidateMerges(data);
    result=__xuiTableViewUpdateContentSize(widget,data);
    if(result==XUI_OK && __xuiTableViewWidgetAlive(widget,data)){
        ++data->iChangeCount;xuiInternalAccessibilityQueue(widget,XUI_ACCESSIBLE_EVENT_TREE_CHANGED);
        xuiInternalAccessibilityQueue(widget,XUI_ACCESSIBLE_EVENT_BOUNDS_CHANGED);
        result=__xuiTableViewInvalidate(widget,data,XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
    }
    return xuiInternalCallbackGroupUpdateEnd(&update,result);
}
static int __xuiTableViewClearOwnedAdapter(xui_widget widget,xui_table_view_data_t* data)
{
    xui_callback_group_entry_t entries[2]={{.pSlot=&data->onCountOwned},{.pSlot=&data->onCellOwned}};
    xui_callback_group_update_t update={0};int result=xuiInternalCallbackGroupUpdateBegin(widget,entries,2,&update);
    if(result!=XUI_OK)return result;
    data->onCountOwned=NULL;data->pCountOwnedUser=NULL;data->onCellOwned=NULL;data->pCellOwnedUser=NULL;data->bAdapterOwned=false;
    xuiInternalCallbackGroupUpdatePublish(&update);return xuiInternalCallbackGroupUpdateEnd(&update,XUI_OK);
}
XUI_API int xuiTableViewGetCellOwned(xui_widget widget,int row,int column,xui_table_view_cell_output_t* output)
{
    xui_table_view_data_t* data=__xuiTableViewGetData(widget);if(!data || !output)return XUI_ERROR_INVALID_ARGUMENT;
    xui_call_scope_t contextScope={0};int result=xuiCallScopeBegin(xuiWidgetGetContext(widget),&contextScope);
    if(result!=XUI_OK)return result;
    xui_table_cell_scope_t cellScope={0};result=xuiTableViewCellOutputCallScopeBegin(output,&cellScope);
    xui_table_view_cell_t raw;xui_table_view_cell_lease_t lease={0};
    if(result==XUI_OK){
        result=__xuiTableViewSyncRowCount(widget,data);
        if(result>=0)result=row>=0 && row<data->iRowCount && column>=0 && column<data->iColumnCount ? XUI_OK:XUI_ERROR_INVALID_ARGUMENT;
        if(result==XUI_OK)result=__xuiTableViewGetCell(widget,data,row,column,&raw,&lease);
        if(result==XUI_OK && !lease.owned){
            /* Raw pointers and raw per-cell callbacks have no owner descriptor.
             * Refuse them explicitly, rather than laundering them into xvalue. */
            if(raw.pValue || raw.onFormat || raw.onRender)result=XUI_ERROR_UNSUPPORTED;
            else{
                xuiTableViewCellOutputInit(&lease.output);lease.owned=true;
                lease.output.iType=raw.iType;lease.output.iRowSpan=raw.iRowSpan;lease.output.iColSpan=raw.iColSpan;
                lease.output.bDisabled=raw.bDisabled;lease.output.bEditing=raw.bEditing;lease.output.bInvalid=raw.bInvalid;
                lease.output.bDirty=raw.bDirty;lease.output.bHasStyle=raw.bHasStyle;lease.output.bReadOnly=raw.bReadOnly;
                lease.output.iBackgroundColor=raw.iBackgroundColor;lease.output.iTextColor=raw.iTextColor;lease.output.iGridColor=raw.iGridColor;
                result=xuiTableViewCellOutputSetText(&lease.output,(xstrview){raw.sText,raw.sText ? strlen(raw.sText):0});
                if(result==XUI_OK)result=xuiTableViewCellOutputSetTooltip(&lease.output,(xstrview){raw.sTooltip,raw.sTooltip ? strlen(raw.sTooltip):0});
            }
        }
        if(result==XUI_OK){
            if(lease.reading){lease.reading=false;result=xuiInternalTableCellReadEnd(&lease.output);}
            if(result==XUI_OK)result=xuiInternalTableCellMove(output,&lease.output);
        }
    }
    result=__xuiTableViewLeaseEnd(&lease,result);
    xerror* first=result!=XUI_OK ? xrtTakeError():NULL;
    __xuiTableViewLeaseOutcome(xuiTableViewCellOutputCallScopeEnd(&cellScope),&result,&first);
    result=xuiInternalCallScopeEndOutcome(&contextScope,result,&first);
    xrtSetErrorTake(first);return result;
}

static int __xuiTableViewSetAdapterOperation(xui_widget pWidget, xui_table_view_count_proc onCount, xui_table_view_cell_proc onCell, void* pUser)
{
	xui_table_view_data_t* pData;
	xui_table_view_count_proc onOldCount;
	xui_table_view_cell_proc onOldCell;
	void* pOldUser;
	int iOldRowCount;
	int iOldSelectedRow;
	int iOldSelectedColumn;
	int iOldHoverRow;
	int iOldHoverColumn;
	int iOldFocusRow;
	int iOldFocusColumn;
	int iRet;
    bool oldOwned;uint64_t publication;

	pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
    oldOwned=pData->bAdapterOwned;
	onOldCount = pData->onCount;
	onOldCell = pData->onCell;
	pOldUser = pData->pAdapterUser;
	iOldRowCount = pData->iRowCount;
	iOldSelectedRow = pData->iSelectedRow;
	iOldSelectedColumn = pData->iSelectedColumn;
	iOldHoverRow = pData->iHoverRow;
	iOldHoverColumn = pData->iHoverColumn;
	iOldFocusRow = pData->iFocusRow;
	iOldFocusColumn = pData->iFocusColumn;
	__xuiTableViewCancelPointer(pWidget, pData, 1);
    if(!__xuiTableViewWidgetAlive(pWidget,pData))return XUI_ERROR_INVALID_STATE;
    publication=++pData->iAdapterPublication;pData->bAdapterOwned=false;
	pData->onCount = onCount;
	pData->onCell = onCell;
	pData->pAdapterUser = pUser;
	__xuiTableViewInvalidateMerges(pData);
	iRet = __xuiTableViewUpdateContentSize(pWidget, pData);
	if ( iRet != XUI_OK ) {
		if ( __xuiTableViewWidgetAlive(pWidget, pData) && pData->iAdapterPublication==publication && !pData->bAdapterOwned && pData->onCount == onCount &&
		     pData->onCell == onCell && pData->pAdapterUser == pUser ) {
            pData->bAdapterOwned=oldOwned;
			pData->onCount = onOldCount;
			pData->onCell = onOldCell;
			pData->pAdapterUser = pOldUser;
			pData->iRowCount = iOldRowCount;
			pData->iSelectedRow = iOldSelectedRow;
			pData->iSelectedColumn = iOldSelectedColumn;
			pData->iHoverRow = iOldHoverRow;
			pData->iHoverColumn = iOldHoverColumn;
			pData->iFocusRow = iOldFocusRow;
			pData->iFocusColumn = iOldFocusColumn;
			__xuiTableViewInvalidateMerges(pData);
			(void)__xuiTableViewUpdateContentSizeCommitted(pWidget, pData);
		}
		return iRet;
	}
    if(oldOwned && pData->iAdapterPublication==publication){
        iRet=__xuiTableViewClearOwnedAdapter(pWidget,pData);if(iRet!=XUI_OK)return iRet;
        if(!__xuiTableViewWidgetAlive(pWidget,pData))return XUI_ERROR_INVALID_STATE;
        if(pData->iAdapterPublication!=publication)return XUI_OK;
    }
	pData->iChangeCount++;
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_TREE_CHANGED);
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_BOUNDS_CHANGED);
	return __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTableViewRefreshAdapter(xui_widget pWidget)
{
	return xuiTableViewRefresh(pWidget);
}

XUI_API int xuiTableViewSetSortOwned(xui_widget pWidget, xui_table_view_sort_proc onSort, void* pUser, const xui_callback_owner_t* pOwner)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	xui_callback_update_t tUpdate = {0};
	int iResult = xuiInternalCallbackUpdateBegin(pWidget, &pData->onSort, onSort != NULL, pUser, pOwner, &tUpdate);
	if ( iResult != XUI_OK ) return iResult;
	pData->onSort = onSort;
	pData->pSortUser = pUser;
	xuiInternalCallbackUpdatePublish(&tUpdate);
	return xuiInternalCallbackUpdateEnd(&tUpdate, XUI_OK);
}

XUI_API int xuiTableViewSetSort(xui_widget pWidget, xui_table_view_sort_proc onSort, void* pUser)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( xuiInternalCallbackSlotOwned(pWidget, &pData->onSort) )
		return xuiTableViewSetSortOwned(pWidget, onSort, pUser, NULL);
	pData->onSort = onSort;
	pData->pSortUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTableViewSetSelectOwned(xui_widget pWidget, xui_table_view_select_proc onSelect, void* pUser, const xui_callback_owner_t* pOwner)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	xui_callback_update_t tUpdate = {0};
	int iResult = xuiInternalCallbackUpdateBegin(pWidget, &pData->onSelect, onSelect != NULL, pUser, pOwner, &tUpdate);
	if ( iResult != XUI_OK ) return iResult;
	pData->onSelect = onSelect;
	pData->pSelectUser = pUser;
	xuiInternalCallbackUpdatePublish(&tUpdate);
	return xuiInternalCallbackUpdateEnd(&tUpdate, XUI_OK);
}

XUI_API int xuiTableViewSetSelect(xui_widget pWidget, xui_table_view_select_proc onSelect, void* pUser)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( xuiInternalCallbackSlotOwned(pWidget, &pData->onSelect) )
		return xuiTableViewSetSelectOwned(pWidget, onSelect, pUser, NULL);
	pData->onSelect = onSelect;
	pData->pSelectUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTableViewSetColumnResizeOwned(xui_widget pWidget, xui_table_view_column_resize_proc onResize, void* pUser, const xui_callback_owner_t* pOwner)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	xui_callback_update_t tUpdate = {0};
	int iResult = xuiInternalCallbackUpdateBegin(pWidget, &pData->onColumnResize, onResize != NULL, pUser, pOwner, &tUpdate);
	if ( iResult != XUI_OK ) return iResult;
	pData->onColumnResize = onResize;
	pData->pColumnResizeUser = pUser;
	xuiInternalCallbackUpdatePublish(&tUpdate);
	return xuiInternalCallbackUpdateEnd(&tUpdate, XUI_OK);
}

XUI_API int xuiTableViewSetColumnResize(xui_widget pWidget, xui_table_view_column_resize_proc onResize, void* pUser)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( xuiInternalCallbackSlotOwned(pWidget, &pData->onColumnResize) )
		return xuiTableViewSetColumnResizeOwned(pWidget, onResize, pUser, NULL);
	pData->onColumnResize = onResize;
	pData->pColumnResizeUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTableViewSetHoverOwned(xui_widget pWidget, xui_table_view_hover_proc onHover, void* pUser, const xui_callback_owner_t* pOwner)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	xui_callback_update_t tUpdate = {0};
	int iResult = xuiInternalCallbackUpdateBegin(pWidget, &pData->onHover, onHover != NULL, pUser, pOwner, &tUpdate);
	if ( iResult != XUI_OK ) return iResult;
	pData->onHover = onHover;
	pData->pHoverUser = pUser;
	xuiInternalCallbackUpdatePublish(&tUpdate);
	return xuiInternalCallbackUpdateEnd(&tUpdate, XUI_OK);
}

XUI_API int xuiTableViewSetHover(xui_widget pWidget, xui_table_view_hover_proc onHover, void* pUser)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( xuiInternalCallbackSlotOwned(pWidget, &pData->onHover) )
		return xuiTableViewSetHoverOwned(pWidget, onHover, pUser, NULL);
	pData->onHover = onHover;
	pData->pHoverUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTableViewSetContextMenuOwned(xui_widget pWidget, xui_table_view_context_proc onContext, void* pUser, const xui_callback_owner_t* pOwner)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	xui_callback_update_t ownedUpdate={0};
    int ownedStatus=xuiInternalCallbackUpdateBegin(pWidget,&pData->onContext,onContext!=NULL,pUser,pOwner,&ownedUpdate);
    if(ownedStatus!=XUI_OK)return ownedStatus;
    pData->onContext = onContext;
	pData->pContextUser = pUser;
    xuiInternalCallbackUpdatePublish(&ownedUpdate);
	return xuiInternalCallbackUpdateEnd(&ownedUpdate,XUI_OK);
}

XUI_API int xuiTableViewSetContextMenu(xui_widget pWidget, xui_table_view_context_proc onContext, void* pUser)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if(xuiInternalCallbackSlotOwned(pWidget,&pData->onContext))return xuiTableViewSetContextMenuOwned(pWidget,onContext,pUser,NULL);
    pData->onContext = onContext;
	pData->pContextUser = pUser;
	return XUI_OK;
}

XUI_API int xuiTableViewSetMergeProviderOwned(xui_widget pWidget, xui_table_view_merge_proc onMerge, void* pUser, const xui_callback_owner_t* pOwner)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiTableViewInvalidate(pWidget, pData,
		XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( iRet != XUI_OK ) return iRet;
	xui_callback_update_t ownedUpdate={0};
    int ownedStatus=xuiInternalCallbackUpdateBegin(pWidget,&pData->onMerge,onMerge!=NULL,pUser,pOwner,&ownedUpdate);
    if(ownedStatus!=XUI_OK)return ownedStatus;
    pData->onMerge = onMerge;
	pData->pMergeUser = pUser;
    xuiInternalCallbackUpdatePublish(&ownedUpdate);
	__xuiTableViewInvalidateMerges(pData);
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_TREE_CHANGED);
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_BOUNDS_CHANGED);
	return xuiInternalCallbackUpdateEnd(&ownedUpdate,XUI_OK);
}

XUI_API int xuiTableViewSetMergeProvider(xui_widget pWidget, xui_table_view_merge_proc onMerge, void* pUser)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiTableViewInvalidate(pWidget, pData,
		XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( iRet != XUI_OK ) return iRet;
	if(xuiInternalCallbackSlotOwned(pWidget,&pData->onMerge))return xuiTableViewSetMergeProviderOwned(pWidget,onMerge,pUser,NULL);
    pData->onMerge = onMerge;
	pData->pMergeUser = pUser;
	__xuiTableViewInvalidateMerges(pData);
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_TREE_CHANGED);
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_BOUNDS_CHANGED);
	return XUI_OK;
}

XUI_API int xuiTableViewSetHeaderRenderer(xui_widget pWidget, xui_table_view_header_renderer_proc onRender, void* pUser)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiTableViewInvalidate(pWidget, pData,
		XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( iRet != XUI_OK ) return iRet;
	pData->onHeaderRender = onRender;
	pData->pHeaderRenderUser = pUser;
	pData->iCallbackGeneration++;
	return XUI_OK;
}

XUI_API int xuiTableViewSetCellRenderer(xui_widget pWidget, xui_table_view_cell_renderer_proc onRender, void* pUser)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiTableViewInvalidate(pWidget, pData,
		XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( iRet != XUI_OK ) return iRet;
	pData->onCellRender = onRender;
	pData->pCellRenderUser = pUser;
	pData->iCallbackGeneration++;
	return XUI_OK;
}

XUI_API int xuiTableViewSetColumnFormatter(xui_widget pWidget, int iColumn, xui_table_view_format_proc onFormat, void* pUser)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	int iRet;
	if ( (pData == NULL) || (iColumn < 0) || (iColumn >= pData->iColumnCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiTableViewInvalidate(pWidget, pData,
		XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( iRet != XUI_OK ) return iRet;
	pData->arrColumns[iColumn].onFormat = onFormat;
	pData->arrColumns[iColumn].pFormatUser = pUser;
	pData->iCallbackGeneration++;
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_NODE_CHANGED);
	return XUI_OK;
}

XUI_API int xuiTableViewSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->pFont == pFont ) return XUI_OK;
    xui_font_slot_update_t fontUpdate={0};
    int fontStatus=xuiInternalFontSlotPrepare(pWidget->pContext,&pData->pFont,pFont,pWidget->bTypeInitialized!=0,false,&fontUpdate);
    if(fontStatus!=XUI_OK)return fontStatus;
    fontUpdate.widget=pWidget;
	iRet = __xuiTableViewInvalidate(pWidget, pData,
		XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( iRet != XUI_OK ) return xuiInternalFontSlotEnd(&fontUpdate,true,iRet);
	fontStatus=xuiInternalFontSlotPublish(&fontUpdate);
    if(fontStatus!=XUI_OK)return xuiInternalFontSlotEnd(&fontUpdate,true,fontStatus);
	return xuiInternalFontSlotEnd(&fontUpdate,true,XUI_OK);
}

XUI_API xui_font xuiTableViewGetFont(xui_widget pWidget)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

static int __xuiTableViewSetDefaultMetricsOperation(xui_widget pWidget, float fColumnWidth, float fRowHeight, float fHeaderHeight)
{
	xui_table_view_data_t* pData;
	float fOldBaseColumnWidth;
	float fOldBaseRowHeight;
	float fOldBaseHeaderHeight;
	int iRet;

	pData = __xuiTableViewGetData(pWidget);
	if ( (pData == NULL) ||
	     ((fColumnWidth != 0.0f) && ((fColumnWidth <= 0.0f) || !__xuiTableViewFloatValid(fColumnWidth))) ||
	     ((fRowHeight != 0.0f) && ((fRowHeight <= 0.0f) || !__xuiTableViewFloatValid(fRowHeight))) ||
	     ((fHeaderHeight != 0.0f) && !__xuiTableViewFloatValid(fHeaderHeight)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->onCount != NULL && __xuiTableViewSyncRowCount(pWidget, pData) < 0 ) return XUI_ERROR_INVALID_STATE;
	if ( fColumnWidth <= 0.0f ) fColumnWidth = pData->fBaseDefaultColumnWidth;
	if ( fRowHeight <= 0.0f ) fRowHeight = pData->fBaseDefaultRowHeight;
	if ( (double)fHeaderHeight + (double)pData->fBaseBorderWidth * 2.0 >
	     (double)XUI_LAYOUT_UNBOUNDED ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !__xuiTableViewDimensionsValid(pData->arrColumns, pData->iColumnCount,
	     fColumnWidth, pData->arrRowPrefix, pData->iRowStateCount,
	     pData->iRowCount, fRowHeight) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->fBaseDefaultColumnWidth == fColumnWidth &&
	     pData->fBaseDefaultRowHeight == fRowHeight &&
	     pData->fBaseHeaderHeight == fHeaderHeight ) return XUI_OK;
	fOldBaseColumnWidth = pData->fBaseDefaultColumnWidth;
	fOldBaseRowHeight = pData->fBaseDefaultRowHeight;
	fOldBaseHeaderHeight = pData->fBaseHeaderHeight;
	__xuiTableViewCancelPointer(pWidget, pData, 1);
	pData->fBaseDefaultColumnWidth = fColumnWidth;
	pData->fBaseDefaultRowHeight = fRowHeight;
	pData->fBaseHeaderHeight = fHeaderHeight;
	iRet = __xuiTableViewPrepare(pWidget, pData);
	if ( iRet != XUI_OK ) {
		pData->fBaseDefaultColumnWidth = fOldBaseColumnWidth;
		pData->fBaseDefaultRowHeight = fOldBaseRowHeight;
		pData->fBaseHeaderHeight = fOldBaseHeaderHeight;
		(void)__xuiTableViewPrepare(pWidget, pData);
		return iRet;
	}
	pData->iChangeCount++;
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_BOUNDS_CHANGED);
	return __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiTableViewGetDefaultMetrics(xui_widget pWidget, float* pColumnWidth, float* pRowHeight, float* pHeaderHeight)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pColumnWidth != NULL ) *pColumnWidth = pData->fBaseDefaultColumnWidth;
	if ( pRowHeight != NULL ) *pRowHeight = pData->fBaseDefaultRowHeight;
	if ( pHeaderHeight != NULL ) *pHeaderHeight = pData->fBaseHeaderHeight;
	return XUI_OK;
}

XUI_API int xuiTableViewSetSelectionMode(xui_widget pWidget, int iMode)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	int iRet;
	if ( (pData == NULL) || !__xuiTableViewSelectionModeValid(iMode) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iSelectionMode == iMode ) return XUI_OK;
	iRet = __xuiTableViewInvalidate(pWidget, pData,
		XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( iRet != XUI_OK ) return iRet;
	__xuiTableViewCancelPointer(pWidget, pData, 1);
	pData->iSelectionMode = iMode;
	if ( iMode == XUI_TABLE_VIEW_SELECTION_ROW ) {
		pData->iSelectedColumn = -1;
		pData->iFocusColumn = -1;
	}
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_SELECTION_CHANGED);
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_FOCUS_CHANGED);
	return XUI_OK;
}

XUI_API int xuiTableViewGetSelectionMode(xui_widget pWidget)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	return (pData != NULL) ? pData->iSelectionMode : XUI_TABLE_VIEW_SELECTION_CELL;
}

static int __xuiTableViewSetSelectedRowOperation(xui_widget pWidget, int iRow)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	int iOldMode;
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iOldMode = pData->iSelectionMode;
	pData->iSelectionMode = XUI_TABLE_VIEW_SELECTION_ROW;
	iRet = __xuiTableViewSelect(pWidget, pData, iRow, -1, 0);
	if ( iRet != XUI_OK && __xuiTableViewWidgetAlive(pWidget, pData) ) {
		pData->iSelectionMode = iOldMode;
	}
	return iRet;
}

XUI_API int xuiTableViewGetSelectedRow(xui_widget pWidget)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	return (pData != NULL) ? pData->iSelectedRow : -1;
}

static int __xuiTableViewSetSelectedCellOperation(xui_widget pWidget, int iRow, int iColumn)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	int iOldMode;
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iOldMode = pData->iSelectionMode;
	pData->iSelectionMode = XUI_TABLE_VIEW_SELECTION_CELL;
	iRet = __xuiTableViewSelect(pWidget, pData, iRow, iColumn, 0);
	if ( iRet != XUI_OK && __xuiTableViewWidgetAlive(pWidget, pData) ) {
		pData->iSelectionMode = iOldMode;
	}
	return iRet;
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

static int __xuiTableViewSetColumnWidthOperation(xui_widget pWidget, int iColumn, float fWidth)
{
	xui_table_view_data_t* pData;
	float fMin;
	float fMax;
	int iRet;

	pData = __xuiTableViewGetData(pWidget);
	if ( (pData == NULL) || (iColumn < 0) || (iColumn >= pData->iColumnCount) || (fWidth <= 0.0f) || !__xuiTableViewFloatValid(fWidth) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->onCount != NULL && __xuiTableViewSyncRowCount(pWidget, pData) < 0 ) return XUI_ERROR_INVALID_STATE;
	fMin = pData->arrColumns[iColumn].fMinWidth;
	fMax = pData->arrColumns[iColumn].fMaxWidth;
	if ( fMin <= 0.0f ) fMin = 30.0f;
	if ( fWidth < fMin ) fWidth = fMin;
	if ( (fMax > 0.0f) && (fWidth > fMax) ) fWidth = fMax;
	if ( pData->arrColumns[iColumn].fWidth == fWidth ) return XUI_OK;
	__xuiTableViewCancelPointer(pWidget, pData, 1);
	{
		float fOldWidth = pData->arrColumns[iColumn].fWidth;
		pData->arrColumns[iColumn].fWidth = fWidth;
		if ( !__xuiTableViewDimensionsValid(pData->arrColumns, pData->iColumnCount,
		     pData->fDefaultColumnWidth, pData->arrRowPrefix, pData->iRowStateCount,
		     pData->iRowCount, pData->fDefaultRowHeight) ) {
			pData->arrColumns[iColumn].fWidth = fOldWidth;
			return XUI_ERROR_INVALID_ARGUMENT;
		}
		__xuiTableViewInvalidateMerges(pData);
		iRet = __xuiTableViewUpdateContentSizeCommitted(pWidget, pData);
		if ( iRet != XUI_OK ) {
			pData->arrColumns[iColumn].fWidth = fOldWidth;
			__xuiTableViewInvalidateMerges(pData);
			(void)__xuiTableViewUpdateContentSizeCommitted(pWidget, pData);
			return iRet;
		}
	}
	pData->iChangeCount++;
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_BOUNDS_CHANGED);
	return __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiTableViewGetRowCountOperation(xui_widget pWidget)
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

static int __xuiTableViewGetCellContentRectOperation(xui_widget pWidget, int iRow, int iColumn, xui_rect_t* pRect)
{
	xui_table_view_data_t* pData;
	xui_table_view_cell_t tCell;
    xui_table_view_cell_lease_t lease={0};
	int iOwnerRow;
	int iOwnerColumn;
	int iRet;

	if ( pRect != NULL ) memset(pRect, 0, sizeof(*pRect));
	pData = __xuiTableViewGetData(pWidget);
	if ( (pData == NULL) || (pRect == NULL) || (iRow < 0) || (iColumn < 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiTableViewSyncRowCount(pWidget, pData);
	if ( iRet < 0 ) return iRet;
	if ( iRow >= iRet || iColumn >= pData->iColumnCount ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiTableViewMergeOwner(pWidget, pData, iRow, iColumn, &iOwnerRow, &iOwnerColumn);
	if ( iRet < 0 ) return iRet;
	iRet = __xuiTableViewGetCell(pWidget, pData, iOwnerRow, iOwnerColumn, &tCell,&lease);
	if ( iRet != XUI_OK ) return iRet;
	*pRect = __xuiTableViewCellContentRectData(pData, iOwnerRow, iOwnerColumn, tCell.iRowSpan, tCell.iColSpan);
	return __xuiTableViewLeaseEnd(&lease,XUI_OK);
}

static int __xuiTableViewGetCellRectOperation(xui_widget pWidget, int iRow, int iColumn, xui_rect_t* pRect)
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

static int __xuiTableViewGetItemAtOperation(xui_widget pWidget, float fX, float fY, int* pRow, int* pColumn)
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
	int iRet;

	if ( pRow != NULL ) *pRow = -1;
	if ( pColumn != NULL ) *pColumn = -1;
	pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( __xuiTableViewSyncRowCount(pWidget, pData) < 0 ) return XUI_ERROR_INVALID_STATE;
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
	iRet = __xuiTableViewMergeOwner(pWidget, pData, iRow, iColumn, &iOwnerRow, &iOwnerColumn);
	if ( iRet < 0 ) return iRet;
	if ( pRow != NULL ) *pRow = iOwnerRow;
	if ( pColumn != NULL ) *pColumn = iOwnerColumn;
	return XUI_OK;
}

XUI_API int xuiTableViewSetColors(xui_widget pWidget, uint32_t iBackground, uint32_t iHeader, uint32_t iRow, uint32_t iSelected, uint32_t iGrid, uint32_t iText)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iBackgroundColor == iBackground && pData->iHeaderColor == iHeader &&
	     pData->iHeaderTextColor == iText && pData->iRowColor == iRow &&
	     pData->iSelectedColor == iSelected && pData->iGridColor == iGrid &&
	     pData->iTextColor == iText ) return XUI_OK;
	iRet = __xuiTableViewInvalidate(pWidget, pData,
		XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( iRet != XUI_OK ) return iRet;
	pData->iBackgroundColor = iBackground;
	pData->iHeaderColor = iHeader;
	pData->iHeaderTextColor = iText;
	pData->iRowColor = iRow;
	pData->iAltRowColor = XUI_COLOR_RGBA(249, 252, 255, 255);
	pData->iHoverColor = XUI_COLOR_RGBA(231, 243, 253, 255);
	pData->iSelectedColor = iSelected;
	pData->iGridColor = iGrid;
	pData->iTextColor = iText;
	return XUI_OK;
}

XUI_API int xuiTableViewSetColorStyle(xui_widget pWidget, const xui_table_view_colors_t* pColors)
{
	xui_table_view_data_t* pData = __xuiTableViewGetData(pWidget);
	xui_table_view_data_t tCandidate;
	xui_table_view_data_t tResolved;
	int iRet;
	if ( (pData == NULL) || (pColors == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	tCandidate = *pData;
	tCandidate.iBackgroundColor = pColors->iBackgroundColor;
	tCandidate.iHeaderColor = pColors->iHeaderColor;
	tCandidate.iHeaderTextColor = pColors->iHeaderTextColor;
	tCandidate.iRowColor = pColors->iRowColor;
	tCandidate.iAltRowColor = pColors->iAltRowColor;
	tCandidate.iHoverColor = pColors->iHoverColor;
	tCandidate.iSelectedColor = pColors->iSelectedColor;
	tCandidate.iDisabledColor = pColors->iDisabledColor;
	tCandidate.iGridColor = pColors->iGridColor;
	tCandidate.iTextColor = pColors->iTextColor;
	tCandidate.iDisabledTextColor = pColors->iDisabledTextColor;
	tCandidate.iFocusRingColor = pColors->iFocusRingColor;
	tCandidate.iBarColor = pColors->iBarColor;
	tCandidate.iThumbColor = pColors->iThumbColor;
	__xuiTableViewResolve(pWidget, &tCandidate, &tResolved);
	iRet = __xuiTableViewInvalidate(pWidget, pData,
		XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( iRet != XUI_OK ) return iRet;
	if ( pData->pFrame != NULL ) {
		iRet = xuiScrollFrameSetColors(pData->pFrame,
			tResolved.iBarColor,
			tResolved.iThumbColor,
			tResolved.iScrollbarHoverColor,
			tResolved.iScrollbarActiveColor,
			tResolved.iScrollbarFocusColor,
			tResolved.iScrollbarDisabledColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	pData->iBackgroundColor = tCandidate.iBackgroundColor;
	pData->iHeaderColor = tCandidate.iHeaderColor;
	pData->iHeaderTextColor = tCandidate.iHeaderTextColor;
	pData->iRowColor = tCandidate.iRowColor;
	pData->iAltRowColor = tCandidate.iAltRowColor;
	pData->iHoverColor = tCandidate.iHoverColor;
	pData->iSelectedColor = tCandidate.iSelectedColor;
	pData->iDisabledColor = tCandidate.iDisabledColor;
	pData->iGridColor = tCandidate.iGridColor;
	pData->iTextColor = tCandidate.iTextColor;
	pData->iDisabledTextColor = tCandidate.iDisabledTextColor;
	pData->iFocusRingColor = tCandidate.iFocusRingColor;
	pData->iBarColor = tCandidate.iBarColor;
	pData->iThumbColor = tCandidate.iThumbColor;
	return XUI_OK;
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
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iDisabledTextColor == iColor ) return XUI_OK;
	iRet = __xuiTableViewInvalidate(pWidget, pData,
		XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( iRet != XUI_OK ) return iRet;
	pData->iDisabledTextColor = iColor;
	return XUI_OK;
}

XUI_API int xuiTableViewSetScrollbarColors(xui_widget pWidget, uint32_t iTrack, uint32_t iThumb, uint32_t iHover, uint32_t iActive, uint32_t iFocus, uint32_t iDisabled)
{
	xui_table_view_data_t tCandidate;
	xui_table_view_data_t tResolved;
	xui_table_view_data_t* pData;
	int iRet;

	pData = __xuiTableViewGetData(pWidget);
	if ( (pData == NULL) || (pData->pFrame == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iBarColor == iTrack && pData->iThumbColor == iThumb &&
	     pData->iScrollbarHoverColor == iHover && pData->iScrollbarActiveColor == iActive &&
	     pData->iScrollbarFocusColor == iFocus && pData->iScrollbarDisabledColor == iDisabled ) return XUI_OK;
	tCandidate = *pData;
	tCandidate.iBarColor = iTrack;
	tCandidate.iThumbColor = iThumb;
	tCandidate.iScrollbarHoverColor = iHover;
	tCandidate.iScrollbarActiveColor = iActive;
	tCandidate.iScrollbarFocusColor = iFocus;
	tCandidate.iScrollbarDisabledColor = iDisabled;
	__xuiTableViewResolve(pWidget, &tCandidate, &tResolved);
	iRet = __xuiTableViewInvalidate(pWidget, pData,
		XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiScrollFrameSetColors(pData->pFrame, tResolved.iBarColor, tResolved.iThumbColor, tResolved.iScrollbarHoverColor, tResolved.iScrollbarActiveColor, tResolved.iScrollbarFocusColor, tResolved.iScrollbarDisabledColor);
	if ( iRet != XUI_OK ) return iRet;
	pData->iBarColor = iTrack;
	pData->iThumbColor = iThumb;
	pData->iScrollbarHoverColor = iHover;
	pData->iScrollbarActiveColor = iActive;
	pData->iScrollbarFocusColor = iFocus;
	pData->iScrollbarDisabledColor = iDisabled;
	return XUI_OK;
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

static int __xuiTableViewRefreshOperation(xui_widget pWidget)
{
	xui_table_view_data_t* pData;
	int iRet;

	pData = __xuiTableViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiTableViewInvalidateMerges(pData);
	iRet = __xuiTableViewUpdateContentSize(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	pData->iChangeCount++;
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_TREE_CHANGED);
	xuiInternalAccessibilityQueue(pWidget, XUI_ACCESSIBLE_EVENT_BOUNDS_CHANGED);
	return __xuiTableViewInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiTableViewEnsureCellVisibleOperation(xui_widget pWidget, int iRow, int iColumn)
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

/* Public provider operations own the barrier until all their callers have unwound. */

XUI_API int xuiTableViewSetColumns(xui_widget pWidget, const xui_table_view_column_t* arrColumns, int iCount)
{
	xui_context pContext = xuiWidgetGetContext(pWidget);
	int iRet;
	xuiInternalOperationEnter(pContext);
	iRet = __xuiTableViewSetColumnsOperation(pWidget, arrColumns, iCount, NULL);
	xuiInternalOperationLeave(pContext);
	return iRet;
}

/* The ordinary setter keeps its existing borrowed-string contract. This
 * variant owns one contiguous copy of both text fields. Other pointer fields
 * (callbacks/user data) keep their explicit C borrowing contract. */
XUI_API int xuiTableViewSetColumnsCopy(xui_widget widget,
    const xui_table_view_column_t* columns, int count)
{
    if (!__xuiTableViewGetData(widget) || count < 0 ||
        count > XUI_TABLE_VIEW_COLUMN_CAPACITY || (count && !columns))
        return XUI_ERROR_INVALID_ARGUMENT;
    size_t bytes = sizeof(xui_table_column_storage_t) + (size_t)count * sizeof(*columns);
    for (int i = 0; i < count; ++i) {
        if (!__xuiTableViewColumnDescValid(&columns[i])) return XUI_ERROR_INVALID_ARGUMENT;
        const char* texts[2] = { columns[i].sId, columns[i].sTitle };
        for (int field = 0; field < 2; ++field) if (texts[field]) {
            size_t length = strlen(texts[field]);
            if (length == (size_t)-1 || length + 1 > (size_t)-1 - bytes)
                return XUI_ERROR_LIMIT_EXCEEDED;
            bytes += length + 1;
        }
    }
    xui_table_column_storage_t* storage = count ? xrtMalloc(bytes) : NULL;
    if (count && !storage) return XUI_ERROR_OUT_OF_MEMORY;
    if (storage) {
        storage->references = 1;
        storage->bytes = bytes;
        memcpy(storage->columns, columns, (size_t)count * sizeof(*columns));
        char* text = (char*)(storage->columns + count);
        for (int i = 0; i < count; ++i) {
            const char* inputs[2] = { columns[i].sId, columns[i].sTitle };
            const char** outputs[2] = { &storage->columns[i].sId, &storage->columns[i].sTitle };
            for (int field = 0; field < 2; ++field) if (inputs[field]) {
                size_t length = strlen(inputs[field]) + 1;
                memcpy(text, inputs[field], length);
                *outputs[field] = text;
                text += length;
            }
        }
    }
    xui_context context = xuiWidgetGetContext(widget);
    xuiInternalOperationEnter(context);
    int result = __xuiTableViewSetColumnsOperation(widget,
        storage ? storage->columns : NULL, count, storage);
    xuiInternalOperationLeave(context);
    __xuiTableColumnStorageRelease(storage);
    return result;
}

XUI_API int xuiTableViewSetRows(xui_widget pWidget, const xui_table_view_row_t* arrRows, int iCount)
{
	xui_context pContext = xuiWidgetGetContext(pWidget);
	int iRet;
	xuiInternalOperationEnter(pContext);
	iRet = __xuiTableViewSetRowsOperation(pWidget, arrRows, iCount);
	xuiInternalOperationLeave(pContext);
	return iRet;
}

XUI_API int xuiTableViewSetAdapter(xui_widget pWidget, xui_table_view_count_proc onCount, xui_table_view_cell_proc onCell, void* pUser)
{
	xui_context pContext = xuiWidgetGetContext(pWidget);
	int iRet;
	xuiInternalOperationEnter(pContext);
	iRet = __xuiTableViewSetAdapterOperation(pWidget, onCount, onCell, pUser);
	xuiInternalOperationLeave(pContext);
	return iRet;
}

XUI_API int xuiTableViewSetDefaultMetrics(xui_widget pWidget, float fColumnWidth, float fRowHeight, float fHeaderHeight)
{
	xui_context pContext = xuiWidgetGetContext(pWidget);
	int iRet;
	xuiInternalOperationEnter(pContext);
	iRet = __xuiTableViewSetDefaultMetricsOperation(pWidget, fColumnWidth, fRowHeight, fHeaderHeight);
	xuiInternalOperationLeave(pContext);
	return iRet;
}

XUI_API int xuiTableViewSetSelectedRow(xui_widget pWidget, int iRow)
{
	xui_context pContext = xuiWidgetGetContext(pWidget);
	int iRet;
	xuiInternalOperationEnter(pContext);
	iRet = __xuiTableViewSetSelectedRowOperation(pWidget, iRow);
	xuiInternalOperationLeave(pContext);
	return iRet;
}

XUI_API int xuiTableViewSetSelectedCell(xui_widget pWidget, int iRow, int iColumn)
{
	xui_context pContext = xuiWidgetGetContext(pWidget);
	int iRet;
	xuiInternalOperationEnter(pContext);
	iRet = __xuiTableViewSetSelectedCellOperation(pWidget, iRow, iColumn);
	xuiInternalOperationLeave(pContext);
	return iRet;
}

XUI_API int xuiTableViewSetColumnWidth(xui_widget pWidget, int iColumn, float fWidth)
{
	xui_context pContext = xuiWidgetGetContext(pWidget);
	int iRet;
	xuiInternalOperationEnter(pContext);
	iRet = __xuiTableViewSetColumnWidthOperation(pWidget, iColumn, fWidth);
	xuiInternalOperationLeave(pContext);
	return iRet;
}

XUI_API int xuiTableViewGetRowCount(xui_widget pWidget)
{
	xui_context pContext = xuiWidgetGetContext(pWidget);
	int iRet;
	xuiInternalOperationEnter(pContext);
	iRet = __xuiTableViewGetRowCountOperation(pWidget);
	xuiInternalOperationLeave(pContext);
	return (iRet < 0) ? 0 : iRet;
}

XUI_API int xuiTableViewGetCellContentRect(xui_widget pWidget, int iRow, int iColumn, xui_rect_t* pRect)
{
	xui_context pContext = xuiWidgetGetContext(pWidget);
	int iRet;
	xuiInternalOperationEnter(pContext);
	iRet = __xuiTableViewGetCellContentRectOperation(pWidget, iRow, iColumn, pRect);
	xuiInternalOperationLeave(pContext);
	return iRet;
}

XUI_API int xuiTableViewGetCellRect(xui_widget pWidget, int iRow, int iColumn, xui_rect_t* pRect)
{
	xui_context pContext = xuiWidgetGetContext(pWidget);
	int iRet;
	xuiInternalOperationEnter(pContext);
	iRet = __xuiTableViewGetCellRectOperation(pWidget, iRow, iColumn, pRect);
	xuiInternalOperationLeave(pContext);
	return iRet;
}

XUI_API int xuiTableViewGetItemAt(xui_widget pWidget, float fX, float fY, int* pRow, int* pColumn)
{
	xui_context pContext = xuiWidgetGetContext(pWidget);
	int iRet;
	xuiInternalOperationEnter(pContext);
	iRet = __xuiTableViewGetItemAtOperation(pWidget, fX, fY, pRow, pColumn);
	xuiInternalOperationLeave(pContext);
	return iRet;
}

XUI_API int xuiTableViewRefresh(xui_widget pWidget)
{
	xui_context pContext = xuiWidgetGetContext(pWidget);
	int iRet;
	xuiInternalOperationEnter(pContext);
	iRet = __xuiTableViewRefreshOperation(pWidget);
	xuiInternalOperationLeave(pContext);
	return iRet;
}

XUI_API int xuiTableViewEnsureCellVisible(xui_widget pWidget, int iRow, int iColumn)
{
	xui_context pContext = xuiWidgetGetContext(pWidget);
	int iRet;
	xuiInternalOperationEnter(pContext);
	iRet = __xuiTableViewEnsureCellVisibleOperation(pWidget, iRow, iColumn);
	xuiInternalOperationLeave(pContext);
	return iRet;
}

#endif
