#include "xui_internal.h"

#include <string.h>

#define XUI_BUTTON_VISUAL_NORMAL	0
#define XUI_BUTTON_VISUAL_HOVER		1
#define XUI_BUTTON_VISUAL_ACTIVE	2
#define XUI_BUTTON_VISUAL_FOCUS		3
#define XUI_BUTTON_VISUAL_DISABLED	4
#define XUI_BUTTON_VISUAL_CHECKED	5
#define XUI_BUTTON_VISUAL_COUNT		6

#define XUI_BUTTON_CACHE_STATE_COUNT	XUI_BUTTON_VISUAL_COUNT

typedef struct xui_button_visual_t {
	uint32_t iFillColor;
	uint32_t iBorderColor;
	float fBorderWidth;
} xui_button_visual_t;

typedef struct xui_button_data_t {
	char* sText;
	int iTextCapacity;
	xui_font pFont;
	xui_button_click_proc onClick;
	void* pClickUser;
	uint32_t iTextColor;
	uint32_t iDisabledTextColor;
	uint32_t iTextFlags;
	uint32_t iIconColor;
	xui_button_visual_t arrVisual[XUI_BUTTON_VISUAL_COUNT];
	int iSemantic;
	int bSelectable;
	int bSelected;
	int bKeyboardActive;
	int iClickCount;
	xui_surface pIconSurface;
	xui_rect_t tIconSrc;
	int iIconPlacement;
	float fIconSize;
	float fIconGap;
	xui_nine_patch_t arrPatch[XUI_BUTTON_VISUAL_COUNT];
	int arrHasPatch[XUI_BUTTON_VISUAL_COUNT];
	int bBadgeVisible;
	int iBadgeAnchor;
	float fBadgeOffsetX;
	float fBadgeOffsetY;
	float fBadgeSize;
	xui_surface pBadgeSurface;
	xui_rect_t tBadgeSrc;
} xui_button_data_t;

typedef struct xui_button_resolved_t {
	xui_font pFont;
	uint32_t iTextColor;
	uint32_t iDisabledTextColor;
	uint32_t iTextFlags;
	uint32_t iIconColor;
	xui_button_visual_t arrVisual[XUI_BUTTON_VISUAL_COUNT];
	int iIconPlacement;
	float fIconSize;
	float fIconGap;
} xui_button_resolved_t;

typedef struct xui_button_content_t {
	xui_rect_t tIconRect;
	xui_rect_t tTextRect;
	xui_rect_t tGroupRect;
} xui_button_content_t;

static uint32_t __xuiButtonColorAlpha(uint32_t iColor)
{
	return iColor & 0x000000ffu;
}

static uint32_t __xuiButtonColorWithAlpha(uint32_t iColor, uint32_t iAlpha)
{
	return (iColor & 0xffffff00u) | (iAlpha & 0xffu);
}

static uint32_t __xuiButtonBlend(uint32_t iA, uint32_t iB, int iStep, int iTotal)
{
	int r;
	int g;
	int b;
	int a;

	if ( iTotal <= 0 ) {
		return iA;
	}
	r = (int)((iA >> 24) & 0xffu) + ((int)((iB >> 24) & 0xffu) - (int)((iA >> 24) & 0xffu)) * iStep / iTotal;
	g = (int)((iA >> 16) & 0xffu) + ((int)((iB >> 16) & 0xffu) - (int)((iA >> 16) & 0xffu)) * iStep / iTotal;
	b = (int)((iA >> 8) & 0xffu) + ((int)((iB >> 8) & 0xffu) - (int)((iA >> 8) & 0xffu)) * iStep / iTotal;
	a = (int)(iA & 0xffu) + ((int)(iB & 0xffu) - (int)(iA & 0xffu)) * iStep / iTotal;
	return XUI_COLOR_RGBA(r, g, b, a);
}

static int __xuiButtonIconPlacementValid(int iPlacement)
{
	return (iPlacement == XUI_BUTTON_ICON_LEFT) ||
	       (iPlacement == XUI_BUTTON_ICON_RIGHT) ||
	       (iPlacement == XUI_BUTTON_ICON_TOP) ||
	       (iPlacement == XUI_BUTTON_ICON_BOTTOM);
}

static int __xuiButtonBadgeAnchorValid(int iAnchor)
{
	return (iAnchor == XUI_BUTTON_BADGE_CONTENT_TOP_RIGHT) ||
	       (iAnchor == XUI_BUTTON_BADGE_WIDGET_TOP_RIGHT) ||
	       (iAnchor == XUI_BUTTON_BADGE_ICON_TOP_RIGHT) ||
	       (iAnchor == XUI_BUTTON_BADGE_TEXT_TOP_RIGHT);
}

static int __xuiButtonDescValid(const xui_button_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) {
		return 0;
	}
	if ( pDesc->fBorderWidth < 0.0f ) {
		return 0;
	}
	return 1;
}

static int __xuiButtonPatchValid(const xui_nine_patch_t* pPatch)
{
	if ( pPatch == NULL ) {
		return 1;
	}
	if ( (pPatch->iSize != 0) && (pPatch->iSize < sizeof(*pPatch)) ) {
		return 0;
	}
	if ( (pPatch->pSurface == NULL) ||
	     (pPatch->tSrc.fW < 0.0f) ||
	     (pPatch->tSrc.fH < 0.0f) ||
	     (pPatch->tSlice.fLeft < 0.0f) ||
	     (pPatch->tSlice.fTop < 0.0f) ||
	     (pPatch->tSlice.fRight < 0.0f) ||
	     (pPatch->tSlice.fBottom < 0.0f) ) {
		return 0;
	}
	if ( (pPatch->iMode != XUI_NINE_PATCH_STRETCH) &&
	     (pPatch->iMode != XUI_NINE_PATCH_TILE) ) {
		return 0;
	}
	return 1;
}

static int __xuiButtonTextSet(xui_button_data_t* pData, const char* sText)
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
		sNew = (char*)xrtRealloc(pData->sText, (size_t)iNeed);
		if ( sNew == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		pData->sText = sNew;
		pData->iTextCapacity = iNeed;
	}
	memcpy(pData->sText, sText, (size_t)iNeed);
	return XUI_OK;
}

static xui_button_data_t* __xuiButtonGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return NULL;
	}
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "button");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_button_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiButtonStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
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

static int __xuiButtonStyleInt(xui_widget pWidget, const char* sName, int* pValue)
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

static int __xuiButtonStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
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

static xui_font __xuiButtonStyleFont(xui_widget pWidget, xui_font pBaseFont)
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

static int __xuiButtonStateIndex(uint32_t iState)
{
	if ( (iState & XUI_WIDGET_STATE_DISABLED) != 0 ) {
		return XUI_BUTTON_VISUAL_DISABLED;
	}
	if ( (iState & XUI_WIDGET_STATE_ACTIVE) != 0 ) {
		return XUI_BUTTON_VISUAL_ACTIVE;
	}
	if ( (iState & XUI_WIDGET_STATE_HOVER) != 0 ) {
		return XUI_BUTTON_VISUAL_HOVER;
	}
	if ( (iState & XUI_BUTTON_STATE_CHECKED) != 0 ) {
		return XUI_BUTTON_VISUAL_CHECKED;
	}
	if ( (iState & XUI_WIDGET_STATE_FOCUS) != 0 ) {
		return XUI_BUTTON_VISUAL_FOCUS;
	}
	return XUI_BUTTON_VISUAL_NORMAL;
}

static uint32_t __xuiButtonVisualStateId(uint32_t iState)
{
	switch ( __xuiButtonStateIndex(iState) ) {
	case XUI_BUTTON_VISUAL_HOVER:
		return XUI_WIDGET_STATE_HOVER;
	case XUI_BUTTON_VISUAL_ACTIVE:
		return XUI_WIDGET_STATE_ACTIVE;
	case XUI_BUTTON_VISUAL_FOCUS:
		return XUI_WIDGET_STATE_FOCUS;
	case XUI_BUTTON_VISUAL_DISABLED:
		return XUI_WIDGET_STATE_DISABLED;
	case XUI_BUTTON_VISUAL_CHECKED:
		return XUI_BUTTON_STATE_CHECKED;
	default:
		return 0;
	}
}

static int __xuiButtonPatchIndex(xui_button_data_t* pData, uint32_t iState)
{
	if ( pData == NULL ) {
		return -1;
	}
	if ( ((iState & XUI_WIDGET_STATE_DISABLED) != 0) && pData->arrHasPatch[XUI_BUTTON_VISUAL_DISABLED] ) {
		return XUI_BUTTON_VISUAL_DISABLED;
	}
	if ( ((iState & XUI_WIDGET_STATE_ACTIVE) != 0) && pData->arrHasPatch[XUI_BUTTON_VISUAL_ACTIVE] ) {
		return XUI_BUTTON_VISUAL_ACTIVE;
	}
	if ( ((iState & XUI_BUTTON_STATE_CHECKED) != 0) && pData->arrHasPatch[XUI_BUTTON_VISUAL_CHECKED] ) {
		return XUI_BUTTON_VISUAL_CHECKED;
	}
	if ( ((iState & XUI_WIDGET_STATE_HOVER) != 0) && pData->arrHasPatch[XUI_BUTTON_VISUAL_HOVER] ) {
		return XUI_BUTTON_VISUAL_HOVER;
	}
	if ( ((iState & XUI_WIDGET_STATE_FOCUS) != 0) && pData->arrHasPatch[XUI_BUTTON_VISUAL_FOCUS] ) {
		return XUI_BUTTON_VISUAL_FOCUS;
	}
	return pData->arrHasPatch[XUI_BUTTON_VISUAL_NORMAL] ? XUI_BUTTON_VISUAL_NORMAL : -1;
}

static uint32_t __xuiButtonComputeState(xui_widget pWidget, xui_button_data_t* pData)
{
	uint32_t iState;

	iState = xuiWidgetGetInputState(pWidget);
	if ( (pData != NULL) && pData->bKeyboardActive ) {
		iState |= XUI_WIDGET_STATE_ACTIVE;
	}
	if ( (pData != NULL) && pData->bSelected ) {
		iState |= XUI_BUTTON_STATE_CHECKED;
	}
	return iState;
}

static int __xuiButtonSyncState(xui_widget pWidget, xui_button_data_t* pData)
{
	uint32_t iState;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iState = __xuiButtonComputeState(pWidget, pData);
	return xuiWidgetSetStateId(pWidget, __xuiButtonVisualStateId(iState));
}

static void __xuiButtonApplyThemeDefaults(xui_button_data_t* pData, const xui_theme_t* pTheme, const xui_button_desc_t* pDesc)
{
	uint32_t iNormal;
	uint32_t iHover;
	uint32_t iActive;
	uint32_t iFocus;
	uint32_t iDisabled;
	uint32_t iChecked;
	uint32_t iBorder;
	float fBorderWidth;

	iNormal = (pDesc != NULL && pDesc->iNormalColor != 0) ? pDesc->iNormalColor : pTheme->iStateNormalColor;
	iHover = (pDesc != NULL && pDesc->iHoverColor != 0) ? pDesc->iHoverColor : pTheme->iStateHoverColor;
	iActive = (pDesc != NULL && pDesc->iActiveColor != 0) ? pDesc->iActiveColor : pTheme->iStateActiveColor;
	iFocus = (pDesc != NULL && pDesc->iFocusColor != 0) ? pDesc->iFocusColor : pTheme->iStateFocusColor;
	iDisabled = (pDesc != NULL && pDesc->iDisabledColor != 0) ? pDesc->iDisabledColor : pTheme->iStateDisabledColor;
	iChecked = (pDesc != NULL && pDesc->iCheckedColor != 0) ? pDesc->iCheckedColor : pTheme->iAccentColor;
	iBorder = (pDesc != NULL && pDesc->iBorderColor != 0) ? pDesc->iBorderColor : pTheme->iBorderColor;
	fBorderWidth = (pDesc != NULL && pDesc->fBorderWidth > 0.0f) ? pDesc->fBorderWidth : pTheme->fBorderWidth;

	pData->arrVisual[XUI_BUTTON_VISUAL_NORMAL].iFillColor = iNormal;
	pData->arrVisual[XUI_BUTTON_VISUAL_HOVER].iFillColor = iHover;
	pData->arrVisual[XUI_BUTTON_VISUAL_ACTIVE].iFillColor = iActive;
	pData->arrVisual[XUI_BUTTON_VISUAL_FOCUS].iFillColor = iNormal;
	pData->arrVisual[XUI_BUTTON_VISUAL_DISABLED].iFillColor = iDisabled;
	pData->arrVisual[XUI_BUTTON_VISUAL_CHECKED].iFillColor = iChecked;
	pData->arrVisual[XUI_BUTTON_VISUAL_NORMAL].iBorderColor = iBorder;
	pData->arrVisual[XUI_BUTTON_VISUAL_HOVER].iBorderColor = iBorder;
	pData->arrVisual[XUI_BUTTON_VISUAL_ACTIVE].iBorderColor = __xuiButtonBlend(iBorder, XUI_COLOR_RGBA(0, 0, 0, 255), 1, 4);
	pData->arrVisual[XUI_BUTTON_VISUAL_FOCUS].iBorderColor = iFocus;
	pData->arrVisual[XUI_BUTTON_VISUAL_DISABLED].iBorderColor = __xuiButtonColorWithAlpha(iBorder, 128);
	pData->arrVisual[XUI_BUTTON_VISUAL_CHECKED].iBorderColor = iFocus;
	pData->arrVisual[XUI_BUTTON_VISUAL_NORMAL].fBorderWidth = fBorderWidth;
	pData->arrVisual[XUI_BUTTON_VISUAL_HOVER].fBorderWidth = fBorderWidth;
	pData->arrVisual[XUI_BUTTON_VISUAL_ACTIVE].fBorderWidth = fBorderWidth;
	pData->arrVisual[XUI_BUTTON_VISUAL_FOCUS].fBorderWidth = fBorderWidth;
	pData->arrVisual[XUI_BUTTON_VISUAL_DISABLED].fBorderWidth = fBorderWidth;
	pData->arrVisual[XUI_BUTTON_VISUAL_CHECKED].fBorderWidth = fBorderWidth;
}

static void __xuiButtonResolve(xui_widget pWidget, xui_button_data_t* pData, xui_button_resolved_t* pResolved)
{
	int iPlacement;
	int iTextFlags;

	memset(pResolved, 0, sizeof(*pResolved));
	pResolved->pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	pResolved->iTextColor = pData->iTextColor;
	pResolved->iDisabledTextColor = pData->iDisabledTextColor;
	pResolved->iTextFlags = pData->iTextFlags | XUI_TEXT_CLIP;
	pResolved->iIconColor = pData->iIconColor;
	memcpy(pResolved->arrVisual, pData->arrVisual, sizeof(pResolved->arrVisual));
	pResolved->iIconPlacement = pData->iIconPlacement;
	pResolved->fIconSize = pData->fIconSize;
	pResolved->fIconGap = pData->fIconGap;

	(void)__xuiButtonStyleColor(pWidget, "button.text_color", &pResolved->iTextColor);
	(void)__xuiButtonStyleColor(pWidget, "text.color", &pResolved->iTextColor);
	(void)__xuiButtonStyleColor(pWidget, "button.disabled_text_color", &pResolved->iDisabledTextColor);
	(void)__xuiButtonStyleColor(pWidget, "text.disabled_color", &pResolved->iDisabledTextColor);
	iTextFlags = (int)pResolved->iTextFlags;
	if ( __xuiButtonStyleInt(pWidget, "text.flags", &iTextFlags) ) {
		pResolved->iTextFlags = (uint32_t)iTextFlags | XUI_TEXT_CLIP;
	}
	(void)__xuiButtonStyleColor(pWidget, "button.icon_color", &pResolved->iIconColor);
	(void)__xuiButtonStyleFloat(pWidget, "button.icon_size", &pResolved->fIconSize);
	(void)__xuiButtonStyleFloat(pWidget, "button.icon_gap", &pResolved->fIconGap);
	iPlacement = pResolved->iIconPlacement;
	if ( __xuiButtonStyleInt(pWidget, "button.icon_placement", &iPlacement) && __xuiButtonIconPlacementValid(iPlacement) ) {
		pResolved->iIconPlacement = iPlacement;
	}

	(void)__xuiButtonStyleColor(pWidget, "button.normal_color", &pResolved->arrVisual[XUI_BUTTON_VISUAL_NORMAL].iFillColor);
	(void)__xuiButtonStyleColor(pWidget, "button.hover_color", &pResolved->arrVisual[XUI_BUTTON_VISUAL_HOVER].iFillColor);
	(void)__xuiButtonStyleColor(pWidget, "button.active_color", &pResolved->arrVisual[XUI_BUTTON_VISUAL_ACTIVE].iFillColor);
	(void)__xuiButtonStyleColor(pWidget, "button.focus_color", &pResolved->arrVisual[XUI_BUTTON_VISUAL_FOCUS].iBorderColor);
	(void)__xuiButtonStyleColor(pWidget, "button.disabled_color", &pResolved->arrVisual[XUI_BUTTON_VISUAL_DISABLED].iFillColor);
	(void)__xuiButtonStyleColor(pWidget, "button.checked_color", &pResolved->arrVisual[XUI_BUTTON_VISUAL_CHECKED].iFillColor);
	(void)__xuiButtonStyleColor(pWidget, "button.border_color", &pResolved->arrVisual[XUI_BUTTON_VISUAL_NORMAL].iBorderColor);
	(void)__xuiButtonStyleFloat(pWidget, "button.border_width", &pResolved->arrVisual[XUI_BUTTON_VISUAL_NORMAL].fBorderWidth);
	(void)__xuiButtonStyleColor(pWidget, "button.hover_border_color", &pResolved->arrVisual[XUI_BUTTON_VISUAL_HOVER].iBorderColor);
	(void)__xuiButtonStyleFloat(pWidget, "button.hover_border_width", &pResolved->arrVisual[XUI_BUTTON_VISUAL_HOVER].fBorderWidth);
	(void)__xuiButtonStyleColor(pWidget, "button.active_border_color", &pResolved->arrVisual[XUI_BUTTON_VISUAL_ACTIVE].iBorderColor);
	(void)__xuiButtonStyleFloat(pWidget, "button.active_border_width", &pResolved->arrVisual[XUI_BUTTON_VISUAL_ACTIVE].fBorderWidth);
	(void)__xuiButtonStyleColor(pWidget, "button.disabled_border_color", &pResolved->arrVisual[XUI_BUTTON_VISUAL_DISABLED].iBorderColor);
	(void)__xuiButtonStyleFloat(pWidget, "button.disabled_border_width", &pResolved->arrVisual[XUI_BUTTON_VISUAL_DISABLED].fBorderWidth);
	(void)__xuiButtonStyleColor(pWidget, "button.checked_border_color", &pResolved->arrVisual[XUI_BUTTON_VISUAL_CHECKED].iBorderColor);
	(void)__xuiButtonStyleFloat(pWidget, "button.checked_border_width", &pResolved->arrVisual[XUI_BUTTON_VISUAL_CHECKED].fBorderWidth);
	(void)__xuiButtonStyleFloat(pWidget, "button.focus_border_width", &pResolved->arrVisual[XUI_BUTTON_VISUAL_FOCUS].fBorderWidth);
	pResolved->pFont = __xuiButtonStyleFont(pWidget, pResolved->pFont);

	if ( pResolved->fIconSize < 0.0f ) {
		pResolved->fIconSize = 0.0f;
	}
	if ( pResolved->fIconGap < 0.0f ) {
		pResolved->fIconGap = 0.0f;
	}
}

static xui_vec2_t __xuiButtonMeasureText(xui_widget pWidget, xui_font pFont, const char* sText)
{
	xui_vec2_t tSize;
	xui_proxy pProxy;

	tSize.fX = 0.0f;
	tSize.fY = 0.0f;
	if ( (pFont == NULL) || (sText == NULL) || (sText[0] == '\0') ) {
		return tSize;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( (pProxy != NULL) && (pProxy->textMeasure != NULL) &&
	     (pProxy->textMeasure(pProxy, pFont, sText, &tSize) == XUI_OK) &&
	     (tSize.fX >= 0.0f) && (tSize.fY >= 0.0f) ) {
		return tSize;
	}
	tSize.fX = (float)strlen(sText) * 7.0f;
	tSize.fY = 18.0f;
	return tSize;
}

static int __xuiButtonContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_button_data_t* pData;
	xui_button_resolved_t tResolved;
	xui_vec2_t tTextSize;
	float fGap;
	int bHasText;
	int bHasIcon;
	int bVertical;

	(void)tConstraint;
	(void)pUser;
	if ( (pWidget == NULL) || (pSize == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pSize->fX = 0.0f;
	pSize->fY = 0.0f;
	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiButtonResolve(pWidget, pData, &tResolved);
	bHasText = (pData->sText != NULL) && (pData->sText[0] != '\0') && (tResolved.pFont != NULL);
	bHasIcon = (pData->pIconSurface != NULL) && (tResolved.fIconSize > 0.0f);
	tTextSize = __xuiButtonMeasureText(pWidget, tResolved.pFont, bHasText ? pData->sText : "");
	if ( bHasText ) {
		pSize->fX = tTextSize.fX;
		pSize->fY = tTextSize.fY;
	}
	if ( bHasIcon ) {
		if ( !bHasText ) {
			pSize->fX = tResolved.fIconSize;
			pSize->fY = tResolved.fIconSize;
		} else {
			fGap = tResolved.fIconGap;
			bVertical = (tResolved.iIconPlacement == XUI_BUTTON_ICON_TOP) ||
			            (tResolved.iIconPlacement == XUI_BUTTON_ICON_BOTTOM);
			if ( bVertical ) {
				pSize->fX = (tTextSize.fX > tResolved.fIconSize) ? tTextSize.fX : tResolved.fIconSize;
				pSize->fY = tResolved.fIconSize + fGap + tTextSize.fY;
			} else {
				pSize->fX = tResolved.fIconSize + fGap + tTextSize.fX;
				pSize->fY = (tTextSize.fY > tResolved.fIconSize) ? tTextSize.fY : tResolved.fIconSize;
			}
		}
	}
	return XUI_OK;
}

static xui_rect_t __xuiButtonSurfaceSrc(xui_proxy pProxy, xui_surface pSurface, xui_rect_t tSrc)
{
	xui_surface_desc_t tDesc;

	if ( (tSrc.fW > 0.0f) && (tSrc.fH > 0.0f) ) {
		return tSrc;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	if ( (pProxy != NULL) &&
	     (pProxy->surfaceGetDesc != NULL) &&
	     (pProxy->surfaceGetDesc(pProxy, pSurface, &tDesc) == XUI_OK) ) {
		tSrc.fX = 0.0f;
		tSrc.fY = 0.0f;
		tSrc.fW = (float)tDesc.iWidth;
		tSrc.fH = (float)tDesc.iHeight;
	}
	return tSrc;
}

static void __xuiButtonScaleFixed(float fTotal, float* pA, float* pB, float* pMid)
{
	float fSum;
	float fScale;

	*pMid = fTotal - *pA - *pB;
	if ( *pMid >= 0.0f ) {
		return;
	}
	fSum = *pA + *pB;
	if ( fSum <= 0.0f ) {
		*pA = 0.0f;
		*pB = 0.0f;
		*pMid = fTotal;
		return;
	}
	fScale = fTotal / fSum;
	*pA *= fScale;
	*pB *= fScale;
	*pMid = 0.0f;
}

static int __xuiButtonDrawSurfacePiece(xui_proxy pProxy, xui_draw_context pDraw, xui_surface pSurface, xui_rect_t tSrc, xui_rect_t tDst, uint32_t iColor)
{
	if ( (tSrc.fW <= 0.0f) || (tSrc.fH <= 0.0f) || (tDst.fW <= 0.0f) || (tDst.fH <= 0.0f) ||
	     (__xuiButtonColorAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	return pProxy->drawSurface(pProxy, pDraw, pSurface, tSrc, tDst, iColor, 0);
}

static int __xuiButtonDrawTilePiece(xui_proxy pProxy, xui_draw_context pDraw, xui_surface pSurface, xui_rect_t tSrc, xui_rect_t tDst, uint32_t iColor)
{
	xui_rect_t tTileSrc;
	xui_rect_t tTileDst;
	float fX;
	float fY;
	float fW;
	float fH;
	int iRet;

	if ( (tSrc.fW <= 0.0f) || (tSrc.fH <= 0.0f) || (tDst.fW <= 0.0f) || (tDst.fH <= 0.0f) ) {
		return XUI_OK;
	}
	for ( fY = 0.0f; fY < tDst.fH; fY += tSrc.fH ) {
		fH = tSrc.fH;
		if ( fY + fH > tDst.fH ) {
			fH = tDst.fH - fY;
		}
		for ( fX = 0.0f; fX < tDst.fW; fX += tSrc.fW ) {
			fW = tSrc.fW;
			if ( fX + fW > tDst.fW ) {
				fW = tDst.fW - fX;
			}
			tTileSrc = tSrc;
			tTileDst = tDst;
			tTileSrc.fW = fW;
			tTileSrc.fH = fH;
			tTileDst.fX += fX;
			tTileDst.fY += fY;
			tTileDst.fW = fW;
			tTileDst.fH = fH;
			iRet = __xuiButtonDrawSurfacePiece(pProxy, pDraw, pSurface, tTileSrc, tTileDst, iColor);
			if ( iRet != XUI_OK ) {
				return iRet;
			}
		}
	}
	return XUI_OK;
}

static int __xuiButtonDrawNinePatch(xui_proxy pProxy, xui_draw_context pDraw, const xui_nine_patch_t* pPatch, xui_rect_t tDst)
{
	xui_rect_t tSrc;
	xui_rect_t arrSrc[9];
	xui_rect_t arrDst[9];
	float arrSrcX[4];
	float arrSrcY[4];
	float arrDstX[4];
	float arrDstY[4];
	float fLeft;
	float fTop;
	float fRight;
	float fBottom;
	float fMidW;
	float fMidH;
	int x;
	int y;
	int iIndex;
	int bTile;
	int iRet;

	if ( (pProxy == NULL) || (pDraw == NULL) || (pPatch == NULL) || (pPatch->pSurface == NULL) ||
	     (tDst.fW <= 0.0f) || (tDst.fH <= 0.0f) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	tSrc = __xuiButtonSurfaceSrc(pProxy, pPatch->pSurface, pPatch->tSrc);
	if ( (tSrc.fW <= 0.0f) || (tSrc.fH <= 0.0f) ) {
		return XUI_OK;
	}
	fLeft = pPatch->tSlice.fLeft;
	fTop = pPatch->tSlice.fTop;
	fRight = pPatch->tSlice.fRight;
	fBottom = pPatch->tSlice.fBottom;
	if ( fLeft + fRight > tSrc.fW ) {
		fRight = tSrc.fW - fLeft;
		if ( fRight < 0.0f ) {
			fRight = 0.0f;
			fLeft = tSrc.fW;
		}
	}
	if ( fTop + fBottom > tSrc.fH ) {
		fBottom = tSrc.fH - fTop;
		if ( fBottom < 0.0f ) {
			fBottom = 0.0f;
			fTop = tSrc.fH;
		}
	}
	__xuiButtonScaleFixed(tDst.fW, &fLeft, &fRight, &fMidW);
	__xuiButtonScaleFixed(tDst.fH, &fTop, &fBottom, &fMidH);
	arrSrcX[0] = tSrc.fX;
	arrSrcX[1] = tSrc.fX + pPatch->tSlice.fLeft;
	arrSrcX[2] = tSrc.fX + tSrc.fW - pPatch->tSlice.fRight;
	arrSrcX[3] = tSrc.fX + tSrc.fW;
	arrSrcY[0] = tSrc.fY;
	arrSrcY[1] = tSrc.fY + pPatch->tSlice.fTop;
	arrSrcY[2] = tSrc.fY + tSrc.fH - pPatch->tSlice.fBottom;
	arrSrcY[3] = tSrc.fY + tSrc.fH;
	arrDstX[0] = tDst.fX;
	arrDstX[1] = tDst.fX + fLeft;
	arrDstX[2] = tDst.fX + tDst.fW - fRight;
	arrDstX[3] = tDst.fX + tDst.fW;
	arrDstY[0] = tDst.fY;
	arrDstY[1] = tDst.fY + fTop;
	arrDstY[2] = tDst.fY + tDst.fH - fBottom;
	arrDstY[3] = tDst.fY + tDst.fH;
	for ( y = 0; y < 3; y++ ) {
		for ( x = 0; x < 3; x++ ) {
			iIndex = y * 3 + x;
			arrSrc[iIndex] = (xui_rect_t){arrSrcX[x], arrSrcY[y], arrSrcX[x + 1] - arrSrcX[x], arrSrcY[y + 1] - arrSrcY[y]};
			arrDst[iIndex] = (xui_rect_t){arrDstX[x], arrDstY[y], arrDstX[x + 1] - arrDstX[x], arrDstY[y + 1] - arrDstY[y]};
		}
	}
	for ( iIndex = 0; iIndex < 9; iIndex++ ) {
		bTile = (pPatch->iMode == XUI_NINE_PATCH_TILE) && (iIndex != 0) && (iIndex != 2) && (iIndex != 6) && (iIndex != 8);
		if ( bTile ) {
			iRet = __xuiButtonDrawTilePiece(pProxy, pDraw, pPatch->pSurface, arrSrc[iIndex], arrDst[iIndex], pPatch->iColor);
		} else {
			iRet = __xuiButtonDrawSurfacePiece(pProxy, pDraw, pPatch->pSurface, arrSrc[iIndex], arrDst[iIndex], pPatch->iColor);
		}
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return XUI_OK;
}

static xui_rect_t __xuiButtonZeroRect(void)
{
	return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

static void __xuiButtonLayoutContent(xui_widget pWidget, xui_button_data_t* pData, const xui_button_resolved_t* pResolved, xui_button_content_t* pContent)
{
	xui_rect_t tContent;
	xui_vec2_t tTextSize;
	float fIconSize;
	float fTextW;
	float fTextH;
	float fGap;
	float fGroupW;
	float fGroupH;
	float fX;
	float fY;
	int bHasText;
	int bHasIcon;
	int bVertical;

	memset(pContent, 0, sizeof(*pContent));
	tContent = xuiWidgetGetContentRect(pWidget);
	if ( (tContent.fW <= 0.0f) || (tContent.fH <= 0.0f) ) {
		tContent = xuiWidgetGetRect(pWidget);
		tContent.fX = 0.0f;
		tContent.fY = 0.0f;
	}
	pContent->tIconRect = __xuiButtonZeroRect();
	pContent->tTextRect = tContent;
	pContent->tGroupRect = tContent;
	bHasText = (pData->sText != NULL) && (pData->sText[0] != '\0') && (pResolved->pFont != NULL);
	bHasIcon = (pData->pIconSurface != NULL) && (pResolved->fIconSize > 0.0f);
	if ( !bHasText && !bHasIcon ) {
		pContent->tGroupRect = (xui_rect_t){tContent.fX + tContent.fW * 0.5f, tContent.fY + tContent.fH * 0.5f, 0.0f, 0.0f};
		return;
	}
	fIconSize = pResolved->fIconSize;
	if ( fIconSize > tContent.fW ) {
		fIconSize = tContent.fW;
	}
	if ( fIconSize > tContent.fH ) {
		fIconSize = tContent.fH;
	}
	fTextW = 0.0f;
	fTextH = 0.0f;
	if ( bHasText ) {
		tTextSize = __xuiButtonMeasureText(pWidget, pResolved->pFont, pData->sText);
		fTextW = tTextSize.fX;
		fTextH = (tTextSize.fY > 0.0f) ? tTextSize.fY : tContent.fH;
	}
	if ( !bHasIcon ) {
		pContent->tTextRect = tContent;
		pContent->tGroupRect = tContent;
		return;
	}
	if ( !bHasText ) {
		pContent->tIconRect = xuiInternalSnapRect((xui_rect_t){tContent.fX + (tContent.fW - fIconSize) * 0.5f, tContent.fY + (tContent.fH - fIconSize) * 0.5f, fIconSize, fIconSize});
		pContent->tGroupRect = pContent->tIconRect;
		return;
	}
	bVertical = (pResolved->iIconPlacement == XUI_BUTTON_ICON_TOP) || (pResolved->iIconPlacement == XUI_BUTTON_ICON_BOTTOM);
	fGap = pResolved->fIconGap;
	if ( bVertical ) {
		if ( fTextW > tContent.fW ) {
			fTextW = tContent.fW;
		}
		fGroupW = (fTextW > fIconSize) ? fTextW : fIconSize;
		fGroupH = fIconSize + fGap + fTextH;
		if ( fGroupH > tContent.fH ) {
			fGroupH = tContent.fH;
			fTextH = tContent.fH - fIconSize - fGap;
			if ( fTextH < 0.0f ) {
				fTextH = 0.0f;
			}
		}
		fX = tContent.fX + (tContent.fW - fGroupW) * 0.5f;
		fY = tContent.fY + (tContent.fH - fGroupH) * 0.5f;
		if ( pResolved->iIconPlacement == XUI_BUTTON_ICON_BOTTOM ) {
			pContent->tTextRect = (xui_rect_t){fX, fY, fGroupW, fTextH};
			pContent->tIconRect = (xui_rect_t){fX + (fGroupW - fIconSize) * 0.5f, fY + fTextH + fGap, fIconSize, fIconSize};
		} else {
			pContent->tIconRect = (xui_rect_t){fX + (fGroupW - fIconSize) * 0.5f, fY, fIconSize, fIconSize};
			pContent->tTextRect = (xui_rect_t){fX, fY + fIconSize + fGap, fGroupW, fTextH};
		}
		pContent->tGroupRect = (xui_rect_t){fX, fY, fGroupW, fGroupH};
	} else {
		if ( fTextW > tContent.fW - fIconSize - fGap ) {
			fTextW = tContent.fW - fIconSize - fGap;
		}
		if ( fTextW < 0.0f ) {
			fTextW = 0.0f;
		}
		fGroupW = fIconSize + fGap + fTextW;
		fGroupH = (fTextH > fIconSize) ? fTextH : fIconSize;
		if ( fGroupH > tContent.fH ) {
			fGroupH = tContent.fH;
		}
		fX = tContent.fX + (tContent.fW - fGroupW) * 0.5f;
		fY = tContent.fY + (tContent.fH - fGroupH) * 0.5f;
		if ( pResolved->iIconPlacement == XUI_BUTTON_ICON_RIGHT ) {
			pContent->tTextRect = (xui_rect_t){fX, fY, fTextW, fGroupH};
			pContent->tIconRect = (xui_rect_t){fX + fTextW + fGap, fY + (fGroupH - fIconSize) * 0.5f, fIconSize, fIconSize};
		} else {
			pContent->tIconRect = (xui_rect_t){fX, fY + (fGroupH - fIconSize) * 0.5f, fIconSize, fIconSize};
			pContent->tTextRect = (xui_rect_t){fX + fIconSize + fGap, fY, fTextW, fGroupH};
		}
		pContent->tGroupRect = (xui_rect_t){fX, fY, fGroupW, fGroupH};
	}
	pContent->tIconRect = xuiInternalSnapRect(pContent->tIconRect);
	pContent->tTextRect = xuiInternalSnapRect(pContent->tTextRect);
	pContent->tGroupRect = xuiInternalSnapRect(pContent->tGroupRect);
}

static xui_rect_t __xuiButtonLayoutBadge(xui_widget pWidget, xui_button_data_t* pData, const xui_button_content_t* pContent)
{
	xui_rect_t tAnchorRect;
	xui_rect_t tContent;
	float fSize;
	float fX;
	float fY;

	tContent = xuiWidgetGetContentRect(pWidget);
	tAnchorRect = pContent->tGroupRect;
	if ( pData->iBadgeAnchor == XUI_BUTTON_BADGE_WIDGET_TOP_RIGHT ) {
		tAnchorRect = xuiWidgetGetRect(pWidget);
		tAnchorRect.fX = 0.0f;
		tAnchorRect.fY = 0.0f;
	} else if ( (pData->iBadgeAnchor == XUI_BUTTON_BADGE_ICON_TOP_RIGHT) && (pContent->tIconRect.fW > 0.0f) ) {
		tAnchorRect = pContent->tIconRect;
	} else if ( (pData->iBadgeAnchor == XUI_BUTTON_BADGE_TEXT_TOP_RIGHT) && (pContent->tTextRect.fW > 0.0f) ) {
		tAnchorRect = pContent->tTextRect;
	}
	if ( (tAnchorRect.fW <= 0.0f) && (tAnchorRect.fH <= 0.0f) ) {
		tAnchorRect = tContent;
	}
	fSize = (pData->fBadgeSize > 0.0f) ? pData->fBadgeSize : 12.0f;
	fX = tAnchorRect.fX + tAnchorRect.fW - fSize * 0.5f + pData->fBadgeOffsetX;
	fY = tAnchorRect.fY + fSize * 0.5f + pData->fBadgeOffsetY;
	return xuiInternalSnapRect((xui_rect_t){fX - fSize * 0.5f, fY - fSize * 0.5f, fSize, fSize});
}

static int __xuiButtonDrawDefaultBackground(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, const xui_button_visual_t* pVisual)
{
	int iRet;

	iRet = XUI_OK;
	if ( (pProxy == NULL) || (pDraw == NULL) || (pVisual == NULL) || (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XUI_OK;
	}
	if ( __xuiButtonColorAlpha(pVisual->iFillColor) != 0 ) {
		if ( pProxy->drawRectFill != NULL ) {
			iRet = pProxy->drawRectFill(pProxy, pDraw, tRect, pVisual->iFillColor);
		}
	}
	if ( (iRet == XUI_OK) && (pVisual->fBorderWidth > 0.0f) && (__xuiButtonColorAlpha(pVisual->iBorderColor) != 0) ) {
		if ( pProxy->drawRectStroke != NULL ) {
			iRet = pProxy->drawRectStroke(pProxy, pDraw, tRect, pVisual->fBorderWidth, pVisual->iBorderColor);
		}
	}
	return iRet;
}

static int __xuiButtonDrawFocusRing(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, const xui_button_visual_t* pVisual)
{
	if ( (pProxy == NULL) || (pDraw == NULL) || (pVisual == NULL) || (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ||
	     (pVisual->fBorderWidth <= 0.0f) || (__xuiButtonColorAlpha(pVisual->iBorderColor) == 0) ) {
		return XUI_OK;
	}
	return (pProxy->drawRectStroke != NULL) ? pProxy->drawRectStroke(pProxy, pDraw, tRect, pVisual->fBorderWidth, pVisual->iBorderColor) : XUI_OK;
}

static int __xuiButtonCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_button_data_t* pData;
	xui_button_resolved_t tResolved;
	xui_button_content_t tContent;
	xui_proxy pProxy;
	xui_rect_t tRect;
	xui_rect_t tSrc;
	xui_rect_t tBadge;
	uint32_t iRenderState;
	uint32_t iTextColor;
	int iVisual;
	int iPatch;
	int bHasIcon;
	int bHasText;
	int iRet;

	(void)pUser;
	if ( (pWidget == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	__xuiButtonResolve(pWidget, pData, &tResolved);
	iRenderState = __xuiButtonVisualStateId(iStateId);
	if ( !xuiWidgetGetEnabled(pWidget) ) {
		iRenderState = XUI_WIDGET_STATE_DISABLED;
	}
	iVisual = __xuiButtonStateIndex(iRenderState);
	if ( iVisual == XUI_BUTTON_VISUAL_FOCUS ) {
		iVisual = XUI_BUTTON_VISUAL_NORMAL;
	}
	tRect = xuiWidgetGetRect(pWidget);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	tRect = xuiInternalSnapRect(tRect);
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XUI_OK;
	}
	iPatch = __xuiButtonPatchIndex(pData, iRenderState);
	if ( iPatch >= 0 ) {
		iRet = __xuiButtonDrawNinePatch(pProxy, pDraw, &pData->arrPatch[iPatch], tRect);
	} else {
		iRet = __xuiButtonDrawDefaultBackground(pProxy, pDraw, tRect, &tResolved.arrVisual[iVisual]);
	}
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( ((iRenderState & XUI_WIDGET_STATE_FOCUS) != 0) &&
	     ((iRenderState & XUI_WIDGET_STATE_DISABLED) == 0) ) {
		iRet = __xuiButtonDrawFocusRing(pProxy, pDraw, tRect, &tResolved.arrVisual[XUI_BUTTON_VISUAL_FOCUS]);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	__xuiButtonLayoutContent(pWidget, pData, &tResolved, &tContent);
	bHasIcon = (pData->pIconSurface != NULL) && (tResolved.fIconSize > 0.0f);
	bHasText = (pData->sText != NULL) && (pData->sText[0] != '\0') && (tResolved.pFont != NULL);
	if ( bHasIcon && (__xuiButtonColorAlpha(tResolved.iIconColor) != 0) ) {
		tSrc = __xuiButtonSurfaceSrc(pProxy, pData->pIconSurface, pData->tIconSrc);
		iRet = __xuiButtonDrawSurfacePiece(pProxy, pDraw, pData->pIconSurface, tSrc, tContent.tIconRect, tResolved.iIconColor);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	if ( bHasText ) {
		iTextColor = ((iRenderState & XUI_WIDGET_STATE_DISABLED) != 0) ? tResolved.iDisabledTextColor : tResolved.iTextColor;
		if ( __xuiButtonColorAlpha(iTextColor) != 0 ) {
			iRet = pProxy->drawText(
				pProxy,
				pDraw,
				tResolved.pFont,
				pData->sText,
				bHasIcon ? tContent.tTextRect : xuiWidgetGetContentRect(pWidget),
				iTextColor,
				bHasIcon ? (XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP) : tResolved.iTextFlags);
			if ( iRet != XUI_OK ) {
				return iRet;
			}
		}
	}
	if ( pData->bBadgeVisible ) {
		tBadge = __xuiButtonLayoutBadge(pWidget, pData, &tContent);
		if ( pData->pBadgeSurface != NULL ) {
			tSrc = __xuiButtonSurfaceSrc(pProxy, pData->pBadgeSurface, pData->tBadgeSrc);
			iRet = __xuiButtonDrawSurfacePiece(pProxy, pDraw, pData->pBadgeSurface, tSrc, tBadge, XUI_COLOR_WHITE);
			if ( iRet != XUI_OK ) {
				return iRet;
			}
		} else if ( (pProxy->drawCircleFill != NULL) && (pProxy->drawCircleStroke != NULL) ) {
			iRet = pProxy->drawCircleFill(pProxy, pDraw, tBadge.fX + tBadge.fW * 0.5f, tBadge.fY + tBadge.fH * 0.5f, tBadge.fW * 0.5f, XUI_COLOR_RGBA(224, 48, 64, 255));
			if ( iRet != XUI_OK ) {
				return iRet;
			}
			iRet = pProxy->drawCircleStroke(pProxy, pDraw, tBadge.fX + tBadge.fW * 0.5f, tBadge.fY + tBadge.fH * 0.5f, tBadge.fW * 0.5f, 1.0f, XUI_COLOR_RGBA(255, 255, 255, 245));
			if ( iRet != XUI_OK ) {
				return iRet;
			}
		}
	}
	return XUI_OK;
}

static void __xuiButtonDoClick(xui_widget pWidget, xui_button_data_t* pData)
{
	if ( (pWidget == NULL) || (pData == NULL) || !xuiWidgetGetEnabled(pWidget) || !xuiWidgetGetVisible(pWidget) ) {
		return;
	}
	if ( pData->bSelectable ) {
		pData->bSelected = pData->bSelected ? 0 : 1;
	}
	pData->iClickCount++;
	(void)__xuiButtonSyncState(pWidget, pData);
	if ( pData->onClick != NULL ) {
		pData->onClick(pWidget, pData->pClickUser);
	}
}

static void __xuiButtonDefaultAction(xui_widget pWidget, void* pUser)
{
	xui_button_data_t* pData;

	(void)pUser;
	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return;
	}
	__xuiButtonDoClick(pWidget, pData);
}

static int __xuiButtonEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_button_data_t* pData;
	xui_context pContext;
	int bLeftButton;

	(void)pUser;
	if ( (pWidget == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pContext = xuiWidgetGetContext(pWidget);
	if ( !xuiWidgetGetEnabled(pWidget) || !xuiWidgetGetVisible(pWidget) ) {
		pData->bKeyboardActive = 0;
		(void)__xuiButtonSyncState(pWidget, pData);
		return XUI_OK;
	}
	bLeftButton = (pEvent->iButton == 0) || (pEvent->iButton == XUI_POINTER_BUTTON_LEFT);
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_ENTER:
	case XUI_EVENT_POINTER_LEAVE:
	case XUI_EVENT_POINTER_MOVE:
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
		(void)__xuiButtonSyncState(pWidget, pData);
		return XUI_OK;
	case XUI_EVENT_POINTER_DOWN:
		if ( bLeftButton ) {
			(void)xuiSetPointerCapture(pContext, pWidget);
			(void)__xuiButtonSyncState(pWidget, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_UP:
		if ( bLeftButton ) {
			(void)xuiReleasePointerCapture(pContext, pWidget);
			(void)__xuiButtonSyncState(pWidget, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		pData->bKeyboardActive = 0;
		(void)__xuiButtonSyncState(pWidget, pData);
		return XUI_OK;
	case XUI_EVENT_POINTER_CLICK:
		if ( bLeftButton ) {
			__xuiButtonDoClick(pWidget, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_KEY_DOWN:
		if ( pEvent->iKey == XUI_KEY_SPACE ) {
			pData->bKeyboardActive = 1;
			(void)__xuiButtonSyncState(pWidget, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_KEY_UP:
		if ( pEvent->iKey == XUI_KEY_SPACE ) {
			if ( pData->bKeyboardActive ) {
				pData->bKeyboardActive = 0;
				__xuiButtonDoClick(pWidget, pData);
			}
			(void)__xuiButtonSyncState(pWidget, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	default:
		break;
	}
	return XUI_OK;
}

static void __xuiButtonDefaultLayout(xui_layout_t* pLayout)
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
	pLayout->tPadding = (xui_thickness_t){10.0f, 6.0f, 10.0f, 6.0f};
}

static void __xuiButtonDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE | XUI_CACHE_UPDATE_ALL_STATES;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiButtonInitCacheStates(xui_widget pWidget)
{
	static const uint32_t arrState[XUI_BUTTON_CACHE_STATE_COUNT] = {
		0,
		XUI_WIDGET_STATE_HOVER,
		XUI_WIDGET_STATE_ACTIVE,
		XUI_WIDGET_STATE_FOCUS,
		XUI_WIDGET_STATE_DISABLED,
		XUI_BUTTON_STATE_CHECKED
	};
	int i;
	int iRet;

	iRet = xuiWidgetSetCacheStateCount(pWidget, XUI_BUTTON_CACHE_STATE_COUNT);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	for ( i = 0; i < XUI_BUTTON_CACHE_STATE_COUNT; i++ ) {
		iRet = xuiWidgetSetCacheStateId(pWidget, i, 0x70000000u + (uint32_t)i);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	for ( i = 0; i < XUI_BUTTON_CACHE_STATE_COUNT; i++ ) {
		iRet = xuiWidgetSetCacheStateId(pWidget, i, arrState[i]);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return XUI_OK;
}

static int __xuiButtonInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetDefaultAction(pWidget, __xuiButtonDefaultAction, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_ENTER, __xuiButtonEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiButtonEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiButtonEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiButtonEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiButtonEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CLICK, __xuiButtonEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiButtonEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiButtonEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiButtonEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiButtonEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_UP, __xuiButtonEvent, NULL);
	return iRet;
}

static int __xuiButtonInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_button_data_t* pData;
	const xui_button_desc_t* pDesc;
	xui_theme_t tTheme;
	xui_context pContext;
	xui_thickness_t tPadding;
	int iRet;

	(void)pUser;
	pData = (xui_button_data_t*)pTypeData;
	pDesc = (const xui_button_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiButtonDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pContext = xuiWidgetGetContext(pWidget);
	memset(&tTheme, 0, sizeof(tTheme));
	tTheme.iSize = sizeof(tTheme);
	(void)xuiGetTheme(pContext, &tTheme);
	__xuiButtonApplyThemeDefaults(pData, &tTheme, pDesc);
	pData->pFont = (pDesc != NULL && pDesc->pFont != NULL) ? pDesc->pFont : xuiGetDefaultFont(pContext);
	pData->iTextColor = (pDesc != NULL && pDesc->iTextColor != 0) ? pDesc->iTextColor : tTheme.iTextColor;
	pData->iDisabledTextColor = (pDesc != NULL && pDesc->iDisabledTextColor != 0) ? pDesc->iDisabledTextColor : __xuiButtonColorWithAlpha(tTheme.iTextColor, 128);
	pData->iTextFlags = ((pDesc != NULL && pDesc->iTextFlags != 0) ? pDesc->iTextFlags : (XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE)) | XUI_TEXT_CLIP;
	pData->iIconColor = pData->iTextColor;
	pData->fIconSize = 16.0f;
	pData->fIconGap = 6.0f;
	pData->iIconPlacement = XUI_BUTTON_ICON_LEFT;
	pData->iBadgeAnchor = XUI_BUTTON_BADGE_CONTENT_TOP_RIGHT;
	pData->fBadgeSize = 12.0f;
	iRet = __xuiButtonTextSet(pData, (pDesc != NULL) ? pDesc->sText : "");
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	tPadding.fLeft = 10.0f;
	tPadding.fTop = 6.0f;
	tPadding.fRight = 10.0f;
	tPadding.fBottom = 6.0f;
	(void)xuiWidgetSetPadding(pWidget, tPadding);
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	iRet = __xuiButtonInitCacheStates(pWidget);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iRet = __xuiButtonInitEvents(pWidget);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return __xuiButtonSyncState(pWidget, pData);
}

static void __xuiButtonDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_button_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_button_data_t*)pTypeData;
	if ( pData == NULL ) {
		return;
	}
	if ( pData->sText != NULL ) {
		xrtFree(pData->sText);
	}
	memset(pData, 0, sizeof(*pData));
}

static void __xuiButtonRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
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

static void __xuiButtonRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;
	uint32_t iLayoutDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	iLayoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiButtonRegisterStyleProperty(pContext, pType, "button.text_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiButtonRegisterStyleProperty(pContext, pType, "button.disabled_text_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiButtonRegisterStyleProperty(pContext, pType, "button.icon_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiButtonRegisterStyleProperty(pContext, pType, "button.normal_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiButtonRegisterStyleProperty(pContext, pType, "button.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiButtonRegisterStyleProperty(pContext, pType, "button.active_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiButtonRegisterStyleProperty(pContext, pType, "button.focus_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiButtonRegisterStyleProperty(pContext, pType, "button.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiButtonRegisterStyleProperty(pContext, pType, "button.checked_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiButtonRegisterStyleProperty(pContext, pType, "button.border_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiButtonRegisterStyleProperty(pContext, pType, "button.border_width", XUI_STYLE_VALUE_FLOAT, iPaintDirty, 0);
	__xuiButtonRegisterStyleProperty(pContext, pType, "button.hover_border_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiButtonRegisterStyleProperty(pContext, pType, "button.hover_border_width", XUI_STYLE_VALUE_FLOAT, iPaintDirty, 0);
	__xuiButtonRegisterStyleProperty(pContext, pType, "button.active_border_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiButtonRegisterStyleProperty(pContext, pType, "button.active_border_width", XUI_STYLE_VALUE_FLOAT, iPaintDirty, 0);
	__xuiButtonRegisterStyleProperty(pContext, pType, "button.disabled_border_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiButtonRegisterStyleProperty(pContext, pType, "button.disabled_border_width", XUI_STYLE_VALUE_FLOAT, iPaintDirty, 0);
	__xuiButtonRegisterStyleProperty(pContext, pType, "button.checked_border_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiButtonRegisterStyleProperty(pContext, pType, "button.checked_border_width", XUI_STYLE_VALUE_FLOAT, iPaintDirty, 0);
	__xuiButtonRegisterStyleProperty(pContext, pType, "button.focus_border_width", XUI_STYLE_VALUE_FLOAT, iPaintDirty, 0);
	__xuiButtonRegisterStyleProperty(pContext, pType, "button.icon_size", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiButtonRegisterStyleProperty(pContext, pType, "button.icon_gap", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiButtonRegisterStyleProperty(pContext, pType, "button.icon_placement", XUI_STYLE_VALUE_INT, iLayoutDirty, 0);
}

XUI_API xui_widget_type xuiButtonGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "button");
	if ( pType != NULL ) {
		__xuiButtonRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "button";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_button_data_t);
	tDesc.onInit = __xuiButtonInit;
	tDesc.onDestroy = __xuiButtonDestroy;
	tDesc.onContentMeasure = __xuiButtonContentMeasure;
	tDesc.onCacheRender = __xuiButtonCacheRender;
	__xuiButtonDefaultLayout(&tDesc.tLayout);
	__xuiButtonDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	__xuiButtonRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiButtonCreate(xui_context pContext, xui_widget* ppWidget, const xui_button_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiButtonDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiButtonGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiButtonSetClick(xui_widget pWidget, xui_button_click_proc onClick, void* pUser)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->onClick = onClick;
	pData->pClickUser = pUser;
	return XUI_OK;
}

XUI_API int xuiButtonSetText(xui_widget pWidget, const char* sText)
{
	xui_button_data_t* pData;
	int iRet;

	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( sText == NULL ) {
		sText = "";
	}
	if ( (pData->sText != NULL) && (strcmp(pData->sText, sText) == 0) ) {
		return XUI_OK;
	}
	iRet = __xuiButtonTextSet(pData, sText);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API const char* xuiButtonGetText(xui_widget pWidget)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	return (pData != NULL && pData->sText != NULL) ? pData->sText : NULL;
}

XUI_API int xuiButtonSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->pFont == pFont ) {
		return XUI_OK;
	}
	pData->pFont = pFont;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_font xuiButtonGetFont(xui_widget pWidget)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API int xuiButtonSetTextFlags(xui_widget pWidget, uint32_t iTextFlags)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->iTextFlags = iTextFlags | XUI_TEXT_CLIP;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiButtonGetTextFlags(xui_widget pWidget)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	return (pData != NULL) ? pData->iTextFlags : 0;
}

XUI_API int xuiButtonSetTextColor(xui_widget pWidget, uint32_t iColor)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->iTextColor == iColor && pData->iIconColor == iColor ) {
		return XUI_OK;
	}
	pData->iTextColor = iColor;
	pData->iIconColor = iColor;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiButtonGetTextColor(xui_widget pWidget)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	return (pData != NULL) ? pData->iTextColor : 0;
}

XUI_API int xuiButtonSetDisabledTextColor(xui_widget pWidget, uint32_t iColor)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->iDisabledTextColor == iColor ) {
		return XUI_OK;
	}
	pData->iDisabledTextColor = iColor;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiButtonGetDisabledTextColor(xui_widget pWidget)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	return (pData != NULL) ? pData->iDisabledTextColor : 0;
}

XUI_API int xuiButtonSetSelectable(xui_widget pWidget, int bSelectable)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	bSelectable = bSelectable ? 1 : 0;
	if ( (pData->bSelectable == bSelectable) && (bSelectable || !pData->bSelected) ) {
		return XUI_OK;
	}
	pData->bSelectable = bSelectable;
	if ( !bSelectable ) {
		pData->bSelected = 0;
	}
	return __xuiButtonSyncState(pWidget, pData);
}

XUI_API int xuiButtonGetSelectable(xui_widget pWidget)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	return (pData != NULL) ? pData->bSelectable : 0;
}

XUI_API int xuiButtonSetSelected(xui_widget pWidget, int bSelected)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	bSelected = bSelected ? 1 : 0;
	if ( pData->bSelectable && (pData->bSelected == bSelected) ) {
		return XUI_OK;
	}
	pData->bSelectable = 1;
	pData->bSelected = bSelected;
	return __xuiButtonSyncState(pWidget, pData);
}

XUI_API int xuiButtonIsSelected(xui_widget pWidget)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	return (pData != NULL) ? pData->bSelected : 0;
}

XUI_API int xuiButtonSetSemantic(xui_widget pWidget, int iSemantic)
{
	xui_button_data_t* pData;
	xui_theme_t tTheme;

	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (iSemantic != XUI_BUTTON_SEMANTIC_DEFAULT) &&
	     (iSemantic != XUI_BUTTON_SEMANTIC_PRIMARY) &&
	     (iSemantic != XUI_BUTTON_SEMANTIC_DANGER) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( iSemantic == XUI_BUTTON_SEMANTIC_PRIMARY ) {
		(void)xuiButtonSetColors(pWidget, XUI_COLOR_RGBA(35, 132, 214, 255), XUI_COLOR_RGBA(56, 151, 228, 255), XUI_COLOR_RGBA(22, 104, 176, 255), pData->arrVisual[XUI_BUTTON_VISUAL_FOCUS].iBorderColor, XUI_COLOR_RGBA(188, 210, 228, 255));
		(void)xuiButtonSetTextColor(pWidget, XUI_COLOR_RGBA(248, 252, 255, 255));
	} else if ( iSemantic == XUI_BUTTON_SEMANTIC_DANGER ) {
		(void)xuiButtonSetColors(pWidget, XUI_COLOR_RGBA(214, 72, 86, 255), XUI_COLOR_RGBA(228, 92, 104, 255), XUI_COLOR_RGBA(178, 52, 66, 255), pData->arrVisual[XUI_BUTTON_VISUAL_FOCUS].iBorderColor, XUI_COLOR_RGBA(230, 200, 204, 255));
		(void)xuiButtonSetTextColor(pWidget, XUI_COLOR_RGBA(255, 250, 250, 255));
	} else {
		memset(&tTheme, 0, sizeof(tTheme));
		tTheme.iSize = sizeof(tTheme);
		(void)xuiGetTheme(xuiWidgetGetContext(pWidget), &tTheme);
		__xuiButtonApplyThemeDefaults(pData, &tTheme, NULL);
		pData->iTextColor = tTheme.iTextColor;
		pData->iIconColor = tTheme.iTextColor;
	}
	pData->iSemantic = iSemantic;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiButtonGetSemantic(xui_widget pWidget)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	return (pData != NULL) ? pData->iSemantic : XUI_BUTTON_SEMANTIC_DEFAULT;
}

XUI_API int xuiButtonSetIcon(xui_widget pWidget, xui_surface pSurface, xui_rect_t tSrc)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->pIconSurface = pSurface;
	pData->tIconSrc = tSrc;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiButtonGetIcon(xui_widget pWidget, xui_surface* ppSurface, xui_rect_t* pSrc)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( ppSurface != NULL ) *ppSurface = pData->pIconSurface;
	if ( pSrc != NULL ) *pSrc = pData->tIconSrc;
	return XUI_OK;
}

XUI_API xui_surface xuiButtonGetIconSurface(xui_widget pWidget)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	return (pData != NULL) ? pData->pIconSurface : NULL;
}

XUI_API int xuiButtonSetIconColor(xui_widget pWidget, uint32_t iColor)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->iIconColor == iColor ) {
		return XUI_OK;
	}
	pData->iIconColor = iColor;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiButtonGetIconColor(xui_widget pWidget)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	return (pData != NULL) ? pData->iIconColor : 0u;
}

XUI_API int xuiButtonSetIconLayout(xui_widget pWidget, int iPlacement, float fIconSize, float fGap)
{
	xui_button_data_t* pData;

	if ( !__xuiButtonIconPlacementValid(iPlacement) || (fIconSize < 0.0f) || (fGap < 0.0f) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->iIconPlacement = iPlacement;
	pData->fIconSize = (fIconSize > 0.0f) ? fIconSize : 16.0f;
	pData->fIconGap = fGap;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiButtonGetIconLayout(xui_widget pWidget, int* pPlacement, float* pIconSize, float* pGap)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pPlacement != NULL ) *pPlacement = pData->iIconPlacement;
	if ( pIconSize != NULL ) *pIconSize = pData->fIconSize;
	if ( pGap != NULL ) *pGap = pData->fIconGap;
	return XUI_OK;
}

XUI_API int xuiButtonSetColors(xui_widget pWidget, uint32_t iNormal, uint32_t iHover, uint32_t iActive, uint32_t iFocus, uint32_t iDisabled)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->arrVisual[XUI_BUTTON_VISUAL_NORMAL].iFillColor = iNormal;
	pData->arrVisual[XUI_BUTTON_VISUAL_HOVER].iFillColor = iHover;
	pData->arrVisual[XUI_BUTTON_VISUAL_ACTIVE].iFillColor = iActive;
	pData->arrVisual[XUI_BUTTON_VISUAL_FOCUS].iFillColor = iNormal;
	pData->arrVisual[XUI_BUTTON_VISUAL_FOCUS].iBorderColor = iFocus;
	pData->arrVisual[XUI_BUTTON_VISUAL_DISABLED].iFillColor = iDisabled;
	pData->arrVisual[XUI_BUTTON_VISUAL_CHECKED].iFillColor = iActive;
	pData->iSemantic = XUI_BUTTON_SEMANTIC_DEFAULT;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiButtonSetStateVisual(xui_widget pWidget, uint32_t iState, uint32_t iFill, float fBorderWidth, uint32_t iBorderColor)
{
	xui_button_data_t* pData;
	int iIndex;

	if ( fBorderWidth < 0.0f ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iIndex = __xuiButtonStateIndex(iState);
	pData->arrVisual[iIndex].iFillColor = iFill;
	pData->arrVisual[iIndex].fBorderWidth = fBorderWidth;
	pData->arrVisual[iIndex].iBorderColor = iBorderColor;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiButtonGetStateVisual(xui_widget pWidget, uint32_t iState, uint32_t* pFill, float* pBorderWidth, uint32_t* pBorderColor)
{
	xui_button_data_t* pData;
	int iIndex;

	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iIndex = __xuiButtonStateIndex(iState);
	if ( pFill != NULL ) *pFill = pData->arrVisual[iIndex].iFillColor;
	if ( pBorderWidth != NULL ) *pBorderWidth = pData->arrVisual[iIndex].fBorderWidth;
	if ( pBorderColor != NULL ) *pBorderColor = pData->arrVisual[iIndex].iBorderColor;
	return XUI_OK;
}

XUI_API int xuiButtonSetBorder(xui_widget pWidget, float fBorderWidth, uint32_t iBorderColor)
{
	xui_button_data_t* pData;
	int i;

	if ( fBorderWidth < 0.0f ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	for ( i = 0; i < XUI_BUTTON_VISUAL_COUNT; i++ ) {
		if ( i == XUI_BUTTON_VISUAL_FOCUS ) {
			continue;
		}
		pData->arrVisual[i].fBorderWidth = fBorderWidth;
		pData->arrVisual[i].iBorderColor = iBorderColor;
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiButtonSetPatch(xui_widget pWidget, uint32_t iState, const xui_nine_patch_t* pPatch)
{
	xui_button_data_t* pData;
	int iIndex;

	if ( !__xuiButtonPatchValid(pPatch) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iIndex = __xuiButtonStateIndex(iState);
	if ( pPatch == NULL ) {
		memset(&pData->arrPatch[iIndex], 0, sizeof(pData->arrPatch[iIndex]));
		pData->arrHasPatch[iIndex] = 0;
	} else {
		pData->arrPatch[iIndex] = *pPatch;
		pData->arrPatch[iIndex].iSize = sizeof(pData->arrPatch[iIndex]);
		if ( pData->arrPatch[iIndex].iColor == 0 ) {
			pData->arrPatch[iIndex].iColor = XUI_COLOR_WHITE;
		}
		pData->arrHasPatch[iIndex] = 1;
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiButtonGetPatch(xui_widget pWidget, uint32_t iState, xui_nine_patch_t* pPatch)
{
	xui_button_data_t* pData;
	int iIndex;

	if ( pPatch == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iIndex = __xuiButtonStateIndex(iState);
	if ( !pData->arrHasPatch[iIndex] ) {
		memset(pPatch, 0, sizeof(*pPatch));
		return XUI_ERROR;
	}
	*pPatch = pData->arrPatch[iIndex];
	pPatch->iSize = sizeof(*pPatch);
	return XUI_OK;
}

XUI_API int xuiButtonClearPatch(xui_widget pWidget, uint32_t iState)
{
	return xuiButtonSetPatch(pWidget, iState, NULL);
}

XUI_API int xuiButtonHasPatch(xui_widget pWidget, uint32_t iState)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return 0;
	}
	return pData->arrHasPatch[__xuiButtonStateIndex(iState)];
}

XUI_API int xuiButtonSetBadgeVisible(xui_widget pWidget, int bVisible)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	bVisible = bVisible ? 1 : 0;
	if ( pData->bBadgeVisible == bVisible ) {
		return XUI_OK;
	}
	pData->bBadgeVisible = bVisible;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiButtonGetBadgeVisible(xui_widget pWidget)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	return (pData != NULL) ? pData->bBadgeVisible : 0;
}

XUI_API int xuiButtonSetBadgeAnchor(xui_widget pWidget, int iAnchor)
{
	xui_button_data_t* pData;

	if ( !__xuiButtonBadgeAnchorValid(iAnchor) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->iBadgeAnchor = iAnchor;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiButtonGetBadgeAnchor(xui_widget pWidget)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	return (pData != NULL) ? pData->iBadgeAnchor : XUI_BUTTON_BADGE_CONTENT_TOP_RIGHT;
}

XUI_API int xuiButtonSetBadgeOffset(xui_widget pWidget, float fX, float fY)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->fBadgeOffsetX = fX;
	pData->fBadgeOffsetY = fY;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiButtonGetBadgeOffset(xui_widget pWidget, float* pX, float* pY)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pX != NULL ) *pX = pData->fBadgeOffsetX;
	if ( pY != NULL ) *pY = pData->fBadgeOffsetY;
	return XUI_OK;
}

XUI_API int xuiButtonSetBadgeSize(xui_widget pWidget, float fSize)
{
	xui_button_data_t* pData;

	if ( fSize <= 0.0f ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->fBadgeSize = fSize;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiButtonGetBadgeSize(xui_widget pWidget)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	return (pData != NULL) ? pData->fBadgeSize : 0.0f;
}

XUI_API int xuiButtonSetBadgeSurface(xui_widget pWidget, xui_surface pSurface, xui_rect_t tSrc)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->pBadgeSurface = pSurface;
	pData->tBadgeSrc = tSrc;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiButtonGetBadgeSurface(xui_widget pWidget, xui_surface* ppSurface, xui_rect_t* pSrc)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( ppSurface != NULL ) *ppSurface = pData->pBadgeSurface;
	if ( pSrc != NULL ) *pSrc = pData->tBadgeSrc;
	return XUI_OK;
}

XUI_API uint32_t xuiButtonGetState(xui_widget pWidget)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	if ( pData == NULL ) {
		return 0;
	}
	(void)__xuiButtonSyncState(pWidget, pData);
	return __xuiButtonComputeState(pWidget, pData);
}

XUI_API int xuiButtonGetClickCount(xui_widget pWidget)
{
	xui_button_data_t* pData;

	pData = __xuiButtonGetData(pWidget);
	return (pData != NULL) ? pData->iClickCount : 0;
}
