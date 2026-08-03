#include "xui_internal.h"

#include <string.h>

typedef struct xui_scroll_frame_data_t {
	xui_scroll_model_t tModel;
	xui_widget pViewport;
	xui_widget pHBar;
	xui_widget pVBar;
	xui_widget pCorner;
	xui_scroll_frame_change_proc onChange;
	void* pChangeUser;
	int iPolicyX;
	int iPolicyY;
	int iScrollbarMode;
	int iWheelAxis;
	int iCornerMode;
	int bContentDragEnabled;
	int bDraggingContent;
	int bShowH;
	int bShowV;
	int bShowCorner;
	int iChangeCount;
	float fScrollbarSize;
	float fMinThumbSize;
	float fButtonSize;
	float fWheelStep;
	float fDragX;
	float fDragY;
	float fDragOffsetX;
	float fDragOffsetY;
	uint32_t iBackgroundColor;
	uint32_t iTrackColor;
	uint32_t iThumbColor;
	uint32_t iHoverColor;
	uint32_t iActiveColor;
	uint32_t iFocusColor;
	uint32_t iDisabledColor;
	uint32_t iButtonColor;
	uint32_t iButtonIconColor;
	uint32_t iCornerColor;
	uint32_t iGripColor;
	xui_rect_t tViewportRect;
	xui_rect_t tHBarRect;
	xui_rect_t tVBarRect;
	xui_rect_t tCornerRect;
} xui_scroll_frame_data_t;

static int __xuiScrollFrameDescValid(const xui_scroll_frame_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	return (pDesc->iSize == 0) || (pDesc->iSize >= sizeof(*pDesc));
}

static int __xuiScrollFramePolicyValid(int iPolicy)
{
	return (iPolicy == XUI_SCROLLBAR_POLICY_AUTO) ||
	       (iPolicy == XUI_SCROLLBAR_POLICY_ALWAYS) ||
	       (iPolicy == XUI_SCROLLBAR_POLICY_HIDDEN);
}

static int __xuiScrollFrameModeValid(int iMode)
{
	return (iMode == XUI_SCROLLBAR_MODE_FULL) || (iMode == XUI_SCROLLBAR_MODE_COMPACT);
}

static float __xuiScrollFrameDefaultScrollbarSize(int iMode)
{
	return (iMode == XUI_SCROLLBAR_MODE_COMPACT) ? 8.0f : 16.0f;
}

static int __xuiScrollFrameDescWantsFullMode(const xui_scroll_frame_desc_t* pDesc)
{
	return (pDesc != NULL) &&
	       (pDesc->iScrollbarMode == XUI_SCROLLBAR_MODE_FULL) &&
	       (pDesc->fScrollbarSize > __xuiScrollFrameDefaultScrollbarSize(XUI_SCROLLBAR_MODE_COMPACT));
}

static int __xuiScrollFrameWheelAxisValid(int iAxis)
{
	return (iAxis == XUI_WHEEL_AXIS_VERTICAL) ||
	       (iAxis == XUI_WHEEL_AXIS_HORIZONTAL) ||
	       (iAxis == XUI_WHEEL_AXIS_BOTH);
}

static int __xuiScrollFrameCornerModeValid(int iMode)
{
	return (iMode == XUI_SCROLL_FRAME_CORNER_NONE) ||
	       (iMode == XUI_SCROLL_FRAME_CORNER_AUTO) ||
	       (iMode == XUI_SCROLL_FRAME_CORNER_GRIP);
}

static float __xuiScrollFrameMaxFloat(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static float __xuiScrollFrameMinFloat(float fA, float fB)
{
	return (fA < fB) ? fA : fB;
}

static float __xuiScrollFrameReserveSize(const xui_scroll_frame_data_t* pData)
{
	float fSize;

	if ( pData == NULL ) {
		return 0.0f;
	}
	fSize = (pData->fScrollbarSize > 0.0f) ? pData->fScrollbarSize : __xuiScrollFrameDefaultScrollbarSize(pData->iScrollbarMode);
	if ( pData->iScrollbarMode == XUI_SCROLLBAR_MODE_COMPACT ) {
		fSize = __xuiScrollFrameMinFloat(fSize, 8.0f);
	}
	if ( fSize < 1.0f ) {
		fSize = 1.0f;
	}
	return fSize;
}

static int __xuiScrollFrameNeedBar(int iPolicy, float fContent, float fViewport)
{
	if ( iPolicy == XUI_SCROLLBAR_POLICY_ALWAYS ) {
		return 1;
	}
	if ( iPolicy == XUI_SCROLLBAR_POLICY_HIDDEN ) {
		return 0;
	}
	return fContent > (fViewport + 0.01f);
}

static xui_scroll_frame_data_t* __xuiScrollFrameGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return NULL;
	}
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "scrollframe");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_scroll_frame_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiScrollFramePointInRect(xui_rect_t tRect, float fX, float fY)
{
	return (fX >= tRect.fX) && (fY >= tRect.fY) &&
	       (fX < (tRect.fX + tRect.fW)) && (fY < (tRect.fY + tRect.fH));
}

static int __xuiScrollFrameAlpha(uint32_t iColor)
{
	return (int)(iColor & 0xffu);
}

static void __xuiScrollFrameComputeShow(const xui_scroll_frame_data_t* pData, float fWidth, float fHeight, int* pShowH, int* pShowV)
{
	float fReserve;
	float fViewportW;
	float fViewportH;
	int bShowH;
	int bShowV;
	int bNextH;
	int bNextV;
	int i;

	fReserve = __xuiScrollFrameReserveSize(pData);
	bShowH = (pData->iPolicyX == XUI_SCROLLBAR_POLICY_ALWAYS);
	bShowV = (pData->iPolicyY == XUI_SCROLLBAR_POLICY_ALWAYS);
	for ( i = 0; i < 4; i++ ) {
		fViewportW = __xuiScrollFrameMaxFloat(0.0f, fWidth - (bShowV ? fReserve : 0.0f));
		fViewportH = __xuiScrollFrameMaxFloat(0.0f, fHeight - (bShowH ? fReserve : 0.0f));
		bNextH = __xuiScrollFrameNeedBar(pData->iPolicyX, pData->tModel.fContentWidth, fViewportW);
		bNextV = __xuiScrollFrameNeedBar(pData->iPolicyY, pData->tModel.fContentHeight, fViewportH);
		if ( (bNextH == bShowH) && (bNextV == bShowV) ) {
			break;
		}
		bShowH = bNextH;
		bShowV = bNextV;
	}
	if ( pShowH != NULL ) *pShowH = bShowH;
	if ( pShowV != NULL ) *pShowV = bShowV;
}

static void __xuiScrollFrameComputeRects(const xui_scroll_frame_data_t* pData, xui_rect_t tContent, xui_rect_t* pViewport, xui_rect_t* pHBar, xui_rect_t* pVBar, xui_rect_t* pCorner, int* pShowH, int* pShowV, int* pShowCorner)
{
	float fReserve;
	int bShowH;
	int bShowV;
	int bShowCorner;
	xui_rect_t tViewport;
	xui_rect_t tHBar;
	xui_rect_t tVBar;
	xui_rect_t tCorner;

	memset(&tHBar, 0, sizeof(tHBar));
	memset(&tVBar, 0, sizeof(tVBar));
	memset(&tCorner, 0, sizeof(tCorner));
	fReserve = __xuiScrollFrameReserveSize(pData);
	__xuiScrollFrameComputeShow(pData, tContent.fW, tContent.fH, &bShowH, &bShowV);
	tViewport = tContent;
	tViewport.fW = __xuiScrollFrameMaxFloat(0.0f, tContent.fW - (bShowV ? fReserve : 0.0f));
	tViewport.fH = __xuiScrollFrameMaxFloat(0.0f, tContent.fH - (bShowH ? fReserve : 0.0f));
	if ( bShowH ) {
		tHBar.fX = tViewport.fX;
		tHBar.fY = tViewport.fY + tViewport.fH;
		tHBar.fW = tViewport.fW;
		tHBar.fH = fReserve;
		if ( !bShowV ) {
			tHBar.fW = tContent.fW;
		}
	}
	if ( bShowV ) {
		tVBar.fX = tViewport.fX + tViewport.fW;
		tVBar.fY = tViewport.fY;
		tVBar.fW = fReserve;
		tVBar.fH = tViewport.fH;
		if ( !bShowH ) {
			tVBar.fH = tContent.fH;
		}
	}
	bShowCorner = bShowH && bShowV && (pData->iCornerMode != XUI_SCROLL_FRAME_CORNER_NONE);
	if ( bShowCorner ) {
		tCorner.fX = tViewport.fX + tViewport.fW;
		tCorner.fY = tViewport.fY + tViewport.fH;
		tCorner.fW = fReserve;
		tCorner.fH = fReserve;
	}
	if ( pViewport != NULL ) *pViewport = tViewport;
	if ( pHBar != NULL ) *pHBar = tHBar;
	if ( pVBar != NULL ) *pVBar = tVBar;
	if ( pCorner != NULL ) *pCorner = tCorner;
	if ( pShowH != NULL ) *pShowH = bShowH;
	if ( pShowV != NULL ) *pShowV = bShowV;
	if ( pShowCorner != NULL ) *pShowCorner = bShowCorner;
}

static int __xuiScrollFrameApplyBarStyle(xui_scroll_frame_data_t* pData)
{
	float fReserve;
	float fThickness;
	int iRet;

	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	fReserve = __xuiScrollFrameReserveSize(pData);
	fThickness = (pData->iScrollbarMode == XUI_SCROLLBAR_MODE_COMPACT) ? __xuiScrollFrameMinFloat(fReserve, 8.0f) : fReserve;
	iRet = xuiScrollBarSetMode(pData->pHBar, pData->iScrollbarMode);
	if ( iRet == XUI_OK ) iRet = xuiScrollBarSetMode(pData->pVBar, pData->iScrollbarMode);
	if ( iRet == XUI_OK ) iRet = xuiScrollBarSetButtonMode(pData->pHBar, (pData->iScrollbarMode == XUI_SCROLLBAR_MODE_FULL) ? XUI_SCROLLBAR_BUTTONS_AUTO : XUI_SCROLLBAR_BUTTONS_OFF);
	if ( iRet == XUI_OK ) iRet = xuiScrollBarSetButtonMode(pData->pVBar, (pData->iScrollbarMode == XUI_SCROLLBAR_MODE_FULL) ? XUI_SCROLLBAR_BUTTONS_AUTO : XUI_SCROLLBAR_BUTTONS_OFF);
	if ( iRet == XUI_OK ) iRet = xuiScrollBarSetMetrics(pData->pHBar, fThickness, pData->fMinThumbSize, pData->fButtonSize);
	if ( iRet == XUI_OK ) iRet = xuiScrollBarSetMetrics(pData->pVBar, fThickness, pData->fMinThumbSize, pData->fButtonSize);
	if ( iRet == XUI_OK ) iRet = xuiScrollBarSetColors(pData->pHBar, pData->iTrackColor, pData->iThumbColor, pData->iHoverColor, pData->iActiveColor, pData->iFocusColor, pData->iDisabledColor);
	if ( iRet == XUI_OK ) iRet = xuiScrollBarSetColors(pData->pVBar, pData->iTrackColor, pData->iThumbColor, pData->iHoverColor, pData->iActiveColor, pData->iFocusColor, pData->iDisabledColor);
	if ( iRet == XUI_OK ) iRet = xuiScrollBarSetButtonColors(pData->pHBar, pData->iButtonColor, pData->iButtonIconColor);
	if ( iRet == XUI_OK ) iRet = xuiScrollBarSetButtonColors(pData->pVBar, pData->iButtonColor, pData->iButtonIconColor);
	return iRet;
}

static int __xuiScrollFrameSyncBars(xui_widget pWidget, xui_scroll_frame_data_t* pData)
{
	float fMaxX;
	float fMaxY;
	int iRet;

	(void)pWidget;
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiScrollFrameApplyBarStyle(pData);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiWidgetSetVisible(pData->pHBar, pData->bShowH);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetVisible(pData->pVBar, pData->bShowV);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetVisible(pData->pCorner, pData->bShowCorner);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiScrollModelGetMaxOffset(&pData->tModel, &fMaxX, &fMaxY);
	iRet = xuiScrollBarSetRange(pData->pHBar, 0.0f, fMaxX, pData->tViewportRect.fW);
	if ( iRet == XUI_OK ) iRet = xuiScrollBarSetRange(pData->pVBar, 0.0f, fMaxY, pData->tViewportRect.fH);
	if ( iRet == XUI_OK ) iRet = xuiScrollBarSetSteps(pData->pHBar, pData->fWheelStep, pData->tViewportRect.fW);
	if ( iRet == XUI_OK ) iRet = xuiScrollBarSetSteps(pData->pVBar, pData->fWheelStep, pData->tViewportRect.fH);
	if ( iRet == XUI_OK ) iRet = xuiScrollBarSetValue(pData->pHBar, pData->tModel.fScrollX);
	if ( iRet == XUI_OK ) iRet = xuiScrollBarSetValue(pData->pVBar, pData->tModel.fScrollY);
	return iRet;
}

static int __xuiScrollFrameArrangeNow(xui_widget pWidget, xui_scroll_frame_data_t* pData, xui_rect_t tContent)
{
	xui_rect_t tViewportWorld;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiScrollFrameComputeRects(pData, tContent, &pData->tViewportRect, &pData->tHBarRect, &pData->tVBarRect, &pData->tCornerRect, &pData->bShowH, &pData->bShowV, &pData->bShowCorner);
	iRet = xuiWidgetArrange(pData->pViewport, pData->tViewportRect);
	if ( iRet == XUI_OK ) iRet = xuiWidgetArrange(pData->pHBar, pData->tHBarRect);
	if ( iRet == XUI_OK ) iRet = xuiWidgetArrange(pData->pVBar, pData->tVBarRect);
	if ( iRet == XUI_OK ) iRet = xuiWidgetArrange(pData->pCorner, pData->tCornerRect);
	if ( iRet != XUI_OK ) return iRet;
	tViewportWorld = xuiWidgetGetWorldRect(pData->pViewport);
	iRet = xuiScrollModelSetViewport(&pData->tModel, tViewportWorld);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiScrollFrameSyncBars(pWidget, pData);
}

static void __xuiScrollFrameNotify(xui_widget pWidget, xui_scroll_frame_data_t* pData)
{
	if ( (pWidget == NULL) || (pData == NULL) ) {
		return;
	}
	pData->iChangeCount++;
	(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( pData->onChange != NULL ) {
		pData->onChange(pWidget, pData->tModel.fScrollX, pData->tModel.fScrollY, pData->pChangeUser);
	}
}

static int __xuiScrollFrameSetOffsetInternal(xui_widget pWidget, xui_scroll_frame_data_t* pData, float fOffsetX, float fOffsetY, int bNotify)
{
	float fOldX;
	float fOldY;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	fOldX = pData->tModel.fScrollX;
	fOldY = pData->tModel.fScrollY;
	iRet = xuiScrollModelSetOffset(&pData->tModel, fOffsetX, fOffsetY);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( (pData->tModel.fScrollX == fOldX) && (pData->tModel.fScrollY == fOldY) ) {
		return XUI_OK;
	}
	(void)xuiScrollBarSetValue(pData->pHBar, pData->tModel.fScrollX);
	(void)xuiScrollBarSetValue(pData->pVBar, pData->tModel.fScrollY);
	if ( bNotify ) {
		__xuiScrollFrameNotify(pWidget, pData);
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_RENDER);
}

static void __xuiScrollFrameHBarChanged(xui_widget pBar, float fValue, void* pUser)
{
	xui_widget pWidget;
	xui_scroll_frame_data_t* pData;

	(void)pBar;
	pWidget = (xui_widget)pUser;
	pData = __xuiScrollFrameGetData(pWidget);
	if ( pData == NULL ) {
		return;
	}
	(void)__xuiScrollFrameSetOffsetInternal(pWidget, pData, fValue, pData->tModel.fScrollY, 1);
}

static void __xuiScrollFrameVBarChanged(xui_widget pBar, float fValue, void* pUser)
{
	xui_widget pWidget;
	xui_scroll_frame_data_t* pData;

	(void)pBar;
	pWidget = (xui_widget)pUser;
	pData = __xuiScrollFrameGetData(pWidget);
	if ( pData == NULL ) {
		return;
	}
	(void)__xuiScrollFrameSetOffsetInternal(pWidget, pData, pData->tModel.fScrollX, fValue, 1);
}

static int __xuiScrollFrameCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_scroll_frame_data_t* pData;
	xui_proxy pProxy;
	xui_rect_t tRect;
	xui_rect_t tGrip;
	float fStep;
	int i;
	int iRet;

	(void)iStateId;
	pData = (xui_scroll_frame_data_t*)pUser;
	if ( (pWidget == NULL) || (pData == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	tRect = xuiWidgetGetContentRect(pWidget);
	if ( __xuiScrollFrameAlpha(pData->iBackgroundColor) != 0 ) {
		iRet = pProxy->drawRectFill(pProxy, pDraw, tRect, pData->iBackgroundColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pData->bShowCorner && (__xuiScrollFrameAlpha(pData->iCornerColor) != 0) ) {
		iRet = pProxy->drawRectFill(pProxy, pDraw, pData->tCornerRect, pData->iCornerColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pData->bShowCorner && (pData->iCornerMode == XUI_SCROLL_FRAME_CORNER_GRIP) && (__xuiScrollFrameAlpha(pData->iGripColor) != 0) ) {
		fStep = __xuiScrollFrameMinFloat(pData->tCornerRect.fW, pData->tCornerRect.fH) / 4.0f;
		if ( fStep < 2.0f ) {
			fStep = 2.0f;
		}
		for ( i = 1; i <= 3; i++ ) {
			tGrip.fW = fStep * (float)i;
			tGrip.fH = 1.0f;
			tGrip.fX = pData->tCornerRect.fX + pData->tCornerRect.fW - tGrip.fW - 2.0f;
			tGrip.fY = pData->tCornerRect.fY + pData->tCornerRect.fH - (float)i * 3.0f - 1.0f;
			iRet = pProxy->drawRectFill(pProxy, pDraw, tGrip, pData->iGripColor);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	return XUI_OK;
}

static int __xuiScrollFrameLayoutMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_scroll_frame_data_t* pData;
	float fWidth;
	float fHeight;
	float fReserve;
	int bShowH;
	int bShowV;

	(void)pWidget;
	pData = (xui_scroll_frame_data_t*)pUser;
	if ( (pData == NULL) || (pSize == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	fWidth = pData->tModel.fContentWidth;
	fHeight = pData->tModel.fContentHeight;
	if ( fWidth <= 0.0f ) fWidth = 0.0f;
	if ( fHeight <= 0.0f ) fHeight = 0.0f;
	__xuiScrollFrameComputeShow(pData,
		(tConstraint.fX < XUI_LAYOUT_UNBOUNDED) ? tConstraint.fX : fWidth,
		(tConstraint.fY < XUI_LAYOUT_UNBOUNDED) ? tConstraint.fY : fHeight,
		&bShowH, &bShowV);
	fReserve = __xuiScrollFrameReserveSize(pData);
	pSize->fX = fWidth + (bShowV ? fReserve : 0.0f);
	pSize->fY = fHeight + (bShowH ? fReserve : 0.0f);
	return XUI_OK;
}

static int __xuiScrollFrameLayoutArrange(xui_widget pWidget, xui_rect_t tContentRect, void* pUser)
{
	return __xuiScrollFrameArrangeNow(pWidget, (xui_scroll_frame_data_t*)pUser, tContentRect);
}

static int __xuiScrollFrameEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_scroll_frame_data_t* pData;
	xui_rect_t tViewportWorld;
	float fDX;
	float fDY;
	float fOldX;
	float fOldY;
	int iRet;

	(void)pUser;
	if ( (pWidget == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiScrollFrameGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE) &&
	     (pEvent->iType != XUI_EVENT_POINTER_MOVE) &&
	     (pEvent->iType != XUI_EVENT_POINTER_UP) &&
	     (pEvent->iType != XUI_EVENT_POINTER_CAPTURE_LOST) ) {
		return XUI_OK;
	}
	tViewportWorld = xuiWidgetGetWorldRect(pData->pViewport);
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_WHEEL:
		if ( !__xuiScrollFramePointInRect(tViewportWorld, pEvent->fX, pEvent->fY) ) {
			return XUI_OK;
		}
		fDX = 0.0f;
		fDY = 0.0f;
		if ( (pData->iWheelAxis == XUI_WHEEL_AXIS_HORIZONTAL) || (pData->iWheelAxis == XUI_WHEEL_AXIS_BOTH) ) {
			fDX = (pEvent->fWheelX != 0.0f) ? (-pEvent->fWheelX * pData->fWheelStep) : 0.0f;
		}
		if ( (pData->iWheelAxis == XUI_WHEEL_AXIS_VERTICAL) || (pData->iWheelAxis == XUI_WHEEL_AXIS_BOTH) ) {
			fDY = (pEvent->fWheelY != 0.0f) ? (-pEvent->fWheelY * pData->fWheelStep) : 0.0f;
		}
		if ( (pData->iWheelAxis == XUI_WHEEL_AXIS_HORIZONTAL) && (fDX == 0.0f) && (pEvent->fWheelY != 0.0f) ) {
			fDX = -pEvent->fWheelY * pData->fWheelStep;
		}
		fOldX = pData->tModel.fScrollX;
		fOldY = pData->tModel.fScrollY;
		iRet = __xuiScrollFrameSetOffsetInternal(pWidget, pData, pData->tModel.fScrollX + fDX, pData->tModel.fScrollY + fDY, 1);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		if ( (pData->tModel.fScrollX != fOldX) || (pData->tModel.fScrollY != fOldY) ) {
			return XUI_EVENT_DISPATCH_STOP;
		}
		return XUI_OK;
	case XUI_EVENT_POINTER_DOWN:
		if ( !pData->bContentDragEnabled || (pEvent->iButton != XUI_POINTER_BUTTON_LEFT) ||
		     !__xuiScrollFramePointInRect(tViewportWorld, pEvent->fX, pEvent->fY) ) {
			return XUI_OK;
		}
		pData->bDraggingContent = 1;
		pData->fDragX = pEvent->fX;
		pData->fDragY = pEvent->fY;
		pData->fDragOffsetX = pData->tModel.fScrollX;
		pData->fDragOffsetY = pData->tModel.fScrollY;
		(void)xuiSetPointerCapture(xuiWidgetGetContext(pWidget), pWidget);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_POINTER_MOVE:
		if ( !pData->bDraggingContent ) {
			return XUI_OK;
		}
		fDX = pData->fDragX - pEvent->fX;
		fDY = pData->fDragY - pEvent->fY;
		(void)__xuiScrollFrameSetOffsetInternal(pWidget, pData, pData->fDragOffsetX + fDX, pData->fDragOffsetY + fDY, 1);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_POINTER_UP:
		if ( !pData->bDraggingContent ) {
			return XUI_OK;
		}
		pData->bDraggingContent = 0;
		if ( xuiGetPointerCapture(xuiWidgetGetContext(pWidget)) == pWidget ) {
			(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
		}
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		pData->bDraggingContent = 0;
		return XUI_OK;
	default:
		break;
	}
	return XUI_OK;
}

static void __xuiScrollFrameDefaultLayout(xui_layout_t* pLayout)
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

static void __xuiScrollFrameDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiScrollFrameInitEvents(xui_widget pWidget)
{
	int iRet;

	iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_WHEEL, __xuiScrollFrameEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiScrollFrameEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiScrollFrameEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_UP, __xuiScrollFrameEvent, NULL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiScrollFrameEvent, NULL);
	return iRet;
}

static void __xuiScrollFrameInitDefaults(xui_scroll_frame_data_t* pData, const xui_scroll_frame_desc_t* pDesc)
{
	memset(pData, 0, sizeof(*pData));
	xuiScrollModelInit(&pData->tModel);
	pData->iPolicyX = (pDesc != NULL && __xuiScrollFramePolicyValid(pDesc->iPolicyX)) ? pDesc->iPolicyX : XUI_SCROLLBAR_POLICY_AUTO;
	pData->iPolicyY = (pDesc != NULL && __xuiScrollFramePolicyValid(pDesc->iPolicyY)) ? pDesc->iPolicyY : XUI_SCROLLBAR_POLICY_AUTO;
	/* FULL is zero, so zero-initialized descs must still resolve to the compact default. */
	pData->iScrollbarMode = __xuiScrollFrameDescWantsFullMode(pDesc) ? XUI_SCROLLBAR_MODE_FULL : XUI_SCROLLBAR_MODE_COMPACT;
	pData->iWheelAxis = (pDesc != NULL && __xuiScrollFrameWheelAxisValid(pDesc->iWheelAxis)) ? pDesc->iWheelAxis : XUI_WHEEL_AXIS_VERTICAL;
	pData->iCornerMode = (pDesc != NULL && pDesc->iCornerMode == XUI_SCROLL_FRAME_CORNER_GRIP) ? XUI_SCROLL_FRAME_CORNER_GRIP : XUI_SCROLL_FRAME_CORNER_AUTO;
	pData->bContentDragEnabled = (pDesc != NULL) ? (pDesc->bContentDragEnabled != 0) : 0;
	pData->fScrollbarSize = (pDesc != NULL && pDesc->fScrollbarSize > 0.0f) ? pDesc->fScrollbarSize : __xuiScrollFrameDefaultScrollbarSize(pData->iScrollbarMode);
	pData->fMinThumbSize = (pDesc != NULL && pDesc->fMinThumbSize > 0.0f) ? pDesc->fMinThumbSize : 18.0f;
	pData->fButtonSize = (pDesc != NULL && pDesc->fButtonSize > 0.0f) ? pDesc->fButtonSize : 0.0f;
	pData->fWheelStep = (pDesc != NULL && pDesc->fWheelStep > 0.0f) ? pDesc->fWheelStep : 48.0f;
	pData->iBackgroundColor = (pDesc != NULL && pDesc->iBackgroundColor != 0) ? pDesc->iBackgroundColor : XUI_COLOR_RGBA(0, 0, 0, 0);
	pData->iTrackColor = (pDesc != NULL && pDesc->iTrackColor != 0) ? pDesc->iTrackColor : XUI_COLOR_RGBA(222, 232, 243, 255);
	pData->iThumbColor = (pDesc != NULL && pDesc->iThumbColor != 0) ? pDesc->iThumbColor : XUI_COLOR_RGBA(126, 161, 196, 245);
	pData->iHoverColor = (pDesc != NULL && pDesc->iHoverColor != 0) ? pDesc->iHoverColor : XUI_COLOR_RGBA(76, 136, 204, 250);
	pData->iActiveColor = (pDesc != NULL && pDesc->iActiveColor != 0) ? pDesc->iActiveColor : XUI_COLOR_RGBA(47, 128, 237, 255);
	pData->iFocusColor = (pDesc != NULL && pDesc->iFocusColor != 0) ? pDesc->iFocusColor : XUI_COLOR_RGBA(47, 128, 237, 180);
	pData->iDisabledColor = (pDesc != NULL && pDesc->iDisabledColor != 0) ? pDesc->iDisabledColor : XUI_COLOR_RGBA(181, 190, 204, 135);
	pData->iButtonColor = (pDesc != NULL && pDesc->iButtonColor != 0) ? pDesc->iButtonColor : XUI_COLOR_RGBA(244, 248, 253, 255);
	pData->iButtonIconColor = (pDesc != NULL && pDesc->iButtonIconColor != 0) ? pDesc->iButtonIconColor : XUI_COLOR_RGBA(88, 114, 145, 255);
	pData->iCornerColor = (pDesc != NULL && pDesc->iCornerColor != 0) ? pDesc->iCornerColor : XUI_COLOR_RGBA(232, 239, 248, 255);
	pData->iGripColor = (pDesc != NULL && pDesc->iGripColor != 0) ? pDesc->iGripColor : XUI_COLOR_RGBA(128, 154, 184, 180);
	(void)xuiScrollModelSetContentSize(&pData->tModel,
		(pDesc != NULL && pDesc->fContentWidth > 0.0f) ? pDesc->fContentWidth : 0.0f,
		(pDesc != NULL && pDesc->fContentHeight > 0.0f) ? pDesc->fContentHeight : 0.0f);
	(void)xuiScrollModelSetOffset(&pData->tModel,
		(pDesc != NULL) ? pDesc->fOffsetX : 0.0f,
		(pDesc != NULL) ? pDesc->fOffsetY : 0.0f);
}

static int __xuiScrollFrameCreateChildren(xui_widget pWidget, xui_scroll_frame_data_t* pData)
{
	xui_context pContext;
	xui_scrollbar_desc_t tBarDesc;
	int iRet;

	pContext = xuiWidgetGetContext(pWidget);
	iRet = xuiWidgetCreate(pContext, &pData->pViewport);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiWidgetAddChild(pWidget, pData->pViewport);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiWidgetSetLayoutType(pData->pViewport, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetFlowMode(pData->pViewport, XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetOverflow(pData->pViewport, XUI_OVERFLOW_CLIP);
	(void)xuiWidgetSetFocusable(pData->pViewport, 0);
	(void)xuiWidgetSetTabStop(pData->pViewport, 0);

	memset(&tBarDesc, 0, sizeof(tBarDesc));
	tBarDesc.iSize = sizeof(tBarDesc);
	tBarDesc.fMin = 0.0f;
	tBarDesc.fMax = 1.0f;
	tBarDesc.fValue = 0.0f;
	tBarDesc.fPage = 1.0f;
	tBarDesc.iOrientation = XUI_ORIENTATION_HORIZONTAL;
	tBarDesc.iMode = pData->iScrollbarMode;
	tBarDesc.iButtonMode = (pData->iScrollbarMode == XUI_SCROLLBAR_MODE_FULL) ? XUI_SCROLLBAR_BUTTONS_AUTO : XUI_SCROLLBAR_BUTTONS_OFF;
	tBarDesc.fThickness = __xuiScrollFrameReserveSize(pData);
	tBarDesc.fMinThumbSize = pData->fMinThumbSize;
	tBarDesc.fButtonSize = pData->fButtonSize;
	tBarDesc.iTrackColor = pData->iTrackColor;
	tBarDesc.iThumbColor = pData->iThumbColor;
	tBarDesc.iHoverColor = pData->iHoverColor;
	tBarDesc.iActiveColor = pData->iActiveColor;
	tBarDesc.iFocusColor = pData->iFocusColor;
	tBarDesc.iDisabledColor = pData->iDisabledColor;
	tBarDesc.iButtonColor = pData->iButtonColor;
	tBarDesc.iButtonIconColor = pData->iButtonIconColor;
	iRet = xuiScrollBarCreate(pContext, &pData->pHBar, &tBarDesc);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiWidgetAddChild(pWidget, pData->pHBar);
	if ( iRet != XUI_OK ) return iRet;
	tBarDesc.iOrientation = XUI_ORIENTATION_VERTICAL;
	iRet = xuiScrollBarCreate(pContext, &pData->pVBar, &tBarDesc);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiWidgetAddChild(pWidget, pData->pVBar);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiWidgetCreate(pContext, &pData->pCorner);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiWidgetAddChild(pWidget, pData->pCorner);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiWidgetSetHitTestVisible(pData->pCorner, 0);
	(void)xuiWidgetSetLayoutType(pData->pCorner, XUI_LAYOUT_MANUAL);
	iRet = xuiScrollBarSetChange(pData->pHBar, __xuiScrollFrameHBarChanged, pWidget);
	if ( iRet == XUI_OK ) iRet = xuiScrollBarSetChange(pData->pVBar, __xuiScrollFrameVBarChanged, pWidget);
	return iRet;
}

static int __xuiScrollFrameInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_scroll_frame_data_t* pData;
	const xui_scroll_frame_desc_t* pDesc;
	int iRet;

	(void)pUser;
	pData = (xui_scroll_frame_data_t*)pTypeData;
	pDesc = (const xui_scroll_frame_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiScrollFrameDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiScrollFrameInitDefaults(pData, pDesc);
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_CLIP);
	(void)xuiWidgetSetFocusable(pWidget, 0);
	(void)xuiWidgetSetTabStop(pWidget, 0);
	iRet = __xuiScrollFrameCreateChildren(pWidget, pData);
	if ( iRet != XUI_OK ) {
		if ( pData->pViewport != NULL ) xuiWidgetDestroy(pData->pViewport);
		if ( pData->pHBar != NULL ) xuiWidgetDestroy(pData->pHBar);
		if ( pData->pVBar != NULL ) xuiWidgetDestroy(pData->pVBar);
		if ( pData->pCorner != NULL ) xuiWidgetDestroy(pData->pCorner);
		return iRet;
	}
	return __xuiScrollFrameInitEvents(pWidget);
}

static void __xuiScrollFrameDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_scroll_frame_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_scroll_frame_data_t*)pTypeData;
	if ( pData != NULL ) {
		memset(pData, 0, sizeof(*pData));
	}
}

XUI_API xui_widget_type xuiScrollFrameGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "scrollframe");
	if ( pType != NULL ) {
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "scrollframe";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_scroll_frame_data_t);
	tDesc.onInit = __xuiScrollFrameInit;
	tDesc.onDestroy = __xuiScrollFrameDestroy;
	tDesc.onLayoutMeasure = __xuiScrollFrameLayoutMeasure;
	tDesc.onLayoutArrange = __xuiScrollFrameLayoutArrange;
	tDesc.onCacheRender = __xuiScrollFrameCacheRender;
	__xuiScrollFrameDefaultLayout(&tDesc.tLayout);
	__xuiScrollFrameDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	return pType;
}

XUI_API int xuiScrollFrameCreate(xui_context pContext, xui_widget* ppWidget, const xui_scroll_frame_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiScrollFrameDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiScrollFrameGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiScrollFrameSetChange(xui_widget pWidget, xui_scroll_frame_change_proc onChange, void* pUser)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onChange = onChange;
	pData->pChangeUser = pUser;
	return XUI_OK;
}

XUI_API xui_scroll_model_t* xuiScrollFrameGetModel(xui_widget pWidget)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	return (pData != NULL) ? &pData->tModel : NULL;
}

XUI_API xui_widget xuiScrollFrameGetViewportWidget(xui_widget pWidget)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	return (pData != NULL) ? pData->pViewport : NULL;
}

XUI_API xui_widget xuiScrollFrameGetHScrollBarWidget(xui_widget pWidget)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	return (pData != NULL) ? pData->pHBar : NULL;
}

XUI_API xui_widget xuiScrollFrameGetVScrollBarWidget(xui_widget pWidget)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	return (pData != NULL) ? pData->pVBar : NULL;
}

XUI_API xui_widget xuiScrollFrameGetCornerWidget(xui_widget pWidget)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	return (pData != NULL) ? pData->pCorner : NULL;
}

XUI_API int xuiScrollFrameLayout(xui_widget pWidget)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiScrollFrameArrangeNow(pWidget, pData, xuiWidgetGetContentRect(pWidget));
}

XUI_API int xuiScrollFrameSetContentSize(xui_widget pWidget, float fWidth, float fHeight)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = xuiScrollModelSetContentSize(&pData->tModel, fWidth, fHeight);
	if ( iRet != XUI_OK ) return iRet;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiScrollFrameGetContentSize(xui_widget pWidget, float* pWidth, float* pHeight)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollModelGetContentSize(&pData->tModel, pWidth, pHeight);
}

XUI_API int xuiScrollFrameSetOffset(xui_widget pWidget, float fOffsetX, float fOffsetY)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiScrollFrameSetOffsetInternal(pWidget, pData, fOffsetX, fOffsetY, 0);
}

XUI_API int xuiScrollFrameScrollBy(xui_widget pWidget, float fDeltaX, float fDeltaY)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiScrollFrameSetOffsetInternal(pWidget, pData, pData->tModel.fScrollX + fDeltaX, pData->tModel.fScrollY + fDeltaY, 0);
}

XUI_API int xuiScrollFrameGetOffset(xui_widget pWidget, float* pOffsetX, float* pOffsetY)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollModelGetOffset(&pData->tModel, pOffsetX, pOffsetY);
}

XUI_API int xuiScrollFrameEnsureRectVisible(xui_widget pWidget, xui_rect_t tContentRect)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	float fOldX;
	float fOldY;
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	fOldX = pData->tModel.fScrollX;
	fOldY = pData->tModel.fScrollY;
	iRet = xuiScrollModelEnsureRectVisible(&pData->tModel, tContentRect);
	if ( iRet != XUI_OK ) return iRet;
	if ( (fOldX != pData->tModel.fScrollX) || (fOldY != pData->tModel.fScrollY) ) {
		(void)xuiScrollBarSetValue(pData->pHBar, pData->tModel.fScrollX);
		(void)xuiScrollBarSetValue(pData->pVBar, pData->tModel.fScrollY);
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiScrollFrameSetScrollbarPolicy(xui_widget pWidget, int iPolicyX, int iPolicyY)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	if ( (pData == NULL) || !__xuiScrollFramePolicyValid(iPolicyX) || !__xuiScrollFramePolicyValid(iPolicyY) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iPolicyX = iPolicyX;
	pData->iPolicyY = iPolicyY;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiScrollFrameGetScrollbarPolicy(xui_widget pWidget, int* pPolicyX, int* pPolicyY)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pPolicyX != NULL ) *pPolicyX = pData->iPolicyX;
	if ( pPolicyY != NULL ) *pPolicyY = pData->iPolicyY;
	return XUI_OK;
}

XUI_API int xuiScrollFrameSetScrollbarMode(xui_widget pWidget, int iMode)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	float fOldDefaultSize;
	if ( (pData == NULL) || !__xuiScrollFrameModeValid(iMode) ) return XUI_ERROR_INVALID_ARGUMENT;
	fOldDefaultSize = __xuiScrollFrameDefaultScrollbarSize(pData->iScrollbarMode);
	if ( pData->fScrollbarSize <= 0.0f || pData->fScrollbarSize == fOldDefaultSize ) {
		pData->fScrollbarSize = __xuiScrollFrameDefaultScrollbarSize(iMode);
	}
	pData->iScrollbarMode = iMode;
	(void)__xuiScrollFrameApplyBarStyle(pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiScrollFrameGetScrollbarMode(xui_widget pWidget)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	return (pData != NULL) ? pData->iScrollbarMode : XUI_SCROLLBAR_MODE_COMPACT;
}

XUI_API int xuiScrollFrameSetWheelAxis(xui_widget pWidget, int iAxis)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	if ( (pData == NULL) || !__xuiScrollFrameWheelAxisValid(iAxis) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iWheelAxis = iAxis;
	return XUI_OK;
}

XUI_API int xuiScrollFrameGetWheelAxis(xui_widget pWidget)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	return (pData != NULL) ? pData->iWheelAxis : XUI_WHEEL_AXIS_VERTICAL;
}

XUI_API int xuiScrollFrameSetWheelStep(xui_widget pWidget, float fStep)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	if ( (pData == NULL) || (fStep <= 0.0f) || (fStep != fStep) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fWheelStep = fStep;
	return XUI_OK;
}

XUI_API float xuiScrollFrameGetWheelStep(xui_widget pWidget)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	return (pData != NULL) ? pData->fWheelStep : 0.0f;
}

XUI_API int xuiScrollFrameSetContentDragEnabled(xui_widget pWidget, int bEnabled)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bContentDragEnabled = bEnabled ? 1 : 0;
	if ( !pData->bContentDragEnabled ) {
		pData->bDraggingContent = 0;
	}
	return XUI_OK;
}

XUI_API int xuiScrollFrameIsContentDragEnabled(xui_widget pWidget)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	return (pData != NULL) ? pData->bContentDragEnabled : 0;
}

XUI_API int xuiScrollFrameSetCornerMode(xui_widget pWidget, int iMode)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	if ( (pData == NULL) || !__xuiScrollFrameCornerModeValid(iMode) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iCornerMode = iMode;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiScrollFrameGetCornerMode(xui_widget pWidget)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	return (pData != NULL) ? pData->iCornerMode : XUI_SCROLL_FRAME_CORNER_AUTO;
}

XUI_API int xuiScrollFrameSetMetrics(xui_widget pWidget, float fScrollbarSize, float fMinThumbSize, float fButtonSize)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	if ( (pData == NULL) || (fScrollbarSize < 0.0f) || (fMinThumbSize < 0.0f) || (fButtonSize < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fScrollbarSize = (fScrollbarSize > 0.0f) ? fScrollbarSize : 10.0f;
	pData->fMinThumbSize = (fMinThumbSize > 0.0f) ? fMinThumbSize : 20.0f;
	pData->fButtonSize = fButtonSize;
	return __xuiScrollFrameApplyBarStyle(pData);
}

XUI_API int xuiScrollFrameGetMetrics(xui_widget pWidget, float* pScrollbarSize, float* pMinThumbSize, float* pButtonSize)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pScrollbarSize != NULL ) *pScrollbarSize = pData->fScrollbarSize;
	if ( pMinThumbSize != NULL ) *pMinThumbSize = pData->fMinThumbSize;
	if ( pButtonSize != NULL ) *pButtonSize = pData->fButtonSize;
	return XUI_OK;
}

XUI_API int xuiScrollFrameSetBackgroundColor(xui_widget pWidget, uint32_t iColor)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iBackgroundColor = iColor;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiScrollFrameGetBackgroundColor(xui_widget pWidget)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	return (pData != NULL) ? pData->iBackgroundColor : 0u;
}

XUI_API int xuiScrollFrameSetColors(xui_widget pWidget, uint32_t iTrack, uint32_t iThumb, uint32_t iHover, uint32_t iActive, uint32_t iFocus, uint32_t iDisabled)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iTrackColor = iTrack;
	pData->iThumbColor = iThumb;
	pData->iHoverColor = iHover;
	pData->iActiveColor = iActive;
	pData->iFocusColor = iFocus;
	pData->iDisabledColor = iDisabled;
	(void)__xuiScrollFrameApplyBarStyle(pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiScrollFrameGetColors(xui_widget pWidget, uint32_t* pTrack, uint32_t* pThumb, uint32_t* pHover, uint32_t* pActive, uint32_t* pFocus, uint32_t* pDisabled)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pTrack != NULL ) *pTrack = pData->iTrackColor;
	if ( pThumb != NULL ) *pThumb = pData->iThumbColor;
	if ( pHover != NULL ) *pHover = pData->iHoverColor;
	if ( pActive != NULL ) *pActive = pData->iActiveColor;
	if ( pFocus != NULL ) *pFocus = pData->iFocusColor;
	if ( pDisabled != NULL ) *pDisabled = pData->iDisabledColor;
	return XUI_OK;
}

XUI_API int xuiScrollFrameSetButtonColors(xui_widget pWidget, uint32_t iButton, uint32_t iIcon)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iButtonColor = iButton;
	pData->iButtonIconColor = iIcon;
	(void)__xuiScrollFrameApplyBarStyle(pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiScrollFrameGetButtonColors(xui_widget pWidget, uint32_t* pButton, uint32_t* pIcon)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pButton != NULL ) *pButton = pData->iButtonColor;
	if ( pIcon != NULL ) *pIcon = pData->iButtonIconColor;
	return XUI_OK;
}

XUI_API int xuiScrollFrameSetCornerColors(xui_widget pWidget, uint32_t iCorner, uint32_t iGrip)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iCornerColor = iCorner;
	pData->iGripColor = iGrip;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiScrollFrameGetCornerColors(xui_widget pWidget, uint32_t* pCorner, uint32_t* pGrip)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pCorner != NULL ) *pCorner = pData->iCornerColor;
	if ( pGrip != NULL ) *pGrip = pData->iGripColor;
	return XUI_OK;
}

XUI_API xui_rect_t xuiScrollFrameGetViewportRect(xui_widget pWidget)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	return (pData != NULL) ? pData->tViewportRect : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API xui_rect_t xuiScrollFrameGetHScrollBarRect(xui_widget pWidget)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	return (pData != NULL) ? pData->tHBarRect : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API xui_rect_t xuiScrollFrameGetVScrollBarRect(xui_widget pWidget)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	return (pData != NULL) ? pData->tVBarRect : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API xui_rect_t xuiScrollFrameGetCornerRect(xui_widget pWidget)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	return (pData != NULL) ? pData->tCornerRect : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API int xuiScrollFrameIsHScrollBarVisible(xui_widget pWidget)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	return (pData != NULL) ? pData->bShowH : 0;
}

XUI_API int xuiScrollFrameIsVScrollBarVisible(xui_widget pWidget)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	return (pData != NULL) ? pData->bShowV : 0;
}

XUI_API int xuiScrollFrameIsCornerVisible(xui_widget pWidget)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	return (pData != NULL) ? pData->bShowCorner : 0;
}

XUI_API int xuiScrollFrameGetChangeCount(xui_widget pWidget)
{
	xui_scroll_frame_data_t* pData = __xuiScrollFrameGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}
