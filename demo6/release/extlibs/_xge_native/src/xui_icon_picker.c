#include "xui_internal.h"

#include <stdio.h>
#include <string.h>

#define XUI_ICON_PICKER_DEFAULT_ROWS		4
#define XUI_ICON_PICKER_DEFAULT_COLUMNS		6
#define XUI_ICON_PICKER_DEFAULT_CELL_W		48.0f
#define XUI_ICON_PICKER_DEFAULT_CELL_H		48.0f
#define XUI_ICON_PICKER_DEFAULT_GAP		4.0f
#define XUI_ICON_PICKER_DEFAULT_ICON_PAD		7.0f
#define XUI_ICON_PICKER_POPUP_PADDING		4.0f

typedef struct xui_icon_picker_data_t {
	xui_widget pPopup;
	xui_widget pPanel;
	xui_widget pFrame;
	xui_widget pViewport;
	xui_icon_category pCategory;
	xui_icon_id* arrIds;
	int iIconCount;
	int iIconCapacity;
	uint32_t iCategoryGeneration;
	xui_icon_id iSelectedId;
	int iSelectedIndex;
	int iHoverIndex;
	int iFocusIndex;
	int iTextMode;
	int iVisibleRows;
	int iVisibleColumns;
	int iPopupPlacement;
	char* sPlaceholder;
	xui_font pFont;
	xui_icon_picker_change_proc onChange;
	void* pChangeUser;
	xui_icon_picker_format_proc onFormat;
	void* pFormatUser;
	float fCellWidth;
	float fCellHeight;
	float fIconPadding;
	float fGapX;
	float fGapY;
	xui_thickness_t tValuePadding;
	float fBorderWidth;
	float fScrollbarSize;
	xui_rect_t tButtonRect;
	xui_rect_t tValueRect;
	uint32_t iTextColor;
	uint32_t iPlaceholderColor;
	uint32_t iDisabledTextColor;
	uint32_t iBackgroundColor;
	uint32_t iHoverBackgroundColor;
	uint32_t iOpenBackgroundColor;
	uint32_t iDisabledBackgroundColor;
	uint32_t iBorderColor;
	uint32_t iHoverBorderColor;
	uint32_t iFocusBorderColor;
	uint32_t iArrowColor;
	uint32_t iDisabledArrowColor;
	uint32_t iButtonColor;
	uint32_t iButtonHoverColor;
	uint32_t iButtonOpenColor;
	uint32_t iPopupPanelColor;
	uint32_t iPopupBorderColor;
	uint32_t iPopupShadowColor;
	uint32_t iItemHoverColor;
	uint32_t iItemSelectedColor;
	uint32_t iItemFocusColor;
	int iChangeCount;
	char sTooltip[256];
} xui_icon_picker_data_t;

static xui_icon_picker_data_t* __xuiIconPickerGetData(xui_widget pWidget);
static int __xuiIconPickerApplyPopup(xui_widget pWidget, xui_icon_picker_data_t* pData);
static int __xuiIconPickerSyncCategory(xui_widget pWidget, xui_icon_picker_data_t* pData);

static float __xuiIconPickerMin(float fA, float fB)
{
	return (fA < fB) ? fA : fB;
}

static float __xuiIconPickerMax(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static uint32_t __xuiIconPickerAlpha(uint32_t iColor)
{
	return iColor & 0xffu;
}

static int __xuiIconPickerFloatValid(float fValue)
{
	return (fValue == fValue) && (fValue >= 0.0f) && (fValue <= XUI_LAYOUT_UNBOUNDED);
}

static int __xuiIconPickerTextModeValid(int iMode)
{
	return (iMode >= XUI_ICON_PICKER_TEXT_NONE) && (iMode <= XUI_ICON_PICKER_TEXT_CUSTOM);
}

static int __xuiIconPickerPlacementValid(int iPlacement)
{
	return (iPlacement == XUI_ICON_PICKER_POPUP_AUTO) ||
	       (iPlacement == XUI_ICON_PICKER_POPUP_BOTTOM) ||
	       (iPlacement == XUI_ICON_PICKER_POPUP_TOP);
}

static int __xuiIconPickerThicknessValid(xui_thickness_t tValue)
{
	return __xuiIconPickerFloatValid(tValue.fLeft) &&
	       __xuiIconPickerFloatValid(tValue.fTop) &&
	       __xuiIconPickerFloatValid(tValue.fRight) &&
	       __xuiIconPickerFloatValid(tValue.fBottom);
}

static int __xuiIconPickerDescValid(const xui_icon_picker_desc_t* pDesc)
{
	if ( pDesc == NULL ) return 1;
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) return 0;
	if ( (pDesc->iVisibleRows < 0) || (pDesc->iVisibleColumns < 0) ) return 0;
	if ( !__xuiIconPickerTextModeValid(pDesc->iTextMode) && (pDesc->iTextMode != 0) ) return 0;
	if ( !__xuiIconPickerPlacementValid(pDesc->iPopupPlacement) ) return 0;
	if ( !__xuiIconPickerFloatValid(pDesc->fCellWidth) ||
	     !__xuiIconPickerFloatValid(pDesc->fCellHeight) ||
	     !__xuiIconPickerFloatValid(pDesc->fIconPadding) ||
	     !__xuiIconPickerFloatValid(pDesc->fGapX) ||
	     !__xuiIconPickerFloatValid(pDesc->fGapY) ||
	     !__xuiIconPickerFloatValid(pDesc->fBorderWidth) ||
	     !__xuiIconPickerFloatValid(pDesc->fScrollbarSize) ) return 0;
	if ( !__xuiIconPickerThicknessValid(pDesc->tValuePadding) ) return 0;
	return 1;
}

static char* __xuiIconPickerDup(const char* sText)
{
	char* sCopy;
	size_t iLen;

	if ( sText == NULL ) sText = "";
	iLen = strlen(sText);
	sCopy = (char*)xrtMalloc(iLen + 1u);
	if ( sCopy == NULL ) return NULL;
	memcpy(sCopy, sText, iLen + 1u);
	return sCopy;
}

static void __xuiIconPickerDefaults(xui_icon_picker_data_t* pData)
{
	memset(pData, 0, sizeof(*pData));
	pData->iSelectedId = XUI_ICON_ID_INVALID;
	pData->iSelectedIndex = -1;
	pData->iHoverIndex = -1;
	pData->iFocusIndex = -1;
	pData->iTextMode = XUI_ICON_PICKER_TEXT_ID;
	pData->iVisibleRows = XUI_ICON_PICKER_DEFAULT_ROWS;
	pData->iVisibleColumns = XUI_ICON_PICKER_DEFAULT_COLUMNS;
	pData->iPopupPlacement = XUI_ICON_PICKER_POPUP_AUTO;
	pData->fCellWidth = XUI_ICON_PICKER_DEFAULT_CELL_W;
	pData->fCellHeight = XUI_ICON_PICKER_DEFAULT_CELL_H;
	pData->fIconPadding = XUI_ICON_PICKER_DEFAULT_ICON_PAD;
	pData->fGapX = XUI_ICON_PICKER_DEFAULT_GAP;
	pData->fGapY = XUI_ICON_PICKER_DEFAULT_GAP;
	pData->tValuePadding = (xui_thickness_t){7.0f, 4.0f, 7.0f, 4.0f};
	pData->fBorderWidth = 1.0f;
	pData->fScrollbarSize = 8.0f;
	pData->iTextColor = XUI_COLOR_RGBA(33, 55, 79, 255);
	pData->iPlaceholderColor = XUI_COLOR_RGBA(116, 132, 150, 220);
	pData->iDisabledTextColor = XUI_COLOR_RGBA(132, 146, 162, 210);
	pData->iBackgroundColor = XUI_COLOR_RGBA(248, 252, 255, 255);
	pData->iHoverBackgroundColor = XUI_COLOR_RGBA(232, 244, 252, 255);
	pData->iOpenBackgroundColor = XUI_COLOR_RGBA(222, 239, 252, 255);
	pData->iDisabledBackgroundColor = XUI_COLOR_RGBA(235, 240, 245, 255);
	pData->iBorderColor = XUI_COLOR_RGBA(132, 174, 214, 255);
	pData->iHoverBorderColor = XUI_COLOR_RGBA(78, 148, 208, 255);
	pData->iFocusBorderColor = XUI_COLOR_RGBA(42, 126, 205, 255);
	pData->iArrowColor = XUI_COLOR_RGBA(42, 92, 136, 255);
	pData->iDisabledArrowColor = XUI_COLOR_RGBA(134, 148, 164, 180);
	pData->iButtonColor = XUI_COLOR_RGBA(236, 246, 253, 255);
	pData->iButtonHoverColor = XUI_COLOR_RGBA(220, 238, 251, 255);
	pData->iButtonOpenColor = XUI_COLOR_RGBA(207, 229, 247, 255);
	pData->iPopupPanelColor = XUI_COLOR_RGBA(250, 253, 255, 255);
	pData->iPopupBorderColor = XUI_COLOR_RGBA(122, 164, 202, 255);
	pData->iPopupShadowColor = XUI_COLOR_RGBA(44, 70, 96, 46);
	pData->iItemHoverColor = XUI_COLOR_RGBA(220, 238, 251, 255);
	pData->iItemSelectedColor = XUI_COLOR_RGBA(190, 220, 246, 255);
	pData->iItemFocusColor = XUI_COLOR_RGBA(42, 126, 205, 255);
}

static int __xuiIconPickerSetPlaceholderData(xui_icon_picker_data_t* pData, const char* sText)
{
	char* sNew;

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	sNew = __xuiIconPickerDup(sText);
	if ( sNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( pData->sPlaceholder != NULL ) xrtFree(pData->sPlaceholder);
	pData->sPlaceholder = sNew;
	return XUI_OK;
}

static int __xuiIconPickerApplyCategoryData(xui_icon_picker_data_t* pData, xui_icon_category pCategory)
{
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pCategory == pData->pCategory ) return XUI_OK;
	if ( pCategory != NULL && xuiIconCategoryAddRef(pCategory) != XUI_OK ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->pCategory != NULL ) {
		(void)xuiIconCategoryRelease(pData->pCategory);
	}
	pData->pCategory = pCategory;
	pData->iCategoryGeneration = 0u;
	return XUI_OK;
}

static void __xuiIconPickerApplyDesc(xui_icon_picker_data_t* pData, const xui_icon_picker_desc_t* pDesc)
{
	if ( (pData == NULL) || (pDesc == NULL) ) return;
	pData->pFont = pDesc->pFont;
	pData->iSelectedId = pDesc->iSelectedId;
	if ( __xuiIconPickerTextModeValid(pDesc->iTextMode) ) pData->iTextMode = pDesc->iTextMode;
	if ( pDesc->iVisibleRows > 0 ) pData->iVisibleRows = pDesc->iVisibleRows;
	if ( pDesc->iVisibleColumns > 0 ) pData->iVisibleColumns = pDesc->iVisibleColumns;
	if ( __xuiIconPickerPlacementValid(pDesc->iPopupPlacement) ) pData->iPopupPlacement = pDesc->iPopupPlacement;
	if ( pDesc->fCellWidth > 0.0f ) pData->fCellWidth = pDesc->fCellWidth;
	if ( pDesc->fCellHeight > 0.0f ) pData->fCellHeight = pDesc->fCellHeight;
	if ( pDesc->fIconPadding > 0.0f ) pData->fIconPadding = pDesc->fIconPadding;
	if ( pDesc->fGapX > 0.0f ) pData->fGapX = pDesc->fGapX;
	if ( pDesc->fGapY > 0.0f ) pData->fGapY = pDesc->fGapY;
	if ( pDesc->tValuePadding.fLeft > 0.0f || pDesc->tValuePadding.fTop > 0.0f ||
	     pDesc->tValuePadding.fRight > 0.0f || pDesc->tValuePadding.fBottom > 0.0f ) {
		pData->tValuePadding = pDesc->tValuePadding;
	}
	if ( pDesc->fBorderWidth > 0.0f ) pData->fBorderWidth = pDesc->fBorderWidth;
	if ( pDesc->fScrollbarSize > 0.0f ) pData->fScrollbarSize = __xuiIconPickerMin(pDesc->fScrollbarSize, 8.0f);
	if ( __xuiIconPickerAlpha(pDesc->iTextColor) ) pData->iTextColor = pDesc->iTextColor;
	if ( __xuiIconPickerAlpha(pDesc->iPlaceholderColor) ) pData->iPlaceholderColor = pDesc->iPlaceholderColor;
	if ( __xuiIconPickerAlpha(pDesc->iDisabledTextColor) ) pData->iDisabledTextColor = pDesc->iDisabledTextColor;
	if ( __xuiIconPickerAlpha(pDesc->iBackgroundColor) ) pData->iBackgroundColor = pDesc->iBackgroundColor;
	if ( __xuiIconPickerAlpha(pDesc->iHoverBackgroundColor) ) pData->iHoverBackgroundColor = pDesc->iHoverBackgroundColor;
	if ( __xuiIconPickerAlpha(pDesc->iOpenBackgroundColor) ) pData->iOpenBackgroundColor = pDesc->iOpenBackgroundColor;
	if ( __xuiIconPickerAlpha(pDesc->iDisabledBackgroundColor) ) pData->iDisabledBackgroundColor = pDesc->iDisabledBackgroundColor;
	if ( __xuiIconPickerAlpha(pDesc->iBorderColor) ) pData->iBorderColor = pDesc->iBorderColor;
	if ( __xuiIconPickerAlpha(pDesc->iHoverBorderColor) ) pData->iHoverBorderColor = pDesc->iHoverBorderColor;
	if ( __xuiIconPickerAlpha(pDesc->iFocusBorderColor) ) pData->iFocusBorderColor = pDesc->iFocusBorderColor;
	if ( __xuiIconPickerAlpha(pDesc->iArrowColor) ) pData->iArrowColor = pDesc->iArrowColor;
	if ( __xuiIconPickerAlpha(pDesc->iDisabledArrowColor) ) pData->iDisabledArrowColor = pDesc->iDisabledArrowColor;
	if ( __xuiIconPickerAlpha(pDesc->iButtonColor) ) pData->iButtonColor = pDesc->iButtonColor;
	if ( __xuiIconPickerAlpha(pDesc->iButtonHoverColor) ) pData->iButtonHoverColor = pDesc->iButtonHoverColor;
	if ( __xuiIconPickerAlpha(pDesc->iButtonOpenColor) ) pData->iButtonOpenColor = pDesc->iButtonOpenColor;
	if ( __xuiIconPickerAlpha(pDesc->iPopupPanelColor) ) pData->iPopupPanelColor = pDesc->iPopupPanelColor;
	if ( __xuiIconPickerAlpha(pDesc->iPopupBorderColor) ) pData->iPopupBorderColor = pDesc->iPopupBorderColor;
	if ( pDesc->iPopupShadowColor != 0u ) pData->iPopupShadowColor = pDesc->iPopupShadowColor;
	if ( __xuiIconPickerAlpha(pDesc->iItemHoverColor) ) pData->iItemHoverColor = pDesc->iItemHoverColor;
	if ( __xuiIconPickerAlpha(pDesc->iItemSelectedColor) ) pData->iItemSelectedColor = pDesc->iItemSelectedColor;
	if ( __xuiIconPickerAlpha(pDesc->iItemFocusColor) ) pData->iItemFocusColor = pDesc->iItemFocusColor;
}

static int __xuiIconPickerFindIndex(const xui_icon_picker_data_t* pData, xui_icon_id iId)
{
	int i;

	if ( (pData == NULL) || (iId == XUI_ICON_ID_INVALID) ) return -1;
	for ( i = 0; i < pData->iIconCount; i++ ) {
		if ( pData->arrIds[i] == iId ) return i;
	}
	return -1;
}

static int __xuiIconPickerReserveIds(xui_icon_picker_data_t* pData, int iNeeded)
{
	xui_icon_id* arrNew;
	int iCapacity;

	if ( pData == NULL || iNeeded < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iNeeded <= pData->iIconCapacity ) return XUI_OK;
	iCapacity = (pData->iIconCapacity > 0) ? pData->iIconCapacity : 16;
	while ( iCapacity < iNeeded ) {
		if ( iCapacity > 0x3fffffff ) return XUI_ERROR_OUT_OF_MEMORY;
		iCapacity *= 2;
	}
	arrNew = (xui_icon_id*)xrtRealloc(pData->arrIds, (size_t)iCapacity * sizeof(*arrNew));
	if ( arrNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	pData->arrIds = arrNew;
	pData->iIconCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiIconPickerSyncCategory(xui_widget pWidget, xui_icon_picker_data_t* pData)
{
	xui_icon pIcon;
	uint32_t iGeneration;
	int iCount;
	int iRet;
	int i;

	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	iGeneration = (pData->pCategory != NULL) ? xuiIconCategoryGetGeneration(pData->pCategory) : 0u;
	iCount = (pData->pCategory != NULL) ? xuiIconCategoryGetIconCount(pData->pCategory) : 0;
	if ( iGeneration == pData->iCategoryGeneration && iCount == pData->iIconCount ) return XUI_OK;
	iRet = __xuiIconPickerReserveIds(pData, iCount);
	if ( iRet != XUI_OK ) return iRet;
	pData->iIconCount = 0;
	for ( i = 0; i < iCount; i++ ) {
		pIcon = xuiIconCategoryGetIconAt(pData->pCategory, i);
		if ( pIcon != NULL ) {
			pData->arrIds[pData->iIconCount++] = xuiIconGetId(pIcon);
		}
	}
	pData->iCategoryGeneration = iGeneration;
	pData->iSelectedIndex = __xuiIconPickerFindIndex(pData, pData->iSelectedId);
	if ( pData->iSelectedIndex < 0 ) pData->iSelectedId = XUI_ICON_ID_INVALID;
	if ( pData->iFocusIndex >= pData->iIconCount ) pData->iFocusIndex = pData->iIconCount - 1;
	if ( pData->iHoverIndex >= pData->iIconCount ) pData->iHoverIndex = -1;
	(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( pData->pViewport != NULL ) {
		(void)xuiWidgetInvalidate(pData->pViewport, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return XUI_OK;
}

static float __xuiIconPickerGridWidth(const xui_icon_picker_data_t* pData)
{
	return pData->fCellWidth * (float)pData->iVisibleColumns +
	       pData->fGapX * (float)(pData->iVisibleColumns - 1);
}

static int __xuiIconPickerTotalRows(const xui_icon_picker_data_t* pData)
{
	if ( (pData == NULL) || (pData->iVisibleColumns <= 0) || (pData->iIconCount <= 0) ) return 0;
	return (pData->iIconCount + pData->iVisibleColumns - 1) / pData->iVisibleColumns;
}

static float __xuiIconPickerVisibleHeight(const xui_icon_picker_data_t* pData)
{
	return pData->fCellHeight * (float)pData->iVisibleRows +
	       pData->fGapY * (float)(pData->iVisibleRows - 1);
}

static float __xuiIconPickerContentHeight(const xui_icon_picker_data_t* pData)
{
	int iRows;

	iRows = __xuiIconPickerTotalRows(pData);
	if ( iRows <= 0 ) return __xuiIconPickerVisibleHeight(pData);
	return pData->fCellHeight * (float)iRows + pData->fGapY * (float)(iRows - 1);
}

static int __xuiIconPickerNeedScrollbar(const xui_icon_picker_data_t* pData)
{
	return __xuiIconPickerTotalRows(pData) > pData->iVisibleRows;
}

static xui_rect_t __xuiIconPickerItemContentRect(const xui_icon_picker_data_t* pData, int iIndex)
{
	int iColumn;
	int iRow;

	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iIconCount) ) {
		return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	}
	iColumn = iIndex % pData->iVisibleColumns;
	iRow = iIndex / pData->iVisibleColumns;
	return (xui_rect_t){
		(float)iColumn * (pData->fCellWidth + pData->fGapX),
		(float)iRow * (pData->fCellHeight + pData->fGapY),
		pData->fCellWidth,
		pData->fCellHeight
	};
}

static int __xuiIconPickerUpdateRects(xui_widget pWidget, xui_icon_picker_data_t* pData)
{
	xui_rect_t tRect;
	float fButtonWidth;

	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	tRect = xuiWidgetGetContentRect(pWidget);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	fButtonWidth = __xuiIconPickerMin(36.0f, __xuiIconPickerMax(24.0f, tRect.fH));
	fButtonWidth = __xuiIconPickerMin(fButtonWidth, tRect.fW);
	pData->tButtonRect = xuiInternalSnapRect((xui_rect_t){tRect.fW - fButtonWidth, 0.0f, fButtonWidth, tRect.fH});
	pData->tValueRect = (xui_rect_t){0.0f, 0.0f, __xuiIconPickerMax(0.0f, tRect.fW - fButtonWidth), tRect.fH};
	pData->tValueRect.fX += pData->tValuePadding.fLeft;
	pData->tValueRect.fY += pData->tValuePadding.fTop;
	pData->tValueRect.fW -= pData->tValuePadding.fLeft + pData->tValuePadding.fRight;
	pData->tValueRect.fH -= pData->tValuePadding.fTop + pData->tValuePadding.fBottom;
	pData->tValueRect.fW = __xuiIconPickerMax(0.0f, pData->tValueRect.fW);
	pData->tValueRect.fH = __xuiIconPickerMax(0.0f, pData->tValueRect.fH);
	pData->tValueRect = xuiInternalSnapRect(pData->tValueRect);
	return XUI_OK;
}

static uint32_t __xuiIconPickerState(xui_widget pWidget, xui_icon_picker_data_t* pData)
{
	uint32_t iState;

	iState = xuiWidgetGetInputState(pWidget);
	if ( !xuiWidgetGetEnabled(pWidget) ) iState |= XUI_WIDGET_STATE_DISABLED;
	if ( xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget ||
	     xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pData->pViewport ) {
		iState |= XUI_WIDGET_STATE_FOCUS;
	}
	if ( pData->pPopup != NULL && xuiPopupIsOpen(pData->pPopup) ) {
		iState |= XUI_ICON_PICKER_STATE_OPEN;
	}
	return iState;
}

static int __xuiIconPickerSyncState(xui_widget pWidget, xui_icon_picker_data_t* pData)
{
	return xuiWidgetSetStateId(pWidget, __xuiIconPickerState(pWidget, pData));
}

static int __xuiIconPickerFormatValue(xui_widget pWidget, xui_icon_picker_data_t* pData, xui_icon pIcon, char* sBuffer, int iCapacity)
{
	const char* sText;
	int iRet;

	if ( (sBuffer == NULL) || (iCapacity <= 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	sBuffer[0] = '\0';
	if ( pIcon == NULL || pData == NULL || pData->iTextMode == XUI_ICON_PICKER_TEXT_NONE ) return XUI_OK;
	switch ( pData->iTextMode ) {
	case XUI_ICON_PICKER_TEXT_ID:
		snprintf(sBuffer, (size_t)iCapacity, "%u", (unsigned)xuiIconGetId(pIcon));
		break;
	case XUI_ICON_PICKER_TEXT_NAME:
		sText = xuiIconGetName(pIcon);
		snprintf(sBuffer, (size_t)iCapacity, "%s", (sText != NULL) ? sText : "");
		break;
	case XUI_ICON_PICKER_TEXT_DISPLAY_NAME:
		sText = xuiIconGetDisplayName(pIcon);
		if ( sText == NULL || sText[0] == '\0' ) sText = xuiIconGetName(pIcon);
		snprintf(sBuffer, (size_t)iCapacity, "%s", (sText != NULL) ? sText : "");
		break;
	case XUI_ICON_PICKER_TEXT_CUSTOM:
		if ( pData->onFormat != NULL ) {
			iRet = pData->onFormat(pWidget, pIcon, sBuffer, iCapacity, pData->pFormatUser);
			if ( iRet != XUI_OK ) {
				sBuffer[0] = '\0';
				return iRet;
			}
			sBuffer[iCapacity - 1] = '\0';
		}
		break;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiIconPickerDrawIcon(xui_widget pWidget, xui_draw_context pDraw, xui_icon pIcon, xui_rect_t tRect, int bEnabled)
{
	xui_icon_draw_desc_t tDesc;
	xui_painter_t tPainter;

	if ( pIcon == NULL || tRect.fW <= 0.0f || tRect.fH <= 0.0f ) return XUI_OK;
	memset(&tPainter, 0, sizeof(tPainter));
	tPainter.iMagic = XUI_PAINTER_MAGIC;
	tPainter.pContext = xuiWidgetGetContext(pWidget);
	tPainter.pDraw = pDraw;
	xuiIconDrawDescDefault(&tDesc);
	tDesc.iFlags |= XUI_ICON_DRAW_IGNORE_CATEGORY_SIZE;
	tDesc.iFitMode = XUI_IMAGE_CONTAIN;
	tDesc.iColor = bEnabled ? XUI_COLOR_WHITE : XUI_COLOR_RGBA(255, 255, 255, 128);
	return xuiIconDraw(&tPainter, pIcon, xuiInternalSnapRect(tRect), &tDesc);
}

static int __xuiIconPickerDrawChevron(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, int bOpen, uint32_t iColor)
{
	float fCX;
	float fCY;
	float fStep;
	int iRet;

	if ( pProxy->drawLine == NULL || __xuiIconPickerAlpha(iColor) == 0u ) return XUI_OK;
	fCX = tRect.fX + tRect.fW * 0.5f;
	fCY = tRect.fY + tRect.fH * 0.5f;
	fStep = __xuiIconPickerMin(4.0f, __xuiIconPickerMin(tRect.fW, tRect.fH) * 0.18f);
	if ( bOpen ) {
		iRet = pProxy->drawLine(pProxy, pDraw, fCX - fStep, fCY + 1.5f, fCX, fCY - 1.5f, 1.0f, iColor);
		if ( iRet != XUI_OK ) return iRet;
		return pProxy->drawLine(pProxy, pDraw, fCX, fCY - 1.5f, fCX + fStep, fCY + 1.5f, 1.0f, iColor);
	}
	iRet = pProxy->drawLine(pProxy, pDraw, fCX - fStep, fCY - 1.5f, fCX, fCY + 1.5f, 1.0f, iColor);
	if ( iRet != XUI_OK ) return iRet;
	return pProxy->drawLine(pProxy, pDraw, fCX, fCY + 1.5f, fCX + fStep, fCY - 1.5f, 1.0f, iColor);
}

static int __xuiIconPickerCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_icon_picker_data_t* pData;
	xui_proxy pProxy;
	xui_icon pIcon;
	xui_font pFont;
	xui_rect_t tRect;
	xui_rect_t tIcon;
	xui_rect_t tText;
	uint32_t iBackground;
	uint32_t iButton;
	uint32_t iBorder;
	uint32_t iTextColor;
	uint32_t iArrow;
	float fIconSide;
	float fGap;
	char sValue[256];
	int bEnabled;
	int bOpen;
	int bHover;
	int bFocus;
	int iRet;

	(void)iStateId;
	(void)pUser;
	pData = __xuiIconPickerGetData(pWidget);
	if ( (pData == NULL) || (pDraw == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	(void)__xuiIconPickerSyncCategory(pWidget, pData);
	(void)__xuiIconPickerUpdateRects(pWidget, pData);
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	bEnabled = xuiWidgetGetEnabled(pWidget);
	bOpen = (pData->pPopup != NULL && xuiPopupIsOpen(pData->pPopup));
	bHover = (xuiWidgetGetInputState(pWidget) & XUI_WIDGET_STATE_HOVER) != 0u;
	bFocus = (xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget ||
	          xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pData->pViewport);
	iBackground = !bEnabled ? pData->iDisabledBackgroundColor :
	              bOpen ? pData->iOpenBackgroundColor :
	              bHover ? pData->iHoverBackgroundColor : pData->iBackgroundColor;
	iButton = bOpen ? pData->iButtonOpenColor :
	          bHover ? pData->iButtonHoverColor : pData->iButtonColor;
	iBorder = bFocus ? pData->iFocusBorderColor :
	          bHover ? pData->iHoverBorderColor : pData->iBorderColor;
	iTextColor = bEnabled ? pData->iTextColor : pData->iDisabledTextColor;
	iArrow = bEnabled ? pData->iArrowColor : pData->iDisabledArrowColor;
	tRect = xuiWidgetGetContentRect(pWidget);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	if ( pProxy->drawRectFill != NULL ) {
		iRet = pProxy->drawRectFill(pProxy, pDraw, xuiInternalSnapRect(tRect), iBackground);
		if ( iRet != XUI_OK ) return iRet;
		iRet = pProxy->drawRectFill(pProxy, pDraw, pData->tButtonRect, iButton);
		if ( iRet != XUI_OK ) return iRet;
	}
	pIcon = (pData->pCategory != NULL && pData->iSelectedId != XUI_ICON_ID_INVALID) ?
	        xuiIconFindById(pData->pCategory, pData->iSelectedId) : NULL;
	sValue[0] = '\0';
	if ( pIcon != NULL ) (void)__xuiIconPickerFormatValue(pWidget, pData, pIcon, sValue, (int)sizeof(sValue));
	if ( pIcon != NULL ) {
		if ( sValue[0] != '\0' ) {
			fGap = 7.0f;
			fIconSide = __xuiIconPickerMin(pData->tValueRect.fH,
				__xuiIconPickerMax(0.0f, pData->tValueRect.fW - fGap - 20.0f));
			tIcon = (xui_rect_t){pData->tValueRect.fX,
				pData->tValueRect.fY + (pData->tValueRect.fH - fIconSide) * 0.5f,
				fIconSide, fIconSide};
			tText = (xui_rect_t){tIcon.fX + tIcon.fW + fGap, pData->tValueRect.fY,
				__xuiIconPickerMax(0.0f, pData->tValueRect.fW - tIcon.fW - fGap),
				pData->tValueRect.fH};
		} else {
			fIconSide = __xuiIconPickerMin(pData->tValueRect.fW, pData->tValueRect.fH);
			tIcon = (xui_rect_t){
				pData->tValueRect.fX + (pData->tValueRect.fW - fIconSide) * 0.5f,
				pData->tValueRect.fY + (pData->tValueRect.fH - fIconSide) * 0.5f,
				fIconSide, fIconSide};
			tText = (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
		}
		iRet = __xuiIconPickerDrawIcon(pWidget, pDraw, pIcon, tIcon, bEnabled);
		if ( iRet != XUI_OK ) return iRet;
	} else {
		tText = pData->tValueRect;
		snprintf(sValue, sizeof(sValue), "%s", (pData->sPlaceholder != NULL) ? pData->sPlaceholder : "");
		iTextColor = bEnabled ? pData->iPlaceholderColor : pData->iDisabledTextColor;
	}
	pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	if ( sValue[0] != '\0' && tText.fW > 0.0f && pFont != NULL && pProxy->drawText != NULL ) {
		iRet = pProxy->drawText(pProxy, pDraw, pFont, sValue, xuiInternalSnapRect(tText), iTextColor,
			XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
		if ( iRet != XUI_OK ) return iRet;
	}
	iRet = __xuiIconPickerDrawChevron(pProxy, pDraw, pData->tButtonRect, bOpen, iArrow);
	if ( iRet != XUI_OK ) return iRet;
	if ( pProxy->drawRectStroke != NULL && pData->fBorderWidth > 0.0f ) {
		return pProxy->drawRectStroke(pProxy, pDraw, xuiInternalSnapRect(tRect), pData->fBorderWidth, iBorder);
	}
	return XUI_OK;
}

static int __xuiIconPickerViewportRender(xui_widget pViewport, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_widget pWidget;
	xui_icon_picker_data_t* pData;
	xui_proxy pProxy;
	xui_icon pIcon;
	xui_rect_t tViewport;
	xui_rect_t tCell;
	xui_rect_t tFill;
	xui_rect_t tIcon;
	float fOffsetY;
	float fRowStride;
	int iStartRow;
	int iEndRow;
	int iStart;
	int iEnd;
	int i;
	int iRet;

	(void)iStateId;
	pWidget = (xui_widget)pUser;
	pData = __xuiIconPickerGetData(pWidget);
	if ( (pViewport == NULL) || (pData == NULL) || (pDraw == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	(void)__xuiIconPickerSyncCategory(pWidget, pData);
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	tViewport = xuiWidgetGetRect(pViewport);
	tViewport.fX = 0.0f;
	tViewport.fY = 0.0f;
	if ( pProxy->drawRectFill != NULL ) {
		iRet = pProxy->drawRectFill(pProxy, pDraw, xuiInternalSnapRect(tViewport), pData->iPopupPanelColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	fOffsetY = 0.0f;
	(void)xuiScrollFrameGetOffset(pData->pFrame, NULL, &fOffsetY);
	fRowStride = pData->fCellHeight + pData->fGapY;
	iStartRow = (fRowStride > 0.0f) ? (int)(fOffsetY / fRowStride) : 0;
	if ( iStartRow < 0 ) iStartRow = 0;
	iEndRow = (fRowStride > 0.0f) ? (int)((fOffsetY + tViewport.fH) / fRowStride) + 2 : 0;
	iStart = iStartRow * pData->iVisibleColumns;
	iEnd = iEndRow * pData->iVisibleColumns;
	if ( iStart < 0 ) iStart = 0;
	if ( iEnd > pData->iIconCount ) iEnd = pData->iIconCount;
	for ( i = iStart; i < iEnd; i++ ) {
		tCell = __xuiIconPickerItemContentRect(pData, i);
		tCell.fY -= fOffsetY;
		tCell = xuiInternalSnapRect(tCell);
		tFill = xuiInternalInsetRect(tCell, 1.0f);
		if ( i == pData->iSelectedIndex && pProxy->drawRectFill != NULL ) {
			iRet = pProxy->drawRectFill(pProxy, pDraw, tFill, pData->iItemSelectedColor);
			if ( iRet != XUI_OK ) return iRet;
		} else if ( i == pData->iHoverIndex && pProxy->drawRectFill != NULL ) {
			iRet = pProxy->drawRectFill(pProxy, pDraw, tFill, pData->iItemHoverColor);
			if ( iRet != XUI_OK ) return iRet;
		}
		if ( i == pData->iFocusIndex && pProxy->drawRectStroke != NULL &&
		     xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pViewport ) {
			iRet = pProxy->drawRectStroke(pProxy, pDraw, tFill, 1.0f, pData->iItemFocusColor);
			if ( iRet != XUI_OK ) return iRet;
		}
		pIcon = (pData->pCategory != NULL) ? xuiIconFindById(pData->pCategory, pData->arrIds[i]) : NULL;
		tIcon = xuiInternalInsetRect(tCell, pData->fIconPadding);
		if ( tIcon.fW > 0.0f && tIcon.fH > 0.0f ) {
			iRet = __xuiIconPickerDrawIcon(pWidget, pDraw, pIcon, tIcon, 1);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	return XUI_OK;
}

static int __xuiIconPickerHitWorld(xui_icon_picker_data_t* pData, float fX, float fY)
{
	xui_rect_t tWorld;
	float fOffsetX;
	float fOffsetY;
	float fLocalX;
	float fLocalY;
	float fStrideX;
	float fStrideY;
	float fCellX;
	float fCellY;
	int iColumn;
	int iRow;
	int iIndex;

	if ( (pData == NULL) || (pData->pViewport == NULL) || (pData->pFrame == NULL) ) return -1;
	tWorld = xuiWidgetGetWorldRect(pData->pViewport);
	if ( fX < tWorld.fX || fY < tWorld.fY ||
	     fX >= tWorld.fX + tWorld.fW || fY >= tWorld.fY + tWorld.fH ) return -1;
	fOffsetX = 0.0f;
	fOffsetY = 0.0f;
	(void)xuiScrollFrameGetOffset(pData->pFrame, &fOffsetX, &fOffsetY);
	fLocalX = fX - tWorld.fX + fOffsetX;
	fLocalY = fY - tWorld.fY + fOffsetY;
	fStrideX = pData->fCellWidth + pData->fGapX;
	fStrideY = pData->fCellHeight + pData->fGapY;
	if ( fLocalX < 0.0f || fLocalY < 0.0f || fStrideX <= 0.0f || fStrideY <= 0.0f ) return -1;
	iColumn = (int)(fLocalX / fStrideX);
	iRow = (int)(fLocalY / fStrideY);
	if ( iColumn < 0 || iColumn >= pData->iVisibleColumns || iRow < 0 ) return -1;
	fCellX = fLocalX - (float)iColumn * fStrideX;
	fCellY = fLocalY - (float)iRow * fStrideY;
	if ( fCellX >= pData->fCellWidth || fCellY >= pData->fCellHeight ) return -1;
	iIndex = iRow * pData->iVisibleColumns + iColumn;
	return (iIndex >= 0 && iIndex < pData->iIconCount) ? iIndex : -1;
}

static int __xuiIconPickerInvalidate(xui_widget pWidget, xui_icon_picker_data_t* pData)
{
	if ( pWidget != NULL ) {
		(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	if ( pData != NULL && pData->pViewport != NULL ) {
		(void)xuiWidgetInvalidate(pData->pViewport, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return XUI_OK;
}

static int __xuiIconPickerSetHover(xui_widget pWidget, xui_icon_picker_data_t* pData, int iIndex)
{
	if ( pData == NULL || pData->iHoverIndex == iIndex ) return XUI_OK;
	pData->iHoverIndex = iIndex;
	if ( pData->pViewport != NULL ) {
		xuiInternalTooltipCancel(xuiWidgetGetContext(pWidget));
	}
	return __xuiIconPickerInvalidate(pWidget, pData);
}

static int __xuiIconPickerSetFocusIndex(xui_widget pWidget, xui_icon_picker_data_t* pData, int iIndex)
{
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iIconCount <= 0 ) iIndex = -1;
	if ( iIndex < -1 ) iIndex = -1;
	if ( iIndex >= pData->iIconCount ) iIndex = pData->iIconCount - 1;
	if ( pData->iFocusIndex == iIndex ) return XUI_OK;
	pData->iFocusIndex = iIndex;
	if ( iIndex >= 0 ) {
		(void)xuiScrollFrameEnsureRectVisible(pData->pFrame, __xuiIconPickerItemContentRect(pData, iIndex));
	}
	return __xuiIconPickerInvalidate(pWidget, pData);
}

static void __xuiIconPickerNotify(xui_widget pWidget, xui_icon_picker_data_t* pData, xui_icon_id iOldId, xui_icon_id iNewId)
{
	xui_icon_picker_change_proc onChange;
	void* pUser;

	if ( pWidget == NULL || pData == NULL || iOldId == iNewId ) return;
	pData->iChangeCount++;
	onChange = pData->onChange;
	pUser = pData->pChangeUser;
	if ( onChange != NULL ) onChange(pWidget, iOldId, iNewId, pUser);
}

static int __xuiIconPickerSetSelectedInternal(xui_widget pWidget, xui_icon_picker_data_t* pData, xui_icon_id iId, int bNotify)
{
	xui_icon_id iOldId;
	int iIndex;

	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	(void)__xuiIconPickerSyncCategory(pWidget, pData);
	iIndex = __xuiIconPickerFindIndex(pData, iId);
	if ( iId != XUI_ICON_ID_INVALID && iIndex < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	iOldId = pData->iSelectedId;
	if ( iOldId == iId ) return XUI_OK;
	pData->iSelectedId = iId;
	pData->iSelectedIndex = iIndex;
	pData->iFocusIndex = iIndex;
	(void)__xuiIconPickerInvalidate(pWidget, pData);
	if ( bNotify ) __xuiIconPickerNotify(pWidget, pData, iOldId, iId);
	return XUI_OK;
}

static int __xuiIconPickerCommitIndex(xui_widget pWidget, xui_icon_picker_data_t* pData, int iIndex)
{
	int iRet;

	if ( pData == NULL || iIndex < 0 || iIndex >= pData->iIconCount ) return XUI_OK;
	iRet = __xuiIconPickerSetSelectedInternal(pWidget, pData, pData->arrIds[iIndex], 1);
	if ( iRet != XUI_OK ) return iRet;
	return xuiIconPickerClose(pWidget);
}

static void __xuiIconPickerFrameChanged(xui_widget pFrame, float fOffsetX, float fOffsetY, void* pUser)
{
	xui_widget pWidget;
	xui_icon_picker_data_t* pData;

	(void)pFrame;
	(void)fOffsetX;
	(void)fOffsetY;
	pWidget = (xui_widget)pUser;
	pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL ) return;
	pData->iHoverIndex = -1;
	if ( pData->pViewport != NULL ) {
		(void)xuiWidgetInvalidate(pData->pViewport, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
}

static int __xuiIconPickerViewportKeyDown(xui_widget pWidget, xui_icon_picker_data_t* pData, const xui_event_t* pEvent)
{
	int iCurrent;
	int iTarget;
	int iPage;

	if ( pEvent->iPhase == XUI_EVENT_PHASE_BUBBLE ) return XUI_OK;
	iCurrent = pData->iFocusIndex;
	if ( iCurrent < 0 && pData->iIconCount > 0 ) iCurrent = 0;
	iTarget = iCurrent;
	iPage = pData->iVisibleRows * pData->iVisibleColumns;
	switch ( pEvent->iKey ) {
	case XUI_KEY_LEFT: iTarget = iCurrent - 1; break;
	case XUI_KEY_RIGHT: iTarget = iCurrent + 1; break;
	case XUI_KEY_UP: iTarget = iCurrent - pData->iVisibleColumns; break;
	case XUI_KEY_DOWN: iTarget = iCurrent + pData->iVisibleColumns; break;
	case XUI_KEY_PAGE_UP: iTarget = iCurrent - iPage; break;
	case XUI_KEY_PAGE_DOWN: iTarget = iCurrent + iPage; break;
	case XUI_KEY_HOME: iTarget = 0; break;
	case XUI_KEY_END: iTarget = pData->iIconCount - 1; break;
	case XUI_KEY_ENTER:
	case XUI_KEY_SPACE:
		(void)__xuiIconPickerCommitIndex(pWidget, pData, pData->iFocusIndex);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_ESCAPE:
		(void)xuiIconPickerClose(pWidget);
		return XUI_EVENT_DISPATCH_STOP;
	default:
		return XUI_OK;
	}
	if ( pData->iIconCount > 0 ) {
		if ( iTarget < 0 ) iTarget = 0;
		if ( iTarget >= pData->iIconCount ) iTarget = pData->iIconCount - 1;
		(void)__xuiIconPickerSetFocusIndex(pWidget, pData, iTarget);
		return XUI_EVENT_DISPATCH_STOP;
	}
	return XUI_OK;
}

static int __xuiIconPickerViewportEvent(xui_widget pViewport, const xui_event_t* pEvent, void* pUser)
{
	xui_widget pWidget;
	xui_icon_picker_data_t* pData;
	int iIndex;

	pWidget = (xui_widget)pUser;
	pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL || pEvent == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_MOVE:
		if ( pEvent->iPhase != XUI_EVENT_PHASE_CAPTURE ) {
			iIndex = __xuiIconPickerHitWorld(pData, pEvent->fX, pEvent->fY);
			return __xuiIconPickerSetHover(pWidget, pData, iIndex);
		}
		break;
	case XUI_EVENT_POINTER_LEAVE:
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		return __xuiIconPickerSetHover(pWidget, pData, -1);
	case XUI_EVENT_POINTER_DOWN:
		if ( pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE || pEvent->iButton != XUI_POINTER_BUTTON_LEFT ) return XUI_OK;
		iIndex = __xuiIconPickerHitWorld(pData, pEvent->fX, pEvent->fY);
		if ( iIndex >= 0 ) {
			(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pViewport);
			(void)__xuiIconPickerSetFocusIndex(pWidget, pData, iIndex);
			(void)__xuiIconPickerCommitIndex(pWidget, pData, iIndex);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_KEY_DOWN:
		return __xuiIconPickerViewportKeyDown(pWidget, pData, pEvent);
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
		(void)__xuiIconPickerSyncState(pWidget, pData);
		return __xuiIconPickerInvalidate(pWidget, pData);
	case XUI_EVENT_POINTER_WHEEL:
		return XUI_OK;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiIconPickerTooltipResolve(xui_context pContext, xui_widget pViewport, xui_tooltip_desc_t* pDesc, void* pUser)
{
	xui_widget pWidget;
	xui_icon_picker_data_t* pData;
	xui_icon pIcon;
	const char* sName;
	const char* sDisplay;

	(void)pContext;
	(void)pViewport;
	pWidget = (xui_widget)pUser;
	pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL || pDesc == NULL || pData->iHoverIndex < 0 ||
	     pData->iHoverIndex >= pData->iIconCount || pData->pCategory == NULL ) return 0;
	pIcon = xuiIconFindById(pData->pCategory, pData->arrIds[pData->iHoverIndex]);
	if ( pIcon == NULL ) return 0;
	sName = xuiIconGetName(pIcon);
	sDisplay = xuiIconGetDisplayName(pIcon);
	if ( sDisplay != NULL && sDisplay[0] != '\0' && sName != NULL && strcmp(sDisplay, sName) != 0 ) {
		snprintf(pData->sTooltip, sizeof(pData->sTooltip), "%u  %s  (%s)",
			(unsigned)xuiIconGetId(pIcon), sDisplay, sName);
	} else {
		snprintf(pData->sTooltip, sizeof(pData->sTooltip), "%u  %s",
			(unsigned)xuiIconGetId(pIcon), (sName != NULL) ? sName : "");
	}
	memset(pDesc, 0, sizeof(*pDesc));
	pDesc->iSize = sizeof(*pDesc);
	pDesc->iType = XUI_TOOLTIP_TEXT;
	pDesc->sText = pData->sTooltip;
	pDesc->iAnchor = XUI_TOOLTIP_ANCHOR_CURSOR;
	pDesc->fDelay = 0.45f;
	return 1;
}

static void __xuiIconPickerPopupChanged(xui_widget pPopup, int bOpen, void* pUser)
{
	xui_widget pWidget;
	xui_icon_picker_data_t* pData;

	(void)pPopup;
	pWidget = (xui_widget)pUser;
	pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL ) return;
	if ( !bOpen ) {
		pData->iHoverIndex = -1;
		pData->iFocusIndex = pData->iSelectedIndex;
	}
	(void)__xuiIconPickerSyncState(pWidget, pData);
	(void)__xuiIconPickerInvalidate(pWidget, pData);
}

static int __xuiIconPickerPointerDown(xui_widget pWidget, xui_icon_picker_data_t* pData, const xui_event_t* pEvent)
{
	if ( pEvent->iPhase == XUI_EVENT_PHASE_BUBBLE || pEvent->iButton != XUI_POINTER_BUTTON_LEFT ) return XUI_OK;
	if ( !xuiWidgetGetEnabled(pWidget) ) return XUI_OK;
	(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
	(void)xuiIconPickerToggle(pWidget);
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiIconPickerKeyDown(xui_widget pWidget, xui_icon_picker_data_t* pData, const xui_event_t* pEvent)
{
	if ( pEvent->iPhase == XUI_EVENT_PHASE_BUBBLE || !xuiWidgetGetEnabled(pWidget) ) return XUI_OK;
	switch ( pEvent->iKey ) {
	case XUI_KEY_ENTER:
	case XUI_KEY_SPACE:
		(void)xuiIconPickerToggle(pWidget);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_UP:
	case XUI_KEY_DOWN:
		(void)xuiIconPickerOpen(pWidget);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_ESCAPE:
		if ( xuiIconPickerIsOpen(pWidget) ) {
			(void)xuiIconPickerClose(pWidget);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiIconPickerEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_icon_picker_data_t* pData;

	(void)pUser;
	pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL || pEvent == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_DOWN:
		return __xuiIconPickerPointerDown(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_CLICK:
		if ( pEvent->iButton == XUI_POINTER_BUTTON_LEFT ) return XUI_EVENT_DISPATCH_STOP;
		break;
	case XUI_EVENT_KEY_DOWN:
		return __xuiIconPickerKeyDown(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_ENTER:
	case XUI_EVENT_POINTER_LEAVE:
	case XUI_EVENT_POINTER_MOVE:
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
		(void)__xuiIconPickerSyncState(pWidget, pData);
		return __xuiIconPickerInvalidate(pWidget, pData);
	case XUI_EVENT_BOUNDS_CHANGED:
		(void)__xuiIconPickerUpdateRects(pWidget, pData);
		if ( xuiIconPickerIsOpen(pWidget) ) {
			(void)__xuiIconPickerApplyPopup(pWidget, pData);
			(void)xuiPopupApplyPlacement(pData->pPopup);
		}
		return XUI_OK;
	case XUI_EVENT_ENABLED_CHANGED:
	case XUI_EVENT_VISIBLE_CHANGED:
		if ( !xuiWidgetGetEnabled(pWidget) || !xuiWidgetGetVisible(pWidget) ) {
			(void)xuiIconPickerClose(pWidget);
		}
		(void)__xuiIconPickerSyncState(pWidget, pData);
		return __xuiIconPickerInvalidate(pWidget, pData);
	default:
		break;
	}
	return XUI_OK;
}

static void __xuiIconPickerDefaultAction(xui_widget pWidget, void* pUser)
{
	(void)pUser;
	(void)xuiIconPickerToggle(pWidget);
}

static void __xuiIconPickerCancelAction(xui_widget pWidget, void* pUser)
{
	(void)pUser;
	(void)xuiIconPickerClose(pWidget);
}

static int __xuiIconPickerUpdate(xui_widget pWidget, float fDelta, void* pUser)
{
	xui_icon_picker_data_t* pData;
	uint32_t iGeneration;

	(void)fDelta;
	(void)pUser;
	pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iGeneration = (pData->pCategory != NULL) ? xuiIconCategoryGetGeneration(pData->pCategory) : 0u;
	if ( iGeneration != pData->iCategoryGeneration ) {
		(void)__xuiIconPickerSyncCategory(pWidget, pData);
		if ( xuiIconPickerIsOpen(pWidget) ) {
			(void)__xuiIconPickerApplyPopup(pWidget, pData);
			(void)xuiPopupApplyPlacement(pData->pPopup);
		}
	}
	return XUI_OK;
}

static int __xuiIconPickerContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_icon_picker_data_t* pData;
	xui_proxy pProxy;
	xui_font_metrics_t tMetrics;
	xui_font pFont;

	(void)tConstraint;
	(void)pUser;
	pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL || pSize == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pSize->fX = 190.0f;
	pSize->fY = 32.0f;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	if ( pProxy != NULL && pProxy->fontGetMetrics != NULL && pFont != NULL &&
	     pProxy->fontGetMetrics(pProxy, pFont, &tMetrics) == XUI_OK ) {
		pSize->fY = __xuiIconPickerMax(32.0f, tMetrics.fLineHeight + 12.0f);
	}
	pSize->fX = xuiInternalSnapSize(pSize->fX);
	pSize->fY = xuiInternalSnapSize(pSize->fY);
	return XUI_OK;
}

static void __xuiIconPickerDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_MANUAL;
	pLayout->iWidthMode = XUI_SIZE_CONTENT;
	pLayout->iHeightMode = XUI_SIZE_CONTENT;
	pLayout->iFlowMode = XUI_FLOW_BLOCK;
	pLayout->iDock = XUI_DOCK_FILL;
	pLayout->iOverflow = XUI_OVERFLOW_VISIBLE;
	pLayout->iAlignX = XUI_ALIGN_START;
	pLayout->iAlignY = XUI_ALIGN_START;
	pLayout->iTableRowSpan = 1;
	pLayout->iTableColumnSpan = 1;
	pLayout->iGridColumnCount = 1;
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fShrink = 1.0f;
}

static void __xuiIconPickerDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiIconPickerInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetDefaultAction(pWidget, __xuiIconPickerDefaultAction, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetCancelAction(pWidget, __xuiIconPickerCancelAction, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_ENTER, __xuiIconPickerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiIconPickerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiIconPickerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiIconPickerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CLICK, __xuiIconPickerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiIconPickerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiIconPickerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiIconPickerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BOUNDS_CHANGED, __xuiIconPickerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_ENABLED_CHANGED, __xuiIconPickerEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_VISIBLE_CHANGED, __xuiIconPickerEvent, NULL);
	return iRet;
}

static int __xuiIconPickerInitViewport(xui_widget pWidget, xui_icon_picker_data_t* pData)
{
	xui_cache_policy_t tPolicy;
	int iRet;

	memset(&tPolicy, 0, sizeof(tPolicy));
	tPolicy.iSize = sizeof(tPolicy);
	tPolicy.iPolicy = XUI_CACHE_POLICY_SELF;
	tPolicy.iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	tPolicy.iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	(void)xuiWidgetSetLayoutType(pData->pViewport, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetFlowMode(pData->pViewport, XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetFocusable(pData->pViewport, 1);
	(void)xuiWidgetSetTabStop(pData->pViewport, 0);
	(void)xuiWidgetSetImeMode(pData->pViewport, XUI_IME_DISABLED);
	(void)xuiWidgetSetCachePolicy(pData->pViewport, &tPolicy);
	iRet = xuiWidgetSetCacheRenderCallback(pData->pViewport, __xuiIconPickerViewportRender, pWidget);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pData->pViewport, XUI_EVENT_POINTER_MOVE, __xuiIconPickerViewportEvent, pWidget);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pData->pViewport, XUI_EVENT_POINTER_LEAVE, __xuiIconPickerViewportEvent, pWidget);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pData->pViewport, XUI_EVENT_POINTER_DOWN, __xuiIconPickerViewportEvent, pWidget);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pData->pViewport, XUI_EVENT_POINTER_WHEEL, __xuiIconPickerViewportEvent, pWidget);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pData->pViewport, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiIconPickerViewportEvent, pWidget);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pData->pViewport, XUI_EVENT_KEY_DOWN, __xuiIconPickerViewportEvent, pWidget);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pData->pViewport, XUI_EVENT_FOCUS, __xuiIconPickerViewportEvent, pWidget);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pData->pViewport, XUI_EVENT_BLUR, __xuiIconPickerViewportEvent, pWidget);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetTooltipResolver(pData->pViewport, __xuiIconPickerTooltipResolve, pWidget);
	return iRet;
}

static int __xuiIconPickerCreatePopup(xui_widget pWidget, xui_icon_picker_data_t* pData)
{
	xui_popup_desc_t tPopupDesc;
	xui_scroll_frame_desc_t tFrameDesc;
	xui_widget pPopupScroll;
	int iRet;

	memset(&tPopupDesc, 0, sizeof(tPopupDesc));
	tPopupDesc.iSize = sizeof(tPopupDesc);
	tPopupDesc.pOwner = pWidget;
	tPopupDesc.fPadding = XUI_ICON_PICKER_POPUP_PADDING;
	tPopupDesc.fBorderWidth = 1.0f;
	tPopupDesc.fShadowSize = 5.0f;
	tPopupDesc.fGap = 2.0f;
	tPopupDesc.iAnchor = XUI_POPUP_ANCHOR_BOTTOM_LEFT;
	tPopupDesc.iDirection = XUI_POPUP_DIRECTION_RIGHT_DOWN;
	tPopupDesc.iOutsidePolicy = XUI_POPUP_OUTSIDE_CLOSE;
	tPopupDesc.iOwnerPolicy = XUI_POPUP_OWNER_PASSTHROUGH;
	tPopupDesc.iEscapePolicy = XUI_POPUP_ESCAPE_CLOSE;
	tPopupDesc.iFocusPolicy = XUI_POPUP_FOCUS_CUSTOM;
	tPopupDesc.iScrollbarMode = XUI_SCROLLBAR_MODE_COMPACT;
	tPopupDesc.iPanelColor = pData->iPopupPanelColor;
	tPopupDesc.iBorderColor = pData->iPopupBorderColor;
	tPopupDesc.iShadowColor = pData->iPopupShadowColor;
	iRet = xuiPopupCreate(xuiWidgetGetContext(pWidget), &pData->pPopup, &tPopupDesc);
	if ( iRet != XUI_OK ) return iRet;
	pData->pPanel = xuiPopupGetContentWidget(pData->pPopup);
	if ( pData->pPanel == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	pPopupScroll = xuiPopupGetScrollViewWidget(pData->pPopup);
	if ( pPopupScroll != NULL ) {
		(void)xuiScrollViewSetScrollbarPolicy(pPopupScroll, XUI_SCROLLBAR_POLICY_HIDDEN, XUI_SCROLLBAR_POLICY_HIDDEN);
	}
	(void)xuiWidgetSetLayoutType(pData->pPanel, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetFlowMode(pData->pPanel, XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetOverflow(pData->pPanel, XUI_OVERFLOW_VISIBLE);
	memset(&tFrameDesc, 0, sizeof(tFrameDesc));
	tFrameDesc.iSize = sizeof(tFrameDesc);
	tFrameDesc.iPolicyX = XUI_SCROLLBAR_POLICY_HIDDEN;
	tFrameDesc.iPolicyY = XUI_SCROLLBAR_POLICY_AUTO;
	tFrameDesc.iScrollbarMode = XUI_SCROLLBAR_MODE_COMPACT;
	tFrameDesc.iWheelAxis = XUI_WHEEL_AXIS_VERTICAL;
	tFrameDesc.fScrollbarSize = pData->fScrollbarSize;
	tFrameDesc.fWheelStep = pData->fCellHeight + pData->fGapY;
	tFrameDesc.iBackgroundColor = pData->iPopupPanelColor;
	iRet = xuiScrollFrameCreate(xuiWidgetGetContext(pWidget), &pData->pFrame, &tFrameDesc);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiWidgetAddChild(pData->pPanel, pData->pFrame);
	if ( iRet != XUI_OK ) return iRet;
	pData->pViewport = xuiScrollFrameGetViewportWidget(pData->pFrame);
	if ( pData->pViewport == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	iRet = __xuiIconPickerInitViewport(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiScrollFrameSetChange(pData->pFrame, __xuiIconPickerFrameChanged, pWidget);
	if ( iRet == XUI_OK ) iRet = xuiPopupSetChange(pData->pPopup, __xuiIconPickerPopupChanged, pWidget);
	if ( iRet == XUI_OK ) iRet = xuiPopupSetFocusPolicy(pData->pPopup, XUI_POPUP_FOCUS_CUSTOM, pData->pViewport);
	return iRet;
}

static int __xuiIconPickerApplyPopup(xui_widget pWidget, xui_icon_picker_data_t* pData)
{
	xui_rect_t tFrame;
	float fGridWidth;
	float fVisibleHeight;
	float fContentHeight;
	float fOuterContentWidth;
	int bNeedScrollbar;
	int iRet;

	if ( pWidget == NULL || pData == NULL || pData->pPopup == NULL || pData->pFrame == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	(void)__xuiIconPickerSyncCategory(pWidget, pData);
	fGridWidth = __xuiIconPickerGridWidth(pData);
	fVisibleHeight = __xuiIconPickerVisibleHeight(pData);
	fContentHeight = __xuiIconPickerContentHeight(pData);
	bNeedScrollbar = __xuiIconPickerNeedScrollbar(pData);
	fOuterContentWidth = fGridWidth + (bNeedScrollbar ? pData->fScrollbarSize : 0.0f);
	iRet = xuiPopupSetOwner(pData->pPopup, pWidget);
	if ( iRet == XUI_OK ) iRet = xuiPopupSetFocusRestore(pData->pPopup, pWidget);
	if ( iRet == XUI_OK ) iRet = xuiPopupSetContentSize(pData->pPopup, fOuterContentWidth, fVisibleHeight);
	if ( iRet == XUI_OK ) iRet = xuiPopupSetColors(pData->pPopup, pData->iPopupPanelColor, pData->iPopupBorderColor, pData->iPopupShadowColor, XUI_COLOR_RGBA(0, 0, 0, 0));
	if ( iRet == XUI_OK ) iRet = xuiPopupSetMetrics(pData->pPopup, XUI_ICON_PICKER_POPUP_PADDING, 1.0f, 5.0f);
	if ( iRet == XUI_OK ) iRet = xuiPopupSetClosePolicy(pData->pPopup, XUI_POPUP_OUTSIDE_CLOSE, XUI_POPUP_OWNER_PASSTHROUGH, XUI_POPUP_ESCAPE_CLOSE);
	if ( iRet == XUI_OK ) iRet = xuiPopupSetFocusPolicy(pData->pPopup, XUI_POPUP_FOCUS_CUSTOM, pData->pViewport);
	if ( iRet == XUI_OK ) iRet = xuiPopupSetGap(pData->pPopup, 2.0f);
	if ( iRet != XUI_OK ) return iRet;
	if ( pData->iPopupPlacement == XUI_ICON_PICKER_POPUP_TOP ) {
		(void)xuiPopupSetAnchor(pData->pPopup, XUI_POPUP_ANCHOR_TOP_LEFT);
		(void)xuiPopupSetDirection(pData->pPopup, XUI_POPUP_DIRECTION_RIGHT_UP);
	} else {
		(void)xuiPopupSetAnchor(pData->pPopup, XUI_POPUP_ANCHOR_BOTTOM_LEFT);
		(void)xuiPopupSetDirection(pData->pPopup, XUI_POPUP_DIRECTION_RIGHT_DOWN);
	}
	(void)xuiPopupClearAnchorRect(pData->pPopup);
	(void)xuiWidgetSetRect(pData->pPanel, (xui_rect_t){0.0f, 0.0f, fOuterContentWidth, fVisibleHeight});
	tFrame = (xui_rect_t){0.0f, 0.0f, fOuterContentWidth, fVisibleHeight};
	(void)xuiWidgetSetRect(pData->pFrame, tFrame);
	(void)xuiScrollFrameSetScrollbarPolicy(pData->pFrame, XUI_SCROLLBAR_POLICY_HIDDEN,
		bNeedScrollbar ? XUI_SCROLLBAR_POLICY_AUTO : XUI_SCROLLBAR_POLICY_HIDDEN);
	(void)xuiScrollFrameSetMetrics(pData->pFrame, pData->fScrollbarSize, 18.0f, 0.0f);
	(void)xuiScrollFrameSetWheelStep(pData->pFrame, pData->fCellHeight + pData->fGapY);
	(void)xuiScrollFrameSetContentSize(pData->pFrame, fGridWidth, fContentHeight);
	(void)xuiWidgetArrange(pData->pFrame, tFrame);
	(void)xuiScrollFrameLayout(pData->pFrame);
	return XUI_OK;
}

static int __xuiIconPickerInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_icon_picker_data_t* pData;
	const xui_icon_picker_desc_t* pDesc;
	int iRet;

	(void)pUser;
	pData = (xui_icon_picker_data_t*)pTypeData;
	pDesc = (const xui_icon_picker_desc_t*)pCreateData;
	if ( pWidget == NULL || pData == NULL || !__xuiIconPickerDescValid(pDesc) ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiIconPickerDefaults(pData);
	iRet = __xuiIconPickerSetPlaceholderData(pData, (pDesc != NULL) ? pDesc->sPlaceholder : "");
	if ( iRet != XUI_OK ) return iRet;
	__xuiIconPickerApplyDesc(pData, pDesc);
	if ( pDesc != NULL && pDesc->pCategory != NULL ) {
		iRet = __xuiIconPickerApplyCategoryData(pData, pDesc->pCategory);
		if ( iRet != XUI_OK ) return iRet;
	}
	(void)xuiWidgetSetLayoutType(pWidget, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetFlowMode(pWidget, XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_VISIBLE);
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	(void)xuiWidgetSetImeMode(pWidget, XUI_IME_DISABLED);
	(void)xuiWidgetSetPadding(pWidget, (xui_thickness_t){0.0f, 0.0f, 0.0f, 0.0f});
	iRet = __xuiIconPickerInitEvents(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiIconPickerCreatePopup(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiIconPickerSyncCategory(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	if ( pData->iSelectedId != XUI_ICON_ID_INVALID && pData->iSelectedIndex < 0 ) {
		pData->iSelectedId = XUI_ICON_ID_INVALID;
	}
	(void)__xuiIconPickerUpdateRects(pWidget, pData);
	(void)__xuiIconPickerApplyPopup(pWidget, pData);
	return __xuiIconPickerSyncState(pWidget, pData);
}

static void __xuiIconPickerDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_icon_picker_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_icon_picker_data_t*)pTypeData;
	if ( pData == NULL ) return;
	if ( pData->pPopup != NULL ) xuiWidgetDestroy(pData->pPopup);
	if ( pData->pCategory != NULL ) (void)xuiIconCategoryRelease(pData->pCategory);
	if ( pData->arrIds != NULL ) xrtFree(pData->arrIds);
	if ( pData->sPlaceholder != NULL ) xrtFree(pData->sPlaceholder);
	memset(pData, 0, sizeof(*pData));
}

static xui_icon_picker_data_t* __xuiIconPickerGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) return NULL;
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "iconpicker");
	if ( pType == NULL || !xuiWidgetIsType(pWidget, pType) ) return NULL;
	return (xui_icon_picker_data_t*)xuiWidgetGetTypeData(pWidget);
}

XUI_API xui_widget_type xuiIconPickerGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) return NULL;
	pType = xuiWidgetFindType(pContext, "iconpicker");
	if ( pType != NULL ) return pType;
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "iconpicker";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_icon_picker_data_t);
	tDesc.onInit = __xuiIconPickerInit;
	tDesc.onDestroy = __xuiIconPickerDestroy;
	tDesc.onContentMeasure = __xuiIconPickerContentMeasure;
	tDesc.onCacheRender = __xuiIconPickerCacheRender;
	tDesc.onUpdate = __xuiIconPickerUpdate;
	__xuiIconPickerDefaultLayout(&tDesc.tLayout);
	__xuiIconPickerDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	return (iRet == XUI_OK) ? pType : NULL;
}

XUI_API int xuiIconPickerCreate(xui_context pContext, xui_widget* ppWidget, const xui_icon_picker_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( ppWidget == NULL || !__xuiIconPickerDescValid(pDesc) ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppWidget = NULL;
	pType = xuiIconPickerGetType(pContext);
	if ( pType == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiIconPickerSetChange(xui_widget pWidget, xui_icon_picker_change_proc onChange, void* pUser)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onChange = onChange;
	pData->pChangeUser = pUser;
	return XUI_OK;
}

XUI_API int xuiIconPickerSetFormatter(xui_widget pWidget, xui_icon_picker_format_proc onFormat, void* pUser)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onFormat = onFormat;
	pData->pFormatUser = pUser;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiIconPickerSetCategory(xui_widget pWidget, xui_icon_category pCategory)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	int iRet;

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	(void)xuiIconPickerClose(pWidget);
	iRet = __xuiIconPickerApplyCategoryData(pData, pCategory);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiIconPickerSyncCategory(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	pData->iSelectedId = XUI_ICON_ID_INVALID;
	pData->iSelectedIndex = -1;
	pData->iFocusIndex = -1;
	pData->iHoverIndex = -1;
	(void)__xuiIconPickerApplyPopup(pWidget, pData);
	return __xuiIconPickerInvalidate(pWidget, pData);
}

XUI_API xui_icon_category xuiIconPickerGetCategory(xui_widget pWidget)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	return (pData != NULL) ? pData->pCategory : NULL;
}

XUI_API int xuiIconPickerSetSelectedId(xui_widget pWidget, xui_icon_id iId)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	return (pData != NULL) ? __xuiIconPickerSetSelectedInternal(pWidget, pData, iId, 0) : XUI_ERROR_INVALID_ARGUMENT;
}

XUI_API xui_icon_id xuiIconPickerGetSelectedId(xui_widget pWidget)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	return (pData != NULL) ? pData->iSelectedId : XUI_ICON_ID_INVALID;
}

XUI_API xui_icon xuiIconPickerGetSelectedIcon(xui_widget pWidget)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL || pData->pCategory == NULL || pData->iSelectedId == XUI_ICON_ID_INVALID ) return NULL;
	return xuiIconFindById(pData->pCategory, pData->iSelectedId);
}

XUI_API int xuiIconPickerClearSelection(xui_widget pWidget)
{
	return xuiIconPickerSetSelectedId(pWidget, XUI_ICON_ID_INVALID);
}

XUI_API int xuiIconPickerSetTextMode(xui_widget pWidget, int iMode)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL || !__xuiIconPickerTextModeValid(iMode) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iTextMode = iMode;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiIconPickerGetTextMode(xui_widget pWidget)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	return (pData != NULL) ? pData->iTextMode : XUI_ICON_PICKER_TEXT_NONE;
}

XUI_API int xuiIconPickerSetPlaceholder(xui_widget pWidget, const char* sText)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiIconPickerSetPlaceholderData(pData, sText);
	if ( iRet != XUI_OK ) return iRet;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API const char* xuiIconPickerGetPlaceholder(xui_widget pWidget)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	return (pData != NULL && pData->sPlaceholder != NULL) ? pData->sPlaceholder : "";
}

XUI_API int xuiIconPickerSetGrid(xui_widget pWidget, int iRows, int iColumns)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL || iRows <= 0 || iColumns <= 0 || iRows > 1024 || iColumns > 1024 ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iVisibleRows = iRows;
	pData->iVisibleColumns = iColumns;
	(void)__xuiIconPickerApplyPopup(pWidget, pData);
	if ( xuiIconPickerIsOpen(pWidget) ) (void)xuiPopupApplyPlacement(pData->pPopup);
	return __xuiIconPickerInvalidate(pWidget, pData);
}

XUI_API int xuiIconPickerGetGrid(xui_widget pWidget, int* pRows, int* pColumns)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pRows != NULL ) *pRows = pData->iVisibleRows;
	if ( pColumns != NULL ) *pColumns = pData->iVisibleColumns;
	return XUI_OK;
}

XUI_API int xuiIconPickerSetCellSize(xui_widget pWidget, float fWidth, float fHeight)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL || fWidth <= 0.0f || fHeight <= 0.0f ||
	     !__xuiIconPickerFloatValid(fWidth) || !__xuiIconPickerFloatValid(fHeight) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fCellWidth = fWidth;
	pData->fCellHeight = fHeight;
	(void)__xuiIconPickerApplyPopup(pWidget, pData);
	if ( xuiIconPickerIsOpen(pWidget) ) (void)xuiPopupApplyPlacement(pData->pPopup);
	return __xuiIconPickerInvalidate(pWidget, pData);
}

XUI_API int xuiIconPickerGetCellSize(xui_widget pWidget, float* pWidth, float* pHeight)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pWidth != NULL ) *pWidth = pData->fCellWidth;
	if ( pHeight != NULL ) *pHeight = pData->fCellHeight;
	return XUI_OK;
}

XUI_API int xuiIconPickerSetSpacing(xui_widget pWidget, float fGapX, float fGapY, float fIconPadding)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL || !__xuiIconPickerFloatValid(fGapX) || !__xuiIconPickerFloatValid(fGapY) ||
	     !__xuiIconPickerFloatValid(fIconPadding) || fIconPadding * 2.0f >= pData->fCellWidth ||
	     fIconPadding * 2.0f >= pData->fCellHeight ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fGapX = fGapX;
	pData->fGapY = fGapY;
	pData->fIconPadding = fIconPadding;
	(void)__xuiIconPickerApplyPopup(pWidget, pData);
	if ( xuiIconPickerIsOpen(pWidget) ) (void)xuiPopupApplyPlacement(pData->pPopup);
	return __xuiIconPickerInvalidate(pWidget, pData);
}

XUI_API int xuiIconPickerGetSpacing(xui_widget pWidget, float* pGapX, float* pGapY, float* pIconPadding)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pGapX != NULL ) *pGapX = pData->fGapX;
	if ( pGapY != NULL ) *pGapY = pData->fGapY;
	if ( pIconPadding != NULL ) *pIconPadding = pData->fIconPadding;
	return XUI_OK;
}

XUI_API int xuiIconPickerSetValuePadding(xui_widget pWidget, xui_thickness_t tPadding)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL || !__xuiIconPickerThicknessValid(tPadding) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->tValuePadding = tPadding;
	(void)__xuiIconPickerUpdateRects(pWidget, pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_thickness_t xuiIconPickerGetValuePadding(xui_widget pWidget)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	return (pData != NULL) ? pData->tValuePadding : (xui_thickness_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API int xuiIconPickerSetPopupPlacement(xui_widget pWidget, int iPlacement)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL || !__xuiIconPickerPlacementValid(iPlacement) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iPopupPlacement = iPlacement;
	(void)__xuiIconPickerApplyPopup(pWidget, pData);
	if ( xuiIconPickerIsOpen(pWidget) ) return xuiPopupApplyPlacement(pData->pPopup);
	return XUI_OK;
}

XUI_API int xuiIconPickerGetPopupPlacement(xui_widget pWidget)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	return (pData != NULL) ? pData->iPopupPlacement : XUI_ICON_PICKER_POPUP_AUTO;
}

XUI_API int xuiIconPickerOpen(xui_widget pWidget)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	int iRet;

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !xuiWidgetGetEnabled(pWidget) || !xuiWidgetGetVisible(pWidget) ) return XUI_OK;
	iRet = __xuiIconPickerSyncCategory(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiIconPickerApplyPopup(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	pData->iHoverIndex = -1;
	pData->iFocusIndex = (pData->iSelectedIndex >= 0) ? pData->iSelectedIndex :
		((pData->iIconCount > 0) ? 0 : -1);
	iRet = xuiPopupSetOpen(pData->pPopup, 1);
	if ( iRet != XUI_OK ) return iRet;
	if ( pData->iFocusIndex >= 0 ) {
		(void)xuiScrollFrameEnsureRectVisible(pData->pFrame,
			__xuiIconPickerItemContentRect(pData, pData->iFocusIndex));
	}
	(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pData->pViewport);
	(void)__xuiIconPickerSyncState(pWidget, pData);
	return __xuiIconPickerInvalidate(pWidget, pData);
}

XUI_API int xuiIconPickerClose(xui_widget pWidget)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	int iRet;

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->pPopup == NULL ) return XUI_OK;
	iRet = xuiPopupSetOpen(pData->pPopup, 0);
	(void)__xuiIconPickerSyncState(pWidget, pData);
	(void)__xuiIconPickerInvalidate(pWidget, pData);
	return iRet;
}

XUI_API int xuiIconPickerToggle(xui_widget pWidget)
{
	return xuiIconPickerIsOpen(pWidget) ? xuiIconPickerClose(pWidget) : xuiIconPickerOpen(pWidget);
}

XUI_API int xuiIconPickerIsOpen(xui_widget pWidget)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	return (pData != NULL && pData->pPopup != NULL) ? xuiPopupIsOpen(pData->pPopup) : 0;
}

XUI_API int xuiIconPickerEnsureVisible(xui_widget pWidget, xui_icon_id iId)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	int iIndex;

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	(void)__xuiIconPickerSyncCategory(pWidget, pData);
	iIndex = __xuiIconPickerFindIndex(pData, iId);
	if ( iIndex < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameEnsureRectVisible(pData->pFrame, __xuiIconPickerItemContentRect(pData, iIndex));
}

XUI_API int xuiIconPickerSetMetrics(xui_widget pWidget, float fBorderWidth, float fScrollbarSize)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL || !__xuiIconPickerFloatValid(fBorderWidth) ||
	     fScrollbarSize <= 0.0f || !__xuiIconPickerFloatValid(fScrollbarSize) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fBorderWidth = fBorderWidth;
	pData->fScrollbarSize = __xuiIconPickerMin(fScrollbarSize, 8.0f);
	(void)__xuiIconPickerApplyPopup(pWidget, pData);
	if ( xuiIconPickerIsOpen(pWidget) ) (void)xuiPopupApplyPlacement(pData->pPopup);
	return __xuiIconPickerInvalidate(pWidget, pData);
}

XUI_API int xuiIconPickerGetMetrics(xui_widget pWidget, float* pBorderWidth, float* pScrollbarSize)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pBorderWidth != NULL ) *pBorderWidth = pData->fBorderWidth;
	if ( pScrollbarSize != NULL ) *pScrollbarSize = pData->fScrollbarSize;
	return XUI_OK;
}

XUI_API int xuiIconPickerSetColors(xui_widget pWidget, uint32_t iText, uint32_t iPlaceholder, uint32_t iDisabledText, uint32_t iBackground, uint32_t iHoverBackground, uint32_t iOpenBackground, uint32_t iDisabledBackground)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iTextColor = iText;
	pData->iPlaceholderColor = iPlaceholder;
	pData->iDisabledTextColor = iDisabledText;
	pData->iBackgroundColor = iBackground;
	pData->iHoverBackgroundColor = iHoverBackground;
	pData->iOpenBackgroundColor = iOpenBackground;
	pData->iDisabledBackgroundColor = iDisabledBackground;
	return __xuiIconPickerInvalidate(pWidget, pData);
}

XUI_API int xuiIconPickerGetColors(xui_widget pWidget, uint32_t* pText, uint32_t* pPlaceholder, uint32_t* pDisabledText, uint32_t* pBackground, uint32_t* pHoverBackground, uint32_t* pOpenBackground, uint32_t* pDisabledBackground)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pText != NULL ) *pText = pData->iTextColor;
	if ( pPlaceholder != NULL ) *pPlaceholder = pData->iPlaceholderColor;
	if ( pDisabledText != NULL ) *pDisabledText = pData->iDisabledTextColor;
	if ( pBackground != NULL ) *pBackground = pData->iBackgroundColor;
	if ( pHoverBackground != NULL ) *pHoverBackground = pData->iHoverBackgroundColor;
	if ( pOpenBackground != NULL ) *pOpenBackground = pData->iOpenBackgroundColor;
	if ( pDisabledBackground != NULL ) *pDisabledBackground = pData->iDisabledBackgroundColor;
	return XUI_OK;
}

XUI_API int xuiIconPickerSetBorderColors(xui_widget pWidget, uint32_t iBorder, uint32_t iHoverBorder, uint32_t iFocusBorder)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iBorderColor = iBorder;
	pData->iHoverBorderColor = iHoverBorder;
	pData->iFocusBorderColor = iFocusBorder;
	return __xuiIconPickerInvalidate(pWidget, pData);
}

XUI_API int xuiIconPickerGetBorderColors(xui_widget pWidget, uint32_t* pBorder, uint32_t* pHoverBorder, uint32_t* pFocusBorder)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pBorder != NULL ) *pBorder = pData->iBorderColor;
	if ( pHoverBorder != NULL ) *pHoverBorder = pData->iHoverBorderColor;
	if ( pFocusBorder != NULL ) *pFocusBorder = pData->iFocusBorderColor;
	return XUI_OK;
}

XUI_API int xuiIconPickerSetArrowColors(xui_widget pWidget, uint32_t iArrow, uint32_t iDisabledArrow)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iArrowColor = iArrow;
	pData->iDisabledArrowColor = iDisabledArrow;
	return __xuiIconPickerInvalidate(pWidget, pData);
}

XUI_API int xuiIconPickerGetArrowColors(xui_widget pWidget, uint32_t* pArrow, uint32_t* pDisabledArrow)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pArrow != NULL ) *pArrow = pData->iArrowColor;
	if ( pDisabledArrow != NULL ) *pDisabledArrow = pData->iDisabledArrowColor;
	return XUI_OK;
}

XUI_API int xuiIconPickerSetButtonColors(xui_widget pWidget, uint32_t iButton, uint32_t iHover, uint32_t iOpen)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iButtonColor = iButton;
	pData->iButtonHoverColor = iHover;
	pData->iButtonOpenColor = iOpen;
	return __xuiIconPickerInvalidate(pWidget, pData);
}

XUI_API int xuiIconPickerGetButtonColors(xui_widget pWidget, uint32_t* pButton, uint32_t* pHover, uint32_t* pOpen)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pButton != NULL ) *pButton = pData->iButtonColor;
	if ( pHover != NULL ) *pHover = pData->iButtonHoverColor;
	if ( pOpen != NULL ) *pOpen = pData->iButtonOpenColor;
	return XUI_OK;
}

XUI_API int xuiIconPickerSetPopupColors(xui_widget pWidget, uint32_t iPanel, uint32_t iBorder, uint32_t iShadow, uint32_t iHover, uint32_t iSelected, uint32_t iFocus)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iPopupPanelColor = iPanel;
	pData->iPopupBorderColor = iBorder;
	pData->iPopupShadowColor = iShadow;
	pData->iItemHoverColor = iHover;
	pData->iItemSelectedColor = iSelected;
	pData->iItemFocusColor = iFocus;
	(void)__xuiIconPickerApplyPopup(pWidget, pData);
	return __xuiIconPickerInvalidate(pWidget, pData);
}

XUI_API int xuiIconPickerGetPopupColors(xui_widget pWidget, uint32_t* pPanel, uint32_t* pBorder, uint32_t* pShadow, uint32_t* pHover, uint32_t* pSelected, uint32_t* pFocus)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pPanel != NULL ) *pPanel = pData->iPopupPanelColor;
	if ( pBorder != NULL ) *pBorder = pData->iPopupBorderColor;
	if ( pShadow != NULL ) *pShadow = pData->iPopupShadowColor;
	if ( pHover != NULL ) *pHover = pData->iItemHoverColor;
	if ( pSelected != NULL ) *pSelected = pData->iItemSelectedColor;
	if ( pFocus != NULL ) *pFocus = pData->iItemFocusColor;
	return XUI_OK;
}

XUI_API int xuiIconPickerSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pFont = pFont;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_font xuiIconPickerGetFont(xui_widget pWidget)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API xui_widget xuiIconPickerGetPopupWidget(xui_widget pWidget)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	return (pData != NULL) ? pData->pPopup : NULL;
}

XUI_API xui_widget xuiIconPickerGetFrameWidget(xui_widget pWidget)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	return (pData != NULL) ? pData->pFrame : NULL;
}

XUI_API xui_widget xuiIconPickerGetViewportWidget(xui_widget pWidget)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	return (pData != NULL) ? pData->pViewport : NULL;
}

XUI_API xui_rect_t xuiIconPickerGetButtonRect(xui_widget pWidget)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL ) return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	(void)__xuiIconPickerUpdateRects(pWidget, pData);
	return pData->tButtonRect;
}

XUI_API xui_rect_t xuiIconPickerGetValueRect(xui_widget pWidget)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	if ( pData == NULL ) return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	(void)__xuiIconPickerUpdateRects(pWidget, pData);
	return pData->tValueRect;
}

XUI_API xui_rect_t xuiIconPickerGetItemRect(xui_widget pWidget, int iIndex)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	xui_rect_t tRect;
	float fOffsetX;
	float fOffsetY;

	if ( pData == NULL ) return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	tRect = __xuiIconPickerItemContentRect(pData, iIndex);
	fOffsetX = 0.0f;
	fOffsetY = 0.0f;
	if ( pData->pFrame != NULL ) (void)xuiScrollFrameGetOffset(pData->pFrame, &fOffsetX, &fOffsetY);
	tRect.fX -= fOffsetX;
	tRect.fY -= fOffsetY;
	return tRect;
}

XUI_API int xuiIconPickerGetHoverIndex(xui_widget pWidget)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	return (pData != NULL) ? pData->iHoverIndex : -1;
}

XUI_API int xuiIconPickerGetFocusIndex(xui_widget pWidget)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	return (pData != NULL) ? pData->iFocusIndex : -1;
}

XUI_API uint32_t xuiIconPickerGetState(xui_widget pWidget)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	return (pData != NULL) ? __xuiIconPickerState(pWidget, pData) : 0u;
}

XUI_API int xuiIconPickerGetChangeCount(xui_widget pWidget)
{
	xui_icon_picker_data_t* pData = __xuiIconPickerGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}
