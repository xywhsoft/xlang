#include "xui_internal.h"

#include <string.h>

#define XUI_BREADCRUMB_DEFAULT_H	32.0f
#define XUI_BREADCRUMB_DEFAULT_GAP	8.0f
#define XUI_BREADCRUMB_DEFAULT_PAD_X	0.0f
#define XUI_BREADCRUMB_DEFAULT_PAD_Y	4.0f
#define XUI_BREADCRUMB_DEFAULT_ICON	12.0f

typedef struct xui_breadcrumb_runtime_item_t {
	char* sText;
	int bClickable;
	int iValue;
	xui_rect_t tRect;
	xui_rect_t tSeparatorRect;
} xui_breadcrumb_runtime_item_t;

typedef struct xui_breadcrumb_data_t {
	xui_breadcrumb_runtime_item_t arrItems[XUI_BREADCRUMB_MAX_ITEMS];
	int iItemCount;
	char* sSeparator;
	xui_surface pSeparatorIcon;
	xui_rect_t tSeparatorIconSrc;
	xui_font pFont;
	xui_breadcrumb_click_proc onClick;
	void* pClickUser;
	uint32_t iTextColor;
	uint32_t iHoverTextColor;
	uint32_t iActiveTextColor;
	uint32_t iDisabledTextColor;
	uint32_t iSeparatorColor;
	uint32_t iBackgroundColor;
	float fSeparatorIconSize;
	float fGap;
	float fPaddingX;
	float fPaddingY;
	int iHoverIndex;
	int iActiveIndex;
	int iClickCount;
} xui_breadcrumb_data_t;

static int __xuiBreadcrumbAlpha(uint32_t iColor)
{
	return (int)(iColor & 0xffu);
}

static int __xuiBreadcrumbDescValid(const xui_breadcrumb_desc_t* pDesc)
{
	if ( pDesc == NULL ) return 1;
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) return 0;
	if ( (pDesc->iItemCount < 0) || (pDesc->iItemCount > XUI_BREADCRUMB_MAX_ITEMS) ) return 0;
	if ( (pDesc->fSeparatorIconSize < 0.0f) || (pDesc->fGap < 0.0f) ||
	     (pDesc->fPaddingX < 0.0f) || (pDesc->fPaddingY < 0.0f) ) {
		return 0;
	}
	return 1;
}

static int __xuiBreadcrumbStringSet(char** psText, const char* sText)
{
	char* sNew;
	size_t iNeed;

	if ( psText == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( sText == NULL ) sText = "";
	iNeed = strlen(sText) + 1u;
	sNew = (char*)xrtMalloc(iNeed);
	if ( sNew == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	memcpy(sNew, sText, iNeed);
	if ( *psText != NULL ) xrtFree(*psText);
	*psText = sNew;
	return XUI_OK;
}

static void __xuiBreadcrumbClearItemsData(xui_breadcrumb_data_t* pData)
{
	int i;

	if ( pData == NULL ) return;
	for ( i = 0; i < XUI_BREADCRUMB_MAX_ITEMS; i++ ) {
		if ( pData->arrItems[i].sText != NULL ) {
			xrtFree(pData->arrItems[i].sText);
			pData->arrItems[i].sText = NULL;
		}
		pData->arrItems[i].bClickable = 0;
		pData->arrItems[i].iValue = 0;
		pData->arrItems[i].tRect = (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
		pData->arrItems[i].tSeparatorRect = (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	}
	pData->iItemCount = 0;
	pData->iHoverIndex = -1;
	pData->iActiveIndex = -1;
}

static int __xuiBreadcrumbSetItemsData(xui_breadcrumb_data_t* pData, const xui_breadcrumb_item_t* pItems, int iItemCount)
{
	char* arrNew[XUI_BREADCRUMB_MAX_ITEMS];
	int i;
	int j;

	if ( (pData == NULL) || (iItemCount < 0) || (iItemCount > XUI_BREADCRUMB_MAX_ITEMS) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(arrNew, 0, sizeof(arrNew));
	for ( i = 0; i < iItemCount; i++ ) {
		const char* sText = (pItems != NULL && pItems[i].sText != NULL) ? pItems[i].sText : "";
		size_t iNeed = strlen(sText) + 1u;
		arrNew[i] = (char*)xrtMalloc(iNeed);
		if ( arrNew[i] == NULL ) {
			for ( j = 0; j < i; j++ ) xrtFree(arrNew[j]);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		memcpy(arrNew[i], sText, iNeed);
	}
	__xuiBreadcrumbClearItemsData(pData);
	for ( i = 0; i < iItemCount; i++ ) {
		pData->arrItems[i].sText = arrNew[i];
		pData->arrItems[i].bClickable = (pItems != NULL && pItems[i].bClickable) ? 1 : 0;
		pData->arrItems[i].iValue = (pItems != NULL) ? pItems[i].iValue : 0;
	}
	pData->iItemCount = iItemCount;
	return XUI_OK;
}

static xui_breadcrumb_data_t* __xuiBreadcrumbGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) return NULL;
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "breadcrumb");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) return NULL;
	return (xui_breadcrumb_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiBreadcrumbStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
{
	xui_style_property_t tProperty;
	int iRet;

	if ( (pWidget == NULL) || (sName == NULL) || (pColor == NULL) ) return 0;
	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_COLOR) ) {
		*pColor = tProperty.tValue.iColor;
		return 1;
	}
	return 0;
}

static int __xuiBreadcrumbStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
{
	xui_style_property_t tProperty;
	int iRet;

	if ( (pWidget == NULL) || (sName == NULL) || (pValue == NULL) ) return 0;
	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_FLOAT) && (tProperty.tValue.fFloat >= 0.0f) ) {
		*pValue = tProperty.tValue.fFloat;
		return 1;
	}
	return 0;
}

static xui_font __xuiBreadcrumbStyleFont(xui_widget pWidget, xui_font pBaseFont)
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

static void __xuiBreadcrumbResolve(xui_widget pWidget, const xui_breadcrumb_data_t* pData, xui_breadcrumb_data_t* pResolved)
{
	*pResolved = *pData;
	pResolved->pFont = __xuiBreadcrumbStyleFont(pWidget, (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget)));
	(void)__xuiBreadcrumbStyleColor(pWidget, "breadcrumb.text.color", &pResolved->iTextColor);
	(void)__xuiBreadcrumbStyleColor(pWidget, "breadcrumb.text.hover_color", &pResolved->iHoverTextColor);
	(void)__xuiBreadcrumbStyleColor(pWidget, "breadcrumb.text.active_color", &pResolved->iActiveTextColor);
	(void)__xuiBreadcrumbStyleColor(pWidget, "breadcrumb.text.disabled_color", &pResolved->iDisabledTextColor);
	(void)__xuiBreadcrumbStyleColor(pWidget, "breadcrumb.separator.color", &pResolved->iSeparatorColor);
	(void)__xuiBreadcrumbStyleColor(pWidget, "breadcrumb.background.color", &pResolved->iBackgroundColor);
	(void)__xuiBreadcrumbStyleFloat(pWidget, "breadcrumb.gap", &pResolved->fGap);
	(void)__xuiBreadcrumbStyleFloat(pWidget, "breadcrumb.padding_x", &pResolved->fPaddingX);
	(void)__xuiBreadcrumbStyleFloat(pWidget, "breadcrumb.padding_y", &pResolved->fPaddingY);
	(void)__xuiBreadcrumbStyleFloat(pWidget, "breadcrumb.separator.icon_size", &pResolved->fSeparatorIconSize);
}

static const char* __xuiBreadcrumbItemText(const xui_breadcrumb_data_t* pData, int iIndex)
{
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) || (pData->arrItems[iIndex].sText == NULL) ) {
		return "";
	}
	return pData->arrItems[iIndex].sText;
}

static const char* __xuiBreadcrumbSeparator(const xui_breadcrumb_data_t* pData)
{
	if ( (pData == NULL) || (pData->sSeparator == NULL) || (pData->sSeparator[0] == '\0') ) {
		return "/";
	}
	return pData->sSeparator;
}

static xui_vec2_t __xuiBreadcrumbMeasureText(xui_proxy pProxy, xui_font pFont, const char* sText)
{
	xui_vec2_t tSize;

	tSize = (xui_vec2_t){0.0f, 16.0f};
	if ( (pProxy != NULL) && (pProxy->textMeasure != NULL) && (pFont != NULL) && (sText != NULL) ) {
		if ( pProxy->textMeasure(pProxy, pFont, sText, &tSize) == XUI_OK ) {
			if ( tSize.fX < 0.0f ) tSize.fX = 0.0f;
			if ( tSize.fY < 1.0f ) tSize.fY = 16.0f;
			return tSize;
		}
	}
	if ( sText != NULL ) tSize.fX = (float)strlen(sText) * 8.0f;
	return tSize;
}

static float __xuiBreadcrumbSeparatorWidth(xui_proxy pProxy, xui_font pFont, const xui_breadcrumb_data_t* pData)
{
	xui_vec2_t tSize;

	if ( pData->pSeparatorIcon != NULL ) {
		return (pData->fSeparatorIconSize > 0.0f) ? pData->fSeparatorIconSize : XUI_BREADCRUMB_DEFAULT_ICON;
	}
	tSize = __xuiBreadcrumbMeasureText(pProxy, pFont, __xuiBreadcrumbSeparator(pData));
	return tSize.fX;
}

static int __xuiBreadcrumbLayoutItems(xui_widget pWidget, const xui_breadcrumb_data_t* pResolved, float* pContentW, float* pContentH)
{
	xui_breadcrumb_data_t* pData;
	xui_proxy pProxy;
	xui_rect_t tContent;
	xui_vec2_t tText;
	float fX;
	float fTextH;
	float fSepW;
	float fSepH;
	int i;

	pData = __xuiBreadcrumbGetData(pWidget);
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	tContent = xuiWidgetGetContentRect(pWidget);
	fX = tContent.fX + pResolved->fPaddingX;
	fTextH = 16.0f;
	fSepW = __xuiBreadcrumbSeparatorWidth(pProxy, pResolved->pFont, pResolved);
	fSepH = (pResolved->pSeparatorIcon != NULL) ? ((pResolved->fSeparatorIconSize > 0.0f) ? pResolved->fSeparatorIconSize : XUI_BREADCRUMB_DEFAULT_ICON) : 16.0f;
	if ( pData != NULL ) {
		for ( i = 0; i < XUI_BREADCRUMB_MAX_ITEMS; i++ ) {
			pData->arrItems[i].tRect = (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
			pData->arrItems[i].tSeparatorRect = (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
		}
	}
	for ( i = 0; i < pResolved->iItemCount; i++ ) {
		tText = __xuiBreadcrumbMeasureText(pProxy, pResolved->pFont, __xuiBreadcrumbItemText(pResolved, i));
		if ( tText.fY > fTextH ) fTextH = tText.fY;
		if ( pData != NULL ) {
			pData->arrItems[i].tRect = xuiInternalSnapRect((xui_rect_t){fX, tContent.fY + pResolved->fPaddingY, tText.fX, tContent.fH - pResolved->fPaddingY * 2.0f});
		}
		fX += tText.fX;
		if ( i < pResolved->iItemCount - 1 ) {
			fX += pResolved->fGap;
			if ( pData != NULL ) {
				pData->arrItems[i].tSeparatorRect = xuiInternalSnapRect((xui_rect_t){fX, tContent.fY + pResolved->fPaddingY, fSepW, tContent.fH - pResolved->fPaddingY * 2.0f});
			}
			fX += fSepW + pResolved->fGap;
		}
	}
	if ( pContentW != NULL ) {
		*pContentW = (fX - tContent.fX) + pResolved->fPaddingX;
	}
	if ( pContentH != NULL ) {
		float fH = fTextH;
		if ( fSepH > fH ) fH = fSepH;
		*pContentH = fH + pResolved->fPaddingY * 2.0f;
	}
	return XUI_OK;
}

static int __xuiBreadcrumbPointInRect(float fX, float fY, xui_rect_t tRect)
{
	return (fX >= tRect.fX) && (fY >= tRect.fY) &&
	       (fX < tRect.fX + tRect.fW) && (fY < tRect.fY + tRect.fH);
}

static int __xuiBreadcrumbHitItem(xui_widget pWidget, float fX, float fY)
{
	xui_breadcrumb_data_t* pData;
	xui_rect_t tWorld;
	int i;

	pData = __xuiBreadcrumbGetData(pWidget);
	if ( pData == NULL ) return -1;
	tWorld = xuiWidgetGetWorldRect(pWidget);
	fX -= tWorld.fX;
	fY -= tWorld.fY;
	for ( i = 0; i < pData->iItemCount; i++ ) {
		if ( pData->arrItems[i].bClickable && __xuiBreadcrumbPointInRect(fX, fY, pData->arrItems[i].tRect) ) {
			return i;
		}
	}
	return -1;
}

static uint32_t __xuiBreadcrumbItemColor(const xui_breadcrumb_data_t* pResolved, int iIndex)
{
	if ( !pResolved->arrItems[iIndex].bClickable ) return pResolved->iDisabledTextColor;
	if ( iIndex == pResolved->iActiveIndex ) return pResolved->iActiveTextColor;
	if ( iIndex == pResolved->iHoverIndex ) return pResolved->iHoverTextColor;
	return pResolved->iTextColor;
}

static int __xuiBreadcrumbDrawText(xui_proxy pProxy, xui_draw_context pDraw, xui_font pFont, const char* sText, xui_rect_t tRect, uint32_t iColor)
{
	if ( (pProxy == NULL) || (pDraw == NULL) || (pProxy->drawText == NULL) || (pFont == NULL) ||
	     (sText == NULL) || (sText[0] == '\0') || (__xuiBreadcrumbAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	return pProxy->drawText(pProxy, pDraw, pFont, sText, xuiInternalSnapRect(tRect), iColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
}

static int __xuiBreadcrumbDrawIcon(xui_proxy pProxy, xui_draw_context pDraw, const xui_breadcrumb_data_t* pResolved, xui_rect_t tRect)
{
	xui_rect_t tSrc;
	xui_rect_t tDst;
	float fSize;

	if ( (pProxy == NULL) || (pDraw == NULL) || (pProxy->drawSurface == NULL) || (pResolved->pSeparatorIcon == NULL) ||
	     (__xuiBreadcrumbAlpha(pResolved->iSeparatorColor) == 0) ) {
		return XUI_OK;
	}
	tSrc = pResolved->tSeparatorIconSrc;
	if ( (tSrc.fW <= 0.0f) || (tSrc.fH <= 0.0f) ) {
		tSrc = (xui_rect_t){0.0f, 0.0f, pResolved->fSeparatorIconSize, pResolved->fSeparatorIconSize};
	}
	fSize = (pResolved->fSeparatorIconSize > 0.0f) ? pResolved->fSeparatorIconSize : XUI_BREADCRUMB_DEFAULT_ICON;
	tDst = (xui_rect_t){tRect.fX + (tRect.fW - fSize) * 0.5f, tRect.fY + (tRect.fH - fSize) * 0.5f, fSize, fSize};
	return pProxy->drawSurface(pProxy, pDraw, pResolved->pSeparatorIcon, tSrc, xuiInternalSnapRect(tDst), pResolved->iSeparatorColor, 0);
}

static int __xuiBreadcrumbContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_breadcrumb_data_t* pData;
	xui_breadcrumb_data_t tResolved;
	float fW;
	float fH;

	pData = (xui_breadcrumb_data_t*)pUser;
	if ( (pWidget == NULL) || (pSize == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiBreadcrumbResolve(pWidget, pData, &tResolved);
	(void)__xuiBreadcrumbLayoutItems(pWidget, &tResolved, &fW, &fH);
	if ( fH < XUI_BREADCRUMB_DEFAULT_H ) fH = XUI_BREADCRUMB_DEFAULT_H;
	if ( (tConstraint.fX > 0.0f) && (tConstraint.fX < XUI_LAYOUT_UNBOUNDED) ) fW = tConstraint.fX;
	if ( (tConstraint.fY > 0.0f) && (tConstraint.fY < XUI_LAYOUT_UNBOUNDED) ) fH = tConstraint.fY;
	pSize->fX = fW;
	pSize->fY = fH;
	return XUI_OK;
}

static int __xuiBreadcrumbCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_breadcrumb_data_t* pData;
	xui_breadcrumb_data_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tContent;
	xui_rect_t tText;
	int i;
	int iRet;

	(void)iStateId;
	pData = (xui_breadcrumb_data_t*)pUser;
	if ( (pWidget == NULL) || (pDraw == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	__xuiBreadcrumbResolve(pWidget, pData, &tResolved);
	tContent = xuiWidgetGetContentRect(pWidget);
	if ( (__xuiBreadcrumbAlpha(tResolved.iBackgroundColor) != 0) && (pProxy->drawRectFill != NULL) ) {
		iRet = pProxy->drawRectFill(pProxy, pDraw, xuiInternalSnapRect(tContent), tResolved.iBackgroundColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	iRet = __xuiBreadcrumbLayoutItems(pWidget, &tResolved, NULL, NULL);
	if ( iRet != XUI_OK ) return iRet;
	tResolved.iHoverIndex = pData->iHoverIndex;
	tResolved.iActiveIndex = pData->iActiveIndex;
	for ( i = 0; i < pData->iItemCount; i++ ) {
		tText = pData->arrItems[i].tRect;
		iRet = __xuiBreadcrumbDrawText(pProxy, pDraw, tResolved.pFont, __xuiBreadcrumbItemText(&tResolved, i),
			tText, __xuiBreadcrumbItemColor(&tResolved, i));
		if ( iRet != XUI_OK ) return iRet;
		if ( i < pData->iItemCount - 1 ) {
			if ( tResolved.pSeparatorIcon != NULL ) {
				iRet = __xuiBreadcrumbDrawIcon(pProxy, pDraw, &tResolved, pData->arrItems[i].tSeparatorRect);
			} else {
				iRet = __xuiBreadcrumbDrawText(pProxy, pDraw, tResolved.pFont, __xuiBreadcrumbSeparator(&tResolved),
					pData->arrItems[i].tSeparatorRect, tResolved.iSeparatorColor);
			}
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	return XUI_OK;
}

static int __xuiBreadcrumbSyncHover(xui_widget pWidget, xui_breadcrumb_data_t* pData, float fX, float fY)
{
	int iHover;

	iHover = __xuiBreadcrumbHitItem(pWidget, fX, fY);
	if ( pData->iHoverIndex != iHover ) {
		pData->iHoverIndex = iHover;
		return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return XUI_OK;
}

static void __xuiBreadcrumbDoClick(xui_widget pWidget, xui_breadcrumb_data_t* pData, int iIndex)
{
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) || !pData->arrItems[iIndex].bClickable ) return;
	pData->iClickCount++;
	if ( pData->onClick != NULL ) {
		pData->onClick(pWidget, iIndex, pData->arrItems[iIndex].iValue, pData->pClickUser);
	}
}

static int __xuiBreadcrumbEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_breadcrumb_data_t* pData;
	xui_context pContext;
	int bLeftButton;
	int iHit;

	(void)pUser;
	if ( (pWidget == NULL) || (pEvent == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiBreadcrumbGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pContext = xuiWidgetGetContext(pWidget);
	bLeftButton = (pEvent->iButton == 0) || (pEvent->iButton == XUI_POINTER_BUTTON_LEFT);
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_ENTER:
	case XUI_EVENT_POINTER_MOVE:
		return __xuiBreadcrumbSyncHover(pWidget, pData, pEvent->fX, pEvent->fY);
	case XUI_EVENT_POINTER_LEAVE:
		if ( pData->iHoverIndex != -1 ) {
			pData->iHoverIndex = -1;
			return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		}
		return XUI_OK;
	case XUI_EVENT_POINTER_DOWN:
		if ( bLeftButton ) {
			iHit = __xuiBreadcrumbHitItem(pWidget, pEvent->fX, pEvent->fY);
			if ( iHit >= 0 ) {
				pData->iActiveIndex = iHit;
				(void)xuiSetPointerCapture(pContext, pWidget);
				(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
				return XUI_EVENT_DISPATCH_STOP;
			}
		}
		break;
	case XUI_EVENT_POINTER_UP:
		if ( bLeftButton ) {
			iHit = __xuiBreadcrumbHitItem(pWidget, pEvent->fX, pEvent->fY);
			if ( (pData->iActiveIndex >= 0) && (iHit == pData->iActiveIndex) ) {
				__xuiBreadcrumbDoClick(pWidget, pData, iHit);
			}
			pData->iActiveIndex = -1;
			(void)xuiReleasePointerCapture(pContext, pWidget);
			(void)__xuiBreadcrumbSyncHover(pWidget, pData, pEvent->fX, pEvent->fY);
			(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		pData->iActiveIndex = -1;
		return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	default:
		break;
	}
	return XUI_OK;
}

static void __xuiBreadcrumbDefaults(xui_breadcrumb_data_t* pData)
{
	memset(pData, 0, sizeof(*pData));
	pData->iTextColor = XUI_COLOR_RGBA(135, 148, 164, 255);
	pData->iHoverTextColor = XUI_COLOR_RGBA(45, 125, 215, 255);
	pData->iActiveTextColor = XUI_COLOR_RGBA(24, 93, 184, 255);
	pData->iDisabledTextColor = XUI_COLOR_RGBA(88, 102, 121, 255);
	pData->iSeparatorColor = XUI_COLOR_RGBA(192, 200, 210, 255);
	pData->iBackgroundColor = XUI_COLOR_RGBA(255, 255, 255, 0);
	pData->fSeparatorIconSize = XUI_BREADCRUMB_DEFAULT_ICON;
	pData->fGap = XUI_BREADCRUMB_DEFAULT_GAP;
	pData->fPaddingX = XUI_BREADCRUMB_DEFAULT_PAD_X;
	pData->fPaddingY = XUI_BREADCRUMB_DEFAULT_PAD_Y;
	pData->iHoverIndex = -1;
	pData->iActiveIndex = -1;
}

static int __xuiBreadcrumbInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_ENTER, __xuiBreadcrumbEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiBreadcrumbEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiBreadcrumbEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiBreadcrumbEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiBreadcrumbEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiBreadcrumbEvent, NULL);
	return iRet;
}

static int __xuiBreadcrumbInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_breadcrumb_data_t* pData;
	const xui_breadcrumb_desc_t* pDesc;
	int iRet;

	(void)pUser;
	pData = (xui_breadcrumb_data_t*)pTypeData;
	pDesc = (const xui_breadcrumb_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiBreadcrumbDescValid(pDesc) ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiBreadcrumbDefaults(pData);
	if ( pDesc != NULL ) {
		pData->pFont = pDesc->pFont;
		if ( pDesc->iTextColor != 0 ) pData->iTextColor = pDesc->iTextColor;
		if ( pDesc->iHoverTextColor != 0 ) pData->iHoverTextColor = pDesc->iHoverTextColor;
		if ( pDesc->iActiveTextColor != 0 ) pData->iActiveTextColor = pDesc->iActiveTextColor;
		if ( pDesc->iDisabledTextColor != 0 ) pData->iDisabledTextColor = pDesc->iDisabledTextColor;
		if ( pDesc->iSeparatorColor != 0 ) pData->iSeparatorColor = pDesc->iSeparatorColor;
		if ( pDesc->iBackgroundColor != 0 ) pData->iBackgroundColor = pDesc->iBackgroundColor;
		if ( pDesc->fSeparatorIconSize > 0.0f ) pData->fSeparatorIconSize = pDesc->fSeparatorIconSize;
		if ( pDesc->fGap > 0.0f ) pData->fGap = pDesc->fGap;
		if ( pDesc->fPaddingX > 0.0f ) pData->fPaddingX = pDesc->fPaddingX;
		if ( pDesc->fPaddingY > 0.0f ) pData->fPaddingY = pDesc->fPaddingY;
		pData->pSeparatorIcon = pDesc->pSeparatorIcon;
		pData->tSeparatorIconSrc = pDesc->tSeparatorIconSrc;
		iRet = __xuiBreadcrumbStringSet(&pData->sSeparator, pDesc->sSeparator != NULL ? pDesc->sSeparator : "/");
		if ( iRet != XUI_OK ) return iRet;
		iRet = __xuiBreadcrumbSetItemsData(pData, pDesc->pItems, pDesc->iItemCount);
		if ( iRet != XUI_OK ) return iRet;
	} else {
		iRet = __xuiBreadcrumbStringSet(&pData->sSeparator, "/");
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pData->pFont == NULL ) pData->pFont = xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	(void)xuiWidgetSetFocusable(pWidget, 0);
	(void)xuiWidgetSetTabStop(pWidget, 0);
	return __xuiBreadcrumbInitEvents(pWidget);
}

static void __xuiBreadcrumbDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_breadcrumb_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_breadcrumb_data_t*)pTypeData;
	if ( pData == NULL ) return;
	__xuiBreadcrumbClearItemsData(pData);
	if ( pData->sSeparator != NULL ) {
		xrtFree(pData->sSeparator);
		pData->sSeparator = NULL;
	}
}

static void __xuiBreadcrumbDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_MANUAL;
	pLayout->iWidthMode = XUI_SIZE_CONTENT;
	pLayout->iHeightMode = XUI_SIZE_CONTENT;
	pLayout->iFlowMode = XUI_FLOW_BLOCK;
	pLayout->iDock = 0;
	pLayout->iOverflow = XUI_OVERFLOW_HIDDEN;
	pLayout->iAlignX = XUI_ALIGN_START;
	pLayout->iAlignY = XUI_ALIGN_START;
	pLayout->iTableRowSpan = 1;
	pLayout->iTableColumnSpan = 1;
	pLayout->iGridColumnCount = 1;
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fShrink = 1.0f;
}

static void __xuiBreadcrumbDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static void __xuiBreadcrumbRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
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

static void __xuiBreadcrumbRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	uint32_t iLayoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;

	__xuiBreadcrumbRegisterStyleProperty(pContext, pType, "breadcrumb.text.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiBreadcrumbRegisterStyleProperty(pContext, pType, "breadcrumb.text.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiBreadcrumbRegisterStyleProperty(pContext, pType, "breadcrumb.text.active_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiBreadcrumbRegisterStyleProperty(pContext, pType, "breadcrumb.text.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiBreadcrumbRegisterStyleProperty(pContext, pType, "breadcrumb.separator.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiBreadcrumbRegisterStyleProperty(pContext, pType, "breadcrumb.background.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiBreadcrumbRegisterStyleProperty(pContext, pType, "breadcrumb.gap", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiBreadcrumbRegisterStyleProperty(pContext, pType, "breadcrumb.padding_x", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiBreadcrumbRegisterStyleProperty(pContext, pType, "breadcrumb.padding_y", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiBreadcrumbRegisterStyleProperty(pContext, pType, "breadcrumb.separator.icon_size", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiBreadcrumbRegisterStyleProperty(pContext, pType, "font.name", XUI_STYLE_VALUE_STRING, iLayoutDirty, XUI_STYLE_PROPERTY_INHERITED);
}

XUI_API xui_widget_type xuiBreadcrumbGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) return NULL;
	pType = xuiWidgetFindType(pContext, "breadcrumb");
	if ( pType != NULL ) {
		__xuiBreadcrumbRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "breadcrumb";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_breadcrumb_data_t);
	tDesc.onInit = __xuiBreadcrumbInit;
	tDesc.onDestroy = __xuiBreadcrumbDestroy;
	tDesc.onContentMeasure = __xuiBreadcrumbContentMeasure;
	tDesc.onCacheRender = __xuiBreadcrumbCacheRender;
	__xuiBreadcrumbDefaultLayout(&tDesc.tLayout);
	__xuiBreadcrumbDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) return NULL;
	__xuiBreadcrumbRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiBreadcrumbCreate(xui_context pContext, xui_widget* ppWidget, const xui_breadcrumb_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiBreadcrumbDescValid(pDesc) ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppWidget = NULL;
	pType = xuiBreadcrumbGetType(pContext);
	if ( pType == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiBreadcrumbSetClick(xui_widget pWidget, xui_breadcrumb_click_proc onClick, void* pUser)
{
	xui_breadcrumb_data_t* pData;

	pData = __xuiBreadcrumbGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onClick = onClick;
	pData->pClickUser = pUser;
	return XUI_OK;
}

XUI_API int xuiBreadcrumbSetItems(xui_widget pWidget, const xui_breadcrumb_item_t* pItems, int iItemCount)
{
	xui_breadcrumb_data_t* pData;
	int iRet;

	pData = __xuiBreadcrumbGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiBreadcrumbSetItemsData(pData, pItems, iItemCount);
	if ( iRet != XUI_OK ) return iRet;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiBreadcrumbClearItems(xui_widget pWidget)
{
	return xuiBreadcrumbSetItems(pWidget, NULL, 0);
}

XUI_API int xuiBreadcrumbAddItem(xui_widget pWidget, const char* sText, int bClickable, int iValue)
{
	xui_breadcrumb_data_t* pData;
	xui_breadcrumb_runtime_item_t* pItem;
	int iRet;

	pData = __xuiBreadcrumbGetData(pWidget);
	if ( (pData == NULL) || (pData->iItemCount >= XUI_BREADCRUMB_MAX_ITEMS) ) return XUI_ERROR_INVALID_ARGUMENT;
	pItem = &pData->arrItems[pData->iItemCount];
	iRet = __xuiBreadcrumbStringSet(&pItem->sText, sText);
	if ( iRet != XUI_OK ) return iRet;
	pItem->bClickable = bClickable ? 1 : 0;
	pItem->iValue = iValue;
	pData->iItemCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiBreadcrumbSetItem(xui_widget pWidget, int iIndex, const char* sText, int bClickable, int iValue)
{
	xui_breadcrumb_data_t* pData;
	int iRet;

	pData = __xuiBreadcrumbGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiBreadcrumbStringSet(&pData->arrItems[iIndex].sText, sText);
	if ( iRet != XUI_OK ) return iRet;
	pData->arrItems[iIndex].bClickable = bClickable ? 1 : 0;
	pData->arrItems[iIndex].iValue = iValue;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiBreadcrumbGetItemCount(xui_widget pWidget)
{
	xui_breadcrumb_data_t* pData;

	pData = __xuiBreadcrumbGetData(pWidget);
	return (pData != NULL) ? pData->iItemCount : 0;
}

XUI_API const char* xuiBreadcrumbGetItemText(xui_widget pWidget, int iIndex)
{
	xui_breadcrumb_data_t* pData;

	pData = __xuiBreadcrumbGetData(pWidget);
	return __xuiBreadcrumbItemText(pData, iIndex);
}

XUI_API int xuiBreadcrumbGetItemClickable(xui_widget pWidget, int iIndex)
{
	xui_breadcrumb_data_t* pData;

	pData = __xuiBreadcrumbGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return 0;
	return pData->arrItems[iIndex].bClickable;
}

XUI_API int xuiBreadcrumbGetItemValue(xui_widget pWidget, int iIndex)
{
	xui_breadcrumb_data_t* pData;

	pData = __xuiBreadcrumbGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return 0;
	return pData->arrItems[iIndex].iValue;
}

XUI_API int xuiBreadcrumbSetSeparator(xui_widget pWidget, const char* sSeparator)
{
	xui_breadcrumb_data_t* pData;
	int iRet;

	pData = __xuiBreadcrumbGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiBreadcrumbStringSet(&pData->sSeparator, (sSeparator != NULL) ? sSeparator : "/");
	if ( iRet != XUI_OK ) return iRet;
	pData->pSeparatorIcon = NULL;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API const char* xuiBreadcrumbGetSeparator(xui_widget pWidget)
{
	xui_breadcrumb_data_t* pData;

	pData = __xuiBreadcrumbGetData(pWidget);
	return __xuiBreadcrumbSeparator(pData);
}

XUI_API int xuiBreadcrumbSetSeparatorIcon(xui_widget pWidget, xui_surface pSurface, xui_rect_t tSrc, float fIconSize)
{
	xui_breadcrumb_data_t* pData;

	pData = __xuiBreadcrumbGetData(pWidget);
	if ( (pData == NULL) || (fIconSize < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pSeparatorIcon = pSurface;
	pData->tSeparatorIconSrc = tSrc;
	if ( fIconSize > 0.0f ) pData->fSeparatorIconSize = fIconSize;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_surface xuiBreadcrumbGetSeparatorIcon(xui_widget pWidget)
{
	xui_breadcrumb_data_t* pData;

	pData = __xuiBreadcrumbGetData(pWidget);
	return (pData != NULL) ? pData->pSeparatorIcon : NULL;
}

XUI_API xui_rect_t xuiBreadcrumbGetSeparatorIconSource(xui_widget pWidget)
{
	xui_breadcrumb_data_t* pData;

	pData = __xuiBreadcrumbGetData(pWidget);
	return (pData != NULL) ? pData->tSeparatorIconSrc : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API float xuiBreadcrumbGetSeparatorIconSize(xui_widget pWidget)
{
	xui_breadcrumb_data_t* pData;

	pData = __xuiBreadcrumbGetData(pWidget);
	return (pData != NULL) ? pData->fSeparatorIconSize : 0.0f;
}

XUI_API int xuiBreadcrumbSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_breadcrumb_data_t* pData;

	pData = __xuiBreadcrumbGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pFont = pFont;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_font xuiBreadcrumbGetFont(xui_widget pWidget)
{
	xui_breadcrumb_data_t* pData;

	pData = __xuiBreadcrumbGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API int xuiBreadcrumbSetTextColors(xui_widget pWidget, uint32_t iNormal, uint32_t iHover, uint32_t iActive, uint32_t iDisabled)
{
	xui_breadcrumb_data_t* pData;

	pData = __xuiBreadcrumbGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iTextColor = iNormal;
	pData->iHoverTextColor = iHover;
	pData->iActiveTextColor = iActive;
	pData->iDisabledTextColor = iDisabled;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiBreadcrumbGetTextColors(xui_widget pWidget, uint32_t* pNormal, uint32_t* pHover, uint32_t* pActive, uint32_t* pDisabled)
{
	xui_breadcrumb_data_t* pData;

	pData = __xuiBreadcrumbGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pNormal != NULL ) *pNormal = pData->iTextColor;
	if ( pHover != NULL ) *pHover = pData->iHoverTextColor;
	if ( pActive != NULL ) *pActive = pData->iActiveTextColor;
	if ( pDisabled != NULL ) *pDisabled = pData->iDisabledTextColor;
	return XUI_OK;
}

XUI_API int xuiBreadcrumbSetSeparatorColor(xui_widget pWidget, uint32_t iColor)
{
	xui_breadcrumb_data_t* pData;

	pData = __xuiBreadcrumbGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iSeparatorColor = iColor;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiBreadcrumbGetSeparatorColor(xui_widget pWidget)
{
	xui_breadcrumb_data_t* pData;

	pData = __xuiBreadcrumbGetData(pWidget);
	return (pData != NULL) ? pData->iSeparatorColor : 0u;
}

XUI_API int xuiBreadcrumbSetBackgroundColor(xui_widget pWidget, uint32_t iColor)
{
	xui_breadcrumb_data_t* pData;

	pData = __xuiBreadcrumbGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iBackgroundColor = iColor;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiBreadcrumbGetBackgroundColor(xui_widget pWidget)
{
	xui_breadcrumb_data_t* pData;

	pData = __xuiBreadcrumbGetData(pWidget);
	return (pData != NULL) ? pData->iBackgroundColor : 0u;
}

XUI_API int xuiBreadcrumbSetMetrics(xui_widget pWidget, float fGap, float fPaddingX, float fPaddingY)
{
	xui_breadcrumb_data_t* pData;

	pData = __xuiBreadcrumbGetData(pWidget);
	if ( (pData == NULL) || (fGap < 0.0f) || (fPaddingX < 0.0f) || (fPaddingY < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fGap = fGap;
	pData->fPaddingX = fPaddingX;
	pData->fPaddingY = fPaddingY;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiBreadcrumbGetMetrics(xui_widget pWidget, float* pGap, float* pPaddingX, float* pPaddingY)
{
	xui_breadcrumb_data_t* pData;

	pData = __xuiBreadcrumbGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pGap != NULL ) *pGap = pData->fGap;
	if ( pPaddingX != NULL ) *pPaddingX = pData->fPaddingX;
	if ( pPaddingY != NULL ) *pPaddingY = pData->fPaddingY;
	return XUI_OK;
}

XUI_API xui_rect_t xuiBreadcrumbGetItemRect(xui_widget pWidget, int iIndex)
{
	xui_breadcrumb_data_t* pData;

	pData = __xuiBreadcrumbGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount) ) return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	return pData->arrItems[iIndex].tRect;
}

XUI_API xui_rect_t xuiBreadcrumbGetSeparatorRect(xui_widget pWidget, int iIndex)
{
	xui_breadcrumb_data_t* pData;

	pData = __xuiBreadcrumbGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iItemCount - 1) ) return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	return pData->arrItems[iIndex].tSeparatorRect;
}

XUI_API int xuiBreadcrumbGetHoverIndex(xui_widget pWidget)
{
	xui_breadcrumb_data_t* pData;

	pData = __xuiBreadcrumbGetData(pWidget);
	return (pData != NULL) ? pData->iHoverIndex : -1;
}

XUI_API int xuiBreadcrumbGetActiveIndex(xui_widget pWidget)
{
	xui_breadcrumb_data_t* pData;

	pData = __xuiBreadcrumbGetData(pWidget);
	return (pData != NULL) ? pData->iActiveIndex : -1;
}

XUI_API int xuiBreadcrumbGetClickCount(xui_widget pWidget)
{
	xui_breadcrumb_data_t* pData;

	pData = __xuiBreadcrumbGetData(pWidget);
	return (pData != NULL) ? pData->iClickCount : 0;
}
