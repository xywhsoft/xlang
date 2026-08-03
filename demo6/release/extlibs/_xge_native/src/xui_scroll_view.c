#include "xui_internal.h"

#include <string.h>

typedef struct xui_scroll_view_data_t {
	xui_widget pFrame;
	xui_widget pContent;
	xui_scroll_view_change_proc onChange;
	void* pChangeUser;
	int iChangeCount;
} xui_scroll_view_data_t;

static int __xuiScrollViewDescValid(const xui_scroll_view_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	return (pDesc->iSize == 0) || (pDesc->iSize >= sizeof(*pDesc));
}

static xui_scroll_view_data_t* __xuiScrollViewGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return NULL;
	}
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "scrollview");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_scroll_view_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiScrollViewDescWantsFullMode(const xui_scroll_view_desc_t* pDesc)
{
	return (pDesc != NULL) &&
	       (pDesc->iScrollbarMode == XUI_SCROLLBAR_MODE_FULL) &&
	       (pDesc->fScrollbarSize > 8.0f);
}

static void __xuiScrollViewMakeFrameDesc(const xui_scroll_view_desc_t* pDesc, xui_scroll_frame_desc_t* pFrameDesc)
{
	memset(pFrameDesc, 0, sizeof(*pFrameDesc));
	pFrameDesc->iSize = sizeof(*pFrameDesc);
	pFrameDesc->iScrollbarMode = XUI_SCROLLBAR_MODE_COMPACT;
	pFrameDesc->iCornerMode = XUI_SCROLL_FRAME_CORNER_AUTO;
	if ( pDesc == NULL ) {
		return;
	}
	pFrameDesc->fContentWidth = pDesc->fContentWidth;
	pFrameDesc->fContentHeight = pDesc->fContentHeight;
	pFrameDesc->fOffsetX = pDesc->fOffsetX;
	pFrameDesc->fOffsetY = pDesc->fOffsetY;
	pFrameDesc->iPolicyX = pDesc->iPolicyX;
	pFrameDesc->iPolicyY = pDesc->iPolicyY;
	pFrameDesc->iWheelAxis = pDesc->iWheelAxis;
	if ( __xuiScrollViewDescWantsFullMode(pDesc) ) pFrameDesc->iScrollbarMode = XUI_SCROLLBAR_MODE_FULL;
	if ( pDesc->iCornerMode == XUI_SCROLL_FRAME_CORNER_GRIP ) pFrameDesc->iCornerMode = XUI_SCROLL_FRAME_CORNER_GRIP;
	pFrameDesc->bContentDragEnabled = pDesc->bContentDragEnabled;
	pFrameDesc->fScrollbarSize = pDesc->fScrollbarSize;
	pFrameDesc->fMinThumbSize = pDesc->fMinThumbSize;
	pFrameDesc->fButtonSize = pDesc->fButtonSize;
	pFrameDesc->fWheelStep = pDesc->fWheelStep;
	pFrameDesc->iBackgroundColor = pDesc->iBackgroundColor;
	pFrameDesc->iTrackColor = pDesc->iTrackColor;
	pFrameDesc->iThumbColor = pDesc->iThumbColor;
	pFrameDesc->iHoverColor = pDesc->iHoverColor;
	pFrameDesc->iActiveColor = pDesc->iActiveColor;
	pFrameDesc->iFocusColor = pDesc->iFocusColor;
	pFrameDesc->iDisabledColor = pDesc->iDisabledColor;
	pFrameDesc->iButtonColor = pDesc->iButtonColor;
	pFrameDesc->iButtonIconColor = pDesc->iButtonIconColor;
	pFrameDesc->iCornerColor = pDesc->iCornerColor;
	pFrameDesc->iGripColor = pDesc->iGripColor;
}

static float __xuiScrollViewMaxFloat(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static int __xuiScrollViewSyncContent(xui_widget pWidget, xui_scroll_view_data_t* pData)
{
	xui_rect_t tViewport;
	xui_rect_t tContent;
	float fContentW;
	float fContentH;
	float fOffsetX;
	float fOffsetY;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pFrame == NULL) || (pData->pContent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	(void)pWidget;
	tViewport = xuiScrollFrameGetViewportRect(pData->pFrame);
	fContentW = 0.0f;
	fContentH = 0.0f;
	fOffsetX = 0.0f;
	fOffsetY = 0.0f;
	iRet = xuiScrollFrameGetContentSize(pData->pFrame, &fContentW, &fContentH);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiScrollFrameGetOffset(pData->pFrame, &fOffsetX, &fOffsetY);
	if ( iRet != XUI_OK ) return iRet;
	tContent.fX = -fOffsetX;
	tContent.fY = -fOffsetY;
	tContent.fW = __xuiScrollViewMaxFloat(fContentW, tViewport.fW);
	tContent.fH = __xuiScrollViewMaxFloat(fContentH, tViewport.fH);
	iRet = xuiWidgetArrange(pData->pContent, tContent);
	if ( iRet != XUI_OK ) return iRet;
	return xuiWidgetInvalidate(pData->pContent, XUI_WIDGET_DIRTY_RENDER);
}

static void __xuiScrollViewFrameChanged(xui_widget pFrame, float fOffsetX, float fOffsetY, void* pUser)
{
	xui_widget pWidget;
	xui_scroll_view_data_t* pData;

	(void)pFrame;
	pWidget = (xui_widget)pUser;
	pData = __xuiScrollViewGetData(pWidget);
	if ( pData == NULL ) {
		return;
	}
	(void)__xuiScrollViewSyncContent(pWidget, pData);
	pData->iChangeCount++;
	if ( pData->onChange != NULL ) {
		pData->onChange(pWidget, fOffsetX, fOffsetY, pData->pChangeUser);
	}
}

static int __xuiScrollViewLayoutMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_scroll_view_data_t* pData;

	(void)pWidget;
	pData = (xui_scroll_view_data_t*)pUser;
	if ( (pData == NULL) || (pSize == NULL) || (pData->pFrame == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return xuiWidgetMeasure(pData->pFrame, tConstraint, pSize);
}

static int __xuiScrollViewLayoutArrange(xui_widget pWidget, xui_rect_t tContentRect, void* pUser)
{
	xui_scroll_view_data_t* pData;
	int iRet;

	pData = (xui_scroll_view_data_t*)pUser;
	if ( (pWidget == NULL) || (pData == NULL) || (pData->pFrame == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = xuiWidgetArrange(pData->pFrame, tContentRect);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiScrollViewSyncContent(pWidget, pData);
}

static void __xuiScrollViewDefaultLayout(xui_layout_t* pLayout)
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

static void __xuiScrollViewDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiScrollViewInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_scroll_view_data_t* pData;
	const xui_scroll_view_desc_t* pDesc;
	xui_scroll_frame_desc_t tFrameDesc;
	xui_widget pViewport;
	int iRet;

	(void)pUser;
	pData = (xui_scroll_view_data_t*)pTypeData;
	pDesc = (const xui_scroll_view_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiScrollViewDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	memset(pData, 0, sizeof(*pData));
	__xuiScrollViewMakeFrameDesc(pDesc, &tFrameDesc);
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
	iRet = xuiScrollFrameSetChange(pData->pFrame, __xuiScrollViewFrameChanged, pWidget);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pData->pFrame);
		pData->pFrame = NULL;
		return iRet;
	}
	pViewport = xuiScrollFrameGetViewportWidget(pData->pFrame);
	if ( pViewport == NULL ) {
		xuiWidgetDestroy(pData->pFrame);
		pData->pFrame = NULL;
		return XUI_ERROR_NOT_INITIALIZED;
	}
	iRet = xuiWidgetCreate(xuiWidgetGetContext(pWidget), &pData->pContent);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pData->pFrame);
		pData->pFrame = NULL;
		return iRet;
	}
	iRet = xuiWidgetAddChild(pViewport, pData->pContent);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pData->pContent);
		pData->pContent = NULL;
		xuiWidgetDestroy(pData->pFrame);
		pData->pFrame = NULL;
		return iRet;
	}
	(void)xuiWidgetSetLayoutType(pData->pContent, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetFlowMode(pData->pContent, XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetOverflow(pData->pContent, XUI_OVERFLOW_VISIBLE);
	(void)xuiWidgetSetFocusable(pData->pContent, 0);
	(void)xuiWidgetSetTabStop(pData->pContent, 0);
	return XUI_OK;
}

static void __xuiScrollViewDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_scroll_view_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_scroll_view_data_t*)pTypeData;
	if ( pData != NULL ) {
		memset(pData, 0, sizeof(*pData));
	}
}

XUI_API xui_widget_type xuiScrollViewGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "scrollview");
	if ( pType != NULL ) {
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "scrollview";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_scroll_view_data_t);
	tDesc.onInit = __xuiScrollViewInit;
	tDesc.onDestroy = __xuiScrollViewDestroy;
	tDesc.onLayoutMeasure = __xuiScrollViewLayoutMeasure;
	tDesc.onLayoutArrange = __xuiScrollViewLayoutArrange;
	__xuiScrollViewDefaultLayout(&tDesc.tLayout);
	__xuiScrollViewDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	return pType;
}

XUI_API int xuiScrollViewCreate(xui_context pContext, xui_widget* ppWidget, const xui_scroll_view_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiScrollViewDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiScrollViewGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiScrollViewSetChange(xui_widget pWidget, xui_scroll_view_change_proc onChange, void* pUser)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onChange = onChange;
	pData->pChangeUser = pUser;
	return XUI_OK;
}

XUI_API xui_widget xuiScrollViewGetFrameWidget(xui_widget pWidget)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	return (pData != NULL) ? pData->pFrame : NULL;
}

XUI_API xui_widget xuiScrollViewGetContentWidget(xui_widget pWidget)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	return (pData != NULL) ? pData->pContent : NULL;
}

XUI_API xui_widget xuiScrollViewGetViewportWidget(xui_widget pWidget)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	return (pData != NULL) ? xuiScrollFrameGetViewportWidget(pData->pFrame) : NULL;
}

XUI_API xui_scroll_model_t* xuiScrollViewGetModel(xui_widget pWidget)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	return (pData != NULL) ? xuiScrollFrameGetModel(pData->pFrame) : NULL;
}

XUI_API int xuiScrollViewLayout(xui_widget pWidget)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = xuiScrollFrameLayout(pData->pFrame);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiScrollViewSyncContent(pWidget, pData);
}

XUI_API int xuiScrollViewSetContentSize(xui_widget pWidget, float fWidth, float fHeight)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = xuiScrollFrameSetContentSize(pData->pFrame, fWidth, fHeight);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiScrollViewSyncContent(pWidget, pData);
}

XUI_API int xuiScrollViewGetContentSize(xui_widget pWidget, float* pWidth, float* pHeight)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameGetContentSize(pData->pFrame, pWidth, pHeight);
}

XUI_API int xuiScrollViewSetOffset(xui_widget pWidget, float fOffsetX, float fOffsetY)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = xuiScrollFrameSetOffset(pData->pFrame, fOffsetX, fOffsetY);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiScrollViewSyncContent(pWidget, pData);
}

XUI_API int xuiScrollViewScrollBy(xui_widget pWidget, float fDeltaX, float fDeltaY)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = xuiScrollFrameScrollBy(pData->pFrame, fDeltaX, fDeltaY);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiScrollViewSyncContent(pWidget, pData);
}

XUI_API int xuiScrollViewGetOffset(xui_widget pWidget, float* pOffsetX, float* pOffsetY)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameGetOffset(pData->pFrame, pOffsetX, pOffsetY);
}

XUI_API int xuiScrollViewEnsureRectVisible(xui_widget pWidget, xui_rect_t tContentRect)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	int iRet;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	iRet = xuiScrollFrameEnsureRectVisible(pData->pFrame, tContentRect);
	if ( iRet != XUI_OK ) return iRet;
	return __xuiScrollViewSyncContent(pWidget, pData);
}

XUI_API int xuiScrollViewEnsureChildVisible(xui_widget pWidget, xui_widget pChild)
{
	xui_scroll_view_data_t* pData;
	xui_widget pScan;
	xui_rect_t tChildWorld;
	xui_rect_t tContentWorld;
	xui_rect_t tContentRect;

	pData = __xuiScrollViewGetData(pWidget);
	if ( (pData == NULL) || !xuiInternalWidgetIsValid(pChild) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pScan = pChild;
	while ( (pScan != NULL) && (pScan != pData->pContent) ) {
		pScan = xuiWidgetGetParent(pScan);
	}
	if ( pScan != pData->pContent ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	tChildWorld = xuiWidgetGetWorldRect(pChild);
	tContentWorld = xuiWidgetGetWorldRect(pData->pContent);
	tContentRect.fX = tChildWorld.fX - tContentWorld.fX;
	tContentRect.fY = tChildWorld.fY - tContentWorld.fY;
	tContentRect.fW = tChildWorld.fW;
	tContentRect.fH = tChildWorld.fH;
	return xuiScrollViewEnsureRectVisible(pWidget, tContentRect);
}

XUI_API int xuiScrollViewSetScrollbarPolicy(xui_widget pWidget, int iPolicyX, int iPolicyY)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameSetScrollbarPolicy(pData->pFrame, iPolicyX, iPolicyY);
}

XUI_API int xuiScrollViewGetScrollbarPolicy(xui_widget pWidget, int* pPolicyX, int* pPolicyY)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameGetScrollbarPolicy(pData->pFrame, pPolicyX, pPolicyY);
}

XUI_API int xuiScrollViewSetScrollbarMode(xui_widget pWidget, int iMode)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameSetScrollbarMode(pData->pFrame, iMode);
}

XUI_API int xuiScrollViewGetScrollbarMode(xui_widget pWidget)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	return (pData != NULL) ? xuiScrollFrameGetScrollbarMode(pData->pFrame) : XUI_SCROLLBAR_MODE_COMPACT;
}

XUI_API int xuiScrollViewSetWheelAxis(xui_widget pWidget, int iAxis)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameSetWheelAxis(pData->pFrame, iAxis);
}

XUI_API int xuiScrollViewGetWheelAxis(xui_widget pWidget)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	return (pData != NULL) ? xuiScrollFrameGetWheelAxis(pData->pFrame) : XUI_WHEEL_AXIS_VERTICAL;
}

XUI_API int xuiScrollViewSetWheelStep(xui_widget pWidget, float fStep)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameSetWheelStep(pData->pFrame, fStep);
}

XUI_API float xuiScrollViewGetWheelStep(xui_widget pWidget)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	return (pData != NULL) ? xuiScrollFrameGetWheelStep(pData->pFrame) : 0.0f;
}

XUI_API int xuiScrollViewSetContentDragEnabled(xui_widget pWidget, int bEnabled)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameSetContentDragEnabled(pData->pFrame, bEnabled);
}

XUI_API int xuiScrollViewIsContentDragEnabled(xui_widget pWidget)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	return (pData != NULL) ? xuiScrollFrameIsContentDragEnabled(pData->pFrame) : 0;
}

XUI_API int xuiScrollViewSetCornerMode(xui_widget pWidget, int iMode)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameSetCornerMode(pData->pFrame, iMode);
}

XUI_API int xuiScrollViewGetCornerMode(xui_widget pWidget)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	return (pData != NULL) ? xuiScrollFrameGetCornerMode(pData->pFrame) : XUI_SCROLL_FRAME_CORNER_AUTO;
}

XUI_API int xuiScrollViewSetMetrics(xui_widget pWidget, float fScrollbarSize, float fMinThumbSize, float fButtonSize)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameSetMetrics(pData->pFrame, fScrollbarSize, fMinThumbSize, fButtonSize);
}

XUI_API int xuiScrollViewGetMetrics(xui_widget pWidget, float* pScrollbarSize, float* pMinThumbSize, float* pButtonSize)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameGetMetrics(pData->pFrame, pScrollbarSize, pMinThumbSize, pButtonSize);
}

XUI_API int xuiScrollViewSetBackgroundColor(xui_widget pWidget, uint32_t iColor)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameSetBackgroundColor(pData->pFrame, iColor);
}

XUI_API uint32_t xuiScrollViewGetBackgroundColor(xui_widget pWidget)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	return (pData != NULL) ? xuiScrollFrameGetBackgroundColor(pData->pFrame) : 0u;
}

XUI_API int xuiScrollViewSetColors(xui_widget pWidget, uint32_t iTrack, uint32_t iThumb, uint32_t iHover, uint32_t iActive, uint32_t iFocus, uint32_t iDisabled)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameSetColors(pData->pFrame, iTrack, iThumb, iHover, iActive, iFocus, iDisabled);
}

XUI_API int xuiScrollViewGetColors(xui_widget pWidget, uint32_t* pTrack, uint32_t* pThumb, uint32_t* pHover, uint32_t* pActive, uint32_t* pFocus, uint32_t* pDisabled)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameGetColors(pData->pFrame, pTrack, pThumb, pHover, pActive, pFocus, pDisabled);
}

XUI_API int xuiScrollViewSetButtonColors(xui_widget pWidget, uint32_t iButton, uint32_t iIcon)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameSetButtonColors(pData->pFrame, iButton, iIcon);
}

XUI_API int xuiScrollViewGetButtonColors(xui_widget pWidget, uint32_t* pButton, uint32_t* pIcon)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameGetButtonColors(pData->pFrame, pButton, pIcon);
}

XUI_API int xuiScrollViewSetCornerColors(xui_widget pWidget, uint32_t iCorner, uint32_t iGrip)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameSetCornerColors(pData->pFrame, iCorner, iGrip);
}

XUI_API int xuiScrollViewGetCornerColors(xui_widget pWidget, uint32_t* pCorner, uint32_t* pGrip)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollFrameGetCornerColors(pData->pFrame, pCorner, pGrip);
}

XUI_API xui_rect_t xuiScrollViewGetViewportRect(xui_widget pWidget)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	return (pData != NULL) ? xuiScrollFrameGetViewportRect(pData->pFrame) : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API int xuiScrollViewGetChangeCount(xui_widget pWidget)
{
	xui_scroll_view_data_t* pData = __xuiScrollViewGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}
