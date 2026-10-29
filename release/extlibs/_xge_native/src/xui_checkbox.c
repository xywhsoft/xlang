#include "xui_internal.h"

#include <string.h>

#define XUI_CHECKBOX_VISUAL_NORMAL	0
#define XUI_CHECKBOX_VISUAL_HOVER	1
#define XUI_CHECKBOX_VISUAL_ACTIVE	2
#define XUI_CHECKBOX_VISUAL_FOCUS	3
#define XUI_CHECKBOX_VISUAL_DISABLED	4
#define XUI_CHECKBOX_VISUAL_CHECKED	5
#define XUI_CHECKBOX_VISUAL_CHECKED_HOVER	6
#define XUI_CHECKBOX_VISUAL_CHECKED_ACTIVE	7
#define XUI_CHECKBOX_VISUAL_CHECKED_FOCUS	8
#define XUI_CHECKBOX_VISUAL_CHECKED_DISABLED	9
#define XUI_CHECKBOX_VISUAL_COUNT	10

typedef struct xui_checkbox_visual_t {
	uint32_t iFillColor;
	uint32_t iBorderColor;
	uint32_t iCheckColor;
	uint32_t iTextColor;
	float fBorderWidth;
} xui_checkbox_visual_t;

typedef struct xui_checkbox_data_t {
	char* sText;
	int iTextCapacity;
	xui_font pFont;
	xui_checkbox_change_proc onChange;
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
	uint32_t iDisabledCheckColor;
	uint32_t iCheckColor;
	float fIndicatorSize;
	float fGap;
	float fFocusWidth;
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
} xui_checkbox_data_t;

static uint32_t __xuiCheckBoxColorAlpha(uint32_t iColor)
{
	return iColor & 0x000000ffu;
}

static uint32_t __xuiCheckBoxColorWithAlpha(uint32_t iColor, uint32_t iAlpha)
{
	return (iColor & 0xffffff00u) | (iAlpha & 0xffu);
}

static int __xuiCheckBoxDescValid(const xui_checkbox_desc_t* pDesc)
{
	return (pDesc == NULL) || (pDesc->iSize >= sizeof(*pDesc));
}

static int __xuiCheckBoxTextSet(xui_checkbox_data_t* pData, const char* sText)
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

static xui_checkbox_data_t* __xuiCheckBoxGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( pWidget == NULL ) {
		return NULL;
	}
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "checkbox");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_checkbox_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiCheckBoxStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
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

static int __xuiCheckBoxStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
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

static int __xuiCheckBoxStyleInt(xui_widget pWidget, const char* sName, int* pValue)
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

static xui_font __xuiCheckBoxStyleFont(xui_widget pWidget, xui_font pBaseFont)
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

static void __xuiCheckBoxResolve(xui_widget pWidget, xui_checkbox_data_t* pData, xui_checkbox_data_t* pResolved)
{
	int iTextFlags;

	*pResolved = *pData;
	pResolved->pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	pResolved->iTextFlags = pData->iTextFlags | XUI_TEXT_CLIP;
	pResolved->fIndicatorSize = (pData->fIndicatorSize > 0.0f) ? pData->fIndicatorSize : 18.0f;
	pResolved->fGap = (pData->fGap >= 0.0f) ? pData->fGap : 8.0f;
	pResolved->fFocusWidth = (pData->fFocusWidth > 0.0f) ? pData->fFocusWidth : 2.0f;
	(void)__xuiCheckBoxStyleColor(pWidget, "choice.text.color", &pResolved->iTextColor);
	(void)__xuiCheckBoxStyleColor(pWidget, "text.color", &pResolved->iTextColor);
	(void)__xuiCheckBoxStyleColor(pWidget, "choice.text.disabled_color", &pResolved->iDisabledTextColor);
	(void)__xuiCheckBoxStyleColor(pWidget, "text.disabled_color", &pResolved->iDisabledTextColor);
	(void)__xuiCheckBoxStyleColor(pWidget, "choice.accent.color", &pResolved->iAccentColor);
	(void)__xuiCheckBoxStyleColor(pWidget, "choice.accent.hover_color", &pResolved->iAccentHoverColor);
	(void)__xuiCheckBoxStyleColor(pWidget, "choice.accent.active_color", &pResolved->iAccentActiveColor);
	(void)__xuiCheckBoxStyleColor(pWidget, "choice.border.color", &pResolved->iBorderColor);
	(void)__xuiCheckBoxStyleColor(pWidget, "choice.border.hover_color", &pResolved->iBorderHoverColor);
	(void)__xuiCheckBoxStyleColor(pWidget, "choice.focus.color", &pResolved->iFocusColor);
	(void)__xuiCheckBoxStyleFloat(pWidget, "choice.indicator.size", &pResolved->fIndicatorSize);
	(void)__xuiCheckBoxStyleFloat(pWidget, "choice.indicator.gap", &pResolved->fGap);
	(void)__xuiCheckBoxStyleFloat(pWidget, "choice.focus.width", &pResolved->fFocusWidth);
	iTextFlags = (int)pResolved->iTextFlags;
	if ( __xuiCheckBoxStyleInt(pWidget, "text.flags", &iTextFlags) ) {
		pResolved->iTextFlags = (uint32_t)iTextFlags | XUI_TEXT_CLIP;
	}
	pResolved->pFont = __xuiCheckBoxStyleFont(pWidget, pResolved->pFont);
}

static uint32_t __xuiCheckBoxComputeState(xui_widget pWidget, xui_checkbox_data_t* pData)
{
	uint32_t iState;

	iState = xuiWidgetGetInputState(pWidget);
	if ( !xuiWidgetGetEnabled(pWidget) ) {
		iState |= XUI_WIDGET_STATE_DISABLED;
	}
	if ( (pData != NULL) && pData->bChecked ) {
		iState |= XUI_CHECKBOX_STATE_CHECKED;
	}
	if ( (pData != NULL) && pData->bKeyboardActive ) {
		iState |= XUI_WIDGET_STATE_ACTIVE;
	}
	return iState;
}

static uint32_t __xuiCheckBoxStateId(uint32_t iState)
{
	if ( ((iState & XUI_WIDGET_STATE_DISABLED) != 0) && ((iState & XUI_CHECKBOX_STATE_CHECKED) != 0) ) return XUI_WIDGET_STATE_DISABLED | XUI_CHECKBOX_STATE_CHECKED;
	if ( (iState & XUI_WIDGET_STATE_DISABLED) != 0 ) return XUI_WIDGET_STATE_DISABLED;
	if ( ((iState & XUI_CHECKBOX_STATE_CHECKED) != 0) && ((iState & XUI_WIDGET_STATE_ACTIVE) != 0) ) return XUI_CHECKBOX_STATE_CHECKED | XUI_WIDGET_STATE_ACTIVE;
	if ( ((iState & XUI_CHECKBOX_STATE_CHECKED) != 0) && ((iState & XUI_WIDGET_STATE_HOVER) != 0) ) return XUI_CHECKBOX_STATE_CHECKED | XUI_WIDGET_STATE_HOVER;
	if ( ((iState & XUI_CHECKBOX_STATE_CHECKED) != 0) && ((iState & XUI_WIDGET_STATE_FOCUS) != 0) ) return XUI_CHECKBOX_STATE_CHECKED | XUI_WIDGET_STATE_FOCUS;
	if ( (iState & XUI_WIDGET_STATE_ACTIVE) != 0 ) return XUI_WIDGET_STATE_ACTIVE;
	if ( (iState & XUI_WIDGET_STATE_HOVER) != 0 ) return XUI_WIDGET_STATE_HOVER;
	if ( (iState & XUI_WIDGET_STATE_FOCUS) != 0 ) return XUI_WIDGET_STATE_FOCUS;
	if ( (iState & XUI_CHECKBOX_STATE_CHECKED) != 0 ) return XUI_CHECKBOX_STATE_CHECKED;
	return 0;
}

static int __xuiCheckBoxSyncState(xui_widget pWidget, xui_checkbox_data_t* pData)
{
	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return xuiWidgetSetStateId(pWidget, __xuiCheckBoxStateId(__xuiCheckBoxComputeState(pWidget, pData)));
}

static int __xuiCheckBoxStateVisual(uint32_t iStateId)
{
	if ( ((iStateId & XUI_WIDGET_STATE_DISABLED) != 0) && ((iStateId & XUI_CHECKBOX_STATE_CHECKED) != 0) ) return XUI_CHECKBOX_VISUAL_CHECKED_DISABLED;
	if ( (iStateId & XUI_WIDGET_STATE_DISABLED) != 0 ) return XUI_CHECKBOX_VISUAL_DISABLED;
	if ( ((iStateId & XUI_CHECKBOX_STATE_CHECKED) != 0) && ((iStateId & XUI_WIDGET_STATE_ACTIVE) != 0) ) return XUI_CHECKBOX_VISUAL_CHECKED_ACTIVE;
	if ( ((iStateId & XUI_CHECKBOX_STATE_CHECKED) != 0) && ((iStateId & XUI_WIDGET_STATE_HOVER) != 0) ) return XUI_CHECKBOX_VISUAL_CHECKED_HOVER;
	if ( ((iStateId & XUI_CHECKBOX_STATE_CHECKED) != 0) && ((iStateId & XUI_WIDGET_STATE_FOCUS) != 0) ) return XUI_CHECKBOX_VISUAL_CHECKED_FOCUS;
	if ( (iStateId & XUI_WIDGET_STATE_ACTIVE) != 0 ) return XUI_CHECKBOX_VISUAL_ACTIVE;
	if ( (iStateId & XUI_WIDGET_STATE_HOVER) != 0 ) return XUI_CHECKBOX_VISUAL_HOVER;
	if ( (iStateId & XUI_CHECKBOX_STATE_CHECKED) != 0 ) return XUI_CHECKBOX_VISUAL_CHECKED;
	if ( (iStateId & XUI_WIDGET_STATE_FOCUS) != 0 ) return XUI_CHECKBOX_VISUAL_FOCUS;
	return XUI_CHECKBOX_VISUAL_NORMAL;
}

static xui_vec2_t __xuiCheckBoxMeasureText(xui_widget pWidget, xui_font pFont, const char* sText)
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

static void __xuiCheckBoxLayoutContent(xui_widget pWidget, xui_checkbox_data_t* pData, xui_checkbox_data_t* pResolved)
{
	xui_rect_t tContent;
	xui_vec2_t tTextSize;
	float fSize;
	float fLineH;

	tContent = xuiWidgetGetContentRect(pWidget);
	fSize = pResolved->fIndicatorSize;
	if ( fSize < 1.0f ) {
		fSize = 1.0f;
	}
	if ( fSize > tContent.fH ) {
		fSize = tContent.fH;
	}
	tTextSize = __xuiCheckBoxMeasureText(pWidget, pResolved->pFont, pData->sText);
	fLineH = (tTextSize.fY > fSize) ? tTextSize.fY : fSize;
	if ( fLineH > tContent.fH ) {
		fLineH = tContent.fH;
	}
	pData->tIndicatorRect = xuiInternalSnapRect((xui_rect_t){tContent.fX, tContent.fY + (tContent.fH - fSize) * 0.5f, fSize, fSize});
	pData->tTextRect = tContent;
	pData->tTextRect.fX += fSize + pResolved->fGap;
	pData->tTextRect.fW -= fSize + pResolved->fGap;
	if ( pData->tTextRect.fW < 0.0f ) {
		pData->tTextRect.fW = 0.0f;
	}
	pData->tTextRect = xuiInternalSnapRect(pData->tTextRect);
}

static int __xuiCheckBoxContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_checkbox_data_t* pData;
	xui_checkbox_data_t tResolved;
	xui_vec2_t tTextSize;
	float fSize;

	(void)tConstraint;
	(void)pUser;
	if ( (pWidget == NULL) || (pSize == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiCheckBoxGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiCheckBoxResolve(pWidget, pData, &tResolved);
	fSize = tResolved.fIndicatorSize;
	tTextSize = __xuiCheckBoxMeasureText(pWidget, tResolved.pFont, pData->sText);
	pSize->fX = fSize;
	pSize->fY = fSize;
	if ( (pData->sText != NULL) && (pData->sText[0] != '\0') ) {
		pSize->fX += tResolved.fGap + tTextSize.fX;
		if ( tTextSize.fY > pSize->fY ) {
			pSize->fY = tTextSize.fY;
		}
	}
	if ( pSize->fY < 24.0f ) {
		pSize->fY = 24.0f;
	}
	return XUI_OK;
}

static xui_checkbox_visual_t __xuiCheckBoxVisual(xui_checkbox_data_t* pResolved, int iVisual)
{
	xui_checkbox_visual_t tVisual;

	memset(&tVisual, 0, sizeof(tVisual));
	tVisual.iFillColor = pResolved->iUncheckedFillColor;
	tVisual.iBorderColor = pResolved->iBorderColor;
	tVisual.iCheckColor = pResolved->iCheckColor;
	tVisual.iTextColor = pResolved->iTextColor;
	tVisual.fBorderWidth = 1.0f;
	switch ( iVisual ) {
	case XUI_CHECKBOX_VISUAL_HOVER:
		tVisual.iFillColor = pResolved->iUncheckedHoverFillColor;
		tVisual.iBorderColor = pResolved->iBorderHoverColor;
		break;
	case XUI_CHECKBOX_VISUAL_ACTIVE:
		tVisual.iFillColor = XUI_COLOR_RGBA(234, 244, 255, 255);
		tVisual.iBorderColor = pResolved->iAccentActiveColor;
		break;
	case XUI_CHECKBOX_VISUAL_CHECKED:
	case XUI_CHECKBOX_VISUAL_CHECKED_FOCUS:
		tVisual.iFillColor = pResolved->iAccentColor;
		tVisual.iBorderColor = pResolved->iAccentColor;
		break;
	case XUI_CHECKBOX_VISUAL_CHECKED_HOVER:
		tVisual.iFillColor = pResolved->iAccentHoverColor;
		tVisual.iBorderColor = pResolved->iAccentHoverColor;
		break;
	case XUI_CHECKBOX_VISUAL_CHECKED_ACTIVE:
		tVisual.iFillColor = pResolved->iAccentActiveColor;
		tVisual.iBorderColor = pResolved->iAccentActiveColor;
		break;
	case XUI_CHECKBOX_VISUAL_DISABLED:
	case XUI_CHECKBOX_VISUAL_CHECKED_DISABLED:
		tVisual.iFillColor = pResolved->iDisabledFillColor;
		tVisual.iBorderColor = pResolved->iDisabledBorderColor;
		tVisual.iCheckColor = pResolved->iDisabledCheckColor;
		tVisual.iTextColor = pResolved->iDisabledTextColor;
		break;
	default:
		break;
	}
	return tVisual;
}

static int __xuiCheckBoxDrawSurfaceIndicator(xui_widget pWidget, xui_draw_context pDraw, xui_checkbox_data_t* pData, xui_checkbox_data_t* pResolved, int bChecked, xui_rect_t tDst)
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
			iRet = xuiBuiltinAssetGetRect(bChecked ? "checkbox_checked" : "checkbox_unchecked", &tSrc);
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

static int __xuiCheckBoxDrawDefaultIndicator(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, xui_checkbox_data_t* pResolved, int iVisual, int bChecked)
{
	xui_checkbox_visual_t tVisual;
	float fStroke;
	float fX0;
	float fY0;
	float fX1;
	float fY1;
	float fX2;
	float fY2;
	int iRet;

	tVisual = __xuiCheckBoxVisual(pResolved, iVisual);
	iRet = XUI_OK;
	if ( __xuiCheckBoxColorAlpha(tVisual.iFillColor) != 0 ) {
		iRet = pProxy->drawRectFill(pProxy, pDraw, tRect, tVisual.iFillColor);
	}
	if ( (iRet == XUI_OK) && (tVisual.fBorderWidth > 0.0f) && (__xuiCheckBoxColorAlpha(tVisual.iBorderColor) != 0) ) {
		iRet = pProxy->drawRectStroke(pProxy, pDraw, tRect, tVisual.fBorderWidth, tVisual.iBorderColor);
	}
	if ( (iRet == XUI_OK) && bChecked && (__xuiCheckBoxColorAlpha(tVisual.iCheckColor) != 0) && (pProxy->drawLine != NULL) ) {
		fStroke = (tRect.fW >= 18.0f) ? 2.4f : 2.0f;
		fX0 = tRect.fX + tRect.fW * 0.26f;
		fY0 = tRect.fY + tRect.fH * 0.54f;
		fX1 = tRect.fX + tRect.fW * 0.43f;
		fY1 = tRect.fY + tRect.fH * 0.70f;
		fX2 = tRect.fX + tRect.fW * 0.76f;
		fY2 = tRect.fY + tRect.fH * 0.34f;
		iRet = pProxy->drawLine(pProxy, pDraw, fX0, fY0, fX1, fY1, fStroke, tVisual.iCheckColor);
		if ( iRet == XUI_OK ) {
			iRet = pProxy->drawLine(pProxy, pDraw, fX1, fY1, fX2, fY2, fStroke, tVisual.iCheckColor);
		}
	}
	return iRet;
}

static int __xuiCheckBoxDrawIndicator(xui_widget pWidget, xui_draw_context pDraw, xui_checkbox_data_t* pData, xui_checkbox_data_t* pResolved, int iVisual, int bChecked, xui_rect_t tRect)
{
	xui_proxy pProxy;
	int iRet;

	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( (pData->pCheckedSurface != NULL) || (pData->pUncheckedSurface != NULL) || pResolved->bUseBuiltinAtlas ) {
		iRet = __xuiCheckBoxDrawSurfaceIndicator(pWidget, pDraw, pData, pResolved, bChecked, tRect);
		if ( iRet == XUI_OK ) {
			return XUI_OK;
		}
	}
	return __xuiCheckBoxDrawDefaultIndicator(pProxy, pDraw, tRect, pResolved, iVisual, bChecked);
}

static int __xuiCheckBoxCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_checkbox_data_t* pData;
	xui_checkbox_data_t tResolved;
	xui_checkbox_visual_t tVisual;
	xui_proxy pProxy;
	xui_rect_t tIndicator;
	uint32_t iState;
	uint32_t iRenderState;
	int iVisual;
	int bChecked;
	int iRet;

	(void)pUser;
	if ( (pWidget == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiCheckBoxGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	__xuiCheckBoxResolve(pWidget, pData, &tResolved);
	iRenderState = iStateId;
	if ( !xuiWidgetGetEnabled(pWidget) ) {
		iRenderState = XUI_WIDGET_STATE_DISABLED | (pData->bChecked ? XUI_CHECKBOX_STATE_CHECKED : 0);
	}
	iVisual = __xuiCheckBoxStateVisual(iRenderState);
	bChecked = ((iRenderState & XUI_CHECKBOX_STATE_CHECKED) != 0);
	__xuiCheckBoxLayoutContent(pWidget, pData, &tResolved);
	tIndicator = pData->tIndicatorRect;
	iRet = __xuiCheckBoxDrawIndicator(pWidget, pDraw, pData, &tResolved, iVisual, bChecked, tIndicator);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iState = iRenderState;
	if ( ((iState & XUI_WIDGET_STATE_FOCUS) != 0) && ((iState & XUI_WIDGET_STATE_DISABLED) == 0) && (tResolved.fFocusWidth > 0.0f) && (__xuiCheckBoxColorAlpha(tResolved.iFocusColor) != 0) ) {
		iRet = pProxy->drawRectStroke(pProxy, pDraw, xuiInternalInsetRect(tIndicator, -2.0f), tResolved.fFocusWidth, __xuiCheckBoxColorWithAlpha(tResolved.iFocusColor, 160));
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	tVisual = __xuiCheckBoxVisual(&tResolved, iVisual);
	if ( (tResolved.pFont != NULL) && (pData->sText != NULL) && (pData->sText[0] != '\0') && (pData->tTextRect.fW > 0.0f) && (__xuiCheckBoxColorAlpha(tVisual.iTextColor) != 0) ) {
		iRet = pProxy->drawText(pProxy, pDraw, tResolved.pFont, pData->sText, pData->tTextRect, tVisual.iTextColor, tResolved.iTextFlags | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
	}
	return iRet;
}

static void __xuiCheckBoxNotify(xui_widget pWidget, xui_checkbox_data_t* pData)
{
	pData->iChangeCount++;
	if ( pData->onChange != NULL ) {
		pData->onChange(pWidget, pData->bChecked, pData->pChangeUser);
	}
}

static int __xuiCheckBoxSetCheckedInternal(xui_widget pWidget, xui_checkbox_data_t* pData, int bChecked, int bNotify)
{
	bChecked = (bChecked != 0);
	if ( pData->bChecked == bChecked ) {
		return XUI_OK;
	}
	pData->bChecked = bChecked;
	(void)__xuiCheckBoxSyncState(pWidget, pData);
	if ( bNotify ) {
		__xuiCheckBoxNotify(pWidget, pData);
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static void __xuiCheckBoxDefaultAction(xui_widget pWidget, void* pUser)
{
	xui_checkbox_data_t* pData;

	(void)pUser;
	pData = __xuiCheckBoxGetData(pWidget);
	if ( pData == NULL ) {
		return;
	}
	(void)__xuiCheckBoxSetCheckedInternal(pWidget, pData, !pData->bChecked, 1);
}

static int __xuiCheckBoxEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_checkbox_data_t* pData;
	xui_context pContext;
	int bLeftButton;

	(void)pUser;
	if ( (pWidget == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiCheckBoxGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pContext = xuiWidgetGetContext(pWidget);
	if ( !xuiWidgetGetEnabled(pWidget) || !xuiWidgetGetVisible(pWidget) ) {
		pData->bKeyboardActive = 0;
		(void)__xuiCheckBoxSyncState(pWidget, pData);
		return XUI_OK;
	}
	bLeftButton = (pEvent->iButton == 0) || (pEvent->iButton == XUI_POINTER_BUTTON_LEFT);
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_ENTER:
	case XUI_EVENT_POINTER_LEAVE:
	case XUI_EVENT_POINTER_MOVE:
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
		(void)__xuiCheckBoxSyncState(pWidget, pData);
		return XUI_OK;
	case XUI_EVENT_POINTER_DOWN:
		if ( bLeftButton ) {
			(void)xuiSetPointerCapture(pContext, pWidget);
			(void)__xuiCheckBoxSyncState(pWidget, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_UP:
		if ( bLeftButton ) {
			(void)xuiReleasePointerCapture(pContext, pWidget);
			(void)__xuiCheckBoxSyncState(pWidget, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		pData->bKeyboardActive = 0;
		(void)__xuiCheckBoxSyncState(pWidget, pData);
		return XUI_OK;
	case XUI_EVENT_POINTER_CLICK:
		if ( bLeftButton ) {
			(void)__xuiCheckBoxSetCheckedInternal(pWidget, pData, !pData->bChecked, 1);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_KEY_DOWN:
		if ( pEvent->iKey == XUI_KEY_SPACE ) {
			pData->bKeyboardActive = 1;
			(void)__xuiCheckBoxSyncState(pWidget, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_KEY_UP:
		if ( pEvent->iKey == XUI_KEY_SPACE ) {
			if ( pData->bKeyboardActive ) {
				pData->bKeyboardActive = 0;
				(void)__xuiCheckBoxSetCheckedInternal(pWidget, pData, !pData->bChecked, 1);
			}
			(void)__xuiCheckBoxSyncState(pWidget, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	default:
		break;
	}
	return XUI_OK;
}

static void __xuiCheckBoxDefaultLayout(xui_layout_t* pLayout)
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

static void __xuiCheckBoxDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE | XUI_CACHE_UPDATE_ALL_STATES;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiCheckBoxInitCacheStates(xui_widget pWidget)
{
	static const uint32_t arrState[XUI_CHECKBOX_VISUAL_COUNT] = {
		0,
		XUI_WIDGET_STATE_HOVER,
		XUI_WIDGET_STATE_ACTIVE,
		XUI_WIDGET_STATE_FOCUS,
		XUI_WIDGET_STATE_DISABLED,
		XUI_CHECKBOX_STATE_CHECKED,
		XUI_CHECKBOX_STATE_CHECKED | XUI_WIDGET_STATE_HOVER,
		XUI_CHECKBOX_STATE_CHECKED | XUI_WIDGET_STATE_ACTIVE,
		XUI_CHECKBOX_STATE_CHECKED | XUI_WIDGET_STATE_FOCUS,
		XUI_CHECKBOX_STATE_CHECKED | XUI_WIDGET_STATE_DISABLED
	};
	int i;
	int iRet;

	iRet = xuiWidgetSetCacheStateCount(pWidget, XUI_CHECKBOX_VISUAL_COUNT);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	for ( i = 0; i < XUI_CHECKBOX_VISUAL_COUNT; i++ ) {
		iRet = xuiWidgetSetCacheStateId(pWidget, i, 0x71000000u + (uint32_t)i);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	for ( i = 0; i < XUI_CHECKBOX_VISUAL_COUNT; i++ ) {
		iRet = xuiWidgetSetCacheStateId(pWidget, i, arrState[i]);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return XUI_OK;
}

static int __xuiCheckBoxInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetDefaultAction(pWidget, __xuiCheckBoxDefaultAction, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_ENTER, __xuiCheckBoxEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiCheckBoxEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiCheckBoxEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiCheckBoxEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiCheckBoxEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CLICK, __xuiCheckBoxEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiCheckBoxEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiCheckBoxEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiCheckBoxEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiCheckBoxEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_UP, __xuiCheckBoxEvent, NULL);
	return iRet;
}

static int __xuiCheckBoxInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_checkbox_data_t* pData;
	const xui_checkbox_desc_t* pDesc;
	xui_theme_t tTheme;
	xui_context pContext;
	xui_thickness_t tPadding;
	int iRet;

	(void)pUser;
	pData = (xui_checkbox_data_t*)pTypeData;
	pDesc = (const xui_checkbox_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiCheckBoxDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pContext = xuiWidgetGetContext(pWidget);
	memset(&tTheme, 0, sizeof(tTheme));
	tTheme.iSize = sizeof(tTheme);
	(void)xuiGetTheme(pContext, &tTheme);
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
	pData->iUncheckedFillColor = XUI_COLOR_RGBA(255, 255, 255, 255);
	pData->iUncheckedHoverFillColor = XUI_COLOR_RGBA(247, 251, 255, 255);
	pData->iDisabledFillColor = XUI_COLOR_RGBA(243, 245, 248, 255);
	pData->iDisabledBorderColor = XUI_COLOR_RGBA(214, 221, 230, 255);
	pData->iDisabledCheckColor = XUI_COLOR_RGBA(174, 184, 196, 255);
	pData->iCheckColor = XUI_COLOR_WHITE;
	pData->fIndicatorSize = (pDesc != NULL && pDesc->fIndicatorSize > 0.0f) ? pDesc->fIndicatorSize : 18.0f;
	pData->fGap = (pDesc != NULL && pDesc->fGap > 0.0f) ? pDesc->fGap : 8.0f;
	pData->fFocusWidth = 2.0f;
	pData->bChecked = (pDesc != NULL) ? (pDesc->bChecked != 0) : 0;
	pData->bUseBuiltinAtlas = (pDesc != NULL) ? (pDesc->bUseBuiltinAtlas != 0) : 0;
	iRet = __xuiCheckBoxTextSet(pData, (pDesc != NULL) ? pDesc->sText : "");
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	tPadding = (xui_thickness_t){4.0f, 3.0f, 4.0f, 3.0f};
	(void)xuiWidgetSetPadding(pWidget, tPadding);
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	iRet = __xuiCheckBoxInitCacheStates(pWidget);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iRet = __xuiCheckBoxInitEvents(pWidget);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return __xuiCheckBoxSyncState(pWidget, pData);
}

static void __xuiCheckBoxDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_checkbox_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_checkbox_data_t*)pTypeData;
	if ( pData == NULL ) {
		return;
	}
	if ( pData->sText != NULL ) {
		xrtFree(pData->sText);
	}
	memset(pData, 0, sizeof(*pData));
}

static void __xuiCheckBoxRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
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

static void __xuiCheckBoxRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;
	uint32_t iLayoutDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	iLayoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiCheckBoxRegisterStyleProperty(pContext, pType, "choice.text.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiCheckBoxRegisterStyleProperty(pContext, pType, "choice.text.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiCheckBoxRegisterStyleProperty(pContext, pType, "choice.indicator.size", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiCheckBoxRegisterStyleProperty(pContext, pType, "choice.indicator.gap", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiCheckBoxRegisterStyleProperty(pContext, pType, "choice.accent.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiCheckBoxRegisterStyleProperty(pContext, pType, "choice.accent.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiCheckBoxRegisterStyleProperty(pContext, pType, "choice.accent.active_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiCheckBoxRegisterStyleProperty(pContext, pType, "choice.border.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCheckBoxRegisterStyleProperty(pContext, pType, "choice.border.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCheckBoxRegisterStyleProperty(pContext, pType, "choice.focus.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiCheckBoxRegisterStyleProperty(pContext, pType, "choice.focus.width", XUI_STYLE_VALUE_FLOAT, iPaintDirty, 0);
	__xuiCheckBoxRegisterStyleProperty(pContext, pType, "text.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiCheckBoxRegisterStyleProperty(pContext, pType, "text.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiCheckBoxRegisterStyleProperty(pContext, pType, "text.flags", XUI_STYLE_VALUE_INT, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiCheckBoxRegisterStyleProperty(pContext, pType, "font.name", XUI_STYLE_VALUE_STRING, iLayoutDirty, XUI_STYLE_PROPERTY_INHERITED);
}

XUI_API xui_widget_type xuiCheckBoxGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "checkbox");
	if ( pType != NULL ) {
		__xuiCheckBoxRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "checkbox";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_checkbox_data_t);
	tDesc.onInit = __xuiCheckBoxInit;
	tDesc.onDestroy = __xuiCheckBoxDestroy;
	tDesc.onContentMeasure = __xuiCheckBoxContentMeasure;
	tDesc.onCacheRender = __xuiCheckBoxCacheRender;
	__xuiCheckBoxDefaultLayout(&tDesc.tLayout);
	__xuiCheckBoxDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	__xuiCheckBoxRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiCheckBoxCreate(xui_context pContext, xui_widget* ppWidget, const xui_checkbox_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiCheckBoxDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiCheckBoxGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiCheckBoxSetChange(xui_widget pWidget, xui_checkbox_change_proc onChange, void* pUser)
{
	xui_checkbox_data_t* pData;

	pData = __xuiCheckBoxGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->onChange = onChange;
	pData->pChangeUser = pUser;
	return XUI_OK;
}

XUI_API int xuiCheckBoxSetText(xui_widget pWidget, const char* sText)
{
	xui_checkbox_data_t* pData;
	int iRet;

	pData = __xuiCheckBoxGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiCheckBoxTextSet(pData, sText);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API const char* xuiCheckBoxGetText(xui_widget pWidget)
{
	xui_checkbox_data_t* pData;

	pData = __xuiCheckBoxGetData(pWidget);
	return (pData != NULL && pData->sText != NULL) ? pData->sText : "";
}

XUI_API int xuiCheckBoxSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_checkbox_data_t* pData;

	pData = __xuiCheckBoxGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->pFont = pFont;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_font xuiCheckBoxGetFont(xui_widget pWidget)
{
	xui_checkbox_data_t* pData;

	pData = __xuiCheckBoxGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API int xuiCheckBoxSetTextFlags(xui_widget pWidget, uint32_t iTextFlags)
{
	xui_checkbox_data_t* pData;

	pData = __xuiCheckBoxGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iTextFlags = iTextFlags | XUI_TEXT_CLIP;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiCheckBoxGetTextFlags(xui_widget pWidget)
{
	xui_checkbox_data_t* pData;

	pData = __xuiCheckBoxGetData(pWidget);
	return (pData != NULL) ? pData->iTextFlags : 0;
}

XUI_API int xuiCheckBoxSetChecked(xui_widget pWidget, int bChecked)
{
	xui_checkbox_data_t* pData;

	pData = __xuiCheckBoxGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return __xuiCheckBoxSetCheckedInternal(pWidget, pData, bChecked, 0);
}

XUI_API int xuiCheckBoxGetChecked(xui_widget pWidget)
{
	xui_checkbox_data_t* pData;

	pData = __xuiCheckBoxGetData(pWidget);
	return (pData != NULL) ? pData->bChecked : 0;
}

XUI_API int xuiCheckBoxSetTextColor(xui_widget pWidget, uint32_t iColor)
{
	xui_checkbox_data_t* pData = __xuiCheckBoxGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iTextColor = iColor;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiCheckBoxGetTextColor(xui_widget pWidget)
{
	xui_checkbox_data_t* pData = __xuiCheckBoxGetData(pWidget);
	return (pData != NULL) ? pData->iTextColor : 0;
}

XUI_API int xuiCheckBoxSetDisabledTextColor(xui_widget pWidget, uint32_t iColor)
{
	xui_checkbox_data_t* pData = __xuiCheckBoxGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iDisabledTextColor = iColor;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiCheckBoxGetDisabledTextColor(xui_widget pWidget)
{
	xui_checkbox_data_t* pData = __xuiCheckBoxGetData(pWidget);
	return (pData != NULL) ? pData->iDisabledTextColor : 0;
}

XUI_API int xuiCheckBoxSetIndicatorSize(xui_widget pWidget, float fSize)
{
	xui_checkbox_data_t* pData = __xuiCheckBoxGetData(pWidget);
	if ( (pData == NULL) || (fSize <= 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fIndicatorSize = fSize;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiCheckBoxGetIndicatorSize(xui_widget pWidget)
{
	xui_checkbox_data_t* pData = __xuiCheckBoxGetData(pWidget);
	return (pData != NULL) ? pData->fIndicatorSize : 0.0f;
}

XUI_API int xuiCheckBoxSetGap(xui_widget pWidget, float fGap)
{
	xui_checkbox_data_t* pData = __xuiCheckBoxGetData(pWidget);
	if ( (pData == NULL) || (fGap < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fGap = fGap;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiCheckBoxGetGap(xui_widget pWidget)
{
	xui_checkbox_data_t* pData = __xuiCheckBoxGetData(pWidget);
	return (pData != NULL) ? pData->fGap : 0.0f;
}

XUI_API int xuiCheckBoxSetColors(xui_widget pWidget, uint32_t iAccent, uint32_t iBorder, uint32_t iHoverBorder, uint32_t iFocus)
{
	xui_checkbox_data_t* pData = __xuiCheckBoxGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iAccentColor = iAccent;
	pData->iAccentHoverColor = iAccent;
	pData->iAccentActiveColor = iAccent;
	pData->iBorderColor = iBorder;
	pData->iBorderHoverColor = iHoverBorder;
	pData->iFocusColor = iFocus;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCheckBoxGetColors(xui_widget pWidget, uint32_t* pAccent, uint32_t* pBorder, uint32_t* pHoverBorder, uint32_t* pFocus)
{
	xui_checkbox_data_t* pData = __xuiCheckBoxGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pAccent != NULL ) *pAccent = pData->iAccentColor;
	if ( pBorder != NULL ) *pBorder = pData->iBorderColor;
	if ( pHoverBorder != NULL ) *pHoverBorder = pData->iBorderHoverColor;
	if ( pFocus != NULL ) *pFocus = pData->iFocusColor;
	return XUI_OK;
}

XUI_API int xuiCheckBoxSetIndicatorSurface(xui_widget pWidget, xui_surface pUncheckedSurface, xui_rect_t tUncheckedSrc, xui_surface pCheckedSurface, xui_rect_t tCheckedSrc)
{
	xui_checkbox_data_t* pData = __xuiCheckBoxGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pUncheckedSurface = pUncheckedSurface;
	pData->tUncheckedSrc = tUncheckedSrc;
	pData->pCheckedSurface = pCheckedSurface;
	pData->tCheckedSrc = tCheckedSrc;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCheckBoxGetIndicatorSurface(xui_widget pWidget, xui_surface* ppUncheckedSurface, xui_rect_t* pUncheckedSrc, xui_surface* ppCheckedSurface, xui_rect_t* pCheckedSrc)
{
	xui_checkbox_data_t* pData = __xuiCheckBoxGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( ppUncheckedSurface != NULL ) *ppUncheckedSurface = pData->pUncheckedSurface;
	if ( pUncheckedSrc != NULL ) *pUncheckedSrc = pData->tUncheckedSrc;
	if ( ppCheckedSurface != NULL ) *ppCheckedSurface = pData->pCheckedSurface;
	if ( pCheckedSrc != NULL ) *pCheckedSrc = pData->tCheckedSrc;
	return XUI_OK;
}

XUI_API int xuiCheckBoxUseBuiltinAtlas(xui_widget pWidget, int bEnable)
{
	xui_checkbox_data_t* pData = __xuiCheckBoxGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bUseBuiltinAtlas = (bEnable != 0);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCheckBoxGetUseBuiltinAtlas(xui_widget pWidget)
{
	xui_checkbox_data_t* pData = __xuiCheckBoxGetData(pWidget);
	return (pData != NULL) ? pData->bUseBuiltinAtlas : 0;
}

XUI_API xui_rect_t xuiCheckBoxGetIndicatorRect(xui_widget pWidget)
{
	xui_checkbox_data_t* pData = __xuiCheckBoxGetData(pWidget);
	return (pData != NULL) ? pData->tIndicatorRect : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API xui_rect_t xuiCheckBoxGetTextRect(xui_widget pWidget)
{
	xui_checkbox_data_t* pData = __xuiCheckBoxGetData(pWidget);
	return (pData != NULL) ? pData->tTextRect : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API uint32_t xuiCheckBoxGetState(xui_widget pWidget)
{
	xui_checkbox_data_t* pData = __xuiCheckBoxGetData(pWidget);
	return (pData != NULL) ? __xuiCheckBoxComputeState(pWidget, pData) : 0;
}
