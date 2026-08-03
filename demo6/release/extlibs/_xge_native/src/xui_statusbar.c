#include "xui_internal.h"

#include <string.h>

typedef struct xui_statusbar_data_t {
	xui_statusbar_item_t arrItems[XUI_STATUSBAR_ITEM_CAPACITY];
	xui_statusbar_metrics_t tMetrics;
	xui_statusbar_colors_t tColors;
	xui_statusbar_select_proc onSelect;
	void* pSelectUser;
	xui_font pFont;
	int iItemCount;
	int iHover;
	int iActive;
	int iSelectCount;
	int iChangeCount;
} xui_statusbar_data_t;

typedef struct xui_statusbar_resolved_t {
	xui_statusbar_metrics_t tMetrics;
	xui_statusbar_colors_t tColors;
	xui_font pFont;
} xui_statusbar_resolved_t;

static xui_statusbar_data_t* __xuiStatusBarGetData(xui_widget pWidget);

static int __xuiStatusBarAlpha(uint32_t iColor)
{
	return (int)(iColor & 0xffu);
}

static uint32_t __xuiStatusBarColorAlpha(uint32_t iColor, uint32_t iAlpha)
{
	if ( iAlpha > 255u ) iAlpha = 255u;
	return (iColor & 0xffffff00u) | (iAlpha & 0xffu);
}

static float __xuiStatusBarMax(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static int __xuiStatusBarSectionValid(int iSection)
{
	return (iSection == XUI_STATUSBAR_SECTION_LEFT) ||
	       (iSection == XUI_STATUSBAR_SECTION_CENTER) ||
	       (iSection == XUI_STATUSBAR_SECTION_RIGHT);
}

static int __xuiStatusBarNormalizeSection(int iSection)
{
	return __xuiStatusBarSectionValid(iSection) ? iSection : XUI_STATUSBAR_SECTION_LEFT;
}

static int __xuiStatusBarItemTypeValid(int iType)
{
	return (iType == XUI_STATUSBAR_ITEM_TEXT) ||
	       (iType == XUI_STATUSBAR_ITEM_PROGRESS) ||
	       (iType == XUI_STATUSBAR_ITEM_SPACER);
}

static int __xuiStatusBarItemInteractive(const xui_statusbar_item_t* pItem)
{
	return (pItem != NULL) &&
	       (pItem->iType != XUI_STATUSBAR_ITEM_SPACER) &&
	       ((pItem->iState & XUI_STATUSBAR_ITEM_ENABLED) != 0u) &&
	       ((pItem->iState & XUI_STATUSBAR_ITEM_CLICKABLE) != 0u);
}

static int __xuiStatusBarRectContains(xui_rect_t tRect, float fX, float fY)
{
	return (fX >= tRect.fX) && (fY >= tRect.fY) && (fX < tRect.fX + tRect.fW) && (fY < tRect.fY + tRect.fH);
}

static xui_rect_t __xuiStatusBarInsetRect(xui_rect_t tRect, float fLeft, float fTop, float fRight, float fBottom)
{
	tRect.fX += fLeft;
	tRect.fY += fTop;
	tRect.fW -= fLeft + fRight;
	tRect.fH -= fTop + fBottom;
	if ( tRect.fW < 0.0f ) tRect.fW = 0.0f;
	if ( tRect.fH < 0.0f ) tRect.fH = 0.0f;
	return tRect;
}

static int __xuiStatusBarDescValid(const xui_statusbar_desc_t* pDesc)
{
	if ( pDesc == NULL ) return 1;
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) return 0;
	if ( (pDesc->iItemCount < 0) || (pDesc->iItemCount > XUI_STATUSBAR_ITEM_CAPACITY) ) return 0;
	if ( (pDesc->iItemCount > 0) && (pDesc->pItems == NULL) ) return 0;
	return 1;
}

static int __xuiStatusBarMetricsValid(const xui_statusbar_metrics_t* pMetrics)
{
	if ( pMetrics == NULL ) return 0;
	if ( (pMetrics->iSize != 0) && (pMetrics->iSize < sizeof(*pMetrics)) ) return 0;
	if ( (pMetrics->fHeight <= 0.0f) ||
	     (pMetrics->fPaddingX < 0.0f) ||
	     (pMetrics->fPaddingY < 0.0f) ||
	     (pMetrics->fGap < 0.0f) ||
	     (pMetrics->fItemPaddingX < 0.0f) ||
	     (pMetrics->fItemPaddingY < 0.0f) ||
	     (pMetrics->fProgressHeight <= 0.0f) ||
	     (pMetrics->fBorderWidth < 0.0f) ||
	     (pMetrics->fTopBorderWidth < 0.0f) ) {
		return 0;
	}
	return 1;
}

static int __xuiStatusBarColorsValid(const xui_statusbar_colors_t* pColors)
{
	if ( pColors == NULL ) return 0;
	return (pColors->iSize == 0) || (pColors->iSize >= sizeof(*pColors));
}

static void __xuiStatusBarDefaultMetrics(xui_statusbar_metrics_t* pMetrics)
{
	memset(pMetrics, 0, sizeof(*pMetrics));
	pMetrics->iSize = sizeof(*pMetrics);
	pMetrics->fHeight = 26.0f;
	pMetrics->fPaddingX = 8.0f;
	pMetrics->fPaddingY = 2.0f;
	pMetrics->fGap = 6.0f;
	pMetrics->fItemPaddingX = 8.0f;
	pMetrics->fItemPaddingY = 3.0f;
	pMetrics->fProgressHeight = 9.0f;
	pMetrics->fBorderWidth = 1.0f;
	pMetrics->fTopBorderWidth = 1.0f;
}

static void __xuiStatusBarDefaultColors(xui_statusbar_colors_t* pColors)
{
	memset(pColors, 0, sizeof(*pColors));
	pColors->iSize = sizeof(*pColors);
	pColors->iBackgroundColor = XUI_COLOR_RGBA(241, 247, 253, 255);
	pColors->iBorderColor = XUI_COLOR_RGBA(192, 210, 228, 255);
	pColors->iHighlightColor = XUI_COLOR_RGBA(255, 255, 255, 130);
	pColors->iItemColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	pColors->iHoverColor = XUI_COLOR_RGBA(225, 239, 251, 255);
	pColors->iActiveColor = XUI_COLOR_RGBA(47, 128, 214, 255);
	pColors->iFocusColor = XUI_COLOR_RGBA(47, 128, 214, 255);
	pColors->iTextColor = XUI_COLOR_RGBA(38, 59, 86, 255);
	pColors->iDisabledTextColor = XUI_COLOR_RGBA(145, 157, 170, 210);
	pColors->iProgressTrackColor = XUI_COLOR_RGBA(214, 231, 246, 255);
	pColors->iProgressFillColor = XUI_COLOR_RGBA(47, 128, 214, 255);
}

static xui_vec2_t __xuiStatusBarMeasureText(xui_widget pWidget, xui_font pFont, const char* sText)
{
	xui_vec2_t tSize;
	xui_proxy pProxy;

	tSize.fX = 0.0f;
	tSize.fY = 16.0f;
	if ( (sText == NULL) || (sText[0] == '\0') ) return tSize;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( (pProxy != NULL) && (pProxy->textMeasure != NULL) && (pFont != NULL) &&
	     (pProxy->textMeasure(pProxy, pFont, sText, &tSize) == XUI_OK) &&
	     (tSize.fX >= 0.0f) && (tSize.fY >= 0.0f) ) {
		return tSize;
	}
	tSize.fX = (float)strlen(sText) * 7.0f;
	tSize.fY = 16.0f;
	return tSize;
}

static int __xuiStatusBarStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
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

static int __xuiStatusBarStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
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

static xui_font __xuiStatusBarStyleFont(xui_widget pWidget, xui_font pBaseFont)
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

static void __xuiStatusBarResolve(xui_widget pWidget, const xui_statusbar_data_t* pData, xui_statusbar_resolved_t* pOut)
{
	memset(pOut, 0, sizeof(*pOut));
	pOut->tMetrics = pData->tMetrics;
	pOut->tMetrics.iSize = sizeof(pOut->tMetrics);
	pOut->tColors = pData->tColors;
	pOut->tColors.iSize = sizeof(pOut->tColors);
	pOut->pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	(void)__xuiStatusBarStyleFloat(pWidget, "statusbar.height", &pOut->tMetrics.fHeight);
	(void)__xuiStatusBarStyleFloat(pWidget, "statusbar.padding.x", &pOut->tMetrics.fPaddingX);
	(void)__xuiStatusBarStyleFloat(pWidget, "statusbar.padding.y", &pOut->tMetrics.fPaddingY);
	(void)__xuiStatusBarStyleFloat(pWidget, "statusbar.gap", &pOut->tMetrics.fGap);
	(void)__xuiStatusBarStyleFloat(pWidget, "statusbar.item.padding.x", &pOut->tMetrics.fItemPaddingX);
	(void)__xuiStatusBarStyleFloat(pWidget, "statusbar.item.padding.y", &pOut->tMetrics.fItemPaddingY);
	(void)__xuiStatusBarStyleFloat(pWidget, "statusbar.progress.height", &pOut->tMetrics.fProgressHeight);
	(void)__xuiStatusBarStyleFloat(pWidget, "statusbar.border.width", &pOut->tMetrics.fBorderWidth);
	(void)__xuiStatusBarStyleFloat(pWidget, "statusbar.top_border.width", &pOut->tMetrics.fTopBorderWidth);
	(void)__xuiStatusBarStyleColor(pWidget, "statusbar.background.color", &pOut->tColors.iBackgroundColor);
	(void)__xuiStatusBarStyleColor(pWidget, "statusbar.border.color", &pOut->tColors.iBorderColor);
	(void)__xuiStatusBarStyleColor(pWidget, "statusbar.highlight.color", &pOut->tColors.iHighlightColor);
	(void)__xuiStatusBarStyleColor(pWidget, "statusbar.item.color", &pOut->tColors.iItemColor);
	(void)__xuiStatusBarStyleColor(pWidget, "statusbar.item.hover_color", &pOut->tColors.iHoverColor);
	(void)__xuiStatusBarStyleColor(pWidget, "statusbar.item.active_color", &pOut->tColors.iActiveColor);
	(void)__xuiStatusBarStyleColor(pWidget, "statusbar.focus.color", &pOut->tColors.iFocusColor);
	(void)__xuiStatusBarStyleColor(pWidget, "statusbar.text.color", &pOut->tColors.iTextColor);
	(void)__xuiStatusBarStyleColor(pWidget, "statusbar.text.disabled_color", &pOut->tColors.iDisabledTextColor);
	(void)__xuiStatusBarStyleColor(pWidget, "statusbar.progress.track_color", &pOut->tColors.iProgressTrackColor);
	(void)__xuiStatusBarStyleColor(pWidget, "statusbar.progress.fill_color", &pOut->tColors.iProgressFillColor);
	pOut->pFont = __xuiStatusBarStyleFont(pWidget, pOut->pFont);
	if ( pOut->tMetrics.fHeight <= 0.0f ) pOut->tMetrics.fHeight = 1.0f;
	if ( pOut->tMetrics.fProgressHeight <= 0.0f ) pOut->tMetrics.fProgressHeight = 1.0f;
}

static float __xuiStatusBarItemWidth(xui_widget pWidget, const xui_statusbar_item_t* pItem, const xui_statusbar_resolved_t* pResolved)
{
	xui_vec2_t tSize;
	float fWidth;

	if ( (pItem == NULL) || (pResolved == NULL) ) return 0.0f;
	if ( pItem->fWidth > 0.0f ) return pItem->fWidth;
	if ( (pItem->iType == XUI_STATUSBAR_ITEM_SPACER) && (pItem->fFlex > 0.0f) ) return 0.0f;
	if ( pItem->iType == XUI_STATUSBAR_ITEM_PROGRESS ) return 92.0f;
	if ( pItem->iType == XUI_STATUSBAR_ITEM_SPACER ) return 12.0f;
	fWidth = 36.0f;
	if ( (pItem->sText != NULL) && (pItem->sText[0] != '\0') ) {
		tSize = __xuiStatusBarMeasureText(pWidget, pResolved->pFont, pItem->sText);
		fWidth = tSize.fX + pResolved->tMetrics.fItemPaddingX * 2.0f;
	}
	return __xuiStatusBarMax(fWidth, 12.0f);
}

static float __xuiStatusBarMeasureSection(xui_widget pWidget, xui_statusbar_data_t* pData, const xui_statusbar_resolved_t* pResolved, int iSection)
{
	float fTotal;
	int iCount;
	int i;

	fTotal = 0.0f;
	iCount = 0;
	for ( i = 0; i < pData->iItemCount; i++ ) {
		if ( pData->arrItems[i].iSection != iSection ) continue;
		if ( iCount > 0 ) fTotal += pResolved->tMetrics.fGap;
		fTotal += __xuiStatusBarItemWidth(pWidget, &pData->arrItems[i], pResolved);
		iCount++;
	}
	return fTotal;
}

static int __xuiStatusBarLayoutSection(xui_widget pWidget, xui_statusbar_data_t* pData, const xui_statusbar_resolved_t* pResolved, int iSection, float fStart, float fEnd)
{
	xui_rect_t tRect;
	float fTotal;
	float fFixed;
	float fFlexTotal;
	float fFlexExtra;
	float fCursor;
	float fWidth;
	float fAvailable;
	int iCount;
	int i;

	if ( (pWidget == NULL) || (pData == NULL) || (pResolved == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	fAvailable = fEnd - fStart;
	if ( fAvailable < 0.0f ) fAvailable = 0.0f;
	fFixed = 0.0f;
	fFlexTotal = 0.0f;
	iCount = 0;
	for ( i = 0; i < pData->iItemCount; i++ ) {
		if ( pData->arrItems[i].iSection != iSection ) continue;
		if ( iCount > 0 ) fFixed += pResolved->tMetrics.fGap;
		if ( (pData->arrItems[i].iType == XUI_STATUSBAR_ITEM_SPACER) && (pData->arrItems[i].fFlex > 0.0f) ) {
			fFlexTotal += pData->arrItems[i].fFlex;
		} else {
			fFixed += __xuiStatusBarItemWidth(pWidget, &pData->arrItems[i], pResolved);
		}
		iCount++;
	}
	fTotal = fFixed;
	fFlexExtra = 0.0f;
	if ( (fFlexTotal > 0.0f) && (fAvailable > fFixed) ) {
		fFlexExtra = fAvailable - fFixed;
		fTotal = fAvailable;
	}
	if ( iSection == XUI_STATUSBAR_SECTION_RIGHT ) {
		fCursor = fEnd - fTotal;
	} else if ( iSection == XUI_STATUSBAR_SECTION_CENTER ) {
		fCursor = (fStart + fEnd - fTotal) * 0.5f;
	} else {
		fCursor = fStart;
	}
	if ( fCursor < fStart ) fCursor = fStart;
	tRect = xuiWidgetGetRect(pWidget);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	tRect = __xuiStatusBarInsetRect(tRect, 0.0f, pResolved->tMetrics.fPaddingY, 0.0f, pResolved->tMetrics.fPaddingY);
	for ( i = 0; i < pData->iItemCount; i++ ) {
		if ( pData->arrItems[i].iSection != iSection ) continue;
		if ( (pData->arrItems[i].iType == XUI_STATUSBAR_ITEM_SPACER) && (pData->arrItems[i].fFlex > 0.0f) && (fFlexTotal > 0.0f) ) {
			fWidth = fFlexExtra * (pData->arrItems[i].fFlex / fFlexTotal);
		} else {
			fWidth = __xuiStatusBarItemWidth(pWidget, &pData->arrItems[i], pResolved);
		}
		pData->arrItems[i].tRect = tRect;
		pData->arrItems[i].tRect.fX = fCursor;
		pData->arrItems[i].tRect.fW = fWidth;
		if ( pData->arrItems[i].tRect.fX < fStart ) {
			pData->arrItems[i].tRect.fW -= fStart - pData->arrItems[i].tRect.fX;
			pData->arrItems[i].tRect.fX = fStart;
		}
		if ( (pData->arrItems[i].tRect.fX + pData->arrItems[i].tRect.fW) > fEnd ) {
			pData->arrItems[i].tRect.fW = fEnd - pData->arrItems[i].tRect.fX;
		}
		if ( pData->arrItems[i].tRect.fW < 0.0f ) pData->arrItems[i].tRect.fW = 0.0f;
		fCursor += fWidth + pResolved->tMetrics.fGap;
	}
	return XUI_OK;
}

static int __xuiStatusBarLayoutItems(xui_widget pWidget, xui_statusbar_data_t* pData, const xui_statusbar_resolved_t* pResolved)
{
	xui_rect_t tRect;
	float fStart;
	float fEnd;
	int i;

	if ( (pWidget == NULL) || (pData == NULL) || (pResolved == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < pData->iItemCount; i++ ) {
		memset(&pData->arrItems[i].tRect, 0, sizeof(pData->arrItems[i].tRect));
	}
	tRect = xuiWidgetGetRect(pWidget);
	fStart = pResolved->tMetrics.fPaddingX;
	fEnd = tRect.fW - pResolved->tMetrics.fPaddingX;
	if ( fEnd < fStart ) fEnd = fStart;
	(void)__xuiStatusBarLayoutSection(pWidget, pData, pResolved, XUI_STATUSBAR_SECTION_LEFT, fStart, fEnd);
	(void)__xuiStatusBarLayoutSection(pWidget, pData, pResolved, XUI_STATUSBAR_SECTION_CENTER, fStart, fEnd);
	(void)__xuiStatusBarLayoutSection(pWidget, pData, pResolved, XUI_STATUSBAR_SECTION_RIGHT, fStart, fEnd);
	return XUI_OK;
}

static int __xuiStatusBarIndexAt(xui_statusbar_data_t* pData, float fX, float fY)
{
	int i;

	if ( pData == NULL ) return -1;
	for ( i = 0; i < pData->iItemCount; i++ ) {
		if ( __xuiStatusBarItemInteractive(&pData->arrItems[i]) &&
		     __xuiStatusBarRectContains(pData->arrItems[i].tRect, fX, fY) ) {
			return i;
		}
	}
	return -1;
}

static int __xuiStatusBarFirstInteractive(xui_statusbar_data_t* pData)
{
	int i;

	if ( pData == NULL ) return -1;
	for ( i = 0; i < pData->iItemCount; i++ ) {
		if ( __xuiStatusBarItemInteractive(&pData->arrItems[i]) ) return i;
	}
	return -1;
}

static int __xuiStatusBarInvalidateItem(xui_widget pWidget, xui_statusbar_data_t* pData, int iIndex)
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

static int __xuiStatusBarSetHover(xui_widget pWidget, xui_statusbar_data_t* pData, int iIndex)
{
	int iOldHover;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( (iIndex < 0) || (iIndex >= pData->iItemCount) || !__xuiStatusBarItemInteractive(&pData->arrItems[iIndex]) ) {
		iIndex = -1;
	}
	if ( pData->iHover == iIndex ) return XUI_OK;
	iOldHover = pData->iHover;
	pData->iHover = iIndex;
	iRet = __xuiStatusBarInvalidateItem(pWidget, pData, iOldHover);
	if ( iRet == XUI_OK ) {
		iRet = __xuiStatusBarInvalidateItem(pWidget, pData, iIndex);
	}
	return iRet;
}

static int __xuiStatusBarSelect(xui_widget pWidget, xui_statusbar_data_t* pData, int iIndex)
{
	if ( (pWidget == NULL) || (pData == NULL) ||
	     (iIndex < 0) || (iIndex >= pData->iItemCount) ||
	     !__xuiStatusBarItemInteractive(&pData->arrItems[iIndex]) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->iSelectCount++;
	pData->iChangeCount++;
	if ( pData->onSelect != NULL ) {
		pData->onSelect(pWidget, iIndex, pData->arrItems[iIndex].iValue, pData->pSelectUser);
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiStatusBarDrawRectFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( __xuiStatusBarAlpha(iColor) == 0 ) return XUI_OK;
	return pProxy->drawRectFill(pProxy, pDraw, tRect, iColor);
}

static int __xuiStatusBarDrawRectStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	if ( (fWidth <= 0.0f) || (__xuiStatusBarAlpha(iColor) == 0) ) return XUI_OK;
	return pProxy->drawRectStroke(pProxy, pDraw, tRect, fWidth, iColor);
}

static int __xuiStatusBarDrawProgress(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, const xui_statusbar_item_t* pItem, const xui_statusbar_resolved_t* pResolved)
{
	xui_rect_t tTrack;
	xui_rect_t tFill;
	float fRange;
	float fRatio;
	int iRet;

	tTrack = __xuiStatusBarInsetRect(tRect, pResolved->tMetrics.fItemPaddingX, 0.0f, pResolved->tMetrics.fItemPaddingX, 0.0f);
	tTrack.fY += (tTrack.fH - pResolved->tMetrics.fProgressHeight) * 0.5f;
	tTrack.fH = pResolved->tMetrics.fProgressHeight;
	if ( tTrack.fW <= 0.0f || tTrack.fH <= 0.0f ) return XUI_OK;
	iRet = __xuiStatusBarDrawRectFill(pProxy, pDraw, xuiInternalSnapRect(tTrack), pResolved->tColors.iProgressTrackColor);
	if ( iRet != XUI_OK ) return iRet;
	fRange = pItem->fMax - pItem->fMin;
	fRatio = (fRange > 0.0f) ? ((pItem->fValue - pItem->fMin) / fRange) : 0.0f;
	if ( fRatio < 0.0f ) fRatio = 0.0f;
	if ( fRatio > 1.0f ) fRatio = 1.0f;
	tFill = __xuiStatusBarInsetRect(tTrack, 1.0f, 1.0f, 1.0f, 1.0f);
	tFill.fW *= fRatio;
	if ( tFill.fW > 0.0f ) {
		iRet = __xuiStatusBarDrawRectFill(pProxy, pDraw, xuiInternalSnapRect(tFill), pResolved->tColors.iProgressFillColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	return __xuiStatusBarDrawRectStroke(pProxy, pDraw, xuiInternalSnapRect(tTrack), pResolved->tMetrics.fBorderWidth, __xuiStatusBarColorAlpha(pResolved->tColors.iBorderColor, 130));
}

static int __xuiStatusBarCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_statusbar_data_t* pData;
	xui_statusbar_resolved_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tRect;
	xui_rect_t tLine;
	xui_rect_t tCell;
	xui_rect_t tText;
	uint32_t iFill;
	uint32_t iBorder;
	uint32_t iText;
	int bPressed;
	int iRet;
	int i;

	(void)iStateId;
	(void)pUser;
	pData = __xuiStatusBarGetData(pWidget);
	if ( (pData == NULL) || (pDraw == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiStatusBarResolve(pWidget, pData, &tResolved);
	(void)__xuiStatusBarLayoutItems(pWidget, pData, &tResolved);
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	tRect = xuiWidgetGetRect(pWidget);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	iRet = __xuiStatusBarDrawRectFill(pProxy, pDraw, xuiInternalSnapRect(tRect), tResolved.tColors.iBackgroundColor);
	if ( iRet != XUI_OK ) return iRet;
	if ( tResolved.tMetrics.fTopBorderWidth > 0.0f ) {
		tLine = (xui_rect_t){0.0f, 0.0f, tRect.fW, tResolved.tMetrics.fTopBorderWidth};
		iRet = __xuiStatusBarDrawRectFill(pProxy, pDraw, xuiInternalSnapRect(tLine), tResolved.tColors.iBorderColor);
		if ( iRet != XUI_OK ) return iRet;
		tLine.fY += tResolved.tMetrics.fTopBorderWidth;
		tLine.fH = 1.0f;
		iRet = __xuiStatusBarDrawRectFill(pProxy, pDraw, xuiInternalSnapRect(tLine), tResolved.tColors.iHighlightColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	for ( i = 0; i < pData->iItemCount; i++ ) {
		if ( (pData->arrItems[i].tRect.fW <= 0.0f) || (pData->arrItems[i].tRect.fH <= 0.0f) ||
		     (pData->arrItems[i].iType == XUI_STATUSBAR_ITEM_SPACER) ) {
			continue;
		}
		iFill = tResolved.tColors.iItemColor;
		iBorder = XUI_COLOR_RGBA(0, 0, 0, 0);
		iText = ((pData->arrItems[i].iState & XUI_STATUSBAR_ITEM_ENABLED) != 0u) ?
			tResolved.tColors.iTextColor : tResolved.tColors.iDisabledTextColor;
		bPressed = 0;
		if ( i == pData->iActive ) {
			iFill = tResolved.tColors.iActiveColor;
			iBorder = __xuiStatusBarColorAlpha(tResolved.tColors.iActiveColor, 235);
			iText = XUI_COLOR_RGBA(255, 255, 255, 255);
			bPressed = 1;
		} else if ( i == pData->iHover ) {
			iFill = tResolved.tColors.iHoverColor;
			iBorder = __xuiStatusBarColorAlpha(tResolved.tColors.iFocusColor, 160);
		}
		if ( (pData->arrItems[i].iState & XUI_STATUSBAR_ITEM_CLICKABLE) != 0u ) {
			tCell = __xuiStatusBarInsetRect(pData->arrItems[i].tRect, 1.0f, tResolved.tMetrics.fItemPaddingY, 1.0f, tResolved.tMetrics.fItemPaddingY);
			if ( bPressed ) tCell.fY += 1.0f;
			if ( (tCell.fW > 0.0f) && (tCell.fH > 0.0f) ) {
				iRet = __xuiStatusBarDrawRectFill(pProxy, pDraw, xuiInternalSnapRect(tCell), iFill);
				if ( iRet != XUI_OK ) return iRet;
				iRet = __xuiStatusBarDrawRectStroke(pProxy, pDraw, xuiInternalSnapRect(tCell), tResolved.tMetrics.fBorderWidth, iBorder);
				if ( iRet != XUI_OK ) return iRet;
			}
		}
		if ( pData->arrItems[i].iType == XUI_STATUSBAR_ITEM_PROGRESS ) {
			iRet = __xuiStatusBarDrawProgress(pProxy, pDraw, pData->arrItems[i].tRect, &pData->arrItems[i], &tResolved);
			if ( iRet != XUI_OK ) return iRet;
		} else if ( (pProxy->drawText != NULL) && (tResolved.pFont != NULL) &&
		            (pData->arrItems[i].sText != NULL) && (pData->arrItems[i].sText[0] != '\0') ) {
			tText = __xuiStatusBarInsetRect(pData->arrItems[i].tRect, tResolved.tMetrics.fItemPaddingX, 0.0f, tResolved.tMetrics.fItemPaddingX, 0.0f);
			if ( bPressed ) tText.fY += 1.0f;
			if ( tText.fW > 0.0f ) {
				iRet = pProxy->drawText(pProxy, pDraw, tResolved.pFont, pData->arrItems[i].sText, tText, iText, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
				if ( iRet != XUI_OK ) return iRet;
			}
		}
	}
	return XUI_OK;
}

static int __xuiStatusBarContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_statusbar_data_t* pData;
	xui_statusbar_resolved_t tResolved;
	float fLeft;
	float fCenter;
	float fRight;

	(void)tConstraint;
	(void)pUser;
	if ( (pWidget == NULL) || (pSize == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiStatusBarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiStatusBarResolve(pWidget, pData, &tResolved);
	fLeft = __xuiStatusBarMeasureSection(pWidget, pData, &tResolved, XUI_STATUSBAR_SECTION_LEFT);
	fCenter = __xuiStatusBarMeasureSection(pWidget, pData, &tResolved, XUI_STATUSBAR_SECTION_CENTER);
	fRight = __xuiStatusBarMeasureSection(pWidget, pData, &tResolved, XUI_STATUSBAR_SECTION_RIGHT);
	pSize->fX = tResolved.tMetrics.fPaddingX * 2.0f + fLeft + fCenter + fRight + tResolved.tMetrics.fGap * 2.0f;
	pSize->fY = tResolved.tMetrics.fHeight;
	return XUI_OK;
}

static int __xuiStatusBarEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_statusbar_data_t* pData;
	xui_statusbar_resolved_t tResolved;
	xui_context pContext;
	xui_rect_t tWorld;
	float fLocalX;
	float fLocalY;
	int iIndex;
	int bLeftButton;

	(void)pUser;
	if ( (pWidget == NULL) || (pEvent == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE ) return XUI_OK;
	pData = __xuiStatusBarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiStatusBarResolve(pWidget, pData, &tResolved);
	(void)__xuiStatusBarLayoutItems(pWidget, pData, &tResolved);
	pContext = xuiWidgetGetContext(pWidget);
	tWorld = xuiWidgetGetWorldRect(pWidget);
	fLocalX = pEvent->fX - tWorld.fX;
	fLocalY = pEvent->fY - tWorld.fY;
	iIndex = __xuiStatusBarIndexAt(pData, fLocalX, fLocalY);
	bLeftButton = (pEvent->iButton == 0) || (pEvent->iButton == XUI_POINTER_BUTTON_LEFT);
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_MOVE:
		(void)__xuiStatusBarSetHover(pWidget, pData, iIndex);
		return (pData->iActive >= 0) ? XUI_EVENT_DISPATCH_STOP : XUI_OK;
	case XUI_EVENT_POINTER_LEAVE:
		if ( pData->iActive < 0 ) {
			(void)__xuiStatusBarSetHover(pWidget, pData, -1);
		}
		return XUI_OK;
	case XUI_EVENT_POINTER_DOWN:
		if ( !bLeftButton || (iIndex < 0) ) return XUI_OK;
		(void)xuiSetFocusWidget(pContext, pWidget);
		(void)xuiSetPointerCapture(pContext, pWidget);
		pData->iHover = iIndex;
		pData->iActive = iIndex;
		(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_POINTER_UP:
		if ( pData->iActive < 0 ) return XUI_OK;
		if ( xuiGetPointerCapture(pContext) == pWidget ) {
			(void)xuiReleasePointerCapture(pContext, pWidget);
		}
		if ( iIndex == pData->iActive ) {
			(void)__xuiStatusBarSelect(pWidget, pData, iIndex);
		}
		pData->iHover = iIndex;
		pData->iActive = -1;
		(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		pData->iActive = -1;
		return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	case XUI_EVENT_KEY_DOWN:
		if ( xuiGetFocusWidget(pContext) != pWidget ) return XUI_OK;
		if ( (pEvent->iKey == XUI_KEY_ENTER) || (pEvent->iKey == XUI_KEY_SPACE) ) {
			iIndex = ((pData->iHover >= 0) && (pData->iHover < pData->iItemCount) && __xuiStatusBarItemInteractive(&pData->arrItems[pData->iHover])) ?
				pData->iHover : __xuiStatusBarFirstInteractive(pData);
			if ( iIndex >= 0 ) {
				pData->iHover = iIndex;
				(void)__xuiStatusBarSelect(pWidget, pData, iIndex);
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

static void __xuiStatusBarDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_MANUAL;
	pLayout->iWidthMode = XUI_SIZE_FILL;
	pLayout->iHeightMode = XUI_SIZE_CONTENT;
	pLayout->iFlowMode = XUI_FLOW_BLOCK;
	pLayout->iDock = XUI_DOCK_BOTTOM;
	pLayout->iOverflow = XUI_OVERFLOW_VISIBLE;
	pLayout->iAlignX = XUI_ALIGN_STRETCH;
	pLayout->iAlignY = XUI_ALIGN_END;
	pLayout->iTableRowSpan = 1;
	pLayout->iTableColumnSpan = 1;
	pLayout->iGridColumnCount = 1;
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fShrink = 1.0f;
}

static void __xuiStatusBarDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE | XUI_CACHE_UPDATE_ALL_STATES;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiStatusBarInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiStatusBarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiStatusBarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiStatusBarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiStatusBarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiStatusBarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiStatusBarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiStatusBarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiStatusBarEvent, NULL);
	return iRet;
}

static int __xuiStatusBarInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_statusbar_data_t* pData;
	const xui_statusbar_desc_t* pDesc;
	int iRet;

	(void)pUser;
	if ( (pWidget == NULL) || (pTypeData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = (xui_statusbar_data_t*)pTypeData;
	pDesc = (const xui_statusbar_desc_t*)pCreateData;
	memset(pData, 0, sizeof(*pData));
	__xuiStatusBarDefaultMetrics(&pData->tMetrics);
	__xuiStatusBarDefaultColors(&pData->tColors);
	pData->iHover = -1;
	pData->iActive = -1;
	pData->pFont = xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	if ( (pDesc != NULL) && (pDesc->pFont != NULL) ) pData->pFont = pDesc->pFont;
	if ( (pDesc != NULL) && pDesc->bHasMetrics ) {
		if ( !__xuiStatusBarMetricsValid(&pDesc->tMetrics) ) return XUI_ERROR_INVALID_ARGUMENT;
		pData->tMetrics = pDesc->tMetrics;
		pData->tMetrics.iSize = sizeof(pData->tMetrics);
	}
	if ( (pDesc != NULL) && pDesc->bHasColors ) {
		if ( !__xuiStatusBarColorsValid(&pDesc->tColors) ) return XUI_ERROR_INVALID_ARGUMENT;
		pData->tColors = pDesc->tColors;
		pData->tColors.iSize = sizeof(pData->tColors);
	}
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 0);
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_VISIBLE);
	iRet = __xuiStatusBarInitEvents(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	if ( (pDesc != NULL) && (pDesc->iItemCount > 0) ) {
		return xuiStatusBarSetItems(pWidget, pDesc->pItems, pDesc->iItemCount);
	}
	return XUI_OK;
}

static void __xuiStatusBarDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_statusbar_data_t* pData;

	(void)pUser;
	pData = (xui_statusbar_data_t*)pTypeData;
	if ( pWidget != NULL ) {
		if ( xuiGetPointerCapture(xuiWidgetGetContext(pWidget)) == pWidget ) {
			(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
		}
	}
	if ( pData != NULL ) memset(pData, 0, sizeof(*pData));
}

static xui_statusbar_data_t* __xuiStatusBarGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) return NULL;
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "statusbar");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) return NULL;
	return (xui_statusbar_data_t*)xuiWidgetGetTypeData(pWidget);
}

static void __xuiStatusBarRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
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

static void __xuiStatusBarRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;
	uint32_t iLayoutDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	iLayoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiStatusBarRegisterStyleProperty(pContext, pType, "font.name", XUI_STYLE_VALUE_STRING, iLayoutDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiStatusBarRegisterStyleProperty(pContext, pType, "statusbar.background.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiStatusBarRegisterStyleProperty(pContext, pType, "statusbar.border.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiStatusBarRegisterStyleProperty(pContext, pType, "statusbar.highlight.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiStatusBarRegisterStyleProperty(pContext, pType, "statusbar.item.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiStatusBarRegisterStyleProperty(pContext, pType, "statusbar.item.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiStatusBarRegisterStyleProperty(pContext, pType, "statusbar.item.active_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiStatusBarRegisterStyleProperty(pContext, pType, "statusbar.focus.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiStatusBarRegisterStyleProperty(pContext, pType, "statusbar.text.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiStatusBarRegisterStyleProperty(pContext, pType, "statusbar.text.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiStatusBarRegisterStyleProperty(pContext, pType, "statusbar.progress.track_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiStatusBarRegisterStyleProperty(pContext, pType, "statusbar.progress.fill_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiStatusBarRegisterStyleProperty(pContext, pType, "statusbar.height", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiStatusBarRegisterStyleProperty(pContext, pType, "statusbar.padding.x", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiStatusBarRegisterStyleProperty(pContext, pType, "statusbar.padding.y", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiStatusBarRegisterStyleProperty(pContext, pType, "statusbar.gap", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiStatusBarRegisterStyleProperty(pContext, pType, "statusbar.item.padding.x", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiStatusBarRegisterStyleProperty(pContext, pType, "statusbar.item.padding.y", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiStatusBarRegisterStyleProperty(pContext, pType, "statusbar.progress.height", XUI_STYLE_VALUE_FLOAT, iPaintDirty, 0);
	__xuiStatusBarRegisterStyleProperty(pContext, pType, "statusbar.border.width", XUI_STYLE_VALUE_FLOAT, iPaintDirty, 0);
	__xuiStatusBarRegisterStyleProperty(pContext, pType, "statusbar.top_border.width", XUI_STYLE_VALUE_FLOAT, iPaintDirty, 0);
}

XUI_API xui_widget_type xuiStatusBarGetType(xui_context pContext)
{
	xui_widget_type pType;
	xui_widget_type_desc_t tDesc;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) return NULL;
	pType = xuiWidgetFindType(pContext, "statusbar");
	if ( pType != NULL ) return pType;
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "statusbar";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_statusbar_data_t);
	tDesc.onInit = __xuiStatusBarInit;
	tDesc.onDestroy = __xuiStatusBarDestroy;
	tDesc.onContentMeasure = __xuiStatusBarContentMeasure;
	tDesc.onCacheRender = __xuiStatusBarCacheRender;
	__xuiStatusBarDefaultLayout(&tDesc.tLayout);
	__xuiStatusBarDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) return NULL;
	__xuiStatusBarRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiStatusBarCreate(xui_context pContext, xui_widget* ppWidget, const xui_statusbar_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( !xuiInternalContextIsValid(pContext) || (ppWidget == NULL) || !__xuiStatusBarDescValid(pDesc) ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppWidget = NULL;
	pType = xuiStatusBarGetType(pContext);
	if ( pType == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiStatusBarSetSelect(xui_widget pWidget, xui_statusbar_select_proc onSelect, void* pUser)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onSelect = onSelect;
	pData->pSelectUser = pUser;
	return XUI_OK;
}

XUI_API int xuiStatusBarSetItems(xui_widget pWidget, const xui_statusbar_item_t* pItems, int iCount)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	int i;

	if ( (pData == NULL) || (iCount < 0) || (iCount > XUI_STATUSBAR_ITEM_CAPACITY) || ((iCount > 0) && (pItems == NULL)) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(pData->arrItems, 0, sizeof(pData->arrItems));
	pData->iItemCount = iCount;
	pData->iHover = -1;
	pData->iActive = -1;
	for ( i = 0; i < iCount; i++ ) {
		if ( !__xuiStatusBarItemTypeValid(pItems[i].iType) ) return XUI_ERROR_INVALID_ARGUMENT;
		pData->arrItems[i] = pItems[i];
		pData->arrItems[i].iSection = __xuiStatusBarNormalizeSection(pData->arrItems[i].iSection);
		if ( pData->arrItems[i].iType == XUI_STATUSBAR_ITEM_SPACER ) {
			pData->arrItems[i].iState &= ~(XUI_STATUSBAR_ITEM_ENABLED | XUI_STATUSBAR_ITEM_CLICKABLE);
		}
		if ( pData->arrItems[i].iType == XUI_STATUSBAR_ITEM_PROGRESS ) {
			if ( pData->arrItems[i].fMax < pData->arrItems[i].fMin ) pData->arrItems[i].fMax = pData->arrItems[i].fMin;
			if ( pData->arrItems[i].fValue < pData->arrItems[i].fMin ) pData->arrItems[i].fValue = pData->arrItems[i].fMin;
			if ( pData->arrItems[i].fValue > pData->arrItems[i].fMax ) pData->arrItems[i].fValue = pData->arrItems[i].fMax;
		}
	}
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiStatusBarAddItem(xui_widget pWidget, int iSection, int iType, const char* sText, float fWidth, uint32_t iState, int iValue)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	xui_statusbar_item_t* pItem;
	int iIndex;

	if ( (pData == NULL) || !__xuiStatusBarItemTypeValid(iType) || (pData->iItemCount >= XUI_STATUSBAR_ITEM_CAPACITY) ) return -1;
	iIndex = pData->iItemCount++;
	pItem = &pData->arrItems[iIndex];
	memset(pItem, 0, sizeof(*pItem));
	pItem->sText = (sText != NULL) ? sText : "";
	pItem->iType = iType;
	pItem->iSection = __xuiStatusBarNormalizeSection(iSection);
	pItem->iState = (iType == XUI_STATUSBAR_ITEM_SPACER) ? 0u : iState;
	pItem->iValue = iValue;
	pItem->fWidth = fWidth;
	pItem->fFlex = 0.0f;
	pItem->fMin = 0.0f;
	pItem->fMax = 1.0f;
	pItem->fValue = 0.0f;
	pData->iChangeCount++;
	(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return iIndex;
}

XUI_API int xuiStatusBarAddText(xui_widget pWidget, int iSection, const char* sText, float fWidth, int bClickable, int iValue)
{
	uint32_t iState = XUI_STATUSBAR_ITEM_ENABLED;
	if ( bClickable ) iState |= XUI_STATUSBAR_ITEM_CLICKABLE;
	return __xuiStatusBarAddItem(pWidget, iSection, XUI_STATUSBAR_ITEM_TEXT, sText, fWidth, iState, iValue);
}

XUI_API int xuiStatusBarAddProgress(xui_widget pWidget, int iSection, float fMin, float fMax, float fValue, float fWidth)
{
	xui_statusbar_data_t* pData;
	int iIndex;

	iIndex = __xuiStatusBarAddItem(pWidget, iSection, XUI_STATUSBAR_ITEM_PROGRESS, "", fWidth, XUI_STATUSBAR_ITEM_ENABLED, 0);
	if ( iIndex < 0 ) return -1;
	pData = __xuiStatusBarGetData(pWidget);
	if ( pData == NULL ) return -1;
	if ( fMax < fMin ) fMax = fMin;
	pData->arrItems[iIndex].fMin = fMin;
	pData->arrItems[iIndex].fMax = fMax;
	if ( fValue < fMin ) fValue = fMin;
	if ( fValue > fMax ) fValue = fMax;
	pData->arrItems[iIndex].fValue = fValue;
	return iIndex;
}

XUI_API int xuiStatusBarAddSpacer(xui_widget pWidget, int iSection, float fWidth)
{
	return __xuiStatusBarAddItem(pWidget, iSection, XUI_STATUSBAR_ITEM_SPACER, "", fWidth, 0u, 0);
}

XUI_API int xuiStatusBarAddFlexibleSpacer(xui_widget pWidget, int iSection, float fWeight)
{
	xui_statusbar_data_t* pData;
	int iIndex;

	iIndex = __xuiStatusBarAddItem(pWidget, iSection, XUI_STATUSBAR_ITEM_SPACER, "", 0.0f, 0u, 0);
	if ( iIndex < 0 ) return -1;
	pData = __xuiStatusBarGetData(pWidget);
	if ( pData == NULL ) return -1;
	if ( fWeight <= 0.0f ) fWeight = 1.0f;
	pData->arrItems[iIndex].fFlex = fWeight;
	return iIndex;
}

XUI_API int xuiStatusBarClear(xui_widget pWidget)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(pData->arrItems, 0, sizeof(pData->arrItems));
	pData->iItemCount = 0;
	pData->iHover = -1;
	pData->iActive = -1;
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiStatusBarGetItemCount(xui_widget pWidget)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	return (pData != NULL) ? pData->iItemCount : 0;
}

XUI_API const xui_statusbar_item_t* xuiStatusBarGetItem(xui_widget pWidget, int iIndex)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return NULL;
	return &pData->arrItems[iIndex];
}

XUI_API xui_rect_t xuiStatusBarGetItemRect(xui_widget pWidget, int iIndex)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	xui_statusbar_resolved_t tResolved;
	xui_rect_t tRect;

	memset(&tRect, 0, sizeof(tRect));
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return tRect;
	__xuiStatusBarResolve(pWidget, pData, &tResolved);
	(void)__xuiStatusBarLayoutItems(pWidget, pData, &tResolved);
	return pData->arrItems[iIndex].tRect;
}

XUI_API int xuiStatusBarGetItemAt(xui_widget pWidget, float fX, float fY)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	xui_statusbar_resolved_t tResolved;
	if ( pData == NULL ) return -1;
	__xuiStatusBarResolve(pWidget, pData, &tResolved);
	(void)__xuiStatusBarLayoutItems(pWidget, pData, &tResolved);
	return __xuiStatusBarIndexAt(pData, fX, fY);
}

XUI_API int xuiStatusBarSetItemEnabled(xui_widget pWidget, int iIndex, int bEnabled)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->arrItems[iIndex].iType == XUI_STATUSBAR_ITEM_SPACER ) return XUI_OK;
	if ( bEnabled ) pData->arrItems[iIndex].iState |= XUI_STATUSBAR_ITEM_ENABLED;
	else {
		pData->arrItems[iIndex].iState &= ~XUI_STATUSBAR_ITEM_ENABLED;
		if ( pData->iHover == iIndex ) pData->iHover = -1;
		if ( pData->iActive == iIndex ) pData->iActive = -1;
	}
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiStatusBarIsItemEnabled(xui_widget pWidget, int iIndex)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return 0;
	return (pData->arrItems[iIndex].iState & XUI_STATUSBAR_ITEM_ENABLED) != 0u;
}

XUI_API int xuiStatusBarSetItemClickable(xui_widget pWidget, int iIndex, int bClickable)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->arrItems[iIndex].iType == XUI_STATUSBAR_ITEM_SPACER ) return XUI_OK;
	if ( bClickable ) pData->arrItems[iIndex].iState |= XUI_STATUSBAR_ITEM_CLICKABLE;
	else {
		pData->arrItems[iIndex].iState &= ~XUI_STATUSBAR_ITEM_CLICKABLE;
		if ( pData->iHover == iIndex ) pData->iHover = -1;
		if ( pData->iActive == iIndex ) pData->iActive = -1;
	}
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiStatusBarIsItemClickable(xui_widget pWidget, int iIndex)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return 0;
	return __xuiStatusBarItemInteractive(&pData->arrItems[iIndex]);
}

XUI_API int xuiStatusBarSetItemText(xui_widget pWidget, int iIndex, const char* sText)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrItems[iIndex].sText = (sText != NULL) ? sText : "";
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiStatusBarSetItemValue(xui_widget pWidget, int iIndex, int iValue)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrItems[iIndex].iValue = iValue;
	pData->iChangeCount++;
	return XUI_OK;
}

XUI_API int xuiStatusBarGetItemValue(xui_widget pWidget, int iIndex)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return 0;
	return pData->arrItems[iIndex].iValue;
}

XUI_API int xuiStatusBarSetItemWidth(xui_widget pWidget, int iIndex, float fWidth)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) || (fWidth < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrItems[iIndex].fWidth = fWidth;
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiStatusBarSetItemFlex(xui_widget pWidget, int iIndex, float fFlex)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) || (fFlex < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrItems[iIndex].fFlex = fFlex;
	if ( fFlex > 0.0f ) pData->arrItems[iIndex].fWidth = 0.0f;
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiStatusBarSetProgress(xui_widget pWidget, int iIndex, float fValue)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	xui_statusbar_item_t* pItem;
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	pItem = &pData->arrItems[iIndex];
	if ( pItem->iType != XUI_STATUSBAR_ITEM_PROGRESS ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( fValue < pItem->fMin ) fValue = pItem->fMin;
	if ( fValue > pItem->fMax ) fValue = pItem->fMax;
	pItem->fValue = fValue;
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiStatusBarGetProgress(xui_widget pWidget, int iIndex)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return 0.0f;
	return pData->arrItems[iIndex].fValue;
}

XUI_API int xuiStatusBarGetHoverIndex(xui_widget pWidget)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	return (pData != NULL) ? pData->iHover : -1;
}

XUI_API int xuiStatusBarSetHoverIndex(xui_widget pWidget, int iIndex)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiStatusBarSetHover(pWidget, pData, iIndex);
}

XUI_API int xuiStatusBarGetActiveIndex(xui_widget pWidget)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	return (pData != NULL) ? pData->iActive : -1;
}

XUI_API int xuiStatusBarSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pFont = pFont;
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_font xuiStatusBarGetFont(xui_widget pWidget)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API int xuiStatusBarSetMetrics(xui_widget pWidget, const xui_statusbar_metrics_t* pMetrics)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	if ( (pData == NULL) || !__xuiStatusBarMetricsValid(pMetrics) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->tMetrics = *pMetrics;
	pData->tMetrics.iSize = sizeof(pData->tMetrics);
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiStatusBarGetMetrics(xui_widget pWidget, xui_statusbar_metrics_t* pMetrics)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	if ( (pData == NULL) || (pMetrics == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pMetrics = pData->tMetrics;
	pMetrics->iSize = sizeof(*pMetrics);
	return XUI_OK;
}

XUI_API int xuiStatusBarSetColors(xui_widget pWidget, const xui_statusbar_colors_t* pColors)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	if ( (pData == NULL) || !__xuiStatusBarColorsValid(pColors) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->tColors = *pColors;
	pData->tColors.iSize = sizeof(pData->tColors);
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiStatusBarGetColors(xui_widget pWidget, xui_statusbar_colors_t* pColors)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	if ( (pData == NULL) || (pColors == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pColors = pData->tColors;
	pColors->iSize = sizeof(*pColors);
	return XUI_OK;
}

XUI_API uint32_t xuiStatusBarGetState(xui_widget pWidget)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	uint32_t iState = 0u;
	if ( pData == NULL ) return XUI_WIDGET_STATE_DISABLED;
	if ( !xuiWidgetGetEnabled(pWidget) ) iState |= XUI_WIDGET_STATE_DISABLED;
	if ( xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget ) iState |= XUI_WIDGET_STATE_FOCUS;
	if ( pData->iHover >= 0 ) iState |= XUI_WIDGET_STATE_HOVER;
	if ( pData->iActive >= 0 ) iState |= XUI_WIDGET_STATE_ACTIVE;
	return iState;
}

XUI_API int xuiStatusBarGetSelectCount(xui_widget pWidget)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	return (pData != NULL) ? pData->iSelectCount : 0;
}

XUI_API int xuiStatusBarGetChangeCount(xui_widget pWidget)
{
	xui_statusbar_data_t* pData = __xuiStatusBarGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}
