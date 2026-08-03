#include "xui_internal.h"

#include <string.h>

#define XUI_INPUT_DOUBLE_CLICK_SECONDS 0.45
#define XUI_INPUT_DOUBLE_CLICK_DISTANCE 4.0f
#define XUI_INPUT_DRAG_DISTANCE 4.0f
#define XUI_INPUT_CONTEXT_PRESS_SECONDS 0.55f
#define XUI_INPUT_CONTEXT_PRESS_DISTANCE 6.0f

typedef struct xui_input_focus_item_t {
	xui_widget pWidget;
	int iOrder;
	int iTabIndex;
} xui_input_focus_item_t;

static int __xuiInputDispatchEventWithFlags(xui_context pContext, const xui_event_t* pEvent, int* pFlags);

static int __xuiInputFloatValid(float fValue)
{
	return (fValue == fValue) && (fValue >= -XUI_CONTEXT_MAX_VIEWPORT) && (fValue <= XUI_CONTEXT_MAX_VIEWPORT);
}

static char* __xuiInputStringDuplicate(const char* sText)
{
	char* sCopy;
	size_t iSize;

	if ( sText == NULL ) {
		return NULL;
	}
	iSize = strlen(sText) + 1u;
	sCopy = (char*)xrtMalloc(iSize);
	if ( sCopy == NULL ) {
		return NULL;
	}
	memcpy(sCopy, sText, iSize);
	return sCopy;
}

int xuiInternalClipboardReadProxy(xui_proxy pProxy, char** psText, int* pTextSize)
{
	char sSmall[512];
	char* sText;
	char* sNew;
	int iLength;
	int iRead;
	int iAttempt;

	if ( pProxy == NULL || psText == NULL || pTextSize == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	*psText = NULL;
	*pTextSize = 0;
	if ( pProxy->clipboardGetText == NULL ) return XUI_ERROR_UNSUPPORTED;
	memset(sSmall, 0, sizeof(sSmall));
	iLength = pProxy->clipboardGetText(pProxy, sSmall, (int)sizeof(sSmall));
	if ( iLength < 0 ) return iLength;
	sText = (char*)xrtMalloc((size_t)iLength + 1u);
	if ( sText == NULL ) return XUI_ERROR_OUT_OF_MEMORY;
	if ( iLength < (int)sizeof(sSmall) ) {
		memcpy(sText, sSmall, (size_t)iLength + 1u);
		*psText = sText;
		*pTextSize = iLength;
		return XUI_OK;
	}
	for ( iAttempt = 0; iAttempt < 4; iAttempt++ ) {
		iRead = pProxy->clipboardGetText(pProxy, sText, iLength + 1);
		if ( iRead < 0 ) {
			xrtFree(sText);
			return iRead;
		}
		if ( iRead <= iLength ) {
			sText[iRead] = '\0';
			*psText = sText;
			*pTextSize = iRead;
			return XUI_OK;
		}
		iLength = iRead;
		sNew = (char*)xrtRealloc(sText, (size_t)iLength + 1u);
		if ( sNew == NULL ) {
			xrtFree(sText);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		sText = sNew;
	}
	xrtFree(sText);
	return XUI_ERROR;
}

int xuiInternalClipboardReadText(xui_context pContext, char** psText, int* pTextSize)
{
	if ( !xuiInternalContextIsValid(pContext) ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiInternalClipboardReadProxy(xuiInternalContextGetProxy(pContext), psText, pTextSize);
}

static int __xuiInputPositiveFloatValid(float fValue)
{
	return (fValue == fValue) && (fValue >= 0.0f) && (fValue <= XUI_CONTEXT_MAX_VIEWPORT);
}

static int __xuiInputDpiValid(float fValue)
{
	return (fValue == fValue) && (fValue > 0.0f) && (fValue <= 64.0f);
}

static int __xuiInputButtonValid(int iButton)
{
	return (iButton == XUI_POINTER_BUTTON_LEFT) ||
	       (iButton == XUI_POINTER_BUTTON_RIGHT) ||
	       (iButton == XUI_POINTER_BUTTON_MIDDLE);
}

static int __xuiInputCodepointValid(uint32_t iCodepoint)
{
	return (iCodepoint > 0u) &&
	       (iCodepoint <= 0x10FFFFu) &&
	       !((iCodepoint >= 0xD800u) && (iCodepoint <= 0xDFFFu));
}

static uint32_t __xuiInputNormalizeModifiers(uint32_t iModifiers)
{
	return iModifiers & (XUI_MOD_SHIFT | XUI_MOD_CTRL | XUI_MOD_ALT | XUI_MOD_SUPER);
}

static uint64_t __xuiInputEventTypeMask(int iType)
{
	switch ( iType ) {
	case XUI_EVENT_POINTER_ENTER: return XUI_EVENT_MASK_POINTER_ENTER;
	case XUI_EVENT_POINTER_LEAVE: return XUI_EVENT_MASK_POINTER_LEAVE;
	case XUI_EVENT_POINTER_MOVE: return XUI_EVENT_MASK_POINTER_MOVE;
	case XUI_EVENT_POINTER_DOWN: return XUI_EVENT_MASK_POINTER_DOWN;
	case XUI_EVENT_POINTER_UP: return XUI_EVENT_MASK_POINTER_UP;
	case XUI_EVENT_POINTER_CLICK: return XUI_EVENT_MASK_POINTER_CLICK;
	case XUI_EVENT_POINTER_WHEEL: return XUI_EVENT_MASK_POINTER_WHEEL;
	case XUI_EVENT_POINTER_DOUBLE_CLICK: return XUI_EVENT_MASK_DOUBLE_CLICK;
	case XUI_EVENT_CONTEXT_MENU: return XUI_EVENT_MASK_CONTEXT_MENU;
	case XUI_EVENT_KEY_DOWN: return XUI_EVENT_MASK_KEY_DOWN;
	case XUI_EVENT_KEY_UP: return XUI_EVENT_MASK_KEY_UP;
	case XUI_EVENT_TEXT: return XUI_EVENT_MASK_TEXT;
	case XUI_EVENT_HOTKEY: return XUI_EVENT_MASK_HOTKEY;
	case XUI_EVENT_COMMAND: return XUI_EVENT_MASK_COMMAND;
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
		return XUI_EVENT_MASK_FOCUS;
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		return XUI_EVENT_MASK_CAPTURE;
	case XUI_EVENT_DRAG_BEGIN:
	case XUI_EVENT_DRAG_MOVE:
	case XUI_EVENT_DRAG_END:
	case XUI_EVENT_DRAG_CANCEL:
		return XUI_EVENT_MASK_DRAG;
	case XUI_EVENT_IME_COMPOSITION:
		return XUI_EVENT_MASK_IME;
	case XUI_EVENT_BOUNDS_CHANGED:
	case XUI_EVENT_VISIBLE_CHANGED:
	case XUI_EVENT_ENABLED_CHANGED:
		return XUI_EVENT_MASK_STATE;
	default:
		return 0ull;
	}
}

static int __xuiInputTextCopy(char* sDst, int iCapacity, const char* sSrc, int iSize)
{
	int iCopy;

	if ( (sDst == NULL) || (iCapacity <= 0) ) {
		return 0;
	}
	sDst[0] = '\0';
	if ( sSrc == NULL ) {
		return 0;
	}
	if ( iSize < 0 ) {
		iSize = (int)strlen(sSrc);
	}
	iCopy = iSize;
	if ( iCopy > (iCapacity - 1) ) {
		iCopy = iCapacity - 1;
	}
	if ( iCopy > 0 ) {
		memcpy(sDst, sSrc, (size_t)iCopy);
	}
	sDst[iCopy] = '\0';
	return iCopy;
}

static void __xuiInputFixEventTextPointers(xui_event_t* pEvents, int iCount)
{
	int i;

	if ( pEvents == NULL ) {
		return;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( (pEvents[i].iTextSize > 0) && (pEvents[i].sText[0] != '\0') && (pEvents[i].sCommand != NULL) ) {
			pEvents[i].sCommand = pEvents[i].sText;
		}
	}
}

static void __xuiInputCompactEvents(xui_context pContext)
{
	int iUnread;

	if ( pContext->iEventRead <= 0 ) {
		return;
	}
	iUnread = pContext->iEventCount - pContext->iEventRead;
	if ( iUnread > 0 ) {
		memmove(pContext->pEvents, pContext->pEvents + pContext->iEventRead, sizeof(pContext->pEvents[0]) * (size_t)iUnread);
		__xuiInputFixEventTextPointers(pContext->pEvents, iUnread);
	}
	pContext->iEventCount = iUnread;
	pContext->iEventRead = 0;
}

static int __xuiInputReserveEvents(xui_context pContext, int iCapacity)
{
	xui_event_t* pEvents;

	if ( iCapacity <= pContext->iEventCapacity ) {
		return XUI_OK;
	}
	if ( iCapacity < (pContext->iEventCapacity * 2) ) {
		iCapacity = pContext->iEventCapacity * 2;
	}
	if ( pContext->pEvents == pContext->arrInlineEvents ) {
		pEvents = (xui_event_t*)xrtMalloc(sizeof(*pEvents) * (size_t)iCapacity);
		if ( pEvents == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		memcpy(pEvents, pContext->pEvents, sizeof(*pEvents) * (size_t)pContext->iEventCount);
		__xuiInputFixEventTextPointers(pEvents, pContext->iEventCount);
	} else {
		pEvents = (xui_event_t*)xrtRealloc(pContext->pEvents, sizeof(*pEvents) * (size_t)iCapacity);
		if ( pEvents == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		__xuiInputFixEventTextPointers(pEvents, pContext->iEventCount);
	}
	pContext->pEvents = pEvents;
	pContext->iEventCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiInputPointerTypeNormalize(int iPointerType)
{
	if ( (iPointerType == XUI_POINTER_TYPE_TOUCH) ||
	     (iPointerType == XUI_POINTER_TYPE_PEN) ) {
		return iPointerType;
	}
	return XUI_POINTER_TYPE_MOUSE;
}

static uint64_t __xuiInputPointerIdNormalize(uint64_t iPointerId, int iPointerType)
{
	iPointerType = __xuiInputPointerTypeNormalize(iPointerType);
	return (iPointerType == XUI_POINTER_TYPE_MOUSE) ? XUI_POINTER_ID_MOUSE : iPointerId;
}

static void __xuiInputPointerStateInit(xui_pointer_state_t* pState, uint64_t iPointerId, int iPointerType)
{
	if ( pState == NULL ) {
		return;
	}
	memset(pState, 0, sizeof(*pState));
	pState->bAllocated = 1;
	pState->iPointerType = __xuiInputPointerTypeNormalize(iPointerType);
	pState->iPointerId = __xuiInputPointerIdNormalize(iPointerId, pState->iPointerType);
}

static int __xuiInputPointerStateReusable(const xui_pointer_state_t* pState)
{
	return (pState != NULL) &&
	       (pState->iPointerType != XUI_POINTER_TYPE_MOUSE) &&
	       (pState->bDown == 0) &&
	       (pState->pPointerCaptureWidget == NULL) &&
	       (pState->pActiveWidget == NULL) &&
	       (pState->pDragWidget == NULL) &&
	       (pState->bContextPressActive == 0);
}

static xui_pointer_state_t* __xuiInputPointerStateFind(xui_context pContext, uint64_t iPointerId, int iPointerType, int bCreate)
{
	xui_pointer_state_t* pReusable;
	xui_pointer_state_t* pState;
	int bWasAllocated;
	int i;

	if ( pContext == NULL ) {
		return NULL;
	}
	iPointerType = __xuiInputPointerTypeNormalize(iPointerType);
	iPointerId = __xuiInputPointerIdNormalize(iPointerId, iPointerType);
	pReusable = NULL;
	for ( i = 0; i < XUI_POINTER_MAX; i++ ) {
		pState = &pContext->arrPointerStates[i];
		if ( pState->bAllocated ) {
			if ( (pState->iPointerId == iPointerId) && (pState->iPointerType == iPointerType) ) {
				return pState;
			}
			if ( (pReusable == NULL) && __xuiInputPointerStateReusable(pState) ) {
				pReusable = pState;
			}
		} else if ( pReusable == NULL ) {
			pReusable = pState;
		}
	}
	if ( !bCreate || (pReusable == NULL) ) {
		return NULL;
	}
	bWasAllocated = pReusable->bAllocated;
	__xuiInputPointerStateInit(pReusable, iPointerId, iPointerType);
	if ( !bWasAllocated && (pContext->iPointerStateCount < XUI_POINTER_MAX) ) {
		pContext->iPointerStateCount++;
	}
	return pReusable;
}

static void __xuiInputPointerStateLoad(xui_context pContext, const xui_pointer_state_t* pState)
{
	if ( (pContext == NULL) || (pState == NULL) ) {
		return;
	}
	pContext->iInputPointerId = pState->iPointerId;
	pContext->iInputPointerType = pState->iPointerType;
	pContext->pHoverWidget = pState->pHoverWidget;
	pContext->pActiveWidget = pState->pActiveWidget;
	pContext->pPointerCaptureWidget = pState->pPointerCaptureWidget;
	pContext->pDragWidget = pState->pDragWidget;
	pContext->pLastClickWidget = pState->pLastClickWidget;
	pContext->pContextPressWidget = pState->pContextPressWidget;
	pContext->fPointerX = pState->fPointerX;
	pContext->fPointerY = pState->fPointerY;
	pContext->fContextPressTime = pState->fContextPressTime;
	pContext->fContextPressStartX = pState->fContextPressStartX;
	pContext->fContextPressStartY = pState->fContextPressStartY;
	pContext->fContextPressLastX = pState->fContextPressLastX;
	pContext->fContextPressLastY = pState->fContextPressLastY;
	pContext->fDragStartX = pState->fDragStartX;
	pContext->fDragStartY = pState->fDragStartY;
	pContext->fLastClickX = pState->fLastClickX;
	pContext->fLastClickY = pState->fLastClickY;
	pContext->iPointerButtons = pState->iPointerButtons;
	pContext->iActiveButton = pState->iActiveButton;
	pContext->iDragButton = pState->iDragButton;
	pContext->iLastClickButton = pState->iLastClickButton;
	pContext->bContextPressActive = pState->bContextPressActive;
	pContext->bContextPressMoved = pState->bContextPressMoved;
	pContext->bContextPressFired = pState->bContextPressFired;
	pContext->bDragActive = pState->bDragActive;
	pContext->fLastClickTime = pState->fLastClickTime;
}

static void __xuiInputPointerStateStore(xui_context pContext, xui_pointer_state_t* pState)
{
	if ( (pContext == NULL) || (pState == NULL) ) {
		return;
	}
	pState->iPointerId = pContext->iInputPointerId;
	pState->iPointerType = pContext->iInputPointerType;
	pState->pHoverWidget = pContext->pHoverWidget;
	pState->pActiveWidget = pContext->pActiveWidget;
	pState->pPointerCaptureWidget = pContext->pPointerCaptureWidget;
	pState->pDragWidget = pContext->pDragWidget;
	pState->pLastClickWidget = pContext->pLastClickWidget;
	pState->pContextPressWidget = pContext->pContextPressWidget;
	pState->fPointerX = pContext->fPointerX;
	pState->fPointerY = pContext->fPointerY;
	pState->fContextPressTime = pContext->fContextPressTime;
	pState->fContextPressStartX = pContext->fContextPressStartX;
	pState->fContextPressStartY = pContext->fContextPressStartY;
	pState->fContextPressLastX = pContext->fContextPressLastX;
	pState->fContextPressLastY = pContext->fContextPressLastY;
	pState->fDragStartX = pContext->fDragStartX;
	pState->fDragStartY = pContext->fDragStartY;
	pState->fLastClickX = pContext->fLastClickX;
	pState->fLastClickY = pContext->fLastClickY;
	pState->iPointerButtons = pContext->iPointerButtons;
	pState->iActiveButton = pContext->iActiveButton;
	pState->iDragButton = pContext->iDragButton;
	pState->iLastClickButton = pContext->iLastClickButton;
	pState->bContextPressActive = pContext->bContextPressActive;
	pState->bContextPressMoved = pContext->bContextPressMoved;
	pState->bContextPressFired = pContext->bContextPressFired;
	pState->bDragActive = pContext->bDragActive;
	pState->fLastClickTime = pContext->fLastClickTime;
	pState->bDown = (pContext->iPointerButtons != 0) ? 1 : 0;
}

static int __xuiInputEventUsesPointerState(const xui_event_t* pEvent)
{
	if ( pEvent == NULL ) {
		return 0;
	}
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_ENTER:
	case XUI_EVENT_POINTER_LEAVE:
	case XUI_EVENT_POINTER_MOVE:
	case XUI_EVENT_POINTER_DOWN:
	case XUI_EVENT_POINTER_UP:
	case XUI_EVENT_POINTER_CLICK:
	case XUI_EVENT_POINTER_WHEEL:
	case XUI_EVENT_POINTER_CAPTURE_LOST:
	case XUI_EVENT_POINTER_DOUBLE_CLICK:
	case XUI_EVENT_DRAG_BEGIN:
	case XUI_EVENT_DRAG_MOVE:
	case XUI_EVENT_DRAG_END:
	case XUI_EVENT_DRAG_CANCEL:
		return 1;
	default:
		break;
	}
	return (pEvent->iPointerType != XUI_POINTER_TYPE_MOUSE) ||
	       (pEvent->iPointerId != XUI_POINTER_ID_MOUSE) ||
	       (pEvent->iButton != 0);
}

static void __xuiInputInitEvent(xui_event_t* pEvent, int iType, xui_widget pTarget, xui_widget pRelated, xui_context pContext)
{
	memset(pEvent, 0, sizeof(*pEvent));
	pEvent->iSize = sizeof(*pEvent);
	pEvent->iType = iType;
	pEvent->pTarget = pTarget;
	pEvent->pRelated = pRelated;
	pEvent->fX = pContext->fPointerX;
	pEvent->fY = pContext->fPointerY;
	pEvent->iButtons = pContext->iPointerButtons;
	pEvent->iModifiers = pContext->iInputModifiers;
	pEvent->iPointerId = pContext->iInputPointerId;
	pEvent->iPointerType = pContext->iInputPointerType;
}

static int __xuiInputPushEvent(xui_context pContext, const xui_event_t* pEvent)
{
	int iRet;
	xui_event_t* pDst;

	if ( pContext->iEventRead > 0 ) {
		__xuiInputCompactEvents(pContext);
	}
	iRet = __xuiInputReserveEvents(pContext, pContext->iEventCount + 1);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pDst = &pContext->pEvents[pContext->iEventCount++];
	*pDst = *pEvent;
	if ( pEvent->sCommand == pEvent->sText ) {
		pDst->sCommand = pDst->sText;
	}
	return XUI_OK;
}

static int __xuiInputPushPointerEvent(xui_context pContext, int iType, xui_widget pTarget, xui_widget pRelated, int iButton, float fWheelX, float fWheelY)
{
	xui_event_t tEvent;

	__xuiInputInitEvent(&tEvent, iType, pTarget, pRelated, pContext);
	tEvent.iButton = iButton;
	tEvent.fWheelX = fWheelX;
	tEvent.fWheelY = fWheelY;
	return __xuiInputPushEvent(pContext, &tEvent);
}

static int __xuiInputSetWidgetFlag(xui_widget pWidget, uint32_t iFlag, int bEnabled)
{
	uint32_t iOldState;
	int iRet;

	if ( pWidget == NULL ) {
		return XUI_OK;
	}
	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iOldState = pWidget->iInputState;
	if ( bEnabled ) {
		pWidget->iInputState |= iFlag;
	} else {
		pWidget->iInputState &= ~iFlag;
	}
	if ( pWidget->iInputState == iOldState ) {
		return XUI_OK;
	}
	iRet = xuiWidgetResolveStyle(pWidget);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_STYLE | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiInputSetHoverWidget(xui_context pContext, xui_widget pWidget)
{
	xui_widget pOldWidget;
	int iRet;

	pOldWidget = pContext->pHoverWidget;
	if ( pOldWidget == pWidget ) {
		return XUI_OK;
	}
	if ( pOldWidget != NULL ) {
		iRet = __xuiInputSetWidgetFlag(pOldWidget, XUI_WIDGET_STATE_HOVER, 0);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		iRet = __xuiInputPushPointerEvent(pContext, XUI_EVENT_POINTER_LEAVE, pOldWidget, pWidget, 0, 0.0f, 0.0f);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	pContext->pHoverWidget = pWidget;
	if ( pWidget != NULL ) {
		iRet = __xuiInputSetWidgetFlag(pWidget, XUI_WIDGET_STATE_HOVER, 1);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		iRet = __xuiInputPushPointerEvent(pContext, XUI_EVENT_POINTER_ENTER, pWidget, pOldWidget, 0, 0.0f, 0.0f);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return XUI_OK;
}

static int __xuiInputSetActiveWidget(xui_context pContext, xui_widget pWidget, int iButton)
{
	xui_widget pOldWidget;
	int iRet;

	pOldWidget = pContext->pActiveWidget;
	if ( (pOldWidget == pWidget) && (pContext->iActiveButton == iButton) ) {
		return XUI_OK;
	}
	if ( pOldWidget != NULL ) {
		iRet = __xuiInputSetWidgetFlag(pOldWidget, XUI_WIDGET_STATE_ACTIVE, 0);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	pContext->pActiveWidget = pWidget;
	pContext->iActiveButton = (pWidget != NULL) ? iButton : 0;
	if ( pWidget != NULL ) {
		iRet = __xuiInputSetWidgetFlag(pWidget, XUI_WIDGET_STATE_ACTIVE, 1);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return XUI_OK;
}

static xui_widget __xuiInputFindFocusable(xui_widget pWidget)
{
	xui_widget pScan;

	for ( pScan = pWidget; pScan != NULL; pScan = pScan->pParent ) {
		if ( pScan->bFocusable && pScan->bVisible && pScan->bEnabled ) {
			return pScan;
		}
	}
	return NULL;
}

static int __xuiInputPointInRect(float fX, float fY, xui_rect_t tRect)
{
	return (fX >= tRect.fX) &&
	       (fY >= tRect.fY) &&
	       (fX < (tRect.fX + tRect.fW)) &&
	       (fY < (tRect.fY + tRect.fH));
}

static xui_widget __xuiInputHitTestWidget(xui_widget pWidget, float fX, float fY, uint32_t iFlags)
{
	xui_widget pChild;
	xui_widget pHit;
	xui_rect_t tWorldRect;

	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return NULL;
	}
	if ( ((iFlags & XUI_WIDGET_HIT_VISIBLE) != 0) && !pWidget->bVisible ) {
		return NULL;
	}
	if ( ((iFlags & XUI_WIDGET_HIT_ENABLED) != 0) && !pWidget->bEnabled ) {
		return NULL;
	}
	if ( !pWidget->bHitTestVisible ) {
		return NULL;
	}
	tWorldRect = xuiWidgetGetWorldRect(pWidget);
	if ( !__xuiInputPointInRect(fX, fY, tWorldRect) ) {
		return NULL;
	}
	if ( (iFlags & XUI_WIDGET_HIT_CHILDREN) != 0 ) {
		for ( pChild = pWidget->pLastChild; pChild != NULL; pChild = pChild->pPrevSibling ) {
			pHit = __xuiInputHitTestWidget(pChild, fX, fY, iFlags);
			if ( pHit != NULL ) {
				return pHit;
			}
		}
	}
	if ( (iFlags & XUI_WIDGET_HIT_SELF) != 0 ) {
		return pWidget;
	}
	return NULL;
}

static int __xuiInputSetPointer(xui_context pContext, float fX, float fY, uint32_t iButtons)
{
	if ( !__xuiInputFloatValid(fX) || !__xuiInputFloatValid(fY) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pContext->fPointerX = fX;
	pContext->fPointerY = fY;
	pContext->iPointerButtons = iButtons;
	return XUI_OK;
}

static int __xuiInputDispatchToWidget(xui_widget pWidget, xui_event_t* pEvent)
{
	int iRet;
	xui_widget_event_proc onHandler;
	void* pHandlerUser;

	if ( pWidget == NULL ) {
		return XUI_OK;
	}
	pEvent->pCurrentTarget = pWidget;
	if ( (pEvent->iType >= 0) && (pEvent->iType < XUI_WIDGET_EVENT_SLOT_COUNT) ) {
		onHandler = pWidget->arrEventHandlers[pEvent->iType];
		pHandlerUser = pWidget->arrEventUsers[pEvent->iType];
		if ( onHandler != NULL ) {
			iRet = onHandler(pWidget, pEvent, pHandlerUser);
			if ( iRet < 0 ) {
				return iRet;
			}
			if ( (iRet & XUI_EVENT_DISPATCH_STOP) != 0 ) {
				pEvent->iFlags |= XUI_EVENT_DISPATCH_STOP;
				return XUI_OK;
			}
		}
	}
	if ( pWidget->onEvent != NULL ) {
		iRet = pWidget->onEvent(pWidget, pEvent, pWidget->pEventUser);
		if ( iRet < 0 ) {
			return iRet;
		}
		if ( (iRet & XUI_EVENT_DISPATCH_STOP) != 0 ) {
			pEvent->iFlags |= XUI_EVENT_DISPATCH_STOP;
		}
	}
	return XUI_OK;
}

static int __xuiInputEventIsKeyboardTargetMessage(int iType)
{
	return (iType == XUI_EVENT_KEY_DOWN) ||
	       (iType == XUI_EVENT_KEY_UP) ||
	       (iType == XUI_EVENT_TEXT) ||
	       (iType == XUI_EVENT_IME_COMPOSITION);
}

static int __xuiInputDispatchPath(const xui_event_t* pSourceEvent, xui_widget* pPath, int iCount, int* pFlags)
{
	xui_event_t tEvent;
	int i;
	int iRet;

	tEvent = *pSourceEvent;
	tEvent.iSize = sizeof(tEvent);
	tEvent.pCurrentTarget = NULL;
	if ( __xuiInputEventIsKeyboardTargetMessage(tEvent.iType) ) {
		tEvent.iPhase = XUI_EVENT_PHASE_TARGET;
		iRet = __xuiInputDispatchToWidget(pPath[0], &tEvent);
		if ( pFlags != NULL ) *pFlags = tEvent.iFlags;
		if ( (iRet != XUI_OK) || ((tEvent.iFlags & XUI_EVENT_DISPATCH_STOP) != 0) ) {
			return iRet;
		}
		for ( i = 1; i < iCount; i++ ) {
			tEvent.iPhase = XUI_EVENT_PHASE_BUBBLE;
			iRet = __xuiInputDispatchToWidget(pPath[i], &tEvent);
			if ( pFlags != NULL ) *pFlags = tEvent.iFlags;
			if ( (iRet != XUI_OK) || ((tEvent.iFlags & XUI_EVENT_DISPATCH_STOP) != 0) ) {
				return iRet;
			}
		}
		if ( pFlags != NULL ) *pFlags = tEvent.iFlags;
		return XUI_OK;
	}
	for ( i = iCount - 1; i > 0; i-- ) {
		tEvent.iPhase = XUI_EVENT_PHASE_CAPTURE;
		iRet = __xuiInputDispatchToWidget(pPath[i], &tEvent);
		if ( pFlags != NULL ) *pFlags = tEvent.iFlags;
		if ( (iRet != XUI_OK) || ((tEvent.iFlags & XUI_EVENT_DISPATCH_STOP) != 0) ) {
			return iRet;
		}
	}
	tEvent.iPhase = XUI_EVENT_PHASE_TARGET;
	iRet = __xuiInputDispatchToWidget(pPath[0], &tEvent);
	if ( pFlags != NULL ) *pFlags = tEvent.iFlags;
	if ( (iRet != XUI_OK) || ((tEvent.iFlags & XUI_EVENT_DISPATCH_STOP) != 0) ) {
		return iRet;
	}
	for ( i = 1; i < iCount; i++ ) {
		tEvent.iPhase = XUI_EVENT_PHASE_BUBBLE;
		iRet = __xuiInputDispatchToWidget(pPath[i], &tEvent);
		if ( pFlags != NULL ) *pFlags = tEvent.iFlags;
		if ( (iRet != XUI_OK) || ((tEvent.iFlags & XUI_EVENT_DISPATCH_STOP) != 0) ) {
			return iRet;
		}
	}
	if ( pFlags != NULL ) *pFlags = tEvent.iFlags;
	return XUI_OK;
}

static int __xuiInputFocusableForTab(xui_widget pWidget)
{
	return (pWidget != NULL) &&
	       pWidget->bFocusable &&
	       pWidget->bTabStop &&
	       pWidget->bVisible &&
	       pWidget->bEnabled;
}

static xui_widget __xuiInputFocusScopeRoot(xui_context pContext)
{
	xui_widget pScan;

	if ( pContext->pFocusWidget != NULL ) {
		for ( pScan = pContext->pFocusWidget; pScan != NULL; pScan = pScan->pParent ) {
			if ( pScan->bFocusScope ) {
				return pScan;
			}
		}
	}
	return (pContext->pRoot != NULL) ? pContext->pRoot : pContext->pOverlayRoot;
}

static int __xuiInputFocusListReserve(xui_input_focus_item_t** ppItems, int* pCapacity, int iCount)
{
	xui_input_focus_item_t* pItems;
	int iCapacity;

	if ( iCount <= *pCapacity ) {
		return XUI_OK;
	}
	iCapacity = (*pCapacity > 0) ? (*pCapacity * 2) : 16;
	if ( iCapacity < iCount ) {
		iCapacity = iCount;
	}
	if ( *ppItems == NULL ) {
		pItems = (xui_input_focus_item_t*)xrtMalloc(sizeof(*pItems) * (size_t)iCapacity);
		if ( pItems == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
	} else {
		pItems = (xui_input_focus_item_t*)xrtRealloc(*ppItems, sizeof(*pItems) * (size_t)iCapacity);
		if ( pItems == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
	}
	*ppItems = pItems;
	*pCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiInputCollectFocusItems(xui_widget pWidget, xui_input_focus_item_t** ppItems, int* pCount, int* pCapacity, int* pOrder)
{
	xui_widget pChild;
	xui_input_focus_item_t* pItem;
	int iRet;

	if ( pWidget == NULL ) {
		return XUI_OK;
	}
	if ( __xuiInputFocusableForTab(pWidget) ) {
		iRet = __xuiInputFocusListReserve(ppItems, pCapacity, *pCount + 1);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		pItem = &(*ppItems)[(*pCount)++];
		pItem->pWidget = pWidget;
		pItem->iOrder = *pOrder;
		pItem->iTabIndex = pWidget->iTabIndex;
	}
	(*pOrder)++;
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		iRet = __xuiInputCollectFocusItems(pChild, ppItems, pCount, pCapacity, pOrder);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return XUI_OK;
}

static int __xuiInputFocusItemAfter(const xui_input_focus_item_t* pA, const xui_input_focus_item_t* pB)
{
	int iATab;
	int iBTab;

	iATab = (pA->iTabIndex > 0) ? pA->iTabIndex : 0x7fffffff;
	iBTab = (pB->iTabIndex > 0) ? pB->iTabIndex : 0x7fffffff;
	if ( iATab != iBTab ) {
		return iATab > iBTab;
	}
	return pA->iOrder > pB->iOrder;
}

static void __xuiInputSortFocusItems(xui_input_focus_item_t* pItems, int iCount)
{
	xui_input_focus_item_t tItem;
	int i;
	int j;

	for ( i = 1; i < iCount; i++ ) {
		tItem = pItems[i];
		j = i - 1;
		while ( (j >= 0) && __xuiInputFocusItemAfter(&pItems[j], &tItem) ) {
			pItems[j + 1] = pItems[j];
			j--;
		}
		pItems[j + 1] = tItem;
	}
}

static xui_widget __xuiInputFocusNextInScope(xui_context pContext, int iForward)
{
	xui_input_focus_item_t* pItems;
	xui_widget pScope;
	xui_widget pResult;
	int iCapacity;
	int iCount;
	int iOrder;
	int i;
	int iCurrent;
	int iRet;

	pScope = __xuiInputFocusScopeRoot(pContext);
	if ( pScope == NULL ) {
		return NULL;
	}
	pItems = NULL;
	iCapacity = 0;
	iCount = 0;
	iOrder = 0;
	iRet = __xuiInputCollectFocusItems(pScope, &pItems, &iCount, &iCapacity, &iOrder);
	if ( iRet != XUI_OK ) {
		if ( pItems != NULL ) {
			xrtFree(pItems);
		}
		return NULL;
	}
	if ( iCount <= 0 ) {
		xrtFree(pItems);
		return NULL;
	}
	__xuiInputSortFocusItems(pItems, iCount);
	iCurrent = -1;
	for ( i = 0; i < iCount; i++ ) {
		if ( pItems[i].pWidget == pContext->pFocusWidget ) {
			iCurrent = i;
			break;
		}
	}
	if ( iCurrent < 0 ) {
		pResult = iForward ? pItems[0].pWidget : pItems[iCount - 1].pWidget;
	} else if ( iForward ) {
		pResult = pItems[(iCurrent + 1) % iCount].pWidget;
	} else {
		pResult = pItems[(iCurrent + iCount - 1) % iCount].pWidget;
	}
	xrtFree(pItems);
	return pResult;
}

static int __xuiInputWidgetWantsDrag(xui_widget pWidget)
{
	return (pWidget != NULL) && pWidget->bDragEnabled && pWidget->bVisible && pWidget->bEnabled;
}

static float __xuiInputDistanceSquared(float fX0, float fY0, float fX1, float fY1)
{
	float fDX;
	float fDY;

	fDX = fX1 - fX0;
	fDY = fY1 - fY0;
	return (fDX * fDX) + (fDY * fDY);
}

void xuiInternalContextPressCancel(xui_context pContext)
{
	if ( pContext == NULL ) {
		return;
	}
	pContext->bContextPressActive = 0;
	pContext->bContextPressMoved = 0;
	pContext->bContextPressFired = 0;
	pContext->fContextPressTime = 0.0f;
	pContext->pContextPressWidget = NULL;
}

static void __xuiInputContextPressBegin(xui_context pContext, xui_widget pWidget)
{
	if ( pContext == NULL ) {
		return;
	}
	pContext->bContextPressActive = (pWidget != NULL) ? 1 : 0;
	pContext->bContextPressMoved = 0;
	pContext->bContextPressFired = 0;
	pContext->fContextPressTime = 0.0f;
	pContext->fContextPressStartX = pContext->fPointerX;
	pContext->fContextPressStartY = pContext->fPointerY;
	pContext->fContextPressLastX = pContext->fPointerX;
	pContext->fContextPressLastY = pContext->fPointerY;
	pContext->pContextPressWidget = pWidget;
}

static void __xuiInputContextPressMove(xui_context pContext)
{
	float fDistance;

	if ( (pContext == NULL) || (pContext->bContextPressActive == 0) ) {
		return;
	}
	pContext->fContextPressLastX = pContext->fPointerX;
	pContext->fContextPressLastY = pContext->fPointerY;
	if ( pContext->bContextPressFired != 0 ) {
		return;
	}
	fDistance = __xuiInputDistanceSquared(pContext->fContextPressStartX, pContext->fContextPressStartY, pContext->fPointerX, pContext->fPointerY);
	if ( fDistance > (XUI_INPUT_CONTEXT_PRESS_DISTANCE * XUI_INPUT_CONTEXT_PRESS_DISTANCE) ) {
		pContext->bContextPressMoved = 1;
	}
}

static int __xuiInputContextPressUpdateCurrent(xui_context pContext, float fDelta)
{
	xui_event_t tEvent;
	xui_widget pTarget;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( fDelta != fDelta ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( fDelta < 0.0f ) {
		fDelta = 0.0f;
	}
	if ( (pContext->bContextPressActive == 0) ||
	     (pContext->bContextPressMoved != 0) ||
	     (pContext->bContextPressFired != 0) ) {
		return XUI_OK;
	}
	pTarget = pContext->pContextPressWidget;
	if ( (pTarget == NULL) ||
	     !xuiInternalWidgetIsValid(pTarget) ||
	     (pTarget->pContext != pContext) ||
	     !pTarget->bVisible ||
	     !pTarget->bEnabled ) {
		xuiInternalContextPressCancel(pContext);
		return XUI_OK;
	}
	pContext->fContextPressTime += fDelta;
	if ( pContext->fContextPressTime < XUI_INPUT_CONTEXT_PRESS_SECONDS ) {
		return XUI_OK;
	}
	pContext->bContextPressFired = 1;
	__xuiInputInitEvent(&tEvent, XUI_EVENT_CONTEXT_MENU, pTarget, NULL, pContext);
	tEvent.fX = pContext->fContextPressLastX;
	tEvent.fY = pContext->fContextPressLastY;
	tEvent.iButton = XUI_POINTER_BUTTON_LEFT;
	return xuiDispatchEvent(pContext, &tEvent);
}

int xuiInternalContextPressUpdate(xui_context pContext, float fDelta)
{
	xui_pointer_state_t* pState;
	int i;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( fDelta != fDelta ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	for ( i = 0; i < XUI_POINTER_MAX; i++ ) {
		pState = &pContext->arrPointerStates[i];
		if ( !pState->bAllocated || (pState->bContextPressActive == 0) ) {
			continue;
		}
		__xuiInputPointerStateLoad(pContext, pState);
		iRet = __xuiInputContextPressUpdateCurrent(pContext, fDelta);
		__xuiInputPointerStateStore(pContext, pState);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return XUI_OK;
}

static int __xuiInputDragCancel(xui_context pContext)
{
	xui_widget pDragWidget;
	int iRet;

	pDragWidget = pContext->pDragWidget;
	if ( pDragWidget == NULL ) {
		return XUI_OK;
	}
	if ( pContext->bDragActive ) {
		iRet = __xuiInputPushPointerEvent(pContext, XUI_EVENT_DRAG_CANCEL, pDragWidget, NULL, pContext->iDragButton, 0.0f, 0.0f);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	pContext->pDragWidget = NULL;
	pContext->bDragActive = 0;
	pContext->iDragButton = 0;
	return XUI_OK;
}

static int __xuiInputDragMove(xui_context pContext, xui_widget pRelated)
{
	float fDistance;
	int iRet;

	if ( pContext->pDragWidget == NULL ) {
		return XUI_OK;
	}
	if ( !pContext->bDragActive ) {
		fDistance = __xuiInputDistanceSquared(pContext->fDragStartX, pContext->fDragStartY, pContext->fPointerX, pContext->fPointerY);
		if ( fDistance < (XUI_INPUT_DRAG_DISTANCE * XUI_INPUT_DRAG_DISTANCE) ) {
			return XUI_OK;
		}
		pContext->bDragActive = 1;
		iRet = __xuiInputPushPointerEvent(pContext, XUI_EVENT_DRAG_BEGIN, pContext->pDragWidget, pRelated, pContext->iDragButton, 0.0f, 0.0f);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return __xuiInputPushPointerEvent(pContext, XUI_EVENT_DRAG_MOVE, pContext->pDragWidget, pRelated, pContext->iDragButton, 0.0f, 0.0f);
}

static int __xuiInputDragEnd(xui_context pContext, xui_widget pRelated)
{
	xui_widget pDragWidget;
	int iRet;

	pDragWidget = pContext->pDragWidget;
	if ( pDragWidget == NULL ) {
		return XUI_OK;
	}
	if ( pContext->bDragActive ) {
		iRet = __xuiInputPushPointerEvent(pContext, XUI_EVENT_DRAG_END, pDragWidget, pRelated, pContext->iDragButton, 0.0f, 0.0f);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	pContext->pDragWidget = NULL;
	pContext->bDragActive = 0;
	pContext->iDragButton = 0;
	return XUI_OK;
}

static int __xuiInputHandleClickEvents(xui_context pContext, xui_widget pWidget, int iButton)
{
	double fNow;
	float fDistance;
	int iRet;

	iRet = __xuiInputPushPointerEvent(pContext, XUI_EVENT_POINTER_CLICK, pWidget, NULL, iButton, 0.0f, 0.0f);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	fNow = xrtTimer();
	fDistance = __xuiInputDistanceSquared(pContext->fLastClickX, pContext->fLastClickY, pContext->fPointerX, pContext->fPointerY);
	if ( (pContext->pLastClickWidget == pWidget) &&
	     (pContext->iLastClickButton == iButton) &&
	     ((fNow - pContext->fLastClickTime) <= XUI_INPUT_DOUBLE_CLICK_SECONDS) &&
	     (fDistance <= (XUI_INPUT_DOUBLE_CLICK_DISTANCE * XUI_INPUT_DOUBLE_CLICK_DISTANCE)) ) {
		iRet = __xuiInputPushPointerEvent(pContext, XUI_EVENT_POINTER_DOUBLE_CLICK, pWidget, NULL, iButton, 0.0f, 0.0f);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		pContext->pLastClickWidget = NULL;
		pContext->fLastClickTime = 0.0;
	} else {
		pContext->pLastClickWidget = pWidget;
		pContext->iLastClickButton = iButton;
		pContext->fLastClickX = pContext->fPointerX;
		pContext->fLastClickY = pContext->fPointerY;
		pContext->fLastClickTime = fNow;
	}
	if ( iButton == XUI_POINTER_BUTTON_RIGHT ) {
		return __xuiInputPushPointerEvent(pContext, XUI_EVENT_CONTEXT_MENU, pWidget, NULL, iButton, 0.0f, 0.0f);
	}
	return XUI_OK;
}

static int __xuiInputPointerMoveCurrent(xui_context pContext, float fX, float fY, uint32_t iButtons)
{
	xui_widget pHitWidget;
	xui_widget pTargetWidget;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiInputSetPointer(pContext, fX, fY, iButtons);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( pContext->bContextPressActive != 0 ) {
		__xuiInputContextPressMove(pContext);
	}
	pHitWidget = xuiHitTest(pContext, fX, fY, XUI_WIDGET_HIT_DEFAULT);
	iRet = __xuiInputSetHoverWidget(pContext, pHitWidget);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iRet = xuiInternalTooltipPointerMove(pContext, pHitWidget);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( pContext->pPointerCaptureWidget != NULL ) {
		pTargetWidget = pContext->pPointerCaptureWidget;
	} else {
		pTargetWidget = (pContext->pActiveWidget != NULL) ? pContext->pActiveWidget : pHitWidget;
	}
	if ( pTargetWidget == NULL ) {
		return XUI_OK;
	}
	iRet = __xuiInputDragMove(pContext, pHitWidget);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return __xuiInputPushPointerEvent(pContext, XUI_EVENT_POINTER_MOVE, pTargetWidget, pHitWidget, 0, 0.0f, 0.0f);
}

static int __xuiInputPointerCanChangeFocus(xui_context pContext)
{
	xui_pointer_state_t* pState;
	int i;

	if ( pContext == NULL ) {
		return 0;
	}
	if ( pContext->iInputPointerType != XUI_POINTER_TYPE_TOUCH ) {
		return 1;
	}
	for ( i = 0; i < XUI_POINTER_MAX; i++ ) {
		pState = &pContext->arrPointerStates[i];
		if ( !pState->bAllocated || !pState->bDown || (pState->iPointerType != XUI_POINTER_TYPE_TOUCH) ) {
			continue;
		}
		if ( pState->iPointerId != pContext->iInputPointerId ) {
			return 0;
		}
	}
	return 1;
}

static int __xuiInputPointerDownCurrent(xui_context pContext, float fX, float fY, int iButton, uint32_t iButtons)
{
	xui_widget pHitWidget;
	xui_widget pFocusWidget;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) || !__xuiInputButtonValid(iButton) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iButtons |= (uint32_t)iButton;
	iRet = __xuiInputSetPointer(pContext, fX, fY, iButtons);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pHitWidget = xuiHitTest(pContext, fX, fY, XUI_WIDGET_HIT_DEFAULT);
	iRet = __xuiInputSetHoverWidget(pContext, pHitWidget);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	xuiInternalTooltipCancel(pContext);
	iRet = __xuiInputSetActiveWidget(pContext, pHitWidget, iButton);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( __xuiInputWidgetWantsDrag(pHitWidget) ) {
		pContext->pDragWidget = pHitWidget;
		pContext->bDragActive = 0;
		pContext->iDragButton = iButton;
		pContext->fDragStartX = fX;
		pContext->fDragStartY = fY;
	} else {
		(void)__xuiInputDragCancel(pContext);
	}
	if ( __xuiInputPointerCanChangeFocus(pContext) ) {
		pFocusWidget = __xuiInputFindFocusable(pHitWidget);
		iRet = xuiSetFocusWidget(pContext, pFocusWidget);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	if ( pHitWidget == NULL ) {
		xuiInternalContextPressCancel(pContext);
		return XUI_OK;
	}
	if ( iButton == XUI_POINTER_BUTTON_LEFT ) {
		__xuiInputContextPressBegin(pContext, pHitWidget);
	} else {
		xuiInternalContextPressCancel(pContext);
	}
	return __xuiInputPushPointerEvent(pContext, XUI_EVENT_POINTER_DOWN, pHitWidget, NULL, iButton, 0.0f, 0.0f);
}

static int __xuiInputPointerUpCurrent(xui_context pContext, float fX, float fY, int iButton, uint32_t iButtons)
{
	xui_widget pHitWidget;
	xui_widget pTargetWidget;
	int bClick;
	int bContextPressFired;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) || !__xuiInputButtonValid(iButton) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iButtons &= ~((uint32_t)iButton);
	iRet = __xuiInputSetPointer(pContext, fX, fY, iButtons);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	pHitWidget = xuiHitTest(pContext, fX, fY, XUI_WIDGET_HIT_DEFAULT);
	iRet = __xuiInputSetHoverWidget(pContext, pHitWidget);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	xuiInternalTooltipCancel(pContext);
	if ( pContext->pPointerCaptureWidget != NULL ) {
		pTargetWidget = pContext->pPointerCaptureWidget;
	} else {
		pTargetWidget = (pContext->pActiveWidget != NULL) ? pContext->pActiveWidget : pHitWidget;
	}
	bClick = (pContext->pActiveWidget != NULL) &&
	         (pContext->pActiveWidget == pHitWidget) &&
	         (pContext->iActiveButton == iButton);
	bContextPressFired = (iButton == XUI_POINTER_BUTTON_LEFT) && (pContext->bContextPressFired != 0);
	if ( pTargetWidget != NULL ) {
		iRet = __xuiInputPushPointerEvent(pContext, XUI_EVENT_POINTER_UP, pTargetWidget, pHitWidget, iButton, 0.0f, 0.0f);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	iRet = __xuiInputDragEnd(pContext, pHitWidget);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iRet = __xuiInputSetActiveWidget(pContext, NULL, 0);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( iButton == XUI_POINTER_BUTTON_LEFT ) {
		xuiInternalContextPressCancel(pContext);
	}
	if ( bClick && !bContextPressFired && (pHitWidget != NULL) ) {
		return __xuiInputHandleClickEvents(pContext, pHitWidget, iButton);
	}
	return XUI_OK;
}

static int __xuiInputPointerWheelCurrent(xui_context pContext, float fX, float fY, float fWheelX, float fWheelY, uint32_t iButtons)
{
	xui_widget pHitWidget;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ||
	     !__xuiInputFloatValid(fWheelX) ||
	     !__xuiInputFloatValid(fWheelY) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiInputSetPointer(pContext, fX, fY, iButtons);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	xuiInternalContextPressCancel(pContext);
	pHitWidget = xuiHitTest(pContext, fX, fY, XUI_WIDGET_HIT_DEFAULT);
	iRet = __xuiInputSetHoverWidget(pContext, pHitWidget);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	xuiInternalTooltipCancel(pContext);
	return __xuiInputPushPointerEvent(pContext, XUI_EVENT_POINTER_WHEEL,
		(pContext->pPointerCaptureWidget != NULL) ? pContext->pPointerCaptureWidget : pHitWidget,
		pHitWidget, 0, fWheelX, fWheelY);
}

static int __xuiInputPointerLeaveCurrent(xui_context pContext)
{
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiInputDragCancel(pContext);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	xuiInternalTooltipCancel(pContext);
	xuiInternalContextPressCancel(pContext);
	return __xuiInputSetHoverWidget(pContext, NULL);
}

XUI_API int xuiInputPointerMoveEx(xui_context pContext, uint64_t iPointerId, int iPointerType, float fX, float fY, uint32_t iButtons)
{
	xui_pointer_state_t* pState;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pState = __xuiInputPointerStateFind(pContext, iPointerId, iPointerType, 1);
	if ( pState == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	__xuiInputPointerStateLoad(pContext, pState);
	iRet = __xuiInputPointerMoveCurrent(pContext, fX, fY, iButtons);
	__xuiInputPointerStateStore(pContext, pState);
	return iRet;
}

XUI_API int xuiInputPointerDownEx(xui_context pContext, uint64_t iPointerId, int iPointerType, float fX, float fY, int iButton, uint32_t iButtons)
{
	xui_pointer_state_t* pState;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pState = __xuiInputPointerStateFind(pContext, iPointerId, iPointerType, 1);
	if ( pState == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	__xuiInputPointerStateLoad(pContext, pState);
	iRet = __xuiInputPointerDownCurrent(pContext, fX, fY, iButton, iButtons);
	__xuiInputPointerStateStore(pContext, pState);
	return iRet;
}

XUI_API int xuiInputPointerUpEx(xui_context pContext, uint64_t iPointerId, int iPointerType, float fX, float fY, int iButton, uint32_t iButtons)
{
	xui_pointer_state_t* pState;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pState = __xuiInputPointerStateFind(pContext, iPointerId, iPointerType, 1);
	if ( pState == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	__xuiInputPointerStateLoad(pContext, pState);
	iRet = __xuiInputPointerUpCurrent(pContext, fX, fY, iButton, iButtons);
	if ( (iRet == XUI_OK) && (pContext->iInputPointerType != XUI_POINTER_TYPE_MOUSE) ) {
		iRet = __xuiInputSetHoverWidget(pContext, NULL);
	}
	__xuiInputPointerStateStore(pContext, pState);
	return iRet;
}

XUI_API int xuiInputPointerWheelEx(xui_context pContext, uint64_t iPointerId, int iPointerType, float fX, float fY, float fWheelX, float fWheelY, uint32_t iButtons)
{
	xui_pointer_state_t* pState;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pState = __xuiInputPointerStateFind(pContext, iPointerId, iPointerType, 1);
	if ( pState == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	__xuiInputPointerStateLoad(pContext, pState);
	iRet = __xuiInputPointerWheelCurrent(pContext, fX, fY, fWheelX, fWheelY, iButtons);
	__xuiInputPointerStateStore(pContext, pState);
	return iRet;
}

XUI_API int xuiInputPointerCancelEx(xui_context pContext, uint64_t iPointerId, int iPointerType)
{
	xui_pointer_state_t* pState;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pState = __xuiInputPointerStateFind(pContext, iPointerId, iPointerType, 0);
	if ( pState == NULL ) {
		return XUI_OK;
	}
	__xuiInputPointerStateLoad(pContext, pState);
	iRet = xuiSetPointerCaptureEx(pContext, pContext->iInputPointerId, pContext->iInputPointerType, NULL);
	if ( iRet == XUI_OK ) {
		iRet = __xuiInputDragCancel(pContext);
	}
	if ( iRet == XUI_OK ) {
		iRet = __xuiInputSetActiveWidget(pContext, NULL, 0);
	}
	if ( iRet == XUI_OK ) {
		iRet = __xuiInputSetHoverWidget(pContext, NULL);
	}
	xuiInternalContextPressCancel(pContext);
	pContext->iPointerButtons = 0;
	__xuiInputPointerStateStore(pContext, pState);
	pState->bDown = 0;
	return iRet;
}

XUI_API int xuiInputPointerMove(xui_context pContext, float fX, float fY, uint32_t iButtons)
{
	return xuiInputPointerMoveEx(pContext, XUI_POINTER_ID_MOUSE, XUI_POINTER_TYPE_MOUSE, fX, fY, iButtons);
}

XUI_API int xuiInputPointerDown(xui_context pContext, float fX, float fY, int iButton, uint32_t iButtons)
{
	return xuiInputPointerDownEx(pContext, XUI_POINTER_ID_MOUSE, XUI_POINTER_TYPE_MOUSE, fX, fY, iButton, iButtons);
}

XUI_API int xuiInputPointerUp(xui_context pContext, float fX, float fY, int iButton, uint32_t iButtons)
{
	return xuiInputPointerUpEx(pContext, XUI_POINTER_ID_MOUSE, XUI_POINTER_TYPE_MOUSE, fX, fY, iButton, iButtons);
}

XUI_API int xuiInputPointerWheel(xui_context pContext, float fX, float fY, float fWheelX, float fWheelY, uint32_t iButtons)
{
	return xuiInputPointerWheelEx(pContext, XUI_POINTER_ID_MOUSE, XUI_POINTER_TYPE_MOUSE, fX, fY, fWheelX, fWheelY, iButtons);
}

XUI_API int xuiInputPointerLeave(xui_context pContext)
{
	xui_pointer_state_t* pState;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pState = __xuiInputPointerStateFind(pContext, XUI_POINTER_ID_MOUSE, XUI_POINTER_TYPE_MOUSE, 1);
	if ( pState == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	__xuiInputPointerStateLoad(pContext, pState);
	iRet = __xuiInputPointerLeaveCurrent(pContext);
	__xuiInputPointerStateStore(pContext, pState);
	return iRet;
}

XUI_API int xuiInputSetModifiers(xui_context pContext, uint32_t iModifiers)
{
	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pContext->iInputModifiers = __xuiInputNormalizeModifiers(iModifiers);
	return XUI_OK;
}

XUI_API uint32_t xuiInputGetModifiers(xui_context pContext)
{
	return xuiInternalContextIsValid(pContext) ? pContext->iInputModifiers : 0u;
}

static int __xuiInputHotkeyMatches(const xui_hotkey_t* pHotkey, int iKey, uint32_t iModifiers)
{
	return (pHotkey != NULL) &&
	       (pHotkey->pWidget != NULL) &&
	       (pHotkey->iKey == iKey) &&
	       (pHotkey->iModifiers == iModifiers);
}

static int __xuiInputPushHotkeyEvent(xui_context pContext, const xui_hotkey_t* pHotkey)
{
	xui_event_t tEvent;
	int iRet;

	__xuiInputInitEvent(&tEvent, (pHotkey->sCommand != NULL) ? XUI_EVENT_COMMAND : XUI_EVENT_HOTKEY, pHotkey->pWidget, NULL, pContext);
	tEvent.iKey = pHotkey->iKey;
	tEvent.iModifiers = pHotkey->iModifiers;
	tEvent.iCommand = pHotkey->iCommand;
	tEvent.pData = pHotkey->pData;
	if ( pHotkey->sCommand != NULL ) {
		tEvent.iTextSize = __xuiInputTextCopy(tEvent.sText, (int)sizeof(tEvent.sText), pHotkey->sCommand, -1);
		tEvent.sCommand = tEvent.sText;
	}
	if ( pHotkey->onEvent != NULL ) {
		iRet = pHotkey->onEvent(pHotkey->pWidget, &tEvent, pHotkey->pUser);
		if ( iRet < 0 ) {
			return iRet;
		}
	}
	return __xuiInputPushEvent(pContext, &tEvent);
}

static int __xuiInputInvokeAction(xui_widget pWidget, int bDefault)
{
	xui_widget pScan;

	for ( pScan = pWidget; pScan != NULL; pScan = pScan->pParent ) {
		if ( bDefault ) {
			if ( pScan->onDefaultAction != NULL ) {
				pScan->onDefaultAction(pScan, pScan->pDefaultActionUser);
				return 1;
			}
		} else if ( pScan->onCancelAction != NULL ) {
			pScan->onCancelAction(pScan, pScan->pCancelActionUser);
			return 1;
		}
	}
	return 0;
}

static int __xuiInputDispatchFocusKeyDown(xui_context pContext, int iKey, uint32_t iModifiers, int* pFlags)
{
	xui_event_t tEvent;
	int iRet;

	__xuiInputInitEvent(&tEvent, XUI_EVENT_KEY_DOWN, pContext->pFocusWidget, NULL, pContext);
	tEvent.iKey = iKey;
	tEvent.iModifiers = iModifiers;
	iRet = __xuiInputDispatchEventWithFlags(pContext, &tEvent, pFlags);
	if ( iRet != XUI_OK ) return iRet;
	return XUI_OK;
}

static int __xuiInputDispatchHotkeysEx(xui_context pContext, int iKey, uint32_t iModifiers, uint32_t* pResult)
{
	int i;
	int iRet;

	for ( i = 0; i < pContext->iHotkeyCount; i++ ) {
		if ( !__xuiInputHotkeyMatches(&pContext->pHotkeys[i], iKey, iModifiers) ) {
			continue;
		}
		iRet = __xuiInputPushHotkeyEvent(pContext, &pContext->pHotkeys[i]);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		if ( pResult != NULL ) {
			*pResult |= XUI_INPUT_RESULT_HOTKEY | XUI_INPUT_RESULT_CONSUMED;
		}
	}
	return XUI_OK;
}

XUI_API int xuiInputKeyDownEx(xui_context pContext, int iKey, uint32_t iModifiers, uint32_t* pResult)
{
	xui_event_t tEvent;
	int iFlags;
	xui_widget pOldFocus;
	int iRet;

	if ( pResult != NULL ) {
		*pResult = 0u;
	}
	if ( !xuiInternalContextIsValid(pContext) || (iKey < 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iModifiers = __xuiInputNormalizeModifiers(iModifiers);
	pContext->iInputModifiers = iModifiers;
	iFlags = 0;
	iRet = __xuiInputDispatchFocusKeyDown(pContext, iKey, iModifiers, &iFlags);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( (iFlags & XUI_EVENT_DISPATCH_STOP) != 0 ) {
		if ( pResult != NULL ) {
			*pResult |= XUI_INPUT_RESULT_CONSUMED;
		}
		return XUI_OK;
	}
	if ( iKey == XUI_KEY_TAB ) {
		pOldFocus = pContext->pFocusWidget;
		iRet = xuiFocusNext(pContext, ((iModifiers & XUI_MOD_SHIFT) == 0));
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		if ( pResult != NULL ) {
			*pResult |= XUI_INPUT_RESULT_CONSUMED;
			if ( pOldFocus != pContext->pFocusWidget ) {
				*pResult |= XUI_INPUT_RESULT_FOCUS_CHANGED;
			}
		}
		return XUI_OK;
	} else if ( iKey == XUI_KEY_ENTER ) {
		if ( __xuiInputInvokeAction(pContext->pFocusWidget, 1) ) {
			if ( pResult != NULL ) {
				*pResult |= XUI_INPUT_RESULT_CONSUMED;
			}
			return XUI_OK;
		}
	} else if ( iKey == XUI_KEY_ESCAPE ) {
		if ( __xuiInputInvokeAction(pContext->pFocusWidget, 0) ) {
			if ( pResult != NULL ) {
				*pResult |= XUI_INPUT_RESULT_CONSUMED;
			}
			return XUI_OK;
		}
	} else if ( iKey == XUI_KEY_CONTEXT_MENU ) {
		__xuiInputInitEvent(&tEvent, XUI_EVENT_CONTEXT_MENU, pContext->pFocusWidget, NULL, pContext);
		tEvent.iKey = iKey;
		tEvent.iModifiers = iModifiers;
		iRet = __xuiInputDispatchEventWithFlags(pContext, &tEvent, &iFlags);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		if ( pResult != NULL ) {
			*pResult |= XUI_INPUT_RESULT_CONSUMED;
		}
		return XUI_OK;
	}
	return __xuiInputDispatchHotkeysEx(pContext, iKey, iModifiers, pResult);
}

XUI_API int xuiInputKeyDown(xui_context pContext, int iKey, uint32_t iModifiers)
{
	return xuiInputKeyDownEx(pContext, iKey, iModifiers, NULL);
}

XUI_API int xuiInputKeyUpEx(xui_context pContext, int iKey, uint32_t iModifiers, uint32_t* pResult)
{
	xui_event_t tEvent;
	int iFlags;
	int iRet;

	if ( pResult != NULL ) {
		*pResult = 0u;
	}
	if ( !xuiInternalContextIsValid(pContext) || (iKey < 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iModifiers = __xuiInputNormalizeModifiers(iModifiers);
	pContext->iInputModifiers = iModifiers;
	__xuiInputInitEvent(&tEvent, XUI_EVENT_KEY_UP, pContext->pFocusWidget, NULL, pContext);
	tEvent.iKey = iKey;
	tEvent.iModifiers = iModifiers;
	iFlags = 0;
	iRet = __xuiInputDispatchEventWithFlags(pContext, &tEvent, &iFlags);
	if ( iRet != XUI_OK ) return iRet;
	if ( (iFlags & XUI_EVENT_DISPATCH_STOP) != 0 && pResult != NULL ) {
		*pResult |= XUI_INPUT_RESULT_CONSUMED;
	}
	return XUI_OK;
}

XUI_API int xuiInputKeyUp(xui_context pContext, int iKey, uint32_t iModifiers)
{
	return xuiInputKeyUpEx(pContext, iKey, iModifiers, NULL);
}

XUI_API int xuiInputTextEx(xui_context pContext, uint32_t iCodepoint, uint32_t* pResult)
{
	xui_event_t tEvent;
	int iFlags;
	int iRet;

	if ( pResult != NULL ) {
		*pResult = 0u;
	}
	if ( !xuiInternalContextIsValid(pContext) || !__xuiInputCodepointValid(iCodepoint) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiInputInitEvent(&tEvent, XUI_EVENT_TEXT, pContext->pFocusWidget, NULL, pContext);
	tEvent.iCodepoint = iCodepoint;
	iFlags = 0;
	iRet = __xuiInputDispatchEventWithFlags(pContext, &tEvent, &iFlags);
	if ( iRet != XUI_OK ) return iRet;
	if ( (iFlags & XUI_EVENT_DISPATCH_STOP) != 0 && pResult != NULL ) {
		*pResult |= XUI_INPUT_RESULT_CONSUMED;
	}
	return XUI_OK;
}

XUI_API int xuiInputText(xui_context pContext, uint32_t iCodepoint)
{
	return xuiInputTextEx(pContext, iCodepoint, NULL);
}

static int __xuiInputWidgetImeEnabled(xui_widget pWidget)
{
	if ( (pWidget == NULL) || !pWidget->bVisible || !pWidget->bEnabled ) {
		return 0;
	}
	return (pWidget->iImeMode == XUI_IME_ENABLED) ||
	       ((pWidget->iImeMode == XUI_IME_AUTO) && pWidget->bFocusable);
}

XUI_API int xuiInputImeComposition(xui_context pContext, const char* sText, int iTextSize, int iCompositionStart, int iCompositionLength)
{
	xui_event_t tEvent;
	xui_widget pTarget;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ||
	     (sText == NULL) ||
	     (iTextSize < -1) ||
	     (iCompositionStart < 0) ||
	     (iCompositionLength < 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( iTextSize < 0 ) iTextSize = (int)strlen(sText);
	if ( iTextSize >= (int)sizeof(tEvent.sText) ) return XUI_ERROR_BUFFER_TOO_SMALL;
	pTarget = pContext->pFocusWidget;
	if ( !__xuiInputWidgetImeEnabled(pTarget) ) {
		return XUI_OK;
	}
	iRet = xuiInternalInputRefreshImePosition(pContext);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	__xuiInputInitEvent(&tEvent, XUI_EVENT_IME_COMPOSITION, pTarget, NULL, pContext);
	tEvent.iTextSize = __xuiInputTextCopy(tEvent.sText, (int)sizeof(tEvent.sText), sText, iTextSize);
	tEvent.iCompositionStart = iCompositionStart;
	tEvent.iCompositionLength = iCompositionLength;
	return __xuiInputPushEvent(pContext, &tEvent);
}

XUI_API int xuiInputViewport(xui_context pContext, float fWidth, float fHeight)
{
	xui_event_t tEvent;
	float fOldWidth;
	float fOldHeight;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ||
	     !__xuiInputPositiveFloatValid(fWidth) ||
	     !__xuiInputPositiveFloatValid(fHeight) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	fOldWidth = pContext->fViewportWidth;
	fOldHeight = pContext->fViewportHeight;
	iRet = xuiSetViewportSize(pContext, fWidth, fHeight);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( (fOldWidth == fWidth) && (fOldHeight == fHeight) ) {
		return XUI_OK;
	}
	__xuiInputInitEvent(&tEvent, XUI_EVENT_VIEWPORT, NULL, NULL, pContext);
	tEvent.fX = fWidth;
	tEvent.fY = fHeight;
	return __xuiInputPushEvent(pContext, &tEvent);
}

XUI_API int xuiInputDpi(xui_context pContext, float fDpiScale)
{
	xui_event_t tEvent;
	float fOldDpiScale;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) || !__xuiInputDpiValid(fDpiScale) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	fOldDpiScale = pContext->fDpiScale;
	iRet = xuiSetVirtualDpi(pContext, fDpiScale);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( fOldDpiScale == fDpiScale ) {
		return XUI_OK;
	}
	__xuiInputInitEvent(&tEvent, XUI_EVENT_DPI, NULL, NULL, pContext);
	tEvent.fX = fDpiScale;
	return __xuiInputPushEvent(pContext, &tEvent);
}

XUI_API int xuiPollEvent(xui_context pContext, xui_event_t* pEvent)
{
	if ( !xuiInternalContextIsValid(pContext) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(pEvent, 0, sizeof(*pEvent));
	if ( pContext->iEventRead >= pContext->iEventCount ) {
		pContext->iEventRead = 0;
		pContext->iEventCount = 0;
		return 0;
	}
	*pEvent = pContext->pEvents[pContext->iEventRead++];
	if ( pContext->pEvents[pContext->iEventRead - 1].sCommand == pContext->pEvents[pContext->iEventRead - 1].sText ) {
		pEvent->sCommand = pEvent->sText;
	}
	if ( pContext->iEventRead >= pContext->iEventCount ) {
		pContext->iEventRead = 0;
		pContext->iEventCount = 0;
	}
	return 1;
}

XUI_API void xuiClearEvents(xui_context pContext)
{
	if ( !xuiInternalContextIsValid(pContext) ) {
		return;
	}
	pContext->iEventCount = 0;
	pContext->iEventRead = 0;
}

static int __xuiInputDispatchEventWithFlags(xui_context pContext, const xui_event_t* pEvent, int* pFlags)
{
	xui_event_t tEvent;
	xui_widget arrInlinePath[64];
	xui_widget* pPath;
	xui_widget pScan;
	xui_widget pTarget;
	xui_pointer_state_t* pPointerState;
	int bPointerEvent;
	int iCount;
	int i;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ||
	     (pEvent == NULL) ||
	     ((pEvent->iSize != 0) && (pEvent->iSize < sizeof(*pEvent))) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pFlags != NULL ) *pFlags = 0;
	tEvent = *pEvent;
	tEvent.iSize = sizeof(tEvent);
	pTarget = tEvent.pTarget;
	if ( (pTarget == NULL) &&
	     ((tEvent.iType == XUI_EVENT_VIEWPORT) || (tEvent.iType == XUI_EVENT_DPI)) ) {
		pTarget = pContext->pRoot;
		tEvent.pTarget = pTarget;
	}
	if ( pTarget == NULL ) {
		return XUI_OK;
	}
	if ( !xuiInternalWidgetIsValid(pTarget) || (pTarget->pContext != pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pPointerState = NULL;
	bPointerEvent = __xuiInputEventUsesPointerState(&tEvent);
	if ( bPointerEvent ) {
		pPointerState = __xuiInputPointerStateFind(pContext, tEvent.iPointerId, tEvent.iPointerType, 1);
		if ( pPointerState == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		__xuiInputPointerStateLoad(pContext, pPointerState);
		pContext->iInputDispatchDepth++;
	}
	iCount = 0;
	for ( pScan = pTarget; pScan != NULL; pScan = pScan->pParent ) {
		iCount++;
	}
	if ( iCount <= 0 ) {
		if ( bPointerEvent ) {
			pContext->iInputDispatchDepth--;
			__xuiInputPointerStateStore(pContext, pPointerState);
		}
		return XUI_OK;
	}
	pPath = arrInlinePath;
	if ( iCount > (int)(sizeof(arrInlinePath) / sizeof(arrInlinePath[0])) ) {
		pPath = (xui_widget*)xrtMalloc(sizeof(*pPath) * (size_t)iCount);
		if ( pPath == NULL ) {
			if ( bPointerEvent ) {
				pContext->iInputDispatchDepth--;
				__xuiInputPointerStateStore(pContext, pPointerState);
			}
			return XUI_ERROR_OUT_OF_MEMORY;
		}
	}
	i = 0;
	for ( pScan = pTarget; pScan != NULL; pScan = pScan->pParent ) {
		pPath[i++] = pScan;
	}
	iRet = __xuiInputDispatchPath(&tEvent, pPath, iCount, pFlags);
	if ( bPointerEvent ) {
		pContext->iInputDispatchDepth--;
		__xuiInputPointerStateStore(pContext, pPointerState);
	}
	if ( pPath != arrInlinePath ) {
		xrtFree(pPath);
	}
	return iRet;
}

XUI_API int xuiDispatchEvent(xui_context pContext, const xui_event_t* pEvent)
{
	return __xuiInputDispatchEventWithFlags(pContext, pEvent, NULL);
}

XUI_API int xuiDispatchPendingEvents(xui_context pContext)
{
	xui_event_t tEvent;
	int iPoll;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	for (;;) {
		iPoll = xuiPollEvent(pContext, &tEvent);
		if ( iPoll < 0 ) {
			return iPoll;
		}
		if ( iPoll == 0 ) {
			return XUI_OK;
		}
		iRet = xuiDispatchEvent(pContext, &tEvent);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
}

XUI_API xui_widget xuiHitTest(xui_context pContext, float fX, float fY, uint32_t iFlags)
{
	xui_widget pHit;

	if ( !xuiInternalContextIsValid(pContext) || !__xuiInputFloatValid(fX) || !__xuiInputFloatValid(fY) ) {
		return NULL;
	}
	if ( (pContext->pRoot == NULL) && (pContext->pOverlayRoot == NULL) ) {
		return NULL;
	}
	if ( iFlags == 0 ) {
		iFlags = XUI_WIDGET_HIT_DEFAULT;
	}
	if ( xuiLayout(pContext) != XUI_OK ) {
		return NULL;
	}
	pHit = __xuiInputHitTestWidget(pContext->pOverlayRoot, fX, fY, iFlags);
	if ( (pHit != NULL) && (pHit != pContext->pOverlayRoot) ) {
		return pHit;
	}
	return __xuiInputHitTestWidget(pContext->pRoot, fX, fY, iFlags);
}

XUI_API xui_widget xuiGetHoverWidget(xui_context pContext)
{
	return xuiInternalContextIsValid(pContext) ? pContext->pHoverWidget : NULL;
}

XUI_API xui_widget xuiGetActiveWidget(xui_context pContext)
{
	return xuiInternalContextIsValid(pContext) ? pContext->pActiveWidget : NULL;
}

XUI_API xui_widget xuiGetFocusWidget(xui_context pContext)
{
	return xuiInternalContextIsValid(pContext) ? pContext->pFocusWidget : NULL;
}

static int __xuiInputSetPointerCaptureCurrent(xui_context pContext, xui_widget pWidget)
{
	xui_widget pOldWidget;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pWidget != NULL ) {
		if ( !xuiInternalWidgetIsValid(pWidget) ||
		     (pWidget->pContext != pContext) ||
		     !pWidget->bVisible ||
		     !pWidget->bEnabled ) {
			return XUI_ERROR_INVALID_ARGUMENT;
		}
	}
	pOldWidget = pContext->pPointerCaptureWidget;
	if ( pOldWidget == pWidget ) {
		return XUI_OK;
	}
	pContext->pPointerCaptureWidget = pWidget;
	if ( pWidget != NULL ) {
		xuiInternalTooltipCancel(pContext);
	}
	if ( pOldWidget != NULL ) {
		if ( pContext->pDragWidget == pOldWidget ) {
			iRet = __xuiInputDragCancel(pContext);
			if ( iRet != XUI_OK ) {
				return iRet;
			}
		}
		iRet = __xuiInputPushPointerEvent(pContext, XUI_EVENT_POINTER_CAPTURE_LOST, pOldWidget, pWidget, 0, 0.0f, 0.0f);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return XUI_OK;
}

XUI_API int xuiSetPointerCaptureEx(xui_context pContext, uint64_t iPointerId, int iPointerType, xui_widget pWidget)
{
	xui_pointer_state_t* pState;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pState = __xuiInputPointerStateFind(pContext, iPointerId, iPointerType, 1);
	if ( pState == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	__xuiInputPointerStateLoad(pContext, pState);
	iRet = __xuiInputSetPointerCaptureCurrent(pContext, pWidget);
	__xuiInputPointerStateStore(pContext, pState);
	return iRet;
}

XUI_API int xuiSetPointerCapture(xui_context pContext, xui_widget pWidget)
{
	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return xuiSetPointerCaptureEx(pContext, pContext->iInputPointerId, pContext->iInputPointerType, pWidget);
}

XUI_API int xuiReleasePointerCaptureEx(xui_context pContext, uint64_t iPointerId, int iPointerType, xui_widget pWidget)
{
	xui_pointer_state_t* pState;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pWidget != NULL) &&
	     (!xuiInternalWidgetIsValid(pWidget) || (pWidget->pContext != pContext)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pState = __xuiInputPointerStateFind(pContext, iPointerId, iPointerType, 0);
	if ( pState == NULL ) {
		return XUI_OK;
	}
	if ( (pWidget != NULL) && (pState->pPointerCaptureWidget != pWidget) ) {
		return XUI_OK;
	}
	__xuiInputPointerStateLoad(pContext, pState);
	iRet = __xuiInputSetPointerCaptureCurrent(pContext, NULL);
	__xuiInputPointerStateStore(pContext, pState);
	return iRet;
}

XUI_API int xuiReleasePointerCapture(xui_context pContext, xui_widget pWidget)
{
	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return xuiReleasePointerCaptureEx(pContext, pContext->iInputPointerId, pContext->iInputPointerType, pWidget);
}

XUI_API xui_widget xuiGetPointerCaptureEx(xui_context pContext, uint64_t iPointerId, int iPointerType)
{
	xui_pointer_state_t* pState;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pState = __xuiInputPointerStateFind(pContext, iPointerId, iPointerType, 0);
	return (pState != NULL) ? pState->pPointerCaptureWidget : NULL;
}

XUI_API xui_widget xuiGetPointerCapture(xui_context pContext)
{
	return xuiInternalContextIsValid(pContext) ? pContext->pPointerCaptureWidget : NULL;
}

XUI_API int xuiFocusNext(xui_context pContext, int iForward)
{
	xui_widget pNext;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pContext->pRoot == NULL) && (pContext->pOverlayRoot == NULL) ) {
		return XUI_OK;
	}
	pNext = __xuiInputFocusNextInScope(pContext, iForward);
	if ( pNext == NULL ) {
		return XUI_OK;
	}
	return xuiSetFocusWidget(pContext, pNext);
}

XUI_API int xuiSetFocusWidget(xui_context pContext, xui_widget pWidget)
{
	xui_widget pOldWidget;
	xui_event_t tEvent;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pWidget != NULL ) {
		if ( !xuiInternalWidgetIsValid(pWidget) ||
		     (pWidget->pContext != pContext) ||
		     !pWidget->bVisible ||
		     !pWidget->bEnabled ||
		     !pWidget->bFocusable ) {
			return XUI_ERROR_INVALID_ARGUMENT;
		}
	}
	pOldWidget = pContext->pFocusWidget;
	if ( pOldWidget == pWidget ) {
		return XUI_OK;
	}
	if ( pOldWidget != NULL ) {
		iRet = __xuiInputSetWidgetFlag(pOldWidget, XUI_WIDGET_STATE_FOCUS, 0);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		__xuiInputInitEvent(&tEvent, XUI_EVENT_BLUR, pOldWidget, pWidget, pContext);
		iRet = __xuiInputPushEvent(pContext, &tEvent);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	pContext->pFocusWidget = pWidget;
	iRet = xuiInternalInputSyncIme(pContext);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( pWidget != NULL ) {
		iRet = __xuiInputSetWidgetFlag(pWidget, XUI_WIDGET_STATE_FOCUS, 1);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		__xuiInputInitEvent(&tEvent, XUI_EVENT_FOCUS, pWidget, pOldWidget, pContext);
		iRet = __xuiInputPushEvent(pContext, &tEvent);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return XUI_OK;
}

static int __xuiInputReserveHotkeys(xui_context pContext, int iCapacity)
{
	xui_hotkey_t* pHotkeys;

	if ( iCapacity <= pContext->iHotkeyCapacity ) {
		return XUI_OK;
	}
	if ( iCapacity < (pContext->iHotkeyCapacity * 2) ) {
		iCapacity = pContext->iHotkeyCapacity * 2;
	}
	if ( pContext->pHotkeys == pContext->arrInlineHotkeys ) {
		pHotkeys = (xui_hotkey_t*)xrtMalloc(sizeof(*pHotkeys) * (size_t)iCapacity);
		if ( pHotkeys == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		memcpy(pHotkeys, pContext->pHotkeys, sizeof(*pHotkeys) * (size_t)pContext->iHotkeyCount);
	} else {
		pHotkeys = (xui_hotkey_t*)xrtRealloc(pContext->pHotkeys, sizeof(*pHotkeys) * (size_t)iCapacity);
		if ( pHotkeys == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
	}
	pContext->pHotkeys = pHotkeys;
	pContext->iHotkeyCapacity = iCapacity;
	return XUI_OK;
}

static void __xuiInputHotkeyReset(xui_hotkey_t* pHotkey)
{
	if ( pHotkey == NULL ) {
		return;
	}
	if ( pHotkey->sCommand != NULL ) {
		xrtFree(pHotkey->sCommand);
	}
	memset(pHotkey, 0, sizeof(*pHotkey));
}

static int __xuiInputHotkeyFind(xui_context pContext, xui_widget pWidget, int iKey, uint32_t iModifiers)
{
	int i;

	for ( i = 0; i < pContext->iHotkeyCount; i++ ) {
		if ( (pContext->pHotkeys[i].pWidget == pWidget) &&
		     (pContext->pHotkeys[i].iKey == iKey) &&
		     (pContext->pHotkeys[i].iModifiers == iModifiers) ) {
			return i;
		}
	}
	return -1;
}

static int __xuiInputHotkeySet(xui_context pContext, xui_widget pWidget, int iKey, uint32_t iModifiers, xui_widget_event_proc onEvent, void* pUser, int iCommand, const char* sCommand, void* pData)
{
	xui_hotkey_t* pHotkey;
	char* sCopy;
	int iIndex;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ||
	     !xuiInternalWidgetIsValid(pWidget) ||
	     (pWidget->pContext != pContext) ||
	     (iKey < 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	sCopy = NULL;
	if ( (sCommand != NULL) && (sCommand[0] != '\0') ) {
		sCopy = __xuiInputStringDuplicate(sCommand);
		if ( sCopy == NULL ) {
			return XUI_ERROR_OUT_OF_MEMORY;
		}
	}
	iIndex = __xuiInputHotkeyFind(pContext, pWidget, iKey, iModifiers);
	if ( iIndex < 0 ) {
		iRet = __xuiInputReserveHotkeys(pContext, pContext->iHotkeyCount + 1);
		if ( iRet != XUI_OK ) {
			if ( sCopy != NULL ) {
				xrtFree(sCopy);
			}
			return iRet;
		}
		iIndex = pContext->iHotkeyCount++;
		memset(&pContext->pHotkeys[iIndex], 0, sizeof(pContext->pHotkeys[iIndex]));
	} else if ( pContext->pHotkeys[iIndex].sCommand != NULL ) {
		xrtFree(pContext->pHotkeys[iIndex].sCommand);
	}
	pHotkey = &pContext->pHotkeys[iIndex];
	pHotkey->pWidget = pWidget;
	pHotkey->iKey = iKey;
	pHotkey->iModifiers = iModifiers;
	pHotkey->onEvent = onEvent;
	pHotkey->pUser = pUser;
	pHotkey->iCommand = iCommand;
	pHotkey->sCommand = sCopy;
	pHotkey->pData = pData;
	if ( sCopy != NULL ) {
		(void)xuiWidgetSetEventInterest(pWidget, XUI_EVENT_MASK_COMMAND, 1);
	} else {
		(void)xuiWidgetSetEventInterest(pWidget, XUI_EVENT_MASK_HOTKEY, 1);
	}
	return XUI_OK;
}

XUI_API int xuiHotKeyRegister(xui_context pContext, xui_widget pWidget, int iKey, uint32_t iModifiers, xui_widget_event_proc onEvent, void* pUser)
{
	return __xuiInputHotkeySet(pContext, pWidget, iKey, iModifiers, onEvent, pUser, 0, NULL, NULL);
}

XUI_API int xuiHotKeyRegisterCommand(xui_context pContext, xui_widget pWidget, int iKey, uint32_t iModifiers, int iCommand, const char* sCommand, void* pData)
{
	return __xuiInputHotkeySet(pContext, pWidget, iKey, iModifiers, NULL, NULL, iCommand, sCommand, pData);
}

XUI_API int xuiHotKeyUnregister(xui_context pContext, xui_widget pWidget, int iKey, uint32_t iModifiers)
{
	int iIndex;
	int iMoveCount;

	if ( !xuiInternalContextIsValid(pContext) ||
	     !xuiInternalWidgetIsValid(pWidget) ||
	     (pWidget->pContext != pContext) ||
	     (iKey < 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iIndex = __xuiInputHotkeyFind(pContext, pWidget, iKey, iModifiers);
	if ( iIndex < 0 ) {
		return XUI_OK;
	}
	__xuiInputHotkeyReset(&pContext->pHotkeys[iIndex]);
	iMoveCount = pContext->iHotkeyCount - iIndex - 1;
	if ( iMoveCount > 0 ) {
		memmove(&pContext->pHotkeys[iIndex], &pContext->pHotkeys[iIndex + 1], sizeof(pContext->pHotkeys[0]) * (size_t)iMoveCount);
	}
	pContext->iHotkeyCount--;
	return XUI_OK;
}

XUI_API int xuiHotKeyClearWidget(xui_widget pWidget)
{
	xui_context pContext;
	int i;

	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pContext = pWidget->pContext;
	if ( !xuiInternalContextIsValid(pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	i = 0;
	while ( i < pContext->iHotkeyCount ) {
		if ( pContext->pHotkeys[i].pWidget == pWidget ) {
			(void)xuiHotKeyUnregister(pContext, pWidget, pContext->pHotkeys[i].iKey, pContext->pHotkeys[i].iModifiers);
		} else {
			i++;
		}
	}
	return XUI_OK;
}

XUI_API int xuiCommandDispatch(xui_context pContext, xui_widget pTarget, int iCommand, const char* sCommand, void* pData)
{
	xui_event_t tEvent;

	if ( !xuiInternalContextIsValid(pContext) ||
	     (pTarget != NULL && (!xuiInternalWidgetIsValid(pTarget) || (pTarget->pContext != pContext))) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiInputInitEvent(&tEvent, XUI_EVENT_COMMAND, pTarget, NULL, pContext);
	tEvent.iCommand = iCommand;
	tEvent.pData = pData;
	if ( sCommand != NULL ) {
		tEvent.iTextSize = __xuiInputTextCopy(tEvent.sText, (int)sizeof(tEvent.sText), sCommand, -1);
		tEvent.sCommand = tEvent.sText;
	}
	return __xuiInputPushEvent(pContext, &tEvent);
}

XUI_API xui_widget xuiDebugWidgetInspectAt(xui_context pContext, float fX, float fY, xui_debug_widget_info_t* pInfo)
{
	xui_widget pWidget;

	pWidget = xuiHitTest(pContext, fX, fY, XUI_WIDGET_HIT_DEFAULT);
	if ( (pWidget != NULL) && (pInfo != NULL) ) {
		(void)xuiDebugWidgetInspect(pWidget, pInfo);
	}
	return pWidget;
}
