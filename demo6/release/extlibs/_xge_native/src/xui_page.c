#include "xui_internal.h"

#include <stdio.h>
#include <string.h>

#define XUI_PAGE_ITEM_CAPACITY 32

typedef struct xui_page_item_t {
	xui_rect_t tRect;
	int iType;
	int iPage;
	int bEnabled;
	char sText[16];
} xui_page_item_t;

typedef struct xui_page_data_t {
	xui_page_change_proc onChange;
	void* pChangeUser;
	xui_font pFont;
	xui_page_item_t arrItems[XUI_PAGE_ITEM_CAPACITY];
	int iItemCount;
	int iPageCount;
	int iCurrentPage;
	int iWindowSize;
	int iHover;
	int iActive;
	int iChangeCount;
	char sFirstText[16];
	char sLastText[16];
	char sPrevText[16];
	char sNextText[16];
	float fItemHeight;
	float fPageWidth;
	float fTextWidth;
	float fNavWidth;
	float fEllipsisWidth;
	uint32_t iBackgroundColor;
	uint32_t iBorderColor;
	uint32_t iTextColor;
	uint32_t iHoverColor;
	uint32_t iActiveColor;
	uint32_t iCurrentColor;
	uint32_t iCurrentTextColor;
	uint32_t iDisabledTextColor;
	uint32_t iFocusColor;
} xui_page_data_t;

static int __xuiPageDescValid(const xui_page_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	return (pDesc->iSize == 0) || (pDesc->iSize >= sizeof(*pDesc));
}

static int __xuiPageClampInt(int iValue, int iMin, int iMax)
{
	if ( iValue < iMin ) return iMin;
	if ( iValue > iMax ) return iMax;
	return iValue;
}

static int __xuiPageNormalizeWindowSize(int iWindowSize)
{
	int iMax;

	iMax = XUI_PAGE_ITEM_CAPACITY - 6;
	if ( iMax < 1 ) iMax = 1;
	if ( iWindowSize < 1 ) iWindowSize = 1;
	if ( iWindowSize > iMax ) iWindowSize = iMax;
	if ( (iWindowSize % 2) == 0 ) {
		iWindowSize++;
		if ( iWindowSize > iMax ) {
			iWindowSize -= 2;
		}
	}
	if ( iWindowSize < 1 ) iWindowSize = 1;
	return iWindowSize;
}

static int __xuiPageClampPage(const xui_page_data_t* pData, int iPage)
{
	int iPageCount;

	iPageCount = (pData != NULL && pData->iPageCount > 0) ? pData->iPageCount : 1;
	return __xuiPageClampInt(iPage, 1, iPageCount);
}

static int __xuiPageColorAlpha(uint32_t iColor)
{
	return (int)(iColor & 0xffu);
}

static void __xuiPageCopyText(char* sDst, int iCapacity, const char* sText, const char* sFallback)
{
	const char* sValue;

	if ( (sDst == NULL) || (iCapacity <= 0) ) {
		return;
	}
	sValue = (sText != NULL) ? sText : sFallback;
	snprintf(sDst, (size_t)iCapacity, "%s", (sValue != NULL) ? sValue : "");
	sDst[iCapacity - 1] = 0;
}

static xui_page_data_t* __xuiPageGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return NULL;
	}
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "page");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_page_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiPageStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
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

static int __xuiPageStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
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

static xui_font __xuiPageStyleFont(xui_widget pWidget, xui_font pBaseFont)
{
	xui_style_property_t tProperty;
	xui_font pFont;
	int iRet;

	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, "font.name", &tProperty);
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_STRING) && (tProperty.tValue.sText != NULL) ) {
		pFont = xuiFindFont(xuiWidgetGetContext(pWidget), tProperty.tValue.sText);
		if ( pFont != NULL ) {
			return pFont;
		}
	}
	return pBaseFont;
}

static void __xuiPageResolve(xui_widget pWidget, const xui_page_data_t* pData, xui_page_data_t* pResolved)
{
	*pResolved = *pData;
	pResolved->pFont = __xuiPageStyleFont(pWidget, (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget)));
	(void)__xuiPageStyleColor(pWidget, "page.background.color", &pResolved->iBackgroundColor);
	(void)__xuiPageStyleColor(pWidget, "page.border.color", &pResolved->iBorderColor);
	(void)__xuiPageStyleColor(pWidget, "page.text.color", &pResolved->iTextColor);
	(void)__xuiPageStyleColor(pWidget, "page.hover.color", &pResolved->iHoverColor);
	(void)__xuiPageStyleColor(pWidget, "page.active.color", &pResolved->iActiveColor);
	(void)__xuiPageStyleColor(pWidget, "page.current.color", &pResolved->iCurrentColor);
	(void)__xuiPageStyleColor(pWidget, "page.current_text.color", &pResolved->iCurrentTextColor);
	(void)__xuiPageStyleColor(pWidget, "page.disabled_text.color", &pResolved->iDisabledTextColor);
	(void)__xuiPageStyleColor(pWidget, "page.focus.color", &pResolved->iFocusColor);
	(void)__xuiPageStyleFloat(pWidget, "page.item.height", &pResolved->fItemHeight);
	(void)__xuiPageStyleFloat(pWidget, "page.width.page", &pResolved->fPageWidth);
	(void)__xuiPageStyleFloat(pWidget, "page.width.text", &pResolved->fTextWidth);
	(void)__xuiPageStyleFloat(pWidget, "page.width.nav", &pResolved->fNavWidth);
	(void)__xuiPageStyleFloat(pWidget, "page.width.ellipsis", &pResolved->fEllipsisWidth);
	if ( pResolved->fItemHeight <= 0.0f ) pResolved->fItemHeight = 28.0f;
	if ( pResolved->fPageWidth <= 0.0f ) pResolved->fPageWidth = 38.0f;
	if ( pResolved->fTextWidth <= 0.0f ) pResolved->fTextWidth = 56.0f;
	if ( pResolved->fNavWidth <= 0.0f ) pResolved->fNavWidth = 46.0f;
	if ( pResolved->fEllipsisWidth <= 0.0f ) pResolved->fEllipsisWidth = 32.0f;
}

static uint32_t __xuiPageBaseState(xui_widget pWidget, const xui_page_data_t* pData)
{
	uint32_t iState;

	iState = xuiWidgetGetInputState(pWidget);
	if ( !xuiWidgetGetEnabled(pWidget) ) {
		iState |= XUI_WIDGET_STATE_DISABLED;
	}
	if ( (iState & XUI_WIDGET_STATE_DISABLED) != 0 ) {
		return XUI_WIDGET_STATE_DISABLED;
	}
	iState &= (XUI_WIDGET_STATE_HOVER | XUI_WIDGET_STATE_FOCUS);
	if ( (pData != NULL) && (pData->iActive >= 0) ) {
		iState |= XUI_WIDGET_STATE_ACTIVE;
	}
	return iState;
}

static uint32_t __xuiPageStateId(xui_widget pWidget, const xui_page_data_t* pData)
{
	uint32_t iState;

	iState = __xuiPageBaseState(pWidget, pData);
	if ( (iState & XUI_WIDGET_STATE_DISABLED) != 0 ) return XUI_WIDGET_STATE_DISABLED;
	if ( (iState & XUI_WIDGET_STATE_ACTIVE) != 0 ) return XUI_WIDGET_STATE_ACTIVE;
	if ( (iState & XUI_WIDGET_STATE_HOVER) != 0 ) return XUI_WIDGET_STATE_HOVER;
	if ( (iState & XUI_WIDGET_STATE_FOCUS) != 0 ) return XUI_WIDGET_STATE_FOCUS;
	return 0;
}

static int __xuiPageSyncState(xui_widget pWidget, xui_page_data_t* pData)
{
	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return xuiWidgetSetStateId(pWidget, __xuiPageStateId(pWidget, pData));
}

static int __xuiPageRectContains(xui_rect_t tRect, float fX, float fY)
{
	return (tRect.fW > 0.0f) &&
	       (tRect.fH > 0.0f) &&
	       (fX >= tRect.fX) &&
	       (fY >= tRect.fY) &&
	       (fX < (tRect.fX + tRect.fW)) &&
	       (fY < (tRect.fY + tRect.fH));
}

static float __xuiPageWidthForType(const xui_page_data_t* pData, int iType)
{
	if ( pData == NULL ) {
		return 0.0f;
	}
	if ( iType == XUI_PAGE_ITEM_PAGE ) return pData->fPageWidth;
	if ( iType == XUI_PAGE_ITEM_ELLIPSIS ) return pData->fEllipsisWidth;
	if ( (iType == XUI_PAGE_ITEM_PREV) || (iType == XUI_PAGE_ITEM_NEXT) ) return pData->fNavWidth;
	return pData->fTextWidth;
}

static void __xuiPageAddItem(xui_page_data_t* pData, int iType, int iPage, const char* sText, int bEnabled, xui_rect_t tBase, float* pX)
{
	xui_page_item_t* pItem;
	float fW;

	if ( (pData == NULL) || (pX == NULL) || (pData->iItemCount >= XUI_PAGE_ITEM_CAPACITY) ) {
		return;
	}
	fW = __xuiPageWidthForType(pData, iType);
	if ( fW < 1.0f ) fW = 1.0f;
	pItem = &pData->arrItems[pData->iItemCount++];
	memset(pItem, 0, sizeof(*pItem));
	pItem->iType = iType;
	pItem->iPage = iPage;
	pItem->bEnabled = (bEnabled != 0);
	pItem->tRect = xuiInternalSnapRect((xui_rect_t){*pX, tBase.fY, fW, tBase.fH});
	__xuiPageCopyText(pItem->sText, (int)sizeof(pItem->sText), sText, "");
	*pX += fW;
}

static void __xuiPageAddPageItem(xui_page_data_t* pData, int iPage, xui_rect_t tBase, float* pX)
{
	char sText[16];

	snprintf(sText, sizeof(sText), "%d", iPage);
	sText[sizeof(sText) - 1] = 0;
	__xuiPageAddItem(pData, XUI_PAGE_ITEM_PAGE, iPage, sText, iPage != pData->iCurrentPage, tBase, pX);
}

static float __xuiPageBuildItems(xui_page_data_t* pData, xui_rect_t tContent)
{
	float fX;
	int iWindowSize;
	int iHalf;
	int iStart;
	int iEnd;
	int iPage;

	if ( pData == NULL ) {
		return 0.0f;
	}
	pData->iItemCount = 0;
	if ( tContent.fH <= 0.0f ) {
		tContent.fH = pData->fItemHeight;
	}
	fX = tContent.fX;
	__xuiPageAddItem(pData, XUI_PAGE_ITEM_PREV, pData->iCurrentPage - 1, pData->sPrevText, pData->iCurrentPage > 1, tContent, &fX);
	__xuiPageAddItem(pData, XUI_PAGE_ITEM_FIRST, 1, pData->sFirstText, pData->iCurrentPage > 1, tContent, &fX);
	iWindowSize = __xuiPageNormalizeWindowSize(pData->iWindowSize);
	if ( iWindowSize > pData->iPageCount ) {
		iWindowSize = pData->iPageCount;
	}
	iHalf = iWindowSize / 2;
	iStart = pData->iCurrentPage - iHalf;
	iEnd = iStart + iWindowSize - 1;
	if ( iStart < 1 ) {
		iStart = 1;
		iEnd = iWindowSize;
	}
	if ( iEnd > pData->iPageCount ) {
		iEnd = pData->iPageCount;
		iStart = iEnd - iWindowSize + 1;
		if ( iStart < 1 ) iStart = 1;
	}
	if ( iStart > 1 ) {
		__xuiPageAddItem(pData, XUI_PAGE_ITEM_ELLIPSIS, 0, "...", 0, tContent, &fX);
	}
	for ( iPage = iStart; iPage <= iEnd; iPage++ ) {
		__xuiPageAddPageItem(pData, iPage, tContent, &fX);
	}
	if ( iEnd < pData->iPageCount ) {
		__xuiPageAddItem(pData, XUI_PAGE_ITEM_ELLIPSIS, 0, "...", 0, tContent, &fX);
	}
	__xuiPageAddItem(pData, XUI_PAGE_ITEM_LAST, pData->iPageCount, pData->sLastText, pData->iCurrentPage < pData->iPageCount, tContent, &fX);
	__xuiPageAddItem(pData, XUI_PAGE_ITEM_NEXT, pData->iCurrentPage + 1, pData->sNextText, pData->iCurrentPage < pData->iPageCount, tContent, &fX);
	return fX - tContent.fX;
}

static int __xuiPageHitTest(xui_widget pWidget, xui_page_data_t* pData, float fLocalX, float fLocalY)
{
	xui_page_data_t tResolved;
	xui_rect_t tContent;
	int i;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return -1;
	}
	__xuiPageResolve(pWidget, pData, &tResolved);
	tContent = xuiWidgetGetContentRect(pWidget);
	(void)__xuiPageBuildItems(&tResolved, tContent);
	pData->iItemCount = tResolved.iItemCount;
	memcpy(pData->arrItems, tResolved.arrItems, sizeof(pData->arrItems));
	for ( i = 0; i < pData->iItemCount; i++ ) {
		if ( __xuiPageRectContains(pData->arrItems[i].tRect, fLocalX, fLocalY) ) {
			return i;
		}
	}
	return -1;
}

static int __xuiPageSetCurrentInternal(xui_widget pWidget, xui_page_data_t* pData, int iPage, int bNotify)
{
	int iOldPage;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iOldPage = pData->iCurrentPage;
	iPage = __xuiPageClampPage(pData, iPage);
	if ( iPage == iOldPage ) {
		return XUI_OK;
	}
	pData->iCurrentPage = iPage;
	pData->iHover = -1;
	pData->iActive = -1;
	pData->iChangeCount++;
	if ( bNotify && (pData->onChange != NULL) ) {
		pData->onChange(pWidget, iOldPage, pData->iCurrentPage, pData->pChangeUser);
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiPageActivateItem(xui_widget pWidget, xui_page_data_t* pData, int iIndex)
{
	xui_page_item_t* pItem;
	int iTarget;

	if ( (pWidget == NULL) || (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) {
		return XUI_OK;
	}
	pItem = &pData->arrItems[iIndex];
	if ( pItem->bEnabled == 0 ) {
		return XUI_OK;
	}
	iTarget = pData->iCurrentPage;
	if ( pItem->iType == XUI_PAGE_ITEM_PREV ) iTarget = pData->iCurrentPage - 1;
	else if ( pItem->iType == XUI_PAGE_ITEM_NEXT ) iTarget = pData->iCurrentPage + 1;
	else if ( pItem->iType == XUI_PAGE_ITEM_FIRST ) iTarget = 1;
	else if ( pItem->iType == XUI_PAGE_ITEM_LAST ) iTarget = pData->iPageCount;
	else if ( pItem->iType == XUI_PAGE_ITEM_PAGE ) iTarget = pItem->iPage;
	return __xuiPageSetCurrentInternal(pWidget, pData, iTarget, 1);
}

static uint32_t __xuiPageItemBackground(const xui_page_data_t* pData, int iIndex, uint32_t iStateId)
{
	const xui_page_item_t* pItem;

	if ( pData == NULL ) return 0;
	if ( (iStateId & XUI_WIDGET_STATE_DISABLED) != 0 ) return pData->iBackgroundColor;
	if ( (iIndex < 0) || (iIndex >= pData->iItemCount) ) return pData->iBackgroundColor;
	pItem = &pData->arrItems[iIndex];
	if ( (pItem->iType == XUI_PAGE_ITEM_PAGE) && (pItem->iPage == pData->iCurrentPage) ) return pData->iCurrentColor;
	if ( (pItem->bEnabled != 0) && (iIndex == pData->iActive) ) return pData->iActiveColor;
	if ( (pItem->bEnabled != 0) && (iIndex == pData->iHover) ) return pData->iHoverColor;
	return pData->iBackgroundColor;
}

static uint32_t __xuiPageItemTextColor(const xui_page_data_t* pData, int iIndex, uint32_t iStateId)
{
	const xui_page_item_t* pItem;

	if ( pData == NULL ) return 0;
	if ( (iStateId & XUI_WIDGET_STATE_DISABLED) != 0 ) return pData->iDisabledTextColor;
	if ( (iIndex < 0) || (iIndex >= pData->iItemCount) ) return pData->iTextColor;
	pItem = &pData->arrItems[iIndex];
	if ( (pItem->iType == XUI_PAGE_ITEM_PAGE) && (pItem->iPage == pData->iCurrentPage) ) return pData->iCurrentTextColor;
	return pItem->bEnabled ? pData->iTextColor : pData->iDisabledTextColor;
}

static int __xuiPagePointerInside(xui_widget pWidget, const xui_event_t* pEvent, float* pLocalX, float* pLocalY)
{
	xui_rect_t tWorld;
	xui_rect_t tContent;
	float fLocalX;
	float fLocalY;

	if ( (pWidget == NULL) || (pEvent == NULL) ) return 0;
	tWorld = xuiWidgetGetWorldRect(pWidget);
	tContent = xuiWidgetGetContentRect(pWidget);
	fLocalX = pEvent->fX - tWorld.fX;
	fLocalY = pEvent->fY - tWorld.fY;
	if ( pLocalX != NULL ) *pLocalX = fLocalX;
	if ( pLocalY != NULL ) *pLocalY = fLocalY;
	return __xuiPageRectContains(tContent, fLocalX, fLocalY);
}

static int __xuiPagePointerMove(xui_widget pWidget, xui_page_data_t* pData, const xui_event_t* pEvent)
{
	float fLocalX;
	float fLocalY;
	int bInside;
	int iHit;

	bInside = __xuiPagePointerInside(pWidget, pEvent, &fLocalX, &fLocalY);
	iHit = bInside ? __xuiPageHitTest(pWidget, pData, fLocalX, fLocalY) : -1;
	pData->iHover = (iHit >= 0 && pData->arrItems[iHit].bEnabled) ? iHit : -1;
	(void)__xuiPageSyncState(pWidget, pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiPagePointerDown(xui_widget pWidget, xui_page_data_t* pData, const xui_event_t* pEvent)
{
	xui_context pContext;
	float fLocalX;
	float fLocalY;
	int iHit;

	if ( pEvent->iButton != XUI_POINTER_BUTTON_LEFT ) return XUI_OK;
	if ( !xuiWidgetGetEnabled(pWidget) || !__xuiPagePointerInside(pWidget, pEvent, &fLocalX, &fLocalY) ) return XUI_OK;
	iHit = __xuiPageHitTest(pWidget, pData, fLocalX, fLocalY);
	if ( (iHit < 0) || (pData->arrItems[iHit].bEnabled == 0) ) return XUI_OK;
	pContext = xuiWidgetGetContext(pWidget);
	(void)xuiSetFocusWidget(pContext, pWidget);
	(void)xuiSetPointerCapture(pContext, pWidget);
	pData->iActive = iHit;
	pData->iHover = iHit;
	(void)__xuiPageSyncState(pWidget, pData);
	(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiPagePointerUp(xui_widget pWidget, xui_page_data_t* pData, const xui_event_t* pEvent)
{
	xui_context pContext;
	float fLocalX;
	float fLocalY;
	int bInside;
	int iHit;
	int iOldActive;
	int iRet;

	if ( pEvent->iButton != XUI_POINTER_BUTTON_LEFT ) return XUI_OK;
	pContext = xuiWidgetGetContext(pWidget);
	iOldActive = pData->iActive;
	bInside = __xuiPagePointerInside(pWidget, pEvent, &fLocalX, &fLocalY);
	iHit = bInside ? __xuiPageHitTest(pWidget, pData, fLocalX, fLocalY) : -1;
	pData->iActive = -1;
	pData->iHover = (iHit >= 0 && pData->arrItems[iHit].bEnabled) ? iHit : -1;
	(void)__xuiPageSyncState(pWidget, pData);
	if ( xuiGetPointerCapture(pContext) == pWidget ) {
		(void)xuiReleasePointerCapture(pContext, pWidget);
	}
	if ( (iOldActive >= 0) && (iOldActive == iHit) ) {
		iRet = __xuiPageActivateItem(pWidget, pData, iHit);
		if ( iRet != XUI_OK ) return iRet;
		return XUI_EVENT_DISPATCH_STOP;
	}
	(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return (iOldActive >= 0) ? XUI_EVENT_DISPATCH_STOP : XUI_OK;
}

static int __xuiPageCancelCapture(xui_widget pWidget, xui_page_data_t* pData)
{
	xui_context pContext;

	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iActive = -1;
	(void)__xuiPageSyncState(pWidget, pData);
	pContext = xuiWidgetGetContext(pWidget);
	if ( xuiGetPointerCapture(pContext) == pWidget ) {
		(void)xuiReleasePointerCapture(pContext, pWidget);
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static void __xuiPageCancelAction(xui_widget pWidget, void* pUser)
{
	(void)pUser;
	(void)__xuiPageCancelCapture(pWidget, __xuiPageGetData(pWidget));
}

static int __xuiPageKeyDown(xui_widget pWidget, xui_page_data_t* pData, const xui_event_t* pEvent)
{
	if ( !xuiWidgetGetEnabled(pWidget) ) return XUI_OK;
	switch ( pEvent->iKey ) {
	case XUI_KEY_ESCAPE:
		(void)__xuiPageCancelCapture(pWidget, pData);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_LEFT:
	case XUI_KEY_UP:
		(void)__xuiPageSetCurrentInternal(pWidget, pData, pData->iCurrentPage - 1, 1);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_RIGHT:
	case XUI_KEY_DOWN:
		(void)__xuiPageSetCurrentInternal(pWidget, pData, pData->iCurrentPage + 1, 1);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_HOME:
		(void)__xuiPageSetCurrentInternal(pWidget, pData, 1, 1);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_END:
		(void)__xuiPageSetCurrentInternal(pWidget, pData, pData->iPageCount, 1);
		return XUI_EVENT_DISPATCH_STOP;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiPageEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_page_data_t* pData;
	int iRet;

	(void)pUser;
	if ( (pWidget == NULL) || (pEvent == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiPageGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_ENTER:
	case XUI_EVENT_POINTER_MOVE:
		return __xuiPagePointerMove(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_LEAVE:
		if ( pData->iActive < 0 ) {
			pData->iHover = -1;
			(void)__xuiPageSyncState(pWidget, pData);
			return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		}
		break;
	case XUI_EVENT_POINTER_DOWN:
		return __xuiPagePointerDown(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_UP:
		return __xuiPagePointerUp(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_CLICK:
		if ( pEvent->iButton == XUI_POINTER_BUTTON_LEFT ) return XUI_EVENT_DISPATCH_STOP;
		break;
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		pData->iActive = -1;
		return __xuiPageSyncState(pWidget, pData);
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
		return __xuiPageSyncState(pWidget, pData);
	case XUI_EVENT_KEY_DOWN:
		return __xuiPageKeyDown(pWidget, pData, pEvent);
	case XUI_EVENT_ENABLED_CHANGED:
	case XUI_EVENT_VISIBLE_CHANGED:
		iRet = __xuiPageSyncState(pWidget, pData);
		if ( iRet != XUI_OK ) return iRet;
		return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiPageDrawRectFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || (__xuiPageColorAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	return pProxy->drawRectFill(pProxy, pDraw, tRect, iColor);
}

static int __xuiPageDrawRectStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || (fWidth <= 0.0f) || (__xuiPageColorAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	return pProxy->drawRectStroke(pProxy, pDraw, tRect, fWidth, iColor);
}

static int __xuiPageCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_page_data_t* pData;
	xui_page_data_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tContent;
	xui_rect_t tGroup;
	float fRight;
	int i;
	int iRet;

	(void)pUser;
	pData = __xuiPageGetData(pWidget);
	if ( (pData == NULL) || (pDraw == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	__xuiPageResolve(pWidget, pData, &tResolved);
	tContent = xuiWidgetGetContentRect(pWidget);
	(void)__xuiPageBuildItems(&tResolved, tContent);
	pData->iItemCount = tResolved.iItemCount;
	memcpy(pData->arrItems, tResolved.arrItems, sizeof(pData->arrItems));
	if ( tResolved.iItemCount <= 0 ) return XUI_OK;
	for ( i = 0; i < tResolved.iItemCount; i++ ) {
		iRet = __xuiPageDrawRectFill(pProxy, pDraw, tResolved.arrItems[i].tRect, __xuiPageItemBackground(&tResolved, i, iStateId));
		if ( iRet != XUI_OK ) return iRet;
		if ( (tResolved.pFont != NULL) && (tResolved.arrItems[i].sText[0] != 0) && (pProxy->drawText != NULL) ) {
			uint32_t iTextColor;
			iTextColor = __xuiPageItemTextColor(&tResolved, i, iStateId);
			if ( __xuiPageColorAlpha(iTextColor) != 0 ) {
				iRet = pProxy->drawText(pProxy, pDraw, tResolved.pFont, tResolved.arrItems[i].sText, tResolved.arrItems[i].tRect, iTextColor, XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
				if ( iRet != XUI_OK ) return iRet;
			}
		}
	}
	tGroup = tResolved.arrItems[0].tRect;
	fRight = tResolved.arrItems[tResolved.iItemCount - 1].tRect.fX + tResolved.arrItems[tResolved.iItemCount - 1].tRect.fW;
	tGroup.fW = fRight - tGroup.fX;
	iRet = __xuiPageDrawRectStroke(pProxy, pDraw, tGroup, 1.0f, tResolved.iBorderColor);
	if ( iRet != XUI_OK ) return iRet;
	for ( i = 1; i < tResolved.iItemCount; i++ ) {
		if ( ((tResolved.arrItems[i - 1].iType == XUI_PAGE_ITEM_PAGE) && (tResolved.arrItems[i - 1].iPage == tResolved.iCurrentPage)) ||
		     ((tResolved.arrItems[i].iType == XUI_PAGE_ITEM_PAGE) && (tResolved.arrItems[i].iPage == tResolved.iCurrentPage)) ) {
			continue;
		}
		iRet = __xuiPageDrawRectFill(pProxy, pDraw, (xui_rect_t){tResolved.arrItems[i].tRect.fX, tGroup.fY, 1.0f, tGroup.fH}, tResolved.iBorderColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( ((iStateId & XUI_WIDGET_STATE_FOCUS) != 0) && ((iStateId & XUI_WIDGET_STATE_DISABLED) == 0) ) {
		iRet = __xuiPageDrawRectStroke(pProxy, pDraw, tGroup, 1.0f, tResolved.iFocusColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiPageContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_page_data_t* pData;
	xui_page_data_t tResolved;
	xui_rect_t tMeasure;

	(void)tConstraint;
	(void)pUser;
	if ( pSize == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiPageGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiPageResolve(pWidget, pData, &tResolved);
	tMeasure = (xui_rect_t){0.0f, 0.0f, 0.0f, tResolved.fItemHeight};
	pSize->fX = __xuiPageBuildItems(&tResolved, tMeasure);
	pSize->fY = tResolved.fItemHeight;
	return XUI_OK;
}

static void __xuiPageDefaultLayout(xui_layout_t* pLayout)
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

static void __xuiPageDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE | XUI_CACHE_UPDATE_ALL_STATES;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiPageInitCacheStates(xui_widget pWidget)
{
	static const uint32_t arrState[] = {
		0,
		XUI_WIDGET_STATE_HOVER,
		XUI_WIDGET_STATE_ACTIVE,
		XUI_WIDGET_STATE_FOCUS,
		XUI_WIDGET_STATE_DISABLED
	};
	int i;
	int iRet;

	iRet = xuiWidgetSetCacheStateCount(pWidget, (int)(sizeof(arrState) / sizeof(arrState[0])));
	if ( iRet != XUI_OK ) return iRet;
	for ( i = 0; i < (int)(sizeof(arrState) / sizeof(arrState[0])); i++ ) {
		iRet = xuiWidgetSetCacheStateId(pWidget, i, 0x7b000000u + (uint32_t)i);
		if ( iRet != XUI_OK ) return iRet;
	}
	for ( i = 0; i < (int)(sizeof(arrState) / sizeof(arrState[0])); i++ ) {
		iRet = xuiWidgetSetCacheStateId(pWidget, i, arrState[i]);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiPageInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetCancelAction(pWidget, __xuiPageCancelAction, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_ENTER, __xuiPageEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiPageEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiPageEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiPageEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiPageEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CLICK, __xuiPageEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiPageEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiPageEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiPageEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiPageEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_ENABLED_CHANGED, __xuiPageEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_VISIBLE_CHANGED, __xuiPageEvent, NULL);
	return iRet;
}

static int __xuiPageInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_page_data_t* pData;
	const xui_page_desc_t* pDesc;
	xui_context pContext;
	int iRet;

	(void)pUser;
	pData = (xui_page_data_t*)pTypeData;
	pDesc = (const xui_page_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiPageDescValid(pDesc) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(pData, 0, sizeof(*pData));
	pContext = xuiWidgetGetContext(pWidget);
	pData->pFont = (pDesc != NULL && pDesc->pFont != NULL) ? pDesc->pFont : xuiGetDefaultFont(pContext);
	pData->iPageCount = (pDesc != NULL && pDesc->iPageCount > 0) ? pDesc->iPageCount : 1;
	pData->iCurrentPage = __xuiPageClampInt((pDesc != NULL && pDesc->iCurrentPage > 0) ? pDesc->iCurrentPage : 1, 1, pData->iPageCount);
	pData->iWindowSize = __xuiPageNormalizeWindowSize((pDesc != NULL && pDesc->iWindowSize > 0) ? pDesc->iWindowSize : 5);
	pData->iHover = -1;
	pData->iActive = -1;
	__xuiPageCopyText(pData->sFirstText, sizeof(pData->sFirstText), (pDesc != NULL) ? pDesc->sFirstText : NULL, "First");
	__xuiPageCopyText(pData->sLastText, sizeof(pData->sLastText), (pDesc != NULL) ? pDesc->sLastText : NULL, "Last");
	__xuiPageCopyText(pData->sPrevText, sizeof(pData->sPrevText), (pDesc != NULL) ? pDesc->sPrevText : NULL, "<");
	__xuiPageCopyText(pData->sNextText, sizeof(pData->sNextText), (pDesc != NULL) ? pDesc->sNextText : NULL, ">");
	pData->fItemHeight = (pDesc != NULL && pDesc->fItemHeight > 0.0f) ? pDesc->fItemHeight : 28.0f;
	pData->fPageWidth = (pDesc != NULL && pDesc->fPageWidth > 0.0f) ? pDesc->fPageWidth : 38.0f;
	pData->fTextWidth = (pDesc != NULL && pDesc->fTextWidth > 0.0f) ? pDesc->fTextWidth : 56.0f;
	pData->fNavWidth = (pDesc != NULL && pDesc->fNavWidth > 0.0f) ? pDesc->fNavWidth : 46.0f;
	pData->fEllipsisWidth = (pDesc != NULL && pDesc->fEllipsisWidth > 0.0f) ? pDesc->fEllipsisWidth : 32.0f;
	pData->iBackgroundColor = (pDesc != NULL && pDesc->iBackgroundColor != 0) ? pDesc->iBackgroundColor : XUI_COLOR_RGBA(255, 255, 255, 255);
	pData->iBorderColor = (pDesc != NULL && pDesc->iBorderColor != 0) ? pDesc->iBorderColor : XUI_COLOR_RGBA(214, 226, 236, 255);
	pData->iTextColor = (pDesc != NULL && pDesc->iTextColor != 0) ? pDesc->iTextColor : XUI_COLOR_RGBA(36, 54, 72, 255);
	pData->iHoverColor = (pDesc != NULL && pDesc->iHoverColor != 0) ? pDesc->iHoverColor : XUI_COLOR_RGBA(239, 247, 252, 255);
	pData->iActiveColor = (pDesc != NULL && pDesc->iActiveColor != 0) ? pDesc->iActiveColor : XUI_COLOR_RGBA(228, 240, 248, 255);
	pData->iCurrentColor = (pDesc != NULL && pDesc->iCurrentColor != 0) ? pDesc->iCurrentColor : XUI_COLOR_RGBA(34, 184, 170, 255);
	pData->iCurrentTextColor = (pDesc != NULL && pDesc->iCurrentTextColor != 0) ? pDesc->iCurrentTextColor : XUI_COLOR_RGBA(255, 255, 255, 255);
	pData->iDisabledTextColor = (pDesc != NULL && pDesc->iDisabledTextColor != 0) ? pDesc->iDisabledTextColor : XUI_COLOR_RGBA(160, 172, 184, 255);
	pData->iFocusColor = (pDesc != NULL && pDesc->iFocusColor != 0) ? pDesc->iFocusColor : XUI_COLOR_RGBA(150, 198, 232, 255);
	(void)xuiWidgetSetPadding(pWidget, (xui_thickness_t){0.0f, 0.0f, 0.0f, 0.0f});
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_CLIP);
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	iRet = __xuiPageInitCacheStates(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiPageInitEvents(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiPageSyncState(pWidget, pData);
}

static void __xuiPageDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_page_data_t* pData;

	(void)pUser;
	pData = (xui_page_data_t*)pTypeData;
	if ( pData != NULL ) {
		(void)__xuiPageCancelCapture(pWidget, pData);
		memset(pData, 0, sizeof(*pData));
	}
}

static void __xuiPageRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
{
	xui_style_property_info_t tInfo;

	if ( xuiStyleFindProperty(pContext, sName) != 0 ) return;
	memset(&tInfo, 0, sizeof(tInfo));
	tInfo.iSize = sizeof(tInfo);
	tInfo.sName = sName;
	tInfo.iValueType = iValueType;
	tInfo.iDirtyFlags = iDirtyFlags;
	tInfo.iFlags = iFlags;
	tInfo.pWidgetType = pType;
	(void)xuiStyleRegisterProperty(pContext, &tInfo, NULL);
}

static void __xuiPageRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;
	uint32_t iLayoutDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	iLayoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiPageRegisterStyleProperty(pContext, pType, "page.background.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiPageRegisterStyleProperty(pContext, pType, "page.border.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiPageRegisterStyleProperty(pContext, pType, "page.text.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiPageRegisterStyleProperty(pContext, pType, "page.hover.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiPageRegisterStyleProperty(pContext, pType, "page.active.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiPageRegisterStyleProperty(pContext, pType, "page.current.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiPageRegisterStyleProperty(pContext, pType, "page.current_text.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiPageRegisterStyleProperty(pContext, pType, "page.disabled_text.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiPageRegisterStyleProperty(pContext, pType, "page.focus.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiPageRegisterStyleProperty(pContext, pType, "page.item.height", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiPageRegisterStyleProperty(pContext, pType, "page.width.page", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiPageRegisterStyleProperty(pContext, pType, "page.width.text", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiPageRegisterStyleProperty(pContext, pType, "page.width.nav", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiPageRegisterStyleProperty(pContext, pType, "page.width.ellipsis", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiPageRegisterStyleProperty(pContext, pType, "font.name", XUI_STYLE_VALUE_STRING, iLayoutDirty, XUI_STYLE_PROPERTY_INHERITED);
}

XUI_API xui_widget_type xuiPageGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) return NULL;
	pType = xuiWidgetFindType(pContext, "page");
	if ( pType != NULL ) {
		__xuiPageRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "page";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_page_data_t);
	tDesc.onInit = __xuiPageInit;
	tDesc.onDestroy = __xuiPageDestroy;
	tDesc.onContentMeasure = __xuiPageContentMeasure;
	tDesc.onCacheRender = __xuiPageCacheRender;
	__xuiPageDefaultLayout(&tDesc.tLayout);
	__xuiPageDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) return NULL;
	__xuiPageRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiPageCreate(xui_context pContext, xui_widget* ppWidget, const xui_page_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiPageDescValid(pDesc) ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppWidget = NULL;
	pType = xuiPageGetType(pContext);
	if ( pType == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiPageSetChange(xui_widget pWidget, xui_page_change_proc onChange, void* pUser)
{
	xui_page_data_t* pData = __xuiPageGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onChange = onChange;
	pData->pChangeUser = pUser;
	return XUI_OK;
}

XUI_API int xuiPageSetPageCount(xui_widget pWidget, int iPageCount)
{
	xui_page_data_t* pData = __xuiPageGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iPageCount < 1 ) iPageCount = 1;
	pData->iPageCount = iPageCount;
	pData->iCurrentPage = __xuiPageClampPage(pData, pData->iCurrentPage);
	pData->iHover = -1;
	pData->iActive = -1;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiPageGetPageCount(xui_widget pWidget)
{
	xui_page_data_t* pData = __xuiPageGetData(pWidget);
	return (pData != NULL) ? pData->iPageCount : 0;
}

XUI_API int xuiPageSetCurrent(xui_widget pWidget, int iPage, int bNotify)
{
	xui_page_data_t* pData = __xuiPageGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiPageSetCurrentInternal(pWidget, pData, iPage, bNotify);
}

XUI_API int xuiPageGetCurrent(xui_widget pWidget)
{
	xui_page_data_t* pData = __xuiPageGetData(pWidget);
	return (pData != NULL) ? pData->iCurrentPage : 0;
}

XUI_API int xuiPageSetTotal(xui_widget pWidget, int iTotalCount, int iPageSize)
{
	int iPageCount;

	if ( iTotalCount < 0 ) iTotalCount = 0;
	if ( iPageSize <= 0 ) iPageSize = 1;
	iPageCount = (iTotalCount + iPageSize - 1) / iPageSize;
	if ( iPageCount < 1 ) iPageCount = 1;
	return xuiPageSetPageCount(pWidget, iPageCount);
}

XUI_API int xuiPageSetWindowSize(xui_widget pWidget, int iWindowSize)
{
	xui_page_data_t* pData = __xuiPageGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iWindowSize = __xuiPageNormalizeWindowSize(iWindowSize);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiPageGetWindowSize(xui_widget pWidget)
{
	xui_page_data_t* pData = __xuiPageGetData(pWidget);
	return (pData != NULL) ? pData->iWindowSize : 0;
}

XUI_API int xuiPageSetText(xui_widget pWidget, const char* sFirst, const char* sLast, const char* sPrev, const char* sNext)
{
	xui_page_data_t* pData = __xuiPageGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiPageCopyText(pData->sFirstText, sizeof(pData->sFirstText), sFirst, "First");
	__xuiPageCopyText(pData->sLastText, sizeof(pData->sLastText), sLast, "Last");
	__xuiPageCopyText(pData->sPrevText, sizeof(pData->sPrevText), sPrev, "<");
	__xuiPageCopyText(pData->sNextText, sizeof(pData->sNextText), sNext, ">");
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiPageGetText(xui_widget pWidget, const char** ppFirst, const char** ppLast, const char** ppPrev, const char** ppNext)
{
	xui_page_data_t* pData = __xuiPageGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( ppFirst != NULL ) *ppFirst = pData->sFirstText;
	if ( ppLast != NULL ) *ppLast = pData->sLastText;
	if ( ppPrev != NULL ) *ppPrev = pData->sPrevText;
	if ( ppNext != NULL ) *ppNext = pData->sNextText;
	return XUI_OK;
}

XUI_API int xuiPageSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_page_data_t* pData = __xuiPageGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pFont = pFont;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_font xuiPageGetFont(xui_widget pWidget)
{
	xui_page_data_t* pData = __xuiPageGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API int xuiPageSetMetrics(xui_widget pWidget, float fItemHeight, float fPageWidth, float fTextWidth, float fNavWidth, float fEllipsisWidth)
{
	xui_page_data_t* pData = __xuiPageGetData(pWidget);
	if ( (pData == NULL) || (fItemHeight < 0.0f) || (fPageWidth < 0.0f) || (fTextWidth < 0.0f) || (fNavWidth < 0.0f) || (fEllipsisWidth < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fItemHeight = (fItemHeight > 0.0f) ? fItemHeight : 28.0f;
	pData->fPageWidth = (fPageWidth > 0.0f) ? fPageWidth : 38.0f;
	pData->fTextWidth = (fTextWidth > 0.0f) ? fTextWidth : 56.0f;
	pData->fNavWidth = (fNavWidth > 0.0f) ? fNavWidth : 46.0f;
	pData->fEllipsisWidth = (fEllipsisWidth > 0.0f) ? fEllipsisWidth : 32.0f;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiPageGetMetrics(xui_widget pWidget, float* pItemHeight, float* pPageWidth, float* pTextWidth, float* pNavWidth, float* pEllipsisWidth)
{
	xui_page_data_t* pData = __xuiPageGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pItemHeight != NULL ) *pItemHeight = pData->fItemHeight;
	if ( pPageWidth != NULL ) *pPageWidth = pData->fPageWidth;
	if ( pTextWidth != NULL ) *pTextWidth = pData->fTextWidth;
	if ( pNavWidth != NULL ) *pNavWidth = pData->fNavWidth;
	if ( pEllipsisWidth != NULL ) *pEllipsisWidth = pData->fEllipsisWidth;
	return XUI_OK;
}

XUI_API int xuiPageSetColors(xui_widget pWidget, uint32_t iBackground, uint32_t iBorder, uint32_t iText, uint32_t iHover, uint32_t iActive, uint32_t iCurrent, uint32_t iCurrentText, uint32_t iDisabledText)
{
	xui_page_data_t* pData = __xuiPageGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iBackgroundColor = iBackground;
	pData->iBorderColor = iBorder;
	pData->iTextColor = iText;
	pData->iHoverColor = iHover;
	pData->iActiveColor = iActive;
	pData->iCurrentColor = iCurrent;
	pData->iCurrentTextColor = iCurrentText;
	pData->iDisabledTextColor = iDisabledText;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiPageGetColors(xui_widget pWidget, uint32_t* pBackground, uint32_t* pBorder, uint32_t* pText, uint32_t* pHover, uint32_t* pActive, uint32_t* pCurrent, uint32_t* pCurrentText, uint32_t* pDisabledText)
{
	xui_page_data_t* pData = __xuiPageGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pBackground != NULL ) *pBackground = pData->iBackgroundColor;
	if ( pBorder != NULL ) *pBorder = pData->iBorderColor;
	if ( pText != NULL ) *pText = pData->iTextColor;
	if ( pHover != NULL ) *pHover = pData->iHoverColor;
	if ( pActive != NULL ) *pActive = pData->iActiveColor;
	if ( pCurrent != NULL ) *pCurrent = pData->iCurrentColor;
	if ( pCurrentText != NULL ) *pCurrentText = pData->iCurrentTextColor;
	if ( pDisabledText != NULL ) *pDisabledText = pData->iDisabledTextColor;
	return XUI_OK;
}

XUI_API int xuiPageSetFocusColor(xui_widget pWidget, uint32_t iColor)
{
	xui_page_data_t* pData = __xuiPageGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iFocusColor = iColor;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiPageGetFocusColor(xui_widget pWidget)
{
	xui_page_data_t* pData = __xuiPageGetData(pWidget);
	return (pData != NULL) ? pData->iFocusColor : 0u;
}

XUI_API int xuiPageGetItemCount(xui_widget pWidget)
{
	xui_page_data_t* pData;
	xui_page_data_t tResolved;

	pData = __xuiPageGetData(pWidget);
	if ( pData == NULL ) return 0;
	__xuiPageResolve(pWidget, pData, &tResolved);
	(void)__xuiPageBuildItems(&tResolved, xuiWidgetGetContentRect(pWidget));
	pData->iItemCount = tResolved.iItemCount;
	memcpy(pData->arrItems, tResolved.arrItems, sizeof(pData->arrItems));
	return pData->iItemCount;
}

XUI_API int xuiPageGetItemInfo(xui_widget pWidget, int iIndex, xui_page_item_info_t* pInfo)
{
	xui_page_data_t* pData;

	if ( pInfo == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiPageGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	(void)xuiPageGetItemCount(pWidget);
	if ( (iIndex < 0) || (iIndex >= pData->iItemCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->iSize = sizeof(*pInfo);
	pInfo->tRect = pData->arrItems[iIndex].tRect;
	pInfo->iType = pData->arrItems[iIndex].iType;
	pInfo->iPage = pData->arrItems[iIndex].iPage;
	pInfo->bEnabled = pData->arrItems[iIndex].bEnabled;
	__xuiPageCopyText(pInfo->sText, sizeof(pInfo->sText), pData->arrItems[iIndex].sText, "");
	return XUI_OK;
}

XUI_API int xuiPageGetHoverItem(xui_widget pWidget)
{
	xui_page_data_t* pData = __xuiPageGetData(pWidget);
	return (pData != NULL) ? pData->iHover : -1;
}

XUI_API int xuiPageGetActiveItem(xui_widget pWidget)
{
	xui_page_data_t* pData = __xuiPageGetData(pWidget);
	return (pData != NULL) ? pData->iActive : -1;
}

XUI_API uint32_t xuiPageGetState(xui_widget pWidget)
{
	xui_page_data_t* pData;

	if ( !xuiInternalWidgetIsValid(pWidget) ) return 0;
	pData = __xuiPageGetData(pWidget);
	return __xuiPageBaseState(pWidget, pData);
}

XUI_API int xuiPageGetChangeCount(xui_widget pWidget)
{
	xui_page_data_t* pData = __xuiPageGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}
