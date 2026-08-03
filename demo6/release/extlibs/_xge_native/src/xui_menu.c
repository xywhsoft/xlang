#include "xui_internal.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define XUI_MENU_TEXT_SAFETY_PAD 6.0f

typedef struct xui_menu_data_t {
	xui_widget pPopup;
	xui_widget pOwner;
	xui_widget pParentMenu;
	xui_widget pOpenSubmenu;
	xui_font pFont;
	xui_menu_item_t arrItems[XUI_MENU_ITEM_CAPACITY];
	xui_rect_t arrItemRect[XUI_MENU_ITEM_CAPACITY];
	xui_menu_metrics_t tMetrics;
	xui_menu_colors_t tColors;
	xui_menu_select_proc onSelect;
	void* pSelectUser;
	float fContentW;
	float fContentH;
	int iItemCount;
	int iHover;
	int iParentItem;
	int iSelectCount;
} xui_menu_data_t;

static xui_menu_data_t* __xuiMenuGetData(xui_widget pWidget);
static int __xuiMenuApplyPopupSize(xui_widget pWidget, xui_menu_data_t* pData);
static int __xuiMenuCloseSubmenu(xui_widget pWidget, xui_menu_data_t* pData);

static int __xuiMenuAlpha(uint32_t iColor)
{
	return (int)(iColor & 0xffu);
}

static float __xuiMenuMax(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static const char* __xuiMenuText(const char* sText)
{
	return (sText != NULL) ? sText : "";
}

static int __xuiMenuDescValid(const xui_menu_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	return (pDesc->iSize == 0) || (pDesc->iSize >= sizeof(*pDesc));
}

static int __xuiMenuMetricsValid(const xui_menu_metrics_t* pMetrics)
{
	if ( pMetrics == NULL ) {
		return 0;
	}
	if ( (pMetrics->iSize != 0) && (pMetrics->iSize < sizeof(*pMetrics)) ) {
		return 0;
	}
	if ( (pMetrics->fItemHeight <= 0.0f) ||
	     (pMetrics->fSeparatorHeight <= 0.0f) ||
	     (pMetrics->fPaddingX < 0.0f) ||
	     (pMetrics->fPaddingY < 0.0f) ||
	     (pMetrics->fMarkWidth < 0.0f) ||
	     (pMetrics->fIconWidth < 0.0f) ||
	     (pMetrics->fShortcutGap < 0.0f) ||
	     (pMetrics->fArrowWidth < 0.0f) ||
	     (pMetrics->fMinWidth < 0.0f) ||
	     (pMetrics->fMaxHeight < 0.0f) ) {
		return 0;
	}
	return 1;
}

static int __xuiMenuColorsValid(const xui_menu_colors_t* pColors)
{
	if ( pColors == NULL ) {
		return 0;
	}
	return (pColors->iSize == 0) || (pColors->iSize >= sizeof(*pColors));
}

static int __xuiMenuItemTypeValid(int iType)
{
	return (iType >= XUI_MENU_ITEM_NORMAL) && (iType <= XUI_MENU_ITEM_SUBMENU);
}

static int __xuiMenuItemEnabled(const xui_menu_item_t* pItem)
{
	return (pItem != NULL) &&
	       (pItem->iType != XUI_MENU_ITEM_SEPARATOR) &&
	       ((pItem->iState & XUI_MENU_ITEM_ENABLED) != 0u);
}

static void __xuiMenuDefaultMetrics(xui_menu_metrics_t* pMetrics)
{
	memset(pMetrics, 0, sizeof(*pMetrics));
	pMetrics->iSize = sizeof(*pMetrics);
	pMetrics->fItemHeight = 24.0f;
	pMetrics->fSeparatorHeight = 9.0f;
	pMetrics->fPaddingX = 1.0f;
	pMetrics->fPaddingY = 1.0f;
	pMetrics->fMarkWidth = 22.0f;
	pMetrics->fIconWidth = 4.0f;
	pMetrics->fShortcutGap = 20.0f;
	pMetrics->fArrowWidth = 16.0f;
	pMetrics->fMinWidth = 112.0f;
	pMetrics->fMaxHeight = 0.0f;
}

static void __xuiMenuDefaultColors(xui_menu_colors_t* pColors)
{
	memset(pColors, 0, sizeof(*pColors));
	pColors->iSize = sizeof(*pColors);
	pColors->iPanelColor = XUI_COLOR_RGBA(250, 252, 255, 255);
	pColors->iBorderColor = XUI_COLOR_RGBA(122, 164, 202, 255);
	pColors->iShadowColor = XUI_COLOR_RGBA(44, 70, 96, 46);
	pColors->iHoverColor = XUI_COLOR_RGBA(54, 125, 190, 255);
	pColors->iTextColor = XUI_COLOR_RGBA(28, 60, 94, 255);
	pColors->iHoverTextColor = XUI_COLOR_RGBA(255, 255, 255, 255);
	pColors->iDisabledTextColor = XUI_COLOR_RGBA(142, 152, 166, 210);
	pColors->iShortcutColor = XUI_COLOR_RGBA(84, 111, 140, 255);
	pColors->iDangerTextColor = XUI_COLOR_RGBA(184, 54, 54, 255);
	pColors->iMarkColor = XUI_COLOR_RGBA(37, 94, 145, 255);
	pColors->iSeparatorColor = XUI_COLOR_RGBA(202, 218, 232, 255);
	pColors->iFocusColor = XUI_COLOR_RGBA(54, 148, 224, 255);
}

static xui_font __xuiMenuResolveFont(xui_widget pWidget, xui_menu_data_t* pData)
{
	if ( (pData != NULL) && (pData->pFont != NULL) ) {
		return pData->pFont;
	}
	return xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
}

static xui_vec2_t __xuiMenuMeasureText(xui_widget pWidget, xui_font pFont, const char* sText)
{
	xui_vec2_t tSize;
	xui_proxy pProxy;
	const unsigned char* pScan;
	float fAsciiW;
	float fWideW;
	float fEstimate;

	tSize.fX = 0.0f;
	tSize.fY = 18.0f;
	if ( (sText == NULL) || (sText[0] == '\0') ) {
		return tSize;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	fAsciiW = 7.0f;
	fWideW = 14.0f;
	if ( (pProxy != NULL) && (pProxy->fontGetMetrics != NULL) && (pFont != NULL) ) {
		xui_font_metrics_t tMetrics;
		if ( (pProxy->fontGetMetrics(pProxy, pFont, &tMetrics) == XUI_OK) && (tMetrics.fSize > 0.0f) ) {
			fAsciiW = tMetrics.fSize * 0.5f;
			fWideW = tMetrics.fSize;
			tSize.fY = (tMetrics.fLineHeight > 0.0f) ? tMetrics.fLineHeight : tMetrics.fSize;
		}
	}
	fEstimate = 0.0f;
	pScan = (const unsigned char*)sText;
	while ( *pScan != '\0' ) {
		if ( *pScan == '\n' ) {
			break;
		}
		if ( *pScan < 0x80u ) {
			fEstimate += fAsciiW;
			pScan++;
		} else {
			fEstimate += fWideW;
			if ( (*pScan & 0xe0u) == 0xc0u && pScan[1] != '\0' ) {
				pScan += 2;
			} else if ( (*pScan & 0xf0u) == 0xe0u && pScan[1] != '\0' && pScan[2] != '\0' ) {
				pScan += 3;
			} else if ( (*pScan & 0xf8u) == 0xf0u && pScan[1] != '\0' && pScan[2] != '\0' && pScan[3] != '\0' ) {
				pScan += 4;
			} else {
				pScan++;
			}
		}
	}
	if ( (pProxy != NULL) && (pProxy->textMeasure != NULL) && (pFont != NULL) &&
	     (pProxy->textMeasure(pProxy, pFont, sText, &tSize) == XUI_OK) &&
	     (tSize.fX >= 0.0f) && (tSize.fY >= 0.0f) ) {
		return tSize;
	}
	tSize.fX = fEstimate;
	return tSize;
}

static int __xuiMenuMeasureData(xui_widget pWidget, xui_menu_data_t* pData)
{
	xui_font pFont;
	xui_vec2_t tText;
	xui_vec2_t tShortcut;
	float fTextW;
	float fShortcutW;
	float fWidth;
	float fHeight;
	float fItemH;
	int i;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pFont = __xuiMenuResolveFont(pWidget, pData);
	fTextW = 0.0f;
	fShortcutW = 0.0f;
	fHeight = pData->tMetrics.fPaddingY * 2.0f;
	for ( i = 0; i < pData->iItemCount; i++ ) {
		if ( pData->arrItems[i].iType == XUI_MENU_ITEM_SEPARATOR ) {
			fHeight += pData->tMetrics.fSeparatorHeight;
			continue;
		}
		tText = __xuiMenuMeasureText(pWidget, pFont, pData->arrItems[i].sText);
		tShortcut = __xuiMenuMeasureText(pWidget, pFont, pData->arrItems[i].sShortcut);
		fTextW = __xuiMenuMax(fTextW, tText.fX);
		fShortcutW = __xuiMenuMax(fShortcutW, tShortcut.fX);
		fHeight += pData->tMetrics.fItemHeight;
	}
	fWidth = pData->tMetrics.fPaddingX * 2.0f +
	         pData->tMetrics.fMarkWidth +
	         pData->tMetrics.fIconWidth +
	         4.0f +
	         fTextW +
	         XUI_MENU_TEXT_SAFETY_PAD +
	         4.0f +
	         pData->tMetrics.fArrowWidth;
	if ( fShortcutW > 0.0f ) {
		fWidth += pData->tMetrics.fShortcutGap + fShortcutW + XUI_MENU_TEXT_SAFETY_PAD;
	}
	fWidth = __xuiMenuMax(fWidth, pData->tMetrics.fMinWidth);
	pData->fContentW = xuiInternalSnapSize(fWidth);
	pData->fContentH = xuiInternalSnapSize(fHeight);
	fHeight = pData->tMetrics.fPaddingY;
	for ( i = 0; i < pData->iItemCount; i++ ) {
		fItemH = (pData->arrItems[i].iType == XUI_MENU_ITEM_SEPARATOR) ?
			pData->tMetrics.fSeparatorHeight : pData->tMetrics.fItemHeight;
		pData->arrItemRect[i] = xuiInternalSnapRect((xui_rect_t){
			pData->tMetrics.fPaddingX,
			fHeight,
			__xuiMenuMax(1.0f, pData->fContentW - pData->tMetrics.fPaddingX * 2.0f),
			fItemH
		});
		fHeight += fItemH;
	}
	return XUI_OK;
}

static int __xuiMenuIndexAt(xui_menu_data_t* pData, float fX, float fY)
{
	xui_rect_t tRect;
	int i;

	if ( pData == NULL ) {
		return -1;
	}
	for ( i = 0; i < pData->iItemCount; i++ ) {
		if ( pData->arrItems[i].iType == XUI_MENU_ITEM_SEPARATOR ) {
			continue;
		}
		tRect = pData->arrItemRect[i];
		if ( (fX >= tRect.fX) &&
		     (fY >= tRect.fY) &&
		     (fX < tRect.fX + tRect.fW) &&
		     (fY < tRect.fY + tRect.fH) ) {
			return i;
		}
	}
	return -1;
}

static int __xuiMenuNextEnabled(xui_menu_data_t* pData, int iStart, int iStep)
{
	int i;
	int iIndex;

	if ( (pData == NULL) || (pData->iItemCount <= 0) || (iStep == 0) ) {
		return -1;
	}
	iIndex = iStart;
	for ( i = 0; i < pData->iItemCount; i++ ) {
		iIndex += iStep;
		if ( iIndex < 0 ) {
			iIndex = pData->iItemCount - 1;
		} else if ( iIndex >= pData->iItemCount ) {
			iIndex = 0;
		}
		if ( __xuiMenuItemEnabled(&pData->arrItems[iIndex]) ) {
			return iIndex;
		}
	}
	return -1;
}

static int __xuiMenuInvalidateItem(xui_widget pWidget, xui_menu_data_t* pData, int iIndex)
{
	xui_rect_t tRect;

	if ( (pWidget == NULL) || (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) {
		return XUI_OK;
	}
	tRect = pData->arrItemRect[iIndex];
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XUI_OK;
	}
	tRect.fX -= 2.0f;
	tRect.fY -= 2.0f;
	tRect.fW += 4.0f;
	tRect.fH += 4.0f;
	return xuiWidgetInvalidateRect(pWidget, tRect, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiMenuSetHover(xui_widget pWidget, xui_menu_data_t* pData, int iIndex)
{
	int iOldHover;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (iIndex < 0) || (iIndex >= pData->iItemCount) ||
	     !__xuiMenuItemEnabled(&pData->arrItems[iIndex]) ) {
		iIndex = -1;
	}
	if ( pData->iHover == iIndex ) {
		return XUI_OK;
	}
	iOldHover = pData->iHover;
	pData->iHover = iIndex;
	iRet = __xuiMenuInvalidateItem(pWidget, pData, iOldHover);
	if ( iRet == XUI_OK ) {
		iRet = __xuiMenuInvalidateItem(pWidget, pData, iIndex);
	}
	return iRet;
}

static int __xuiMenuDrawRectFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( __xuiMenuAlpha(iColor) == 0 ) {
		return XUI_OK;
	}
	return (pProxy->drawRectFill != NULL) ? pProxy->drawRectFill(pProxy, pDraw, tRect, iColor) : XUI_OK;
}

static int __xuiMenuDrawRectStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	if ( (fWidth <= 0.0f) || (__xuiMenuAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	return (pProxy->drawRectStroke != NULL) ? pProxy->drawRectStroke(pProxy, pDraw, tRect, fWidth, iColor) : XUI_OK;
}

static int __xuiMenuDrawCheck(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	float fX0;
	float fY0;
	float fX1;
	float fY1;
	float fX2;
	float fY2;
	int iRet;

	if ( pProxy->drawLine == NULL ) {
		return XUI_OK;
	}
	fX0 = tRect.fX + tRect.fW * 0.25f;
	fY0 = tRect.fY + tRect.fH * 0.54f;
	fX1 = tRect.fX + tRect.fW * 0.43f;
	fY1 = tRect.fY + tRect.fH * 0.72f;
	fX2 = tRect.fX + tRect.fW * 0.78f;
	fY2 = tRect.fY + tRect.fH * 0.28f;
	iRet = pProxy->drawLine(pProxy, pDraw, fX0, fY0, fX1, fY1, 2.0f, iColor);
	if ( iRet != XUI_OK ) return iRet;
	return pProxy->drawLine(pProxy, pDraw, fX1, fY1, fX2, fY2, 2.0f, iColor);
}

static int __xuiMenuDrawRadio(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	float fCx;
	float fCy;
	float fR;
	int iRet;

	if ( (pProxy->drawCircleStroke == NULL) || (pProxy->drawCircleFill == NULL) ) {
		return XUI_OK;
	}
	fCx = tRect.fX + tRect.fW * 0.5f;
	fCy = tRect.fY + tRect.fH * 0.5f;
	fR = (tRect.fW < tRect.fH ? tRect.fW : tRect.fH) * 0.32f;
	iRet = pProxy->drawCircleStroke(pProxy, pDraw, fCx, fCy, fR, 1.4f, iColor);
	if ( iRet != XUI_OK ) return iRet;
	return pProxy->drawCircleFill(pProxy, pDraw, fCx, fCy, fR * 0.48f, iColor);
}

static int __xuiMenuDrawArrow(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	xui_vec2_t tA;
	xui_vec2_t tB;
	xui_vec2_t tC;
	float fCx;
	float fCy;

	fCx = tRect.fX + tRect.fW * 0.56f;
	fCy = tRect.fY + tRect.fH * 0.5f;
	tA = (xui_vec2_t){fCx - 2.0f, fCy - 4.0f};
	tB = (xui_vec2_t){fCx - 2.0f, fCy + 4.0f};
	tC = (xui_vec2_t){fCx + 3.0f, fCy};
	if ( pProxy->drawTriangleFill != NULL ) {
		return pProxy->drawTriangleFill(pProxy, pDraw, tA, tB, tC, iColor);
	}
	if ( pProxy->drawLine != NULL ) {
		return pProxy->drawLine(pProxy, pDraw, tA.fX, tA.fY, tC.fX, tC.fY, 1.4f, iColor);
	}
	return XUI_OK;
}

static int __xuiMenuCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_menu_data_t* pData;
	xui_proxy pProxy;
	xui_font pFont;
	xui_rect_t tRect;
	xui_rect_t tItem;
	xui_rect_t tMark;
	xui_rect_t tText;
	xui_rect_t tShortcut;
	xui_rect_t tArrow;
	xui_vec2_t tShortcutSize;
	float fTextX;
	float fTextRight;
	float fShortcutW;
	uint32_t iTextColor;
	uint32_t iShortcutColor;
	int iRet;
	int i;

	(void)iStateId;
	(void)pUser;
	pData = __xuiMenuGetData(pWidget);
	if ( (pData == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	iRet = __xuiMenuMeasureData(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	pFont = __xuiMenuResolveFont(pWidget, pData);
	tRect = (xui_rect_t){0.0f, 0.0f, pData->fContentW, pData->fContentH};
	iRet = __xuiMenuDrawRectFill(pProxy, pDraw, tRect, pData->tColors.iPanelColor);
	if ( iRet != XUI_OK ) return iRet;
	for ( i = 0; i < pData->iItemCount; i++ ) {
		tItem = pData->arrItemRect[i];
		if ( pData->arrItems[i].iType == XUI_MENU_ITEM_SEPARATOR ) {
			if ( (pProxy->drawLine != NULL) && (__xuiMenuAlpha(pData->tColors.iSeparatorColor) != 0) ) {
				iRet = pProxy->drawLine(pProxy, pDraw,
					tItem.fX + pData->tMetrics.fMarkWidth + 4.0f,
					tItem.fY + tItem.fH * 0.5f,
					tItem.fX + tItem.fW - 6.0f,
					tItem.fY + tItem.fH * 0.5f,
					1.0f,
					pData->tColors.iSeparatorColor);
				if ( iRet != XUI_OK ) return iRet;
			}
			continue;
		}
		if ( i == pData->iHover ) {
			iRet = __xuiMenuDrawRectFill(pProxy, pDraw, tItem, pData->tColors.iHoverColor);
			if ( iRet != XUI_OK ) return iRet;
		}
		tMark = (xui_rect_t){
			tItem.fX + 3.0f,
			tItem.fY + (tItem.fH - 15.0f) * 0.5f,
			15.0f,
			15.0f
		};
		iTextColor = pData->tColors.iTextColor;
		iShortcutColor = pData->tColors.iShortcutColor;
		if ( i == pData->iHover ) {
			iTextColor = pData->tColors.iHoverTextColor;
			iShortcutColor = pData->tColors.iHoverTextColor;
		}
		if ( !__xuiMenuItemEnabled(&pData->arrItems[i]) ) {
			iTextColor = pData->tColors.iDisabledTextColor;
			iShortcutColor = pData->tColors.iDisabledTextColor;
		} else if ( (pData->arrItems[i].iState & XUI_MENU_ITEM_DANGER) != 0u && i != pData->iHover ) {
			iTextColor = pData->tColors.iDangerTextColor;
		}
		if ( ((pData->arrItems[i].iType == XUI_MENU_ITEM_CHECK) ||
		      (pData->arrItems[i].iType == XUI_MENU_ITEM_RADIO)) &&
		     ((pData->arrItems[i].iState & XUI_MENU_ITEM_CHECKED) != 0u) ) {
			if ( pData->arrItems[i].iType == XUI_MENU_ITEM_RADIO ) {
				iRet = __xuiMenuDrawRadio(pProxy, pDraw, tMark, (i == pData->iHover) ? pData->tColors.iHoverTextColor : pData->tColors.iMarkColor);
			} else {
				iRet = __xuiMenuDrawCheck(pProxy, pDraw, tMark, (i == pData->iHover) ? pData->tColors.iHoverTextColor : pData->tColors.iMarkColor);
			}
			if ( iRet != XUI_OK ) return iRet;
		}
		if ( pData->arrItems[i].iIcon != 0 ) {
			xui_rect_t tIcon = (xui_rect_t){
				tItem.fX + pData->tMetrics.fMarkWidth + 2.0f,
				tItem.fY + tItem.fH * 0.5f - 3.0f,
				6.0f,
				6.0f
			};
			iRet = __xuiMenuDrawRectFill(pProxy, pDraw, tIcon, (i == pData->iHover) ? pData->tColors.iHoverTextColor : pData->tColors.iMarkColor);
			if ( iRet != XUI_OK ) return iRet;
		}
		fTextX = tItem.fX + pData->tMetrics.fMarkWidth + pData->tMetrics.fIconWidth + 4.0f;
		fTextRight = tItem.fX + tItem.fW - pData->tMetrics.fArrowWidth - 4.0f;
		fShortcutW = 0.0f;
		if ( (pData->arrItems[i].sShortcut != NULL) && (pData->arrItems[i].sShortcut[0] != '\0') ) {
			tShortcutSize = __xuiMenuMeasureText(pWidget, pFont, pData->arrItems[i].sShortcut);
			fShortcutW = tShortcutSize.fX + XUI_MENU_TEXT_SAFETY_PAD;
			fTextRight -= pData->tMetrics.fShortcutGap + fShortcutW;
		}
		tText = (xui_rect_t){
			fTextX,
			tItem.fY,
			__xuiMenuMax(1.0f, fTextRight - fTextX),
			tItem.fH
		};
		if ( (pFont != NULL) && (pProxy->drawText != NULL) && (pData->arrItems[i].sText != NULL) ) {
			iRet = pProxy->drawText(pProxy, pDraw, pFont, pData->arrItems[i].sText, tText, iTextColor,
				XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
			if ( iRet != XUI_OK ) return iRet;
		}
		if ( (pFont != NULL) && (pProxy->drawText != NULL) &&
		     (pData->arrItems[i].sShortcut != NULL) && (pData->arrItems[i].sShortcut[0] != '\0') ) {
			tShortcut = (xui_rect_t){
				tItem.fX + tItem.fW - pData->tMetrics.fArrowWidth - 4.0f - fShortcutW,
				tItem.fY,
				__xuiMenuMax(1.0f, fShortcutW),
				tItem.fH
			};
			iRet = pProxy->drawText(pProxy, pDraw, pFont, pData->arrItems[i].sShortcut, tShortcut, iShortcutColor,
				XUI_TEXT_ALIGN_RIGHT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
			if ( iRet != XUI_OK ) return iRet;
		}
		if ( pData->arrItems[i].iType == XUI_MENU_ITEM_SUBMENU ) {
			tArrow = (xui_rect_t){tItem.fX + tItem.fW - pData->tMetrics.fArrowWidth, tItem.fY, pData->tMetrics.fArrowWidth, tItem.fH};
			iRet = __xuiMenuDrawArrow(pProxy, pDraw, tArrow, iShortcutColor);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	return XUI_OK;
}

static int __xuiMenuApplyCheckState(xui_menu_data_t* pData, int iIndex)
{
	int i;

	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->arrItems[iIndex].iType == XUI_MENU_ITEM_CHECK ) {
		pData->arrItems[iIndex].iState ^= XUI_MENU_ITEM_CHECKED;
		return XUI_OK;
	}
	if ( pData->arrItems[iIndex].iType == XUI_MENU_ITEM_RADIO ) {
		for ( i = iIndex; i >= 0 && pData->arrItems[i].iType == XUI_MENU_ITEM_RADIO; i-- ) {
			pData->arrItems[i].iState &= ~XUI_MENU_ITEM_CHECKED;
		}
		for ( i = iIndex + 1; i < pData->iItemCount && pData->arrItems[i].iType == XUI_MENU_ITEM_RADIO; i++ ) {
			pData->arrItems[i].iState &= ~XUI_MENU_ITEM_CHECKED;
		}
		pData->arrItems[iIndex].iState |= XUI_MENU_ITEM_CHECKED;
	}
	return XUI_OK;
}

static xui_widget __xuiMenuRoot(xui_widget pWidget)
{
	xui_menu_data_t* pData;
	xui_widget pScan;

	pScan = pWidget;
	for (;;) {
		pData = __xuiMenuGetData(pScan);
		if ( (pData == NULL) || (pData->pParentMenu == NULL) ) {
			break;
		}
		pScan = pData->pParentMenu;
	}
	return pScan;
}

static int __xuiMenuOpenSubmenu(xui_widget pWidget, xui_menu_data_t* pData, int iIndex)
{
	xui_menu_data_t* pSubData;
	xui_widget pSubmenu;
	xui_rect_t tItem;
	xui_rect_t tWorld;
	xui_widget pOwner;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->arrItems[iIndex].iType != XUI_MENU_ITEM_SUBMENU ) {
		return XUI_OK;
	}
	pSubmenu = pData->arrItems[iIndex].pSubmenu;
	pSubData = __xuiMenuGetData(pSubmenu);
	if ( pSubData == NULL ) {
		return XUI_OK;
	}
	if ( pData->pOpenSubmenu == pSubmenu && xuiMenuIsOpen(pSubmenu) ) {
		return XUI_OK;
	}
	iRet = __xuiMenuCloseSubmenu(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	pSubData->pParentMenu = pWidget;
	pSubData->iParentItem = iIndex;
	if ( pSubData->onSelect == NULL ) {
		pSubData->onSelect = pData->onSelect;
		pSubData->pSelectUser = pData->pSelectUser;
	}
	tItem = pData->arrItemRect[iIndex];
	tWorld = xuiWidgetGetWorldRect(pWidget);
	pOwner = pWidget;
	iRet = xuiMenuOpenAt(pSubmenu, pOwner, tWorld.fX + tItem.fX + tItem.fW - 1.0f, tWorld.fY + tItem.fY);
	if ( iRet != XUI_OK ) return iRet;
	if ( pSubData->pPopup != NULL ) {
		(void)xuiPopupSetClosePolicy(pSubData->pPopup, XUI_POPUP_OUTSIDE_IGNORE, XUI_POPUP_OWNER_PASSTHROUGH, XUI_POPUP_ESCAPE_CLOSE);
		(void)xuiPopupSetFocusRestore(pSubData->pPopup, pWidget);
	}
	pData->pOpenSubmenu = pSubmenu;
	return XUI_OK;
}

static int __xuiMenuCloseSubmenu(xui_widget pWidget, xui_menu_data_t* pData)
{
	xui_widget pSubmenu;
	xui_menu_data_t* pSubData;

	(void)pWidget;
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pSubmenu = pData->pOpenSubmenu;
	pData->pOpenSubmenu = NULL;
	pSubData = __xuiMenuGetData(pSubmenu);
	if ( pSubData != NULL ) {
		(void)__xuiMenuCloseSubmenu(pSubmenu, pSubData);
		if ( pSubData->pPopup != NULL ) {
			(void)xuiPopupSetOpen(pSubData->pPopup, 0);
		}
		pSubData->pParentMenu = NULL;
		pSubData->iParentItem = -1;
		pSubData->iHover = -1;
	}
	return XUI_OK;
}

static int __xuiMenuCommit(xui_widget pWidget, xui_menu_data_t* pData, int iIndex)
{
	xui_widget pRoot;
	xui_menu_select_proc onSelect;
	void* pUser;
	int iValue;

	if ( (pWidget == NULL) || (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ||
	     !__xuiMenuItemEnabled(&pData->arrItems[iIndex]) ) {
		return XUI_OK;
	}
	if ( pData->arrItems[iIndex].iType == XUI_MENU_ITEM_SUBMENU ) {
		return __xuiMenuOpenSubmenu(pWidget, pData, iIndex);
	}
	(void)__xuiMenuApplyCheckState(pData, iIndex);
	(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	pData->iSelectCount++;
	onSelect = pData->onSelect;
	pUser = pData->pSelectUser;
	iValue = pData->arrItems[iIndex].iValue;
	pRoot = __xuiMenuRoot(pWidget);
	(void)xuiMenuClose(pRoot);
	if ( onSelect != NULL ) {
		onSelect(pWidget, iIndex, iValue, pUser);
	}
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiMenuAsciiEqual(const char* sA, const char* sB, int iLen)
{
	int i;

	if ( (sA == NULL) || (sB == NULL) ) {
		return 0;
	}
	for ( i = 0; i < iLen; i++ ) {
		if ( tolower((unsigned char)sA[i]) != tolower((unsigned char)sB[i]) ) {
			return 0;
		}
	}
	return sB[iLen] == '\0';
}

static int __xuiMenuShortcutToken(const char* sToken, int iLen, int* pKey, uint32_t* pModifiers)
{
	int iValue;

	if ( (sToken == NULL) || (iLen <= 0) || (pKey == NULL) || (pModifiers == NULL) ) {
		return 0;
	}
	if ( __xuiMenuAsciiEqual(sToken, "ctrl", iLen) || __xuiMenuAsciiEqual(sToken, "control", iLen) ) {
		*pModifiers |= XUI_MOD_CTRL;
		return 1;
	}
	if ( __xuiMenuAsciiEqual(sToken, "alt", iLen) || __xuiMenuAsciiEqual(sToken, "option", iLen) ) {
		*pModifiers |= XUI_MOD_ALT;
		return 1;
	}
	if ( __xuiMenuAsciiEqual(sToken, "shift", iLen) ) {
		*pModifiers |= XUI_MOD_SHIFT;
		return 1;
	}
	if ( __xuiMenuAsciiEqual(sToken, "super", iLen) || __xuiMenuAsciiEqual(sToken, "win", iLen) || __xuiMenuAsciiEqual(sToken, "cmd", iLen) ) {
		*pModifiers |= XUI_MOD_SUPER;
		return 1;
	}
	if ( iLen == 1 ) {
		*pKey = toupper((unsigned char)sToken[0]);
		return 1;
	}
	if ( __xuiMenuAsciiEqual(sToken, "space", iLen) ) { *pKey = XUI_KEY_SPACE; return 1; }
	if ( __xuiMenuAsciiEqual(sToken, "tab", iLen) ) { *pKey = XUI_KEY_TAB; return 1; }
	if ( __xuiMenuAsciiEqual(sToken, "enter", iLen) || __xuiMenuAsciiEqual(sToken, "return", iLen) ) { *pKey = XUI_KEY_ENTER; return 1; }
	if ( __xuiMenuAsciiEqual(sToken, "esc", iLen) || __xuiMenuAsciiEqual(sToken, "escape", iLen) ) { *pKey = XUI_KEY_ESCAPE; return 1; }
	if ( __xuiMenuAsciiEqual(sToken, "left", iLen) ) { *pKey = XUI_KEY_LEFT; return 1; }
	if ( __xuiMenuAsciiEqual(sToken, "right", iLen) ) { *pKey = XUI_KEY_RIGHT; return 1; }
	if ( __xuiMenuAsciiEqual(sToken, "up", iLen) ) { *pKey = XUI_KEY_UP; return 1; }
	if ( __xuiMenuAsciiEqual(sToken, "down", iLen) ) { *pKey = XUI_KEY_DOWN; return 1; }
	if ( __xuiMenuAsciiEqual(sToken, "home", iLen) ) { *pKey = XUI_KEY_HOME; return 1; }
	if ( __xuiMenuAsciiEqual(sToken, "end", iLen) ) { *pKey = XUI_KEY_END; return 1; }
	if ( __xuiMenuAsciiEqual(sToken, "pageup", iLen) || __xuiMenuAsciiEqual(sToken, "pgup", iLen) ) { *pKey = XUI_KEY_PAGE_UP; return 1; }
	if ( __xuiMenuAsciiEqual(sToken, "pagedown", iLen) || __xuiMenuAsciiEqual(sToken, "pgdn", iLen) ) { *pKey = XUI_KEY_PAGE_DOWN; return 1; }
	if ( __xuiMenuAsciiEqual(sToken, "delete", iLen) || __xuiMenuAsciiEqual(sToken, "del", iLen) ) { *pKey = 46; return 1; }
	if ( __xuiMenuAsciiEqual(sToken, "backspace", iLen) || __xuiMenuAsciiEqual(sToken, "bksp", iLen) ) { *pKey = 8; return 1; }
	if ( (iLen >= 2) && (iLen <= 3) && (tolower((unsigned char)sToken[0]) == 'f') ) {
		iValue = atoi(sToken + 1);
		if ( (iValue >= 1) && (iValue <= 24) ) {
			*pKey = 111 + iValue;
			return 1;
		}
	}
	return 0;
}

static int __xuiMenuShortcutParse(const char* sShortcut, int* pKey, uint32_t* pModifiers)
{
	const char* sStart;
	const char* sEnd;
	int iKey;
	uint32_t iModifiers;

	if ( (sShortcut == NULL) || (pKey == NULL) || (pModifiers == NULL) ) {
		return 0;
	}
	iKey = 0;
	iModifiers = 0;
	sStart = sShortcut;
	while ( *sStart != '\0' ) {
		while ( (*sStart == ' ') || (*sStart == '\t') || (*sStart == '+') ) sStart++;
		if ( *sStart == '\0' ) break;
		sEnd = sStart;
		while ( (*sEnd != '\0') && (*sEnd != '+') ) sEnd++;
		while ( (sEnd > sStart) && ((sEnd[-1] == ' ') || (sEnd[-1] == '\t')) ) sEnd--;
		if ( !__xuiMenuShortcutToken(sStart, (int)(sEnd - sStart), &iKey, &iModifiers) ) {
			return 0;
		}
		sStart = sEnd;
	}
	if ( iKey == 0 ) {
		return 0;
	}
	*pKey = iKey;
	*pModifiers = iModifiers;
	return 1;
}

static int __xuiMenuShortcutMatch(const char* sShortcut, int iKey, uint32_t iModifiers)
{
	int iShortcutKey;
	uint32_t iShortcutModifiers;

	if ( !__xuiMenuShortcutParse(sShortcut, &iShortcutKey, &iShortcutModifiers) ) {
		return 0;
	}
	if ( (iShortcutKey >= 'A') && (iShortcutKey <= 'Z') && (iKey >= 'a') && (iKey <= 'z') ) {
		iKey = toupper((unsigned char)iKey);
	}
	return (iShortcutKey == iKey) && (iShortcutModifiers == iModifiers);
}

static int __xuiMenuCommitShortcut(xui_widget pWidget, xui_menu_data_t* pData, int iKey, uint32_t iModifiers)
{
	int i;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_OK;
	}
	for ( i = 0; i < pData->iItemCount; i++ ) {
		if ( __xuiMenuItemEnabled(&pData->arrItems[i]) &&
		     __xuiMenuShortcutMatch(pData->arrItems[i].sShortcut, iKey, iModifiers) ) {
			return __xuiMenuCommit(pWidget, pData, i);
		}
	}
	return XUI_OK;
}

static int __xuiMenuEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_menu_data_t* pData;
	xui_rect_t tWorld;
	float fLocalX;
	float fLocalY;
	int iIndex;
	int iNext;

	(void)pUser;
	if ( (pWidget == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE ) {
		return XUI_OK;
	}
	pData = __xuiMenuGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_MOVE:
		(void)__xuiMenuMeasureData(pWidget, pData);
		tWorld = xuiWidgetGetWorldRect(pWidget);
		fLocalX = pEvent->fX - tWorld.fX;
		fLocalY = pEvent->fY - tWorld.fY;
		iIndex = __xuiMenuIndexAt(pData, fLocalX, fLocalY);
		(void)__xuiMenuSetHover(pWidget, pData, iIndex);
		if ( iIndex >= 0 && pData->arrItems[iIndex].iType == XUI_MENU_ITEM_SUBMENU ) {
			(void)__xuiMenuOpenSubmenu(pWidget, pData, iIndex);
		} else if ( iIndex >= 0 ) {
			(void)__xuiMenuCloseSubmenu(pWidget, pData);
		}
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_POINTER_DOWN:
		if ( pEvent->iButton != 0 && pEvent->iButton != XUI_POINTER_BUTTON_LEFT ) {
			return XUI_OK;
		}
		(void)__xuiMenuMeasureData(pWidget, pData);
		tWorld = xuiWidgetGetWorldRect(pWidget);
		fLocalX = pEvent->fX - tWorld.fX;
		fLocalY = pEvent->fY - tWorld.fY;
		iIndex = __xuiMenuIndexAt(pData, fLocalX, fLocalY);
		(void)__xuiMenuSetHover(pWidget, pData, iIndex);
		return __xuiMenuCommit(pWidget, pData, iIndex);
	case XUI_EVENT_KEY_DOWN:
		if ( __xuiMenuCommitShortcut(pWidget, pData, pEvent->iKey, pEvent->iModifiers) & XUI_EVENT_DISPATCH_STOP ) {
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( pEvent->iKey == XUI_KEY_DOWN ) {
			iNext = __xuiMenuNextEnabled(pData, pData->iHover, 1);
			(void)__xuiMenuSetHover(pWidget, pData, iNext);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( pEvent->iKey == XUI_KEY_UP ) {
			iNext = __xuiMenuNextEnabled(pData, pData->iHover, -1);
			(void)__xuiMenuSetHover(pWidget, pData, iNext);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( pEvent->iKey == XUI_KEY_RIGHT ) {
			if ( pData->iHover >= 0 && pData->arrItems[pData->iHover].iType == XUI_MENU_ITEM_SUBMENU ) {
				return __xuiMenuOpenSubmenu(pWidget, pData, pData->iHover);
			}
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( pEvent->iKey == XUI_KEY_LEFT ) {
			if ( pData->pParentMenu != NULL ) {
				xui_menu_data_t* pParentData = __xuiMenuGetData(pData->pParentMenu);
				if ( pParentData != NULL ) {
					pParentData->pOpenSubmenu = NULL;
				}
				(void)xuiMenuClose(pWidget);
				(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pData->pParentMenu);
			}
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( (pEvent->iKey == XUI_KEY_ENTER) || (pEvent->iKey == XUI_KEY_SPACE) ) {
			return __xuiMenuCommit(pWidget, pData, pData->iHover);
		}
		if ( pEvent->iKey == XUI_KEY_ESCAPE ) {
			(void)xuiMenuClose(__xuiMenuRoot(pWidget));
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_BLUR:
	case XUI_EVENT_FOCUS:
		(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		break;
	default:
		break;
	}
	return XUI_OK;
}

static void __xuiMenuCancelAction(xui_widget pWidget, void* pUser)
{
	(void)pUser;
	(void)xuiMenuClose(__xuiMenuRoot(pWidget));
}

static void __xuiMenuPopupChanged(xui_widget pPopup, int bOpen, void* pUser)
{
	xui_widget pMenu;
	xui_menu_data_t* pData;
	xui_menu_data_t* pParentData;

	(void)pPopup;
	pMenu = (xui_widget)pUser;
	pData = __xuiMenuGetData(pMenu);
	if ( pData == NULL ) {
		return;
	}
	if ( !bOpen ) {
		(void)__xuiMenuCloseSubmenu(pMenu, pData);
		pData->iHover = -1;
		if ( pData->pParentMenu != NULL ) {
			pParentData = __xuiMenuGetData(pData->pParentMenu);
			if ( (pParentData != NULL) && (pParentData->pOpenSubmenu == pMenu) ) {
				pParentData->pOpenSubmenu = NULL;
			}
		}
		(void)xuiWidgetInvalidate(pMenu, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
}

static void __xuiMenuDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_MANUAL;
	pLayout->iWidthMode = XUI_SIZE_CONTENT;
	pLayout->iHeightMode = XUI_SIZE_CONTENT;
	pLayout->iFlowMode = XUI_FLOW_ABSOLUTE;
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

static void __xuiMenuDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE | XUI_CACHE_UPDATE_ALL_STATES;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiMenuInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiMenuEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiMenuEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiMenuEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiMenuEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiMenuEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetCancelAction(pWidget, __xuiMenuCancelAction, NULL);
	return iRet;
}

static int __xuiMenuInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_menu_data_t* pData;
	const xui_menu_desc_t* pDesc;
	xui_cache_policy_t tPolicy;
	int iRet;

	(void)pUser;
	if ( (pWidget == NULL) || (pTypeData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pDesc = (const xui_menu_desc_t*)pCreateData;
	if ( !__xuiMenuDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = (xui_menu_data_t*)pTypeData;
	__xuiMenuDefaultMetrics(&pData->tMetrics);
	__xuiMenuDefaultColors(&pData->tColors);
	pData->pFont = (pDesc != NULL && pDesc->pFont != NULL) ? pDesc->pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	pData->pOwner = (pDesc != NULL) ? pDesc->pOwner : NULL;
	pData->iHover = -1;
	pData->iParentItem = -1;
	if ( pDesc != NULL && pDesc->bHasMetrics && __xuiMenuMetricsValid(&pDesc->tMetrics) ) {
		pData->tMetrics = pDesc->tMetrics;
		pData->tMetrics.iSize = sizeof(pData->tMetrics);
	}
	if ( pDesc != NULL && pDesc->bHasColors && __xuiMenuColorsValid(&pDesc->tColors) ) {
		pData->tColors = pDesc->tColors;
		pData->tColors.iSize = sizeof(pData->tColors);
	}
	(void)xuiWidgetSetLayoutType(pWidget, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetFlowMode(pWidget, XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_VISIBLE);
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 0);
	(void)xuiWidgetSetFocusScope(pWidget, 1);
	__xuiMenuDefaultCachePolicy(&tPolicy);
	(void)xuiWidgetSetCachePolicy(pWidget, &tPolicy);
	(void)xuiWidgetSetCacheRenderCallback(pWidget, __xuiMenuCacheRender, NULL);
	iRet = __xuiMenuInitEvents(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiMenuMeasureData(pWidget, pData);
}

static void __xuiMenuDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_menu_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_menu_data_t*)pTypeData;
	if ( pData != NULL ) {
		memset(pData, 0, sizeof(*pData));
	}
}

static xui_menu_data_t* __xuiMenuGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return NULL;
	}
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "menu");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_menu_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiMenuCreatePopup(xui_widget pWidget, xui_menu_data_t* pData, const xui_menu_desc_t* pDesc)
{
	xui_popup_desc_t tPopupDesc;
	xui_widget pPopup;
	xui_widget pContent;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(&tPopupDesc, 0, sizeof(tPopupDesc));
	tPopupDesc.iSize = sizeof(tPopupDesc);
	tPopupDesc.pOwner = (pDesc != NULL) ? pDesc->pOwner : NULL;
	tPopupDesc.fContentWidth = pData->fContentW;
	tPopupDesc.fContentHeight = pData->fContentH;
	tPopupDesc.fPadding = 3.0f;
	tPopupDesc.fBorderWidth = 1.0f;
	tPopupDesc.fShadowSize = 4.0f;
	tPopupDesc.fScrollbarSize = 8.0f;
	tPopupDesc.iAnchor = XUI_POPUP_ANCHOR_BOTTOM_LEFT;
	tPopupDesc.iDirection = XUI_POPUP_DIRECTION_RIGHT_DOWN;
	tPopupDesc.iOutsidePolicy = XUI_POPUP_OUTSIDE_CLOSE;
	tPopupDesc.iOwnerPolicy = XUI_POPUP_OWNER_CLOSE;
	tPopupDesc.iEscapePolicy = XUI_POPUP_ESCAPE_CLOSE;
	tPopupDesc.iFocusPolicy = XUI_POPUP_FOCUS_CUSTOM;
	tPopupDesc.bConsumeInside = 1;
	tPopupDesc.iScrollbarMode = XUI_SCROLLBAR_MODE_COMPACT;
	tPopupDesc.iPanelColor = pData->tColors.iPanelColor;
	tPopupDesc.iBorderColor = pData->tColors.iBorderColor;
	tPopupDesc.iShadowColor = pData->tColors.iShadowColor;
	tPopupDesc.iBackdropColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	pPopup = NULL;
	iRet = xuiPopupCreate(xuiWidgetGetContext(pWidget), &pPopup, &tPopupDesc);
	if ( iRet != XUI_OK ) return iRet;
	pData->pPopup = pPopup;
	iRet = xuiPopupSetChange(pPopup, __xuiMenuPopupChanged, pWidget);
	if ( iRet == XUI_OK ) iRet = xuiPopupSetFocusPolicy(pPopup, XUI_POPUP_FOCUS_CUSTOM, pWidget);
	if ( iRet == XUI_OK ) {
		pContent = xuiPopupGetContentWidget(pPopup);
		if ( pContent == NULL ) {
			iRet = XUI_ERROR_NOT_INITIALIZED;
		} else {
			iRet = xuiWidgetAddChild(pContent, pWidget);
		}
	}
	if ( iRet == XUI_OK ) {
		iRet = __xuiMenuApplyPopupSize(pWidget, pData);
	}
	if ( iRet != XUI_OK ) {
		(void)xuiWidgetRemoveFromParent(pWidget);
		xuiWidgetDestroy(pData->pPopup);
		pData->pPopup = NULL;
		return iRet;
	}
	return XUI_OK;
}

static int __xuiMenuApplyPopupSize(xui_widget pWidget, xui_menu_data_t* pData)
{
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiMenuMeasureData(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiWidgetSetRect(pWidget, (xui_rect_t){0.0f, 0.0f, pData->fContentW, pData->fContentH});
	if ( pData->pPopup != NULL ) {
		iRet = xuiPopupSetContentSize(pData->pPopup, pData->fContentW, pData->fContentH);
		if ( iRet != XUI_OK ) return iRet;
		iRet = xuiPopupSetMaxSize(pData->pPopup, 0.0f, pData->tMetrics.fMaxHeight);
		if ( iRet != XUI_OK ) return iRet;
		(void)xuiPopupSetColors(pData->pPopup, pData->tColors.iPanelColor, pData->tColors.iBorderColor, pData->tColors.iShadowColor, XUI_COLOR_RGBA(0, 0, 0, 0));
		if ( xuiPopupIsOpen(pData->pPopup) ) {
			(void)xuiPopupApplyPlacement(pData->pPopup);
		}
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_widget_type xuiMenuGetType(xui_context pContext)
{
	xui_widget_type pType;
	xui_widget_type_desc_t tDesc;
	xui_layout_t tLayout;
	xui_cache_policy_t tPolicy;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "menu");
	if ( pType != NULL ) {
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "menu";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_menu_data_t);
	tDesc.onInit = __xuiMenuInit;
	tDesc.onDestroy = __xuiMenuDestroy;
	tDesc.onCacheRender = __xuiMenuCacheRender;
	__xuiMenuDefaultLayout(&tLayout);
	__xuiMenuDefaultCachePolicy(&tPolicy);
	tDesc.tLayout = tLayout;
	tDesc.tCachePolicy = tPolicy;
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	return pType;
}

XUI_API int xuiMenuCreate(xui_context pContext, xui_widget* ppWidget, const xui_menu_desc_t* pDesc)
{
	xui_widget_type pType;
	xui_menu_data_t* pData;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) || (ppWidget == NULL) || !__xuiMenuDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiMenuGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	iRet = xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pData = __xuiMenuGetData(*ppWidget);
	iRet = __xuiMenuCreatePopup(*ppWidget, pData, pDesc);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(*ppWidget);
		*ppWidget = NULL;
		return iRet;
	}
	return XUI_OK;
}

XUI_API int xuiMenuSetItems(xui_widget pWidget, const xui_menu_item_t* pItems, int iCount)
{
	xui_menu_data_t* pData = __xuiMenuGetData(pWidget);
	int i;

	if ( (pData == NULL) || (iCount < 0) || (iCount > XUI_MENU_ITEM_CAPACITY) || ((iCount > 0) && (pItems == NULL)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( !__xuiMenuItemTypeValid(pItems[i].iType) ) {
			return XUI_ERROR_INVALID_ARGUMENT;
		}
	}
	memset(pData->arrItems, 0, sizeof(pData->arrItems));
	for ( i = 0; i < iCount; i++ ) {
		pData->arrItems[i] = pItems[i];
	}
	pData->iItemCount = iCount;
	pData->iHover = __xuiMenuNextEnabled(pData, -1, 1);
	return __xuiMenuApplyPopupSize(pWidget, pData);
}

XUI_API int xuiMenuAddItem(xui_widget pWidget, const xui_menu_item_t* pItem)
{
	xui_menu_data_t* pData = __xuiMenuGetData(pWidget);
	xui_menu_item_t tItem;

	if ( (pData == NULL) || (pItem == NULL) || (pData->iItemCount >= XUI_MENU_ITEM_CAPACITY) || !__xuiMenuItemTypeValid(pItem->iType) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	tItem = *pItem;
	pData->arrItems[pData->iItemCount++] = tItem;
	if ( pData->iHover < 0 ) {
		pData->iHover = __xuiMenuNextEnabled(pData, -1, 1);
	}
	return __xuiMenuApplyPopupSize(pWidget, pData);
}

XUI_API int xuiMenuAddSeparator(xui_widget pWidget)
{
	xui_menu_item_t tItem;

	memset(&tItem, 0, sizeof(tItem));
	tItem.iType = XUI_MENU_ITEM_SEPARATOR;
	return xuiMenuAddItem(pWidget, &tItem);
}

XUI_API int xuiMenuClear(xui_widget pWidget)
{
	xui_menu_data_t* pData = __xuiMenuGetData(pWidget);

	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	(void)__xuiMenuCloseSubmenu(pWidget, pData);
	memset(pData->arrItems, 0, sizeof(pData->arrItems));
	memset(pData->arrItemRect, 0, sizeof(pData->arrItemRect));
	pData->iItemCount = 0;
	pData->iHover = -1;
	return __xuiMenuApplyPopupSize(pWidget, pData);
}

XUI_API int xuiMenuGetItemCount(xui_widget pWidget)
{
	xui_menu_data_t* pData = __xuiMenuGetData(pWidget);
	return (pData != NULL) ? pData->iItemCount : 0;
}

XUI_API const xui_menu_item_t* xuiMenuGetItem(xui_widget pWidget, int iIndex)
{
	xui_menu_data_t* pData = __xuiMenuGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) {
		return NULL;
	}
	return &pData->arrItems[iIndex];
}

XUI_API xui_rect_t xuiMenuGetItemRect(xui_widget pWidget, int iIndex)
{
	xui_menu_data_t* pData = __xuiMenuGetData(pWidget);
	xui_rect_t tEmpty;

	memset(&tEmpty, 0, sizeof(tEmpty));
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) {
		return tEmpty;
	}
	(void)__xuiMenuMeasureData(pWidget, pData);
	return pData->arrItemRect[iIndex];
}

XUI_API int xuiMenuSetItemState(xui_widget pWidget, int iIndex, uint32_t iMask, uint32_t iState)
{
	xui_menu_data_t* pData = __xuiMenuGetData(pWidget);
	uint32_t iOld;

	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iOld = pData->arrItems[iIndex].iState;
	pData->arrItems[iIndex].iState = (iOld & ~iMask) | (iState & iMask);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiMenuGetItemState(xui_widget pWidget, int iIndex)
{
	xui_menu_data_t* pData = __xuiMenuGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) {
		return 0u;
	}
	return pData->arrItems[iIndex].iState;
}

XUI_API int xuiMenuGetHoverIndex(xui_widget pWidget)
{
	xui_menu_data_t* pData = __xuiMenuGetData(pWidget);
	return (pData != NULL) ? pData->iHover : -1;
}

XUI_API int xuiMenuSetHoverIndex(xui_widget pWidget, int iIndex)
{
	xui_menu_data_t* pData = __xuiMenuGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiMenuSetHover(pWidget, pData, iIndex);
}

XUI_API int xuiMenuCommitHover(xui_widget pWidget)
{
	xui_menu_data_t* pData = __xuiMenuGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiMenuCommit(pWidget, pData, pData->iHover);
}

XUI_API int xuiMenuSetSelect(xui_widget pWidget, xui_menu_select_proc onSelect, void* pUser)
{
	xui_menu_data_t* pData = __xuiMenuGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onSelect = onSelect;
	pData->pSelectUser = pUser;
	return XUI_OK;
}

XUI_API int xuiMenuGetSelectCount(xui_widget pWidget)
{
	xui_menu_data_t* pData = __xuiMenuGetData(pWidget);
	return (pData != NULL) ? pData->iSelectCount : 0;
}

XUI_API int xuiMenuSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_menu_data_t* pData = __xuiMenuGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pFont = pFont;
	return __xuiMenuApplyPopupSize(pWidget, pData);
}

XUI_API xui_font xuiMenuGetFont(xui_widget pWidget)
{
	xui_menu_data_t* pData = __xuiMenuGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API int xuiMenuSetMetrics(xui_widget pWidget, const xui_menu_metrics_t* pMetrics)
{
	xui_menu_data_t* pData = __xuiMenuGetData(pWidget);
	if ( (pData == NULL) || !__xuiMenuMetricsValid(pMetrics) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->tMetrics = *pMetrics;
	pData->tMetrics.iSize = sizeof(pData->tMetrics);
	return __xuiMenuApplyPopupSize(pWidget, pData);
}

XUI_API int xuiMenuGetMetrics(xui_widget pWidget, xui_menu_metrics_t* pMetrics)
{
	xui_menu_data_t* pData = __xuiMenuGetData(pWidget);
	if ( (pData == NULL) || (pMetrics == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pMetrics = pData->tMetrics;
	pMetrics->iSize = sizeof(*pMetrics);
	return XUI_OK;
}

XUI_API int xuiMenuSetColors(xui_widget pWidget, const xui_menu_colors_t* pColors)
{
	xui_menu_data_t* pData = __xuiMenuGetData(pWidget);
	if ( (pData == NULL) || !__xuiMenuColorsValid(pColors) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->tColors = *pColors;
	pData->tColors.iSize = sizeof(pData->tColors);
	return __xuiMenuApplyPopupSize(pWidget, pData);
}

XUI_API int xuiMenuGetColors(xui_widget pWidget, xui_menu_colors_t* pColors)
{
	xui_menu_data_t* pData = __xuiMenuGetData(pWidget);
	if ( (pData == NULL) || (pColors == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pColors = pData->tColors;
	pColors->iSize = sizeof(*pColors);
	return XUI_OK;
}

XUI_API int xuiMenuMeasure(xui_widget pWidget, float* pWidth, float* pHeight)
{
	xui_menu_data_t* pData = __xuiMenuGetData(pWidget);
	int iRet;

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiMenuMeasureData(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	if ( pWidth != NULL ) *pWidth = pData->fContentW;
	if ( pHeight != NULL ) *pHeight = pData->fContentH;
	return XUI_OK;
}

XUI_API int xuiMenuOpenAt(xui_widget pWidget, xui_widget pOwner, float fX, float fY)
{
	xui_menu_data_t* pData = __xuiMenuGetData(pWidget);
	xui_rect_t tAnchor;
	int iRet;

	if ( pData == NULL || pData->pPopup == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiMenuApplyPopupSize(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	pData->pOwner = pOwner;
	(void)xuiPopupSetOwner(pData->pPopup, pOwner);
	(void)xuiPopupSetFocusRestore(pData->pPopup, (pData->pParentMenu != NULL) ? pData->pParentMenu : pOwner);
	(void)xuiPopupSetClosePolicy(pData->pPopup,
		(pData->pParentMenu != NULL) ? XUI_POPUP_OUTSIDE_IGNORE : XUI_POPUP_OUTSIDE_CLOSE,
		(pData->pParentMenu != NULL) ? XUI_POPUP_OWNER_PASSTHROUGH : XUI_POPUP_OWNER_CLOSE,
		XUI_POPUP_ESCAPE_CLOSE);
	tAnchor = (xui_rect_t){fX, fY, 0.0f, 0.0f};
	(void)xuiPopupSetAnchorRect(pData->pPopup, tAnchor);
	(void)xuiPopupSetAnchor(pData->pPopup, XUI_POPUP_ANCHOR_FIXED);
	(void)xuiPopupSetDirection(pData->pPopup, XUI_POPUP_DIRECTION_RIGHT_DOWN);
	(void)xuiPopupSetGap(pData->pPopup, 0.0f);
	(void)xuiPopupSetScroll(pData->pPopup, 0.0f, 0.0f);
	pData->iHover = __xuiMenuNextEnabled(pData, -1, 1);
	iRet = xuiPopupSetOpen(pData->pPopup, 1);
	if ( iRet == XUI_OK ) {
		(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
		(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return iRet;
}

XUI_API int xuiMenuOpenForOwner(xui_widget pWidget, xui_widget pOwner)
{
	xui_menu_data_t* pData = __xuiMenuGetData(pWidget);
	int iRet;

	if ( pData == NULL || pData->pPopup == NULL || pOwner == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiMenuApplyPopupSize(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	pData->pOwner = pOwner;
	(void)xuiPopupSetOwner(pData->pPopup, pOwner);
	(void)xuiPopupSetFocusRestore(pData->pPopup, (pData->pParentMenu != NULL) ? pData->pParentMenu : pOwner);
	(void)xuiPopupSetClosePolicy(pData->pPopup,
		(pData->pParentMenu != NULL) ? XUI_POPUP_OUTSIDE_IGNORE : XUI_POPUP_OUTSIDE_CLOSE,
		(pData->pParentMenu != NULL) ? XUI_POPUP_OWNER_PASSTHROUGH : XUI_POPUP_OWNER_CLOSE,
		XUI_POPUP_ESCAPE_CLOSE);
	(void)xuiPopupClearAnchorRect(pData->pPopup);
	(void)xuiPopupSetAnchor(pData->pPopup, XUI_POPUP_ANCHOR_BOTTOM_LEFT);
	(void)xuiPopupSetDirection(pData->pPopup, XUI_POPUP_DIRECTION_RIGHT_DOWN);
	(void)xuiPopupSetGap(pData->pPopup, 2.0f);
	(void)xuiPopupSetScroll(pData->pPopup, 0.0f, 0.0f);
	pData->iHover = __xuiMenuNextEnabled(pData, -1, 1);
	iRet = xuiPopupSetOpen(pData->pPopup, 1);
	if ( iRet == XUI_OK ) {
		(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
		(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return iRet;
}

XUI_API int xuiMenuClose(xui_widget pWidget)
{
	xui_menu_data_t* pData = __xuiMenuGetData(pWidget);
	xui_menu_data_t* pParentData;

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	(void)__xuiMenuCloseSubmenu(pWidget, pData);
	pData->iHover = -1;
	if ( pData->pPopup != NULL ) {
		(void)xuiPopupSetOpen(pData->pPopup, 0);
	}
	if ( pData->pParentMenu != NULL ) {
		pParentData = __xuiMenuGetData(pData->pParentMenu);
		if ( (pParentData != NULL) && (pParentData->pOpenSubmenu == pWidget) ) {
			pParentData->pOpenSubmenu = NULL;
		}
		pData->pParentMenu = NULL;
		pData->iParentItem = -1;
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiMenuIsOpen(xui_widget pWidget)
{
	xui_menu_data_t* pData = __xuiMenuGetData(pWidget);
	return (pData != NULL && pData->pPopup != NULL) ? xuiPopupIsOpen(pData->pPopup) : 0;
}

XUI_API xui_widget xuiMenuGetPopupWidget(xui_widget pWidget)
{
	xui_menu_data_t* pData = __xuiMenuGetData(pWidget);
	return (pData != NULL) ? pData->pPopup : NULL;
}

XUI_API xui_widget xuiMenuGetContentWidget(xui_widget pWidget)
{
	return (xuiInternalWidgetIsValid(pWidget) && (__xuiMenuGetData(pWidget) != NULL)) ? pWidget : NULL;
}

XUI_API xui_widget xuiMenuGetOwner(xui_widget pWidget)
{
	xui_menu_data_t* pData = __xuiMenuGetData(pWidget);
	if ( (pData == NULL) || (pData->pPopup == NULL) ) {
		return NULL;
	}
	return xuiPopupGetOwner(pData->pPopup);
}
