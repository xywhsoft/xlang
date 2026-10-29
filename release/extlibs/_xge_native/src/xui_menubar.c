#include "xui_internal.h"

#include <ctype.h>
#include <string.h>

typedef struct xui_menubar_data_t {
	xui_font pFont;
	xui_menubar_item_t arrItems[XUI_MENUBAR_ITEM_CAPACITY];
	xui_menubar_metrics_t tMetrics;
	xui_menubar_colors_t tColors;
	xui_menu_select_proc onSelect;
	void* pSelectUser;
	int iItemCount;
	int iHover;
	int iActive;
	int iOpen;
	int iChangeCount;
} xui_menubar_data_t;

typedef struct xui_menubar_resolved_t {
	xui_font pFont;
	xui_menubar_metrics_t tMetrics;
	xui_menubar_colors_t tColors;
} xui_menubar_resolved_t;

static xui_menubar_data_t* __xuiMenuBarGetData(xui_widget pWidget);

static int __xuiMenuBarAlpha(uint32_t iColor)
{
	return (int)(iColor & 0xffu);
}

static float __xuiMenuBarMax(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static int __xuiMenuBarAsciiUpper(int iChar)
{
	return (iChar >= 'a' && iChar <= 'z') ? (iChar - 'a' + 'A') : iChar;
}

static int __xuiMenuBarItemEnabled(const xui_menubar_item_t* pItem)
{
	return (pItem != NULL) && ((pItem->iState & XUI_MENUBAR_ITEM_ENABLED) != 0);
}

static int __xuiMenuBarDescValid(const xui_menubar_desc_t* pDesc)
{
	if ( pDesc == NULL ) return 1;
	return (pDesc->iSize == 0) || (pDesc->iSize >= sizeof(*pDesc));
}

static int __xuiMenuBarMetricsValid(const xui_menubar_metrics_t* pMetrics)
{
	if ( pMetrics == NULL ) return 0;
	if ( (pMetrics->iSize != 0) && (pMetrics->iSize < sizeof(*pMetrics)) ) return 0;
	if ( (pMetrics->fHeight <= 0.0f) ||
	     (pMetrics->fPaddingX < 0.0f) ||
	     (pMetrics->fPaddingY < 0.0f) ||
	     (pMetrics->fItemPaddingX < 0.0f) ||
	     (pMetrics->fItemGap < 0.0f) ||
	     (pMetrics->fBorderWidth < 0.0f) ) {
		return 0;
	}
	return 1;
}

static int __xuiMenuBarColorsValid(const xui_menubar_colors_t* pColors)
{
	if ( pColors == NULL ) return 0;
	return (pColors->iSize == 0) || (pColors->iSize >= sizeof(*pColors));
}

static void __xuiMenuBarDefaultMetrics(xui_menubar_metrics_t* pMetrics)
{
	memset(pMetrics, 0, sizeof(*pMetrics));
	pMetrics->iSize = sizeof(*pMetrics);
	pMetrics->fHeight = 26.0f;
	pMetrics->fPaddingX = 5.0f;
	pMetrics->fPaddingY = 3.0f;
	pMetrics->fItemPaddingX = 10.0f;
	pMetrics->fItemGap = 1.0f;
	pMetrics->fBorderWidth = 1.0f;
}

static void __xuiMenuBarDefaultColors(xui_menubar_colors_t* pColors)
{
	memset(pColors, 0, sizeof(*pColors));
	pColors->iSize = sizeof(*pColors);
	pColors->iBackgroundColor = XUI_COLOR_RGBA(247, 250, 254, 255);
	pColors->iBorderColor = XUI_COLOR_RGBA(206, 220, 234, 255);
	pColors->iItemColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	pColors->iHoverColor = XUI_COLOR_RGBA(224, 238, 250, 255);
	pColors->iActiveColor = XUI_COLOR_RGBA(47, 128, 214, 255);
	pColors->iTextColor = XUI_COLOR_RGBA(35, 58, 86, 255);
	pColors->iDisabledTextColor = XUI_COLOR_RGBA(142, 152, 166, 210);
	pColors->iFocusColor = XUI_COLOR_RGBA(47, 128, 214, 255);
}

static int __xuiMenuBarTextToDisplay(const char* sText, char* sBuffer, int iBufferSize, int* pMnemonic, int* pMnemonicOffset)
{
	int i;
	int j;
	int iMnemonic;
	int iMnemonicOffset;

	if ( pMnemonic != NULL ) *pMnemonic = 0;
	if ( pMnemonicOffset != NULL ) *pMnemonicOffset = -1;
	if ( (sBuffer != NULL) && (iBufferSize > 0) ) sBuffer[0] = '\0';
	if ( sText == NULL ) return 0;
	iMnemonic = 0;
	iMnemonicOffset = -1;
	j = 0;
	for ( i = 0; sText[i] != '\0'; i++ ) {
		if ( sText[i] == '&' ) {
			if ( sText[i + 1] == '&' ) {
				if ( (sBuffer != NULL) && (j < iBufferSize - 1) ) sBuffer[j] = '&';
				j++;
				i++;
			} else if ( sText[i + 1] != '\0' ) {
				iMnemonic = __xuiMenuBarAsciiUpper((unsigned char)sText[i + 1]);
				iMnemonicOffset = j;
			}
			continue;
		}
		if ( (sBuffer != NULL) && (j < iBufferSize - 1) ) sBuffer[j] = sText[i];
		j++;
	}
	if ( (sBuffer != NULL) && (iBufferSize > 0) ) sBuffer[(j < iBufferSize) ? j : (iBufferSize - 1)] = '\0';
	if ( pMnemonic != NULL ) *pMnemonic = iMnemonic;
	if ( pMnemonicOffset != NULL ) *pMnemonicOffset = iMnemonicOffset;
	return j;
}

static int __xuiMenuBarItemMnemonic(xui_menubar_item_t* pItem)
{
	int iMnemonic;

	if ( pItem == NULL ) return 0;
	if ( pItem->iMnemonic != 0 ) return __xuiMenuBarAsciiUpper(pItem->iMnemonic);
	(void)__xuiMenuBarTextToDisplay(pItem->sText, NULL, 0, &iMnemonic, NULL);
	return iMnemonic;
}

static xui_vec2_t __xuiMenuBarMeasureText(xui_widget pWidget, xui_font pFont, const char* sText)
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

static int __xuiMenuBarStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
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

static int __xuiMenuBarStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
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

static xui_font __xuiMenuBarStyleFont(xui_widget pWidget, xui_font pBaseFont)
{
	xui_style_property_t tProperty;
	xui_font pFont;
	int iRet;

	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, "font.name", &tProperty);
	if ( (iRet == XUI_OK) &&
	     (tProperty.tValue.iType == XUI_STYLE_VALUE_STRING) &&
	     (tProperty.tValue.sText != NULL) &&
	     (tProperty.tValue.sText[0] != '\0') ) {
		pFont = xuiFindFont(xuiWidgetGetContext(pWidget), tProperty.tValue.sText);
		if ( pFont != NULL ) return pFont;
	}
	return (pBaseFont != NULL) ? pBaseFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
}

static void __xuiMenuBarResolve(xui_widget pWidget, xui_menubar_data_t* pData, xui_menubar_resolved_t* pOut)
{
	if ( (pWidget == NULL) || (pData == NULL) || (pOut == NULL) ) return;
	pOut->pFont = __xuiMenuBarStyleFont(pWidget, pData->pFont);
	pOut->tMetrics = pData->tMetrics;
	pOut->tMetrics.iSize = sizeof(pOut->tMetrics);
	pOut->tColors = pData->tColors;
	pOut->tColors.iSize = sizeof(pOut->tColors);
	(void)__xuiMenuBarStyleFloat(pWidget, "menubar.height", &pOut->tMetrics.fHeight);
	(void)__xuiMenuBarStyleFloat(pWidget, "menubar.padding.x", &pOut->tMetrics.fPaddingX);
	(void)__xuiMenuBarStyleFloat(pWidget, "menubar.padding.y", &pOut->tMetrics.fPaddingY);
	(void)__xuiMenuBarStyleFloat(pWidget, "menubar.item.padding.x", &pOut->tMetrics.fItemPaddingX);
	(void)__xuiMenuBarStyleFloat(pWidget, "menubar.item.gap", &pOut->tMetrics.fItemGap);
	(void)__xuiMenuBarStyleFloat(pWidget, "menubar.border.width", &pOut->tMetrics.fBorderWidth);
	(void)__xuiMenuBarStyleColor(pWidget, "menubar.background.color", &pOut->tColors.iBackgroundColor);
	(void)__xuiMenuBarStyleColor(pWidget, "menubar.border.color", &pOut->tColors.iBorderColor);
	(void)__xuiMenuBarStyleColor(pWidget, "menubar.item.color", &pOut->tColors.iItemColor);
	(void)__xuiMenuBarStyleColor(pWidget, "menubar.item.hover_color", &pOut->tColors.iHoverColor);
	(void)__xuiMenuBarStyleColor(pWidget, "menubar.item.active_color", &pOut->tColors.iActiveColor);
	(void)__xuiMenuBarStyleColor(pWidget, "menubar.text.color", &pOut->tColors.iTextColor);
	(void)__xuiMenuBarStyleColor(pWidget, "menubar.text.disabled_color", &pOut->tColors.iDisabledTextColor);
	(void)__xuiMenuBarStyleColor(pWidget, "menubar.focus.color", &pOut->tColors.iFocusColor);
	if ( pOut->tMetrics.fHeight < 18.0f ) pOut->tMetrics.fHeight = 18.0f;
}

static int __xuiMenuBarLayoutItems(xui_widget pWidget, xui_menubar_data_t* pData, const xui_menubar_resolved_t* pResolved, xui_vec2_t* pMeasured)
{
	xui_rect_t tRect;
	xui_vec2_t tText;
	char sDisplay[128];
	float fX;
	float fY;
	float fH;
	float fW;
	int i;

	if ( (pWidget == NULL) || (pData == NULL) || (pResolved == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	tRect = xuiWidgetGetRect(pWidget);
	fX = pResolved->tMetrics.fPaddingX;
	fY = pResolved->tMetrics.fPaddingY;
	fH = pResolved->tMetrics.fHeight - pResolved->tMetrics.fPaddingY * 2.0f;
	if ( tRect.fH > 0.0f ) {
		fH = tRect.fH - pResolved->tMetrics.fPaddingY * 2.0f;
	}
	if ( fH < 1.0f ) fH = 1.0f;
	for ( i = 0; i < pData->iItemCount; i++ ) {
		(void)__xuiMenuBarTextToDisplay(pData->arrItems[i].sText, sDisplay, (int)sizeof(sDisplay), NULL, NULL);
		tText = __xuiMenuBarMeasureText(pWidget, pResolved->pFont, sDisplay);
		fW = tText.fX + pResolved->tMetrics.fItemPaddingX * 2.0f;
		if ( fW < 20.0f ) fW = 20.0f;
		pData->arrItems[i].tRect = xuiInternalSnapRect((xui_rect_t){fX, fY, fW, fH});
		fX += fW + pResolved->tMetrics.fItemGap;
	}
	if ( pMeasured != NULL ) {
		pMeasured->fX = xuiInternalSnapSize(__xuiMenuBarMax(fX + pResolved->tMetrics.fPaddingX - pResolved->tMetrics.fItemGap, pResolved->tMetrics.fPaddingX * 2.0f));
		pMeasured->fY = xuiInternalSnapSize(pResolved->tMetrics.fHeight);
	}
	return XUI_OK;
}

static int __xuiMenuBarIndexAt(xui_menubar_data_t* pData, float fX, float fY)
{
	xui_rect_t tRect;
	int i;

	if ( pData == NULL ) return -1;
	for ( i = 0; i < pData->iItemCount; i++ ) {
		tRect = pData->arrItems[i].tRect;
		if ( __xuiMenuBarItemEnabled(&pData->arrItems[i]) &&
		     (fX >= tRect.fX) && (fY >= tRect.fY) &&
		     (fX < tRect.fX + tRect.fW) && (fY < tRect.fY + tRect.fH) ) {
			return i;
		}
	}
	return -1;
}

static int __xuiMenuBarNextEnabled(xui_menubar_data_t* pData, int iCurrent, int iStep)
{
	int i;
	int iIndex;

	if ( (pData == NULL) || (pData->iItemCount <= 0) || (iStep == 0) ) return -1;
	iIndex = iCurrent;
	for ( i = 0; i < pData->iItemCount; i++ ) {
		iIndex += iStep;
		if ( iIndex < 0 ) iIndex = pData->iItemCount - 1;
		else if ( iIndex >= pData->iItemCount ) iIndex = 0;
		if ( __xuiMenuBarItemEnabled(&pData->arrItems[iIndex]) ) return iIndex;
	}
	return -1;
}

static int __xuiMenuBarMnemonicIndex(xui_menubar_data_t* pData, int iKey)
{
	int i;
	int iMnemonic;

	if ( pData == NULL ) return -1;
	iKey = __xuiMenuBarAsciiUpper(iKey);
	for ( i = 0; i < pData->iItemCount; i++ ) {
		iMnemonic = __xuiMenuBarItemMnemonic(&pData->arrItems[i]);
		if ( __xuiMenuBarItemEnabled(&pData->arrItems[i]) && (iMnemonic != 0) && (iMnemonic == iKey) ) {
			return i;
		}
	}
	return -1;
}

static int __xuiMenuBarMenuNextEnabled(xui_widget pMenu, int iCurrent, int iStep)
{
	const xui_menu_item_t* pItem;
	int iCount;
	int i;
	int iIndex;

	if ( (pMenu == NULL) || (iStep == 0) ) return -1;
	iCount = xuiMenuGetItemCount(pMenu);
	if ( iCount <= 0 ) return -1;
	iIndex = iCurrent;
	for ( i = 0; i < iCount; i++ ) {
		iIndex += iStep;
		if ( iIndex < 0 ) iIndex = iCount - 1;
		else if ( iIndex >= iCount ) iIndex = 0;
		pItem = xuiMenuGetItem(pMenu, iIndex);
		if ( (pItem != NULL) &&
		     (pItem->iType != XUI_MENU_ITEM_SEPARATOR) &&
		     ((pItem->iState & XUI_MENU_ITEM_ENABLED) != 0u) ) {
			return iIndex;
		}
	}
	return -1;
}

static int __xuiMenuBarSyncOpen(xui_widget pWidget, xui_menubar_data_t* pData)
{
	xui_widget pMenu;

	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( (pData->iOpen < 0) || (pData->iOpen >= pData->iItemCount) ) return XUI_OK;
	pMenu = pData->arrItems[pData->iOpen].pMenu;
	if ( (pMenu == NULL) || !xuiMenuIsOpen(pMenu) ) {
		pData->iOpen = -1;
		pData->iActive = -1;
		(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return XUI_OK;
}

static int __xuiMenuBarInvalidateItem(xui_widget pWidget, xui_menubar_data_t* pData, int iIndex)
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

static int __xuiMenuBarSetHover(xui_widget pWidget, xui_menubar_data_t* pData, int iIndex)
{
	int iOldHover;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( (iIndex < 0) || (iIndex >= pData->iItemCount) || !__xuiMenuBarItemEnabled(&pData->arrItems[iIndex]) ) {
		iIndex = -1;
	}
	if ( pData->iHover == iIndex ) return XUI_OK;
	iOldHover = pData->iHover;
	pData->iHover = iIndex;
	iRet = __xuiMenuBarInvalidateItem(pWidget, pData, iOldHover);
	if ( iRet == XUI_OK ) {
		iRet = __xuiMenuBarInvalidateItem(pWidget, pData, iIndex);
	}
	return iRet;
}

static int __xuiMenuBarCloseOpen(xui_widget pWidget, xui_menubar_data_t* pData)
{
	xui_widget pMenu;

	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( (pData->iOpen >= 0) && (pData->iOpen < pData->iItemCount) ) {
		pMenu = pData->arrItems[pData->iOpen].pMenu;
		if ( pMenu != NULL ) (void)xuiMenuClose(pMenu);
	}
	pData->iOpen = -1;
	pData->iActive = -1;
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiMenuBarOpenItemInternal(xui_widget pWidget, xui_menubar_data_t* pData, int iIndex)
{
	xui_menubar_resolved_t tResolved;
	xui_rect_t tWorld;
	xui_rect_t tItem;
	xui_widget pMenu;
	xui_widget pPopup;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ||
	     !__xuiMenuBarItemEnabled(&pData->arrItems[iIndex]) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiMenuBarResolve(pWidget, pData, &tResolved);
	(void)__xuiMenuBarLayoutItems(pWidget, pData, &tResolved, NULL);
	if ( (pData->iOpen >= 0) && (pData->iOpen < pData->iItemCount) && (pData->iOpen != iIndex) ) {
		pMenu = pData->arrItems[pData->iOpen].pMenu;
		if ( pMenu != NULL ) (void)xuiMenuClose(pMenu);
	}
	pData->iHover = iIndex;
	pData->iActive = iIndex;
	pData->iOpen = iIndex;
	pData->iChangeCount++;
	pMenu = pData->arrItems[iIndex].pMenu;
	if ( pMenu != NULL ) {
		if ( pData->onSelect != NULL ) {
			(void)xuiMenuSetSelect(pMenu, pData->onSelect, pData->pSelectUser);
		}
		tWorld = xuiWidgetGetWorldRect(pWidget);
		tItem = pData->arrItems[iIndex].tRect;
		iRet = xuiMenuOpenAt(pMenu, pWidget, tWorld.fX + tItem.fX, tWorld.fY + tItem.fY + tItem.fH);
		if ( iRet != XUI_OK ) return iRet;
		pPopup = xuiMenuGetPopupWidget(pMenu);
		if ( pPopup != NULL ) {
			(void)xuiPopupSetClosePolicy(pPopup, XUI_POPUP_OUTSIDE_CLOSE, XUI_POPUP_OWNER_CLOSE, XUI_POPUP_ESCAPE_CLOSE);
			(void)xuiPopupSetFocusRestore(pPopup, pWidget);
		}
		(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
	} else {
		pData->iOpen = -1;
		pData->iActive = -1;
		if ( pData->onSelect != NULL ) {
			pData->onSelect(pWidget, iIndex, pData->arrItems[iIndex].iValue, pData->pSelectUser);
		}
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiMenuBarHotkeyEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_menubar_data_t* pData;
	int iIndex;

	(void)pUser;
	if ( (pWidget == NULL) || (pEvent == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiMenuBarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget ) {
		return XUI_OK;
	}
	iIndex = __xuiMenuBarMnemonicIndex(pData, pEvent->iKey);
	if ( iIndex >= 0 ) {
		(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
		(void)__xuiMenuBarOpenItemInternal(pWidget, pData, iIndex);
		return XUI_EVENT_DISPATCH_STOP;
	}
	return XUI_OK;
}

static int __xuiMenuBarRegisterMnemonics(xui_widget pWidget, xui_menubar_data_t* pData)
{
	xui_context pContext;
	int i;
	int iMnemonic;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pContext = xuiWidgetGetContext(pWidget);
	for ( i = 0; i < pData->iItemCount; i++ ) {
		iMnemonic = __xuiMenuBarItemMnemonic(&pData->arrItems[i]);
		if ( iMnemonic != 0 ) {
			iRet = xuiHotKeyRegister(pContext, pWidget, iMnemonic, XUI_MOD_ALT, __xuiMenuBarHotkeyEvent, NULL);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	return XUI_OK;
}

static int __xuiMenuBarResetMnemonics(xui_widget pWidget, xui_menubar_data_t* pData)
{
	xui_context pContext;
	int i;
	int iMnemonic;

	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pContext = xuiWidgetGetContext(pWidget);
	for ( i = 0; i < pData->iItemCount; i++ ) {
		iMnemonic = __xuiMenuBarItemMnemonic(&pData->arrItems[i]);
		if ( iMnemonic != 0 ) {
			(void)xuiHotKeyUnregister(pContext, pWidget, iMnemonic, XUI_MOD_ALT);
		}
	}
	return XUI_OK;
}

static int __xuiMenuBarDrawRectFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( __xuiMenuBarAlpha(iColor) == 0 ) return XUI_OK;
	return (pProxy->drawRectFill != NULL) ? pProxy->drawRectFill(pProxy, pDraw, tRect, iColor) : XUI_OK;
}

static int __xuiMenuBarDrawRectStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	if ( (fWidth <= 0.0f) || (__xuiMenuBarAlpha(iColor) == 0) ) return XUI_OK;
	return (pProxy->drawRectStroke != NULL) ? pProxy->drawRectStroke(pProxy, pDraw, tRect, fWidth, iColor) : XUI_OK;
}

static int __xuiMenuBarCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_menubar_data_t* pData;
	xui_menubar_resolved_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tRect;
	xui_rect_t tItem;
	xui_rect_t tText;
	xui_rect_t tFocus;
	uint32_t iBack;
	uint32_t iText;
	char sDisplay[128];
	int i;
	int iRet;
	int bHot;

	(void)iStateId;
	(void)pUser;
	pData = __xuiMenuBarGetData(pWidget);
	if ( (pData == NULL) || (pDraw == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	(void)__xuiMenuBarSyncOpen(pWidget, pData);
	__xuiMenuBarResolve(pWidget, pData, &tResolved);
	(void)__xuiMenuBarLayoutItems(pWidget, pData, &tResolved, NULL);
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	tRect = xuiWidgetGetRect(pWidget);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	iRet = __xuiMenuBarDrawRectFill(pProxy, pDraw, tRect, tResolved.tColors.iBackgroundColor);
	if ( iRet != XUI_OK ) return iRet;
	if ( tResolved.tMetrics.fBorderWidth > 0.0f ) {
		iRet = __xuiMenuBarDrawRectStroke(pProxy, pDraw, tRect, tResolved.tMetrics.fBorderWidth, tResolved.tColors.iBorderColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	for ( i = 0; i < pData->iItemCount; i++ ) {
		tItem = pData->arrItems[i].tRect;
		bHot = (i == pData->iHover) || (i == pData->iOpen) || (i == pData->iActive);
		iBack = tResolved.tColors.iItemColor;
		iText = tResolved.tColors.iTextColor;
		if ( !__xuiMenuBarItemEnabled(&pData->arrItems[i]) ) {
			iBack = XUI_COLOR_RGBA(0, 0, 0, 0);
			iText = tResolved.tColors.iDisabledTextColor;
		} else if ( i == pData->iOpen || i == pData->iActive ) {
			iBack = tResolved.tColors.iActiveColor;
			iText = XUI_COLOR_RGBA(255, 255, 255, 255);
		} else if ( bHot ) {
			iBack = tResolved.tColors.iHoverColor;
		}
		iRet = __xuiMenuBarDrawRectFill(pProxy, pDraw, tItem, iBack);
		if ( iRet != XUI_OK ) return iRet;
		if ( (xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget) && (pData->iOpen < 0) &&
		     (i == pData->iHover) && __xuiMenuBarItemEnabled(&pData->arrItems[i]) ) {
			tFocus = tItem;
			tFocus.fX += 4.0f;
			tFocus.fY = tItem.fY + tItem.fH - 3.0f;
			tFocus.fW -= 8.0f;
			tFocus.fH = 2.0f;
			if ( tFocus.fW > 0.0f ) {
				iRet = __xuiMenuBarDrawRectFill(pProxy, pDraw, tFocus, tResolved.tColors.iFocusColor);
				if ( iRet != XUI_OK ) return iRet;
			}
		}
		tText = tItem;
		tText.fX += tResolved.tMetrics.fItemPaddingX;
		tText.fW -= tResolved.tMetrics.fItemPaddingX * 2.0f;
		(void)__xuiMenuBarTextToDisplay(pData->arrItems[i].sText, sDisplay, (int)sizeof(sDisplay), NULL, NULL);
		if ( (tResolved.pFont != NULL) && (pProxy->drawText != NULL) && (tText.fW > 0.0f) ) {
			iRet = pProxy->drawText(pProxy, pDraw, tResolved.pFont, sDisplay, tText, iText,
				XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	return XUI_OK;
}

static int __xuiMenuBarContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_menubar_data_t* pData;
	xui_menubar_resolved_t tResolved;

	(void)tConstraint;
	(void)pUser;
	if ( (pWidget == NULL) || (pSize == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiMenuBarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiMenuBarResolve(pWidget, pData, &tResolved);
	return __xuiMenuBarLayoutItems(pWidget, pData, &tResolved, pSize);
}

static int __xuiMenuBarUpdate(xui_widget pWidget, float fDelta, void* pUser)
{
	xui_menubar_data_t* pData;
	xui_context pContext;
	xui_rect_t tWorld;
	int iIndex;

	(void)fDelta;
	(void)pUser;
	pData = __xuiMenuBarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( !xuiWidgetGetVisible(pWidget) || !xuiWidgetGetEnabled(pWidget) ) {
		return (pData->iOpen >= 0) ? __xuiMenuBarCloseOpen(pWidget, pData) : XUI_OK;
	}
	(void)__xuiMenuBarSyncOpen(pWidget, pData);
	if ( pData->iOpen >= 0 ) {
		pContext = xuiWidgetGetContext(pWidget);
		tWorld = xuiWidgetGetWorldRect(pWidget);
		iIndex = __xuiMenuBarIndexAt(pData, pContext->fPointerX - tWorld.fX, pContext->fPointerY - tWorld.fY);
		if ( (iIndex >= 0) && (iIndex != pData->iOpen) ) {
			return __xuiMenuBarOpenItemInternal(pWidget, pData, iIndex);
		}
	}
	return XUI_OK;
}

static int __xuiMenuBarEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_menubar_data_t* pData;
	xui_menubar_resolved_t tResolved;
	xui_rect_t tWorld;
	xui_widget pMenu;
	float fLocalX;
	float fLocalY;
	int iIndex;
	int iNext;

	(void)pUser;
	if ( (pWidget == NULL) || (pEvent == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE ) return XUI_OK;
	pData = __xuiMenuBarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	(void)__xuiMenuBarSyncOpen(pWidget, pData);
	__xuiMenuBarResolve(pWidget, pData, &tResolved);
	(void)__xuiMenuBarLayoutItems(pWidget, pData, &tResolved, NULL);
	tWorld = xuiWidgetGetWorldRect(pWidget);
	fLocalX = pEvent->fX - tWorld.fX;
	fLocalY = pEvent->fY - tWorld.fY;
	iIndex = __xuiMenuBarIndexAt(pData, fLocalX, fLocalY);
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_MOVE:
		(void)__xuiMenuBarSetHover(pWidget, pData, iIndex);
		if ( (iIndex >= 0) && (pData->iOpen >= 0) && (iIndex != pData->iOpen) ) {
			(void)__xuiMenuBarOpenItemInternal(pWidget, pData, iIndex);
		}
		return (pData->iOpen >= 0) ? XUI_EVENT_DISPATCH_STOP : XUI_OK;
	case XUI_EVENT_POINTER_LEAVE:
		if ( pData->iOpen < 0 ) {
			(void)__xuiMenuBarSetHover(pWidget, pData, -1);
		}
		return XUI_OK;
	case XUI_EVENT_POINTER_DOWN:
		if ( pEvent->iButton != 0 && pEvent->iButton != XUI_POINTER_BUTTON_LEFT ) return XUI_OK;
		if ( iIndex < 0 ) {
			if ( pData->iOpen >= 0 ) {
				(void)__xuiMenuBarCloseOpen(pWidget, pData);
				return XUI_EVENT_DISPATCH_STOP;
			}
			return XUI_OK;
		}
		(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
		if ( (pData->iOpen == iIndex) && (pData->arrItems[iIndex].pMenu != NULL) && xuiMenuIsOpen(pData->arrItems[iIndex].pMenu) ) {
			(void)__xuiMenuBarCloseOpen(pWidget, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		(void)__xuiMenuBarOpenItemInternal(pWidget, pData, iIndex);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_POINTER_UP:
		return (pData->iActive >= 0) ? XUI_EVENT_DISPATCH_STOP : XUI_OK;
	case XUI_EVENT_KEY_DOWN:
		if ( (pEvent->iModifiers & XUI_MOD_ALT) != 0u ) {
			iIndex = __xuiMenuBarMnemonicIndex(pData, pEvent->iKey);
			if ( iIndex >= 0 ) {
				(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
				(void)__xuiMenuBarOpenItemInternal(pWidget, pData, iIndex);
				return XUI_EVENT_DISPATCH_STOP;
			}
		}
		if ( xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) != pWidget && pData->iOpen < 0 ) return XUI_OK;
		if ( pEvent->iKey == XUI_KEY_F10 || pEvent->iKey == XUI_KEY_CONTEXT_MENU ) {
			(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
			iIndex = (pData->iHover >= 0) ? pData->iHover : __xuiMenuBarNextEnabled(pData, -1, 1);
			(void)__xuiMenuBarSetHover(pWidget, pData, iIndex);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( pEvent->iKey == XUI_KEY_LEFT || pEvent->iKey == XUI_KEY_RIGHT ) {
			iNext = __xuiMenuBarNextEnabled(pData, (pData->iHover >= 0) ? pData->iHover : pData->iOpen, (pEvent->iKey == XUI_KEY_RIGHT) ? 1 : -1);
			if ( iNext >= 0 ) {
				(void)__xuiMenuBarSetHover(pWidget, pData, iNext);
				if ( pData->iOpen >= 0 ) {
					(void)__xuiMenuBarOpenItemInternal(pWidget, pData, iNext);
				}
				return XUI_EVENT_DISPATCH_STOP;
			}
		}
		if ( pEvent->iKey == XUI_KEY_DOWN || pEvent->iKey == XUI_KEY_UP ) {
			if ( pData->iOpen >= 0 ) {
				pMenu = pData->arrItems[pData->iOpen].pMenu;
				iNext = __xuiMenuBarMenuNextEnabled(pMenu, xuiMenuGetHoverIndex(pMenu), (pEvent->iKey == XUI_KEY_DOWN) ? 1 : -1);
				if ( iNext >= 0 ) (void)xuiMenuSetHoverIndex(pMenu, iNext);
			} else {
				iIndex = (pData->iHover >= 0) ? pData->iHover : __xuiMenuBarNextEnabled(pData, -1, 1);
				if ( iIndex >= 0 ) (void)__xuiMenuBarOpenItemInternal(pWidget, pData, iIndex);
			}
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( pEvent->iKey == XUI_KEY_ENTER || pEvent->iKey == XUI_KEY_SPACE ) {
			if ( pData->iOpen >= 0 ) {
				pMenu = pData->arrItems[pData->iOpen].pMenu;
				if ( pMenu != NULL ) (void)xuiMenuCommitHover(pMenu);
			} else {
				iIndex = (pData->iHover >= 0) ? pData->iHover : __xuiMenuBarNextEnabled(pData, -1, 1);
				if ( iIndex >= 0 ) (void)__xuiMenuBarOpenItemInternal(pWidget, pData, iIndex);
			}
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( pEvent->iKey == XUI_KEY_ESCAPE ) {
			(void)__xuiMenuBarCloseOpen(pWidget, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
		(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		break;
	default:
		break;
	}
	return XUI_OK;
}

static void __xuiMenuBarDefaultLayout(xui_layout_t* pLayout)
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

static void __xuiMenuBarDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE | XUI_CACHE_UPDATE_ALL_STATES;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiMenuBarInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiMenuBarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiMenuBarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiMenuBarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiMenuBarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiMenuBarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiMenuBarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiMenuBarEvent, NULL);
	return iRet;
}

static int __xuiMenuBarInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_menubar_data_t* pData;
	const xui_menubar_desc_t* pDesc;
	int iRet;

	(void)pUser;
	if ( (pWidget == NULL) || (pTypeData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pDesc = (const xui_menubar_desc_t*)pCreateData;
	if ( !__xuiMenuBarDescValid(pDesc) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = (xui_menubar_data_t*)pTypeData;
	__xuiMenuBarDefaultMetrics(&pData->tMetrics);
	__xuiMenuBarDefaultColors(&pData->tColors);
	pData->pFont = (pDesc != NULL && pDesc->pFont != NULL) ? pDesc->pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	pData->iHover = -1;
	pData->iActive = -1;
	pData->iOpen = -1;
	if ( pDesc != NULL && pDesc->bHasMetrics && __xuiMenuBarMetricsValid(&pDesc->tMetrics) ) {
		pData->tMetrics = pDesc->tMetrics;
		pData->tMetrics.iSize = sizeof(pData->tMetrics);
	}
	if ( pDesc != NULL && pDesc->bHasColors && __xuiMenuBarColorsValid(&pDesc->tColors) ) {
		pData->tColors = pDesc->tColors;
		pData->tColors.iSize = sizeof(pData->tColors);
	}
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 0);
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_VISIBLE);
	iRet = __xuiMenuBarInitEvents(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	return XUI_OK;
}

static void __xuiMenuBarDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_menubar_data_t* pData;

	(void)pUser;
	pData = (xui_menubar_data_t*)pTypeData;
	if ( (pWidget != NULL) && (pData != NULL) ) {
		(void)__xuiMenuBarResetMnemonics(pWidget, pData);
		(void)__xuiMenuBarCloseOpen(pWidget, pData);
	}
	if ( pData != NULL ) memset(pData, 0, sizeof(*pData));
}

static xui_menubar_data_t* __xuiMenuBarGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) return NULL;
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "menubar");
	if ( pType == NULL ) pType = xuiWidgetFindType(pContext, "menuBar");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) return NULL;
	return (xui_menubar_data_t*)xuiWidgetGetTypeData(pWidget);
}

static void __xuiMenuBarRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
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

static void __xuiMenuBarRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;
	uint32_t iLayoutDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	iLayoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiMenuBarRegisterStyleProperty(pContext, pType, "font.name", XUI_STYLE_VALUE_STRING, iLayoutDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiMenuBarRegisterStyleProperty(pContext, pType, "menubar.background.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiMenuBarRegisterStyleProperty(pContext, pType, "menubar.border.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiMenuBarRegisterStyleProperty(pContext, pType, "menubar.item.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiMenuBarRegisterStyleProperty(pContext, pType, "menubar.item.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiMenuBarRegisterStyleProperty(pContext, pType, "menubar.item.active_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiMenuBarRegisterStyleProperty(pContext, pType, "menubar.text.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiMenuBarRegisterStyleProperty(pContext, pType, "menubar.text.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiMenuBarRegisterStyleProperty(pContext, pType, "menubar.focus.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiMenuBarRegisterStyleProperty(pContext, pType, "menubar.height", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiMenuBarRegisterStyleProperty(pContext, pType, "menubar.padding.x", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiMenuBarRegisterStyleProperty(pContext, pType, "menubar.padding.y", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiMenuBarRegisterStyleProperty(pContext, pType, "menubar.item.padding.x", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiMenuBarRegisterStyleProperty(pContext, pType, "menubar.item.gap", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiMenuBarRegisterStyleProperty(pContext, pType, "menubar.border.width", XUI_STYLE_VALUE_FLOAT, iPaintDirty, 0);
}

XUI_API xui_widget_type xuiMenuBarGetType(xui_context pContext)
{
	xui_widget_type pType;
	xui_widget_type_desc_t tDesc;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) return NULL;
	pType = xuiWidgetFindType(pContext, "menubar");
	if ( pType != NULL ) return pType;
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "menubar";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_menubar_data_t);
	tDesc.onInit = __xuiMenuBarInit;
	tDesc.onDestroy = __xuiMenuBarDestroy;
	tDesc.onContentMeasure = __xuiMenuBarContentMeasure;
	tDesc.onCacheRender = __xuiMenuBarCacheRender;
	tDesc.onUpdate = __xuiMenuBarUpdate;
	__xuiMenuBarDefaultLayout(&tDesc.tLayout);
	__xuiMenuBarDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) return NULL;
	__xuiMenuBarRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiMenuBarCreate(xui_context pContext, xui_widget* ppWidget, const xui_menubar_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( !xuiInternalContextIsValid(pContext) || (ppWidget == NULL) || !__xuiMenuBarDescValid(pDesc) ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppWidget = NULL;
	pType = xuiMenuBarGetType(pContext);
	if ( pType == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiMenuBarSetItems(xui_widget pWidget, const xui_menubar_item_t* pItems, int iCount)
{
	xui_menubar_data_t* pData = __xuiMenuBarGetData(pWidget);
	int i;

	if ( (pData == NULL) || (iCount < 0) || (iCount > XUI_MENUBAR_ITEM_CAPACITY) || ((iCount > 0) && (pItems == NULL)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	(void)__xuiMenuBarResetMnemonics(pWidget, pData);
	(void)__xuiMenuBarCloseOpen(pWidget, pData);
	memset(pData->arrItems, 0, sizeof(pData->arrItems));
	for ( i = 0; i < iCount; i++ ) {
		pData->arrItems[i] = pItems[i];
		if ( pData->arrItems[i].sText == NULL ) pData->arrItems[i].sText = "";
		if ( pData->arrItems[i].iMnemonic != 0 ) pData->arrItems[i].iMnemonic = __xuiMenuBarAsciiUpper(pData->arrItems[i].iMnemonic);
	}
	pData->iItemCount = iCount;
	pData->iHover = -1;
	pData->iActive = -1;
	pData->iOpen = -1;
	pData->iChangeCount++;
	(void)__xuiMenuBarRegisterMnemonics(pWidget, pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiMenuBarAddItem(xui_widget pWidget, const char* sText, xui_widget pMenu, int iValue)
{
	xui_menubar_data_t* pData = __xuiMenuBarGetData(pWidget);
	xui_menubar_item_t* pItem;

	if ( (pData == NULL) || (pData->iItemCount >= XUI_MENUBAR_ITEM_CAPACITY) ) return XUI_ERROR_INVALID_ARGUMENT;
	pItem = &pData->arrItems[pData->iItemCount++];
	memset(pItem, 0, sizeof(*pItem));
	pItem->sText = (sText != NULL) ? sText : "";
	pItem->iState = XUI_MENUBAR_ITEM_ENABLED;
	pItem->iValue = iValue;
	pItem->pMenu = pMenu;
	pItem->iMnemonic = __xuiMenuBarItemMnemonic(pItem);
	pData->iChangeCount++;
	if ( pItem->iMnemonic != 0 ) {
		(void)xuiHotKeyRegister(xuiWidgetGetContext(pWidget), pWidget, pItem->iMnemonic, XUI_MOD_ALT, __xuiMenuBarHotkeyEvent, NULL);
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiMenuBarClear(xui_widget pWidget)
{
	xui_menubar_data_t* pData = __xuiMenuBarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	(void)__xuiMenuBarResetMnemonics(pWidget, pData);
	(void)__xuiMenuBarCloseOpen(pWidget, pData);
	memset(pData->arrItems, 0, sizeof(pData->arrItems));
	pData->iItemCount = 0;
	pData->iHover = -1;
	pData->iActive = -1;
	pData->iOpen = -1;
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiMenuBarGetItemCount(xui_widget pWidget)
{
	xui_menubar_data_t* pData = __xuiMenuBarGetData(pWidget);
	return (pData != NULL) ? pData->iItemCount : 0;
}

XUI_API const xui_menubar_item_t* xuiMenuBarGetItem(xui_widget pWidget, int iIndex)
{
	xui_menubar_data_t* pData = __xuiMenuBarGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return NULL;
	return &pData->arrItems[iIndex];
}

XUI_API xui_rect_t xuiMenuBarGetItemRect(xui_widget pWidget, int iIndex)
{
	xui_menubar_data_t* pData = __xuiMenuBarGetData(pWidget);
	xui_menubar_resolved_t tResolved;
	xui_rect_t tEmpty;

	memset(&tEmpty, 0, sizeof(tEmpty));
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return tEmpty;
	__xuiMenuBarResolve(pWidget, pData, &tResolved);
	(void)__xuiMenuBarLayoutItems(pWidget, pData, &tResolved, NULL);
	return pData->arrItems[iIndex].tRect;
}

XUI_API int xuiMenuBarSetItemMenu(xui_widget pWidget, int iIndex, xui_widget pMenu)
{
	xui_menubar_data_t* pData = __xuiMenuBarGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrItems[iIndex].pMenu = pMenu;
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiMenuBarSetItemEnabled(xui_widget pWidget, int iIndex, int bEnabled)
{
	xui_menubar_data_t* pData = __xuiMenuBarGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( bEnabled ) pData->arrItems[iIndex].iState |= XUI_MENUBAR_ITEM_ENABLED;
	else {
		pData->arrItems[iIndex].iState &= ~XUI_MENUBAR_ITEM_ENABLED;
		if ( pData->iHover == iIndex ) pData->iHover = -1;
		if ( pData->iOpen == iIndex ) (void)__xuiMenuBarCloseOpen(pWidget, pData);
	}
	pData->iChangeCount++;
	(void)__xuiMenuBarResetMnemonics(pWidget, pData);
	(void)__xuiMenuBarRegisterMnemonics(pWidget, pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiMenuBarIsItemEnabled(xui_widget pWidget, int iIndex)
{
	xui_menubar_data_t* pData = __xuiMenuBarGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return 0;
	return __xuiMenuBarItemEnabled(&pData->arrItems[iIndex]);
}

XUI_API int xuiMenuBarSetItemMnemonic(xui_widget pWidget, int iIndex, int iMnemonic)
{
	xui_menubar_data_t* pData = __xuiMenuBarGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	(void)__xuiMenuBarResetMnemonics(pWidget, pData);
	pData->arrItems[iIndex].iMnemonic = __xuiMenuBarAsciiUpper(iMnemonic);
	pData->iChangeCount++;
	(void)__xuiMenuBarRegisterMnemonics(pWidget, pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiMenuBarGetHoverIndex(xui_widget pWidget)
{
	xui_menubar_data_t* pData = __xuiMenuBarGetData(pWidget);
	return (pData != NULL) ? pData->iHover : -1;
}

XUI_API int xuiMenuBarSetHoverIndex(xui_widget pWidget, int iIndex)
{
	xui_menubar_data_t* pData = __xuiMenuBarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiMenuBarSetHover(pWidget, pData, iIndex);
}

XUI_API int xuiMenuBarGetActiveIndex(xui_widget pWidget)
{
	xui_menubar_data_t* pData = __xuiMenuBarGetData(pWidget);
	return (pData != NULL) ? pData->iActive : -1;
}

XUI_API int xuiMenuBarGetOpenIndex(xui_widget pWidget)
{
	xui_menubar_data_t* pData = __xuiMenuBarGetData(pWidget);
	if ( pData == NULL ) return -1;
	(void)__xuiMenuBarSyncOpen(pWidget, pData);
	return pData->iOpen;
}

XUI_API int xuiMenuBarOpenItem(xui_widget pWidget, int iIndex)
{
	xui_menubar_data_t* pData = __xuiMenuBarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiMenuBarOpenItemInternal(pWidget, pData, iIndex);
}

XUI_API int xuiMenuBarClose(xui_widget pWidget)
{
	xui_menubar_data_t* pData = __xuiMenuBarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiMenuBarCloseOpen(pWidget, pData);
}

XUI_API int xuiMenuBarIsOpen(xui_widget pWidget)
{
	return xuiMenuBarGetOpenIndex(pWidget) >= 0;
}

XUI_API xui_widget xuiMenuBarGetOpenMenu(xui_widget pWidget)
{
	xui_menubar_data_t* pData = __xuiMenuBarGetData(pWidget);
	if ( pData == NULL ) return NULL;
	(void)__xuiMenuBarSyncOpen(pWidget, pData);
	return (pData->iOpen >= 0 && pData->iOpen < pData->iItemCount) ? pData->arrItems[pData->iOpen].pMenu : NULL;
}

XUI_API int xuiMenuBarSetSelect(xui_widget pWidget, xui_menu_select_proc onSelect, void* pUser)
{
	xui_menubar_data_t* pData = __xuiMenuBarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onSelect = onSelect;
	pData->pSelectUser = pUser;
	return XUI_OK;
}

XUI_API int xuiMenuBarSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_menubar_data_t* pData = __xuiMenuBarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pFont = pFont;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_font xuiMenuBarGetFont(xui_widget pWidget)
{
	xui_menubar_data_t* pData = __xuiMenuBarGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API int xuiMenuBarSetMetrics(xui_widget pWidget, const xui_menubar_metrics_t* pMetrics)
{
	xui_menubar_data_t* pData = __xuiMenuBarGetData(pWidget);
	if ( (pData == NULL) || !__xuiMenuBarMetricsValid(pMetrics) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->tMetrics = *pMetrics;
	pData->tMetrics.iSize = sizeof(pData->tMetrics);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiMenuBarGetMetrics(xui_widget pWidget, xui_menubar_metrics_t* pMetrics)
{
	xui_menubar_data_t* pData = __xuiMenuBarGetData(pWidget);
	if ( (pData == NULL) || (pMetrics == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pMetrics = pData->tMetrics;
	pMetrics->iSize = sizeof(*pMetrics);
	return XUI_OK;
}

XUI_API int xuiMenuBarSetColors(xui_widget pWidget, const xui_menubar_colors_t* pColors)
{
	xui_menubar_data_t* pData = __xuiMenuBarGetData(pWidget);
	if ( (pData == NULL) || !__xuiMenuBarColorsValid(pColors) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->tColors = *pColors;
	pData->tColors.iSize = sizeof(pData->tColors);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiMenuBarGetColors(xui_widget pWidget, xui_menubar_colors_t* pColors)
{
	xui_menubar_data_t* pData = __xuiMenuBarGetData(pWidget);
	if ( (pData == NULL) || (pColors == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	*pColors = pData->tColors;
	pColors->iSize = sizeof(*pColors);
	return XUI_OK;
}

XUI_API int xuiMenuBarGetState(xui_widget pWidget)
{
	xui_menubar_data_t* pData = __xuiMenuBarGetData(pWidget);
	int iState;

	if ( pData == NULL ) return 0;
	iState = (int)xuiWidgetGetInputState(pWidget);
	if ( xuiMenuBarIsOpen(pWidget) ) iState |= XUI_MENUBAR_STATE_OPEN;
	return iState;
}

XUI_API int xuiMenuBarGetChangeCount(xui_widget pWidget)
{
	xui_menubar_data_t* pData = __xuiMenuBarGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}
