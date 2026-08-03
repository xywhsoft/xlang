#include "xui_internal.h"

#include <string.h>

#define XUI_RADIO_VISUAL_NORMAL	0
#define XUI_RADIO_VISUAL_HOVER		1
#define XUI_RADIO_VISUAL_ACTIVE	2
#define XUI_RADIO_VISUAL_FOCUS		3
#define XUI_RADIO_VISUAL_DISABLED	4
#define XUI_RADIO_VISUAL_CHECKED	5
#define XUI_RADIO_VISUAL_CHECKED_HOVER	6
#define XUI_RADIO_VISUAL_CHECKED_ACTIVE	7
#define XUI_RADIO_VISUAL_CHECKED_FOCUS	8
#define XUI_RADIO_VISUAL_CHECKED_DISABLED	9
#define XUI_RADIO_VISUAL_COUNT		10

typedef struct xui_radio_visual_t {
	uint32_t iFillColor;
	uint32_t iBorderColor;
	uint32_t iDotColor;
	uint32_t iTextColor;
	float fBorderWidth;
} xui_radio_visual_t;

typedef struct xui_radio_data_t {
	char* sText;
	int iTextCapacity;
	xui_font pFont;
	xui_radio_change_proc onChange;
	void* pChangeUser;
	uint32_t iTextColor;
	uint32_t iDisabledTextColor;
	uint32_t iTextFlags;
	uint32_t iAccentColor;
	uint32_t iAccentHoverColor;
	uint32_t iAccentActiveColor;
	uint32_t iBorderColor;
	uint32_t iBorderHoverColor;
	uint32_t iFocusColor;
	uint32_t iUncheckedFillColor;
	uint32_t iUncheckedHoverFillColor;
	uint32_t iDisabledFillColor;
	uint32_t iDisabledBorderColor;
	uint32_t iDisabledDotColor;
	uint32_t iDotColor;
	float fIndicatorSize;
	float fGap;
	float fFocusWidth;
	float fDotScale;
	int bChecked;
	int bKeyboardActive;
	int iChangeCount;
	int bUseBuiltinAtlas;
	xui_surface pUncheckedSurface;
	xui_surface pCheckedSurface;
	xui_rect_t tUncheckedSrc;
	xui_rect_t tCheckedSrc;
	xui_rect_t tIndicatorRect;
	xui_rect_t tTextRect;
} xui_radio_data_t;

typedef struct xui_radio_group_data_t {
	xui_radio_group_change_proc onChange;
	void* pChangeUser;
	int iOrientation;
	int iSelectedIndex;
	float fGap;
	int bSyncing;
} xui_radio_group_data_t;

static int __xuiRadioSetCheckedInternal(xui_widget pWidget, xui_radio_data_t* pData, int bChecked, int bNotify, int bFromGroup);
static int __xuiRadioGroupSelectRadioInternal(xui_widget pGroup, xui_widget pRadio, int bNotify);
static int __xuiRadioGroupIndexOf(xui_widget pGroup, xui_widget pRadio);

static uint32_t __xuiRadioColorWithAlpha(uint32_t iColor, uint32_t iAlpha)
{
	return (iColor & 0xffffff00u) | (iAlpha & 0xffu);
}

static uint32_t __xuiRadioColorAlpha(uint32_t iColor)
{
	return iColor & 0xffu;
}

static int __xuiRadioDescValid(const xui_radio_desc_t* pDesc)
{
	return (pDesc == NULL) || (pDesc->iSize >= sizeof(*pDesc));
}

static int __xuiRadioGroupDescValid(const xui_radio_group_desc_t* pDesc)
{
	return (pDesc == NULL) || (pDesc->iSize >= sizeof(*pDesc));
}

static int __xuiRadioTextSet(xui_radio_data_t* pData, const char* sText)
{
	char* sNew;
	int iNeed;

	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( sText == NULL ) {
		sText = "";
	}
	iNeed = (int)strlen(sText) + 1;
	if ( iNeed > pData->iTextCapacity ) {
		sNew = (char*)xrtMalloc((size_t)iNeed);
		if ( sNew == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		if ( pData->sText != NULL ) {
			xrtFree(pData->sText);
		}
		pData->sText = sNew;
		pData->iTextCapacity = iNeed;
	}
	memcpy(pData->sText, sText, (size_t)iNeed);
	return XUI_OK;
}

static xui_radio_data_t* __xuiRadioGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( pWidget == NULL ) {
		return NULL;
	}
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "radio");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_radio_data_t*)xuiWidgetGetTypeData(pWidget);
}

static xui_radio_group_data_t* __xuiRadioGroupGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( pWidget == NULL ) {
		return NULL;
	}
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "radiogroup");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_radio_group_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiRadioStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
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

static int __xuiRadioStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
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

static int __xuiRadioStyleInt(xui_widget pWidget, const char* sName, int* pValue)
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

static xui_font __xuiRadioStyleFont(xui_widget pWidget, xui_font pBaseFont)
{
	xui_style_property_t tProperty;
	xui_context pContext;
	xui_font pFont;
	int iRet;

	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, "font.name", &tProperty);
	if ( (iRet == XUI_OK) &&
	     (tProperty.tValue.iType == XUI_STYLE_VALUE_STRING) &&
	     (tProperty.tValue.sText != NULL) ) {
		pContext = xuiWidgetGetContext(pWidget);
		pFont = xuiFindFont(pContext, tProperty.tValue.sText);
		if ( pFont != NULL ) {
			return pFont;
		}
	}
	return pBaseFont;
}

static void __xuiRadioResolve(xui_widget pWidget, xui_radio_data_t* pData, xui_radio_data_t* pResolved)
{
	int iTextFlags;

	*pResolved = *pData;
	pResolved->pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	pResolved->iTextFlags = pData->iTextFlags | XUI_TEXT_CLIP;
	pResolved->fIndicatorSize = (pData->fIndicatorSize > 0.0f) ? pData->fIndicatorSize : 18.0f;
	pResolved->fGap = (pData->fGap >= 0.0f) ? pData->fGap : 8.0f;
	pResolved->fFocusWidth = (pData->fFocusWidth > 0.0f) ? pData->fFocusWidth : 2.0f;
	pResolved->fDotScale = (pData->fDotScale > 0.0f) ? pData->fDotScale : 0.42f;
	(void)__xuiRadioStyleColor(pWidget, "choice.text.color", &pResolved->iTextColor);
	(void)__xuiRadioStyleColor(pWidget, "text.color", &pResolved->iTextColor);
	(void)__xuiRadioStyleColor(pWidget, "choice.text.disabled_color", &pResolved->iDisabledTextColor);
	(void)__xuiRadioStyleColor(pWidget, "text.disabled_color", &pResolved->iDisabledTextColor);
	(void)__xuiRadioStyleColor(pWidget, "choice.accent.color", &pResolved->iAccentColor);
	(void)__xuiRadioStyleColor(pWidget, "choice.accent.hover_color", &pResolved->iAccentHoverColor);
	(void)__xuiRadioStyleColor(pWidget, "choice.accent.active_color", &pResolved->iAccentActiveColor);
	(void)__xuiRadioStyleColor(pWidget, "choice.border.color", &pResolved->iBorderColor);
	(void)__xuiRadioStyleColor(pWidget, "choice.border.hover_color", &pResolved->iBorderHoverColor);
	(void)__xuiRadioStyleColor(pWidget, "choice.focus.color", &pResolved->iFocusColor);
	(void)__xuiRadioStyleFloat(pWidget, "choice.indicator.size", &pResolved->fIndicatorSize);
	(void)__xuiRadioStyleFloat(pWidget, "choice.indicator.gap", &pResolved->fGap);
	(void)__xuiRadioStyleFloat(pWidget, "choice.focus.width", &pResolved->fFocusWidth);
	(void)__xuiRadioStyleFloat(pWidget, "radio.dot.scale", &pResolved->fDotScale);
	iTextFlags = (int)pResolved->iTextFlags;
	if ( __xuiRadioStyleInt(pWidget, "text.flags", &iTextFlags) ) {
		pResolved->iTextFlags = (uint32_t)iTextFlags | XUI_TEXT_CLIP;
	}
	pResolved->pFont = __xuiRadioStyleFont(pWidget, pResolved->pFont);
}

static uint32_t __xuiRadioComputeState(xui_widget pWidget, xui_radio_data_t* pData)
{
	uint32_t iState;

	iState = xuiWidgetGetInputState(pWidget);
	if ( !xuiWidgetGetEnabled(pWidget) ) {
		iState |= XUI_WIDGET_STATE_DISABLED;
	}
	if ( (pData != NULL) && pData->bChecked ) {
		iState |= XUI_RADIO_STATE_CHECKED;
	}
	if ( (pData != NULL) && pData->bKeyboardActive ) {
		iState |= XUI_WIDGET_STATE_ACTIVE;
	}
	return iState;
}

static uint32_t __xuiRadioStateId(uint32_t iState)
{
	if ( ((iState & XUI_WIDGET_STATE_DISABLED) != 0) && ((iState & XUI_RADIO_STATE_CHECKED) != 0) ) return XUI_WIDGET_STATE_DISABLED | XUI_RADIO_STATE_CHECKED;
	if ( (iState & XUI_WIDGET_STATE_DISABLED) != 0 ) return XUI_WIDGET_STATE_DISABLED;
	if ( ((iState & XUI_RADIO_STATE_CHECKED) != 0) && ((iState & XUI_WIDGET_STATE_ACTIVE) != 0) ) return XUI_RADIO_STATE_CHECKED | XUI_WIDGET_STATE_ACTIVE;
	if ( ((iState & XUI_RADIO_STATE_CHECKED) != 0) && ((iState & XUI_WIDGET_STATE_HOVER) != 0) ) return XUI_RADIO_STATE_CHECKED | XUI_WIDGET_STATE_HOVER;
	if ( ((iState & XUI_RADIO_STATE_CHECKED) != 0) && ((iState & XUI_WIDGET_STATE_FOCUS) != 0) ) return XUI_RADIO_STATE_CHECKED | XUI_WIDGET_STATE_FOCUS;
	if ( (iState & XUI_WIDGET_STATE_ACTIVE) != 0 ) return XUI_WIDGET_STATE_ACTIVE;
	if ( (iState & XUI_WIDGET_STATE_HOVER) != 0 ) return XUI_WIDGET_STATE_HOVER;
	if ( (iState & XUI_WIDGET_STATE_FOCUS) != 0 ) return XUI_WIDGET_STATE_FOCUS;
	if ( (iState & XUI_RADIO_STATE_CHECKED) != 0 ) return XUI_RADIO_STATE_CHECKED;
	return 0;
}

static int __xuiRadioSyncState(xui_widget pWidget, xui_radio_data_t* pData)
{
	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return xuiWidgetSetStateId(pWidget, __xuiRadioStateId(__xuiRadioComputeState(pWidget, pData)));
}

static int __xuiRadioStateVisual(uint32_t iStateId)
{
	if ( ((iStateId & XUI_WIDGET_STATE_DISABLED) != 0) && ((iStateId & XUI_RADIO_STATE_CHECKED) != 0) ) return XUI_RADIO_VISUAL_CHECKED_DISABLED;
	if ( (iStateId & XUI_WIDGET_STATE_DISABLED) != 0 ) return XUI_RADIO_VISUAL_DISABLED;
	if ( ((iStateId & XUI_RADIO_STATE_CHECKED) != 0) && ((iStateId & XUI_WIDGET_STATE_ACTIVE) != 0) ) return XUI_RADIO_VISUAL_CHECKED_ACTIVE;
	if ( ((iStateId & XUI_RADIO_STATE_CHECKED) != 0) && ((iStateId & XUI_WIDGET_STATE_HOVER) != 0) ) return XUI_RADIO_VISUAL_CHECKED_HOVER;
	if ( ((iStateId & XUI_RADIO_STATE_CHECKED) != 0) && ((iStateId & XUI_WIDGET_STATE_FOCUS) != 0) ) return XUI_RADIO_VISUAL_CHECKED_FOCUS;
	if ( (iStateId & XUI_WIDGET_STATE_ACTIVE) != 0 ) return XUI_RADIO_VISUAL_ACTIVE;
	if ( (iStateId & XUI_WIDGET_STATE_HOVER) != 0 ) return XUI_RADIO_VISUAL_HOVER;
	if ( (iStateId & XUI_RADIO_STATE_CHECKED) != 0 ) return XUI_RADIO_VISUAL_CHECKED;
	if ( (iStateId & XUI_WIDGET_STATE_FOCUS) != 0 ) return XUI_RADIO_VISUAL_FOCUS;
	return XUI_RADIO_VISUAL_NORMAL;
}

static xui_vec2_t __xuiRadioMeasureText(xui_widget pWidget, xui_font pFont, const char* sText)
{
	xui_proxy pProxy;
	xui_vec2_t tSize;

	memset(&tSize, 0, sizeof(tSize));
	if ( (pFont == NULL) || (sText == NULL) || (sText[0] == '\0') ) {
		return tSize;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( (pProxy != NULL) && (pProxy->textMeasure != NULL) ) {
		(void)pProxy->textMeasure(pProxy, pFont, sText, &tSize);
	}
	return tSize;
}

static void __xuiRadioLayoutContent(xui_widget pWidget, xui_radio_data_t* pData, xui_radio_data_t* pResolved)
{
	xui_rect_t tContent;
	xui_vec2_t tTextSize;
	float fSize;
	float fLineH;

	tContent = xuiWidgetGetContentRect(pWidget);
	fSize = pResolved->fIndicatorSize;
	if ( fSize < 1.0f ) fSize = 1.0f;
	if ( fSize > tContent.fH ) fSize = tContent.fH;
	tTextSize = __xuiRadioMeasureText(pWidget, pResolved->pFont, pData->sText);
	fLineH = (tTextSize.fY > fSize) ? tTextSize.fY : fSize;
	if ( fLineH > tContent.fH ) fLineH = tContent.fH;
	pData->tIndicatorRect = xuiInternalSnapRect((xui_rect_t){tContent.fX, tContent.fY + (tContent.fH - fSize) * 0.5f, fSize, fSize});
	pData->tTextRect = tContent;
	pData->tTextRect.fX += fSize + pResolved->fGap;
	pData->tTextRect.fW -= fSize + pResolved->fGap;
	if ( pData->tTextRect.fW < 0.0f ) pData->tTextRect.fW = 0.0f;
	pData->tTextRect = xuiInternalSnapRect(pData->tTextRect);
}

static int __xuiRadioContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_radio_data_t* pData;
	xui_radio_data_t tResolved;
	xui_vec2_t tTextSize;
	float fSize;

	(void)tConstraint;
	(void)pUser;
	if ( (pWidget == NULL) || (pSize == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiRadioGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiRadioResolve(pWidget, pData, &tResolved);
	fSize = tResolved.fIndicatorSize;
	tTextSize = __xuiRadioMeasureText(pWidget, tResolved.pFont, pData->sText);
	pSize->fX = fSize;
	pSize->fY = fSize;
	if ( (pData->sText != NULL) && (pData->sText[0] != '\0') ) {
		pSize->fX += tResolved.fGap + tTextSize.fX;
		if ( tTextSize.fY > pSize->fY ) pSize->fY = tTextSize.fY;
	}
	if ( pSize->fY < 24.0f ) pSize->fY = 24.0f;
	return XUI_OK;
}

static xui_radio_visual_t __xuiRadioVisual(xui_radio_data_t* pResolved, int iVisual)
{
	xui_radio_visual_t tVisual;

	memset(&tVisual, 0, sizeof(tVisual));
	tVisual.iFillColor = pResolved->iUncheckedFillColor;
	tVisual.iBorderColor = pResolved->iBorderColor;
	tVisual.iDotColor = pResolved->iDotColor;
	tVisual.iTextColor = pResolved->iTextColor;
	tVisual.fBorderWidth = 1.0f;
	switch ( iVisual ) {
	case XUI_RADIO_VISUAL_HOVER:
		tVisual.iFillColor = pResolved->iUncheckedHoverFillColor;
		tVisual.iBorderColor = pResolved->iBorderHoverColor;
		break;
	case XUI_RADIO_VISUAL_ACTIVE:
		tVisual.iFillColor = XUI_COLOR_RGBA(234, 244, 255, 255);
		tVisual.iBorderColor = pResolved->iAccentActiveColor;
		break;
	case XUI_RADIO_VISUAL_CHECKED:
	case XUI_RADIO_VISUAL_CHECKED_FOCUS:
		tVisual.iFillColor = pResolved->iAccentColor;
		tVisual.iBorderColor = pResolved->iAccentColor;
		break;
	case XUI_RADIO_VISUAL_CHECKED_HOVER:
		tVisual.iFillColor = pResolved->iAccentHoverColor;
		tVisual.iBorderColor = pResolved->iAccentHoverColor;
		break;
	case XUI_RADIO_VISUAL_CHECKED_ACTIVE:
		tVisual.iFillColor = pResolved->iAccentActiveColor;
		tVisual.iBorderColor = pResolved->iAccentActiveColor;
		break;
	case XUI_RADIO_VISUAL_DISABLED:
	case XUI_RADIO_VISUAL_CHECKED_DISABLED:
		tVisual.iFillColor = pResolved->iDisabledFillColor;
		tVisual.iBorderColor = pResolved->iDisabledBorderColor;
		tVisual.iDotColor = pResolved->iDisabledDotColor;
		tVisual.iTextColor = pResolved->iDisabledTextColor;
		break;
	default:
		break;
	}
	return tVisual;
}

static int __xuiRadioDrawSurfaceIndicator(xui_widget pWidget, xui_draw_context pDraw, xui_radio_data_t* pData, xui_radio_data_t* pResolved, int bChecked, xui_rect_t tDst)
{
	xui_proxy pProxy;
	xui_surface pSurface;
	xui_rect_t tSrc;
	int iRet;

	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	pSurface = bChecked ? pResolved->pCheckedSurface : pResolved->pUncheckedSurface;
	tSrc = bChecked ? pResolved->tCheckedSrc : pResolved->tUncheckedSrc;
	if ( pSurface == NULL && pResolved->bUseBuiltinAtlas ) {
		iRet = xuiBuiltinAssetGetAtlas(xuiWidgetGetContext(pWidget), &pSurface);
		if ( iRet == XUI_OK ) {
			iRet = xuiBuiltinAssetGetRect(bChecked ? "radio_checked" : "radio_unchecked", &tSrc);
		}
		if ( iRet != XUI_OK ) {
			pSurface = NULL;
		}
	}
	if ( pSurface == NULL ) {
		return XUI_ERROR_FILE_NOT_FOUND;
	}
	return pProxy->drawSurface(pProxy, pDraw, pSurface, tSrc, tDst, XUI_COLOR_WHITE, 0);
}

static int __xuiRadioDrawDefaultIndicator(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, xui_radio_data_t* pResolved, int iVisual, int bChecked)
{
	xui_radio_visual_t tVisual;
	float fCX;
	float fCY;
	float fRadius;
	float fDotRadius;
	int iRet;

	tVisual = __xuiRadioVisual(pResolved, iVisual);
	fCX = tRect.fX + tRect.fW * 0.5f;
	fCY = tRect.fY + tRect.fH * 0.5f;
	fRadius = ((tRect.fW < tRect.fH) ? tRect.fW : tRect.fH) * 0.5f - 0.5f;
	if ( fRadius < 1.0f ) fRadius = 1.0f;
	iRet = XUI_OK;
	if ( __xuiRadioColorAlpha(tVisual.iFillColor) != 0 ) {
		iRet = pProxy->drawCircleFill(pProxy, pDraw, fCX, fCY, fRadius, tVisual.iFillColor);
	}
	if ( (iRet == XUI_OK) && (tVisual.fBorderWidth > 0.0f) && (__xuiRadioColorAlpha(tVisual.iBorderColor) != 0) ) {
		iRet = pProxy->drawCircleStroke(pProxy, pDraw, fCX, fCY, fRadius, tVisual.fBorderWidth, tVisual.iBorderColor);
	}
	if ( (iRet == XUI_OK) && bChecked && (__xuiRadioColorAlpha(tVisual.iDotColor) != 0) ) {
		fDotRadius = fRadius * pResolved->fDotScale;
		if ( fDotRadius < 2.0f ) fDotRadius = 2.0f;
		iRet = pProxy->drawCircleFill(pProxy, pDraw, fCX, fCY, fDotRadius, tVisual.iDotColor);
	}
	return iRet;
}

static int __xuiRadioDrawIndicator(xui_widget pWidget, xui_draw_context pDraw, xui_radio_data_t* pData, xui_radio_data_t* pResolved, int iVisual, int bChecked, xui_rect_t tRect)
{
	xui_proxy pProxy;
	int iRet;

	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( (pData->pCheckedSurface != NULL) || (pData->pUncheckedSurface != NULL) || pResolved->bUseBuiltinAtlas ) {
		iRet = __xuiRadioDrawSurfaceIndicator(pWidget, pDraw, pData, pResolved, bChecked, tRect);
		if ( iRet == XUI_OK ) {
			return XUI_OK;
		}
	}
	return __xuiRadioDrawDefaultIndicator(pProxy, pDraw, tRect, pResolved, iVisual, bChecked);
}

static int __xuiRadioCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_radio_data_t* pData;
	xui_radio_data_t tResolved;
	xui_radio_visual_t tVisual;
	xui_proxy pProxy;
	xui_rect_t tIndicator;
	uint32_t iRenderState;
	int iVisual;
	int bChecked;
	int iRet;

	(void)pUser;
	if ( (pWidget == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiRadioGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	__xuiRadioResolve(pWidget, pData, &tResolved);
	iRenderState = iStateId;
	if ( !xuiWidgetGetEnabled(pWidget) ) {
		iRenderState = XUI_WIDGET_STATE_DISABLED | (pData->bChecked ? XUI_RADIO_STATE_CHECKED : 0);
	}
	iVisual = __xuiRadioStateVisual(iRenderState);
	bChecked = ((iRenderState & XUI_RADIO_STATE_CHECKED) != 0);
	__xuiRadioLayoutContent(pWidget, pData, &tResolved);
	tIndicator = pData->tIndicatorRect;
	iRet = __xuiRadioDrawIndicator(pWidget, pDraw, pData, &tResolved, iVisual, bChecked, tIndicator);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( ((iRenderState & XUI_WIDGET_STATE_FOCUS) != 0) && ((iRenderState & XUI_WIDGET_STATE_DISABLED) == 0) && (tResolved.fFocusWidth > 0.0f) && (__xuiRadioColorAlpha(tResolved.iFocusColor) != 0) ) {
		iRet = pProxy->drawCircleStroke(
			pProxy,
			pDraw,
			tIndicator.fX + tIndicator.fW * 0.5f,
			tIndicator.fY + tIndicator.fH * 0.5f,
			(tIndicator.fW < tIndicator.fH ? tIndicator.fW : tIndicator.fH) * 0.5f + 2.0f,
			tResolved.fFocusWidth,
			__xuiRadioColorWithAlpha(tResolved.iFocusColor, 160));
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	tVisual = __xuiRadioVisual(&tResolved, iVisual);
	if ( (tResolved.pFont != NULL) && (pData->sText != NULL) && (pData->sText[0] != '\0') && (pData->tTextRect.fW > 0.0f) && (__xuiRadioColorAlpha(tVisual.iTextColor) != 0) ) {
		iRet = pProxy->drawText(pProxy, pDraw, tResolved.pFont, pData->sText, pData->tTextRect, tVisual.iTextColor, tResolved.iTextFlags | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
	}
	return iRet;
}

static void __xuiRadioNotify(xui_widget pWidget, xui_radio_data_t* pData)
{
	pData->iChangeCount++;
	if ( pData->onChange != NULL ) {
		pData->onChange(pWidget, pData->bChecked, pData->pChangeUser);
	}
}

static xui_widget __xuiRadioFindGroup(xui_widget pWidget)
{
	xui_widget pParent;

	for ( pParent = xuiWidgetGetParent(pWidget); pParent != NULL; pParent = xuiWidgetGetParent(pParent) ) {
		if ( __xuiRadioGroupGetData(pParent) != NULL ) {
			return pParent;
		}
	}
	return NULL;
}

static int __xuiRadioSetCheckedInternal(xui_widget pWidget, xui_radio_data_t* pData, int bChecked, int bNotify, int bFromGroup)
{
	xui_widget pGroup;
	int iIndex;

	bChecked = (bChecked != 0);
	if ( !bFromGroup ) {
		pGroup = __xuiRadioFindGroup(pWidget);
		if ( bChecked && (pGroup != NULL) ) {
			return __xuiRadioGroupSelectRadioInternal(pGroup, pWidget, bNotify);
		}
		if ( !bChecked && (pGroup != NULL) ) {
			iIndex = __xuiRadioGroupIndexOf(pGroup, pWidget);
			if ( xuiRadioGroupGetSelectedIndex(pGroup) == iIndex ) {
				return __xuiRadioGroupSelectRadioInternal(pGroup, NULL, bNotify);
			}
		}
	}
	if ( pData->bChecked == bChecked ) {
		return XUI_OK;
	}
	pData->bChecked = bChecked;
	(void)__xuiRadioSyncState(pWidget, pData);
	if ( bNotify ) {
		__xuiRadioNotify(pWidget, pData);
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static void __xuiRadioDefaultAction(xui_widget pWidget, void* pUser)
{
	xui_radio_data_t* pData;

	(void)pUser;
	pData = __xuiRadioGetData(pWidget);
	if ( pData == NULL ) {
		return;
	}
	(void)__xuiRadioSetCheckedInternal(pWidget, pData, 1, 1, 0);
}

static int __xuiRadioEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_radio_data_t* pData;
	xui_context pContext;
	int bLeftButton;

	(void)pUser;
	if ( (pWidget == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiRadioGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pContext = xuiWidgetGetContext(pWidget);
	if ( !xuiWidgetGetEnabled(pWidget) || !xuiWidgetGetVisible(pWidget) ) {
		pData->bKeyboardActive = 0;
		(void)__xuiRadioSyncState(pWidget, pData);
		return XUI_OK;
	}
	bLeftButton = (pEvent->iButton == 0) || (pEvent->iButton == XUI_POINTER_BUTTON_LEFT);
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_ENTER:
	case XUI_EVENT_POINTER_LEAVE:
	case XUI_EVENT_POINTER_MOVE:
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
		(void)__xuiRadioSyncState(pWidget, pData);
		return XUI_OK;
	case XUI_EVENT_POINTER_DOWN:
		if ( bLeftButton ) {
			(void)xuiSetPointerCapture(pContext, pWidget);
			(void)__xuiRadioSyncState(pWidget, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_UP:
		if ( bLeftButton ) {
			(void)xuiReleasePointerCapture(pContext, pWidget);
			(void)__xuiRadioSyncState(pWidget, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		pData->bKeyboardActive = 0;
		(void)__xuiRadioSyncState(pWidget, pData);
		return XUI_OK;
	case XUI_EVENT_POINTER_CLICK:
		if ( bLeftButton ) {
			(void)__xuiRadioSetCheckedInternal(pWidget, pData, 1, 1, 0);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_KEY_DOWN:
		if ( pEvent->iKey == XUI_KEY_SPACE ) {
			pData->bKeyboardActive = 1;
			(void)__xuiRadioSyncState(pWidget, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_KEY_UP:
		if ( pEvent->iKey == XUI_KEY_SPACE ) {
			if ( pData->bKeyboardActive ) {
				pData->bKeyboardActive = 0;
				(void)__xuiRadioSetCheckedInternal(pWidget, pData, 1, 1, 0);
			}
			(void)__xuiRadioSyncState(pWidget, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	default:
		break;
	}
	return XUI_OK;
}

static void __xuiRadioDefaultLayout(xui_layout_t* pLayout)
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
	pLayout->tPadding = (xui_thickness_t){4.0f, 3.0f, 4.0f, 3.0f};
}

static void __xuiRadioDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE | XUI_CACHE_UPDATE_ALL_STATES;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiRadioInitCacheStates(xui_widget pWidget)
{
	static const uint32_t arrState[XUI_RADIO_VISUAL_COUNT] = {
		0,
		XUI_WIDGET_STATE_HOVER,
		XUI_WIDGET_STATE_ACTIVE,
		XUI_WIDGET_STATE_FOCUS,
		XUI_WIDGET_STATE_DISABLED,
		XUI_RADIO_STATE_CHECKED,
		XUI_RADIO_STATE_CHECKED | XUI_WIDGET_STATE_HOVER,
		XUI_RADIO_STATE_CHECKED | XUI_WIDGET_STATE_ACTIVE,
		XUI_RADIO_STATE_CHECKED | XUI_WIDGET_STATE_FOCUS,
		XUI_RADIO_STATE_CHECKED | XUI_WIDGET_STATE_DISABLED
	};
	int i;
	int iRet;

	iRet = xuiWidgetSetCacheStateCount(pWidget, XUI_RADIO_VISUAL_COUNT);
	if ( iRet != XUI_OK ) return iRet;
	for ( i = 0; i < XUI_RADIO_VISUAL_COUNT; i++ ) {
		iRet = xuiWidgetSetCacheStateId(pWidget, i, 0x72000000u + (uint32_t)i);
		if ( iRet != XUI_OK ) return iRet;
	}
	for ( i = 0; i < XUI_RADIO_VISUAL_COUNT; i++ ) {
		iRet = xuiWidgetSetCacheStateId(pWidget, i, arrState[i]);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiRadioInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetDefaultAction(pWidget, __xuiRadioDefaultAction, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_ENTER, __xuiRadioEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiRadioEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiRadioEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiRadioEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiRadioEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CLICK, __xuiRadioEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiRadioEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiRadioEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiRadioEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiRadioEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_UP, __xuiRadioEvent, NULL);
	return iRet;
}

static int __xuiRadioInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_radio_data_t* pData;
	const xui_radio_desc_t* pDesc;
	xui_context pContext;
	xui_thickness_t tPadding;
	int iRet;

	(void)pUser;
	pData = (xui_radio_data_t*)pTypeData;
	pDesc = (const xui_radio_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiRadioDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pContext = xuiWidgetGetContext(pWidget);
	pData->pFont = (pDesc != NULL && pDesc->pFont != NULL) ? pDesc->pFont : xuiGetDefaultFont(pContext);
	pData->iTextColor = (pDesc != NULL && pDesc->iTextColor != 0) ? pDesc->iTextColor : XUI_COLOR_RGBA(31, 41, 55, 255);
	pData->iDisabledTextColor = (pDesc != NULL && pDesc->iDisabledTextColor != 0) ? pDesc->iDisabledTextColor : XUI_COLOR_RGBA(156, 163, 175, 255);
	pData->iTextFlags = ((pDesc != NULL && pDesc->iTextFlags != 0) ? pDesc->iTextFlags : (XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE)) | XUI_TEXT_CLIP;
	pData->iAccentColor = XUI_COLOR_RGBA(47, 128, 237, 255);
	pData->iAccentHoverColor = XUI_COLOR_RGBA(64, 146, 255, 255);
	pData->iAccentActiveColor = XUI_COLOR_RGBA(31, 111, 214, 255);
	pData->iBorderColor = XUI_COLOR_RGBA(184, 196, 210, 255);
	pData->iBorderHoverColor = XUI_COLOR_RGBA(127, 181, 243, 255);
	pData->iFocusColor = XUI_COLOR_RGBA(47, 128, 237, 255);
	pData->iUncheckedFillColor = XUI_COLOR_WHITE;
	pData->iUncheckedHoverFillColor = XUI_COLOR_RGBA(247, 251, 255, 255);
	pData->iDisabledFillColor = XUI_COLOR_RGBA(243, 245, 248, 255);
	pData->iDisabledBorderColor = XUI_COLOR_RGBA(214, 221, 230, 255);
	pData->iDisabledDotColor = XUI_COLOR_RGBA(174, 184, 196, 255);
	pData->iDotColor = XUI_COLOR_WHITE;
	pData->fIndicatorSize = (pDesc != NULL && pDesc->fIndicatorSize > 0.0f) ? pDesc->fIndicatorSize : 18.0f;
	pData->fGap = (pDesc != NULL && pDesc->fGap > 0.0f) ? pDesc->fGap : 8.0f;
	pData->fFocusWidth = 2.0f;
	pData->fDotScale = 0.42f;
	pData->bChecked = (pDesc != NULL) ? (pDesc->bChecked != 0) : 0;
	pData->bUseBuiltinAtlas = (pDesc != NULL) ? (pDesc->bUseBuiltinAtlas != 0) : 0;
	iRet = __xuiRadioTextSet(pData, (pDesc != NULL) ? pDesc->sText : "");
	if ( iRet != XUI_OK ) return iRet;
	tPadding = (xui_thickness_t){4.0f, 3.0f, 4.0f, 3.0f};
	(void)xuiWidgetSetPadding(pWidget, tPadding);
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	iRet = __xuiRadioInitCacheStates(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiRadioInitEvents(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiRadioSyncState(pWidget, pData);
}

static void __xuiRadioDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_radio_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_radio_data_t*)pTypeData;
	if ( pData == NULL ) return;
	if ( pData->sText != NULL ) {
		xrtFree(pData->sText);
	}
	memset(pData, 0, sizeof(*pData));
}

static void __xuiRadioRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
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

static void __xuiRadioRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;
	uint32_t iLayoutDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	iLayoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiRadioRegisterStyleProperty(pContext, pType, "choice.text.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiRadioRegisterStyleProperty(pContext, pType, "choice.text.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiRadioRegisterStyleProperty(pContext, pType, "choice.indicator.size", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiRadioRegisterStyleProperty(pContext, pType, "choice.indicator.gap", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiRadioRegisterStyleProperty(pContext, pType, "choice.accent.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiRadioRegisterStyleProperty(pContext, pType, "choice.accent.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiRadioRegisterStyleProperty(pContext, pType, "choice.accent.active_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiRadioRegisterStyleProperty(pContext, pType, "choice.border.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiRadioRegisterStyleProperty(pContext, pType, "choice.border.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiRadioRegisterStyleProperty(pContext, pType, "choice.focus.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiRadioRegisterStyleProperty(pContext, pType, "choice.focus.width", XUI_STYLE_VALUE_FLOAT, iPaintDirty, 0);
	__xuiRadioRegisterStyleProperty(pContext, pType, "radio.dot.scale", XUI_STYLE_VALUE_FLOAT, iPaintDirty, 0);
	__xuiRadioRegisterStyleProperty(pContext, pType, "text.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiRadioRegisterStyleProperty(pContext, pType, "text.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiRadioRegisterStyleProperty(pContext, pType, "text.flags", XUI_STYLE_VALUE_INT, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiRadioRegisterStyleProperty(pContext, pType, "font.name", XUI_STYLE_VALUE_STRING, iLayoutDirty, XUI_STYLE_PROPERTY_INHERITED);
}

XUI_API xui_widget_type xuiRadioGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "radio");
	if ( pType != NULL ) {
		__xuiRadioRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "radio";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_radio_data_t);
	tDesc.onInit = __xuiRadioInit;
	tDesc.onDestroy = __xuiRadioDestroy;
	tDesc.onContentMeasure = __xuiRadioContentMeasure;
	tDesc.onCacheRender = __xuiRadioCacheRender;
	__xuiRadioDefaultLayout(&tDesc.tLayout);
	__xuiRadioDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	__xuiRadioRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiRadioCreate(xui_context pContext, xui_widget* ppWidget, const xui_radio_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiRadioDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiRadioGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiRadioSetChange(xui_widget pWidget, xui_radio_change_proc onChange, void* pUser)
{
	xui_radio_data_t* pData = __xuiRadioGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onChange = onChange;
	pData->pChangeUser = pUser;
	return XUI_OK;
}

XUI_API int xuiRadioSetText(xui_widget pWidget, const char* sText)
{
	xui_radio_data_t* pData;
	int iRet;

	pData = __xuiRadioGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiRadioTextSet(pData, sText);
	if ( iRet != XUI_OK ) return iRet;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API const char* xuiRadioGetText(xui_widget pWidget)
{
	xui_radio_data_t* pData = __xuiRadioGetData(pWidget);
	return (pData != NULL && pData->sText != NULL) ? pData->sText : "";
}

XUI_API int xuiRadioSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_radio_data_t* pData = __xuiRadioGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pFont = pFont;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_font xuiRadioGetFont(xui_widget pWidget)
{
	xui_radio_data_t* pData = __xuiRadioGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API int xuiRadioSetTextFlags(xui_widget pWidget, uint32_t iTextFlags)
{
	xui_radio_data_t* pData = __xuiRadioGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iTextFlags = iTextFlags | XUI_TEXT_CLIP;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiRadioGetTextFlags(xui_widget pWidget)
{
	xui_radio_data_t* pData = __xuiRadioGetData(pWidget);
	return (pData != NULL) ? pData->iTextFlags : 0;
}

XUI_API int xuiRadioSetChecked(xui_widget pWidget, int bChecked)
{
	xui_radio_data_t* pData = __xuiRadioGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiRadioSetCheckedInternal(pWidget, pData, bChecked, 0, 0);
}

XUI_API int xuiRadioGetChecked(xui_widget pWidget)
{
	xui_radio_data_t* pData = __xuiRadioGetData(pWidget);
	return (pData != NULL) ? pData->bChecked : 0;
}

XUI_API int xuiRadioSetTextColor(xui_widget pWidget, uint32_t iColor)
{
	xui_radio_data_t* pData = __xuiRadioGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iTextColor = iColor;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiRadioGetTextColor(xui_widget pWidget)
{
	xui_radio_data_t* pData = __xuiRadioGetData(pWidget);
	return (pData != NULL) ? pData->iTextColor : 0;
}

XUI_API int xuiRadioSetDisabledTextColor(xui_widget pWidget, uint32_t iColor)
{
	xui_radio_data_t* pData = __xuiRadioGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iDisabledTextColor = iColor;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiRadioGetDisabledTextColor(xui_widget pWidget)
{
	xui_radio_data_t* pData = __xuiRadioGetData(pWidget);
	return (pData != NULL) ? pData->iDisabledTextColor : 0;
}

XUI_API int xuiRadioSetIndicatorSize(xui_widget pWidget, float fSize)
{
	xui_radio_data_t* pData = __xuiRadioGetData(pWidget);
	if ( (pData == NULL) || (fSize <= 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fIndicatorSize = fSize;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiRadioGetIndicatorSize(xui_widget pWidget)
{
	xui_radio_data_t* pData = __xuiRadioGetData(pWidget);
	return (pData != NULL) ? pData->fIndicatorSize : 0.0f;
}

XUI_API int xuiRadioSetGap(xui_widget pWidget, float fGap)
{
	xui_radio_data_t* pData = __xuiRadioGetData(pWidget);
	if ( (pData == NULL) || (fGap < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fGap = fGap;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiRadioGetGap(xui_widget pWidget)
{
	xui_radio_data_t* pData = __xuiRadioGetData(pWidget);
	return (pData != NULL) ? pData->fGap : 0.0f;
}

XUI_API int xuiRadioSetColors(xui_widget pWidget, uint32_t iAccent, uint32_t iBorder, uint32_t iHoverBorder, uint32_t iFocus)
{
	xui_radio_data_t* pData = __xuiRadioGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iAccentColor = iAccent;
	pData->iAccentHoverColor = iAccent;
	pData->iAccentActiveColor = iAccent;
	pData->iBorderColor = iBorder;
	pData->iBorderHoverColor = iHoverBorder;
	pData->iFocusColor = iFocus;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiRadioGetColors(xui_widget pWidget, uint32_t* pAccent, uint32_t* pBorder, uint32_t* pHoverBorder, uint32_t* pFocus)
{
	xui_radio_data_t* pData = __xuiRadioGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pAccent != NULL ) *pAccent = pData->iAccentColor;
	if ( pBorder != NULL ) *pBorder = pData->iBorderColor;
	if ( pHoverBorder != NULL ) *pHoverBorder = pData->iBorderHoverColor;
	if ( pFocus != NULL ) *pFocus = pData->iFocusColor;
	return XUI_OK;
}

XUI_API int xuiRadioSetIndicatorSurface(xui_widget pWidget, xui_surface pUncheckedSurface, xui_rect_t tUncheckedSrc, xui_surface pCheckedSurface, xui_rect_t tCheckedSrc)
{
	xui_radio_data_t* pData = __xuiRadioGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pUncheckedSurface = pUncheckedSurface;
	pData->tUncheckedSrc = tUncheckedSrc;
	pData->pCheckedSurface = pCheckedSurface;
	pData->tCheckedSrc = tCheckedSrc;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiRadioGetIndicatorSurface(xui_widget pWidget, xui_surface* ppUncheckedSurface, xui_rect_t* pUncheckedSrc, xui_surface* ppCheckedSurface, xui_rect_t* pCheckedSrc)
{
	xui_radio_data_t* pData = __xuiRadioGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( ppUncheckedSurface != NULL ) *ppUncheckedSurface = pData->pUncheckedSurface;
	if ( pUncheckedSrc != NULL ) *pUncheckedSrc = pData->tUncheckedSrc;
	if ( ppCheckedSurface != NULL ) *ppCheckedSurface = pData->pCheckedSurface;
	if ( pCheckedSrc != NULL ) *pCheckedSrc = pData->tCheckedSrc;
	return XUI_OK;
}

XUI_API int xuiRadioUseBuiltinAtlas(xui_widget pWidget, int bEnable)
{
	xui_radio_data_t* pData = __xuiRadioGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bUseBuiltinAtlas = (bEnable != 0);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiRadioGetUseBuiltinAtlas(xui_widget pWidget)
{
	xui_radio_data_t* pData = __xuiRadioGetData(pWidget);
	return (pData != NULL) ? pData->bUseBuiltinAtlas : 0;
}

XUI_API xui_rect_t xuiRadioGetIndicatorRect(xui_widget pWidget)
{
	xui_radio_data_t* pData = __xuiRadioGetData(pWidget);
	return (pData != NULL) ? pData->tIndicatorRect : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API xui_rect_t xuiRadioGetTextRect(xui_widget pWidget)
{
	xui_radio_data_t* pData = __xuiRadioGetData(pWidget);
	return (pData != NULL) ? pData->tTextRect : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API uint32_t xuiRadioGetState(xui_widget pWidget)
{
	xui_radio_data_t* pData = __xuiRadioGetData(pWidget);
	return (pData != NULL) ? __xuiRadioComputeState(pWidget, pData) : 0;
}

static int __xuiRadioGroupIsOption(xui_widget pWidget)
{
	xui_widget_type pCheckCardType;

	if ( pWidget == NULL ) return 0;
	if ( __xuiRadioGetData(pWidget) != NULL ) return 1;
	pCheckCardType = xuiCheckCardGetType(xuiWidgetGetContext(pWidget));
	return (pCheckCardType != NULL) && xuiWidgetIsType(pWidget, pCheckCardType);
}

static int __xuiRadioGroupSetOptionChecked(xui_widget pOption, int bChecked, int bNotify)
{
	xui_radio_data_t* pRadioData;

	pRadioData = __xuiRadioGetData(pOption);
	if ( pRadioData != NULL ) return __xuiRadioSetCheckedInternal(pOption, pRadioData, bChecked, bNotify, 1);
	return xuiCheckCardSetCheckedFromGroup(pOption, bChecked, bNotify);
}

static int __xuiRadioGroupIndexOf(xui_widget pGroup, xui_widget pRadio)
{
	xui_widget pChild;
	int iIndex;

	iIndex = 0;
	for ( pChild = xuiWidgetGetFirstChild(pGroup); pChild != NULL; pChild = xuiWidgetGetNextSibling(pChild) ) {
		if ( !__xuiRadioGroupIsOption(pChild) ) {
			continue;
		}
		if ( pChild == pRadio ) {
			return iIndex;
		}
		iIndex++;
	}
	return -1;
}

static xui_widget __xuiRadioGroupRadioAt(xui_widget pGroup, int iIndex)
{
	xui_widget pChild;
	int i;

	i = 0;
	for ( pChild = xuiWidgetGetFirstChild(pGroup); pChild != NULL; pChild = xuiWidgetGetNextSibling(pChild) ) {
		if ( !__xuiRadioGroupIsOption(pChild) ) {
			continue;
		}
		if ( i == iIndex ) {
			return pChild;
		}
		i++;
	}
	return NULL;
}

static int __xuiRadioGroupNotify(xui_widget pGroup, xui_radio_group_data_t* pData)
{
	xui_widget pSelected;

	if ( (pGroup == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->onChange != NULL ) {
		pSelected = __xuiRadioGroupRadioAt(pGroup, pData->iSelectedIndex);
		pData->onChange(pGroup, pSelected, pData->iSelectedIndex, pData->pChangeUser);
	}
	return XUI_OK;
}

static int __xuiRadioGroupSelectRadioInternal(xui_widget pGroup, xui_widget pRadio, int bNotify)
{
	xui_radio_group_data_t* pGroupData;
	xui_widget pChild;
	int iIndex;
	int iNewIndex;
	int iOldIndex;
	int iRet;

	pGroupData = __xuiRadioGroupGetData(pGroup);
	if ( pGroupData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pRadio != NULL) && (__xuiRadioGroupIndexOf(pGroup, pRadio) < 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iOldIndex = pGroupData->iSelectedIndex;
	iNewIndex = -1;
	pGroupData->bSyncing = 1;
	iIndex = 0;
	for ( pChild = xuiWidgetGetFirstChild(pGroup); pChild != NULL; pChild = xuiWidgetGetNextSibling(pChild) ) {
		if ( !__xuiRadioGroupIsOption(pChild) ) {
			continue;
		}
		if ( pChild == pRadio ) {
			iNewIndex = iIndex;
			iRet = __xuiRadioGroupSetOptionChecked(pChild, 1, bNotify);
		} else {
			iRet = __xuiRadioGroupSetOptionChecked(pChild, 0, bNotify);
		}
		if ( iRet != XUI_OK ) {
			pGroupData->bSyncing = 0;
			return iRet;
		}
		iIndex++;
	}
	pGroupData->bSyncing = 0;
	pGroupData->iSelectedIndex = iNewIndex;
	if ( bNotify && (iOldIndex != iNewIndex) ) {
		(void)__xuiRadioGroupNotify(pGroup, pGroupData);
	}
	return XUI_OK;
}

static void __xuiRadioGroupApplyLayout(xui_widget pGroup, xui_radio_group_data_t* pData)
{
	if ( (pGroup == NULL) || (pData == NULL) ) {
		return;
	}
	(void)xuiWidgetSetLayoutType(pGroup, (pData->iOrientation == XUI_RADIO_GROUP_HORIZONTAL) ? XUI_LAYOUT_ROW : XUI_LAYOUT_COLUMN);
	(void)xuiWidgetSetGap(pGroup, pData->fGap);
}

static void __xuiRadioGroupDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_COLUMN;
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
	pLayout->fGap = 6.0f;
}

static int __xuiRadioGroupInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_radio_group_data_t* pData;
	const xui_radio_group_desc_t* pDesc;

	(void)pUser;
	pData = (xui_radio_group_data_t*)pTypeData;
	pDesc = (const xui_radio_group_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiRadioGroupDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->iOrientation = (pDesc != NULL && pDesc->iOrientation == XUI_RADIO_GROUP_HORIZONTAL) ? XUI_RADIO_GROUP_HORIZONTAL : XUI_RADIO_GROUP_VERTICAL;
	pData->iSelectedIndex = (pDesc != NULL) ? pDesc->iSelectedIndex : -1;
	pData->fGap = (pDesc != NULL && pDesc->fGap >= 0.0f) ? pDesc->fGap : 6.0f;
	(void)xuiWidgetSetSizeMode(pWidget, XUI_SIZE_CONTENT, XUI_SIZE_CONTENT);
	__xuiRadioGroupApplyLayout(pWidget, pData);
	return XUI_OK;
}

static void __xuiRadioGroupDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	(void)pWidget;
	(void)pUser;
	if ( pTypeData != NULL ) {
		memset(pTypeData, 0, sizeof(xui_radio_group_data_t));
	}
}

XUI_API xui_widget_type xuiRadioGroupGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "radiogroup");
	if ( pType != NULL ) {
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "radiogroup";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT;
	tDesc.iTypeDataSize = sizeof(xui_radio_group_data_t);
	tDesc.onInit = __xuiRadioGroupInit;
	tDesc.onDestroy = __xuiRadioGroupDestroy;
	__xuiRadioGroupDefaultLayout(&tDesc.tLayout);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	return pType;
}

XUI_API int xuiRadioGroupCreate(xui_context pContext, xui_widget* ppWidget, const xui_radio_group_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiRadioGroupDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiRadioGroupGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	(void)xuiRadioGetType(pContext);
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiRadioGroupSetChange(xui_widget pGroup, xui_radio_group_change_proc onChange, void* pUser)
{
	xui_radio_group_data_t* pData = __xuiRadioGroupGetData(pGroup);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onChange = onChange;
	pData->pChangeUser = pUser;
	return XUI_OK;
}

XUI_API int xuiRadioGroupAddRadio(xui_widget pGroup, xui_widget pRadio)
{
	xui_radio_group_data_t* pGroupData;
	xui_radio_data_t* pRadioData;
	int iIndex;
	int iRet;

	pGroupData = __xuiRadioGroupGetData(pGroup);
	pRadioData = __xuiRadioGetData(pRadio);
	if ( (pGroupData == NULL) || (pRadioData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iIndex = __xuiRadioGroupIndexOf(pGroup, pRadio);
	if ( iIndex < 0 ) {
		iIndex = 0;
		if ( xuiWidgetGetParent(pRadio) != pGroup ) {
			iRet = xuiWidgetAddChild(pGroup, pRadio);
			if ( iRet != XUI_OK ) return iRet;
		}
		iIndex = __xuiRadioGroupIndexOf(pGroup, pRadio);
	}
	if ( (pRadioData->bChecked && pGroupData->iSelectedIndex != iIndex) ||
	     (pGroupData->iSelectedIndex == iIndex) ) {
		return __xuiRadioGroupSelectRadioInternal(pGroup, pRadio, 0);
	}
	if ( pGroupData->iSelectedIndex >= 0 ) {
		pRadio = __xuiRadioGroupRadioAt(pGroup, pGroupData->iSelectedIndex);
		if ( pRadio != NULL ) {
			return __xuiRadioGroupSelectRadioInternal(pGroup, pRadio, 0);
		}
	}
	return XUI_OK;
}

XUI_API int xuiRadioGroupAddCheckCard(xui_widget pGroup, xui_widget pCard)
{
	xui_radio_group_data_t* pGroupData;
	int iIndex;
	int iRet;

	pGroupData = __xuiRadioGroupGetData(pGroup);
	if ( (pGroupData == NULL) || !__xuiRadioGroupIsOption(pCard) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iIndex = __xuiRadioGroupIndexOf(pGroup, pCard);
	if ( iIndex < 0 ) {
		iRet = xuiCheckCardSetRadioGroup(pCard, pGroup);
		if ( iRet != XUI_OK ) return iRet;
		iIndex = __xuiRadioGroupIndexOf(pGroup, pCard);
	}
	if ( xuiCheckCardGetChecked(pCard) || pGroupData->iSelectedIndex == iIndex ) {
		return __xuiRadioGroupSelectRadioInternal(pGroup, pCard, 0);
	}
	if ( pGroupData->iSelectedIndex >= 0 ) {
		pCard = __xuiRadioGroupRadioAt(pGroup, pGroupData->iSelectedIndex);
		if ( pCard != NULL ) return __xuiRadioGroupSelectRadioInternal(pGroup, pCard, 0);
	}
	return XUI_OK;
}

XUI_API int xuiRadioGroupAddOption(xui_widget pGroup, xui_widget* ppRadio, const xui_radio_desc_t* pDesc)
{
	xui_context pContext;
	xui_widget pRadio;
	int iRet;

	if ( (ppRadio == NULL) || (__xuiRadioGroupGetData(pGroup) == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppRadio = NULL;
	pContext = xuiWidgetGetContext(pGroup);
	iRet = xuiRadioCreate(pContext, &pRadio, pDesc);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiRadioGroupAddRadio(pGroup, pRadio);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pRadio);
		return iRet;
	}
	*ppRadio = pRadio;
	return XUI_OK;
}

XUI_API int xuiRadioGroupSetSelectedIndex(xui_widget pGroup, int iIndex)
{
	xui_radio_group_data_t* pData;
	xui_widget pRadio;

	pData = __xuiRadioGroupGetData(pGroup);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( iIndex < 0 ) {
		return __xuiRadioGroupSelectRadioInternal(pGroup, NULL, 1);
	}
	pRadio = __xuiRadioGroupRadioAt(pGroup, iIndex);
	if ( pRadio == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return __xuiRadioGroupSelectRadioInternal(pGroup, pRadio, 1);
}

XUI_API int xuiRadioGroupGetSelectedIndex(xui_widget pGroup)
{
	xui_radio_group_data_t* pData = __xuiRadioGroupGetData(pGroup);
	return (pData != NULL) ? pData->iSelectedIndex : -1;
}

XUI_API xui_widget xuiRadioGroupGetSelectedRadio(xui_widget pGroup)
{
	xui_radio_group_data_t* pData = __xuiRadioGroupGetData(pGroup);
	xui_widget pSelected;

	if ( pData == NULL ) return NULL;
	pSelected = __xuiRadioGroupRadioAt(pGroup, pData->iSelectedIndex);
	return (__xuiRadioGetData(pSelected) != NULL) ? pSelected : NULL;
}

XUI_API int xuiRadioGroupSetSelectedWidget(xui_widget pGroup, xui_widget pOption)
{
	xui_radio_group_data_t* pData;

	pData = __xuiRadioGroupGetData(pGroup);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pOption == NULL ) return __xuiRadioGroupSelectRadioInternal(pGroup, NULL, 1);
	if ( __xuiRadioGroupIndexOf(pGroup, pOption) < 0 ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiRadioGroupSelectRadioInternal(pGroup, pOption, 1);
}

XUI_API xui_widget xuiRadioGroupGetSelectedWidget(xui_widget pGroup)
{
	xui_radio_group_data_t* pData = __xuiRadioGroupGetData(pGroup);
	return (pData != NULL) ? __xuiRadioGroupRadioAt(pGroup, pData->iSelectedIndex) : NULL;
}

XUI_API int xuiRadioGroupSetOrientation(xui_widget pGroup, int iOrientation)
{
	xui_radio_group_data_t* pData = __xuiRadioGroupGetData(pGroup);
	if ( (pData == NULL) || ((iOrientation != XUI_RADIO_GROUP_VERTICAL) && (iOrientation != XUI_RADIO_GROUP_HORIZONTAL)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->iOrientation = iOrientation;
	__xuiRadioGroupApplyLayout(pGroup, pData);
	return xuiWidgetInvalidate(pGroup, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiRadioGroupGetOrientation(xui_widget pGroup)
{
	xui_radio_group_data_t* pData = __xuiRadioGroupGetData(pGroup);
	return (pData != NULL) ? pData->iOrientation : XUI_RADIO_GROUP_VERTICAL;
}

XUI_API int xuiRadioGroupSetGap(xui_widget pGroup, float fGap)
{
	xui_radio_group_data_t* pData = __xuiRadioGroupGetData(pGroup);
	if ( (pData == NULL) || (fGap < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fGap = fGap;
	__xuiRadioGroupApplyLayout(pGroup, pData);
	return xuiWidgetInvalidate(pGroup, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiRadioGroupGetGap(xui_widget pGroup)
{
	xui_radio_group_data_t* pData = __xuiRadioGroupGetData(pGroup);
	return (pData != NULL) ? pData->fGap : 0.0f;
}
