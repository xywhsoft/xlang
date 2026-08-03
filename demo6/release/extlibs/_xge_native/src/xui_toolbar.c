#include "xui_internal.h"

#include <string.h>

typedef struct xui_toolbar_data_t {
	xui_toolbar_item_t arrItems[XUI_TOOLBAR_ITEM_CAPACITY];
	xui_toolbar_metrics_t tMetrics;
	xui_toolbar_colors_t tColors;
	xui_toolbar_select_proc onSelect;
	void* pSelectUser;
	xui_toolbar_overflow_proc onOverflow;
	void* pOverflowUser;
	xui_font pFont;
	int iItemCount;
	int iHover;
	int iActive;
	int bOverflowEnabled;
	int bOverflowActive;
	int iOverflowFirst;
	int iOverflowCount;
	xui_rect_t tOverflowRect;
	int iSelectCount;
	int iOverflowSelectCount;
	int iChangeCount;
} xui_toolbar_data_t;

typedef struct xui_toolbar_resolved_t {
	xui_toolbar_metrics_t tMetrics;
	xui_toolbar_colors_t tColors;
	xui_font pFont;
} xui_toolbar_resolved_t;

static xui_toolbar_data_t* __xuiToolbarGetData(xui_widget pWidget);

static int __xuiToolbarAlpha(uint32_t iColor)
{
	return (int)(iColor & 0xffu);
}

static uint32_t __xuiToolbarColorAlpha(uint32_t iColor, uint32_t iAlpha)
{
	if ( iAlpha > 255u ) iAlpha = 255u;
	return (iColor & 0xffffff00u) | (iAlpha & 0xffu);
}

static float __xuiToolbarMax(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static int __xuiToolbarOrientationValid(int iOrientation)
{
	return (iOrientation == XUI_ORIENTATION_HORIZONTAL) ||
	       (iOrientation == XUI_ORIENTATION_VERTICAL);
}

static int __xuiToolbarItemTypeValid(int iType)
{
	return (iType == XUI_TOOLBAR_ITEM_BUTTON) ||
	       (iType == XUI_TOOLBAR_ITEM_TOGGLE) ||
	       (iType == XUI_TOOLBAR_ITEM_SEPARATOR);
}

static int __xuiToolbarItemInteractive(const xui_toolbar_item_t* pItem)
{
	return (pItem != NULL) &&
	       (pItem->iType != XUI_TOOLBAR_ITEM_SEPARATOR) &&
	       ((pItem->iState & XUI_TOOLBAR_ITEM_ENABLED) != 0);
}

static int __xuiToolbarRectContains(xui_rect_t tRect, float fX, float fY)
{
	return (fX >= tRect.fX) && (fY >= tRect.fY) && (fX < tRect.fX + tRect.fW) && (fY < tRect.fY + tRect.fH);
}

static int __xuiToolbarDescValid(const xui_toolbar_desc_t* pDesc)
{
	if ( pDesc == NULL ) return 1;
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) return 0;
	if ( (pDesc->iItemCount < 0) || (pDesc->iItemCount > XUI_TOOLBAR_ITEM_CAPACITY) ) return 0;
	if ( (pDesc->iItemCount > 0) && (pDesc->pItems == NULL) ) return 0;
	return 1;
}

static int __xuiToolbarMetricsValid(const xui_toolbar_metrics_t* pMetrics)
{
	if ( pMetrics == NULL ) return 0;
	if ( (pMetrics->iSize != 0) && (pMetrics->iSize < sizeof(*pMetrics)) ) return 0;
	if ( !__xuiToolbarOrientationValid(pMetrics->iOrientation) ) return 0;
	if ( (pMetrics->fItemWidth <= 0.0f) ||
	     (pMetrics->fItemHeight <= 0.0f) ||
	     (pMetrics->fSeparatorSize <= 0.0f) ||
	     (pMetrics->fGroupGap < 0.0f) ||
	     (pMetrics->fPaddingX < 0.0f) ||
	     (pMetrics->fPaddingY < 0.0f) ||
	     (pMetrics->fOverflowSize <= 0.0f) ||
	     (pMetrics->fBorderWidth < 0.0f) ||
	     (pMetrics->fIconSize < 0.0f) ||
	     (pMetrics->fIconGap < 0.0f) ) {
		return 0;
	}
	return 1;
}

static int __xuiToolbarColorsValid(const xui_toolbar_colors_t* pColors)
{
	if ( pColors == NULL ) return 0;
	return (pColors->iSize == 0) || (pColors->iSize >= sizeof(*pColors));
}

static void __xuiToolbarDefaultMetrics(xui_toolbar_metrics_t* pMetrics)
{
	memset(pMetrics, 0, sizeof(*pMetrics));
	pMetrics->iSize = sizeof(*pMetrics);
	pMetrics->iOrientation = XUI_ORIENTATION_HORIZONTAL;
	pMetrics->fItemWidth = 64.0f;
	pMetrics->fItemHeight = 26.0f;
	pMetrics->fSeparatorSize = 10.0f;
	pMetrics->fGroupGap = 6.0f;
	pMetrics->fPaddingX = 4.0f;
	pMetrics->fPaddingY = 3.0f;
	pMetrics->fOverflowSize = 26.0f;
	pMetrics->fBorderWidth = 1.0f;
	pMetrics->fIconSize = 14.0f;
	pMetrics->fIconGap = 5.0f;
}

static void __xuiToolbarDefaultColors(xui_toolbar_colors_t* pColors)
{
	memset(pColors, 0, sizeof(*pColors));
	pColors->iSize = sizeof(*pColors);
	pColors->iBackgroundColor = XUI_COLOR_RGBA(247, 250, 254, 255);
	pColors->iBorderColor = XUI_COLOR_RGBA(206, 220, 234, 255);
	pColors->iItemColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	pColors->iHoverColor = XUI_COLOR_RGBA(225, 239, 251, 255);
	pColors->iActiveColor = XUI_COLOR_RGBA(47, 128, 214, 255);
	pColors->iCheckedColor = XUI_COLOR_RGBA(207, 231, 250, 255);
	pColors->iFocusColor = XUI_COLOR_RGBA(47, 128, 214, 255);
	pColors->iDisabledColor = XUI_COLOR_RGBA(231, 236, 243, 190);
	pColors->iSeparatorColor = XUI_COLOR_RGBA(185, 202, 220, 255);
	pColors->iTextColor = XUI_COLOR_RGBA(35, 58, 86, 255);
	pColors->iDisabledTextColor = XUI_COLOR_RGBA(142, 152, 166, 210);
	pColors->iIconColor = XUI_COLOR_RGBA(35, 58, 86, 255);
}

static xui_vec2_t __xuiToolbarMeasureText(xui_widget pWidget, xui_font pFont, const char* sText)
{
	xui_vec2_t tSize;
	xui_proxy pProxy;

	tSize.fX = 0.0f;
	tSize.fY = 18.0f;
	if ( (sText == NULL) || (sText[0] == '\0') ) return tSize;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( (pProxy != NULL) && (pProxy->textMeasure != NULL) && (pFont != NULL) &&
	     (pProxy->textMeasure(pProxy, pFont, sText, &tSize) == XUI_OK) &&
	     (tSize.fX >= 0.0f) && (tSize.fY >= 0.0f) ) {
		return tSize;
	}
	tSize.fX = (float)strlen(sText) * 7.0f;
	tSize.fY = 18.0f;
	return tSize;
}

static int __xuiToolbarStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
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

static int __xuiToolbarStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
{
	xui_style_property_t tProperty;
	int iRet;

	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_FLOAT) && (tProperty.tValue.fFloat >= 0.0f) ) {
		*pValue = tProperty.tValue.fFloat;
		return 1;
	}
	return 0;
}

static int __xuiToolbarStyleInt(xui_widget pWidget, const char* sName, int* pValue)
{
	xui_style_property_t tProperty;
	int iRet;

	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) &&
	     ((tProperty.tValue.iType == XUI_STYLE_VALUE_INT) || (tProperty.tValue.iType == XUI_STYLE_VALUE_BOOL)) ) {
		*pValue = tProperty.tValue.iInt;
		return 1;
	}
	return 0;
}

static xui_font __xuiToolbarStyleFont(xui_widget pWidget, xui_font pBaseFont)
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
		if ( pFont != NULL ) return pFont;
	}
	return pBaseFont;
}

static void __xuiToolbarResolve(xui_widget pWidget, const xui_toolbar_data_t* pData, xui_toolbar_resolved_t* pOut)
{
	int iValue;

	memset(pOut, 0, sizeof(*pOut));
	pOut->tMetrics = pData->tMetrics;
	pOut->tMetrics.iSize = sizeof(pOut->tMetrics);
	pOut->tColors = pData->tColors;
	pOut->tColors.iSize = sizeof(pOut->tColors);
	pOut->pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));

	iValue = pOut->tMetrics.iOrientation;
	if ( __xuiToolbarStyleInt(pWidget, "toolbar.orientation", &iValue) && __xuiToolbarOrientationValid(iValue) ) {
		pOut->tMetrics.iOrientation = iValue;
	}
	(void)__xuiToolbarStyleFloat(pWidget, "toolbar.item.width", &pOut->tMetrics.fItemWidth);
	(void)__xuiToolbarStyleFloat(pWidget, "toolbar.item.height", &pOut->tMetrics.fItemHeight);
	(void)__xuiToolbarStyleFloat(pWidget, "toolbar.separator.size", &pOut->tMetrics.fSeparatorSize);
	(void)__xuiToolbarStyleFloat(pWidget, "toolbar.group_gap", &pOut->tMetrics.fGroupGap);
	(void)__xuiToolbarStyleFloat(pWidget, "toolbar.padding.x", &pOut->tMetrics.fPaddingX);
	(void)__xuiToolbarStyleFloat(pWidget, "toolbar.padding.y", &pOut->tMetrics.fPaddingY);
	(void)__xuiToolbarStyleFloat(pWidget, "toolbar.overflow.size", &pOut->tMetrics.fOverflowSize);
	(void)__xuiToolbarStyleFloat(pWidget, "toolbar.border.width", &pOut->tMetrics.fBorderWidth);
	(void)__xuiToolbarStyleFloat(pWidget, "toolbar.icon.size", &pOut->tMetrics.fIconSize);
	(void)__xuiToolbarStyleFloat(pWidget, "toolbar.icon.gap", &pOut->tMetrics.fIconGap);

	(void)__xuiToolbarStyleColor(pWidget, "toolbar.background.color", &pOut->tColors.iBackgroundColor);
	(void)__xuiToolbarStyleColor(pWidget, "toolbar.border.color", &pOut->tColors.iBorderColor);
	(void)__xuiToolbarStyleColor(pWidget, "toolbar.item.color", &pOut->tColors.iItemColor);
	(void)__xuiToolbarStyleColor(pWidget, "toolbar.item.hover_color", &pOut->tColors.iHoverColor);
	(void)__xuiToolbarStyleColor(pWidget, "toolbar.item.active_color", &pOut->tColors.iActiveColor);
	(void)__xuiToolbarStyleColor(pWidget, "toolbar.item.checked_color", &pOut->tColors.iCheckedColor);
	(void)__xuiToolbarStyleColor(pWidget, "toolbar.focus.color", &pOut->tColors.iFocusColor);
	(void)__xuiToolbarStyleColor(pWidget, "toolbar.item.disabled_color", &pOut->tColors.iDisabledColor);
	(void)__xuiToolbarStyleColor(pWidget, "toolbar.separator.color", &pOut->tColors.iSeparatorColor);
	(void)__xuiToolbarStyleColor(pWidget, "toolbar.text.color", &pOut->tColors.iTextColor);
	(void)__xuiToolbarStyleColor(pWidget, "toolbar.text.disabled_color", &pOut->tColors.iDisabledTextColor);
	(void)__xuiToolbarStyleColor(pWidget, "toolbar.icon.color", &pOut->tColors.iIconColor);

	if ( pOut->tMetrics.fItemWidth < 1.0f ) pOut->tMetrics.fItemWidth = 1.0f;
	if ( pOut->tMetrics.fItemHeight < 1.0f ) pOut->tMetrics.fItemHeight = 1.0f;
	if ( pOut->tMetrics.fSeparatorSize < 1.0f ) pOut->tMetrics.fSeparatorSize = 1.0f;
	if ( pOut->tMetrics.fOverflowSize < 1.0f ) pOut->tMetrics.fOverflowSize = 1.0f;
	pOut->pFont = __xuiToolbarStyleFont(pWidget, pOut->pFont);
}

static int __xuiToolbarItemSize(const xui_toolbar_item_t* pItem, const xui_toolbar_resolved_t* pResolved)
{
	if ( (pItem != NULL) && (pItem->iType == XUI_TOOLBAR_ITEM_SEPARATOR) ) {
		return (int)pResolved->tMetrics.fSeparatorSize;
	}
	return (pResolved->tMetrics.iOrientation == XUI_ORIENTATION_VERTICAL) ?
		(int)pResolved->tMetrics.fItemHeight : (int)pResolved->tMetrics.fItemWidth;
}

static int __xuiToolbarMeasureItems(xui_widget pWidget, xui_toolbar_data_t* pData, const xui_toolbar_resolved_t* pResolved, xui_vec2_t* pSize)
{
	float fAxis;
	float fCross;
	float fItem;
	int iPrevGroup;
	int i;

	(void)pWidget;
	if ( (pData == NULL) || (pResolved == NULL) || (pSize == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	fAxis = pResolved->tMetrics.fPaddingX * 2.0f;
	fCross = pResolved->tMetrics.fPaddingY * 2.0f;
	fCross += (pResolved->tMetrics.iOrientation == XUI_ORIENTATION_VERTICAL) ? pResolved->tMetrics.fItemWidth : pResolved->tMetrics.fItemHeight;
	iPrevGroup = 0;
	for ( i = 0; i < pData->iItemCount; i++ ) {
		if ( (i > 0) &&
		     (pData->arrItems[i].iType != XUI_TOOLBAR_ITEM_SEPARATOR) &&
		     (pData->arrItems[i].iGroup != iPrevGroup) ) {
			fAxis += pResolved->tMetrics.fGroupGap;
		}
		fItem = (float)__xuiToolbarItemSize(&pData->arrItems[i], pResolved);
		fAxis += __xuiToolbarMax(fItem, 1.0f);
		if ( pData->arrItems[i].iType != XUI_TOOLBAR_ITEM_SEPARATOR ) {
			iPrevGroup = pData->arrItems[i].iGroup;
		}
	}
	if ( pResolved->tMetrics.iOrientation == XUI_ORIENTATION_VERTICAL ) {
		pSize->fX = xuiInternalSnapSize(fCross);
		pSize->fY = xuiInternalSnapSize(fAxis);
	} else {
		pSize->fX = xuiInternalSnapSize(fAxis);
		pSize->fY = xuiInternalSnapSize(fCross);
	}
	return XUI_OK;
}

static int __xuiToolbarLayoutItems(xui_widget pWidget, xui_toolbar_data_t* pData, const xui_toolbar_resolved_t* pResolved)
{
	xui_rect_t tContent;
	xui_rect_t tInner;
	xui_rect_t tItem;
	float fLimit;
	float fCursor;
	float fSize;
	int bVertical;
	int iPrevGroup;
	int i;

	if ( (pWidget == NULL) || (pData == NULL) || (pResolved == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	tContent = xuiInternalSnapRect(xuiWidgetGetContentRect(pWidget));
	tInner = tContent;
	tInner.fX += pResolved->tMetrics.fPaddingX;
	tInner.fY += pResolved->tMetrics.fPaddingY;
	tInner.fW -= pResolved->tMetrics.fPaddingX * 2.0f;
	tInner.fH -= pResolved->tMetrics.fPaddingY * 2.0f;
	if ( tInner.fW < 0.0f ) tInner.fW = 0.0f;
	if ( tInner.fH < 0.0f ) tInner.fH = 0.0f;
	memset(&pData->tOverflowRect, 0, sizeof(pData->tOverflowRect));
	pData->iOverflowFirst = -1;
	pData->iOverflowCount = 0;
	for ( i = 0; i < pData->iItemCount; i++ ) {
		memset(&pData->arrItems[i].tRect, 0, sizeof(pData->arrItems[i].tRect));
	}
	bVertical = (pResolved->tMetrics.iOrientation == XUI_ORIENTATION_VERTICAL);
	fLimit = bVertical ? (tInner.fY + tInner.fH) : (tInner.fX + tInner.fW);
	if ( pData->bOverflowEnabled ) {
		fLimit -= pResolved->tMetrics.fOverflowSize;
	}
	fCursor = bVertical ? tInner.fY : tInner.fX;
	iPrevGroup = 0;
	for ( i = 0; i < pData->iItemCount; i++ ) {
		if ( (i > 0) &&
		     (pData->arrItems[i].iType != XUI_TOOLBAR_ITEM_SEPARATOR) &&
		     (pData->arrItems[i].iGroup != iPrevGroup) ) {
			fCursor += pResolved->tMetrics.fGroupGap;
		}
		fSize = (float)__xuiToolbarItemSize(&pData->arrItems[i], pResolved);
		if ( fSize < 1.0f ) fSize = 1.0f;
		if ( pData->bOverflowEnabled && ((fCursor + fSize) > fLimit) ) {
			pData->iOverflowFirst = i;
			pData->iOverflowCount = pData->iItemCount - i;
			break;
		}
		if ( bVertical ) {
			tItem = (xui_rect_t){tInner.fX, fCursor, tInner.fW, fSize};
			if ( (tItem.fY + tItem.fH) > (tInner.fY + tInner.fH) ) {
				tItem.fH = tInner.fY + tInner.fH - tItem.fY;
			}
		} else {
			tItem = (xui_rect_t){fCursor, tInner.fY, fSize, tInner.fH};
			if ( (tItem.fX + tItem.fW) > (tInner.fX + tInner.fW) ) {
				tItem.fW = tInner.fX + tInner.fW - tItem.fX;
			}
		}
		if ( tItem.fW < 0.0f ) tItem.fW = 0.0f;
		if ( tItem.fH < 0.0f ) tItem.fH = 0.0f;
		pData->arrItems[i].tRect = xuiInternalSnapRect(tItem);
		fCursor += fSize;
		if ( pData->arrItems[i].iType != XUI_TOOLBAR_ITEM_SEPARATOR ) {
			iPrevGroup = pData->arrItems[i].iGroup;
		}
	}
	if ( pData->iOverflowCount > 0 ) {
		if ( bVertical ) {
			pData->tOverflowRect = (xui_rect_t){tInner.fX, tInner.fY + tInner.fH - pResolved->tMetrics.fOverflowSize, tInner.fW, pResolved->tMetrics.fOverflowSize};
		} else {
			pData->tOverflowRect = (xui_rect_t){tInner.fX + tInner.fW - pResolved->tMetrics.fOverflowSize, tInner.fY, pResolved->tMetrics.fOverflowSize, tInner.fH};
		}
		pData->tOverflowRect = xuiInternalSnapRect(pData->tOverflowRect);
	}
	return XUI_OK;
}

static int __xuiToolbarIndexAt(xui_toolbar_data_t* pData, float fX, float fY)
{
	int i;

	if ( pData == NULL ) return -1;
	for ( i = 0; i < pData->iItemCount; i++ ) {
		if ( __xuiToolbarItemInteractive(&pData->arrItems[i]) &&
		     __xuiToolbarRectContains(pData->arrItems[i].tRect, fX, fY) ) {
			return i;
		}
	}
	return -1;
}

static int __xuiToolbarFirstEnabled(xui_toolbar_data_t* pData)
{
	int i;

	if ( pData == NULL ) return -1;
	for ( i = 0; i < pData->iItemCount; i++ ) {
		if ( __xuiToolbarItemInteractive(&pData->arrItems[i]) ) return i;
	}
	return -1;
}

static int __xuiToolbarNextEnabled(xui_toolbar_data_t* pData, int iCurrent, int iStep)
{
	int i;
	int iIndex;

	if ( (pData == NULL) || (pData->iItemCount <= 0) || (iStep == 0) ) return -1;
	iIndex = iCurrent;
	for ( i = 0; i < pData->iItemCount; i++ ) {
		iIndex += iStep;
		if ( iIndex < 0 ) iIndex = pData->iItemCount - 1;
		else if ( iIndex >= pData->iItemCount ) iIndex = 0;
		if ( __xuiToolbarItemInteractive(&pData->arrItems[iIndex]) ) return iIndex;
	}
	return -1;
}

static int __xuiToolbarInvalidateItem(xui_widget pWidget, xui_toolbar_data_t* pData, int iIndex)
{
	xui_rect_t tRect;

	if ( (pWidget == NULL) || (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) {
		return XUI_OK;
	}
	tRect = pData->arrItems[iIndex].tRect;
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XUI_OK;
	}
	tRect.fX -= 2.0f;
	tRect.fY -= 2.0f;
	tRect.fW += 4.0f;
	tRect.fH += 4.0f;
	return xuiWidgetInvalidateRect(pWidget, tRect, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiToolbarSetHover(xui_widget pWidget, xui_toolbar_data_t* pData, int iIndex)
{
	int iOldHover;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( (iIndex < 0) || (iIndex >= pData->iItemCount) || !__xuiToolbarItemInteractive(&pData->arrItems[iIndex]) ) {
		iIndex = -1;
	}
	if ( pData->iHover == iIndex ) return XUI_OK;
	iOldHover = pData->iHover;
	pData->iHover = iIndex;
	iRet = __xuiToolbarInvalidateItem(pWidget, pData, iOldHover);
	if ( iRet == XUI_OK ) {
		iRet = __xuiToolbarInvalidateItem(pWidget, pData, iIndex);
	}
	return iRet;
}

static int __xuiToolbarSelect(xui_widget pWidget, xui_toolbar_data_t* pData, int iIndex)
{
	if ( (pWidget == NULL) || (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ||
	     !__xuiToolbarItemInteractive(&pData->arrItems[iIndex]) ) {
		return XUI_OK;
	}
	if ( pData->arrItems[iIndex].iType == XUI_TOOLBAR_ITEM_TOGGLE ) {
		pData->arrItems[iIndex].iState ^= XUI_TOOLBAR_ITEM_CHECKED;
		pData->iChangeCount++;
	}
	pData->iSelectCount++;
	if ( pData->onSelect != NULL ) {
		pData->onSelect(pWidget, iIndex, pData->arrItems[iIndex].iValue, pData->pSelectUser);
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiToolbarDrawRectFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( __xuiToolbarAlpha(iColor) == 0 ) return XUI_OK;
	return pProxy->drawRectFill(pProxy, pDraw, tRect, iColor);
}

static int __xuiToolbarDrawRectStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	if ( (fWidth <= 0.0f) || (__xuiToolbarAlpha(iColor) == 0) ) return XUI_OK;
	return pProxy->drawRectStroke(pProxy, pDraw, tRect, fWidth, iColor);
}

static xui_rect_t __xuiToolbarInsetRect(xui_rect_t tRect, float fLeft, float fTop, float fRight, float fBottom)
{
	tRect.fX += fLeft;
	tRect.fY += fTop;
	tRect.fW -= fLeft + fRight;
	tRect.fH -= fTop + fBottom;
	if ( tRect.fW < 0.0f ) tRect.fW = 0.0f;
	if ( tRect.fH < 0.0f ) tRect.fH = 0.0f;
	return tRect;
}

static int __xuiToolbarDrawSeparator(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, int bVerticalToolbar, uint32_t iColor)
{
	xui_rect_t tLine;
	int iRet;

	if ( __xuiToolbarAlpha(iColor) == 0 ) return XUI_OK;
	tLine = tRect;
	if ( bVerticalToolbar ) {
		tLine.fY += (tLine.fH - 1.0f) * 0.5f;
		tLine.fH = 1.0f;
		tLine.fX += 6.0f;
		tLine.fW -= 12.0f;
	} else {
		tLine.fX += (tLine.fW - 1.0f) * 0.5f;
		tLine.fW = 1.0f;
		tLine.fY += 5.0f;
		tLine.fH -= 10.0f;
	}
	if ( (tLine.fW <= 0.0f) || (tLine.fH <= 0.0f) ) return XUI_OK;
	iRet = pProxy->drawRectFill(pProxy, pDraw, xuiInternalSnapRect(tLine), iColor);
	if ( iRet != XUI_OK ) return iRet;
	if ( bVerticalToolbar ) {
		tLine.fY += 1.0f;
	} else {
		tLine.fX += 1.0f;
	}
	return pProxy->drawRectFill(pProxy, pDraw, xuiInternalSnapRect(tLine), XUI_COLOR_RGBA(255, 255, 255, 112));
}

static int __xuiToolbarDrawOverflowDots(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	xui_rect_t tDot;
	int i;
	int iRet;

	tDot.fW = 2.0f;
	tDot.fH = 2.0f;
	tDot.fX = tRect.fX + (tRect.fW - 10.0f) * 0.5f;
	tDot.fY = tRect.fY + (tRect.fH - 2.0f) * 0.5f;
	for ( i = 0; i < 3; i++ ) {
		iRet = __xuiToolbarDrawRectFill(pProxy, pDraw, tDot, iColor);
		if ( iRet != XUI_OK ) return iRet;
		tDot.fX += 4.0f;
	}
	return XUI_OK;
}

static int __xuiToolbarDrawItemContent(xui_widget pWidget, xui_proxy pProxy, xui_draw_context pDraw, const xui_toolbar_item_t* pItem, xui_rect_t tRect, const xui_toolbar_resolved_t* pResolved, uint32_t iTextColor, uint32_t iIconColor, int bPressed)
{
	xui_rect_t tIcon;
	xui_rect_t tText;
	xui_vec2_t tTextSize;
	float fIconSize;
	float fGroupW;
	int bHasIcon;
	int bHasText;
	int iRet;

	bHasIcon = (pItem->pIcon != NULL) && (pResolved->tMetrics.fIconSize > 0.0f);
	bHasText = (pItem->sText != NULL) && (pItem->sText[0] != '\0');
	if ( !bHasIcon && !bHasText ) return XUI_OK;
	tText = __xuiToolbarInsetRect(tRect, 6.0f, 0.0f, 6.0f, 0.0f);
	if ( bPressed ) tText.fY += 1.0f;
	if ( bHasIcon ) {
		fIconSize = pResolved->tMetrics.fIconSize;
		if ( fIconSize > tText.fH ) fIconSize = tText.fH;
		if ( !bHasText ) {
			tIcon = (xui_rect_t){tText.fX + (tText.fW - fIconSize) * 0.5f, tText.fY + (tText.fH - fIconSize) * 0.5f, fIconSize, fIconSize};
		} else {
			tTextSize = __xuiToolbarMeasureText(pWidget, pResolved->pFont, pItem->sText);
			fGroupW = fIconSize + pResolved->tMetrics.fIconGap + tTextSize.fX;
			if ( fGroupW > tText.fW ) fGroupW = tText.fW;
			tIcon = (xui_rect_t){tText.fX + (tText.fW - fGroupW) * 0.5f, tText.fY + (tText.fH - fIconSize) * 0.5f, fIconSize, fIconSize};
			tText.fX = tIcon.fX + fIconSize + pResolved->tMetrics.fIconGap;
			tText.fW = tRect.fX + tRect.fW - 6.0f - tText.fX;
		}
		if ( pProxy->drawSurface != NULL ) {
			iRet = pProxy->drawSurface(pProxy, pDraw, pItem->pIcon, pItem->tIconSrc, xuiInternalSnapRect(tIcon), iIconColor, 0);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	if ( bHasText && (pResolved->pFont != NULL) && (pProxy->drawText != NULL) && (tText.fW > 0.0f) ) {
		return pProxy->drawText(pProxy, pDraw, pResolved->pFont, pItem->sText, tText, iTextColor,
			(bHasIcon ? XUI_TEXT_ALIGN_LEFT : XUI_TEXT_ALIGN_CENTER) | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
	}
	return XUI_OK;
}

static int __xuiToolbarCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_toolbar_data_t* pData;
	xui_toolbar_resolved_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tRect;
	xui_rect_t tDraw;
	xui_rect_t tFocus;
	uint32_t iFill;
	uint32_t iBorder;
	uint32_t iText;
	uint32_t iIcon;
	int bVertical;
	int bPressed;
	int i;
	int iRet;

	(void)iStateId;
	(void)pUser;
	pData = __xuiToolbarGetData(pWidget);
	if ( (pData == NULL) || (pDraw == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiToolbarResolve(pWidget, pData, &tResolved);
	(void)__xuiToolbarLayoutItems(pWidget, pData, &tResolved);
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	tRect = xuiWidgetGetRect(pWidget);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	iRet = __xuiToolbarDrawRectFill(pProxy, pDraw, tRect, tResolved.tColors.iBackgroundColor);
	if ( iRet != XUI_OK ) return iRet;
	if ( tResolved.tMetrics.fBorderWidth > 0.0f ) {
		iRet = __xuiToolbarDrawRectStroke(pProxy, pDraw, tRect, tResolved.tMetrics.fBorderWidth, tResolved.tColors.iBorderColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	bVertical = (tResolved.tMetrics.iOrientation == XUI_ORIENTATION_VERTICAL);
	for ( i = 0; i < pData->iItemCount; i++ ) {
		if ( (pData->arrItems[i].tRect.fW <= 0.0f) || (pData->arrItems[i].tRect.fH <= 0.0f) ) continue;
		if ( pData->arrItems[i].iType == XUI_TOOLBAR_ITEM_SEPARATOR ) {
			iRet = __xuiToolbarDrawSeparator(pProxy, pDraw, pData->arrItems[i].tRect, bVertical, tResolved.tColors.iSeparatorColor);
			if ( iRet != XUI_OK ) return iRet;
			continue;
		}
		iFill = tResolved.tColors.iItemColor;
		iBorder = XUI_COLOR_RGBA(0, 0, 0, 0);
		iText = tResolved.tColors.iTextColor;
		iIcon = tResolved.tColors.iIconColor;
		bPressed = 0;
		if ( !__xuiToolbarItemInteractive(&pData->arrItems[i]) ) {
			iFill = tResolved.tColors.iDisabledColor;
			iText = tResolved.tColors.iDisabledTextColor;
			iIcon = tResolved.tColors.iDisabledTextColor;
			iBorder = __xuiToolbarColorAlpha(tResolved.tColors.iBorderColor, 120);
		} else if ( i == pData->iActive ) {
			iFill = tResolved.tColors.iActiveColor;
			iText = XUI_COLOR_RGBA(255, 255, 255, 255);
			iIcon = XUI_COLOR_RGBA(255, 255, 255, 255);
			iBorder = __xuiToolbarColorAlpha(tResolved.tColors.iActiveColor, 235);
			bPressed = 1;
		} else if ( (pData->arrItems[i].iState & XUI_TOOLBAR_ITEM_CHECKED) != 0 ) {
			iFill = tResolved.tColors.iCheckedColor;
			iBorder = __xuiToolbarColorAlpha(tResolved.tColors.iFocusColor, 120);
		} else if ( i == pData->iHover ) {
			iFill = tResolved.tColors.iHoverColor;
			iBorder = __xuiToolbarColorAlpha(tResolved.tColors.iFocusColor, 160);
		}
		tDraw = bVertical ?
			__xuiToolbarInsetRect(pData->arrItems[i].tRect, 3.0f, 2.0f, 3.0f, 2.0f) :
			__xuiToolbarInsetRect(pData->arrItems[i].tRect, 2.0f, 3.0f, 2.0f, 3.0f);
		if ( bPressed ) tDraw.fY += 1.0f;
		iRet = __xuiToolbarDrawRectFill(pProxy, pDraw, xuiInternalSnapRect(tDraw), iFill);
		if ( iRet != XUI_OK ) return iRet;
		iRet = __xuiToolbarDrawRectStroke(pProxy, pDraw, xuiInternalSnapRect(tDraw), tResolved.tMetrics.fBorderWidth, iBorder);
		if ( iRet != XUI_OK ) return iRet;
		if ( (xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget) && (i == pData->iHover) ) {
			tFocus = tDraw;
			tFocus.fX += 5.0f;
			tFocus.fY = tDraw.fY + tDraw.fH - 3.0f;
			tFocus.fW -= 10.0f;
			tFocus.fH = 2.0f;
			if ( tFocus.fW > 0.0f ) {
				iRet = __xuiToolbarDrawRectFill(pProxy, pDraw, xuiInternalSnapRect(tFocus), tResolved.tColors.iFocusColor);
				if ( iRet != XUI_OK ) return iRet;
			}
		}
		iRet = __xuiToolbarDrawItemContent(pWidget, pProxy, pDraw, &pData->arrItems[i], pData->arrItems[i].tRect, &tResolved, iText, iIcon, bPressed);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( (pData->iOverflowCount > 0) && (pData->tOverflowRect.fW > 0.0f) && (pData->tOverflowRect.fH > 0.0f) ) {
		tDraw = bVertical ?
			__xuiToolbarInsetRect(pData->tOverflowRect, 3.0f, 2.0f, 3.0f, 2.0f) :
			__xuiToolbarInsetRect(pData->tOverflowRect, 2.0f, 3.0f, 2.0f, 3.0f);
		if ( pData->bOverflowActive ) tDraw.fY += 1.0f;
		iFill = pData->bOverflowActive ? tResolved.tColors.iActiveColor : tResolved.tColors.iHoverColor;
		iText = pData->bOverflowActive ? XUI_COLOR_RGBA(255, 255, 255, 255) : tResolved.tColors.iTextColor;
		iRet = __xuiToolbarDrawRectFill(pProxy, pDraw, xuiInternalSnapRect(tDraw), iFill);
		if ( iRet != XUI_OK ) return iRet;
		iRet = __xuiToolbarDrawRectStroke(pProxy, pDraw, xuiInternalSnapRect(tDraw), tResolved.tMetrics.fBorderWidth, __xuiToolbarColorAlpha(tResolved.tColors.iFocusColor, 160));
		if ( iRet != XUI_OK ) return iRet;
		iRet = __xuiToolbarDrawOverflowDots(pProxy, pDraw, tDraw, iText);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiToolbarContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_toolbar_data_t* pData;
	xui_toolbar_resolved_t tResolved;

	(void)tConstraint;
	(void)pUser;
	if ( (pWidget == NULL) || (pSize == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiToolbarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiToolbarResolve(pWidget, pData, &tResolved);
	return __xuiToolbarMeasureItems(pWidget, pData, &tResolved, pSize);
}

static int __xuiToolbarTooltipResolve(xui_context pContext, xui_widget pWidget, xui_tooltip_desc_t* pDesc, void* pUser)
{
	xui_toolbar_data_t* pData;
	xui_toolbar_resolved_t tResolved;
	xui_rect_t tWorld;
	const char* sText;
	int iIndex;

	(void)pUser;
	if ( (pContext == NULL) || (pWidget == NULL) || (pDesc == NULL) ) return 0;
	pData = __xuiToolbarGetData(pWidget);
	if ( pData == NULL ) return 0;
	__xuiToolbarResolve(pWidget, pData, &tResolved);
	(void)__xuiToolbarLayoutItems(pWidget, pData, &tResolved);
	tWorld = xuiWidgetGetWorldRect(pWidget);
	iIndex = __xuiToolbarIndexAt(pData, pContext->fTooltipMouseX - tWorld.fX, pContext->fTooltipMouseY - tWorld.fY);
	if ( iIndex < 0 ) return 0;
	sText = pData->arrItems[iIndex].sTooltip;
	if ( (sText == NULL) || (sText[0] == '\0') ) return 0;
	memset(pDesc, 0, sizeof(*pDesc));
	pDesc->iSize = sizeof(*pDesc);
	pDesc->iType = XUI_TOOLTIP_TEXT;
	pDesc->sText = sText;
	pDesc->iAnchor = XUI_TOOLTIP_ANCHOR_CURSOR;
	pDesc->fOffsetX = 12.0f;
	pDesc->fOffsetY = 16.0f;
	pDesc->bFollowCursor = 1;
	return 1;
}

static int __xuiToolbarEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_toolbar_data_t* pData;
	xui_toolbar_resolved_t tResolved;
	xui_context pContext;
	xui_rect_t tWorld;
	float fLocalX;
	float fLocalY;
	int iIndex;
	int iNext;
	int bLeftButton;
	int bOverflowHit;

	(void)pUser;
	if ( (pWidget == NULL) || (pEvent == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE ) return XUI_OK;
	pData = __xuiToolbarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiToolbarResolve(pWidget, pData, &tResolved);
	(void)__xuiToolbarLayoutItems(pWidget, pData, &tResolved);
	pContext = xuiWidgetGetContext(pWidget);
	tWorld = xuiWidgetGetWorldRect(pWidget);
	fLocalX = pEvent->fX - tWorld.fX;
	fLocalY = pEvent->fY - tWorld.fY;
	iIndex = __xuiToolbarIndexAt(pData, fLocalX, fLocalY);
	bOverflowHit = (pData->iOverflowCount > 0) && __xuiToolbarRectContains(pData->tOverflowRect, fLocalX, fLocalY);
	bLeftButton = (pEvent->iButton == 0) || (pEvent->iButton == XUI_POINTER_BUTTON_LEFT);
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_MOVE:
		if ( pData->bOverflowActive ) {
			return XUI_EVENT_DISPATCH_STOP;
		}
		(void)__xuiToolbarSetHover(pWidget, pData, iIndex);
		return (pData->iActive >= 0) ? XUI_EVENT_DISPATCH_STOP : XUI_OK;
	case XUI_EVENT_POINTER_LEAVE:
		if ( (pData->iActive < 0) && !pData->bOverflowActive ) {
			(void)__xuiToolbarSetHover(pWidget, pData, -1);
		}
		return XUI_OK;
	case XUI_EVENT_POINTER_DOWN:
		if ( !bLeftButton ) return XUI_OK;
		if ( bOverflowHit ) {
			(void)xuiSetFocusWidget(pContext, pWidget);
			(void)xuiSetPointerCapture(pContext, pWidget);
			pData->iActive = -1;
			pData->bOverflowActive = 1;
			(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( iIndex < 0 ) return XUI_OK;
		(void)xuiSetFocusWidget(pContext, pWidget);
		(void)xuiSetPointerCapture(pContext, pWidget);
		pData->iHover = iIndex;
		pData->iActive = iIndex;
		pData->bOverflowActive = 0;
		(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_POINTER_UP:
		if ( (pData->iActive < 0) && !pData->bOverflowActive ) return XUI_OK;
		if ( xuiGetPointerCapture(pContext) == pWidget ) {
			(void)xuiReleasePointerCapture(pContext, pWidget);
		}
		if ( pData->bOverflowActive ) {
			if ( bOverflowHit && (pData->onOverflow != NULL) ) {
				pData->iOverflowSelectCount++;
				pData->onOverflow(pWidget, pData->iOverflowFirst, pData->iOverflowCount, pData->pOverflowUser);
			}
		} else if ( iIndex == pData->iActive ) {
			(void)__xuiToolbarSelect(pWidget, pData, iIndex);
		}
		pData->iHover = iIndex;
		pData->iActive = -1;
		pData->bOverflowActive = 0;
		(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		pData->iActive = -1;
		pData->bOverflowActive = 0;
		return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	case XUI_EVENT_KEY_DOWN:
		if ( xuiGetFocusWidget(pContext) != pWidget ) return XUI_OK;
		if ( ((tResolved.tMetrics.iOrientation == XUI_ORIENTATION_HORIZONTAL) && (pEvent->iKey == XUI_KEY_LEFT || pEvent->iKey == XUI_KEY_RIGHT)) ||
		     ((tResolved.tMetrics.iOrientation == XUI_ORIENTATION_VERTICAL) && (pEvent->iKey == XUI_KEY_UP || pEvent->iKey == XUI_KEY_DOWN)) ) {
			iNext = __xuiToolbarNextEnabled(pData, pData->iHover, (pEvent->iKey == XUI_KEY_RIGHT || pEvent->iKey == XUI_KEY_DOWN) ? 1 : -1);
			if ( iNext >= 0 ) (void)__xuiToolbarSetHover(pWidget, pData, iNext);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( (pEvent->iKey == XUI_KEY_ENTER) || (pEvent->iKey == XUI_KEY_SPACE) ) {
			iIndex = ((pData->iHover >= 0) && (pData->iHover < pData->iItemCount) && __xuiToolbarItemInteractive(&pData->arrItems[pData->iHover])) ?
				pData->iHover : __xuiToolbarFirstEnabled(pData);
			if ( iIndex >= 0 ) {
				pData->iHover = iIndex;
				(void)__xuiToolbarSelect(pWidget, pData, iIndex);
				return XUI_EVENT_DISPATCH_STOP;
			}
		}
		break;
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
		return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	default:
		break;
	}
	return XUI_OK;
}

static void __xuiToolbarDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_MANUAL;
	pLayout->iWidthMode = XUI_SIZE_CONTENT;
	pLayout->iHeightMode = XUI_SIZE_CONTENT;
	pLayout->iFlowMode = XUI_FLOW_BLOCK;
	pLayout->iDock = XUI_DOCK_TOP;
	pLayout->iOverflow = XUI_OVERFLOW_VISIBLE;
	pLayout->iAlignX = XUI_ALIGN_STRETCH;
	pLayout->iAlignY = XUI_ALIGN_START;
	pLayout->iTableRowSpan = 1;
	pLayout->iTableColumnSpan = 1;
	pLayout->iGridColumnCount = 1;
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fShrink = 1.0f;
}

static void __xuiToolbarDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE | XUI_CACHE_UPDATE_ALL_STATES;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiToolbarInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiToolbarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiToolbarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiToolbarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiToolbarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiToolbarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiToolbarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiToolbarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiToolbarEvent, NULL);
	return iRet;
}

static int __xuiToolbarInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_toolbar_data_t* pData;
	const xui_toolbar_desc_t* pDesc;
	int iRet;

	(void)pUser;
	if ( (pWidget == NULL) || (pTypeData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = (xui_toolbar_data_t*)pTypeData;
	pDesc = (const xui_toolbar_desc_t*)pCreateData;
	memset(pData, 0, sizeof(*pData));
	__xuiToolbarDefaultMetrics(&pData->tMetrics);
	__xuiToolbarDefaultColors(&pData->tColors);
	pData->iHover = -1;
	pData->iActive = -1;
	pData->iOverflowFirst = -1;
	pData->pFont = xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	if ( (pDesc != NULL) && (pDesc->pFont != NULL) ) pData->pFont = pDesc->pFont;
	if ( (pDesc != NULL) && pDesc->bHasMetrics ) {
		if ( !__xuiToolbarMetricsValid(&pDesc->tMetrics) ) return XUI_ERROR_INVALID_ARGUMENT;
		pData->tMetrics = pDesc->tMetrics;
		pData->tMetrics.iSize = sizeof(pData->tMetrics);
	}
	if ( (pDesc != NULL) && pDesc->bHasColors ) {
		if ( !__xuiToolbarColorsValid(&pDesc->tColors) ) return XUI_ERROR_INVALID_ARGUMENT;
		pData->tColors = pDesc->tColors;
		pData->tColors.iSize = sizeof(pData->tColors);
	}
	pData->bOverflowEnabled = (pDesc != NULL) ? (pDesc->bOverflowEnabled != 0) : 0;
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 0);
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_VISIBLE);
	(void)xuiWidgetSetTooltipResolver(pWidget, __xuiToolbarTooltipResolve, NULL);
	iRet = __xuiToolbarInitEvents(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	if ( (pDesc != NULL) && (pDesc->iItemCount > 0) ) {
		return xuiToolbarSetItems(pWidget, pDesc->pItems, pDesc->iItemCount);
	}
	return XUI_OK;
}

static void __xuiToolbarDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_toolbar_data_t* pData;

	(void)pUser;
	pData = (xui_toolbar_data_t*)pTypeData;
	if ( pWidget != NULL ) {
		if ( xuiGetPointerCapture(xuiWidgetGetContext(pWidget)) == pWidget ) {
			(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
		}
		(void)xuiWidgetClearTooltip(pWidget);
	}
	if ( pData != NULL ) memset(pData, 0, sizeof(*pData));
}

static xui_toolbar_data_t* __xuiToolbarGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) return NULL;
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "toolbar");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) return NULL;
	return (xui_toolbar_data_t*)xuiWidgetGetTypeData(pWidget);
}

static void __xuiToolbarRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
{
	xui_style_property_info_t tInfo;

	if ( xuiStyleFindProperty(pContext, sName) != 0 ) return;
	memset(&tInfo, 0, sizeof(tInfo));
	tInfo.iSize = sizeof(tInfo);
	tInfo.sName = sName;
	tInfo.iValueType = iValueType;
	tInfo.iFlags = iFlags;
	tInfo.iDirtyFlags = iDirtyFlags;
	tInfo.pWidgetType = pType;
	(void)xuiStyleRegisterProperty(pContext, &tInfo, NULL);
}

static void __xuiToolbarRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;
	uint32_t iLayoutDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	iLayoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiToolbarRegisterStyleProperty(pContext, pType, "font.name", XUI_STYLE_VALUE_STRING, iLayoutDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiToolbarRegisterStyleProperty(pContext, pType, "toolbar.background.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiToolbarRegisterStyleProperty(pContext, pType, "toolbar.border.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiToolbarRegisterStyleProperty(pContext, pType, "toolbar.item.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiToolbarRegisterStyleProperty(pContext, pType, "toolbar.item.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiToolbarRegisterStyleProperty(pContext, pType, "toolbar.item.active_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiToolbarRegisterStyleProperty(pContext, pType, "toolbar.item.checked_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiToolbarRegisterStyleProperty(pContext, pType, "toolbar.item.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiToolbarRegisterStyleProperty(pContext, pType, "toolbar.focus.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiToolbarRegisterStyleProperty(pContext, pType, "toolbar.separator.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiToolbarRegisterStyleProperty(pContext, pType, "toolbar.text.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiToolbarRegisterStyleProperty(pContext, pType, "toolbar.text.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiToolbarRegisterStyleProperty(pContext, pType, "toolbar.icon.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiToolbarRegisterStyleProperty(pContext, pType, "toolbar.orientation", XUI_STYLE_VALUE_INT, iLayoutDirty, 0);
	__xuiToolbarRegisterStyleProperty(pContext, pType, "toolbar.item.width", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiToolbarRegisterStyleProperty(pContext, pType, "toolbar.item.height", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiToolbarRegisterStyleProperty(pContext, pType, "toolbar.separator.size", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiToolbarRegisterStyleProperty(pContext, pType, "toolbar.group_gap", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiToolbarRegisterStyleProperty(pContext, pType, "toolbar.padding.x", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiToolbarRegisterStyleProperty(pContext, pType, "toolbar.padding.y", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiToolbarRegisterStyleProperty(pContext, pType, "toolbar.overflow.size", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiToolbarRegisterStyleProperty(pContext, pType, "toolbar.border.width", XUI_STYLE_VALUE_FLOAT, iPaintDirty, 0);
	__xuiToolbarRegisterStyleProperty(pContext, pType, "toolbar.icon.size", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiToolbarRegisterStyleProperty(pContext, pType, "toolbar.icon.gap", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
}

XUI_API xui_widget_type xuiToolbarGetType(xui_context pContext)
{
	xui_widget_type pType;
	xui_widget_type_desc_t tDesc;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) return NULL;
	pType = xuiWidgetFindType(pContext, "toolbar");
	if ( pType != NULL ) return pType;
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "toolbar";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_toolbar_data_t);
	tDesc.onInit = __xuiToolbarInit;
	tDesc.onDestroy = __xuiToolbarDestroy;
	tDesc.onContentMeasure = __xuiToolbarContentMeasure;
	tDesc.onCacheRender = __xuiToolbarCacheRender;
	__xuiToolbarDefaultLayout(&tDesc.tLayout);
	__xuiToolbarDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) return NULL;
	__xuiToolbarRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiToolbarCreate(xui_context pContext, xui_widget* ppWidget, const xui_toolbar_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( !xuiInternalContextIsValid(pContext) || (ppWidget == NULL) || !__xuiToolbarDescValid(pDesc) ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppWidget = NULL;
	pType = xuiToolbarGetType(pContext);
	if ( pType == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiToolbarSetSelect(xui_widget pWidget, xui_toolbar_select_proc onSelect, void* pUser)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onSelect = onSelect;
	pData->pSelectUser = pUser;
	return XUI_OK;
}

XUI_API int xuiToolbarSetOverflow(xui_widget pWidget, int bEnabled, float fButtonSize, xui_toolbar_overflow_proc onOverflow, void* pUser)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( fButtonSize < 1.0f ) fButtonSize = 1.0f;
	pData->bOverflowEnabled = bEnabled ? 1 : 0;
	pData->tMetrics.fOverflowSize = fButtonSize;
	pData->onOverflow = onOverflow;
	pData->pOverflowUser = pUser;
	if ( !pData->bOverflowEnabled ) {
		pData->iOverflowFirst = -1;
		pData->iOverflowCount = 0;
		memset(&pData->tOverflowRect, 0, sizeof(pData->tOverflowRect));
	}
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiToolbarIsOverflowEnabled(xui_widget pWidget)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	return (pData != NULL) ? pData->bOverflowEnabled : 0;
}

XUI_API int xuiToolbarSetItems(xui_widget pWidget, const xui_toolbar_item_t* pItems, int iCount)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	int i;

	if ( (pData == NULL) || (iCount < 0) || (iCount > XUI_TOOLBAR_ITEM_CAPACITY) || ((iCount > 0) && (pItems == NULL)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(pData->arrItems, 0, sizeof(pData->arrItems));
	for ( i = 0; i < iCount; i++ ) {
		pData->arrItems[i] = pItems[i];
		if ( !__xuiToolbarItemTypeValid(pData->arrItems[i].iType) ) pData->arrItems[i].iType = XUI_TOOLBAR_ITEM_BUTTON;
		if ( pData->arrItems[i].sText == NULL ) pData->arrItems[i].sText = "";
		if ( pData->arrItems[i].sTooltip == NULL ) pData->arrItems[i].sTooltip = "";
		if ( pData->arrItems[i].iGroup < 0 ) pData->arrItems[i].iGroup = 0;
		if ( pData->arrItems[i].iType == XUI_TOOLBAR_ITEM_SEPARATOR ) {
			pData->arrItems[i].iState = 0;
		}
	}
	pData->iItemCount = iCount;
	pData->iHover = -1;
	pData->iActive = -1;
	pData->bOverflowActive = 0;
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiToolbarAddItem(xui_widget pWidget, const char* sText, int iType, int iValue)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	xui_toolbar_item_t* pItem;

	if ( (pData == NULL) || (pData->iItemCount >= XUI_TOOLBAR_ITEM_CAPACITY) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !__xuiToolbarItemTypeValid(iType) ) iType = XUI_TOOLBAR_ITEM_BUTTON;
	pItem = &pData->arrItems[pData->iItemCount++];
	memset(pItem, 0, sizeof(*pItem));
	pItem->sText = (sText != NULL) ? sText : "";
	pItem->sTooltip = "";
	pItem->iType = iType;
	pItem->iValue = iValue;
	pItem->iState = (iType == XUI_TOOLBAR_ITEM_SEPARATOR) ? 0 : XUI_TOOLBAR_ITEM_ENABLED;
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiToolbarAddSeparator(xui_widget pWidget)
{
	return xuiToolbarAddItem(pWidget, "", XUI_TOOLBAR_ITEM_SEPARATOR, 0);
}

XUI_API int xuiToolbarClear(xui_widget pWidget)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(pData->arrItems, 0, sizeof(pData->arrItems));
	pData->iItemCount = 0;
	pData->iHover = -1;
	pData->iActive = -1;
	pData->iOverflowFirst = -1;
	pData->iOverflowCount = 0;
	pData->bOverflowActive = 0;
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiToolbarGetItemCount(xui_widget pWidget)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	return (pData != NULL) ? pData->iItemCount : 0;
}

XUI_API const xui_toolbar_item_t* xuiToolbarGetItem(xui_widget pWidget, int iIndex)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return NULL;
	return &pData->arrItems[iIndex];
}

XUI_API xui_rect_t xuiToolbarGetItemRect(xui_widget pWidget, int iIndex)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	xui_toolbar_resolved_t tResolved;
	xui_rect_t tEmpty;

	memset(&tEmpty, 0, sizeof(tEmpty));
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return tEmpty;
	__xuiToolbarResolve(pWidget, pData, &tResolved);
	(void)__xuiToolbarLayoutItems(pWidget, pData, &tResolved);
	return pData->arrItems[iIndex].tRect;
}

XUI_API int xuiToolbarGetItemAt(xui_widget pWidget, float fX, float fY)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	xui_toolbar_resolved_t tResolved;
	if ( pData == NULL ) return -1;
	__xuiToolbarResolve(pWidget, pData, &tResolved);
	(void)__xuiToolbarLayoutItems(pWidget, pData, &tResolved);
	return __xuiToolbarIndexAt(pData, fX, fY);
}

XUI_API int xuiToolbarSetOrientation(xui_widget pWidget, int iOrientation)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	if ( (pData == NULL) || !__xuiToolbarOrientationValid(iOrientation) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->tMetrics.iOrientation = iOrientation;
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiToolbarGetOrientation(xui_widget pWidget)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	return (pData != NULL) ? pData->tMetrics.iOrientation : XUI_ORIENTATION_HORIZONTAL;
}

XUI_API int xuiToolbarSetItemSize(xui_widget pWidget, float fWidth, float fHeight, float fSeparatorSize)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( fWidth < 1.0f ) fWidth = 1.0f;
	if ( fHeight < 1.0f ) fHeight = 1.0f;
	if ( fSeparatorSize < 1.0f ) fSeparatorSize = 1.0f;
	pData->tMetrics.fItemWidth = fWidth;
	pData->tMetrics.fItemHeight = fHeight;
	pData->tMetrics.fSeparatorSize = fSeparatorSize;
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiToolbarSetItemGroup(xui_widget pWidget, int iIndex, int iGroup)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrItems[iIndex].iGroup = (iGroup < 0) ? 0 : iGroup;
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiToolbarGetItemGroup(xui_widget pWidget, int iIndex)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return 0;
	return pData->arrItems[iIndex].iGroup;
}

XUI_API int xuiToolbarSetItemTooltip(xui_widget pWidget, int iIndex, const char* sText)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrItems[iIndex].sTooltip = (sText != NULL) ? sText : "";
	pData->iChangeCount++;
	return XUI_OK;
}

XUI_API const char* xuiToolbarGetItemTooltip(xui_widget pWidget, int iIndex)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return "";
	return (pData->arrItems[iIndex].sTooltip != NULL) ? pData->arrItems[iIndex].sTooltip : "";
}

XUI_API const char* xuiToolbarGetHoverTooltip(xui_widget pWidget)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	if ( (pData == NULL) || (pData->iHover < 0) || (pData->iHover >= pData->iItemCount) ) return "";
	return xuiToolbarGetItemTooltip(pWidget, pData->iHover);
}

XUI_API int xuiToolbarSetItemEnabled(xui_widget pWidget, int iIndex, int bEnabled)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->arrItems[iIndex].iType == XUI_TOOLBAR_ITEM_SEPARATOR ) return XUI_OK;
	if ( bEnabled ) pData->arrItems[iIndex].iState |= XUI_TOOLBAR_ITEM_ENABLED;
	else {
		pData->arrItems[iIndex].iState &= ~XUI_TOOLBAR_ITEM_ENABLED;
		if ( pData->iHover == iIndex ) pData->iHover = -1;
		if ( pData->iActive == iIndex ) pData->iActive = -1;
	}
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiToolbarIsItemEnabled(xui_widget pWidget, int iIndex)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return 0;
	return __xuiToolbarItemInteractive(&pData->arrItems[iIndex]);
}

XUI_API int xuiToolbarSetItemChecked(xui_widget pWidget, int iIndex, int bChecked)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( bChecked ) pData->arrItems[iIndex].iState |= XUI_TOOLBAR_ITEM_CHECKED;
	else pData->arrItems[iIndex].iState &= ~XUI_TOOLBAR_ITEM_CHECKED;
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiToolbarGetItemChecked(xui_widget pWidget, int iIndex)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return 0;
	return (pData->arrItems[iIndex].iState & XUI_TOOLBAR_ITEM_CHECKED) != 0;
}

XUI_API int xuiToolbarSetItemIcon(xui_widget pWidget, int iIndex, xui_surface pIcon, xui_rect_t tSrc)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) || (tSrc.fW < 0.0f) || (tSrc.fH < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrItems[iIndex].pIcon = pIcon;
	pData->arrItems[iIndex].tIconSrc = tSrc;
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiToolbarGetHoverIndex(xui_widget pWidget)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	return (pData != NULL) ? pData->iHover : -1;
}

XUI_API int xuiToolbarSetHoverIndex(xui_widget pWidget, int iIndex)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiToolbarSetHover(pWidget, pData, iIndex);
}

XUI_API int xuiToolbarGetActiveIndex(xui_widget pWidget)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	return (pData != NULL) ? pData->iActive : -1;
}

XUI_API int xuiToolbarGetOverflowFirst(xui_widget pWidget)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	xui_toolbar_resolved_t tResolved;
	if ( pData == NULL ) return -1;
	__xuiToolbarResolve(pWidget, pData, &tResolved);
	(void)__xuiToolbarLayoutItems(pWidget, pData, &tResolved);
	return pData->iOverflowFirst;
}

XUI_API int xuiToolbarGetOverflowCount(xui_widget pWidget)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	xui_toolbar_resolved_t tResolved;
	if ( pData == NULL ) return 0;
	__xuiToolbarResolve(pWidget, pData, &tResolved);
	(void)__xuiToolbarLayoutItems(pWidget, pData, &tResolved);
	return pData->iOverflowCount;
}

XUI_API xui_rect_t xuiToolbarGetOverflowRect(xui_widget pWidget)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	xui_toolbar_resolved_t tResolved;
	xui_rect_t tEmpty;
	memset(&tEmpty, 0, sizeof(tEmpty));
	if ( pData == NULL ) return tEmpty;
	__xuiToolbarResolve(pWidget, pData, &tResolved);
	(void)__xuiToolbarLayoutItems(pWidget, pData, &tResolved);
	return pData->tOverflowRect;
}

XUI_API int xuiToolbarSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pFont = pFont;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_font xuiToolbarGetFont(xui_widget pWidget)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API int xuiToolbarSetMetrics(xui_widget pWidget, const xui_toolbar_metrics_t* pMetrics)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	if ( (pData == NULL) || !__xuiToolbarMetricsValid(pMetrics) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->tMetrics = *pMetrics;
	pData->tMetrics.iSize = sizeof(pData->tMetrics);
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiToolbarGetMetrics(xui_widget pWidget, xui_toolbar_metrics_t* pMetrics)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	if ( (pData == NULL) || (pMetrics == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pMetrics = pData->tMetrics;
	pMetrics->iSize = sizeof(*pMetrics);
	return XUI_OK;
}

XUI_API int xuiToolbarSetColors(xui_widget pWidget, const xui_toolbar_colors_t* pColors)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	if ( (pData == NULL) || !__xuiToolbarColorsValid(pColors) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->tColors = *pColors;
	pData->tColors.iSize = sizeof(pData->tColors);
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiToolbarGetColors(xui_widget pWidget, xui_toolbar_colors_t* pColors)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	if ( (pData == NULL) || (pColors == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pColors = pData->tColors;
	pColors->iSize = sizeof(*pColors);
	return XUI_OK;
}

XUI_API uint32_t xuiToolbarGetState(xui_widget pWidget)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	uint32_t iState;
	if ( pData == NULL ) return XUI_WIDGET_STATE_DISABLED;
	iState = xuiWidgetGetInputState(pWidget);
	if ( pData->bOverflowActive ) iState |= XUI_TOOLBAR_STATE_OVERFLOW_ACTIVE;
	return iState;
}

XUI_API int xuiToolbarGetSelectCount(xui_widget pWidget)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	return (pData != NULL) ? pData->iSelectCount : 0;
}

XUI_API int xuiToolbarGetOverflowSelectCount(xui_widget pWidget)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	return (pData != NULL) ? pData->iOverflowSelectCount : 0;
}

XUI_API int xuiToolbarGetChangeCount(xui_widget pWidget)
{
	xui_toolbar_data_t* pData = __xuiToolbarGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}
