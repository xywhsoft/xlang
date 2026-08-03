#include "xui_internal.h"

#include <string.h>

#define XUI_CANVAS_DEFAULT_WIDTH 640.0f
#define XUI_CANVAS_DEFAULT_HEIGHT 480.0f

typedef struct xui_canvas_data_t {
	xui_widget pFrame;
	xui_widget pViewport;
	xui_surface pSurface;
	float fCanvasWidth;
	float fCanvasHeight;
	float fPenWidth;
	float fLastPenX;
	float fLastPenY;
	int bPenEnabled;
	int bPenDown;
	int iChangeCount;
	int iDrawCount;
	uint32_t iClearColor;
	uint32_t iBackgroundColor;
	uint32_t iBorderColor;
	uint32_t iPenColor;
} xui_canvas_data_t;

static int __xuiCanvasDescValid(const xui_canvas_desc_t* pDesc)
{
	if ( pDesc == NULL ) return 1;
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) return 0;
	if ( (pDesc->fCanvasWidth < 0.0f) || (pDesc->fCanvasHeight < 0.0f) ||
	     (pDesc->fPenWidth < 0.0f) ) return 0;
	return 1;
}

static int __xuiCanvasAlpha(uint32_t iColor)
{
	return (int)(iColor & 0xffu);
}

static float __xuiCanvasMaxFloat(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static int __xuiCanvasToIntSize(float fValue)
{
	int iValue;

	if ( fValue <= 1.0f ) return 1;
	if ( fValue > 32767.0f ) return 32767;
	iValue = (int)(fValue + 0.5f);
	return (iValue > 0) ? iValue : 1;
}

static xui_canvas_data_t* __xuiCanvasGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) return NULL;
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "canvas");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) return NULL;
	return (xui_canvas_data_t*)xuiWidgetGetTypeData(pWidget);
}

static xui_proxy __xuiCanvasProxy(xui_widget pWidget)
{
	return xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
}

static int __xuiCanvasStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
{
	xui_style_property_t tProperty;
	int iRet;

	if ( (pWidget == NULL) || (sName == NULL) || (pColor == NULL) ) return 0;
	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_COLOR) ) {
		*pColor = tProperty.tValue.iColor;
		return 1;
	}
	return 0;
}

static void __xuiCanvasResolve(xui_widget pWidget, const xui_canvas_data_t* pData, xui_canvas_data_t* pResolved)
{
	*pResolved = *pData;
	(void)__xuiCanvasStyleColor(pWidget, "canvas.background.color", &pResolved->iBackgroundColor);
	(void)__xuiCanvasStyleColor(pWidget, "canvas.border.color", &pResolved->iBorderColor);
	(void)__xuiCanvasStyleColor(pWidget, "canvas.pen.color", &pResolved->iPenColor);
}

static int __xuiCanvasInvalidateViewport(xui_canvas_data_t* pData)
{
	if ( (pData == NULL) || (pData->pViewport == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiWidgetInvalidate(pData->pViewport, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiCanvasEnsureSurface(xui_widget pWidget, xui_canvas_data_t* pData, int bClear)
{
	xui_surface_desc_t tDesc;
	xui_proxy pProxy;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->pSurface != NULL ) return XUI_OK;
	pProxy = __xuiCanvasProxy(pWidget);
	if ( (pProxy == NULL) || (pProxy->surfaceCreate == NULL) ) return XUI_ERROR_NOT_INITIALIZED;
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iKind = XUI_SURFACE_KIND_TEXTURE;
	tDesc.iFormat = XUI_SURFACE_FORMAT_RGBA8;
	tDesc.iWidth = __xuiCanvasToIntSize(pData->fCanvasWidth);
	tDesc.iHeight = __xuiCanvasToIntSize(pData->fCanvasHeight);
	tDesc.iFlags = XUI_SURFACE_ALPHA_PREMULTIPLIED | XUI_SURFACE_USAGE_TARGET;
	iRet = pProxy->surfaceCreate(pProxy, &pData->pSurface, &tDesc);
	if ( iRet != XUI_OK ) return iRet;
	if ( bClear ) {
		if ( pProxy->surfaceClear != NULL ) {
			iRet = pProxy->surfaceClear(pProxy, pData->pSurface, pData->iClearColor);
		} else if ( (pProxy->drawBegin != NULL) && (pProxy->drawClearRect != NULL) && (pProxy->drawEnd != NULL) ) {
			xui_draw_context pDraw = NULL;
			iRet = pProxy->drawBegin(pProxy, &pDraw, pData->pSurface);
			if ( iRet == XUI_OK ) {
				iRet = pProxy->drawClearRect(pProxy, pDraw, (xui_rect_t){0.0f, 0.0f, pData->fCanvasWidth, pData->fCanvasHeight}, pData->iClearColor);
				(void)pProxy->drawEnd(pProxy, pDraw);
			}
		}
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiCanvasWithDraw(xui_widget pWidget, int (*onDraw)(xui_proxy, xui_draw_context, void*), void* pUser)
{
	xui_canvas_data_t* pData;
	xui_proxy pProxy;
	xui_draw_context pDraw;
	int iRet;

	pData = __xuiCanvasGetData(pWidget);
	if ( (pData == NULL) || (onDraw == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pProxy = __xuiCanvasProxy(pWidget);
	if ( (pProxy == NULL) || (pProxy->drawBegin == NULL) || (pProxy->drawEnd == NULL) ) return XUI_ERROR_NOT_INITIALIZED;
	iRet = __xuiCanvasEnsureSurface(pWidget, pData, 1);
	if ( iRet != XUI_OK ) return iRet;
	pDraw = NULL;
	iRet = pProxy->drawBegin(pProxy, &pDraw, pData->pSurface);
	if ( iRet != XUI_OK ) return iRet;
	iRet = onDraw(pProxy, pDraw, pUser);
	(void)pProxy->drawEnd(pProxy, pDraw);
	if ( iRet == XUI_OK ) {
		pData->iDrawCount++;
		(void)__xuiCanvasInvalidateViewport(pData);
	}
	return iRet;
}

static void __xuiCanvasFrameChanged(xui_widget pFrame, float fOffsetX, float fOffsetY, void* pUser)
{
	xui_widget pWidget;
	xui_canvas_data_t* pData;

	(void)pFrame;
	(void)fOffsetX;
	(void)fOffsetY;
	pWidget = (xui_widget)pUser;
	pData = __xuiCanvasGetData(pWidget);
	if ( pData == NULL ) return;
	pData->iChangeCount++;
	(void)__xuiCanvasInvalidateViewport(pData);
}

static int __xuiCanvasViewportRender(xui_widget pViewport, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_widget pWidget;
	xui_canvas_data_t* pData;
	xui_canvas_data_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tViewport;
	xui_rect_t tSrc;
	xui_rect_t tDst;
	float fOffsetX;
	float fOffsetY;
	int iRet;

	(void)iStateId;
	pWidget = (xui_widget)pUser;
	pData = __xuiCanvasGetData(pWidget);
	if ( (pData == NULL) || (pViewport == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	pProxy = __xuiCanvasProxy(pWidget);
	if ( pProxy == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	__xuiCanvasResolve(pWidget, pData, &tResolved);
	tViewport = xuiWidgetGetContentRect(pViewport);
	iRet = XUI_OK;
	if ( (__xuiCanvasAlpha(tResolved.iBackgroundColor) != 0) && (pProxy->drawRectFill != NULL) ) {
		iRet = pProxy->drawRectFill(pProxy, pDraw, tViewport, tResolved.iBackgroundColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	iRet = __xuiCanvasEnsureSurface(pWidget, pData, 1);
	if ( iRet != XUI_OK ) return iRet;
	fOffsetX = 0.0f;
	fOffsetY = 0.0f;
	(void)xuiScrollFrameGetOffset(pData->pFrame, &fOffsetX, &fOffsetY);
	tSrc = (xui_rect_t){fOffsetX, fOffsetY, tViewport.fW, tViewport.fH};
	tDst = tViewport;
	if ( tSrc.fX < 0.0f ) tSrc.fX = 0.0f;
	if ( tSrc.fY < 0.0f ) tSrc.fY = 0.0f;
	if ( (tSrc.fX + tSrc.fW) > pData->fCanvasWidth ) tSrc.fW = __xuiCanvasMaxFloat(0.0f, pData->fCanvasWidth - tSrc.fX);
	if ( (tSrc.fY + tSrc.fH) > pData->fCanvasHeight ) tSrc.fH = __xuiCanvasMaxFloat(0.0f, pData->fCanvasHeight - tSrc.fY);
	if ( (tSrc.fW > 0.0f) && (tSrc.fH > 0.0f) && (pProxy->drawSurface != NULL) ) {
		tDst.fW = tSrc.fW;
		tDst.fH = tSrc.fH;
		iRet = pProxy->drawSurface(pProxy, pDraw, pData->pSurface, tSrc, tDst, XUI_COLOR_WHITE, 0);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( (__xuiCanvasAlpha(tResolved.iBorderColor) != 0) && (pProxy->drawRectStroke != NULL) ) {
		iRet = pProxy->drawRectStroke(pProxy, pDraw, tViewport, 1.0f, tResolved.iBorderColor);
	}
	return iRet;
}

static xui_vec2_t __xuiCanvasEventToContent(xui_canvas_data_t* pData, const xui_event_t* pEvent)
{
	xui_rect_t tViewport;
	float fOffsetX;
	float fOffsetY;

	tViewport = xuiWidgetGetWorldRect(pData->pViewport);
	fOffsetX = 0.0f;
	fOffsetY = 0.0f;
	(void)xuiScrollFrameGetOffset(pData->pFrame, &fOffsetX, &fOffsetY);
	return (xui_vec2_t){pEvent->fX - tViewport.fX + fOffsetX, pEvent->fY - tViewport.fY + fOffsetY};
}

static int __xuiCanvasViewportEvent(xui_widget pViewport, const xui_event_t* pEvent, void* pUser)
{
	xui_widget pWidget;
	xui_canvas_data_t* pData;
	xui_vec2_t tPoint;

	(void)pViewport;
	pWidget = (xui_widget)pUser;
	pData = __xuiCanvasGetData(pWidget);
	if ( (pData == NULL) || (pEvent == NULL) || !pData->bPenEnabled ) return XUI_OK;
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_DOWN:
		if ( pEvent->iButton != XUI_POINTER_BUTTON_LEFT ) return XUI_OK;
		tPoint = __xuiCanvasEventToContent(pData, pEvent);
		pData->bPenDown = 1;
		pData->fLastPenX = tPoint.fX;
		pData->fLastPenY = tPoint.fY;
		(void)xuiSetPointerCapture(xuiWidgetGetContext(pWidget), pViewport);
		(void)xuiCanvasDrawPoint(pWidget, tPoint.fX, tPoint.fY, pData->fPenWidth, pData->iPenColor);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_POINTER_MOVE:
		if ( !pData->bPenDown ) return XUI_OK;
		tPoint = __xuiCanvasEventToContent(pData, pEvent);
		(void)xuiCanvasDrawLine(pWidget, pData->fLastPenX, pData->fLastPenY, tPoint.fX, tPoint.fY, pData->fPenWidth, pData->iPenColor);
		pData->fLastPenX = tPoint.fX;
		pData->fLastPenY = tPoint.fY;
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_POINTER_UP:
		if ( pEvent->iButton == XUI_POINTER_BUTTON_LEFT ) {
			pData->bPenDown = 0;
			if ( xuiGetPointerCapture(xuiWidgetGetContext(pWidget)) == pViewport ) {
				(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pViewport);
			}
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		pData->bPenDown = 0;
		break;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiCanvasLayoutMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_canvas_data_t* pData;

	(void)pWidget;
	pData = (xui_canvas_data_t*)pUser;
	if ( (pData == NULL) || (pData->pFrame == NULL) || (pSize == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiWidgetMeasure(pData->pFrame, tConstraint, pSize);
}

static int __xuiCanvasLayoutArrange(xui_widget pWidget, xui_rect_t tContentRect, void* pUser)
{
	xui_canvas_data_t* pData;

	(void)pWidget;
	pData = (xui_canvas_data_t*)pUser;
	if ( (pData == NULL) || (pData->pFrame == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiWidgetArrange(pData->pFrame, tContentRect);
}

static void __xuiCanvasDefaultLayout(xui_layout_t* pLayout)
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

static void __xuiCanvasDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiCanvasInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_canvas_data_t* pData;
	const xui_canvas_desc_t* pDesc;
	xui_scroll_frame_desc_t tFrameDesc;
	int iRet;

	(void)pUser;
	pData = (xui_canvas_data_t*)pTypeData;
	pDesc = (const xui_canvas_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiCanvasDescValid(pDesc) ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(pData, 0, sizeof(*pData));
	pData->fCanvasWidth = (pDesc != NULL && pDesc->fCanvasWidth > 0.0f) ? pDesc->fCanvasWidth : XUI_CANVAS_DEFAULT_WIDTH;
	pData->fCanvasHeight = (pDesc != NULL && pDesc->fCanvasHeight > 0.0f) ? pDesc->fCanvasHeight : XUI_CANVAS_DEFAULT_HEIGHT;
	pData->fPenWidth = (pDesc != NULL && pDesc->fPenWidth > 0.0f) ? pDesc->fPenWidth : 3.0f;
	pData->bPenEnabled = (pDesc != NULL) ? (pDesc->bPenEnabled != 0) : 0;
	pData->iClearColor = (pDesc != NULL && pDesc->iClearColor != 0) ? pDesc->iClearColor : XUI_COLOR_RGBA(255, 255, 255, 255);
	pData->iBackgroundColor = (pDesc != NULL && pDesc->iBackgroundColor != 0) ? pDesc->iBackgroundColor : XUI_COLOR_RGBA(246, 249, 252, 255);
	pData->iBorderColor = (pDesc != NULL && pDesc->iBorderColor != 0) ? pDesc->iBorderColor : XUI_COLOR_RGBA(190, 205, 220, 255);
	pData->iPenColor = (pDesc != NULL && pDesc->iPenColor != 0) ? pDesc->iPenColor : XUI_COLOR_RGBA(34, 107, 214, 255);
	memset(&tFrameDesc, 0, sizeof(tFrameDesc));
	tFrameDesc.iSize = sizeof(tFrameDesc);
	tFrameDesc.fContentWidth = pData->fCanvasWidth;
	tFrameDesc.fContentHeight = pData->fCanvasHeight;
	tFrameDesc.fOffsetX = (pDesc != NULL) ? pDesc->fOffsetX : 0.0f;
	tFrameDesc.fOffsetY = (pDesc != NULL) ? pDesc->fOffsetY : 0.0f;
	tFrameDesc.iPolicyX = (pDesc != NULL) ? pDesc->iPolicyX : XUI_SCROLLBAR_POLICY_AUTO;
	tFrameDesc.iPolicyY = (pDesc != NULL) ? pDesc->iPolicyY : XUI_SCROLLBAR_POLICY_AUTO;
	tFrameDesc.iScrollbarMode = (pDesc != NULL && pDesc->iScrollbarMode == XUI_SCROLLBAR_MODE_FULL && pDesc->fScrollbarSize > 8.0f) ? XUI_SCROLLBAR_MODE_FULL : XUI_SCROLLBAR_MODE_COMPACT;
	tFrameDesc.iWheelAxis = (pDesc != NULL) ? pDesc->iWheelAxis : XUI_WHEEL_AXIS_BOTH;
	tFrameDesc.iCornerMode = (pDesc != NULL) ? pDesc->iCornerMode : XUI_SCROLL_FRAME_CORNER_GRIP;
	tFrameDesc.bContentDragEnabled = (pDesc != NULL) ? pDesc->bContentDragEnabled : 0;
	tFrameDesc.fScrollbarSize = (pDesc != NULL) ? pDesc->fScrollbarSize : 8.0f;
	tFrameDesc.fMinThumbSize = (pDesc != NULL) ? pDesc->fMinThumbSize : 18.0f;
	tFrameDesc.fButtonSize = (pDesc != NULL) ? pDesc->fButtonSize : 0.0f;
	tFrameDesc.fWheelStep = (pDesc != NULL) ? pDesc->fWheelStep : 48.0f;
	tFrameDesc.iBackgroundColor = pData->iBackgroundColor;
	if ( pDesc != NULL ) {
		tFrameDesc.iTrackColor = pDesc->iTrackColor;
		tFrameDesc.iThumbColor = pDesc->iThumbColor;
		tFrameDesc.iHoverColor = pDesc->iHoverColor;
		tFrameDesc.iActiveColor = pDesc->iActiveColor;
		tFrameDesc.iFocusColor = pDesc->iFocusColor;
		tFrameDesc.iDisabledColor = pDesc->iDisabledColor;
		tFrameDesc.iButtonColor = pDesc->iButtonColor;
		tFrameDesc.iButtonIconColor = pDesc->iButtonIconColor;
		tFrameDesc.iCornerColor = pDesc->iCornerColor;
		tFrameDesc.iGripColor = pDesc->iGripColor;
	}
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_CLIP);
	(void)xuiWidgetSetFocusable(pWidget, 0);
	(void)xuiWidgetSetTabStop(pWidget, 0);
	iRet = xuiScrollFrameCreate(xuiWidgetGetContext(pWidget), &pData->pFrame, &tFrameDesc);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiWidgetAddChild(pWidget, pData->pFrame);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pData->pFrame);
		pData->pFrame = NULL;
		return iRet;
	}
	(void)xuiWidgetSetFlowMode(pData->pFrame, XUI_FLOW_ABSOLUTE);
	iRet = xuiScrollFrameSetChange(pData->pFrame, __xuiCanvasFrameChanged, pWidget);
	if ( iRet != XUI_OK ) return iRet;
	pData->pViewport = xuiScrollFrameGetViewportWidget(pData->pFrame);
	if ( pData->pViewport == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	iRet = xuiWidgetSetCacheRenderCallback(pData->pViewport, __xuiCanvasViewportRender, pWidget);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pData->pViewport, XUI_EVENT_POINTER_DOWN, __xuiCanvasViewportEvent, pWidget);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pData->pViewport, XUI_EVENT_POINTER_MOVE, __xuiCanvasViewportEvent, pWidget);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pData->pViewport, XUI_EVENT_POINTER_UP, __xuiCanvasViewportEvent, pWidget);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pData->pViewport, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiCanvasViewportEvent, pWidget);
	return iRet;
}

static void __xuiCanvasDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_canvas_data_t* pData;
	xui_proxy pProxy;

	(void)pUser;
	pData = (xui_canvas_data_t*)pTypeData;
	if ( pData != NULL ) {
		pProxy = (pWidget != NULL) ? __xuiCanvasProxy(pWidget) : NULL;
		if ( (pProxy != NULL) && (pProxy->surfaceDestroy != NULL) && (pData->pSurface != NULL) ) {
			pProxy->surfaceDestroy(pProxy, pData->pSurface);
		}
		memset(pData, 0, sizeof(*pData));
	}
}

static void __xuiCanvasRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
{
	xui_style_property_info_t tInfo;

	if ( xuiStyleFindProperty(pContext, sName) != 0 ) return;
	memset(&tInfo, 0, sizeof(tInfo));
	tInfo.iSize = sizeof(tInfo);
	tInfo.sName = sName;
	tInfo.iValueType = iValueType;
	tInfo.iDirtyFlags = iDirtyFlags;
	tInfo.iFlags = iFlags;
	tInfo.pWidgetType = pType;
	(void)xuiStyleRegisterProperty(pContext, &tInfo, NULL);
}

static void __xuiCanvasRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiCanvasRegisterStyleProperty(pContext, pType, "canvas.background.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCanvasRegisterStyleProperty(pContext, pType, "canvas.border.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiCanvasRegisterStyleProperty(pContext, pType, "canvas.pen.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
}

XUI_API xui_widget_type xuiCanvasGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) return NULL;
	pType = xuiWidgetFindType(pContext, "canvas");
	if ( pType != NULL ) {
		__xuiCanvasRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "canvas";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_canvas_data_t);
	tDesc.onInit = __xuiCanvasInit;
	tDesc.onDestroy = __xuiCanvasDestroy;
	tDesc.onLayoutMeasure = __xuiCanvasLayoutMeasure;
	tDesc.onLayoutArrange = __xuiCanvasLayoutArrange;
	__xuiCanvasDefaultLayout(&tDesc.tLayout);
	__xuiCanvasDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) return NULL;
	__xuiCanvasRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiCanvasCreate(xui_context pContext, xui_widget* ppWidget, const xui_canvas_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiCanvasDescValid(pDesc) ) return XUI_ERROR_INVALID_ARGUMENT;
	*ppWidget = NULL;
	pType = xuiCanvasGetType(pContext);
	if ( pType == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API xui_widget xuiCanvasGetFrameWidget(xui_widget pWidget)
{
	xui_canvas_data_t* pData = __xuiCanvasGetData(pWidget);
	return (pData != NULL) ? pData->pFrame : NULL;
}

XUI_API xui_widget xuiCanvasGetViewportWidget(xui_widget pWidget)
{
	xui_canvas_data_t* pData = __xuiCanvasGetData(pWidget);
	return (pData != NULL) ? pData->pViewport : NULL;
}

XUI_API xui_scroll_model_t* xuiCanvasGetModel(xui_widget pWidget)
{
	xui_canvas_data_t* pData = __xuiCanvasGetData(pWidget);
	return (pData != NULL && pData->pFrame != NULL) ? xuiScrollFrameGetModel(pData->pFrame) : NULL;
}

XUI_API xui_surface xuiCanvasGetSurface(xui_widget pWidget)
{
	xui_canvas_data_t* pData = __xuiCanvasGetData(pWidget);
	if ( pData == NULL ) return NULL;
	(void)__xuiCanvasEnsureSurface(pWidget, pData, 1);
	return pData->pSurface;
}

XUI_API int xuiCanvasSetCanvasSize(xui_widget pWidget, float fWidth, float fHeight)
{
	xui_canvas_data_t* pData;
	xui_proxy pProxy;
	int iRet;

	pData = __xuiCanvasGetData(pWidget);
	if ( (pData == NULL) || (fWidth <= 0.0f) || (fHeight <= 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( (pData->fCanvasWidth == fWidth) && (pData->fCanvasHeight == fHeight) ) return XUI_OK;
	pProxy = __xuiCanvasProxy(pWidget);
	if ( (pProxy != NULL) && (pProxy->surfaceDestroy != NULL) && (pData->pSurface != NULL) ) {
		pProxy->surfaceDestroy(pProxy, pData->pSurface);
	}
	pData->pSurface = NULL;
	pData->fCanvasWidth = fWidth;
	pData->fCanvasHeight = fHeight;
	iRet = xuiScrollFrameSetContentSize(pData->pFrame, fWidth, fHeight);
	if ( iRet == XUI_OK ) iRet = __xuiCanvasEnsureSurface(pWidget, pData, 1);
	if ( iRet == XUI_OK ) iRet = __xuiCanvasInvalidateViewport(pData);
	return iRet;
}

XUI_API int xuiCanvasGetCanvasSize(xui_widget pWidget, float* pWidth, float* pHeight)
{
	xui_canvas_data_t* pData = __xuiCanvasGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pWidth != NULL ) *pWidth = pData->fCanvasWidth;
	if ( pHeight != NULL ) *pHeight = pData->fCanvasHeight;
	return XUI_OK;
}

XUI_API int xuiCanvasSetOffset(xui_widget pWidget, float fOffsetX, float fOffsetY)
{
	xui_canvas_data_t* pData = __xuiCanvasGetData(pWidget);
	int iRet;

	if ( (pData == NULL) || (pData->pFrame == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = xuiScrollFrameSetOffset(pData->pFrame, fOffsetX, fOffsetY);
	if ( iRet == XUI_OK ) (void)__xuiCanvasInvalidateViewport(pData);
	return iRet;
}

XUI_API int xuiCanvasScrollBy(xui_widget pWidget, float fDeltaX, float fDeltaY)
{
	xui_canvas_data_t* pData = __xuiCanvasGetData(pWidget);
	int iRet;

	if ( (pData == NULL) || (pData->pFrame == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = xuiScrollFrameScrollBy(pData->pFrame, fDeltaX, fDeltaY);
	if ( iRet == XUI_OK ) (void)__xuiCanvasInvalidateViewport(pData);
	return iRet;
}

XUI_API int xuiCanvasGetOffset(xui_widget pWidget, float* pOffsetX, float* pOffsetY)
{
	xui_canvas_data_t* pData = __xuiCanvasGetData(pWidget);
	return (pData != NULL && pData->pFrame != NULL) ? xuiScrollFrameGetOffset(pData->pFrame, pOffsetX, pOffsetY) : XUI_ERROR_INVALID_ARGUMENT;
}

XUI_API int xuiCanvasEnsureRectVisible(xui_widget pWidget, xui_rect_t tContentRect)
{
	xui_canvas_data_t* pData = __xuiCanvasGetData(pWidget);
	int iRet;

	if ( (pData == NULL) || (pData->pFrame == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = xuiScrollFrameEnsureRectVisible(pData->pFrame, tContentRect);
	if ( iRet == XUI_OK ) (void)__xuiCanvasInvalidateViewport(pData);
	return iRet;
}

XUI_API int xuiCanvasClear(xui_widget pWidget, uint32_t iColor)
{
	xui_canvas_data_t* pData;
	xui_proxy pProxy;
	int iRet;

	pData = __xuiCanvasGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pProxy = __xuiCanvasProxy(pWidget);
	if ( pProxy == NULL ) return XUI_ERROR_NOT_INITIALIZED;
	iRet = __xuiCanvasEnsureSurface(pWidget, pData, 0);
	if ( iRet != XUI_OK ) return iRet;
	pData->iClearColor = iColor;
	if ( pProxy->surfaceClear != NULL ) {
		iRet = pProxy->surfaceClear(pProxy, pData->pSurface, iColor);
	} else {
		iRet = xuiCanvasClearRect(pWidget, (xui_rect_t){0.0f, 0.0f, pData->fCanvasWidth, pData->fCanvasHeight}, iColor);
	}
	if ( iRet == XUI_OK ) {
		pData->iDrawCount++;
		(void)__xuiCanvasInvalidateViewport(pData);
	}
	return iRet;
}

typedef struct xui_canvas_clear_rect_args_t { xui_rect_t tRect; uint32_t iColor; } xui_canvas_clear_rect_args_t;
static int __xuiCanvasDoClearRect(xui_proxy pProxy, xui_draw_context pDraw, void* pUser)
{
	xui_canvas_clear_rect_args_t* pArgs = (xui_canvas_clear_rect_args_t*)pUser;
	return (pProxy->drawClearRect != NULL) ? pProxy->drawClearRect(pProxy, pDraw, pArgs->tRect, pArgs->iColor) : XUI_ERROR_UNSUPPORTED;
}
XUI_API int xuiCanvasClearRect(xui_widget pWidget, xui_rect_t tRect, uint32_t iColor)
{
	xui_canvas_clear_rect_args_t tArgs;
	tArgs.tRect = tRect;
	tArgs.iColor = iColor;
	return __xuiCanvasWithDraw(pWidget, __xuiCanvasDoClearRect, &tArgs);
}

typedef struct xui_canvas_surface_args_t { xui_surface pSurface; xui_rect_t tSrc; xui_rect_t tDst; uint32_t iColor; uint32_t iFlags; } xui_canvas_surface_args_t;
static int __xuiCanvasDoSurface(xui_proxy pProxy, xui_draw_context pDraw, void* pUser)
{
	xui_canvas_surface_args_t* pArgs = (xui_canvas_surface_args_t*)pUser;
	if ( (__xuiCanvasAlpha(pArgs->iColor) == 0) || (pArgs->tDst.fW <= 0.0f) || (pArgs->tDst.fH <= 0.0f) ) {
		return XUI_OK;
	}
	return (pProxy->drawSurface != NULL) ? pProxy->drawSurface(pProxy, pDraw, pArgs->pSurface, pArgs->tSrc, pArgs->tDst, pArgs->iColor, pArgs->iFlags) : XUI_ERROR_UNSUPPORTED;
}
XUI_API int xuiCanvasDrawSurface(xui_widget pWidget, xui_surface pSurface, xui_rect_t tSrc, xui_rect_t tDst, uint32_t iColor, uint32_t iFlags)
{
	xui_canvas_surface_args_t tArgs;
	if ( pSurface == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	tArgs.pSurface = pSurface; tArgs.tSrc = tSrc; tArgs.tDst = tDst; tArgs.iColor = iColor; tArgs.iFlags = iFlags;
	return __xuiCanvasWithDraw(pWidget, __xuiCanvasDoSurface, &tArgs);
}

typedef struct xui_canvas_quad_args_t { xui_surface pSurface; const xui_surface_vertex_t* pVertices; uint32_t iFlags; } xui_canvas_quad_args_t;
static int __xuiCanvasDoSurfaceQuad(xui_proxy pProxy, xui_draw_context pDraw, void* pUser)
{
	xui_canvas_quad_args_t* pArgs = (xui_canvas_quad_args_t*)pUser;
	if ( ((pArgs->pVertices[0].iColor | pArgs->pVertices[1].iColor | pArgs->pVertices[2].iColor | pArgs->pVertices[3].iColor) & 0xffu) == 0u ) {
		return XUI_OK;
	}
	return (pProxy->drawSurfaceQuad != NULL) ? pProxy->drawSurfaceQuad(pProxy, pDraw, pArgs->pSurface, pArgs->pVertices, pArgs->iFlags) : XUI_ERROR_UNSUPPORTED;
}
XUI_API int xuiCanvasDrawSurfaceQuad(xui_widget pWidget, xui_surface pSurface, const xui_surface_vertex_t* pVertices, uint32_t iFlags)
{
	xui_canvas_quad_args_t tArgs;
	if ( (pSurface == NULL) || (pVertices == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	tArgs.pSurface = pSurface; tArgs.pVertices = pVertices; tArgs.iFlags = iFlags;
	return __xuiCanvasWithDraw(pWidget, __xuiCanvasDoSurfaceQuad, &tArgs);
}

typedef struct xui_canvas_mesh_args_t { const xui_mesh_vertex_t* pVertices; int iVertexCount; const uint32_t* pIndices; int iIndexCount; uint32_t iFlags; } xui_canvas_mesh_args_t;
static int __xuiCanvasDoMesh(xui_proxy pProxy, xui_draw_context pDraw, void* pUser)
{
	xui_canvas_mesh_args_t* pArgs = (xui_canvas_mesh_args_t*)pUser;
	int i;
	uint32_t iAlphaMask;

	iAlphaMask = 0u;
	for ( i = 0; i < pArgs->iVertexCount; i++ ) {
		iAlphaMask |= pArgs->pVertices[i].iColor;
	}
	if ( (iAlphaMask & 0xffu) == 0u ) {
		return XUI_OK;
	}
	return (pProxy->drawMeshTriangles != NULL) ? pProxy->drawMeshTriangles(pProxy, pDraw, pArgs->pVertices, pArgs->iVertexCount, pArgs->pIndices, pArgs->iIndexCount, pArgs->iFlags) : XUI_ERROR_UNSUPPORTED;
}
XUI_API int xuiCanvasDrawMeshTriangles(xui_widget pWidget, const xui_mesh_vertex_t* pVertices, int iVertexCount, const uint32_t* pIndices, int iIndexCount, uint32_t iFlags)
{
	xui_canvas_mesh_args_t tArgs;
	if ( (pVertices == NULL) || (iVertexCount <= 0) || (pIndices == NULL) || (iIndexCount <= 0) ) return XUI_ERROR_INVALID_ARGUMENT;
	tArgs.pVertices = pVertices; tArgs.iVertexCount = iVertexCount; tArgs.pIndices = pIndices; tArgs.iIndexCount = iIndexCount; tArgs.iFlags = iFlags;
	return __xuiCanvasWithDraw(pWidget, __xuiCanvasDoMesh, &tArgs);
}

typedef struct xui_canvas_line_args_t { float fX0; float fY0; float fX1; float fY1; float fWidth; uint32_t iColor; } xui_canvas_line_args_t;
static int __xuiCanvasDoPoint(xui_proxy pProxy, xui_draw_context pDraw, void* pUser)
{
	xui_canvas_line_args_t* pArgs = (xui_canvas_line_args_t*)pUser;
	if ( (pArgs->fWidth <= 0.0f) || (__xuiCanvasAlpha(pArgs->iColor) == 0) ) {
		return XUI_OK;
	}
	return (pProxy->drawPoint != NULL) ? pProxy->drawPoint(pProxy, pDraw, pArgs->fX0, pArgs->fY0, pArgs->fWidth, pArgs->iColor) : XUI_ERROR_UNSUPPORTED;
}
static int __xuiCanvasDoLine(xui_proxy pProxy, xui_draw_context pDraw, void* pUser)
{
	xui_canvas_line_args_t* pArgs = (xui_canvas_line_args_t*)pUser;
	if ( (pArgs->fWidth <= 0.0f) || (__xuiCanvasAlpha(pArgs->iColor) == 0) ) {
		return XUI_OK;
	}
	return (pProxy->drawLine != NULL) ? pProxy->drawLine(pProxy, pDraw, pArgs->fX0, pArgs->fY0, pArgs->fX1, pArgs->fY1, pArgs->fWidth, pArgs->iColor) : XUI_ERROR_UNSUPPORTED;
}
XUI_API int xuiCanvasDrawPoint(xui_widget pWidget, float fX, float fY, float fSize, uint32_t iColor)
{
	xui_canvas_line_args_t tArgs = {fX, fY, 0.0f, 0.0f, fSize, iColor};
	return __xuiCanvasWithDraw(pWidget, __xuiCanvasDoPoint, &tArgs);
}
XUI_API int xuiCanvasDrawLine(xui_widget pWidget, float fX0, float fY0, float fX1, float fY1, float fWidth, uint32_t iColor)
{
	xui_canvas_line_args_t tArgs = {fX0, fY0, fX1, fY1, fWidth, iColor};
	return __xuiCanvasWithDraw(pWidget, __xuiCanvasDoLine, &tArgs);
}

typedef struct xui_canvas_triangle_args_t { xui_vec2_t tA; xui_vec2_t tB; xui_vec2_t tC; float fWidth; uint32_t iColor; } xui_canvas_triangle_args_t;
static int __xuiCanvasDoTriangleFill(xui_proxy pProxy, xui_draw_context pDraw, void* pUser)
{
	xui_canvas_triangle_args_t* pArgs = (xui_canvas_triangle_args_t*)pUser;
	if ( __xuiCanvasAlpha(pArgs->iColor) == 0 ) {
		return XUI_OK;
	}
	return (pProxy->drawTriangleFill != NULL) ? pProxy->drawTriangleFill(pProxy, pDraw, pArgs->tA, pArgs->tB, pArgs->tC, pArgs->iColor) : XUI_ERROR_UNSUPPORTED;
}
static int __xuiCanvasDoTriangleStroke(xui_proxy pProxy, xui_draw_context pDraw, void* pUser)
{
	xui_canvas_triangle_args_t* pArgs = (xui_canvas_triangle_args_t*)pUser;
	if ( (pArgs->fWidth <= 0.0f) || (__xuiCanvasAlpha(pArgs->iColor) == 0) ) {
		return XUI_OK;
	}
	return (pProxy->drawTriangleStroke != NULL) ? pProxy->drawTriangleStroke(pProxy, pDraw, pArgs->tA, pArgs->tB, pArgs->tC, pArgs->fWidth, pArgs->iColor) : XUI_ERROR_UNSUPPORTED;
}
XUI_API int xuiCanvasDrawTriangleFill(xui_widget pWidget, xui_vec2_t tA, xui_vec2_t tB, xui_vec2_t tC, uint32_t iColor)
{
	xui_canvas_triangle_args_t tArgs = {tA, tB, tC, 0.0f, iColor};
	return __xuiCanvasWithDraw(pWidget, __xuiCanvasDoTriangleFill, &tArgs);
}
XUI_API int xuiCanvasDrawTriangleStroke(xui_widget pWidget, xui_vec2_t tA, xui_vec2_t tB, xui_vec2_t tC, float fWidth, uint32_t iColor)
{
	xui_canvas_triangle_args_t tArgs = {tA, tB, tC, fWidth, iColor};
	return __xuiCanvasWithDraw(pWidget, __xuiCanvasDoTriangleStroke, &tArgs);
}

typedef struct xui_canvas_rect_args_t { xui_rect_t tRect; float fA; float fB; uint32_t iColor; } xui_canvas_rect_args_t;
static int __xuiCanvasDoRectFill(xui_proxy pProxy, xui_draw_context pDraw, void* pUser)
{
	xui_canvas_rect_args_t* pArgs = (xui_canvas_rect_args_t*)pUser;
	if ( (pArgs->tRect.fW <= 0.0f) || (pArgs->tRect.fH <= 0.0f) || (__xuiCanvasAlpha(pArgs->iColor) == 0) ) {
		return XUI_OK;
	}
	return (pProxy->drawRectFill != NULL) ? pProxy->drawRectFill(pProxy, pDraw, pArgs->tRect, pArgs->iColor) : XUI_ERROR_UNSUPPORTED;
}
static int __xuiCanvasDoRectStroke(xui_proxy pProxy, xui_draw_context pDraw, void* pUser)
{
	xui_canvas_rect_args_t* pArgs = (xui_canvas_rect_args_t*)pUser;
	if ( (pArgs->tRect.fW <= 0.0f) || (pArgs->tRect.fH <= 0.0f) || (pArgs->fA <= 0.0f) || (__xuiCanvasAlpha(pArgs->iColor) == 0) ) {
		return XUI_OK;
	}
	return (pProxy->drawRectStroke != NULL) ? pProxy->drawRectStroke(pProxy, pDraw, pArgs->tRect, pArgs->fA, pArgs->iColor) : XUI_ERROR_UNSUPPORTED;
}
XUI_API int xuiCanvasDrawRectFill(xui_widget pWidget, xui_rect_t tRect, uint32_t iColor)
{
	xui_canvas_rect_args_t tArgs = {tRect, 0.0f, 0.0f, iColor};
	return __xuiCanvasWithDraw(pWidget, __xuiCanvasDoRectFill, &tArgs);
}
XUI_API int xuiCanvasDrawRectStroke(xui_widget pWidget, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	xui_canvas_rect_args_t tArgs = {tRect, fWidth, 0.0f, iColor};
	return __xuiCanvasWithDraw(pWidget, __xuiCanvasDoRectStroke, &tArgs);
}
typedef struct xui_canvas_circle_args_t { float fX; float fY; float fRadius; float fWidth; uint32_t iColor; } xui_canvas_circle_args_t;
static int __xuiCanvasDoCircleFill(xui_proxy pProxy, xui_draw_context pDraw, void* pUser)
{
	xui_canvas_circle_args_t* pArgs = (xui_canvas_circle_args_t*)pUser;
	if ( (pArgs->fRadius <= 0.0f) || (__xuiCanvasAlpha(pArgs->iColor) == 0) ) {
		return XUI_OK;
	}
	return (pProxy->drawCircleFill != NULL) ? pProxy->drawCircleFill(pProxy, pDraw, pArgs->fX, pArgs->fY, pArgs->fRadius, pArgs->iColor) : XUI_ERROR_UNSUPPORTED;
}
static int __xuiCanvasDoCircleStroke(xui_proxy pProxy, xui_draw_context pDraw, void* pUser)
{
	xui_canvas_circle_args_t* pArgs = (xui_canvas_circle_args_t*)pUser;
	if ( (pArgs->fRadius <= 0.0f) || (pArgs->fWidth <= 0.0f) || (__xuiCanvasAlpha(pArgs->iColor) == 0) ) {
		return XUI_OK;
	}
	return (pProxy->drawCircleStroke != NULL) ? pProxy->drawCircleStroke(pProxy, pDraw, pArgs->fX, pArgs->fY, pArgs->fRadius, pArgs->fWidth, pArgs->iColor) : XUI_ERROR_UNSUPPORTED;
}
XUI_API int xuiCanvasDrawCircleFill(xui_widget pWidget, float fX, float fY, float fRadius, uint32_t iColor)
{
	xui_canvas_circle_args_t tArgs = {fX, fY, fRadius, 0.0f, iColor};
	return __xuiCanvasWithDraw(pWidget, __xuiCanvasDoCircleFill, &tArgs);
}
XUI_API int xuiCanvasDrawCircleStroke(xui_widget pWidget, float fX, float fY, float fRadius, float fWidth, uint32_t iColor)
{
	xui_canvas_circle_args_t tArgs = {fX, fY, fRadius, fWidth, iColor};
	return __xuiCanvasWithDraw(pWidget, __xuiCanvasDoCircleStroke, &tArgs);
}

typedef struct xui_canvas_text_args_t { xui_font pFont; const char* sText; xui_rect_t tRect; uint32_t iColor; uint32_t iFlags; } xui_canvas_text_args_t;
static int __xuiCanvasDoText(xui_proxy pProxy, xui_draw_context pDraw, void* pUser)
{
	xui_canvas_text_args_t* pArgs = (xui_canvas_text_args_t*)pUser;
	if ( (pArgs->sText[0] == '\0') || (pArgs->tRect.fW <= 0.0f) || (pArgs->tRect.fH <= 0.0f) || (__xuiCanvasAlpha(pArgs->iColor) == 0) ) {
		return XUI_OK;
	}
	return (pProxy->drawText != NULL) ? pProxy->drawText(pProxy, pDraw, pArgs->pFont, pArgs->sText, pArgs->tRect, pArgs->iColor, pArgs->iFlags) : XUI_ERROR_UNSUPPORTED;
}
XUI_API int xuiCanvasDrawText(xui_widget pWidget, xui_font pFont, const char* sText, xui_rect_t tRect, uint32_t iColor, uint32_t iFlags)
{
	xui_canvas_text_args_t tArgs;
	if ( (pFont == NULL) || (sText == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	tArgs.pFont = pFont; tArgs.sText = sText; tArgs.tRect = tRect; tArgs.iColor = iColor; tArgs.iFlags = iFlags;
	return __xuiCanvasWithDraw(pWidget, __xuiCanvasDoText, &tArgs);
}

XUI_API int xuiCanvasSetPen(xui_widget pWidget, int bEnabled, float fWidth, uint32_t iColor)
{
	xui_canvas_data_t* pData = __xuiCanvasGetData(pWidget);
	if ( (pData == NULL) || (fWidth < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bPenEnabled = (bEnabled != 0);
	if ( fWidth > 0.0f ) pData->fPenWidth = fWidth;
	pData->iPenColor = iColor;
	return XUI_OK;
}

XUI_API int xuiCanvasGetPen(xui_widget pWidget, int* pEnabled, float* pWidth, uint32_t* pColor)
{
	xui_canvas_data_t* pData = __xuiCanvasGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pEnabled != NULL ) *pEnabled = pData->bPenEnabled;
	if ( pWidth != NULL ) *pWidth = pData->fPenWidth;
	if ( pColor != NULL ) *pColor = pData->iPenColor;
	return XUI_OK;
}

XUI_API int xuiCanvasSetColors(xui_widget pWidget, uint32_t iBackground, uint32_t iBorder, uint32_t iPen)
{
	xui_canvas_data_t* pData = __xuiCanvasGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iBackgroundColor = iBackground;
	pData->iBorderColor = iBorder;
	pData->iPenColor = iPen;
	return __xuiCanvasInvalidateViewport(pData);
}

XUI_API int xuiCanvasGetColors(xui_widget pWidget, uint32_t* pBackground, uint32_t* pBorder, uint32_t* pPen)
{
	xui_canvas_data_t* pData = __xuiCanvasGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pBackground != NULL ) *pBackground = pData->iBackgroundColor;
	if ( pBorder != NULL ) *pBorder = pData->iBorderColor;
	if ( pPen != NULL ) *pPen = pData->iPenColor;
	return XUI_OK;
}

XUI_API int xuiCanvasSetScrollbarPolicy(xui_widget pWidget, int iPolicyX, int iPolicyY)
{
	xui_canvas_data_t* pData = __xuiCanvasGetData(pWidget);
	return (pData != NULL && pData->pFrame != NULL) ? xuiScrollFrameSetScrollbarPolicy(pData->pFrame, iPolicyX, iPolicyY) : XUI_ERROR_INVALID_ARGUMENT;
}

XUI_API int xuiCanvasSetScrollbarMode(xui_widget pWidget, int iMode)
{
	xui_canvas_data_t* pData = __xuiCanvasGetData(pWidget);
	return (pData != NULL && pData->pFrame != NULL) ? xuiScrollFrameSetScrollbarMode(pData->pFrame, iMode) : XUI_ERROR_INVALID_ARGUMENT;
}

XUI_API int xuiCanvasGetChangeCount(xui_widget pWidget)
{
	xui_canvas_data_t* pData = __xuiCanvasGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}

XUI_API int xuiCanvasGetDrawCount(xui_widget pWidget)
{
	xui_canvas_data_t* pData = __xuiCanvasGetData(pWidget);
	return (pData != NULL) ? pData->iDrawCount : 0;
}
