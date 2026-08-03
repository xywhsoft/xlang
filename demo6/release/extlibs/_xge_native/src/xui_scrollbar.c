#include "xui_internal.h"

#include <string.h>

#define XUI_SCROLLBAR_STATE_HOVER_SHIFT	8
#define XUI_SCROLLBAR_STATE_ACTIVE_SHIFT	12
#define XUI_SCROLLBAR_STATE_PART_MASK	0x0fu
#define XUI_SCROLLBAR_REPEAT_DELAY		0.35f
#define XUI_SCROLLBAR_REPEAT_INTERVAL	0.06f
#define XUI_SCROLLBAR_REPEAT_MAX_STEPS	16

typedef struct xui_scrollbar_data_t {
	xui_scrollbar_change_proc onChange;
	void* pChangeUser;
	float fMin;
	float fMax;
	float fValue;
	float fPage;
	float fSmallStep;
	float fLargeStep;
	float fThickness;
	float fMinThumbSize;
	float fButtonSize;
	uint32_t iTrackColor;
	uint32_t iThumbColor;
	uint32_t iHoverColor;
	uint32_t iActiveColor;
	uint32_t iFocusColor;
	uint32_t iDisabledColor;
	uint32_t iButtonColor;
	uint32_t iButtonIconColor;
	int iOrientation;
	int iMode;
	int iButtonMode;
	int iHoverPart;
	int iActivePart;
	int bDraggingThumb;
	int iRepeatPart;
	int iRepeatDirection;
	float fDragStartMouse;
	float fDragStartValue;
	float fRepeatMouse;
	float fRepeatElapsed;
	float fRepeatNext;
	int iChangeCount;
	xui_rect_t tTrackRect;
	xui_rect_t tThumbRect;
	xui_rect_t tDecreaseRect;
	xui_rect_t tIncreaseRect;
} xui_scrollbar_data_t;

static int __xuiScrollBarDescValid(const xui_scrollbar_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	return (pDesc->iSize == 0) || (pDesc->iSize >= sizeof(*pDesc));
}

static int __xuiScrollBarOrientationValid(int iOrientation)
{
	return (iOrientation == XUI_ORIENTATION_HORIZONTAL) || (iOrientation == XUI_ORIENTATION_VERTICAL);
}

static int __xuiScrollBarModeValid(int iMode)
{
	return (iMode == XUI_SCROLLBAR_MODE_FULL) || (iMode == XUI_SCROLLBAR_MODE_COMPACT);
}

static int __xuiScrollBarButtonModeValid(int iButtonMode)
{
	return (iButtonMode == XUI_SCROLLBAR_BUTTONS_AUTO) ||
	       (iButtonMode == XUI_SCROLLBAR_BUTTONS_ON) ||
	       (iButtonMode == XUI_SCROLLBAR_BUTTONS_OFF);
}

static int __xuiScrollBarPartValid(int iPart)
{
	return (iPart == XUI_SCROLLBAR_PART_NONE) ||
	       (iPart == XUI_SCROLLBAR_PART_TRACK) ||
	       (iPart == XUI_SCROLLBAR_PART_THUMB) ||
	       (iPart == XUI_SCROLLBAR_PART_DECREASE) ||
	       (iPart == XUI_SCROLLBAR_PART_INCREASE);
}

static float __xuiScrollBarClampFloat(float fValue, float fMin, float fMax)
{
	if ( fValue < fMin ) {
		return fMin;
	}
	if ( fValue > fMax ) {
		return fMax;
	}
	return fValue;
}

static float __xuiScrollBarAbsFloat(float fValue)
{
	return (fValue < 0.0f) ? -fValue : fValue;
}

static float __xuiScrollBarMinFloat(float fA, float fB)
{
	return (fA < fB) ? fA : fB;
}

static void __xuiScrollBarNormalizeRange(float* pMin, float* pMax, float* pPage)
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
	if ( (*pPage) < 0.0f ) {
		*pPage = 0.0f;
	}
}

static float __xuiScrollBarRange(const xui_scrollbar_data_t* pData)
{
	if ( pData == NULL ) {
		return 0.0f;
	}
	return (pData->fMax > pData->fMin) ? (pData->fMax - pData->fMin) : 0.0f;
}

static float __xuiScrollBarSmallStep(const xui_scrollbar_data_t* pData)
{
	float fRange;
	float fStep;

	if ( pData == NULL ) {
		return 1.0f;
	}
	if ( pData->fSmallStep > 0.0f ) {
		return pData->fSmallStep;
	}
	fRange = __xuiScrollBarRange(pData);
	if ( pData->fPage > 0.0f ) {
		fStep = pData->fPage * 0.10f;
	} else if ( fRange > 0.0f ) {
		fStep = fRange * 0.05f;
	} else {
		fStep = 1.0f;
	}
	if ( (fRange >= 1.0f) && (fStep < 1.0f) ) {
		fStep = 1.0f;
	}
	if ( (fRange > 0.0f) && (fStep > fRange) ) {
		fStep = fRange;
	}
	return (fStep > 0.0f) ? fStep : 1.0f;
}

static float __xuiScrollBarLargeStep(const xui_scrollbar_data_t* pData)
{
	if ( pData == NULL ) {
		return 1.0f;
	}
	if ( pData->fLargeStep > 0.0f ) {
		return pData->fLargeStep;
	}
	if ( pData->fPage > 0.0f ) {
		return pData->fPage;
	}
	return __xuiScrollBarSmallStep(pData);
}

static uint32_t __xuiScrollBarColorWithAlpha(uint32_t iColor, uint32_t iAlpha)
{
	return (iColor & 0xffffff00u) | (iAlpha & 0xffu);
}

static int __xuiScrollBarColorAlpha(uint32_t iColor)
{
	return (int)(iColor & 0xffu);
}

static xui_scrollbar_data_t* __xuiScrollBarGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return NULL;
	}
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "scrollbar");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_scrollbar_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiScrollBarStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
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

static int __xuiScrollBarStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
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

static void __xuiScrollBarResolve(xui_widget pWidget, const xui_scrollbar_data_t* pData, xui_scrollbar_data_t* pResolved)
{
	*pResolved = *pData;
	(void)__xuiScrollBarStyleColor(pWidget, "scrollbar.track.color", &pResolved->iTrackColor);
	(void)__xuiScrollBarStyleColor(pWidget, "scrollbar.thumb.color", &pResolved->iThumbColor);
	(void)__xuiScrollBarStyleColor(pWidget, "scrollbar.thumb.hover_color", &pResolved->iHoverColor);
	(void)__xuiScrollBarStyleColor(pWidget, "scrollbar.thumb.active_color", &pResolved->iActiveColor);
	(void)__xuiScrollBarStyleColor(pWidget, "scrollbar.focus.color", &pResolved->iFocusColor);
	(void)__xuiScrollBarStyleColor(pWidget, "scrollbar.disabled.color", &pResolved->iDisabledColor);
	(void)__xuiScrollBarStyleColor(pWidget, "scrollbar.button.color", &pResolved->iButtonColor);
	(void)__xuiScrollBarStyleColor(pWidget, "scrollbar.button.icon_color", &pResolved->iButtonIconColor);
	(void)__xuiScrollBarStyleFloat(pWidget, "scrollbar.thickness", &pResolved->fThickness);
	(void)__xuiScrollBarStyleFloat(pWidget, "scrollbar.min_thumb_size", &pResolved->fMinThumbSize);
	(void)__xuiScrollBarStyleFloat(pWidget, "scrollbar.button_size", &pResolved->fButtonSize);
	if ( pResolved->fMinThumbSize < 1.0f ) {
		pResolved->fMinThumbSize = 1.0f;
	}
}

static uint32_t __xuiScrollBarBaseState(xui_widget pWidget)
{
	uint32_t iState;

	iState = xuiWidgetGetInputState(pWidget);
	if ( !xuiWidgetGetEnabled(pWidget) ) {
		iState |= XUI_WIDGET_STATE_DISABLED;
	}
	if ( (iState & XUI_WIDGET_STATE_DISABLED) != 0 ) {
		iState = XUI_WIDGET_STATE_DISABLED;
	}
	return iState & (XUI_WIDGET_STATE_HOVER | XUI_WIDGET_STATE_ACTIVE | XUI_WIDGET_STATE_FOCUS | XUI_WIDGET_STATE_DISABLED);
}

static uint32_t __xuiScrollBarStateId(xui_widget pWidget, const xui_scrollbar_data_t* pData)
{
	uint32_t iStateId;
	int iHoverPart;
	int iActivePart;

	iStateId = __xuiScrollBarBaseState(pWidget);
	if ( (iStateId & XUI_WIDGET_STATE_DISABLED) != 0 ) {
		return iStateId;
	}
	iHoverPart = (pData != NULL && __xuiScrollBarPartValid(pData->iHoverPart)) ? pData->iHoverPart : XUI_SCROLLBAR_PART_NONE;
	iActivePart = (pData != NULL && __xuiScrollBarPartValid(pData->iActivePart)) ? pData->iActivePart : XUI_SCROLLBAR_PART_NONE;
	iStateId |= ((uint32_t)iHoverPart & XUI_SCROLLBAR_STATE_PART_MASK) << XUI_SCROLLBAR_STATE_HOVER_SHIFT;
	iStateId |= ((uint32_t)iActivePart & XUI_SCROLLBAR_STATE_PART_MASK) << XUI_SCROLLBAR_STATE_ACTIVE_SHIFT;
	return iStateId;
}

static int __xuiScrollBarStateHoverPart(uint32_t iStateId)
{
	return (int)((iStateId >> XUI_SCROLLBAR_STATE_HOVER_SHIFT) & XUI_SCROLLBAR_STATE_PART_MASK);
}

static int __xuiScrollBarStateActivePart(uint32_t iStateId)
{
	return (int)((iStateId >> XUI_SCROLLBAR_STATE_ACTIVE_SHIFT) & XUI_SCROLLBAR_STATE_PART_MASK);
}

static int __xuiScrollBarSyncState(xui_widget pWidget, xui_scrollbar_data_t* pData)
{
	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return xuiWidgetSetStateId(pWidget, __xuiScrollBarStateId(pWidget, pData));
}

static float __xuiScrollBarAxisLen(const xui_scrollbar_data_t* pData, xui_rect_t tRect)
{
	return (pData->iOrientation == XUI_ORIENTATION_HORIZONTAL) ? tRect.fW : tRect.fH;
}

static float __xuiScrollBarAxisStart(const xui_scrollbar_data_t* pData, xui_rect_t tRect)
{
	return (pData->iOrientation == XUI_ORIENTATION_HORIZONTAL) ? tRect.fX : tRect.fY;
}

static float __xuiScrollBarAxisPoint(const xui_scrollbar_data_t* pData, float fX, float fY)
{
	return (pData->iOrientation == XUI_ORIENTATION_HORIZONTAL) ? fX : fY;
}

static float __xuiScrollBarCrossLen(const xui_scrollbar_data_t* pData, xui_rect_t tRect)
{
	return (pData->iOrientation == XUI_ORIENTATION_HORIZONTAL) ? tRect.fH : tRect.fW;
}

static float __xuiScrollBarVisualThickness(const xui_scrollbar_data_t* pData, xui_rect_t tRect)
{
	float fCross;
	float fSize;

	fCross = __xuiScrollBarCrossLen(pData, tRect);
	if ( pData->iMode == XUI_SCROLLBAR_MODE_COMPACT ) {
		fSize = (pData->fThickness > 0.0f) ? pData->fThickness : 8.0f;
		if ( fSize > 8.0f ) {
			fSize = 8.0f;
		}
	} else {
		if ( pData->fThickness > 0.0f ) {
			fSize = pData->fThickness;
		} else {
			fSize = fCross * 0.58f;
			if ( fSize < 10.0f ) {
				fSize = (fCross < 10.0f) ? fCross : 10.0f;
			}
			if ( fSize > 16.0f ) {
				fSize = 16.0f;
			}
		}
	}
	if ( fSize > fCross ) {
		fSize = fCross;
	}
	if ( fSize < 1.0f ) {
		fSize = 1.0f;
	}
	return fSize;
}

static float __xuiScrollBarButtonSize(const xui_scrollbar_data_t* pData, xui_rect_t tContent)
{
	float fLong;
	float fCross;
	float fButton;

	if ( (pData->iMode == XUI_SCROLLBAR_MODE_COMPACT) || (pData->iButtonMode == XUI_SCROLLBAR_BUTTONS_OFF) ) {
		return 0.0f;
	}
	fLong = __xuiScrollBarAxisLen(pData, tContent);
	fCross = __xuiScrollBarCrossLen(pData, tContent);
	if ( (fLong <= 0.0f) || (fCross <= 0.0f) ) {
		return 0.0f;
	}
	fButton = (pData->fButtonSize > 0.0f) ? pData->fButtonSize : fCross;
	if ( fButton > fCross ) {
		fButton = fCross;
	}
	if ( pData->iButtonMode == XUI_SCROLLBAR_BUTTONS_AUTO && fLong < (fButton * 3.0f) ) {
		return 0.0f;
	}
	return fButton;
}

static xui_rect_t __xuiScrollBarButtonRect(const xui_scrollbar_data_t* pData, xui_rect_t tContent, int bEnd)
{
	xui_rect_t tRect;
	float fButton;

	tRect = tContent;
	fButton = __xuiScrollBarButtonSize(pData, tContent);
	if ( fButton <= 0.0f ) {
		tRect.fW = 0.0f;
		tRect.fH = 0.0f;
		return tRect;
	}
	if ( pData->iOrientation == XUI_ORIENTATION_HORIZONTAL ) {
		tRect.fW = fButton;
		if ( bEnd ) {
			tRect.fX = tContent.fX + tContent.fW - fButton;
		}
	} else {
		tRect.fH = fButton;
		if ( bEnd ) {
			tRect.fY = tContent.fY + tContent.fH - fButton;
		}
	}
	return xuiInternalSnapRect(tRect);
}

static xui_rect_t __xuiScrollBarTrackRect(const xui_scrollbar_data_t* pData, xui_rect_t tContent)
{
	xui_rect_t tTrack;
	float fButton;

	tTrack = tContent;
	fButton = __xuiScrollBarButtonSize(pData, tContent);
	if ( fButton > 0.0f ) {
		if ( pData->iOrientation == XUI_ORIENTATION_HORIZONTAL ) {
			tTrack.fX += fButton;
			tTrack.fW -= fButton * 2.0f;
		} else {
			tTrack.fY += fButton;
			tTrack.fH -= fButton * 2.0f;
		}
	}
	if ( tTrack.fW < 0.0f ) tTrack.fW = 0.0f;
	if ( tTrack.fH < 0.0f ) tTrack.fH = 0.0f;
	return xuiInternalSnapRect(tTrack);
}

static float __xuiScrollBarThumbLen(const xui_scrollbar_data_t* pData, float fTrackLen)
{
	float fRange;
	float fTotal;
	float fLen;

	if ( (pData == NULL) || (fTrackLen <= 0.0f) ) {
		return 0.0f;
	}
	fRange = __xuiScrollBarRange(pData);
	if ( fRange <= 0.0f ) {
		return fTrackLen;
	}
	if ( pData->fPage > 0.0f ) {
		fTotal = fRange + pData->fPage;
		fLen = (fTotal > 0.0f) ? (fTrackLen * (pData->fPage / fTotal)) : pData->fMinThumbSize;
	} else {
		fLen = pData->fMinThumbSize;
	}
	if ( fLen < pData->fMinThumbSize ) {
		fLen = pData->fMinThumbSize;
	}
	if ( fLen > fTrackLen ) {
		fLen = fTrackLen;
	}
	return fLen;
}

static xui_rect_t __xuiScrollBarThumbRect(const xui_scrollbar_data_t* pData, xui_rect_t tTrack)
{
	xui_rect_t tThumb;
	float fTrackLen;
	float fThumbLen;
	float fTravel;
	float fRange;
	float fRate;

	tThumb = tTrack;
	fTrackLen = __xuiScrollBarAxisLen(pData, tTrack);
	fThumbLen = __xuiScrollBarThumbLen(pData, fTrackLen);
	fTravel = fTrackLen - fThumbLen;
	fRange = __xuiScrollBarRange(pData);
	fRate = (fRange > 0.0f) ? ((pData->fValue - pData->fMin) / fRange) : 0.0f;
	fRate = __xuiScrollBarClampFloat(fRate, 0.0f, 1.0f);
	if ( pData->iOrientation == XUI_ORIENTATION_HORIZONTAL ) {
		tThumb.fX = tTrack.fX + fTravel * fRate;
		tThumb.fW = fThumbLen;
	} else {
		tThumb.fY = tTrack.fY + fTravel * fRate;
		tThumb.fH = fThumbLen;
	}
	return xuiInternalSnapRect(tThumb);
}

static xui_rect_t __xuiScrollBarVisualCrossRect(const xui_scrollbar_data_t* pData, xui_rect_t tRect, xui_rect_t tBasis)
{
	float fSize;

	fSize = __xuiScrollBarVisualThickness(pData, tBasis);
	if ( pData->iOrientation == XUI_ORIENTATION_HORIZONTAL ) {
		tRect.fY += (tRect.fH - fSize) * 0.5f;
		tRect.fH = fSize;
	} else {
		tRect.fX += (tRect.fW - fSize) * 0.5f;
		tRect.fW = fSize;
	}
	if ( tRect.fW < 1.0f ) tRect.fW = 1.0f;
	if ( tRect.fH < 1.0f ) tRect.fH = 1.0f;
	return xuiInternalSnapRect(tRect);
}

static void __xuiScrollBarUpdateRects(xui_widget pWidget, xui_scrollbar_data_t* pData, const xui_scrollbar_data_t* pResolved)
{
	xui_rect_t tContent;
	xui_rect_t tTrack;
	xui_rect_t tThumb;

	tContent = xuiWidgetGetContentRect(pWidget);
	pData->tDecreaseRect = __xuiScrollBarButtonRect(pResolved, tContent, 0);
	pData->tIncreaseRect = __xuiScrollBarButtonRect(pResolved, tContent, 1);
	tTrack = __xuiScrollBarTrackRect(pResolved, tContent);
	tThumb = __xuiScrollBarThumbRect(pResolved, tTrack);
	pData->tTrackRect = tTrack;
	pData->tThumbRect = __xuiScrollBarVisualCrossRect(pResolved, tThumb, tContent);
}

static xui_rect_t __xuiScrollBarVisualTrackRect(const xui_scrollbar_data_t* pData, xui_rect_t tTrack, xui_rect_t tContent)
{
	return __xuiScrollBarVisualCrossRect(pData, tTrack, tContent);
}

static xui_rect_t __xuiScrollBarButtonVisualRect(xui_rect_t tButton)
{
	xui_rect_t tRect;
	float fSide;

	tRect = tButton;
	fSide = __xuiScrollBarMinFloat(tButton.fW, tButton.fH) - 4.0f;
	if ( fSide < 8.0f ) {
		fSide = __xuiScrollBarMinFloat(tButton.fW, tButton.fH);
	}
	tRect.fX = tButton.fX + (tButton.fW - fSide) * 0.5f;
	tRect.fY = tButton.fY + (tButton.fH - fSide) * 0.5f;
	tRect.fW = fSide;
	tRect.fH = fSide;
	return xuiInternalSnapRect(tRect);
}

static int __xuiScrollBarRectContains(xui_rect_t tRect, float fX, float fY)
{
	return (tRect.fW > 0.0f) &&
	       (tRect.fH > 0.0f) &&
	       (fX >= tRect.fX) &&
	       (fY >= tRect.fY) &&
	       (fX < (tRect.fX + tRect.fW)) &&
	       (fY < (tRect.fY + tRect.fH));
}

static int __xuiScrollBarHitPart(xui_widget pWidget, xui_scrollbar_data_t* pData, const xui_scrollbar_data_t* pResolved, float fX, float fY)
{
	xui_rect_t tWorld;
	xui_rect_t tContent;
	float fLocalX;
	float fLocalY;

	if ( (pWidget == NULL) || (pData == NULL) || (pResolved == NULL) ) {
		return XUI_SCROLLBAR_PART_NONE;
	}
	tWorld = xuiWidgetGetWorldRect(pWidget);
	fLocalX = fX - tWorld.fX;
	fLocalY = fY - tWorld.fY;
	tContent = xuiWidgetGetContentRect(pWidget);
	if ( !__xuiScrollBarRectContains(tContent, fLocalX, fLocalY) ) {
		return XUI_SCROLLBAR_PART_NONE;
	}
	__xuiScrollBarUpdateRects(pWidget, pData, pResolved);
	if ( __xuiScrollBarRectContains(pData->tDecreaseRect, fLocalX, fLocalY) ) {
		return XUI_SCROLLBAR_PART_DECREASE;
	}
	if ( __xuiScrollBarRectContains(pData->tIncreaseRect, fLocalX, fLocalY) ) {
		return XUI_SCROLLBAR_PART_INCREASE;
	}
	if ( __xuiScrollBarRectContains(pData->tThumbRect, fLocalX, fLocalY) ) {
		return XUI_SCROLLBAR_PART_THUMB;
	}
	if ( __xuiScrollBarRectContains(pData->tTrackRect, fLocalX, fLocalY) ) {
		return XUI_SCROLLBAR_PART_TRACK;
	}
	return XUI_SCROLLBAR_PART_NONE;
}

static float __xuiScrollBarEventAxis(xui_widget pWidget, const xui_scrollbar_data_t* pData, const xui_event_t* pEvent)
{
	xui_rect_t tWorld;

	tWorld = xuiWidgetGetWorldRect(pWidget);
	if ( pData->iOrientation == XUI_ORIENTATION_HORIZONTAL ) {
		return pEvent->fX - tWorld.fX;
	}
	return pEvent->fY - tWorld.fY;
}

static int __xuiScrollBarTrackDirectionAtAxis(xui_scrollbar_data_t* pData, float fMouse)
{
	float fThumbStart;
	float fThumbEnd;

	if ( pData == NULL ) {
		return 0;
	}
	fThumbStart = __xuiScrollBarAxisStart(pData, pData->tThumbRect);
	fThumbEnd = fThumbStart + __xuiScrollBarAxisLen(pData, pData->tThumbRect);
	if ( fMouse < fThumbStart ) {
		return -1;
	}
	if ( fMouse > fThumbEnd ) {
		return 1;
	}
	return 0;
}

static int __xuiScrollBarTrackDirection(xui_scrollbar_data_t* pData, float fLocalX, float fLocalY)
{
	if ( pData == NULL ) {
		return 0;
	}
	return __xuiScrollBarTrackDirectionAtAxis(pData, __xuiScrollBarAxisPoint(pData, fLocalX, fLocalY));
}

static int __xuiScrollBarSetParts(xui_widget pWidget, xui_scrollbar_data_t* pData, int iHoverPart, int iActivePart)
{
	int bChanged;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( !__xuiScrollBarPartValid(iHoverPart) ) iHoverPart = XUI_SCROLLBAR_PART_NONE;
	if ( !__xuiScrollBarPartValid(iActivePart) ) iActivePart = XUI_SCROLLBAR_PART_NONE;
	bChanged = (pData->iHoverPart != iHoverPart) || (pData->iActivePart != iActivePart);
	pData->iHoverPart = iHoverPart;
	pData->iActivePart = iActivePart;
	if ( bChanged ) {
		(void)__xuiScrollBarSyncState(pWidget, pData);
		return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return __xuiScrollBarSyncState(pWidget, pData);
}

static int __xuiScrollBarSetValueInternal(xui_widget pWidget, xui_scrollbar_data_t* pData, float fValue, int bNotify)
{
	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	fValue = __xuiScrollBarClampFloat(fValue, pData->fMin, pData->fMax);
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

static int __xuiScrollBarStepValue(xui_widget pWidget, xui_scrollbar_data_t* pData, float fStep)
{
	return __xuiScrollBarSetValueInternal(pWidget, pData, pData->fValue + fStep, 1);
}

static void __xuiScrollBarStopRepeat(xui_scrollbar_data_t* pData)
{
	if ( pData == NULL ) {
		return;
	}
	pData->iRepeatPart = XUI_SCROLLBAR_PART_NONE;
	pData->iRepeatDirection = 0;
	pData->fRepeatMouse = 0.0f;
	pData->fRepeatElapsed = 0.0f;
	pData->fRepeatNext = 0.0f;
}

static void __xuiScrollBarStartRepeat(xui_scrollbar_data_t* pData, int iPart, int iDirection, float fMouse)
{
	if ( pData == NULL ) {
		return;
	}
	if ( ((iPart != XUI_SCROLLBAR_PART_DECREASE) &&
	      (iPart != XUI_SCROLLBAR_PART_INCREASE) &&
	      (iPart != XUI_SCROLLBAR_PART_TRACK)) ||
	     (iDirection == 0) ) {
		__xuiScrollBarStopRepeat(pData);
		return;
	}
	pData->iRepeatPart = iPart;
	pData->iRepeatDirection = (iDirection < 0) ? -1 : 1;
	pData->fRepeatMouse = fMouse;
	pData->fRepeatElapsed = 0.0f;
	pData->fRepeatNext = XUI_SCROLLBAR_REPEAT_DELAY;
}

static int __xuiScrollBarRepeatOnce(xui_widget pWidget, xui_scrollbar_data_t* pData)
{
	xui_scrollbar_data_t tResolved;
	float fBefore;
	int iDirection;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiScrollBarResolve(pWidget, pData, &tResolved);
	if ( pData->iRepeatPart == XUI_SCROLLBAR_PART_DECREASE ) {
		return __xuiScrollBarStepValue(pWidget, pData, -__xuiScrollBarSmallStep(&tResolved));
	}
	if ( pData->iRepeatPart == XUI_SCROLLBAR_PART_INCREASE ) {
		return __xuiScrollBarStepValue(pWidget, pData, __xuiScrollBarSmallStep(&tResolved));
	}
	if ( pData->iRepeatPart == XUI_SCROLLBAR_PART_TRACK ) {
		__xuiScrollBarUpdateRects(pWidget, pData, &tResolved);
		iDirection = __xuiScrollBarTrackDirectionAtAxis(pData, pData->fRepeatMouse);
		if ( (iDirection == 0) || (iDirection != pData->iRepeatDirection) ) {
			__xuiScrollBarStopRepeat(pData);
			return XUI_OK;
		}
		fBefore = pData->fValue;
		iRet = __xuiScrollBarStepValue(pWidget, pData, __xuiScrollBarLargeStep(&tResolved) * (float)iDirection);
		if ( (iRet == XUI_OK) && (pData->fValue == fBefore) ) {
			__xuiScrollBarStopRepeat(pData);
		}
		return iRet;
	}
	__xuiScrollBarStopRepeat(pData);
	return XUI_OK;
}

static int __xuiScrollBarUpdateRepeat(xui_widget pWidget, xui_scrollbar_data_t* pData, float fDelta)
{
	xui_context pContext;
	int iRet;
	int iSteps;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->iRepeatPart == XUI_SCROLLBAR_PART_NONE ) {
		return XUI_OK;
	}
	pContext = xuiWidgetGetContext(pWidget);
	if ( (xuiGetPointerCapture(pContext) != pWidget) ||
	     !xuiWidgetGetEnabled(pWidget) ||
	     !xuiWidgetGetVisible(pWidget) ||
	     (pData->bDraggingThumb != 0) ) {
		__xuiScrollBarStopRepeat(pData);
		return XUI_OK;
	}
	pData->fRepeatElapsed += fDelta;
	iSteps = 0;
	while ( (pData->iRepeatPart != XUI_SCROLLBAR_PART_NONE) &&
	        (pData->fRepeatElapsed >= pData->fRepeatNext) &&
	        (iSteps < XUI_SCROLLBAR_REPEAT_MAX_STEPS) ) {
		iRet = __xuiScrollBarRepeatOnce(pWidget, pData);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		pData->fRepeatNext += XUI_SCROLLBAR_REPEAT_INTERVAL;
		iSteps++;
	}
	if ( (pData->iRepeatPart != XUI_SCROLLBAR_PART_NONE) && (iSteps >= XUI_SCROLLBAR_REPEAT_MAX_STEPS) ) {
		pData->fRepeatNext = pData->fRepeatElapsed + XUI_SCROLLBAR_REPEAT_INTERVAL;
	}
	return XUI_OK;
}

static int __xuiScrollBarUpdate(xui_widget pWidget, float fDelta, void* pUser)
{
	return __xuiScrollBarUpdateRepeat(pWidget, (xui_scrollbar_data_t*)pUser, fDelta);
}

static int __xuiScrollBarDragValue(xui_widget pWidget, xui_scrollbar_data_t* pData, const xui_event_t* pEvent)
{
	xui_scrollbar_data_t tResolved;
	xui_rect_t tTrack;
	float fMouse;
	float fTrackLen;
	float fThumbLen;
	float fTravel;
	float fRange;

	if ( (pWidget == NULL) || (pData == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiScrollBarResolve(pWidget, pData, &tResolved);
	__xuiScrollBarUpdateRects(pWidget, pData, &tResolved);
	tTrack = pData->tTrackRect;
	fTrackLen = __xuiScrollBarAxisLen(&tResolved, tTrack);
	fThumbLen = __xuiScrollBarAxisLen(&tResolved, pData->tThumbRect);
	fTravel = fTrackLen - fThumbLen;
	fRange = __xuiScrollBarRange(pData);
	if ( (fTravel <= 0.0f) || (fRange <= 0.0f) ) {
		return XUI_OK;
	}
	fMouse = __xuiScrollBarEventAxis(pWidget, pData, pEvent);
	return __xuiScrollBarSetValueInternal(pWidget, pData, pData->fDragStartValue + ((fMouse - pData->fDragStartMouse) / fTravel) * fRange, 1);
}

static void __xuiScrollBarCancelDrag(xui_widget pWidget, xui_scrollbar_data_t* pData)
{
	xui_context pContext;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return;
	}
	pData->bDraggingThumb = 0;
	__xuiScrollBarStopRepeat(pData);
	(void)__xuiScrollBarSetParts(pWidget, pData, pData->iHoverPart, XUI_SCROLLBAR_PART_NONE);
	pContext = xuiWidgetGetContext(pWidget);
	if ( xuiGetPointerCapture(pContext) == pWidget ) {
		(void)xuiReleasePointerCapture(pContext, pWidget);
	}
}

static void __xuiScrollBarCancelAction(xui_widget pWidget, void* pUser)
{
	xui_scrollbar_data_t* pData;

	(void)pUser;
	pData = __xuiScrollBarGetData(pWidget);
	__xuiScrollBarCancelDrag(pWidget, pData);
}

static int __xuiScrollBarPointerDown(xui_widget pWidget, xui_scrollbar_data_t* pData, const xui_event_t* pEvent)
{
	xui_scrollbar_data_t tResolved;
	xui_context pContext;
	xui_rect_t tWorld;
	int iPart;
	int iDirection;
	float fLocalX;
	float fLocalY;

	if ( pEvent->iButton != XUI_POINTER_BUTTON_LEFT ) {
		return XUI_OK;
	}
	if ( !xuiWidgetGetEnabled(pWidget) ) {
		return XUI_OK;
	}
	__xuiScrollBarResolve(pWidget, pData, &tResolved);
	iPart = __xuiScrollBarHitPart(pWidget, pData, &tResolved, pEvent->fX, pEvent->fY);
	if ( iPart == XUI_SCROLLBAR_PART_NONE ) {
		__xuiScrollBarStopRepeat(pData);
		(void)__xuiScrollBarSetParts(pWidget, pData, XUI_SCROLLBAR_PART_NONE, XUI_SCROLLBAR_PART_NONE);
		return XUI_OK;
	}
	pContext = xuiWidgetGetContext(pWidget);
	(void)xuiSetPointerCapture(pContext, pWidget);
	(void)__xuiScrollBarSetParts(pWidget, pData, iPart, iPart);
	if ( iPart == XUI_SCROLLBAR_PART_THUMB ) {
		__xuiScrollBarStopRepeat(pData);
		pData->bDraggingThumb = 1;
		pData->fDragStartMouse = __xuiScrollBarEventAxis(pWidget, pData, pEvent);
		pData->fDragStartValue = pData->fValue;
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( iPart == XUI_SCROLLBAR_PART_DECREASE ) {
		(void)__xuiScrollBarStepValue(pWidget, pData, -__xuiScrollBarSmallStep(&tResolved));
		__xuiScrollBarStartRepeat(pData, iPart, -1, __xuiScrollBarEventAxis(pWidget, pData, pEvent));
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( iPart == XUI_SCROLLBAR_PART_INCREASE ) {
		(void)__xuiScrollBarStepValue(pWidget, pData, __xuiScrollBarSmallStep(&tResolved));
		__xuiScrollBarStartRepeat(pData, iPart, 1, __xuiScrollBarEventAxis(pWidget, pData, pEvent));
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( iPart == XUI_SCROLLBAR_PART_TRACK ) {
		tWorld = xuiWidgetGetWorldRect(pWidget);
		fLocalX = pEvent->fX - tWorld.fX;
		fLocalY = pEvent->fY - tWorld.fY;
		iDirection = __xuiScrollBarTrackDirection(pData, fLocalX, fLocalY);
		if ( iDirection != 0 ) {
			(void)__xuiScrollBarStepValue(pWidget, pData, __xuiScrollBarLargeStep(&tResolved) * (float)iDirection);
			__xuiScrollBarStartRepeat(pData, iPart, iDirection, __xuiScrollBarAxisPoint(pData, fLocalX, fLocalY));
		} else {
			__xuiScrollBarStopRepeat(pData);
		}
		return XUI_EVENT_DISPATCH_STOP;
	}
	return XUI_OK;
}

static int __xuiScrollBarPointerMove(xui_widget pWidget, xui_scrollbar_data_t* pData, const xui_event_t* pEvent)
{
	xui_scrollbar_data_t tResolved;
	xui_context pContext;
	int iPart;

	pContext = xuiWidgetGetContext(pWidget);
	if ( (pData->bDraggingThumb != 0) && (xuiGetPointerCapture(pContext) == pWidget) ) {
		(void)__xuiScrollBarDragValue(pWidget, pData, pEvent);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( (pData->iRepeatPart == XUI_SCROLLBAR_PART_TRACK) && (xuiGetPointerCapture(pContext) == pWidget) ) {
		pData->fRepeatMouse = __xuiScrollBarEventAxis(pWidget, pData, pEvent);
	}
	__xuiScrollBarResolve(pWidget, pData, &tResolved);
	iPart = __xuiScrollBarHitPart(pWidget, pData, &tResolved, pEvent->fX, pEvent->fY);
	(void)__xuiScrollBarSetParts(pWidget, pData, iPart, pData->iActivePart);
	return XUI_OK;
}

static int __xuiScrollBarPointerUp(xui_widget pWidget, xui_scrollbar_data_t* pData, const xui_event_t* pEvent)
{
	xui_context pContext;
	int bWasActive;

	if ( pEvent->iButton != XUI_POINTER_BUTTON_LEFT ) {
		return XUI_OK;
	}
	pContext = xuiWidgetGetContext(pWidget);
	bWasActive = (pData->bDraggingThumb != 0) ||
	             (pData->iActivePart != XUI_SCROLLBAR_PART_NONE) ||
	             (xuiGetPointerCapture(pContext) == pWidget);
	pData->bDraggingThumb = 0;
	__xuiScrollBarStopRepeat(pData);
	(void)__xuiScrollBarSetParts(pWidget, pData, pData->iHoverPart, XUI_SCROLLBAR_PART_NONE);
	if ( xuiGetPointerCapture(pContext) == pWidget ) {
		(void)xuiReleasePointerCapture(pContext, pWidget);
	}
	return bWasActive ? XUI_EVENT_DISPATCH_STOP : XUI_OK;
}

static int __xuiScrollBarWheel(xui_widget pWidget, xui_scrollbar_data_t* pData, const xui_event_t* pEvent)
{
	xui_scrollbar_data_t tResolved;
	float fDelta;
	float fStep;

	if ( !xuiWidgetGetEnabled(pWidget) ) {
		return XUI_OK;
	}
	fDelta = (__xuiScrollBarAbsFloat(pEvent->fWheelY) >= __xuiScrollBarAbsFloat(pEvent->fWheelX)) ? pEvent->fWheelY : pEvent->fWheelX;
	if ( fDelta == 0.0f ) {
		return XUI_OK;
	}
	__xuiScrollBarResolve(pWidget, pData, &tResolved);
	fStep = __xuiScrollBarLargeStep(&tResolved);
	(void)__xuiScrollBarStepValue(pWidget, pData, -fDelta * fStep);
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiScrollBarKeyDown(xui_widget pWidget, xui_scrollbar_data_t* pData, const xui_event_t* pEvent)
{
	xui_scrollbar_data_t tResolved;
	float fStep;

	if ( !xuiWidgetGetEnabled(pWidget) ) {
		return XUI_OK;
	}
	__xuiScrollBarResolve(pWidget, pData, &tResolved);
	switch ( pEvent->iKey ) {
	case XUI_KEY_ESCAPE:
		__xuiScrollBarCancelDrag(pWidget, pData);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_HOME:
		(void)__xuiScrollBarSetValueInternal(pWidget, pData, pData->fMin, 1);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_END:
		(void)__xuiScrollBarSetValueInternal(pWidget, pData, pData->fMax, 1);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_PAGE_UP:
		(void)__xuiScrollBarStepValue(pWidget, pData, -__xuiScrollBarLargeStep(&tResolved));
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_PAGE_DOWN:
		(void)__xuiScrollBarStepValue(pWidget, pData, __xuiScrollBarLargeStep(&tResolved));
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_UP:
	case XUI_KEY_LEFT:
		fStep = -__xuiScrollBarSmallStep(&tResolved);
		(void)__xuiScrollBarStepValue(pWidget, pData, fStep);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_KEY_DOWN:
	case XUI_KEY_RIGHT:
		fStep = __xuiScrollBarSmallStep(&tResolved);
		(void)__xuiScrollBarStepValue(pWidget, pData, fStep);
		return XUI_EVENT_DISPATCH_STOP;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiScrollBarEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_scrollbar_data_t* pData;
	int iRet;

	(void)pUser;
	if ( (pWidget == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiScrollBarGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_ENTER:
	case XUI_EVENT_POINTER_MOVE:
		return __xuiScrollBarPointerMove(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_LEAVE:
		if ( !pData->bDraggingThumb ) {
			return __xuiScrollBarSetParts(pWidget, pData, XUI_SCROLLBAR_PART_NONE, pData->iActivePart);
		}
		break;
	case XUI_EVENT_POINTER_DOWN:
		return __xuiScrollBarPointerDown(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_UP:
		return __xuiScrollBarPointerUp(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_CLICK:
		if ( pEvent->iButton == XUI_POINTER_BUTTON_LEFT ) {
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_WHEEL:
		return __xuiScrollBarWheel(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		pData->bDraggingThumb = 0;
		__xuiScrollBarStopRepeat(pData);
		return __xuiScrollBarSetParts(pWidget, pData, pData->iHoverPart, XUI_SCROLLBAR_PART_NONE);
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
		return __xuiScrollBarSyncState(pWidget, pData);
	case XUI_EVENT_KEY_DOWN:
		return __xuiScrollBarKeyDown(pWidget, pData, pEvent);
	case XUI_EVENT_ENABLED_CHANGED:
	case XUI_EVENT_VISIBLE_CHANGED:
		if ( !xuiWidgetGetEnabled(pWidget) || !xuiWidgetGetVisible(pWidget) ) {
			__xuiScrollBarCancelDrag(pWidget, pData);
		}
		iRet = __xuiScrollBarSyncState(pWidget, pData);
		if ( iRet != XUI_OK ) return iRet;
		return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiScrollBarDrawRectFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || (__xuiScrollBarColorAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	return pProxy->drawRectFill(pProxy, pDraw, tRect, iColor);
}

static int __xuiScrollBarDrawRectStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || (fWidth <= 0.0f) || (__xuiScrollBarColorAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	fWidth = xuiInternalSnapSize(fWidth);
	tRect = xuiInternalStrokeCenterRectInside(tRect, fWidth, NULL);
	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XUI_OK;
	}
	return pProxy->drawRectStroke(pProxy, pDraw, tRect, fWidth, iColor);
}

static int __xuiScrollBarDrawArrow(xui_proxy pProxy, xui_draw_context pDraw, const xui_scrollbar_data_t* pData, xui_rect_t tRect, int bEnd, uint32_t iColor)
{
	xui_vec2_t tA;
	xui_vec2_t tB;
	xui_vec2_t tC;
	float fCX;
	float fCY;
	float fSize;

	if ( (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) || (__xuiScrollBarColorAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	fCX = tRect.fX + tRect.fW * 0.5f;
	fCY = tRect.fY + tRect.fH * 0.5f;
	fSize = __xuiScrollBarMinFloat(tRect.fW, tRect.fH) * 0.26f;
	if ( fSize < 3.0f ) {
		fSize = 3.0f;
	}
	if ( pData->iOrientation == XUI_ORIENTATION_HORIZONTAL ) {
		if ( bEnd ) {
			tA = (xui_vec2_t){fCX + fSize * 0.55f, fCY};
			tB = (xui_vec2_t){fCX - fSize * 0.45f, fCY - fSize};
			tC = (xui_vec2_t){fCX - fSize * 0.45f, fCY + fSize};
		} else {
			tA = (xui_vec2_t){fCX - fSize * 0.55f, fCY};
			tB = (xui_vec2_t){fCX + fSize * 0.45f, fCY - fSize};
			tC = (xui_vec2_t){fCX + fSize * 0.45f, fCY + fSize};
		}
	} else if ( bEnd ) {
		tA = (xui_vec2_t){fCX, fCY + fSize * 0.55f};
		tB = (xui_vec2_t){fCX - fSize, fCY - fSize * 0.45f};
		tC = (xui_vec2_t){fCX + fSize, fCY - fSize * 0.45f};
	} else {
		tA = (xui_vec2_t){fCX, fCY - fSize * 0.55f};
		tB = (xui_vec2_t){fCX - fSize, fCY + fSize * 0.45f};
		tC = (xui_vec2_t){fCX + fSize, fCY + fSize * 0.45f};
	}
	return pProxy->drawTriangleFill(pProxy, pDraw, tA, tB, tC, iColor);
}

static uint32_t __xuiScrollBarPartFillColor(const xui_scrollbar_data_t* pData, uint32_t iStateId, int iPart)
{
	int iHoverPart;
	int iActivePart;

	if ( (iStateId & XUI_WIDGET_STATE_DISABLED) != 0 ) {
		return pData->iDisabledColor;
	}
	iHoverPart = __xuiScrollBarStateHoverPart(iStateId);
	iActivePart = __xuiScrollBarStateActivePart(iStateId);
	if ( iActivePart == iPart ) {
		return pData->iActiveColor;
	}
	if ( iHoverPart == iPart ) {
		return pData->iHoverColor;
	}
	return pData->iThumbColor;
}

static int __xuiScrollBarCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_scrollbar_data_t* pData;
	xui_scrollbar_data_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tContent;
	xui_rect_t tTrackVisual;
	xui_rect_t tFocus;
	xui_rect_t tButtonVisual;
	uint32_t iTrackColor;
	uint32_t iThumbColor;
	uint32_t iButtonColor;
	int iRet;

	(void)pUser;
	pData = __xuiScrollBarGetData(pWidget);
	if ( (pData == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	__xuiScrollBarResolve(pWidget, pData, &tResolved);
	__xuiScrollBarUpdateRects(pWidget, pData, &tResolved);
	tContent = xuiWidgetGetContentRect(pWidget);
	tTrackVisual = __xuiScrollBarVisualTrackRect(&tResolved, pData->tTrackRect, tContent);
	iTrackColor = ((iStateId & XUI_WIDGET_STATE_DISABLED) != 0) ? __xuiScrollBarColorWithAlpha(tResolved.iDisabledColor, 88) : tResolved.iTrackColor;
	iThumbColor = __xuiScrollBarPartFillColor(&tResolved, iStateId, XUI_SCROLLBAR_PART_THUMB);
	if ( tResolved.iMode == XUI_SCROLLBAR_MODE_FULL ) {
		iButtonColor = ((iStateId & XUI_WIDGET_STATE_DISABLED) != 0) ? __xuiScrollBarColorWithAlpha(tResolved.iButtonColor, 120) : tResolved.iButtonColor;
		if ( __xuiScrollBarStateActivePart(iStateId) == XUI_SCROLLBAR_PART_DECREASE ) {
			iButtonColor = __xuiScrollBarColorWithAlpha(tResolved.iActiveColor, 120);
		} else if ( __xuiScrollBarStateHoverPart(iStateId) == XUI_SCROLLBAR_PART_DECREASE ) {
			iButtonColor = __xuiScrollBarColorWithAlpha(tResolved.iHoverColor, 100);
		}
		tButtonVisual = __xuiScrollBarButtonVisualRect(pData->tDecreaseRect);
		iRet = __xuiScrollBarDrawRectFill(pProxy, pDraw, tButtonVisual, iButtonColor);
		if ( iRet != XUI_OK ) return iRet;
		iButtonColor = ((iStateId & XUI_WIDGET_STATE_DISABLED) != 0) ? __xuiScrollBarColorWithAlpha(tResolved.iButtonColor, 120) : tResolved.iButtonColor;
		if ( __xuiScrollBarStateActivePart(iStateId) == XUI_SCROLLBAR_PART_INCREASE ) {
			iButtonColor = __xuiScrollBarColorWithAlpha(tResolved.iActiveColor, 120);
		} else if ( __xuiScrollBarStateHoverPart(iStateId) == XUI_SCROLLBAR_PART_INCREASE ) {
			iButtonColor = __xuiScrollBarColorWithAlpha(tResolved.iHoverColor, 100);
		}
		tButtonVisual = __xuiScrollBarButtonVisualRect(pData->tIncreaseRect);
		iRet = __xuiScrollBarDrawRectFill(pProxy, pDraw, tButtonVisual, iButtonColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	iRet = __xuiScrollBarDrawRectFill(pProxy, pDraw, tTrackVisual, iTrackColor);
	if ( iRet != XUI_OK ) return iRet;
	if ( tResolved.iMode == XUI_SCROLLBAR_MODE_FULL && __xuiScrollBarColorAlpha(tResolved.iButtonIconColor) != 0 ) {
		iRet = __xuiScrollBarDrawArrow(pProxy, pDraw, &tResolved, __xuiScrollBarButtonVisualRect(pData->tDecreaseRect), 0, ((iStateId & XUI_WIDGET_STATE_DISABLED) != 0) ? __xuiScrollBarColorWithAlpha(tResolved.iButtonIconColor, 100) : tResolved.iButtonIconColor);
		if ( iRet != XUI_OK ) return iRet;
		iRet = __xuiScrollBarDrawArrow(pProxy, pDraw, &tResolved, __xuiScrollBarButtonVisualRect(pData->tIncreaseRect), 1, ((iStateId & XUI_WIDGET_STATE_DISABLED) != 0) ? __xuiScrollBarColorWithAlpha(tResolved.iButtonIconColor, 100) : tResolved.iButtonIconColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	iRet = __xuiScrollBarDrawRectFill(pProxy, pDraw, pData->tThumbRect, iThumbColor);
	if ( iRet != XUI_OK ) return iRet;
	if ( (iStateId & XUI_WIDGET_STATE_FOCUS) != 0 ) {
		tFocus = xuiInternalInsetRect(tTrackVisual, -1.0f);
		iRet = __xuiScrollBarDrawRectStroke(pProxy, pDraw, tFocus, 1.0f, __xuiScrollBarColorWithAlpha(tResolved.iFocusColor, 130));
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiScrollBarContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_scrollbar_data_t* pData;
	xui_scrollbar_data_t tResolved;
	float fThickness;
	float fLong;

	(void)tConstraint;
	(void)pUser;
	if ( pSize == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiScrollBarGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiScrollBarResolve(pWidget, pData, &tResolved);
	fThickness = (tResolved.fThickness > 0.0f) ? tResolved.fThickness : ((tResolved.iMode == XUI_SCROLLBAR_MODE_COMPACT) ? 8.0f : 18.0f);
	fLong = fThickness * 6.0f;
	if ( fLong < 96.0f ) {
		fLong = 96.0f;
	}
	if ( tResolved.iOrientation == XUI_ORIENTATION_HORIZONTAL ) {
		*pSize = (xui_vec2_t){fLong, fThickness};
	} else {
		*pSize = (xui_vec2_t){fThickness, fLong};
	}
	return XUI_OK;
}

static void __xuiScrollBarDefaultLayout(xui_layout_t* pLayout)
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

static void __xuiScrollBarDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiScrollBarInitCacheStates(xui_widget pWidget)
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
		iRet = xuiWidgetSetCacheStateId(pWidget, i, 0x78000000u + (uint32_t)i);
		if ( iRet != XUI_OK ) return iRet;
	}
	for ( i = 0; i < (int)(sizeof(arrState) / sizeof(arrState[0])); i++ ) {
		iRet = xuiWidgetSetCacheStateId(pWidget, i, arrState[i]);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiScrollBarInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetCancelAction(pWidget, __xuiScrollBarCancelAction, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_ENTER, __xuiScrollBarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiScrollBarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiScrollBarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiScrollBarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiScrollBarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CLICK, __xuiScrollBarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_WHEEL, __xuiScrollBarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiScrollBarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiScrollBarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiScrollBarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiScrollBarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_ENABLED_CHANGED, __xuiScrollBarEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_VISIBLE_CHANGED, __xuiScrollBarEvent, NULL);
	return iRet;
}

static int __xuiScrollBarInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_scrollbar_data_t* pData;
	const xui_scrollbar_desc_t* pDesc;
	float fMin;
	float fMax;
	float fPage;
	int iRet;

	(void)pUser;
	pData = (xui_scrollbar_data_t*)pTypeData;
	pDesc = (const xui_scrollbar_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiScrollBarDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(pData, 0, sizeof(*pData));
	fMin = (pDesc != NULL) ? pDesc->fMin : 0.0f;
	fMax = (pDesc != NULL) ? pDesc->fMax : 1.0f;
	if ( (pDesc == NULL) || (fMin == 0.0f && fMax == 0.0f) ) {
		fMax = 1.0f;
	}
	fPage = (pDesc != NULL && pDesc->fPage > 0.0f) ? pDesc->fPage : 0.2f;
	__xuiScrollBarNormalizeRange(&fMin, &fMax, &fPage);
	pData->fMin = fMin;
	pData->fMax = fMax;
	pData->fPage = fPage;
	pData->fValue = __xuiScrollBarClampFloat((pDesc != NULL) ? pDesc->fValue : fMin, fMin, fMax);
	pData->fSmallStep = (pDesc != NULL && pDesc->fSmallStep > 0.0f) ? pDesc->fSmallStep : 0.0f;
	pData->fLargeStep = (pDesc != NULL && pDesc->fLargeStep > 0.0f) ? pDesc->fLargeStep : 0.0f;
	pData->fThickness = (pDesc != NULL && pDesc->fThickness > 0.0f) ? pDesc->fThickness : 0.0f;
	pData->fMinThumbSize = (pDesc != NULL && pDesc->fMinThumbSize > 0.0f) ? pDesc->fMinThumbSize : 18.0f;
	pData->fButtonSize = (pDesc != NULL && pDesc->fButtonSize > 0.0f) ? pDesc->fButtonSize : 0.0f;
	pData->iTrackColor = (pDesc != NULL && pDesc->iTrackColor != 0) ? pDesc->iTrackColor : XUI_COLOR_RGBA(222, 232, 243, 255);
	pData->iThumbColor = (pDesc != NULL && pDesc->iThumbColor != 0) ? pDesc->iThumbColor : XUI_COLOR_RGBA(126, 161, 196, 245);
	pData->iHoverColor = (pDesc != NULL && pDesc->iHoverColor != 0) ? pDesc->iHoverColor : XUI_COLOR_RGBA(76, 136, 204, 250);
	pData->iActiveColor = (pDesc != NULL && pDesc->iActiveColor != 0) ? pDesc->iActiveColor : XUI_COLOR_RGBA(47, 128, 237, 255);
	pData->iFocusColor = (pDesc != NULL && pDesc->iFocusColor != 0) ? pDesc->iFocusColor : XUI_COLOR_RGBA(47, 128, 237, 180);
	pData->iDisabledColor = (pDesc != NULL && pDesc->iDisabledColor != 0) ? pDesc->iDisabledColor : XUI_COLOR_RGBA(181, 190, 204, 135);
	pData->iButtonColor = (pDesc != NULL && pDesc->iButtonColor != 0) ? pDesc->iButtonColor : XUI_COLOR_RGBA(244, 248, 253, 255);
	pData->iButtonIconColor = (pDesc != NULL && pDesc->iButtonIconColor != 0) ? pDesc->iButtonIconColor : XUI_COLOR_RGBA(88, 114, 145, 255);
	pData->iOrientation = (pDesc != NULL && __xuiScrollBarOrientationValid(pDesc->iOrientation)) ? pDesc->iOrientation : XUI_ORIENTATION_VERTICAL;
	pData->iMode = (pDesc != NULL && __xuiScrollBarModeValid(pDesc->iMode)) ? pDesc->iMode : XUI_SCROLLBAR_MODE_FULL;
	pData->iButtonMode = (pDesc != NULL && __xuiScrollBarButtonModeValid(pDesc->iButtonMode)) ? pDesc->iButtonMode : XUI_SCROLLBAR_BUTTONS_AUTO;
	pData->iHoverPart = XUI_SCROLLBAR_PART_NONE;
	pData->iActivePart = XUI_SCROLLBAR_PART_NONE;
	(void)xuiWidgetSetPadding(pWidget, (xui_thickness_t){0.0f, 0.0f, 0.0f, 0.0f});
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_CLIP);
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	iRet = __xuiScrollBarInitCacheStates(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiScrollBarInitEvents(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiScrollBarSyncState(pWidget, pData);
}

static void __xuiScrollBarDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_scrollbar_data_t* pData;

	(void)pUser;
	pData = (xui_scrollbar_data_t*)pTypeData;
	if ( pData != NULL ) {
		__xuiScrollBarCancelDrag(pWidget, pData);
		memset(pData, 0, sizeof(*pData));
	}
}

static void __xuiScrollBarRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
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

static void __xuiScrollBarRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;
	uint32_t iLayoutDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	iLayoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiScrollBarRegisterStyleProperty(pContext, pType, "scrollbar.track.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiScrollBarRegisterStyleProperty(pContext, pType, "scrollbar.thumb.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiScrollBarRegisterStyleProperty(pContext, pType, "scrollbar.thumb.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiScrollBarRegisterStyleProperty(pContext, pType, "scrollbar.thumb.active_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiScrollBarRegisterStyleProperty(pContext, pType, "scrollbar.focus.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiScrollBarRegisterStyleProperty(pContext, pType, "scrollbar.disabled.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiScrollBarRegisterStyleProperty(pContext, pType, "scrollbar.button.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiScrollBarRegisterStyleProperty(pContext, pType, "scrollbar.button.icon_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiScrollBarRegisterStyleProperty(pContext, pType, "scrollbar.thickness", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiScrollBarRegisterStyleProperty(pContext, pType, "scrollbar.min_thumb_size", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiScrollBarRegisterStyleProperty(pContext, pType, "scrollbar.button_size", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
}

XUI_API xui_widget_type xuiScrollBarGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "scrollbar");
	if ( pType != NULL ) {
		__xuiScrollBarRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "scrollbar";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_scrollbar_data_t);
	tDesc.onInit = __xuiScrollBarInit;
	tDesc.onDestroy = __xuiScrollBarDestroy;
	tDesc.onContentMeasure = __xuiScrollBarContentMeasure;
	tDesc.onCacheRender = __xuiScrollBarCacheRender;
	tDesc.onUpdate = __xuiScrollBarUpdate;
	__xuiScrollBarDefaultLayout(&tDesc.tLayout);
	__xuiScrollBarDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	__xuiScrollBarRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiScrollBarCreate(xui_context pContext, xui_widget* ppWidget, const xui_scrollbar_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiScrollBarDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiScrollBarGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiScrollBarSetChange(xui_widget pWidget, xui_scrollbar_change_proc onChange, void* pUser)
{
	xui_scrollbar_data_t* pData = __xuiScrollBarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onChange = onChange;
	pData->pChangeUser = pUser;
	return XUI_OK;
}

XUI_API int xuiScrollBarSetRange(xui_widget pWidget, float fMin, float fMax, float fPage)
{
	xui_scrollbar_data_t* pData;

	pData = __xuiScrollBarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiScrollBarNormalizeRange(&fMin, &fMax, &fPage);
	pData->fMin = fMin;
	pData->fMax = fMax;
	pData->fPage = fPage;
	(void)__xuiScrollBarSetValueInternal(pWidget, pData, pData->fValue, 0);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiScrollBarGetRange(xui_widget pWidget, float* pMin, float* pMax, float* pPage)
{
	xui_scrollbar_data_t* pData = __xuiScrollBarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pMin != NULL ) *pMin = pData->fMin;
	if ( pMax != NULL ) *pMax = pData->fMax;
	if ( pPage != NULL ) *pPage = pData->fPage;
	return XUI_OK;
}

XUI_API int xuiScrollBarSetPage(xui_widget pWidget, float fPage)
{
	xui_scrollbar_data_t* pData = __xuiScrollBarGetData(pWidget);
	if ( (pData == NULL) || (fPage < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fPage = fPage;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiScrollBarGetPage(xui_widget pWidget)
{
	xui_scrollbar_data_t* pData = __xuiScrollBarGetData(pWidget);
	return (pData != NULL) ? pData->fPage : 0.0f;
}

XUI_API int xuiScrollBarSetValue(xui_widget pWidget, float fValue)
{
	xui_scrollbar_data_t* pData = __xuiScrollBarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiScrollBarSetValueInternal(pWidget, pData, fValue, 0);
}

XUI_API float xuiScrollBarGetValue(xui_widget pWidget)
{
	xui_scrollbar_data_t* pData = __xuiScrollBarGetData(pWidget);
	return (pData != NULL) ? pData->fValue : 0.0f;
}

XUI_API int xuiScrollBarSetSteps(xui_widget pWidget, float fSmallStep, float fLargeStep)
{
	xui_scrollbar_data_t* pData = __xuiScrollBarGetData(pWidget);
	if ( (pData == NULL) || (fSmallStep < 0.0f) || (fLargeStep < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fSmallStep = fSmallStep;
	pData->fLargeStep = fLargeStep;
	return XUI_OK;
}

XUI_API int xuiScrollBarGetSteps(xui_widget pWidget, float* pSmallStep, float* pLargeStep)
{
	xui_scrollbar_data_t* pData = __xuiScrollBarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pSmallStep != NULL ) *pSmallStep = pData->fSmallStep;
	if ( pLargeStep != NULL ) *pLargeStep = pData->fLargeStep;
	return XUI_OK;
}

XUI_API int xuiScrollBarSetOrientation(xui_widget pWidget, int iOrientation)
{
	xui_scrollbar_data_t* pData = __xuiScrollBarGetData(pWidget);
	if ( (pData == NULL) || !__xuiScrollBarOrientationValid(iOrientation) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iOrientation == iOrientation ) return XUI_OK;
	pData->iOrientation = iOrientation;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiScrollBarGetOrientation(xui_widget pWidget)
{
	xui_scrollbar_data_t* pData = __xuiScrollBarGetData(pWidget);
	return (pData != NULL) ? pData->iOrientation : XUI_ORIENTATION_VERTICAL;
}

XUI_API int xuiScrollBarSetMode(xui_widget pWidget, int iMode)
{
	xui_scrollbar_data_t* pData = __xuiScrollBarGetData(pWidget);
	if ( (pData == NULL) || !__xuiScrollBarModeValid(iMode) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iMode == iMode ) return XUI_OK;
	pData->iMode = iMode;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiScrollBarGetMode(xui_widget pWidget)
{
	xui_scrollbar_data_t* pData = __xuiScrollBarGetData(pWidget);
	return (pData != NULL) ? pData->iMode : XUI_SCROLLBAR_MODE_FULL;
}

XUI_API int xuiScrollBarSetButtonMode(xui_widget pWidget, int iButtonMode)
{
	xui_scrollbar_data_t* pData = __xuiScrollBarGetData(pWidget);
	if ( (pData == NULL) || !__xuiScrollBarButtonModeValid(iButtonMode) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iButtonMode == iButtonMode ) return XUI_OK;
	pData->iButtonMode = iButtonMode;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiScrollBarGetButtonMode(xui_widget pWidget)
{
	xui_scrollbar_data_t* pData = __xuiScrollBarGetData(pWidget);
	return (pData != NULL) ? pData->iButtonMode : XUI_SCROLLBAR_BUTTONS_AUTO;
}

XUI_API int xuiScrollBarSetMetrics(xui_widget pWidget, float fThickness, float fMinThumbSize, float fButtonSize)
{
	xui_scrollbar_data_t* pData = __xuiScrollBarGetData(pWidget);
	if ( (pData == NULL) || (fThickness < 0.0f) || (fMinThumbSize < 0.0f) || (fButtonSize < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fThickness = (fThickness > 0.0f) ? fThickness : 10.0f;
	pData->fMinThumbSize = (fMinThumbSize > 0.0f) ? fMinThumbSize : 20.0f;
	pData->fButtonSize = fButtonSize;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiScrollBarGetMetrics(xui_widget pWidget, float* pThickness, float* pMinThumbSize, float* pButtonSize)
{
	xui_scrollbar_data_t* pData = __xuiScrollBarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pThickness != NULL ) *pThickness = pData->fThickness;
	if ( pMinThumbSize != NULL ) *pMinThumbSize = pData->fMinThumbSize;
	if ( pButtonSize != NULL ) *pButtonSize = pData->fButtonSize;
	return XUI_OK;
}

XUI_API int xuiScrollBarSetColors(xui_widget pWidget, uint32_t iTrack, uint32_t iThumb, uint32_t iHover, uint32_t iActive, uint32_t iFocus, uint32_t iDisabled)
{
	xui_scrollbar_data_t* pData = __xuiScrollBarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iTrackColor = iTrack;
	pData->iThumbColor = iThumb;
	pData->iHoverColor = iHover;
	pData->iActiveColor = iActive;
	pData->iFocusColor = iFocus;
	pData->iDisabledColor = iDisabled;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiScrollBarGetColors(xui_widget pWidget, uint32_t* pTrack, uint32_t* pThumb, uint32_t* pHover, uint32_t* pActive, uint32_t* pFocus, uint32_t* pDisabled)
{
	xui_scrollbar_data_t* pData = __xuiScrollBarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pTrack != NULL ) *pTrack = pData->iTrackColor;
	if ( pThumb != NULL ) *pThumb = pData->iThumbColor;
	if ( pHover != NULL ) *pHover = pData->iHoverColor;
	if ( pActive != NULL ) *pActive = pData->iActiveColor;
	if ( pFocus != NULL ) *pFocus = pData->iFocusColor;
	if ( pDisabled != NULL ) *pDisabled = pData->iDisabledColor;
	return XUI_OK;
}

XUI_API int xuiScrollBarSetButtonColors(xui_widget pWidget, uint32_t iButton, uint32_t iIcon)
{
	xui_scrollbar_data_t* pData = __xuiScrollBarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iButtonColor = iButton;
	pData->iButtonIconColor = iIcon;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiScrollBarGetButtonColors(xui_widget pWidget, uint32_t* pButton, uint32_t* pIcon)
{
	xui_scrollbar_data_t* pData = __xuiScrollBarGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pButton != NULL ) *pButton = pData->iButtonColor;
	if ( pIcon != NULL ) *pIcon = pData->iButtonIconColor;
	return XUI_OK;
}

static xui_rect_t __xuiScrollBarGetComputedRect(xui_widget pWidget, int iWhich)
{
	xui_scrollbar_data_t* pData;
	xui_scrollbar_data_t tResolved;
	xui_rect_t tZero;

	memset(&tZero, 0, sizeof(tZero));
	pData = __xuiScrollBarGetData(pWidget);
	if ( pData == NULL ) return tZero;
	__xuiScrollBarResolve(pWidget, pData, &tResolved);
	__xuiScrollBarUpdateRects(pWidget, pData, &tResolved);
	if ( iWhich == XUI_SCROLLBAR_PART_TRACK ) return pData->tTrackRect;
	if ( iWhich == XUI_SCROLLBAR_PART_THUMB ) return pData->tThumbRect;
	if ( iWhich == XUI_SCROLLBAR_PART_DECREASE ) return pData->tDecreaseRect;
	if ( iWhich == XUI_SCROLLBAR_PART_INCREASE ) return pData->tIncreaseRect;
	return tZero;
}

XUI_API xui_rect_t xuiScrollBarGetTrackRect(xui_widget pWidget)
{
	return __xuiScrollBarGetComputedRect(pWidget, XUI_SCROLLBAR_PART_TRACK);
}

XUI_API xui_rect_t xuiScrollBarGetThumbRect(xui_widget pWidget)
{
	return __xuiScrollBarGetComputedRect(pWidget, XUI_SCROLLBAR_PART_THUMB);
}

XUI_API xui_rect_t xuiScrollBarGetDecreaseRect(xui_widget pWidget)
{
	return __xuiScrollBarGetComputedRect(pWidget, XUI_SCROLLBAR_PART_DECREASE);
}

XUI_API xui_rect_t xuiScrollBarGetIncreaseRect(xui_widget pWidget)
{
	return __xuiScrollBarGetComputedRect(pWidget, XUI_SCROLLBAR_PART_INCREASE);
}

XUI_API int xuiScrollBarGetHoverPart(xui_widget pWidget)
{
	xui_scrollbar_data_t* pData = __xuiScrollBarGetData(pWidget);
	return (pData != NULL) ? pData->iHoverPart : XUI_SCROLLBAR_PART_NONE;
}

XUI_API int xuiScrollBarGetActivePart(xui_widget pWidget)
{
	xui_scrollbar_data_t* pData = __xuiScrollBarGetData(pWidget);
	return (pData != NULL) ? pData->iActivePart : XUI_SCROLLBAR_PART_NONE;
}

XUI_API uint32_t xuiScrollBarGetState(xui_widget pWidget)
{
	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return 0;
	}
	return __xuiScrollBarBaseState(pWidget);
}

XUI_API int xuiScrollBarGetChangeCount(xui_widget pWidget)
{
	xui_scrollbar_data_t* pData = __xuiScrollBarGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}
