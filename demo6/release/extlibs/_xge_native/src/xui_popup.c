#include "xui_internal.h"

#include <string.h>

typedef struct xui_popup_data_t {
	xui_widget pOwner;
	xui_widget pPanel;
	xui_widget pScrollView;
	xui_widget pContent;
	xui_widget pFocusRestore;
	xui_widget pCustomFocus;
	xui_popup_change_proc onChange;
	void* pChangeUser;
	xui_rect_t tAnchorRect;
	xui_rect_t tPopupRect;
	xui_rect_t tPanelLocalRect;
	int bAnchorRectSet;
	int bOpen;
	int bModal;
	int bConsumeInside;
	int bMatchOwnerWidth;
	int bShield;
	int bFocusRestoreExplicit;
	int iAnchor;
	int iDirection;
	int iOutsidePolicy;
	int iOwnerPolicy;
	int iEscapePolicy;
	int iFocusPolicy;
	int iScrollbarMode;
	int iChangeCount;
	float fContentWidth;
	float fContentHeight;
	float fMaxWidth;
	float fMaxHeight;
	float fGap;
	float fOffsetX;
	float fOffsetY;
	float fMargin;
	float fPadding;
	float fBorderWidth;
	float fShadowSize;
	float fScrollbarSize;
	uint32_t iPanelColor;
	uint32_t iBorderColor;
	uint32_t iShadowColor;
	uint32_t iBackdropColor;
	uint32_t iTrackColor;
	uint32_t iThumbColor;
	uint32_t iHoverColor;
	uint32_t iActiveColor;
	uint32_t iFocusColor;
	uint32_t iDisabledColor;
} xui_popup_data_t;

static int __xuiPopupSetOpenInternal(xui_widget pWidget, xui_popup_data_t* pData, int bOpen, int bNotify);

static int __xuiPopupDescValid(const xui_popup_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	return (pDesc->iSize == 0) || (pDesc->iSize >= sizeof(*pDesc));
}

static uint32_t __xuiPopupAlpha(uint32_t iColor)
{
	return iColor & 0xFFu;
}

static float __xuiPopupMin(float fA, float fB)
{
	return (fA < fB) ? fA : fB;
}

static float __xuiPopupMax(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static float __xuiPopupClamp(float fValue, float fMin, float fMax)
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

static int __xuiPopupFloatValid(float fValue)
{
	return (fValue == fValue) && (fValue >= -XUI_CONTEXT_MAX_VIEWPORT) && (fValue <= XUI_CONTEXT_MAX_VIEWPORT);
}

static int __xuiPopupAnchorValid(int iAnchor)
{
	return (iAnchor >= XUI_POPUP_ANCHOR_BOTTOM_LEFT) && (iAnchor <= XUI_POPUP_ANCHOR_FIXED);
}

static int __xuiPopupDirectionValid(int iDirection)
{
	return (iDirection >= XUI_POPUP_DIRECTION_RIGHT_DOWN) && (iDirection <= XUI_POPUP_DIRECTION_LEFT_UP);
}

static int __xuiPopupOutsidePolicyValid(int iPolicy)
{
	return (iPolicy >= XUI_POPUP_OUTSIDE_CLOSE) && (iPolicy <= XUI_POPUP_OUTSIDE_CONSUME);
}

static int __xuiPopupOwnerPolicyValid(int iPolicy)
{
	return (iPolicy >= XUI_POPUP_OWNER_PASSTHROUGH) && (iPolicy <= XUI_POPUP_OWNER_CONSUME);
}

static int __xuiPopupEscapePolicyValid(int iPolicy)
{
	return (iPolicy >= XUI_POPUP_ESCAPE_CLOSE) && (iPolicy <= XUI_POPUP_ESCAPE_IGNORE);
}

static int __xuiPopupFocusPolicyValid(int iPolicy)
{
	return (iPolicy >= XUI_POPUP_FOCUS_POPUP) && (iPolicy <= XUI_POPUP_FOCUS_CUSTOM);
}

static int __xuiPopupScrollbarModeValid(int iMode)
{
	return (iMode == XUI_SCROLLBAR_MODE_FULL) || (iMode == XUI_SCROLLBAR_MODE_COMPACT);
}

static xui_popup_data_t* __xuiPopupGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return NULL;
	}
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "popup");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_popup_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiPopupRectContains(xui_rect_t tRect, float fX, float fY)
{
	return (fX >= tRect.fX) &&
	       (fY >= tRect.fY) &&
	       (fX < (tRect.fX + tRect.fW)) &&
	       (fY < (tRect.fY + tRect.fH));
}

static int __xuiPopupRectFits(xui_rect_t tRect, float fWindowW, float fWindowH, float fMargin)
{
	return (tRect.fX >= fMargin) &&
	       (tRect.fY >= fMargin) &&
	       ((tRect.fX + tRect.fW) <= (fWindowW - fMargin)) &&
	       ((tRect.fY + tRect.fH) <= (fWindowH - fMargin));
}

static int __xuiPopupFlipVertical(int iDirection)
{
	switch ( iDirection ) {
	case XUI_POPUP_DIRECTION_RIGHT_DOWN: return XUI_POPUP_DIRECTION_RIGHT_UP;
	case XUI_POPUP_DIRECTION_RIGHT_UP: return XUI_POPUP_DIRECTION_RIGHT_DOWN;
	case XUI_POPUP_DIRECTION_LEFT_DOWN: return XUI_POPUP_DIRECTION_LEFT_UP;
	case XUI_POPUP_DIRECTION_LEFT_UP: return XUI_POPUP_DIRECTION_LEFT_DOWN;
	default: return XUI_POPUP_DIRECTION_RIGHT_DOWN;
	}
}

static int __xuiPopupFlipHorizontal(int iDirection)
{
	switch ( iDirection ) {
	case XUI_POPUP_DIRECTION_RIGHT_DOWN: return XUI_POPUP_DIRECTION_LEFT_DOWN;
	case XUI_POPUP_DIRECTION_RIGHT_UP: return XUI_POPUP_DIRECTION_LEFT_UP;
	case XUI_POPUP_DIRECTION_LEFT_DOWN: return XUI_POPUP_DIRECTION_RIGHT_DOWN;
	case XUI_POPUP_DIRECTION_LEFT_UP: return XUI_POPUP_DIRECTION_RIGHT_UP;
	default: return XUI_POPUP_DIRECTION_RIGHT_DOWN;
	}
}

static int __xuiPopupFlipAnchorVertical(int iAnchor)
{
	switch ( iAnchor ) {
	case XUI_POPUP_ANCHOR_BOTTOM_LEFT: return XUI_POPUP_ANCHOR_TOP_LEFT;
	case XUI_POPUP_ANCHOR_BOTTOM_RIGHT: return XUI_POPUP_ANCHOR_TOP_RIGHT;
	case XUI_POPUP_ANCHOR_TOP_LEFT: return XUI_POPUP_ANCHOR_BOTTOM_LEFT;
	case XUI_POPUP_ANCHOR_TOP_RIGHT: return XUI_POPUP_ANCHOR_BOTTOM_RIGHT;
	default: return iAnchor;
	}
}

static int __xuiPopupFlipAnchorHorizontal(int iAnchor)
{
	switch ( iAnchor ) {
	case XUI_POPUP_ANCHOR_BOTTOM_LEFT: return XUI_POPUP_ANCHOR_BOTTOM_RIGHT;
	case XUI_POPUP_ANCHOR_BOTTOM_RIGHT: return XUI_POPUP_ANCHOR_BOTTOM_LEFT;
	case XUI_POPUP_ANCHOR_TOP_LEFT: return XUI_POPUP_ANCHOR_TOP_RIGHT;
	case XUI_POPUP_ANCHOR_TOP_RIGHT: return XUI_POPUP_ANCHOR_TOP_LEFT;
	default: return iAnchor;
	}
}

static xui_vec2_t __xuiPopupAnchorPoint(xui_rect_t tAnchor, int iAnchor)
{
	xui_vec2_t tPoint;

	switch ( iAnchor ) {
	case XUI_POPUP_ANCHOR_BOTTOM_RIGHT:
		tPoint.fX = tAnchor.fX + tAnchor.fW;
		tPoint.fY = tAnchor.fY + tAnchor.fH;
		break;
	case XUI_POPUP_ANCHOR_TOP_LEFT:
		tPoint.fX = tAnchor.fX;
		tPoint.fY = tAnchor.fY;
		break;
	case XUI_POPUP_ANCHOR_TOP_RIGHT:
		tPoint.fX = tAnchor.fX + tAnchor.fW;
		tPoint.fY = tAnchor.fY;
		break;
	case XUI_POPUP_ANCHOR_CURSOR:
	case XUI_POPUP_ANCHOR_FIXED:
		tPoint.fX = tAnchor.fX;
		tPoint.fY = tAnchor.fY;
		break;
	case XUI_POPUP_ANCHOR_BOTTOM_LEFT:
	default:
		tPoint.fX = tAnchor.fX;
		tPoint.fY = tAnchor.fY + tAnchor.fH;
		break;
	}
	return tPoint;
}

static xui_rect_t __xuiPopupRectFrom(xui_vec2_t tPoint, float fWidth, float fHeight, float fGap, int iDirection)
{
	xui_rect_t tRect;

	tRect.fW = fWidth;
	tRect.fH = fHeight;
	switch ( iDirection ) {
	case XUI_POPUP_DIRECTION_RIGHT_UP:
		tRect.fX = tPoint.fX;
		tRect.fY = tPoint.fY - fGap - fHeight;
		break;
	case XUI_POPUP_DIRECTION_LEFT_DOWN:
		tRect.fX = tPoint.fX - fWidth;
		tRect.fY = tPoint.fY + fGap;
		break;
	case XUI_POPUP_DIRECTION_LEFT_UP:
		tRect.fX = tPoint.fX - fWidth;
		tRect.fY = tPoint.fY - fGap - fHeight;
		break;
	case XUI_POPUP_DIRECTION_RIGHT_DOWN:
	default:
		tRect.fX = tPoint.fX;
		tRect.fY = tPoint.fY + fGap;
		break;
	}
	return tRect;
}

static int __xuiPopupOwnerAvailable(const xui_popup_data_t* pData)
{
	if ( pData == NULL || pData->pOwner == NULL ) {
		return 1;
	}
	if ( !xuiInternalWidgetIsValid(pData->pOwner) ) {
		return 0;
	}
	return xuiWidgetGetVisible(pData->pOwner) && xuiWidgetGetEnabled(pData->pOwner);
}

static xui_rect_t __xuiPopupResolveAnchor(xui_widget pWidget, const xui_popup_data_t* pData)
{
	xui_rect_t tAnchor;

	(void)pWidget;
	memset(&tAnchor, 0, sizeof(tAnchor));
	if ( pData == NULL ) {
		return tAnchor;
	}
	if ( pData->bAnchorRectSet ) {
		return pData->tAnchorRect;
	}
	if ( pData->pOwner != NULL && xuiInternalWidgetIsValid(pData->pOwner) ) {
		return xuiWidgetGetWorldRect(pData->pOwner);
	}
	return tAnchor;
}

static void __xuiPopupResolveContentSize(xui_widget pWidget, xui_popup_data_t* pData, xui_rect_t tAnchor, float* pWidth, float* pHeight)
{
	xui_rect_t tContent;
	float fInset;
	float fWidth;
	float fHeight;

	fWidth = (pData->fContentWidth > 0.0f) ? pData->fContentWidth : 0.0f;
	fHeight = (pData->fContentHeight > 0.0f) ? pData->fContentHeight : 0.0f;
	if ( pData->pContent != NULL ) {
		tContent = xuiWidgetGetRect(pData->pContent);
		if ( fWidth <= 0.0f ) {
			fWidth = tContent.fW;
		}
		if ( fHeight <= 0.0f ) {
			fHeight = tContent.fH;
		}
	}
	if ( pData->bMatchOwnerWidth && (tAnchor.fW > 0.0f) ) {
		fInset = (pData->fPadding + pData->fBorderWidth) * 2.0f;
		fWidth = __xuiPopupMax(1.0f, tAnchor.fW - fInset);
	}
	if ( pWidget != NULL && (fWidth <= 0.0f || fHeight <= 0.0f) ) {
		xui_rect_t tRect = xuiWidgetGetRect(pWidget);
		if ( fWidth <= 0.0f ) fWidth = tRect.fW;
		if ( fHeight <= 0.0f ) fHeight = tRect.fH;
	}
	if ( pWidth != NULL ) *pWidth = __xuiPopupMax(1.0f, fWidth);
	if ( pHeight != NULL ) *pHeight = __xuiPopupMax(1.0f, fHeight);
}

static float __xuiPopupScrollbarReserve(const xui_popup_data_t* pData)
{
	float fReserve;

	if ( pData == NULL ) {
		return 8.0f;
	}
	fReserve = pData->fScrollbarSize;
	if ( fReserve <= 0.0f ) {
		fReserve = (pData->iScrollbarMode == XUI_SCROLLBAR_MODE_FULL) ? 16.0f : 8.0f;
	}
	return __xuiPopupMax(1.0f, fReserve);
}

static void __xuiPopupApplyScrollStyleData(xui_popup_data_t* pData)
{
	if ( (pData == NULL) || (pData->pScrollView == NULL) ) {
		return;
	}
	(void)xuiScrollViewSetScrollbarMode(pData->pScrollView, pData->iScrollbarMode);
	(void)xuiScrollViewSetMetrics(pData->pScrollView,
		pData->fScrollbarSize,
		18.0f,
		(pData->iScrollbarMode == XUI_SCROLLBAR_MODE_FULL) ? pData->fScrollbarSize : 0.0f);
}

static void __xuiPopupResolveOuterSize(xui_popup_data_t* pData, float fWindowW, float fWindowH, float fContentW, float fContentH, float* pOuterW, float* pOuterH)
{
	float fBaseW;
	float fBaseH;
	float fLimitW;
	float fLimitH;
	float fViewportW;
	float fViewportH;
	float fOuterW;
	float fOuterH;
	float fInset;
	float fReserve;
	float fMargin;
	int bNeedH;
	int bNeedV;
	int bNextH;
	int bNextV;
	int i;

	if ( pData == NULL ) {
		return;
	}
	fInset = (pData->fPadding + pData->fBorderWidth) * 2.0f;
	fReserve = __xuiPopupScrollbarReserve(pData);
	fMargin = __xuiPopupMax(0.0f, pData->fMargin);
	fOuterW = fWindowW - fMargin * 2.0f;
	fOuterH = fWindowH - fMargin * 2.0f;
	if ( pData->fMaxWidth > 0.0f ) {
		fOuterW = __xuiPopupMin(fOuterW, pData->fMaxWidth);
	}
	if ( pData->fMaxHeight > 0.0f ) {
		fOuterH = __xuiPopupMin(fOuterH, pData->fMaxHeight);
	}
	fOuterW = __xuiPopupMax(1.0f, fOuterW);
	fOuterH = __xuiPopupMax(1.0f, fOuterH);
	fBaseW = __xuiPopupMax(1.0f, fOuterW - fInset);
	fBaseH = __xuiPopupMax(1.0f, fOuterH - fInset);
	bNeedH = 0;
	bNeedV = 0;
	fViewportW = __xuiPopupMin(fContentW, fBaseW);
	fViewportH = __xuiPopupMin(fContentH, fBaseH);
	for ( i = 0; i < 4; i++ ) {
		fLimitW = fBaseW;
		fLimitH = fBaseH;
		if ( bNeedV && ((fContentW + fReserve) > fBaseW) ) {
			fLimitW = __xuiPopupMax(1.0f, fBaseW - fReserve);
		}
		if ( bNeedH && ((fContentH + fReserve) > fBaseH) ) {
			fLimitH = __xuiPopupMax(1.0f, fBaseH - fReserve);
		}
		fViewportW = __xuiPopupMin(fContentW, fLimitW);
		fViewportH = __xuiPopupMin(fContentH, fLimitH);
		bNextH = (fContentW > fViewportW);
		bNextV = (fContentH > fViewportH);
		if ( (bNextH == bNeedH) && (bNextV == bNeedV) ) {
			break;
		}
		bNeedH = bNextH;
		bNeedV = bNextV;
	}
	if ( pOuterW != NULL ) {
		*pOuterW = xuiInternalSnapSize(fViewportW + fInset + (bNeedV ? fReserve : 0.0f));
	}
	if ( pOuterH != NULL ) {
		*pOuterH = xuiInternalSnapSize(fViewportH + fInset + (bNeedH ? fReserve : 0.0f));
	}
}

static int __xuiPopupShouldUseShield(const xui_popup_data_t* pData)
{
	if ( pData == NULL ) {
		return 0;
	}
	if ( pData->bModal ) {
		return 1;
	}
	if ( pData->iOutsidePolicy != XUI_POPUP_OUTSIDE_IGNORE ) {
		return 1;
	}
	if ( pData->iOwnerPolicy != XUI_POPUP_OWNER_PASSTHROUGH ) {
		return 1;
	}
	return __xuiPopupAlpha(pData->iBackdropColor) != 0u;
}

static int __xuiPopupArrange(xui_widget pWidget, xui_popup_data_t* pData, float fWindowW, float fWindowH)
{
	xui_rect_t tShell;
	xui_rect_t tPanel;
	xui_rect_t tScroll;
	float fInset;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pPanel == NULL) || (pData->pScrollView == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->bShield = __xuiPopupShouldUseShield(pData);
	if ( pData->bShield ) {
		tShell.fX = 0.0f;
		tShell.fY = 0.0f;
		tShell.fW = fWindowW;
		tShell.fH = fWindowH;
		tPanel = pData->tPopupRect;
	} else {
		tShell = pData->tPopupRect;
		tPanel.fX = 0.0f;
		tPanel.fY = 0.0f;
		tPanel.fW = pData->tPopupRect.fW;
		tPanel.fH = pData->tPopupRect.fH;
	}
	pData->tPanelLocalRect = tPanel;
	iRet = xuiWidgetSetRect(pWidget, xuiInternalSnapRect(tShell));
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiWidgetSetRect(pData->pPanel, xuiInternalSnapRect(tPanel));
	if ( iRet != XUI_OK ) return iRet;
	fInset = __xuiPopupMax(0.0f, pData->fPadding + pData->fBorderWidth);
	tScroll.fX = fInset;
	tScroll.fY = fInset;
	tScroll.fW = __xuiPopupMax(1.0f, tPanel.fW - fInset * 2.0f);
	tScroll.fH = __xuiPopupMax(1.0f, tPanel.fH - fInset * 2.0f);
	iRet = xuiWidgetSetRect(pData->pScrollView, xuiInternalSnapRect(tScroll));
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiScrollViewLayout(pData->pScrollView);
	if ( iRet != XUI_OK ) return iRet;
	(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	return xuiWidgetInvalidate(pData->pPanel, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiPopupApplyPlacementData(xui_widget pWidget, xui_popup_data_t* pData)
{
	xui_context pContext;
	xui_vec2_t tWindow;
	xui_rect_t tAnchor;
	xui_rect_t tRect;
	xui_rect_t arrCandidate[4];
	xui_vec2_t tPoint;
	float fContentW;
	float fContentH;
	float fOuterW;
	float fOuterH;
	float fMargin;
	int arrDirection[4];
	int arrAnchor[4];
	int i;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) || (pData->pScrollView == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( !__xuiPopupOwnerAvailable(pData) ) {
		return __xuiPopupSetOpenInternal(pWidget, pData, 0, 1);
	}
	pContext = xuiWidgetGetContext(pWidget);
	tWindow = xuiGetViewportSize(pContext);
	if ( tWindow.fX <= 0.0f ) tWindow.fX = 1.0f;
	if ( tWindow.fY <= 0.0f ) tWindow.fY = 1.0f;
	tAnchor = __xuiPopupResolveAnchor(pWidget, pData);
	__xuiPopupResolveContentSize(pWidget, pData, tAnchor, &fContentW, &fContentH);
	pData->fContentWidth = fContentW;
	pData->fContentHeight = fContentH;
	iRet = xuiScrollViewSetContentSize(pData->pScrollView, fContentW, fContentH);
	if ( iRet != XUI_OK ) return iRet;
	__xuiPopupResolveOuterSize(pData, tWindow.fX, tWindow.fY, fContentW, fContentH, &fOuterW, &fOuterH);
	fMargin = __xuiPopupMax(0.0f, pData->fMargin);
	arrDirection[0] = pData->iDirection;
	arrDirection[1] = __xuiPopupFlipVertical(pData->iDirection);
	arrDirection[2] = __xuiPopupFlipHorizontal(pData->iDirection);
	arrDirection[3] = __xuiPopupFlipHorizontal(arrDirection[1]);
	arrAnchor[0] = pData->iAnchor;
	arrAnchor[1] = __xuiPopupFlipAnchorVertical(pData->iAnchor);
	arrAnchor[2] = __xuiPopupFlipAnchorHorizontal(pData->iAnchor);
	arrAnchor[3] = __xuiPopupFlipAnchorHorizontal(arrAnchor[1]);
	for ( i = 0; i < 4; i++ ) {
		tPoint = __xuiPopupAnchorPoint(tAnchor, arrAnchor[i]);
		arrCandidate[i] = __xuiPopupRectFrom(tPoint, fOuterW, fOuterH, pData->fGap, arrDirection[i]);
		arrCandidate[i].fX += pData->fOffsetX;
		arrCandidate[i].fY += pData->fOffsetY;
		if ( __xuiPopupRectFits(arrCandidate[i], tWindow.fX, tWindow.fY, fMargin) ) {
			break;
		}
	}
	if ( i >= 4 ) {
		tRect = arrCandidate[0];
		tRect.fX = __xuiPopupClamp(tRect.fX, fMargin, tWindow.fX - fOuterW - fMargin);
		tRect.fY = __xuiPopupClamp(tRect.fY, fMargin, tWindow.fY - fOuterH - fMargin);
	} else {
		tRect = arrCandidate[i];
	}
	tRect.fW = fOuterW;
	tRect.fH = fOuterH;
	pData->tPopupRect = xuiInternalSnapRect(tRect);
	return __xuiPopupArrange(pWidget, pData, tWindow.fX, tWindow.fY);
}

static xui_widget __xuiPopupFindFirstFocusable(xui_widget pWidget)
{
	xui_widget pChild;
	xui_widget pResult;

	if ( pWidget == NULL ) {
		return NULL;
	}
	if ( xuiWidgetGetFocusable(pWidget) && xuiWidgetGetVisible(pWidget) && xuiWidgetGetEnabled(pWidget) ) {
		return pWidget;
	}
	for ( pChild = xuiWidgetGetFirstChild(pWidget); pChild != NULL; pChild = xuiWidgetGetNextSibling(pChild) ) {
		pResult = __xuiPopupFindFirstFocusable(pChild);
		if ( pResult != NULL ) {
			return pResult;
		}
	}
	return NULL;
}

static int __xuiPopupFocusOpenTarget(xui_widget pWidget, xui_popup_data_t* pData)
{
	xui_widget pFocus;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_OK;
	}
	pFocus = NULL;
	switch ( pData->iFocusPolicy ) {
	case XUI_POPUP_FOCUS_NONE:
		return XUI_OK;
	case XUI_POPUP_FOCUS_FIRST_CHILD:
		pFocus = __xuiPopupFindFirstFocusable(pData->pContent);
		if ( pFocus == NULL ) {
			pFocus = pWidget;
		}
		break;
	case XUI_POPUP_FOCUS_CUSTOM:
		if ( pData->pCustomFocus != NULL && xuiInternalWidgetIsValid(pData->pCustomFocus) ) {
			pFocus = pData->pCustomFocus;
		}
		if ( pFocus == NULL ) {
			pFocus = pWidget;
		}
		break;
	case XUI_POPUP_FOCUS_POPUP:
	default:
		pFocus = pWidget;
		break;
	}
	if ( pFocus == NULL ) {
		return XUI_OK;
	}
	return xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pFocus);
}

static int __xuiPopupRestoreFocus(xui_widget pWidget, xui_popup_data_t* pData)
{
	xui_widget pRestore;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_OK;
	}
	pRestore = pData->pFocusRestore;
	if ( pRestore != NULL &&
	     xuiInternalWidgetIsValid(pRestore) &&
	     xuiWidgetGetVisible(pRestore) &&
	     xuiWidgetGetEnabled(pRestore) &&
	     xuiWidgetGetFocusable(pRestore) ) {
		(void)xuiSetFocusWidget(xuiWidgetGetContext(pWidget), pRestore);
	}
	if ( !pData->bFocusRestoreExplicit ) {
		pData->pFocusRestore = NULL;
	}
	return XUI_OK;
}

static void __xuiPopupNotify(xui_widget pWidget, xui_popup_data_t* pData)
{
	if ( (pWidget == NULL) || (pData == NULL) ) {
		return;
	}
	pData->iChangeCount++;
	if ( pData->onChange != NULL ) {
		pData->onChange(pWidget, pData->bOpen, pData->pChangeUser);
	}
}

static int __xuiPopupSetOpenInternal(xui_widget pWidget, xui_popup_data_t* pData, int bOpen, int bNotify)
{
	xui_context pContext;
	int iRet;

	if ( (pWidget == NULL) || (pData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pContext = xuiWidgetGetContext(pWidget);
	bOpen = bOpen ? 1 : 0;
	if ( pData->bOpen == bOpen ) {
		if ( bOpen ) {
			iRet = xuiOverlayAttach(pContext, pData->pOwner, pWidget, XUI_LAYER_POPUP, 0);
			if ( iRet != XUI_OK ) return iRet;
			iRet = xuiOverlayBringToFront(pWidget);
			if ( iRet != XUI_OK ) return iRet;
			return __xuiPopupApplyPlacementData(pWidget, pData);
		}
		return XUI_OK;
	}
	if ( bOpen ) {
		if ( !__xuiPopupOwnerAvailable(pData) ) {
			return XUI_OK;
		}
		iRet = xuiOverlayAttach(pContext, pData->pOwner, pWidget, XUI_LAYER_POPUP, 0);
		if ( iRet != XUI_OK ) return iRet;
		iRet = xuiOverlayBringToFront(pWidget);
		if ( iRet != XUI_OK ) return iRet;
		if ( !pData->bFocusRestoreExplicit ) {
			pData->pFocusRestore = xuiGetFocusWidget(pContext);
		}
		pData->bOpen = 1;
		iRet = xuiWidgetSetEnabled(pWidget, 1);
		if ( iRet != XUI_OK ) return iRet;
		iRet = xuiWidgetSetVisible(pWidget, 1);
		if ( iRet != XUI_OK ) return iRet;
		(void)xuiScrollViewSetOffset(pData->pScrollView, 0.0f, 0.0f);
		iRet = __xuiPopupApplyPlacementData(pWidget, pData);
		if ( iRet != XUI_OK ) return iRet;
		(void)__xuiPopupFocusOpenTarget(pWidget, pData);
	} else {
		pData->bOpen = 0;
		(void)xuiReleasePointerCapture(pContext, pWidget);
		iRet = xuiWidgetSetVisible(pWidget, 0);
		if ( iRet != XUI_OK ) return iRet;
		(void)__xuiPopupRestoreFocus(pWidget, pData);
		(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	if ( bNotify ) {
		__xuiPopupNotify(pWidget, pData);
	}
	return XUI_OK;
}

static int __xuiPopupPointerPress(int iType)
{
	return (iType == XUI_EVENT_POINTER_DOWN) || (iType == XUI_EVENT_POINTER_WHEEL);
}

static int __xuiPopupEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_popup_data_t* pData;
	xui_rect_t tOwnerRect;
	int bInside;
	int bOwner;
	int bPress;

	pData = (xui_popup_data_t*)pUser;
	if ( (pWidget == NULL) || (pData == NULL) || (pEvent == NULL) || !pData->bOpen ) {
		return XUI_OK;
	}
	if ( !__xuiPopupOwnerAvailable(pData) ) {
		(void)__xuiPopupSetOpenInternal(pWidget, pData, 0, 1);
		return XUI_EVENT_DISPATCH_STOP;
	}
	if ( pEvent->iType == XUI_EVENT_KEY_DOWN ) {
		if ( (pEvent->iKey == XUI_KEY_ESCAPE) && (pData->iEscapePolicy == XUI_POPUP_ESCAPE_CLOSE) ) {
			(void)__xuiPopupSetOpenInternal(pWidget, pData, 0, 1);
			return XUI_EVENT_DISPATCH_STOP;
		}
		return XUI_OK;
	}
	if ( (pEvent->iType != XUI_EVENT_POINTER_DOWN) &&
	     (pEvent->iType != XUI_EVENT_POINTER_UP) &&
	     (pEvent->iType != XUI_EVENT_POINTER_CLICK) &&
	     (pEvent->iType != XUI_EVENT_POINTER_WHEEL) ) {
		return XUI_OK;
	}
	bInside = __xuiPopupRectContains(pData->tPopupRect, pEvent->fX, pEvent->fY);
	if ( bInside ) {
		if ( pEvent->iType == XUI_EVENT_POINTER_DOWN ) {
			(void)__xuiPopupFocusOpenTarget(pWidget, pData);
		}
		if ( pData->bConsumeInside && (pEvent->iPhase != XUI_EVENT_PHASE_CAPTURE) ) {
			return XUI_EVENT_DISPATCH_STOP;
		}
		return XUI_OK;
	}
	bOwner = 0;
	if ( pData->pOwner != NULL && xuiInternalWidgetIsValid(pData->pOwner) ) {
		tOwnerRect = xuiWidgetGetWorldRect(pData->pOwner);
		bOwner = __xuiPopupRectContains(tOwnerRect, pEvent->fX, pEvent->fY);
	}
	bPress = __xuiPopupPointerPress(pEvent->iType);
	if ( bOwner ) {
		if ( bPress && ((pData->iOwnerPolicy == XUI_POPUP_OWNER_CLOSE) || (pData->iOwnerPolicy == XUI_POPUP_OWNER_TOGGLE)) ) {
			(void)__xuiPopupSetOpenInternal(pWidget, pData, 0, 1);
			return (pData->iOwnerPolicy == XUI_POPUP_OWNER_CLOSE) ? XUI_EVENT_DISPATCH_STOP : XUI_OK;
		}
		return (pData->iOwnerPolicy == XUI_POPUP_OWNER_CONSUME) ? XUI_EVENT_DISPATCH_STOP : XUI_OK;
	}
	if ( bPress ) {
		if ( pData->iOutsidePolicy == XUI_POPUP_OUTSIDE_CLOSE ) {
			(void)__xuiPopupSetOpenInternal(pWidget, pData, 0, 1);
			return XUI_EVENT_DISPATCH_STOP;
		}
		if ( (pData->iOutsidePolicy == XUI_POPUP_OUTSIDE_CONSUME) || pData->bModal ) {
			return XUI_EVENT_DISPATCH_STOP;
		}
	}
	return pData->bShield ? XUI_EVENT_DISPATCH_STOP : XUI_OK;
}

static void __xuiPopupCancelAction(xui_widget pWidget, void* pUser)
{
	xui_popup_data_t* pData;

	pData = (xui_popup_data_t*)pUser;
	if ( (pWidget != NULL) && (pData != NULL) && pData->bOpen && (pData->iEscapePolicy == XUI_POPUP_ESCAPE_CLOSE) ) {
		(void)__xuiPopupSetOpenInternal(pWidget, pData, 0, 1);
	}
}

static int __xuiPopupPanelCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_popup_data_t* pData;
	xui_proxy pProxy;
	xui_rect_t tRect;
	xui_rect_t tShadow;
	int iRet;

	(void)iStateId;
	pData = (xui_popup_data_t*)pUser;
	if ( (pWidget == NULL) || (pData == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	tRect = xuiWidgetGetContentRect(pWidget);
	if ( (__xuiPopupAlpha(pData->iShadowColor) != 0u) && (pData->fShadowSize > 0.0f) ) {
		tShadow = tRect;
		tShadow.fX += pData->fShadowSize;
		tShadow.fY += pData->fShadowSize;
		iRet = pProxy->drawRectFill(pProxy, pDraw, tShadow, pData->iShadowColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( __xuiPopupAlpha(pData->iPanelColor) != 0u ) {
		iRet = pProxy->drawRectFill(pProxy, pDraw, tRect, pData->iPanelColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	if ( (__xuiPopupAlpha(pData->iBorderColor) != 0u) && (pData->fBorderWidth > 0.0f) ) {
		iRet = pProxy->drawRectStroke(pProxy, pDraw, tRect, pData->fBorderWidth, pData->iBorderColor);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static int __xuiPopupShellCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_popup_data_t* pData;
	xui_proxy pProxy;
	xui_rect_t tRect;

	(void)iStateId;
	pData = (xui_popup_data_t*)pUser;
	if ( (pWidget == NULL) || (pData == NULL) || (pDraw == NULL) || !pData->bOpen || !pData->bShield || (__xuiPopupAlpha(pData->iBackdropColor) == 0u) ) {
		return XUI_OK;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	tRect = xuiWidgetGetContentRect(pWidget);
	return pProxy->drawRectFill(pProxy, pDraw, tRect, pData->iBackdropColor);
}

static void __xuiPopupDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_MANUAL;
	pLayout->iWidthMode = XUI_SIZE_CONTENT;
	pLayout->iHeightMode = XUI_SIZE_CONTENT;
	pLayout->iFlowMode = XUI_FLOW_ABSOLUTE;
	pLayout->iOverflow = XUI_OVERFLOW_VISIBLE;
	pLayout->iAlignX = XUI_ALIGN_START;
	pLayout->iAlignY = XUI_ALIGN_START;
	pLayout->iTableRowSpan = 1;
	pLayout->iTableColumnSpan = 1;
	pLayout->iGridColumnCount = 1;
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fShrink = 1.0f;
	pLayout->iLayer = XUI_LAYER_POPUP;
}

static void __xuiPopupDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static void __xuiPopupInitDefaults(xui_popup_data_t* pData)
{
	memset(pData, 0, sizeof(*pData));
	pData->iAnchor = XUI_POPUP_ANCHOR_BOTTOM_LEFT;
	pData->iDirection = XUI_POPUP_DIRECTION_RIGHT_DOWN;
	pData->iOutsidePolicy = XUI_POPUP_OUTSIDE_CLOSE;
	pData->iOwnerPolicy = XUI_POPUP_OWNER_PASSTHROUGH;
	pData->iEscapePolicy = XUI_POPUP_ESCAPE_CLOSE;
	pData->iFocusPolicy = XUI_POPUP_FOCUS_POPUP;
	pData->iScrollbarMode = XUI_SCROLLBAR_MODE_COMPACT;
	pData->fGap = 4.0f;
	pData->fMargin = 6.0f;
	pData->fPadding = 4.0f;
	pData->fBorderWidth = 1.0f;
	pData->fShadowSize = 4.0f;
	pData->fScrollbarSize = 8.0f;
	pData->iPanelColor = XUI_COLOR_RGBA(250, 253, 255, 255);
	pData->iBorderColor = XUI_COLOR_RGBA(166, 190, 218, 255);
	pData->iShadowColor = XUI_COLOR_RGBA(35, 63, 96, 40);
	pData->iBackdropColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	pData->iTrackColor = XUI_COLOR_RGBA(226, 238, 249, 255);
	pData->iThumbColor = XUI_COLOR_RGBA(91, 139, 190, 210);
	pData->iHoverColor = XUI_COLOR_RGBA(68, 128, 198, 235);
	pData->iActiveColor = XUI_COLOR_RGBA(40, 104, 174, 255);
	pData->iFocusColor = XUI_COLOR_RGBA(54, 148, 224, 255);
	pData->iDisabledColor = XUI_COLOR_RGBA(176, 190, 206, 150);
}

static void __xuiPopupApplyDesc(xui_popup_data_t* pData, const xui_popup_desc_t* pDesc)
{
	if ( (pData == NULL) || (pDesc == NULL) ) {
		return;
	}
	pData->pOwner = pDesc->pOwner;
	if ( __xuiPopupFloatValid(pDesc->fContentWidth) && pDesc->fContentWidth > 0.0f ) pData->fContentWidth = pDesc->fContentWidth;
	if ( __xuiPopupFloatValid(pDesc->fContentHeight) && pDesc->fContentHeight > 0.0f ) pData->fContentHeight = pDesc->fContentHeight;
	if ( __xuiPopupFloatValid(pDesc->fMaxWidth) && pDesc->fMaxWidth > 0.0f ) pData->fMaxWidth = pDesc->fMaxWidth;
	if ( __xuiPopupFloatValid(pDesc->fMaxHeight) && pDesc->fMaxHeight > 0.0f ) pData->fMaxHeight = pDesc->fMaxHeight;
	if ( __xuiPopupFloatValid(pDesc->fGap) && pDesc->fGap >= 0.0f ) pData->fGap = pDesc->fGap;
	if ( __xuiPopupFloatValid(pDesc->fOffsetX) ) pData->fOffsetX = pDesc->fOffsetX;
	if ( __xuiPopupFloatValid(pDesc->fOffsetY) ) pData->fOffsetY = pDesc->fOffsetY;
	if ( __xuiPopupFloatValid(pDesc->fMargin) && pDesc->fMargin >= 0.0f ) pData->fMargin = pDesc->fMargin;
	if ( __xuiPopupFloatValid(pDesc->fPadding) && pDesc->fPadding >= 0.0f ) pData->fPadding = pDesc->fPadding;
	if ( __xuiPopupFloatValid(pDesc->fBorderWidth) && pDesc->fBorderWidth >= 0.0f ) pData->fBorderWidth = pDesc->fBorderWidth;
	if ( __xuiPopupFloatValid(pDesc->fShadowSize) && pDesc->fShadowSize >= 0.0f ) pData->fShadowSize = pDesc->fShadowSize;
	if ( __xuiPopupAnchorValid(pDesc->iAnchor) ) pData->iAnchor = pDesc->iAnchor;
	if ( __xuiPopupDirectionValid(pDesc->iDirection) ) pData->iDirection = pDesc->iDirection;
	if ( __xuiPopupOutsidePolicyValid(pDesc->iOutsidePolicy) ) pData->iOutsidePolicy = pDesc->iOutsidePolicy;
	if ( __xuiPopupOwnerPolicyValid(pDesc->iOwnerPolicy) ) pData->iOwnerPolicy = pDesc->iOwnerPolicy;
	if ( __xuiPopupEscapePolicyValid(pDesc->iEscapePolicy) ) pData->iEscapePolicy = pDesc->iEscapePolicy;
	if ( __xuiPopupFocusPolicyValid(pDesc->iFocusPolicy) ) pData->iFocusPolicy = pDesc->iFocusPolicy;
	pData->bModal = pDesc->bModal ? 1 : 0;
	pData->bConsumeInside = pDesc->bConsumeInside ? 1 : 0;
	pData->bMatchOwnerWidth = pDesc->bMatchOwnerWidth ? 1 : 0;
	if ( __xuiPopupFloatValid(pDesc->fScrollbarSize) && pDesc->fScrollbarSize > 0.0f ) pData->fScrollbarSize = pDesc->fScrollbarSize;
	if ( __xuiPopupScrollbarModeValid(pDesc->iScrollbarMode) ) pData->iScrollbarMode = pDesc->iScrollbarMode;
	if ( __xuiPopupAlpha(pDesc->iPanelColor) != 0u ) pData->iPanelColor = pDesc->iPanelColor;
	if ( __xuiPopupAlpha(pDesc->iBorderColor) != 0u ) pData->iBorderColor = pDesc->iBorderColor;
	if ( pDesc->iShadowColor != 0u ) pData->iShadowColor = pDesc->iShadowColor;
	if ( pDesc->iBackdropColor != 0u ) pData->iBackdropColor = pDesc->iBackdropColor;
	if ( pDesc->iTrackColor != 0u ) pData->iTrackColor = pDesc->iTrackColor;
	if ( pDesc->iThumbColor != 0u ) pData->iThumbColor = pDesc->iThumbColor;
	if ( pDesc->iHoverColor != 0u ) pData->iHoverColor = pDesc->iHoverColor;
	if ( pDesc->iActiveColor != 0u ) pData->iActiveColor = pDesc->iActiveColor;
	if ( pDesc->iFocusColor != 0u ) pData->iFocusColor = pDesc->iFocusColor;
	if ( pDesc->iDisabledColor != 0u ) pData->iDisabledColor = pDesc->iDisabledColor;
}

static int __xuiPopupInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_popup_data_t* pData;
	const xui_popup_desc_t* pDesc;
	xui_scroll_view_desc_t tScrollDesc;
	xui_cache_policy_t tPolicy;
	int bOpen;
	int iRet;

	(void)pUser;
	pData = (xui_popup_data_t*)pTypeData;
	pDesc = (const xui_popup_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiPopupDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiPopupInitDefaults(pData);
	__xuiPopupApplyDesc(pData, pDesc);
	bOpen = (pDesc != NULL && pDesc->bOpen) ? 1 : 0;
	memset(&tScrollDesc, 0, sizeof(tScrollDesc));
	tScrollDesc.iSize = sizeof(tScrollDesc);
	tScrollDesc.fContentWidth = pData->fContentWidth;
	tScrollDesc.fContentHeight = pData->fContentHeight;
	tScrollDesc.iPolicyX = XUI_SCROLLBAR_POLICY_AUTO;
	tScrollDesc.iPolicyY = XUI_SCROLLBAR_POLICY_AUTO;
	tScrollDesc.iScrollbarMode = pData->iScrollbarMode;
	tScrollDesc.iWheelAxis = XUI_WHEEL_AXIS_VERTICAL;
	tScrollDesc.iCornerMode = XUI_SCROLL_FRAME_CORNER_AUTO;
	tScrollDesc.bContentDragEnabled = 0;
	tScrollDesc.fScrollbarSize = pData->fScrollbarSize;
	tScrollDesc.fMinThumbSize = 18.0f;
	tScrollDesc.fButtonSize = (pData->iScrollbarMode == XUI_SCROLLBAR_MODE_FULL) ? pData->fScrollbarSize : 0.0f;
	tScrollDesc.fWheelStep = 32.0f;
	tScrollDesc.iBackgroundColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	tScrollDesc.iTrackColor = pData->iTrackColor;
	tScrollDesc.iThumbColor = pData->iThumbColor;
	tScrollDesc.iHoverColor = pData->iHoverColor;
	tScrollDesc.iActiveColor = pData->iActiveColor;
	tScrollDesc.iFocusColor = pData->iFocusColor;
	tScrollDesc.iDisabledColor = pData->iDisabledColor;
	tScrollDesc.iCornerColor = XUI_COLOR_RGBA(229, 238, 248, 255);
	tScrollDesc.iGripColor = XUI_COLOR_RGBA(128, 153, 180, 180);
	iRet = xuiWidgetCreate(xuiWidgetGetContext(pWidget), &pData->pPanel);
	if ( iRet != XUI_OK ) return iRet;
	iRet = xuiScrollViewCreate(xuiWidgetGetContext(pWidget), &pData->pScrollView, &tScrollDesc);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pData->pPanel);
		pData->pPanel = NULL;
		return iRet;
	}
	pData->pContent = xuiScrollViewGetContentWidget(pData->pScrollView);
	(void)xuiWidgetSetLayoutType(pWidget, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetFlowMode(pWidget, XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_VISIBLE);
	(void)xuiWidgetSetLayer(pWidget, XUI_LAYER_POPUP, 0);
	(void)xuiWidgetSetFocusable(pWidget, 1);
	(void)xuiWidgetSetTabStop(pWidget, 0);
	(void)xuiWidgetSetFocusScope(pWidget, 1);
	(void)xuiWidgetSetEventCallback(pWidget, __xuiPopupEvent, pData);
	(void)xuiWidgetSetCancelAction(pWidget, __xuiPopupCancelAction, pData);
	__xuiPopupDefaultCachePolicy(&tPolicy);
	(void)xuiWidgetSetCachePolicy(pWidget, &tPolicy);
	(void)xuiWidgetSetCacheRenderCallback(pWidget, __xuiPopupShellCacheRender, pData);
	(void)xuiWidgetSetLayoutType(pData->pPanel, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetFlowMode(pData->pPanel, XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetOverflow(pData->pPanel, XUI_OVERFLOW_VISIBLE);
	(void)xuiWidgetSetFocusable(pData->pPanel, 0);
	(void)xuiWidgetSetTabStop(pData->pPanel, 0);
	(void)xuiWidgetSetCachePolicy(pData->pPanel, &tPolicy);
	(void)xuiWidgetSetCacheRenderCallback(pData->pPanel, __xuiPopupPanelCacheRender, pData);
	(void)xuiWidgetSetOverflow(pData->pScrollView, XUI_OVERFLOW_CLIP);
	iRet = xuiWidgetAddChild(pWidget, pData->pPanel);
	if ( iRet == XUI_OK ) iRet = xuiWidgetAddChild(pData->pPanel, pData->pScrollView);
	if ( iRet != XUI_OK ) {
		xuiWidgetDestroy(pData->pScrollView);
		xuiWidgetDestroy(pData->pPanel);
		pData->pScrollView = NULL;
		pData->pPanel = NULL;
		pData->pContent = NULL;
		return iRet;
	}
	(void)xuiWidgetSetVisible(pWidget, 0);
	(void)xuiWidgetSetEnabled(pWidget, 1);
	if ( bOpen ) {
		iRet = __xuiPopupSetOpenInternal(pWidget, pData, 1, 0);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

static void __xuiPopupDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_popup_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_popup_data_t*)pTypeData;
	if ( pData != NULL ) {
		memset(pData, 0, sizeof(*pData));
	}
}

XUI_API xui_widget_type xuiPopupGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "popup");
	if ( pType != NULL ) {
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "popup";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_popup_data_t);
	tDesc.onInit = __xuiPopupInit;
	tDesc.onDestroy = __xuiPopupDestroy;
	tDesc.onCacheRender = __xuiPopupShellCacheRender;
	__xuiPopupDefaultLayout(&tDesc.tLayout);
	__xuiPopupDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	return pType;
}

XUI_API int xuiPopupCreate(xui_context pContext, xui_widget* ppWidget, const xui_popup_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiPopupDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiPopupGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiPopupSetChange(xui_widget pWidget, xui_popup_change_proc onChange, void* pUser)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->onChange = onChange;
	pData->pChangeUser = pUser;
	return XUI_OK;
}

XUI_API int xuiPopupSetOpen(xui_widget pWidget, int bOpen)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiPopupSetOpenInternal(pWidget, pData, bOpen, 1);
}

XUI_API int xuiPopupIsOpen(xui_widget pWidget)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	return (pData != NULL) ? pData->bOpen : 0;
}

XUI_API int xuiPopupToggle(xui_widget pWidget)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiPopupSetOpenInternal(pWidget, pData, !pData->bOpen, 1);
}

XUI_API int xuiPopupApplyPlacement(xui_widget pWidget)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return __xuiPopupApplyPlacementData(pWidget, pData);
}

XUI_API int xuiPopupSetOwner(xui_widget pWidget, xui_widget pOwner)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pOwner != NULL && (!xuiInternalWidgetIsValid(pOwner) || xuiWidgetGetContext(pOwner) != xuiWidgetGetContext(pWidget)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->pOwner = pOwner;
	pWidget->pOverlayOwner = pOwner;
	return pData->bOpen ? xuiPopupApplyPlacement(pWidget) : XUI_OK;
}

XUI_API xui_widget xuiPopupGetOwner(xui_widget pWidget)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	return (pData != NULL) ? pData->pOwner : NULL;
}

XUI_API int xuiPopupSetAnchorRect(xui_widget pWidget, xui_rect_t tAnchor)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL || !__xuiPopupFloatValid(tAnchor.fX) || !__xuiPopupFloatValid(tAnchor.fY) || tAnchor.fW < 0.0f || tAnchor.fH < 0.0f ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->tAnchorRect = tAnchor;
	pData->bAnchorRectSet = 1;
	return pData->bOpen ? xuiPopupApplyPlacement(pWidget) : XUI_OK;
}

XUI_API int xuiPopupClearAnchorRect(xui_widget pWidget)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	memset(&pData->tAnchorRect, 0, sizeof(pData->tAnchorRect));
	pData->bAnchorRectSet = 0;
	return pData->bOpen ? xuiPopupApplyPlacement(pWidget) : XUI_OK;
}

XUI_API xui_rect_t xuiPopupGetAnchorRect(xui_widget pWidget)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	return (pData != NULL) ? __xuiPopupResolveAnchor(pWidget, pData) : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API int xuiPopupSetAnchor(xui_widget pWidget, int iAnchor)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL || !__xuiPopupAnchorValid(iAnchor) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iAnchor = iAnchor;
	return pData->bOpen ? xuiPopupApplyPlacement(pWidget) : XUI_OK;
}

XUI_API int xuiPopupGetAnchor(xui_widget pWidget)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	return (pData != NULL) ? pData->iAnchor : XUI_POPUP_ANCHOR_BOTTOM_LEFT;
}

XUI_API int xuiPopupSetDirection(xui_widget pWidget, int iDirection)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL || !__xuiPopupDirectionValid(iDirection) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iDirection = iDirection;
	return pData->bOpen ? xuiPopupApplyPlacement(pWidget) : XUI_OK;
}

XUI_API int xuiPopupGetDirection(xui_widget pWidget)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	return (pData != NULL) ? pData->iDirection : XUI_POPUP_DIRECTION_RIGHT_DOWN;
}

XUI_API int xuiPopupSetGap(xui_widget pWidget, float fGap)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL || !__xuiPopupFloatValid(fGap) || fGap < 0.0f ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fGap = fGap;
	return pData->bOpen ? xuiPopupApplyPlacement(pWidget) : XUI_OK;
}

XUI_API float xuiPopupGetGap(xui_widget pWidget)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	return (pData != NULL) ? pData->fGap : 0.0f;
}

XUI_API int xuiPopupSetOffset(xui_widget pWidget, float fOffsetX, float fOffsetY)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL || !__xuiPopupFloatValid(fOffsetX) || !__xuiPopupFloatValid(fOffsetY) ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fOffsetX = fOffsetX;
	pData->fOffsetY = fOffsetY;
	return pData->bOpen ? xuiPopupApplyPlacement(pWidget) : XUI_OK;
}

XUI_API int xuiPopupGetOffset(xui_widget pWidget, float* pOffsetX, float* pOffsetY)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pOffsetX != NULL ) *pOffsetX = pData->fOffsetX;
	if ( pOffsetY != NULL ) *pOffsetY = pData->fOffsetY;
	return XUI_OK;
}

XUI_API int xuiPopupSetMargin(xui_widget pWidget, float fMargin)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL || !__xuiPopupFloatValid(fMargin) || fMargin < 0.0f ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->fMargin = fMargin;
	return pData->bOpen ? xuiPopupApplyPlacement(pWidget) : XUI_OK;
}

XUI_API float xuiPopupGetMargin(xui_widget pWidget)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	return (pData != NULL) ? pData->fMargin : 0.0f;
}

XUI_API int xuiPopupSetContentSize(xui_widget pWidget, float fWidth, float fHeight)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL || !__xuiPopupFloatValid(fWidth) || !__xuiPopupFloatValid(fHeight) || fWidth < 0.0f || fHeight < 0.0f ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->fContentWidth = fWidth;
	pData->fContentHeight = fHeight;
	if ( pData->pScrollView != NULL ) {
		(void)xuiScrollViewSetContentSize(pData->pScrollView, fWidth, fHeight);
	}
	return pData->bOpen ? xuiPopupApplyPlacement(pWidget) : XUI_OK;
}

XUI_API int xuiPopupGetContentSize(xui_widget pWidget, float* pWidth, float* pHeight)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pWidth != NULL ) *pWidth = pData->fContentWidth;
	if ( pHeight != NULL ) *pHeight = pData->fContentHeight;
	return XUI_OK;
}

XUI_API int xuiPopupSetMaxSize(xui_widget pWidget, float fMaxWidth, float fMaxHeight)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL || !__xuiPopupFloatValid(fMaxWidth) || !__xuiPopupFloatValid(fMaxHeight) || fMaxWidth < 0.0f || fMaxHeight < 0.0f ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->fMaxWidth = fMaxWidth;
	pData->fMaxHeight = fMaxHeight;
	return pData->bOpen ? xuiPopupApplyPlacement(pWidget) : XUI_OK;
}

XUI_API int xuiPopupGetMaxSize(xui_widget pWidget, float* pMaxWidth, float* pMaxHeight)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pMaxWidth != NULL ) *pMaxWidth = pData->fMaxWidth;
	if ( pMaxHeight != NULL ) *pMaxHeight = pData->fMaxHeight;
	return XUI_OK;
}

XUI_API int xuiPopupSetMatchOwnerWidth(xui_widget pWidget, int bEnabled)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bMatchOwnerWidth = bEnabled ? 1 : 0;
	return pData->bOpen ? xuiPopupApplyPlacement(pWidget) : XUI_OK;
}

XUI_API int xuiPopupGetMatchOwnerWidth(xui_widget pWidget)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	return (pData != NULL) ? pData->bMatchOwnerWidth : 0;
}

XUI_API int xuiPopupSetClosePolicy(xui_widget pWidget, int iOutsidePolicy, int iOwnerPolicy, int iEscapePolicy)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL || !__xuiPopupOutsidePolicyValid(iOutsidePolicy) || !__xuiPopupOwnerPolicyValid(iOwnerPolicy) || !__xuiPopupEscapePolicyValid(iEscapePolicy) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->iOutsidePolicy = iOutsidePolicy;
	pData->iOwnerPolicy = iOwnerPolicy;
	pData->iEscapePolicy = iEscapePolicy;
	return pData->bOpen ? xuiPopupApplyPlacement(pWidget) : XUI_OK;
}

XUI_API int xuiPopupGetClosePolicy(xui_widget pWidget, int* pOutsidePolicy, int* pOwnerPolicy, int* pEscapePolicy)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pOutsidePolicy != NULL ) *pOutsidePolicy = pData->iOutsidePolicy;
	if ( pOwnerPolicy != NULL ) *pOwnerPolicy = pData->iOwnerPolicy;
	if ( pEscapePolicy != NULL ) *pEscapePolicy = pData->iEscapePolicy;
	return XUI_OK;
}

XUI_API int xuiPopupSetModal(xui_widget pWidget, int bModal)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bModal = bModal ? 1 : 0;
	return pData->bOpen ? xuiPopupApplyPlacement(pWidget) : XUI_OK;
}

XUI_API int xuiPopupIsModal(xui_widget pWidget)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	return (pData != NULL) ? pData->bModal : 0;
}

XUI_API int xuiPopupSetConsumeInside(xui_widget pWidget, int bEnabled)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->bConsumeInside = bEnabled ? 1 : 0;
	return XUI_OK;
}

XUI_API int xuiPopupGetConsumeInside(xui_widget pWidget)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	return (pData != NULL) ? pData->bConsumeInside : 0;
}

XUI_API int xuiPopupSetFocusPolicy(xui_widget pWidget, int iFocusPolicy, xui_widget pCustomFocus)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL || !__xuiPopupFocusPolicyValid(iFocusPolicy) ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pCustomFocus != NULL && (!xuiInternalWidgetIsValid(pCustomFocus) || xuiWidgetGetContext(pCustomFocus) != xuiWidgetGetContext(pWidget)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->iFocusPolicy = iFocusPolicy;
	pData->pCustomFocus = pCustomFocus;
	return XUI_OK;
}

XUI_API int xuiPopupGetFocusPolicy(xui_widget pWidget)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	return (pData != NULL) ? pData->iFocusPolicy : XUI_POPUP_FOCUS_POPUP;
}

XUI_API int xuiPopupSetFocusRestore(xui_widget pWidget, xui_widget pRestore)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pRestore != NULL && (!xuiInternalWidgetIsValid(pRestore) || xuiWidgetGetContext(pRestore) != xuiWidgetGetContext(pWidget)) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->pFocusRestore = pRestore;
	pData->bFocusRestoreExplicit = (pRestore != NULL);
	return XUI_OK;
}

XUI_API int xuiPopupSetScroll(xui_widget pWidget, float fOffsetX, float fOffsetY)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL || pData->pScrollView == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollViewSetOffset(pData->pScrollView, fOffsetX, fOffsetY);
}

XUI_API int xuiPopupGetScroll(xui_widget pWidget, float* pOffsetX, float* pOffsetY)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL || pData->pScrollView == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	return xuiScrollViewGetOffset(pData->pScrollView, pOffsetX, pOffsetY);
}

XUI_API int xuiPopupSetScrollbarStyle(xui_widget pWidget, int iMode, float fScrollbarSize)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL || !__xuiPopupScrollbarModeValid(iMode) || !__xuiPopupFloatValid(fScrollbarSize) || fScrollbarSize <= 0.0f ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->iScrollbarMode = iMode;
	pData->fScrollbarSize = fScrollbarSize;
	__xuiPopupApplyScrollStyleData(pData);
	return pData->bOpen ? xuiPopupApplyPlacement(pWidget) : XUI_OK;
}

XUI_API int xuiPopupGetScrollbarStyle(xui_widget pWidget, int* pMode, float* pScrollbarSize)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pMode != NULL ) *pMode = pData->iScrollbarMode;
	if ( pScrollbarSize != NULL ) *pScrollbarSize = pData->fScrollbarSize;
	return XUI_OK;
}

XUI_API xui_widget xuiPopupGetPanelWidget(xui_widget pWidget)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	return (pData != NULL) ? pData->pPanel : NULL;
}

XUI_API xui_widget xuiPopupGetScrollViewWidget(xui_widget pWidget)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	return (pData != NULL) ? pData->pScrollView : NULL;
}

XUI_API xui_widget xuiPopupGetFrameWidget(xui_widget pWidget)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	return (pData != NULL && pData->pScrollView != NULL) ? xuiScrollViewGetFrameWidget(pData->pScrollView) : NULL;
}

XUI_API xui_widget xuiPopupGetViewportWidget(xui_widget pWidget)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	return (pData != NULL && pData->pScrollView != NULL) ? xuiScrollViewGetViewportWidget(pData->pScrollView) : NULL;
}

XUI_API xui_widget xuiPopupGetContentWidget(xui_widget pWidget)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	return (pData != NULL) ? pData->pContent : NULL;
}

XUI_API xui_scroll_model_t* xuiPopupGetModel(xui_widget pWidget)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	return (pData != NULL && pData->pScrollView != NULL) ? xuiScrollViewGetModel(pData->pScrollView) : NULL;
}

XUI_API xui_rect_t xuiPopupGetPopupRect(xui_widget pWidget)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	return (pData != NULL) ? pData->tPopupRect : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API xui_rect_t xuiPopupGetViewportRect(xui_widget pWidget)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	return (pData != NULL && pData->pScrollView != NULL) ? xuiScrollViewGetViewportRect(pData->pScrollView) : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API xui_rect_t xuiPopupGetContentRect(xui_widget pWidget)
{
	xui_popup_data_t* pData;
	xui_rect_t tViewport;
	xui_rect_t tContent;
	float fOffsetX;
	float fOffsetY;

	pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL || pData->pScrollView == NULL ) {
		return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	}
	tViewport = xuiScrollViewGetViewportRect(pData->pScrollView);
	fOffsetX = 0.0f;
	fOffsetY = 0.0f;
	(void)xuiScrollViewGetOffset(pData->pScrollView, &fOffsetX, &fOffsetY);
	tContent.fX = tViewport.fX - fOffsetX;
	tContent.fY = tViewport.fY - fOffsetY;
	tContent.fW = pData->fContentWidth;
	tContent.fH = pData->fContentHeight;
	return tContent;
}

XUI_API int xuiPopupSetColors(xui_widget pWidget, uint32_t iPanel, uint32_t iBorder, uint32_t iShadow, uint32_t iBackdrop)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	pData->iPanelColor = iPanel;
	pData->iBorderColor = iBorder;
	pData->iShadowColor = iShadow;
	pData->iBackdropColor = iBackdrop;
	if ( pData->bOpen ) {
		(void)xuiPopupApplyPlacement(pWidget);
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiPopupGetColors(xui_widget pWidget, uint32_t* pPanel, uint32_t* pBorder, uint32_t* pShadow, uint32_t* pBackdrop)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pPanel != NULL ) *pPanel = pData->iPanelColor;
	if ( pBorder != NULL ) *pBorder = pData->iBorderColor;
	if ( pShadow != NULL ) *pShadow = pData->iShadowColor;
	if ( pBackdrop != NULL ) *pBackdrop = pData->iBackdropColor;
	return XUI_OK;
}

XUI_API int xuiPopupSetMetrics(xui_widget pWidget, float fPadding, float fBorderWidth, float fShadowSize)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL || !__xuiPopupFloatValid(fPadding) || !__xuiPopupFloatValid(fBorderWidth) || !__xuiPopupFloatValid(fShadowSize) ||
	     fPadding < 0.0f || fBorderWidth < 0.0f || fShadowSize < 0.0f ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData->fPadding = fPadding;
	pData->fBorderWidth = fBorderWidth;
	pData->fShadowSize = fShadowSize;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiPopupGetMetrics(xui_widget pWidget, float* pPadding, float* pBorderWidth, float* pShadowSize)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pPadding != NULL ) *pPadding = pData->fPadding;
	if ( pBorderWidth != NULL ) *pBorderWidth = pData->fBorderWidth;
	if ( pShadowSize != NULL ) *pShadowSize = pData->fShadowSize;
	return XUI_OK;
}

XUI_API int xuiPopupGetChangeCount(xui_widget pWidget)
{
	xui_popup_data_t* pData = __xuiPopupGetData(pWidget);
	return (pData != NULL) ? pData->iChangeCount : 0;
}
