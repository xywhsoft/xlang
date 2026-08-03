#include "xui_internal.h"

#include <string.h>

typedef struct xui_check_card_data_t {
	xui_check_card_change_proc onChange;
	void* pChangeUser;
	xui_widget pRadioGroup;
	int bChecked;
	int bKeyboardActive;
	int iChangeCount;
	float fBorderWidth;
	float fCheckedBorderWidth;
	float fCornerSize;
	float fFocusWidth;
	uint32_t iBackgroundColor;
	uint32_t iHoverBackgroundColor;
	uint32_t iActiveBackgroundColor;
	uint32_t iCheckedBackgroundColor;
	uint32_t iBorderColor;
	uint32_t iHoverBorderColor;
	uint32_t iCheckedBorderColor;
	uint32_t iDisabledBorderColor;
	uint32_t iCornerColor;
	uint32_t iCheckColor;
	uint32_t iFocusColor;
	xui_rect_t tCornerRect;
} xui_check_card_data_t;

static int __xuiCheckCardDescValid(const xui_check_card_desc_t* pDesc)
{
	if ( pDesc == NULL ) return 1;
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) return 0;
	if ( (pDesc->fBorderWidth < 0.0f) ||
	     (pDesc->fCheckedBorderWidth < 0.0f) || (pDesc->fCornerSize < 0.0f) ||
	     (pDesc->fFocusWidth < 0.0f) || (pDesc->fMinWidth < 0.0f) || (pDesc->fMinHeight < 0.0f) ) return 0;
	return 1;
}

static int __xuiCheckCardAlpha(uint32_t iColor)
{
	return (int)(iColor & 0xffu);
}

static uint32_t __xuiCheckCardWithAlpha(uint32_t iColor, uint32_t iAlpha)
{
	return (iColor & 0xffffff00u) | (iAlpha & 0xffu);
}

static xui_check_card_data_t* __xuiCheckCardGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) return NULL;
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "checkcard");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) return NULL;
	return (xui_check_card_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiCheckCardStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
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

static int __xuiCheckCardStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
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

static void __xuiCheckCardResolve(xui_widget pWidget, const xui_check_card_data_t* pData, xui_check_card_data_t* pResolved)
{
	*pResolved = *pData;
	(void)__xuiCheckCardStyleColor(pWidget, "checkcard.background.color", &pResolved->iBackgroundColor);
	(void)__xuiCheckCardStyleColor(pWidget, "checkcard.background.hover_color", &pResolved->iHoverBackgroundColor);
	(void)__xuiCheckCardStyleColor(pWidget, "checkcard.background.active_color", &pResolved->iActiveBackgroundColor);
	(void)__xuiCheckCardStyleColor(pWidget, "checkcard.background.checked_color", &pResolved->iCheckedBackgroundColor);
	(void)__xuiCheckCardStyleColor(pWidget, "checkcard.border.color", &pResolved->iBorderColor);
	(void)__xuiCheckCardStyleColor(pWidget, "checkcard.border.hover_color", &pResolved->iHoverBorderColor);
	(void)__xuiCheckCardStyleColor(pWidget, "checkcard.border.checked_color", &pResolved->iCheckedBorderColor);
	(void)__xuiCheckCardStyleColor(pWidget, "checkcard.border.disabled_color", &pResolved->iDisabledBorderColor);
	(void)__xuiCheckCardStyleColor(pWidget, "checkcard.corner.color", &pResolved->iCornerColor);
	(void)__xuiCheckCardStyleColor(pWidget, "checkcard.check.color", &pResolved->iCheckColor);
	(void)__xuiCheckCardStyleColor(pWidget, "checkcard.focus.color", &pResolved->iFocusColor);
	(void)__xuiCheckCardStyleFloat(pWidget, "checkcard.border.width", &pResolved->fBorderWidth);
	(void)__xuiCheckCardStyleFloat(pWidget, "checkcard.border.checked_width", &pResolved->fCheckedBorderWidth);
	(void)__xuiCheckCardStyleFloat(pWidget, "checkcard.corner.size", &pResolved->fCornerSize);
	(void)__xuiCheckCardStyleFloat(pWidget, "checkcard.focus.width", &pResolved->fFocusWidth);
}

static uint32_t __xuiCheckCardComputeState(xui_widget pWidget, const xui_check_card_data_t* pData)
{
	uint32_t iState;

	iState = xuiWidgetGetInputState(pWidget);
	if ( !xuiWidgetGetEnabled(pWidget) ) iState |= XUI_WIDGET_STATE_DISABLED;
	if ( (pData != NULL) && pData->bChecked ) iState |= XUI_CHECKCARD_STATE_CHECKED;
	if ( (pData != NULL) && pData->bKeyboardActive ) iState |= XUI_WIDGET_STATE_ACTIVE;
	return iState;
}

static uint32_t __xuiCheckCardStateId(uint32_t iState)
{
	if ( ((iState & XUI_WIDGET_STATE_DISABLED) != 0) && ((iState & XUI_CHECKCARD_STATE_CHECKED) != 0) ) return XUI_WIDGET_STATE_DISABLED | XUI_CHECKCARD_STATE_CHECKED;
	if ( (iState & XUI_WIDGET_STATE_DISABLED) != 0 ) return XUI_WIDGET_STATE_DISABLED;
	if ( ((iState & XUI_CHECKCARD_STATE_CHECKED) != 0) && ((iState & XUI_WIDGET_STATE_ACTIVE) != 0) ) return XUI_CHECKCARD_STATE_CHECKED | XUI_WIDGET_STATE_ACTIVE;
	if ( ((iState & XUI_CHECKCARD_STATE_CHECKED) != 0) && ((iState & XUI_WIDGET_STATE_HOVER) != 0) ) return XUI_CHECKCARD_STATE_CHECKED | XUI_WIDGET_STATE_HOVER;
	if ( ((iState & XUI_CHECKCARD_STATE_CHECKED) != 0) && ((iState & XUI_WIDGET_STATE_FOCUS) != 0) ) return XUI_CHECKCARD_STATE_CHECKED | XUI_WIDGET_STATE_FOCUS;
	if ( (iState & XUI_WIDGET_STATE_ACTIVE) != 0 ) return XUI_WIDGET_STATE_ACTIVE;
	if ( (iState & XUI_WIDGET_STATE_HOVER) != 0 ) return XUI_WIDGET_STATE_HOVER;
	if ( (iState & XUI_WIDGET_STATE_FOCUS) != 0 ) return XUI_WIDGET_STATE_FOCUS;
	if ( (iState & XUI_CHECKCARD_STATE_CHECKED) != 0 ) return XUI_CHECKCARD_STATE_CHECKED;
	return 0;
}

static int __xuiCheckCardSyncState(xui_widget pWidget, xui_check_card_data_t* pData)
{
	return xuiWidgetSetStateId(pWidget, __xuiCheckCardStateId(__xuiCheckCardComputeState(pWidget, pData)));
}

static void __xuiCheckCardNotify(xui_widget pWidget, xui_check_card_data_t* pData)
{
	pData->iChangeCount++;
	if ( pData->onChange != NULL ) pData->onChange(pWidget, pData->bChecked, pData->pChangeUser);
}

static int __xuiCheckCardSetCheckedInternal(xui_widget pWidget, xui_check_card_data_t* pData, int bChecked, int bNotify, int bFromGroup)
{
	bChecked = (bChecked != 0);
	if ( !bFromGroup && (pData->pRadioGroup != NULL) ) {
		if ( bChecked ) return xuiRadioGroupSetSelectedWidget(pData->pRadioGroup, pWidget);
		if ( xuiRadioGroupGetSelectedWidget(pData->pRadioGroup) == pWidget ) return xuiRadioGroupSetSelectedWidget(pData->pRadioGroup, NULL);
	}
	if ( pData->bChecked == bChecked ) return XUI_OK;
	pData->bChecked = bChecked;
	(void)__xuiCheckCardSyncState(pWidget, pData);
	if ( bNotify ) __xuiCheckCardNotify(pWidget, pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static void __xuiCheckCardDefaultAction(xui_widget pWidget, void* pUser)
{
	xui_check_card_data_t* pData;

	(void)pUser;
	pData = __xuiCheckCardGetData(pWidget);
	if ( pData == NULL || !xuiWidgetGetEnabled(pWidget) ) return;
	if ( pData->pRadioGroup != NULL ) {
		(void)__xuiCheckCardSetCheckedInternal(pWidget, pData, 1, 1, 0);
	} else {
		(void)__xuiCheckCardSetCheckedInternal(pWidget, pData, !pData->bChecked, 1, 0);
	}
}

static int __xuiCheckCardEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_check_card_data_t* pData;

	(void)pUser;
	if ( (pWidget == NULL) || (pEvent == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData = __xuiCheckCardGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_ENTER:
	case XUI_EVENT_POINTER_LEAVE:
	case XUI_EVENT_POINTER_MOVE:
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
	case XUI_EVENT_ENABLED_CHANGED:
	case XUI_EVENT_VISIBLE_CHANGED:
		(void)__xuiCheckCardSyncState(pWidget, pData);
		return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	case XUI_EVENT_POINTER_DOWN:
		if ( pEvent->iButton == XUI_POINTER_BUTTON_LEFT && xuiWidgetGetEnabled(pWidget) ) {
			(void)xuiSetPointerCapture(xuiWidgetGetContext(pWidget), pWidget);
			(void)__xuiCheckCardSyncState(pWidget, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_UP:
		if ( pEvent->iButton == XUI_POINTER_BUTTON_LEFT ) {
			if ( xuiGetPointerCapture(xuiWidgetGetContext(pWidget)) == pWidget ) (void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_CLICK:
		if ( pEvent->iButton == XUI_POINTER_BUTTON_LEFT ) {
			__xuiCheckCardDefaultAction(pWidget, NULL);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		return __xuiCheckCardSyncState(pWidget, pData);
	case XUI_EVENT_KEY_DOWN:
		if ( (pEvent->iKey == XUI_KEY_SPACE) || (pEvent->iKey == XUI_KEY_ENTER) ) {
			pData->bKeyboardActive = 1;
			(void)__xuiCheckCardSyncState(pWidget, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_KEY_UP:
		if ( ((pEvent->iKey == XUI_KEY_SPACE) || (pEvent->iKey == XUI_KEY_ENTER)) && pData->bKeyboardActive ) {
			pData->bKeyboardActive = 0;
			__xuiCheckCardDefaultAction(pWidget, NULL);
			(void)__xuiCheckCardSyncState(pWidget, pData);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiCheckCardDrawRectFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( __xuiCheckCardAlpha(iColor) == 0 ) return XUI_OK;
	return pProxy->drawRectFill(pProxy, pDraw, tRect, iColor);
}

static int __xuiCheckCardDrawRectStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	if ( fWidth <= 0.0f || __xuiCheckCardAlpha(iColor) == 0 ) return XUI_OK;
	return pProxy->drawRectStroke(pProxy, pDraw, tRect, fWidth, iColor);
}

static int __xuiCheckCardCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_check_card_data_t* pData;
	xui_check_card_data_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tRect;
	xui_vec2_t tA;
	xui_vec2_t tB;
	xui_vec2_t tC;
	uint32_t iBg;
	uint32_t iBorder;
	float fBorderWidth;
	float fCorner;
	float fCornerInset;
	float fCornerRight;
	float fCornerTop;
	int bChecked;
	int iRet;

	(void)pUser;
	pData = __xuiCheckCardGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	__xuiCheckCardResolve(pWidget, pData, &tResolved);
	tRect = xuiWidgetGetContentRect(pWidget);
	bChecked = ((iStateId & XUI_CHECKCARD_STATE_CHECKED) != 0);
	iBg = bChecked ? tResolved.iCheckedBackgroundColor : tResolved.iBackgroundColor;
	if ( (iStateId & XUI_WIDGET_STATE_ACTIVE) != 0 ) iBg = tResolved.iActiveBackgroundColor;
	else if ( (iStateId & XUI_WIDGET_STATE_HOVER) != 0 ) iBg = tResolved.iHoverBackgroundColor;
	iBorder = bChecked ? tResolved.iCheckedBorderColor : tResolved.iBorderColor;
	if ( (iStateId & XUI_WIDGET_STATE_DISABLED) != 0 ) iBorder = tResolved.iDisabledBorderColor;
	else if ( !bChecked && ((iStateId & XUI_WIDGET_STATE_HOVER) != 0) ) iBorder = tResolved.iHoverBorderColor;
	fBorderWidth = bChecked ? tResolved.fCheckedBorderWidth : tResolved.fBorderWidth;
	iRet = __xuiCheckCardDrawRectFill(pProxy, pDraw, tRect, iBg);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiCheckCardDrawRectStroke(pProxy, pDraw, xuiInternalInsetRect(tRect, fBorderWidth * 0.5f), fBorderWidth, iBorder);
	if ( iRet != XUI_OK ) return iRet;
	fCornerInset = (fBorderWidth > 0.0f) ? fBorderWidth : 0.0f;
	pData->tCornerRect = (xui_rect_t){tRect.fX + tRect.fW - tResolved.fCornerSize - fCornerInset, tRect.fY + fCornerInset, tResolved.fCornerSize, tResolved.fCornerSize};
	if ( bChecked && tResolved.fCornerSize > 0.0f && pProxy->drawTriangleFill != NULL ) {
		fCorner = tResolved.fCornerSize;
		fCornerRight = tRect.fX + tRect.fW - fCornerInset;
		fCornerTop = tRect.fY + fCornerInset;
		tA = (xui_vec2_t){fCornerRight - fCorner, fCornerTop};
		tB = (xui_vec2_t){fCornerRight, fCornerTop};
		tC = (xui_vec2_t){fCornerRight, fCornerTop + fCorner};
		iRet = pProxy->drawTriangleFill(pProxy, pDraw, tA, tB, tC, tResolved.iCornerColor);
		if ( iRet != XUI_OK ) return iRet;
		if ( pProxy->drawLine != NULL ) {
			iRet = pProxy->drawLine(pProxy, pDraw, fCornerRight - fCorner * 0.40f, fCornerTop + fCorner * 0.32f, fCornerRight - fCorner * 0.28f, fCornerTop + fCorner * 0.46f, 1.7f, tResolved.iCheckColor);
			if ( iRet == XUI_OK ) iRet = pProxy->drawLine(pProxy, pDraw, fCornerRight - fCorner * 0.28f, fCornerTop + fCorner * 0.46f, fCornerRight - fCorner * 0.12f, fCornerTop + fCorner * 0.24f, 1.7f, tResolved.iCheckColor);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	if ( ((iStateId & XUI_WIDGET_STATE_FOCUS) != 0) && ((iStateId & XUI_WIDGET_STATE_DISABLED) == 0) ) {
		iRet = __xuiCheckCardDrawRectStroke(pProxy, pDraw, xuiInternalInsetRect(tRect, 2.0f), tResolved.fFocusWidth, __xuiCheckCardWithAlpha(tResolved.iFocusColor, 150));
	}
	return iRet;
}

static void __xuiCheckCardDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_COLUMN;
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

static void __xuiCheckCardDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE | XUI_CACHE_UPDATE_ALL_STATES;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiCheckCardInitCacheStates(xui_widget pWidget)
{
	static const uint32_t arrState[] = {
		0, XUI_WIDGET_STATE_HOVER, XUI_WIDGET_STATE_ACTIVE, XUI_WIDGET_STATE_FOCUS, XUI_WIDGET_STATE_DISABLED,
		XUI_CHECKCARD_STATE_CHECKED, XUI_CHECKCARD_STATE_CHECKED | XUI_WIDGET_STATE_HOVER,
		XUI_CHECKCARD_STATE_CHECKED | XUI_WIDGET_STATE_ACTIVE, XUI_CHECKCARD_STATE_CHECKED | XUI_WIDGET_STATE_FOCUS,
		XUI_CHECKCARD_STATE_CHECKED | XUI_WIDGET_STATE_DISABLED
	};
	int i;
	int iRet;

	iRet = xuiWidgetSetCacheStateCount(pWidget, (int)(sizeof(arrState) / sizeof(arrState[0])));
	if ( iRet != XUI_OK ) return iRet;
	for ( i = 0; i < (int)(sizeof(arrState) / sizeof(arrState[0])); i++ ) {
		iRet = xuiWidgetSetCacheStateId(pWidget, i, 0x7c000000u + (uint32_t)i);
		if ( iRet != XUI_OK ) return iRet;
	}
	for ( i = 0; i < (int)(sizeof(arrState) / sizeof(arrState[0])); i++ ) {
		iRet = xuiWidgetSetCacheStateId(pWidget, i, arrState[i]);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiCheckCardInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_check_card_data_t* pData;
	const xui_check_card_desc_t* pDesc;
	xui_thickness_t tPadding;
	int iRet;

	(void)pUser;
	pData = (xui_check_card_data_t*)pTypeData;
	pDesc = (const xui_check_card_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiCheckCardDescValid(pDesc) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(pData, 0, sizeof(*pData));
	pData->bChecked = (pDesc != NULL) ? (pDesc->bChecked != 0) : 0;
	pData->fBorderWidth = (pDesc != NULL && pDesc->fBorderWidth > 0.0f) ? pDesc->fBorderWidth : 1.0f;
	pData->fCheckedBorderWidth = (pDesc != NULL && pDesc->fCheckedBorderWidth > 0.0f) ? pDesc->fCheckedBorderWidth : 1.0f;
	pData->fCornerSize = (pDesc != NULL && pDesc->fCornerSize > 0.0f) ? pDesc->fCornerSize : 16.0f;
	pData->fFocusWidth = (pDesc != NULL && pDesc->fFocusWidth > 0.0f) ? pDesc->fFocusWidth : 0.0f;
	pData->iBackgroundColor = (pDesc != NULL && pDesc->iBackgroundColor != 0) ? pDesc->iBackgroundColor : XUI_COLOR_RGBA(255, 255, 255, 255);
	pData->iHoverBackgroundColor = (pDesc != NULL && pDesc->iHoverBackgroundColor != 0) ? pDesc->iHoverBackgroundColor : XUI_COLOR_RGBA(250, 253, 253, 255);
	pData->iActiveBackgroundColor = (pDesc != NULL && pDesc->iActiveBackgroundColor != 0) ? pDesc->iActiveBackgroundColor : XUI_COLOR_RGBA(244, 250, 250, 255);
	pData->iCheckedBackgroundColor = (pDesc != NULL && pDesc->iCheckedBackgroundColor != 0) ? pDesc->iCheckedBackgroundColor : XUI_COLOR_RGBA(255, 255, 255, 255);
	pData->iBorderColor = (pDesc != NULL && pDesc->iBorderColor != 0) ? pDesc->iBorderColor : XUI_COLOR_RGBA(222, 226, 230, 255);
	pData->iHoverBorderColor = (pDesc != NULL && pDesc->iHoverBorderColor != 0) ? pDesc->iHoverBorderColor : XUI_COLOR_RGBA(126, 211, 205, 255);
	pData->iCheckedBorderColor = (pDesc != NULL && pDesc->iCheckedBorderColor != 0) ? pDesc->iCheckedBorderColor : XUI_COLOR_RGBA(29, 196, 186, 255);
	pData->iDisabledBorderColor = (pDesc != NULL && pDesc->iDisabledBorderColor != 0) ? pDesc->iDisabledBorderColor : XUI_COLOR_RGBA(214, 219, 225, 160);
	pData->iCornerColor = (pDesc != NULL && pDesc->iCornerColor != 0) ? pDesc->iCornerColor : pData->iCheckedBorderColor;
	pData->iCheckColor = (pDesc != NULL && pDesc->iCheckColor != 0) ? pDesc->iCheckColor : XUI_COLOR_RGBA(255, 255, 255, 255);
	pData->iFocusColor = (pDesc != NULL && pDesc->iFocusColor != 0) ? pDesc->iFocusColor : XUI_COLOR_RGBA(29, 196, 186, 180);
	tPadding = (xui_thickness_t){16.0f, 14.0f, 16.0f, 14.0f};
	if ( pDesc != NULL && (pDesc->tPadding.fLeft != 0.0f || pDesc->tPadding.fTop != 0.0f || pDesc->tPadding.fRight != 0.0f || pDesc->tPadding.fBottom != 0.0f) ) tPadding = pDesc->tPadding;
	(void)xuiWidgetSetPadding(pWidget, tPadding);
	(void)xuiWidgetSetMinSize(pWidget, (xui_vec2_t){(pDesc != NULL && pDesc->fMinWidth > 0.0f) ? pDesc->fMinWidth : 120.0f, (pDesc != NULL && pDesc->fMinHeight > 0.0f) ? pDesc->fMinHeight : 56.0f});
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 1);
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_CLIP);
	iRet = __xuiCheckCardInitCacheStates(pWidget);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetDefaultAction(pWidget, __xuiCheckCardDefaultAction, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_ENTER, __xuiCheckCardEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiCheckCardEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiCheckCardEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiCheckCardEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiCheckCardEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CLICK, __xuiCheckCardEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiCheckCardEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_FOCUS, __xuiCheckCardEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_BLUR, __xuiCheckCardEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_DOWN, __xuiCheckCardEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_KEY_UP, __xuiCheckCardEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_ENABLED_CHANGED, __xuiCheckCardEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_VISIBLE_CHANGED, __xuiCheckCardEvent, NULL);
	if ( iRet != XUI_OK ) return iRet;
	if ( (pDesc != NULL) && (pDesc->pRadioGroup != NULL) ) {
		iRet = xuiRadioGroupAddCheckCard(pDesc->pRadioGroup, pWidget);
		if ( iRet != XUI_OK ) return iRet;
	}
	return __xuiCheckCardSyncState(pWidget, pData);
}

static void __xuiCheckCardDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	(void)pWidget;
	(void)pUser;
	if ( pTypeData != NULL ) memset(pTypeData, 0, sizeof(xui_check_card_data_t));
}

static void __xuiCheckCardRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags)
{
	xui_style_property_info_t tInfo;

	if ( xuiStyleFindProperty(pContext, sName) != 0 ) return;
	memset(&tInfo, 0, sizeof(tInfo));
	tInfo.iSize = sizeof(tInfo);
	tInfo.sName = sName;
	tInfo.iValueType = iValueType;
	tInfo.iDirtyFlags = iDirtyFlags;
	tInfo.pWidgetType = pType;
	(void)xuiStyleRegisterProperty(pContext, &tInfo, NULL);
}

static void __xuiCheckCardRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiCheckCardRegisterStyleProperty(pContext, pType, "checkcard.background.color", XUI_STYLE_VALUE_COLOR, iPaintDirty);
	__xuiCheckCardRegisterStyleProperty(pContext, pType, "checkcard.background.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty);
	__xuiCheckCardRegisterStyleProperty(pContext, pType, "checkcard.background.active_color", XUI_STYLE_VALUE_COLOR, iPaintDirty);
	__xuiCheckCardRegisterStyleProperty(pContext, pType, "checkcard.background.checked_color", XUI_STYLE_VALUE_COLOR, iPaintDirty);
	__xuiCheckCardRegisterStyleProperty(pContext, pType, "checkcard.border.color", XUI_STYLE_VALUE_COLOR, iPaintDirty);
	__xuiCheckCardRegisterStyleProperty(pContext, pType, "checkcard.border.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty);
	__xuiCheckCardRegisterStyleProperty(pContext, pType, "checkcard.border.checked_color", XUI_STYLE_VALUE_COLOR, iPaintDirty);
	__xuiCheckCardRegisterStyleProperty(pContext, pType, "checkcard.border.disabled_color", XUI_STYLE_VALUE_COLOR, iPaintDirty);
	__xuiCheckCardRegisterStyleProperty(pContext, pType, "checkcard.corner.color", XUI_STYLE_VALUE_COLOR, iPaintDirty);
	__xuiCheckCardRegisterStyleProperty(pContext, pType, "checkcard.check.color", XUI_STYLE_VALUE_COLOR, iPaintDirty);
	__xuiCheckCardRegisterStyleProperty(pContext, pType, "checkcard.focus.color", XUI_STYLE_VALUE_COLOR, iPaintDirty);
	__xuiCheckCardRegisterStyleProperty(pContext, pType, "checkcard.border.width", XUI_STYLE_VALUE_FLOAT, iPaintDirty);
	__xuiCheckCardRegisterStyleProperty(pContext, pType, "checkcard.border.checked_width", XUI_STYLE_VALUE_FLOAT, iPaintDirty);
	__xuiCheckCardRegisterStyleProperty(pContext, pType, "checkcard.corner.size", XUI_STYLE_VALUE_FLOAT, iPaintDirty);
	__xuiCheckCardRegisterStyleProperty(pContext, pType, "checkcard.focus.width", XUI_STYLE_VALUE_FLOAT, iPaintDirty);
}

XUI_API xui_widget_type xuiCheckCardGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) return NULL;
	pType = xuiWidgetFindType(pContext, "checkcard");
	if ( pType != NULL ) {
		__xuiCheckCardRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "checkcard";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_check_card_data_t);
	tDesc.onInit = __xuiCheckCardInit;
	tDesc.onDestroy = __xuiCheckCardDestroy;
	tDesc.onCacheRender = __xuiCheckCardCacheRender;
	__xuiCheckCardDefaultLayout(&tDesc.tLayout);
	__xuiCheckCardDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) return NULL;
	__xuiCheckCardRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiCheckCardCreate(xui_context pContext, xui_widget* ppWidget, const xui_check_card_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiCheckCardDescValid(pDesc) ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppWidget = NULL;
	pType = xuiCheckCardGetType(pContext);
	if ( pType == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiCheckCardSetChange(xui_widget pWidget, xui_check_card_change_proc onChange, void* pUser)
{
	xui_check_card_data_t* pData = __xuiCheckCardGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onChange = onChange;
	pData->pChangeUser = pUser;
	return XUI_OK;
}

XUI_API int xuiCheckCardSetChecked(xui_widget pWidget, int bChecked)
{
	xui_check_card_data_t* pData = __xuiCheckCardGetData(pWidget);
	return (pData != NULL) ? __xuiCheckCardSetCheckedInternal(pWidget, pData, bChecked, 0, 0) : XUI_ERROR_INVALID_ARGUMENT;
}

XUI_API int xuiCheckCardSetCheckedFromGroup(xui_widget pWidget, int bChecked, int bNotify)
{
	xui_check_card_data_t* pData = __xuiCheckCardGetData(pWidget);
	return (pData != NULL) ? __xuiCheckCardSetCheckedInternal(pWidget, pData, bChecked, bNotify, 1) : XUI_ERROR_INVALID_ARGUMENT;
}

XUI_API int xuiCheckCardGetChecked(xui_widget pWidget)
{
	xui_check_card_data_t* pData = __xuiCheckCardGetData(pWidget);
	return (pData != NULL) ? pData->bChecked : 0;
}

XUI_API int xuiCheckCardSetRadioGroup(xui_widget pWidget, xui_widget pGroup)
{
	xui_check_card_data_t* pData = __xuiCheckCardGetData(pWidget);
	int iRet;

	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pGroup != NULL ) {
		if ( xuiWidgetFindType(xuiWidgetGetContext(pGroup), "radiogroup") == NULL || !xuiWidgetIsType(pGroup, xuiWidgetFindType(xuiWidgetGetContext(pGroup), "radiogroup")) ) return XUI_ERROR_INVALID_ARGUMENT;
		pData->pRadioGroup = pGroup;
		if ( xuiWidgetGetParent(pWidget) != pGroup ) {
			iRet = xuiWidgetAddChild(pGroup, pWidget);
			if ( iRet != XUI_OK ) return iRet;
		}
		if ( pData->bChecked ) return xuiRadioGroupSetSelectedWidget(pGroup, pWidget);
		return XUI_OK;
	}
	pData->pRadioGroup = NULL;
	return XUI_OK;
}

XUI_API xui_widget xuiCheckCardGetRadioGroup(xui_widget pWidget)
{
	xui_check_card_data_t* pData = __xuiCheckCardGetData(pWidget);
	return (pData != NULL) ? pData->pRadioGroup : NULL;
}

XUI_API int xuiCheckCardSetMetrics(xui_widget pWidget, float fBorderWidth, float fCheckedBorderWidth, float fCornerSize, float fFocusWidth)
{
	xui_check_card_data_t* pData = __xuiCheckCardGetData(pWidget);
	if ( (pData == NULL) || (fBorderWidth < 0.0f) || (fCheckedBorderWidth < 0.0f) || (fCornerSize < 0.0f) || (fFocusWidth < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fBorderWidth = fBorderWidth; pData->fCheckedBorderWidth = fCheckedBorderWidth; pData->fCornerSize = fCornerSize; pData->fFocusWidth = fFocusWidth;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCheckCardGetMetrics(xui_widget pWidget, float* pBorderWidth, float* pCheckedBorderWidth, float* pCornerSize, float* pFocusWidth)
{
	xui_check_card_data_t* pData = __xuiCheckCardGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pBorderWidth != NULL ) *pBorderWidth = pData->fBorderWidth;
	if ( pCheckedBorderWidth != NULL ) *pCheckedBorderWidth = pData->fCheckedBorderWidth;
	if ( pCornerSize != NULL ) *pCornerSize = pData->fCornerSize;
	if ( pFocusWidth != NULL ) *pFocusWidth = pData->fFocusWidth;
	return XUI_OK;
}

XUI_API int xuiCheckCardSetColors(xui_widget pWidget, uint32_t iBackground, uint32_t iHoverBackground, uint32_t iActiveBackground, uint32_t iCheckedBackground, uint32_t iBorder, uint32_t iHoverBorder, uint32_t iCheckedBorder, uint32_t iCorner, uint32_t iCheck)
{
	xui_check_card_data_t* pData = __xuiCheckCardGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iBackgroundColor = iBackground; pData->iHoverBackgroundColor = iHoverBackground; pData->iActiveBackgroundColor = iActiveBackground; pData->iCheckedBackgroundColor = iCheckedBackground;
	pData->iBorderColor = iBorder; pData->iHoverBorderColor = iHoverBorder; pData->iCheckedBorderColor = iCheckedBorder; pData->iCornerColor = iCorner; pData->iCheckColor = iCheck;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCheckCardGetColors(xui_widget pWidget, uint32_t* pBackground, uint32_t* pHoverBackground, uint32_t* pActiveBackground, uint32_t* pCheckedBackground, uint32_t* pBorder, uint32_t* pHoverBorder, uint32_t* pCheckedBorder, uint32_t* pCorner, uint32_t* pCheck)
{
	xui_check_card_data_t* pData = __xuiCheckCardGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pBackground != NULL ) *pBackground = pData->iBackgroundColor;
	if ( pHoverBackground != NULL ) *pHoverBackground = pData->iHoverBackgroundColor;
	if ( pActiveBackground != NULL ) *pActiveBackground = pData->iActiveBackgroundColor;
	if ( pCheckedBackground != NULL ) *pCheckedBackground = pData->iCheckedBackgroundColor;
	if ( pBorder != NULL ) *pBorder = pData->iBorderColor;
	if ( pHoverBorder != NULL ) *pHoverBorder = pData->iHoverBorderColor;
	if ( pCheckedBorder != NULL ) *pCheckedBorder = pData->iCheckedBorderColor;
	if ( pCorner != NULL ) *pCorner = pData->iCornerColor;
	if ( pCheck != NULL ) *pCheck = pData->iCheckColor;
	return XUI_OK;
}

XUI_API int xuiCheckCardSetStateColors(xui_widget pWidget, uint32_t iDisabledBorder, uint32_t iFocus)
{
	xui_check_card_data_t* pData = __xuiCheckCardGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iDisabledBorderColor = iDisabledBorder;
	pData->iFocusColor = iFocus;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiCheckCardGetStateColors(xui_widget pWidget, uint32_t* pDisabledBorder, uint32_t* pFocus)
{
	xui_check_card_data_t* pData = __xuiCheckCardGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pDisabledBorder != NULL ) *pDisabledBorder = pData->iDisabledBorderColor;
	if ( pFocus != NULL ) *pFocus = pData->iFocusColor;
	return XUI_OK;
}

XUI_API xui_rect_t xuiCheckCardGetCornerRect(xui_widget pWidget)
{
	xui_check_card_data_t* pData = __xuiCheckCardGetData(pWidget);
	return (pData != NULL) ? pData->tCornerRect : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API uint32_t xuiCheckCardGetState(xui_widget pWidget)
{
	xui_check_card_data_t* pData = __xuiCheckCardGetData(pWidget);
	return (pData != NULL) ? __xuiCheckCardComputeState(pWidget, pData) : 0;
}

XUI_API int xuiCheckCardGetChangeCount(xui_widget pWidget)
{
	xui_check_card_data_t* pData = __xuiCheckCardGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}
