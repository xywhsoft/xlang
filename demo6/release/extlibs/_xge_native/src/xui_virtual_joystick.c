#include "xui_internal.h"

#include <math.h>
#include <string.h>

typedef struct xui_virtual_joystick_channel_t {
	int bPressed;
	float fValue;
} xui_virtual_joystick_channel_t;

typedef struct xui_virtual_joystick_data_t {
	xui_virtual_joystick_change_proc onChange;
	void* pChangeUser;
	xui_virtual_joystick_state_t tState;
	xui_virtual_joystick_channel_t arrChannels[XUI_VIRTUAL_JOYSTICK_CHANNEL_COUNT];
	float fRadius;
	float fKnobSize;
	float fDeadZone;
	int bUseBuiltinAtlas;
	int bPointerActive;
	uint64_t iActivePointerId;
	int iActivePointerType;
	int iChangeCount;
	xui_surface pBaseSurface;
	xui_surface pBaseActiveSurface;
	xui_surface pKnobSurface;
	xui_surface pKnobActiveSurface;
	xui_surface pRippleSurface;
	xui_rect_t tBaseSrc;
	xui_rect_t tBaseActiveSrc;
	xui_rect_t tKnobSrc;
	xui_rect_t tKnobActiveSrc;
	xui_rect_t tRippleSrc;
	xui_rect_t tBaseRect;
	xui_rect_t tKnobRect;
	uint32_t iBaseColor;
	uint32_t iBaseActiveColor;
	uint32_t iKnobColor;
	uint32_t iKnobActiveColor;
	uint32_t iRippleColor;
	uint32_t iFocusColor;
	uint32_t iDisabledColor;
} xui_virtual_joystick_data_t;

static int __xuiVirtualJoystickDescValid(const xui_virtual_joystick_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	return (pDesc->iSize == 0) || (pDesc->iSize >= sizeof(*pDesc));
}

static float __xuiVirtualJoystickAbs(float fValue)
{
	return (fValue < 0.0f) ? -fValue : fValue;
}

static float __xuiVirtualJoystickClamp(float fValue, float fMin, float fMax)
{
	if ( fValue < fMin ) return fMin;
	if ( fValue > fMax ) return fMax;
	return fValue;
}

static uint32_t __xuiVirtualJoystickColorWithAlpha(uint32_t iColor, uint32_t iAlpha)
{
	return (iColor & 0xffffff00u) | (iAlpha & 0xffu);
}

static uint32_t __xuiVirtualJoystickAlpha(uint32_t iColor)
{
	return iColor & 0xffu;
}

static int __xuiVirtualJoystickRectValid(xui_rect_t tRect)
{
	return (tRect.fW > 0.0f) && (tRect.fH > 0.0f);
}

static xui_virtual_joystick_data_t* __xuiVirtualJoystickGetData(xui_widget pWidget)
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
	pType = xuiWidgetFindType(pContext, "virtual_joystick");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_virtual_joystick_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiVirtualJoystickStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
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

static int __xuiVirtualJoystickStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
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

static int __xuiVirtualJoystickStyleInt(xui_widget pWidget, const char* sName, int* pValue)
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

static void __xuiVirtualJoystickResolve(xui_widget pWidget, const xui_virtual_joystick_data_t* pData, xui_virtual_joystick_data_t* pResolved)
{
	int iUseAtlas;

	*pResolved = *pData;
	(void)__xuiVirtualJoystickStyleFloat(pWidget, "virtual_joystick.radius", &pResolved->fRadius);
	(void)__xuiVirtualJoystickStyleFloat(pWidget, "virtual_joystick.knob_size", &pResolved->fKnobSize);
	(void)__xuiVirtualJoystickStyleFloat(pWidget, "virtual_joystick.deadzone", &pResolved->fDeadZone);
	(void)__xuiVirtualJoystickStyleColor(pWidget, "virtual_joystick.base.color", &pResolved->iBaseColor);
	(void)__xuiVirtualJoystickStyleColor(pWidget, "virtual_joystick.base.active_color", &pResolved->iBaseActiveColor);
	(void)__xuiVirtualJoystickStyleColor(pWidget, "virtual_joystick.knob.color", &pResolved->iKnobColor);
	(void)__xuiVirtualJoystickStyleColor(pWidget, "virtual_joystick.knob.active_color", &pResolved->iKnobActiveColor);
	(void)__xuiVirtualJoystickStyleColor(pWidget, "virtual_joystick.ripple.color", &pResolved->iRippleColor);
	(void)__xuiVirtualJoystickStyleColor(pWidget, "virtual_joystick.focus.color", &pResolved->iFocusColor);
	(void)__xuiVirtualJoystickStyleColor(pWidget, "virtual_joystick.disabled.color", &pResolved->iDisabledColor);
	iUseAtlas = pResolved->bUseBuiltinAtlas;
	if ( __xuiVirtualJoystickStyleInt(pWidget, "virtual_joystick.atlas.enabled", &iUseAtlas) ) {
		pResolved->bUseBuiltinAtlas = iUseAtlas ? 1 : 0;
	}
	if ( pResolved->fRadius <= 0.0f ) pResolved->fRadius = 84.0f;
	if ( pResolved->fRadius < 24.0f ) pResolved->fRadius = 24.0f;
	if ( pResolved->fKnobSize <= 0.0f ) pResolved->fKnobSize = pResolved->fRadius * 0.64f;
	if ( pResolved->fKnobSize < 18.0f ) pResolved->fKnobSize = 18.0f;
	if ( pResolved->fKnobSize > pResolved->fRadius * 1.35f ) pResolved->fKnobSize = pResolved->fRadius * 1.35f;
	if ( pResolved->fDeadZone < 0.0f ) pResolved->fDeadZone = 0.0f;
	if ( pResolved->fDeadZone > 1.0f ) pResolved->fDeadZone = 1.0f;
}

static void __xuiVirtualJoystickNormalize(float* pX, float* pY, float* pMagnitude, float* pAngle, float fDeadZone)
{
	float fX;
	float fY;
	float fMag;

	fX = (pX != NULL) ? *pX : 0.0f;
	fY = (pY != NULL) ? *pY : 0.0f;
	fMag = sqrtf(fX * fX + fY * fY);
	if ( fMag > 1.0f ) {
		fX /= fMag;
		fY /= fMag;
		fMag = 1.0f;
	}
	if ( fMag < fDeadZone ) {
		fX = 0.0f;
		fY = 0.0f;
		fMag = 0.0f;
	}
	if ( pX != NULL ) *pX = fX;
	if ( pY != NULL ) *pY = fY;
	if ( pMagnitude != NULL ) *pMagnitude = fMag;
	if ( pAngle != NULL ) *pAngle = (fMag > 0.0f) ? atan2f(fY, fX) : 0.0f;
}

static int __xuiVirtualJoystickStateChanged(const xui_virtual_joystick_state_t* pA, const xui_virtual_joystick_state_t* pB)
{
	if ( pA->bActive != pB->bActive ) return 1;
	if ( pA->iSource != pB->iSource ) return 1;
	if ( pA->iPointerId != pB->iPointerId ) return 1;
	if ( pA->iPointerType != pB->iPointerType ) return 1;
	if ( __xuiVirtualJoystickAbs(pA->fX - pB->fX) > 0.0005f ) return 1;
	if ( __xuiVirtualJoystickAbs(pA->fY - pB->fY) > 0.0005f ) return 1;
	if ( __xuiVirtualJoystickAbs(pA->fMagnitude - pB->fMagnitude) > 0.0005f ) return 1;
	if ( __xuiVirtualJoystickAbs(pA->fAngle - pB->fAngle) > 0.0005f ) return 1;
	return 0;
}

static uint32_t __xuiVirtualJoystickVisualStateId(xui_widget pWidget, const xui_virtual_joystick_data_t* pData)
{
	uint32_t iState;

	if ( !xuiWidgetGetEnabled(pWidget) ) {
		return XUI_WIDGET_STATE_DISABLED;
	}
	iState = xuiWidgetGetInputState(pWidget);
	if ( (pData != NULL) && (pData->tState.bActive != 0) ) {
		return XUI_WIDGET_STATE_ACTIVE;
	}
	if ( (iState & XUI_WIDGET_STATE_FOCUS) != 0 ) {
		return XUI_WIDGET_STATE_FOCUS;
	}
	if ( (iState & XUI_WIDGET_STATE_HOVER) != 0 ) {
		return XUI_WIDGET_STATE_HOVER;
	}
	return 0;
}

static int __xuiVirtualJoystickSyncState(xui_widget pWidget, xui_virtual_joystick_data_t* pData)
{
	return xuiWidgetSetStateId(pWidget, __xuiVirtualJoystickVisualStateId(pWidget, pData));
}

static int __xuiVirtualJoystickSetStateInternal(
	xui_widget pWidget,
	xui_virtual_joystick_data_t* pData,
	float fX,
	float fY,
	int bForceActive,
	int iSource,
	uint64_t iPointerId,
	int iPointerType,
	int bNotify)
{
	xui_virtual_joystick_state_t tState;
	xui_virtual_joystick_data_t tResolved;
	int bChanged;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiVirtualJoystickResolve(pWidget, pData, &tResolved);
	__xuiVirtualJoystickNormalize(&fX, &fY, &tState.fMagnitude, &tState.fAngle, tResolved.fDeadZone);
	tState.iSize = sizeof(tState);
	tState.fX = fX;
	tState.fY = fY;
	tState.bActive = (bForceActive || (tState.fMagnitude > 0.0f)) ? 1 : 0;
	tState.iSource = tState.bActive ? iSource : XUI_VIRTUAL_JOYSTICK_SOURCE_NONE;
	tState.iPointerId = tState.bActive ? iPointerId : 0;
	tState.iPointerType = tState.bActive ? iPointerType : XUI_POINTER_TYPE_MOUSE;
	bChanged = __xuiVirtualJoystickStateChanged(&pData->tState, &tState);
	if ( !bChanged ) {
		(void)__xuiVirtualJoystickSyncState(pWidget, pData);
		return XUI_OK;
	}
	pData->tState = tState;
	pData->iChangeCount++;
	(void)__xuiVirtualJoystickSyncState(pWidget, pData);
	(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( bNotify && pData->onChange != NULL ) {
		pData->onChange(pWidget, &pData->tState, pData->pChangeUser);
	}
	return XUI_OK;
}

static int __xuiVirtualJoystickChannelActive(const xui_virtual_joystick_data_t* pData)
{
	int i;

	if ( pData == NULL ) return 0;
	for ( i = 0; i < XUI_VIRTUAL_JOYSTICK_CHANNEL_COUNT; i++ ) {
		if ( pData->arrChannels[i].bPressed && pData->arrChannels[i].fValue > 0.0f ) {
			return 1;
		}
	}
	return 0;
}

static int __xuiVirtualJoystickApplyChannels(xui_widget pWidget, xui_virtual_joystick_data_t* pData, int bNotify)
{
	float fLeft;
	float fRight;
	float fUp;
	float fDown;
	float fX;
	float fY;
	int bActive;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	fLeft = pData->arrChannels[XUI_VIRTUAL_JOYSTICK_CHANNEL_LEFT].bPressed ? pData->arrChannels[XUI_VIRTUAL_JOYSTICK_CHANNEL_LEFT].fValue : 0.0f;
	fRight = pData->arrChannels[XUI_VIRTUAL_JOYSTICK_CHANNEL_RIGHT].bPressed ? pData->arrChannels[XUI_VIRTUAL_JOYSTICK_CHANNEL_RIGHT].fValue : 0.0f;
	fUp = pData->arrChannels[XUI_VIRTUAL_JOYSTICK_CHANNEL_UP].bPressed ? pData->arrChannels[XUI_VIRTUAL_JOYSTICK_CHANNEL_UP].fValue : 0.0f;
	fDown = pData->arrChannels[XUI_VIRTUAL_JOYSTICK_CHANNEL_DOWN].bPressed ? pData->arrChannels[XUI_VIRTUAL_JOYSTICK_CHANNEL_DOWN].fValue : 0.0f;
	fX = fRight - fLeft;
	fY = fDown - fUp;
	bActive = __xuiVirtualJoystickChannelActive(pData);
	return __xuiVirtualJoystickSetStateInternal(
		pWidget,
		pData,
		fX,
		fY,
		bActive,
		bActive ? XUI_VIRTUAL_JOYSTICK_SOURCE_CHANNEL : XUI_VIRTUAL_JOYSTICK_SOURCE_NONE,
		0,
		XUI_POINTER_TYPE_MOUSE,
		bNotify);
}

static void __xuiVirtualJoystickComputeRects(
	xui_widget pWidget,
	xui_virtual_joystick_data_t* pData,
	const xui_virtual_joystick_data_t* pResolved)
{
	xui_rect_t tContent;
	float fBaseSize;
	float fKnobSize;
	float fTravel;
	float fCenterX;
	float fCenterY;
	float fKnobX;
	float fKnobY;

	if ( (pWidget == NULL) || (pData == NULL) || (pResolved == NULL) ) return;
	tContent = xuiWidgetGetContentRect(pWidget);
	fBaseSize = pResolved->fRadius * 2.0f;
	fKnobSize = pResolved->fKnobSize;
	fCenterX = tContent.fX + tContent.fW * 0.5f;
	fCenterY = tContent.fY + tContent.fH * 0.5f;
	fTravel = pResolved->fRadius - fKnobSize * 0.5f;
	if ( fTravel < 0.0f ) fTravel = 0.0f;
	pData->tBaseRect = xuiInternalSnapRect((xui_rect_t){
		fCenterX - fBaseSize * 0.5f,
		fCenterY - fBaseSize * 0.5f,
		fBaseSize,
		fBaseSize
	});
	fKnobX = fCenterX + pData->tState.fX * fTravel;
	fKnobY = fCenterY + pData->tState.fY * fTravel;
	pData->tKnobRect = xuiInternalSnapRect((xui_rect_t){
		fKnobX - fKnobSize * 0.5f,
		fKnobY - fKnobSize * 0.5f,
		fKnobSize,
		fKnobSize
	});
}

static int __xuiVirtualJoystickPointToVector(
	xui_widget pWidget,
	const xui_virtual_joystick_data_t* pResolved,
	const xui_event_t* pEvent,
	float* pX,
	float* pY)
{
	xui_rect_t tWorld;
	xui_rect_t tContent;
	float fLocalX;
	float fLocalY;
	float fCenterX;
	float fCenterY;
	float fTravel;

	if ( (pWidget == NULL) || (pResolved == NULL) || (pEvent == NULL) || (pX == NULL) || (pY == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	tWorld = xuiWidgetGetWorldRect(pWidget);
	tContent = xuiWidgetGetContentRect(pWidget);
	fLocalX = pEvent->fX - tWorld.fX;
	fLocalY = pEvent->fY - tWorld.fY;
	fCenterX = tContent.fX + tContent.fW * 0.5f;
	fCenterY = tContent.fY + tContent.fH * 0.5f;
	fTravel = pResolved->fRadius - pResolved->fKnobSize * 0.5f;
	if ( fTravel <= 0.0f ) {
		*pX = 0.0f;
		*pY = 0.0f;
		return XUI_OK;
	}
	*pX = (fLocalX - fCenterX) / fTravel;
	*pY = (fLocalY - fCenterY) / fTravel;
	return XUI_OK;
}

static int __xuiVirtualJoystickPointerMatches(const xui_virtual_joystick_data_t* pData, const xui_event_t* pEvent)
{
	if ( (pData == NULL) || (pEvent == NULL) ) return 0;
	return (pData->bPointerActive != 0) &&
	       (pData->iActivePointerId == pEvent->iPointerId) &&
	       (pData->iActivePointerType == pEvent->iPointerType);
}

static int __xuiVirtualJoystickPointerDown(xui_widget pWidget, xui_virtual_joystick_data_t* pData, const xui_event_t* pEvent)
{
	xui_context pContext;
	xui_virtual_joystick_data_t tResolved;
	float fX;
	float fY;
	int iRet;

	if ( (pEvent->iButton != XUI_POINTER_BUTTON_LEFT) || !xuiWidgetGetEnabled(pWidget) ) {
		return XUI_OK;
	}
	__xuiVirtualJoystickResolve(pWidget, pData, &tResolved);
	iRet = __xuiVirtualJoystickPointToVector(pWidget, &tResolved, pEvent, &fX, &fY);
	if ( iRet != XUI_OK ) return iRet;
	pContext = xuiWidgetGetContext(pWidget);
	(void)xuiSetFocusWidget(pContext, pWidget);
	(void)xuiSetPointerCapture(pContext, pWidget);
	pData->bPointerActive = 1;
	pData->iActivePointerId = pEvent->iPointerId;
	pData->iActivePointerType = pEvent->iPointerType;
	iRet = __xuiVirtualJoystickSetStateInternal(
		pWidget,
		pData,
		fX,
		fY,
		1,
		XUI_VIRTUAL_JOYSTICK_SOURCE_POINTER,
		pEvent->iPointerId,
		pEvent->iPointerType,
		1);
	if ( iRet != XUI_OK ) return iRet;
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiVirtualJoystickPointerMove(xui_widget pWidget, xui_virtual_joystick_data_t* pData, const xui_event_t* pEvent)
{
	xui_virtual_joystick_data_t tResolved;
	float fX;
	float fY;
	int iRet;

	if ( __xuiVirtualJoystickPointerMatches(pData, pEvent) &&
	     (xuiGetPointerCaptureEx(xuiWidgetGetContext(pWidget), pEvent->iPointerId, pEvent->iPointerType) == pWidget) ) {
		__xuiVirtualJoystickResolve(pWidget, pData, &tResolved);
		iRet = __xuiVirtualJoystickPointToVector(pWidget, &tResolved, pEvent, &fX, &fY);
		if ( iRet != XUI_OK ) return iRet;
		iRet = __xuiVirtualJoystickSetStateInternal(
			pWidget,
			pData,
			fX,
			fY,
			1,
			XUI_VIRTUAL_JOYSTICK_SOURCE_POINTER,
			pEvent->iPointerId,
			pEvent->iPointerType,
			1);
		if ( iRet != XUI_OK ) return iRet;
		return XUI_EVENT_DISPATCH_STOP;
	}
	return __xuiVirtualJoystickSyncState(pWidget, pData);
}

static int __xuiVirtualJoystickPointerUp(xui_widget pWidget, xui_virtual_joystick_data_t* pData, const xui_event_t* pEvent)
{
	xui_context pContext;
	int bWasActive;
	int iRet;

	if ( pEvent->iButton != XUI_POINTER_BUTTON_LEFT ) {
		return XUI_OK;
	}
	pContext = xuiWidgetGetContext(pWidget);
	bWasActive = __xuiVirtualJoystickPointerMatches(pData, pEvent) ||
		(xuiGetPointerCaptureEx(pContext, pEvent->iPointerId, pEvent->iPointerType) == pWidget);
	if ( !bWasActive ) {
		return XUI_OK;
	}
	pData->bPointerActive = 0;
	pData->iActivePointerId = 0;
	pData->iActivePointerType = XUI_POINTER_TYPE_MOUSE;
	if ( xuiGetPointerCaptureEx(pContext, pEvent->iPointerId, pEvent->iPointerType) == pWidget ) {
		(void)xuiReleasePointerCaptureEx(pContext, pEvent->iPointerId, pEvent->iPointerType, pWidget);
	}
	iRet = __xuiVirtualJoystickApplyChannels(pWidget, pData, 1);
	if ( iRet != XUI_OK ) return iRet;
	return XUI_EVENT_DISPATCH_STOP;
}

static void __xuiVirtualJoystickCancelPointer(xui_widget pWidget, xui_virtual_joystick_data_t* pData)
{
	xui_context pContext;

	if ( (pWidget == NULL) || (pData == NULL) ) return;
	pContext = xuiWidgetGetContext(pWidget);
	if ( pData->bPointerActive ) {
		if ( xuiGetPointerCaptureEx(pContext, pData->iActivePointerId, pData->iActivePointerType) == pWidget ) {
			(void)xuiReleasePointerCaptureEx(pContext, pData->iActivePointerId, pData->iActivePointerType, pWidget);
		}
	}
	pData->bPointerActive = 0;
	pData->iActivePointerId = 0;
	pData->iActivePointerType = XUI_POINTER_TYPE_MOUSE;
	(void)__xuiVirtualJoystickApplyChannels(pWidget, pData, 1);
}

static void __xuiVirtualJoystickCancelAction(xui_widget pWidget, void* pUser)
{
	xui_virtual_joystick_data_t* pData;

	(void)pUser;
	pData = __xuiVirtualJoystickGetData(pWidget);
	if ( pData != NULL ) {
		__xuiVirtualJoystickCancelPointer(pWidget, pData);
	}
}

static int __xuiVirtualJoystickEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_virtual_joystick_data_t* pData;
	int iRet;

	(void)pUser;
	if ( (pWidget == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiVirtualJoystickGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_ENTER:
	case XUI_EVENT_POINTER_MOVE:
		return __xuiVirtualJoystickPointerMove(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_LEAVE:
		if ( pData->bPointerActive == 0 ) {
			return __xuiVirtualJoystickSyncState(pWidget, pData);
		}
		break;
	case XUI_EVENT_POINTER_DOWN:
		return __xuiVirtualJoystickPointerDown(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_UP:
		return __xuiVirtualJoystickPointerUp(pWidget, pData, pEvent);
	case XUI_EVENT_POINTER_CLICK:
		if ( pEvent->iButton == XUI_POINTER_BUTTON_LEFT ) {
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		pData->bPointerActive = 0;
		pData->iActivePointerId = 0;
		pData->iActivePointerType = XUI_POINTER_TYPE_MOUSE;
		return __xuiVirtualJoystickApplyChannels(pWidget, pData, 1);
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
		return __xuiVirtualJoystickSyncState(pWidget, pData);
	case XUI_EVENT_KEY_DOWN:
		if ( pEvent->iKey == XUI_KEY_ESCAPE ) {
			__xuiVirtualJoystickCancelPointer(pWidget, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_ENABLED_CHANGED:
	case XUI_EVENT_VISIBLE_CHANGED:
		if ( !xuiWidgetGetEnabled(pWidget) || !xuiWidgetGetVisible(pWidget) ) {
			__xuiVirtualJoystickCancelPointer(pWidget, pData);
		}
		iRet = __xuiVirtualJoystickSyncState(pWidget, pData);
		if ( iRet != XUI_OK ) return iRet;
		return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiVirtualJoystickDrawCircleFallback(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor, int bStroke)
{
	float fRadius;

	if ( (pProxy == NULL) || (pDraw == NULL) || !__xuiVirtualJoystickRectValid(tRect) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( __xuiVirtualJoystickAlpha(iColor) == 0u ) {
		return XUI_OK;
	}
	fRadius = (tRect.fW < tRect.fH ? tRect.fW : tRect.fH) * 0.5f;
	if ( fRadius <= 0.0f ) {
		return XUI_OK;
	}
	if ( bStroke ) {
		if ( pProxy->drawCircleStroke != NULL ) {
			if ( fRadius <= 1.0f ) return XUI_OK;
			return pProxy->drawCircleStroke(pProxy, pDraw, tRect.fX + tRect.fW * 0.5f, tRect.fY + tRect.fH * 0.5f, fRadius - 1.0f, 2.0f, iColor);
		}
		if ( pProxy->drawRectStroke != NULL ) {
			return pProxy->drawRectStroke(pProxy, pDraw, tRect, 2.0f, iColor);
		}
	} else {
		if ( pProxy->drawCircleFill != NULL ) {
			return pProxy->drawCircleFill(pProxy, pDraw, tRect.fX + tRect.fW * 0.5f, tRect.fY + tRect.fH * 0.5f, fRadius, iColor);
		}
		if ( pProxy->drawRectFill != NULL ) {
			return pProxy->drawRectFill(pProxy, pDraw, tRect, iColor);
		}
	}
	return XUI_OK;
}

static int __xuiVirtualJoystickDrawAsset(
	xui_widget pWidget,
	xui_proxy pProxy,
	xui_draw_context pDraw,
	xui_surface pSurface,
	xui_rect_t tSrc,
	const char* sBuiltinName,
	xui_rect_t tDst,
	uint32_t iColor)
{
	xui_surface pAtlas;
	xui_rect_t tAssetSrc;
	int iRet;

	if ( (pWidget == NULL) || (pProxy == NULL) || (pDraw == NULL) || !__xuiVirtualJoystickRectValid(tDst) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( __xuiVirtualJoystickAlpha(iColor) == 0u ) {
		return XUI_OK;
	}
	if ( (pSurface != NULL) && __xuiVirtualJoystickRectValid(tSrc) ) {
		return pProxy->drawSurface(pProxy, pDraw, pSurface, tSrc, tDst, iColor, 0);
	}
	if ( sBuiltinName == NULL ) {
		return XUI_ERROR_FILE_NOT_FOUND;
	}
	iRet = xuiBuiltinAssetGetAtlas(xuiWidgetGetContext(pWidget), &pAtlas);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iRet = xuiBuiltinAssetGetRect(sBuiltinName, &tAssetSrc);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return pProxy->drawSurface(pProxy, pDraw, pAtlas, tAssetSrc, tDst, iColor, 0);
}

static int __xuiVirtualJoystickCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_virtual_joystick_data_t* pData;
	xui_virtual_joystick_data_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tContent;
	xui_rect_t tFocus;
	uint32_t iBaseColor;
	uint32_t iKnobColor;
	uint32_t iRippleColor;
	int bActive;
	int bDisabled;
	int iRet;

	(void)pUser;
	pData = __xuiVirtualJoystickGetData(pWidget);
	if ( (pData == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	__xuiVirtualJoystickResolve(pWidget, pData, &tResolved);
	__xuiVirtualJoystickComputeRects(pWidget, pData, &tResolved);
	tContent = xuiWidgetGetContentRect(pWidget);
	bActive = ((iStateId & XUI_WIDGET_STATE_ACTIVE) != 0) || (pData->tState.bActive != 0);
	bDisabled = (iStateId & XUI_WIDGET_STATE_DISABLED) != 0;
	iBaseColor = bActive ? tResolved.iBaseActiveColor : tResolved.iBaseColor;
	iKnobColor = bActive ? tResolved.iKnobActiveColor : tResolved.iKnobColor;
	iRippleColor = tResolved.iRippleColor;
	if ( bDisabled ) {
		iBaseColor = __xuiVirtualJoystickColorWithAlpha(tResolved.iDisabledColor, 130);
		iKnobColor = __xuiVirtualJoystickColorWithAlpha(tResolved.iDisabledColor, 180);
		iRippleColor = __xuiVirtualJoystickColorWithAlpha(tResolved.iDisabledColor, 70);
	}
	if ( tResolved.bUseBuiltinAtlas || tResolved.pBaseSurface != NULL || tResolved.pBaseActiveSurface != NULL ) {
		iRet = __xuiVirtualJoystickDrawAsset(
			pWidget,
			pProxy,
			pDraw,
			bActive ? tResolved.pBaseActiveSurface : tResolved.pBaseSurface,
			bActive ? tResolved.tBaseActiveSrc : tResolved.tBaseSrc,
			tResolved.bUseBuiltinAtlas ? (bActive ? "virtual_joystick_base_active" : "virtual_joystick_base") : NULL,
			pData->tBaseRect,
			iBaseColor);
		if ( iRet != XUI_OK ) {
			(void)__xuiVirtualJoystickDrawCircleFallback(pProxy, pDraw, pData->tBaseRect, __xuiVirtualJoystickColorWithAlpha(iBaseColor, 86), 0);
			(void)__xuiVirtualJoystickDrawCircleFallback(pProxy, pDraw, pData->tBaseRect, iBaseColor, 1);
		}
	} else {
		(void)__xuiVirtualJoystickDrawCircleFallback(pProxy, pDraw, pData->tBaseRect, __xuiVirtualJoystickColorWithAlpha(iBaseColor, 86), 0);
		(void)__xuiVirtualJoystickDrawCircleFallback(pProxy, pDraw, pData->tBaseRect, iBaseColor, 1);
	}
	if ( (bActive || pData->tState.fMagnitude > 0.0f) && (tResolved.bUseBuiltinAtlas || tResolved.pRippleSurface != NULL) ) {
		(void)__xuiVirtualJoystickDrawAsset(
			pWidget,
			pProxy,
			pDraw,
			tResolved.pRippleSurface,
			tResolved.tRippleSrc,
			tResolved.bUseBuiltinAtlas ? "virtual_joystick_ripple" : NULL,
			pData->tBaseRect,
			iRippleColor);
	}
	if ( tResolved.bUseBuiltinAtlas || tResolved.pKnobSurface != NULL || tResolved.pKnobActiveSurface != NULL ) {
		iRet = __xuiVirtualJoystickDrawAsset(
			pWidget,
			pProxy,
			pDraw,
			bActive ? tResolved.pKnobActiveSurface : tResolved.pKnobSurface,
			bActive ? tResolved.tKnobActiveSrc : tResolved.tKnobSrc,
			tResolved.bUseBuiltinAtlas ? (bActive ? "virtual_joystick_knob_active" : "virtual_joystick_knob") : NULL,
			pData->tKnobRect,
			iKnobColor);
		if ( iRet != XUI_OK ) {
			(void)__xuiVirtualJoystickDrawCircleFallback(pProxy, pDraw, pData->tKnobRect, iKnobColor, 0);
			(void)__xuiVirtualJoystickDrawCircleFallback(pProxy, pDraw, pData->tKnobRect, XUI_COLOR_RGBA(255, 255, 255, 170), 1);
		}
	} else {
		(void)__xuiVirtualJoystickDrawCircleFallback(pProxy, pDraw, pData->tKnobRect, iKnobColor, 0);
		(void)__xuiVirtualJoystickDrawCircleFallback(pProxy, pDraw, pData->tKnobRect, XUI_COLOR_RGBA(255, 255, 255, 170), 1);
	}
	if ( ((iStateId & XUI_WIDGET_STATE_FOCUS) != 0) && !bDisabled && (pProxy->drawRectStroke != NULL) &&
	     (__xuiVirtualJoystickAlpha(tResolved.iFocusColor) != 0u) ) {
		tFocus = xuiInternalInsetRect(tContent, 1.0f);
		iRet = pProxy->drawRectStroke(pProxy, pDraw, tFocus, 1.5f, tResolved.iFocusColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiVirtualJoystickContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_virtual_joystick_data_t* pData;
	xui_virtual_joystick_data_t tResolved;
	float fSize;

	(void)tConstraint;
	(void)pUser;
	if ( pSize == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiVirtualJoystickGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiVirtualJoystickResolve(pWidget, pData, &tResolved);
	fSize = tResolved.fRadius * 2.0f + 12.0f;
	*pSize = (xui_vec2_t){fSize, fSize};
	return XUI_OK;
}

static void __xuiVirtualJoystickDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_MANUAL;
	pLayout->iWidthMode = XUI_SIZE_CONTENT;
	pLayout->iHeightMode = XUI_SIZE_CONTENT;
	pLayout->iFlowMode = XUI_FLOW_BLOCK;
	pLayout->iDock = 0;
	pLayout->iOverflow = XUI_OVERFLOW_VISIBLE;
	pLayout->iAlignX = XUI_ALIGN_START;
	pLayout->iAlignY = XUI_ALIGN_START;
	pLayout->iTableRowSpan = 1;
	pLayout->iTableColumnSpan = 1;
	pLayout->iGridColumnCount = 1;
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fShrink = 0.0f;
}

static void __xuiVirtualJoystickDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE | XUI_CACHE_UPDATE_ALL_STATES;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiVirtualJoystickInitCacheStates(xui_widget pWidget)
{
	static const uint32_t arrStates[] = {
		0,
		XUI_WIDGET_STATE_HOVER,
		XUI_WIDGET_STATE_ACTIVE,
		XUI_WIDGET_STATE_FOCUS,
		XUI_WIDGET_STATE_DISABLED
	};
	int i;
	int iRet;

	iRet = xuiWidgetSetCacheStateCount(pWidget, (int)(sizeof(arrStates) / sizeof(arrStates[0])));
	if ( iRet != XUI_OK ) return iRet;
	for ( i = 0; i < (int)(sizeof(arrStates) / sizeof(arrStates[0])); i++ ) {
		iRet = xuiWidgetSetCacheStateId(pWidget, i, 0x87000000u + (uint32_t)i);
		if ( iRet != XUI_OK ) return iRet;
	}
	for ( i = 0; i < (int)(sizeof(arrStates) / sizeof(arrStates[0])); i++ ) {
		iRet = xuiWidgetSetCacheStateId(pWidget, i, arrStates[i]);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiVirtualJoystickInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetCancelAction(pWidget, __xuiVirtualJoystickCancelAction, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_ENTER, __xuiVirtualJoystickEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiVirtualJoystickEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiVirtualJoystickEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiVirtualJoystickEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiVirtualJoystickEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CLICK, __xuiVirtualJoystickEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiVirtualJoystickEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiVirtualJoystickEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiVirtualJoystickEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiVirtualJoystickEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_ENABLED_CHANGED, __xuiVirtualJoystickEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_VISIBLE_CHANGED, __xuiVirtualJoystickEvent, NULL);
	return iRet;
}

static int __xuiVirtualJoystickInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_virtual_joystick_data_t* pData;
	const xui_virtual_joystick_desc_t* pDesc;
	int iRet;

	(void)pUser;
	pData = (xui_virtual_joystick_data_t*)pTypeData;
	pDesc = (const xui_virtual_joystick_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiVirtualJoystickDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(pData, 0, sizeof(*pData));
	pData->onChange = (pDesc != NULL) ? pDesc->onChange : NULL;
	pData->pChangeUser = (pDesc != NULL) ? pDesc->pChangeUser : NULL;
	pData->fRadius = (pDesc != NULL && pDesc->fRadius > 0.0f) ? pDesc->fRadius : 84.0f;
	pData->fKnobSize = (pDesc != NULL && pDesc->fKnobSize > 0.0f) ? pDesc->fKnobSize : 54.0f;
	pData->fDeadZone = (pDesc != NULL && pDesc->fDeadZone >= 0.0f) ? pDesc->fDeadZone : 0.08f;
	pData->bUseBuiltinAtlas = (pDesc == NULL || pDesc->iSize == 0 || pDesc->bUseBuiltinAtlas != 0) ? 1 : 0;
	pData->pBaseSurface = (pDesc != NULL) ? pDesc->pBaseSurface : NULL;
	pData->pBaseActiveSurface = (pDesc != NULL) ? pDesc->pBaseActiveSurface : NULL;
	pData->pKnobSurface = (pDesc != NULL) ? pDesc->pKnobSurface : NULL;
	pData->pKnobActiveSurface = (pDesc != NULL) ? pDesc->pKnobActiveSurface : NULL;
	pData->pRippleSurface = (pDesc != NULL) ? pDesc->pRippleSurface : NULL;
	pData->tBaseSrc = (pDesc != NULL) ? pDesc->tBaseSrc : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	pData->tBaseActiveSrc = (pDesc != NULL) ? pDesc->tBaseActiveSrc : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	pData->tKnobSrc = (pDesc != NULL) ? pDesc->tKnobSrc : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	pData->tKnobActiveSrc = (pDesc != NULL) ? pDesc->tKnobActiveSrc : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	pData->tRippleSrc = (pDesc != NULL) ? pDesc->tRippleSrc : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	pData->iBaseColor = (pDesc != NULL && pDesc->iBaseColor != 0) ? pDesc->iBaseColor : XUI_COLOR_RGBA(255, 255, 255, 220);
	pData->iBaseActiveColor = (pDesc != NULL && pDesc->iBaseActiveColor != 0) ? pDesc->iBaseActiveColor : XUI_COLOR_RGBA(255, 255, 255, 255);
	pData->iKnobColor = (pDesc != NULL && pDesc->iKnobColor != 0) ? pDesc->iKnobColor : XUI_COLOR_RGBA(255, 255, 255, 245);
	pData->iKnobActiveColor = (pDesc != NULL && pDesc->iKnobActiveColor != 0) ? pDesc->iKnobActiveColor : XUI_COLOR_RGBA(255, 255, 255, 255);
	pData->iRippleColor = (pDesc != NULL && pDesc->iRippleColor != 0) ? pDesc->iRippleColor : XUI_COLOR_RGBA(90, 170, 255, 125);
	pData->iFocusColor = (pDesc != NULL && pDesc->iFocusColor != 0) ? pDesc->iFocusColor : XUI_COLOR_RGBA(64, 149, 255, 150);
	pData->iDisabledColor = (pDesc != NULL && pDesc->iDisabledColor != 0) ? pDesc->iDisabledColor : XUI_COLOR_RGBA(150, 164, 184, 120);
	pData->tState.iSize = sizeof(pData->tState);
	pData->tState.iPointerType = XUI_POINTER_TYPE_MOUSE;
	pData->iActivePointerType = XUI_POINTER_TYPE_MOUSE;
	(void)xuiWidgetSetPadding(pWidget, (xui_thickness_t){0.0f, 0.0f, 0.0f, 0.0f});
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_VISIBLE);
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	(void)xuiWidgetSetHitTestVisible(pWidget, 1);
	iRet = __xuiVirtualJoystickInitCacheStates(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiVirtualJoystickInitEvents(pWidget);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiVirtualJoystickSyncState(pWidget, pData);
}

static void __xuiVirtualJoystickDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_virtual_joystick_data_t* pData;

	(void)pUser;
	pData = (xui_virtual_joystick_data_t*)pTypeData;
	if ( pData != NULL ) {
		__xuiVirtualJoystickCancelPointer(pWidget, pData);
		memset(pData, 0, sizeof(*pData));
	}
}

static void __xuiVirtualJoystickRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
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

static void __xuiVirtualJoystickRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;
	uint32_t iLayoutDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	iLayoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiVirtualJoystickRegisterStyleProperty(pContext, pType, "virtual_joystick.radius", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiVirtualJoystickRegisterStyleProperty(pContext, pType, "virtual_joystick.knob_size", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiVirtualJoystickRegisterStyleProperty(pContext, pType, "virtual_joystick.deadzone", XUI_STYLE_VALUE_FLOAT, iPaintDirty, 0);
	__xuiVirtualJoystickRegisterStyleProperty(pContext, pType, "virtual_joystick.atlas.enabled", XUI_STYLE_VALUE_BOOL, iPaintDirty, 0);
	__xuiVirtualJoystickRegisterStyleProperty(pContext, pType, "virtual_joystick.base.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiVirtualJoystickRegisterStyleProperty(pContext, pType, "virtual_joystick.base.active_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiVirtualJoystickRegisterStyleProperty(pContext, pType, "virtual_joystick.knob.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiVirtualJoystickRegisterStyleProperty(pContext, pType, "virtual_joystick.knob.active_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiVirtualJoystickRegisterStyleProperty(pContext, pType, "virtual_joystick.ripple.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiVirtualJoystickRegisterStyleProperty(pContext, pType, "virtual_joystick.focus.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiVirtualJoystickRegisterStyleProperty(pContext, pType, "virtual_joystick.disabled.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
}

XUI_API xui_widget_type xuiVirtualJoystickGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "virtual_joystick");
	if ( pType != NULL ) {
		__xuiVirtualJoystickRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "virtual_joystick";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_virtual_joystick_data_t);
	tDesc.onInit = __xuiVirtualJoystickInit;
	tDesc.onDestroy = __xuiVirtualJoystickDestroy;
	tDesc.onContentMeasure = __xuiVirtualJoystickContentMeasure;
	tDesc.onCacheRender = __xuiVirtualJoystickCacheRender;
	__xuiVirtualJoystickDefaultLayout(&tDesc.tLayout);
	__xuiVirtualJoystickDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	__xuiVirtualJoystickRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiVirtualJoystickCreate(xui_context pContext, xui_widget* ppWidget, const xui_virtual_joystick_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiVirtualJoystickDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiVirtualJoystickGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiVirtualJoystickSetChange(xui_widget pWidget, xui_virtual_joystick_change_proc onChange, void* pUser)
{
	xui_virtual_joystick_data_t* pData;

	pData = __xuiVirtualJoystickGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onChange = onChange;
	pData->pChangeUser = pUser;
	return XUI_OK;
}

XUI_API int xuiVirtualJoystickSetValue(xui_widget pWidget, float fX, float fY, int bNotify)
{
	xui_virtual_joystick_data_t* pData;

	pData = __xuiVirtualJoystickGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiVirtualJoystickCancelPointer(pWidget, pData);
	return __xuiVirtualJoystickSetStateInternal(
		pWidget,
		pData,
		fX,
		fY,
		0,
		XUI_VIRTUAL_JOYSTICK_SOURCE_PROGRAM,
		0,
		XUI_POINTER_TYPE_MOUSE,
		bNotify);
}

XUI_API int xuiVirtualJoystickReset(xui_widget pWidget, int bNotify)
{
	xui_virtual_joystick_data_t* pData;
	int i;

	pData = __xuiVirtualJoystickGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	__xuiVirtualJoystickCancelPointer(pWidget, pData);
	for ( i = 0; i < XUI_VIRTUAL_JOYSTICK_CHANNEL_COUNT; i++ ) {
		pData->arrChannels[i].bPressed = 0;
		pData->arrChannels[i].fValue = 0.0f;
	}
	return __xuiVirtualJoystickSetStateInternal(
		pWidget,
		pData,
		0.0f,
		0.0f,
		0,
		XUI_VIRTUAL_JOYSTICK_SOURCE_NONE,
		0,
		XUI_POINTER_TYPE_MOUSE,
		bNotify);
}

XUI_API int xuiVirtualJoystickGetState(xui_widget pWidget, xui_virtual_joystick_state_t* pState)
{
	xui_virtual_joystick_data_t* pData;

	if ( pState == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiVirtualJoystickGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*pState = pData->tState;
	if ( pState->iSize == 0 ) pState->iSize = sizeof(*pState);
	return XUI_OK;
}

XUI_API float xuiVirtualJoystickGetX(xui_widget pWidget)
{
	xui_virtual_joystick_data_t* pData;

	pData = __xuiVirtualJoystickGetData(pWidget);
	return (pData != NULL) ? pData->tState.fX : 0.0f;
}

XUI_API float xuiVirtualJoystickGetY(xui_widget pWidget)
{
	xui_virtual_joystick_data_t* pData;

	pData = __xuiVirtualJoystickGetData(pWidget);
	return (pData != NULL) ? pData->tState.fY : 0.0f;
}

XUI_API float xuiVirtualJoystickGetMagnitude(xui_widget pWidget)
{
	xui_virtual_joystick_data_t* pData;

	pData = __xuiVirtualJoystickGetData(pWidget);
	return (pData != NULL) ? pData->tState.fMagnitude : 0.0f;
}

XUI_API float xuiVirtualJoystickGetAngle(xui_widget pWidget)
{
	xui_virtual_joystick_data_t* pData;

	pData = __xuiVirtualJoystickGetData(pWidget);
	return (pData != NULL) ? pData->tState.fAngle : 0.0f;
}

XUI_API int xuiVirtualJoystickSetChannel(xui_widget pWidget, int iChannel, int bPressed, float fValue, int bNotify)
{
	xui_virtual_joystick_data_t* pData;

	pData = __xuiVirtualJoystickGetData(pWidget);
	if ( (pData == NULL) || (iChannel < 0) || (iChannel >= XUI_VIRTUAL_JOYSTICK_CHANNEL_COUNT) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	fValue = __xuiVirtualJoystickClamp(fValue, 0.0f, 1.0f);
	pData->arrChannels[iChannel].bPressed = bPressed ? 1 : 0;
	pData->arrChannels[iChannel].fValue = pData->arrChannels[iChannel].bPressed ? fValue : 0.0f;
	if ( pData->bPointerActive ) {
		return XUI_OK;
	}
	return __xuiVirtualJoystickApplyChannels(pWidget, pData, bNotify);
}

XUI_API int xuiVirtualJoystickGetChannel(xui_widget pWidget, int iChannel, int* pPressed, float* pValue)
{
	xui_virtual_joystick_data_t* pData;

	pData = __xuiVirtualJoystickGetData(pWidget);
	if ( (pData == NULL) || (iChannel < 0) || (iChannel >= XUI_VIRTUAL_JOYSTICK_CHANNEL_COUNT) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pPressed != NULL ) *pPressed = pData->arrChannels[iChannel].bPressed;
	if ( pValue != NULL ) *pValue = pData->arrChannels[iChannel].fValue;
	return XUI_OK;
}

XUI_API int xuiVirtualJoystickClearChannels(xui_widget pWidget, int bNotify)
{
	xui_virtual_joystick_data_t* pData;
	int i;

	pData = __xuiVirtualJoystickGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < XUI_VIRTUAL_JOYSTICK_CHANNEL_COUNT; i++ ) {
		pData->arrChannels[i].bPressed = 0;
		pData->arrChannels[i].fValue = 0.0f;
	}
	if ( pData->bPointerActive ) {
		return XUI_OK;
	}
	return __xuiVirtualJoystickApplyChannels(pWidget, pData, bNotify);
}

XUI_API int xuiVirtualJoystickSetMetrics(xui_widget pWidget, float fRadius, float fKnobSize, float fDeadZone)
{
	xui_virtual_joystick_data_t* pData;

	pData = __xuiVirtualJoystickGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( fRadius <= 0.0f || fKnobSize <= 0.0f || fDeadZone < 0.0f || fDeadZone > 1.0f ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->fRadius = fRadius;
	pData->fKnobSize = fKnobSize;
	pData->fDeadZone = fDeadZone;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiVirtualJoystickGetMetrics(xui_widget pWidget, float* pRadius, float* pKnobSize, float* pDeadZone)
{
	xui_virtual_joystick_data_t* pData;

	pData = __xuiVirtualJoystickGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pRadius != NULL ) *pRadius = pData->fRadius;
	if ( pKnobSize != NULL ) *pKnobSize = pData->fKnobSize;
	if ( pDeadZone != NULL ) *pDeadZone = pData->fDeadZone;
	return XUI_OK;
}

XUI_API int xuiVirtualJoystickUseBuiltinAtlas(xui_widget pWidget, int bEnable)
{
	xui_virtual_joystick_data_t* pData;

	pData = __xuiVirtualJoystickGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bUseBuiltinAtlas = bEnable ? 1 : 0;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiVirtualJoystickGetUseBuiltinAtlas(xui_widget pWidget)
{
	xui_virtual_joystick_data_t* pData;

	pData = __xuiVirtualJoystickGetData(pWidget);
	return (pData != NULL) ? pData->bUseBuiltinAtlas : 0;
}

XUI_API int xuiVirtualJoystickSetSurface(xui_widget pWidget, int iPart, xui_surface pSurface, xui_rect_t tSrc)
{
	xui_virtual_joystick_data_t* pData;

	pData = __xuiVirtualJoystickGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	switch ( iPart ) {
	case XUI_VIRTUAL_JOYSTICK_PART_BASE:
		pData->pBaseSurface = pSurface;
		pData->tBaseSrc = tSrc;
		break;
	case XUI_VIRTUAL_JOYSTICK_PART_BASE_ACTIVE:
		pData->pBaseActiveSurface = pSurface;
		pData->tBaseActiveSrc = tSrc;
		break;
	case XUI_VIRTUAL_JOYSTICK_PART_KNOB:
		pData->pKnobSurface = pSurface;
		pData->tKnobSrc = tSrc;
		break;
	case XUI_VIRTUAL_JOYSTICK_PART_KNOB_ACTIVE:
		pData->pKnobActiveSurface = pSurface;
		pData->tKnobActiveSrc = tSrc;
		break;
	case XUI_VIRTUAL_JOYSTICK_PART_RIPPLE:
		pData->pRippleSurface = pSurface;
		pData->tRippleSrc = tSrc;
		break;
	default:
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiVirtualJoystickGetSurface(xui_widget pWidget, int iPart, xui_surface* ppSurface, xui_rect_t* pSrc)
{
	xui_virtual_joystick_data_t* pData;
	xui_surface pSurface;
	xui_rect_t tSrc;

	pData = __xuiVirtualJoystickGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pSurface = NULL;
	memset(&tSrc, 0, sizeof(tSrc));
	switch ( iPart ) {
	case XUI_VIRTUAL_JOYSTICK_PART_BASE:
		pSurface = pData->pBaseSurface;
		tSrc = pData->tBaseSrc;
		break;
	case XUI_VIRTUAL_JOYSTICK_PART_BASE_ACTIVE:
		pSurface = pData->pBaseActiveSurface;
		tSrc = pData->tBaseActiveSrc;
		break;
	case XUI_VIRTUAL_JOYSTICK_PART_KNOB:
		pSurface = pData->pKnobSurface;
		tSrc = pData->tKnobSrc;
		break;
	case XUI_VIRTUAL_JOYSTICK_PART_KNOB_ACTIVE:
		pSurface = pData->pKnobActiveSurface;
		tSrc = pData->tKnobActiveSrc;
		break;
	case XUI_VIRTUAL_JOYSTICK_PART_RIPPLE:
		pSurface = pData->pRippleSurface;
		tSrc = pData->tRippleSrc;
		break;
	default:
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( ppSurface != NULL ) *ppSurface = pSurface;
	if ( pSrc != NULL ) *pSrc = tSrc;
	return XUI_OK;
}

XUI_API int xuiVirtualJoystickSetColors(xui_widget pWidget, uint32_t iBase, uint32_t iBaseActive, uint32_t iKnob, uint32_t iKnobActive, uint32_t iRipple, uint32_t iFocus, uint32_t iDisabled)
{
	xui_virtual_joystick_data_t* pData;

	pData = __xuiVirtualJoystickGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iBaseColor = iBase;
	pData->iBaseActiveColor = iBaseActive;
	pData->iKnobColor = iKnob;
	pData->iKnobActiveColor = iKnobActive;
	pData->iRippleColor = iRipple;
	pData->iFocusColor = iFocus;
	pData->iDisabledColor = iDisabled;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiVirtualJoystickGetColors(xui_widget pWidget, uint32_t* pBase, uint32_t* pBaseActive, uint32_t* pKnob, uint32_t* pKnobActive, uint32_t* pRipple, uint32_t* pFocus, uint32_t* pDisabled)
{
	xui_virtual_joystick_data_t* pData;

	pData = __xuiVirtualJoystickGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pBase != NULL ) *pBase = pData->iBaseColor;
	if ( pBaseActive != NULL ) *pBaseActive = pData->iBaseActiveColor;
	if ( pKnob != NULL ) *pKnob = pData->iKnobColor;
	if ( pKnobActive != NULL ) *pKnobActive = pData->iKnobActiveColor;
	if ( pRipple != NULL ) *pRipple = pData->iRippleColor;
	if ( pFocus != NULL ) *pFocus = pData->iFocusColor;
	if ( pDisabled != NULL ) *pDisabled = pData->iDisabledColor;
	return XUI_OK;
}

XUI_API int xuiVirtualJoystickGetChangeCount(xui_widget pWidget)
{
	xui_virtual_joystick_data_t* pData;

	pData = __xuiVirtualJoystickGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}

XUI_API xui_rect_t xuiVirtualJoystickGetBaseRect(xui_widget pWidget)
{
	xui_virtual_joystick_data_t* pData;

	pData = __xuiVirtualJoystickGetData(pWidget);
	return (pData != NULL) ? pData->tBaseRect : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API xui_rect_t xuiVirtualJoystickGetKnobRect(xui_widget pWidget)
{
	xui_virtual_joystick_data_t* pData;

	pData = __xuiVirtualJoystickGetData(pWidget);
	return (pData != NULL) ? pData->tKnobRect : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}
