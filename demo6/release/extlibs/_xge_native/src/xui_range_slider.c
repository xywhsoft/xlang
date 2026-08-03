#include "xui_internal.h"

#include <string.h>

typedef struct xui_range_slider_data_t {
	xui_range_slider_change_proc onChange;
	void* pChangeUser;
	float fMin;
	float fMax;
	float fStart;
	float fEnd;
	float fStep;
	float fPageStep;
	float fMinInterval;
	float fMaxInterval;
	float fTrackSize;
	float fKnobSize;
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
	int iActiveThumb;
	int iChangeCount;
	xui_rect_t tTrackRect;
	xui_rect_t tFillRect;
	xui_rect_t tStartKnobRect;
	xui_rect_t tEndKnobRect;
} xui_range_slider_data_t;

static int __xuiRangeSliderDescValid(const xui_range_slider_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	return (pDesc->iSize == 0) || (pDesc->iSize >= sizeof(*pDesc));
}

static int __xuiRangeSliderOrientationValid(int iOrientation)
{
	return (iOrientation == XUI_ORIENTATION_HORIZONTAL) || (iOrientation == XUI_ORIENTATION_VERTICAL);
}

static float __xuiRangeSliderClampFloat(float fValue, float fMin, float fMax)
{
	if ( fValue < fMin ) return fMin;
	if ( fValue > fMax ) return fMax;
	return fValue;
}

static float __xuiRangeSliderAbsFloat(float fValue)
{
	return (fValue < 0.0f) ? -fValue : fValue;
}

static float __xuiRangeSliderMinFloat(float fA, float fB)
{
	return (fA < fB) ? fA : fB;
}

static float __xuiRangeSliderMaxFloat(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static void __xuiRangeSliderNormalizeRange(float* pMin, float* pMax)
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

static float __xuiRangeSliderEffectiveMinInterval(float fMinInterval, float fMin, float fMax)
{
	float fRange;

	fRange = fMax - fMin;
	if ( fRange < 0.0f ) fRange = 0.0f;
	if ( fMinInterval <= 0.0f ) return 0.0f;
	if ( fMinInterval > fRange ) return fRange;
	return fMinInterval;
}

static float __xuiRangeSliderEffectiveMaxInterval(float fMaxInterval, float fMinInterval, float fMin, float fMax)
{
	float fRange;
	float fMinEffective;

	fRange = fMax - fMin;
	if ( fRange < 0.0f ) fRange = 0.0f;
	fMinEffective = __xuiRangeSliderEffectiveMinInterval(fMinInterval, fMin, fMax);
	if ( fMaxInterval <= 0.0f ) return fRange;
	if ( fMaxInterval < fMinEffective ) return fMinEffective;
	if ( fMaxInterval > fRange ) return fRange;
	return fMaxInterval;
}

static uint32_t __xuiRangeSliderColorWithAlpha(uint32_t iColor, uint32_t iAlpha)
{
	return (iColor & 0xffffff00u) | (iAlpha & 0xffu);
}

static int __xuiRangeSliderColorAlpha(uint32_t iColor)
{
	return (int)(iColor & 0xffu);
}

static uint32_t __xuiRangeSliderLighten(uint32_t iColor, int iAmount)
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

static float __xuiRangeSliderRateFromValue(const xui_range_slider_data_t* pData, float fValue)
{
	float fRange;

	if ( pData == NULL ) {
		return 0.0f;
	}
	fRange = pData->fMax - pData->fMin;
	if ( fRange <= 0.0f ) {
		return 0.0f;
	}
	return __xuiRangeSliderClampFloat((fValue - pData->fMin) / fRange, 0.0f, 1.0f);
}

static float __xuiRangeSliderSmallStep(const xui_range_slider_data_t* pData)
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

static float __xuiRangeSliderPageStep(const xui_range_slider_data_t* pData)
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
	fStep = (fRange > 0.0f) ? (fRange * 0.10f) : __xuiRangeSliderSmallStep(pData);
	if ( (fRange >= 1.0f) && (fStep < 1.0f) ) {
		fStep = 1.0f;
	}
	return (fStep > 0.0f) ? fStep : 1.0f;
}

static xui_range_slider_data_t* __xuiRangeSliderGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return NULL;
	}
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "rangeslider");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_range_slider_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiRangeSliderStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
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

static int __xuiRangeSliderStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
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

static void __xuiRangeSliderResolve(xui_widget pWidget, const xui_range_slider_data_t* pData, xui_range_slider_data_t* pResolved)
{
	*pResolved = *pData;
	(void)__xuiRangeSliderStyleColor(pWidget, "rangeslider.track.color", &pResolved->iTrackColor);
	(void)__xuiRangeSliderStyleColor(pWidget, "rangeslider.fill.color", &pResolved->iFillColor);
	(void)__xuiRangeSliderStyleColor(pWidget, "rangeslider.fill.hover_color", &pResolved->iFillHoverColor);
	(void)__xuiRangeSliderStyleColor(pWidget, "rangeslider.fill.active_color", &pResolved->iFillActiveColor);
	(void)__xuiRangeSliderStyleColor(pWidget, "rangeslider.knob.color", &pResolved->iKnobColor);
	(void)__xuiRangeSliderStyleColor(pWidget, "rangeslider.knob.border_color", &pResolved->iKnobBorderColor);
	(void)__xuiRangeSliderStyleColor(pWidget, "rangeslider.focus.color", &pResolved->iFocusColor);
	(void)__xuiRangeSliderStyleColor(pWidget, "rangeslider.disabled.color", &pResolved->iDisabledColor);
	(void)__xuiRangeSliderStyleFloat(pWidget, "rangeslider.track.size", &pResolved->fTrackSize);
	(void)__xuiRangeSliderStyleFloat(pWidget, "rangeslider.knob.size", &pResolved->fKnobSize);
	if ( pResolved->fTrackSize <= 0.0f ) pResolved->fTrackSize = 4.0f;
	if ( pResolved->fKnobSize <= 0.0f ) pResolved->fKnobSize = 14.0f;
}

static uint32_t __xuiRangeSliderBaseState(xui_widget pWidget, const xui_range_slider_data_t* pData)
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

static uint32_t __xuiRangeSliderStateId(xui_widget pWidget, const xui_range_slider_data_t* pData)
{
	uint32_t iState;

	iState = __xuiRangeSliderBaseState(pWidget, pData);
	if ( (iState & XUI_WIDGET_STATE_DISABLED) != 0 ) return XUI_WIDGET_STATE_DISABLED;
	if ( (iState & XUI_WIDGET_STATE_ACTIVE) != 0 ) return XUI_WIDGET_STATE_ACTIVE;
	if ( (iState & XUI_WIDGET_STATE_HOVER) != 0 ) return XUI_WIDGET_STATE_HOVER;
	if ( (iState & XUI_WIDGET_STATE_FOCUS) != 0 ) return XUI_WIDGET_STATE_FOCUS;
	return 0;
}

static int __xuiRangeSliderSyncState(xui_widget pWidget, xui_range_slider_data_t* pData)
{
	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return xuiWidgetSetStateId(pWidget, __xuiRangeSliderStateId(pWidget, pData));
}

static int __xuiRangeSliderRectContains(xui_rect_t tRect, float fX, float fY)
{
	return (tRect.fW > 0.0f) &&
	       (tRect.fH > 0.0f) &&
	       (fX >= tRect.fX) &&
	       (fY >= tRect.fY) &&
	       (fX < (tRect.fX + tRect.fW)) &&
	       (fY < (tRect.fY + tRect.fH));
}

static xui_rect_t __xuiRangeSliderTrackRect(const xui_range_slider_data_t* pData, xui_rect_t tContent)
{
	xui_rect_t tTrack;
	float fCrossSize;
	float fInset;

	tTrack = tContent;
	fCrossSize = (pData->fTrackSize > 0.0f) ? pData->fTrackSize : 4.0f;
	if ( pData->iOrientation == XUI_ORIENTATION_HORIZONTAL ) {
		if ( fCrossSize > tContent.fH ) fCrossSize = tContent.fH;
		fInset = __xuiRangeSliderMinFloat(pData->fKnobSize * 0.5f, tContent.fW * 0.5f);
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
		fInset = __xuiRangeSliderMinFloat(pData->fKnobSize * 0.5f, tContent.fH * 0.5f);
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

static xui_rect_t __xuiRangeSliderFillRect(const xui_range_slider_data_t* pData, xui_rect_t tTrack)
{
	xui_rect_t tFill;
	float fStartRate;
	float fEndRate;
	float fA;
	float fB;

	tFill = tTrack;
	fStartRate = __xuiRangeSliderRateFromValue(pData, pData->fStart);
	fEndRate = __xuiRangeSliderRateFromValue(pData, pData->fEnd);
	if ( fEndRate < fStartRate ) {
		fA = fEndRate;
		fB = fStartRate;
	} else {
		fA = fStartRate;
		fB = fEndRate;
	}
	if ( pData->iOrientation == XUI_ORIENTATION_HORIZONTAL ) {
		tFill.fX = tTrack.fX + tTrack.fW * fA;
		tFill.fW = tTrack.fW * (fB - fA);
	} else {
		tFill.fY = tTrack.fY + tTrack.fH * (1.0f - fB);
		tFill.fH = tTrack.fH * (fB - fA);
	}
	return xuiInternalSnapRect(tFill);
}

static xui_rect_t __xuiRangeSliderKnobRectForValue(const xui_range_slider_data_t* pData, xui_rect_t tContent, xui_rect_t tTrack, float fValue)
{
	xui_rect_t tRect;
	float fRate;
	float fSize;
	float fCenter;

	memset(&tRect, 0, sizeof(tRect));
	if ( (tContent.fW <= 0.0f) || (tContent.fH <= 0.0f) ) {
		return tRect;
	}
	fRate = __xuiRangeSliderRateFromValue(pData, fValue);
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

static void __xuiRangeSliderUpdateRects(xui_widget pWidget, xui_range_slider_data_t* pData, const xui_range_slider_data_t* pResolved)
{
	xui_rect_t tContent;

	tContent = xuiWidgetGetContentRect(pWidget);
	pData->tTrackRect = __xuiRangeSliderTrackRect(pResolved, tContent);
	pData->tFillRect = __xuiRangeSliderFillRect(pResolved, pData->tTrackRect);
	pData->tStartKnobRect = __xuiRangeSliderKnobRectForValue(pResolved, tContent, pData->tTrackRect, pResolved->fStart);
	pData->tEndKnobRect = __xuiRangeSliderKnobRectForValue(pResolved, tContent, pData->tTrackRect, pResolved->fEnd);
}

static float __xuiRangeSliderValueFromLocalPoint(const xui_range_slider_data_t* pData, float fLocalX, float fLocalY)
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
	fRate = __xuiRangeSliderClampFloat(fRate, 0.0f, 1.0f);
	fRange = pData->fMax - pData->fMin;
	return pData->fMin + fRange * fRate;
}

static int __xuiRangeSliderNearestThumb(const xui_range_slider_data_t* pData, float fLocalX, float fLocalY)
{
	float fStartCenter;
	float fEndCenter;
	float fDistanceStart;
	float fDistanceEnd;

	if ( pData == NULL ) {
		return XUI_RANGE_SLIDER_THUMB_NONE;
	}
	if ( __xuiRangeSliderRectContains(pData->tStartKnobRect, fLocalX, fLocalY) ) {
		return XUI_RANGE_SLIDER_THUMB_START;
	}
	if ( __xuiRangeSliderRectContains(pData->tEndKnobRect, fLocalX, fLocalY) ) {
		return XUI_RANGE_SLIDER_THUMB_END;
	}
	if ( pData->iOrientation == XUI_ORIENTATION_HORIZONTAL ) {
		fStartCenter = pData->tStartKnobRect.fX + pData->tStartKnobRect.fW * 0.5f;
		fEndCenter = pData->tEndKnobRect.fX + pData->tEndKnobRect.fW * 0.5f;
		fDistanceStart = __xuiRangeSliderAbsFloat(fLocalX - fStartCenter);
		fDistanceEnd = __xuiRangeSliderAbsFloat(fLocalX - fEndCenter);
	} else {
		fStartCenter = pData->tStartKnobRect.fY + pData->tStartKnobRect.fH * 0.5f;
		fEndCenter = pData->tEndKnobRect.fY + pData->tEndKnobRect.fH * 0.5f;
		fDistanceStart = __xuiRangeSliderAbsFloat(fLocalY - fStartCenter);
		fDistanceEnd = __xuiRangeSliderAbsFloat(fLocalY - fEndCenter);
	}
	return (fDistanceStart <= fDistanceEnd) ? XUI_RANGE_SLIDER_THUMB_START : XUI_RANGE_SLIDER_THUMB_END;
}

static void __xuiRangeSliderNormalizeValues(float* pStart, float* pEnd, float fMin, float fMax, float fMinInterval, float fMaxInterval)
{
	float fCenter;
	float fInterval;
	float fTargetInterval;
	float fMinEffective;
	float fMaxEffective;
	float fSwap;

	*pStart = __xuiRangeSliderClampFloat(*pStart, fMin, fMax);
	*pEnd = __xuiRangeSliderClampFloat(*pEnd, fMin, fMax);
	if ( *pEnd < *pStart ) {
		fSwap = *pStart;
		*pStart = *pEnd;
		*pEnd = fSwap;
	}
	fMinEffective = __xuiRangeSliderEffectiveMinInterval(fMinInterval, fMin, fMax);
	fMaxEffective = __xuiRangeSliderEffectiveMaxInterval(fMaxInterval, fMinInterval, fMin, fMax);
	fInterval = *pEnd - *pStart;
	fTargetInterval = fInterval;
	if ( fTargetInterval < fMinEffective ) {
		fTargetInterval = fMinEffective;
	}
	if ( fTargetInterval > fMaxEffective ) {
		fTargetInterval = fMaxEffective;
	}
	if ( fTargetInterval != fInterval ) {
		fCenter = (*pStart + *pEnd) * 0.5f;
		*pStart = fCenter - fTargetInterval * 0.5f;
		*pEnd = *pStart + fTargetInterval;
		if ( *pStart < fMin ) {
			*pStart = fMin;
			*pEnd = fMin + fTargetInterval;
		}
		if ( *pEnd > fMax ) {
			*pEnd = fMax;
			*pStart = fMax - fTargetInterval;
		}
		*pStart = __xuiRangeSliderClampFloat(*pStart, fMin, fMax);
		*pEnd = __xuiRangeSliderClampFloat(*pEnd, fMin, fMax);
	}
}

static int __xuiRangeSliderSetValuesInternal(xui_widget pWidget, xui_range_slider_data_t* pData, float fStart, float fEnd, int bNotify)
{
	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiRangeSliderNormalizeValues(&fStart, &fEnd, pData->fMin, pData->fMax, pData->fMinInterval, pData->fMaxInterval);
	if ( (pData->fStart == fStart) && (pData->fEnd == fEnd) ) {
		return XUI_OK;
	}
	pData->fStart = fStart;
	pData->fEnd = fEnd;
	pData->iChangeCount++;
	if ( bNotify && (pData->onChange != NULL) ) {
		pData->onChange(pWidget, pData->fStart, pData->fEnd, pData->pChangeUser);
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiRangeSliderSetThumbValueInternal(xui_widget pWidget, xui_range_slider_data_t* pData, int iThumb, float fValue, int bNotify)
{
	float fStart;
	float fEnd;
	float fMaxInterval;
	float fMinInterval;
	float fLower;
	float fUpper;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	fStart = pData->fStart;
	fEnd = pData->fEnd;
	fMinInterval = __xuiRangeSliderEffectiveMinInterval(pData->fMinInterval, pData->fMin, pData->fMax);
	fMaxInterval = __xuiRangeSliderEffectiveMaxInterval(pData->fMaxInterval, pData->fMinInterval, pData->fMin, pData->fMax);
	if ( iThumb == XUI_RANGE_SLIDER_THUMB_START ) {
		fLower = __xuiRangeSliderMaxFloat(pData->fMin, fEnd - fMaxInterval);
		fUpper = fEnd - fMinInterval;
		if ( fUpper < fLower ) fUpper = fLower;
		fStart = __xuiRangeSliderClampFloat(fValue, fLower, fUpper);
	} else if ( iThumb == XUI_RANGE_SLIDER_THUMB_END ) {
		fLower = fStart + fMinInterval;
		fUpper = __xuiRangeSliderMinFloat(pData->fMax, fStart + fMaxInterval);
		if ( fUpper < fLower ) fLower = fUpper;
		fEnd = __xuiRangeSliderClampFloat(fValue, fLower, fUpper);
	} else {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return __xuiRangeSliderSetValuesInternal(pWidget, pData, fStart, fEnd, bNotify);
}

static int __xuiRangeSliderSetValueFromEvent(xui_widget pWidget, xui_range_slider_data_t* pData, const xui_event_t* pEvent, int iThumb, int bNotify)
{
	xui_range_slider_data_t tResolved;
	xui_rect_t tWorld;
	float fLocalX;
	float fLocalY;
	float fValue;

	if ( (pWidget == NULL) || (pData == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiRangeSliderResolve(pWidget, pData, &tResolved);
	__xuiRangeSliderUpdateRects(pWidget, pData, &tResolved);
	tWorld = xuiWidgetGetWorldRect(pWidget);
	fLocalX = pEvent->fX - tWorld.fX;
	fLocalY = pEvent->fY - tWorld.fY;
	if ( iThumb == XUI_RANGE_SLIDER_THUMB_NONE ) {
		iThumb = __xuiRangeSliderNearestThumb(pData, fLocalX, fLocalY);
	}
	fValue = __xuiRangeSliderValueFromLocalPoint(pData, fLocalX, fLocalY);
	pData->iActiveThumb = iThumb;
	return __xuiRangeSliderSetThumbValueInternal(pWidget, pData, iThumb, fValue, bNotify);
}

static void __xuiRangeSliderCancelDrag(xui_widget pWidget, xui_range_slider_data_t* pData)
{
	xui_context pContext;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return;
	}
	pData->bDragging = 0;
	(void)__xuiRangeSliderSyncState(pWidget, pData);
	pContext = xuiWidgetGetContext(pWidget);
	if ( xuiGetPointerCapture(pContext) == pWidget ) {
		(void)xuiReleasePointerCapture(pContext, pWidget);
	}
}

static void __xuiRangeSliderCancelAction(xui_widget pWidget, void* pUser)
{
	xui_range_slider_data_t* pData;

	(void)pUser;
	pData = __xuiRangeSliderGetData(pWidget);
	__xuiRangeSliderCancelDrag(pWidget, pData);
}

static int __xuiRangeSliderPointerInside(xui_widget pWidget, const xui_event_t* pEvent)
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
	return __xuiRangeSliderRectContains(tContent, fLocalX, fLocalY);
}

static int __xuiRangeSliderPointerDown(xui_widget pWidget, xui_range_slider_data_t* pData, const xui_event_t* pEvent)
{
	xui_context pContext;
	int iRet;

	if ( pEvent->iButton != XUI_POINTER_BUTTON_LEFT ) {
		return XUI_OK;
	}
	if ( !xuiWidgetGetEnabled(pWidget) || !__xuiRangeSliderPointerInside(pWidget, pEvent) ) {
		return XUI_OK;
	}
	pContext = xuiWidgetGetContext(pWidget);
	(void)xuiSetFocusWidget(pContext, pWidget);
	(void)xuiSetPointerCapture(pContext, pWidget);
	pData->bDragging = 1;
	(void)__xuiRangeSliderSyncState(pWidget, pData);
	iRet = __xuiRangeSliderSetValueFromEvent(pWidget, pData, pEvent, XUI_RANGE_SLIDER_THUMB_NONE, 1);
	if ( iRet != XUI_OK ) return iRet;
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiRangeSliderPointerMove(xui_widget pWidget, xui_range_slider_data_t* pData, const xui_event_t* pEvent)
{
	int iRet;

	if ( (pData->bDragging != 0) && (xuiGetPointerCapture(xuiWidgetGetContext(pWidget)) == pWidget) ) {
		iRet = __xuiRangeSliderSetValueFromEvent(pWidget, pData, pEvent, pData->iActiveThumb, 1);
		if ( iRet != XUI_OK ) return iRet;
		(void)__xuiRangeSliderSyncState(pWidget, pData);
		return XUI_EVENT_DISPATCH_STOP;
	}
	return __xuiRangeSliderSyncState(pWidget, pData);
}

static int __xuiRangeSliderPointerUp(xui_widget pWidget, xui_range_slider_data_t* pData, const xui_event_t* pEvent)
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
		iRet = __xuiRangeSliderSetValueFromEvent(pWidget, pData, pEvent, pData->iActiveThumb, 1);
		if ( iRet != XUI_OK ) return iRet;
	}
	pData->bDragging = 0;
	(void)__xuiRangeSliderSyncState(pWidget, pData);
	if ( xuiGetPointerCapture(pContext) == pWidget ) {
		(void)xuiReleasePointerCapture(pContext, pWidget);
	}
	return bWasDragging ? XUI_EVENT_DISPATCH_STOP : XUI_OK;
}

static int __xuiRangeSliderWheel(xui_widget pWidget, xui_range_slider_data_t* pData, const xui_event_t* pEvent)
{
	xui_range_slider_data_t tResolved;
	xui_rect_t tWorld;
	float fLocalX;
	float fLocalY;
	float fDelta;
	int iThumb;

	if ( !xuiWidgetGetEnabled(pWidget) ) {
		return XUI_OK;
	}
	fDelta = (__xuiRangeSliderAbsFloat(pEvent->fWheelY) >= __xuiRangeSliderAbsFloat(pEvent->fWheelX)) ? pEvent->fWheelY : pEvent->fWheelX;
	if ( fDelta == 0.0f ) {
		return XUI_OK;
	}
	iThumb = pData->iActiveThumb;
	if ( iThumb == XUI_RANGE_SLIDER_THUMB_NONE ) {
		__xuiRangeSliderResolve(pWidget, pData, &tResolved);
		__xuiRangeSliderUpdateRects(pWidget, pData, &tResolved);
		tWorld = xuiWidgetGetWorldRect(pWidget);
		fLocalX = pEvent->fX - tWorld.fX;
		fLocalY = pEvent->fY - tWorld.fY;
		iThumb = __xuiRangeSliderNearestThumb(pData, fLocalX, fLocalY);
	}
	pData->iActiveThumb = iThumb;
	(void)__xuiRangeSliderSetThumbValueInternal(pWidget, pData, iThumb, (iThumb == XUI_RANGE_SLIDER_THUMB_START ? pData->fStart : pData->fEnd) + fDelta * __xuiRangeSliderSmallStep(pData), 1);
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiRangeSliderKeyDown(xui_widget pWidget, xui_range_slider_data_t* pData, const xui_event_t* pEvent)
{
	int iThumb;
	float fValue;

	if ( !xuiWidgetGetEnabled(pWidget) ) {
		return XUI_OK;
	}
	iThumb = (pData->iActiveThumb != XUI_RANGE_SLIDER_THUMB_NONE) ? pData->iActiveThumb : XUI_RANGE_SLIDER_THUMB_END;
	fValue = (iThumb == XUI_RANGE_SLIDER_THUMB_START) ? pData->fStart : pData->fEnd;
	switch ( pEvent->iKey ) {
	case XUI_KEY_ESCAPE:
		__xuiRangeSliderCancelDrag(pWidget, pData);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_HOME:
		pData->iActiveThumb = iThumb;
		(void)__xuiRangeSliderSetThumbValueInternal(pWidget, pData, iThumb, (iThumb == XUI_RANGE_SLIDER_THUMB_START) ? pData->fMin : pData->fStart, 1);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_END:
		pData->iActiveThumb = iThumb;
		(void)__xuiRangeSliderSetThumbValueInternal(pWidget, pData, iThumb, (iThumb == XUI_RANGE_SLIDER_THUMB_START) ? pData->fEnd : pData->fMax, 1);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_PAGE_DOWN:
		pData->iActiveThumb = iThumb;
		(void)__xuiRangeSliderSetThumbValueInternal(pWidget, pData, iThumb, fValue - __xuiRangeSliderPageStep(pData), 1);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_PAGE_UP:
		pData->iActiveThumb = iThumb;
		(void)__xuiRangeSliderSetThumbValueInternal(pWidget, pData, iThumb, fValue + __xuiRangeSliderPageStep(pData), 1);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_LEFT:
	case XUI_KEY_DOWN:
		pData->iActiveThumb = iThumb;
		(void)__xuiRangeSliderSetThumbValueInternal(pWidget, pData, iThumb, fValue - __xuiRangeSliderSmallStep(pData), 1);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_RIGHT:
	case XUI_KEY_UP:
		pData->iActiveThumb = iThumb;
		(void)__xuiRangeSliderSetThumbValueInternal(pWidget, pData, iThumb, fValue + __xuiRangeSliderSmallStep(pData), 1);
		return XUI_EVENT_DISPATCH_STOP;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiRangeSliderEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_range_slider_data_t* pData;
	int iRet;

	(void)pUser;
	if ( (pWidget == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiRangeSliderGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_ENTER:
	case XUI_EVENT_POINTER_MOVE:
		return __xuiRangeSliderPointerMove(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_LEAVE:
		if ( pData->bDragging == 0 ) {
			return __xuiRangeSliderSyncState(pWidget, pData);
		}
		break;
	case XUI_EVENT_POINTER_DOWN:
		return __xuiRangeSliderPointerDown(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_UP:
		return __xuiRangeSliderPointerUp(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_CLICK:
		if ( pEvent->iButton == XUI_POINTER_BUTTON_LEFT ) {
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_WHEEL:
		return __xuiRangeSliderWheel(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		pData->bDragging = 0;
		return __xuiRangeSliderSyncState(pWidget, pData);
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
		return __xuiRangeSliderSyncState(pWidget, pData);
	case XUI_EVENT_KEY_DOWN:
		return __xuiRangeSliderKeyDown(pWidget, pData, pEvent);
	case XUI_EVENT_ENABLED_CHANGED:
	case XUI_EVENT_VISIBLE_CHANGED:
		iRet = __xuiRangeSliderSyncState(pWidget, pData);
		if ( iRet != XUI_OK ) return iRet;
		return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiRangeSliderDrawRectFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || (__xuiRangeSliderColorAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	return pProxy->drawRectFill(pProxy, pDraw, tRect, iColor);
}

static int __xuiRangeSliderDrawRectStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || (fWidth <= 0.0f) || (__xuiRangeSliderColorAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	return pProxy->drawRectStroke(pProxy, pDraw, tRect, fWidth, iColor);
}

static uint32_t __xuiRangeSliderFillForState(const xui_range_slider_data_t* pData, uint32_t iStateId)
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

static int __xuiRangeSliderDrawKnob(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iKnobColor, uint32_t iBorderColor)
{
	int iRet;

	iRet = __xuiRangeSliderDrawRectFill(pProxy, pDraw, tRect, iKnobColor);
	if ( iRet == XUI_OK ) {
		iRet = __xuiRangeSliderDrawRectStroke(pProxy, pDraw, tRect, 1.0f, iBorderColor);
	}
	return iRet;
}

static int __xuiRangeSliderCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_range_slider_data_t* pData;
	xui_range_slider_data_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tContent;
	xui_rect_t tFocus;
	uint32_t iTrackColor;
	uint32_t iFillColor;
	uint32_t iKnobColor;
	uint32_t iBorderColor;
	int iRet;

	(void)pUser;
	pData = __xuiRangeSliderGetData(pWidget);
	if ( (pData == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	__xuiRangeSliderResolve(pWidget, pData, &tResolved);
	__xuiRangeSliderUpdateRects(pWidget, pData, &tResolved);
	tContent = xuiWidgetGetContentRect(pWidget);
	iTrackColor = ((iStateId & XUI_WIDGET_STATE_DISABLED) != 0) ? __xuiRangeSliderColorWithAlpha(tResolved.iDisabledColor, 72) : tResolved.iTrackColor;
	iFillColor = __xuiRangeSliderFillForState(&tResolved, iStateId);
	iKnobColor = ((iStateId & XUI_WIDGET_STATE_DISABLED) != 0) ? XUI_COLOR_RGBA(232, 237, 244, 255) : tResolved.iKnobColor;
	iBorderColor = ((iStateId & XUI_WIDGET_STATE_DISABLED) != 0) ? __xuiRangeSliderColorWithAlpha(tResolved.iKnobBorderColor, 105) : tResolved.iKnobBorderColor;
	iRet = __xuiRangeSliderDrawRectFill(pProxy, pDraw, pData->tTrackRect, iTrackColor);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiRangeSliderDrawRectFill(pProxy, pDraw, pData->tFillRect, iFillColor);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiRangeSliderDrawKnob(pProxy, pDraw, pData->tStartKnobRect, iKnobColor, iBorderColor);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiRangeSliderDrawKnob(pProxy, pDraw, pData->tEndKnobRect, iKnobColor, iBorderColor);
	if ( iRet != XUI_OK ) return iRet;
	if ( ((iStateId & XUI_WIDGET_STATE_FOCUS) != 0) && ((iStateId & XUI_WIDGET_STATE_DISABLED) == 0) ) {
		tFocus = xuiInternalInsetRect(tContent, 1.0f);
		iRet = __xuiRangeSliderDrawRectStroke(pProxy, pDraw, tFocus, 1.5f, tResolved.iFocusColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiRangeSliderContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_range_slider_data_t* pData;
	xui_range_slider_data_t tResolved;
	float fCross;
	float fLong;

	(void)tConstraint;
	(void)pUser;
	if ( pSize == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiRangeSliderGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiRangeSliderResolve(pWidget, pData, &tResolved);
	fCross = __xuiRangeSliderMaxFloat(tResolved.fKnobSize, tResolved.fTrackSize);
	if ( fCross < 18.0f ) fCross = 18.0f;
	fLong = __xuiRangeSliderMaxFloat(tResolved.fKnobSize * 8.0f, 144.0f);
	if ( tResolved.iOrientation == XUI_ORIENTATION_HORIZONTAL ) {
		*pSize = (xui_vec2_t){fLong, fCross};
	} else {
		*pSize = (xui_vec2_t){fCross, fLong};
	}
	return XUI_OK;
}

static void __xuiRangeSliderDefaultLayout(xui_layout_t* pLayout)
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

static void __xuiRangeSliderDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE | XUI_CACHE_UPDATE_ALL_STATES;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiRangeSliderInitCacheStates(xui_widget pWidget)
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
		iRet = xuiWidgetSetCacheStateId(pWidget, i, 0x7a000000u + (uint32_t)i);
		if ( iRet != XUI_OK ) return iRet;
	}
	for ( i = 0; i < (int)(sizeof(arrState) / sizeof(arrState[0])); i++ ) {
		iRet = xuiWidgetSetCacheStateId(pWidget, i, arrState[i]);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiRangeSliderInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetCancelAction(pWidget, __xuiRangeSliderCancelAction, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_ENTER, __xuiRangeSliderEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiRangeSliderEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiRangeSliderEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiRangeSliderEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiRangeSliderEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CLICK, __xuiRangeSliderEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_WHEEL, __xuiRangeSliderEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiRangeSliderEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiRangeSliderEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiRangeSliderEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiRangeSliderEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_ENABLED_CHANGED, __xuiRangeSliderEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_VISIBLE_CHANGED, __xuiRangeSliderEvent, NULL);
	return iRet;
}

static int __xuiRangeSliderInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_range_slider_data_t* pData;
	const xui_range_slider_desc_t* pDesc;
	float fMin;
	float fMax;
	float fStart;
	float fEnd;
	float fMaxInterval;
	float fMinInterval;
	uint32_t iFill;
	int iRet;

	(void)pUser;
	pData = (xui_range_slider_data_t*)pTypeData;
	pDesc = (const xui_range_slider_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiRangeSliderDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(pData, 0, sizeof(*pData));
	fMin = (pDesc != NULL) ? pDesc->fMin : 0.0f;
	fMax = (pDesc != NULL) ? pDesc->fMax : 1.0f;
	if ( (pDesc == NULL) || (fMin == 0.0f && fMax == 0.0f) ) {
		fMax = 1.0f;
	}
	__xuiRangeSliderNormalizeRange(&fMin, &fMax);
	fMinInterval = (pDesc != NULL && pDesc->fMinInterval > 0.0f) ? pDesc->fMinInterval : 0.0f;
	fMaxInterval = (pDesc != NULL && pDesc->fMaxInterval > 0.0f) ? pDesc->fMaxInterval : 0.0f;
	if ( (fMaxInterval > 0.0f) && (fMinInterval > fMaxInterval) ) {
		fMinInterval = fMaxInterval;
	}
	fStart = (pDesc != NULL) ? pDesc->fStart : fMin;
	fEnd = (pDesc != NULL) ? pDesc->fEnd : fMax;
	__xuiRangeSliderNormalizeValues(&fStart, &fEnd, fMin, fMax, fMinInterval, fMaxInterval);
	iFill = (pDesc != NULL && pDesc->iFillColor != 0) ? pDesc->iFillColor : XUI_COLOR_RGBA(0, 184, 169, 255);
	pData->fMin = fMin;
	pData->fMax = fMax;
	pData->fStart = fStart;
	pData->fEnd = fEnd;
	pData->fStep = (pDesc != NULL && pDesc->fStep > 0.0f) ? pDesc->fStep : 0.0f;
	pData->fPageStep = (pDesc != NULL && pDesc->fPageStep > 0.0f) ? pDesc->fPageStep : 0.0f;
	pData->fMinInterval = fMinInterval;
	pData->fMaxInterval = fMaxInterval;
	pData->fTrackSize = (pDesc != NULL && pDesc->fTrackSize > 0.0f) ? pDesc->fTrackSize : 4.0f;
	pData->fKnobSize = (pDesc != NULL && pDesc->fKnobSize > 0.0f) ? pDesc->fKnobSize : 14.0f;
	pData->iTrackColor = (pDesc != NULL && pDesc->iTrackColor != 0) ? pDesc->iTrackColor : XUI_COLOR_RGBA(229, 231, 235, 255);
	pData->iFillColor = iFill;
	pData->iFillHoverColor = __xuiRangeSliderLighten(iFill, 12);
	pData->iFillActiveColor = __xuiRangeSliderLighten(iFill, -18);
	pData->iKnobColor = (pDesc != NULL && pDesc->iKnobColor != 0) ? pDesc->iKnobColor : XUI_COLOR_RGBA(255, 255, 255, 255);
	pData->iKnobBorderColor = (pDesc != NULL && pDesc->iKnobBorderColor != 0) ? pDesc->iKnobBorderColor : iFill;
	pData->iFocusColor = (pDesc != NULL && pDesc->iFocusColor != 0) ? pDesc->iFocusColor : __xuiRangeSliderColorWithAlpha(iFill, 130);
	pData->iDisabledColor = (pDesc != NULL && pDesc->iDisabledColor != 0) ? pDesc->iDisabledColor : XUI_COLOR_RGBA(172, 184, 200, 150);
	pData->iOrientation = (pDesc != NULL && __xuiRangeSliderOrientationValid(pDesc->iOrientation)) ? pDesc->iOrientation : XUI_ORIENTATION_HORIZONTAL;
	pData->iActiveThumb = XUI_RANGE_SLIDER_THUMB_END;
	(void)xuiWidgetSetPadding(pWidget, (xui_thickness_t){0.0f, 0.0f, 0.0f, 0.0f});
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_CLIP);
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	iRet = __xuiRangeSliderInitCacheStates(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiRangeSliderInitEvents(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiRangeSliderSyncState(pWidget, pData);
}

static void __xuiRangeSliderDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_range_slider_data_t* pData;

	(void)pUser;
	pData = (xui_range_slider_data_t*)pTypeData;
	if ( pData != NULL ) {
		__xuiRangeSliderCancelDrag(pWidget, pData);
		memset(pData, 0, sizeof(*pData));
	}
}

static void __xuiRangeSliderRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
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

static void __xuiRangeSliderRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;
	uint32_t iLayoutDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	iLayoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiRangeSliderRegisterStyleProperty(pContext, pType, "rangeslider.track.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiRangeSliderRegisterStyleProperty(pContext, pType, "rangeslider.fill.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiRangeSliderRegisterStyleProperty(pContext, pType, "rangeslider.fill.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiRangeSliderRegisterStyleProperty(pContext, pType, "rangeslider.fill.active_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiRangeSliderRegisterStyleProperty(pContext, pType, "rangeslider.knob.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiRangeSliderRegisterStyleProperty(pContext, pType, "rangeslider.knob.border_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiRangeSliderRegisterStyleProperty(pContext, pType, "rangeslider.focus.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiRangeSliderRegisterStyleProperty(pContext, pType, "rangeslider.disabled.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiRangeSliderRegisterStyleProperty(pContext, pType, "rangeslider.track.size", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiRangeSliderRegisterStyleProperty(pContext, pType, "rangeslider.knob.size", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
}

XUI_API xui_widget_type xuiRangeSliderGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "rangeslider");
	if ( pType != NULL ) {
		__xuiRangeSliderRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "rangeslider";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_range_slider_data_t);
	tDesc.onInit = __xuiRangeSliderInit;
	tDesc.onDestroy = __xuiRangeSliderDestroy;
	tDesc.onContentMeasure = __xuiRangeSliderContentMeasure;
	tDesc.onCacheRender = __xuiRangeSliderCacheRender;
	__xuiRangeSliderDefaultLayout(&tDesc.tLayout);
	__xuiRangeSliderDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	__xuiRangeSliderRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiRangeSliderCreate(xui_context pContext, xui_widget* ppWidget, const xui_range_slider_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiRangeSliderDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiRangeSliderGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiRangeSliderSetChange(xui_widget pWidget, xui_range_slider_change_proc onChange, void* pUser)
{
	xui_range_slider_data_t* pData = __xuiRangeSliderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onChange = onChange;
	pData->pChangeUser = pUser;
	return XUI_OK;
}

XUI_API int xuiRangeSliderSetRange(xui_widget pWidget, float fMin, float fMax)
{
	xui_range_slider_data_t* pData;

	pData = __xuiRangeSliderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiRangeSliderNormalizeRange(&fMin, &fMax);
	pData->fMin = fMin;
	pData->fMax = fMax;
	(void)__xuiRangeSliderSetValuesInternal(pWidget, pData, pData->fStart, pData->fEnd, 0);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiRangeSliderGetRange(xui_widget pWidget, float* pMin, float* pMax)
{
	xui_range_slider_data_t* pData = __xuiRangeSliderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pMin != NULL ) *pMin = pData->fMin;
	if ( pMax != NULL ) *pMax = pData->fMax;
	return XUI_OK;
}

XUI_API int xuiRangeSliderSetValues(xui_widget pWidget, float fStart, float fEnd)
{
	xui_range_slider_data_t* pData = __xuiRangeSliderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiRangeSliderSetValuesInternal(pWidget, pData, fStart, fEnd, 0);
}

XUI_API int xuiRangeSliderGetValues(xui_widget pWidget, float* pStart, float* pEnd)
{
	xui_range_slider_data_t* pData = __xuiRangeSliderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pStart != NULL ) *pStart = pData->fStart;
	if ( pEnd != NULL ) *pEnd = pData->fEnd;
	return XUI_OK;
}

XUI_API float xuiRangeSliderGetStart(xui_widget pWidget)
{
	xui_range_slider_data_t* pData = __xuiRangeSliderGetData(pWidget);
	return (pData != NULL) ? pData->fStart : 0.0f;
}

XUI_API float xuiRangeSliderGetEnd(xui_widget pWidget)
{
	xui_range_slider_data_t* pData = __xuiRangeSliderGetData(pWidget);
	return (pData != NULL) ? pData->fEnd : 0.0f;
}

XUI_API float xuiRangeSliderGetStartRate(xui_widget pWidget)
{
	xui_range_slider_data_t* pData = __xuiRangeSliderGetData(pWidget);
	return __xuiRangeSliderRateFromValue(pData, (pData != NULL) ? pData->fStart : 0.0f);
}

XUI_API float xuiRangeSliderGetEndRate(xui_widget pWidget)
{
	xui_range_slider_data_t* pData = __xuiRangeSliderGetData(pWidget);
	return __xuiRangeSliderRateFromValue(pData, (pData != NULL) ? pData->fEnd : 0.0f);
}

XUI_API int xuiRangeSliderSetStep(xui_widget pWidget, float fStep, float fPageStep)
{
	xui_range_slider_data_t* pData = __xuiRangeSliderGetData(pWidget);
	if ( (pData == NULL) || (fStep < 0.0f) || (fPageStep < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fStep = fStep;
	pData->fPageStep = fPageStep;
	return XUI_OK;
}

XUI_API int xuiRangeSliderGetStep(xui_widget pWidget, float* pStep, float* pPageStep)
{
	xui_range_slider_data_t* pData = __xuiRangeSliderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pStep != NULL ) *pStep = pData->fStep;
	if ( pPageStep != NULL ) *pPageStep = pData->fPageStep;
	return XUI_OK;
}

XUI_API int xuiRangeSliderSetIntervalLimits(xui_widget pWidget, float fMinInterval, float fMaxInterval)
{
	xui_range_slider_data_t* pData;
	int iRet;

	pData = __xuiRangeSliderGetData(pWidget);
	if ( (pData == NULL) || (fMinInterval < 0.0f) || (fMaxInterval < 0.0f) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (fMaxInterval > 0.0f) && (fMinInterval > fMaxInterval) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->fMinInterval = fMinInterval;
	pData->fMaxInterval = fMaxInterval;
	iRet = __xuiRangeSliderSetValuesInternal(pWidget, pData, pData->fStart, pData->fEnd, 0);
	if ( iRet != XUI_OK ) return iRet;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiRangeSliderGetIntervalLimits(xui_widget pWidget, float* pMinInterval, float* pMaxInterval)
{
	xui_range_slider_data_t* pData;

	pData = __xuiRangeSliderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pMinInterval != NULL ) *pMinInterval = pData->fMinInterval;
	if ( pMaxInterval != NULL ) *pMaxInterval = pData->fMaxInterval;
	return XUI_OK;
}

XUI_API int xuiRangeSliderSetOrientation(xui_widget pWidget, int iOrientation)
{
	xui_range_slider_data_t* pData = __xuiRangeSliderGetData(pWidget);
	if ( (pData == NULL) || !__xuiRangeSliderOrientationValid(iOrientation) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iOrientation == iOrientation ) return XUI_OK;
	pData->iOrientation = iOrientation;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiRangeSliderGetOrientation(xui_widget pWidget)
{
	xui_range_slider_data_t* pData = __xuiRangeSliderGetData(pWidget);
	return (pData != NULL) ? pData->iOrientation : XUI_ORIENTATION_HORIZONTAL;
}

XUI_API int xuiRangeSliderSetMetrics(xui_widget pWidget, float fTrackSize, float fKnobSize)
{
	xui_range_slider_data_t* pData = __xuiRangeSliderGetData(pWidget);
	if ( (pData == NULL) || (fTrackSize < 0.0f) || (fKnobSize < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fTrackSize = (fTrackSize > 0.0f) ? fTrackSize : 4.0f;
	pData->fKnobSize = (fKnobSize > 0.0f) ? fKnobSize : 14.0f;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiRangeSliderGetMetrics(xui_widget pWidget, float* pTrackSize, float* pKnobSize)
{
	xui_range_slider_data_t* pData = __xuiRangeSliderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pTrackSize != NULL ) *pTrackSize = pData->fTrackSize;
	if ( pKnobSize != NULL ) *pKnobSize = pData->fKnobSize;
	return XUI_OK;
}

XUI_API int xuiRangeSliderSetColors(xui_widget pWidget, uint32_t iTrack, uint32_t iFill, uint32_t iKnob, uint32_t iFocus, uint32_t iDisabled)
{
	xui_range_slider_data_t* pData = __xuiRangeSliderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iTrackColor = iTrack;
	pData->iFillColor = iFill;
	pData->iFillHoverColor = __xuiRangeSliderLighten(iFill, 12);
	pData->iFillActiveColor = __xuiRangeSliderLighten(iFill, -18);
	pData->iKnobColor = iKnob;
	pData->iFocusColor = iFocus;
	pData->iDisabledColor = iDisabled;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiRangeSliderGetColors(xui_widget pWidget, uint32_t* pTrack, uint32_t* pFill, uint32_t* pKnob, uint32_t* pFocus, uint32_t* pDisabled)
{
	xui_range_slider_data_t* pData = __xuiRangeSliderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pTrack != NULL ) *pTrack = pData->iTrackColor;
	if ( pFill != NULL ) *pFill = pData->iFillColor;
	if ( pKnob != NULL ) *pKnob = pData->iKnobColor;
	if ( pFocus != NULL ) *pFocus = pData->iFocusColor;
	if ( pDisabled != NULL ) *pDisabled = pData->iDisabledColor;
	return XUI_OK;
}

XUI_API int xuiRangeSliderSetKnobBorderColor(xui_widget pWidget, uint32_t iColor)
{
	xui_range_slider_data_t* pData = __xuiRangeSliderGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iKnobBorderColor = iColor;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiRangeSliderGetKnobBorderColor(xui_widget pWidget)
{
	xui_range_slider_data_t* pData = __xuiRangeSliderGetData(pWidget);
	return (pData != NULL) ? pData->iKnobBorderColor : 0;
}

static xui_rect_t __xuiRangeSliderGetComputedRect(xui_widget pWidget, int iWhich)
{
	xui_range_slider_data_t* pData;
	xui_range_slider_data_t tResolved;
	xui_rect_t tZero;

	memset(&tZero, 0, sizeof(tZero));
	pData = __xuiRangeSliderGetData(pWidget);
	if ( pData == NULL ) return tZero;
	__xuiRangeSliderResolve(pWidget, pData, &tResolved);
	__xuiRangeSliderUpdateRects(pWidget, pData, &tResolved);
	if ( iWhich == 1 ) return pData->tTrackRect;
	if ( iWhich == 2 ) return pData->tFillRect;
	if ( iWhich == 3 ) return pData->tStartKnobRect;
	if ( iWhich == 4 ) return pData->tEndKnobRect;
	return tZero;
}

XUI_API xui_rect_t xuiRangeSliderGetTrackRect(xui_widget pWidget)
{
	return __xuiRangeSliderGetComputedRect(pWidget, 1);
}

XUI_API xui_rect_t xuiRangeSliderGetFillRect(xui_widget pWidget)
{
	return __xuiRangeSliderGetComputedRect(pWidget, 2);
}

XUI_API xui_rect_t xuiRangeSliderGetStartKnobRect(xui_widget pWidget)
{
	return __xuiRangeSliderGetComputedRect(pWidget, 3);
}

XUI_API xui_rect_t xuiRangeSliderGetEndKnobRect(xui_widget pWidget)
{
	return __xuiRangeSliderGetComputedRect(pWidget, 4);
}

XUI_API int xuiRangeSliderGetActiveThumb(xui_widget pWidget)
{
	xui_range_slider_data_t* pData = __xuiRangeSliderGetData(pWidget);
	return (pData != NULL) ? pData->iActiveThumb : XUI_RANGE_SLIDER_THUMB_NONE;
}

XUI_API uint32_t xuiRangeSliderGetState(xui_widget pWidget)
{
	xui_range_slider_data_t* pData;

	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return 0;
	}
	pData = __xuiRangeSliderGetData(pWidget);
	return __xuiRangeSliderBaseState(pWidget, pData);
}

XUI_API int xuiRangeSliderGetChangeCount(xui_widget pWidget)
{
	xui_range_slider_data_t* pData = __xuiRangeSliderGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}
