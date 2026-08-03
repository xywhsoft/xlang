#include "xui_internal.h"

#include <string.h>

#define XUI_TOAST_MAGIC 0x58544F41u

typedef struct xui_toast_item_t {
	int iId;
	int iType;
	char* sTitle;
	char* sMessage;
	float fDuration;
	float fElapsed;
	float fHeight;
	xui_rect_t tRect;
	xui_rect_t tIconRect;
	xui_rect_t tTitleRect;
	xui_rect_t tMessageRect;
	xui_rect_t tCloseRect;
	xui_toast_click_proc onClick;
	void* pUser;
	int bClosing;
	int iCloseReason;
	int iMessageLineCount;
} xui_toast_item_t;

typedef struct xui_toast_item_view_t {
	xui_toast pToast;
	int iSlot;
} xui_toast_item_view_t;

struct xui_toast_t {
	uint32_t iMagic;
	xui_context pContext;
	xui_font pFont;
	xui_widget arrWidgets[XUI_TOAST_VISIBLE_CAPACITY];
	xui_toast_item_t arrActive[XUI_TOAST_VISIBLE_CAPACITY];
	xui_toast_item_t arrPending[XUI_TOAST_QUEUE_CAPACITY];
	xui_toast_metrics_t tMetrics;
	xui_toast_colors_t tColors;
	xui_toast_close_proc onClose;
	void* pCloseUser;
	int iNextId;
	int iPlacement;
	int iDirection;
	int iActiveCount;
	int iPendingCount;
	int iHoverItem;
	int iHoverClose;
	int iShowCount;
	int iCloseCount;
	int iExpireCount;
	int iDropCount;
	int iChangeCount;
};

static int __xuiToastValid(xui_toast pToast)
{
	return (pToast != NULL) && (pToast->iMagic == XUI_TOAST_MAGIC);
}

static float __xuiToastMin(float fA, float fB)
{
	return (fA < fB) ? fA : fB;
}

static float __xuiToastMax(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static float __xuiToastClamp(float fValue, float fMin, float fMax)
{
	if ( fMax < fMin ) return fMin;
	if ( fValue < fMin ) return fMin;
	if ( fValue > fMax ) return fMax;
	return fValue;
}

static int __xuiToastFloatValid(float fValue)
{
	return (fValue == fValue) && (fValue >= 0.0f) && (fValue <= XUI_LAYOUT_UNBOUNDED);
}

static uint32_t __xuiToastColorWithAlpha(uint32_t iColor, uint32_t iAlpha)
{
	return (iColor & 0xFFFFFF00u) | (iAlpha & 0xFFu);
}

static uint32_t __xuiToastAlpha(uint32_t iColor)
{
	return iColor & 0xffu;
}

static int __xuiToastNormalizeType(int iType)
{
	if ( iType < XUI_TOAST_TYPE_INFO || iType > XUI_TOAST_TYPE_ERROR ) {
		return XUI_TOAST_TYPE_INFO;
	}
	return iType;
}

static int __xuiToastNormalizePlacement(int iPlacement)
{
	if ( iPlacement < XUI_TOAST_PLACEMENT_TOP_RIGHT || iPlacement > XUI_TOAST_PLACEMENT_BOTTOM_CENTER ) {
		return XUI_TOAST_PLACEMENT_TOP_RIGHT;
	}
	return iPlacement;
}

static int __xuiToastNormalizeDirection(int iDirection)
{
	if ( (iDirection != XUI_TOAST_DIRECTION_DOWN) && (iDirection != XUI_TOAST_DIRECTION_UP) ) {
		return XUI_TOAST_DIRECTION_AUTO;
	}
	return iDirection;
}

static int __xuiToastDirection(xui_toast pToast)
{
	if ( !__xuiToastValid(pToast) ) {
		return XUI_TOAST_DIRECTION_DOWN;
	}
	if ( pToast->iDirection == XUI_TOAST_DIRECTION_DOWN || pToast->iDirection == XUI_TOAST_DIRECTION_UP ) {
		return pToast->iDirection;
	}
	if ( pToast->iPlacement == XUI_TOAST_PLACEMENT_BOTTOM_LEFT ||
	     pToast->iPlacement == XUI_TOAST_PLACEMENT_BOTTOM_CENTER ||
	     pToast->iPlacement == XUI_TOAST_PLACEMENT_BOTTOM_RIGHT ) {
		return XUI_TOAST_DIRECTION_UP;
	}
	return XUI_TOAST_DIRECTION_DOWN;
}

static char* __xuiToastDupText(const char* sText)
{
	char* sCopy;
	size_t iLen;

	if ( sText == NULL ) {
		sText = "";
	}
	iLen = strlen(sText);
	sCopy = (char*)xrtMalloc(iLen + 1u);
	if ( sCopy == NULL ) {
		return NULL;
	}
	memcpy(sCopy, sText, iLen + 1u);
	return sCopy;
}

static void __xuiToastItemFree(xui_toast_item_t* pItem)
{
	if ( pItem == NULL ) {
		return;
	}
	if ( pItem->sTitle != NULL ) {
		xrtFree(pItem->sTitle);
	}
	if ( pItem->sMessage != NULL ) {
		xrtFree(pItem->sMessage);
	}
	memset(pItem, 0, sizeof(*pItem));
}

static void __xuiToastItemMove(xui_toast_item_t* pDst, xui_toast_item_t* pSrc)
{
	if ( (pDst == NULL) || (pSrc == NULL) ) {
		return;
	}
	*pDst = *pSrc;
	memset(pSrc, 0, sizeof(*pSrc));
}

static void __xuiToastDefaultMetrics(xui_toast_metrics_t* pMetrics)
{
	memset(pMetrics, 0, sizeof(*pMetrics));
	pMetrics->iSize = sizeof(*pMetrics);
	pMetrics->fWidth = 320.0f;
	pMetrics->fMinWidth = 180.0f;
	pMetrics->fMinHeight = 58.0f;
	pMetrics->fMargin = 18.0f;
	pMetrics->fGap = 8.0f;
	pMetrics->fPaddingX = 14.0f;
	pMetrics->fPaddingY = 10.0f;
	pMetrics->fIconSize = 28.0f;
	pMetrics->fIconGap = 10.0f;
	pMetrics->fCloseSize = 16.0f;
	pMetrics->fProgressHeight = 2.0f;
	pMetrics->iMaxVisible = 0;
}

static void __xuiToastDefaultColors(xui_toast_colors_t* pColors)
{
	memset(pColors, 0, sizeof(*pColors));
	pColors->iSize = sizeof(*pColors);
	pColors->iBackgroundColor = XUI_COLOR_RGBA(247, 252, 255, 246);
	pColors->iBorderColor = XUI_COLOR_RGBA(128, 174, 207, 210);
	pColors->iTextColor = XUI_COLOR_RGBA(31, 58, 82, 255);
	pColors->iMutedTextColor = XUI_COLOR_RGBA(92, 112, 130, 255);
	pColors->iShadowColor = XUI_COLOR_RGBA(6, 22, 42, 48);
	pColors->iInfoColor = XUI_COLOR_RGBA(78, 159, 220, 255);
	pColors->iSuccessColor = XUI_COLOR_RGBA(43, 184, 150, 255);
	pColors->iWarningColor = XUI_COLOR_RGBA(231, 156, 45, 255);
	pColors->iErrorColor = XUI_COLOR_RGBA(216, 76, 90, 255);
	pColors->iCloseColor = XUI_COLOR_RGBA(96, 126, 148, 255);
	pColors->iCloseHoverColor = XUI_COLOR_RGBA(31, 58, 82, 255);
}

static int __xuiToastMetricsValid(const xui_toast_metrics_t* pMetrics)
{
	if ( pMetrics == NULL ) return 0;
	if ( (pMetrics->iSize != 0) && (pMetrics->iSize < sizeof(*pMetrics)) ) return 0;
	return __xuiToastFloatValid(pMetrics->fWidth) &&
	       __xuiToastFloatValid(pMetrics->fMinWidth) &&
	       __xuiToastFloatValid(pMetrics->fMinHeight) &&
	       __xuiToastFloatValid(pMetrics->fMargin) &&
	       __xuiToastFloatValid(pMetrics->fGap) &&
	       __xuiToastFloatValid(pMetrics->fPaddingX) &&
	       __xuiToastFloatValid(pMetrics->fPaddingY) &&
	       __xuiToastFloatValid(pMetrics->fIconSize) &&
	       __xuiToastFloatValid(pMetrics->fIconGap) &&
	       __xuiToastFloatValid(pMetrics->fCloseSize) &&
	       __xuiToastFloatValid(pMetrics->fProgressHeight) &&
	       (pMetrics->iMaxVisible >= 0);
}

static int __xuiToastColorsValid(const xui_toast_colors_t* pColors)
{
	if ( pColors == NULL ) return 0;
	return (pColors->iSize == 0) || (pColors->iSize >= sizeof(*pColors));
}

static int __xuiToastDescValid(const xui_toast_desc_t* pDesc)
{
	if ( pDesc == NULL ) return 1;
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) return 0;
	if ( pDesc->bHasMetrics && !__xuiToastMetricsValid(&pDesc->tMetrics) ) return 0;
	if ( pDesc->bHasColors && !__xuiToastColorsValid(&pDesc->tColors) ) return 0;
	return 1;
}

static xui_font __xuiToastFont(xui_toast pToast)
{
	if ( pToast == NULL ) return NULL;
	return (pToast->pFont != NULL) ? pToast->pFont : xuiGetDefaultFont(pToast->pContext);
}

static uint32_t __xuiToastTypeColor(xui_toast pToast, int iType)
{
	if ( !__xuiToastValid(pToast) ) return 0;
	switch ( iType ) {
	case XUI_TOAST_TYPE_SUCCESS:
		return pToast->tColors.iSuccessColor;
	case XUI_TOAST_TYPE_WARNING:
		return pToast->tColors.iWarningColor;
	case XUI_TOAST_TYPE_ERROR:
		return pToast->tColors.iErrorColor;
	case XUI_TOAST_TYPE_INFO:
	default:
		return pToast->tColors.iInfoColor;
	}
}

static const char* __xuiToastIconName(int iType)
{
	switch ( iType ) {
	case XUI_TOAST_TYPE_SUCCESS:
		return "property_check_12";
	case XUI_TOAST_TYPE_WARNING:
		return "msgbox_war";
	case XUI_TOAST_TYPE_ERROR:
		return "msgbox_error";
	case XUI_TOAST_TYPE_INFO:
	default:
		return "msgbox_info";
	}
}

static xui_rect_t __xuiToastRootRect(xui_toast pToast)
{
	xui_widget pRoot;
	xui_vec2_t tViewport;
	xui_rect_t tRect;

	tRect = (xui_rect_t){0.0f, 0.0f, 640.0f, 420.0f};
	if ( !__xuiToastValid(pToast) ) {
		return tRect;
	}
	pRoot = xuiGetRootWidget(pToast->pContext);
	if ( pRoot != NULL ) {
		tRect = xuiWidgetGetRect(pRoot);
	}
	if ( (tRect.fW <= 1.0f) || (tRect.fH <= 1.0f) ) {
		tViewport = xuiGetViewportSize(pToast->pContext);
		if ( (tViewport.fX > 1.0f) && (tViewport.fY > 1.0f) ) {
			tRect = (xui_rect_t){0.0f, 0.0f, tViewport.fX, tViewport.fY};
		}
	}
	return tRect;
}

static int __xuiToastMeasureText(xui_toast pToast, const char* sText, float fWidth, xui_vec2_t* pSize, int* pLineCount)
{
	xui_text_layout_desc_t tDesc;
	xui_text_layout pLayout;
	xui_font pFont;
	int iRet;

	if ( !__xuiToastValid(pToast) || (pSize == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( sText == NULL ) sText = "";
	if ( fWidth < 1.0f ) fWidth = 1.0f;
	pSize->fX = 0.0f;
	pSize->fY = 0.0f;
	if ( pLineCount != NULL ) *pLineCount = 0;
	if ( sText[0] == 0 ) {
		return XUI_OK;
	}
	pFont = __xuiToastFont(pToast);
	if ( pFont == NULL ) {
		pSize->fX = __xuiToastMin((float)strlen(sText) * 7.0f, fWidth);
		pSize->fY = 18.0f;
		if ( pLineCount != NULL ) *pLineCount = 1;
		return XUI_OK;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sText = sText;
	tDesc.iTextSize = -1;
	tDesc.pFont = pFont;
	tDesc.fMaxWidth = fWidth;
	tDesc.fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	tDesc.iWrapMode = XUI_TEXT_WRAP_WORD;
	tDesc.iFlags = XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP;
	tDesc.fLineGap = 2.0f;
	pLayout = NULL;
	iRet = xuiTextLayoutCreate(pToast->pContext, &pLayout, &tDesc);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	*pSize = xuiTextLayoutGetSize(pLayout);
	if ( pLineCount != NULL ) {
		*pLineCount = xuiTextLayoutGetLineCount(pLayout);
	}
	xuiTextLayoutDestroy(pLayout);
	return XUI_OK;
}

static int __xuiToastVisibleLimit(xui_toast pToast)
{
	xui_rect_t tRoot;
	float fUsable;
	int iLimit;

	if ( !__xuiToastValid(pToast) ) {
		return 0;
	}
	if ( pToast->tMetrics.iMaxVisible > 0 ) {
		iLimit = pToast->tMetrics.iMaxVisible;
	} else {
		tRoot = __xuiToastRootRect(pToast);
		fUsable = tRoot.fH - pToast->tMetrics.fMargin * 2.0f + pToast->tMetrics.fGap;
		iLimit = (int)(fUsable / (pToast->tMetrics.fMinHeight + pToast->tMetrics.fGap));
	}
	if ( iLimit < 1 ) iLimit = 1;
	if ( iLimit > XUI_TOAST_VISIBLE_CAPACITY ) iLimit = XUI_TOAST_VISIBLE_CAPACITY;
	return iLimit;
}

static int __xuiToastInvalidateActive(xui_toast pToast)
{
	int i;
	int iRet;

	if ( !__xuiToastValid(pToast) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	for ( i = 0; i < XUI_TOAST_VISIBLE_CAPACITY; i++ ) {
		if ( pToast->arrWidgets[i] == NULL ) {
			continue;
		}
		iRet = xuiWidgetInvalidate(pToast->arrWidgets[i], XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return XUI_OK;
}

static int __xuiToastLayout(xui_toast pToast)
{
	xui_toast_item_t* pItem;
	xui_rect_t tRoot;
	xui_vec2_t tTitleSize;
	xui_vec2_t tMessageSize;
	float fWidth;
	float fTextX;
	float fCloseX;
	float fTextW;
	float fTitleH;
	float fMessageH;
	float fBodyH;
	float fStackH;
	float fX;
	float fY;
	float fContentY;
	int iDirection;
	int i;
	int iRet;

	if ( !__xuiToastValid(pToast) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	tRoot = __xuiToastRootRect(pToast);
	fWidth = pToast->tMetrics.fWidth;
	if ( tRoot.fW > 0.0f ) {
		fWidth = __xuiToastMin(fWidth, tRoot.fW - pToast->tMetrics.fMargin * 2.0f);
	}
	fWidth = __xuiToastMax(fWidth, pToast->tMetrics.fMinWidth);
	if ( tRoot.fW > 0.0f ) {
		fWidth = __xuiToastMin(fWidth, tRoot.fW);
	}
	if ( fWidth < 32.0f ) fWidth = 32.0f;
	fTextX = pToast->tMetrics.fPaddingX + pToast->tMetrics.fIconSize + pToast->tMetrics.fIconGap;
	fCloseX = fWidth - pToast->tMetrics.fPaddingX - pToast->tMetrics.fCloseSize;
	fTextW = fCloseX - fTextX - 8.0f;
	if ( fTextW < 48.0f ) fTextW = 48.0f;
	fStackH = 0.0f;
	for ( i = 0; i < pToast->iActiveCount; i++ ) {
		pItem = &pToast->arrActive[i];
		tTitleSize = (xui_vec2_t){0.0f, 0.0f};
		tMessageSize = (xui_vec2_t){0.0f, 0.0f};
		fTitleH = 0.0f;
		fMessageH = 0.0f;
		if ( (pItem->sTitle != NULL) && (pItem->sTitle[0] != 0) ) {
			iRet = __xuiToastMeasureText(pToast, pItem->sTitle, fTextW, &tTitleSize, NULL);
			if ( iRet != XUI_OK ) tTitleSize = (xui_vec2_t){fTextW, 18.0f};
			fTitleH = __xuiToastMax(tTitleSize.fY, 16.0f);
		}
		if ( (pItem->sMessage != NULL) && (pItem->sMessage[0] != 0) ) {
			iRet = __xuiToastMeasureText(pToast, pItem->sMessage, fTextW, &tMessageSize, &pItem->iMessageLineCount);
			if ( iRet != XUI_OK ) {
				tMessageSize = (xui_vec2_t){fTextW, 18.0f};
				pItem->iMessageLineCount = 1;
			}
			fMessageH = tMessageSize.fY;
		} else {
			pItem->iMessageLineCount = 0;
		}
		fBodyH = fTitleH + fMessageH;
		if ( (fTitleH > 0.0f) && (fMessageH > 0.0f) ) {
			fBodyH += 2.0f;
		}
		fBodyH = __xuiToastMax(fBodyH, pToast->tMetrics.fIconSize);
		pItem->fHeight = __xuiToastMax(pToast->tMetrics.fMinHeight, fBodyH + pToast->tMetrics.fPaddingY * 2.0f);
		pItem->tIconRect = xuiInternalSnapRect((xui_rect_t){
			pToast->tMetrics.fPaddingX,
			(pItem->fHeight - pToast->tMetrics.fIconSize) * 0.5f,
			pToast->tMetrics.fIconSize,
			pToast->tMetrics.fIconSize
		});
		pItem->tCloseRect = xuiInternalSnapRect((xui_rect_t){
			fWidth - pToast->tMetrics.fPaddingX - pToast->tMetrics.fCloseSize,
			pToast->tMetrics.fPaddingY - 1.0f,
			pToast->tMetrics.fCloseSize,
			pToast->tMetrics.fCloseSize
		});
		fContentY = (pItem->fHeight - fBodyH) * 0.5f;
		if ( fTitleH > 0.0f ) {
			pItem->tTitleRect = xuiInternalSnapRect((xui_rect_t){fTextX, fContentY, fTextW, fTitleH});
			if ( fMessageH > 0.0f ) {
				pItem->tMessageRect = xuiInternalSnapRect((xui_rect_t){fTextX, fContentY + fTitleH + 2.0f, fTextW, fMessageH});
			} else {
				pItem->tMessageRect = (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
			}
		} else {
			pItem->tTitleRect = (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
			pItem->tMessageRect = xuiInternalSnapRect((xui_rect_t){fTextX, (pItem->fHeight - fMessageH) * 0.5f, fTextW, fMessageH});
		}
		fStackH += pItem->fHeight;
		if ( i + 1 < pToast->iActiveCount ) fStackH += pToast->tMetrics.fGap;
	}
	if ( pToast->iPlacement == XUI_TOAST_PLACEMENT_TOP_LEFT || pToast->iPlacement == XUI_TOAST_PLACEMENT_BOTTOM_LEFT ) {
		fX = tRoot.fX + pToast->tMetrics.fMargin;
	} else if ( pToast->iPlacement == XUI_TOAST_PLACEMENT_TOP_CENTER || pToast->iPlacement == XUI_TOAST_PLACEMENT_BOTTOM_CENTER ) {
		fX = tRoot.fX + (tRoot.fW - fWidth) * 0.5f;
	} else {
		fX = tRoot.fX + tRoot.fW - pToast->tMetrics.fMargin - fWidth;
	}
	if ( fX < tRoot.fX ) fX = tRoot.fX;
	if ( fX + fWidth > tRoot.fX + tRoot.fW ) fX = tRoot.fX + tRoot.fW - fWidth;
	iDirection = __xuiToastDirection(pToast);
	if ( iDirection == XUI_TOAST_DIRECTION_UP ) {
		fY = tRoot.fY + tRoot.fH - pToast->tMetrics.fMargin - fStackH;
	} else {
		fY = tRoot.fY + pToast->tMetrics.fMargin;
	}
	if ( fY < tRoot.fY ) fY = tRoot.fY;
	for ( i = 0; i < pToast->iActiveCount; i++ ) {
		pItem = &pToast->arrActive[i];
		pItem->tRect = xuiInternalSnapRect((xui_rect_t){fX, fY, fWidth, pItem->fHeight});
		if ( pToast->arrWidgets[i] != NULL ) {
			iRet = xuiWidgetSetRect(pToast->arrWidgets[i], pItem->tRect);
			if ( iRet == XUI_OK ) iRet = xuiWidgetSetVisible(pToast->arrWidgets[i], 1);
			if ( iRet == XUI_OK ) iRet = xuiWidgetSetEnabled(pToast->arrWidgets[i], 1);
			if ( iRet == XUI_OK ) iRet = xuiWidgetSetHitTestVisible(pToast->arrWidgets[i], 1);
			if ( iRet == XUI_OK ) iRet = xuiWidgetSetLayer(pToast->arrWidgets[i], XUI_LAYER_TOOLTIP, 120 + i);
			if ( iRet != XUI_OK ) return iRet;
		}
		fY += pItem->fHeight + pToast->tMetrics.fGap;
	}
	for ( i = pToast->iActiveCount; i < XUI_TOAST_VISIBLE_CAPACITY; i++ ) {
		if ( pToast->arrWidgets[i] != NULL ) {
			iRet = xuiWidgetSetVisible(pToast->arrWidgets[i], 0);
			if ( iRet == XUI_OK ) iRet = xuiWidgetSetEnabled(pToast->arrWidgets[i], 0);
			if ( iRet == XUI_OK ) iRet = xuiWidgetSetHitTestVisible(pToast->arrWidgets[i], 0);
			if ( iRet == XUI_OK ) iRet = xuiWidgetSetRect(pToast->arrWidgets[i], (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f});
			if ( iRet != XUI_OK ) return iRet;
		}
	}
	return __xuiToastInvalidateActive(pToast);
}

static void __xuiToastNotifyClose(xui_toast pToast, int iToastId, int iReason)
{
	if ( __xuiToastValid(pToast) && (pToast->onClose != NULL) && (iToastId > 0) ) {
		pToast->onClose(pToast, iToastId, iReason, pToast->pCloseUser);
	}
}

static int __xuiToastFindActiveById(xui_toast pToast, int iToastId)
{
	int i;

	if ( !__xuiToastValid(pToast) || (iToastId <= 0) ) return -1;
	for ( i = 0; i < pToast->iActiveCount; i++ ) {
		if ( pToast->arrActive[i].iId == iToastId ) return i;
	}
	return -1;
}

static int __xuiToastFindPendingById(xui_toast pToast, int iToastId)
{
	int i;

	if ( !__xuiToastValid(pToast) || (iToastId <= 0) ) return -1;
	for ( i = 0; i < pToast->iPendingCount; i++ ) {
		if ( pToast->arrPending[i].iId == iToastId ) return i;
	}
	return -1;
}

static int __xuiToastDropPendingAt(xui_toast pToast, int iIndex)
{
	int iToastId;
	int i;

	if ( !__xuiToastValid(pToast) || (iIndex < 0) || (iIndex >= pToast->iPendingCount) ) return 0;
	iToastId = pToast->arrPending[iIndex].iId;
	__xuiToastItemFree(&pToast->arrPending[iIndex]);
	for ( i = iIndex; i + 1 < pToast->iPendingCount; i++ ) {
		__xuiToastItemMove(&pToast->arrPending[i], &pToast->arrPending[i + 1]);
	}
	pToast->iPendingCount--;
	pToast->iDropCount++;
	pToast->iChangeCount++;
	return iToastId;
}

static int __xuiToastActivatePending(xui_toast pToast)
{
	int iLimit;
	int i;

	if ( !__xuiToastValid(pToast) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iLimit = __xuiToastVisibleLimit(pToast);
	while ( (pToast->iActiveCount < iLimit) && (pToast->iPendingCount > 0) ) {
		__xuiToastItemMove(&pToast->arrActive[pToast->iActiveCount], &pToast->arrPending[0]);
		pToast->arrActive[pToast->iActiveCount].fElapsed = 0.0f;
		pToast->iActiveCount++;
		for ( i = 0; i + 1 < pToast->iPendingCount; i++ ) {
			__xuiToastItemMove(&pToast->arrPending[i], &pToast->arrPending[i + 1]);
		}
		pToast->iPendingCount--;
	}
	return __xuiToastLayout(pToast);
}

static int __xuiToastRemoveActiveAt(xui_toast pToast, int iIndex, int iReason)
{
	int iToastId;
	int i;
	int iRet;

	if ( !__xuiToastValid(pToast) || (iIndex < 0) || (iIndex >= pToast->iActiveCount) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iToastId = pToast->arrActive[iIndex].iId;
	__xuiToastItemFree(&pToast->arrActive[iIndex]);
	for ( i = iIndex; i + 1 < pToast->iActiveCount; i++ ) {
		__xuiToastItemMove(&pToast->arrActive[i], &pToast->arrActive[i + 1]);
	}
	pToast->iActiveCount--;
	pToast->iHoverItem = -1;
	pToast->iHoverClose = -1;
	if ( iReason == XUI_TOAST_CLOSE_TIMEOUT ) {
		pToast->iExpireCount++;
	} else {
		pToast->iCloseCount++;
	}
	pToast->iChangeCount++;
	iRet = __xuiToastActivatePending(pToast);
	__xuiToastNotifyClose(pToast, iToastId, iReason);
	return iRet;
}

static int __xuiToastRemovePendingAt(xui_toast pToast, int iIndex, int iReason)
{
	int iToastId;
	int i;
	int iRet;

	if ( !__xuiToastValid(pToast) || (iIndex < 0) || (iIndex >= pToast->iPendingCount) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iToastId = pToast->arrPending[iIndex].iId;
	__xuiToastItemFree(&pToast->arrPending[iIndex]);
	for ( i = iIndex; i + 1 < pToast->iPendingCount; i++ ) {
		__xuiToastItemMove(&pToast->arrPending[i], &pToast->arrPending[i + 1]);
	}
	pToast->iPendingCount--;
	pToast->iCloseCount++;
	pToast->iChangeCount++;
	iRet = __xuiToastLayout(pToast);
	__xuiToastNotifyClose(pToast, iToastId, iReason);
	return iRet;
}

static int __xuiToastRequestActiveClose(xui_toast pToast, int iIndex, int iReason)
{
	xui_toast_item_t* pItem;

	if ( !__xuiToastValid(pToast) || (iIndex < 0) || (iIndex >= pToast->iActiveCount) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pItem = &pToast->arrActive[iIndex];
	if ( pItem->bClosing ) {
		return XUI_OK;
	}
	pItem->bClosing = 1;
	pItem->iCloseReason = iReason;
	pToast->iHoverItem = -1;
	pToast->iHoverClose = -1;
	if ( pToast->arrWidgets[iIndex] != NULL ) {
		return xuiWidgetInvalidate(pToast->arrWidgets[iIndex], XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	}
	return XUI_OK;
}

static int __xuiToastProcessClosing(xui_toast pToast)
{
	xui_toast_click_proc onClick;
	void* pUser;
	int iId;
	int iReason;
	int iIndex;
	int i;
	int iRet;

	if ( !__xuiToastValid(pToast) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	i = 0;
	while ( i < pToast->iActiveCount ) {
		if ( !pToast->arrActive[i].bClosing ) {
			i++;
			continue;
		}
		iId = pToast->arrActive[i].iId;
		iReason = pToast->arrActive[i].iCloseReason;
		onClick = (iReason == XUI_TOAST_CLOSE_CLICK) ? pToast->arrActive[i].onClick : NULL;
		pUser = pToast->arrActive[i].pUser;
		if ( onClick != NULL ) {
			onClick(pToast, iId, pUser);
		}
		iIndex = __xuiToastFindActiveById(pToast, iId);
		if ( iIndex >= 0 ) {
			iRet = __xuiToastRemoveActiveAt(pToast, iIndex, iReason);
			if ( iRet != XUI_OK ) return iRet;
			i = 0;
			continue;
		}
		i = 0;
	}
	return XUI_OK;
}

static int __xuiToastUpdateService(xui_toast pToast, float fDelta)
{
	int i;
	int iRet;

	if ( !__xuiToastValid(pToast) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiToastProcessClosing(pToast);
	if ( iRet != XUI_OK ) return iRet;
	if ( fDelta > 0.0f ) {
		i = 0;
		while ( i < pToast->iActiveCount ) {
			if ( pToast->arrActive[i].bClosing ) {
				i++;
				continue;
			}
			pToast->arrActive[i].fElapsed += fDelta;
			if ( pToast->arrWidgets[i] != NULL ) {
				(void)xuiWidgetInvalidate(pToast->arrWidgets[i], XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
			}
			if ( pToast->arrActive[i].fElapsed >= pToast->arrActive[i].fDuration ) {
				iRet = __xuiToastRemoveActiveAt(pToast, i, XUI_TOAST_CLOSE_TIMEOUT);
				if ( iRet != XUI_OK ) return iRet;
				continue;
			}
			i++;
		}
	}
	return __xuiToastActivatePending(pToast);
}

static int __xuiToastDrawTextLayout(xui_toast pToast, xui_toast_item_t* pItem, xui_draw_context pDraw)
{
	xui_text_layout_desc_t tDesc;
	xui_text_layout pLayout;
	xui_text_line_t tLine;
	xui_proxy pProxy;
	xui_font pFont;
	const char* sText;
	char* sLine;
	xui_rect_t tLineRect;
	int i;
	int iRet;

	if ( !__xuiToastValid(pToast) || (pItem == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pItem->sMessage == NULL) || (pItem->sMessage[0] == 0) || (pItem->tMessageRect.fW <= 0.0f) ) {
		return XUI_OK;
	}
	pProxy = xuiInternalContextGetProxy(pToast->pContext);
	pFont = __xuiToastFont(pToast);
	if ( (pProxy == NULL) || (pProxy->drawText == NULL) || (pFont == NULL) ) {
		return XUI_OK;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sText = pItem->sMessage;
	tDesc.iTextSize = -1;
	tDesc.pFont = pFont;
	tDesc.fMaxWidth = pItem->tMessageRect.fW;
	tDesc.fMaxHeight = pItem->tMessageRect.fH;
	tDesc.iWrapMode = XUI_TEXT_WRAP_WORD;
	tDesc.iFlags = XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP | XUI_TEXT_CLIP;
	tDesc.fLineGap = 2.0f;
	pLayout = NULL;
	iRet = xuiTextLayoutCreate(pToast->pContext, &pLayout, &tDesc);
	if ( iRet != XUI_OK ) {
		return XUI_OK;
	}
	sText = xuiTextLayoutGetText(pLayout);
	for ( i = 0; i < xuiTextLayoutGetLineCount(pLayout); i++ ) {
		memset(&tLine, 0, sizeof(tLine));
		tLine.iSize = sizeof(tLine);
		if ( xuiTextLayoutGetLine(pLayout, i, &tLine) != XUI_OK || tLine.iTextSize <= 0 ) {
			continue;
		}
		sLine = (char*)xrtMalloc((size_t)tLine.iTextSize + 1u);
		if ( sLine == NULL ) {
			xuiTextLayoutDestroy(pLayout);
			return XUI_ERROR_OUT_OF_MEMORY;
		}
		memcpy(sLine, sText + tLine.iTextOffset, (size_t)tLine.iTextSize);
		sLine[tLine.iTextSize] = 0;
		tLineRect.fX = pItem->tMessageRect.fX + tLine.fX;
		tLineRect.fY = pItem->tMessageRect.fY + tLine.fY;
		tLineRect.fW = pItem->tMessageRect.fW;
		tLineRect.fH = tLine.fH;
		(void)pProxy->drawText(pProxy, pDraw, pFont, sLine, xuiInternalSnapRect(tLineRect), pToast->tColors.iMutedTextColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP | XUI_TEXT_CLIP);
		xrtFree(sLine);
	}
	xuiTextLayoutDestroy(pLayout);
	return XUI_OK;
}

static int __xuiToastDrawAtlas(xui_toast pToast, xui_draw_context pDraw, const char* sName, xui_rect_t tDst, uint32_t iColor)
{
	xui_proxy pProxy;
	xui_surface pAtlas;
	xui_rect_t tSrc;
	int iRet;

	if ( !__xuiToastValid(pToast) || (pDraw == NULL) || (sName == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(pToast->pContext);
	if ( (pProxy == NULL) || (pProxy->drawSurface == NULL) ) {
		return XUI_ERROR_UNSUPPORTED;
	}
	pAtlas = NULL;
	iRet = xuiBuiltinAssetGetAtlas(pToast->pContext, &pAtlas);
	if ( iRet == XUI_OK ) {
		iRet = xuiBuiltinAssetGetRect(sName, &tSrc);
	}
	if ( (iRet == XUI_OK) && (pAtlas != NULL) ) {
		return pProxy->drawSurface(pProxy, pDraw, pAtlas, tSrc, xuiInternalSnapRect(tDst), iColor, 0);
	}
	return iRet;
}

static int __xuiToastRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_toast_item_view_t* pView;
	xui_toast pToast;
	xui_toast_item_t* pItem;
	xui_proxy pProxy;
	xui_rect_t tRect;
	xui_rect_t tShadow;
	xui_rect_t tBand;
	xui_rect_t tIconBg;
	xui_rect_t tIcon;
	xui_rect_t tClose;
	xui_rect_t tProgress;
	uint32_t iTypeColor;
	uint32_t iCloseColor;
	float fIconInset;
	float fProgress;

	(void)iStateId;
	pView = (xui_toast_item_view_t*)pUser;
	pToast = (pView != NULL) ? pView->pToast : NULL;
	if ( !__xuiToastValid(pToast) || (pView->iSlot < 0) || (pView->iSlot >= pToast->iActiveCount) || (pDraw == NULL) ) {
		return XUI_OK;
	}
	pItem = &pToast->arrActive[pView->iSlot];
	pProxy = xuiInternalContextGetProxy(pToast->pContext);
	if ( pProxy == NULL ) {
		return XUI_OK;
	}
	tRect = xuiInternalSnapRect((xui_rect_t){0.0f, 0.0f, pItem->tRect.fW, pItem->tRect.fH});
	iTypeColor = __xuiToastTypeColor(pToast, pItem->iType);
	if ( (__xuiToastAlpha(pToast->tColors.iShadowColor) != 0) && (pProxy->drawRectFill != NULL) ) {
		tShadow = xuiInternalSnapRect((xui_rect_t){0.0f, 2.0f, tRect.fW, tRect.fH});
		(void)pProxy->drawRectFill(pProxy, pDraw, tShadow, pToast->tColors.iShadowColor);
	}
	if ( pProxy->drawRectFill != NULL ) {
		(void)pProxy->drawRectFill(pProxy, pDraw, tRect, pToast->tColors.iBackgroundColor);
	}
	if ( (__xuiToastAlpha(pToast->tColors.iBorderColor) != 0) && (pProxy->drawRectStroke != NULL) ) {
		(void)pProxy->drawRectStroke(pProxy, pDraw, tRect, 1.0f, pToast->tColors.iBorderColor);
	}
	if ( pProxy->drawRectFill != NULL ) {
		tBand = xuiInternalSnapRect((xui_rect_t){0.0f, 0.0f, 4.0f, tRect.fH});
		(void)pProxy->drawRectFill(pProxy, pDraw, tBand, iTypeColor);
	}
	tIconBg = xuiInternalSnapRect(pItem->tIconRect);
	if ( pProxy->drawRectFill != NULL ) {
		(void)pProxy->drawRectFill(pProxy, pDraw, tIconBg, __xuiToastColorWithAlpha(iTypeColor, 32));
	}
	fIconInset = __xuiToastClamp(pToast->tMetrics.fIconSize * 0.1f, 2.0f, 3.0f);
	tIcon = xuiInternalInsetRect(pItem->tIconRect, fIconInset);
	if ( __xuiToastDrawAtlas(pToast, pDraw, __xuiToastIconName(pItem->iType), tIcon, iTypeColor) != XUI_OK ) {
		if ( pProxy->drawCircleFill != NULL ) {
			(void)pProxy->drawCircleFill(pProxy, pDraw, pItem->tIconRect.fX + pItem->tIconRect.fW * 0.5f, pItem->tIconRect.fY + pItem->tIconRect.fH * 0.5f, pItem->tIconRect.fW * 0.28f, iTypeColor);
		}
	}
	if ( (pProxy->drawText != NULL) && (__xuiToastFont(pToast) != NULL) ) {
		if ( (pItem->sTitle != NULL) && (pItem->sTitle[0] != 0) ) {
			(void)pProxy->drawText(pProxy, pDraw, __xuiToastFont(pToast), pItem->sTitle, pItem->tTitleRect, pToast->tColors.iTextColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
		}
		(void)__xuiToastDrawTextLayout(pToast, pItem, pDraw);
	}
	iCloseColor = (pView->iSlot == pToast->iHoverClose) ? pToast->tColors.iCloseHoverColor : pToast->tColors.iCloseColor;
	tClose = xuiInternalInsetRect(pItem->tCloseRect, (pItem->tCloseRect.fW - 10.0f) * 0.5f);
	if ( __xuiToastDrawAtlas(pToast, pDraw, "clear_10", tClose, iCloseColor) != XUI_OK && pProxy->drawLine != NULL ) {
		(void)pProxy->drawLine(pProxy, pDraw, tClose.fX, tClose.fY, tClose.fX + tClose.fW, tClose.fY + tClose.fH, 1.0f, iCloseColor);
		(void)pProxy->drawLine(pProxy, pDraw, tClose.fX + tClose.fW, tClose.fY, tClose.fX, tClose.fY + tClose.fH, 1.0f, iCloseColor);
	}
	if ( (pProxy->drawRectFill != NULL) && (pItem->fDuration > 0.0f) ) {
		fProgress = 1.0f - (pItem->fElapsed / pItem->fDuration);
		if ( fProgress < 0.0f ) fProgress = 0.0f;
		if ( fProgress > 1.0f ) fProgress = 1.0f;
		tProgress = xuiInternalSnapRect((xui_rect_t){4.0f, tRect.fH - pToast->tMetrics.fProgressHeight, (tRect.fW - 4.0f) * fProgress, pToast->tMetrics.fProgressHeight});
		(void)pProxy->drawRectFill(pProxy, pDraw, tProgress, __xuiToastColorWithAlpha(iTypeColor, 96));
	}
	(void)pWidget;
	return XUI_OK;
}

static int __xuiToastUpdate(xui_widget pWidget, float fDelta, void* pUser)
{
	xui_toast_item_view_t* pView;

	(void)pWidget;
	pView = (xui_toast_item_view_t*)pUser;
	if ( (pView == NULL) || (pView->iSlot != 0) ) {
		return XUI_OK;
	}
	return __xuiToastUpdateService(pView->pToast, fDelta);
}

static int __xuiToastPointInRect(xui_rect_t tRect, float fX, float fY)
{
	return (fX >= tRect.fX) && (fY >= tRect.fY) && (fX < tRect.fX + tRect.fW) && (fY < tRect.fY + tRect.fH);
}

static int __xuiToastEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_toast_item_view_t* pView;
	xui_toast pToast;
	xui_toast_item_t* pItem;
	xui_rect_t tWorld;
	float fLocalX;
	float fLocalY;
	int bClose;
	int iRet;

	pView = (xui_toast_item_view_t*)pUser;
	pToast = (pView != NULL) ? pView->pToast : NULL;
	if ( !__xuiToastValid(pToast) || (pEvent == NULL) || (pView->iSlot < 0) || (pView->iSlot >= pToast->iActiveCount) ) {
		return XUI_OK;
	}
	pItem = &pToast->arrActive[pView->iSlot];
	tWorld = xuiWidgetGetWorldRect(pWidget);
	fLocalX = pEvent->fX - tWorld.fX;
	fLocalY = pEvent->fY - tWorld.fY;
	switch ( pEvent->iType ) {
	case XUI_EVENT_POINTER_ENTER:
	case XUI_EVENT_POINTER_MOVE:
		bClose = __xuiToastPointInRect(pItem->tCloseRect, fLocalX, fLocalY);
		if ( (pToast->iHoverItem != pView->iSlot) || (pToast->iHoverClose != (bClose ? pView->iSlot : -1)) ) {
			pToast->iHoverItem = pView->iSlot;
			pToast->iHoverClose = bClose ? pView->iSlot : -1;
			(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		}
		return XUI_EVENT_DISPATCH_STOP;
	case XUI_EVENT_POINTER_LEAVE:
	case XUI_EVENT_POINTER_CAPTURE_LOST:
		if ( pToast->iHoverItem == pView->iSlot || pToast->iHoverClose == pView->iSlot ) {
			pToast->iHoverItem = -1;
			pToast->iHoverClose = -1;
			(void)xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
		}
		return XUI_OK;
	case XUI_EVENT_POINTER_DOWN:
		if ( pEvent->iButton != XUI_POINTER_BUTTON_LEFT || pItem->bClosing ) {
			return XUI_EVENT_DISPATCH_STOP;
		}
		bClose = __xuiToastPointInRect(pItem->tCloseRect, fLocalX, fLocalY);
		iRet = __xuiToastRequestActiveClose(pToast, pView->iSlot, bClose ? XUI_TOAST_CLOSE_BUTTON : XUI_TOAST_CLOSE_CLICK);
		if ( iRet < 0 ) return iRet;
		return XUI_EVENT_DISPATCH_STOP;
	default:
		break;
	}
	return XUI_OK;
}

static int __xuiToastTypeInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_cache_policy_t tPolicy;
	xui_toast_item_view_t* pView;
	const xui_toast_item_view_t* pCreateView;
	int iRet;

	(void)pUser;
	if ( (pWidget == NULL) || (pTypeData == NULL) || (pCreateData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pView = (xui_toast_item_view_t*)pTypeData;
	pCreateView = (const xui_toast_item_view_t*)pCreateData;
	*pView = *pCreateView;
	memset(&tPolicy, 0, sizeof(tPolicy));
	tPolicy.iSize = sizeof(tPolicy);
	tPolicy.iPolicy = XUI_CACHE_POLICY_SELF;
	tPolicy.iFlags = XUI_CACHE_CLEAR_ON_UPDATE | XUI_CACHE_UPDATE_ALL_STATES;
	tPolicy.iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
	(void)xuiWidgetSetLayoutType(pWidget, XUI_LAYOUT_MANUAL);
	(void)xuiWidgetSetFlowMode(pWidget, XUI_FLOW_ABSOLUTE);
	(void)xuiWidgetSetOverflow(pWidget, XUI_OVERFLOW_VISIBLE);
	(void)xuiWidgetSetFocusable(pWidget, 0);
	(void)xuiWidgetSetTabStop(pWidget, 0);
	(void)xuiWidgetSetVisible(pWidget, 0);
	(void)xuiWidgetSetEnabled(pWidget, 0);
	(void)xuiWidgetSetHitTestVisible(pWidget, 0);
	(void)xuiWidgetSetLayer(pWidget, XUI_LAYER_TOOLTIP, 120 + pView->iSlot);
	(void)xuiWidgetSetCachePolicy(pWidget, &tPolicy);
	iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_ENTER, __xuiToastEvent, pTypeData);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_LEAVE, __xuiToastEvent, pTypeData);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_MOVE, __xuiToastEvent, pTypeData);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiToastEvent, pTypeData);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_CAPTURE_LOST, __xuiToastEvent, pTypeData);
	return iRet;
}

static void __xuiToastTypeDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_toast_item_view_t* pView;

	(void)pWidget;
	(void)pUser;
	pView = (xui_toast_item_view_t*)pTypeData;
	if ( pView != NULL ) {
		pView->pToast = NULL;
		pView->iSlot = -1;
	}
}

static xui_widget_type __xuiToastEnsureType(xui_context pContext)
{
	xui_widget_type pType;
	xui_widget_type_desc_t tDesc;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "toast-item");
	if ( pType != NULL ) {
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "toast-item";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iTypeDataSize = sizeof(xui_toast_item_view_t);
	tDesc.onInit = __xuiToastTypeInit;
	tDesc.onDestroy = __xuiToastTypeDestroy;
	tDesc.onCacheRender = __xuiToastRender;
	tDesc.onUpdate = __xuiToastUpdate;
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet == XUI_ERROR_ALREADY_INITIALIZED ) {
		return xuiWidgetFindType(pContext, "toast-item");
	}
	return (iRet == XUI_OK) ? pType : NULL;
}

static int __xuiToastCreateWidgets(xui_toast pToast)
{
	xui_widget_type pType;
	xui_toast_item_view_t tView;
	int i;
	int iRet;

	if ( !__xuiToastValid(pToast) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pType = __xuiToastEnsureType(pToast->pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	for ( i = 0; i < XUI_TOAST_VISIBLE_CAPACITY; i++ ) {
		memset(&tView, 0, sizeof(tView));
		tView.pToast = pToast;
		tView.iSlot = i;
		iRet = xuiWidgetCreateTyped(pToast->pContext, pType, &pToast->arrWidgets[i], &tView);
		if ( iRet != XUI_OK ) return iRet;
		iRet = xuiOverlayAttach(pToast->pContext, NULL, pToast->arrWidgets[i], XUI_LAYER_TOOLTIP, 120 + i);
		if ( iRet != XUI_OK ) return iRet;
	}
	return XUI_OK;
}

XUI_API int xuiToastCreate(xui_context pContext, xui_toast* ppToast, const xui_toast_desc_t* pDesc)
{
	xui_toast pToast;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) || (ppToast == NULL) || !__xuiToastDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppToast = NULL;
	pToast = (xui_toast)xrtCalloc(1, sizeof(*pToast));
	if ( pToast == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pToast->iMagic = XUI_TOAST_MAGIC;
	pToast->pContext = pContext;
	pToast->pFont = (pDesc != NULL && pDesc->pFont != NULL) ? pDesc->pFont : xuiGetDefaultFont(pContext);
	pToast->iNextId = 1;
	pToast->iPlacement = (pDesc != NULL) ? __xuiToastNormalizePlacement(pDesc->iPlacement) : XUI_TOAST_PLACEMENT_TOP_RIGHT;
	pToast->iDirection = (pDesc != NULL) ? __xuiToastNormalizeDirection(pDesc->iDirection) : XUI_TOAST_DIRECTION_AUTO;
	pToast->iHoverItem = -1;
	pToast->iHoverClose = -1;
	__xuiToastDefaultMetrics(&pToast->tMetrics);
	__xuiToastDefaultColors(&pToast->tColors);
	if ( pDesc != NULL && pDesc->bHasMetrics ) pToast->tMetrics = pDesc->tMetrics;
	if ( pDesc != NULL && pDesc->bHasColors ) pToast->tColors = pDesc->tColors;
	pToast->tMetrics.iSize = sizeof(pToast->tMetrics);
	pToast->tColors.iSize = sizeof(pToast->tColors);
	if ( pToast->tMetrics.fMinWidth < 32.0f ) pToast->tMetrics.fMinWidth = 32.0f;
	if ( pToast->tMetrics.fWidth < pToast->tMetrics.fMinWidth ) pToast->tMetrics.fWidth = pToast->tMetrics.fMinWidth;
	if ( pToast->tMetrics.fMinHeight < 24.0f ) pToast->tMetrics.fMinHeight = 24.0f;
	iRet = __xuiToastCreateWidgets(pToast);
	if ( iRet != XUI_OK ) {
		xuiToastDestroy(pToast);
		return iRet;
	}
	*ppToast = pToast;
	return XUI_OK;
}

XUI_API void xuiToastDestroy(xui_toast pToast)
{
	int i;

	if ( !__xuiToastValid(pToast) ) {
		return;
	}
	for ( i = 0; i < XUI_TOAST_VISIBLE_CAPACITY; i++ ) {
		if ( pToast->arrWidgets[i] != NULL ) {
			xuiWidgetDestroy(pToast->arrWidgets[i]);
			pToast->arrWidgets[i] = NULL;
		}
		__xuiToastItemFree(&pToast->arrActive[i]);
	}
	for ( i = 0; i < XUI_TOAST_QUEUE_CAPACITY; i++ ) {
		__xuiToastItemFree(&pToast->arrPending[i]);
	}
	pToast->iMagic = 0;
	xrtFree(pToast);
}

XUI_API int xuiToastShow(xui_toast pToast, int iType, const char* sTitle, const char* sMessage, float fDuration, xui_toast_click_proc onClick, void* pUser)
{
	xui_toast_item_t tItem;
	int iId;
	int iDroppedId;
	int iRet;

	if ( !__xuiToastValid(pToast) || (fDuration != fDuration) ) {
		return -1;
	}
	memset(&tItem, 0, sizeof(tItem));
	tItem.sTitle = __xuiToastDupText(sTitle);
	tItem.sMessage = __xuiToastDupText(sMessage);
	if ( (tItem.sTitle == NULL) || (tItem.sMessage == NULL) ) {
		__xuiToastItemFree(&tItem);
		return -1;
	}
	iId = pToast->iNextId++;
	if ( pToast->iNextId <= 0 ) pToast->iNextId = 1;
	tItem.iId = iId;
	tItem.iType = __xuiToastNormalizeType(iType);
	tItem.fDuration = (fDuration <= 0.0f) ? 3.0f : fDuration;
	tItem.onClick = onClick;
	tItem.pUser = pUser;
	iDroppedId = 0;
	if ( pToast->iPendingCount >= XUI_TOAST_QUEUE_CAPACITY ) {
		iDroppedId = __xuiToastDropPendingAt(pToast, 0);
	}
	__xuiToastItemMove(&pToast->arrPending[pToast->iPendingCount], &tItem);
	pToast->iPendingCount++;
	pToast->iShowCount++;
	pToast->iChangeCount++;
	iRet = __xuiToastActivatePending(pToast);
	if ( iDroppedId > 0 ) {
		__xuiToastNotifyClose(pToast, iDroppedId, XUI_TOAST_CLOSE_CAPACITY_DROP);
	}
	return (iRet == XUI_OK) ? iId : -1;
}

XUI_API int xuiToastClose(xui_toast pToast, int iToastId)
{
	int iIndex;

	if ( !__xuiToastValid(pToast) || (iToastId <= 0) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iIndex = __xuiToastFindActiveById(pToast, iToastId);
	if ( iIndex >= 0 ) {
		return __xuiToastRemoveActiveAt(pToast, iIndex, XUI_TOAST_CLOSE_API);
	}
	iIndex = __xuiToastFindPendingById(pToast, iToastId);
	if ( iIndex >= 0 ) {
		return __xuiToastRemovePendingAt(pToast, iIndex, XUI_TOAST_CLOSE_API);
	}
	return XUI_ERROR;
}

XUI_API int xuiToastClear(xui_toast pToast)
{
	int arrIds[XUI_TOAST_VISIBLE_CAPACITY + XUI_TOAST_QUEUE_CAPACITY];
	int iCount;
	int i;
	int iRet;

	if ( !__xuiToastValid(pToast) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iCount = 0;
	for ( i = 0; i < pToast->iActiveCount; i++ ) {
		arrIds[iCount++] = pToast->arrActive[i].iId;
		__xuiToastItemFree(&pToast->arrActive[i]);
	}
	for ( i = 0; i < pToast->iPendingCount; i++ ) {
		arrIds[iCount++] = pToast->arrPending[i].iId;
		__xuiToastItemFree(&pToast->arrPending[i]);
	}
	pToast->iActiveCount = 0;
	pToast->iPendingCount = 0;
	pToast->iHoverItem = -1;
	pToast->iHoverClose = -1;
	pToast->iCloseCount += iCount;
	pToast->iChangeCount++;
	iRet = __xuiToastLayout(pToast);
	for ( i = 0; i < iCount; i++ ) {
		__xuiToastNotifyClose(pToast, arrIds[i], XUI_TOAST_CLOSE_CLEAR);
	}
	return iRet;
}

XUI_API int xuiToastGetActiveCount(xui_toast pToast)
{
	return __xuiToastValid(pToast) ? pToast->iActiveCount : 0;
}

XUI_API int xuiToastGetPendingCount(xui_toast pToast)
{
	return __xuiToastValid(pToast) ? pToast->iPendingCount : 0;
}

XUI_API int xuiToastSetPlacement(xui_toast pToast, int iPlacement)
{
	if ( !__xuiToastValid(pToast) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pToast->iPlacement = __xuiToastNormalizePlacement(iPlacement);
	pToast->iChangeCount++;
	return __xuiToastLayout(pToast);
}

XUI_API int xuiToastGetPlacement(xui_toast pToast)
{
	return __xuiToastValid(pToast) ? pToast->iPlacement : XUI_TOAST_PLACEMENT_TOP_RIGHT;
}

XUI_API int xuiToastSetDirection(xui_toast pToast, int iDirection)
{
	if ( !__xuiToastValid(pToast) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pToast->iDirection = __xuiToastNormalizeDirection(iDirection);
	pToast->iChangeCount++;
	return __xuiToastLayout(pToast);
}

XUI_API int xuiToastGetDirection(xui_toast pToast)
{
	return __xuiToastValid(pToast) ? pToast->iDirection : XUI_TOAST_DIRECTION_AUTO;
}

XUI_API int xuiToastSetMetrics(xui_toast pToast, const xui_toast_metrics_t* pMetrics)
{
	if ( !__xuiToastValid(pToast) || !__xuiToastMetricsValid(pMetrics) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pToast->tMetrics = *pMetrics;
	pToast->tMetrics.iSize = sizeof(pToast->tMetrics);
	if ( pToast->tMetrics.fMinWidth < 32.0f ) pToast->tMetrics.fMinWidth = 32.0f;
	if ( pToast->tMetrics.fWidth < pToast->tMetrics.fMinWidth ) pToast->tMetrics.fWidth = pToast->tMetrics.fMinWidth;
	if ( pToast->tMetrics.fMinHeight < 24.0f ) pToast->tMetrics.fMinHeight = 24.0f;
	if ( pToast->tMetrics.iMaxVisible > XUI_TOAST_VISIBLE_CAPACITY ) pToast->tMetrics.iMaxVisible = XUI_TOAST_VISIBLE_CAPACITY;
	pToast->iChangeCount++;
	return __xuiToastActivatePending(pToast);
}

XUI_API int xuiToastGetMetrics(xui_toast pToast, xui_toast_metrics_t* pMetrics)
{
	if ( !__xuiToastValid(pToast) || (pMetrics == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*pMetrics = pToast->tMetrics;
	pMetrics->iSize = sizeof(*pMetrics);
	return XUI_OK;
}

XUI_API int xuiToastSetColors(xui_toast pToast, const xui_toast_colors_t* pColors)
{
	if ( !__xuiToastValid(pToast) || !__xuiToastColorsValid(pColors) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pToast->tColors = *pColors;
	pToast->tColors.iSize = sizeof(pToast->tColors);
	pToast->iChangeCount++;
	return __xuiToastInvalidateActive(pToast);
}

XUI_API int xuiToastGetColors(xui_toast pToast, xui_toast_colors_t* pColors)
{
	if ( !__xuiToastValid(pToast) || (pColors == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*pColors = pToast->tColors;
	pColors->iSize = sizeof(*pColors);
	return XUI_OK;
}

XUI_API int xuiToastSetFont(xui_toast pToast, xui_font pFont)
{
	if ( !__xuiToastValid(pToast) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pToast->pFont = pFont;
	pToast->iChangeCount++;
	return __xuiToastLayout(pToast);
}

XUI_API xui_font xuiToastGetFont(xui_toast pToast)
{
	return __xuiToastValid(pToast) ? pToast->pFont : NULL;
}

XUI_API int xuiToastSetClose(xui_toast pToast, xui_toast_close_proc onClose, void* pUser)
{
	if ( !__xuiToastValid(pToast) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pToast->onClose = onClose;
	pToast->pCloseUser = pUser;
	return XUI_OK;
}

XUI_API xui_widget xuiToastGetItemWidget(xui_toast pToast, int iSlot)
{
	if ( !__xuiToastValid(pToast) || (iSlot < 0) || (iSlot >= XUI_TOAST_VISIBLE_CAPACITY) ) {
		return NULL;
	}
	return pToast->arrWidgets[iSlot];
}

XUI_API xui_rect_t xuiToastGetItemRect(xui_toast pToast, int iSlot)
{
	if ( !__xuiToastValid(pToast) || (iSlot < 0) || (iSlot >= pToast->iActiveCount) ) {
		return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	}
	return pToast->arrActive[iSlot].tRect;
}

XUI_API xui_rect_t xuiToastGetIconRect(xui_toast pToast, int iSlot)
{
	xui_rect_t tRect;

	if ( !__xuiToastValid(pToast) || (iSlot < 0) || (iSlot >= pToast->iActiveCount) ) {
		return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	}
	tRect = pToast->arrActive[iSlot].tIconRect;
	tRect.fX += pToast->arrActive[iSlot].tRect.fX;
	tRect.fY += pToast->arrActive[iSlot].tRect.fY;
	return tRect;
}

XUI_API xui_rect_t xuiToastGetCloseRect(xui_toast pToast, int iSlot)
{
	xui_rect_t tRect;

	if ( !__xuiToastValid(pToast) || (iSlot < 0) || (iSlot >= pToast->iActiveCount) ) {
		return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	}
	tRect = pToast->arrActive[iSlot].tCloseRect;
	tRect.fX += pToast->arrActive[iSlot].tRect.fX;
	tRect.fY += pToast->arrActive[iSlot].tRect.fY;
	return tRect;
}

XUI_API int xuiToastGetShowCount(xui_toast pToast)
{
	return __xuiToastValid(pToast) ? pToast->iShowCount : 0;
}

XUI_API int xuiToastGetCloseCount(xui_toast pToast)
{
	return __xuiToastValid(pToast) ? pToast->iCloseCount : 0;
}

XUI_API int xuiToastGetExpireCount(xui_toast pToast)
{
	return __xuiToastValid(pToast) ? pToast->iExpireCount : 0;
}

XUI_API int xuiToastGetDropCount(xui_toast pToast)
{
	return __xuiToastValid(pToast) ? pToast->iDropCount : 0;
}

XUI_API int xuiToastGetChangeCount(xui_toast pToast)
{
	return __xuiToastValid(pToast) ? pToast->iChangeCount : 0;
}
