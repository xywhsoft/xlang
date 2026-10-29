#include "xui_internal.h"

#include <string.h>

typedef struct xui_slider_data_t {
	xui_slider_change_proc onChange;
	void* pChangeUser;
	float fMin;
	float fMax;
	float fValue;
	float fStep;
	float fPageStep;
	float fTrackSize;
	float fKnobSize;
	float fTrackRadius;
	uint32_t iTrackColor;
	uint32_t iFillColor;
	uint32_t iFillHoverColor;
	uint32_t iFillActiveColor;
	uint32_t iKnobColor;
	uint32_t iKnobBorderColor;
	uint32_t iFocusColor;
	uint32_t iDisabledColor;
	int iOrientation;
	int bDragging;
	int iChangeCount;
	xui_rect_t tTrackRect;
	xui_rect_t tFillRect;
	xui_rect_t tKnobRect;
} xui_slider_data_t;

static int __xuiSliderDescValid(const xui_slider_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	return (pDesc->iSize == 0) || (pDesc->iSize >= sizeof(*pDesc));
}

static int __xuiSliderOrientationValid(int iOrientation)
{
	return (iOrientation == XUI_ORIENTATION_HORIZONTAL) || (iOrientation == XUI_ORIENTATION_VERTICAL);
}

static float __xuiSliderClampFloat(float fValue, float fMin, float fMax)
{
	if ( fValue < fMin ) return fMin;
	if ( fValue > fMax ) return fMax;
	return fValue;
}

static float __xuiSliderAbsFloat(float fValue)
{
	return (fValue < 0.0f) ? -fValue : fValue;
}

static float __xuiSliderMinFloat(float fA, float fB)
{
	return (fA < fB) ? fA : fB;
}

static float __xuiSliderMaxFloat(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static void __xuiSliderNormalizeRange(float* pMin, float* pMax)
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

static uint32_t __xuiSliderColorWithAlpha(uint32_t iColor, uint32_t iAlpha)
{
	return (iColor & 0xffffff00u) | (iAlpha & 0xffu);
}

static int __xuiSliderColorAlpha(uint32_t iColor)
{
	return (int)(iColor & 0xffu);
}

static uint32_t __xuiSliderLighten(uint32_t iColor, int iAmount)
{
	int iR;
	int iG;
	int iB;
	uint32_t iA;

	iR = (int)((iColor >> 24) & 0xffu) + iAmount;
	iG = (int)((iColor >> 16) & 0xffu) + iAmount;
	iB = (int)((iColor >> 8) & 0xffu) + iAmount;
	iA = iColor & 0xffu;
	if ( iR < 0 ) iR = 0;
	if ( iR > 255 ) iR = 255;
	if ( iG < 0 ) iG = 0;
	if ( iG > 255 ) iG = 255;
	if ( iB < 0 ) iB = 0;
	if ( iB > 255 ) iB = 255;
	return XUI_COLOR_RGBA(iR, iG, iB, iA);
}

static float __xuiSliderRateFromData(const xui_slider_data_t* pData)
{
	float fRange;

	if ( pData == NULL ) {
		return 0.0f;
	}
	fRange = pData->fMax - pData->fMin;
	if ( fRange <= 0.0f ) {
		return 0.0f;
	}
	return __xuiSliderClampFloat((pData->fValue - pData->fMin) / fRange, 0.0f, 1.0f);
}

static float __xuiSliderSmallStep(const xui_slider_data_t* pData)
{
	float fRange;
	float fStep;

	if ( pData == NULL ) {
		return 1.0f;
	}
	if ( pData->fStep > 0.0f ) {
		return pData->fStep;
	}
	fRange = pData->fMax - pData->fMin;
	fStep = (fRange > 0.0f) ? (fRange * 0.01f) : 1.0f;
	if ( (fRange >= 1.0f) && (fStep < 1.0f) ) {
		fStep = 1.0f;
	}
	return (fStep > 0.0f) ? fStep : 1.0f;
}

static float __xuiSliderPageStep(const xui_slider_data_t* pData)
{
	float fRange;
	float fStep;

	if ( pData == NULL ) {
		return 1.0f;
	}
	if ( pData->fPageStep > 0.0f ) {
		return pData->fPageStep;
	}
	fRange = pData->fMax - pData->fMin;
	fStep = (fRange > 0.0f) ? (fRange * 0.10f) : __xuiSliderSmallStep(pData);
	if ( (fRange >= 1.0f) && (fStep < 1.0f) ) {
		fStep = 1.0f;
	}
	return (fStep > 0.0f) ? fStep : 1.0f;
}

static xui_slider_data_t* __xuiSliderGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return NULL;
	}
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "slider");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_slider_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiSliderStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
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

static int __xuiSliderStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
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

static void __xuiSliderResolve(xui_widget pWidget, const xui_slider_data_t* pData, xui_slider_data_t* pResolved)
{
	*pResolved = *pData;
	(void)__xuiSliderStyleColor(pWidget, "slider.track.color", &pResolved->iTrackColor);
	(void)__xuiSliderStyleColor(pWidget, "slider.fill.color", &pResolved->iFillColor);
	(void)__xuiSliderStyleColor(pWidget, "slider.fill.hover_color", &pResolved->iFillHoverColor);
	(void)__xuiSliderStyleColor(pWidget, "slider.fill.active_color", &pResolved->iFillActiveColor);
	(void)__xuiSliderStyleColor(pWidget, "slider.knob.color", &pResolved->iKnobColor);
	(void)__xuiSliderStyleColor(pWidget, "slider.knob.border_color", &pResolved->iKnobBorderColor);
	(void)__xuiSliderStyleColor(pWidget, "slider.focus.color", &pResolved->iFocusColor);
	(void)__xuiSliderStyleColor(pWidget, "slider.disabled.color", &pResolved->iDisabledColor);
	(void)__xuiSliderStyleFloat(pWidget, "slider.track.size", &pResolved->fTrackSize);
	(void)__xuiSliderStyleFloat(pWidget, "slider.knob.size", &pResolved->fKnobSize);
	(void)__xuiSliderStyleFloat(pWidget, "slider.track.radius", &pResolved->fTrackRadius);
	if ( pResolved->fTrackSize <= 0.0f ) {
		pResolved->fTrackSize = 4.0f;
	}
	if ( pResolved->fKnobSize <= 0.0f ) {
		pResolved->fKnobSize = 14.0f;
	}
	if ( pResolved->fTrackRadius < -1.0f ) {
		pResolved->fTrackRadius = -1.0f;
	}
}

static uint32_t __xuiSliderBaseState(xui_widget pWidget, const xui_slider_data_t* pData)
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
	if ( (pData != NULL) && (pData->bDragging != 0) ) {
		iState |= XUI_WIDGET_STATE_ACTIVE;
	}
	return iState;
}

static uint32_t __xuiSliderStateId(xui_widget pWidget, const xui_slider_data_t* pData)
{
	uint32_t iState;

	iState = __xuiSliderBaseState(pWidget, pData);
	if ( (iState & XUI_WIDGET_STATE_DISABLED) != 0 ) {
		return XUI_WIDGET_STATE_DISABLED;
	}
	if ( (iState & XUI_WIDGET_STATE_ACTIVE) != 0 ) {
		return XUI_WIDGET_STATE_ACTIVE;
	}
	if ( (iState & XUI_WIDGET_STATE_HOVER) != 0 ) {
		return XUI_WIDGET_STATE_HOVER;
	}
	if ( (iState & XUI_WIDGET_STATE_FOCUS) != 0 ) {
		return XUI_WIDGET_STATE_FOCUS;
	}
	return 0;
}

static int __xuiSliderSyncState(xui_widget pWidget, xui_slider_data_t* pData)
{
	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return xuiWidgetSetStateId(pWidget, __xuiSliderStateId(pWidget, pData));
}

static int __xuiSliderRectContains(xui_rect_t tRect, float fX, float fY)
{
	return (tRect.fW > 0.0f) &&
	       (tRect.fH > 0.0f) &&
	       (fX >= tRect.fX) &&
	       (fY >= tRect.fY) &&
	       (fX < (tRect.fX + tRect.fW)) &&
	       (fY < (tRect.fY + tRect.fH));
}

static xui_rect_t __xuiSliderTrackRect(const xui_slider_data_t* pData, xui_rect_t tContent)
{
	xui_rect_t tTrack;
	float fCrossSize;
	float fInset;

	tTrack = tContent;
	fCrossSize = (pData->fTrackSize > 0.0f) ? pData->fTrackSize : 4.0f;
	if ( pData->iOrientation == XUI_ORIENTATION_HORIZONTAL ) {
		if ( fCrossSize > tContent.fH ) fCrossSize = tContent.fH;
		fInset = __xuiSliderMinFloat(pData->fKnobSize * 0.5f, tContent.fW * 0.5f);
		tTrack.fX = tContent.fX + fInset;
		tTrack.fW = tContent.fW - fInset * 2.0f;
		if ( tTrack.fW < 1.0f ) {
			tTrack.fX = tContent.fX + tContent.fW * 0.5f - 0.5f;
			tTrack.fW = 1.0f;
		}
		tTrack.fY = tContent.fY + (tContent.fH - fCrossSize) * 0.5f;
		tTrack.fH = fCrossSize;
	} else {
		if ( fCrossSize > tContent.fW ) fCrossSize = tContent.fW;
		fInset = __xuiSliderMinFloat(pData->fKnobSize * 0.5f, tContent.fH * 0.5f);
		tTrack.fY = tContent.fY + fInset;
		tTrack.fH = tContent.fH - fInset * 2.0f;
		if ( tTrack.fH < 1.0f ) {
			tTrack.fY = tContent.fY + tContent.fH * 0.5f - 0.5f;
			tTrack.fH = 1.0f;
		}
		tTrack.fX = tContent.fX + (tContent.fW - fCrossSize) * 0.5f;
		tTrack.fW = fCrossSize;
	}
	if ( tTrack.fW < 0.0f ) tTrack.fW = 0.0f;
	if ( tTrack.fH < 0.0f ) tTrack.fH = 0.0f;
	return xuiInternalSnapRect(tTrack);
}

static xui_rect_t __xuiSliderFillRect(const xui_slider_data_t* pData, xui_rect_t tTrack)
{
	xui_rect_t tFill;
	float fRate;

	tFill = tTrack;
	fRate = __xuiSliderRateFromData(pData);
	if ( pData->iOrientation == XUI_ORIENTATION_HORIZONTAL ) {
		tFill.fW = tTrack.fW * fRate;
	} else {
		tFill.fH = tTrack.fH * fRate;
		tFill.fY = tTrack.fY + tTrack.fH - tFill.fH;
	}
	return xuiInternalSnapRect(tFill);
}

static xui_rect_t __xuiSliderKnobRect(const xui_slider_data_t* pData, xui_rect_t tContent, xui_rect_t tTrack)
{
	xui_rect_t tRect;
	float fRate;
	float fSize;
	float fCenter;

	memset(&tRect, 0, sizeof(tRect));
	if ( (tContent.fW <= 0.0f) || (tContent.fH <= 0.0f) ) {
		return tRect;
	}
	fRate = __xuiSliderRateFromData(pData);
	fSize = (pData->fKnobSize > 0.0f) ? pData->fKnobSize : 14.0f;
	if ( pData->iOrientation == XUI_ORIENTATION_HORIZONTAL ) {
		if ( fSize > tContent.fH ) fSize = tContent.fH;
		if ( fSize > tContent.fW ) fSize = tContent.fW;
		fCenter = tTrack.fX + tTrack.fW * fRate;
		tRect.fX = fCenter - fSize * 0.5f;
		tRect.fY = tContent.fY + (tContent.fH - fSize) * 0.5f;
		tRect.fW = fSize;
		tRect.fH = fSize;
		if ( tRect.fX < tContent.fX ) tRect.fX = tContent.fX;
		if ( tRect.fX + tRect.fW > tContent.fX + tContent.fW ) tRect.fX = tContent.fX + tContent.fW - tRect.fW;
	} else {
		if ( fSize > tContent.fW ) fSize = tContent.fW;
		if ( fSize > tContent.fH ) fSize = tContent.fH;
		fCenter = tTrack.fY + tTrack.fH * (1.0f - fRate);
		tRect.fX = tContent.fX + (tContent.fW - fSize) * 0.5f;
		tRect.fY = fCenter - fSize * 0.5f;
		tRect.fW = fSize;
		tRect.fH = fSize;
		if ( tRect.fY < tContent.fY ) tRect.fY = tContent.fY;
		if ( tRect.fY + tRect.fH > tContent.fY + tContent.fH ) tRect.fY = tContent.fY + tContent.fH - tRect.fH;
	}
	return xuiInternalSnapRect(tRect);
}

static void __xuiSliderUpdateRects(xui_widget pWidget, xui_slider_data_t* pData, const xui_slider_data_t* pResolved)
{
	xui_rect_t tContent;

	tContent = xuiWidgetGetContentRect(pWidget);
	pData->tTrackRect = __xuiSliderTrackRect(pResolved, tContent);
	pData->tFillRect = __xuiSliderFillRect(pResolved, pData->tTrackRect);
	pData->tKnobRect = __xuiSliderKnobRect(pResolved, tContent, pData->tTrackRect);
}

static float __xuiSliderValueFromLocalPoint(const xui_slider_data_t* pData, float fLocalX, float fLocalY)
{
	float fRate;
	float fRange;

	if ( pData == NULL ) {
		return 0.0f;
	}
	if ( pData->iOrientation == XUI_ORIENTATION_HORIZONTAL ) {
		fRate = (pData->tTrackRect.fW > 0.0f) ? ((fLocalX - pData->tTrackRect.fX) / pData->tTrackRect.fW) : 0.0f;
	} else {
		fRate = (pData->tTrackRect.fH > 0.0f) ? (1.0f - ((fLocalY - pData->tTrackRect.fY) / pData->tTrackRect.fH)) : 0.0f;
	}
	fRate = __xuiSliderClampFloat(fRate, 0.0f, 1.0f);
	fRange = pData->fMax - pData->fMin;
	return pData->fMin + fRange * fRate;
}

static int __xuiSliderSetValueInternal(xui_widget pWidget, xui_slider_data_t* pData, float fValue, int bNotify)
{
	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	fValue = __xuiSliderClampFloat(fValue, pData->fMin, pData->fMax);
	if ( pData->fValue == fValue ) {
		return XUI_OK;
	}
	pData->fValue = fValue;
	pData->iChangeCount++;
	if ( bNotify && (pData->onChange != NULL) ) {
		pData->onChange(pWidget, pData->fValue, pData->pChangeUser);
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiSliderSetValueFromEvent(xui_widget pWidget, xui_slider_data_t* pData, const xui_event_t* pEvent, int bNotify)
{
	xui_slider_data_t tResolved;
	xui_rect_t tWorld;
	float fLocalX;
	float fLocalY;

	if ( (pWidget == NULL) || (pData == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiSliderResolve(pWidget, pData, &tResolved);
	__xuiSliderUpdateRects(pWidget, pData, &tResolved);
	tWorld = xuiWidgetGetWorldRect(pWidget);
	fLocalX = pEvent->fX - tWorld.fX;
	fLocalY = pEvent->fY - tWorld.fY;
	return __xuiSliderSetValueInternal(pWidget, pData, __xuiSliderValueFromLocalPoint(pData, fLocalX, fLocalY), bNotify);
}

static void __xuiSliderCancelDrag(xui_widget pWidget, xui_slider_data_t* pData)
{
	xui_context pContext;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return;
	}
	pData->bDragging = 0;
	(void)__xuiSliderSyncState(pWidget, pData);
	pContext = xuiWidgetGetContext(pWidget);
	if ( xuiGetPointerCapture(pContext) == pWidget ) {
		(void)xuiReleasePointerCapture(pContext, pWidget);
	}
}

static void __xuiSliderCancelAction(xui_widget pWidget, void* pUser)
{
	xui_slider_data_t* pData;

	(void)pUser;
	pData = __xuiSliderGetData(pWidget);
	__xuiSliderCancelDrag(pWidget, pData);
}

static int __xuiSliderPointerInside(xui_widget pWidget, const xui_event_t* pEvent)
{
	xui_rect_t tWorld;
	xui_rect_t tContent;
	float fLocalX;
	float fLocalY;

	if ( (pWidget == NULL) || (pEvent == NULL) ) {
		return 0;
	}
	tWorld = xuiWidgetGetWorldRect(pWidget);
	tContent = xuiWidgetGetContentRect(pWidget);
	fLocalX = pEvent->fX - tWorld.fX;
	fLocalY = pEvent->fY - tWorld.fY;
	return __xuiSliderRectContains(tContent, fLocalX, fLocalY);
}

static int __xuiSliderPointerDown(xui_widget pWidget, xui_slider_data_t* pData, const xui_event_t* pEvent)
{
	xui_context pContext;
	int iRet;

	if ( pEvent->iButton != XUI_POINTER_BUTTON_LEFT ) {
		return XUI_OK;
	}
	if ( !xuiWidgetGetEnabled(pWidget) || !__xuiSliderPointerInside(pWidget, pEvent) ) {
		return XUI_OK;
	}
	pContext = xuiWidgetGetContext(pWidget);
	(void)xuiSetFocusWidget(pContext, pWidget);
	(void)xuiSetPointerCapture(pContext, pWidget);
	pData->bDragging = 1;
	(void)__xuiSliderSyncState(pWidget, pData);
	iRet = __xuiSliderSetValueFromEvent(pWidget, pData, pEvent, 1);
	if ( iRet != XUI_OK ) return iRet;
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiSliderPointerMove(xui_widget pWidget, xui_slider_data_t* pData, const xui_event_t* pEvent)
{
	int iRet;

	if ( (pData->bDragging != 0) && (xuiGetPointerCapture(xuiWidgetGetContext(pWidget)) == pWidget) ) {
		iRet = __xuiSliderSetValueFromEvent(pWidget, pData, pEvent, 1);
		if ( iRet != XUI_OK ) return iRet;
		(void)__xuiSliderSyncState(pWidget, pData);
		return XUI_EVENT_DISPATCH_STOP;
	}
	return __xuiSliderSyncState(pWidget, pData);
}

static int __xuiSliderPointerUp(xui_widget pWidget, xui_slider_data_t* pData, const xui_event_t* pEvent)
{
	xui_context pContext;
	int bWasDragging;
	int iRet;

	if ( pEvent->iButton != XUI_POINTER_BUTTON_LEFT ) {
		return XUI_OK;
	}
	pContext = xuiWidgetGetContext(pWidget);
	bWasDragging = (pData->bDragging != 0) || (xuiGetPointerCapture(pContext) == pWidget);
	if ( bWasDragging ) {
		iRet = __xuiSliderSetValueFromEvent(pWidget, pData, pEvent, 1);
		if ( iRet != XUI_OK ) return iRet;
	}
	pData->bDragging = 0;
	(void)__xuiSliderSyncState(pWidget, pData);
	if ( xuiGetPointerCapture(pContext) == pWidget ) {
		(void)xuiReleasePointerCapture(pContext, pWidget);
	}
	return bWasDragging ? XUI_EVENT_DISPATCH_STOP : XUI_OK;
}

static int __xuiSliderWheel(xui_widget pWidget, xui_slider_data_t* pData, const xui_event_t* pEvent)
{
	float fDelta;

	if ( !xuiWidgetGetEnabled(pWidget) ) {
		return XUI_OK;
	}
	fDelta = (__xuiSliderAbsFloat(pEvent->fWheelY) >= __xuiSliderAbsFloat(pEvent->fWheelX)) ? pEvent->fWheelY : pEvent->fWheelX;
	if ( fDelta == 0.0f ) {
		return XUI_OK;
	}
	(void)__xuiSliderSetValueInternal(pWidget, pData, pData->fValue + fDelta * __xuiSliderSmallStep(pData), 1);
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiSliderKeyDown(xui_widget pWidget, xui_slider_data_t* pData, const xui_event_t* pEvent)
{
	if ( !xuiWidgetGetEnabled(pWidget) ) {
		return XUI_OK;
	}
	switch ( pEvent->iKey ) {
	case XUI_KEY_ESCAPE:
		__xuiSliderCancelDrag(pWidget, pData);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_HOME:
		(void)__xuiSliderSetValueInternal(pWidget, pData, pData->fMin, 1);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_END:
		(void)__xuiSliderSetValueInternal(pWidget, pData, pData->fMax, 1);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_PAGE_DOWN:
		(void)__xuiSliderSetValueInternal(pWidget, pData, pData->fValue - __xuiSliderPageStep(pData), 1);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_PAGE_UP:
		(void)__xuiSliderSetValueInternal(pWidget, pData, pData->fValue + __xuiSliderPageStep(pData), 1);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_LEFT:
	case XUI_KEY_DOWN:
		(void)__xuiSliderSetValueInternal(pWidget, pData, pData->fValue - __xuiSliderSmallStep(pData), 1);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_RIGHT:
	case XUI_KEY_UP:
		(void)__xuiSliderSetValueInternal(pWidget, pData, pData->fValue + __xuiSliderSmallStep(pData), 1);
		return XUI_EVENT_DISPATCH_STOP;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiSliderEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_slider_data_t* pData;
	int iRet;

	(void)pUser;
	if ( (pWidget == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiSliderGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_ENTER:
	case XUI_EVENT_POINTER_MOVE:
		return __xuiSliderPointerMove(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_LEAVE:
		if ( pData->bDragging == 0 ) {
			return __xuiSliderSyncState(pWidget, pData);
		}
		break;
	case XUI_EVENT_POINTER_DOWN:
		return __xuiSliderPointerDown(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_UP:
		return __xuiSliderPointerUp(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_CLICK:
		if ( pEvent->iButton == XUI_POINTER_BUTTON_LEFT ) {
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_WHEEL:
		return __xuiSliderWheel(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		pData->bDragging = 0;
		return __xuiSliderSyncState(pWidget, pData);
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
		return __xuiSliderSyncState(pWidget, pData);
	case XUI_EVENT_KEY_DOWN:
		return __xuiSliderKeyDown(pWidget, pData, pEvent);
	case XUI_EVENT_ENABLED_CHANGED:
	case XUI_EVENT_VISIBLE_CHANGED:
		iRet = __xuiSliderSyncState(pWidget, pData);
		if ( iRet != XUI_OK ) return iRet;
		return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiSliderDrawRectFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || (__xuiSliderColorAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	return pProxy->drawRectFill(pProxy, pDraw, tRect, iColor);
}

static int __xuiSliderDrawRectStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || (fWidth <= 0.0f) || (__xuiSliderColorAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	return pProxy->drawRectStroke(pProxy, pDraw, tRect, fWidth, iColor);
}

static int __xuiSliderDrawTrackFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fRadius, uint32_t iColor)
{
	xui_rect_t tBody;
	float fMaxRadius;
	float fCX0;
	float fCY0;
	float fCX1;
	float fCY1;
	int iRet;

	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || (__xuiSliderColorAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	fMaxRadius = __xuiSliderMinFloat(tRect.fW, tRect.fH) * 0.5f;
	if ( fRadius < 0.0f ) fRadius = fMaxRadius;
	if ( fRadius > fMaxRadius ) fRadius = fMaxRadius;
	if ( (fRadius <= 0.0f) || (pProxy->drawCircleFill == NULL) ) {
		return __xuiSliderDrawRectFill(pProxy, pDraw, tRect, iColor);
	}
	tBody = tRect;
	if ( tRect.fW >= tRect.fH ) {
		tBody.fX += fRadius;
		tBody.fW -= fRadius * 2.0f;
		fCX0 = tRect.fX + fRadius;
		fCY0 = tRect.fY + tRect.fH * 0.5f;
		fCX1 = tRect.fX + tRect.fW - fRadius;
		fCY1 = fCY0;
	} else {
		tBody.fY += fRadius;
		tBody.fH -= fRadius * 2.0f;
		fCX0 = tRect.fX + tRect.fW * 0.5f;
		fCY0 = tRect.fY + fRadius;
		fCX1 = fCX0;
		fCY1 = tRect.fY + tRect.fH - fRadius;
	}
	if ( (tBody.fW > 0.0f) && (tBody.fH > 0.0f) ) {
		iRet = __xuiSliderDrawRectFill(pProxy, pDraw, tBody, iColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	iRet = pProxy->drawCircleFill(pProxy, pDraw, fCX0, fCY0, fRadius, iColor);
	if ( iRet != XUI_OK ) return iRet;
	return pProxy->drawCircleFill(pProxy, pDraw, fCX1, fCY1, fRadius, iColor);
}

static int __xuiSliderDrawCircleFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	float fRadius;

	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || (__xuiSliderColorAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	fRadius = __xuiSliderMinFloat(tRect.fW, tRect.fH) * 0.5f;
	if ( pProxy->drawCircleFill != NULL ) {
		return pProxy->drawCircleFill(pProxy, pDraw, tRect.fX + tRect.fW * 0.5f, tRect.fY + tRect.fH * 0.5f, fRadius, iColor);
	}
	return __xuiSliderDrawRectFill(pProxy, pDraw, tRect, iColor);
}

static int __xuiSliderDrawCircleStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	float fRadius;

	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || (fWidth <= 0.0f) || (__xuiSliderColorAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	fRadius = __xuiSliderMinFloat(tRect.fW, tRect.fH) * 0.5f - fWidth * 0.5f;
	if ( fRadius <= 0.0f ) {
		return XUI_OK;
	}
	if ( pProxy->drawCircleStroke != NULL ) {
		return pProxy->drawCircleStroke(pProxy, pDraw, tRect.fX + tRect.fW * 0.5f, tRect.fY + tRect.fH * 0.5f, fRadius, fWidth, iColor);
	}
	return __xuiSliderDrawRectStroke(pProxy, pDraw, tRect, fWidth, iColor);
}

static uint32_t __xuiSliderFillForState(const xui_slider_data_t* pData, uint32_t iStateId)
{
	if ( (iStateId & XUI_WIDGET_STATE_DISABLED) != 0 ) {
		return pData->iDisabledColor;
	}
	if ( (iStateId & XUI_WIDGET_STATE_ACTIVE) != 0 ) {
		return pData->iFillActiveColor;
	}
	if ( (iStateId & XUI_WIDGET_STATE_HOVER) != 0 ) {
		return pData->iFillHoverColor;
	}
	return pData->iFillColor;
}

static int __xuiSliderCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_slider_data_t* pData;
	xui_slider_data_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tContent;
	xui_rect_t tFocus;
	uint32_t iTrackColor;
	uint32_t iFillColor;
	uint32_t iKnobColor;
	uint32_t iBorderColor;
	float fTrackRadius;
	int iRet;

	(void)pUser;
	pData = __xuiSliderGetData(pWidget);
	if ( (pData == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	__xuiSliderResolve(pWidget, pData, &tResolved);
	__xuiSliderUpdateRects(pWidget, pData, &tResolved);
	tContent = xuiWidgetGetContentRect(pWidget);
	iTrackColor = ((iStateId & XUI_WIDGET_STATE_DISABLED) != 0) ? __xuiSliderColorWithAlpha(tResolved.iDisabledColor, 72) : tResolved.iTrackColor;
	iFillColor = __xuiSliderFillForState(&tResolved, iStateId);
	iKnobColor = ((iStateId & XUI_WIDGET_STATE_DISABLED) != 0) ? XUI_COLOR_RGBA(232, 237, 244, 255) : tResolved.iKnobColor;
	iBorderColor = ((iStateId & XUI_WIDGET_STATE_DISABLED) != 0) ? __xuiSliderColorWithAlpha(tResolved.iKnobBorderColor, 105) : tResolved.iKnobBorderColor;
	fTrackRadius = (tResolved.fTrackRadius >= 0.0f) ? tResolved.fTrackRadius : (__xuiSliderMinFloat(pData->tTrackRect.fW, pData->tTrackRect.fH) * 0.5f);
	iRet = __xuiSliderDrawTrackFill(pProxy, pDraw, pData->tTrackRect, fTrackRadius, iTrackColor);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiSliderDrawTrackFill(pProxy, pDraw, pData->tFillRect, __xuiSliderMinFloat(fTrackRadius, __xuiSliderMinFloat(pData->tFillRect.fW, pData->tFillRect.fH) * 0.5f), iFillColor);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiSliderDrawCircleFill(pProxy, pDraw, pData->tKnobRect, iKnobColor);
	if ( iRet == XUI_OK ) {
		iRet = __xuiSliderDrawCircleStroke(pProxy, pDraw, pData->tKnobRect, 1.0f, iBorderColor);
	}
	if ( iRet != XUI_OK ) return iRet;
	if ( ((iStateId & XUI_WIDGET_STATE_FOCUS) != 0) && ((iStateId & XUI_WIDGET_STATE_DISABLED) == 0) ) {
		tFocus = xuiInternalInsetRect(tContent, 1.0f);
		iRet = __xuiSliderDrawRectStroke(pProxy, pDraw, tFocus, 1.5f, tResolved.iFocusColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiSliderContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_slider_data_t* pData;
	xui_slider_data_t tResolved;
	float fCross;
	float fLong;

	(void)tConstraint;
	(void)pUser;
	if ( pSize == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiSliderGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiSliderResolve(pWidget, pData, &tResolved);
	fCross = __xuiSliderMaxFloat(tResolved.fKnobSize, tResolved.fTrackSize);
	if ( fCross < 18.0f ) fCross = 18.0f;
	fLong = __xuiSliderMaxFloat(tResolved.fKnobSize * 8.0f, 144.0f);
	if ( tResolved.iOrientation == XUI_ORIENTATION_HORIZONTAL ) {
		*pSize = (xui_vec2_t){fLong, fCross};
	} else {
		*pSize = (xui_vec2_t){fCross, fLong};
	}
	return XUI_OK;
}

static void __xuiSliderDefaultLayout(xui_layout_t* pLayout)
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

static void __xuiSliderDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE | XUI_CACHE_UPDATE_ALL_STATES;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiSliderInitCacheStates(xui_widget pWidget)
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
		iRet = xuiWidgetSetCacheStateId(pWidget, i, 0x79000000u + (uint32_t)i);
		if ( iRet != XUI_OK ) return iRet;
	}
	for ( i = 0; i < (int)(sizeof(arrState) / sizeof(arrState[0])); i++ ) {
		iRet = xuiWidgetSetCacheStateId(pWidget, i, arrState[i]);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiSliderInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetCancelAction(pWidget, __xuiSliderCancelAction, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_ENTER, __xuiSliderEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiSliderEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiSliderEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiSliderEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiSliderEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CLICK, __xuiSliderEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_WHEEL, __xuiSliderEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiSliderEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiSliderEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiSliderEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiSliderEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_ENABLED_CHANGED, __xuiSliderEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_VISIBLE_CHANGED, __xuiSliderEvent, NULL);
	return iRet;
}

static int __xuiSliderInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_slider_data_t* pData;
	const xui_slider_desc_t* pDesc;
	float fMin;
	float fMax;
	uint32_t iFill;
	int iRet;

	(void)pUser;
	pData = (xui_slider_data_t*)pTypeData;
	pDesc = (const xui_slider_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiSliderDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(pData, 0, sizeof(*pData));
	fMin = (pDesc != NULL) ? pDesc->fMin : 0.0f;
	fMax = (pDesc != NULL) ? pDesc->fMax : 1.0f;
	if ( (pDesc == NULL) || (fMin == 0.0f && fMax == 0.0f) ) {
		fMax = 1.0f;
	}
	__xuiSliderNormalizeRange(&fMin, &fMax);
	iFill = (pDesc != NULL && pDesc->iFillColor != 0) ? pDesc->iFillColor : XUI_COLOR_RGBA(47, 128, 237, 255);
	pData->fMin = fMin;
	pData->fMax = fMax;
	pData->fValue = __xuiSliderClampFloat((pDesc != NULL) ? pDesc->fValue : fMin, fMin, fMax);
	pData->fStep = (pDesc != NULL && pDesc->fStep > 0.0f) ? pDesc->fStep : 0.0f;
	pData->fPageStep = (pDesc != NULL && pDesc->fPageStep > 0.0f) ? pDesc->fPageStep : 0.0f;
	pData->fTrackSize = (pDesc != NULL && pDesc->fTrackSize > 0.0f) ? pDesc->fTrackSize : 4.0f;
	pData->fKnobSize = (pDesc != NULL && pDesc->fKnobSize > 0.0f) ? pDesc->fKnobSize : 14.0f;
	pData->fTrackRadius = (pDesc != NULL && pDesc->fTrackRadius >= 0.0f) ? pDesc->fTrackRadius : -1.0f;
	pData->iTrackColor = (pDesc != NULL && pDesc->iTrackColor != 0) ? pDesc->iTrackColor : XUI_COLOR_RGBA(218, 230, 242, 255);
	pData->iFillColor = iFill;
	pData->iFillHoverColor = __xuiSliderLighten(iFill, 12);
	pData->iFillActiveColor = __xuiSliderLighten(iFill, -18);
	pData->iKnobColor = (pDesc != NULL && pDesc->iKnobColor != 0) ? pDesc->iKnobColor : XUI_COLOR_RGBA(255, 255, 255, 255);
	pData->iKnobBorderColor = (pDesc != NULL && pDesc->iKnobBorderColor != 0) ? pDesc->iKnobBorderColor : XUI_COLOR_RGBA(146, 169, 196, 255);
	pData->iFocusColor = (pDesc != NULL && pDesc->iFocusColor != 0) ? pDesc->iFocusColor : XUI_COLOR_RGBA(47, 128, 237, 130);
	pData->iDisabledColor = (pDesc != NULL && pDesc->iDisabledColor != 0) ? pDesc->iDisabledColor : XUI_COLOR_RGBA(172, 184, 200, 150);
	pData->iOrientation = (pDesc != NULL && __xuiSliderOrientationValid(pDesc->iOrientation)) ? pDesc->iOrientation : XUI_ORIENTATION_HORIZONTAL;
	(void)xuiWidgetSetPadding(pWidget, (xui_thickness_t){0.0f, 0.0f, 0.0f, 0.0f});
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_CLIP);
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	iRet = __xuiSliderInitCacheStates(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiSliderInitEvents(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiSliderSyncState(pWidget, pData);
}

static void __xuiSliderDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_slider_data_t* pData;

	(void)pUser;
	pData = (xui_slider_data_t*)pTypeData;
	if ( pData != NULL ) {
		__xuiSliderCancelDrag(pWidget, pData);
		memset(pData, 0, sizeof(*pData));
	}
}

static void __xuiSliderRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
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

static void __xuiSliderRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;
	uint32_t iLayoutDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	iLayoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiSliderRegisterStyleProperty(pContext, pType, "slider.track.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiSliderRegisterStyleProperty(pContext, pType, "slider.fill.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiSliderRegisterStyleProperty(pContext, pType, "slider.fill.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiSliderRegisterStyleProperty(pContext, pType, "slider.fill.active_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiSliderRegisterStyleProperty(pContext, pType, "slider.knob.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiSliderRegisterStyleProperty(pContext, pType, "slider.knob.border_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiSliderRegisterStyleProperty(pContext, pType, "slider.focus.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiSliderRegisterStyleProperty(pContext, pType, "slider.disabled.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiSliderRegisterStyleProperty(pContext, pType, "slider.track.size", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiSliderRegisterStyleProperty(pContext, pType, "slider.knob.size", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiSliderRegisterStyleProperty(pContext, pType, "slider.track.radius", XUI_STYLE_VALUE_FLOAT, iPaintDirty, 0);
}

XUI_API xui_widget_type xuiSliderGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "slider");
	if ( pType != NULL ) {
		__xuiSliderRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "slider";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_slider_data_t);
	tDesc.onInit = __xuiSliderInit;
	tDesc.onDestroy = __xuiSliderDestroy;
	tDesc.onContentMeasure = __xuiSliderContentMeasure;
	tDesc.onCacheRender = __xuiSliderCacheRender;
	__xuiSliderDefaultLayout(&tDesc.tLayout);
	__xuiSliderDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	__xuiSliderRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiSliderCreate(xui_context pContext, xui_widget* ppWidget, const xui_slider_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiSliderDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiSliderGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiSliderSetChange(xui_widget pWidget, xui_slider_change_proc onChange, void* pUser)
{
	xui_slider_data_t* pData = __xuiSliderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onChange = onChange;
	pData->pChangeUser = pUser;
	return XUI_OK;
}

XUI_API int xuiSliderSetRange(xui_widget pWidget, float fMin, float fMax)
{
	xui_slider_data_t* pData;

	pData = __xuiSliderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiSliderNormalizeRange(&fMin, &fMax);
	pData->fMin = fMin;
	pData->fMax = fMax;
	(void)__xuiSliderSetValueInternal(pWidget, pData, pData->fValue, 0);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiSliderGetRange(xui_widget pWidget, float* pMin, float* pMax)
{
	xui_slider_data_t* pData = __xuiSliderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pMin != NULL ) *pMin = pData->fMin;
	if ( pMax != NULL ) *pMax = pData->fMax;
	return XUI_OK;
}

XUI_API int xuiSliderSetValue(xui_widget pWidget, float fValue)
{
	xui_slider_data_t* pData = __xuiSliderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiSliderSetValueInternal(pWidget, pData, fValue, 0);
}

XUI_API float xuiSliderGetValue(xui_widget pWidget)
{
	xui_slider_data_t* pData = __xuiSliderGetData(pWidget);
	return (pData != NULL) ? pData->fValue : 0.0f;
}

XUI_API float xuiSliderGetRate(xui_widget pWidget)
{
	xui_slider_data_t* pData = __xuiSliderGetData(pWidget);
	return __xuiSliderRateFromData(pData);
}

XUI_API int xuiSliderSetStep(xui_widget pWidget, float fStep, float fPageStep)
{
	xui_slider_data_t* pData = __xuiSliderGetData(pWidget);
	if ( (pData == NULL) || (fStep < 0.0f) || (fPageStep < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fStep = fStep;
	pData->fPageStep = fPageStep;
	return XUI_OK;
}

XUI_API int xuiSliderGetStep(xui_widget pWidget, float* pStep, float* pPageStep)
{
	xui_slider_data_t* pData = __xuiSliderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pStep != NULL ) *pStep = pData->fStep;
	if ( pPageStep != NULL ) *pPageStep = pData->fPageStep;
	return XUI_OK;
}

XUI_API int xuiSliderSetOrientation(xui_widget pWidget, int iOrientation)
{
	xui_slider_data_t* pData = __xuiSliderGetData(pWidget);
	if ( (pData == NULL) || !__xuiSliderOrientationValid(iOrientation) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iOrientation == iOrientation ) return XUI_OK;
	pData->iOrientation = iOrientation;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiSliderGetOrientation(xui_widget pWidget)
{
	xui_slider_data_t* pData = __xuiSliderGetData(pWidget);
	return (pData != NULL) ? pData->iOrientation : XUI_ORIENTATION_HORIZONTAL;
}

XUI_API int xuiSliderSetMetrics(xui_widget pWidget, float fTrackSize, float fKnobSize, float fTrackRadius)
{
	xui_slider_data_t* pData = __xuiSliderGetData(pWidget);
	if ( (pData == NULL) || (fTrackSize < 0.0f) || (fKnobSize < 0.0f) || (fTrackRadius < -1.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fTrackSize = (fTrackSize > 0.0f) ? fTrackSize : 4.0f;
	pData->fKnobSize = (fKnobSize > 0.0f) ? fKnobSize : 14.0f;
	pData->fTrackRadius = fTrackRadius;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiSliderGetMetrics(xui_widget pWidget, float* pTrackSize, float* pKnobSize, float* pTrackRadius)
{
	xui_slider_data_t* pData = __xuiSliderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pTrackSize != NULL ) *pTrackSize = pData->fTrackSize;
	if ( pKnobSize != NULL ) *pKnobSize = pData->fKnobSize;
	if ( pTrackRadius != NULL ) *pTrackRadius = pData->fTrackRadius;
	return XUI_OK;
}

XUI_API int xuiSliderSetColors(xui_widget pWidget, uint32_t iTrack, uint32_t iFill, uint32_t iKnob, uint32_t iFocus, uint32_t iDisabled)
{
	xui_slider_data_t* pData = __xuiSliderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iTrackColor = iTrack;
	pData->iFillColor = iFill;
	pData->iFillHoverColor = __xuiSliderLighten(iFill, 12);
	pData->iFillActiveColor = __xuiSliderLighten(iFill, -18);
	pData->iKnobColor = iKnob;
	pData->iFocusColor = iFocus;
	pData->iDisabledColor = iDisabled;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiSliderGetColors(xui_widget pWidget, uint32_t* pTrack, uint32_t* pFill, uint32_t* pKnob, uint32_t* pFocus, uint32_t* pDisabled)
{
	xui_slider_data_t* pData = __xuiSliderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pTrack != NULL ) *pTrack = pData->iTrackColor;
	if ( pFill != NULL ) *pFill = pData->iFillColor;
	if ( pKnob != NULL ) *pKnob = pData->iKnobColor;
	if ( pFocus != NULL ) *pFocus = pData->iFocusColor;
	if ( pDisabled != NULL ) *pDisabled = pData->iDisabledColor;
	return XUI_OK;
}

XUI_API int xuiSliderSetKnobBorderColor(xui_widget pWidget, uint32_t iColor)
{
	xui_slider_data_t* pData = __xuiSliderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iKnobBorderColor = iColor;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiSliderGetKnobBorderColor(xui_widget pWidget)
{
	xui_slider_data_t* pData = __xuiSliderGetData(pWidget);
	return (pData != NULL) ? pData->iKnobBorderColor : 0;
}

static xui_rect_t __xuiSliderGetComputedRect(xui_widget pWidget, int iWhich)
{
	xui_slider_data_t* pData;
	xui_slider_data_t tResolved;
	xui_rect_t tZero;

	memset(&tZero, 0, sizeof(tZero));
	pData = __xuiSliderGetData(pWidget);
	if ( pData == NULL ) return tZero;
	__xuiSliderResolve(pWidget, pData, &tResolved);
	__xuiSliderUpdateRects(pWidget, pData, &tResolved);
	if ( iWhich == 1 ) return pData->tTrackRect;
	if ( iWhich == 2 ) return pData->tFillRect;
	if ( iWhich == 3 ) return pData->tKnobRect;
	return tZero;
}

XUI_API xui_rect_t xuiSliderGetTrackRect(xui_widget pWidget)
{
	return __xuiSliderGetComputedRect(pWidget, 1);
}

XUI_API xui_rect_t xuiSliderGetFillRect(xui_widget pWidget)
{
	return __xuiSliderGetComputedRect(pWidget, 2);
}

XUI_API xui_rect_t xuiSliderGetKnobRect(xui_widget pWidget)
{
	return __xuiSliderGetComputedRect(pWidget, 3);
}

XUI_API uint32_t xuiSliderGetState(xui_widget pWidget)
{
	xui_slider_data_t* pData;

	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return 0;
	}
	pData = __xuiSliderGetData(pWidget);
	return __xuiSliderBaseState(pWidget, pData);
}

XUI_API int xuiSliderGetChangeCount(xui_widget pWidget)
{
	xui_slider_data_t* pData = __xuiSliderGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}
