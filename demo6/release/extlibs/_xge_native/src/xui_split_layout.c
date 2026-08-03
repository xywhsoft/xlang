#include "xui_internal.h"

#include <string.h>

typedef struct xui_split_layout_pane_t {
	xui_widget pWidget;
	float fWeight;
	float fFixedSize;
	float fResolvedSize;
	float fMinSize;
	float fMaxSize;
	int iMode;
} xui_split_layout_pane_t;

typedef struct xui_split_layout_divider_t {
	xui_widget pWidget;
	xui_rect_t tLayoutRect;
	xui_rect_t tVisualRect;
	xui_rect_t tHitRect;
} xui_split_layout_divider_t;

typedef struct xui_split_layout_data_t {
	xui_split_layout_change_proc onChange;
	void* pChangeUser;
	xui_split_layout_pane_t arrPanes[XUI_SPLIT_LAYOUT_MAX_PANES];
	xui_split_layout_divider_t arrDividers[XUI_SPLIT_LAYOUT_MAX_PANES - 1];
	xui_widget pShadowWidget;
	xui_rect_t tContentRect;
	xui_rect_t tShadowRect;
	int iOrientation;
	int iPaneCount;
	int iHoverDivider;
	int iActiveDivider;
	int iChangeCount;
	int bShadowDrag;
	float fDividerSize;
	float fDividerVisualSize;
	float fDividerHitSize;
	float fResolvedDividerSize;
	float fResolvedDividerVisualSize;
	float fResolvedDividerHitSize;
	float fDragStartMouse;
	float fDragCurrentMouse;
	float fDragOffset;
	uint32_t iDividerColor;
	uint32_t iDividerHoverColor;
	uint32_t iDividerActiveColor;
	uint32_t iShadowColor;
} xui_split_layout_data_t;

static xui_thickness_t __xuiSplitLayoutThickness(float fLeft, float fTop, float fRight, float fBottom)
{
	xui_thickness_t tValue;

	tValue.fLeft = fLeft;
	tValue.fTop = fTop;
	tValue.fRight = fRight;
	tValue.fBottom = fBottom;
	return tValue;
}

static int __xuiSplitLayoutDescValid(const xui_split_layout_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) {
		return 0;
	}
	if ( (pDesc->iOrientation != 0) &&
	     (pDesc->iOrientation != XUI_ORIENTATION_HORIZONTAL) &&
	     (pDesc->iOrientation != XUI_ORIENTATION_VERTICAL) ) {
		return 0;
	}
	if ( (pDesc->iPaneCount != 0) &&
	     ((pDesc->iPaneCount < 1) || (pDesc->iPaneCount > XUI_SPLIT_LAYOUT_MAX_PANES)) ) {
		return 0;
	}
	if ( (pDesc->fDividerSize < 0.0f) ||
	     (pDesc->fDividerVisualSize < 0.0f) ||
	     (pDesc->fDividerHitSize < 0.0f) ) {
		return 0;
	}
	return 1;
}

static int __xuiSplitLayoutOrientationValid(int iOrientation)
{
	return (iOrientation == XUI_ORIENTATION_HORIZONTAL) || (iOrientation == XUI_ORIENTATION_VERTICAL);
}

static int __xuiSplitLayoutPaneModeValid(int iMode)
{
	return (iMode == XUI_SPLIT_PANE_GROW) || (iMode == XUI_SPLIT_PANE_FIXED);
}

static float __xuiSplitLayoutAbsFloat(float fValue)
{
	return (fValue < 0.0f) ? -fValue : fValue;
}

static float __xuiSplitLayoutMinFloat(float fA, float fB)
{
	return (fA < fB) ? fA : fB;
}

static float __xuiSplitLayoutMaxFloat(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static float __xuiSplitLayoutClampFloat(float fValue, float fMin, float fMax)
{
	if ( fValue < fMin ) return fMin;
	if ( fValue > fMax ) return fMax;
	return fValue;
}

static int __xuiSplitLayoutAlpha(uint32_t iColor)
{
	return (int)(iColor & 0xffu);
}

static xui_split_layout_data_t* __xuiSplitLayoutGetData(xui_widget pWidget)
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
	pType = xuiWidgetFindType(pContext, "splitlayout");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_split_layout_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiSplitLayoutStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
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

static int __xuiSplitLayoutStyleFloat(xui_widget pWidget, const char* sName, float* pValue)
{
	xui_style_property_t tProperty;
	int iRet;

	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) &&
	     (tProperty.tValue.iType == XUI_STYLE_VALUE_FLOAT) &&
	     (tProperty.tValue.fFloat >= 0.0f) ) {
		*pValue = tProperty.tValue.fFloat;
		return 1;
	}
	return 0;
}

static void __xuiSplitLayoutResolve(xui_widget pWidget, xui_split_layout_data_t* pData)
{
	pData->fResolvedDividerSize = pData->fDividerSize;
	pData->fResolvedDividerVisualSize = pData->fDividerVisualSize;
	pData->fResolvedDividerHitSize = pData->fDividerHitSize;
	(void)__xuiSplitLayoutStyleFloat(pWidget, "splitlayout.divider.size", &pData->fResolvedDividerSize);
	(void)__xuiSplitLayoutStyleFloat(pWidget, "splitlayout.divider.visual_size", &pData->fResolvedDividerVisualSize);
	(void)__xuiSplitLayoutStyleFloat(pWidget, "splitlayout.divider.hit_size", &pData->fResolvedDividerHitSize);
	if ( pData->fResolvedDividerSize <= 0.0f ) {
		pData->fResolvedDividerSize = 8.0f;
	}
	if ( pData->fResolvedDividerVisualSize <= 0.0f ) {
		pData->fResolvedDividerVisualSize = 4.0f;
	}
	if ( pData->fResolvedDividerHitSize <= 0.0f ) {
		pData->fResolvedDividerHitSize = 12.0f;
	}
	if ( pData->fResolvedDividerVisualSize > pData->fResolvedDividerHitSize ) {
		pData->fResolvedDividerHitSize = pData->fResolvedDividerVisualSize;
	}
}

static void __xuiSplitLayoutResolveColors(xui_widget pWidget, const xui_split_layout_data_t* pData, uint32_t* pDivider, uint32_t* pHover, uint32_t* pActive, uint32_t* pShadow)
{
	*pDivider = pData->iDividerColor;
	*pHover = pData->iDividerHoverColor;
	*pActive = pData->iDividerActiveColor;
	*pShadow = pData->iShadowColor;
	(void)__xuiSplitLayoutStyleColor(pWidget, "splitlayout.divider.color", pDivider);
	(void)__xuiSplitLayoutStyleColor(pWidget, "splitlayout.divider.hover_color", pHover);
	(void)__xuiSplitLayoutStyleColor(pWidget, "splitlayout.divider.active_color", pActive);
	(void)__xuiSplitLayoutStyleColor(pWidget, "splitlayout.shadow.color", pShadow);
}

static float __xuiSplitLayoutAxisSize(const xui_split_layout_data_t* pData, xui_rect_t tRect)
{
	return (pData->iOrientation == XUI_ORIENTATION_VERTICAL) ? tRect.fW : tRect.fH;
}

static float __xuiSplitLayoutCrossSize(const xui_split_layout_data_t* pData, xui_rect_t tRect)
{
	return (pData->iOrientation == XUI_ORIENTATION_VERTICAL) ? tRect.fH : tRect.fW;
}

static float __xuiSplitLayoutAxisStart(const xui_split_layout_data_t* pData, xui_rect_t tRect)
{
	return (pData->iOrientation == XUI_ORIENTATION_VERTICAL) ? tRect.fX : tRect.fY;
}

static float __xuiSplitLayoutAxisEnd(const xui_split_layout_data_t* pData, xui_rect_t tRect)
{
	return __xuiSplitLayoutAxisStart(pData, tRect) + __xuiSplitLayoutAxisSize(pData, tRect);
}

static float __xuiSplitLayoutEventAxis(xui_widget pSplit, const xui_split_layout_data_t* pData, const xui_event_t* pEvent)
{
	xui_rect_t tWorld;

	tWorld = xuiWidgetGetWorldRect(pSplit);
	return (pData->iOrientation == XUI_ORIENTATION_VERTICAL) ? (pEvent->fX - tWorld.fX) : (pEvent->fY - tWorld.fY);
}

static float __xuiSplitLayoutPaneMin(const xui_split_layout_pane_t* pPane)
{
	return (pPane->fMinSize > 0.0f) ? pPane->fMinSize : 0.0f;
}

static float __xuiSplitLayoutPaneMax(const xui_split_layout_pane_t* pPane)
{
	return (pPane->fMaxSize > 0.0f) ? pPane->fMaxSize : XUI_LAYOUT_UNBOUNDED;
}

static float __xuiSplitLayoutClampPaneSize(const xui_split_layout_pane_t* pPane, float fSize)
{
	float fMin;
	float fMax;

	fMin = __xuiSplitLayoutPaneMin(pPane);
	fMax = __xuiSplitLayoutPaneMax(pPane);
	if ( fSize < fMin ) {
		fSize = fMin;
	}
	if ( fSize > fMax ) {
		fSize = fMax;
	}
	return fSize;
}

static void __xuiSplitLayoutResolveOversubscribed(xui_split_layout_data_t* pData, float fAvailable)
{
	float arrBase[XUI_SPLIT_LAYOUT_MAX_PANES];
	float fRequired;
	int i;

	fRequired = 0.0f;
	for ( i = 0; i < pData->iPaneCount; i++ ) {
		if ( pData->arrPanes[i].iMode == XUI_SPLIT_PANE_FIXED ) {
			arrBase[i] = __xuiSplitLayoutClampPaneSize(&pData->arrPanes[i], pData->arrPanes[i].fFixedSize);
		} else {
			arrBase[i] = __xuiSplitLayoutPaneMin(&pData->arrPanes[i]);
		}
		if ( arrBase[i] < 0.0f ) {
			arrBase[i] = 0.0f;
		}
		fRequired += arrBase[i];
	}
	if ( fAvailable < 0.0f ) {
		fAvailable = 0.0f;
	}
	if ( fRequired <= 0.0f ) {
		for ( i = 0; i < pData->iPaneCount; i++ ) {
			pData->arrPanes[i].fResolvedSize = (pData->iPaneCount > 0) ? (fAvailable / (float)pData->iPaneCount) : 0.0f;
		}
		return;
	}
	for ( i = 0; i < pData->iPaneCount; i++ ) {
		pData->arrPanes[i].fResolvedSize = fAvailable * (arrBase[i] / fRequired);
	}
}

static void __xuiSplitLayoutResolveSizes(xui_split_layout_data_t* pData, float fAxisSize)
{
	float fDividerTotal;
	float fAvailable;
	float fFixedTotal;
	float fGrowMinTotal;
	float fRemaining;
	float fActiveWeight;
	float fWeight;
	float fSize;
	float fMin;
	float fMax;
	int arrLocked[XUI_SPLIT_LAYOUT_MAX_PANES];
	int iActiveCount;
	int iChanged;
	int i;

	if ( pData->iPaneCount <= 0 ) {
		return;
	}
	fDividerTotal = (float)(pData->iPaneCount - 1) * pData->fResolvedDividerSize;
	fAvailable = fAxisSize - fDividerTotal;
	if ( fAvailable < 0.0f ) {
		fAvailable = 0.0f;
	}

	fFixedTotal = 0.0f;
	fGrowMinTotal = 0.0f;
	memset(arrLocked, 0, sizeof(arrLocked));
	for ( i = 0; i < pData->iPaneCount; i++ ) {
		pData->arrPanes[i].fResolvedSize = 0.0f;
		if ( pData->arrPanes[i].iMode == XUI_SPLIT_PANE_FIXED ) {
			pData->arrPanes[i].fResolvedSize = __xuiSplitLayoutClampPaneSize(&pData->arrPanes[i], pData->arrPanes[i].fFixedSize);
			fFixedTotal += pData->arrPanes[i].fResolvedSize;
			arrLocked[i] = 1;
		} else {
			fGrowMinTotal += __xuiSplitLayoutPaneMin(&pData->arrPanes[i]);
		}
	}
	if ( fFixedTotal + fGrowMinTotal > fAvailable ) {
		__xuiSplitLayoutResolveOversubscribed(pData, fAvailable);
		return;
	}

	fRemaining = fAvailable - fFixedTotal;
	if ( fRemaining < 0.0f ) {
		fRemaining = 0.0f;
	}
	iActiveCount = 0;
	fActiveWeight = 0.0f;
	for ( i = 0; i < pData->iPaneCount; i++ ) {
		if ( pData->arrPanes[i].iMode == XUI_SPLIT_PANE_GROW ) {
			iActiveCount++;
			fWeight = (pData->arrPanes[i].fWeight > 0.0f) ? pData->arrPanes[i].fWeight : 1.0f;
			fActiveWeight += fWeight;
		}
	}
	while ( iActiveCount > 0 ) {
		iChanged = 0;
		for ( i = 0; i < pData->iPaneCount; i++ ) {
			if ( (pData->arrPanes[i].iMode != XUI_SPLIT_PANE_GROW) || (arrLocked[i] != 0) ) {
				continue;
			}
			fWeight = (pData->arrPanes[i].fWeight > 0.0f) ? pData->arrPanes[i].fWeight : 1.0f;
			fSize = (fActiveWeight > 0.0f) ? (fRemaining * fWeight / fActiveWeight) : (fRemaining / (float)iActiveCount);
			fMin = __xuiSplitLayoutPaneMin(&pData->arrPanes[i]);
			fMax = __xuiSplitLayoutPaneMax(&pData->arrPanes[i]);
			if ( fSize < fMin ) {
				pData->arrPanes[i].fResolvedSize = fMin;
				arrLocked[i] = 1;
				fRemaining -= fMin;
				fActiveWeight -= fWeight;
				iActiveCount--;
				iChanged = 1;
				break;
			}
			if ( fSize > fMax ) {
				pData->arrPanes[i].fResolvedSize = fMax;
				arrLocked[i] = 1;
				fRemaining -= fMax;
				fActiveWeight -= fWeight;
				iActiveCount--;
				iChanged = 1;
				break;
			}
		}
		if ( iChanged == 0 ) {
			break;
		}
		if ( fRemaining < 0.0f ) {
			fRemaining = 0.0f;
		}
		if ( fActiveWeight < 0.0f ) {
			fActiveWeight = 0.0f;
		}
	}
	for ( i = 0; i < pData->iPaneCount; i++ ) {
		if ( (pData->arrPanes[i].iMode == XUI_SPLIT_PANE_GROW) && (arrLocked[i] == 0) ) {
			fWeight = (pData->arrPanes[i].fWeight > 0.0f) ? pData->arrPanes[i].fWeight : 1.0f;
			pData->arrPanes[i].fResolvedSize = (fActiveWeight > 0.0f) ? (fRemaining * fWeight / fActiveWeight) : (fRemaining / (float)iActiveCount);
		}
	}
}

static int __xuiSplitLayoutDividerIndex(xui_split_layout_data_t* pData, xui_widget pDivider)
{
	int i;

	if ( (pData == NULL) || (pDivider == NULL) ) {
		return -1;
	}
	for ( i = 0; i < pData->iPaneCount - 1; i++ ) {
		if ( pData->arrDividers[i].pWidget == pDivider ) {
			return i;
		}
	}
	return -1;
}

static uint32_t __xuiSplitLayoutDividerState(xui_widget pSplit, xui_split_layout_data_t* pData, int iDivider)
{
	uint32_t iState;
	xui_widget pDivider;

	iState = 0;
	if ( !xuiWidgetGetEnabled(pSplit) ) {
		return XUI_WIDGET_STATE_DISABLED;
	}
	if ( (iDivider >= 0) && (iDivider < pData->iPaneCount - 1) ) {
		pDivider = pData->arrDividers[iDivider].pWidget;
		if ( xuiGetFocusWidget(xuiWidgetGetContext(pSplit)) == pDivider ) {
			iState |= XUI_WIDGET_STATE_FOCUS;
		}
	}
	if ( pData->iActiveDivider == iDivider ) {
		iState |= XUI_WIDGET_STATE_ACTIVE;
	} else if ( pData->iHoverDivider == iDivider ) {
		iState |= XUI_WIDGET_STATE_HOVER;
	}
	return iState;
}

static void __xuiSplitLayoutSyncDividerState(xui_widget pSplit, xui_split_layout_data_t* pData, int iDivider)
{
	xui_widget pDivider;
	uint32_t iState;

	if ( (iDivider < 0) || (iDivider >= pData->iPaneCount - 1) ) {
		return;
	}
	pDivider = pData->arrDividers[iDivider].pWidget;
	if ( pDivider == NULL ) {
		return;
	}
	iState = __xuiSplitLayoutDividerState(pSplit, pData, iDivider);
	(void)xuiWidgetSetStateId(pDivider, iState);
	(void)xuiWidgetInvalidate(pDivider, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static void __xuiSplitLayoutSyncAllDividerStates(xui_widget pSplit, xui_split_layout_data_t* pData)
{
	int i;

	for ( i = 0; i < pData->iPaneCount - 1; i++ ) {
		__xuiSplitLayoutSyncDividerState(pSplit, pData, i);
	}
}

static int __xuiSplitLayoutDrawRect(xui_proxy pProxy, xui_draw_context pDraw, xui_rect_t tRect, uint32_t iColor)
{
	if ( (__xuiSplitLayoutAlpha(iColor) == 0) || (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XUI_OK;
	}
	if ( (pProxy == NULL) || (pProxy->drawRectFill == NULL) ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return pProxy->drawRectFill(pProxy, pDraw, xuiInternalSnapRect(tRect), iColor);
}

static int __xuiSplitLayoutDividerRender(xui_widget pDivider, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_widget pSplit;
	xui_split_layout_data_t* pData;
	xui_proxy pProxy;
	xui_rect_t tRect;
	xui_rect_t tHit;
	uint32_t iDivider;
	uint32_t iHover;
	uint32_t iActive;
	uint32_t iShadow;
	uint32_t iColor;
	int iIndex;

	(void)iStateId;
	pSplit = (xui_widget)pUser;
	pData = __xuiSplitLayoutGetData(pSplit);
	if ( (pData == NULL) || (pDivider == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iIndex = __xuiSplitLayoutDividerIndex(pData, pDivider);
	if ( iIndex < 0 ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiSplitLayoutResolveColors(pSplit, pData, &iDivider, &iHover, &iActive, &iShadow);
	(void)iShadow;
	iColor = iDivider;
	if ( !xuiWidgetGetEnabled(pSplit) ) {
		iColor = (iDivider & 0xffffff00u) | ((iDivider & 0xffu) / 2u);
	} else if ( pData->iActiveDivider == iIndex ) {
		iColor = iActive;
	} else if ( pData->iHoverDivider == iIndex ) {
		iColor = iHover;
	}
	tRect = pData->arrDividers[iIndex].tVisualRect;
	tHit = pData->arrDividers[iIndex].tHitRect;
	tRect.fX -= tHit.fX;
	tRect.fY -= tHit.fY;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pSplit));
	return __xuiSplitLayoutDrawRect(pProxy, pDraw, tRect, iColor);
}

static int __xuiSplitLayoutShadowRender(xui_widget pShadow, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_widget pSplit;
	xui_split_layout_data_t* pData;
	xui_proxy pProxy;
	xui_rect_t tRect;
	uint32_t iDivider;
	uint32_t iHover;
	uint32_t iActive;
	uint32_t iShadow;

	(void)iStateId;
	pSplit = (xui_widget)pUser;
	pData = __xuiSplitLayoutGetData(pSplit);
	if ( (pData == NULL) || (pShadow == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiSplitLayoutResolveColors(pSplit, pData, &iDivider, &iHover, &iActive, &iShadow);
	(void)iDivider;
	(void)iHover;
	(void)iActive;
	tRect = xuiWidgetGetRect(pShadow);
	tRect.fX = 0.0f;
	tRect.fY = 0.0f;
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pSplit));
	return __xuiSplitLayoutDrawRect(pProxy, pDraw, tRect, iShadow);
}

static int __xuiSplitLayoutInitCacheStates(xui_widget pWidget)
{
	static const uint32_t arrState[] = {
		0,
		XUI_WIDGET_STATE_HOVER,
		XUI_WIDGET_STATE_ACTIVE,
		XUI_WIDGET_STATE_FOCUS,
		XUI_WIDGET_STATE_DISABLED,
		XUI_WIDGET_STATE_ACTIVE | XUI_WIDGET_STATE_FOCUS
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

static void __xuiSplitLayoutDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE | XUI_CACHE_UPDATE_ALL_STATES;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static int __xuiSplitLayoutConfigurePane(xui_widget pPane)
{
	if ( pPane == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	(void)xuiWidgetSetLayoutType(pPane, XUI_LAYOUT_COLUMN);
	(void)xuiWidgetSetSizeMode(pPane, XUI_SIZE_FILL, XUI_SIZE_FILL);
	(void)xuiWidgetSetFlex(pPane, 1.0f, 1.0f);
	(void)xuiWidgetSetPadding(pPane, __xuiSplitLayoutThickness(8.0f, 8.0f, 8.0f, 8.0f));
	(void)xuiWidgetSetGap(pPane, 6.0f);
	(void)xuiWidgetSetOverflow(pPane, XUI_OVERFLOW_CLIP);
	(void)xuiWidgetSetAlign(pPane, XUI_ALIGN_STRETCH, XUI_ALIGN_STRETCH);
	return XUI_OK;
}

static int __xuiSplitLayoutConfigureDivider(xui_widget pSplit, xui_widget pDivider)
{
	xui_cache_policy_t tPolicy;
	int iRet;

	if ( (pSplit == NULL) || (pDivider == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiSplitLayoutDefaultCachePolicy(&tPolicy);
	iRet = xuiWidgetSetCachePolicy(pDivider, &tPolicy);
	if ( iRet == XUI_OK ) iRet = __xuiSplitLayoutInitCacheStates(pDivider);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetCacheRenderCallback(pDivider, __xuiSplitLayoutDividerRender, pSplit);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetLayoutType(pDivider, XUI_LAYOUT_MANUAL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetSizeMode(pDivider, XUI_SIZE_FIXED, XUI_SIZE_FIXED);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetOverflow(pDivider, XUI_OVERFLOW_VISIBLE);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetLayer(pDivider, XUI_LAYER_NORMAL, 10);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetFocusable(pDivider, 1);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetTabStop(pDivider, 0);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetCancelAction(pDivider, NULL, NULL);
	return iRet;
}

static int __xuiSplitLayoutInitDividerEvents(xui_widget pDivider, xui_widget pSplit);

static int __xuiSplitLayoutCreatePane(xui_widget pSplit, xui_split_layout_data_t* pData, int iIndex)
{
	xui_widget pPane;
	int iRet;

	if ( pData->arrPanes[iIndex].pWidget != NULL ) {
		return XUI_OK;
	}
	iRet = xuiWidgetCreate(xuiWidgetGetContext(pSplit), &pPane);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iRet = __xuiSplitLayoutConfigurePane(pPane);
	if ( iRet == XUI_OK ) iRet = xuiWidgetAddChild(pSplit, pPane);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pPane);
		return iRet;
	}
	pData->arrPanes[iIndex].pWidget = pPane;
	return XUI_OK;
}

static int __xuiSplitLayoutCreateDivider(xui_widget pSplit, xui_split_layout_data_t* pData, int iIndex)
{
	xui_widget pDivider;
	int iRet;

	if ( pData->arrDividers[iIndex].pWidget != NULL ) {
		return XUI_OK;
	}
	iRet = xuiWidgetCreate(xuiWidgetGetContext(pSplit), &pDivider);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iRet = __xuiSplitLayoutConfigureDivider(pSplit, pDivider);
	if ( iRet == XUI_OK ) iRet = __xuiSplitLayoutInitDividerEvents(pDivider, pSplit);
	if ( iRet == XUI_OK ) iRet = xuiWidgetAddChild(pSplit, pDivider);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pDivider);
		return iRet;
	}
	pData->arrDividers[iIndex].pWidget = pDivider;
	return XUI_OK;
}

static void __xuiSplitLayoutDefaultPane(xui_split_layout_pane_t* pPane)
{
	xui_widget pWidget;

	pWidget = pPane->pWidget;
	memset(pPane, 0, sizeof(*pPane));
	pPane->pWidget = pWidget;
	pPane->fWeight = 1.0f;
	pPane->fFixedSize = 160.0f;
	pPane->fResolvedSize = 0.0f;
	pPane->fMinSize = 64.0f;
	pPane->fMaxSize = 0.0f;
	pPane->iMode = XUI_SPLIT_PANE_GROW;
}

static int __xuiSplitLayoutSetPaneCountInternal(xui_widget pWidget, xui_split_layout_data_t* pData, int iPaneCount)
{
	int i;
	int iRet;

	if ( (iPaneCount < 1) || (iPaneCount > XUI_SPLIT_LAYOUT_MAX_PANES) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( iPaneCount < pData->iPaneCount ) {
		for ( i = iPaneCount; i < pData->iPaneCount; i++ ) {
			if ( pData->arrPanes[i].pWidget != NULL ) {
				xuiWidgetDestroy(pData->arrPanes[i].pWidget);
				pData->arrPanes[i].pWidget = NULL;
			}
		}
		for ( i = iPaneCount - 1; i < pData->iPaneCount - 1; i++ ) {
			if ( (i >= 0) && (pData->arrDividers[i].pWidget != NULL) ) {
				xuiWidgetDestroy(pData->arrDividers[i].pWidget);
				pData->arrDividers[i].pWidget = NULL;
			}
		}
	}
	for ( i = 0; i < iPaneCount; i++ ) {
		if ( pData->arrPanes[i].fWeight <= 0.0f ) {
			__xuiSplitLayoutDefaultPane(&pData->arrPanes[i]);
		}
		iRet = __xuiSplitLayoutCreatePane(pWidget, pData, i);
		if ( iRet != XUI_OK ) return iRet;
	}
	for ( i = 0; i < iPaneCount - 1; i++ ) {
		iRet = __xuiSplitLayoutCreateDivider(pWidget, pData, i);
		if ( iRet != XUI_OK ) return iRet;
	}
	pData->iPaneCount = iPaneCount;
	if ( pData->iHoverDivider >= iPaneCount - 1 ) pData->iHoverDivider = -1;
	if ( pData->iActiveDivider >= iPaneCount - 1 ) pData->iActiveDivider = -1;
	__xuiSplitLayoutSyncAllDividerStates(pWidget, pData);
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiSplitLayoutEnsureShadow(xui_widget pSplit, xui_split_layout_data_t* pData)
{
	xui_cache_policy_t tPolicy;
	xui_context pContext;
	int iRet;

	if ( pData->pShadowWidget != NULL ) {
		return XUI_OK;
	}
	pContext = xuiWidgetGetContext(pSplit);
	iRet = xuiWidgetCreate(pContext, &pData->pShadowWidget);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	__xuiSplitLayoutDefaultCachePolicy(&tPolicy);
	iRet = xuiWidgetSetCachePolicy(pData->pShadowWidget, &tPolicy);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetCacheRenderCallback(pData->pShadowWidget, __xuiSplitLayoutShadowRender, pSplit);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetLayoutType(pData->pShadowWidget, XUI_LAYOUT_MANUAL);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetOverflow(pData->pShadowWidget, XUI_OVERFLOW_VISIBLE);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetFocusable(pData->pShadowWidget, 0);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetTabStop(pData->pShadowWidget, 0);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetVisible(pData->pShadowWidget, 0);
	if ( iRet == XUI_OK ) iRet = xuiOverlayAttach(pContext, pSplit, pData->pShadowWidget, XUI_LAYER_DRAG, 1000);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pData->pShadowWidget);
		pData->pShadowWidget = NULL;
		return iRet;
	}
	return XUI_OK;
}

static void __xuiSplitLayoutHideShadow(xui_split_layout_data_t* pData)
{
	if ( pData->pShadowWidget != NULL ) {
		(void)xuiWidgetSetVisible(pData->pShadowWidget, 0);
	}
}

static float __xuiSplitLayoutDragAxis(xui_split_layout_data_t* pData)
{
	return pData->fDragCurrentMouse - pData->fDragOffset;
}

static float __xuiSplitLayoutClampDragAxis(xui_split_layout_data_t* pData, int iDivider, float fAxis)
{
	xui_split_layout_pane_t* pBefore;
	xui_split_layout_pane_t* pAfter;
	xui_rect_t tBeforeRect;
	xui_rect_t tAfterRect;
	float fBeforeStart;
	float fAfterEnd;
	float fMinAxis;
	float fMaxAxis;
	float fBeforeMax;
	float fAfterMax;

	if ( (iDivider < 0) || (iDivider >= pData->iPaneCount - 1) ) {
		return fAxis;
	}
	pBefore = &pData->arrPanes[iDivider];
	pAfter = &pData->arrPanes[iDivider + 1];
	if ( (pBefore->pWidget == NULL) || (pAfter->pWidget == NULL) ) {
		return fAxis;
	}
	tBeforeRect = xuiWidgetGetRect(pBefore->pWidget);
	tAfterRect = xuiWidgetGetRect(pAfter->pWidget);
	fBeforeStart = __xuiSplitLayoutAxisStart(pData, tBeforeRect);
	fAfterEnd = __xuiSplitLayoutAxisEnd(pData, tAfterRect);
	fBeforeMax = __xuiSplitLayoutPaneMax(pBefore);
	fAfterMax = __xuiSplitLayoutPaneMax(pAfter);
	fMinAxis = fBeforeStart + __xuiSplitLayoutPaneMin(pBefore);
	if ( fAfterMax < XUI_LAYOUT_UNBOUNDED ) {
		fMinAxis = __xuiSplitLayoutMaxFloat(fMinAxis, fAfterEnd - pData->fResolvedDividerSize - fAfterMax);
	}
	fMaxAxis = fAfterEnd - pData->fResolvedDividerSize - __xuiSplitLayoutPaneMin(pAfter);
	if ( fBeforeMax < XUI_LAYOUT_UNBOUNDED ) {
		fMaxAxis = __xuiSplitLayoutMinFloat(fMaxAxis, fBeforeStart + fBeforeMax);
	}
	if ( fMinAxis > fMaxAxis ) {
		fAxis = (fMinAxis + fMaxAxis) * 0.5f;
	} else {
		fAxis = __xuiSplitLayoutClampFloat(fAxis, fMinAxis, fMaxAxis);
	}
	return fAxis;
}

static int __xuiSplitLayoutUpdateShadow(xui_widget pSplit, xui_split_layout_data_t* pData)
{
	xui_rect_t tWorld;
	xui_rect_t tShadow;
	float fAxis;
	float fVisualStart;
	int iRet;

	if ( (pData->iActiveDivider < 0) || (pData->iActiveDivider >= pData->iPaneCount - 1) ) {
		__xuiSplitLayoutHideShadow(pData);
		return XUI_OK;
	}
	iRet = __xuiSplitLayoutEnsureShadow(pSplit, pData);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	fAxis = __xuiSplitLayoutClampDragAxis(pData, pData->iActiveDivider, __xuiSplitLayoutDragAxis(pData));
	fVisualStart = fAxis + (pData->fResolvedDividerSize - pData->fResolvedDividerVisualSize) * 0.5f;
	tWorld = xuiWidgetGetWorldRect(pSplit);
	if ( pData->iOrientation == XUI_ORIENTATION_VERTICAL ) {
		tShadow = (xui_rect_t){tWorld.fX + fVisualStart, tWorld.fY + pData->tContentRect.fY, pData->fResolvedDividerVisualSize, pData->tContentRect.fH};
	} else {
		tShadow = (xui_rect_t){tWorld.fX + pData->tContentRect.fX, tWorld.fY + fVisualStart, pData->tContentRect.fW, pData->fResolvedDividerVisualSize};
	}
	tShadow = xuiInternalSnapRect(tShadow);
	pData->tShadowRect = tShadow;
	iRet = xuiWidgetSetRect(pData->pShadowWidget, tShadow);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetVisible(pData->pShadowWidget, 1);
	if ( iRet == XUI_OK ) iRet = xuiOverlayBringToFront(pData->pShadowWidget);
	if ( iRet == XUI_OK ) iRet = xuiWidgetInvalidate(pData->pShadowWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return iRet;
}

static int __xuiSplitLayoutCommitDrag(xui_widget pSplit, xui_split_layout_data_t* pData)
{
	xui_split_layout_pane_t* pBefore;
	xui_split_layout_pane_t* pAfter;
	xui_rect_t tBeforeRect;
	xui_rect_t tAfterRect;
	float fAxis;
	float fBeforeStart;
	float fAfterEnd;
	float fNewBefore;
	float fNewAfter;
	float fPairSize;
	float fTotalWeight;
	float fBeforeWeight;
	float fAfterWeight;
	int iDivider;
	int bChanged;

	iDivider = pData->iActiveDivider;
	if ( (iDivider < 0) || (iDivider >= pData->iPaneCount - 1) ) {
		return XUI_OK;
	}
	pBefore = &pData->arrPanes[iDivider];
	pAfter = &pData->arrPanes[iDivider + 1];
	if ( (pBefore->pWidget == NULL) || (pAfter->pWidget == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	fAxis = __xuiSplitLayoutClampDragAxis(pData, iDivider, __xuiSplitLayoutDragAxis(pData));
	tBeforeRect = xuiWidgetGetRect(pBefore->pWidget);
	tAfterRect = xuiWidgetGetRect(pAfter->pWidget);
	fBeforeStart = __xuiSplitLayoutAxisStart(pData, tBeforeRect);
	fAfterEnd = __xuiSplitLayoutAxisEnd(pData, tAfterRect);
	fNewBefore = fAxis - fBeforeStart;
	fNewAfter = fAfterEnd - fAxis - pData->fResolvedDividerSize;
	if ( fNewBefore < 0.0f ) fNewBefore = 0.0f;
	if ( fNewAfter < 0.0f ) fNewAfter = 0.0f;

	bChanged = 0;
	if ( (pBefore->iMode == XUI_SPLIT_PANE_FIXED) && (pAfter->iMode == XUI_SPLIT_PANE_FIXED) ) {
		if ( __xuiSplitLayoutAbsFloat(pBefore->fFixedSize - fNewBefore) > 0.01f ) {
			pBefore->fFixedSize = fNewBefore;
			bChanged = 1;
		}
		if ( __xuiSplitLayoutAbsFloat(pAfter->fFixedSize - fNewAfter) > 0.01f ) {
			pAfter->fFixedSize = fNewAfter;
			bChanged = 1;
		}
	} else if ( pBefore->iMode == XUI_SPLIT_PANE_FIXED ) {
		if ( __xuiSplitLayoutAbsFloat(pBefore->fFixedSize - fNewBefore) > 0.01f ) {
			pBefore->fFixedSize = fNewBefore;
			bChanged = 1;
		}
	} else if ( pAfter->iMode == XUI_SPLIT_PANE_FIXED ) {
		if ( __xuiSplitLayoutAbsFloat(pAfter->fFixedSize - fNewAfter) > 0.01f ) {
			pAfter->fFixedSize = fNewAfter;
			bChanged = 1;
		}
	} else {
		fPairSize = fNewBefore + fNewAfter;
		fTotalWeight = ((pBefore->fWeight > 0.0f) ? pBefore->fWeight : 1.0f) + ((pAfter->fWeight > 0.0f) ? pAfter->fWeight : 1.0f);
		if ( fPairSize > 0.0f ) {
			fBeforeWeight = fTotalWeight * (fNewBefore / fPairSize);
			fAfterWeight = fTotalWeight - fBeforeWeight;
		} else {
			fBeforeWeight = fTotalWeight * 0.5f;
			fAfterWeight = fTotalWeight - fBeforeWeight;
		}
		if ( __xuiSplitLayoutAbsFloat(pBefore->fWeight - fBeforeWeight) > 0.0001f ) {
			pBefore->fWeight = fBeforeWeight;
			bChanged = 1;
		}
		if ( __xuiSplitLayoutAbsFloat(pAfter->fWeight - fAfterWeight) > 0.0001f ) {
			pAfter->fWeight = fAfterWeight;
			bChanged = 1;
		}
	}
	if ( bChanged != 0 ) {
		pData->iChangeCount++;
		if ( pData->onChange != NULL ) {
			pData->onChange(pSplit, iDivider, pData->pChangeUser);
		}
	}
	return xuiWidgetInvalidate(pSplit, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiSplitLayoutPointerInside(xui_widget pSplit, xui_split_layout_data_t* pData, int iDivider, const xui_event_t* pEvent)
{
	xui_rect_t tWorld;
	xui_rect_t tHit;
	float fX;
	float fY;

	if ( (iDivider < 0) || (iDivider >= pData->iPaneCount - 1) ) {
		return 0;
	}
	tWorld = xuiWidgetGetWorldRect(pSplit);
	tHit = pData->arrDividers[iDivider].tHitRect;
	fX = pEvent->fX - tWorld.fX;
	fY = pEvent->fY - tWorld.fY;
	return (fX >= tHit.fX) && (fX <= tHit.fX + tHit.fW) &&
	       (fY >= tHit.fY) && (fY <= tHit.fY + tHit.fH);
}

static int __xuiSplitLayoutDividerPointerDown(xui_widget pDivider, xui_widget pSplit, xui_split_layout_data_t* pData, int iDivider, const xui_event_t* pEvent)
{
	xui_context pContext;
	float fMouse;
	float fLayoutStart;
	int iRet;

	if ( pEvent->iButton != XUI_POINTER_BUTTON_LEFT ) {
		return XUI_OK;
	}
	if ( !xuiWidgetGetEnabled(pSplit) || !__xuiSplitLayoutPointerInside(pSplit, pData, iDivider, pEvent) ) {
		return XUI_OK;
	}
	pContext = xuiWidgetGetContext(pSplit);
	fMouse = __xuiSplitLayoutEventAxis(pSplit, pData, pEvent);
	fLayoutStart = __xuiSplitLayoutAxisStart(pData, pData->arrDividers[iDivider].tLayoutRect);
	pData->iActiveDivider = iDivider;
	pData->iHoverDivider = iDivider;
	pData->fDragStartMouse = fMouse;
	pData->fDragCurrentMouse = fMouse;
	pData->fDragOffset = fMouse - fLayoutStart;
	(void)xuiSetFocusWidget(pContext, pDivider);
	(void)xuiSetPointerCapture(pContext, pDivider);
	__xuiSplitLayoutSyncAllDividerStates(pSplit, pData);
	if ( pData->bShadowDrag != 0 ) {
		iRet = __xuiSplitLayoutUpdateShadow(pSplit, pData);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_EVENT_DISPATCH_STOP;
}

static int __xuiSplitLayoutDividerPointerMove(xui_widget pDivider, xui_widget pSplit, xui_split_layout_data_t* pData, int iDivider, const xui_event_t* pEvent)
{
	int iRet;

	if ( (pData->iActiveDivider == iDivider) && (xuiGetPointerCapture(xuiWidgetGetContext(pSplit)) == pDivider) ) {
		pData->fDragCurrentMouse = __xuiSplitLayoutEventAxis(pSplit, pData, pEvent);
		if ( pData->bShadowDrag != 0 ) {
			iRet = __xuiSplitLayoutUpdateShadow(pSplit, pData);
		} else {
			iRet = __xuiSplitLayoutCommitDrag(pSplit, pData);
		}
		if ( iRet != XUI_OK ) return iRet;
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( pData->iHoverDivider != iDivider ) {
		pData->iHoverDivider = iDivider;
		__xuiSplitLayoutSyncAllDividerStates(pSplit, pData);
	}
	return XUI_OK;
}

static int __xuiSplitLayoutDividerPointerUp(xui_widget pDivider, xui_widget pSplit, xui_split_layout_data_t* pData, int iDivider, const xui_event_t* pEvent)
{
	xui_context pContext;
	int bWasActive;
	int iRet;

	if ( pEvent->iButton != XUI_POINTER_BUTTON_LEFT ) {
		return XUI_OK;
	}
	pContext = xuiWidgetGetContext(pSplit);
	bWasActive = (pData->iActiveDivider == iDivider) || (xuiGetPointerCapture(pContext) == pDivider);
	if ( bWasActive ) {
		pData->fDragCurrentMouse = __xuiSplitLayoutEventAxis(pSplit, pData, pEvent);
		iRet = __xuiSplitLayoutCommitDrag(pSplit, pData);
		if ( iRet != XUI_OK ) return iRet;
	}
	pData->iActiveDivider = -1;
	__xuiSplitLayoutHideShadow(pData);
	__xuiSplitLayoutSyncAllDividerStates(pSplit, pData);
	if ( xuiGetPointerCapture(pContext) == pDivider ) {
		(void)xuiReleasePointerCapture(pContext, pDivider);
	}
	return bWasActive ? XUI_EVENT_DISPATCH_STOP : XUI_OK;
}

static void __xuiSplitLayoutCancelDrag(xui_widget pDivider, void* pUser)
{
	xui_widget pSplit;
	xui_split_layout_data_t* pData;
	xui_context pContext;

	pSplit = (xui_widget)pUser;
	pData = __xuiSplitLayoutGetData(pSplit);
	if ( (pData == NULL) || (pDivider == NULL) ) {
		return;
	}
	pContext = xuiWidgetGetContext(pSplit);
	pData->iActiveDivider = -1;
	__xuiSplitLayoutHideShadow(pData);
	__xuiSplitLayoutSyncAllDividerStates(pSplit, pData);
	if ( xuiGetPointerCapture(pContext) == pDivider ) {
		(void)xuiReleasePointerCapture(pContext, pDivider);
	}
}

static int __xuiSplitLayoutDividerEvent(xui_widget pDivider, const xui_event_t* pEvent, void* pUser)
{
	xui_widget pSplit;
	xui_split_layout_data_t* pData;
	int iDivider;

	pSplit = (xui_widget)pUser;
	pData = __xuiSplitLayoutGetData(pSplit);
	if ( (pData == NULL) || (pDivider == NULL) || (pEvent == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iDivider = __xuiSplitLayoutDividerIndex(pData, pDivider);
	if ( iDivider < 0 ) {
		return XUI_OK;
	}
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_ENTER:
		pData->iHoverDivider = iDivider;
		__xuiSplitLayoutSyncAllDividerStates(pSplit, pData);
		return XUI_OK;
	case XUI_EVENT_POINTER_MOVE:
		return __xuiSplitLayoutDividerPointerMove(pDivider, pSplit, pData, iDivider, pEvent);
	case XUI_EVENT_POINTER_LEAVE:
		if ( pData->iActiveDivider < 0 ) {
			pData->iHoverDivider = -1;
			__xuiSplitLayoutSyncAllDividerStates(pSplit, pData);
		}
		return XUI_OK;
	case XUI_EVENT_POINTER_DOWN:
		return __xuiSplitLayoutDividerPointerDown(pDivider, pSplit, pData, iDivider, pEvent);
	case XUI_EVENT_POINTER_UP:
		return __xuiSplitLayoutDividerPointerUp(pDivider, pSplit, pData, iDivider, pEvent);
	case XUI_EVENT_POINTER_CLICK:
		if ( pEvent->iButton == XUI_POINTER_BUTTON_LEFT ) {
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		pData->iActiveDivider = -1;
		__xuiSplitLayoutHideShadow(pData);
		__xuiSplitLayoutSyncAllDividerStates(pSplit, pData);
		return XUI_OK;
	case XUI_EVENT_FOCUS:
	case XUI_EVENT_BLUR:
	case XUI_EVENT_ENABLED_CHANGED:
	case XUI_EVENT_VISIBLE_CHANGED:
		__xuiSplitLayoutSyncAllDividerStates(pSplit, pData);
		return xuiWidgetInvalidate(pDivider, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	case XUI_EVENT_KEY_DOWN:
		if ( pEvent->iKey == XUI_KEY_ESCAPE ) {
			__xuiSplitLayoutCancelDrag(pDivider, pSplit);
			return XUI_EVENT_DISPATCH_STOP;
		}
		break;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiSplitLayoutInitDividerEvents(xui_widget pDivider, xui_widget pSplit)
{
	int iRet;

	iRet = xuiWidgetSetCancelAction(pDivider, __xuiSplitLayoutCancelDrag, pSplit);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pDivider, XUI_EVENT_POINTER_ENTER, __xuiSplitLayoutDividerEvent, pSplit);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pDivider, XUI_EVENT_POINTER_LEAVE, __xuiSplitLayoutDividerEvent, pSplit);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pDivider, XUI_EVENT_POINTER_MOVE, __xuiSplitLayoutDividerEvent, pSplit);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pDivider, XUI_EVENT_POINTER_DOWN, __xuiSplitLayoutDividerEvent, pSplit);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pDivider, XUI_EVENT_POINTER_UP, __xuiSplitLayoutDividerEvent, pSplit);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pDivider, XUI_EVENT_POINTER_CLICK, __xuiSplitLayoutDividerEvent, pSplit);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pDivider, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiSplitLayoutDividerEvent, pSplit);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pDivider, XUI_EVENT_FOCUS, __xuiSplitLayoutDividerEvent, pSplit);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pDivider, XUI_EVENT_BLUR, __xuiSplitLayoutDividerEvent, pSplit);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pDivider, XUI_EVENT_KEY_DOWN, __xuiSplitLayoutDividerEvent, pSplit);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pDivider, XUI_EVENT_ENABLED_CHANGED, __xuiSplitLayoutDividerEvent, pSplit);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pDivider, XUI_EVENT_VISIBLE_CHANGED, __xuiSplitLayoutDividerEvent, pSplit);
	return iRet;
}

static int __xuiSplitLayoutMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_split_layout_data_t* pData;
	xui_vec2_t tChild;
	xui_vec2_t tChildConstraint;
	float fAxis;
	float fCross;
	float fChildAxis;
	float fChildCross;
	int iRet;
	int i;

	(void)tConstraint;
	(void)pUser;
	pData = __xuiSplitLayoutGetData(pWidget);
	if ( (pData == NULL) || (pSize == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiSplitLayoutResolve(pWidget, pData);
	fAxis = (float)(pData->iPaneCount - 1) * pData->fResolvedDividerSize;
	fCross = 0.0f;
	tChildConstraint = (xui_vec2_t){XUI_LAYOUT_UNBOUNDED, XUI_LAYOUT_UNBOUNDED};
	for ( i = 0; i < pData->iPaneCount; i++ ) {
		if ( pData->arrPanes[i].pWidget == NULL ) {
			continue;
		}
		tChild = (xui_vec2_t){0.0f, 0.0f};
		iRet = xuiWidgetMeasure(pData->arrPanes[i].pWidget, tChildConstraint, &tChild);
		if ( iRet != XUI_OK ) return iRet;
		fChildAxis = (pData->iOrientation == XUI_ORIENTATION_VERTICAL) ? tChild.fX : tChild.fY;
		fChildCross = (pData->iOrientation == XUI_ORIENTATION_VERTICAL) ? tChild.fY : tChild.fX;
		if ( pData->arrPanes[i].iMode == XUI_SPLIT_PANE_FIXED ) {
			fAxis += __xuiSplitLayoutClampPaneSize(&pData->arrPanes[i], pData->arrPanes[i].fFixedSize);
		} else {
			fAxis += __xuiSplitLayoutMaxFloat(fChildAxis, __xuiSplitLayoutPaneMin(&pData->arrPanes[i]));
		}
		fCross = __xuiSplitLayoutMaxFloat(fCross, fChildCross);
	}
	if ( pData->iOrientation == XUI_ORIENTATION_VERTICAL ) {
		pSize->fX = __xuiSplitLayoutMaxFloat(fAxis, 240.0f);
		pSize->fY = __xuiSplitLayoutMaxFloat(fCross, 160.0f);
	} else {
		pSize->fX = __xuiSplitLayoutMaxFloat(fCross, 240.0f);
		pSize->fY = __xuiSplitLayoutMaxFloat(fAxis, 160.0f);
	}
	return XUI_OK;
}

static int __xuiSplitLayoutArrange(xui_widget pWidget, xui_rect_t tContentRect, void* pUser)
{
	xui_split_layout_data_t* pData;
	xui_rect_t tPane;
	xui_rect_t tDivider;
	xui_rect_t tVisual;
	xui_rect_t tHit;
	float fAxis;
	float fCross;
	float fOffset;
	float fVisualInset;
	float fHitInset;
	int iRet;
	int i;

	(void)pUser;
	pData = __xuiSplitLayoutGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiSplitLayoutResolve(pWidget, pData);
	pData->tContentRect = tContentRect;
	fAxis = __xuiSplitLayoutAxisSize(pData, tContentRect);
	fCross = __xuiSplitLayoutCrossSize(pData, tContentRect);
	__xuiSplitLayoutResolveSizes(pData, fAxis);

	fOffset = __xuiSplitLayoutAxisStart(pData, tContentRect);
	for ( i = 0; i < pData->iPaneCount; i++ ) {
		if ( pData->iOrientation == XUI_ORIENTATION_VERTICAL ) {
			tPane = (xui_rect_t){fOffset, tContentRect.fY, pData->arrPanes[i].fResolvedSize, fCross};
		} else {
			tPane = (xui_rect_t){tContentRect.fX, fOffset, fCross, pData->arrPanes[i].fResolvedSize};
		}
		if ( pData->arrPanes[i].pWidget != NULL ) {
			iRet = xuiWidgetArrange(pData->arrPanes[i].pWidget, xuiInternalSnapRect(tPane));
			if ( iRet != XUI_OK ) return iRet;
		}
		fOffset += pData->arrPanes[i].fResolvedSize;
		if ( i < pData->iPaneCount - 1 ) {
			if ( pData->iOrientation == XUI_ORIENTATION_VERTICAL ) {
				tDivider = (xui_rect_t){fOffset, tContentRect.fY, pData->fResolvedDividerSize, fCross};
			} else {
				tDivider = (xui_rect_t){tContentRect.fX, fOffset, fCross, pData->fResolvedDividerSize};
			}
			fVisualInset = (pData->fResolvedDividerSize - pData->fResolvedDividerVisualSize) * 0.5f;
			fHitInset = (pData->fResolvedDividerSize - pData->fResolvedDividerHitSize) * 0.5f;
			if ( pData->iOrientation == XUI_ORIENTATION_VERTICAL ) {
				tVisual = (xui_rect_t){tDivider.fX + fVisualInset, tDivider.fY, pData->fResolvedDividerVisualSize, tDivider.fH};
				tHit = (xui_rect_t){tDivider.fX + fHitInset, tDivider.fY, pData->fResolvedDividerHitSize, tDivider.fH};
			} else {
				tVisual = (xui_rect_t){tDivider.fX, tDivider.fY + fVisualInset, tDivider.fW, pData->fResolvedDividerVisualSize};
				tHit = (xui_rect_t){tDivider.fX, tDivider.fY + fHitInset, tDivider.fW, pData->fResolvedDividerHitSize};
			}
			pData->arrDividers[i].tLayoutRect = tDivider;
			pData->arrDividers[i].tVisualRect = tVisual;
			pData->arrDividers[i].tHitRect = tHit;
			if ( pData->arrDividers[i].pWidget != NULL ) {
				iRet = xuiWidgetArrange(pData->arrDividers[i].pWidget, xuiInternalSnapRect(tHit));
				if ( iRet != XUI_OK ) return iRet;
				__xuiSplitLayoutSyncDividerState(pWidget, pData, i);
			}
			fOffset += pData->fResolvedDividerSize;
		}
	}
	if ( (pData->iActiveDivider >= 0) && (pData->bShadowDrag != 0) ) {
		(void)__xuiSplitLayoutUpdateShadow(pWidget, pData);
	}
	return XUI_OK;
}

static void __xuiSplitLayoutDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_MANUAL;
	pLayout->iWidthMode = XUI_SIZE_FIXED;
	pLayout->iHeightMode = XUI_SIZE_FIXED;
	pLayout->iFlowMode = XUI_FLOW_BLOCK;
	pLayout->iDock = XUI_DOCK_FILL;
	pLayout->iOverflow = XUI_OVERFLOW_CLIP;
	pLayout->iAlignX = XUI_ALIGN_START;
	pLayout->iAlignY = XUI_ALIGN_START;
	pLayout->iTableRowSpan = 1;
	pLayout->iTableColumnSpan = 1;
	pLayout->iGridColumnCount = 1;
	pLayout->fPreferredWidth = 360.0f;
	pLayout->fPreferredHeight = 220.0f;
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fShrink = 1.0f;
}

static void __xuiSplitLayoutInitDefaults(xui_split_layout_data_t* pData)
{
	int i;

	memset(pData, 0, sizeof(*pData));
	pData->iOrientation = XUI_ORIENTATION_VERTICAL;
	pData->iPaneCount = 0;
	pData->iHoverDivider = -1;
	pData->iActiveDivider = -1;
	pData->bShadowDrag = 1;
	pData->fDividerSize = 8.0f;
	pData->fDividerVisualSize = 4.0f;
	pData->fDividerHitSize = 12.0f;
	pData->fResolvedDividerSize = pData->fDividerSize;
	pData->fResolvedDividerVisualSize = pData->fDividerVisualSize;
	pData->fResolvedDividerHitSize = pData->fDividerHitSize;
	pData->iDividerColor = XUI_COLOR_RGBA(149, 176, 206, 220);
	pData->iDividerHoverColor = XUI_COLOR_RGBA(93, 154, 220, 235);
	pData->iDividerActiveColor = XUI_COLOR_RGBA(46, 124, 214, 245);
	pData->iShadowColor = XUI_COLOR_RGBA(46, 124, 214, 110);
	for ( i = 0; i < XUI_SPLIT_LAYOUT_MAX_PANES; i++ ) {
		__xuiSplitLayoutDefaultPane(&pData->arrPanes[i]);
	}
}

static int __xuiSplitLayoutInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_split_layout_data_t* pData;
	const xui_split_layout_desc_t* pDesc;
	int iPaneCount;
	int iRet;

	(void)pUser;
	pData = (xui_split_layout_data_t*)pTypeData;
	pDesc = (const xui_split_layout_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiSplitLayoutDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiSplitLayoutInitDefaults(pData);
	if ( pDesc != NULL ) {
		if ( __xuiSplitLayoutOrientationValid(pDesc->iOrientation) ) {
			pData->iOrientation = pDesc->iOrientation;
		}
		pData->bShadowDrag = pDesc->bShadowDrag ? 1 : 0;
		if ( pDesc->fDividerSize > 0.0f ) pData->fDividerSize = pDesc->fDividerSize;
		if ( pDesc->fDividerVisualSize > 0.0f ) pData->fDividerVisualSize = pDesc->fDividerVisualSize;
		if ( pDesc->fDividerHitSize > 0.0f ) pData->fDividerHitSize = pDesc->fDividerHitSize;
		if ( __xuiSplitLayoutAlpha(pDesc->iDividerColor) != 0 ) pData->iDividerColor = pDesc->iDividerColor;
		if ( __xuiSplitLayoutAlpha(pDesc->iDividerHoverColor) != 0 ) pData->iDividerHoverColor = pDesc->iDividerHoverColor;
		if ( __xuiSplitLayoutAlpha(pDesc->iDividerActiveColor) != 0 ) pData->iDividerActiveColor = pDesc->iDividerActiveColor;
		if ( __xuiSplitLayoutAlpha(pDesc->iShadowColor) != 0 ) pData->iShadowColor = pDesc->iShadowColor;
	}
	(void)xuiWidgetSetLayoutType(pWidget, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_CLIP);
	(void)xuiWidgetSetFocusable(pWidget, 0);
	(void)xuiWidgetSetTabStop(pWidget, 0);
	iPaneCount = (pDesc != NULL && pDesc->iPaneCount > 0) ? pDesc->iPaneCount : 2;
	iRet = __xuiSplitLayoutSetPaneCountInternal(pWidget, pData, iPaneCount);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return XUI_OK;
}

static void __xuiSplitLayoutDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_split_layout_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_split_layout_data_t*)pTypeData;
	if ( (pData != NULL) && (pData->pShadowWidget != NULL) ) {
		xuiWidgetDestroy(pData->pShadowWidget);
		pData->pShadowWidget = NULL;
	}
}

static void __xuiSplitLayoutRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
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

static void __xuiSplitLayoutRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	uint32_t iPaintDirty;
	uint32_t iLayoutDirty;

	iPaintDirty = XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	iLayoutDirty = XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER;
	__xuiSplitLayoutRegisterStyleProperty(pContext, pType, "splitlayout.divider.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiSplitLayoutRegisterStyleProperty(pContext, pType, "splitlayout.divider.hover_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiSplitLayoutRegisterStyleProperty(pContext, pType, "splitlayout.divider.active_color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiSplitLayoutRegisterStyleProperty(pContext, pType, "splitlayout.shadow.color", XUI_STYLE_VALUE_COLOR, iPaintDirty, 0);
	__xuiSplitLayoutRegisterStyleProperty(pContext, pType, "splitlayout.divider.size", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiSplitLayoutRegisterStyleProperty(pContext, pType, "splitlayout.divider.visual_size", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
	__xuiSplitLayoutRegisterStyleProperty(pContext, pType, "splitlayout.divider.hit_size", XUI_STYLE_VALUE_FLOAT, iLayoutDirty, 0);
}

XUI_API xui_widget_type xuiSplitLayoutGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "splitlayout");
	if ( pType != NULL ) {
		__xuiSplitLayoutRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "splitlayout";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_split_layout_data_t);
	tDesc.onInit = __xuiSplitLayoutInit;
	tDesc.onDestroy = __xuiSplitLayoutDestroy;
	tDesc.onLayoutMeasure = __xuiSplitLayoutMeasure;
	tDesc.onLayoutArrange = __xuiSplitLayoutArrange;
	__xuiSplitLayoutDefaultLayout(&tDesc.tLayout);
	__xuiSplitLayoutDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	__xuiSplitLayoutRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiSplitLayoutCreate(xui_context pContext, xui_widget* ppWidget, const xui_split_layout_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( !xuiInternalContextIsValid(pContext) || (ppWidget == NULL) || !__xuiSplitLayoutDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiSplitLayoutGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiSplitLayoutSetChange(xui_widget pWidget, xui_split_layout_change_proc onChange, void* pUser)
{
	xui_split_layout_data_t* pData;

	pData = __xuiSplitLayoutGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onChange = onChange;
	pData->pChangeUser = pUser;
	return XUI_OK;
}

XUI_API int xuiSplitLayoutSetOrientation(xui_widget pWidget, int iOrientation)
{
	xui_split_layout_data_t* pData;

	pData = __xuiSplitLayoutGetData(pWidget);
	if ( (pData == NULL) || !__xuiSplitLayoutOrientationValid(iOrientation) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iOrientation == iOrientation ) return XUI_OK;
	pData->iOrientation = iOrientation;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiSplitLayoutGetOrientation(xui_widget pWidget)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	return (pData != NULL) ? pData->iOrientation : XUI_ORIENTATION_VERTICAL;
}

XUI_API int xuiSplitLayoutSetPaneCount(xui_widget pWidget, int iPaneCount)
{
	xui_split_layout_data_t* pData;

	pData = __xuiSplitLayoutGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pData->iPaneCount == iPaneCount ) return XUI_OK;
	return __xuiSplitLayoutSetPaneCountInternal(pWidget, pData, iPaneCount);
}

XUI_API int xuiSplitLayoutGetPaneCount(xui_widget pWidget)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	return (pData != NULL) ? pData->iPaneCount : 0;
}

XUI_API xui_widget xuiSplitLayoutGetPaneWidget(xui_widget pWidget, int iIndex)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iPaneCount) ) return NULL;
	return pData->arrPanes[iIndex].pWidget;
}

XUI_API int xuiSplitLayoutAddPaneChild(xui_widget pWidget, int iPane, xui_widget pChild)
{
	xui_widget pPane;

	pPane = xuiSplitLayoutGetPaneWidget(pWidget, iPane);
	if ( (pPane == NULL) || (pChild == NULL) ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiWidgetAddChild(pPane, pChild);
}

XUI_API int xuiSplitLayoutSetPaneWeight(xui_widget pWidget, int iIndex, float fWeight)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iPaneCount) || (fWeight <= 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrPanes[iIndex].fWeight = fWeight;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiSplitLayoutGetPaneWeight(xui_widget pWidget, int iIndex)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iPaneCount) ) return 0.0f;
	return pData->arrPanes[iIndex].fWeight;
}

XUI_API int xuiSplitLayoutSetPaneMode(xui_widget pWidget, int iIndex, int iMode)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iPaneCount) || !__xuiSplitLayoutPaneModeValid(iMode) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrPanes[iIndex].iMode = iMode;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiSplitLayoutGetPaneMode(xui_widget pWidget, int iIndex)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iPaneCount) ) return XUI_SPLIT_PANE_GROW;
	return pData->arrPanes[iIndex].iMode;
}

XUI_API int xuiSplitLayoutSetPaneFixedSize(xui_widget pWidget, int iIndex, float fSize)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iPaneCount) || (fSize < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrPanes[iIndex].fFixedSize = fSize;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiSplitLayoutGetPaneFixedSize(xui_widget pWidget, int iIndex)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iPaneCount) ) return 0.0f;
	return pData->arrPanes[iIndex].fFixedSize;
}

XUI_API int xuiSplitLayoutSetPaneMinSize(xui_widget pWidget, int iIndex, float fSize)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iPaneCount) || (fSize < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrPanes[iIndex].fMinSize = fSize;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiSplitLayoutGetPaneMinSize(xui_widget pWidget, int iIndex)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iPaneCount) ) return 0.0f;
	return pData->arrPanes[iIndex].fMinSize;
}

XUI_API int xuiSplitLayoutSetPaneMaxSize(xui_widget pWidget, int iIndex, float fSize)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iPaneCount) || (fSize < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->arrPanes[iIndex].fMaxSize = fSize;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiSplitLayoutGetPaneMaxSize(xui_widget pWidget, int iIndex)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iPaneCount) ) return 0.0f;
	return pData->arrPanes[iIndex].fMaxSize;
}

XUI_API float xuiSplitLayoutGetPaneSize(xui_widget pWidget, int iIndex)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iPaneCount) ) return 0.0f;
	return pData->arrPanes[iIndex].fResolvedSize;
}

XUI_API int xuiSplitLayoutSetDividerMetrics(xui_widget pWidget, float fLayoutSize, float fVisualSize, float fHitSize)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	if ( (pData == NULL) || (fLayoutSize < 0.0f) || (fVisualSize < 0.0f) || (fHitSize < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fDividerSize = (fLayoutSize > 0.0f) ? fLayoutSize : 8.0f;
	pData->fDividerVisualSize = (fVisualSize > 0.0f) ? fVisualSize : 4.0f;
	pData->fDividerHitSize = (fHitSize > 0.0f) ? fHitSize : 12.0f;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiSplitLayoutGetDividerMetrics(xui_widget pWidget, float* pLayoutSize, float* pVisualSize, float* pHitSize)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pLayoutSize != NULL ) *pLayoutSize = pData->fDividerSize;
	if ( pVisualSize != NULL ) *pVisualSize = pData->fDividerVisualSize;
	if ( pHitSize != NULL ) *pHitSize = pData->fDividerHitSize;
	return XUI_OK;
}

XUI_API int xuiSplitLayoutSetDividerSize(xui_widget pWidget, float fSize)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	if ( (pData == NULL) || (fSize < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fDividerSize = (fSize > 0.0f) ? fSize : 8.0f;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiSplitLayoutSetDividerVisualSize(xui_widget pWidget, float fSize)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	if ( (pData == NULL) || (fSize < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fDividerVisualSize = (fSize > 0.0f) ? fSize : 4.0f;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiSplitLayoutSetDividerHitSize(xui_widget pWidget, float fSize)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	if ( (pData == NULL) || (fSize < 0.0f) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fDividerHitSize = (fSize > 0.0f) ? fSize : 12.0f;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiSplitLayoutSetShadowDrag(xui_widget pWidget, int bShadowDrag)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bShadowDrag = bShadowDrag ? 1 : 0;
	if ( pData->bShadowDrag == 0 ) {
		__xuiSplitLayoutHideShadow(pData);
	}
	return XUI_OK;
}

XUI_API int xuiSplitLayoutGetShadowDrag(xui_widget pWidget)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	return (pData != NULL) ? pData->bShadowDrag : 0;
}

XUI_API int xuiSplitLayoutSetColors(xui_widget pWidget, uint32_t iDivider, uint32_t iHover, uint32_t iActive, uint32_t iShadow)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iDividerColor = iDivider;
	pData->iDividerHoverColor = iHover;
	pData->iDividerActiveColor = iActive;
	pData->iShadowColor = iShadow;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiSplitLayoutGetColors(xui_widget pWidget, uint32_t* pDivider, uint32_t* pHover, uint32_t* pActive, uint32_t* pShadow)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pDivider != NULL ) *pDivider = pData->iDividerColor;
	if ( pHover != NULL ) *pHover = pData->iDividerHoverColor;
	if ( pActive != NULL ) *pActive = pData->iDividerActiveColor;
	if ( pShadow != NULL ) *pShadow = pData->iShadowColor;
	return XUI_OK;
}

XUI_API xui_rect_t xuiSplitLayoutGetPaneRect(xui_widget pWidget, int iIndex)
{
	xui_widget pPane = xuiSplitLayoutGetPaneWidget(pWidget, iIndex);
	return (pPane != NULL) ? xuiWidgetGetRect(pPane) : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API xui_rect_t xuiSplitLayoutGetDividerLayoutRect(xui_widget pWidget, int iIndex)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iPaneCount - 1) ) return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	return pData->arrDividers[iIndex].tLayoutRect;
}

XUI_API xui_rect_t xuiSplitLayoutGetDividerVisualRect(xui_widget pWidget, int iIndex)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iPaneCount - 1) ) return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	return pData->arrDividers[iIndex].tVisualRect;
}

XUI_API xui_rect_t xuiSplitLayoutGetDividerHitRect(xui_widget pWidget, int iIndex)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	if ( (pData == NULL) || (iIndex < 0) || (iIndex >= pData->iPaneCount - 1) ) return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	return pData->arrDividers[iIndex].tHitRect;
}

XUI_API xui_rect_t xuiSplitLayoutGetShadowRect(xui_widget pWidget)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	if ( pData == NULL ) return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	return pData->tShadowRect;
}

XUI_API int xuiSplitLayoutGetHoverDivider(xui_widget pWidget)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	return (pData != NULL) ? pData->iHoverDivider : -1;
}

XUI_API int xuiSplitLayoutGetActiveDivider(xui_widget pWidget)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	return (pData != NULL) ? pData->iActiveDivider : -1;
}

XUI_API int xuiSplitLayoutGetChangeCount(xui_widget pWidget)
{
	xui_split_layout_data_t* pData = __xuiSplitLayoutGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}
