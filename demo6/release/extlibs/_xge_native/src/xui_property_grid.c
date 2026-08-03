#include "xui_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XUI_PROPERTY_GRID_VISIBLE_CATEGORY 1
#define XUI_PROPERTY_GRID_VISIBLE_PROPERTY 2

typedef struct xui_property_grid_data_t {
	xui_widget pWidget;
	xui_widget pTableGrid;
	xui_widget pTableView;
	xui_font pFont;
	xui_table_view_column_t arrColumns[2];
	xui_table_view_row_t arrRows[XUI_PROPERTY_GRID_VISIBLE_CAPACITY];
	xui_property_grid_category_t arrCategories[XUI_PROPERTY_GRID_CATEGORY_CAPACITY];
	xui_property_grid_property_t arrProperties[XUI_PROPERTY_GRID_PROPERTY_CAPACITY];
	int arrVisibleKind[XUI_PROPERTY_GRID_VISIBLE_CAPACITY];
	int arrVisibleIndex[XUI_PROPERTY_GRID_VISIBLE_CAPACITY];
	int iCategoryCount;
	int iPropertyCount;
	int iVisibleCount;
	int iSelectedProperty;
	int iDescriptionMode;
	float fDescriptionPanelHeight;
	float fNameWidth;
	float fRowHeight;
	float fCategoryHeight;
	xui_rect_t tDescriptionRect;
	xui_property_grid_style_t tStyle;
	xui_property_grid_select_proc onSelect;
	void* pSelectUser;
	xui_property_grid_validate_proc onValidate;
	void* pValidateUser;
	xui_property_grid_change_proc onChange;
	void* pChangeUser;
	xui_property_grid_action_proc onAction;
	void* pActionUser;
	xui_property_grid_render_proc onRender;
	void* pRenderUser;
	int iSelectCount;
	int iToggleCount;
} xui_property_grid_data_t;

static xui_property_grid_data_t* __xuiPropertyGridGetData(xui_widget pWidget);

static int __xuiPropertyGridAlpha(uint32_t iColor)
{
	return (int)(iColor & 0xffu);
}

static float __xuiPropertyGridMax(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static float __xuiPropertyGridMin(float fA, float fB)
{
	return (fA < fB) ? fA : fB;
}

static int __xuiPropertyGridFloatSame(float fA, float fB)
{
	float fDelta = fA - fB;
	if ( fDelta < 0.0f ) fDelta = -fDelta;
	return fDelta < 0.01f;
}

static void __xuiPropertyGridCopy(char* sDst, int iDstSize, const char* sSrc)
{
	if ( (sDst == NULL) || (iDstSize <= 0) ) {
		return;
	}
	if ( sSrc == NULL ) {
		sSrc = "";
	}
	snprintf(sDst, (size_t)iDstSize, "%s", sSrc);
	sDst[iDstSize - 1] = '\0';
}

static int __xuiPropertyGridTextEqualNoCase(const char* sA, const char* sB)
{
	unsigned char a;
	unsigned char b;

	if ( (sA == NULL) || (sB == NULL) ) {
		return 0;
	}
	while ( (*sA != '\0') && (*sB != '\0') ) {
		a = (unsigned char)*sA++;
		b = (unsigned char)*sB++;
		if ( (a >= 'A') && (a <= 'Z') ) a = (unsigned char)(a - 'A' + 'a');
		if ( (b >= 'A') && (b <= 'Z') ) b = (unsigned char)(b - 'A' + 'a');
		if ( a != b ) return 0;
	}
	return (*sA == '\0') && (*sB == '\0');
}

static int __xuiPropertyGridBoolText(const char* sText)
{
	if ( sText == NULL ) {
		return 0;
	}
	return (strcmp(sText, "1") == 0) ||
	       __xuiPropertyGridTextEqualNoCase(sText, "true") ||
	       __xuiPropertyGridTextEqualNoCase(sText, "yes") ||
	       __xuiPropertyGridTextEqualNoCase(sText, "on") ||
	       __xuiPropertyGridTextEqualNoCase(sText, "enabled") ||
	       __xuiPropertyGridTextEqualNoCase(sText, "checked");
}

static int __xuiPropertyGridHexValue(int c)
{
	if ( c >= '0' && c <= '9' ) return c - '0';
	if ( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
	if ( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
	return -1;
}

static int __xuiPropertyGridParseColor(const char* sText, uint32_t* pColor)
{
	int iLen;
	int i;
	int v;
	uint32_t iValue;
	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char a;

	if ( (sText == NULL) || (pColor == NULL) ) {
		return 0;
	}
	while ( (*sText == ' ') || (*sText == '\t') ) {
		sText++;
	}
	if ( *sText == '#' ) {
		sText++;
	}
	iLen = 0;
	while ( sText[iLen] != '\0' && sText[iLen] != ' ' && sText[iLen] != '\t' ) {
		iLen++;
	}
	if ( (iLen != 6) && (iLen != 8) ) {
		return 0;
	}
	iValue = 0;
	for ( i = 0; i < iLen; i++ ) {
		v = __xuiPropertyGridHexValue((unsigned char)sText[i]);
		if ( v < 0 ) return 0;
		iValue = (iValue << 4) | (uint32_t)v;
	}
	if ( iLen == 6 ) {
		r = (unsigned char)((iValue >> 16) & 0xffu);
		g = (unsigned char)((iValue >> 8) & 0xffu);
		b = (unsigned char)(iValue & 0xffu);
		a = 255;
	} else {
		r = (unsigned char)((iValue >> 24) & 0xffu);
		g = (unsigned char)((iValue >> 16) & 0xffu);
		b = (unsigned char)((iValue >> 8) & 0xffu);
		a = (unsigned char)(iValue & 0xffu);
	}
	*pColor = XUI_COLOR_RGBA(r, g, b, a);
	return 1;
}

static void __xuiPropertyGridFormatColor(uint32_t iColor, char* sValue, int iCapacity)
{
	unsigned int r;
	unsigned int g;
	unsigned int b;
	unsigned int a;

	if ( (sValue == NULL) || (iCapacity <= 0) ) {
		return;
	}
	r = (iColor >> 24) & 0xffu;
	g = (iColor >> 16) & 0xffu;
	b = (iColor >> 8) & 0xffu;
	a = iColor & 0xffu;
	if ( a == 255 ) {
		snprintf(sValue, (size_t)iCapacity, "#%02X%02X%02X", r, g, b);
	} else {
		snprintf(sValue, (size_t)iCapacity, "#%02X%02X%02X%02X", r, g, b, a);
	}
	sValue[iCapacity - 1] = '\0';
}

static int __xuiPropertyGridDescValid(const xui_property_grid_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) {
		return 0;
	}
	if ( (pDesc->fNameWidth < 0.0f) || (pDesc->fRowHeight < 0.0f) ||
	     (pDesc->fCategoryHeight < 0.0f) || (pDesc->fDescriptionPanelHeight < 0.0f) ) {
		return 0;
	}
	return 1;
}

static int __xuiPropertyGridDescriptionModeValid(int iMode)
{
	return (iMode == XUI_PROPERTY_GRID_DESCRIPTION_NONE) ||
	       (iMode == XUI_PROPERTY_GRID_DESCRIPTION_TOOLTIP) ||
	       (iMode == XUI_PROPERTY_GRID_DESCRIPTION_PANEL) ||
	       (iMode == XUI_PROPERTY_GRID_DESCRIPTION_BOTH);
}

static int __xuiPropertyGridEditModeValid(int iMode)
{
	return (iMode == XUI_TABLE_GRID_EDIT_DISPLAY) ||
	       (iMode == XUI_TABLE_GRID_EDIT_QUICK) ||
	       (iMode == XUI_TABLE_GRID_EDIT_IMMEDIATE);
}

static int __xuiPropertyGridScrollbarModeValid(int iMode)
{
	return (iMode == XUI_SCROLLBAR_MODE_FULL) || (iMode == XUI_SCROLLBAR_MODE_COMPACT);
}

static int __xuiPropertyGridValidCategory(const xui_property_grid_data_t* pData, int iCategory)
{
	return (pData != NULL) && (iCategory >= 0) && (iCategory < pData->iCategoryCount);
}

static int __xuiPropertyGridValidProperty(const xui_property_grid_data_t* pData, int iProperty)
{
	return (pData != NULL) && (iProperty >= 0) && (iProperty < pData->iPropertyCount);
}

static int __xuiPropertyGridVisiblePropertyRow(const xui_property_grid_data_t* pData, int iProperty)
{
	int i;

	if ( pData == NULL ) {
		return -1;
	}
	for ( i = 0; i < pData->iVisibleCount; i++ ) {
		if ( (pData->arrVisibleKind[i] == XUI_PROPERTY_GRID_VISIBLE_PROPERTY) &&
		     (pData->arrVisibleIndex[i] == iProperty) ) {
			return i;
		}
	}
	return -1;
}

static int __xuiPropertyGridVisibleRowProperty(const xui_property_grid_data_t* pData, int iRow)
{
	if ( (pData == NULL) || (iRow < 0) || (iRow >= pData->iVisibleCount) ) {
		return -1;
	}
	return (pData->arrVisibleKind[iRow] == XUI_PROPERTY_GRID_VISIBLE_PROPERTY) ? pData->arrVisibleIndex[iRow] : -1;
}

static int __xuiPropertyGridVisibleRowCategory(const xui_property_grid_data_t* pData, int iRow)
{
	if ( (pData == NULL) || (iRow < 0) || (iRow >= pData->iVisibleCount) ) {
		return -1;
	}
	return (pData->arrVisibleKind[iRow] == XUI_PROPERTY_GRID_VISIBLE_CATEGORY) ? pData->arrVisibleIndex[iRow] : -1;
}

static int __xuiPropertyGridPropertyDirty(const xui_property_grid_property_t* pProp)
{
	if ( pProp == NULL ) {
		return 0;
	}
	if ( (pProp->iFlags & XUI_PROPERTY_FLAG_DIRTY) != 0 ) {
		return 1;
	}
	return (pProp->bAutoDirty != 0) &&
	       (pProp->sDefaultValue[0] != '\0') &&
	       (strcmp(pProp->sValue, pProp->sDefaultValue) != 0);
}

static void __xuiPropertyGridDefaultStyle(xui_property_grid_style_t* pStyle)
{
	if ( pStyle == NULL ) {
		return;
	}
	memset(pStyle, 0, sizeof(*pStyle));
	pStyle->iBackgroundColor = XUI_COLOR_RGBA(248, 250, 253, 255);
	pStyle->iGridColor = XUI_COLOR_RGBA(184, 211, 232, 255);
	pStyle->iCategoryBackgroundColor = XUI_COLOR_RGBA(218, 235, 249, 255);
	pStyle->iCategoryHoverColor = XUI_COLOR_RGBA(203, 226, 245, 255);
	pStyle->iCategoryTextColor = XUI_COLOR_RGBA(40, 84, 126, 255);
	pStyle->iCategoryIconColor = XUI_COLOR_RGBA(48, 122, 190, 255);
	pStyle->iNameBackgroundColor = XUI_COLOR_RGBA(235, 242, 249, 255);
	pStyle->iNameTextColor = XUI_COLOR_RGBA(58, 77, 96, 255);
	pStyle->iNameHoverColor = XUI_COLOR_RGBA(225, 236, 246, 255);
	pStyle->iValueBackgroundColor = XUI_COLOR_RGBA(248, 250, 253, 255);
	pStyle->iValueTextColor = XUI_COLOR_RGBA(42, 52, 66, 255);
	pStyle->iSelectedColor = XUI_COLOR_RGBA(190, 219, 242, 255);
	pStyle->iReadonlyTextColor = XUI_COLOR_RGBA(116, 128, 142, 255);
	pStyle->iInvalidColor = XUI_COLOR_RGBA(220, 64, 64, 230);
	pStyle->iDirtyColor = XUI_COLOR_RGBA(240, 166, 52, 230);
}

static void __xuiPropertyGridApplyDesc(xui_property_grid_data_t* pData, const xui_property_grid_desc_t* pDesc)
{
	xui_property_grid_style_t tDefaultStyle;

	__xuiPropertyGridDefaultStyle(&tDefaultStyle);
	pData->pFont = (pDesc != NULL) ? pDesc->pFont : NULL;
	pData->fNameWidth = 150.0f;
	pData->fRowHeight = 24.0f;
	pData->fCategoryHeight = 26.0f;
	pData->iDescriptionMode = XUI_PROPERTY_GRID_DESCRIPTION_TOOLTIP;
	pData->fDescriptionPanelHeight = 0.0f;
	pData->tStyle = tDefaultStyle;
	if ( pDesc == NULL ) {
		return;
	}
	if ( pDesc->fNameWidth > 20.0f ) pData->fNameWidth = pDesc->fNameWidth;
	if ( pDesc->fRowHeight > 8.0f ) pData->fRowHeight = pDesc->fRowHeight;
	if ( pDesc->fCategoryHeight > 8.0f ) pData->fCategoryHeight = pDesc->fCategoryHeight;
	if ( __xuiPropertyGridDescriptionModeValid(pDesc->iDescriptionMode) ) pData->iDescriptionMode = pDesc->iDescriptionMode;
	if ( pDesc->fDescriptionPanelHeight > 0.0f ) pData->fDescriptionPanelHeight = pDesc->fDescriptionPanelHeight;
	if ( pDesc->iSize >= sizeof(*pDesc) ) {
		if ( __xuiPropertyGridAlpha(pDesc->tStyle.iBackgroundColor) != 0 ) pData->tStyle.iBackgroundColor = pDesc->tStyle.iBackgroundColor;
		if ( __xuiPropertyGridAlpha(pDesc->tStyle.iGridColor) != 0 ) pData->tStyle.iGridColor = pDesc->tStyle.iGridColor;
		if ( __xuiPropertyGridAlpha(pDesc->tStyle.iCategoryBackgroundColor) != 0 ) pData->tStyle.iCategoryBackgroundColor = pDesc->tStyle.iCategoryBackgroundColor;
		if ( __xuiPropertyGridAlpha(pDesc->tStyle.iCategoryHoverColor) != 0 ) pData->tStyle.iCategoryHoverColor = pDesc->tStyle.iCategoryHoverColor;
		if ( __xuiPropertyGridAlpha(pDesc->tStyle.iCategoryTextColor) != 0 ) pData->tStyle.iCategoryTextColor = pDesc->tStyle.iCategoryTextColor;
		if ( __xuiPropertyGridAlpha(pDesc->tStyle.iCategoryIconColor) != 0 ) pData->tStyle.iCategoryIconColor = pDesc->tStyle.iCategoryIconColor;
		if ( __xuiPropertyGridAlpha(pDesc->tStyle.iNameBackgroundColor) != 0 ) pData->tStyle.iNameBackgroundColor = pDesc->tStyle.iNameBackgroundColor;
		if ( __xuiPropertyGridAlpha(pDesc->tStyle.iNameTextColor) != 0 ) pData->tStyle.iNameTextColor = pDesc->tStyle.iNameTextColor;
		if ( __xuiPropertyGridAlpha(pDesc->tStyle.iNameHoverColor) != 0 ) pData->tStyle.iNameHoverColor = pDesc->tStyle.iNameHoverColor;
		if ( __xuiPropertyGridAlpha(pDesc->tStyle.iValueBackgroundColor) != 0 ) pData->tStyle.iValueBackgroundColor = pDesc->tStyle.iValueBackgroundColor;
		if ( __xuiPropertyGridAlpha(pDesc->tStyle.iValueTextColor) != 0 ) pData->tStyle.iValueTextColor = pDesc->tStyle.iValueTextColor;
		if ( __xuiPropertyGridAlpha(pDesc->tStyle.iSelectedColor) != 0 ) pData->tStyle.iSelectedColor = pDesc->tStyle.iSelectedColor;
		if ( __xuiPropertyGridAlpha(pDesc->tStyle.iReadonlyTextColor) != 0 ) pData->tStyle.iReadonlyTextColor = pDesc->tStyle.iReadonlyTextColor;
		if ( __xuiPropertyGridAlpha(pDesc->tStyle.iInvalidColor) != 0 ) pData->tStyle.iInvalidColor = pDesc->tStyle.iInvalidColor;
		if ( __xuiPropertyGridAlpha(pDesc->tStyle.iDirtyColor) != 0 ) pData->tStyle.iDirtyColor = pDesc->tStyle.iDirtyColor;
	}
}

static int __xuiPropertyGridColumnSame(const xui_table_view_column_t* pA, const xui_table_view_column_t* pB)
{
	if ( (pA == NULL) || (pB == NULL) ) {
		return 0;
	}
	return (pA->iId == pB->iId) &&
	       (pA->sId == pB->sId || ((pA->sId != NULL) && (pB->sId != NULL) && (strcmp(pA->sId, pB->sId) == 0))) &&
	       (pA->sTitle == pB->sTitle || ((pA->sTitle != NULL) && (pB->sTitle != NULL) && (strcmp(pA->sTitle, pB->sTitle) == 0))) &&
	       __xuiPropertyGridFloatSame(pA->fWidth, pB->fWidth) &&
	       __xuiPropertyGridFloatSame(pA->fMinWidth, pB->fMinWidth) &&
	       __xuiPropertyGridFloatSame(pA->fMaxWidth, pB->fMaxWidth) &&
	       (pA->bVisible == pB->bVisible) &&
	       (pA->bVisibleSet == pB->bVisibleSet) &&
	       (pA->bResizable == pB->bResizable) &&
	       (pA->bResizableSet == pB->bResizableSet) &&
	       (pA->iAlign == pB->iAlign) &&
	       (pA->iType == pB->iType) &&
	       (pA->bHasStyle == pB->bHasStyle) &&
	       (pA->iBackgroundColor == pB->iBackgroundColor) &&
	       (pA->iTextColor == pB->iTextColor) &&
	       (pA->iGridColor == pB->iGridColor) &&
	       (pA->onFormat == pB->onFormat) &&
	       (pA->pFormatUser == pB->pFormatUser) &&
	       (pA->onRender == pB->onRender) &&
	       (pA->pRenderUser == pB->pRenderUser);
}

static void __xuiPropertyGridSyncColumns(xui_property_grid_data_t* pData)
{
	xui_table_view_column_t arrColumns[2];
	xui_rect_t tViewport;
	xui_rect_t tContent;
	float fAvailableWidth;
	float fNameWidth;
	float fValueWidth;

	if ( (pData == NULL) || (pData->pTableGrid == NULL) || (pData->pTableView == NULL) ) {
		return;
	}
	tViewport = xuiTableViewGetViewportRect(pData->pTableView);
	tContent = xuiWidgetGetContentRect(pData->pWidget);
	fAvailableWidth = (tViewport.fW > 1.0f) ? tViewport.fW : tContent.fW;
	if ( fAvailableWidth > 1.0f ) {
		fAvailableWidth -= 1.0f;
	}
	if ( fAvailableWidth < 40.0f ) {
		fAvailableWidth = 40.0f;
	}
	fNameWidth = pData->fNameWidth;
	if ( fNameWidth > fAvailableWidth - 40.0f ) {
		fNameWidth = fAvailableWidth - 40.0f;
	}
	if ( fNameWidth < 40.0f ) {
		fNameWidth = (fAvailableWidth > 80.0f) ? 40.0f : (fAvailableWidth * 0.5f);
	}
	fValueWidth = fAvailableWidth - fNameWidth;
	if ( fValueWidth < 1.0f ) {
		fValueWidth = 1.0f;
	}
	memset(arrColumns, 0, sizeof(arrColumns));
	arrColumns[0].iId = 0;
	arrColumns[0].sId = "name";
	arrColumns[0].sTitle = "";
	arrColumns[0].fWidth = fNameWidth;
	arrColumns[0].fMinWidth = 60.0f;
	arrColumns[0].bVisible = 1;
	arrColumns[0].bVisibleSet = 1;
	arrColumns[0].bResizable = 0;
	arrColumns[0].bResizableSet = 1;
	arrColumns[0].iAlign = XUI_TEXT_ALIGN_LEFT;
	arrColumns[0].iType = XUI_TABLE_CELL_TYPE_TEXT;
	arrColumns[1] = arrColumns[0];
	arrColumns[1].iId = 1;
	arrColumns[1].sId = "value";
	arrColumns[1].fWidth = fValueWidth;
	arrColumns[1].fMinWidth = 1.0f;
	if ( __xuiPropertyGridColumnSame(&pData->arrColumns[0], &arrColumns[0]) &&
	     __xuiPropertyGridColumnSame(&pData->arrColumns[1], &arrColumns[1]) ) {
		return;
	}
	pData->arrColumns[0] = arrColumns[0];
	pData->arrColumns[1] = arrColumns[1];
	(void)xuiTableGridSetColumns(pData->pTableGrid, pData->arrColumns, 2);
}

static void __xuiPropertyGridApplyStyle(xui_property_grid_data_t* pData)
{
	if ( (pData == NULL) || (pData->pTableGrid == NULL) || (pData->pTableView == NULL) ) {
		return;
	}
	(void)xuiTableGridSetColors(pData->pTableGrid,
		pData->tStyle.iBackgroundColor,
		pData->tStyle.iCategoryBackgroundColor,
		pData->tStyle.iValueBackgroundColor,
		pData->tStyle.iSelectedColor,
		pData->tStyle.iGridColor,
		pData->tStyle.iValueTextColor);
	(void)xuiTableViewSetDisabledTextColor(pData->pTableView, pData->tStyle.iReadonlyTextColor);
}

static void __xuiPropertyGridRebuildVisible(xui_property_grid_data_t* pData)
{
	int i;
	int iCategory;

	if ( pData == NULL ) {
		return;
	}
	pData->iVisibleCount = 0;
	memset(pData->arrRows, 0, sizeof(pData->arrRows));
	for ( iCategory = 0; (iCategory < pData->iCategoryCount) && (pData->iVisibleCount < XUI_PROPERTY_GRID_VISIBLE_CAPACITY); iCategory++ ) {
		pData->arrVisibleKind[pData->iVisibleCount] = XUI_PROPERTY_GRID_VISIBLE_CATEGORY;
		pData->arrVisibleIndex[pData->iVisibleCount] = iCategory;
		pData->arrRows[pData->iVisibleCount].fHeight = pData->fCategoryHeight;
		pData->arrRows[pData->iVisibleCount].bSelected = 0;
		pData->arrRows[pData->iVisibleCount].bDisabled = 0;
		pData->arrRows[pData->iVisibleCount].bHasStyle = 1;
		pData->arrRows[pData->iVisibleCount].iBackgroundColor = pData->tStyle.iCategoryBackgroundColor;
		pData->arrRows[pData->iVisibleCount].iTextColor = pData->tStyle.iCategoryTextColor;
		pData->arrRows[pData->iVisibleCount].iGridColor = pData->tStyle.iGridColor;
		pData->iVisibleCount++;
		if ( pData->arrCategories[iCategory].bExpanded == 0 ) {
			continue;
		}
		for ( i = 0; (i < pData->iPropertyCount) && (pData->iVisibleCount < XUI_PROPERTY_GRID_VISIBLE_CAPACITY); i++ ) {
			if ( pData->arrProperties[i].iCategory != iCategory ) {
				continue;
			}
			if ( (pData->arrProperties[i].iFlags & XUI_PROPERTY_FLAG_HIDDEN) != 0 ) {
				continue;
			}
			pData->arrVisibleKind[pData->iVisibleCount] = XUI_PROPERTY_GRID_VISIBLE_PROPERTY;
			pData->arrVisibleIndex[pData->iVisibleCount] = i;
			pData->arrRows[pData->iVisibleCount].fHeight = pData->fRowHeight;
			pData->arrRows[pData->iVisibleCount].bSelected = (i == pData->iSelectedProperty);
			pData->iVisibleCount++;
		}
	}
	if ( pData->pTableGrid != NULL ) {
		(void)xuiTableGridSetRows(pData->pTableGrid, pData->arrRows, pData->iVisibleCount);
	}
	if ( pData->pTableView != NULL ) {
		(void)xuiTableViewRefresh(pData->pTableView);
	}
	if ( pData->pWidget != NULL ) {
		(void)xuiWidgetInvalidate(pData->pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
}

static int __xuiPropertyGridCountProc(xui_widget pWidget, void* pUser)
{
	xui_property_grid_data_t* pData = (xui_property_grid_data_t*)pUser;
	(void)pWidget;
	return (pData != NULL) ? pData->iVisibleCount : 0;
}

static void __xuiPropertyGridDrawCategoryArrow(xui_property_grid_data_t* pData, xui_draw_context pDraw, xui_rect_t tRect, int bExpanded)
{
	xui_proxy_t tProxy;
	xui_vec2_t tA;
	xui_vec2_t tB;
	xui_vec2_t tC;
	float fCX;
	float fCY;
	float fS;

	if ( (pData == NULL) || (pDraw == NULL) || (xuiGetProxy(xuiWidgetGetContext(pData->pWidget), &tProxy) != XUI_OK) ||
	     (tProxy.drawTriangleFill == NULL) ) {
		return;
	}
	fCX = tRect.fX + 12.0f;
	fCY = tRect.fY + tRect.fH * 0.5f;
	fS = 4.0f;
	if ( bExpanded ) {
		tA = (xui_vec2_t){fCX - fS, fCY - 2.0f};
		tB = (xui_vec2_t){fCX + fS, fCY - 2.0f};
		tC = (xui_vec2_t){fCX, fCY + 4.0f};
	} else {
		tA = (xui_vec2_t){fCX - 2.0f, fCY - fS};
		tB = (xui_vec2_t){fCX - 2.0f, fCY + fS};
		tC = (xui_vec2_t){fCX + 4.0f, fCY};
	}
	(void)tProxy.drawTriangleFill(&tProxy, pDraw, tA, tB, tC, pData->tStyle.iCategoryIconColor);
}

static int __xuiPropertyGridCategoryRenderer(xui_widget pWidget, int iRow, int iColumn, const xui_table_view_cell_t* pCell, xui_draw_context_t* pDraw, xui_rect_t tRect, int iState, void* pUser)
{
	xui_property_grid_data_t* pData;
	xui_property_grid_category_t* pCategory;
	xui_proxy_t tProxy;
	xui_rect_t tText;
	uint32_t iBackground;
	int iCategory;
	int iRet;

	(void)pWidget;
	(void)iColumn;
	(void)pCell;
	pData = (xui_property_grid_data_t*)pUser;
	iCategory = __xuiPropertyGridVisibleRowCategory(pData, iRow);
	if ( !__xuiPropertyGridValidCategory(pData, iCategory) ) {
		return 0;
	}
	if ( xuiGetProxy(xuiWidgetGetContext(pData->pWidget), &tProxy) != XUI_OK ) {
		return 0;
	}
	pCategory = &pData->arrCategories[iCategory];
	iBackground = ((iState & XUI_TABLE_CELL_HOVER) != 0) ? pData->tStyle.iCategoryHoverColor : pData->tStyle.iCategoryBackgroundColor;
	if ( tProxy.drawRectFill != NULL ) {
		iRet = tProxy.drawRectFill(&tProxy, pDraw, xuiInternalSnapRect(tRect), iBackground);
		if ( iRet != XUI_OK ) return iRet;
	}
	__xuiPropertyGridDrawCategoryArrow(pData, pDraw, tRect, pCategory->bExpanded);
	tText = tRect;
	tText.fX += 24.0f;
	tText.fW = __xuiPropertyGridMax(1.0f, tText.fW - 28.0f);
	if ( (tProxy.drawText != NULL) && (pData->pFont != NULL) ) {
		iRet = tProxy.drawText(&tProxy, pDraw, pData->pFont, pCategory->sName, xuiInternalSnapRect(tText), pData->tStyle.iCategoryTextColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( tProxy.drawLine != NULL ) {
		iRet = tProxy.drawLine(&tProxy, pDraw, tRect.fX, tRect.fY + tRect.fH - 0.5f, tRect.fX + tRect.fW, tRect.fY + tRect.fH - 0.5f, 1.0f, pData->tStyle.iGridColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	return 1;
}

static int __xuiPropertyGridNameRenderer(xui_widget pWidget, int iRow, int iColumn, const xui_table_view_cell_t* pCell, xui_draw_context_t* pDraw, xui_rect_t tRect, int iState, void* pUser)
{
	xui_property_grid_data_t* pData;
	xui_property_grid_property_t* pProp;
	xui_proxy_t tProxy;
	xui_rect_t tText;
	uint32_t iBackground;
	uint32_t iText;
	int iProperty;
	int iRet;

	(void)pWidget;
	(void)iColumn;
	(void)pCell;
	pData = (xui_property_grid_data_t*)pUser;
	iProperty = __xuiPropertyGridVisibleRowProperty(pData, iRow);
	if ( !__xuiPropertyGridValidProperty(pData, iProperty) ) {
		return 0;
	}
	if ( xuiGetProxy(xuiWidgetGetContext(pData->pWidget), &tProxy) != XUI_OK ) {
		return 0;
	}
	pProp = &pData->arrProperties[iProperty];
	iBackground = ((iState & XUI_TABLE_CELL_HOVER) != 0) ? pData->tStyle.iNameHoverColor : pData->tStyle.iNameBackgroundColor;
	if ( tProxy.drawRectFill != NULL ) {
		iRet = tProxy.drawRectFill(&tProxy, pDraw, xuiInternalSnapRect(tRect), iBackground);
		if ( iRet != XUI_OK ) return iRet;
	}
	iText = ((pProp->iFlags & XUI_PROPERTY_FLAG_DISABLED) != 0) ? pData->tStyle.iReadonlyTextColor : pData->tStyle.iNameTextColor;
	tText = tRect;
	tText.fX += 8.0f;
	tText.fW = __xuiPropertyGridMax(1.0f, tText.fW - 12.0f);
	if ( (tProxy.drawText != NULL) && (pData->pFont != NULL) ) {
		iRet = tProxy.drawText(&tProxy, pDraw, pData->pFont, pProp->sName, xuiInternalSnapRect(tText), iText, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( tProxy.drawLine != NULL ) {
		iRet = tProxy.drawLine(&tProxy, pDraw, tRect.fX, tRect.fY + tRect.fH - 0.5f, tRect.fX + tRect.fW, tRect.fY + tRect.fH - 0.5f, 1.0f, pData->tStyle.iGridColor);
		if ( iRet != XUI_OK ) return iRet;
		iRet = tProxy.drawLine(&tProxy, pDraw, tRect.fX + tRect.fW - 0.5f, tRect.fY, tRect.fX + tRect.fW - 0.5f, tRect.fY + tRect.fH, 1.0f, pData->tStyle.iGridColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	return 1;
}

static int __xuiPropertyGridValueRenderer(xui_widget pWidget, int iRow, int iColumn, const xui_table_view_cell_t* pCell, xui_draw_context_t* pDraw, xui_rect_t tRect, int iState, void* pUser)
{
	xui_property_grid_data_t* pData;
	xui_property_grid_property_t* pProp;
	xui_property_grid_render_proc onRender;
	void* pRenderUser;
	xui_proxy_t tProxy;
	xui_rect_t tText;
	xui_vec2_t tA;
	xui_vec2_t tB;
	xui_vec2_t tC;
	const char* sText;
	uint32_t iText;
	int iProperty;
	int iValue;
	int iRet;
	int i;

	(void)pWidget;
	pData = (xui_property_grid_data_t*)pUser;
	iProperty = __xuiPropertyGridVisibleRowProperty(pData, iRow);
	if ( !__xuiPropertyGridValidProperty(pData, iProperty) ) {
		return 0;
	}
	pProp = &pData->arrProperties[iProperty];
	onRender = (pProp->onRender != NULL) ? pProp->onRender : pData->onRender;
	pRenderUser = (pProp->onRender != NULL) ? pProp->pRenderUser : pData->pRenderUser;
	if ( onRender != NULL ) {
		iRet = onRender(pData->pWidget, iProperty, iColumn, pCell, pDraw, tRect, iState, pRenderUser);
		if ( iRet != 0 ) return iRet;
	}
	if ( pProp->iType != XUI_TABLE_CELL_TYPE_ENUM || pProp->bHasEditorConfig == 0 ) {
		return 0;
	}
	if ( xuiGetProxy(xuiWidgetGetContext(pData->pWidget), &tProxy) != XUI_OK ) {
		return 0;
	}
	sText = pProp->sValue;
	if ( pProp->tEditorConfig.iEnumItemCount > 0 ) {
		if ( pProp->tEditorConfig.bEnumUseValue ) {
			iValue = atoi(pProp->sValue);
			if ( pProp->tEditorConfig.arrEnumItemData != NULL ) {
				for ( i = 0; i < pProp->tEditorConfig.iEnumItemCount; i++ ) {
					if ( pProp->tEditorConfig.arrEnumItemData[i].iValue == iValue &&
					     pProp->tEditorConfig.arrEnumItemData[i].sText != NULL ) {
						sText = pProp->tEditorConfig.arrEnumItemData[i].sText;
						break;
					}
				}
			} else if ( pProp->tEditorConfig.arrEnumItems != NULL &&
			            iValue >= 0 && iValue < pProp->tEditorConfig.iEnumItemCount &&
			            pProp->tEditorConfig.arrEnumItems[iValue] != NULL ) {
				sText = pProp->tEditorConfig.arrEnumItems[iValue];
			}
		} else {
			if ( pProp->tEditorConfig.arrEnumItemData != NULL ) {
				for ( i = 0; i < pProp->tEditorConfig.iEnumItemCount; i++ ) {
					if ( pProp->tEditorConfig.arrEnumItemData[i].sText != NULL &&
					     strcmp(pProp->tEditorConfig.arrEnumItemData[i].sText, pProp->sValue) == 0 ) {
						sText = pProp->tEditorConfig.arrEnumItemData[i].sText;
						break;
					}
				}
			} else if ( pProp->tEditorConfig.arrEnumItems != NULL ) {
				for ( i = 0; i < pProp->tEditorConfig.iEnumItemCount; i++ ) {
					if ( pProp->tEditorConfig.arrEnumItems[i] != NULL &&
					     strcmp(pProp->tEditorConfig.arrEnumItems[i], pProp->sValue) == 0 ) {
						sText = pProp->tEditorConfig.arrEnumItems[i];
						break;
					}
				}
			}
		}
	}
	iText = (pCell != NULL && __xuiPropertyGridAlpha(pCell->iTextColor) != 0) ? pCell->iTextColor : pData->tStyle.iValueTextColor;
	tText = (xui_rect_t){tRect.fX + 8.0f, tRect.fY + 1.0f, __xuiPropertyGridMax(1.0f, tRect.fW - 16.0f), __xuiPropertyGridMax(1.0f, tRect.fH - 2.0f)};
	if ( (tProxy.drawText != NULL) && (pData->pFont != NULL) && (__xuiPropertyGridAlpha(iText) != 0) ) {
		iRet = tProxy.drawText(&tProxy, pDraw, pData->pFont, sText, xuiInternalSnapRect(tText), iText, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( tProxy.drawLine != NULL ) {
		iRet = tProxy.drawLine(&tProxy, pDraw, tRect.fX, tRect.fY + tRect.fH - 0.5f, tRect.fX + tRect.fW, tRect.fY + tRect.fH - 0.5f, 1.0f, (pCell != NULL) ? pCell->iGridColor : pData->tStyle.iGridColor);
		if ( iRet != XUI_OK ) return iRet;
		iRet = tProxy.drawLine(&tProxy, pDraw, tRect.fX + tRect.fW - 0.5f, tRect.fY, tRect.fX + tRect.fW - 0.5f, tRect.fY + tRect.fH, 1.0f, (pCell != NULL) ? pCell->iGridColor : pData->tStyle.iGridColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( ((iState & XUI_TABLE_CELL_INVALID) != 0) && tProxy.drawRectFill != NULL ) {
		iRet = tProxy.drawRectFill(&tProxy, pDraw, xuiInternalSnapRect((xui_rect_t){tRect.fX, tRect.fY, 3.0f, tRect.fH}), XUI_COLOR_RGBA(218, 82, 82, 255));
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( ((iState & XUI_TABLE_CELL_DIRTY) != 0) && tProxy.drawTriangleFill != NULL ) {
		tA = (xui_vec2_t){tRect.fX + tRect.fW - 8.0f, tRect.fY};
		tB = (xui_vec2_t){tRect.fX + tRect.fW, tRect.fY};
		tC = (xui_vec2_t){tRect.fX + tRect.fW, tRect.fY + 8.0f};
		iRet = tProxy.drawTriangleFill(&tProxy, pDraw, tA, tB, tC, XUI_COLOR_RGBA(245, 158, 11, 255));
		if ( iRet != XUI_OK ) return iRet;
	}
	return 1;
}

static int __xuiPropertyGridCellProc(xui_widget pWidget, int iRow, int iColumn, xui_table_view_cell_t* pCell, void* pUser)
{
	xui_property_grid_data_t* pData;
	xui_property_grid_property_t* pProp;
	xui_property_grid_category_t* pCategory;
	int iIndex;

	(void)pWidget;
	pData = (xui_property_grid_data_t*)pUser;
	if ( (pData == NULL) || (pCell == NULL) || (iRow < 0) || (iRow >= pData->iVisibleCount) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->arrVisibleKind[iRow] == XUI_PROPERTY_GRID_VISIBLE_CATEGORY ) {
		iIndex = pData->arrVisibleIndex[iRow];
		if ( !__xuiPropertyGridValidCategory(pData, iIndex) ) return XUI_ERROR_INVALID_ARGUMENT;
		pCategory = &pData->arrCategories[iIndex];
		pCell->sText = pCategory->sName;
		pCell->iType = XUI_TABLE_CELL_TYPE_TEXT;
		pCell->iColSpan = 2;
		pCell->onRender = __xuiPropertyGridCategoryRenderer;
		pCell->pRenderUser = pData;
		pCell->bHasStyle = 1;
		pCell->iBackgroundColor = pData->tStyle.iCategoryBackgroundColor;
		pCell->iTextColor = pData->tStyle.iCategoryTextColor;
		pCell->iGridColor = pData->tStyle.iGridColor;
		if ( iColumn > 0 ) {
			pCell->sText = "";
		}
		return XUI_OK;
	}
	iIndex = pData->arrVisibleIndex[iRow];
	if ( !__xuiPropertyGridValidProperty(pData, iIndex) ) return XUI_ERROR_INVALID_ARGUMENT;
	pProp = &pData->arrProperties[iIndex];
	pCell->sText = (iColumn == 0) ? pProp->sName : pProp->sValue;
	pCell->sTooltip = ((pData->iDescriptionMode == XUI_PROPERTY_GRID_DESCRIPTION_TOOLTIP || pData->iDescriptionMode == XUI_PROPERTY_GRID_DESCRIPTION_BOTH) && pProp->sDescription[0] != '\0') ? pProp->sDescription : NULL;
	pCell->iType = (iColumn == 0) ? XUI_TABLE_CELL_TYPE_TEXT : pProp->iType;
	pCell->bInvalid = (iColumn == 1) && ((pProp->iFlags & XUI_PROPERTY_FLAG_INVALID) != 0);
	pCell->bDirty = (iColumn == 1) && __xuiPropertyGridPropertyDirty(pProp);
	pCell->bDisabled = (iColumn == 0) ||
	                   ((pProp->iFlags & XUI_PROPERTY_FLAG_DISABLED) != 0) ||
	                   ((iColumn == 1) && ((pProp->iFlags & XUI_PROPERTY_FLAG_READONLY) != 0));
	pCell->bHasStyle = 1;
	pCell->iBackgroundColor = (iColumn == 0) ? pData->tStyle.iNameBackgroundColor : pData->tStyle.iValueBackgroundColor;
	pCell->iTextColor = (iColumn == 0) ? pData->tStyle.iNameTextColor : pData->tStyle.iValueTextColor;
	if ( (pProp->iFlags & (XUI_PROPERTY_FLAG_READONLY | XUI_PROPERTY_FLAG_DISABLED)) != 0 ) {
		pCell->iTextColor = pData->tStyle.iReadonlyTextColor;
	}
	pCell->iGridColor = pData->tStyle.iGridColor;
	if ( iColumn == 0 ) {
		pCell->onRender = __xuiPropertyGridNameRenderer;
		pCell->pRenderUser = pData;
	} else if ( (pProp->onRender != NULL) || (pData->onRender != NULL) || (pProp->iType == XUI_TABLE_CELL_TYPE_ENUM && pProp->bHasEditorConfig != 0) ) {
		pCell->onRender = __xuiPropertyGridValueRenderer;
		pCell->pRenderUser = pData;
	}
	return XUI_OK;
}

static int __xuiPropertyGridMergeProc(xui_widget pWidget, int iRow, int iColumn, int* pRowSpan, int* pColSpan, void* pUser)
{
	xui_property_grid_data_t* pData;

	(void)pWidget;
	pData = (xui_property_grid_data_t*)pUser;
	if ( (pData == NULL) || (iRow < 0) || (iRow >= pData->iVisibleCount) ) {
		return 0;
	}
	if ( (pData->arrVisibleKind[iRow] == XUI_PROPERTY_GRID_VISIBLE_CATEGORY) && (iColumn == 0) ) {
		if ( pRowSpan != NULL ) *pRowSpan = 1;
		if ( pColSpan != NULL ) *pColSpan = 2;
		return 1;
	}
	return 0;
}

static void __xuiPropertyGridSetProc(xui_widget pWidget, int iRow, int iColumn, const char* sValue, int iType, void* pUser)
{
	xui_property_grid_data_t* pData;
	int iProperty;

	(void)pWidget;
	(void)iType;
	pData = (xui_property_grid_data_t*)pUser;
	iProperty = __xuiPropertyGridVisibleRowProperty(pData, iRow);
	if ( (iColumn != 1) || !__xuiPropertyGridValidProperty(pData, iProperty) ) {
		return;
	}
	(void)xuiPropertyGridSetValue(pData->pWidget, iProperty, sValue);
}

static int __xuiPropertyGridValidateProc(xui_widget pWidget, int iRow, int iColumn, const char* sValue, int iType, void* pUser)
{
	xui_property_grid_data_t* pData;
	xui_property_grid_property_t* pProp;
	int iProperty;

	(void)pWidget;
	pData = (xui_property_grid_data_t*)pUser;
	iProperty = __xuiPropertyGridVisibleRowProperty(pData, iRow);
	if ( (iColumn != 1) || !__xuiPropertyGridValidProperty(pData, iProperty) ) {
		return 0;
	}
	pProp = &pData->arrProperties[iProperty];
	if ( pData->onValidate == NULL ) {
		return 1;
	}
	return pData->onValidate(pData->pWidget, iProperty, pProp->sId, sValue, iType, pData->pValidateUser);
}

static void __xuiPropertyGridChangeProc(xui_widget pWidget, int iRow, int iColumn, const char* sValue, int iType, void* pUser)
{
	xui_property_grid_data_t* pData;
	xui_property_grid_property_t* pProp;
	int iProperty;

	(void)pWidget;
	pData = (xui_property_grid_data_t*)pUser;
	iProperty = __xuiPropertyGridVisibleRowProperty(pData, iRow);
	if ( (iColumn != 1) || !__xuiPropertyGridValidProperty(pData, iProperty) ) {
		return;
	}
	pProp = &pData->arrProperties[iProperty];
	if ( pData->onChange != NULL ) {
		pData->onChange(pData->pWidget, iProperty, pProp->sId, sValue, iType, pData->pChangeUser);
	}
	if ( (pData->iDescriptionMode == XUI_PROPERTY_GRID_DESCRIPTION_PANEL) ||
	     (pData->iDescriptionMode == XUI_PROPERTY_GRID_DESCRIPTION_BOTH) ) {
		(void)xuiWidgetInvalidate(pData->pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
}

static int __xuiPropertyGridEditorProc(xui_widget pWidget, int iRow, int iColumn, const xui_table_view_cell_t* pCell, xui_rect_t tWorldRect, void* pUser)
{
	xui_property_grid_data_t* pData;
	xui_property_grid_property_t* pProp;
	xui_property_grid_action_proc onAction;
	void* pActionUser;
	int iProperty;

	(void)pWidget;
	(void)pCell;
	pData = (xui_property_grid_data_t*)pUser;
	iProperty = __xuiPropertyGridVisibleRowProperty(pData, iRow);
	if ( (iColumn != 1) || !__xuiPropertyGridValidProperty(pData, iProperty) ) {
		return 0;
	}
	pProp = &pData->arrProperties[iProperty];
	onAction = (pProp->onAction != NULL) ? pProp->onAction : pData->onAction;
	pActionUser = (pProp->onAction != NULL) ? pProp->pActionUser : pData->pActionUser;
	if ( onAction == NULL ) {
		return 0;
	}
	return onAction(pData->pWidget, iProperty, pProp->sId, tWorldRect, pActionUser);
}

static int __xuiPropertyGridEditorConfigProc(xui_widget pWidget, int iRow, int iColumn, int iType, xui_table_grid_editor_config_t* pConfig, void* pUser)
{
	xui_property_grid_data_t* pData;
	xui_property_grid_property_t* pProp;
	int iProperty;
	int i;

	(void)pWidget;
	(void)iType;
	pData = (xui_property_grid_data_t*)pUser;
	iProperty = __xuiPropertyGridVisibleRowProperty(pData, iRow);
	if ( (iColumn != 1) || !__xuiPropertyGridValidProperty(pData, iProperty) || (pConfig == NULL) ) {
		return 0;
	}
	pProp = &pData->arrProperties[iProperty];
	if ( pProp->bHasEditorConfig == 0 ) {
		return 0;
	}
	*pConfig = pProp->tEditorConfig;
	if ( pProp->iType == XUI_TABLE_CELL_TYPE_ENUM ) {
		if ( pConfig->bEnumUseValue ) {
			pConfig->iEnumSelectedValue = atoi(pProp->sValue);
		} else if ( pConfig->iEnumSelected < 0 && pConfig->arrEnumItemData != NULL ) {
			for ( i = 0; i < pConfig->iEnumItemCount; i++ ) {
				if ( pConfig->arrEnumItemData[i].sText != NULL && strcmp(pConfig->arrEnumItemData[i].sText, pProp->sValue) == 0 ) {
					pConfig->iEnumSelected = i;
					break;
				}
			}
		} else if ( pConfig->iEnumSelected < 0 && pConfig->arrEnumItems != NULL ) {
			for ( i = 0; i < pConfig->iEnumItemCount; i++ ) {
				if ( pConfig->arrEnumItems[i] != NULL && strcmp(pConfig->arrEnumItems[i], pProp->sValue) == 0 ) {
					pConfig->iEnumSelected = i;
					break;
				}
			}
		}
	}
	return 1;
}

static void __xuiPropertyGridNotifySelect(xui_property_grid_data_t* pData, int iProperty)
{
	xui_property_grid_property_t* pProp;

	if ( !__xuiPropertyGridValidProperty(pData, iProperty) ) {
		return;
	}
	pData->iSelectedProperty = iProperty;
	pData->iSelectCount++;
	pProp = &pData->arrProperties[iProperty];
	if ( pData->onSelect != NULL ) {
		pData->onSelect(pData->pWidget, iProperty, pProp->sId, pData->pSelectUser);
	}
	if ( (pData->iDescriptionMode == XUI_PROPERTY_GRID_DESCRIPTION_PANEL) ||
	     (pData->iDescriptionMode == XUI_PROPERTY_GRID_DESCRIPTION_BOTH) ) {
		(void)xuiWidgetInvalidate(pData->pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
}

static void __xuiPropertyGridSelectProc(xui_widget pWidget, int iRow, int iColumn, int iSelectionMode, void* pUser)
{
	xui_property_grid_data_t* pData;
	int iProperty;

	(void)pWidget;
	(void)iColumn;
	(void)iSelectionMode;
	pData = (xui_property_grid_data_t*)pUser;
	iProperty = __xuiPropertyGridVisibleRowProperty(pData, iRow);
	__xuiPropertyGridNotifySelect(pData, iProperty);
}

static int __xuiPropertyGridHitCell(xui_property_grid_data_t* pData, const xui_event_t* pEvent, int* pRow, int* pColumn)
{
	xui_rect_t tTableWorld;
	float fLocalX;
	float fLocalY;

	if ( (pData == NULL) || (pData->pTableView == NULL) || (pEvent == NULL) ) {
		return 0;
	}
	tTableWorld = xuiWidgetGetWorldRect(pData->pTableView);
	fLocalX = pEvent->fX - tTableWorld.fX;
	fLocalY = pEvent->fY - tTableWorld.fY;
	return xuiTableViewGetItemAt(pData->pTableView, fLocalX, fLocalY, pRow, pColumn) == XUI_OK;
}

static int __xuiPropertyGridEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_property_grid_data_t* pData;
	int iRow;
	int iColumn;
	int iCategory;
	int iProperty;

	(void)pUser;
	pData = __xuiPropertyGridGetData(pWidget);
	if ( (pData == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pEvent->iPhase != XUI_EVENT_PHASE_CAPTURE ) {
		return XUI_OK;
	}
	if ( (pEvent->iType == XUI_EVENT_POINTER_DOWN) && (pEvent->iButton == XUI_POINTER_BUTTON_LEFT) ) {
		iRow = -1;
		iColumn = -1;
		if ( __xuiPropertyGridHitCell(pData, pEvent, &iRow, &iColumn) ) {
			iCategory = __xuiPropertyGridVisibleRowCategory(pData, iRow);
			if ( __xuiPropertyGridValidCategory(pData, iCategory) ) {
				(void)xuiPropertyGridSetCategoryExpanded(pWidget, iCategory, !pData->arrCategories[iCategory].bExpanded);
				return XUI_EVENT_DISPATCH_STOP;
			}
			if ( iColumn == 0 ) {
				iProperty = __xuiPropertyGridVisibleRowProperty(pData, iRow);
				if ( __xuiPropertyGridValidProperty(pData, iProperty) ) {
					(void)xuiPropertyGridSetSelected(pWidget, iProperty);
					__xuiPropertyGridNotifySelect(pData, iProperty);
					return XUI_EVENT_DISPATCH_STOP;
				}
			}
		}
	}
	if ( (pEvent->iType == XUI_EVENT_POINTER_UP) && (pEvent->iButton == XUI_POINTER_BUTTON_LEFT) ) {
		iRow = -1;
		iColumn = -1;
		if ( __xuiPropertyGridHitCell(pData, pEvent, &iRow, &iColumn) ) {
			iCategory = __xuiPropertyGridVisibleRowCategory(pData, iRow);
			if ( __xuiPropertyGridValidCategory(pData, iCategory) ) {
				return XUI_EVENT_DISPATCH_STOP;
			}
			if ( iColumn == 0 ) {
				iProperty = __xuiPropertyGridVisibleRowProperty(pData, iRow);
				if ( __xuiPropertyGridValidProperty(pData, iProperty) ) {
					(void)xuiPropertyGridSetSelected(pWidget, iProperty);
					if ( xuiTableGridGetEditMode(pData->pTableGrid) == XUI_TABLE_GRID_EDIT_QUICK ) {
						(void)xuiPropertyGridBeginEdit(pWidget, iProperty);
						return XUI_EVENT_DISPATCH_STOP;
					}
				}
			}
		}
	}
	return XUI_OK;
}

static int __xuiPropertyGridRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_property_grid_data_t* pData;
	xui_property_grid_property_t* pProp;
	xui_proxy_t tProxy;
	xui_rect_t tRect;
	xui_rect_t tText;
	const char* sText;
	int iRet;

	(void)iStateId;
	(void)pUser;
	pData = __xuiPropertyGridGetData(pWidget);
	if ( (pData == NULL) || (pDraw == NULL) || (xuiGetProxy(xuiWidgetGetContext(pWidget), &tProxy) != XUI_OK) ) {
		return XUI_OK;
	}
	tRect = xuiWidgetGetContentRect(pWidget);
	if ( tProxy.drawRectFill != NULL ) {
		iRet = tProxy.drawRectFill(&tProxy, pDraw, xuiInternalSnapRect(tRect), pData->tStyle.iBackgroundColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( ((pData->iDescriptionMode != XUI_PROPERTY_GRID_DESCRIPTION_PANEL) && (pData->iDescriptionMode != XUI_PROPERTY_GRID_DESCRIPTION_BOTH)) ||
	     (pData->fDescriptionPanelHeight <= 0.0f) ||
	     (pData->tDescriptionRect.fW <= 0.0f) ||
	     (pData->tDescriptionRect.fH <= 0.0f) ) {
		return XUI_OK;
	}
	tRect = pData->tDescriptionRect;
	if ( tProxy.drawRectFill != NULL ) {
		iRet = tProxy.drawRectFill(&tProxy, pDraw, xuiInternalSnapRect(tRect), pData->tStyle.iNameBackgroundColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( tProxy.drawLine != NULL ) {
		iRet = tProxy.drawLine(&tProxy, pDraw, tRect.fX, tRect.fY + 0.5f, tRect.fX + tRect.fW, tRect.fY + 0.5f, 1.0f, pData->tStyle.iGridColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( !__xuiPropertyGridValidProperty(pData, pData->iSelectedProperty) || (pData->pFont == NULL) || (tProxy.drawText == NULL) ) {
		return XUI_OK;
	}
	pProp = &pData->arrProperties[pData->iSelectedProperty];
	sText = (pProp->sDescription[0] != '\0') ? pProp->sDescription : pProp->sName;
	tText = (xui_rect_t){tRect.fX + 8.0f, tRect.fY + 4.0f, __xuiPropertyGridMax(1.0f, tRect.fW - 16.0f), __xuiPropertyGridMax(1.0f, tRect.fH - 8.0f)};
	return tProxy.drawText(&tProxy, pDraw, pData->pFont, sText, xuiInternalSnapRect(tText), pData->tStyle.iNameTextColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP | XUI_TEXT_CLIP);
}

static int __xuiPropertyGridArrange(xui_widget pWidget, xui_rect_t tContentRect, void* pUser)
{
	xui_property_grid_data_t* pData;
	xui_rect_t tTable;
	float fPanelHeight;

	(void)pUser;
	pData = __xuiPropertyGridGetData(pWidget);
	if ( (pData == NULL) || (pData->pTableGrid == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	tTable = tContentRect;
	pData->tDescriptionRect = (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	if ( ((pData->iDescriptionMode == XUI_PROPERTY_GRID_DESCRIPTION_PANEL) || (pData->iDescriptionMode == XUI_PROPERTY_GRID_DESCRIPTION_BOTH)) &&
	     (pData->fDescriptionPanelHeight > 0.0f) &&
	     (tContentRect.fH > pData->fDescriptionPanelHeight + 20.0f) ) {
		fPanelHeight = __xuiPropertyGridMin(pData->fDescriptionPanelHeight, tContentRect.fH - 20.0f);
		tTable.fH = __xuiPropertyGridMax(1.0f, tContentRect.fH - fPanelHeight);
		pData->tDescriptionRect = (xui_rect_t){tContentRect.fX, tContentRect.fY + tTable.fH, tContentRect.fW, fPanelHeight};
	}
	(void)xuiWidgetArrange(pData->pTableGrid, tTable);
	__xuiPropertyGridSyncColumns(pData);
	return XUI_OK;
}

static int __xuiPropertyGridUpdate(xui_widget pWidget, float fDelta, void* pUser)
{
	xui_property_grid_data_t* pData;

	(void)fDelta;
	(void)pUser;
	pData = __xuiPropertyGridGetData(pWidget);
	if ( pData != NULL ) {
		__xuiPropertyGridSyncColumns(pData);
	}
	return XUI_OK;
}

static void __xuiPropertyGridDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_MANUAL;
	pLayout->iWidthMode = XUI_SIZE_CONTENT;
	pLayout->iHeightMode = XUI_SIZE_CONTENT;
	pLayout->iFlowMode = XUI_FLOW_BLOCK;
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fShrink = 1.0f;
	pLayout->iTableRowSpan = 1;
	pLayout->iTableColumnSpan = 1;
	pLayout->iGridColumnCount = 1;
}

static int __xuiPropertyGridContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	(void)pWidget;
	(void)tConstraint;
	(void)pUser;
	if ( pSize == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pSize->fX = 360.0f;
	pSize->fY = 240.0f;
	return XUI_OK;
}

static int __xuiPropertyGridInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiPropertyGridEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiPropertyGridEvent, NULL);
	return iRet;
}

static int __xuiPropertyGridInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_property_grid_data_t* pData;
	const xui_property_grid_desc_t* pDesc;
	xui_table_grid_desc_t tGridDesc;
	xui_widget pFrame;
	int iRet;

	(void)pUser;
	pData = (xui_property_grid_data_t*)pTypeData;
	pDesc = (const xui_property_grid_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiPropertyGridDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(pData, 0, sizeof(*pData));
	pData->pWidget = pWidget;
	pData->iSelectedProperty = -1;
	__xuiPropertyGridApplyDesc(pData, pDesc);
	memset(&tGridDesc, 0, sizeof(tGridDesc));
	tGridDesc.iSize = sizeof(tGridDesc);
	tGridDesc.arrColumns = pData->arrColumns;
	tGridDesc.iColumnCount = 2;
	tGridDesc.arrRows = pData->arrRows;
	tGridDesc.iRowCount = 0;
	tGridDesc.onCount = __xuiPropertyGridCountProc;
	tGridDesc.onCell = __xuiPropertyGridCellProc;
	tGridDesc.onSet = __xuiPropertyGridSetProc;
	tGridDesc.pAdapterUser = pData;
	tGridDesc.onValidate = __xuiPropertyGridValidateProc;
	tGridDesc.pValidateUser = pData;
	tGridDesc.onChange = __xuiPropertyGridChangeProc;
	tGridDesc.pChangeUser = pData;
	tGridDesc.onEditor = __xuiPropertyGridEditorProc;
	tGridDesc.pEditorUser = pData;
	tGridDesc.onEditorConfig = __xuiPropertyGridEditorConfigProc;
	tGridDesc.pEditorConfigUser = pData;
	tGridDesc.pFont = pData->pFont;
	tGridDesc.fDefaultColumnWidth = 120.0f;
	tGridDesc.fDefaultRowHeight = pData->fRowHeight;
	tGridDesc.fHeaderHeight = 0.0f;
	tGridDesc.iSelectionMode = XUI_TABLE_VIEW_SELECTION_CELL;
	tGridDesc.iEditMode = (pDesc != NULL && __xuiPropertyGridEditModeValid(pDesc->iEditMode)) ? pDesc->iEditMode : XUI_TABLE_GRID_EDIT_QUICK;
	tGridDesc.iScrollbarMode = XUI_SCROLLBAR_MODE_COMPACT;
	iRet = xuiTableGridCreate(xuiWidgetGetContext(pWidget), &pData->pTableGrid, &tGridDesc);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiWidgetSetRect(pData->pTableGrid, (xui_rect_t){0.0f, 0.0f, 360.0f, 240.0f});
	iRet = xuiWidgetAddChild(pWidget, pData->pTableGrid);
	if ( iRet != XUI_OK ) return iRet;
	pData->pTableView = xuiTableGridGetTableView(pData->pTableGrid);
	if ( pData->pTableView == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	(void)xuiTableViewSetSelect(pData->pTableView, __xuiPropertyGridSelectProc, pData);
	(void)xuiTableViewSetMergeProvider(pData->pTableView, __xuiPropertyGridMergeProc, pData);
	pFrame = xuiTableViewGetFrameWidget(pData->pTableView);
	if ( pFrame != NULL ) {
		(void)xuiScrollFrameSetScrollbarPolicy(pFrame, XUI_SCROLLBAR_POLICY_HIDDEN, XUI_SCROLLBAR_POLICY_AUTO);
	}
	(void)xuiTableGridSetSelectionMode(pData->pTableGrid, XUI_TABLE_VIEW_SELECTION_CELL);
	(void)xuiTableGridSetDefaultMetrics(pData->pTableGrid, 120.0f, pData->fRowHeight, 0.0f);
	(void)xuiWidgetSetLayoutType(pWidget, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetFlowMode(pWidget, XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_VISIBLE);
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	(void)xuiWidgetSetPadding(pWidget, (xui_thickness_t){0.0f, 0.0f, 0.0f, 0.0f});
	__xuiPropertyGridApplyStyle(pData);
	__xuiPropertyGridSyncColumns(pData);
	return __xuiPropertyGridInitEvents(pWidget);
}

static void __xuiPropertyGridDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_property_grid_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_property_grid_data_t*)pTypeData;
	if ( pData != NULL ) {
		memset(pData, 0, sizeof(*pData));
	}
}

static xui_property_grid_data_t* __xuiPropertyGridGetData(xui_widget pWidget)
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
	pType = xuiWidgetFindType(pContext, "propertygrid");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_property_grid_data_t*)xuiWidgetGetTypeData(pWidget);
}

XUI_API xui_widget_type xuiPropertyGridGetType(xui_context pContext)
{
	xui_widget_type pType;
	xui_widget_type_desc_t tDesc;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "propertygrid");
	if ( pType != NULL ) {
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "propertygrid";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iTypeDataSize = sizeof(xui_property_grid_data_t);
	tDesc.onInit = __xuiPropertyGridInit;
	tDesc.onDestroy = __xuiPropertyGridDestroy;
	tDesc.onContentMeasure = __xuiPropertyGridContentMeasure;
	tDesc.onLayoutArrange = __xuiPropertyGridArrange;
	tDesc.onCacheRender = __xuiPropertyGridRender;
	tDesc.onUpdate = __xuiPropertyGridUpdate;
	__xuiPropertyGridDefaultLayout(&tDesc.tLayout);
	if ( xuiWidgetRegisterType(pContext, &pType, &tDesc) != XUI_OK ) {
		return NULL;
	}
	return pType;
}

XUI_API int xuiPropertyGridCreate(xui_context pContext, xui_widget* ppWidget, const xui_property_grid_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiPropertyGridDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiPropertyGridGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API xui_widget xuiPropertyGridGetTableGrid(xui_widget pWidget)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	return (pData != NULL) ? pData->pTableGrid : NULL;
}

XUI_API xui_widget xuiPropertyGridGetTableView(xui_widget pWidget)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	return (pData != NULL) ? pData->pTableView : NULL;
}

XUI_API int xuiPropertyGridClear(xui_widget pWidget)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->pTableGrid != NULL ) {
		(void)xuiTableGridEndEdit(pData->pTableGrid, 1);
	}
	memset(pData->arrCategories, 0, sizeof(pData->arrCategories));
	memset(pData->arrProperties, 0, sizeof(pData->arrProperties));
	memset(pData->arrVisibleKind, 0, sizeof(pData->arrVisibleKind));
	memset(pData->arrVisibleIndex, 0, sizeof(pData->arrVisibleIndex));
	pData->iCategoryCount = 0;
	pData->iPropertyCount = 0;
	pData->iSelectedProperty = -1;
	__xuiPropertyGridRebuildVisible(pData);
	return XUI_OK;
}

XUI_API int xuiPropertyGridAddCategory(xui_widget pWidget, const char* sId, const char* sName, int bExpanded)
{
	xui_property_grid_data_t* pData;
	xui_property_grid_category_t* pCategory;
	int iIndex;

	pData = __xuiPropertyGridGetData(pWidget);
	if ( (pData == NULL) || (pData->iCategoryCount >= XUI_PROPERTY_GRID_CATEGORY_CAPACITY) ) {
		return -1;
	}
	if ( (sId != NULL) && (sId[0] != '\0') && (xuiPropertyGridFindCategory(pWidget, sId) >= 0) ) {
		return -1;
	}
	iIndex = pData->iCategoryCount++;
	pCategory = &pData->arrCategories[iIndex];
	memset(pCategory, 0, sizeof(*pCategory));
	__xuiPropertyGridCopy(pCategory->sId, sizeof(pCategory->sId), ((sId != NULL) && (sId[0] != '\0')) ? sId : sName);
	__xuiPropertyGridCopy(pCategory->sName, sizeof(pCategory->sName), ((sName != NULL) && (sName[0] != '\0')) ? sName : pCategory->sId);
	pCategory->bExpanded = bExpanded != 0;
	__xuiPropertyGridRebuildVisible(pData);
	return iIndex;
}

XUI_API int xuiPropertyGridAddProperty(xui_widget pWidget, int iCategory, const xui_property_desc_t* pDesc)
{
	xui_property_grid_data_t* pData;
	xui_property_grid_property_t* pProp;
	int iIndex;

	pData = __xuiPropertyGridGetData(pWidget);
	if ( (pData == NULL) || (pDesc == NULL) || !__xuiPropertyGridValidCategory(pData, iCategory) ||
	     (pData->iPropertyCount >= XUI_PROPERTY_GRID_PROPERTY_CAPACITY) ) {
		return -1;
	}
	if ( (pDesc->sId == NULL) || (pDesc->sId[0] == '\0') || (xuiPropertyGridFindProperty(pWidget, pDesc->sId) >= 0) ) {
		return -1;
	}
	iIndex = pData->iPropertyCount++;
	pProp = &pData->arrProperties[iIndex];
	memset(pProp, 0, sizeof(*pProp));
	__xuiPropertyGridCopy(pProp->sId, sizeof(pProp->sId), pDesc->sId);
	__xuiPropertyGridCopy(pProp->sName, sizeof(pProp->sName), ((pDesc->sName != NULL) && (pDesc->sName[0] != '\0')) ? pDesc->sName : pDesc->sId);
	__xuiPropertyGridCopy(pProp->sDescription, sizeof(pProp->sDescription), pDesc->sDescription);
	__xuiPropertyGridCopy(pProp->sValue, sizeof(pProp->sValue), pDesc->sValue);
	__xuiPropertyGridCopy(pProp->sDefaultValue, sizeof(pProp->sDefaultValue), pDesc->sDefaultValue);
	pProp->iCategory = iCategory;
	pProp->iType = pDesc->iType;
	if ( (pProp->iType < XUI_TABLE_CELL_TYPE_TEXT) || (pProp->iType > XUI_TABLE_CELL_TYPE_IMAGE) ) {
		pProp->iType = XUI_TABLE_CELL_TYPE_TEXT;
	}
	pProp->iFlags = pDesc->iFlags;
	pProp->bAutoDirty = (pProp->sDefaultValue[0] != '\0');
	pData->arrCategories[iCategory].iPropertyCount++;
	__xuiPropertyGridRebuildVisible(pData);
	return iIndex;
}

XUI_API int xuiPropertyGridFindCategory(xui_widget pWidget, const char* sId)
{
	xui_property_grid_data_t* pData;
	int i;

	pData = __xuiPropertyGridGetData(pWidget);
	if ( (pData == NULL) || (sId == NULL) || (sId[0] == '\0') ) {
		return -1;
	}
	for ( i = 0; i < pData->iCategoryCount; i++ ) {
		if ( strcmp(pData->arrCategories[i].sId, sId) == 0 ) {
			return i;
		}
	}
	return -1;
}

XUI_API int xuiPropertyGridFindProperty(xui_widget pWidget, const char* sId)
{
	xui_property_grid_data_t* pData;
	int i;

	pData = __xuiPropertyGridGetData(pWidget);
	if ( (pData == NULL) || (sId == NULL) || (sId[0] == '\0') ) {
		return -1;
	}
	for ( i = 0; i < pData->iPropertyCount; i++ ) {
		if ( strcmp(pData->arrProperties[i].sId, sId) == 0 ) {
			return i;
		}
	}
	return -1;
}

XUI_API int xuiPropertyGridSetCategoryExpanded(xui_widget pWidget, int iCategory, int bExpanded)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	if ( !__xuiPropertyGridValidCategory(pData, iCategory) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->arrCategories[iCategory].bExpanded != (bExpanded != 0) ) {
		pData->arrCategories[iCategory].bExpanded = bExpanded != 0;
		pData->iToggleCount++;
	}
	if ( pData->pTableGrid != NULL ) {
		(void)xuiTableGridEndEdit(pData->pTableGrid, 1);
	}
	__xuiPropertyGridRebuildVisible(pData);
	return XUI_OK;
}

XUI_API int xuiPropertyGridGetCategoryExpanded(xui_widget pWidget, int iCategory)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	return __xuiPropertyGridValidCategory(pData, iCategory) ? pData->arrCategories[iCategory].bExpanded : 0;
}

XUI_API int xuiPropertyGridSetSelected(xui_widget pWidget, int iProperty)
{
	xui_property_grid_data_t* pData;
	int iRow;

	pData = __xuiPropertyGridGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !__xuiPropertyGridValidProperty(pData, iProperty) ) {
		iProperty = -1;
	}
	pData->iSelectedProperty = iProperty;
	for ( iRow = 0; iRow < pData->iVisibleCount; iRow++ ) {
		pData->arrRows[iRow].bSelected = (__xuiPropertyGridVisibleRowProperty(pData, iRow) == iProperty);
	}
	iRow = __xuiPropertyGridVisiblePropertyRow(pData, iProperty);
	if ( (iRow >= 0) && (pData->pTableView != NULL) ) {
		(void)xuiTableViewSetSelectedCell(pData->pTableView, iRow, 1);
	} else if ( pData->pTableView != NULL ) {
		(void)xuiTableViewSetSelectedCell(pData->pTableView, -1, -1);
	}
	if ( pData->pTableView != NULL ) {
		(void)xuiTableViewRefresh(pData->pTableView);
	}
	return XUI_OK;
}

XUI_API int xuiPropertyGridGetSelected(xui_widget pWidget)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	return (pData != NULL) ? pData->iSelectedProperty : -1;
}

XUI_API int xuiPropertyGridGetCategoryCount(xui_widget pWidget)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	return (pData != NULL) ? pData->iCategoryCount : 0;
}

XUI_API int xuiPropertyGridGetPropertyCount(xui_widget pWidget)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	return (pData != NULL) ? pData->iPropertyCount : 0;
}

XUI_API int xuiPropertyGridGetVisibleCount(xui_widget pWidget)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	return (pData != NULL) ? pData->iVisibleCount : 0;
}

XUI_API int xuiPropertyGridGetVisibleProperty(xui_widget pWidget, int iVisible)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	return __xuiPropertyGridVisibleRowProperty(pData, iVisible);
}

XUI_API int xuiPropertyGridSetValue(xui_widget pWidget, int iProperty, const char* sValue)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	if ( !__xuiPropertyGridValidProperty(pData, iProperty) ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiPropertyGridCopy(pData->arrProperties[iProperty].sValue, sizeof(pData->arrProperties[iProperty].sValue), sValue);
	if ( pData->pTableView != NULL ) (void)xuiTableViewRefresh(pData->pTableView);
	if ( pData->pWidget != NULL ) (void)xuiWidgetInvalidate(pData->pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return XUI_OK;
}

XUI_API const char* xuiPropertyGridGetValue(xui_widget pWidget, int iProperty)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	return __xuiPropertyGridValidProperty(pData, iProperty) ? pData->arrProperties[iProperty].sValue : NULL;
}

XUI_API int xuiPropertyGridGetBool(xui_widget pWidget, int iProperty, int bDefault)
{
	const char* sValue = xuiPropertyGridGetValue(pWidget, iProperty);
	return (sValue != NULL) ? __xuiPropertyGridBoolText(sValue) : bDefault;
}

XUI_API int xuiPropertyGridSetBool(xui_widget pWidget, int iProperty, int bValue)
{
	return xuiPropertyGridSetValue(pWidget, iProperty, bValue ? "true" : "false");
}

XUI_API int xuiPropertyGridGetInt(xui_widget pWidget, int iProperty, int iDefault)
{
	const char* sValue = xuiPropertyGridGetValue(pWidget, iProperty);
	return (sValue != NULL && sValue[0] != '\0') ? atoi(sValue) : iDefault;
}

XUI_API int xuiPropertyGridSetInt(xui_widget pWidget, int iProperty, int iValue)
{
	char sValue[32];
	snprintf(sValue, sizeof(sValue), "%d", iValue);
	return xuiPropertyGridSetValue(pWidget, iProperty, sValue);
}

XUI_API float xuiPropertyGridGetFloat(xui_widget pWidget, int iProperty, float fDefault)
{
	const char* sValue = xuiPropertyGridGetValue(pWidget, iProperty);
	return (sValue != NULL && sValue[0] != '\0') ? (float)atof(sValue) : fDefault;
}

XUI_API int xuiPropertyGridSetFloat(xui_widget pWidget, int iProperty, float fValue)
{
	char sValue[48];
	snprintf(sValue, sizeof(sValue), "%.6g", fValue);
	return xuiPropertyGridSetValue(pWidget, iProperty, sValue);
}

XUI_API uint32_t xuiPropertyGridGetColor(xui_widget pWidget, int iProperty, uint32_t iDefault)
{
	const char* sValue;
	uint32_t iColor;

	sValue = xuiPropertyGridGetValue(pWidget, iProperty);
	if ( (sValue != NULL) && __xuiPropertyGridParseColor(sValue, &iColor) ) {
		return iColor;
	}
	return iDefault;
}

XUI_API int xuiPropertyGridSetColor(xui_widget pWidget, int iProperty, uint32_t iColor)
{
	char sValue[16];
	__xuiPropertyGridFormatColor(iColor, sValue, (int)sizeof(sValue));
	return xuiPropertyGridSetValue(pWidget, iProperty, sValue);
}

XUI_API int xuiPropertyGridSetPropertyFlags(xui_widget pWidget, int iProperty, int iFlags)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	if ( !__xuiPropertyGridValidProperty(pData, iProperty) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrProperties[iProperty].iFlags = iFlags;
	__xuiPropertyGridRebuildVisible(pData);
	return XUI_OK;
}

XUI_API int xuiPropertyGridGetPropertyFlags(xui_widget pWidget, int iProperty)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	return __xuiPropertyGridValidProperty(pData, iProperty) ? pData->arrProperties[iProperty].iFlags : 0;
}

XUI_API int xuiPropertyGridSetEditorConfig(xui_widget pWidget, int iProperty, const xui_table_grid_editor_config_t* pConfig)
{
	xui_property_grid_data_t* pData;
	xui_property_grid_property_t* pProp;
	int i;
	int iCount;

	pData = __xuiPropertyGridGetData(pWidget);
	if ( !__xuiPropertyGridValidProperty(pData, iProperty) ) return XUI_ERROR_INVALID_ARGUMENT;
	pProp = &pData->arrProperties[iProperty];
	if ( pConfig == NULL ) {
		memset(&pProp->tEditorConfig, 0, sizeof(pProp->tEditorConfig));
		memset(pProp->arrEnumItems, 0, sizeof(pProp->arrEnumItems));
		memset(pProp->arrEnumItemData, 0, sizeof(pProp->arrEnumItemData));
		memset(pProp->arrEnumEnabled, 0, sizeof(pProp->arrEnumEnabled));
		pProp->bHasEditorConfig = 0;
		return XUI_OK;
	}
	memset(pProp->arrEnumItems, 0, sizeof(pProp->arrEnumItems));
	memset(pProp->arrEnumItemData, 0, sizeof(pProp->arrEnumItemData));
	memset(pProp->arrEnumEnabled, 0, sizeof(pProp->arrEnumEnabled));
	pProp->tEditorConfig = *pConfig;
	if ( (pConfig->arrEnumItemData != NULL) && (pConfig->iEnumItemCount > 0) ) {
		iCount = pConfig->iEnumItemCount;
		if ( iCount > XUI_PROPERTY_GRID_OPTION_CAPACITY ) iCount = XUI_PROPERTY_GRID_OPTION_CAPACITY;
		for ( i = 0; i < iCount; i++ ) {
			pProp->arrEnumItemData[i] = pConfig->arrEnumItemData[i];
			if ( (pProp->arrEnumItemData[i].sText == NULL) && (pConfig->arrEnumItems != NULL) ) {
				pProp->arrEnumItemData[i].sText = pConfig->arrEnumItems[i];
			}
			if ( pConfig->arrEnumEnabled != NULL ) {
				pProp->arrEnumItemData[i].bEnabled = pConfig->arrEnumEnabled[i];
			}
			pProp->arrEnumItems[i] = pProp->arrEnumItemData[i].sText;
			pProp->arrEnumEnabled[i] = pProp->arrEnumItemData[i].bEnabled;
		}
		pProp->tEditorConfig.arrEnumItemData = pProp->arrEnumItemData;
		pProp->tEditorConfig.arrEnumItems = pProp->arrEnumItems;
		pProp->tEditorConfig.arrEnumEnabled = pProp->arrEnumEnabled;
		pProp->tEditorConfig.iEnumItemCount = iCount;
	} else if ( (pConfig->arrEnumItems != NULL) && (pConfig->iEnumItemCount > 0) ) {
		iCount = pConfig->iEnumItemCount;
		if ( iCount > XUI_PROPERTY_GRID_OPTION_CAPACITY ) iCount = XUI_PROPERTY_GRID_OPTION_CAPACITY;
		for ( i = 0; i < iCount; i++ ) {
			pProp->arrEnumItems[i] = pConfig->arrEnumItems[i];
			pProp->arrEnumEnabled[i] = (pConfig->arrEnumEnabled != NULL) ? pConfig->arrEnumEnabled[i] : 1;
		}
		pProp->tEditorConfig.arrEnumItems = pProp->arrEnumItems;
		pProp->tEditorConfig.arrEnumEnabled = (pConfig->arrEnumEnabled != NULL) ? pProp->arrEnumEnabled : NULL;
		pProp->tEditorConfig.iEnumItemCount = iCount;
	}
	if ( (pConfig->arrEnumEnabled != NULL) && (pProp->tEditorConfig.iEnumItemCount > 0) ) {
		iCount = pProp->tEditorConfig.iEnumItemCount;
		if ( iCount > XUI_PROPERTY_GRID_OPTION_CAPACITY ) iCount = XUI_PROPERTY_GRID_OPTION_CAPACITY;
		for ( i = 0; i < iCount; i++ ) {
			pProp->arrEnumEnabled[i] = pConfig->arrEnumEnabled[i];
			if ( pProp->tEditorConfig.arrEnumItemData == pProp->arrEnumItemData ) pProp->arrEnumItemData[i].bEnabled = pConfig->arrEnumEnabled[i];
		}
		pProp->tEditorConfig.arrEnumEnabled = pProp->arrEnumEnabled;
	}
	pProp->bHasEditorConfig = 1;
	return XUI_OK;
}

XUI_API int xuiPropertyGridSetRenderer(xui_widget pWidget, int iProperty, xui_property_grid_render_proc onRender, void* pUser)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	if ( !__xuiPropertyGridValidProperty(pData, iProperty) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrProperties[iProperty].onRender = onRender;
	pData->arrProperties[iProperty].pRenderUser = pUser;
	if ( pData->pTableView != NULL ) (void)xuiTableViewRefresh(pData->pTableView);
	return XUI_OK;
}

XUI_API int xuiPropertyGridSetAction(xui_widget pWidget, int iProperty, xui_property_grid_action_proc onAction, void* pUser)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	if ( !__xuiPropertyGridValidProperty(pData, iProperty) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrProperties[iProperty].onAction = onAction;
	pData->arrProperties[iProperty].pActionUser = pUser;
	return XUI_OK;
}

XUI_API int xuiPropertyGridIsEditing(xui_widget pWidget)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	return (pData != NULL && pData->pTableGrid != NULL) ? xuiTableGridIsEditing(pData->pTableGrid) : 0;
}

XUI_API int xuiPropertyGridBeginEdit(xui_widget pWidget, int iProperty)
{
	xui_property_grid_data_t* pData;
	int iRow;

	pData = __xuiPropertyGridGetData(pWidget);
	if ( !__xuiPropertyGridValidProperty(pData, iProperty) ) {
		return 0;
	}
	iRow = __xuiPropertyGridVisiblePropertyRow(pData, iProperty);
	return (iRow >= 0 && pData->pTableGrid != NULL) ? xuiTableGridBeginEdit(pData->pTableGrid, iRow, 1) : 0;
}

XUI_API int xuiPropertyGridEndEdit(xui_widget pWidget, int bCommit)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	return (pData != NULL && pData->pTableGrid != NULL) ? xuiTableGridEndEdit(pData->pTableGrid, bCommit) : 1;
}

XUI_API int xuiPropertyGridSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pFont = pFont;
	if ( pData->pTableGrid != NULL ) (void)xuiTableGridSetFont(pData->pTableGrid, pFont);
	if ( pData->pWidget != NULL ) (void)xuiWidgetInvalidate(pData->pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return XUI_OK;
}

XUI_API xui_font xuiPropertyGridGetFont(xui_widget pWidget)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API int xuiPropertyGridSetMetrics(xui_widget pWidget, float fNameWidth, float fRowHeight, float fCategoryHeight)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( fNameWidth > 20.0f ) pData->fNameWidth = fNameWidth;
	if ( fRowHeight > 8.0f ) pData->fRowHeight = fRowHeight;
	if ( fCategoryHeight > 8.0f ) pData->fCategoryHeight = fCategoryHeight;
	if ( pData->pTableGrid != NULL ) (void)xuiTableGridSetDefaultMetrics(pData->pTableGrid, 120.0f, pData->fRowHeight, 0.0f);
	__xuiPropertyGridSyncColumns(pData);
	__xuiPropertyGridRebuildVisible(pData);
	return XUI_OK;
}

XUI_API int xuiPropertyGridGetMetrics(xui_widget pWidget, float* pNameWidth, float* pRowHeight, float* pCategoryHeight)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pNameWidth != NULL ) *pNameWidth = pData->fNameWidth;
	if ( pRowHeight != NULL ) *pRowHeight = pData->fRowHeight;
	if ( pCategoryHeight != NULL ) *pCategoryHeight = pData->fCategoryHeight;
	return XUI_OK;
}

XUI_API int xuiPropertyGridSetDescriptionMode(xui_widget pWidget, int iMode, float fPanelHeight)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	if ( (pData == NULL) || !__xuiPropertyGridDescriptionModeValid(iMode) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iDescriptionMode = iMode;
	pData->fDescriptionPanelHeight = (fPanelHeight > 0.0f) ? fPanelHeight : 0.0f;
	if ( pData->pTableView != NULL ) (void)xuiTableViewRefresh(pData->pTableView);
	if ( pData->pWidget != NULL ) (void)xuiWidgetInvalidate(pData->pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return XUI_OK;
}

XUI_API int xuiPropertyGridGetDescriptionMode(xui_widget pWidget)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	return (pData != NULL) ? pData->iDescriptionMode : XUI_PROPERTY_GRID_DESCRIPTION_NONE;
}

XUI_API int xuiPropertyGridSetEditMode(xui_widget pWidget, int iMode)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	if ( (pData == NULL) || (pData->pTableGrid == NULL) || !__xuiPropertyGridEditModeValid(iMode) ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiTableGridSetEditMode(pData->pTableGrid, iMode);
}

XUI_API int xuiPropertyGridGetEditMode(xui_widget pWidget)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	return (pData != NULL && pData->pTableGrid != NULL) ? xuiTableGridGetEditMode(pData->pTableGrid) : XUI_TABLE_GRID_EDIT_DISPLAY;
}

XUI_API int xuiPropertyGridSetScroll(xui_widget pWidget, float fScrollY)
{
	xui_property_grid_data_t* pData;
	float fX;

	pData = __xuiPropertyGridGetData(pWidget);
	if ( (pData == NULL) || (pData->pTableView == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	fX = 0.0f;
	(void)xuiTableViewGetOffset(pData->pTableView, &fX, NULL);
	return xuiTableViewSetOffset(pData->pTableView, fX, fScrollY);
}

XUI_API float xuiPropertyGridGetScroll(xui_widget pWidget)
{
	xui_property_grid_data_t* pData;
	float fY;

	pData = __xuiPropertyGridGetData(pWidget);
	fY = 0.0f;
	if ( (pData != NULL) && (pData->pTableView != NULL) ) {
		(void)xuiTableViewGetOffset(pData->pTableView, NULL, &fY);
	}
	return fY;
}

XUI_API int xuiPropertyGridSetScrollbarMode(xui_widget pWidget, int iMode)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	if ( (pData == NULL) || (pData->pTableGrid == NULL) || !__xuiPropertyGridScrollbarModeValid(iMode) ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiTableGridSetScrollbarMode(pData->pTableGrid, iMode);
}

XUI_API int xuiPropertyGridGetScrollbarMode(xui_widget pWidget)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	return (pData != NULL && pData->pTableView != NULL) ? xuiTableViewGetScrollbarMode(pData->pTableView) : XUI_SCROLLBAR_MODE_COMPACT;
}

XUI_API int xuiPropertyGridSetSelect(xui_widget pWidget, xui_property_grid_select_proc onSelect, void* pUser)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onSelect = onSelect;
	pData->pSelectUser = pUser;
	return XUI_OK;
}

XUI_API int xuiPropertyGridSetValidate(xui_widget pWidget, xui_property_grid_validate_proc onValidate, void* pUser)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onValidate = onValidate;
	pData->pValidateUser = pUser;
	return XUI_OK;
}

XUI_API int xuiPropertyGridSetChange(xui_widget pWidget, xui_property_grid_change_proc onChange, void* pUser)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onChange = onChange;
	pData->pChangeUser = pUser;
	return XUI_OK;
}

XUI_API int xuiPropertyGridSetGlobalAction(xui_widget pWidget, xui_property_grid_action_proc onAction, void* pUser)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onAction = onAction;
	pData->pActionUser = pUser;
	return XUI_OK;
}

XUI_API int xuiPropertyGridSetGlobalRenderer(xui_widget pWidget, xui_property_grid_render_proc onRender, void* pUser)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onRender = onRender;
	pData->pRenderUser = pUser;
	if ( pData->pTableView != NULL ) (void)xuiTableViewRefresh(pData->pTableView);
	return XUI_OK;
}

XUI_API int xuiPropertyGridSetStyle(xui_widget pWidget, const xui_property_grid_style_t* pStyle)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	if ( (pData == NULL) || (pStyle == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->tStyle = *pStyle;
	__xuiPropertyGridApplyStyle(pData);
	__xuiPropertyGridRebuildVisible(pData);
	return XUI_OK;
}

XUI_API int xuiPropertyGridGetStyle(xui_widget pWidget, xui_property_grid_style_t* pStyle)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	if ( (pData == NULL) || (pStyle == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pStyle = pData->tStyle;
	return XUI_OK;
}

XUI_API int xuiPropertyGridSetColors(xui_widget pWidget, uint32_t iBackground, uint32_t iCategory, uint32_t iRow, uint32_t iSelected, uint32_t iGrid, uint32_t iText)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->tStyle.iBackgroundColor = iBackground;
	pData->tStyle.iCategoryBackgroundColor = iCategory;
	pData->tStyle.iNameBackgroundColor = iRow;
	pData->tStyle.iValueBackgroundColor = iRow;
	pData->tStyle.iSelectedColor = iSelected;
	pData->tStyle.iGridColor = iGrid;
	pData->tStyle.iValueTextColor = iText;
	__xuiPropertyGridApplyStyle(pData);
	__xuiPropertyGridRebuildVisible(pData);
	return XUI_OK;
}

XUI_API int xuiPropertyGridGetSelectCount(xui_widget pWidget)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	return (pData != NULL) ? pData->iSelectCount : 0;
}

XUI_API int xuiPropertyGridGetToggleCount(xui_widget pWidget)
{
	xui_property_grid_data_t* pData = __xuiPropertyGridGetData(pWidget);
	return (pData != NULL) ? pData->iToggleCount : 0;
}
