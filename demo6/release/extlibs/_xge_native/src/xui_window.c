#include "xui_internal.h"

#include <string.h>

typedef struct xui_window_data_t {
	xui_widget pClient;
	xui_widget pCollapseButton;
	xui_widget pMaximizeButton;
	xui_widget pCloseButton;
	xui_font pFont;
	xui_surface pIconSurface;
	xui_rect_t tIconSrc;
	xui_window_close_proc onClose;
	void* pCloseUser;
	char sTitle[XUI_WINDOW_TITLE_CAPACITY];
	int bHasIcon;
	int bOpen;
	int bTopMost;
	int bActive;
	int bShowTitleBar;
	int bMovable;
	int bDragAnywhere;
	int bResizable;
	int bResizeEdgesExplicit;
	uint32_t iResizeEdges;
	int bShowCollapse;
	int bShowMaximize;
	int bShowClose;
	int bCollapsed;
	int bMaximized;
	int iHoverPart;
	int iActivePart;
	int iInteractionEdges;
	xui_rect_t tRestoreRect;
	xui_rect_t tDragStartRect;
	float fDragStartX;
	float fDragStartY;
	float fExpandedHeight;
	float fTitleBarHeight;
	float fBorderWidth;
	float fResizeGrip;
	float fButtonSize;
	float fIconSize;
	float fMinWidth;
	float fMinHeight;
	uint32_t iBackgroundColor;
	uint32_t iClientColor;
	uint32_t iTitleBarColor;
	uint32_t iInactiveTitleBarColor;
	uint32_t iTitleTextColor;
	uint32_t iInactiveTitleTextColor;
	uint32_t iBorderColor;
	uint32_t iActiveBorderColor;
	uint32_t iButtonColor;
	uint32_t iButtonHoverColor;
	uint32_t iButtonActiveColor;
	uint32_t iCloseHoverColor;
	uint32_t iCloseActiveColor;
	xui_rect_t tTitleBarRect;
	xui_rect_t tClientRect;
	xui_rect_t tCollapseButtonRect;
	xui_rect_t tMaximizeButtonRect;
	xui_rect_t tCloseButtonRect;
	int iChangeCount;
	int iCloseCount;
} xui_window_data_t;

typedef struct xui_window_resolved_t {
	xui_font pFont;
	float fTitleBarHeight;
	float fBorderWidth;
	float fResizeGrip;
	float fButtonSize;
	float fIconSize;
	float fMinWidth;
	float fMinHeight;
	uint32_t iBackgroundColor;
	uint32_t iClientColor;
	uint32_t iTitleBarColor;
	uint32_t iInactiveTitleBarColor;
	uint32_t iTitleTextColor;
	uint32_t iInactiveTitleTextColor;
	uint32_t iBorderColor;
	uint32_t iActiveBorderColor;
	uint32_t iButtonColor;
	uint32_t iButtonHoverColor;
	uint32_t iButtonActiveColor;
	uint32_t iCloseHoverColor;
	uint32_t iCloseActiveColor;
} xui_window_resolved_t;

static int __xuiWindowSetOpenInternal(xui_widget pWidget, xui_window_data_t* pData, int bOpen, int bRaise);
static int __xuiWindowApplyLayerAndZ(xui_widget pWidget, xui_window_data_t* pData);

static uint32_t __xuiWindowAlpha(uint32_t iColor)
{
	return iColor & 0xffu;
}

static float __xuiWindowMin(float fA, float fB)
{
	return (fA < fB) ? fA : fB;
}

static float __xuiWindowMax(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static float __xuiWindowClamp(float fValue, float fMin, float fMax)
{
	if ( fMax < fMin ) {
		return fMin;
	}
	if ( fValue < fMin ) {
		return fMin;
	}
	if ( fValue > fMax ) {
		return fMax;
	}
	return fValue;
}

static int __xuiWindowFloatValid(float fValue)
{
	return (fValue == fValue) && (fValue >= 0.0f) && (fValue <= XUI_LAYOUT_UNBOUNDED);
}

static uint32_t __xuiWindowDefaultResizeEdges(const xui_window_data_t* pData)
{
	uint32_t iEdges;

	iEdges = XUI_WINDOW_EDGE_ALL;
	if ( (pData != NULL) && !pData->bShowTitleBar && pData->bDragAnywhere ) {
		iEdges &= ~XUI_WINDOW_EDGE_TOP;
	}
	return iEdges;
}

static void __xuiWindowApplyDefaultResizeEdges(xui_window_data_t* pData)
{
	if ( (pData == NULL) || pData->bResizeEdgesExplicit ) {
		return;
	}
	pData->iResizeEdges = __xuiWindowDefaultResizeEdges(pData);
}

static int __xuiWindowDescValid(const xui_window_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) {
		return 0;
	}
	if ( (pDesc->tIconSrc.fW < 0.0f) || (pDesc->tIconSrc.fH < 0.0f) ) {
		return 0;
	}
	if ( ((pDesc->fTitleBarHeight != 0.0f) && !__xuiWindowFloatValid(pDesc->fTitleBarHeight)) ||
	     ((pDesc->fBorderWidth != 0.0f) && !__xuiWindowFloatValid(pDesc->fBorderWidth)) ||
	     ((pDesc->fResizeGrip != 0.0f) && !__xuiWindowFloatValid(pDesc->fResizeGrip)) ||
	     ((pDesc->fButtonSize != 0.0f) && !__xuiWindowFloatValid(pDesc->fButtonSize)) ||
	     ((pDesc->fIconSize != 0.0f) && !__xuiWindowFloatValid(pDesc->fIconSize)) ||
	     ((pDesc->fMinWidth != 0.0f) && !__xuiWindowFloatValid(pDesc->fMinWidth)) ||
	     ((pDesc->fMinHeight != 0.0f) && !__xuiWindowFloatValid(pDesc->fMinHeight)) ) {
		return 0;
	}
	return 1;
}

static xui_window_data_t* __xuiWindowGetData(xui_widget pWidget)
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
	pType = xuiWidgetFindType(pContext, "window");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_window_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiWindowRectContains(xui_rect_t tRect, float fX, float fY)
{
	return (fX >= tRect.fX) &&
	       (fY >= tRect.fY) &&
	       (fX < (tRect.fX + tRect.fW)) &&
	       (fY < (tRect.fY + tRect.fH));
}

static int __xuiWindowStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
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

static int __xuiWindowStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
{
	xui_style_property_t tProperty;
	int iRet;

	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) && (tProperty.tValue.iType == XUI_STYLE_VALUE_FLOAT) && __xuiWindowFloatValid(tProperty.tValue.fFloat) ) {
		*pValue = tProperty.tValue.fFloat;
		return 1;
	}
	return 0;
}

static xui_font __xuiWindowStyleFont(xui_widget pWidget, xui_font pBaseFont)
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

static void __xuiWindowResolve(xui_widget pWidget, const xui_window_data_t* pData, xui_window_resolved_t* pResolved)
{
	memset(pResolved, 0, sizeof(*pResolved));
	pResolved->pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	pResolved->fTitleBarHeight = pData->fTitleBarHeight;
	pResolved->fBorderWidth = pData->fBorderWidth;
	pResolved->fResizeGrip = pData->fResizeGrip;
	pResolved->fButtonSize = pData->fButtonSize;
	pResolved->fIconSize = pData->fIconSize;
	pResolved->fMinWidth = pData->fMinWidth;
	pResolved->fMinHeight = pData->fMinHeight;
	pResolved->iBackgroundColor = pData->iBackgroundColor;
	pResolved->iClientColor = pData->iClientColor;
	pResolved->iTitleBarColor = pData->iTitleBarColor;
	pResolved->iInactiveTitleBarColor = pData->iInactiveTitleBarColor;
	pResolved->iTitleTextColor = pData->iTitleTextColor;
	pResolved->iInactiveTitleTextColor = pData->iInactiveTitleTextColor;
	pResolved->iBorderColor = pData->iBorderColor;
	pResolved->iActiveBorderColor = pData->iActiveBorderColor;
	pResolved->iButtonColor = pData->iButtonColor;
	pResolved->iButtonHoverColor = pData->iButtonHoverColor;
	pResolved->iButtonActiveColor = pData->iButtonActiveColor;
	pResolved->iCloseHoverColor = pData->iCloseHoverColor;
	pResolved->iCloseActiveColor = pData->iCloseActiveColor;

	(void)__xuiWindowStyleColor(pWidget, "window.background.color", &pResolved->iBackgroundColor);
	(void)__xuiWindowStyleColor(pWidget, "window.client.color", &pResolved->iClientColor);
	(void)__xuiWindowStyleColor(pWidget, "window.titlebar.color", &pResolved->iTitleBarColor);
	(void)__xuiWindowStyleColor(pWidget, "window.titlebar.inactive_color", &pResolved->iInactiveTitleBarColor);
	(void)__xuiWindowStyleColor(pWidget, "window.title.text_color", &pResolved->iTitleTextColor);
	(void)__xuiWindowStyleColor(pWidget, "window.title.inactive_text_color", &pResolved->iInactiveTitleTextColor);
	(void)__xuiWindowStyleColor(pWidget, "window.border.color", &pResolved->iBorderColor);
	(void)__xuiWindowStyleColor(pWidget, "window.border.active_color", &pResolved->iActiveBorderColor);
	(void)__xuiWindowStyleColor(pWidget, "window.button.color", &pResolved->iButtonColor);
	(void)__xuiWindowStyleColor(pWidget, "window.button.hover_color", &pResolved->iButtonHoverColor);
	(void)__xuiWindowStyleColor(pWidget, "window.button.active_color", &pResolved->iButtonActiveColor);
	(void)__xuiWindowStyleColor(pWidget, "window.close.hover_color", &pResolved->iCloseHoverColor);
	(void)__xuiWindowStyleColor(pWidget, "window.close.active_color", &pResolved->iCloseActiveColor);
	(void)__xuiWindowStyleFloat(pWidget, "window.titlebar.height", &pResolved->fTitleBarHeight);
	(void)__xuiWindowStyleFloat(pWidget, "window.border.width", &pResolved->fBorderWidth);
	(void)__xuiWindowStyleFloat(pWidget, "window.resize_grip", &pResolved->fResizeGrip);
	(void)__xuiWindowStyleFloat(pWidget, "window.button.size", &pResolved->fButtonSize);
	(void)__xuiWindowStyleFloat(pWidget, "window.icon.size", &pResolved->fIconSize);
	(void)__xuiWindowStyleFloat(pWidget, "window.min_width", &pResolved->fMinWidth);
	(void)__xuiWindowStyleFloat(pWidget, "window.min_height", &pResolved->fMinHeight);
	pResolved->pFont = __xuiWindowStyleFont(pWidget, pResolved->pFont);

	if ( pResolved->fTitleBarHeight < 0.0f ) pResolved->fTitleBarHeight = 0.0f;
	if ( pResolved->fButtonSize < 12.0f ) pResolved->fButtonSize = 12.0f;
	if ( pResolved->fResizeGrip < 2.0f ) pResolved->fResizeGrip = 2.0f;
}

static int __xuiWindowDrawRectFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( __xuiWindowAlpha(iColor) == 0u ) {
		return XUI_OK;
	}
	if ( (pProxy != NULL) && (pProxy->drawRectFill != NULL) ) {
		return pProxy->drawRectFill(pProxy, pDraw, tRect, iColor);
	}
	return XUI_ERROR_NOT_INITIALIZED;
}

static int __xuiWindowDrawRectStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iColor)
{
	if ( (fWidth <= 0.0f) || (__xuiWindowAlpha(iColor) == 0u) ) {
		return XUI_OK;
	}
	if ( (pProxy != NULL) && (pProxy->drawRectStroke != NULL) ) {
		return pProxy->drawRectStroke(pProxy, pDraw, tRect, fWidth, iColor);
	}
	return XUI_ERROR_NOT_INITIALIZED;
}

static int __xuiWindowDrawLine(xui_proxy pProxy, xui_draw_context pDraw, float fX0, float fY0, float fX1, float fY1, float fWidth, uint32_t iColor)
{
	if ( __xuiWindowAlpha(iColor) == 0u ) {
		return XUI_OK;
	}
	if ( (pProxy != NULL) && (pProxy->drawLine != NULL) ) {
		return pProxy->drawLine(pProxy, pDraw, fX0, fY0, fX1, fY1, fWidth, iColor);
	}
	if ( (pProxy != NULL) && (pProxy->drawRectFill != NULL) ) {
		float fX = __xuiWindowMin(fX0, fX1);
		float fY = __xuiWindowMin(fY0, fY1);
		float fW = __xuiWindowMax(1.0f, __xuiWindowMax(fX0, fX1) - fX + fWidth);
		float fH = __xuiWindowMax(1.0f, __xuiWindowMax(fY0, fY1) - fY + fWidth);
		return pProxy->drawRectFill(pProxy, pDraw, (xui_rect_t){fX, fY, fW, fH}, iColor);
	}
	return XUI_ERROR_NOT_INITIALIZED;
}

static int __xuiWindowDrawBottomSquareFill(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	return __xuiWindowDrawRectFill(pProxy, pDraw, tRect, iColor);
}

static int __xuiWindowDrawBottomSquareStroke(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, float fWidth, uint32_t iBottomFillColor, uint32_t iBorderColor)
{
	(void)iBottomFillColor;
	return __xuiWindowDrawRectStroke(pProxy, pDraw, tRect, fWidth, iBorderColor);
}

static float __xuiWindowCollapsedHeight(const xui_window_data_t* pData, const xui_window_resolved_t* pResolved)
{
	float fHeight;

	if ( (pData == NULL) || (pResolved == NULL) ) {
		return 28.0f;
	}
	fHeight = pResolved->fBorderWidth * 2.0f;
	if ( pData->bShowTitleBar ) {
		fHeight += pResolved->fTitleBarHeight;
	} else {
		fHeight += 24.0f;
	}
	if ( fHeight < 24.0f ) {
		fHeight = 24.0f;
	}
	return fHeight;
}

static float __xuiWindowMinWidth(const xui_window_data_t* pData, const xui_window_resolved_t* pResolved)
{
	float fWidth;
	float fButtonCount;

	if ( (pData == NULL) || (pResolved == NULL) ) {
		return 120.0f;
	}
	fWidth = (pResolved->fMinWidth > 0.0f) ? pResolved->fMinWidth : 160.0f;
	if ( pData->bShowTitleBar ) {
		fButtonCount = 0.0f;
		if ( pData->bShowCollapse ) fButtonCount += 1.0f;
		if ( pData->bShowMaximize ) fButtonCount += 1.0f;
		if ( pData->bShowClose ) fButtonCount += 1.0f;
		fWidth = __xuiWindowMax(fWidth, 16.0f + fButtonCount * (pResolved->fButtonSize + 4.0f) + 72.0f);
	}
	return __xuiWindowMax(fWidth, 120.0f);
}

static float __xuiWindowMinHeight(const xui_window_data_t* pData, const xui_window_resolved_t* pResolved)
{
	float fHeight;
	float fCollapsed;

	if ( (pData == NULL) || (pResolved == NULL) ) {
		return 80.0f;
	}
	fCollapsed = __xuiWindowCollapsedHeight(pData, pResolved);
	if ( pData->bCollapsed ) {
		return fCollapsed;
	}
	fHeight = (pResolved->fMinHeight > 0.0f) ? pResolved->fMinHeight : (fCollapsed + 72.0f);
	return __xuiWindowMax(fHeight, fCollapsed + 32.0f);
}

static xui_rect_t __xuiWindowParentRect(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget pParent;
	xui_vec2_t tViewport;
	xui_rect_t tRect;

	memset(&tRect, 0, sizeof(tRect));
	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return tRect;
	}
	pContext = xuiWidgetGetContext(pWidget);
	tViewport = xuiGetViewportSize(pContext);
	if ( tViewport.fX > 0.0f && tViewport.fY > 0.0f ) {
		tRect.fW = tViewport.fX;
		tRect.fH = tViewport.fY;
		return tRect;
	}
	pParent = xuiWidgetGetParent(pWidget);
	if ( pParent != NULL ) {
		tRect = xuiWidgetGetWorldRect(pParent);
		tRect.fX = 0.0f;
		tRect.fY = 0.0f;
		return tRect;
	}
	tRect.fW = 1.0f;
	tRect.fH = 1.0f;
	return tRect;
}

static xui_rect_t __xuiWindowClampRect(xui_widget pWidget, xui_window_data_t* pData, const xui_window_resolved_t* pResolved, xui_rect_t tRect)
{
	xui_rect_t tParent;
	float fMinW;
	float fMinH;
	float fRight;
	float fBottom;
	float fParentRight;
	float fParentBottom;

	tParent = __xuiWindowParentRect(pWidget);
	fMinW = __xuiWindowMinWidth(pData, pResolved);
	fMinH = __xuiWindowMinHeight(pData, pResolved);
	tRect.fW = __xuiWindowMax(tRect.fW, fMinW);
	tRect.fH = pData->bCollapsed ? __xuiWindowCollapsedHeight(pData, pResolved) : __xuiWindowMax(tRect.fH, fMinH);
	if ( (tParent.fW > 0.0f) && (tRect.fW > tParent.fW) && (tParent.fW >= fMinW) ) {
		tRect.fW = tParent.fW;
	}
	if ( (tParent.fH > 0.0f) && (tRect.fH > tParent.fH) && (tParent.fH >= fMinH) ) {
		tRect.fH = tParent.fH;
	}
	fRight = tRect.fX + tRect.fW;
	fBottom = tRect.fY + tRect.fH;
	fParentRight = tParent.fX + tParent.fW;
	fParentBottom = tParent.fY + tParent.fH;
	if ( tRect.fX < tParent.fX ) {
		tRect.fX = tParent.fX;
	}
	if ( tRect.fY < tParent.fY ) {
		tRect.fY = tParent.fY;
	}
	if ( (tParent.fW > 0.0f) && (fRight > fParentRight) ) {
		tRect.fX = fParentRight - tRect.fW;
		if ( tRect.fX < tParent.fX ) tRect.fX = tParent.fX;
	}
	if ( (tParent.fH > 0.0f) && (fBottom > fParentBottom) ) {
		tRect.fY = fParentBottom - tRect.fH;
		if ( tRect.fY < tParent.fY ) tRect.fY = tParent.fY;
	}
	return xuiInternalSnapRect(tRect);
}

static xui_rect_t __xuiWindowClampMoveRect(xui_widget pWidget, xui_rect_t tRect)
{
	xui_rect_t tParent;
	float fParentRight;
	float fParentBottom;

	tParent = __xuiWindowParentRect(pWidget);
	fParentRight = tParent.fX + tParent.fW;
	fParentBottom = tParent.fY + tParent.fH;
	if ( tParent.fW > 0.0f ) {
		if ( tRect.fW >= tParent.fW ) {
			tRect.fX = tParent.fX;
		} else {
			if ( tRect.fX < tParent.fX ) {
				tRect.fX = tParent.fX;
			}
			if ( tRect.fX + tRect.fW > fParentRight ) {
				tRect.fX = fParentRight - tRect.fW;
			}
		}
	}
	if ( tParent.fH > 0.0f ) {
		if ( tRect.fH >= tParent.fH ) {
			tRect.fY = tParent.fY;
		} else {
			if ( tRect.fY < tParent.fY ) {
				tRect.fY = tParent.fY;
			}
			if ( tRect.fY + tRect.fH > fParentBottom ) {
				tRect.fY = fParentBottom - tRect.fH;
			}
		}
	}
	return xuiInternalSnapRect(tRect);
}

static int __xuiWindowApplyRect(xui_widget pWidget, xui_window_data_t* pData, xui_rect_t tRect)
{
	xui_window_resolved_t tResolved;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiWindowResolve(pWidget, pData, &tResolved);
	tRect = __xuiWindowClampRect(pWidget, pData, &tResolved, tRect);
	if ( !pData->bCollapsed && tRect.fH > __xuiWindowCollapsedHeight(pData, &tResolved) ) {
		pData->fExpandedHeight = tRect.fH;
	}
	return xuiWidgetSetRect(pWidget, tRect);
}

static int __xuiWindowSetTreeLayer(xui_widget pWidget, int iLayer, int iZIndex)
{
	xui_widget pChild;
	int iRet;

	if ( pWidget == NULL ) {
		return XUI_OK;
	}
	iRet = xuiWidgetSetLayer(pWidget, iLayer, iZIndex);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	for ( pChild = pWidget->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		iRet = __xuiWindowSetTreeLayer(pChild, iLayer, iZIndex);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return XUI_OK;
}

static int __xuiWindowApplyLayerAndZ(xui_widget pWidget, xui_window_data_t* pData)
{
	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return __xuiWindowSetTreeLayer(pWidget, XUI_LAYER_FLOATING, pData->bTopMost ? XUI_WINDOW_Z_TOPMOST : XUI_WINDOW_Z_NORMAL);
}

static int __xuiWindowDeactivateInTree(xui_widget pRoot, xui_widget pExcept)
{
	xui_widget pChild;
	xui_window_data_t* pData;
	int iRet;

	if ( pRoot == NULL ) {
		return XUI_OK;
	}
	pData = __xuiWindowGetData(pRoot);
	if ( (pData != NULL) && (pRoot != pExcept) && pData->bActive ) {
		pData->bActive = 0;
		(void)__xuiWindowApplyLayerAndZ(pRoot, pData);
		iRet = xuiWidgetInvalidate(pRoot, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		if ( iRet != XUI_OK ) return iRet;
	}
	for ( pChild = pRoot->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		iRet = __xuiWindowDeactivateInTree(pChild, pExcept);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiWindowActivate(xui_widget pWidget, xui_window_data_t* pData, int bRaise)
{
	xui_context pContext;
	xui_widget pOverlayRoot;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || !pData->bOpen ) {
		return XUI_OK;
	}
	pContext = xuiWidgetGetContext(pWidget);
	iRet = __xuiWindowDeactivateInTree(xuiGetRootWidget(pContext), pWidget);
	if ( iRet != XUI_OK ) return iRet;
	iRet = __xuiWindowDeactivateInTree(xuiOverlayRoot(pContext), pWidget);
	if ( iRet != XUI_OK ) return iRet;
	pData->bActive = 1;
	iRet = __xuiWindowApplyLayerAndZ(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	if ( bRaise ) {
		pOverlayRoot = xuiOverlayRoot(pContext);
		if ( pWidget->pParent != pOverlayRoot ) {
			iRet = xuiOverlayAttach(pContext, NULL, pWidget, XUI_LAYER_FLOATING, pData->bTopMost ? XUI_WINDOW_Z_TOPMOST : XUI_WINDOW_Z_NORMAL);
			if ( iRet != XUI_OK ) return iRet;
			(void)__xuiWindowApplyLayerAndZ(pWidget, pData);
		} else {
			iRet = xuiOverlayBringToFront(pWidget);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	(void)xuiSetFocusWidget(pContext, pWidget);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static xui_widget __xuiWindowFindActiveInTree(xui_widget pRoot)
{
	xui_widget pChild;
	xui_widget pFound;
	xui_window_data_t* pData;

	if ( pRoot == NULL ) {
		return NULL;
	}
	pData = __xuiWindowGetData(pRoot);
	if ( (pData != NULL) && pData->bOpen && pData->bActive ) {
		return pRoot;
	}
	for ( pChild = pRoot->pFirstChild; pChild != NULL; pChild = pChild->pNextSibling ) {
		pFound = __xuiWindowFindActiveInTree(pChild);
		if ( pFound != NULL ) {
			return pFound;
		}
	}
	return NULL;
}

static uint32_t __xuiWindowResizeEdgesAtData(xui_widget pWidget, xui_window_data_t* pData, const xui_window_resolved_t* pResolved, float fX, float fY)
{
	xui_rect_t tWorld;
	float fLocalX;
	float fLocalY;
	float fGrip;
	float fTopGrip;
	uint32_t iEdges;

	if ( (pWidget == NULL) || (pData == NULL) || (pResolved == NULL) || !pData->bResizable || pData->bMaximized ) {
		return 0u;
	}
	tWorld = xuiWidgetGetWorldRect(pWidget);
	if ( !__xuiWindowRectContains(tWorld, fX, fY) ) {
		return 0u;
	}
	fLocalX = fX - tWorld.fX;
	fLocalY = fY - tWorld.fY;
	fGrip = pResolved->fResizeGrip;
	fTopGrip = pData->bShowTitleBar ? __xuiWindowMin(4.0f, fGrip) : fGrip;
	iEdges = 0u;
	if ( fLocalX <= fGrip ) iEdges |= XUI_WINDOW_EDGE_LEFT;
	if ( fLocalX >= (tWorld.fW - fGrip) ) iEdges |= XUI_WINDOW_EDGE_RIGHT;
	if ( fLocalY <= fTopGrip ) iEdges |= XUI_WINDOW_EDGE_TOP;
	if ( fLocalY >= (tWorld.fH - fGrip) ) iEdges |= XUI_WINDOW_EDGE_BOTTOM;
	return iEdges & pData->iResizeEdges;
}

static int __xuiWindowCanStartMove(xui_widget pWidget, xui_window_data_t* pData, const xui_window_resolved_t* pResolved, const xui_event_t* pEvent)
{
	xui_rect_t tWorld;
	float fLocalX;
	float fLocalY;

	if ( (pWidget == NULL) || (pData == NULL) || (pResolved == NULL) || (pEvent == NULL) ) {
		return 0;
	}
	if ( !pData->bMovable || pData->bMaximized ) {
		return 0;
	}
	tWorld = xuiWidgetGetWorldRect(pWidget);
	fLocalX = pEvent->fX - tWorld.fX;
	fLocalY = pEvent->fY - tWorld.fY;
	if ( pData->bDragAnywhere ) {
		return (pEvent->pTarget == pWidget) || (pEvent->pTarget == pData->pClient);
	}
	if ( !pData->bShowTitleBar || pEvent->pTarget != pWidget ) {
		return 0;
	}
	return __xuiWindowRectContains((xui_rect_t){0.0f, 0.0f, tWorld.fW, pResolved->fBorderWidth + pResolved->fTitleBarHeight}, fLocalX, fLocalY);
}

static xui_rect_t __xuiWindowBuildInteractionRect(xui_widget pWidget, xui_window_data_t* pData, float fX, float fY)
{
	xui_window_resolved_t tResolved;
	xui_rect_t tRect;
	float fDX;
	float fDY;
	float fRight;
	float fBottom;
	float fMinW;
	float fMinH;

	__xuiWindowResolve(pWidget, pData, &tResolved);
	tRect = pData->tDragStartRect;
	fDX = fX - pData->fDragStartX;
	fDY = fY - pData->fDragStartY;
	if ( pData->iInteractionEdges == -1 ) {
		tRect.fX += fDX;
		tRect.fY += fDY;
		return __xuiWindowClampMoveRect(pWidget, tRect);
	}
	fRight = tRect.fX + tRect.fW;
	fBottom = tRect.fY + tRect.fH;
	fMinW = __xuiWindowMinWidth(pData, &tResolved);
	fMinH = __xuiWindowMinHeight(pData, &tResolved);
	if ( (pData->iInteractionEdges & XUI_WINDOW_EDGE_LEFT) != 0u ) {
		tRect.fX += fDX;
		tRect.fW -= fDX;
		if ( tRect.fW < fMinW ) {
			tRect.fW = fMinW;
			tRect.fX = fRight - fMinW;
		}
	}
	if ( (pData->iInteractionEdges & XUI_WINDOW_EDGE_RIGHT) != 0u ) {
		tRect.fW += fDX;
		if ( tRect.fW < fMinW ) tRect.fW = fMinW;
	}
	if ( (pData->iInteractionEdges & XUI_WINDOW_EDGE_TOP) != 0u ) {
		tRect.fY += fDY;
		tRect.fH -= fDY;
		if ( tRect.fH < fMinH ) {
			tRect.fH = fMinH;
			tRect.fY = fBottom - fMinH;
		}
	}
	if ( (pData->iInteractionEdges & XUI_WINDOW_EDGE_BOTTOM) != 0u ) {
		tRect.fH += fDY;
		if ( tRect.fH < fMinH ) tRect.fH = fMinH;
	}
	return __xuiWindowClampRect(pWidget, pData, &tResolved, tRect);
}

static int __xuiWindowSetHoverPart(xui_widget pWidget, xui_window_data_t* pData, int iPart)
{
	if ( pData->iHoverPart == iPart ) {
		return XUI_OK;
	}
	pData->iHoverPart = iPart;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiWindowSetActivePart(xui_widget pWidget, xui_window_data_t* pData, int iPart)
{
	if ( pData->iActivePart == iPart ) {
		return XUI_OK;
	}
	pData->iActivePart = iPart;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiWindowButtonPart(xui_widget pButton, const xui_window_data_t* pData)
{
	if ( pButton == NULL || pData == NULL ) return XUI_WINDOW_PART_NONE;
	if ( pButton == pData->pCollapseButton ) return XUI_WINDOW_PART_COLLAPSE;
	if ( pButton == pData->pMaximizeButton ) return XUI_WINDOW_PART_MAXIMIZE;
	if ( pButton == pData->pCloseButton ) return XUI_WINDOW_PART_CLOSE;
	return XUI_WINDOW_PART_NONE;
}

static xui_rect_t __xuiWindowButtonRect(xui_widget pButton)
{
	xui_rect_t tRect;

	tRect = xuiWidgetGetRect(pButton);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	return xuiInternalSnapRect(tRect);
}

static int __xuiWindowButtonRender(xui_widget pButton, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_widget pWindow;
	xui_window_data_t* pData;
	xui_window_resolved_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tRect;
	xui_rect_t tIcon;
	uint32_t iFill;
	uint32_t iIcon;
	int iPart;
	int iRet;

	(void)iStateId;
	pWindow = (xui_widget)pUser;
	pData = __xuiWindowGetData(pWindow);
	if ( (pButton == NULL) || (pDraw == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiWindowResolve(pWindow, pData, &tResolved);
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pButton));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	iPart = __xuiWindowButtonPart(pButton, pData);
	tRect = __xuiWindowButtonRect(pButton);
	iFill = tResolved.iButtonColor;
	if ( pData->iActivePart == iPart ) {
		iFill = (iPart == XUI_WINDOW_PART_CLOSE) ? tResolved.iCloseActiveColor : tResolved.iButtonActiveColor;
	} else if ( pData->iHoverPart == iPart ) {
		iFill = (iPart == XUI_WINDOW_PART_CLOSE) ? tResolved.iCloseHoverColor : tResolved.iButtonHoverColor;
	}
	iIcon = (iPart == XUI_WINDOW_PART_CLOSE) ? XUI_COLOR_RGBA(171, 72, 76, 255) :
		(pData->bActive ? tResolved.iTitleTextColor : tResolved.iInactiveTitleTextColor);
	iRet = __xuiWindowDrawRectFill(pProxy, pDraw, tRect, iFill);
	if ( iRet != XUI_OK ) return iRet;
	tIcon.fW = 10.0f;
	tIcon.fH = 10.0f;
	tIcon.fX = tRect.fX + (tRect.fW - tIcon.fW) * 0.5f;
	tIcon.fY = tRect.fY + (tRect.fH - tIcon.fH) * 0.5f;
	if ( iPart == XUI_WINDOW_PART_COLLAPSE ) {
		if ( pData->bCollapsed ) {
			iRet = __xuiWindowDrawLine(pProxy, pDraw, tIcon.fX + 1.0f, tIcon.fY + 6.0f, tIcon.fX + 5.0f, tIcon.fY + 2.0f, 1.2f, iIcon);
			if ( iRet != XUI_OK ) return iRet;
			return __xuiWindowDrawLine(pProxy, pDraw, tIcon.fX + 5.0f, tIcon.fY + 2.0f, tIcon.fX + 9.0f, tIcon.fY + 6.0f, 1.2f, iIcon);
		}
		iRet = __xuiWindowDrawLine(pProxy, pDraw, tIcon.fX + 1.0f, tIcon.fY + 4.0f, tIcon.fX + 5.0f, tIcon.fY + 8.0f, 1.2f, iIcon);
		if ( iRet != XUI_OK ) return iRet;
		return __xuiWindowDrawLine(pProxy, pDraw, tIcon.fX + 5.0f, tIcon.fY + 8.0f, tIcon.fX + 9.0f, tIcon.fY + 4.0f, 1.2f, iIcon);
	}
	if ( iPart == XUI_WINDOW_PART_MAXIMIZE ) {
		if ( pData->bMaximized ) {
			iRet = __xuiWindowDrawRectStroke(pProxy, pDraw, (xui_rect_t){tIcon.fX + 1.0f, tIcon.fY + 4.0f, 6.0f, 5.0f}, 1.0f, iIcon);
			if ( iRet != XUI_OK ) return iRet;
			return __xuiWindowDrawRectStroke(pProxy, pDraw, (xui_rect_t){tIcon.fX + 3.0f, tIcon.fY + 1.0f, 6.0f, 5.0f}, 1.0f, iIcon);
		}
		return __xuiWindowDrawRectStroke(pProxy, pDraw, (xui_rect_t){tIcon.fX + 1.0f, tIcon.fY + 1.0f, 8.0f, 8.0f}, 1.0f, iIcon);
	}
	if ( iPart == XUI_WINDOW_PART_CLOSE ) {
		iRet = __xuiWindowDrawLine(pProxy, pDraw, tIcon.fX + 1.0f, tIcon.fY + 1.0f, tIcon.fX + 9.0f, tIcon.fY + 9.0f, 1.3f, iIcon);
		if ( iRet != XUI_OK ) return iRet;
		return __xuiWindowDrawLine(pProxy, pDraw, tIcon.fX + 9.0f, tIcon.fY + 1.0f, tIcon.fX + 1.0f, tIcon.fY + 9.0f, 1.3f, iIcon);
	}
	return XUI_OK;
}

static int __xuiWindowButtonEvent(xui_widget pButton, const xui_event_t* pEvent, void* pUser)
{
	xui_widget pWindow;
	xui_window_data_t* pData;
	xui_context pContext;
	int iPart;
	int bLeftButton;
	int bInside;

	pWindow = (xui_widget)pUser;
	pData = __xuiWindowGetData(pWindow);
	if ( (pButton == NULL) || (pEvent == NULL) || (pData == NULL) ) {
		return XUI_OK;
	}
	pContext = xuiWidgetGetContext(pWindow);
	iPart = __xuiWindowButtonPart(pButton, pData);
	bLeftButton = (pEvent->iButton == 0) || (pEvent->iButton == XUI_POINTER_BUTTON_LEFT);
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_ENTER:
	case XUI_EVENT_POINTER_MOVE:
		(void)__xuiWindowSetHoverPart(pWindow, pData, iPart);
		return XUI_OK;
	case XUI_EVENT_POINTER_LEAVE:
		if ( pData->iHoverPart == iPart ) {
			(void)__xuiWindowSetHoverPart(pWindow, pData, XUI_WINDOW_PART_NONE);
		}
		return XUI_OK;
	case XUI_EVENT_POINTER_DOWN:
		if ( bLeftButton ) {
			(void)__xuiWindowActivate(pWindow, pData, 1);
			(void)__xuiWindowSetActivePart(pWindow, pData, iPart);
			(void)xuiSetPointerCapture(pContext, pButton);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_UP:
		if ( bLeftButton && (pData->iActivePart == iPart) ) {
			xui_rect_t tWorld = xuiWidgetGetWorldRect(pButton);
			bInside = __xuiWindowRectContains(tWorld, pEvent->fX, pEvent->fY);
			(void)xuiReleasePointerCapture(pContext, pButton);
			(void)__xuiWindowSetActivePart(pWindow, pData, XUI_WINDOW_PART_NONE);
			if ( bInside ) {
				if ( iPart == XUI_WINDOW_PART_COLLAPSE ) {
					(void)xuiWindowSetCollapsed(pWindow, !pData->bCollapsed);
				} else if ( iPart == XUI_WINDOW_PART_MAXIMIZE ) {
					(void)xuiWindowSetMaximized(pWindow, !pData->bMaximized);
				} else if ( iPart == XUI_WINDOW_PART_CLOSE ) {
					pData->iCloseCount++;
					(void)xuiWindowSetOpen(pWindow, 0);
					if ( pData->onClose != NULL ) {
						pData->onClose(pWindow, pData->pCloseUser);
					}
				}
			}
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		if ( pData->iActivePart == iPart ) {
			(void)__xuiWindowSetActivePart(pWindow, pData, XUI_WINDOW_PART_NONE);
		}
		return XUI_EVENT_DISPATCH_STOP;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiWindowClientRender(xui_widget pClient, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_widget pWindow;
	xui_window_data_t* pData;
	xui_window_resolved_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tRect;

	(void)iStateId;
	pWindow = (xui_widget)pUser;
	pData = __xuiWindowGetData(pWindow);
	if ( (pClient == NULL) || (pDraw == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiWindowResolve(pWindow, pData, &tResolved);
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pClient));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	tRect = xuiWidgetGetRect(pClient);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	return __xuiWindowDrawRectFill(pProxy, pDraw, xuiInternalSnapRect(tRect), tResolved.iClientColor);
}

static int __xuiWindowCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_window_data_t* pData;
	xui_window_resolved_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tRect;
	xui_rect_t tTitle;
	xui_rect_t tText;
	xui_rect_t tIcon;
	uint32_t iBorder;
	uint32_t iTitle;
	uint32_t iTitleText;
	uint32_t iBottomFill;
	float fTextRight;
	int iRet;

	(void)iStateId;
	(void)pUser;
	pData = __xuiWindowGetData(pWidget);
	if ( (pWidget == NULL) || (pDraw == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiWindowResolve(pWidget, pData, &tResolved);
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	tRect = xuiWidgetGetRect(pWidget);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	tRect = xuiInternalSnapRect(tRect);
	iBorder = pData->bActive ? tResolved.iActiveBorderColor : tResolved.iBorderColor;
	iTitle = pData->bActive ? tResolved.iTitleBarColor : tResolved.iInactiveTitleBarColor;
	iTitleText = pData->bActive ? tResolved.iTitleTextColor : tResolved.iInactiveTitleTextColor;
	iBottomFill = (pData->bCollapsed && pData->bShowTitleBar) ? iTitle : tResolved.iBackgroundColor;
	iRet = __xuiWindowDrawBottomSquareFill(pProxy, pDraw, tRect, tResolved.iBackgroundColor);
	if ( iRet != XUI_OK ) return iRet;
	if ( pData->bShowTitleBar ) {
		tTitle = pData->tTitleBarRect;
		tTitle = xuiInternalSnapRect(tTitle);
		iRet = __xuiWindowDrawRectFill(pProxy, pDraw, tTitle, iTitle);
		if ( iRet != XUI_OK ) return iRet;
		if ( tTitle.fH > 1.0f ) {
			iRet = __xuiWindowDrawRectFill(pProxy, pDraw, (xui_rect_t){tTitle.fX, tTitle.fY + tTitle.fH - 1.0f, tTitle.fW, 1.0f}, iBorder);
			if ( iRet != XUI_OK ) return iRet;
		}
		if ( pData->bTopMost ) {
			iRet = __xuiWindowDrawRectFill(pProxy, pDraw, (xui_rect_t){tTitle.fX, tTitle.fY, tTitle.fW, 2.0f}, XUI_COLOR_RGBA(47, 128, 208, 255));
			if ( iRet != XUI_OK ) return iRet;
		}
		tText = tTitle;
		tText.fX += 8.0f;
		tText.fW -= 16.0f;
		fTextRight = tTitle.fX + tTitle.fW - 8.0f;
		if ( xuiWidgetGetVisible(pData->pCloseButton) ) fTextRight = pData->tCloseButtonRect.fX - 6.0f;
		if ( xuiWidgetGetVisible(pData->pMaximizeButton) && pData->tMaximizeButtonRect.fX < fTextRight ) fTextRight = pData->tMaximizeButtonRect.fX - 6.0f;
		if ( xuiWidgetGetVisible(pData->pCollapseButton) && pData->tCollapseButtonRect.fX < fTextRight ) fTextRight = pData->tCollapseButtonRect.fX - 6.0f;
		tText.fW = fTextRight - tText.fX;
		if ( tText.fW < 0.0f ) tText.fW = 0.0f;
		if ( pData->bHasIcon && pData->pIconSurface != NULL && pProxy->drawSurface != NULL ) {
			tIcon.fW = tResolved.fIconSize;
			tIcon.fH = tResolved.fIconSize;
			tIcon.fX = tTitle.fX + 8.0f;
			tIcon.fY = tTitle.fY + (tTitle.fH - tIcon.fH) * 0.5f;
			iRet = pProxy->drawSurface(pProxy, pDraw, pData->pIconSurface, pData->tIconSrc, xuiInternalSnapRect(tIcon), XUI_COLOR_RGBA(255, 255, 255, 255), 0);
			if ( iRet != XUI_OK ) return iRet;
			tText.fX = tIcon.fX + tIcon.fW + 8.0f;
			tText.fW = fTextRight - tText.fX;
			if ( tText.fW < 0.0f ) tText.fW = 0.0f;
		}
		if ( (tResolved.pFont != NULL) && (pProxy->drawText != NULL) && (pData->sTitle[0] != '\0') ) {
			iRet = pProxy->drawText(pProxy, pDraw, tResolved.pFont, pData->sTitle, xuiInternalSnapRect(tText), iTitleText, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	return __xuiWindowDrawBottomSquareStroke(pProxy, pDraw, tRect, tResolved.fBorderWidth, iBottomFill, iBorder);
}

static int __xuiWindowMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_window_data_t* pData;
	xui_window_resolved_t tResolved;
	xui_vec2_t tClientSize;
	xui_vec2_t tClientConstraint;
	float fChrome;
	int iRet;

	(void)pUser;
	if ( (pWidget == NULL) || (pSize == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiWindowGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiWindowResolve(pWidget, pData, &tResolved);
	memset(&tClientSize, 0, sizeof(tClientSize));
	fChrome = tResolved.fBorderWidth * 2.0f + (pData->bShowTitleBar ? tResolved.fTitleBarHeight : 0.0f);
	if ( (pData->pClient != NULL) && !pData->bCollapsed ) {
		tClientConstraint.fX = (tConstraint.fX > tResolved.fBorderWidth * 2.0f) ? (tConstraint.fX - tResolved.fBorderWidth * 2.0f) : XUI_LAYOUT_UNBOUNDED;
		tClientConstraint.fY = (tConstraint.fY > fChrome) ? (tConstraint.fY - fChrome) : XUI_LAYOUT_UNBOUNDED;
		iRet = xuiWidgetMeasure(pData->pClient, tClientConstraint, &tClientSize);
		if ( iRet != XUI_OK ) return iRet;
	}
	pSize->fX = __xuiWindowMax(__xuiWindowMinWidth(pData, &tResolved), tClientSize.fX + tResolved.fBorderWidth * 2.0f);
	pSize->fY = pData->bCollapsed ? __xuiWindowCollapsedHeight(pData, &tResolved) :
		__xuiWindowMax(__xuiWindowMinHeight(pData, &tResolved), tClientSize.fY + fChrome);
	return XUI_OK;
}

static int __xuiWindowArrange(xui_widget pWidget, xui_rect_t tContentRect, void* pUser)
{
	xui_window_data_t* pData;
	xui_window_resolved_t tResolved;
	xui_rect_t tRect;
	xui_rect_t tClient;
	xui_rect_t tButton;
	float fRight;
	float fGap;
	float fTitleH;
	int iRet;

	(void)pUser;
	pData = __xuiWindowGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiWindowResolve(pWidget, pData, &tResolved);
	tRect = (xui_rect_t){tContentRect.fX, tContentRect.fY, tContentRect.fW, tContentRect.fH};
	if ( tRect.fW <= 0.0f || tRect.fH <= 0.0f ) {
		tRect = xuiWidgetGetRect(pWidget);
		tRect.fX = 0.0f;
		tRect.fY = 0.0f;
	}
	fTitleH = pData->bShowTitleBar ? __xuiWindowMin(tResolved.fTitleBarHeight, tRect.fH) : 0.0f;
	pData->tTitleBarRect = (xui_rect_t){tRect.fX + tResolved.fBorderWidth, tRect.fY + tResolved.fBorderWidth,
		__xuiWindowMax(0.0f, tRect.fW - tResolved.fBorderWidth * 2.0f), fTitleH};
	tClient.fX = tRect.fX + tResolved.fBorderWidth;
	tClient.fY = tRect.fY + tResolved.fBorderWidth + fTitleH;
	tClient.fW = __xuiWindowMax(0.0f, tRect.fW - tResolved.fBorderWidth * 2.0f);
	tClient.fH = pData->bCollapsed ? 0.0f : __xuiWindowMax(0.0f, tRect.fH - tResolved.fBorderWidth * 2.0f - fTitleH);
	pData->tClientRect = xuiInternalSnapRect(tClient);
	fRight = tRect.fX + tRect.fW - tResolved.fBorderWidth - 5.0f;
	fGap = 4.0f;
	tButton.fW = tResolved.fButtonSize;
	tButton.fH = tResolved.fButtonSize;
	tButton.fY = pData->tTitleBarRect.fY + (pData->tTitleBarRect.fH - tButton.fH) * 0.5f;
	if ( pData->bShowTitleBar && pData->bShowClose ) {
		tButton.fX = fRight - tButton.fW;
		pData->tCloseButtonRect = xuiInternalSnapRect(tButton);
		fRight = tButton.fX - fGap;
	} else {
		memset(&pData->tCloseButtonRect, 0, sizeof(pData->tCloseButtonRect));
	}
	if ( pData->bShowTitleBar && pData->bShowMaximize ) {
		tButton.fX = fRight - tButton.fW;
		pData->tMaximizeButtonRect = xuiInternalSnapRect(tButton);
		fRight = tButton.fX - fGap;
	} else {
		memset(&pData->tMaximizeButtonRect, 0, sizeof(pData->tMaximizeButtonRect));
	}
	if ( pData->bShowTitleBar && pData->bShowCollapse ) {
		tButton.fX = fRight - tButton.fW;
		pData->tCollapseButtonRect = xuiInternalSnapRect(tButton);
	} else {
		memset(&pData->tCollapseButtonRect, 0, sizeof(pData->tCollapseButtonRect));
	}
	if ( pData->pClient != NULL ) {
		iRet = xuiWidgetSetVisible(pData->pClient, !pData->bCollapsed);
		if ( iRet != XUI_OK ) return iRet;
		iRet = xuiWidgetArrange(pData->pClient, pData->tClientRect);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pData->pCollapseButton != NULL ) {
		iRet = xuiWidgetSetVisible(pData->pCollapseButton, pData->bShowTitleBar && pData->bShowCollapse);
		if ( iRet != XUI_OK ) return iRet;
		iRet = xuiWidgetArrange(pData->pCollapseButton, pData->tCollapseButtonRect);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pData->pMaximizeButton != NULL ) {
		iRet = xuiWidgetSetVisible(pData->pMaximizeButton, pData->bShowTitleBar && pData->bShowMaximize);
		if ( iRet != XUI_OK ) return iRet;
		iRet = xuiWidgetArrange(pData->pMaximizeButton, pData->tMaximizeButtonRect);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( pData->pCloseButton != NULL ) {
		iRet = xuiWidgetSetVisible(pData->pCloseButton, pData->bShowTitleBar && pData->bShowClose);
		if ( iRet != XUI_OK ) return iRet;
		iRet = xuiWidgetArrange(pData->pCloseButton, pData->tCloseButtonRect);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiWindowEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_window_data_t* pData;
	xui_window_resolved_t tResolved;
	xui_context pContext;
	uint32_t iEdges;
	xui_rect_t tRect;
	int bLeftButton;
	int iPart;

	(void)pUser;
	pData = __xuiWindowGetData(pWidget);
	if ( (pWidget == NULL) || (pEvent == NULL) || (pData == NULL) || !pData->bOpen ) {
		return XUI_OK;
	}
	pContext = xuiWidgetGetContext(pWidget);
	bLeftButton = (pEvent->iButton == 0) || (pEvent->iButton == XUI_POINTER_BUTTON_LEFT);
	__xuiWindowResolve(pWidget, pData, &tResolved);
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_DOWN:
		if ( bLeftButton && __xuiWindowRectContains(xuiWidgetGetWorldRect(pWidget), pEvent->fX, pEvent->fY) ) {
			(void)__xuiWindowActivate(pWidget, pData, 1);
			if ( pEvent->iPhase == XUI_EVENT_PHASE_CAPTURE ) {
				return XUI_OK;
			}
			iEdges = __xuiWindowResizeEdgesAtData(pWidget, pData, &tResolved, pEvent->fX, pEvent->fY);
			if ( iEdges != 0u ) {
				pData->iInteractionEdges = (int)iEdges;
				iPart = XUI_WINDOW_PART_RESIZE;
			} else if ( __xuiWindowCanStartMove(pWidget, pData, &tResolved, pEvent) ) {
				pData->iInteractionEdges = -1;
				iPart = XUI_WINDOW_PART_TITLE_BAR;
			} else {
				if ( pEvent->pTarget == pWidget ) {
					return XUI_EVENT_DISPATCH_STOP;
				}
				return XUI_OK;
			}
			pData->fDragStartX = pEvent->fX;
			pData->fDragStartY = pEvent->fY;
			pData->tDragStartRect = xuiWidgetGetRect(pWidget);
			(void)__xuiWindowSetActivePart(pWidget, pData, iPart);
			(void)xuiSetPointerCapture(pContext, pWidget);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_MOVE:
		if ( (xuiGetPointerCapture(pContext) == pWidget) && (pData->iInteractionEdges != 0) ) {
			tRect = __xuiWindowBuildInteractionRect(pWidget, pData, pEvent->fX, pEvent->fY);
			(void)xuiWidgetSetRect(pWidget, tRect);
			if ( !pData->bCollapsed ) {
				pData->fExpandedHeight = tRect.fH;
			}
			pData->iChangeCount++;
			return XUI_EVENT_DISPATCH_STOP;
		}
		iEdges = __xuiWindowResizeEdgesAtData(pWidget, pData, &tResolved, pEvent->fX, pEvent->fY);
		if ( iEdges != 0u ) {
			(void)__xuiWindowSetHoverPart(pWidget, pData, XUI_WINDOW_PART_RESIZE);
		} else if ( __xuiWindowCanStartMove(pWidget, pData, &tResolved, pEvent) ) {
			(void)__xuiWindowSetHoverPart(pWidget, pData, XUI_WINDOW_PART_TITLE_BAR);
		} else if ( pData->iHoverPart == XUI_WINDOW_PART_RESIZE || pData->iHoverPart == XUI_WINDOW_PART_TITLE_BAR ) {
			(void)__xuiWindowSetHoverPart(pWidget, pData, XUI_WINDOW_PART_NONE);
		}
		break;
	case XUI_EVENT_POINTER_UP:
		if ( bLeftButton && (pData->iInteractionEdges != 0) ) {
			pData->iInteractionEdges = 0;
			(void)__xuiWindowSetActivePart(pWidget, pData, XUI_WINDOW_PART_NONE);
			(void)xuiReleasePointerCapture(pContext, pWidget);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_LEAVE:
		if ( pData->iHoverPart == XUI_WINDOW_PART_RESIZE || pData->iHoverPart == XUI_WINDOW_PART_TITLE_BAR ) {
			(void)__xuiWindowSetHoverPart(pWidget, pData, XUI_WINDOW_PART_NONE);
		}
		break;
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		pData->iInteractionEdges = 0;
		(void)__xuiWindowSetActivePart(pWidget, pData, XUI_WINDOW_PART_NONE);
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_KEY_DOWN:
		if ( (pEvent->iKey == XUI_KEY_ESCAPE) && (pData->iInteractionEdges != 0) ) {
			pData->iInteractionEdges = 0;
			(void)__xuiWindowSetActivePart(pWidget, pData, XUI_WINDOW_PART_NONE);
			(void)xuiReleasePointerCapture(pContext, pWidget);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_FOCUS:
		if ( xuiGetFocusWidget(pContext) == pWidget ) {
			(void)__xuiWindowActivate(pWidget, pData, 0);
		}
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_BLUR:
		(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		return XUI_EVENT_DISPATCH_STOP;
	default:
		break;
	}
	return XUI_OK;
}

static void __xuiWindowDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_MANUAL;
	pLayout->iWidthMode = XUI_SIZE_FIXED;
	pLayout->iHeightMode = XUI_SIZE_FIXED;
	pLayout->iFlowMode = XUI_FLOW_ABSOLUTE;
	pLayout->iDock = 0;
	pLayout->iOverflow = XUI_OVERFLOW_VISIBLE;
	pLayout->iAlignX = XUI_ALIGN_START;
	pLayout->iAlignY = XUI_ALIGN_START;
	pLayout->iTableRowSpan = 1;
	pLayout->iTableColumnSpan = 1;
	pLayout->iGridColumnCount = 1;
	pLayout->iLayer = XUI_LAYER_FLOATING;
	pLayout->iZIndex = XUI_WINDOW_Z_NORMAL;
	pLayout->fPreferredWidth = 300.0f;
	pLayout->fPreferredHeight = 200.0f;
	pLayout->fMinWidth = 120.0f;
	pLayout->fMinHeight = 64.0f;
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fShrink = 1.0f;
}

static void __xuiWindowDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static xui_thickness_t __xuiWindowThickness(float fLeft, float fTop, float fRight, float fBottom)
{
	xui_thickness_t tValue;

	tValue.fLeft = fLeft;
	tValue.fTop = fTop;
	tValue.fRight = fRight;
	tValue.fBottom = fBottom;
	return tValue;
}

static int __xuiWindowInitChildren(xui_widget pWidget, xui_window_data_t* pData)
{
	xui_cache_policy_t tPolicy;
	xui_widget pClient;
	xui_widget pCollapse;
	xui_widget pMaximize;
	xui_widget pClose;
	int iRet;

	pClient = NULL;
	pCollapse = NULL;
	pMaximize = NULL;
	pClose = NULL;
	iRet = xuiWidgetCreate(xuiWidgetGetContext(pWidget), &pClient);
	if ( iRet == XUI_OK ) iRet = xuiWidgetCreate(xuiWidgetGetContext(pWidget), &pCollapse);
	if ( iRet == XUI_OK ) iRet = xuiWidgetCreate(xuiWidgetGetContext(pWidget), &pMaximize);
	if ( iRet == XUI_OK ) iRet = xuiWidgetCreate(xuiWidgetGetContext(pWidget), &pClose);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pClient);
		xuiWidgetDestroy(pCollapse);
		xuiWidgetDestroy(pMaximize);
		xuiWidgetDestroy(pClose);
		return iRet;
	}
	__xuiWindowDefaultCachePolicy(&tPolicy);
	pData->pClient = pClient;
	pData->pCollapseButton = pCollapse;
	pData->pMaximizeButton = pMaximize;
	pData->pCloseButton = pClose;

	(void)xuiWidgetSetLayoutType(pClient, XUI_LAYOUT_COLUMN);
	(void)xuiWidgetSetSizeMode(pClient, XUI_SIZE_FILL, XUI_SIZE_FILL);
	(void)xuiWidgetSetFlowMode(pClient, XUI_FLOW_BLOCK);
	(void)xuiWidgetSetPadding(pClient, __xuiWindowThickness(10.0f, 10.0f, 10.0f, 10.0f));
	(void)xuiWidgetSetGap(pClient, 6.0f);
	(void)xuiWidgetSetOverflow(pClient, XUI_OVERFLOW_CLIP);
	(void)xuiWidgetSetCachePolicy(pClient, &tPolicy);
	(void)xuiWidgetSetCacheRenderCallback(pClient, __xuiWindowClientRender, pWidget);

	(void)xuiWidgetSetLayoutType(pCollapse, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetLayoutType(pMaximize, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetLayoutType(pClose, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetSizeMode(pCollapse, XUI_SIZE_FIXED, XUI_SIZE_FIXED);
	(void)xuiWidgetSetSizeMode(pMaximize, XUI_SIZE_FIXED, XUI_SIZE_FIXED);
	(void)xuiWidgetSetSizeMode(pClose, XUI_SIZE_FIXED, XUI_SIZE_FIXED);
	(void)xuiWidgetSetFocusable(pCollapse, 0);
	(void)xuiWidgetSetFocusable(pMaximize, 0);
	(void)xuiWidgetSetFocusable(pClose, 0);
	(void)xuiWidgetSetTabStop(pCollapse, 0);
	(void)xuiWidgetSetTabStop(pMaximize, 0);
	(void)xuiWidgetSetTabStop(pClose, 0);
	(void)xuiWidgetSetCachePolicy(pCollapse, &tPolicy);
	(void)xuiWidgetSetCachePolicy(pMaximize, &tPolicy);
	(void)xuiWidgetSetCachePolicy(pClose, &tPolicy);
	(void)xuiWidgetSetCacheRenderCallback(pCollapse, __xuiWindowButtonRender, pWidget);
	(void)xuiWidgetSetCacheRenderCallback(pMaximize, __xuiWindowButtonRender, pWidget);
	(void)xuiWidgetSetCacheRenderCallback(pClose, __xuiWindowButtonRender, pWidget);
	(void)xuiWidgetSetEventHandler(pCollapse, XUI_EVENT_POINTER_ENTER, __xuiWindowButtonEvent, pWidget);
	(void)xuiWidgetSetEventHandler(pCollapse, XUI_EVENT_POINTER_LEAVE, __xuiWindowButtonEvent, pWidget);
	(void)xuiWidgetSetEventHandler(pCollapse, XUI_EVENT_POINTER_MOVE, __xuiWindowButtonEvent, pWidget);
	(void)xuiWidgetSetEventHandler(pCollapse, XUI_EVENT_POINTER_DOWN, __xuiWindowButtonEvent, pWidget);
	(void)xuiWidgetSetEventHandler(pCollapse, XUI_EVENT_POINTER_UP, __xuiWindowButtonEvent, pWidget);
	(void)xuiWidgetSetEventHandler(pCollapse, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiWindowButtonEvent, pWidget);
	(void)xuiWidgetSetEventHandler(pMaximize, XUI_EVENT_POINTER_ENTER, __xuiWindowButtonEvent, pWidget);
	(void)xuiWidgetSetEventHandler(pMaximize, XUI_EVENT_POINTER_LEAVE, __xuiWindowButtonEvent, pWidget);
	(void)xuiWidgetSetEventHandler(pMaximize, XUI_EVENT_POINTER_MOVE, __xuiWindowButtonEvent, pWidget);
	(void)xuiWidgetSetEventHandler(pMaximize, XUI_EVENT_POINTER_DOWN, __xuiWindowButtonEvent, pWidget);
	(void)xuiWidgetSetEventHandler(pMaximize, XUI_EVENT_POINTER_UP, __xuiWindowButtonEvent, pWidget);
	(void)xuiWidgetSetEventHandler(pMaximize, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiWindowButtonEvent, pWidget);
	(void)xuiWidgetSetEventHandler(pClose, XUI_EVENT_POINTER_ENTER, __xuiWindowButtonEvent, pWidget);
	(void)xuiWidgetSetEventHandler(pClose, XUI_EVENT_POINTER_LEAVE, __xuiWindowButtonEvent, pWidget);
	(void)xuiWidgetSetEventHandler(pClose, XUI_EVENT_POINTER_MOVE, __xuiWindowButtonEvent, pWidget);
	(void)xuiWidgetSetEventHandler(pClose, XUI_EVENT_POINTER_DOWN, __xuiWindowButtonEvent, pWidget);
	(void)xuiWidgetSetEventHandler(pClose, XUI_EVENT_POINTER_UP, __xuiWindowButtonEvent, pWidget);
	(void)xuiWidgetSetEventHandler(pClose, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiWindowButtonEvent, pWidget);

	iRet = xuiWidgetAddChild(pWidget, pClient);
	if ( iRet == XUI_OK ) iRet = xuiWidgetAddChild(pWidget, pCollapse);
	if ( iRet == XUI_OK ) iRet = xuiWidgetAddChild(pWidget, pMaximize);
	if ( iRet == XUI_OK ) iRet = xuiWidgetAddChild(pWidget, pClose);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pClient);
		xuiWidgetDestroy(pCollapse);
		xuiWidgetDestroy(pMaximize);
		xuiWidgetDestroy(pClose);
		pData->pClient = NULL;
		pData->pCollapseButton = NULL;
		pData->pMaximizeButton = NULL;
		pData->pCloseButton = NULL;
		return iRet;
	}
	return XUI_OK;
}

static void __xuiWindowDefaults(xui_window_data_t* pData)
{
	memset(pData, 0, sizeof(*pData));
	pData->bOpen = 1;
	pData->bShowTitleBar = 1;
	pData->bMovable = 1;
	pData->bResizable = 1;
	pData->iResizeEdges = __xuiWindowDefaultResizeEdges(pData);
	pData->bShowCollapse = 1;
	pData->bShowMaximize = 1;
	pData->bShowClose = 1;
	pData->fExpandedHeight = 200.0f;
	pData->fTitleBarHeight = 30.0f;
	pData->fBorderWidth = 1.0f;
	pData->fResizeGrip = 6.0f;
	pData->fButtonSize = 20.0f;
	pData->fIconSize = 16.0f;
	pData->fMinWidth = 160.0f;
	pData->fMinHeight = 120.0f;
	pData->iBackgroundColor = XUI_COLOR_RGBA(248, 252, 255, 255);
	pData->iClientColor = XUI_COLOR_RGBA(248, 252, 255, 255);
	pData->iTitleBarColor = XUI_COLOR_RGBA(226, 240, 250, 255);
	pData->iInactiveTitleBarColor = XUI_COLOR_RGBA(236, 245, 251, 255);
	pData->iTitleTextColor = XUI_COLOR_RGBA(34, 59, 84, 255);
	pData->iInactiveTitleTextColor = XUI_COLOR_RGBA(102, 123, 142, 255);
	pData->iBorderColor = XUI_COLOR_RGBA(151, 178, 205, 255);
	pData->iActiveBorderColor = XUI_COLOR_RGBA(47, 128, 237, 255);
	pData->iButtonColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	pData->iButtonHoverColor = XUI_COLOR_RGBA(206, 225, 243, 255);
	pData->iButtonActiveColor = XUI_COLOR_RGBA(184, 209, 235, 255);
	pData->iCloseHoverColor = XUI_COLOR_RGBA(255, 226, 226, 255);
	pData->iCloseActiveColor = XUI_COLOR_RGBA(244, 205, 205, 255);
}

static void __xuiWindowApplyDesc(xui_window_data_t* pData, const xui_window_desc_t* pDesc)
{
	if ( (pData == NULL) || (pDesc == NULL) ) {
		return;
	}
	if ( pDesc->sTitle != NULL ) {
		strncpy(pData->sTitle, pDesc->sTitle, sizeof(pData->sTitle) - 1u);
		pData->sTitle[sizeof(pData->sTitle) - 1u] = '\0';
	}
	pData->pFont = pDesc->pFont;
	pData->pIconSurface = pDesc->pIconSurface;
	pData->tIconSrc = pDesc->tIconSrc;
	pData->bHasIcon = (pDesc->bHasIcon || (pDesc->pIconSurface != NULL)) ? 1 : 0;
	pData->bOpen = pDesc->bClosed ? 0 : 1;
	pData->bTopMost = pDesc->bTopMost ? 1 : 0;
	pData->bShowTitleBar = pDesc->bNoTitleBar ? 0 : 1;
	pData->bMovable = pDesc->bNotMovable ? 0 : 1;
	pData->bDragAnywhere = pDesc->bDragAnywhere ? 1 : 0;
	pData->bResizable = pDesc->bNotResizable ? 0 : 1;
	pData->bShowCollapse = pDesc->bHideCollapse ? 0 : 1;
	pData->bShowMaximize = pDesc->bHideMaximize ? 0 : 1;
	pData->bShowClose = pDesc->bHideClose ? 0 : 1;
	pData->bCollapsed = pDesc->bCollapsed ? 1 : 0;
	pData->bMaximized = pDesc->bMaximized ? 1 : 0;
	if ( pDesc->iResizeEdges != 0u ) {
		pData->bResizeEdgesExplicit = 1;
		pData->iResizeEdges = pDesc->iResizeEdges & XUI_WINDOW_EDGE_ALL;
	} else {
		__xuiWindowApplyDefaultResizeEdges(pData);
	}
	if ( pDesc->fTitleBarHeight > 0.0f ) pData->fTitleBarHeight = pDesc->fTitleBarHeight;
	if ( pDesc->fBorderWidth > 0.0f ) pData->fBorderWidth = pDesc->fBorderWidth;
	if ( pDesc->fResizeGrip > 0.0f ) pData->fResizeGrip = pDesc->fResizeGrip;
	if ( pDesc->fButtonSize > 0.0f ) pData->fButtonSize = pDesc->fButtonSize;
	if ( pDesc->fIconSize > 0.0f ) pData->fIconSize = pDesc->fIconSize;
	if ( pDesc->fMinWidth > 0.0f ) pData->fMinWidth = pDesc->fMinWidth;
	if ( pDesc->fMinHeight > 0.0f ) pData->fMinHeight = pDesc->fMinHeight;
	if ( __xuiWindowAlpha(pDesc->iBackgroundColor) != 0u ) pData->iBackgroundColor = pDesc->iBackgroundColor;
	if ( __xuiWindowAlpha(pDesc->iClientColor) != 0u ) pData->iClientColor = pDesc->iClientColor;
	if ( __xuiWindowAlpha(pDesc->iTitleBarColor) != 0u ) pData->iTitleBarColor = pDesc->iTitleBarColor;
	if ( __xuiWindowAlpha(pDesc->iInactiveTitleBarColor) != 0u ) pData->iInactiveTitleBarColor = pDesc->iInactiveTitleBarColor;
	if ( __xuiWindowAlpha(pDesc->iTitleTextColor) != 0u ) pData->iTitleTextColor = pDesc->iTitleTextColor;
	if ( __xuiWindowAlpha(pDesc->iInactiveTitleTextColor) != 0u ) pData->iInactiveTitleTextColor = pDesc->iInactiveTitleTextColor;
	if ( __xuiWindowAlpha(pDesc->iBorderColor) != 0u ) pData->iBorderColor = pDesc->iBorderColor;
	if ( __xuiWindowAlpha(pDesc->iActiveBorderColor) != 0u ) pData->iActiveBorderColor = pDesc->iActiveBorderColor;
	if ( pDesc->iButtonColor != 0u ) pData->iButtonColor = pDesc->iButtonColor;
	if ( pDesc->iButtonHoverColor != 0u ) pData->iButtonHoverColor = pDesc->iButtonHoverColor;
	if ( pDesc->iButtonActiveColor != 0u ) pData->iButtonActiveColor = pDesc->iButtonActiveColor;
	if ( pDesc->iCloseHoverColor != 0u ) pData->iCloseHoverColor = pDesc->iCloseHoverColor;
	if ( pDesc->iCloseActiveColor != 0u ) pData->iCloseActiveColor = pDesc->iCloseActiveColor;
}

static int __xuiWindowInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_window_data_t* pData;
	const xui_window_desc_t* pDesc;
	xui_cache_policy_t tPolicy;
	xui_rect_t tInitial;
	int bOpen;
	int iRet;

	(void)pUser;
	if ( (pWidget == NULL) || (pTypeData == NULL) || !__xuiWindowDescValid((const xui_window_desc_t*)pCreateData) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = (xui_window_data_t*)pTypeData;
	pDesc = (const xui_window_desc_t*)pCreateData;
	__xuiWindowDefaults(pData);
	__xuiWindowApplyDesc(pData, pDesc);
	bOpen = pData->bOpen;
	pData->bOpen = 0;
	iRet = __xuiWindowInitChildren(pWidget, pData);
	if ( iRet != XUI_OK ) return iRet;
	__xuiWindowDefaultCachePolicy(&tPolicy);
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 0);
	(void)xuiWidgetSetFocusScope(pWidget, 1);
	(void)xuiWidgetSetLayoutType(pWidget, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetFlowMode(pWidget, XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_VISIBLE);
	(void)xuiWidgetSetSizeMode(pWidget, XUI_SIZE_FIXED, XUI_SIZE_FIXED);
	(void)xuiWidgetSetPreferredSize(pWidget, (xui_vec2_t){300.0f, 200.0f});
	(void)xuiWidgetSetMinSize(pWidget, (xui_vec2_t){pData->fMinWidth, pData->fMinHeight});
	(void)xuiWidgetSetCachePolicy(pWidget, &tPolicy);
	(void)xuiWidgetSetCacheRenderCallback(pWidget, __xuiWindowCacheRender, NULL);
	(void)xuiWidgetSetEventCallback(pWidget, __xuiWindowEvent, NULL);
	(void)__xuiWindowApplyLayerAndZ(pWidget, pData);
	tInitial = xuiWidgetGetRect(pWidget);
	if ( tInitial.fW <= 0.0f ) tInitial.fW = 300.0f;
	if ( tInitial.fH <= 0.0f ) tInitial.fH = pData->bCollapsed ? 32.0f : 200.0f;
	(void)xuiWidgetSetRect(pWidget, tInitial);
	if ( bOpen ) {
		iRet = __xuiWindowSetOpenInternal(pWidget, pData, 1, 1);
		if ( iRet != XUI_OK ) return iRet;
	} else {
		(void)xuiWidgetSetVisible(pWidget, 0);
	}
	return XUI_OK;
}

static void __xuiWindowDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_window_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_window_data_t*)pTypeData;
	if ( pData != NULL ) {
		memset(pData, 0, sizeof(*pData));
	}
}

static void __xuiWindowRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
{
	xui_style_property_info_t tInfo;

	memset(&tInfo, 0, sizeof(tInfo));
	tInfo.iSize = sizeof(tInfo);
	tInfo.sName = sName;
	tInfo.iValueType = iValueType;
	tInfo.iDirtyFlags = iDirtyFlags;
	tInfo.iFlags = iFlags;
	tInfo.pWidgetType = pType;
	(void)xuiStyleRegisterProperty(pContext, &tInfo, NULL);
}

static void __xuiWindowRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;
	uint32_t iLayoutDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	iLayoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiWindowRegisterStyleProperty(pContext, pType, "font.name", XUI_STYLE_VALUE_STRING, iLayoutDirty, XUI_STYLE_PROPERTY_INHERITED);
	__xuiWindowRegisterStyleProperty(pContext, pType, "window.background.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiWindowRegisterStyleProperty(pContext, pType, "window.client.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiWindowRegisterStyleProperty(pContext, pType, "window.titlebar.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiWindowRegisterStyleProperty(pContext, pType, "window.titlebar.inactive_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiWindowRegisterStyleProperty(pContext, pType, "window.title.text_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiWindowRegisterStyleProperty(pContext, pType, "window.title.inactive_text_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiWindowRegisterStyleProperty(pContext, pType, "window.border.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiWindowRegisterStyleProperty(pContext, pType, "window.border.active_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiWindowRegisterStyleProperty(pContext, pType, "window.button.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiWindowRegisterStyleProperty(pContext, pType, "window.button.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiWindowRegisterStyleProperty(pContext, pType, "window.button.active_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiWindowRegisterStyleProperty(pContext, pType, "window.close.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiWindowRegisterStyleProperty(pContext, pType, "window.close.active_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiWindowRegisterStyleProperty(pContext, pType, "window.titlebar.height", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiWindowRegisterStyleProperty(pContext, pType, "window.border.width", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiWindowRegisterStyleProperty(pContext, pType, "window.resize_grip", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiWindowRegisterStyleProperty(pContext, pType, "window.button.size", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiWindowRegisterStyleProperty(pContext, pType, "window.icon.size", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiWindowRegisterStyleProperty(pContext, pType, "window.min_width", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiWindowRegisterStyleProperty(pContext, pType, "window.min_height", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
}

XUI_API xui_widget_type xuiWindowGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "window");
	if ( pType != NULL ) {
		__xuiWindowRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "window";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_window_data_t);
	tDesc.onInit = __xuiWindowInit;
	tDesc.onDestroy = __xuiWindowDestroy;
	tDesc.onLayoutMeasure = __xuiWindowMeasure;
	tDesc.onLayoutArrange = __xuiWindowArrange;
	tDesc.onCacheRender = __xuiWindowCacheRender;
	__xuiWindowDefaultLayout(&tDesc.tLayout);
	__xuiWindowDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	__xuiWindowRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiWindowCreate(xui_context pContext, xui_widget* ppWidget, const xui_window_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( !xuiInternalContextIsValid(pContext) || (ppWidget == NULL) || !__xuiWindowDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiWindowGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiWindowSetClose(xui_widget pWidget, xui_window_close_proc onClose, void* pUser)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onClose = onClose;
	pData->pCloseUser = pUser;
	return XUI_OK;
}

XUI_API xui_widget xuiWindowGetClientWidget(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	return (pData != NULL) ? pData->pClient : NULL;
}

XUI_API xui_widget xuiWindowGetCollapseButtonWidget(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	return (pData != NULL) ? pData->pCollapseButton : NULL;
}

XUI_API xui_widget xuiWindowGetMaximizeButtonWidget(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	return (pData != NULL) ? pData->pMaximizeButton : NULL;
}

XUI_API xui_widget xuiWindowGetCloseButtonWidget(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	return (pData != NULL) ? pData->pCloseButton : NULL;
}

XUI_API int xuiWindowAddChild(xui_widget pWidget, xui_widget pChild)
{
	xui_widget pClient;
	int iLayer;
	int iZIndex;
	int iRet;

	pClient = xuiWindowGetClientWidget(pWidget);
	if ( (pClient == NULL) || (pChild == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = xuiWidgetAddChild(pClient, pChild);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iRet = xuiWidgetGetLayer(pWidget, &iLayer, &iZIndex);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return __xuiWindowSetTreeLayer(pChild, iLayer, iZIndex);
}

XUI_API int xuiWindowInsertBefore(xui_widget pWidget, xui_widget pChild, xui_widget pBefore)
{
	xui_widget pClient;
	int iLayer;
	int iZIndex;
	int iRet;

	pClient = xuiWindowGetClientWidget(pWidget);
	if ( (pClient == NULL) || (pChild == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = xuiWidgetInsertBefore(pClient, pChild, pBefore);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iRet = xuiWidgetGetLayer(pWidget, &iLayer, &iZIndex);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return __xuiWindowSetTreeLayer(pChild, iLayer, iZIndex);
}

XUI_API int xuiWindowSetTitle(xui_widget pWidget, const char* sTitle)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( sTitle == NULL ) sTitle = "";
	strncpy(pData->sTitle, sTitle, sizeof(pData->sTitle) - 1u);
	pData->sTitle[sizeof(pData->sTitle) - 1u] = '\0';
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API const char* xuiWindowGetTitle(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	return (pData != NULL) ? pData->sTitle : NULL;
}

XUI_API int xuiWindowSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pFont = pFont;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_font xuiWindowGetFont(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API int xuiWindowSetIcon(xui_widget pWidget, xui_surface pSurface, xui_rect_t tSrc)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	if ( pData == NULL || tSrc.fW < 0.0f || tSrc.fH < 0.0f ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->pIconSurface = pSurface;
	pData->tIconSrc = tSrc;
	pData->bHasIcon = (pSurface != NULL) ? 1 : 0;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_surface xuiWindowGetIconSurface(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	return (pData != NULL) ? pData->pIconSurface : NULL;
}

XUI_API xui_rect_t xuiWindowGetIconSource(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	xui_rect_t tEmpty;
	memset(&tEmpty, 0, sizeof(tEmpty));
	return (pData != NULL) ? pData->tIconSrc : tEmpty;
}

static int __xuiWindowSetOpenInternal(xui_widget pWidget, xui_window_data_t* pData, int bOpen, int bRaise)
{
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	bOpen = bOpen ? 1 : 0;
	if ( pData->bOpen == bOpen ) {
		if ( bOpen && bRaise ) {
			return __xuiWindowActivate(pWidget, pData, 1);
		}
		return XUI_OK;
	}
	pData->bOpen = bOpen;
	pData->iChangeCount++;
	if ( bOpen ) {
		iRet = xuiWidgetSetVisible(pWidget, 1);
		if ( iRet != XUI_OK ) return iRet;
		iRet = xuiWidgetSetEnabled(pWidget, 1);
		if ( iRet != XUI_OK ) return iRet;
		iRet = xuiOverlayAttach(xuiWidgetGetContext(pWidget), NULL, pWidget, XUI_LAYER_FLOATING, pData->bTopMost ? XUI_WINDOW_Z_TOPMOST : XUI_WINDOW_Z_NORMAL);
		if ( iRet != XUI_OK ) return iRet;
		return __xuiWindowActivate(pWidget, pData, bRaise);
	}
	if ( pData->bActive ) {
		pData->bActive = 0;
	}
	(void)xuiReleasePointerCapture(xuiWidgetGetContext(pWidget), pWidget);
	iRet = xuiWidgetSetVisible(pWidget, 0);
	if ( iRet != XUI_OK ) return iRet;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiWindowSetOpen(xui_widget pWidget, int bOpen)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiWindowSetOpenInternal(pWidget, pData, bOpen, 1);
}

XUI_API int xuiWindowIsOpen(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	return (pData != NULL) ? pData->bOpen : 0;
}

XUI_API int xuiWindowSetShowTitleBar(xui_widget pWidget, int bShow)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	bShow = bShow ? 1 : 0;
	if ( pData->bShowTitleBar == bShow ) return XUI_OK;
	pData->bShowTitleBar = bShow;
	__xuiWindowApplyDefaultResizeEdges(pData);
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiWindowGetShowTitleBar(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	return (pData != NULL) ? pData->bShowTitleBar : 0;
}

XUI_API int xuiWindowSetMovable(xui_widget pWidget, int bEnabled)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bMovable = bEnabled ? 1 : 0;
	return XUI_OK;
}

XUI_API int xuiWindowIsMovable(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	return (pData != NULL) ? pData->bMovable : 0;
}

XUI_API int xuiWindowSetDragAnywhere(xui_widget pWidget, int bEnabled)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bDragAnywhere = bEnabled ? 1 : 0;
	__xuiWindowApplyDefaultResizeEdges(pData);
	return XUI_OK;
}

XUI_API int xuiWindowIsDragAnywhere(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	return (pData != NULL) ? pData->bDragAnywhere : 0;
}

XUI_API int xuiWindowSetResizable(xui_widget pWidget, int bEnabled)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bResizable = bEnabled ? 1 : 0;
	return XUI_OK;
}

XUI_API int xuiWindowIsResizable(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	return (pData != NULL) ? pData->bResizable : 0;
}

XUI_API int xuiWindowSetResizeEdges(xui_widget pWidget, uint32_t iEdges)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bResizeEdgesExplicit = 1;
	pData->iResizeEdges = iEdges & XUI_WINDOW_EDGE_ALL;
	return XUI_OK;
}

XUI_API uint32_t xuiWindowGetResizeEdges(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	return (pData != NULL) ? pData->iResizeEdges : 0u;
}

XUI_API int xuiWindowSetShowCollapse(xui_widget pWidget, int bShow)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bShowCollapse = bShow ? 1 : 0;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiWindowGetShowCollapse(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	return (pData != NULL) ? pData->bShowCollapse : 0;
}

XUI_API int xuiWindowSetShowMaximize(xui_widget pWidget, int bShow)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bShowMaximize = bShow ? 1 : 0;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiWindowGetShowMaximize(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	return (pData != NULL) ? pData->bShowMaximize : 0;
}

XUI_API int xuiWindowSetShowClose(xui_widget pWidget, int bShow)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bShowClose = bShow ? 1 : 0;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiWindowGetShowClose(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	return (pData != NULL) ? pData->bShowClose : 0;
}

XUI_API int xuiWindowSetCollapsed(xui_widget pWidget, int bCollapsed)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	xui_window_resolved_t tResolved;
	xui_rect_t tRect;
	float fCollapsedHeight;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	bCollapsed = bCollapsed ? 1 : 0;
	if ( pData->bCollapsed == bCollapsed ) return XUI_OK;
	__xuiWindowResolve(pWidget, pData, &tResolved);
	tRect = xuiWidgetGetRect(pWidget);
	fCollapsedHeight = __xuiWindowCollapsedHeight(pData, &tResolved);
	if ( bCollapsed ) {
		if ( tRect.fH > fCollapsedHeight ) pData->fExpandedHeight = tRect.fH;
		tRect.fH = fCollapsedHeight;
	} else {
		if ( pData->fExpandedHeight < fCollapsedHeight + 48.0f ) pData->fExpandedHeight = fCollapsedHeight + 96.0f;
		tRect.fH = pData->fExpandedHeight;
	}
	pData->bCollapsed = bCollapsed;
	pData->iChangeCount++;
	(void)__xuiWindowApplyRect(pWidget, pData, tRect);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiWindowIsCollapsed(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	return (pData != NULL) ? pData->bCollapsed : 0;
}

XUI_API int xuiWindowSetMaximized(xui_widget pWidget, int bMaximized)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	xui_rect_t tRect;
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	bMaximized = bMaximized ? 1 : 0;
	if ( pData->bMaximized == bMaximized ) {
		if ( bMaximized && pData->bCollapsed ) {
			pData->bCollapsed = 0;
			pData->iChangeCount++;
			tRect = __xuiWindowParentRect(pWidget);
			(void)__xuiWindowApplyRect(pWidget, pData, tRect);
			return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		}
		return XUI_OK;
	}
	if ( bMaximized ) {
		pData->tRestoreRect = xuiWidgetGetRect(pWidget);
		pData->bCollapsed = 0;
		pData->bMaximized = 1;
		tRect = __xuiWindowParentRect(pWidget);
		(void)__xuiWindowApplyRect(pWidget, pData, tRect);
	} else {
		pData->bMaximized = 0;
		if ( pData->tRestoreRect.fW > 0.0f && pData->tRestoreRect.fH > 0.0f ) {
			(void)__xuiWindowApplyRect(pWidget, pData, pData->tRestoreRect);
		}
	}
	pData->iChangeCount++;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiWindowIsMaximized(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	return (pData != NULL) ? pData->bMaximized : 0;
}

XUI_API int xuiWindowBringToFront(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiWindowActivate(pWidget, pData, 1);
}

XUI_API int xuiWindowSetTopMost(xui_widget pWidget, int bTopMost)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	bTopMost = bTopMost ? 1 : 0;
	if ( pData->bTopMost == bTopMost ) return XUI_OK;
	pData->bTopMost = bTopMost;
	pData->iChangeCount++;
	(void)__xuiWindowApplyLayerAndZ(pWidget, pData);
	if ( pData->bOpen ) {
		(void)xuiOverlayBringToFront(pWidget);
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiWindowIsTopMost(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	return (pData != NULL) ? pData->bTopMost : 0;
}

XUI_API int xuiWindowIsActive(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	return (pData != NULL) ? pData->bActive : 0;
}

XUI_API xui_widget xuiWindowGetActive(xui_context pContext)
{
	xui_widget pFound;
	if ( !xuiInternalContextIsValid(pContext) ) return NULL;
	pFound = __xuiWindowFindActiveInTree(xuiOverlayRoot(pContext));
	if ( pFound != NULL ) return pFound;
	return __xuiWindowFindActiveInTree(xuiGetRootWidget(pContext));
}

XUI_API int xuiWindowSetChrome(xui_widget pWidget, float fTitleBarHeight, float fBorderWidth, float fResizeGrip, float fButtonSize)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( fTitleBarHeight >= 20.0f ) pData->fTitleBarHeight = fTitleBarHeight;
	if ( fBorderWidth >= 0.0f ) pData->fBorderWidth = fBorderWidth;
	if ( fResizeGrip >= 2.0f ) pData->fResizeGrip = fResizeGrip;
	if ( fButtonSize >= 12.0f ) pData->fButtonSize = fButtonSize;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiWindowGetChrome(xui_widget pWidget, float* pTitleBarHeight, float* pBorderWidth, float* pResizeGrip, float* pButtonSize)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pTitleBarHeight != NULL ) *pTitleBarHeight = pData->fTitleBarHeight;
	if ( pBorderWidth != NULL ) *pBorderWidth = pData->fBorderWidth;
	if ( pResizeGrip != NULL ) *pResizeGrip = pData->fResizeGrip;
	if ( pButtonSize != NULL ) *pButtonSize = pData->fButtonSize;
	return XUI_OK;
}

XUI_API int xuiWindowSetIconSize(xui_widget pWidget, float fIconSize)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	if ( pData == NULL || fIconSize < 0.0f ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( fIconSize >= 1.0f ) pData->fIconSize = fIconSize;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiWindowGetIconSize(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	return (pData != NULL) ? pData->fIconSize : 0.0f;
}

XUI_API int xuiWindowSetMinSize(xui_widget pWidget, float fWidth, float fHeight)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	if ( pData == NULL || fWidth < 0.0f || fHeight < 0.0f ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fMinWidth = fWidth;
	pData->fMinHeight = fHeight;
	(void)xuiWidgetSetMinSize(pWidget, (xui_vec2_t){fWidth, fHeight});
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiWindowGetMinSize(xui_widget pWidget, float* pWidth, float* pHeight)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pWidth != NULL ) *pWidth = pData->fMinWidth;
	if ( pHeight != NULL ) *pHeight = pData->fMinHeight;
	return XUI_OK;
}

XUI_API int xuiWindowSetColors(xui_widget pWidget, uint32_t iBackground, uint32_t iClient, uint32_t iTitleBar, uint32_t iTitleText, uint32_t iBorder, uint32_t iButtonNormal, uint32_t iButtonHover, uint32_t iButtonActive)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iBackgroundColor = iBackground;
	pData->iClientColor = iClient;
	pData->iTitleBarColor = iTitleBar;
	pData->iTitleTextColor = iTitleText;
	pData->iBorderColor = iBorder;
	pData->iActiveBorderColor = iBorder;
	pData->iButtonColor = iButtonNormal;
	pData->iButtonHoverColor = iButtonHover;
	pData->iButtonActiveColor = iButtonActive;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiWindowGetColors(xui_widget pWidget, uint32_t* pBackground, uint32_t* pClient, uint32_t* pTitleBar, uint32_t* pTitleText, uint32_t* pBorder, uint32_t* pButtonNormal, uint32_t* pButtonHover, uint32_t* pButtonActive)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pBackground != NULL ) *pBackground = pData->iBackgroundColor;
	if ( pClient != NULL ) *pClient = pData->iClientColor;
	if ( pTitleBar != NULL ) *pTitleBar = pData->iTitleBarColor;
	if ( pTitleText != NULL ) *pTitleText = pData->iTitleTextColor;
	if ( pBorder != NULL ) *pBorder = pData->iBorderColor;
	if ( pButtonNormal != NULL ) *pButtonNormal = pData->iButtonColor;
	if ( pButtonHover != NULL ) *pButtonHover = pData->iButtonHoverColor;
	if ( pButtonActive != NULL ) *pButtonActive = pData->iButtonActiveColor;
	return XUI_OK;
}

XUI_API int xuiWindowSetStateColors(xui_widget pWidget, uint32_t iInactiveTitleBar, uint32_t iInactiveTitleText, uint32_t iActiveBorder, uint32_t iCloseHover, uint32_t iCloseActive)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iInactiveTitleBarColor = iInactiveTitleBar;
	pData->iInactiveTitleTextColor = iInactiveTitleText;
	pData->iActiveBorderColor = iActiveBorder;
	pData->iCloseHoverColor = iCloseHover;
	pData->iCloseActiveColor = iCloseActive;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiWindowGetStateColors(xui_widget pWidget, uint32_t* pInactiveTitleBar, uint32_t* pInactiveTitleText, uint32_t* pActiveBorder, uint32_t* pCloseHover, uint32_t* pCloseActive)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pInactiveTitleBar != NULL ) *pInactiveTitleBar = pData->iInactiveTitleBarColor;
	if ( pInactiveTitleText != NULL ) *pInactiveTitleText = pData->iInactiveTitleTextColor;
	if ( pActiveBorder != NULL ) *pActiveBorder = pData->iActiveBorderColor;
	if ( pCloseHover != NULL ) *pCloseHover = pData->iCloseHoverColor;
	if ( pCloseActive != NULL ) *pCloseActive = pData->iCloseActiveColor;
	return XUI_OK;
}

XUI_API xui_rect_t xuiWindowGetTitleBarRect(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	xui_rect_t tEmpty;
	memset(&tEmpty, 0, sizeof(tEmpty));
	return (pData != NULL) ? pData->tTitleBarRect : tEmpty;
}

XUI_API xui_rect_t xuiWindowGetClientRect(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	xui_rect_t tEmpty;
	memset(&tEmpty, 0, sizeof(tEmpty));
	return (pData != NULL) ? pData->tClientRect : tEmpty;
}

XUI_API xui_rect_t xuiWindowGetCollapseButtonRect(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	xui_rect_t tEmpty;
	memset(&tEmpty, 0, sizeof(tEmpty));
	return (pData != NULL) ? pData->tCollapseButtonRect : tEmpty;
}

XUI_API xui_rect_t xuiWindowGetMaximizeButtonRect(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	xui_rect_t tEmpty;
	memset(&tEmpty, 0, sizeof(tEmpty));
	return (pData != NULL) ? pData->tMaximizeButtonRect : tEmpty;
}

XUI_API xui_rect_t xuiWindowGetCloseButtonRect(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	xui_rect_t tEmpty;
	memset(&tEmpty, 0, sizeof(tEmpty));
	return (pData != NULL) ? pData->tCloseButtonRect : tEmpty;
}

XUI_API uint32_t xuiWindowGetResizeEdgesAt(xui_widget pWidget, float fX, float fY)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	xui_window_resolved_t tResolved;
	if ( pData == NULL ) return 0u;
	__xuiWindowResolve(pWidget, pData, &tResolved);
	return __xuiWindowResizeEdgesAtData(pWidget, pData, &tResolved, fX, fY);
}

XUI_API int xuiWindowGetHoverPart(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	return (pData != NULL) ? pData->iHoverPart : XUI_WINDOW_PART_NONE;
}

XUI_API int xuiWindowGetActivePart(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	return (pData != NULL) ? pData->iActivePart : XUI_WINDOW_PART_NONE;
}

XUI_API uint32_t xuiWindowGetState(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	uint32_t iState;
	if ( pData == NULL ) return 0u;
	iState = 0u;
	if ( pData->bOpen ) iState |= XUI_WINDOW_STATE_OPEN;
	if ( pData->bCollapsed ) iState |= XUI_WINDOW_STATE_COLLAPSED;
	if ( pData->bMaximized ) iState |= XUI_WINDOW_STATE_MAXIMIZED;
	if ( pData->bActive ) iState |= XUI_WINDOW_STATE_ACTIVE;
	if ( pData->bTopMost ) iState |= XUI_WINDOW_STATE_TOPMOST;
	if ( pData->iInteractionEdges == -1 ) iState |= XUI_WINDOW_STATE_DRAGGING;
	if ( pData->iInteractionEdges > 0 ) iState |= XUI_WINDOW_STATE_RESIZING;
	return iState;
}

XUI_API int xuiWindowGetChangeCount(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}

XUI_API int xuiWindowGetCloseCount(xui_widget pWidget)
{
	xui_window_data_t* pData = __xuiWindowGetData(pWidget);
	return (pData != NULL) ? pData->iCloseCount : 0;
}
