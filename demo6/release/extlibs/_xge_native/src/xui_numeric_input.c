#include "xui_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XUI_NUMERIC_INPUT_MAX_TEXT	96

typedef struct xui_numeric_input_data_t {
	xui_widget pInput;
	xui_font pFont;
	xui_numeric_input_change_proc onChange;
	void* pChangeUser;
	xui_numeric_input_error_proc onError;
	void* pErrorUser;
	xui_numeric_input_format_proc onFormat;
	void* pFormatUser;
	float fMin;
	float fMax;
	float fValue;
	float fStep;
	float fSpinnerWidth;
	int iPrecision;
	int bInteger;
	int bReadonly;
	int bError;
	int bSpinnerVisible;
	int bPointerInside;
	int iHoverButton;
	int iActiveButton;
	int iChangeCount;
	uint32_t iTextColor;
	uint32_t iPlaceholderColor;
	uint32_t iDisabledTextColor;
	uint32_t iBackgroundColor;
	uint32_t iHoverBackgroundColor;
	uint32_t iDisabledBackgroundColor;
	uint32_t iBorderColor;
	uint32_t iHoverBorderColor;
	uint32_t iFocusBorderColor;
	uint32_t iErrorBackgroundColor;
	uint32_t iErrorBorderColor;
	uint32_t iSelectionColor;
	uint32_t iCursorColor;
	uint32_t iSpinnerColor;
	uint32_t iSpinnerHoverColor;
	uint32_t iSpinnerActiveColor;
	uint32_t iSpinnerBorderColor;
	uint32_t iSpinnerIconColor;
	uint32_t iSpinnerDisabledIconColor;
	float fBorderWidth;
	xui_rect_t tSpinnerRect;
	xui_rect_t tButtonUpRect;
	xui_rect_t tButtonDownRect;
} xui_numeric_input_data_t;

static int __xuiNumericInputDescValid(const xui_numeric_input_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) {
		return 0;
	}
	if ( (pDesc->iPrecision < 0) ||
	     (pDesc->fStep < 0.0f) ||
	     (pDesc->fSpinnerWidth < 0.0f) ||
	     (pDesc->fBorderWidth < 0.0f) ) {
		return 0;
	}
	return 1;
}

static float __xuiNumericInputMinFloat(float fA, float fB)
{
	return (fA < fB) ? fA : fB;
}

static float __xuiNumericInputMaxFloat(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static float __xuiNumericInputAbsFloat(float fValue)
{
	return (fValue < 0.0f) ? -fValue : fValue;
}

static float __xuiNumericInputClampFloat(float fValue, float fMin, float fMax)
{
	if ( fValue < fMin ) return fMin;
	if ( fValue > fMax ) return fMax;
	return fValue;
}

static float __xuiNumericInputRoundFloat(float fValue)
{
	int iValue;

	if ( fValue >= 0.0f ) {
		iValue = (int)(fValue + 0.5f);
	} else {
		iValue = (int)(fValue - 0.5f);
	}
	return (float)iValue;
}

static int __xuiNumericInputAlpha(uint32_t iColor)
{
	return (int)(iColor & 0xffu);
}

static uint32_t __xuiNumericInputColorWithAlpha(uint32_t iColor, uint32_t iAlpha)
{
	return (iColor & 0xffffff00u) | (iAlpha & 0xffu);
}

static void __xuiNumericInputNormalizeRange(float* pMin, float* pMax)
{
	float fSwap;

	if ( (*pMax) < (*pMin) ) {
		fSwap = *pMin;
		*pMin = *pMax;
		*pMax = fSwap;
	}
	if ( (*pMax) == (*pMin) ) {
		*pMax = (*pMin) + 1.0f;
	}
}

static int __xuiNumericInputPrecision(int iPrecision)
{
	if ( iPrecision < 0 ) return 0;
	if ( iPrecision > 9 ) return 9;
	return iPrecision;
}

static int __xuiNumericInputRectContains(xui_rect_t tRect, float fX, float fY)
{
	return (fX >= tRect.fX) &&
	       (fY >= tRect.fY) &&
	       (fX < tRect.fX + tRect.fW) &&
	       (fY < tRect.fY + tRect.fH);
}

static xui_numeric_input_data_t* __xuiNumericInputGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return NULL;
	}
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "numeric_input");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_numeric_input_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiNumericInputStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
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

static int __xuiNumericInputStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
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

static xui_font __xuiNumericInputStyleFont(xui_widget pWidget, xui_font pBaseFont)
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

static void __xuiNumericInputResolve(xui_widget pWidget, const xui_numeric_input_data_t* pData, xui_numeric_input_data_t* pResolved)
{
	*pResolved = *pData;
	pResolved->pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	(void)__xuiNumericInputStyleColor(pWidget, "numeric_input.background.color", &pResolved->iBackgroundColor);
	(void)__xuiNumericInputStyleColor(pWidget, "numeric_input.background.hover_color", &pResolved->iHoverBackgroundColor);
	(void)__xuiNumericInputStyleColor(pWidget, "numeric_input.background.disabled_color", &pResolved->iDisabledBackgroundColor);
	(void)__xuiNumericInputStyleColor(pWidget, "numeric_input.border.color", &pResolved->iBorderColor);
	(void)__xuiNumericInputStyleColor(pWidget, "numeric_input.border.hover_color", &pResolved->iHoverBorderColor);
	(void)__xuiNumericInputStyleColor(pWidget, "numeric_input.border.focus_color", &pResolved->iFocusBorderColor);
	(void)__xuiNumericInputStyleColor(pWidget, "numeric_input.error.background_color", &pResolved->iErrorBackgroundColor);
	(void)__xuiNumericInputStyleColor(pWidget, "numeric_input.error.border_color", &pResolved->iErrorBorderColor);
	(void)__xuiNumericInputStyleColor(pWidget, "numeric_input.spinner.color", &pResolved->iSpinnerColor);
	(void)__xuiNumericInputStyleColor(pWidget, "numeric_input.spinner.hover_color", &pResolved->iSpinnerHoverColor);
	(void)__xuiNumericInputStyleColor(pWidget, "numeric_input.spinner.active_color", &pResolved->iSpinnerActiveColor);
	(void)__xuiNumericInputStyleColor(pWidget, "numeric_input.spinner.border_color", &pResolved->iSpinnerBorderColor);
	(void)__xuiNumericInputStyleColor(pWidget, "numeric_input.spinner.icon_color", &pResolved->iSpinnerIconColor);
	(void)__xuiNumericInputStyleColor(pWidget, "numeric_input.spinner.icon_disabled_color", &pResolved->iSpinnerDisabledIconColor);
	(void)__xuiNumericInputStyleFloat(pWidget, "numeric_input.border.width", &pResolved->fBorderWidth);
	(void)__xuiNumericInputStyleFloat(pWidget, "numeric_input.spinner.width", &pResolved->fSpinnerWidth);
	pResolved->pFont = __xuiNumericInputStyleFont(pWidget, pResolved->pFont);
	if ( pResolved->fBorderWidth < 0.0f ) pResolved->fBorderWidth = 0.0f;
	if ( pResolved->fSpinnerWidth < 0.0f ) pResolved->fSpinnerWidth = 0.0f;
}

static int __xuiNumericInputDrawRectFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( (__xuiNumericInputAlpha(iColor) == 0) || (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XUI_OK;
	}
	tRect = xuiInternalSnapRect(tRect);
	return (pProxy->drawRectFill != NULL) ? pProxy->drawRectFill(pProxy, pDraw, tRect, iColor) : XUI_OK;
}

static int __xuiNumericInputDrawRectStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	if ( (fWidth <= 0.0f) || (__xuiNumericInputAlpha(iColor) == 0) || (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XUI_OK;
	}
	tRect = xuiInternalStrokeCenterRectInside(tRect, fWidth, NULL);
	return (pProxy->drawRectStroke != NULL) ? pProxy->drawRectStroke(pProxy, pDraw, tRect, xuiInternalSnapSize(fWidth), iColor) : XUI_OK;
}

static int __xuiNumericInputDrawLine(xui_proxy pProxy, xui_draw_context pDraw, float fX0, float fY0, float fX1, float fY1, uint32_t iColor)
{
	if ( (__xuiNumericInputAlpha(iColor) == 0) || (pProxy->drawLine == NULL) ) {
		return XUI_OK;
	}
	return pProxy->drawLine(pProxy, pDraw,
		xuiInternalSnapPixel(fX0), xuiInternalSnapPixel(fY0),
		xuiInternalSnapPixel(fX1), xuiInternalSnapPixel(fY1),
		1.0f, iColor);
}

static int __xuiNumericInputDrawArrow(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, int iUp, uint32_t iColor)
{
	xui_vec2_t tA;
	xui_vec2_t tB;
	xui_vec2_t tC;
	float fCX;
	float fCY;
	float fW;
	float fH;

	if ( (__xuiNumericInputAlpha(iColor) == 0) || (pProxy->drawTriangleFill == NULL) ) {
		return XUI_OK;
	}
	fCX = tRect.fX + tRect.fW * 0.5f;
	fCY = tRect.fY + tRect.fH * 0.5f;
	fW = __xuiNumericInputMinFloat(tRect.fW * 0.32f, 6.0f);
	fH = __xuiNumericInputMinFloat(tRect.fH * 0.28f, 4.0f);
	if ( fW < 3.0f ) fW = 3.0f;
	if ( fH < 2.0f ) fH = 2.0f;
	if ( iUp ) {
		tA = (xui_vec2_t){xuiInternalSnapPixel(fCX), xuiInternalSnapPixel(fCY - fH * 0.55f)};
		tB = (xui_vec2_t){xuiInternalSnapPixel(fCX - fW), xuiInternalSnapPixel(fCY + fH * 0.55f)};
		tC = (xui_vec2_t){xuiInternalSnapPixel(fCX + fW), xuiInternalSnapPixel(fCY + fH * 0.55f)};
	} else {
		tA = (xui_vec2_t){xuiInternalSnapPixel(fCX - fW), xuiInternalSnapPixel(fCY - fH * 0.55f)};
		tB = (xui_vec2_t){xuiInternalSnapPixel(fCX + fW), xuiInternalSnapPixel(fCY - fH * 0.55f)};
		tC = (xui_vec2_t){xuiInternalSnapPixel(fCX), xuiInternalSnapPixel(fCY + fH * 0.55f)};
	}
	return pProxy->drawTriangleFill(pProxy, pDraw, tA, tB, tC, iColor);
}

static void __xuiNumericInputFormatValue(xui_widget pWidget, xui_numeric_input_data_t* pData, char* sBuffer, int iCapacity)
{
	char sFormat[16];
	int iRet;

	if ( (sBuffer == NULL) || (iCapacity <= 0) ) {
		return;
	}
	sBuffer[0] = '\0';
	if ( (pData != NULL) && (pData->onFormat != NULL) ) {
		iRet = pData->onFormat(pData->fValue, sBuffer, iCapacity, pData->pFormatUser);
		if ( iRet > 0 ) {
			sBuffer[iCapacity - 1] = '\0';
			return;
		}
	}
	if ( (pData != NULL) && pData->bInteger ) {
		snprintf(sBuffer, (size_t)iCapacity, "%d", (int)__xuiNumericInputRoundFloat(pData->fValue));
	} else {
		snprintf(sFormat, sizeof(sFormat), "%%.%df", (pData != NULL) ? pData->iPrecision : 3);
		snprintf(sBuffer, (size_t)iCapacity, sFormat, (pData != NULL) ? pData->fValue : 0.0f);
	}
	(void)pWidget;
	sBuffer[iCapacity - 1] = '\0';
}

static int __xuiNumericInputSetInputTextFromValue(xui_widget pWidget, xui_numeric_input_data_t* pData)
{
	char sBuffer[XUI_NUMERIC_INPUT_MAX_TEXT];

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pInput == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiNumericInputFormatValue(pWidget, pData, sBuffer, (int)sizeof(sBuffer));
	return xuiInputSetText(pData->pInput, sBuffer);
}

static int __xuiNumericInputSyncInputStyle(xui_widget pWidget, xui_numeric_input_data_t* pData)
{
	xui_numeric_input_data_t tResolved;
	uint32_t iText;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pInput == NULL) ) {
		return XUI_OK;
	}
	__xuiNumericInputResolve(pWidget, pData, &tResolved);
	iText = xuiWidgetGetEnabled(pWidget) ? tResolved.iTextColor : tResolved.iDisabledTextColor;
	(void)xuiInputSetColors(pData->pInput, XUI_COLOR_RGBA(0, 0, 0, 0), iText, XUI_COLOR_RGBA(0, 0, 0, 0), XUI_COLOR_RGBA(0, 0, 0, 0));
	(void)xuiInputSetErrorColors(pData->pInput, XUI_COLOR_RGBA(0, 0, 0, 0), XUI_COLOR_RGBA(0, 0, 0, 0));
	(void)xuiInputSetExtendedColors(pData->pInput,
		tResolved.iPlaceholderColor,
		tResolved.iDisabledTextColor,
		XUI_COLOR_RGBA(0, 0, 0, 0),
		XUI_COLOR_RGBA(0, 0, 0, 0),
		XUI_COLOR_RGBA(0, 0, 0, 0),
		tResolved.iSelectionColor,
		tResolved.iCursorColor);
	(void)xuiInputSetBorderWidth(pData->pInput, 0.0f);
	(void)xuiInputSetReadonly(pData->pInput, pData->bReadonly);
	(void)xuiInputSetError(pData->pInput, pData->bError);
	(void)xuiInputSetTextAlign(pData->pInput, XUI_INPUT_ALIGN_RIGHT);
	if ( xuiInputGetFont(pData->pInput) != tResolved.pFont ) {
		(void)xuiInputSetFont(pData->pInput, tResolved.pFont);
	}
	return XUI_OK;
}

static int __xuiNumericInputSetErrorInternal(xui_widget pWidget, xui_numeric_input_data_t* pData, int bError)
{
	bError = bError ? 1 : 0;
	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->bError != bError ) {
		pData->bError = bError;
		if ( pData->pInput != NULL ) {
			(void)xuiInputSetError(pData->pInput, bError);
		}
		if ( pData->onError != NULL ) {
			pData->onError(pWidget, bError, pData->pErrorUser);
		}
		return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return XUI_OK;
}

static int __xuiNumericInputSetValueInternal(xui_widget pWidget, xui_numeric_input_data_t* pData, float fValue, int bNotify)
{
	int bChanged;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( fValue != fValue ) {
		fValue = pData->fMin;
	}
	fValue = __xuiNumericInputClampFloat(fValue, pData->fMin, pData->fMax);
	if ( pData->bInteger ) {
		fValue = __xuiNumericInputRoundFloat(fValue);
		fValue = __xuiNumericInputClampFloat(fValue, pData->fMin, pData->fMax);
	}
	bChanged = (pData->fValue != fValue);
	pData->fValue = fValue;
	iRet = __xuiNumericInputSetErrorInternal(pWidget, pData, 0);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiNumericInputSetInputTextFromValue(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	if ( bChanged ) {
		pData->iChangeCount++;
		if ( bNotify && (pData->onChange != NULL) ) {
			pData->onChange(pWidget, pData->fValue, pData->pChangeUser);
		}
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiNumericInputParseText(const char* sText, float* pValue)
{
	char* sEnd;
	double fValue;

	if ( (sText == NULL) || (pValue == NULL) ) {
		return 0;
	}
	while ( isspace((unsigned char)*sText) ) {
		sText++;
	}
	if ( *sText == '\0' ) {
		return 0;
	}
	sEnd = NULL;
	fValue = strtod(sText, &sEnd);
	if ( sEnd == sText ) {
		return 0;
	}
	while ( (sEnd != NULL) && isspace((unsigned char)*sEnd) ) {
		sEnd++;
	}
	if ( (sEnd == NULL) || (*sEnd != '\0') || (fValue != fValue) ) {
		return 0;
	}
	*pValue = (float)fValue;
	return 1;
}

static int __xuiNumericInputCommitInternal(xui_widget pWidget, xui_numeric_input_data_t* pData, int bNotify)
{
	const char* sText;
	float fValue;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pInput == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	sText = xuiInputGetText(pData->pInput);
	if ( !__xuiNumericInputParseText(sText, &fValue) ) {
		return __xuiNumericInputSetErrorInternal(pWidget, pData, 1);
	}
	return __xuiNumericInputSetValueInternal(pWidget, pData, fValue, bNotify);
}

static int __xuiNumericInputCanUserChange(xui_widget pWidget, const xui_numeric_input_data_t* pData)
{
	return (pWidget != NULL) &&
	       (pData != NULL) &&
	       xuiWidgetGetEnabled(pWidget) &&
	       !pData->bReadonly;
}

static void __xuiNumericInputUpdateRects(xui_widget pWidget, xui_numeric_input_data_t* pData, const xui_numeric_input_data_t* pResolved, xui_rect_t* pInputRect)
{
	xui_rect_t tContent;
	xui_rect_t tInput;
	float fSpinnerW;
	float fHalf;

	if ( (pWidget == NULL) || (pData == NULL) || (pResolved == NULL) ) {
		return;
	}
	tContent = xuiWidgetGetContentRect(pWidget);
	fSpinnerW = (pResolved->bSpinnerVisible != 0) ? pResolved->fSpinnerWidth : 0.0f;
	if ( fSpinnerW < 0.0f ) fSpinnerW = 0.0f;
	if ( fSpinnerW > tContent.fW ) fSpinnerW = tContent.fW;
	pData->tSpinnerRect = (xui_rect_t){tContent.fX + tContent.fW - fSpinnerW, tContent.fY, fSpinnerW, tContent.fH};
	fHalf = pData->tSpinnerRect.fH * 0.5f;
	pData->tButtonUpRect = (xui_rect_t){pData->tSpinnerRect.fX, pData->tSpinnerRect.fY, pData->tSpinnerRect.fW, fHalf};
	pData->tButtonDownRect = (xui_rect_t){pData->tSpinnerRect.fX, pData->tSpinnerRect.fY + fHalf, pData->tSpinnerRect.fW, pData->tSpinnerRect.fH - fHalf};
	tInput = tContent;
	tInput.fW -= fSpinnerW;
	if ( tInput.fW < 0.0f ) tInput.fW = 0.0f;
	tInput = xuiInternalSnapRect(tInput);
	pData->tSpinnerRect = xuiInternalSnapRect(pData->tSpinnerRect);
	pData->tButtonUpRect = xuiInternalSnapRect(pData->tButtonUpRect);
	pData->tButtonDownRect = xuiInternalSnapRect(pData->tButtonDownRect);
	if ( pInputRect != NULL ) {
		*pInputRect = tInput;
	}
}

static int __xuiNumericInputButtonAtLocal(xui_numeric_input_data_t* pData, float fLocalX, float fLocalY)
{
	if ( (pData == NULL) || !pData->bSpinnerVisible || (pData->tSpinnerRect.fW <= 0.0f) ) {
		return XUI_NUMERIC_INPUT_BUTTON_NONE;
	}
	if ( __xuiNumericInputRectContains(pData->tButtonUpRect, fLocalX, fLocalY) ) {
		return XUI_NUMERIC_INPUT_BUTTON_UP;
	}
	if ( __xuiNumericInputRectContains(pData->tButtonDownRect, fLocalX, fLocalY) ) {
		return XUI_NUMERIC_INPUT_BUTTON_DOWN;
	}
	return XUI_NUMERIC_INPUT_BUTTON_NONE;
}

static int __xuiNumericInputEventButton(xui_widget pWidget, xui_numeric_input_data_t* pData, const xui_event_t* pEvent)
{
	xui_numeric_input_data_t tResolved;
	xui_rect_t tWorld;
	float fLocalX;
	float fLocalY;

	if ( (pWidget == NULL) || (pData == NULL) || (pEvent == NULL) ) {
		return XUI_NUMERIC_INPUT_BUTTON_NONE;
	}
	__xuiNumericInputResolve(pWidget, pData, &tResolved);
	__xuiNumericInputUpdateRects(pWidget, pData, &tResolved, NULL);
	tWorld = xuiWidgetGetWorldRect(pWidget);
	fLocalX = pEvent->fX - tWorld.fX;
	fLocalY = pEvent->fY - tWorld.fY;
	pData->bPointerInside = __xuiNumericInputRectContains(xuiWidgetGetContentRect(pWidget), fLocalX, fLocalY);
	return __xuiNumericInputButtonAtLocal(pData, fLocalX, fLocalY);
}

static int __xuiNumericInputButtonEnabledData(const xui_numeric_input_data_t* pData, int iButton)
{
	if ( (pData == NULL) || !pData->bSpinnerVisible ) {
		return 0;
	}
	if ( iButton == XUI_NUMERIC_INPUT_BUTTON_UP ) {
		return pData->fValue < pData->fMax;
	}
	if ( iButton == XUI_NUMERIC_INPUT_BUTTON_DOWN ) {
		return pData->fValue > pData->fMin;
	}
	return 0;
}

static int __xuiNumericInputStepInternal(xui_widget pWidget, xui_numeric_input_data_t* pData, int iDirection, int bNotify)
{
	int iRet;
	float fDelta;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( !__xuiNumericInputCanUserChange(pWidget, pData) || (iDirection == 0) ) {
		return XUI_OK;
	}
	iRet = __xuiNumericInputCommitInternal(pWidget, pData, 0);
	if ( iRet != XUI_OK ) return iRet;
	if ( pData->bError ) {
		return XUI_OK;
	}
	fDelta = (iDirection > 0) ? pData->fStep : -pData->fStep;
	return __xuiNumericInputSetValueInternal(pWidget, pData, pData->fValue + fDelta, bNotify);
}

static void __xuiNumericInputCancelAction(xui_widget pWidget, void* pUser)
{
	xui_numeric_input_data_t* pData;

	(void)pUser;
	pData = __xuiNumericInputGetData(pWidget);
	if ( pData == NULL ) {
		return;
	}
	pData->iActiveButton = XUI_NUMERIC_INPUT_BUTTON_NONE;
	if ( xuiGetPointerCapture(xuiWidgetGetContext(pWidget)) == pWidget ) {
		(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
	}
	(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiNumericInputPointerDown(xui_widget pWidget, xui_numeric_input_data_t* pData, const xui_event_t* pEvent)
{
	xui_context pContext;
	int iButton;

	if ( (pEvent->iButton != XUI_POINTER_BUTTON_LEFT) || (pEvent->iPhase == XUI_EVENT_PHASE_BUBBLE) ) {
		return XUI_OK;
	}
	iButton = __xuiNumericInputEventButton(pWidget, pData, pEvent);
	if ( iButton == XUI_NUMERIC_INPUT_BUTTON_NONE ) {
		pData->iHoverButton = XUI_NUMERIC_INPUT_BUTTON_NONE;
		return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	pContext = xuiWidgetGetContext(pWidget);
	if ( pData->pInput != NULL ) {
		(void)xuiSetFocusWidget(pContext, pData->pInput);
	}
	pData->iHoverButton = iButton;
	if ( __xuiNumericInputCanUserChange(pWidget, pData) && __xuiNumericInputButtonEnabledData(pData, iButton) ) {
		pData->iActiveButton = iButton;
		(void)xuiSetPointerCapture(pContext, pWidget);
	}
	(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiNumericInputPointerMove(xui_widget pWidget, xui_numeric_input_data_t* pData, const xui_event_t* pEvent)
{
	int iButton;

	if ( pEvent->iPhase == XUI_EVENT_PHASE_BUBBLE ) {
		return XUI_OK;
	}
	iButton = __xuiNumericInputEventButton(pWidget, pData, pEvent);
	if ( pData->iHoverButton != iButton ) {
		pData->iHoverButton = iButton;
		return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return XUI_OK;
}

static int __xuiNumericInputPointerUp(xui_widget pWidget, xui_numeric_input_data_t* pData, const xui_event_t* pEvent)
{
	xui_context pContext;
	int iButton;
	int iActive;
	int iRet;

	if ( pEvent->iButton != XUI_POINTER_BUTTON_LEFT ) {
		return XUI_OK;
	}
	pContext = xuiWidgetGetContext(pWidget);
	iActive = pData->iActiveButton;
	iButton = __xuiNumericInputEventButton(pWidget, pData, pEvent);
	pData->iActiveButton = XUI_NUMERIC_INPUT_BUTTON_NONE;
	if ( xuiGetPointerCapture(pContext) == pWidget ) {
		(void)xuiReleasePointerCapture(pContext, pWidget);
	}
	if ( (iActive != XUI_NUMERIC_INPUT_BUTTON_NONE) &&
	     (iActive == iButton) &&
	     __xuiNumericInputButtonEnabledData(pData, iActive) ) {
		iRet = __xuiNumericInputStepInternal(pWidget, pData, (iActive == XUI_NUMERIC_INPUT_BUTTON_UP) ? 1 : -1, 1);
		if ( iRet != XUI_OK ) return iRet;
	}
	(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return (iActive != XUI_NUMERIC_INPUT_BUTTON_NONE) ? XUI_EVENT_DISPATCH_STOP : XUI_OK;
}

static int __xuiNumericInputWheel(xui_widget pWidget, xui_numeric_input_data_t* pData, const xui_event_t* pEvent)
{
	float fDelta;
	int iRet;

	if ( !__xuiNumericInputCanUserChange(pWidget, pData) || (pEvent->iPhase == XUI_EVENT_PHASE_BUBBLE) ) {
		return XUI_OK;
	}
	fDelta = (__xuiNumericInputAbsFloat(pEvent->fWheelY) >= __xuiNumericInputAbsFloat(pEvent->fWheelX)) ? pEvent->fWheelY : pEvent->fWheelX;
	if ( fDelta == 0.0f ) {
		return XUI_OK;
	}
	iRet = __xuiNumericInputStepInternal(pWidget, pData, (fDelta > 0.0f) ? 1 : -1, 1);
	if ( iRet != XUI_OK ) return iRet;
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiNumericInputKeyDown(xui_widget pWidget, xui_numeric_input_data_t* pData, const xui_event_t* pEvent)
{
	int iRet;

	if ( pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE ) {
		return XUI_OK;
	}
	switch ( pEvent->iKey ) {
	case XUI_KEY_UP:
		iRet = __xuiNumericInputStepInternal(pWidget, pData, 1, 1);
		if ( iRet != XUI_OK ) return iRet;
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_DOWN:
		iRet = __xuiNumericInputStepInternal(pWidget, pData, -1, 1);
		if ( iRet != XUI_OK ) return iRet;
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_ENTER:
		iRet = __xuiNumericInputCommitInternal(pWidget, pData, 1);
		if ( iRet != XUI_OK ) return iRet;
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_ESCAPE:
		pData->iActiveButton = XUI_NUMERIC_INPUT_BUTTON_NONE;
		if ( xuiGetPointerCapture(xuiWidgetGetContext(pWidget)) == pWidget ) {
			(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
		}
		(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		return XUI_OK;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiNumericInputEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_numeric_input_data_t* pData;
	int iRet;

	(void)pUser;
	if ( (pWidget == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiNumericInputGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_ENTER:
	case XUI_EVENT_POINTER_MOVE:
		return __xuiNumericInputPointerMove(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_LEAVE:
		if ( pEvent->iPhase != XUI_EVENT_PHASE_CAPTURE ) {
			pData->bPointerInside = 0;
			pData->iHoverButton = XUI_NUMERIC_INPUT_BUTTON_NONE;
			return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		}
		break;
	case XUI_EVENT_POINTER_DOWN:
		return __xuiNumericInputPointerDown(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_UP:
		return __xuiNumericInputPointerUp(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_CLICK:
		if ( __xuiNumericInputEventButton(pWidget, pData, pEvent) != XUI_NUMERIC_INPUT_BUTTON_NONE ) {
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_WHEEL:
		return __xuiNumericInputWheel(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		pData->iActiveButton = XUI_NUMERIC_INPUT_BUTTON_NONE;
		return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	case XUI_EVENT_FOCUS:
		if ( pEvent->iPhase != XUI_EVENT_PHASE_CAPTURE ) {
			return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		}
		break;
	case XUI_EVENT_BLUR:
		if ( pEvent->iPhase != XUI_EVENT_PHASE_CAPTURE ) {
			iRet = __xuiNumericInputCommitInternal(pWidget, pData, 1);
			if ( iRet != XUI_OK ) return iRet;
			return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		}
		break;
	case XUI_EVENT_KEY_DOWN:
		return __xuiNumericInputKeyDown(pWidget, pData, pEvent);
	case XUI_EVENT_ENABLED_CHANGED:
		(void)__xuiNumericInputSyncInputStyle(pWidget, pData);
		pData->iActiveButton = XUI_NUMERIC_INPUT_BUTTON_NONE;
		return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	case XUI_EVENT_VISIBLE_CHANGED:
		pData->iActiveButton = XUI_NUMERIC_INPUT_BUTTON_NONE;
		return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiNumericInputContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_numeric_input_data_t* pData;
	xui_numeric_input_data_t tResolved;
	xui_font_metrics_t tMetrics;
	xui_proxy pProxy;

	(void)tConstraint;
	pData = (xui_numeric_input_data_t*)pUser;
	if ( (pWidget == NULL) || (pData == NULL) || (pSize == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiNumericInputResolve(pWidget, pData, &tResolved);
	pSize->fX = 128.0f;
	pSize->fY = 26.0f;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( (pProxy != NULL) && (pProxy->fontGetMetrics != NULL) && (tResolved.pFont != NULL) &&
	     (pProxy->fontGetMetrics(pProxy, tResolved.pFont, &tMetrics) == XUI_OK) &&
	     (tMetrics.fLineHeight > 0.0f) ) {
		pSize->fY = tMetrics.fLineHeight + 8.0f;
	}
	if ( pSize->fY < 24.0f ) pSize->fY = 24.0f;
	if ( pSize->fX < tResolved.fSpinnerWidth + 72.0f ) {
		pSize->fX = tResolved.fSpinnerWidth + 72.0f;
	}
	pSize->fX = xuiInternalSnapSize(pSize->fX);
	pSize->fY = xuiInternalSnapSize(pSize->fY);
	return XUI_OK;
}

static int __xuiNumericInputLayoutArrange(xui_widget pWidget, xui_rect_t tContentRect, void* pUser)
{
	xui_numeric_input_data_t* pData;
	xui_numeric_input_data_t tResolved;
	xui_rect_t tInput;

	(void)tContentRect;
	pData = (xui_numeric_input_data_t*)pUser;
	if ( (pWidget == NULL) || (pData == NULL) || (pData->pInput == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiNumericInputResolve(pWidget, pData, &tResolved);
	__xuiNumericInputUpdateRects(pWidget, pData, &tResolved, &tInput);
	return xuiWidgetArrange(pData->pInput, tInput);
}

static uint32_t __xuiNumericInputState(xui_widget pWidget, xui_numeric_input_data_t* pData)
{
	uint32_t iState;
	xui_context pContext;
	xui_widget pFocus;

	iState = xuiWidgetGetInputState(pWidget);
	if ( pData != NULL && pData->bPointerInside ) {
		iState |= XUI_WIDGET_STATE_HOVER;
	}
	if ( !xuiWidgetGetEnabled(pWidget) ) {
		iState |= XUI_WIDGET_STATE_DISABLED;
	}
	if ( (pData != NULL) && pData->bError ) {
		iState |= XUI_NUMERIC_INPUT_STATE_ERROR;
	}
	if ( (pData != NULL) && pData->bReadonly ) {
		iState |= XUI_NUMERIC_INPUT_STATE_READONLY;
	}
	if ( pData != NULL ) {
		pContext = xuiWidgetGetContext(pWidget);
		pFocus = xuiGetFocusWidget(pContext);
		if ( (pFocus == pData->pInput) || (pFocus == pWidget) ) {
			iState |= XUI_WIDGET_STATE_FOCUS;
		}
	}
	return iState;
}

static int __xuiNumericInputDrawSpinnerButton(xui_widget pWidget, xui_draw_context pDraw, xui_proxy pProxy, xui_numeric_input_data_t* pData, const xui_numeric_input_data_t* pResolved, int iButton)
{
	xui_rect_t tRect;
	uint32_t iFill;
	uint32_t iIcon;
	int iEnabled;
	int iRet;

	(void)pWidget;
	tRect = (iButton == XUI_NUMERIC_INPUT_BUTTON_UP) ? pData->tButtonUpRect : pData->tButtonDownRect;
	iEnabled = __xuiNumericInputButtonEnabledData(pData, iButton) && xuiWidgetGetEnabled(pWidget) && !pData->bReadonly;
	iFill = pResolved->iSpinnerColor;
	if ( iEnabled && (pData->iActiveButton == iButton) ) {
		iFill = pResolved->iSpinnerActiveColor;
	} else if ( iEnabled && (pData->iHoverButton == iButton) ) {
		iFill = pResolved->iSpinnerHoverColor;
	}
	iRet = __xuiNumericInputDrawRectFill(pProxy, pDraw, tRect, iFill);
	if ( iRet != XUI_OK ) return iRet;
	iIcon = iEnabled ? pResolved->iSpinnerIconColor : pResolved->iSpinnerDisabledIconColor;
	return __xuiNumericInputDrawArrow(pProxy, pDraw, tRect, iButton == XUI_NUMERIC_INPUT_BUTTON_UP, iIcon);
}

static int __xuiNumericInputCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_numeric_input_data_t* pData;
	xui_numeric_input_data_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tRect;
	uint32_t iState;
	uint32_t iBackground;
	uint32_t iBorder;
	int iRet;

	(void)iStateId;
	(void)pUser;
	if ( (pWidget == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiNumericInputGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	__xuiNumericInputResolve(pWidget, pData, &tResolved);
	__xuiNumericInputUpdateRects(pWidget, pData, &tResolved, NULL);
	iState = __xuiNumericInputState(pWidget, pData);
	tRect = xuiWidgetGetRect(pWidget);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	tRect = xuiInternalSnapRect(tRect);
	iBackground = tResolved.iBackgroundColor;
	iBorder = tResolved.iBorderColor;
	if ( (iState & XUI_WIDGET_STATE_DISABLED) != 0 ) {
		iBackground = tResolved.iDisabledBackgroundColor;
		iBorder = __xuiNumericInputColorWithAlpha(tResolved.iBorderColor, 120);
	} else if ( (iState & XUI_NUMERIC_INPUT_STATE_ERROR) != 0 ) {
		iBackground = tResolved.iErrorBackgroundColor;
		iBorder = tResolved.iErrorBorderColor;
	} else if ( (iState & XUI_WIDGET_STATE_FOCUS) != 0 ) {
		iBorder = tResolved.iFocusBorderColor;
	} else if ( (iState & XUI_WIDGET_STATE_HOVER) != 0 ) {
		iBackground = tResolved.iHoverBackgroundColor;
		iBorder = tResolved.iHoverBorderColor;
	}
	iRet = __xuiNumericInputDrawRectFill(pProxy, pDraw, tRect, iBackground);
	if ( iRet != XUI_OK ) return iRet;
	if ( pData->bSpinnerVisible && pData->tSpinnerRect.fW > 0.0f ) {
		iRet = __xuiNumericInputDrawSpinnerButton(pWidget, pDraw, pProxy, pData, &tResolved, XUI_NUMERIC_INPUT_BUTTON_UP);
		if ( iRet != XUI_OK ) return iRet;
		iRet = __xuiNumericInputDrawSpinnerButton(pWidget, pDraw, pProxy, pData, &tResolved, XUI_NUMERIC_INPUT_BUTTON_DOWN);
		if ( iRet != XUI_OK ) return iRet;
		iRet = __xuiNumericInputDrawLine(pProxy, pDraw, pData->tSpinnerRect.fX, pData->tSpinnerRect.fY + 1.0f, pData->tSpinnerRect.fX, pData->tSpinnerRect.fY + pData->tSpinnerRect.fH - 1.0f, tResolved.iSpinnerBorderColor);
		if ( iRet != XUI_OK ) return iRet;
		iRet = __xuiNumericInputDrawLine(pProxy, pDraw, pData->tButtonDownRect.fX + 1.0f, pData->tButtonDownRect.fY, pData->tButtonDownRect.fX + pData->tButtonDownRect.fW - 1.0f, pData->tButtonDownRect.fY, tResolved.iSpinnerBorderColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	iRet = __xuiNumericInputDrawRectStroke(pProxy, pDraw, tRect, tResolved.fBorderWidth, iBorder);
	if ( iRet != XUI_OK ) return iRet;
	return XUI_OK;
}

static void __xuiNumericInputDefaultLayout(xui_layout_t* pLayout)
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

static void __xuiNumericInputDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiNumericInputInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetCancelAction(pWidget, __xuiNumericInputCancelAction, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_ENTER, __xuiNumericInputEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiNumericInputEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiNumericInputEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiNumericInputEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiNumericInputEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CLICK, __xuiNumericInputEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_WHEEL, __xuiNumericInputEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiNumericInputEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiNumericInputEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiNumericInputEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiNumericInputEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_ENABLED_CHANGED, __xuiNumericInputEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_VISIBLE_CHANGED, __xuiNumericInputEvent, NULL);
	return iRet;
}

static int __xuiNumericInputCreateInputChild(xui_widget pWidget, xui_numeric_input_data_t* pData, const xui_numeric_input_desc_t* pDesc)
{
	xui_input_desc_t tInputDesc;
	int iRet;

	memset(&tInputDesc, 0, sizeof(tInputDesc));
	tInputDesc.iSize = sizeof(tInputDesc);
	tInputDesc.sPlaceholder = (pDesc != NULL) ? pDesc->sPlaceholder : "";
	tInputDesc.pFont = pData->pFont;
	tInputDesc.iMaxLength = XUI_NUMERIC_INPUT_MAX_TEXT - 1;
	tInputDesc.iTextAlign = XUI_INPUT_ALIGN_RIGHT;
	tInputDesc.bReadonly = pData->bReadonly;
	tInputDesc.bError = pData->bError;
	tInputDesc.iTextColor = pData->iTextColor;
	tInputDesc.iPlaceholderColor = pData->iPlaceholderColor;
	tInputDesc.iDisabledTextColor = pData->iDisabledTextColor;
	tInputDesc.iSelectionColor = pData->iSelectionColor;
	tInputDesc.iCursorColor = pData->iCursorColor;
	iRet = xuiInputCreate(xuiWidgetGetContext(pWidget), &pData->pInput, &tInputDesc);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiWidgetSetTabStop(pData->pInput, 1);
	(void)xuiWidgetSetFocusable(pData->pInput, 1);
	iRet = xuiWidgetAddChild(pWidget, pData->pInput);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pData->pInput);
		pData->pInput = NULL;
		return iRet;
	}
	iRet = __xuiNumericInputSyncInputStyle(pWidget, pData);
	if ( iRet == XUI_OK ) {
		iRet = __xuiNumericInputSetInputTextFromValue(pWidget, pData);
	}
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pData->pInput);
		pData->pInput = NULL;
	}
	return iRet;
}

static int __xuiNumericInputInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_numeric_input_data_t* pData;
	const xui_numeric_input_desc_t* pDesc;
	float fMin;
	float fMax;
	float fValue;
	int iRet;

	(void)pUser;
	pData = (xui_numeric_input_data_t*)pTypeData;
	pDesc = (const xui_numeric_input_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiNumericInputDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(pData, 0, sizeof(*pData));
	fMin = (pDesc != NULL) ? pDesc->fMin : 0.0f;
	fMax = (pDesc != NULL) ? pDesc->fMax : 100.0f;
	if ( (pDesc == NULL) || ((fMin == 0.0f) && (fMax == 0.0f)) ) {
		fMax = 100.0f;
	}
	__xuiNumericInputNormalizeRange(&fMin, &fMax);
	fValue = (pDesc != NULL) ? pDesc->fValue : fMin;
	pData->pFont = (pDesc != NULL) ? pDesc->pFont : NULL;
	pData->fMin = fMin;
	pData->fMax = fMax;
	pData->fValue = __xuiNumericInputClampFloat(fValue, fMin, fMax);
	pData->fStep = (pDesc != NULL && pDesc->fStep > 0.0f) ? pDesc->fStep : 1.0f;
	pData->iPrecision = (pDesc != NULL && pDesc->iPrecision > 0) ? __xuiNumericInputPrecision(pDesc->iPrecision) : 3;
	pData->bInteger = (pDesc != NULL && pDesc->bInteger != 0) ? 1 : 0;
	if ( pData->bInteger ) {
		pData->fValue = __xuiNumericInputClampFloat(__xuiNumericInputRoundFloat(pData->fValue), pData->fMin, pData->fMax);
	}
	pData->bReadonly = (pDesc != NULL && pDesc->bReadonly != 0) ? 1 : 0;
	pData->bSpinnerVisible = (pDesc == NULL || pDesc->bSpinnerVisible >= 0) ? 1 : 0;
	pData->fSpinnerWidth = (pDesc != NULL && pDesc->fSpinnerWidth > 0.0f) ? pDesc->fSpinnerWidth : 22.0f;
	pData->iTextColor = (pDesc != NULL && pDesc->iTextColor != 0) ? pDesc->iTextColor : XUI_COLOR_RGBA(36, 48, 66, 255);
	pData->iPlaceholderColor = (pDesc != NULL && pDesc->iPlaceholderColor != 0) ? pDesc->iPlaceholderColor : XUI_COLOR_RGBA(135, 148, 166, 255);
	pData->iDisabledTextColor = (pDesc != NULL && pDesc->iDisabledTextColor != 0) ? pDesc->iDisabledTextColor : XUI_COLOR_RGBA(150, 160, 172, 255);
	pData->iBackgroundColor = (pDesc != NULL && pDesc->iBackgroundColor != 0) ? pDesc->iBackgroundColor : XUI_COLOR_RGBA(255, 255, 255, 255);
	pData->iHoverBackgroundColor = (pDesc != NULL && pDesc->iHoverBackgroundColor != 0) ? pDesc->iHoverBackgroundColor : XUI_COLOR_RGBA(249, 252, 255, 255);
	pData->iDisabledBackgroundColor = (pDesc != NULL && pDesc->iDisabledBackgroundColor != 0) ? pDesc->iDisabledBackgroundColor : XUI_COLOR_RGBA(242, 245, 249, 255);
	pData->iBorderColor = (pDesc != NULL && pDesc->iBorderColor != 0) ? pDesc->iBorderColor : XUI_COLOR_RGBA(176, 190, 207, 255);
	pData->iHoverBorderColor = (pDesc != NULL && pDesc->iHoverBorderColor != 0) ? pDesc->iHoverBorderColor : XUI_COLOR_RGBA(105, 166, 226, 255);
	pData->iFocusBorderColor = (pDesc != NULL && pDesc->iFocusBorderColor != 0) ? pDesc->iFocusBorderColor : XUI_COLOR_RGBA(47, 128, 237, 255);
	pData->iErrorBackgroundColor = (pDesc != NULL && pDesc->iErrorBackgroundColor != 0) ? pDesc->iErrorBackgroundColor : XUI_COLOR_RGBA(255, 248, 248, 255);
	pData->iErrorBorderColor = (pDesc != NULL && pDesc->iErrorBorderColor != 0) ? pDesc->iErrorBorderColor : XUI_COLOR_RGBA(220, 72, 72, 255);
	pData->iSelectionColor = (pDesc != NULL && pDesc->iSelectionColor != 0) ? pDesc->iSelectionColor : XUI_COLOR_RGBA(47, 128, 237, 78);
	pData->iCursorColor = (pDesc != NULL && pDesc->iCursorColor != 0) ? pDesc->iCursorColor : XUI_COLOR_RGBA(33, 94, 170, 255);
	pData->iSpinnerColor = (pDesc != NULL && pDesc->iSpinnerColor != 0) ? pDesc->iSpinnerColor : XUI_COLOR_RGBA(246, 250, 255, 255);
	pData->iSpinnerHoverColor = (pDesc != NULL && pDesc->iSpinnerHoverColor != 0) ? pDesc->iSpinnerHoverColor : XUI_COLOR_RGBA(231, 241, 252, 255);
	pData->iSpinnerActiveColor = (pDesc != NULL && pDesc->iSpinnerActiveColor != 0) ? pDesc->iSpinnerActiveColor : XUI_COLOR_RGBA(215, 232, 249, 255);
	pData->iSpinnerBorderColor = (pDesc != NULL && pDesc->iSpinnerBorderColor != 0) ? pDesc->iSpinnerBorderColor : XUI_COLOR_RGBA(205, 220, 236, 255);
	pData->iSpinnerIconColor = (pDesc != NULL && pDesc->iSpinnerIconColor != 0) ? pDesc->iSpinnerIconColor : XUI_COLOR_RGBA(68, 86, 110, 255);
	pData->iSpinnerDisabledIconColor = (pDesc != NULL && pDesc->iSpinnerDisabledIconColor != 0) ? pDesc->iSpinnerDisabledIconColor : XUI_COLOR_RGBA(157, 169, 184, 150);
	pData->fBorderWidth = (pDesc != NULL && pDesc->fBorderWidth > 0.0f) ? pDesc->fBorderWidth : 1.0f;
	pData->iHoverButton = XUI_NUMERIC_INPUT_BUTTON_NONE;
	pData->iActiveButton = XUI_NUMERIC_INPUT_BUTTON_NONE;
	(void)xuiWidgetSetPadding(pWidget, (xui_thickness_t){0.0f, 0.0f, 0.0f, 0.0f});
	(void)xuiWidgetSetFocusable(pWidget, 0);
	(void)xuiWidgetSetTabStop(pWidget, 0);
	iRet = __xuiNumericInputInitEvents(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiNumericInputCreateInputChild(pWidget, pData, pDesc);
}

static void __xuiNumericInputDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_numeric_input_data_t* pData;

	(void)pUser;
	pData = (xui_numeric_input_data_t*)pTypeData;
	if ( pData == NULL ) {
		return;
	}
	if ( (pWidget != NULL) && (xuiGetPointerCapture(xuiWidgetGetContext(pWidget)) == pWidget) ) {
		(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
	}
	memset(pData, 0, sizeof(*pData));
}

static void __xuiNumericInputRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
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

static void __xuiNumericInputRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;
	uint32_t iLayoutDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	iLayoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiNumericInputRegisterStyleProperty(pContext, pType, "numeric_input.background.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiNumericInputRegisterStyleProperty(pContext, pType, "numeric_input.background.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiNumericInputRegisterStyleProperty(pContext, pType, "numeric_input.background.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiNumericInputRegisterStyleProperty(pContext, pType, "numeric_input.border.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiNumericInputRegisterStyleProperty(pContext, pType, "numeric_input.border.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiNumericInputRegisterStyleProperty(pContext, pType, "numeric_input.border.focus_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiNumericInputRegisterStyleProperty(pContext, pType, "numeric_input.error.background_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiNumericInputRegisterStyleProperty(pContext, pType, "numeric_input.error.border_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiNumericInputRegisterStyleProperty(pContext, pType, "numeric_input.spinner.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiNumericInputRegisterStyleProperty(pContext, pType, "numeric_input.spinner.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiNumericInputRegisterStyleProperty(pContext, pType, "numeric_input.spinner.active_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiNumericInputRegisterStyleProperty(pContext, pType, "numeric_input.spinner.border_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiNumericInputRegisterStyleProperty(pContext, pType, "numeric_input.spinner.icon_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiNumericInputRegisterStyleProperty(pContext, pType, "numeric_input.spinner.icon_disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiNumericInputRegisterStyleProperty(pContext, pType, "numeric_input.border.width", XUI_STYLE_VALUE_FLOAT, iPaintDirty, 0);
	__xuiNumericInputRegisterStyleProperty(pContext, pType, "numeric_input.spinner.width", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiNumericInputRegisterStyleProperty(pContext, pType, "font.name", XUI_STYLE_VALUE_STRING, iLayoutDirty, XUI_STYLE_PROPERTY_INHERITED);
}

XUI_API xui_widget_type xuiNumericInputGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "numeric_input");
	if ( pType != NULL ) {
		__xuiNumericInputRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "numeric_input";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_numeric_input_data_t);
	tDesc.onInit = __xuiNumericInputInit;
	tDesc.onDestroy = __xuiNumericInputDestroy;
	tDesc.onContentMeasure = __xuiNumericInputContentMeasure;
	tDesc.onLayoutArrange = __xuiNumericInputLayoutArrange;
	tDesc.onCacheRender = __xuiNumericInputCacheRender;
	__xuiNumericInputDefaultLayout(&tDesc.tLayout);
	__xuiNumericInputDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	__xuiNumericInputRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiNumericInputCreate(xui_context pContext, xui_widget* ppWidget, const xui_numeric_input_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiNumericInputDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiNumericInputGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiNumericInputSetChange(xui_widget pWidget, xui_numeric_input_change_proc onChange, void* pUser)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onChange = onChange;
	pData->pChangeUser = pUser;
	return XUI_OK;
}

XUI_API int xuiNumericInputSetErrorChange(xui_widget pWidget, xui_numeric_input_error_proc onError, void* pUser)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onError = onError;
	pData->pErrorUser = pUser;
	return XUI_OK;
}

XUI_API int xuiNumericInputSetFormatter(xui_widget pWidget, xui_numeric_input_format_proc onFormat, void* pUser)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onFormat = onFormat;
	pData->pFormatUser = pUser;
	return __xuiNumericInputSetInputTextFromValue(pWidget, pData);
}

XUI_API int xuiNumericInputSetRange(xui_widget pWidget, float fMin, float fMax)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiNumericInputNormalizeRange(&fMin, &fMax);
	pData->fMin = fMin;
	pData->fMax = fMax;
	return __xuiNumericInputSetValueInternal(pWidget, pData, pData->fValue, 0);
}

XUI_API int xuiNumericInputGetRange(xui_widget pWidget, float* pMin, float* pMax)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pMin != NULL ) *pMin = pData->fMin;
	if ( pMax != NULL ) *pMax = pData->fMax;
	return XUI_OK;
}

XUI_API int xuiNumericInputSetStep(xui_widget pWidget, float fStep)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	if ( (pData == NULL) || (fStep < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fStep = (fStep > 0.0f) ? fStep : 1.0f;
	return XUI_OK;
}

XUI_API float xuiNumericInputGetStep(xui_widget pWidget)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	return (pData != NULL) ? pData->fStep : 0.0f;
}

XUI_API int xuiNumericInputSetInteger(xui_widget pWidget, int bInteger)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bInteger = bInteger ? 1 : 0;
	return __xuiNumericInputSetValueInternal(pWidget, pData, pData->fValue, 0);
}

XUI_API int xuiNumericInputIsInteger(xui_widget pWidget)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	return (pData != NULL) ? pData->bInteger : 0;
}

XUI_API int xuiNumericInputSetPrecision(xui_widget pWidget, int iPrecision)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iPrecision = __xuiNumericInputPrecision(iPrecision);
	return __xuiNumericInputSetInputTextFromValue(pWidget, pData);
}

XUI_API int xuiNumericInputGetPrecision(xui_widget pWidget)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	return (pData != NULL) ? pData->iPrecision : 0;
}

XUI_API int xuiNumericInputSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pFont = pFont;
	(void)__xuiNumericInputSyncInputStyle(pWidget, pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_font xuiNumericInputGetFont(xui_widget pWidget)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API int xuiNumericInputSetReadonly(xui_widget pWidget, int bReadonly)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bReadonly = bReadonly ? 1 : 0;
	(void)__xuiNumericInputSyncInputStyle(pWidget, pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiNumericInputIsReadonly(xui_widget pWidget)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	return (pData != NULL) ? pData->bReadonly : 0;
}

XUI_API int xuiNumericInputSetSpinnerVisible(xui_widget pWidget, int bVisible)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bSpinnerVisible = bVisible ? 1 : 0;
	pData->iHoverButton = XUI_NUMERIC_INPUT_BUTTON_NONE;
	pData->iActiveButton = XUI_NUMERIC_INPUT_BUTTON_NONE;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiNumericInputGetSpinnerVisible(xui_widget pWidget)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	return (pData != NULL) ? pData->bSpinnerVisible : 0;
}

XUI_API int xuiNumericInputSetSpinnerWidth(xui_widget pWidget, float fWidth)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	if ( (pData == NULL) || (fWidth < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fSpinnerWidth = (fWidth > 0.0f) ? fWidth : 22.0f;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiNumericInputGetSpinnerWidth(xui_widget pWidget)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	return (pData != NULL) ? pData->fSpinnerWidth : 0.0f;
}

XUI_API int xuiNumericInputSetColors(xui_widget pWidget, uint32_t iBackground, uint32_t iText, uint32_t iBorder, uint32_t iFocus)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	int iRet;

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iBackgroundColor = iBackground;
	pData->iTextColor = iText;
	pData->iBorderColor = iBorder;
	pData->iFocusBorderColor = iFocus;
	iRet = __xuiNumericInputSyncInputStyle(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiNumericInputGetColors(xui_widget pWidget, uint32_t* pBackground, uint32_t* pText, uint32_t* pBorder, uint32_t* pFocus)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pBackground != NULL ) *pBackground = pData->iBackgroundColor;
	if ( pText != NULL ) *pText = pData->iTextColor;
	if ( pBorder != NULL ) *pBorder = pData->iBorderColor;
	if ( pFocus != NULL ) *pFocus = pData->iFocusBorderColor;
	return XUI_OK;
}

XUI_API int xuiNumericInputSetExtendedColors(xui_widget pWidget, uint32_t iPlaceholder, uint32_t iDisabledText, uint32_t iHoverBackground, uint32_t iDisabledBackground, uint32_t iHoverBorder, uint32_t iErrorBackground, uint32_t iErrorBorder, uint32_t iSelection, uint32_t iCursor)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	int iRet;

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iPlaceholderColor = iPlaceholder;
	pData->iDisabledTextColor = iDisabledText;
	pData->iHoverBackgroundColor = iHoverBackground;
	pData->iDisabledBackgroundColor = iDisabledBackground;
	pData->iHoverBorderColor = iHoverBorder;
	pData->iErrorBackgroundColor = iErrorBackground;
	pData->iErrorBorderColor = iErrorBorder;
	pData->iSelectionColor = iSelection;
	pData->iCursorColor = iCursor;
	iRet = __xuiNumericInputSyncInputStyle(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiNumericInputGetExtendedColors(xui_widget pWidget, uint32_t* pPlaceholder, uint32_t* pDisabledText, uint32_t* pHoverBackground, uint32_t* pDisabledBackground, uint32_t* pHoverBorder, uint32_t* pErrorBackground, uint32_t* pErrorBorder, uint32_t* pSelection, uint32_t* pCursor)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pPlaceholder != NULL ) *pPlaceholder = pData->iPlaceholderColor;
	if ( pDisabledText != NULL ) *pDisabledText = pData->iDisabledTextColor;
	if ( pHoverBackground != NULL ) *pHoverBackground = pData->iHoverBackgroundColor;
	if ( pDisabledBackground != NULL ) *pDisabledBackground = pData->iDisabledBackgroundColor;
	if ( pHoverBorder != NULL ) *pHoverBorder = pData->iHoverBorderColor;
	if ( pErrorBackground != NULL ) *pErrorBackground = pData->iErrorBackgroundColor;
	if ( pErrorBorder != NULL ) *pErrorBorder = pData->iErrorBorderColor;
	if ( pSelection != NULL ) *pSelection = pData->iSelectionColor;
	if ( pCursor != NULL ) *pCursor = pData->iCursorColor;
	return XUI_OK;
}

XUI_API int xuiNumericInputSetBorderWidth(xui_widget pWidget, float fBorderWidth)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);

	if ( (pData == NULL) || (fBorderWidth < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fBorderWidth = fBorderWidth;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiNumericInputGetBorderWidth(xui_widget pWidget)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	return (pData != NULL) ? pData->fBorderWidth : 0.0f;
}

XUI_API int xuiNumericInputSetSpinnerColors(xui_widget pWidget, uint32_t iColor, uint32_t iHover, uint32_t iActive, uint32_t iBorder, uint32_t iIcon, uint32_t iDisabledIcon)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iSpinnerColor = iColor;
	pData->iSpinnerHoverColor = iHover;
	pData->iSpinnerActiveColor = iActive;
	pData->iSpinnerBorderColor = iBorder;
	pData->iSpinnerIconColor = iIcon;
	pData->iSpinnerDisabledIconColor = iDisabledIcon;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiNumericInputGetSpinnerColors(xui_widget pWidget, uint32_t* pColor, uint32_t* pHover, uint32_t* pActive, uint32_t* pBorder, uint32_t* pIcon, uint32_t* pDisabledIcon)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pColor != NULL ) *pColor = pData->iSpinnerColor;
	if ( pHover != NULL ) *pHover = pData->iSpinnerHoverColor;
	if ( pActive != NULL ) *pActive = pData->iSpinnerActiveColor;
	if ( pBorder != NULL ) *pBorder = pData->iSpinnerBorderColor;
	if ( pIcon != NULL ) *pIcon = pData->iSpinnerIconColor;
	if ( pDisabledIcon != NULL ) *pDisabledIcon = pData->iSpinnerDisabledIconColor;
	return XUI_OK;
}

XUI_API int xuiNumericInputSetValue(xui_widget pWidget, float fValue)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiNumericInputSetValueInternal(pWidget, pData, fValue, 0);
}

XUI_API float xuiNumericInputGetValue(xui_widget pWidget)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	return (pData != NULL) ? pData->fValue : 0.0f;
}

XUI_API int xuiNumericInputCommit(xui_widget pWidget)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiNumericInputCommitInternal(pWidget, pData, 1);
}

XUI_API int xuiNumericInputStep(xui_widget pWidget, int iDirection)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiNumericInputStepInternal(pWidget, pData, iDirection, 1);
}

XUI_API int xuiNumericInputSetText(xui_widget pWidget, const char* sText)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	if ( (pData == NULL) || (pData->pInput == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiInputSetText(pData->pInput, sText);
}

XUI_API const char* xuiNumericInputGetText(xui_widget pWidget)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	return (pData != NULL && pData->pInput != NULL) ? xuiInputGetText(pData->pInput) : "";
}

XUI_API int xuiNumericInputSetMenuTitle(xui_widget pWidget, int iCommand, const char* sTitle)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	if ( (pData == NULL) || (pData->pInput == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiInputSetMenuTitle(pData->pInput, iCommand, sTitle);
}

XUI_API const char* xuiNumericInputGetMenuTitle(xui_widget pWidget, int iCommand)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	return (pData != NULL && pData->pInput != NULL) ? xuiInputGetMenuTitle(pData->pInput, iCommand) : "";
}

XUI_API int xuiNumericInputOpenMenu(xui_widget pWidget, float fX, float fY)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	int iRet;
	if ( (pData == NULL) || (pData->pInput == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = __xuiNumericInputSyncInputStyle(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	return xuiInputOpenMenu(pData->pInput, fX, fY);
}

XUI_API xui_widget xuiNumericInputGetMenuWidget(xui_widget pWidget)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	return (pData != NULL && pData->pInput != NULL) ? xuiInputGetMenuWidget(pData->pInput) : NULL;
}

XUI_API xui_widget xuiNumericInputGetInputWidget(xui_widget pWidget)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	return (pData != NULL) ? pData->pInput : NULL;
}

XUI_API xui_rect_t xuiNumericInputGetSpinnerRect(xui_widget pWidget)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	xui_numeric_input_data_t tResolved;
	if ( pData == NULL ) return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	__xuiNumericInputResolve(pWidget, pData, &tResolved);
	__xuiNumericInputUpdateRects(pWidget, pData, &tResolved, NULL);
	return pData->tSpinnerRect;
}

XUI_API xui_rect_t xuiNumericInputGetButtonRect(xui_widget pWidget, int iButton)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	xui_numeric_input_data_t tResolved;
	if ( pData == NULL ) return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	__xuiNumericInputResolve(pWidget, pData, &tResolved);
	__xuiNumericInputUpdateRects(pWidget, pData, &tResolved, NULL);
	if ( iButton == XUI_NUMERIC_INPUT_BUTTON_UP ) return pData->tButtonUpRect;
	if ( iButton == XUI_NUMERIC_INPUT_BUTTON_DOWN ) return pData->tButtonDownRect;
	return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API int xuiNumericInputGetHoverButton(xui_widget pWidget)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	return (pData != NULL) ? pData->iHoverButton : XUI_NUMERIC_INPUT_BUTTON_NONE;
}

XUI_API int xuiNumericInputGetActiveButton(xui_widget pWidget)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	return (pData != NULL) ? pData->iActiveButton : XUI_NUMERIC_INPUT_BUTTON_NONE;
}

XUI_API int xuiNumericInputIsButtonEnabled(xui_widget pWidget, int iButton)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	if ( pData == NULL ) return 0;
	return xuiWidgetGetEnabled(pWidget) && !pData->bReadonly && __xuiNumericInputButtonEnabledData(pData, iButton);
}

XUI_API int xuiNumericInputGetError(xui_widget pWidget)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	return (pData != NULL) ? pData->bError : 0;
}

XUI_API uint32_t xuiNumericInputGetState(xui_widget pWidget)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	return (pData != NULL) ? __xuiNumericInputState(pWidget, pData) : 0u;
}

XUI_API int xuiNumericInputGetChangeCount(xui_widget pWidget)
{
	xui_numeric_input_data_t* pData = __xuiNumericInputGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}
