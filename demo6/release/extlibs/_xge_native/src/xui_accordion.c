#include "xui_internal.h"

#include <stdio.h>
#include <string.h>

#define XUI_ACCORDION_DEFAULT_BORDER_COLOR XUI_COLOR_RGBA(127, 196, 229, 255)
#define XUI_ACCORDION_DEFAULT_INDICATOR_COLOR XUI_COLOR_RGBA(47, 125, 215, 255)

typedef struct xui_accordion_section_t {
	xui_widget pSection;
	xui_widget pHeader;
	xui_widget pClient;
	char sTitle[XUI_ACCORDION_TITLE_CAPACITY];
	int iId;
	int bExpanded;
	int bEnabled;
	xui_rect_t tSectionRect;
	xui_rect_t tHeaderRect;
	xui_rect_t tClientRect;
	xui_rect_t tArrowRect;
	xui_rect_t tTextRect;
} xui_accordion_section_t;

typedef struct xui_accordion_data_t {
	xui_accordion_select_proc onSelect;
	void* pSelectUser;
	xui_accordion_section_t arrSections[XUI_ACCORDION_SECTION_CAPACITY];
	xui_font pFont;
	int iSectionCount;
	int iMode;
	int iSelected;
	int iHoverIndex;
	int iActiveIndex;
	int iChangeCount;
	float fHeaderHeight;
	float fSpacing;
	float fContentPadding;
	float fContentHeight;
	uint32_t iBackgroundColor;
	uint32_t iHeaderColor;
	uint32_t iHoverColor;
	uint32_t iExpandedColor;
	uint32_t iContentColor;
	uint32_t iBorderColor;
	uint32_t iTextColor;
	uint32_t iActiveTextColor;
	uint32_t iDisabledTextColor;
} xui_accordion_data_t;

typedef struct xui_accordion_resolved_t {
	xui_font pFont;
	float fHeaderHeight;
	float fSpacing;
	float fContentPadding;
	uint32_t iBackgroundColor;
	uint32_t iHeaderColor;
	uint32_t iHoverColor;
	uint32_t iExpandedColor;
	uint32_t iContentColor;
	uint32_t iBorderColor;
	uint32_t iTextColor;
	uint32_t iActiveTextColor;
	uint32_t iDisabledTextColor;
} xui_accordion_resolved_t;

static xui_thickness_t __xuiAccordionThickness(float fLeft, float fTop, float fRight, float fBottom)
{
	xui_thickness_t tValue;

	tValue.fLeft = fLeft;
	tValue.fTop = fTop;
	tValue.fRight = fRight;
	tValue.fBottom = fBottom;
	return tValue;
}

static int __xuiAccordionAlpha(uint32_t iColor)
{
	return (int)(iColor & 0xffu);
}

static uint32_t __xuiAccordionIndicatorColor(const xui_accordion_resolved_t* pResolved)
{
	if ( pResolved == NULL ) {
		return XUI_ACCORDION_DEFAULT_INDICATOR_COLOR;
	}
	if ( pResolved->iBorderColor == XUI_ACCORDION_DEFAULT_BORDER_COLOR ) {
		return XUI_ACCORDION_DEFAULT_INDICATOR_COLOR;
	}
	return pResolved->iBorderColor;
}

static float __xuiAccordionMaxFloat(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static int __xuiAccordionRectContains(xui_rect_t tRect, float fX, float fY)
{
	return (fX >= tRect.fX) && (fY >= tRect.fY) && (fX < (tRect.fX + tRect.fW)) && (fY < (tRect.fY + tRect.fH));
}

static int __xuiAccordionModeValid(int iMode)
{
	return (iMode == XUI_ACCORDION_MODE_MULTIPLE) || (iMode == XUI_ACCORDION_MODE_SINGLE);
}

static int __xuiAccordionDescValid(const xui_accordion_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) {
		return 0;
	}
	if ( (pDesc->iSectionCount < 0) || (pDesc->iSectionCount > XUI_ACCORDION_SECTION_CAPACITY) ) {
		return 0;
	}
	if ( (pDesc->iMode != 0) && !__xuiAccordionModeValid(pDesc->iMode) ) {
		return 0;
	}
	if ( (pDesc->fHeaderHeight < 0.0f) || (pDesc->fSpacing < 0.0f) || (pDesc->fContentPadding < 0.0f) ) {
		return 0;
	}
	return 1;
}

static void __xuiAccordionSetTitle(xui_accordion_section_t* pSection, const char* sTitle)
{
	if ( pSection == NULL ) {
		return;
	}
	if ( sTitle == NULL ) {
		sTitle = "";
	}
	(void)snprintf(pSection->sTitle, sizeof(pSection->sTitle), "%s", sTitle);
	pSection->sTitle[sizeof(pSection->sTitle) - 1] = '\0';
}

static xui_accordion_data_t* __xuiAccordionGetData(xui_widget pWidget)
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
	pType = xuiWidgetFindType(pContext, "accordion");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_accordion_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiAccordionStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
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

static int __xuiAccordionStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
{
	xui_style_property_t tProperty;
	int iRet;

	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) &&
	     (tProperty.tValue.iType == XUI_STYLE_VALUE_FLOAT) &&
	     (tProperty.tValue.fFloat >= 0.0f) ) {
		*pValue = tProperty.tValue.fFloat;
		return 1;
	}
	return 0;
}

static void __xuiAccordionResolve(xui_widget pWidget, const xui_accordion_data_t* pData, xui_accordion_resolved_t* pOut)
{
	pOut->pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	pOut->fHeaderHeight = pData->fHeaderHeight;
	pOut->fSpacing = pData->fSpacing;
	pOut->fContentPadding = pData->fContentPadding;
	pOut->iBackgroundColor = pData->iBackgroundColor;
	pOut->iHeaderColor = pData->iHeaderColor;
	pOut->iHoverColor = pData->iHoverColor;
	pOut->iExpandedColor = pData->iExpandedColor;
	pOut->iContentColor = pData->iContentColor;
	pOut->iBorderColor = pData->iBorderColor;
	pOut->iTextColor = pData->iTextColor;
	pOut->iActiveTextColor = pData->iActiveTextColor;
	pOut->iDisabledTextColor = pData->iDisabledTextColor;
	(void)__xuiAccordionStyleFloat(pWidget, "accordion.header.height", &pOut->fHeaderHeight);
	(void)__xuiAccordionStyleFloat(pWidget, "accordion.spacing", &pOut->fSpacing);
	(void)__xuiAccordionStyleFloat(pWidget, "accordion.content.padding", &pOut->fContentPadding);
	(void)__xuiAccordionStyleColor(pWidget, "accordion.background.color", &pOut->iBackgroundColor);
	(void)__xuiAccordionStyleColor(pWidget, "accordion.header.color", &pOut->iHeaderColor);
	(void)__xuiAccordionStyleColor(pWidget, "accordion.header.hover_color", &pOut->iHoverColor);
	(void)__xuiAccordionStyleColor(pWidget, "accordion.header.expanded_color", &pOut->iExpandedColor);
	(void)__xuiAccordionStyleColor(pWidget, "accordion.content.color", &pOut->iContentColor);
	(void)__xuiAccordionStyleColor(pWidget, "accordion.border.color", &pOut->iBorderColor);
	(void)__xuiAccordionStyleColor(pWidget, "accordion.text.color", &pOut->iTextColor);
	(void)__xuiAccordionStyleColor(pWidget, "accordion.text.active_color", &pOut->iActiveTextColor);
	(void)__xuiAccordionStyleColor(pWidget, "accordion.text.disabled_color", &pOut->iDisabledTextColor);
	if ( pOut->fHeaderHeight < 18.0f ) pOut->fHeaderHeight = 18.0f;
	if ( pOut->fSpacing < 0.0f ) pOut->fSpacing = 0.0f;
	if ( pOut->fContentPadding < 0.0f ) pOut->fContentPadding = 0.0f;
}

static int __xuiAccordionDrawFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( (pProxy == NULL) || (pProxy->drawRectFill == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || (__xuiAccordionAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	return pProxy->drawRectFill(pProxy, pDraw, xuiInternalSnapRect(tRect), iColor);
}

static int __xuiAccordionDrawRectLines(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor);

static int __xuiAccordionDrawRectFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	return __xuiAccordionDrawFill(pProxy, pDraw, tRect, iColor);
}

static int __xuiAccordionDrawRectStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	return __xuiAccordionDrawRectLines(pProxy, pDraw, tRect, iColor);
}

static int __xuiAccordionDrawLine(xui_proxy pProxy, xui_draw_context pDraw, float fX0, float fY0, float fX1, float fY1, uint32_t iColor)
{
	if ( (pProxy == NULL) || (pProxy->drawLine == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	if ( __xuiAccordionAlpha(iColor) == 0 ) {
		return XUI_OK;
	}
	return pProxy->drawLine(pProxy, pDraw,
		xuiInternalSnapPixel(fX0), xuiInternalSnapPixel(fY0),
		xuiInternalSnapPixel(fX1), xuiInternalSnapPixel(fY1),
		1.0f, iColor);
}

static int __xuiAccordionDrawRectSideLines(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor, int bTop, int bRight, int bBottom, int bLeft)
{
	float fLeft;
	float fTop;
	float fRight;
	float fBottom;
	float fWidth;
	float fHeight;
	int iRet;

	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XUI_OK;
	}
	if ( __xuiAccordionAlpha(iColor) == 0 ) {
		return XUI_OK;
	}
	tRect = xuiInternalSnapRect(tRect);
	fLeft = tRect.fX;
	fTop = tRect.fY;
	fRight = tRect.fX + tRect.fW - 1.0f;
	fBottom = tRect.fY + tRect.fH - 1.0f;
	if ( fRight < fLeft ) fRight = fLeft;
	if ( fBottom < fTop ) fBottom = fTop;
	fWidth = fRight - fLeft + 1.0f;
	fHeight = fBottom - fTop + 1.0f;

	iRet = XUI_OK;
	if ( bTop ) iRet = __xuiAccordionDrawFill(pProxy, pDraw, (xui_rect_t){fLeft, fTop, fWidth, 1.0f}, iColor);
	if ( (iRet == XUI_OK) && bRight ) iRet = __xuiAccordionDrawFill(pProxy, pDraw, (xui_rect_t){fRight, fTop, 1.0f, fHeight}, iColor);
	if ( (iRet == XUI_OK) && bBottom ) iRet = __xuiAccordionDrawFill(pProxy, pDraw, (xui_rect_t){fLeft, fBottom, fWidth, 1.0f}, iColor);
	if ( (iRet == XUI_OK) && bLeft ) iRet = __xuiAccordionDrawFill(pProxy, pDraw, (xui_rect_t){fLeft, fTop, 1.0f, fHeight}, iColor);
	return iRet;
}

static int __xuiAccordionDrawRectLines(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	return __xuiAccordionDrawRectSideLines(pProxy, pDraw, tRect, iColor, 1, 1, 1, 1);
}

static int __xuiAccordionDrawExpandedHeader(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iFill, uint32_t iBorder)
{
	int iRet;

	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XUI_OK;
	}
	iRet = __xuiAccordionDrawFill(pProxy, pDraw, tRect, iFill);
	if ( (iRet != XUI_OK) || (__xuiAccordionAlpha(iBorder) == 0) ) {
		return iRet;
	}
	return __xuiAccordionDrawRectSideLines(pProxy, pDraw, tRect, iBorder, 1, 1, 0, 1);
}

static int __xuiAccordionDrawExpandedIndicator(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iIndicator)
{
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XUI_OK;
	}
	return __xuiAccordionDrawFill(pProxy, pDraw, (xui_rect_t){0.0f, 0.0f, 3.0f, tRect.fH}, iIndicator);
}

static int __xuiAccordionDrawTriangle(xui_proxy pProxy, xui_draw_context pDraw, xui_vec2_t tA, xui_vec2_t tB, xui_vec2_t tC, uint32_t iColor)
{
	int iRet;

	if ( __xuiAccordionAlpha(iColor) == 0 ) {
		return XUI_OK;
	}
	if ( (pProxy != NULL) && (pProxy->drawTriangleFill != NULL) && (pDraw != NULL) ) {
		return pProxy->drawTriangleFill(pProxy, pDraw, tA, tB, tC, iColor);
	}
	iRet = __xuiAccordionDrawLine(pProxy, pDraw, tA.fX, tA.fY, tB.fX, tB.fY, iColor);
	if ( iRet == XUI_OK ) iRet = __xuiAccordionDrawLine(pProxy, pDraw, tB.fX, tB.fY, tC.fX, tC.fY, iColor);
	if ( iRet == XUI_OK ) iRet = __xuiAccordionDrawLine(pProxy, pDraw, tC.fX, tC.fY, tA.fX, tA.fY, iColor);
	return iRet;
}

static int __xuiAccordionDrawText(xui_proxy pProxy, xui_draw_context pDraw, xui_font pFont, const char* sText, xui_rect_t tRect, uint32_t iColor)
{
	if ( (sText == NULL) || (*sText == 0) || (pFont == NULL) ) {
		return XUI_OK;
	}
	if ( (pProxy == NULL) || (pProxy->drawText == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || (__xuiAccordionAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	return pProxy->drawText(pProxy, pDraw, pFont, sText, xuiInternalSnapRect(tRect), iColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
}

static void __xuiAccordionDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE | XUI_CACHE_UPDATE_ALL_STATES;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiAccordionInvalidateSection(const xui_accordion_section_t* pSection, uint32_t iFlags)
{
	if ( pSection == NULL ) {
		return XUI_OK;
	}
	if ( pSection->pSection != NULL ) (void)xuiWidgetInvalidate(pSection->pSection, iFlags);
	if ( pSection->pHeader != NULL ) (void)xuiWidgetInvalidate(pSection->pHeader, iFlags);
	if ( pSection->pClient != NULL ) (void)xuiWidgetInvalidate(pSection->pClient, iFlags);
	return XUI_OK;
}

static int __xuiAccordionInvalidateAll(xui_widget pAccordion, xui_accordion_data_t* pData, uint32_t iFlags)
{
	int i;

	if ( (pAccordion == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	(void)xuiWidgetInvalidate(pAccordion, iFlags);
	for ( i = 0; i < pData->iSectionCount; i++ ) {
		(void)__xuiAccordionInvalidateSection(&pData->arrSections[i], iFlags);
	}
	return XUI_OK;
}

static int __xuiAccordionHeaderIndex(const xui_accordion_data_t* pData, xui_widget pHeader)
{
	int i;

	if ( pData == NULL ) {
		return -1;
	}
	for ( i = 0; i < pData->iSectionCount; i++ ) {
		if ( pData->arrSections[i].pHeader == pHeader ) {
			return i;
		}
	}
	return -1;
}

static int __xuiAccordionClientIndex(const xui_accordion_data_t* pData, xui_widget pClient)
{
	int i;

	if ( pData == NULL ) {
		return -1;
	}
	for ( i = 0; i < pData->iSectionCount; i++ ) {
		if ( pData->arrSections[i].pClient == pClient ) {
			return i;
		}
	}
	return -1;
}

static int __xuiAccordionSectionEnabled(const xui_accordion_data_t* pData, int iIndex)
{
	return (pData != NULL) &&
	       (iIndex >= 0) &&
	       (iIndex < pData->iSectionCount) &&
	       (pData->arrSections[iIndex].bEnabled != 0);
}

static int __xuiAccordionFirstEnabled(const xui_accordion_data_t* pData)
{
	int i;

	if ( pData == NULL ) return -1;
	for ( i = 0; i < pData->iSectionCount; i++ ) {
		if ( __xuiAccordionSectionEnabled(pData, i) ) {
			return i;
		}
	}
	return -1;
}

static int __xuiAccordionLastEnabled(const xui_accordion_data_t* pData)
{
	int i;

	if ( pData == NULL ) return -1;
	for ( i = pData->iSectionCount - 1; i >= 0; i-- ) {
		if ( __xuiAccordionSectionEnabled(pData, i) ) {
			return i;
		}
	}
	return -1;
}

static int __xuiAccordionStepEnabled(const xui_accordion_data_t* pData, int iStart, int iStep)
{
	int i;
	int iFallback;

	if ( (pData == NULL) || (pData->iSectionCount <= 0) ) {
		return -1;
	}
	iFallback = (iStep >= 0) ? __xuiAccordionFirstEnabled(pData) : __xuiAccordionLastEnabled(pData);
	if ( iFallback < 0 ) {
		return -1;
	}
	if ( (iStart < 0) || (iStart >= pData->iSectionCount) ) {
		return iFallback;
	}
	for ( i = iStart + ((iStep >= 0) ? 1 : -1); (i >= 0) && (i < pData->iSectionCount); i += (iStep >= 0) ? 1 : -1 ) {
		if ( __xuiAccordionSectionEnabled(pData, i) ) {
			return i;
		}
	}
	return iFallback;
}

static int __xuiAccordionSyncSections(xui_widget pAccordion, xui_accordion_data_t* pData)
{
	int i;

	if ( (pAccordion == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	for ( i = 0; i < pData->iSectionCount; i++ ) {
		xui_accordion_section_t* pSection = &pData->arrSections[i];
		if ( pSection->pSection != NULL ) {
			(void)xuiWidgetSetVisible(pSection->pSection, 1);
			(void)xuiWidgetSetEnabled(pSection->pSection, pSection->bEnabled);
		}
		if ( pSection->pHeader != NULL ) {
			(void)xuiWidgetSetEnabled(pSection->pHeader, pSection->bEnabled);
			(void)xuiWidgetInvalidate(pSection->pHeader, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		}
		if ( pSection->pClient != NULL ) {
			(void)xuiWidgetSetVisible(pSection->pClient, pSection->bExpanded);
			(void)xuiWidgetSetEnabled(pSection->pClient, pSection->bEnabled);
			(void)xuiWidgetInvalidate(pSection->pClient, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		}
	}
	return xuiWidgetInvalidate(pAccordion, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiAccordionSetHover(xui_widget pAccordion, xui_accordion_data_t* pData, int iIndex)
{
	int iOld;

	if ( (pAccordion == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( iIndex == pData->iHoverIndex ) {
		return XUI_OK;
	}
	iOld = pData->iHoverIndex;
	pData->iHoverIndex = iIndex;
	if ( (iOld >= 0) && (iOld < pData->iSectionCount) ) {
		(void)__xuiAccordionInvalidateSection(&pData->arrSections[iOld], XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	if ( (iIndex >= 0) && (iIndex < pData->iSectionCount) ) {
		(void)__xuiAccordionInvalidateSection(&pData->arrSections[iIndex], XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return xuiWidgetInvalidate(pAccordion, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiAccordionSetActive(xui_widget pAccordion, xui_accordion_data_t* pData, int iIndex)
{
	int iOld;

	if ( (pAccordion == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( iIndex == pData->iActiveIndex ) {
		return XUI_OK;
	}
	iOld = pData->iActiveIndex;
	pData->iActiveIndex = iIndex;
	if ( (iOld >= 0) && (iOld < pData->iSectionCount) ) {
		(void)__xuiAccordionInvalidateSection(&pData->arrSections[iOld], XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	if ( (iIndex >= 0) && (iIndex < pData->iSectionCount) ) {
		(void)__xuiAccordionInvalidateSection(&pData->arrSections[iIndex], XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return xuiWidgetInvalidate(pAccordion, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiAccordionSetExpandedInternal(xui_widget pAccordion, xui_accordion_data_t* pData, int iIndex, int bExpanded, int bNotify)
{
	int i;
	int bChanged;

	if ( (pAccordion == NULL) || (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iSectionCount) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	bExpanded = bExpanded ? 1 : 0;
	bChanged = 0;
	if ( (pData->iMode == XUI_ACCORDION_MODE_SINGLE) && bExpanded ) {
		for ( i = 0; i < pData->iSectionCount; i++ ) {
			if ( (i != iIndex) && pData->arrSections[i].bExpanded ) {
				pData->arrSections[i].bExpanded = 0;
				bChanged = 1;
			}
		}
	}
	if ( pData->arrSections[iIndex].bExpanded != bExpanded ) {
		pData->arrSections[iIndex].bExpanded = bExpanded;
		bChanged = 1;
	}
	if ( bExpanded ) {
		pData->iSelected = iIndex;
	} else if ( pData->iSelected == iIndex ) {
		pData->iSelected = -1;
	}
	if ( bChanged ) {
		pData->iChangeCount++;
		(void)__xuiAccordionSyncSections(pAccordion, pData);
		if ( bNotify && (pData->onSelect != NULL) ) {
			pData->onSelect(pAccordion, iIndex, pData->arrSections[iIndex].iId, pData->pSelectUser);
		}
	} else {
		(void)__xuiAccordionInvalidateAll(pAccordion, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return XUI_OK;
}

static int __xuiAccordionClickSection(xui_widget pAccordion, xui_accordion_data_t* pData, int iIndex)
{
	if ( !__xuiAccordionSectionEnabled(pData, iIndex) ) {
		return XUI_OK;
	}
	pData->iSelected = iIndex;
	return __xuiAccordionSetExpandedInternal(pAccordion, pData, iIndex, !pData->arrSections[iIndex].bExpanded, 1);
}

static int __xuiAccordionHeaderEvent(xui_widget pHeader, const xui_event_t* pEvent, void* pUser)
{
	xui_widget pAccordion;
	xui_accordion_data_t* pData;
	xui_rect_t tWorld;
	xui_rect_t tLocal;
	float fX;
	float fY;
	int iIndex;

	pAccordion = (xui_widget)pUser;
	pData = __xuiAccordionGetData(pAccordion);
	if ( (pHeader == NULL) || (pEvent == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iIndex = __xuiAccordionHeaderIndex(pData, pHeader);
	if ( iIndex < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	tWorld = xuiWidgetGetWorldRect(pHeader);
	fX = pEvent->fX - tWorld.fX;
	fY = pEvent->fY - tWorld.fY;
	tLocal = (xui_rect_t){0.0f, 0.0f, tWorld.fW, tWorld.fH};

	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_ENTER:
	case XUI_EVENT_POINTER_MOVE:
		if ( __xuiAccordionSectionEnabled(pData, iIndex) ) {
			(void)__xuiAccordionSetHover(pAccordion, pData, iIndex);
		}
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_POINTER_LEAVE:
		if ( pData->iHoverIndex == iIndex ) {
			(void)__xuiAccordionSetHover(pAccordion, pData, -1);
		}
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_POINTER_DOWN:
		if ( (pEvent->iButton == XUI_POINTER_BUTTON_LEFT) && __xuiAccordionSectionEnabled(pData, iIndex) ) {
			(void)__xuiAccordionSetActive(pAccordion, pData, iIndex);
			(void)xuiSetFocusWidget(xuiWidgetGetContext(pAccordion), pAccordion);
			(void)xuiSetPointerCapture(xuiWidgetGetContext(pAccordion), pHeader);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_UP:
		if ( pData->iActiveIndex == iIndex ) {
			(void)__xuiAccordionSetActive(pAccordion, pData, -1);
			if ( xuiGetPointerCapture(xuiWidgetGetContext(pAccordion)) == pHeader ) {
				(void)xuiReleasePointerCapture(xuiWidgetGetContext(pAccordion), pHeader);
			}
			if ( __xuiAccordionRectContains(tLocal, fX, fY) ) {
				(void)__xuiAccordionClickSection(pAccordion, pData, iIndex);
			}
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		if ( pData->iActiveIndex == iIndex ) {
			(void)__xuiAccordionSetActive(pAccordion, pData, -1);
		}
		return XUI_EVENT_DISPATCH_STOP;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiAccordionEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_accordion_data_t* pData;
	int iNext;

	(void)pUser;
	pData = __xuiAccordionGetData(pWidget);
	if ( (pData == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	switch ( pEvent->iType ) {
	case XUI_EVENT_KEY_DOWN:
		if ( !xuiWidgetGetEnabled(pWidget) ) {
			return XUI_OK;
		}
		if ( pEvent->iKey == XUI_KEY_DOWN ) {
			iNext = __xuiAccordionStepEnabled(pData, pData->iSelected, 1);
			if ( iNext >= 0 ) {
				pData->iSelected = iNext;
				(void)__xuiAccordionInvalidateAll(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
				return XUI_EVENT_DISPATCH_STOP;
			}
		} else if ( pEvent->iKey == XUI_KEY_UP ) {
			iNext = __xuiAccordionStepEnabled(pData, pData->iSelected, -1);
			if ( iNext >= 0 ) {
				pData->iSelected = iNext;
				(void)__xuiAccordionInvalidateAll(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
				return XUI_EVENT_DISPATCH_STOP;
			}
		} else if ( pEvent->iKey == XUI_KEY_HOME ) {
			iNext = __xuiAccordionFirstEnabled(pData);
			if ( iNext >= 0 ) {
				pData->iSelected = iNext;
				(void)__xuiAccordionInvalidateAll(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
				return XUI_EVENT_DISPATCH_STOP;
			}
		} else if ( pEvent->iKey == XUI_KEY_END ) {
			iNext = __xuiAccordionLastEnabled(pData);
			if ( iNext >= 0 ) {
				pData->iSelected = iNext;
				(void)__xuiAccordionInvalidateAll(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
				return XUI_EVENT_DISPATCH_STOP;
			}
		} else if ( (pEvent->iKey == XUI_KEY_SPACE) || (pEvent->iKey == XUI_KEY_ENTER) ) {
			iNext = pData->iSelected;
			if ( (iNext < 0) || (iNext >= pData->iSectionCount) || !__xuiAccordionSectionEnabled(pData, iNext) ) {
				iNext = __xuiAccordionFirstEnabled(pData);
			}
			if ( iNext >= 0 ) {
				(void)__xuiAccordionClickSection(pWidget, pData, iNext);
				return XUI_EVENT_DISPATCH_STOP;
			}
		} else if ( pEvent->iKey == XUI_KEY_ESCAPE ) {
			(void)__xuiAccordionSetActive(pWidget, pData, -1);
			(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), xuiGetPointerCapture(xuiWidgetGetContext(pWidget)));
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
	case XUI_EVENT_ENABLED_CHANGED:
		(void)__xuiAccordionSyncSections(pWidget, pData);
		return XUI_EVENT_DISPATCH_STOP;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiAccordionHeaderRender(xui_widget pHeader, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_widget pAccordion;
	xui_accordion_data_t* pData;
	xui_accordion_resolved_t tResolved;
	xui_accordion_section_t* pSection;
	xui_proxy pProxy;
	xui_rect_t tRect;
	xui_rect_t tArrow;
	xui_rect_t tText;
	uint32_t iFill;
	uint32_t iText;
	uint32_t iIndicator;
	int iIndex;
	int bEnabled;
	int iRet;

	(void)iStateId;
	pAccordion = (xui_widget)pUser;
	pData = __xuiAccordionGetData(pAccordion);
	if ( (pHeader == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iIndex = __xuiAccordionHeaderIndex(pData, pHeader);
	if ( iIndex < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pSection = &pData->arrSections[iIndex];
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pAccordion));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	__xuiAccordionResolve(pAccordion, pData, &tResolved);
	tRect = xuiWidgetGetRect(pHeader);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	iFill = tResolved.iHeaderColor;
	iText = tResolved.iTextColor;
	bEnabled = xuiWidgetGetEnabled(pAccordion) && pSection->bEnabled;
	if ( !bEnabled ) {
		iText = tResolved.iDisabledTextColor;
	} else if ( pData->iActiveIndex == iIndex ) {
		iFill = tResolved.iHoverColor;
	} else if ( pSection->bExpanded ) {
		iFill = tResolved.iExpandedColor;
		iText = tResolved.iActiveTextColor;
	} else if ( pData->iHoverIndex == iIndex ) {
		iFill = tResolved.iHoverColor;
	}
	if ( pSection->bExpanded ) {
		iRet = __xuiAccordionDrawExpandedHeader(pProxy, pDraw, tRect, iFill, tResolved.iBorderColor);
	} else {
		iRet = __xuiAccordionDrawRectFill(pProxy, pDraw, tRect, iFill);
		if ( iRet == XUI_OK ) iRet = __xuiAccordionDrawRectStroke(pProxy, pDraw, tRect, tResolved.iBorderColor);
	}
	if ( (iRet == XUI_OK) && bEnabled && pSection->bExpanded ) {
		iIndicator = __xuiAccordionIndicatorColor(&tResolved);
		iRet = __xuiAccordionDrawExpandedIndicator(pProxy, pDraw, tRect, iIndicator);
	}
	if ( iRet != XUI_OK ) return iRet;
	tArrow = (xui_rect_t){8.0f, (tRect.fH - 12.0f) * 0.5f, 12.0f, 12.0f};
	tText = (xui_rect_t){28.0f, 0.0f, __xuiAccordionMaxFloat(0.0f, tRect.fW - 36.0f), tRect.fH};
	pSection->tArrowRect = (xui_rect_t){pSection->tHeaderRect.fX + tArrow.fX, pSection->tHeaderRect.fY + tArrow.fY, tArrow.fW, tArrow.fH};
	pSection->tTextRect = (xui_rect_t){pSection->tHeaderRect.fX + tText.fX, pSection->tHeaderRect.fY + tText.fY, tText.fW, tText.fH};
	if ( pSection->bExpanded ) {
		iRet = __xuiAccordionDrawTriangle(pProxy, pDraw,
			(xui_vec2_t){tArrow.fX + 1.0f, tArrow.fY + 4.0f},
			(xui_vec2_t){tArrow.fX + 11.0f, tArrow.fY + 4.0f},
			(xui_vec2_t){tArrow.fX + 6.0f, tArrow.fY + 9.0f},
			iText);
	} else {
		iRet = __xuiAccordionDrawTriangle(pProxy, pDraw,
			(xui_vec2_t){tArrow.fX + 4.0f, tArrow.fY + 1.0f},
			(xui_vec2_t){tArrow.fX + 9.0f, tArrow.fY + 6.0f},
			(xui_vec2_t){tArrow.fX + 4.0f, tArrow.fY + 11.0f},
			iText);
	}
	if ( iRet == XUI_OK ) {
		iRet = __xuiAccordionDrawText(pProxy, pDraw, tResolved.pFont, pSection->sTitle, tText, iText);
	}
	return iRet;
}

static int __xuiAccordionClientRender(xui_widget pClient, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_widget pAccordion;
	xui_accordion_data_t* pData;
	xui_accordion_resolved_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tRect;
	int iIndex;
	int iRet;

	(void)iStateId;
	pAccordion = (xui_widget)pUser;
	pData = __xuiAccordionGetData(pAccordion);
	if ( (pClient == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iIndex = __xuiAccordionClientIndex(pData, pClient);
	if ( iIndex < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pAccordion));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	__xuiAccordionResolve(pAccordion, pData, &tResolved);
	tRect = xuiWidgetGetRect(pClient);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	iRet = __xuiAccordionDrawFill(pProxy, pDraw, tRect, tResolved.iContentColor);
	if ( iRet == XUI_OK ) iRet = __xuiAccordionDrawRectLines(pProxy, pDraw, tRect, tResolved.iBorderColor);
	return iRet;
}

static int __xuiAccordionCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_accordion_data_t* pData;
	xui_accordion_resolved_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tRect;

	(void)iStateId;
	(void)pUser;
	pData = __xuiAccordionGetData(pWidget);
	if ( (pData == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	__xuiAccordionResolve(pWidget, pData, &tResolved);
	tRect = xuiWidgetGetRect(pWidget);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	return __xuiAccordionDrawFill(pProxy, pDraw, tRect, tResolved.iBackgroundColor);
}

static void __xuiAccordionUpdateClientPadding(xui_accordion_data_t* pData, float fContentPadding)
{
	xui_thickness_t tPadding;
	int i;

	if ( pData == NULL ) {
		return;
	}
	tPadding = __xuiAccordionThickness(fContentPadding, fContentPadding, fContentPadding, fContentPadding);
	for ( i = 0; i < pData->iSectionCount; i++ ) {
		if ( pData->arrSections[i].pClient != NULL ) {
			(void)xuiWidgetSetPadding(pData->arrSections[i].pClient, tPadding);
		}
	}
}

static int __xuiAccordionMeasureClient(xui_widget pClient, float fWidth, xui_vec2_t* pOut)
{
	xui_vec2_t tConstraint;
	xui_vec2_t tMeasured;
	int iRet;

	if ( pOut == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pOut->fX = 0.0f;
	pOut->fY = 0.0f;
	if ( pClient == NULL ) {
		return XUI_OK;
	}
	tConstraint.fX = (fWidth > 0.0f) ? fWidth : XUI_LAYOUT_UNBOUNDED;
	tConstraint.fY = XUI_LAYOUT_UNBOUNDED;
	iRet = xuiWidgetMeasure(pClient, tConstraint, &tMeasured);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	*pOut = tMeasured;
	return XUI_OK;
}

static int __xuiAccordionContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_accordion_data_t* pData;
	xui_accordion_resolved_t tResolved;
	xui_vec2_t tClient;
	float fWidth;
	float fHeight;
	int i;

	(void)pUser;
	pData = __xuiAccordionGetData(pWidget);
	if ( (pData == NULL) || (pSize == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiAccordionResolve(pWidget, pData, &tResolved);
	__xuiAccordionUpdateClientPadding(pData, tResolved.fContentPadding);
	fWidth = (tConstraint.fX > 0.0f && tConstraint.fX < XUI_LAYOUT_UNBOUNDED) ? tConstraint.fX : 320.0f;
	fHeight = 0.0f;
	for ( i = 0; i < pData->iSectionCount; i++ ) {
		if ( i > 0 ) fHeight += tResolved.fSpacing;
		fHeight += tResolved.fHeaderHeight;
		if ( pData->arrSections[i].bExpanded ) {
			if ( __xuiAccordionMeasureClient(pData->arrSections[i].pClient, fWidth, &tClient) == XUI_OK ) {
				fHeight += tClient.fY;
				fWidth = __xuiAccordionMaxFloat(fWidth, tClient.fX);
			}
		}
	}
	if ( fWidth <= 0.0f ) fWidth = 320.0f;
	pSize->fX = fWidth;
	pSize->fY = fHeight;
	return XUI_OK;
}

static int __xuiAccordionArrange(xui_widget pWidget, xui_rect_t tContentRect, void* pUser)
{
	xui_accordion_data_t* pData;
	xui_accordion_resolved_t tResolved;
	xui_accordion_section_t* pSection;
	xui_vec2_t tClientSize;
	xui_rect_t tSectionRect;
	xui_rect_t tHeaderRect;
	xui_rect_t tClientRect;
	float fY;
	float fClientHeight;
	int iRet;
	int i;

	(void)pUser;
	pData = __xuiAccordionGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiAccordionResolve(pWidget, pData, &tResolved);
	__xuiAccordionUpdateClientPadding(pData, tResolved.fContentPadding);
	fY = tContentRect.fY;
	pData->fContentHeight = 0.0f;
	for ( i = 0; i < pData->iSectionCount; i++ ) {
		pSection = &pData->arrSections[i];
		if ( i > 0 ) {
			fY += tResolved.fSpacing;
			pData->fContentHeight += tResolved.fSpacing;
		}
		fClientHeight = 0.0f;
		if ( pSection->bExpanded ) {
			iRet = __xuiAccordionMeasureClient(pSection->pClient, tContentRect.fW, &tClientSize);
			if ( iRet != XUI_OK ) return iRet;
			fClientHeight = tClientSize.fY;
		}
		tSectionRect = xuiInternalSnapRect((xui_rect_t){tContentRect.fX, fY, tContentRect.fW, tResolved.fHeaderHeight + fClientHeight});
		tHeaderRect = xuiInternalSnapRect((xui_rect_t){0.0f, 0.0f, tSectionRect.fW, tResolved.fHeaderHeight});
		tClientRect = xuiInternalSnapRect((xui_rect_t){0.0f, tResolved.fHeaderHeight, tSectionRect.fW, fClientHeight});
		pSection->tSectionRect = tSectionRect;
		pSection->tHeaderRect = (xui_rect_t){tSectionRect.fX + tHeaderRect.fX, tSectionRect.fY + tHeaderRect.fY, tHeaderRect.fW, tHeaderRect.fH};
		pSection->tClientRect = (xui_rect_t){tSectionRect.fX + tClientRect.fX, tSectionRect.fY + tClientRect.fY, tClientRect.fW, tClientRect.fH};
		pSection->tArrowRect = (xui_rect_t){pSection->tHeaderRect.fX + 8.0f, pSection->tHeaderRect.fY + (pSection->tHeaderRect.fH - 12.0f) * 0.5f, 12.0f, 12.0f};
		pSection->tTextRect = (xui_rect_t){pSection->tHeaderRect.fX + 28.0f, pSection->tHeaderRect.fY, __xuiAccordionMaxFloat(0.0f, pSection->tHeaderRect.fW - 36.0f), pSection->tHeaderRect.fH};
		if ( pSection->pSection != NULL ) {
			iRet = xuiWidgetArrange(pSection->pSection, tSectionRect);
			if ( iRet != XUI_OK ) return iRet;
		}
		if ( pSection->pHeader != NULL ) {
			iRet = xuiWidgetArrange(pSection->pHeader, tHeaderRect);
			if ( iRet != XUI_OK ) return iRet;
			(void)xuiWidgetSetCacheRenderCallback(pSection->pHeader, __xuiAccordionHeaderRender, pWidget);
		}
		if ( pSection->pClient != NULL ) {
			(void)xuiWidgetSetVisible(pSection->pClient, pSection->bExpanded);
			iRet = xuiWidgetArrange(pSection->pClient, tClientRect);
			if ( iRet != XUI_OK ) return iRet;
			(void)xuiWidgetSetCacheRenderCallback(pSection->pClient, __xuiAccordionClientRender, pWidget);
		}
		fY += tSectionRect.fH;
		pData->fContentHeight += tSectionRect.fH;
	}
	return XUI_OK;
}

static void __xuiAccordionDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_MANUAL;
	pLayout->iWidthMode = XUI_SIZE_FIXED;
	pLayout->iHeightMode = XUI_SIZE_CONTENT;
	pLayout->iFlowMode = XUI_FLOW_BLOCK;
	pLayout->iDock = XUI_DOCK_FILL;
	pLayout->iOverflow = XUI_OVERFLOW_CLIP;
	pLayout->iAlignX = XUI_ALIGN_START;
	pLayout->iAlignY = XUI_ALIGN_START;
	pLayout->iTableRowSpan = 1;
	pLayout->iTableColumnSpan = 1;
	pLayout->iGridColumnCount = 1;
	pLayout->fPreferredWidth = 320.0f;
	pLayout->fPreferredHeight = 180.0f;
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fShrink = 1.0f;
}

static void __xuiAccordionDefaultData(xui_accordion_data_t* pData)
{
	int i;

	memset(pData, 0, sizeof(*pData));
	pData->iMode = XUI_ACCORDION_MODE_MULTIPLE;
	pData->iSelected = -1;
	pData->iHoverIndex = -1;
	pData->iActiveIndex = -1;
	pData->fHeaderHeight = 28.0f;
	pData->fSpacing = 4.0f;
	pData->fContentPadding = 8.0f;
	pData->iBackgroundColor = XUI_COLOR_RGBA(246, 251, 255, 255);
	pData->iHeaderColor = XUI_COLOR_RGBA(220, 236, 248, 255);
	pData->iHoverColor = XUI_COLOR_RGBA(220, 236, 248, 255);
	pData->iExpandedColor = XUI_COLOR_RGBA(220, 236, 248, 255);
	pData->iContentColor = XUI_COLOR_RGBA(248, 252, 255, 255);
	pData->iBorderColor = XUI_ACCORDION_DEFAULT_BORDER_COLOR;
	pData->iTextColor = XUI_COLOR_RGBA(28, 46, 64, 255);
	pData->iActiveTextColor = XUI_COLOR_RGBA(28, 46, 64, 255);
	pData->iDisabledTextColor = XUI_COLOR_RGBA(132, 148, 160, 255);
	for ( i = 0; i < XUI_ACCORDION_SECTION_CAPACITY; i++ ) {
		pData->arrSections[i].bEnabled = 1;
	}
}

static int __xuiAccordionConfigureSectionWidget(xui_widget pWidget)
{
	if ( pWidget == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	(void)xuiWidgetSetLayoutType(pWidget, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetSizeMode(pWidget, XUI_SIZE_FILL, XUI_SIZE_CONTENT);
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_CLIP);
	(void)xuiWidgetSetFocusable(pWidget, 0);
	(void)xuiWidgetSetTabStop(pWidget, 0);
	(void)xuiWidgetSetHitTestVisible(pWidget, 1);
	return XUI_OK;
}

static int __xuiAccordionConfigureHeaderWidget(xui_widget pAccordion, xui_widget pHeader)
{
	xui_cache_policy_t tPolicy;
	int iRet;

	if ( (pAccordion == NULL) || (pHeader == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiAccordionDefaultCachePolicy(&tPolicy);
	iRet = xuiWidgetSetCachePolicy(pHeader, &tPolicy);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetLayoutType(pHeader, XUI_LAYOUT_MANUAL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetSizeMode(pHeader, XUI_SIZE_FILL, XUI_SIZE_FIXED);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetOverflow(pHeader, XUI_OVERFLOW_CLIP);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetFocusable(pHeader, 0);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetTabStop(pHeader, 0);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pHeader, XUI_EVENT_POINTER_ENTER, __xuiAccordionHeaderEvent, pAccordion);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pHeader, XUI_EVENT_POINTER_LEAVE, __xuiAccordionHeaderEvent, pAccordion);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pHeader, XUI_EVENT_POINTER_MOVE, __xuiAccordionHeaderEvent, pAccordion);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pHeader, XUI_EVENT_POINTER_DOWN, __xuiAccordionHeaderEvent, pAccordion);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pHeader, XUI_EVENT_POINTER_UP, __xuiAccordionHeaderEvent, pAccordion);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pHeader, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiAccordionHeaderEvent, pAccordion);
	return iRet;
}

static int __xuiAccordionConfigureClientWidget(xui_widget pWidget, float fContentPadding)
{
	xui_cache_policy_t tPolicy;
	int iRet;

	if ( pWidget == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiAccordionDefaultCachePolicy(&tPolicy);
	iRet = xuiWidgetSetCachePolicy(pWidget, &tPolicy);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetLayoutType(pWidget, XUI_LAYOUT_COLUMN);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetSizeMode(pWidget, XUI_SIZE_FILL, XUI_SIZE_CONTENT);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetPadding(pWidget, __xuiAccordionThickness(fContentPadding, fContentPadding, fContentPadding, fContentPadding));
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetGap(pWidget, 6.0f);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_CLIP);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetFocusable(pWidget, 0);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetTabStop(pWidget, 0);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetAlign(pWidget, XUI_ALIGN_STRETCH, XUI_ALIGN_START);
	return iRet;
}

static int __xuiAccordionCreateSectionWidgets(xui_widget pAccordion, xui_accordion_data_t* pData, int iIndex)
{
	xui_context pContext;
	xui_widget pSection;
	xui_widget pHeader;
	xui_widget pClient;
	int iRet;
	int bHeaderAdded;
	int bClientAdded;

	if ( (pAccordion == NULL) || (pData == NULL) || (iIndex < 0) || (iIndex >= XUI_ACCORDION_SECTION_CAPACITY) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pContext = xuiWidgetGetContext(pAccordion);
	pSection = NULL;
	pHeader = NULL;
	pClient = NULL;
	bHeaderAdded = 0;
	bClientAdded = 0;
	iRet = xuiWidgetCreate(pContext, &pSection);
	if ( iRet == XUI_OK ) iRet = xuiWidgetCreate(pContext, &pHeader);
	if ( iRet == XUI_OK ) iRet = xuiWidgetCreate(pContext, &pClient);
	if ( iRet == XUI_OK ) iRet = __xuiAccordionConfigureSectionWidget(pSection);
	if ( iRet == XUI_OK ) iRet = __xuiAccordionConfigureHeaderWidget(pAccordion, pHeader);
	if ( iRet == XUI_OK ) iRet = __xuiAccordionConfigureClientWidget(pClient, pData->fContentPadding);
	if ( iRet == XUI_OK ) iRet = xuiWidgetAddChild(pSection, pHeader);
	if ( iRet == XUI_OK ) bHeaderAdded = 1;
	if ( iRet == XUI_OK ) iRet = xuiWidgetAddChild(pSection, pClient);
	if ( iRet == XUI_OK ) bClientAdded = 1;
	if ( iRet == XUI_OK ) iRet = xuiWidgetAddChild(pAccordion, pSection);
	if ( iRet != XUI_OK ) {
		if ( pSection != NULL ) xuiWidgetDestroy(pSection);
		if ( !bHeaderAdded && (pHeader != NULL) ) xuiWidgetDestroy(pHeader);
		if ( !bClientAdded && (pClient != NULL) ) xuiWidgetDestroy(pClient);
		return iRet;
	}
	pData->arrSections[iIndex].pSection = pSection;
	pData->arrSections[iIndex].pHeader = pHeader;
	pData->arrSections[iIndex].pClient = pClient;
	return XUI_OK;
}

static int __xuiAccordionInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_accordion_data_t* pData;
	const xui_accordion_desc_t* pDesc;
	int iRet;
	int i;

	(void)pUser;
	pData = (xui_accordion_data_t*)pTypeData;
	pDesc = (const xui_accordion_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiAccordionDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiAccordionDefaultData(pData);
	if ( pDesc != NULL ) {
		if ( __xuiAccordionModeValid(pDesc->iMode) ) pData->iMode = pDesc->iMode;
		if ( pDesc->pFont != NULL ) pData->pFont = pDesc->pFont;
		if ( pDesc->fHeaderHeight > 0.0f ) pData->fHeaderHeight = pDesc->fHeaderHeight;
		if ( pDesc->fSpacing > 0.0f ) pData->fSpacing = pDesc->fSpacing;
		if ( pDesc->fContentPadding > 0.0f ) pData->fContentPadding = pDesc->fContentPadding;
		if ( __xuiAccordionAlpha(pDesc->iBackgroundColor) != 0 ) pData->iBackgroundColor = pDesc->iBackgroundColor;
		if ( __xuiAccordionAlpha(pDesc->iHeaderColor) != 0 ) pData->iHeaderColor = pDesc->iHeaderColor;
		if ( __xuiAccordionAlpha(pDesc->iHoverColor) != 0 ) pData->iHoverColor = pDesc->iHoverColor;
		if ( __xuiAccordionAlpha(pDesc->iExpandedColor) != 0 ) pData->iExpandedColor = pDesc->iExpandedColor;
		if ( __xuiAccordionAlpha(pDesc->iContentColor) != 0 ) pData->iContentColor = pDesc->iContentColor;
		if ( __xuiAccordionAlpha(pDesc->iBorderColor) != 0 ) pData->iBorderColor = pDesc->iBorderColor;
		if ( __xuiAccordionAlpha(pDesc->iTextColor) != 0 ) pData->iTextColor = pDesc->iTextColor;
		if ( __xuiAccordionAlpha(pDesc->iActiveTextColor) != 0 ) pData->iActiveTextColor = pDesc->iActiveTextColor;
		if ( __xuiAccordionAlpha(pDesc->iDisabledTextColor) != 0 ) pData->iDisabledTextColor = pDesc->iDisabledTextColor;
	}
	(void)xuiWidgetSetLayoutType(pWidget, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_CLIP);
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	if ( (pDesc != NULL) && (pDesc->arrSections != NULL) ) {
		for ( i = 0; i < pDesc->iSectionCount; i++ ) {
			iRet = xuiAccordionAddSection(pWidget, pDesc->arrSections[i].sTitle, pDesc->arrSections[i].iId, pDesc->arrSections[i].bExpanded, NULL);
			if ( iRet != XUI_OK ) return iRet;
			if ( pDesc->arrSections[i].bDisabled ) {
				(void)xuiAccordionSetSectionEnabled(pWidget, i, 0);
			}
		}
	}
	(void)xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiAccordionEvent, NULL);
	(void)xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiAccordionEvent, NULL);
	(void)xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiAccordionEvent, NULL);
	(void)xuiWidgetSetEventHandler(pWidget, XUI_EVENT_ENABLED_CHANGED, __xuiAccordionEvent, NULL);
	return __xuiAccordionSyncSections(pWidget, pData);
}

static void __xuiAccordionDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_accordion_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_accordion_data_t*)pTypeData;
	if ( pData != NULL ) {
		memset(pData, 0, sizeof(*pData));
	}
}

static void __xuiAccordionRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
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

static void __xuiAccordionRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;
	uint32_t iLayoutDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	iLayoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiAccordionRegisterStyleProperty(pContext, pType, "accordion.background.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiAccordionRegisterStyleProperty(pContext, pType, "accordion.header.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiAccordionRegisterStyleProperty(pContext, pType, "accordion.header.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiAccordionRegisterStyleProperty(pContext, pType, "accordion.header.expanded_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiAccordionRegisterStyleProperty(pContext, pType, "accordion.content.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiAccordionRegisterStyleProperty(pContext, pType, "accordion.border.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiAccordionRegisterStyleProperty(pContext, pType, "accordion.text.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiAccordionRegisterStyleProperty(pContext, pType, "accordion.text.active_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiAccordionRegisterStyleProperty(pContext, pType, "accordion.text.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiAccordionRegisterStyleProperty(pContext, pType, "accordion.header.height", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiAccordionRegisterStyleProperty(pContext, pType, "accordion.spacing", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiAccordionRegisterStyleProperty(pContext, pType, "accordion.content.padding", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
}

XUI_API xui_widget_type xuiAccordionGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	xui_cache_policy_t tPolicy;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "accordion");
	if ( pType != NULL ) {
		__xuiAccordionRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "accordion";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_accordion_data_t);
	tDesc.onInit = __xuiAccordionInit;
	tDesc.onDestroy = __xuiAccordionDestroy;
	tDesc.onContentMeasure = __xuiAccordionContentMeasure;
	tDesc.onLayoutArrange = __xuiAccordionArrange;
	tDesc.onCacheRender = __xuiAccordionCacheRender;
	__xuiAccordionDefaultLayout(&tDesc.tLayout);
	__xuiAccordionDefaultCachePolicy(&tPolicy);
	tDesc.tCachePolicy = tPolicy;
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	__xuiAccordionRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiAccordionCreate(xui_context pContext, xui_widget* ppWidget, const xui_accordion_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiAccordionDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiAccordionGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiAccordionSetSelect(xui_widget pWidget, xui_accordion_select_proc onSelect, void* pUser)
{
	xui_accordion_data_t* pData;

	pData = __xuiAccordionGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onSelect = onSelect;
	pData->pSelectUser = pUser;
	return XUI_OK;
}

XUI_API int xuiAccordionClear(xui_widget pWidget)
{
	xui_accordion_data_t* pData;
	int i;

	pData = __xuiAccordionGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < pData->iSectionCount; i++ ) {
		if ( pData->arrSections[i].pSection != NULL ) {
			xuiWidgetDestroy(pData->arrSections[i].pSection);
		}
	}
	memset(pData->arrSections, 0, sizeof(pData->arrSections));
	for ( i = 0; i < XUI_ACCORDION_SECTION_CAPACITY; i++ ) {
		pData->arrSections[i].bEnabled = 1;
	}
	pData->iSectionCount = 0;
	pData->iSelected = -1;
	pData->iHoverIndex = -1;
	pData->iActiveIndex = -1;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiAccordionAddSection(xui_widget pWidget, const char* sTitle, int iId, int bExpanded, xui_widget* ppClient)
{
	xui_accordion_data_t* pData;
	xui_accordion_section_t* pSection;
	int iIndex;
	int iRet;

	if ( ppClient != NULL ) *ppClient = NULL;
	pData = __xuiAccordionGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iSectionCount >= XUI_ACCORDION_SECTION_CAPACITY ) return XUI_ERROR_OUT_OF_MEMORY;
	iIndex = pData->iSectionCount;
	iRet = __xuiAccordionCreateSectionWidgets(pWidget, pData, iIndex);
	if ( iRet != XUI_OK ) return iRet;
	pSection = &pData->arrSections[iIndex];
	__xuiAccordionSetTitle(pSection, sTitle);
	pSection->iId = iId;
	pSection->bExpanded = bExpanded ? 1 : 0;
	pSection->bEnabled = 1;
	pData->iSectionCount++;
	if ( pData->iMode == XUI_ACCORDION_MODE_SINGLE && pSection->bExpanded ) {
		(void)__xuiAccordionSetExpandedInternal(pWidget, pData, iIndex, 1, 0);
	} else if ( pSection->bExpanded && pData->iSelected < 0 ) {
		pData->iSelected = iIndex;
	}
	if ( ppClient != NULL ) *ppClient = pSection->pClient;
	return __xuiAccordionSyncSections(pWidget, pData);
}

XUI_API int xuiAccordionGetSectionCount(xui_widget pWidget)
{
	xui_accordion_data_t* pData;

	pData = __xuiAccordionGetData(pWidget);
	return (pData != NULL) ? pData->iSectionCount : 0;
}

XUI_API xui_widget xuiAccordionGetSectionWidget(xui_widget pWidget, int iIndex)
{
	xui_accordion_data_t* pData;

	pData = __xuiAccordionGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iSectionCount) ) return NULL;
	return pData->arrSections[iIndex].pSection;
}

XUI_API xui_widget xuiAccordionGetHeaderWidget(xui_widget pWidget, int iIndex)
{
	xui_accordion_data_t* pData;

	pData = __xuiAccordionGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iSectionCount) ) return NULL;
	return pData->arrSections[iIndex].pHeader;
}

XUI_API xui_widget xuiAccordionGetButtonWidget(xui_widget pWidget, int iIndex)
{
	return xuiAccordionGetHeaderWidget(pWidget, iIndex);
}

XUI_API xui_widget xuiAccordionGetClientWidget(xui_widget pWidget, int iIndex)
{
	xui_accordion_data_t* pData;

	pData = __xuiAccordionGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iSectionCount) ) return NULL;
	return pData->arrSections[iIndex].pClient;
}

XUI_API int xuiAccordionAddSectionChild(xui_widget pWidget, int iIndex, xui_widget pChild)
{
	xui_widget pClient;

	if ( pChild == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pClient = xuiAccordionGetClientWidget(pWidget, iIndex);
	if ( pClient == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiWidgetAddChild(pClient, pChild);
}

XUI_API int xuiAccordionSetSectionTitle(xui_widget pWidget, int iIndex, const char* sTitle)
{
	xui_accordion_data_t* pData;

	pData = __xuiAccordionGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iSectionCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiAccordionSetTitle(&pData->arrSections[iIndex], sTitle);
	return __xuiAccordionInvalidateSection(&pData->arrSections[iIndex], XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API const char* xuiAccordionGetSectionTitle(xui_widget pWidget, int iIndex)
{
	xui_accordion_data_t* pData;

	pData = __xuiAccordionGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iSectionCount) ) return NULL;
	return pData->arrSections[iIndex].sTitle;
}

XUI_API int xuiAccordionSetSectionId(xui_widget pWidget, int iIndex, int iId)
{
	xui_accordion_data_t* pData;

	pData = __xuiAccordionGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iSectionCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrSections[iIndex].iId = iId;
	return XUI_OK;
}

XUI_API int xuiAccordionGetSectionId(xui_widget pWidget, int iIndex)
{
	xui_accordion_data_t* pData;

	pData = __xuiAccordionGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iSectionCount) ) return 0;
	return pData->arrSections[iIndex].iId;
}

XUI_API int xuiAccordionSetExpanded(xui_widget pWidget, int iIndex, int bExpanded)
{
	xui_accordion_data_t* pData;

	pData = __xuiAccordionGetData(pWidget);
	return __xuiAccordionSetExpandedInternal(pWidget, pData, iIndex, bExpanded, 0);
}

XUI_API int xuiAccordionIsExpanded(xui_widget pWidget, int iIndex)
{
	xui_accordion_data_t* pData;

	pData = __xuiAccordionGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iSectionCount) ) return 0;
	return pData->arrSections[iIndex].bExpanded;
}

XUI_API int xuiAccordionSetSectionEnabled(xui_widget pWidget, int iIndex, int bEnabled)
{
	xui_accordion_data_t* pData;

	pData = __xuiAccordionGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iSectionCount) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrSections[iIndex].bEnabled = bEnabled ? 1 : 0;
	if ( !pData->arrSections[iIndex].bEnabled ) {
		if ( pData->iHoverIndex == iIndex ) pData->iHoverIndex = -1;
		if ( pData->iActiveIndex == iIndex ) pData->iActiveIndex = -1;
	}
	return __xuiAccordionSyncSections(pWidget, pData);
}

XUI_API int xuiAccordionIsSectionEnabled(xui_widget pWidget, int iIndex)
{
	xui_accordion_data_t* pData;

	pData = __xuiAccordionGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iSectionCount) ) return 0;
	return pData->arrSections[iIndex].bEnabled;
}

XUI_API int xuiAccordionSetMode(xui_widget pWidget, int iMode)
{
	xui_accordion_data_t* pData;
	int i;
	int bKeepFound;

	pData = __xuiAccordionGetData(pWidget);
	if ( (pData == NULL) || !__xuiAccordionModeValid(iMode) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iMode = iMode;
	if ( iMode == XUI_ACCORDION_MODE_SINGLE ) {
		bKeepFound = 0;
		for ( i = 0; i < pData->iSectionCount; i++ ) {
			if ( pData->arrSections[i].bExpanded ) {
				if ( !bKeepFound ) {
					bKeepFound = 1;
					pData->iSelected = i;
				} else {
					pData->arrSections[i].bExpanded = 0;
				}
			}
		}
	}
	return __xuiAccordionSyncSections(pWidget, pData);
}

XUI_API int xuiAccordionGetMode(xui_widget pWidget)
{
	xui_accordion_data_t* pData;

	pData = __xuiAccordionGetData(pWidget);
	return (pData != NULL) ? pData->iMode : XUI_ACCORDION_MODE_MULTIPLE;
}

XUI_API int xuiAccordionSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_accordion_data_t* pData;

	pData = __xuiAccordionGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pFont = pFont;
	return __xuiAccordionInvalidateAll(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_font xuiAccordionGetFont(xui_widget pWidget)
{
	xui_accordion_data_t* pData;

	pData = __xuiAccordionGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API int xuiAccordionSetMetrics(xui_widget pWidget, float fHeaderHeight, float fSpacing, float fContentPadding)
{
	xui_accordion_data_t* pData;

	pData = __xuiAccordionGetData(pWidget);
	if ( (pData == NULL) || (fHeaderHeight < 18.0f) || (fSpacing < 0.0f) || (fContentPadding < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fHeaderHeight = fHeaderHeight;
	pData->fSpacing = fSpacing;
	pData->fContentPadding = fContentPadding;
	__xuiAccordionUpdateClientPadding(pData, fContentPadding);
	return __xuiAccordionInvalidateAll(pWidget, pData, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiAccordionGetMetrics(xui_widget pWidget, float* pHeaderHeight, float* pSpacing, float* pContentPadding)
{
	xui_accordion_data_t* pData;

	pData = __xuiAccordionGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pHeaderHeight != NULL ) *pHeaderHeight = pData->fHeaderHeight;
	if ( pSpacing != NULL ) *pSpacing = pData->fSpacing;
	if ( pContentPadding != NULL ) *pContentPadding = pData->fContentPadding;
	return XUI_OK;
}

XUI_API int xuiAccordionSetColors(xui_widget pWidget, uint32_t iBackground, uint32_t iHeader, uint32_t iHover, uint32_t iExpanded, uint32_t iContent, uint32_t iBorder, uint32_t iText, uint32_t iActiveText, uint32_t iDisabledText)
{
	xui_accordion_data_t* pData;

	pData = __xuiAccordionGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iBackgroundColor = iBackground;
	pData->iHeaderColor = iHeader;
	pData->iHoverColor = iHover;
	pData->iExpandedColor = iExpanded;
	pData->iContentColor = iContent;
	pData->iBorderColor = iBorder;
	pData->iTextColor = iText;
	pData->iActiveTextColor = iActiveText;
	pData->iDisabledTextColor = iDisabledText;
	return __xuiAccordionInvalidateAll(pWidget, pData, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiAccordionGetColors(xui_widget pWidget, uint32_t* pBackground, uint32_t* pHeader, uint32_t* pHover, uint32_t* pExpanded, uint32_t* pContent, uint32_t* pBorder, uint32_t* pText, uint32_t* pActiveText, uint32_t* pDisabledText)
{
	xui_accordion_data_t* pData;

	pData = __xuiAccordionGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pBackground != NULL ) *pBackground = pData->iBackgroundColor;
	if ( pHeader != NULL ) *pHeader = pData->iHeaderColor;
	if ( pHover != NULL ) *pHover = pData->iHoverColor;
	if ( pExpanded != NULL ) *pExpanded = pData->iExpandedColor;
	if ( pContent != NULL ) *pContent = pData->iContentColor;
	if ( pBorder != NULL ) *pBorder = pData->iBorderColor;
	if ( pText != NULL ) *pText = pData->iTextColor;
	if ( pActiveText != NULL ) *pActiveText = pData->iActiveTextColor;
	if ( pDisabledText != NULL ) *pDisabledText = pData->iDisabledTextColor;
	return XUI_OK;
}

XUI_API float xuiAccordionGetContentHeight(xui_widget pWidget)
{
	xui_accordion_data_t* pData;

	pData = __xuiAccordionGetData(pWidget);
	return (pData != NULL) ? pData->fContentHeight : 0.0f;
}

XUI_API xui_rect_t xuiAccordionGetSectionRect(xui_widget pWidget, int iIndex)
{
	xui_accordion_data_t* pData;

	pData = __xuiAccordionGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iSectionCount) ) return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	return pData->arrSections[iIndex].tSectionRect;
}

XUI_API xui_rect_t xuiAccordionGetHeaderRect(xui_widget pWidget, int iIndex)
{
	xui_accordion_data_t* pData;

	pData = __xuiAccordionGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iSectionCount) ) return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	return pData->arrSections[iIndex].tHeaderRect;
}

XUI_API xui_rect_t xuiAccordionGetClientRect(xui_widget pWidget, int iIndex)
{
	xui_accordion_data_t* pData;

	pData = __xuiAccordionGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iSectionCount) ) return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	return pData->arrSections[iIndex].tClientRect;
}

XUI_API xui_rect_t xuiAccordionGetArrowRect(xui_widget pWidget, int iIndex)
{
	xui_accordion_data_t* pData;

	pData = __xuiAccordionGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iSectionCount) ) return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	return pData->arrSections[iIndex].tArrowRect;
}

XUI_API int xuiAccordionGetSelected(xui_widget pWidget)
{
	xui_accordion_data_t* pData;

	pData = __xuiAccordionGetData(pWidget);
	return (pData != NULL) ? pData->iSelected : -1;
}

XUI_API int xuiAccordionGetHoverIndex(xui_widget pWidget)
{
	xui_accordion_data_t* pData;

	pData = __xuiAccordionGetData(pWidget);
	return (pData != NULL) ? pData->iHoverIndex : -1;
}

XUI_API int xuiAccordionGetActiveIndex(xui_widget pWidget)
{
	xui_accordion_data_t* pData;

	pData = __xuiAccordionGetData(pWidget);
	return (pData != NULL) ? pData->iActiveIndex : -1;
}

XUI_API uint32_t xuiAccordionGetState(xui_widget pWidget)
{
	xui_accordion_data_t* pData;
	uint32_t iState;

	pData = __xuiAccordionGetData(pWidget);
	if ( pData == NULL ) return 0;
	iState = 0;
	if ( pData->iHoverIndex >= 0 ) iState |= XUI_WIDGET_STATE_HOVER;
	if ( pData->iActiveIndex >= 0 ) iState |= XUI_WIDGET_STATE_ACTIVE;
	if ( xuiGetFocusWidget(xuiWidgetGetContext(pWidget)) == pWidget ) iState |= XUI_WIDGET_STATE_FOCUS;
	if ( !xuiWidgetGetEnabled(pWidget) ) iState |= XUI_WIDGET_STATE_DISABLED;
	return iState;
}

XUI_API int xuiAccordionGetChangeCount(xui_widget pWidget)
{
	xui_accordion_data_t* pData;

	pData = __xuiAccordionGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}
