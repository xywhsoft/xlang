#include "xui_internal.h"

#include <string.h>

#define XUI_TOGGLE_VISUAL_NORMAL	0
#define XUI_TOGGLE_VISUAL_HOVER		1
#define XUI_TOGGLE_VISUAL_ACTIVE	2
#define XUI_TOGGLE_VISUAL_FOCUS		3
#define XUI_TOGGLE_VISUAL_DISABLED	4
#define XUI_TOGGLE_VISUAL_CHECKED	5
#define XUI_TOGGLE_VISUAL_CHECKED_HOVER	6
#define XUI_TOGGLE_VISUAL_CHECKED_ACTIVE	7
#define XUI_TOGGLE_VISUAL_CHECKED_FOCUS	8
#define XUI_TOGGLE_VISUAL_CHECKED_DISABLED	9
#define XUI_TOGGLE_VISUAL_COUNT		10

typedef struct xui_toggle_visual_t {
	uint32_t iTrackColor;
	uint32_t iTrackBorderColor;
	uint32_t iThumbColor;
	uint32_t iThumbBorderColor;
	uint32_t iTextColor;
	float fTrackBorderWidth;
	float fThumbBorderWidth;
} xui_toggle_visual_t;

typedef struct xui_toggle_data_t {
	char* sText;
	int iTextCapacity;
	char* sUncheckedText;
	int iUncheckedTextCapacity;
	char* sCheckedText;
	int iCheckedTextCapacity;
	xui_font pFont;
	xui_toggle_change_proc onChange;
	void* pChangeUser;
	uint32_t iTextColor;
	uint32_t iDisabledTextColor;
	uint32_t iUncheckedTextColor;
	uint32_t iCheckedTextColor;
	uint32_t iTextFlags;
	uint32_t iAccentColor;
	uint32_t iAccentHoverColor;
	uint32_t iAccentActiveColor;
	uint32_t iTrackColor;
	uint32_t iTrackHoverColor;
	uint32_t iTrackActiveColor;
	uint32_t iTrackBorderColor;
	uint32_t iThumbColor;
	uint32_t iThumbBorderColor;
	uint32_t iFocusColor;
	uint32_t iDisabledTrackColor;
	uint32_t iDisabledTrackBorderColor;
	uint32_t iDisabledThumbColor;
	uint32_t iDisabledThumbBorderColor;
	float fTrackWidth;
	float fTrackHeight;
	float fThumbSize;
	float fGap;
	float fFocusWidth;
	float fInnerTextPadding;
	float fInnerTextGap;
	int bChecked;
	int bKeyboardActive;
	int iChangeCount;
	int bUseBuiltinAtlas;
	xui_surface pUncheckedSurface;
	xui_surface pCheckedSurface;
	xui_rect_t tUncheckedSrc;
	xui_rect_t tCheckedSrc;
	xui_rect_t tTrackRect;
	xui_rect_t tThumbRect;
	xui_rect_t tTextRect;
	xui_rect_t tInnerTextRect;
} xui_toggle_data_t;

static uint32_t __xuiToggleColorWithAlpha(uint32_t iColor, uint32_t iAlpha)
{
	return (iColor & 0xffffff00u) | (iAlpha & 0xffu);
}

static uint32_t __xuiToggleColorAlpha(uint32_t iColor)
{
	return iColor & 0xffu;
}

static int __xuiToggleDescValid(const xui_toggle_desc_t* pDesc)
{
	return (pDesc == NULL) || (pDesc->iSize >= sizeof(*pDesc));
}

static int __xuiToggleStringSet(char** ppText, int* pCapacity, const char* sText)
{
	char* sNew;
	int iNeed;

	if ( (ppText == NULL) || (pCapacity == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( sText == NULL ) {
		sText = "";
	}
	iNeed = (int)strlen(sText) + 1;
	if ( iNeed > *pCapacity ) {
		sNew = (char*)xrtMalloc((size_t)iNeed);
		if ( sNew == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		if ( *ppText != NULL ) {
			xrtFree(*ppText);
		}
		*ppText = sNew;
		*pCapacity = iNeed;
	}
	memcpy(*ppText, sText, (size_t)iNeed);
	return XUI_OK;
}

static int __xuiToggleTextSet(xui_toggle_data_t* pData, const char* sText)
{
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return __xuiToggleStringSet(&pData->sText, &pData->iTextCapacity, sText);
}

static int __xuiToggleInnerTextSet(xui_toggle_data_t* pData, const char* sUncheckedText, const char* sCheckedText)
{
	int iRet;

	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiToggleStringSet(&pData->sUncheckedText, &pData->iUncheckedTextCapacity, sUncheckedText);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return __xuiToggleStringSet(&pData->sCheckedText, &pData->iCheckedTextCapacity, sCheckedText);
}

static xui_toggle_data_t* __xuiToggleGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( pWidget == NULL ) {
		return NULL;
	}
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "toggle");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_toggle_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiToggleStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
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

static int __xuiToggleStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
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

static int __xuiToggleStyleInt(xui_widget pWidget, const char* sName, int* pValue)
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

static xui_font __xuiToggleStyleFont(xui_widget pWidget, xui_font pBaseFont)
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

static void __xuiToggleResolve(xui_widget pWidget, xui_toggle_data_t* pData, xui_toggle_data_t* pResolved)
{
	int iTextFlags;

	*pResolved = *pData;
	pResolved->pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	pResolved->iTextFlags = pData->iTextFlags | XUI_TEXT_CLIP;
	pResolved->fTrackWidth = (pData->fTrackWidth > 0.0f) ? pData->fTrackWidth : 38.0f;
	pResolved->fTrackHeight = (pData->fTrackHeight > 0.0f) ? pData->fTrackHeight : 22.0f;
	pResolved->fThumbSize = (pData->fThumbSize > 0.0f) ? pData->fThumbSize : 14.0f;
	pResolved->fGap = (pData->fGap >= 0.0f) ? pData->fGap : 8.0f;
	pResolved->fFocusWidth = (pData->fFocusWidth > 0.0f) ? pData->fFocusWidth : 2.0f;
	pResolved->fInnerTextPadding = (pData->fInnerTextPadding > 0.0f) ? pData->fInnerTextPadding : 6.0f;
	pResolved->fInnerTextGap = (pData->fInnerTextGap >= 0.0f) ? pData->fInnerTextGap : 2.0f;
	(void)__xuiToggleStyleColor(pWidget, "choice.text.color", &pResolved->iTextColor);
	(void)__xuiToggleStyleColor(pWidget, "text.color", &pResolved->iTextColor);
	(void)__xuiToggleStyleColor(pWidget, "choice.text.disabled_color", &pResolved->iDisabledTextColor);
	(void)__xuiToggleStyleColor(pWidget, "text.disabled_color", &pResolved->iDisabledTextColor);
	(void)__xuiToggleStyleColor(pWidget, "toggle.inner_text.unchecked_color", &pResolved->iUncheckedTextColor);
	(void)__xuiToggleStyleColor(pWidget, "toggle.inner_text.checked_color", &pResolved->iCheckedTextColor);
	(void)__xuiToggleStyleColor(pWidget, "choice.accent.color", &pResolved->iAccentColor);
	(void)__xuiToggleStyleColor(pWidget, "choice.accent.hover_color", &pResolved->iAccentHoverColor);
	(void)__xuiToggleStyleColor(pWidget, "choice.accent.active_color", &pResolved->iAccentActiveColor);
	(void)__xuiToggleStyleColor(pWidget, "choice.focus.color", &pResolved->iFocusColor);
	(void)__xuiToggleStyleColor(pWidget, "toggle.track.color", &pResolved->iTrackColor);
	(void)__xuiToggleStyleColor(pWidget, "toggle.track.hover_color", &pResolved->iTrackHoverColor);
	(void)__xuiToggleStyleColor(pWidget, "toggle.track.active_color", &pResolved->iTrackActiveColor);
	(void)__xuiToggleStyleColor(pWidget, "toggle.track.border_color", &pResolved->iTrackBorderColor);
	(void)__xuiToggleStyleColor(pWidget, "toggle.thumb.color", &pResolved->iThumbColor);
	(void)__xuiToggleStyleColor(pWidget, "toggle.thumb.border_color", &pResolved->iThumbBorderColor);
	(void)__xuiToggleStyleFloat(pWidget, "toggle.track.width", &pResolved->fTrackWidth);
	(void)__xuiToggleStyleFloat(pWidget, "toggle.track.height", &pResolved->fTrackHeight);
	(void)__xuiToggleStyleFloat(pWidget, "toggle.thumb.size", &pResolved->fThumbSize);
	(void)__xuiToggleStyleFloat(pWidget, "choice.indicator.gap", &pResolved->fGap);
	(void)__xuiToggleStyleFloat(pWidget, "choice.focus.width", &pResolved->fFocusWidth);
	(void)__xuiToggleStyleFloat(pWidget, "toggle.inner_text.padding", &pResolved->fInnerTextPadding);
	(void)__xuiToggleStyleFloat(pWidget, "toggle.inner_text.gap", &pResolved->fInnerTextGap);
	iTextFlags = (int)pResolved->iTextFlags;
	if ( __xuiToggleStyleInt(pWidget, "text.flags", &iTextFlags) ) {
		pResolved->iTextFlags = (uint32_t)iTextFlags | XUI_TEXT_CLIP;
	}
	pResolved->pFont = __xuiToggleStyleFont(pWidget, pResolved->pFont);
}

static uint32_t __xuiToggleComputeState(xui_widget pWidget, xui_toggle_data_t* pData)
{
	uint32_t iState;

	iState = xuiWidgetGetInputState(pWidget);
	if ( !xuiWidgetGetEnabled(pWidget) ) {
		iState |= XUI_WIDGET_STATE_DISABLED;
	}
	if ( (pData != NULL) && pData->bChecked ) {
		iState |= XUI_TOGGLE_STATE_CHECKED;
	}
	if ( (pData != NULL) && pData->bKeyboardActive ) {
		iState |= XUI_WIDGET_STATE_ACTIVE;
	}
	return iState;
}

static uint32_t __xuiToggleStateId(uint32_t iState)
{
	if ( ((iState & XUI_WIDGET_STATE_DISABLED) != 0) && ((iState & XUI_TOGGLE_STATE_CHECKED) != 0) ) return XUI_WIDGET_STATE_DISABLED | XUI_TOGGLE_STATE_CHECKED;
	if ( (iState & XUI_WIDGET_STATE_DISABLED) != 0 ) return XUI_WIDGET_STATE_DISABLED;
	if ( ((iState & XUI_TOGGLE_STATE_CHECKED) != 0) && ((iState & XUI_WIDGET_STATE_ACTIVE) != 0) ) return XUI_TOGGLE_STATE_CHECKED | XUI_WIDGET_STATE_ACTIVE;
	if ( ((iState & XUI_TOGGLE_STATE_CHECKED) != 0) && ((iState & XUI_WIDGET_STATE_HOVER) != 0) ) return XUI_TOGGLE_STATE_CHECKED | XUI_WIDGET_STATE_HOVER;
	if ( ((iState & XUI_TOGGLE_STATE_CHECKED) != 0) && ((iState & XUI_WIDGET_STATE_FOCUS) != 0) ) return XUI_TOGGLE_STATE_CHECKED | XUI_WIDGET_STATE_FOCUS;
	if ( (iState & XUI_WIDGET_STATE_ACTIVE) != 0 ) return XUI_WIDGET_STATE_ACTIVE;
	if ( (iState & XUI_WIDGET_STATE_HOVER) != 0 ) return XUI_WIDGET_STATE_HOVER;
	if ( (iState & XUI_WIDGET_STATE_FOCUS) != 0 ) return XUI_WIDGET_STATE_FOCUS;
	if ( (iState & XUI_TOGGLE_STATE_CHECKED) != 0 ) return XUI_TOGGLE_STATE_CHECKED;
	return 0;
}

static int __xuiToggleSyncState(xui_widget pWidget, xui_toggle_data_t* pData)
{
	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return xuiWidgetSetStateId(pWidget, __xuiToggleStateId(__xuiToggleComputeState(pWidget, pData)));
}

static int __xuiToggleStateVisual(uint32_t iStateId)
{
	if ( ((iStateId & XUI_WIDGET_STATE_DISABLED) != 0) && ((iStateId & XUI_TOGGLE_STATE_CHECKED) != 0) ) return XUI_TOGGLE_VISUAL_CHECKED_DISABLED;
	if ( (iStateId & XUI_WIDGET_STATE_DISABLED) != 0 ) return XUI_TOGGLE_VISUAL_DISABLED;
	if ( ((iStateId & XUI_TOGGLE_STATE_CHECKED) != 0) && ((iStateId & XUI_WIDGET_STATE_ACTIVE) != 0) ) return XUI_TOGGLE_VISUAL_CHECKED_ACTIVE;
	if ( ((iStateId & XUI_TOGGLE_STATE_CHECKED) != 0) && ((iStateId & XUI_WIDGET_STATE_HOVER) != 0) ) return XUI_TOGGLE_VISUAL_CHECKED_HOVER;
	if ( ((iStateId & XUI_TOGGLE_STATE_CHECKED) != 0) && ((iStateId & XUI_WIDGET_STATE_FOCUS) != 0) ) return XUI_TOGGLE_VISUAL_CHECKED_FOCUS;
	if ( (iStateId & XUI_WIDGET_STATE_ACTIVE) != 0 ) return XUI_TOGGLE_VISUAL_ACTIVE;
	if ( (iStateId & XUI_WIDGET_STATE_HOVER) != 0 ) return XUI_TOGGLE_VISUAL_HOVER;
	if ( (iStateId & XUI_TOGGLE_STATE_CHECKED) != 0 ) return XUI_TOGGLE_VISUAL_CHECKED;
	if ( (iStateId & XUI_WIDGET_STATE_FOCUS) != 0 ) return XUI_TOGGLE_VISUAL_FOCUS;
	return XUI_TOGGLE_VISUAL_NORMAL;
}

static xui_vec2_t __xuiToggleMeasureText(xui_widget pWidget, xui_font pFont, const char* sText)
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

static int __xuiToggleHasSurfaceIndicator(const xui_toggle_data_t* pData)
{
	return (pData != NULL) &&
	       ((pData->pCheckedSurface != NULL) || (pData->pUncheckedSurface != NULL) || pData->bUseBuiltinAtlas);
}

static int __xuiToggleHasInnerText(const xui_toggle_data_t* pData)
{
	if ( (pData == NULL) || __xuiToggleHasSurfaceIndicator(pData) ) {
		return 0;
	}
	return ((pData->sUncheckedText != NULL) && (pData->sUncheckedText[0] != '\0')) ||
	       ((pData->sCheckedText != NULL) && (pData->sCheckedText[0] != '\0'));
}

static xui_vec2_t __xuiToggleMeasureInnerText(xui_widget pWidget, xui_toggle_data_t* pResolved)
{
	xui_vec2_t tUnchecked;
	xui_vec2_t tChecked;

	memset(&tUnchecked, 0, sizeof(tUnchecked));
	memset(&tChecked, 0, sizeof(tChecked));
	if ( !__xuiToggleHasInnerText(pResolved) ) {
		return tUnchecked;
	}
	tUnchecked = __xuiToggleMeasureText(pWidget, pResolved->pFont, pResolved->sUncheckedText);
	tChecked = __xuiToggleMeasureText(pWidget, pResolved->pFont, pResolved->sCheckedText);
	if ( tChecked.fX > tUnchecked.fX ) {
		tUnchecked.fX = tChecked.fX;
	}
	if ( tChecked.fY > tUnchecked.fY ) {
		tUnchecked.fY = tChecked.fY;
	}
	return tUnchecked;
}

static float __xuiToggleEffectiveTrackWidth(xui_widget pWidget, xui_toggle_data_t* pResolved, float fTrackW, float fThumb)
{
	xui_vec2_t tInnerSize;
	float fMinW;

	if ( __xuiToggleHasInnerText(pResolved) ) {
		tInnerSize = __xuiToggleMeasureInnerText(pWidget, pResolved);
		if ( fTrackW < 54.0f ) {
			fTrackW = 54.0f;
		}
		fMinW = fThumb + tInnerSize.fX + pResolved->fInnerTextPadding * 2.0f + pResolved->fInnerTextGap;
		if ( fTrackW < fMinW ) {
			fTrackW = fMinW;
		}
	}
	return fTrackW;
}

static void __xuiToggleLayoutContent(xui_widget pWidget, xui_toggle_data_t* pData, xui_toggle_data_t* pResolved, int bChecked)
{
	xui_rect_t tContent;
	xui_vec2_t tTextSize;
	float fTrackW;
	float fTrackH;
	float fThumb;
	float fInset;
	float fThumbX;

	tContent = xuiWidgetGetContentRect(pWidget);
	fTrackW = pResolved->fTrackWidth;
	fTrackH = pResolved->fTrackHeight;
	fThumb = pResolved->fThumbSize;
	if ( fTrackW < 2.0f ) fTrackW = 2.0f;
	if ( fTrackH < 2.0f ) fTrackH = 2.0f;
	if ( fTrackW > tContent.fW ) fTrackW = tContent.fW;
	if ( fTrackH > tContent.fH ) fTrackH = tContent.fH;
	if ( fThumb < 1.0f ) fThumb = 1.0f;
	if ( fThumb > fTrackH - 4.0f ) fThumb = fTrackH - 4.0f;
	if ( fThumb < 1.0f ) fThumb = 1.0f;
	fTrackW = __xuiToggleEffectiveTrackWidth(pWidget, pResolved, fTrackW, fThumb);
	if ( fTrackW > tContent.fW ) fTrackW = tContent.fW;
	fInset = (fTrackH - fThumb) * 0.5f;
	if ( fInset < 1.0f ) fInset = 1.0f;
	pData->tTrackRect = xuiInternalSnapRect((xui_rect_t){tContent.fX, tContent.fY + (tContent.fH - fTrackH) * 0.5f, fTrackW, fTrackH});
	fThumbX = bChecked ? (pData->tTrackRect.fX + pData->tTrackRect.fW - fInset - fThumb) : (pData->tTrackRect.fX + fInset);
	pData->tThumbRect = xuiInternalSnapRect((xui_rect_t){fThumbX, pData->tTrackRect.fY + (pData->tTrackRect.fH - fThumb) * 0.5f, fThumb, fThumb});
	memset(&pData->tInnerTextRect, 0, sizeof(pData->tInnerTextRect));
	if ( __xuiToggleHasInnerText(pResolved) ) {
		pData->tInnerTextRect = pData->tTrackRect;
		if ( bChecked ) {
			pData->tInnerTextRect.fX += pResolved->fInnerTextPadding;
			pData->tInnerTextRect.fW = pData->tThumbRect.fX - pResolved->fInnerTextGap - pData->tInnerTextRect.fX;
		} else {
			pData->tInnerTextRect.fX = pData->tThumbRect.fX + pData->tThumbRect.fW + pResolved->fInnerTextGap;
			pData->tInnerTextRect.fW = pData->tTrackRect.fX + pData->tTrackRect.fW - pResolved->fInnerTextPadding - pData->tInnerTextRect.fX;
		}
		if ( pData->tInnerTextRect.fW < 0.0f ) {
			pData->tInnerTextRect.fW = 0.0f;
		}
		pData->tInnerTextRect = xuiInternalSnapRect(pData->tInnerTextRect);
	}
	tTextSize = __xuiToggleMeasureText(pWidget, pResolved->pFont, pData->sText);
	(void)tTextSize;
	pData->tTextRect = tContent;
	pData->tTextRect.fX += fTrackW + pResolved->fGap;
	pData->tTextRect.fW -= fTrackW + pResolved->fGap;
	if ( pData->tTextRect.fW < 0.0f ) {
		pData->tTextRect.fW = 0.0f;
	}
	pData->tTextRect = xuiInternalSnapRect(pData->tTextRect);
}

static int __xuiToggleContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_toggle_data_t* pData;
	xui_toggle_data_t tResolved;
	xui_vec2_t tTextSize;
	xui_vec2_t tInnerSize;
	float fTrackH;
	float fThumb;

	(void)tConstraint;
	(void)pUser;
	if ( (pWidget == NULL) || (pSize == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiToggleGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiToggleResolve(pWidget, pData, &tResolved);
	tTextSize = __xuiToggleMeasureText(pWidget, tResolved.pFont, pData->sText);
	fTrackH = tResolved.fTrackHeight;
	if ( fTrackH < 2.0f ) {
		fTrackH = 2.0f;
	}
	fThumb = tResolved.fThumbSize;
	if ( fThumb < 1.0f ) fThumb = 1.0f;
	if ( fThumb > fTrackH - 4.0f ) fThumb = fTrackH - 4.0f;
	if ( fThumb < 1.0f ) fThumb = 1.0f;
	tInnerSize = __xuiToggleMeasureInnerText(pWidget, &tResolved);
	pSize->fX = __xuiToggleEffectiveTrackWidth(pWidget, &tResolved, tResolved.fTrackWidth, fThumb);
	pSize->fY = fTrackH;
	if ( __xuiToggleHasInnerText(&tResolved) && (tInnerSize.fY > pSize->fY) ) {
		pSize->fY = tInnerSize.fY;
	}
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

static xui_toggle_visual_t __xuiToggleVisual(xui_toggle_data_t* pResolved, int iVisual)
{
	xui_toggle_visual_t tVisual;

	memset(&tVisual, 0, sizeof(tVisual));
	tVisual.iTrackColor = pResolved->iTrackColor;
	tVisual.iTrackBorderColor = pResolved->iTrackBorderColor;
	tVisual.iThumbColor = pResolved->iThumbColor;
	tVisual.iThumbBorderColor = pResolved->iThumbBorderColor;
	tVisual.iTextColor = pResolved->iTextColor;
	tVisual.fTrackBorderWidth = 1.0f;
	tVisual.fThumbBorderWidth = 1.0f;
	switch ( iVisual ) {
	case XUI_TOGGLE_VISUAL_HOVER:
		tVisual.iTrackColor = pResolved->iTrackHoverColor;
		break;
	case XUI_TOGGLE_VISUAL_ACTIVE:
		tVisual.iTrackColor = pResolved->iTrackActiveColor;
		break;
	case XUI_TOGGLE_VISUAL_CHECKED:
	case XUI_TOGGLE_VISUAL_CHECKED_FOCUS:
		tVisual.iTrackColor = pResolved->iAccentColor;
		tVisual.iTrackBorderColor = pResolved->iAccentColor;
		break;
	case XUI_TOGGLE_VISUAL_CHECKED_HOVER:
		tVisual.iTrackColor = pResolved->iAccentHoverColor;
		tVisual.iTrackBorderColor = pResolved->iAccentHoverColor;
		break;
	case XUI_TOGGLE_VISUAL_CHECKED_ACTIVE:
		tVisual.iTrackColor = pResolved->iAccentActiveColor;
		tVisual.iTrackBorderColor = pResolved->iAccentActiveColor;
		break;
	case XUI_TOGGLE_VISUAL_DISABLED:
	case XUI_TOGGLE_VISUAL_CHECKED_DISABLED:
		tVisual.iTrackColor = pResolved->iDisabledTrackColor;
		tVisual.iTrackBorderColor = pResolved->iDisabledTrackBorderColor;
		tVisual.iThumbColor = pResolved->iDisabledThumbColor;
		tVisual.iThumbBorderColor = pResolved->iDisabledThumbBorderColor;
		tVisual.iTextColor = pResolved->iDisabledTextColor;
		break;
	default:
		break;
	}
	return tVisual;
}

static int __xuiToggleDrawSurfaceIndicator(xui_widget pWidget, xui_draw_context pDraw, xui_toggle_data_t* pData, xui_toggle_data_t* pResolved, int bChecked, xui_rect_t tDst)
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
			iRet = xuiBuiltinAssetGetRect(bChecked ? "toggle_checked" : "toggle_unchecked", &tSrc);
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

static int __xuiToggleDrawDefaultIndicator(xui_widget pWidget, xui_draw_context pDraw, xui_toggle_data_t* pData, xui_toggle_data_t* pResolved, int iVisual, int bChecked)
{
	xui_toggle_visual_t tVisual;
	xui_proxy pProxy;
	xui_rect_t tTrack;
	xui_rect_t tThumb;
	const char* sInnerText;
	uint32_t iInnerColor;
	int iRet;

	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	tTrack = pData->tTrackRect;
	tThumb = pData->tThumbRect;
	tVisual = __xuiToggleVisual(pResolved, iVisual);
	iRet = XUI_OK;
	if ( __xuiToggleColorAlpha(tVisual.iTrackColor) != 0 ) {
		iRet = pProxy->drawRectFill(pProxy, pDraw, tTrack, tVisual.iTrackColor);
	}
	if ( (iRet == XUI_OK) && (tVisual.fTrackBorderWidth > 0.0f) && (__xuiToggleColorAlpha(tVisual.iTrackBorderColor) != 0) ) {
		iRet = pProxy->drawRectStroke(pProxy, pDraw, tTrack, tVisual.fTrackBorderWidth, tVisual.iTrackBorderColor);
	}
	if ( (iRet == XUI_OK) && __xuiToggleHasInnerText(pResolved) && (pData->tInnerTextRect.fW > 0.0f) && (pResolved->pFont != NULL) ) {
		sInnerText = bChecked ? pResolved->sCheckedText : pResolved->sUncheckedText;
		if ( (sInnerText != NULL) && (sInnerText[0] != '\0') ) {
			iInnerColor = bChecked ? pResolved->iCheckedTextColor : pResolved->iUncheckedTextColor;
			if ( !xuiWidgetGetEnabled(pWidget) ) {
				iInnerColor = pResolved->iDisabledTextColor;
			}
			if ( __xuiToggleColorAlpha(iInnerColor) != 0 ) {
				iRet = pProxy->drawText(pProxy, pDraw, pResolved->pFont, sInnerText, pData->tInnerTextRect, iInnerColor, XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
			}
		}
	}
	if ( iRet == XUI_OK ) {
		if ( __xuiToggleColorAlpha(tVisual.iThumbColor) != 0 ) {
			iRet = pProxy->drawRectFill(pProxy, pDraw, tThumb, tVisual.iThumbColor);
		}
		if ( (iRet == XUI_OK) && (tVisual.fThumbBorderWidth > 0.0f) && (__xuiToggleColorAlpha(tVisual.iThumbBorderColor) != 0) ) {
			iRet = pProxy->drawRectStroke(pProxy, pDraw, tThumb, tVisual.fThumbBorderWidth, tVisual.iThumbBorderColor);
		}
	}
	return iRet;
}

static int __xuiToggleDrawIndicator(xui_widget pWidget, xui_draw_context pDraw, xui_toggle_data_t* pData, xui_toggle_data_t* pResolved, int iVisual, int bChecked)
{
	int iRet;

	if ( (pData->pCheckedSurface != NULL) || (pData->pUncheckedSurface != NULL) || pResolved->bUseBuiltinAtlas ) {
		iRet = __xuiToggleDrawSurfaceIndicator(pWidget, pDraw, pData, pResolved, bChecked, pData->tTrackRect);
		if ( iRet == XUI_OK ) {
			return XUI_OK;
		}
	}
	return __xuiToggleDrawDefaultIndicator(pWidget, pDraw, pData, pResolved, iVisual, bChecked);
}

static int __xuiToggleCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_toggle_data_t* pData;
	xui_toggle_data_t tResolved;
	xui_toggle_visual_t tVisual;
	xui_proxy pProxy;
	uint32_t iRenderState;
	int iVisual;
	int bChecked;
	int iRet;

	(void)pUser;
	if ( (pWidget == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiToggleGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	__xuiToggleResolve(pWidget, pData, &tResolved);
	iRenderState = iStateId;
	if ( !xuiWidgetGetEnabled(pWidget) ) {
		iRenderState = XUI_WIDGET_STATE_DISABLED | (pData->bChecked ? XUI_TOGGLE_STATE_CHECKED : 0);
	}
	iVisual = __xuiToggleStateVisual(iRenderState);
	bChecked = ((iRenderState & XUI_TOGGLE_STATE_CHECKED) != 0);
	__xuiToggleLayoutContent(pWidget, pData, &tResolved, bChecked);
	iRet = __xuiToggleDrawIndicator(pWidget, pDraw, pData, &tResolved, iVisual, bChecked);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( ((iRenderState & XUI_WIDGET_STATE_FOCUS) != 0) && ((iRenderState & XUI_WIDGET_STATE_DISABLED) == 0) && (tResolved.fFocusWidth > 0.0f) && (__xuiToggleColorAlpha(tResolved.iFocusColor) != 0) ) {
		iRet = pProxy->drawRectStroke(pProxy, pDraw, xuiInternalInsetRect(pData->tTrackRect, -2.0f), tResolved.fFocusWidth, __xuiToggleColorWithAlpha(tResolved.iFocusColor, 160));
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	tVisual = __xuiToggleVisual(&tResolved, iVisual);
	if ( (tResolved.pFont != NULL) && (pData->sText != NULL) && (pData->sText[0] != '\0') && (pData->tTextRect.fW > 0.0f) && (__xuiToggleColorAlpha(tVisual.iTextColor) != 0) ) {
		iRet = pProxy->drawText(pProxy, pDraw, tResolved.pFont, pData->sText, pData->tTextRect, tVisual.iTextColor, tResolved.iTextFlags | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
	}
	return iRet;
}

static void __xuiToggleNotify(xui_widget pWidget, xui_toggle_data_t* pData)
{
	pData->iChangeCount++;
	if ( pData->onChange != NULL ) {
		pData->onChange(pWidget, pData->bChecked, pData->pChangeUser);
	}
}

static int __xuiToggleSetCheckedInternal(xui_widget pWidget, xui_toggle_data_t* pData, int bChecked, int bNotify)
{
	bChecked = (bChecked != 0);
	if ( pData->bChecked == bChecked ) {
		return XUI_OK;
	}
	pData->bChecked = bChecked;
	(void)__xuiToggleSyncState(pWidget, pData);
	if ( bNotify ) {
		__xuiToggleNotify(pWidget, pData);
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static void __xuiToggleDefaultAction(xui_widget pWidget, void* pUser)
{
	xui_toggle_data_t* pData;

	(void)pUser;
	pData = __xuiToggleGetData(pWidget);
	if ( pData == NULL ) {
		return;
	}
	(void)__xuiToggleSetCheckedInternal(pWidget, pData, !pData->bChecked, 1);
}

static int __xuiToggleEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_toggle_data_t* pData;
	xui_context pContext;
	int bLeftButton;

	(void)pUser;
	if ( (pWidget == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiToggleGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pContext = xuiWidgetGetContext(pWidget);
	if ( !xuiWidgetGetEnabled(pWidget) || !xuiWidgetGetVisible(pWidget) ) {
		pData->bKeyboardActive = 0;
		(void)__xuiToggleSyncState(pWidget, pData);
		return XUI_OK;
	}
	bLeftButton = (pEvent->iButton == 0) || (pEvent->iButton == XUI_POINTER_BUTTON_LEFT);
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_ENTER:
	case XUI_EVENT_POINTER_LEAVE:
	case XUI_EVENT_POINTER_MOVE:
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
		(void)__xuiToggleSyncState(pWidget, pData);
		return XUI_OK;
	case XUI_EVENT_POINTER_DOWN:
		if ( bLeftButton ) {
			(void)xuiSetPointerCapture(pContext, pWidget);
			(void)__xuiToggleSyncState(pWidget, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_UP:
		if ( bLeftButton ) {
			(void)xuiReleasePointerCapture(pContext, pWidget);
			(void)__xuiToggleSyncState(pWidget, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		pData->bKeyboardActive = 0;
		(void)__xuiToggleSyncState(pWidget, pData);
		return XUI_OK;
	case XUI_EVENT_POINTER_CLICK:
		if ( bLeftButton ) {
			(void)__xuiToggleSetCheckedInternal(pWidget, pData, !pData->bChecked, 1);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_KEY_DOWN:
		if ( pEvent->iKey == XUI_KEY_SPACE ) {
			pData->bKeyboardActive = 1;
			(void)__xuiToggleSyncState(pWidget, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_KEY_UP:
		if ( pEvent->iKey == XUI_KEY_SPACE ) {
			if ( pData->bKeyboardActive ) {
				pData->bKeyboardActive = 0;
				(void)__xuiToggleSetCheckedInternal(pWidget, pData, !pData->bChecked, 1);
			}
			(void)__xuiToggleSyncState(pWidget, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	default:
		break;
	}
	return XUI_OK;
}

static void __xuiToggleDefaultLayout(xui_layout_t* pLayout)
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

static void __xuiToggleDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE | XUI_CACHE_UPDATE_ALL_STATES;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiToggleInitCacheStates(xui_widget pWidget)
{
	static const uint32_t arrState[XUI_TOGGLE_VISUAL_COUNT] = {
		0,
		XUI_WIDGET_STATE_HOVER,
		XUI_WIDGET_STATE_ACTIVE,
		XUI_WIDGET_STATE_FOCUS,
		XUI_WIDGET_STATE_DISABLED,
		XUI_TOGGLE_STATE_CHECKED,
		XUI_TOGGLE_STATE_CHECKED | XUI_WIDGET_STATE_HOVER,
		XUI_TOGGLE_STATE_CHECKED | XUI_WIDGET_STATE_ACTIVE,
		XUI_TOGGLE_STATE_CHECKED | XUI_WIDGET_STATE_FOCUS,
		XUI_TOGGLE_STATE_CHECKED | XUI_WIDGET_STATE_DISABLED
	};
	int i;
	int iRet;

	iRet = xuiWidgetSetCacheStateCount(pWidget, XUI_TOGGLE_VISUAL_COUNT);
	if ( iRet != XUI_OK ) return iRet;
	for ( i = 0; i < XUI_TOGGLE_VISUAL_COUNT; i++ ) {
		iRet = xuiWidgetSetCacheStateId(pWidget, i, 0x73000000u + (uint32_t)i);
		if ( iRet != XUI_OK ) return iRet;
	}
	for ( i = 0; i < XUI_TOGGLE_VISUAL_COUNT; i++ ) {
		iRet = xuiWidgetSetCacheStateId(pWidget, i, arrState[i]);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiToggleInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetDefaultAction(pWidget, __xuiToggleDefaultAction, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_ENTER, __xuiToggleEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiToggleEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiToggleEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiToggleEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiToggleEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CLICK, __xuiToggleEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiToggleEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiToggleEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiToggleEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiToggleEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_UP, __xuiToggleEvent, NULL);
	return iRet;
}

static int __xuiToggleInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_toggle_data_t* pData;
	const xui_toggle_desc_t* pDesc;
	xui_context pContext;
	xui_thickness_t tPadding;
	int iRet;

	(void)pUser;
	pData = (xui_toggle_data_t*)pTypeData;
	pDesc = (const xui_toggle_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiToggleDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pContext = xuiWidgetGetContext(pWidget);
	pData->pFont = (pDesc != NULL && pDesc->pFont != NULL) ? pDesc->pFont : xuiGetDefaultFont(pContext);
	pData->iTextColor = (pDesc != NULL && pDesc->iTextColor != 0) ? pDesc->iTextColor : XUI_COLOR_RGBA(31, 41, 55, 255);
	pData->iDisabledTextColor = (pDesc != NULL && pDesc->iDisabledTextColor != 0) ? pDesc->iDisabledTextColor : XUI_COLOR_RGBA(156, 163, 175, 255);
	pData->iUncheckedTextColor = (pDesc != NULL && pDesc->iUncheckedTextColor != 0) ? pDesc->iUncheckedTextColor : XUI_COLOR_RGBA(112, 126, 140, 255);
	pData->iCheckedTextColor = (pDesc != NULL && pDesc->iCheckedTextColor != 0) ? pDesc->iCheckedTextColor : XUI_COLOR_WHITE;
	pData->iTextFlags = ((pDesc != NULL && pDesc->iTextFlags != 0) ? pDesc->iTextFlags : (XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE)) | XUI_TEXT_CLIP;
	pData->iAccentColor = XUI_COLOR_RGBA(47, 128, 237, 255);
	pData->iAccentHoverColor = XUI_COLOR_RGBA(64, 146, 255, 255);
	pData->iAccentActiveColor = XUI_COLOR_RGBA(31, 111, 214, 255);
	pData->iTrackColor = XUI_COLOR_RGBA(215, 225, 237, 255);
	pData->iTrackHoverColor = XUI_COLOR_RGBA(204, 218, 233, 255);
	pData->iTrackActiveColor = XUI_COLOR_RGBA(190, 207, 226, 255);
	pData->iTrackBorderColor = XUI_COLOR_RGBA(185, 199, 216, 255);
	pData->iThumbColor = XUI_COLOR_WHITE;
	pData->iThumbBorderColor = XUI_COLOR_RGBA(186, 199, 214, 255);
	pData->iFocusColor = XUI_COLOR_RGBA(47, 128, 237, 255);
	pData->iDisabledTrackColor = XUI_COLOR_RGBA(232, 237, 244, 255);
	pData->iDisabledTrackBorderColor = XUI_COLOR_RGBA(214, 221, 230, 255);
	pData->iDisabledThumbColor = XUI_COLOR_RGBA(247, 249, 252, 255);
	pData->iDisabledThumbBorderColor = XUI_COLOR_RGBA(205, 214, 225, 255);
	pData->fTrackWidth = (pDesc != NULL && pDesc->fTrackWidth > 0.0f) ? pDesc->fTrackWidth : 38.0f;
	pData->fTrackHeight = (pDesc != NULL && pDesc->fTrackHeight > 0.0f) ? pDesc->fTrackHeight : 22.0f;
	pData->fThumbSize = (pDesc != NULL && pDesc->fThumbSize > 0.0f) ? pDesc->fThumbSize : 14.0f;
	pData->fGap = (pDesc != NULL && pDesc->fGap > 0.0f) ? pDesc->fGap : 8.0f;
	pData->fFocusWidth = 2.0f;
	pData->fInnerTextPadding = (pDesc != NULL && pDesc->fInnerTextPadding > 0.0f) ? pDesc->fInnerTextPadding : 6.0f;
	pData->fInnerTextGap = (pDesc != NULL && pDesc->fInnerTextGap >= 0.0f) ? pDesc->fInnerTextGap : 2.0f;
	pData->bChecked = (pDesc != NULL) ? (pDesc->bChecked != 0) : 0;
	pData->bUseBuiltinAtlas = (pDesc != NULL) ? (pDesc->bUseBuiltinAtlas != 0) : 0;
	iRet = __xuiToggleTextSet(pData, (pDesc != NULL) ? pDesc->sText : "");
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiToggleInnerTextSet(pData, (pDesc != NULL) ? pDesc->sUncheckedText : "", (pDesc != NULL) ? pDesc->sCheckedText : "");
	if ( iRet != XUI_OK ) return iRet;
	tPadding = (xui_thickness_t){4.0f, 3.0f, 4.0f, 3.0f};
	(void)xuiWidgetSetPadding(pWidget, tPadding);
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	iRet = __xuiToggleInitCacheStates(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiToggleInitEvents(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiToggleSyncState(pWidget, pData);
}

static void __xuiToggleDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_toggle_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_toggle_data_t*)pTypeData;
	if ( pData == NULL ) return;
	if ( pData->sText != NULL ) {
		xrtFree(pData->sText);
	}
	if ( pData->sUncheckedText != NULL ) {
		xrtFree(pData->sUncheckedText);
	}
	if ( pData->sCheckedText != NULL ) {
		xrtFree(pData->sCheckedText);
	}
	memset(pData, 0, sizeof(*pData));
}

static void __xuiToggleRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
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

static void __xuiToggleRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;
	uint32_t iLayoutDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	iLayoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiToggleRegisterStyleProperty(pContext, pType, "choice.text.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiToggleRegisterStyleProperty(pContext, pType, "choice.text.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiToggleRegisterStyleProperty(pContext, pType, "choice.indicator.gap", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiToggleRegisterStyleProperty(pContext, pType, "toggle.inner_text.unchecked_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiToggleRegisterStyleProperty(pContext, pType, "toggle.inner_text.checked_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiToggleRegisterStyleProperty(pContext, pType, "toggle.inner_text.padding", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiToggleRegisterStyleProperty(pContext, pType, "toggle.inner_text.gap", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiToggleRegisterStyleProperty(pContext, pType, "choice.accent.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiToggleRegisterStyleProperty(pContext, pType, "choice.accent.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiToggleRegisterStyleProperty(pContext, pType, "choice.accent.active_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiToggleRegisterStyleProperty(pContext, pType, "choice.focus.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiToggleRegisterStyleProperty(pContext, pType, "choice.focus.width", XUI_STYLE_VALUE_FLOAT, iPaintDirty, 0);
	__xuiToggleRegisterStyleProperty(pContext, pType, "toggle.track.width", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiToggleRegisterStyleProperty(pContext, pType, "toggle.track.height", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiToggleRegisterStyleProperty(pContext, pType, "toggle.thumb.size", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiToggleRegisterStyleProperty(pContext, pType, "toggle.track.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiToggleRegisterStyleProperty(pContext, pType, "toggle.track.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiToggleRegisterStyleProperty(pContext, pType, "toggle.track.active_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiToggleRegisterStyleProperty(pContext, pType, "toggle.track.border_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiToggleRegisterStyleProperty(pContext, pType, "toggle.thumb.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiToggleRegisterStyleProperty(pContext, pType, "toggle.thumb.border_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiToggleRegisterStyleProperty(pContext, pType, "text.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiToggleRegisterStyleProperty(pContext, pType, "text.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiToggleRegisterStyleProperty(pContext, pType, "text.flags", XUI_STYLE_VALUE_INT, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiToggleRegisterStyleProperty(pContext, pType, "font.name", XUI_STYLE_VALUE_STRING, iLayoutDirty, XUI_STYLE_PROPERTY_INHERITED);
}

XUI_API xui_widget_type xuiToggleGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "toggle");
	if ( pType != NULL ) {
		__xuiToggleRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "toggle";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_toggle_data_t);
	tDesc.onInit = __xuiToggleInit;
	tDesc.onDestroy = __xuiToggleDestroy;
	tDesc.onContentMeasure = __xuiToggleContentMeasure;
	tDesc.onCacheRender = __xuiToggleCacheRender;
	__xuiToggleDefaultLayout(&tDesc.tLayout);
	__xuiToggleDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	__xuiToggleRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiToggleCreate(xui_context pContext, xui_widget* ppWidget, const xui_toggle_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiToggleDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiToggleGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiToggleSetChange(xui_widget pWidget, xui_toggle_change_proc onChange, void* pUser)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onChange = onChange;
	pData->pChangeUser = pUser;
	return XUI_OK;
}

XUI_API int xuiToggleSetText(xui_widget pWidget, const char* sText)
{
	xui_toggle_data_t* pData;
	int iRet;

	pData = __xuiToggleGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiToggleTextSet(pData, sText);
	if ( iRet != XUI_OK ) return iRet;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API const char* xuiToggleGetText(xui_widget pWidget)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	return (pData != NULL && pData->sText != NULL) ? pData->sText : "";
}

XUI_API int xuiToggleSetInnerText(xui_widget pWidget, const char* sUncheckedText, const char* sCheckedText)
{
	xui_toggle_data_t* pData;
	int iRet;

	pData = __xuiToggleGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiToggleInnerTextSet(pData, sUncheckedText, sCheckedText);
	if ( iRet != XUI_OK ) return iRet;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API const char* xuiToggleGetUncheckedText(xui_widget pWidget)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	return (pData != NULL && pData->sUncheckedText != NULL) ? pData->sUncheckedText : "";
}

XUI_API const char* xuiToggleGetCheckedText(xui_widget pWidget)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	return (pData != NULL && pData->sCheckedText != NULL) ? pData->sCheckedText : "";
}

XUI_API int xuiToggleSetInnerTextColor(xui_widget pWidget, uint32_t iUncheckedColor, uint32_t iCheckedColor)
{
	xui_toggle_data_t* pData;

	pData = __xuiToggleGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iUncheckedTextColor = iUncheckedColor;
	pData->iCheckedTextColor = iCheckedColor;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiToggleGetInnerTextColor(xui_widget pWidget, uint32_t* pUncheckedColor, uint32_t* pCheckedColor)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pUncheckedColor != NULL ) *pUncheckedColor = pData->iUncheckedTextColor;
	if ( pCheckedColor != NULL ) *pCheckedColor = pData->iCheckedTextColor;
	return XUI_OK;
}

XUI_API int xuiToggleSetInnerTextMetrics(xui_widget pWidget, float fPadding, float fGap)
{
	xui_toggle_data_t* pData;

	pData = __xuiToggleGetData(pWidget);
	if ( (pData == NULL) || (fPadding < 0.0f) || (fGap < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fInnerTextPadding = fPadding;
	pData->fInnerTextGap = fGap;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiToggleGetInnerTextMetrics(xui_widget pWidget, float* pPadding, float* pGap)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pPadding != NULL ) *pPadding = pData->fInnerTextPadding;
	if ( pGap != NULL ) *pGap = pData->fInnerTextGap;
	return XUI_OK;
}

XUI_API int xuiToggleSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pFont = pFont;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_font xuiToggleGetFont(xui_widget pWidget)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API int xuiToggleSetChecked(xui_widget pWidget, int bChecked)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiToggleSetCheckedInternal(pWidget, pData, bChecked, 0);
}

XUI_API int xuiToggleGetChecked(xui_widget pWidget)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	return (pData != NULL) ? pData->bChecked : 0;
}

XUI_API int xuiToggleSetTextColor(xui_widget pWidget, uint32_t iColor)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iTextColor = iColor;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiToggleGetTextColor(xui_widget pWidget)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	return (pData != NULL) ? pData->iTextColor : 0;
}

XUI_API int xuiToggleSetDisabledTextColor(xui_widget pWidget, uint32_t iColor)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iDisabledTextColor = iColor;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiToggleGetDisabledTextColor(xui_widget pWidget)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	return (pData != NULL) ? pData->iDisabledTextColor : 0;
}

XUI_API int xuiToggleSetTrackSize(xui_widget pWidget, float fWidth, float fHeight)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	if ( (pData == NULL) || (fWidth <= 0.0f) || (fHeight <= 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fTrackWidth = fWidth;
	pData->fTrackHeight = fHeight;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiToggleGetTrackWidth(xui_widget pWidget)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	return (pData != NULL) ? pData->fTrackWidth : 0.0f;
}

XUI_API float xuiToggleGetTrackHeight(xui_widget pWidget)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	return (pData != NULL) ? pData->fTrackHeight : 0.0f;
}

XUI_API int xuiToggleSetThumbSize(xui_widget pWidget, float fSize)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	if ( (pData == NULL) || (fSize <= 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fThumbSize = fSize;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiToggleGetThumbSize(xui_widget pWidget)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	return (pData != NULL) ? pData->fThumbSize : 0.0f;
}

XUI_API int xuiToggleSetGap(xui_widget pWidget, float fGap)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	if ( (pData == NULL) || (fGap < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fGap = fGap;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiToggleGetGap(xui_widget pWidget)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	return (pData != NULL) ? pData->fGap : 0.0f;
}

XUI_API int xuiToggleSetColors(xui_widget pWidget, uint32_t iAccent, uint32_t iTrack, uint32_t iHoverTrack, uint32_t iFocus)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iAccentColor = iAccent;
	pData->iAccentHoverColor = iAccent;
	pData->iAccentActiveColor = iAccent;
	pData->iTrackColor = iTrack;
	pData->iTrackHoverColor = iHoverTrack;
	pData->iFocusColor = iFocus;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiToggleGetColors(xui_widget pWidget, uint32_t* pAccent, uint32_t* pTrack, uint32_t* pHoverTrack, uint32_t* pFocus)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pAccent != NULL ) *pAccent = pData->iAccentColor;
	if ( pTrack != NULL ) *pTrack = pData->iTrackColor;
	if ( pHoverTrack != NULL ) *pHoverTrack = pData->iTrackHoverColor;
	if ( pFocus != NULL ) *pFocus = pData->iFocusColor;
	return XUI_OK;
}

XUI_API int xuiToggleSetIndicatorSurface(xui_widget pWidget, xui_surface pUncheckedSurface, xui_rect_t tUncheckedSrc, xui_surface pCheckedSurface, xui_rect_t tCheckedSrc)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pUncheckedSurface = pUncheckedSurface;
	pData->tUncheckedSrc = tUncheckedSrc;
	pData->pCheckedSurface = pCheckedSurface;
	pData->tCheckedSrc = tCheckedSrc;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiToggleGetIndicatorSurface(xui_widget pWidget, xui_surface* ppUncheckedSurface, xui_rect_t* pUncheckedSrc, xui_surface* ppCheckedSurface, xui_rect_t* pCheckedSrc)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( ppUncheckedSurface != NULL ) *ppUncheckedSurface = pData->pUncheckedSurface;
	if ( pUncheckedSrc != NULL ) *pUncheckedSrc = pData->tUncheckedSrc;
	if ( ppCheckedSurface != NULL ) *ppCheckedSurface = pData->pCheckedSurface;
	if ( pCheckedSrc != NULL ) *pCheckedSrc = pData->tCheckedSrc;
	return XUI_OK;
}

XUI_API int xuiToggleUseBuiltinAtlas(xui_widget pWidget, int bEnable)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bUseBuiltinAtlas = (bEnable != 0);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiToggleGetUseBuiltinAtlas(xui_widget pWidget)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	return (pData != NULL) ? pData->bUseBuiltinAtlas : 0;
}

XUI_API xui_rect_t xuiToggleGetTrackRect(xui_widget pWidget)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	return (pData != NULL) ? pData->tTrackRect : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API xui_rect_t xuiToggleGetThumbRect(xui_widget pWidget)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	return (pData != NULL) ? pData->tThumbRect : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API xui_rect_t xuiToggleGetTextRect(xui_widget pWidget)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	return (pData != NULL) ? pData->tTextRect : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API xui_rect_t xuiToggleGetInnerTextRect(xui_widget pWidget)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	return (pData != NULL) ? pData->tInnerTextRect : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API uint32_t xuiToggleGetState(xui_widget pWidget)
{
	xui_toggle_data_t* pData = __xuiToggleGetData(pWidget);
	return (pData != NULL) ? __xuiToggleComputeState(pWidget, pData) : 0;
}
