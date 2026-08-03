#include "xui_internal.h"

#include <string.h>

typedef struct xui_carousel_data_t {
	xui_carousel_change_proc onChange;
	void* pChangeUser;
	xui_widget arrPages[XUI_CAROUSEL_MAX_PAGES];
	xui_widget pOverlay;
	xui_font pFont;
	int iPageCount;
	int iCurrent;
	int bLoop;
	int bAutoPlay;
	int bShowIndicators;
	int bShowArrowsOnHover;
	int iHoverIndicator;
	int iActiveIndicator;
	int iHoverArrow;
	int iActiveArrow;
	int iChangeCount;
	int bHover;
	float fAutoInterval;
	float fAutoElapsed;
	float fArrowSize;
	float fIndicatorSize;
	float fIndicatorGap;
	float fIndicatorBottom;
	xui_rect_t tContentRect;
	xui_rect_t tPrevArrowRect;
	xui_rect_t tNextArrowRect;
	xui_rect_t tIndicatorGroupRect;
	xui_rect_t arrIndicatorRects[XUI_CAROUSEL_MAX_PAGES];
	uint32_t iBackgroundColor;
	uint32_t iArrowColor;
	uint32_t iArrowHoverColor;
	uint32_t iArrowTextColor;
	uint32_t iIndicatorColor;
	uint32_t iIndicatorActiveColor;
	uint32_t iIndicatorHoverColor;
	uint32_t iFocusColor;
} xui_carousel_data_t;

static int __xuiCarouselDescValid(const xui_carousel_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) {
		return 0;
	}
	if ( (pDesc->iPageCount < 0) || (pDesc->iPageCount > XUI_CAROUSEL_MAX_PAGES) ) {
		return 0;
	}
	if ( (pDesc->fAutoInterval < 0.0f) ||
	     (pDesc->fArrowSize < 0.0f) ||
	     (pDesc->fIndicatorSize < 0.0f) ||
	     (pDesc->fIndicatorGap < 0.0f) ||
	     (pDesc->fIndicatorBottom < 0.0f) ) {
		return 0;
	}
	return 1;
}

static int __xuiCarouselAlpha(uint32_t iColor)
{
	return (int)(iColor & 0xffu);
}

static float __xuiCarouselMaxFloat(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static int __xuiCarouselClampInt(int iValue, int iMin, int iMax)
{
	if ( iValue < iMin ) return iMin;
	if ( iValue > iMax ) return iMax;
	return iValue;
}

static int __xuiCarouselRectContains(xui_rect_t tRect, float fX, float fY)
{
	return (tRect.fW > 0.0f) &&
	       (tRect.fH > 0.0f) &&
	       (fX >= tRect.fX) &&
	       (fY >= tRect.fY) &&
	       (fX < (tRect.fX + tRect.fW)) &&
	       (fY < (tRect.fY + tRect.fH));
}

static xui_carousel_data_t* __xuiCarouselGetData(xui_widget pWidget)
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
	pType = xuiWidgetFindType(pContext, "carousel");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_carousel_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiCarouselStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
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

static int __xuiCarouselStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
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

static int __xuiCarouselStyleInt(xui_widget pWidget, const char* sName, int* pValue)
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

static xui_font __xuiCarouselStyleFont(xui_widget pWidget, xui_font pBaseFont)
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

static void __xuiCarouselResolve(xui_widget pWidget, const xui_carousel_data_t* pData, xui_carousel_data_t* pResolved)
{
	int iValue;

	*pResolved = *pData;
	pResolved->pFont = __xuiCarouselStyleFont(pWidget, (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget)));
	(void)__xuiCarouselStyleColor(pWidget, "carousel.background.color", &pResolved->iBackgroundColor);
	(void)__xuiCarouselStyleColor(pWidget, "carousel.arrow.color", &pResolved->iArrowColor);
	(void)__xuiCarouselStyleColor(pWidget, "carousel.arrow.hover_color", &pResolved->iArrowHoverColor);
	(void)__xuiCarouselStyleColor(pWidget, "carousel.arrow.text_color", &pResolved->iArrowTextColor);
	(void)__xuiCarouselStyleColor(pWidget, "carousel.indicator.color", &pResolved->iIndicatorColor);
	(void)__xuiCarouselStyleColor(pWidget, "carousel.indicator.active_color", &pResolved->iIndicatorActiveColor);
	(void)__xuiCarouselStyleColor(pWidget, "carousel.indicator.hover_color", &pResolved->iIndicatorHoverColor);
	(void)__xuiCarouselStyleColor(pWidget, "carousel.focus.color", &pResolved->iFocusColor);
	(void)__xuiCarouselStyleFloat(pWidget, "carousel.arrow.size", &pResolved->fArrowSize);
	(void)__xuiCarouselStyleFloat(pWidget, "carousel.indicator.size", &pResolved->fIndicatorSize);
	(void)__xuiCarouselStyleFloat(pWidget, "carousel.indicator.gap", &pResolved->fIndicatorGap);
	(void)__xuiCarouselStyleFloat(pWidget, "carousel.indicator.bottom", &pResolved->fIndicatorBottom);
	(void)__xuiCarouselStyleFloat(pWidget, "carousel.auto.interval", &pResolved->fAutoInterval);
	iValue = pResolved->bAutoPlay;
	if ( __xuiCarouselStyleInt(pWidget, "carousel.auto.enabled", &iValue) ) pResolved->bAutoPlay = iValue ? 1 : 0;
	iValue = pResolved->bShowIndicators;
	if ( __xuiCarouselStyleInt(pWidget, "carousel.indicator.visible", &iValue) ) pResolved->bShowIndicators = iValue ? 1 : 0;
	iValue = pResolved->bShowArrowsOnHover;
	if ( __xuiCarouselStyleInt(pWidget, "carousel.arrow.hover_only", &iValue) ) pResolved->bShowArrowsOnHover = iValue ? 1 : 0;
	if ( pResolved->fArrowSize < 16.0f ) pResolved->fArrowSize = 16.0f;
	if ( pResolved->fIndicatorSize < 4.0f ) pResolved->fIndicatorSize = 4.0f;
	if ( pResolved->fIndicatorGap < 0.0f ) pResolved->fIndicatorGap = 0.0f;
	if ( pResolved->fIndicatorBottom < 0.0f ) pResolved->fIndicatorBottom = 0.0f;
	if ( pResolved->fAutoInterval < 0.0f ) pResolved->fAutoInterval = 0.0f;
}

static int __xuiCarouselInvalidate(xui_widget pWidget, xui_carousel_data_t* pData, uint32_t iFlags)
{
	int iRet;

	iRet = xuiWidgetInvalidate(pWidget, iFlags);
	if ( (iRet == XUI_OK) && (pData != NULL) && (pData->pOverlay != NULL) ) {
		iRet = xuiWidgetInvalidate(pData->pOverlay, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return iRet;
}

static void __xuiCarouselClearRects(xui_carousel_data_t* pData)
{
	int i;

	if ( pData == NULL ) return;
	pData->tPrevArrowRect = (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	pData->tNextArrowRect = (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	pData->tIndicatorGroupRect = (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	for ( i = 0; i < XUI_CAROUSEL_MAX_PAGES; i++ ) {
		pData->arrIndicatorRects[i] = (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	}
}

static void __xuiCarouselComputeRects(xui_widget pCarousel, xui_carousel_data_t* pData, const xui_carousel_data_t* pResolved, xui_rect_t tContent)
{
	float fArrow;
	float fIndicator;
	float fGap;
	float fGroupW;
	float fGroupH;
	float fX;
	float fY;
	int i;

	(void)pCarousel;
	if ( (pData == NULL) || (pResolved == NULL) ) return;
	pData->tContentRect = tContent;
	__xuiCarouselClearRects(pData);
	if ( (tContent.fW <= 0.0f) || (tContent.fH <= 0.0f) ) return;

	fArrow = __xuiCarouselMaxFloat(16.0f, pResolved->fArrowSize);
	pData->tPrevArrowRect = xuiInternalSnapRect((xui_rect_t){
		tContent.fX + 18.0f,
		tContent.fY + (tContent.fH - fArrow) * 0.5f,
		fArrow,
		fArrow
	});
	pData->tNextArrowRect = xuiInternalSnapRect((xui_rect_t){
		tContent.fX + tContent.fW - fArrow - 18.0f,
		tContent.fY + (tContent.fH - fArrow) * 0.5f,
		fArrow,
		fArrow
	});
	if ( pResolved->bShowIndicators && (pResolved->iPageCount > 0) ) {
		fIndicator = __xuiCarouselMaxFloat(4.0f, pResolved->fIndicatorSize);
		fGap = __xuiCarouselMaxFloat(0.0f, pResolved->fIndicatorGap);
		fGroupW = ((float)pResolved->iPageCount * fIndicator) + ((float)(pResolved->iPageCount - 1) * fGap) + 16.0f;
		fGroupH = fIndicator + 10.0f;
		fX = tContent.fX + (tContent.fW - fGroupW) * 0.5f;
		fY = tContent.fY + tContent.fH - fGroupH - pResolved->fIndicatorBottom;
		pData->tIndicatorGroupRect = xuiInternalSnapRect((xui_rect_t){fX, fY, fGroupW, fGroupH});
		fX += 8.0f;
		fY += (fGroupH - fIndicator) * 0.5f;
		for ( i = 0; i < pResolved->iPageCount; i++ ) {
			pData->arrIndicatorRects[i] = xuiInternalSnapRect((xui_rect_t){fX, fY, fIndicator, fIndicator});
			fX += fIndicator + fGap;
		}
	}
}

static int __xuiCarouselDrawFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || (__xuiCarouselAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	if ( (pProxy != NULL) && (pProxy->drawRectFill != NULL) && (pDraw != NULL) ) {
		return pProxy->drawRectFill(pProxy, pDraw, xuiInternalSnapRect(tRect), iColor);
	}
	return XUI_ERROR_NOT_INITIALIZED;
}

static int __xuiCarouselDrawRectFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || (__xuiCarouselAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	return __xuiCarouselDrawFill(pProxy, pDraw, tRect, iColor);
}

static int __xuiCarouselDrawStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || (fWidth <= 0.0f) || (__xuiCarouselAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	if ( (pProxy != NULL) && (pDraw != NULL) ) {
		if ( pProxy->drawRectStroke != NULL ) {
			return pProxy->drawRectStroke(pProxy, pDraw, xuiInternalSnapRect(tRect), fWidth, iColor);
		}
	}
	return XUI_ERROR_NOT_INITIALIZED;
}

static int __xuiCarouselDrawCircle(xui_proxy pProxy, xui_draw_context pDraw, float fX, float fY, float fRadius, uint32_t iColor)
{
	if ( (fRadius <= 0.0f) || (__xuiCarouselAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	if ( (pProxy != NULL) && (pProxy->drawCircleFill != NULL) && (pDraw != NULL) ) {
		return pProxy->drawCircleFill(pProxy, pDraw, xuiInternalSnapPixel(fX), xuiInternalSnapPixel(fY), fRadius, iColor);
	}
	return __xuiCarouselDrawRectFill(pProxy, pDraw, (xui_rect_t){fX - fRadius, fY - fRadius, fRadius * 2.0f, fRadius * 2.0f}, iColor);
}

static int __xuiCarouselDrawLine(xui_proxy pProxy, xui_draw_context pDraw, float fX0, float fY0, float fX1, float fY1, float fWidth, uint32_t iColor)
{
	if ( __xuiCarouselAlpha(iColor) == 0 ) {
		return XUI_OK;
	}
	if ( (pProxy != NULL) && (pProxy->drawLine != NULL) && (pDraw != NULL) ) {
		return pProxy->drawLine(pProxy, pDraw,
			xuiInternalSnapPixel(fX0), xuiInternalSnapPixel(fY0),
			xuiInternalSnapPixel(fX1), xuiInternalSnapPixel(fY1),
			fWidth, iColor);
	}
	return XUI_ERROR_NOT_INITIALIZED;
}

static int __xuiCarouselDrawArrow(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, int iArrow, uint32_t iFill, uint32_t iLine)
{
	float fCX;
	float fCY;
	float fS;
	int iRet;

	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) return XUI_OK;
	fCX = tRect.fX + tRect.fW * 0.5f;
	fCY = tRect.fY + tRect.fH * 0.5f;
	fS = tRect.fW * 0.22f;
	iRet = __xuiCarouselDrawCircle(pProxy, pDraw, fCX, fCY, tRect.fW * 0.5f, iFill);
	if ( iRet != XUI_OK ) return iRet;
	if ( iArrow == XUI_CAROUSEL_ARROW_PREV ) {
		iRet = __xuiCarouselDrawLine(pProxy, pDraw, fCX + fS * 0.45f, fCY - fS, fCX - fS * 0.45f, fCY, 2.0f, iLine);
		if ( iRet == XUI_OK ) iRet = __xuiCarouselDrawLine(pProxy, pDraw, fCX - fS * 0.45f, fCY, fCX + fS * 0.45f, fCY + fS, 2.0f, iLine);
	} else {
		iRet = __xuiCarouselDrawLine(pProxy, pDraw, fCX - fS * 0.45f, fCY - fS, fCX + fS * 0.45f, fCY, 2.0f, iLine);
		if ( iRet == XUI_OK ) iRet = __xuiCarouselDrawLine(pProxy, pDraw, fCX + fS * 0.45f, fCY, fCX - fS * 0.45f, fCY + fS, 2.0f, iLine);
	}
	return iRet;
}

static int __xuiCarouselBaseRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_carousel_data_t* pData;
	xui_carousel_data_t tResolved;
	xui_proxy pProxy;

	(void)iStateId;
	(void)pUser;
	pData = __xuiCarouselGetData(pWidget);
	if ( (pData == NULL) || (pDraw == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	__xuiCarouselResolve(pWidget, pData, &tResolved);
	return __xuiCarouselDrawFill(pProxy, pDraw, xuiWidgetGetContentRect(pWidget), tResolved.iBackgroundColor);
}

static int __xuiCarouselOverlayRender(xui_widget pOverlay, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_widget pCarousel;
	xui_carousel_data_t* pData;
	xui_carousel_data_t tResolved;
	xui_proxy pProxy;
	uint32_t iFill;
	float fCX;
	float fCY;
	float fRadius;
	xui_rect_t tFocusRect;
	int i;
	int iRet;
	int bShowArrows;

	(void)iStateId;
	pCarousel = (xui_widget)pUser;
	pData = __xuiCarouselGetData(pCarousel);
	if ( (pOverlay == NULL) || (pData == NULL) || (pDraw == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pCarousel));
	if ( pProxy == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	__xuiCarouselResolve(pCarousel, pData, &tResolved);
	__xuiCarouselComputeRects(pCarousel, pData, &tResolved, xuiWidgetGetContentRect(pCarousel));

	iRet = XUI_OK;
	bShowArrows = !tResolved.bShowArrowsOnHover || pData->bHover || (pData->iActiveArrow != XUI_CAROUSEL_ARROW_NONE);
	if ( bShowArrows && (tResolved.iPageCount > 1) ) {
		iFill = (pData->iHoverArrow == XUI_CAROUSEL_ARROW_PREV || pData->iActiveArrow == XUI_CAROUSEL_ARROW_PREV) ? tResolved.iArrowHoverColor : tResolved.iArrowColor;
		iRet = __xuiCarouselDrawArrow(pProxy, pDraw, pData->tPrevArrowRect, XUI_CAROUSEL_ARROW_PREV, iFill, tResolved.iArrowTextColor);
		if ( iRet != XUI_OK ) return iRet;
		iFill = (pData->iHoverArrow == XUI_CAROUSEL_ARROW_NEXT || pData->iActiveArrow == XUI_CAROUSEL_ARROW_NEXT) ? tResolved.iArrowHoverColor : tResolved.iArrowColor;
		iRet = __xuiCarouselDrawArrow(pProxy, pDraw, pData->tNextArrowRect, XUI_CAROUSEL_ARROW_NEXT, iFill, tResolved.iArrowTextColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( tResolved.bShowIndicators && (tResolved.iPageCount > 1) ) {
		iRet = __xuiCarouselDrawRectFill(pProxy, pDraw, pData->tIndicatorGroupRect, XUI_COLOR_RGBA(0, 0, 0, 62));
		if ( iRet != XUI_OK ) return iRet;
		for ( i = 0; i < tResolved.iPageCount; i++ ) {
			fCX = pData->arrIndicatorRects[i].fX + pData->arrIndicatorRects[i].fW * 0.5f;
			fCY = pData->arrIndicatorRects[i].fY + pData->arrIndicatorRects[i].fH * 0.5f;
			fRadius = pData->arrIndicatorRects[i].fW * 0.5f;
			if ( i == tResolved.iCurrent ) {
				iFill = tResolved.iIndicatorActiveColor;
			} else if ( i == pData->iHoverIndicator ) {
				iFill = tResolved.iIndicatorHoverColor;
			} else {
				iFill = tResolved.iIndicatorColor;
			}
			iRet = __xuiCarouselDrawCircle(pProxy, pDraw, fCX, fCY, fRadius, iFill);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	if ( (xuiGetFocusWidget(xuiWidgetGetContext(pCarousel)) == pCarousel) && (tResolved.iFocusColor & 0xffu) != 0u ) {
		tFocusRect = xuiWidgetGetContentRect(pOverlay);
		tFocusRect.fX += 0.5f;
		tFocusRect.fY += 0.5f;
		tFocusRect.fW -= 1.0f;
		tFocusRect.fH -= 1.0f;
		iRet = __xuiCarouselDrawStroke(pProxy, pDraw, tFocusRect, 1.0f, tResolved.iFocusColor);
	}
	return iRet;
}

static int __xuiCarouselConfigurePage(xui_widget pPage)
{
	if ( pPage == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	(void)xuiWidgetSetLayoutType(pPage, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetSizeMode(pPage, XUI_SIZE_FILL, XUI_SIZE_FILL);
	(void)xuiWidgetSetOverflow(pPage, XUI_OVERFLOW_CLIP);
	(void)xuiWidgetSetFocusable(pPage, 0);
	(void)xuiWidgetSetTabStop(pPage, 0);
	(void)xuiWidgetSetHitTestVisible(pPage, 1);
	return XUI_OK;
}

static int __xuiCarouselConfigureOverlay(xui_widget pOverlay, xui_widget pCarousel)
{
	xui_cache_policy_t tPolicy;
	int iRet;

	if ( (pOverlay == NULL) || (pCarousel == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&tPolicy, 0, sizeof(tPolicy));
	tPolicy.iSize = sizeof(tPolicy);
	tPolicy.iPolicy = XUI_CACHE_POLICY_SELF;
	tPolicy.iFlags = XUI_CACHE_CLEAR_ON_UPDATE | XUI_CACHE_UPDATE_ALL_STATES;
	tPolicy.iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	iRet = xuiWidgetSetCachePolicy(pOverlay, &tPolicy);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetCacheRenderCallback(pOverlay, __xuiCarouselOverlayRender, pCarousel);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetLayoutType(pOverlay, XUI_LAYOUT_MANUAL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetSizeMode(pOverlay, XUI_SIZE_FIXED, XUI_SIZE_FIXED);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetOverflow(pOverlay, XUI_OVERFLOW_VISIBLE);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetFocusable(pOverlay, 0);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetTabStop(pOverlay, 0);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetHitTestVisible(pOverlay, 0);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetLayer(pOverlay, XUI_LAYER_NORMAL, 10);
	return iRet;
}

static int __xuiCarouselEnsurePage(xui_widget pCarousel, xui_carousel_data_t* pData, int iIndex)
{
	xui_widget pPage;
	int iRet;

	if ( (pCarousel == NULL) || (pData == NULL) || (iIndex < 0) || (iIndex >= XUI_CAROUSEL_MAX_PAGES) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->arrPages[iIndex] != NULL ) {
		return XUI_OK;
	}
	iRet = xuiWidgetCreate(xuiWidgetGetContext(pCarousel), &pPage);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiCarouselConfigurePage(pPage);
	if ( iRet == XUI_OK ) iRet = xuiWidgetAddChild(pCarousel, pPage);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pPage);
		return iRet;
	}
	pData->arrPages[iIndex] = pPage;
	(void)xuiWidgetSetVisible(pPage, 0);
	return XUI_OK;
}

static int __xuiCarouselSyncPages(xui_widget pCarousel, xui_carousel_data_t* pData)
{
	int i;
	int iRet;

	if ( (pCarousel == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iCurrent = __xuiCarouselClampInt(pData->iCurrent, 0, pData->iPageCount - 1);
	for ( i = 0; i < XUI_CAROUSEL_MAX_PAGES; i++ ) {
		if ( pData->arrPages[i] == NULL ) continue;
		iRet = xuiWidgetSetVisible(pData->arrPages[i], (i < pData->iPageCount) && (i == pData->iCurrent));
		if ( iRet != XUI_OK ) return iRet;
		iRet = xuiWidgetSetEnabled(pData->arrPages[i], (i < pData->iPageCount));
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pData->pOverlay != NULL ) {
		iRet = xuiWidgetSetVisible(pData->pOverlay, 1);
		if ( iRet != XUI_OK ) return iRet;
	}
	return __xuiCarouselInvalidate(pCarousel, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiCarouselSetPageCountInternal(xui_widget pCarousel, xui_carousel_data_t* pData, int iPageCount)
{
	int i;
	int iRet;

	if ( (pCarousel == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iPageCount < 1 ) iPageCount = 1;
	if ( iPageCount > XUI_CAROUSEL_MAX_PAGES ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < iPageCount; i++ ) {
		iRet = __xuiCarouselEnsurePage(pCarousel, pData, i);
		if ( iRet != XUI_OK ) return iRet;
	}
	pData->iPageCount = iPageCount;
	if ( pData->iHoverIndicator >= iPageCount ) pData->iHoverIndicator = -1;
	if ( pData->iActiveIndicator >= iPageCount ) pData->iActiveIndicator = -1;
	return __xuiCarouselSyncPages(pCarousel, pData);
}

static int __xuiCarouselSetCurrentInternal(xui_widget pCarousel, xui_carousel_data_t* pData, int iIndex, int bNotify)
{
	int iOld;
	int iRet;

	if ( (pCarousel == NULL) || (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iPageCount) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->iCurrent == iIndex ) {
		pData->fAutoElapsed = 0.0f;
		return XUI_OK;
	}
	iOld = pData->iCurrent;
	pData->iCurrent = iIndex;
	pData->fAutoElapsed = 0.0f;
	pData->iChangeCount++;
	iRet = __xuiCarouselSyncPages(pCarousel, pData);
	if ( iRet != XUI_OK ) return iRet;
	if ( bNotify && (pData->onChange != NULL) ) {
		pData->onChange(pCarousel, iOld, iIndex, pData->pChangeUser);
	}
	return XUI_OK;
}

static int __xuiCarouselMove(xui_widget pCarousel, xui_carousel_data_t* pData, int iDelta, int bNotify)
{
	int iNext;

	if ( (pCarousel == NULL) || (pData == NULL) || (pData->iPageCount <= 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	iNext = pData->iCurrent + iDelta;
	if ( pData->bLoop ) {
		while ( iNext < 0 ) iNext += pData->iPageCount;
		while ( iNext >= pData->iPageCount ) iNext -= pData->iPageCount;
	} else {
		if ( iNext < 0 ) iNext = 0;
		if ( iNext >= pData->iPageCount ) iNext = pData->iPageCount - 1;
	}
	return __xuiCarouselSetCurrentInternal(pCarousel, pData, iNext, bNotify);
}

static int __xuiCarouselHitIndicator(const xui_carousel_data_t* pData, float fX, float fY)
{
	int i;

	if ( pData == NULL ) return -1;
	for ( i = 0; i < pData->iPageCount; i++ ) {
		if ( __xuiCarouselRectContains(pData->arrIndicatorRects[i], fX, fY) ) {
			return i;
		}
	}
	return -1;
}

static int __xuiCarouselHitArrow(const xui_carousel_data_t* pData, const xui_carousel_data_t* pResolved, float fX, float fY)
{
	int bShowArrows;

	if ( (pData == NULL) || (pResolved == NULL) || (pResolved->iPageCount <= 1) ) return XUI_CAROUSEL_ARROW_NONE;
	bShowArrows = !pResolved->bShowArrowsOnHover || pData->bHover || (pData->iActiveArrow != XUI_CAROUSEL_ARROW_NONE);
	if ( !bShowArrows ) return XUI_CAROUSEL_ARROW_NONE;
	if ( __xuiCarouselRectContains(pData->tPrevArrowRect, fX, fY) ) return XUI_CAROUSEL_ARROW_PREV;
	if ( __xuiCarouselRectContains(pData->tNextArrowRect, fX, fY) ) return XUI_CAROUSEL_ARROW_NEXT;
	return XUI_CAROUSEL_ARROW_NONE;
}

static int __xuiCarouselUpdateHover(xui_widget pCarousel, xui_carousel_data_t* pData, const xui_event_t* pEvent)
{
	xui_carousel_data_t tResolved;
	xui_rect_t tWorld;
	float fX;
	float fY;
	int iIndicator;
	int iArrow;
	int bChanged;

	if ( (pCarousel == NULL) || (pData == NULL) || (pEvent == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiCarouselResolve(pCarousel, pData, &tResolved);
	__xuiCarouselComputeRects(pCarousel, pData, &tResolved, xuiWidgetGetContentRect(pCarousel));
	tWorld = xuiWidgetGetWorldRect(pCarousel);
	fX = pEvent->fX - tWorld.fX;
	fY = pEvent->fY - tWorld.fY;
	iIndicator = tResolved.bShowIndicators ? __xuiCarouselHitIndicator(pData, fX, fY) : -1;
	iArrow = __xuiCarouselHitArrow(pData, &tResolved, fX, fY);
	bChanged = (pData->iHoverIndicator != iIndicator) || (pData->iHoverArrow != iArrow);
	pData->iHoverIndicator = iIndicator;
	pData->iHoverArrow = iArrow;
	if ( bChanged ) {
		return __xuiCarouselInvalidate(pCarousel, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return XUI_OK;
}

static int __xuiCarouselEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_carousel_data_t* pData;
	xui_carousel_data_t tResolved;
	xui_rect_t tWorld;
	float fX;
	float fY;
	int iHitIndicator;
	int iHitArrow;
	int iRet;

	(void)pUser;
	pData = __xuiCarouselGetData(pWidget);
	if ( (pData == NULL) || (pEvent == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiCarouselResolve(pWidget, pData, &tResolved);
	__xuiCarouselComputeRects(pWidget, pData, &tResolved, xuiWidgetGetContentRect(pWidget));
	tWorld = xuiWidgetGetWorldRect(pWidget);
	fX = pEvent->fX - tWorld.fX;
	fY = pEvent->fY - tWorld.fY;
	iHitIndicator = tResolved.bShowIndicators ? __xuiCarouselHitIndicator(pData, fX, fY) : -1;
	iHitArrow = __xuiCarouselHitArrow(pData, &tResolved, fX, fY);

	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_ENTER:
		pData->bHover = 1;
		(void)__xuiCarouselUpdateHover(pWidget, pData, pEvent);
		return __xuiCarouselInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	case XUI_EVENT_POINTER_MOVE:
		pData->bHover = 1;
		(void)__xuiCarouselUpdateHover(pWidget, pData, pEvent);
		return (pData->iHoverIndicator >= 0 || pData->iHoverArrow != XUI_CAROUSEL_ARROW_NONE) ? XUI_EVENT_DISPATCH_STOP : XUI_OK;
	case XUI_EVENT_POINTER_LEAVE:
		pData->bHover = 0;
		pData->iHoverIndicator = -1;
		pData->iHoverArrow = XUI_CAROUSEL_ARROW_NONE;
		return __xuiCarouselInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	case XUI_EVENT_POINTER_DOWN:
		if ( (pEvent->iButton == XUI_POINTER_BUTTON_LEFT) &&
		     ((iHitIndicator >= 0) || (iHitArrow != XUI_CAROUSEL_ARROW_NONE)) ) {
			pData->iActiveIndicator = iHitIndicator;
			pData->iActiveArrow = iHitArrow;
			pData->bHover = 1;
			(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pWidget);
			(void)xuiSetPointerCapture(xuiWidgetGetContext(pWidget), pWidget);
			(void)__xuiCarouselInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_UP:
		if ( (pEvent->iButton == XUI_POINTER_BUTTON_LEFT) &&
		     ((pData->iActiveIndicator >= 0) || (pData->iActiveArrow != XUI_CAROUSEL_ARROW_NONE)) ) {
			iRet = XUI_OK;
			if ( (pData->iActiveIndicator >= 0) && (iHitIndicator == pData->iActiveIndicator) ) {
				iRet = __xuiCarouselSetCurrentInternal(pWidget, pData, pData->iActiveIndicator, 1);
			} else if ( (pData->iActiveArrow == XUI_CAROUSEL_ARROW_PREV) && (iHitArrow == XUI_CAROUSEL_ARROW_PREV) ) {
				iRet = __xuiCarouselMove(pWidget, pData, -1, 1);
			} else if ( (pData->iActiveArrow == XUI_CAROUSEL_ARROW_NEXT) && (iHitArrow == XUI_CAROUSEL_ARROW_NEXT) ) {
				iRet = __xuiCarouselMove(pWidget, pData, 1, 1);
			}
			pData->iActiveIndicator = -1;
			pData->iActiveArrow = XUI_CAROUSEL_ARROW_NONE;
			(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
			(void)__xuiCarouselUpdateHover(pWidget, pData, pEvent);
			(void)__xuiCarouselInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
			return (iRet == XUI_OK) ? (int)XUI_EVENT_DISPATCH_STOP : iRet;
		}
		break;
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		pData->iActiveIndicator = -1;
		pData->iActiveArrow = XUI_CAROUSEL_ARROW_NONE;
		return __xuiCarouselInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	case XUI_EVENT_KEY_DOWN:
		if ( pEvent->iKey == XUI_KEY_LEFT ) return __xuiCarouselMove(pWidget, pData, -1, 1) | (int)XUI_EVENT_DISPATCH_STOP;
		if ( pEvent->iKey == XUI_KEY_RIGHT ) return __xuiCarouselMove(pWidget, pData, 1, 1) | (int)XUI_EVENT_DISPATCH_STOP;
		break;
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
	case XUI_EVENT_ENABLED_CHANGED:
	case XUI_EVENT_VISIBLE_CHANGED:
		return __xuiCarouselInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiCarouselArrange(xui_widget pWidget, xui_rect_t tContentRect, void* pUser)
{
	xui_carousel_data_t* pData;
	xui_carousel_data_t tResolved;
	xui_rect_t tChild;
	int i;
	int iRet;

	(void)pUser;
	pData = __xuiCarouselGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiCarouselResolve(pWidget, pData, &tResolved);
	__xuiCarouselComputeRects(pWidget, pData, &tResolved, tContentRect);
	tChild = (xui_rect_t){tContentRect.fX, tContentRect.fY, tContentRect.fW, tContentRect.fH};
	for ( i = 0; i < pData->iPageCount; i++ ) {
		if ( pData->arrPages[i] == NULL ) continue;
		iRet = xuiWidgetArrange(pData->arrPages[i], tChild);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pData->pOverlay != NULL ) {
		iRet = xuiWidgetArrange(pData->pOverlay, tChild);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiCarouselContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	(void)pWidget;
	(void)tConstraint;
	(void)pUser;
	if ( pSize == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pSize->fX = 460.0f;
	pSize->fY = 290.0f;
	return XUI_OK;
}

static int __xuiCarouselUpdate(xui_widget pWidget, float fDelta, void* pUser)
{
	xui_carousel_data_t* pData;
	xui_carousel_data_t tResolved;
	int iRet;

	(void)pUser;
	pData = __xuiCarouselGetData(pWidget);
	if ( (pData == NULL) || (fDelta < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiCarouselResolve(pWidget, pData, &tResolved);
	if ( !tResolved.bAutoPlay || (tResolved.fAutoInterval <= 0.0f) || (tResolved.iPageCount <= 1) ) {
		pData->fAutoElapsed = 0.0f;
		return XUI_OK;
	}
	if ( pData->iActiveArrow != XUI_CAROUSEL_ARROW_NONE || pData->iActiveIndicator >= 0 ) {
		return XUI_OK;
	}
	pData->fAutoElapsed += fDelta;
	if ( pData->fAutoElapsed < tResolved.fAutoInterval ) {
		return XUI_OK;
	}
	pData->fAutoElapsed = 0.0f;
	iRet = __xuiCarouselMove(pWidget, pData, 1, 1);
	return (iRet == XUI_ERROR_INVALID_ARGUMENT) ? XUI_OK : iRet;
}

static void __xuiCarouselDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_MANUAL;
	pLayout->iWidthMode = XUI_SIZE_FIXED;
	pLayout->iHeightMode = XUI_SIZE_FIXED;
	pLayout->iFlowMode = XUI_FLOW_BLOCK;
	pLayout->iDock = XUI_DOCK_FILL;
	pLayout->iOverflow = XUI_OVERFLOW_CLIP;
	pLayout->iAlignX = XUI_ALIGN_START;
	pLayout->iAlignY = XUI_ALIGN_START;
	pLayout->iTableRowSpan = 1;
	pLayout->iTableColumnSpan = 1;
	pLayout->iGridColumnCount = 1;
	pLayout->fPreferredWidth = 460.0f;
	pLayout->fPreferredHeight = 290.0f;
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fShrink = 1.0f;
}

static void __xuiCarouselDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE | XUI_CACHE_UPDATE_ALL_STATES;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiCarouselInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_ENTER, __xuiCarouselEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiCarouselEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiCarouselEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiCarouselEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiCarouselEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiCarouselEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiCarouselEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiCarouselEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiCarouselEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_ENABLED_CHANGED, __xuiCarouselEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_VISIBLE_CHANGED, __xuiCarouselEvent, NULL);
	return iRet;
}

static int __xuiCarouselInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_carousel_data_t* pData;
	const xui_carousel_desc_t* pDesc;
	xui_context pContext;
	int iRet;
	int iPageCount;

	(void)pUser;
	pData = (xui_carousel_data_t*)pTypeData;
	pDesc = (const xui_carousel_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiCarouselDescValid(pDesc) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(pData, 0, sizeof(*pData));
	pContext = xuiWidgetGetContext(pWidget);
	pData->pFont = (pDesc != NULL && pDesc->pFont != NULL) ? pDesc->pFont : xuiGetDefaultFont(pContext);
	pData->iPageCount = 1;
	pData->iCurrent = 0;
	pData->bLoop = 1;
	pData->bAutoPlay = (pDesc != NULL && pDesc->bAutoPlay) ? 1 : 0;
	pData->bShowIndicators = (pDesc != NULL) ? (pDesc->bShowIndicators ? 1 : 0) : 1;
	pData->bShowArrowsOnHover = (pDesc != NULL) ? (pDesc->bShowArrowsOnHover ? 1 : 0) : 1;
	pData->iHoverIndicator = -1;
	pData->iActiveIndicator = -1;
	pData->iHoverArrow = XUI_CAROUSEL_ARROW_NONE;
	pData->iActiveArrow = XUI_CAROUSEL_ARROW_NONE;
	pData->fAutoInterval = (pDesc != NULL && pDesc->fAutoInterval > 0.0f) ? pDesc->fAutoInterval : 0.0f;
	if ( pData->bAutoPlay && pData->fAutoInterval <= 0.0f ) pData->fAutoInterval = 3.0f;
	pData->fArrowSize = (pDesc != NULL && pDesc->fArrowSize > 0.0f) ? pDesc->fArrowSize : 36.0f;
	pData->fIndicatorSize = (pDesc != NULL && pDesc->fIndicatorSize > 0.0f) ? pDesc->fIndicatorSize : 9.0f;
	pData->fIndicatorGap = (pDesc != NULL && pDesc->fIndicatorGap > 0.0f) ? pDesc->fIndicatorGap : 7.0f;
	pData->fIndicatorBottom = (pDesc != NULL && pDesc->fIndicatorBottom > 0.0f) ? pDesc->fIndicatorBottom : 20.0f;
	pData->iBackgroundColor = (pDesc != NULL && pDesc->iBackgroundColor != 0) ? pDesc->iBackgroundColor : XUI_COLOR_RGBA(255, 82, 36, 255);
	pData->iArrowColor = (pDesc != NULL && pDesc->iArrowColor != 0) ? pDesc->iArrowColor : XUI_COLOR_RGBA(96, 44, 28, 100);
	pData->iArrowHoverColor = (pDesc != NULL && pDesc->iArrowHoverColor != 0) ? pDesc->iArrowHoverColor : XUI_COLOR_RGBA(96, 44, 28, 170);
	pData->iArrowTextColor = (pDesc != NULL && pDesc->iArrowTextColor != 0) ? pDesc->iArrowTextColor : XUI_COLOR_RGBA(255, 255, 255, 245);
	pData->iIndicatorColor = (pDesc != NULL && pDesc->iIndicatorColor != 0) ? pDesc->iIndicatorColor : XUI_COLOR_RGBA(255, 255, 255, 150);
	pData->iIndicatorActiveColor = (pDesc != NULL && pDesc->iIndicatorActiveColor != 0) ? pDesc->iIndicatorActiveColor : XUI_COLOR_RGBA(255, 255, 255, 255);
	pData->iIndicatorHoverColor = (pDesc != NULL && pDesc->iIndicatorHoverColor != 0) ? pDesc->iIndicatorHoverColor : XUI_COLOR_RGBA(255, 255, 255, 215);
	pData->iFocusColor = (pDesc != NULL && pDesc->iFocusColor != 0) ? pDesc->iFocusColor : XUI_COLOR_RGBA(255, 255, 255, 110);
	(void)xuiWidgetSetLayoutType(pWidget, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_CLIP);
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	iRet = xuiWidgetCreate(pContext, &pData->pOverlay);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiCarouselConfigureOverlay(pData->pOverlay, pWidget);
	if ( iRet == XUI_OK ) iRet = __xuiCarouselInitEvents(pWidget);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pData->pOverlay);
		pData->pOverlay = NULL;
		return iRet;
	}
	iPageCount = (pDesc != NULL && pDesc->iPageCount > 0) ? pDesc->iPageCount : 1;
	if ( iPageCount > XUI_CAROUSEL_MAX_PAGES ) iPageCount = XUI_CAROUSEL_MAX_PAGES;
	iRet = __xuiCarouselSetPageCountInternal(pWidget, pData, iPageCount);
	if ( iRet == XUI_OK ) {
		pData->iCurrent = __xuiCarouselClampInt((pDesc != NULL) ? pDesc->iCurrent : 0, 0, pData->iPageCount - 1);
		iRet = __xuiCarouselSyncPages(pWidget, pData);
	}
	if ( iRet == XUI_OK ) iRet = xuiWidgetAddChild(pWidget, pData->pOverlay);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pData->pOverlay);
		pData->pOverlay = NULL;
		return iRet;
	}
	return XUI_OK;
}

static void __xuiCarouselDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_carousel_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_carousel_data_t*)pTypeData;
	if ( pData != NULL ) {
		memset(pData, 0, sizeof(*pData));
	}
}

static void __xuiCarouselRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
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

static void __xuiCarouselRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;
	uint32_t iLayoutDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	iLayoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiCarouselRegisterStyleProperty(pContext, pType, "carousel.background.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCarouselRegisterStyleProperty(pContext, pType, "carousel.arrow.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCarouselRegisterStyleProperty(pContext, pType, "carousel.arrow.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCarouselRegisterStyleProperty(pContext, pType, "carousel.arrow.text_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCarouselRegisterStyleProperty(pContext, pType, "carousel.indicator.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCarouselRegisterStyleProperty(pContext, pType, "carousel.indicator.active_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCarouselRegisterStyleProperty(pContext, pType, "carousel.indicator.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCarouselRegisterStyleProperty(pContext, pType, "carousel.focus.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCarouselRegisterStyleProperty(pContext, pType, "carousel.arrow.size", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiCarouselRegisterStyleProperty(pContext, pType, "carousel.indicator.size", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiCarouselRegisterStyleProperty(pContext, pType, "carousel.indicator.gap", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiCarouselRegisterStyleProperty(pContext, pType, "carousel.indicator.bottom", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiCarouselRegisterStyleProperty(pContext, pType, "carousel.auto.enabled", XUI_STYLE_VALUE_BOOL, iPaintDirty, 0);
	__xuiCarouselRegisterStyleProperty(pContext, pType, "carousel.auto.interval", XUI_STYLE_VALUE_FLOAT, iPaintDirty, 0);
	__xuiCarouselRegisterStyleProperty(pContext, pType, "carousel.indicator.visible", XUI_STYLE_VALUE_BOOL, iLayoutDirty, 0);
	__xuiCarouselRegisterStyleProperty(pContext, pType, "carousel.arrow.hover_only", XUI_STYLE_VALUE_BOOL, iLayoutDirty, 0);
	__xuiCarouselRegisterStyleProperty(pContext, pType, "font.name", XUI_STYLE_VALUE_STRING, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
}

XUI_API xui_widget_type xuiCarouselGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_cache_policy_t tPolicy;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) return NULL;
	pType = xuiWidgetFindType(pContext, "carousel");
	if ( pType != NULL ) {
		__xuiCarouselRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "carousel";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_carousel_data_t);
	tDesc.onInit = __xuiCarouselInit;
	tDesc.onDestroy = __xuiCarouselDestroy;
	tDesc.onContentMeasure = __xuiCarouselContentMeasure;
	tDesc.onLayoutArrange = __xuiCarouselArrange;
	tDesc.onCacheRender = __xuiCarouselBaseRender;
	tDesc.onUpdate = __xuiCarouselUpdate;
	__xuiCarouselDefaultLayout(&tDesc.tLayout);
	__xuiCarouselDefaultCachePolicy(&tPolicy);
	tDesc.tCachePolicy = tPolicy;
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) return NULL;
	__xuiCarouselRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiCarouselCreate(xui_context pContext, xui_widget* ppWidget, const xui_carousel_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiCarouselDescValid(pDesc) ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppWidget = NULL;
	pType = xuiCarouselGetType(pContext);
	if ( pType == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiCarouselSetChange(xui_widget pWidget, xui_carousel_change_proc onChange, void* pUser)
{
	xui_carousel_data_t* pData = __xuiCarouselGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onChange = onChange;
	pData->pChangeUser = pUser;
	return XUI_OK;
}

XUI_API int xuiCarouselSetPageCount(xui_widget pWidget, int iPageCount)
{
	xui_carousel_data_t* pData = __xuiCarouselGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiCarouselSetPageCountInternal(pWidget, pData, iPageCount);
}

XUI_API int xuiCarouselGetPageCount(xui_widget pWidget)
{
	xui_carousel_data_t* pData = __xuiCarouselGetData(pWidget);
	return (pData != NULL) ? pData->iPageCount : 0;
}

XUI_API xui_widget xuiCarouselGetPageWidget(xui_widget pWidget, int iIndex)
{
	xui_carousel_data_t* pData = __xuiCarouselGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iPageCount) ) return NULL;
	return pData->arrPages[iIndex];
}

XUI_API int xuiCarouselAddPageChild(xui_widget pWidget, int iIndex, xui_widget pChild)
{
	xui_widget pPage;

	if ( pChild == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pPage = xuiCarouselGetPageWidget(pWidget, iIndex);
	if ( pPage == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiWidgetAddChild(pPage, pChild);
}

XUI_API int xuiCarouselSetCurrent(xui_widget pWidget, int iIndex, int bNotify)
{
	xui_carousel_data_t* pData = __xuiCarouselGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiCarouselSetCurrentInternal(pWidget, pData, iIndex, bNotify);
}

XUI_API int xuiCarouselGetCurrent(xui_widget pWidget)
{
	xui_carousel_data_t* pData = __xuiCarouselGetData(pWidget);
	return (pData != NULL) ? pData->iCurrent : -1;
}

XUI_API int xuiCarouselNext(xui_widget pWidget, int bNotify)
{
	xui_carousel_data_t* pData = __xuiCarouselGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiCarouselMove(pWidget, pData, 1, bNotify);
}

XUI_API int xuiCarouselPrev(xui_widget pWidget, int bNotify)
{
	xui_carousel_data_t* pData = __xuiCarouselGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiCarouselMove(pWidget, pData, -1, bNotify);
}

XUI_API int xuiCarouselSetAutoPlay(xui_widget pWidget, int bEnabled, float fInterval)
{
	xui_carousel_data_t* pData = __xuiCarouselGetData(pWidget);
	if ( (pData == NULL) || (fInterval < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bAutoPlay = bEnabled ? 1 : 0;
	pData->fAutoInterval = (fInterval > 0.0f) ? fInterval : 0.0f;
	if ( pData->bAutoPlay && pData->fAutoInterval <= 0.0f ) pData->fAutoInterval = 3.0f;
	pData->fAutoElapsed = 0.0f;
	return __xuiCarouselInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCarouselGetAutoPlay(xui_widget pWidget, int* pEnabled, float* pInterval)
{
	xui_carousel_data_t* pData = __xuiCarouselGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pEnabled != NULL ) *pEnabled = pData->bAutoPlay;
	if ( pInterval != NULL ) *pInterval = pData->fAutoInterval;
	return XUI_OK;
}

XUI_API int xuiCarouselSetLoop(xui_widget pWidget, int bLoop)
{
	xui_carousel_data_t* pData = __xuiCarouselGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bLoop = bLoop ? 1 : 0;
	return XUI_OK;
}

XUI_API int xuiCarouselGetLoop(xui_widget pWidget)
{
	xui_carousel_data_t* pData = __xuiCarouselGetData(pWidget);
	return (pData != NULL) ? pData->bLoop : 0;
}

XUI_API int xuiCarouselSetIndicatorsVisible(xui_widget pWidget, int bVisible)
{
	xui_carousel_data_t* pData = __xuiCarouselGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bShowIndicators = bVisible ? 1 : 0;
	pData->iHoverIndicator = -1;
	pData->iActiveIndicator = -1;
	return __xuiCarouselInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCarouselGetIndicatorsVisible(xui_widget pWidget)
{
	xui_carousel_data_t* pData = __xuiCarouselGetData(pWidget);
	return (pData != NULL) ? pData->bShowIndicators : 0;
}

XUI_API int xuiCarouselSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_carousel_data_t* pData = __xuiCarouselGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pFont = (pFont != NULL) ? pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	return __xuiCarouselInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_font xuiCarouselGetFont(xui_widget pWidget)
{
	xui_carousel_data_t* pData = __xuiCarouselGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API int xuiCarouselSetArrowsOnHover(xui_widget pWidget, int bEnabled)
{
	xui_carousel_data_t* pData = __xuiCarouselGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bShowArrowsOnHover = bEnabled ? 1 : 0;
	pData->iHoverArrow = XUI_CAROUSEL_ARROW_NONE;
	pData->iActiveArrow = XUI_CAROUSEL_ARROW_NONE;
	return __xuiCarouselInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCarouselGetArrowsOnHover(xui_widget pWidget)
{
	xui_carousel_data_t* pData = __xuiCarouselGetData(pWidget);
	return (pData != NULL) ? pData->bShowArrowsOnHover : 0;
}

XUI_API int xuiCarouselSetMetrics(xui_widget pWidget, float fArrowSize, float fIndicatorSize, float fIndicatorGap, float fIndicatorBottom)
{
	xui_carousel_data_t* pData = __xuiCarouselGetData(pWidget);
	if ( (pData == NULL) || (fArrowSize < 0.0f) || (fIndicatorSize < 0.0f) || (fIndicatorGap < 0.0f) || (fIndicatorBottom < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fArrowSize = (fArrowSize > 0.0f) ? fArrowSize : 36.0f;
	pData->fIndicatorSize = (fIndicatorSize > 0.0f) ? fIndicatorSize : 9.0f;
	pData->fIndicatorGap = fIndicatorGap;
	pData->fIndicatorBottom = fIndicatorBottom;
	return __xuiCarouselInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCarouselGetMetrics(xui_widget pWidget, float* pArrowSize, float* pIndicatorSize, float* pIndicatorGap, float* pIndicatorBottom)
{
	xui_carousel_data_t* pData = __xuiCarouselGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pArrowSize != NULL ) *pArrowSize = pData->fArrowSize;
	if ( pIndicatorSize != NULL ) *pIndicatorSize = pData->fIndicatorSize;
	if ( pIndicatorGap != NULL ) *pIndicatorGap = pData->fIndicatorGap;
	if ( pIndicatorBottom != NULL ) *pIndicatorBottom = pData->fIndicatorBottom;
	return XUI_OK;
}

XUI_API int xuiCarouselSetColors(xui_widget pWidget, uint32_t iBackground, uint32_t iArrow, uint32_t iArrowHover, uint32_t iArrowText, uint32_t iIndicator, uint32_t iIndicatorActive, uint32_t iIndicatorHover)
{
	xui_carousel_data_t* pData = __xuiCarouselGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iBackgroundColor = iBackground;
	pData->iArrowColor = iArrow;
	pData->iArrowHoverColor = iArrowHover;
	pData->iArrowTextColor = iArrowText;
	pData->iIndicatorColor = iIndicator;
	pData->iIndicatorActiveColor = iIndicatorActive;
	pData->iIndicatorHoverColor = iIndicatorHover;
	return __xuiCarouselInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCarouselGetColors(xui_widget pWidget, uint32_t* pBackground, uint32_t* pArrow, uint32_t* pArrowHover, uint32_t* pArrowText, uint32_t* pIndicator, uint32_t* pIndicatorActive, uint32_t* pIndicatorHover)
{
	xui_carousel_data_t* pData = __xuiCarouselGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pBackground != NULL ) *pBackground = pData->iBackgroundColor;
	if ( pArrow != NULL ) *pArrow = pData->iArrowColor;
	if ( pArrowHover != NULL ) *pArrowHover = pData->iArrowHoverColor;
	if ( pArrowText != NULL ) *pArrowText = pData->iArrowTextColor;
	if ( pIndicator != NULL ) *pIndicator = pData->iIndicatorColor;
	if ( pIndicatorActive != NULL ) *pIndicatorActive = pData->iIndicatorActiveColor;
	if ( pIndicatorHover != NULL ) *pIndicatorHover = pData->iIndicatorHoverColor;
	return XUI_OK;
}

XUI_API int xuiCarouselSetFocusColor(xui_widget pWidget, uint32_t iColor)
{
	xui_carousel_data_t* pData = __xuiCarouselGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iFocusColor = iColor;
	return __xuiCarouselInvalidate(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiCarouselGetFocusColor(xui_widget pWidget)
{
	xui_carousel_data_t* pData = __xuiCarouselGetData(pWidget);
	return (pData != NULL) ? pData->iFocusColor : 0u;
}

XUI_API int xuiCarouselGetHoverIndicator(xui_widget pWidget)
{
	xui_carousel_data_t* pData = __xuiCarouselGetData(pWidget);
	return (pData != NULL) ? pData->iHoverIndicator : -1;
}

XUI_API int xuiCarouselGetHoverArrow(xui_widget pWidget)
{
	xui_carousel_data_t* pData = __xuiCarouselGetData(pWidget);
	return (pData != NULL) ? pData->iHoverArrow : XUI_CAROUSEL_ARROW_NONE;
}

XUI_API int xuiCarouselGetChangeCount(xui_widget pWidget)
{
	xui_carousel_data_t* pData = __xuiCarouselGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}
