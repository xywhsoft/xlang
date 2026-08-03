#include "xui_internal.h"

#include <string.h>

#define XUI_MSGTIP_MAGIC 0x584D5354u

struct xui_msgtip_t {
	uint32_t iMagic;
	xui_context pContext;
	xui_widget pWidget;
	char* sText;
	xui_font pFont;
	xui_surface pIconSurface;
	xui_rect_t tIconSrc;
	xui_rect_t tTipRect;
	xui_rect_t tIconRect;
	xui_rect_t tTextRect;
	xui_msgtip_metrics_t tMetrics;
	xui_msgtip_colors_t tColors;
	xui_msgtip_close_proc onClose;
	void* pCloseUser;
	float fDuration;
	float fElapsed;
	int iType;
	int iWrapLineCount;
	int iShowCount;
	int iCloseCount;
	int iExpireCount;
	int iChangeCount;
	int bOpen;
	int bCustomIcon;
};

static int __xuiMsgTipValid(xui_msgtip pTip)
{
	return (pTip != NULL) && (pTip->iMagic == XUI_MSGTIP_MAGIC);
}

static float __xuiMsgTipMin(float fA, float fB)
{
	return (fA < fB) ? fA : fB;
}

static float __xuiMsgTipMax(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static float __xuiMsgTipPixelCeil(float fValue)
{
	if ( fValue <= 0.0f ) {
		return 0.0f;
	}
	return (float)xuiInternalPixelCeil(fValue);
}

static float __xuiMsgTipClamp(float fValue, float fMin, float fMax)
{
	if ( fMax < fMin ) {
		return fMin;
	}
	if ( fValue < fMin ) return fMin;
	if ( fValue > fMax ) return fMax;
	return fValue;
}

static int __xuiMsgTipFloatValid(float fValue)
{
	return (fValue == fValue) && (fValue >= 0.0f) && (fValue <= XUI_LAYOUT_UNBOUNDED);
}

static int __xuiMsgTipSignedFloatValid(float fValue)
{
	return (fValue == fValue) && (fValue >= -XUI_CONTEXT_MAX_VIEWPORT) && (fValue <= XUI_CONTEXT_MAX_VIEWPORT);
}

static int __xuiMsgTipNormalizeType(int iType)
{
	if ( iType < XUI_MSGTIP_ICON_NONE || iType > XUI_MSGTIP_ICON_ERROR ) {
		return XUI_MSGTIP_ICON_INFO;
	}
	return iType;
}

static char* __xuiMsgTipDupText(const char* sText)
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

static int __xuiMsgTipReplaceText(char** ppText, const char* sText)
{
	char* sCopy;

	if ( ppText == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	sCopy = __xuiMsgTipDupText(sText);
	if ( sCopy == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( *ppText != NULL ) {
		xrtFree(*ppText);
	}
	*ppText = sCopy;
	return XUI_OK;
}

static void __xuiMsgTipDefaultMetrics(xui_msgtip_metrics_t* pMetrics)
{
	memset(pMetrics, 0, sizeof(*pMetrics));
	pMetrics->iSize = sizeof(*pMetrics);
	pMetrics->fMinWidth = 92.0f;
	pMetrics->fMaxWidth = 360.0f;
	pMetrics->fMinHeight = 40.0f;
	pMetrics->fPaddingX = 16.0f;
	pMetrics->fPaddingY = 10.0f;
	pMetrics->fIconSize = 20.0f;
	pMetrics->fIconGap = 8.0f;
	pMetrics->fOffsetY = -72.0f;
}

static uint32_t __xuiMsgTipIconColor(int iType)
{
	switch ( iType ) {
	case XUI_MSGTIP_ICON_WAR:
		return XUI_COLOR_RGBA(231, 156, 45, 255);
	case XUI_MSGTIP_ICON_ERROR:
		return XUI_COLOR_RGBA(216, 76, 90, 255);
	case XUI_MSGTIP_ICON_QUEST:
		return XUI_COLOR_RGBA(59, 126, 204, 255);
	case XUI_MSGTIP_ICON_INFO:
	default:
		return XUI_COLOR_RGBA(60, 177, 128, 255);
	}
}

static void __xuiMsgTipDefaultColors(xui_msgtip_colors_t* pColors)
{
	memset(pColors, 0, sizeof(*pColors));
	pColors->iSize = sizeof(*pColors);
	pColors->iBackgroundColor = XUI_COLOR_RGBA(32, 42, 54, 222);
	pColors->iBorderColor = XUI_COLOR_RGBA(114, 151, 190, 120);
	pColors->iTextColor = XUI_COLOR_RGBA(255, 255, 255, 255);
	pColors->iIconColor = __xuiMsgTipIconColor(XUI_MSGTIP_ICON_INFO);
	pColors->iShadowColor = 0;
}

static int __xuiMsgTipMetricsValid(const xui_msgtip_metrics_t* pMetrics)
{
	if ( pMetrics == NULL ) return 0;
	if ( (pMetrics->iSize != 0) && (pMetrics->iSize < sizeof(*pMetrics)) ) return 0;
	return __xuiMsgTipFloatValid(pMetrics->fMinWidth) &&
	       __xuiMsgTipFloatValid(pMetrics->fMaxWidth) &&
	       __xuiMsgTipFloatValid(pMetrics->fMinHeight) &&
	       __xuiMsgTipFloatValid(pMetrics->fPaddingX) &&
	       __xuiMsgTipFloatValid(pMetrics->fPaddingY) &&
	       __xuiMsgTipFloatValid(pMetrics->fIconSize) &&
	       __xuiMsgTipFloatValid(pMetrics->fIconGap) &&
	       __xuiMsgTipSignedFloatValid(pMetrics->fOffsetY);
}

static int __xuiMsgTipColorsValid(const xui_msgtip_colors_t* pColors)
{
	if ( pColors == NULL ) return 0;
	return (pColors->iSize == 0) || (pColors->iSize >= sizeof(*pColors));
}

static int __xuiMsgTipDescValid(const xui_msgtip_desc_t* pDesc)
{
	if ( pDesc == NULL ) return 1;
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) return 0;
	if ( pDesc->fDuration != pDesc->fDuration ) return 0;
	if ( (pDesc->tIconSrc.fW < 0.0f) || (pDesc->tIconSrc.fH < 0.0f) ) return 0;
	if ( pDesc->bHasMetrics && !__xuiMsgTipMetricsValid(&pDesc->tMetrics) ) return 0;
	if ( pDesc->bHasColors && !__xuiMsgTipColorsValid(&pDesc->tColors) ) return 0;
	return 1;
}

static xui_font __xuiMsgTipFont(xui_msgtip pTip)
{
	if ( pTip == NULL ) return NULL;
	return (pTip->pFont != NULL) ? pTip->pFont : xuiGetDefaultFont(pTip->pContext);
}

static const char* __xuiMsgTipIconName(int iType)
{
	switch ( iType ) {
	case XUI_MSGTIP_ICON_INFO:
		return "msgbox_info";
	case XUI_MSGTIP_ICON_QUEST:
		return "msgbox_quest";
	case XUI_MSGTIP_ICON_WAR:
		return "msgbox_war";
	case XUI_MSGTIP_ICON_ERROR:
		return "msgbox_error";
	default:
		return NULL;
	}
}

static const char* __xuiMsgTipIconFallbackText(int iType)
{
	switch ( iType ) {
	case XUI_MSGTIP_ICON_QUEST:
		return "?";
	case XUI_MSGTIP_ICON_WAR:
		return "!";
	case XUI_MSGTIP_ICON_ERROR:
		return "x";
	case XUI_MSGTIP_ICON_INFO:
	default:
		return "i";
	}
}

static int __xuiMsgTipHasIcon(xui_msgtip pTip)
{
	if ( !__xuiMsgTipValid(pTip) ) return 0;
	if ( pTip->bCustomIcon && (pTip->pIconSurface != NULL) ) return 1;
	return pTip->iType != XUI_MSGTIP_ICON_NONE;
}

static xui_rect_t __xuiMsgTipRootRect(xui_msgtip pTip)
{
	xui_vec2_t tViewport;
	xui_widget pRoot;
	xui_rect_t tRect;

	tRect = (xui_rect_t){0.0f, 0.0f, 640.0f, 420.0f};
	if ( !__xuiMsgTipValid(pTip) ) {
		return tRect;
	}
	pRoot = xuiGetRootWidget(pTip->pContext);
	if ( pRoot != NULL ) {
		tRect = xuiWidgetGetRect(pRoot);
	}
	if ( (tRect.fW <= 1.0f) || (tRect.fH <= 1.0f) ) {
		tViewport = xuiGetViewportSize(pTip->pContext);
		if ( (tViewport.fX > 1.0f) && (tViewport.fY > 1.0f) ) {
			tRect = (xui_rect_t){0.0f, 0.0f, tViewport.fX, tViewport.fY};
		}
	}
	return tRect;
}

static int __xuiMsgTipMeasureText(xui_msgtip pTip, float fWidth, xui_vec2_t* pSize, int* pLineCount)
{
	xui_text_layout_desc_t tDesc;
	xui_text_layout pLayout;
	xui_font pFont;
	const char* sText;
	int iRet;

	if ( !__xuiMsgTipValid(pTip) || (pSize == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pSize->fX = 0.0f;
	pSize->fY = 0.0f;
	if ( pLineCount != NULL ) {
		*pLineCount = 0;
	}
	sText = (pTip->sText != NULL) ? pTip->sText : "";
	pFont = __xuiMsgTipFont(pTip);
	if ( pFont == NULL ) {
		pSize->fX = __xuiMsgTipMin((float)strlen(sText) * 7.0f, fWidth);
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
	tDesc.iWrapMode = XUI_TEXT_WRAP_CHAR;
	tDesc.iFlags = XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP;
	tDesc.fLineGap = 2.0f;
	pLayout = NULL;
	iRet = xuiTextLayoutCreate(pTip->pContext, &pLayout, &tDesc);
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

static int __xuiMsgTipLayout(xui_msgtip pTip)
{
	xui_rect_t tRoot;
	xui_rect_t tTip;
	xui_vec2_t tTextSize;
	float fMaxWidth;
	float fTextLimit;
	float fTextW;
	float fContentW;
	float fContentH;
	float fIconBlock;
	float fWidth;
	float fHeight;
	int bIcon;
	int iRet;

	if ( !__xuiMsgTipValid(pTip) || (pTip->pWidget == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	tRoot = __xuiMsgTipRootRect(pTip);
	if ( (tRoot.fW <= 0.0f) || (tRoot.fH <= 0.0f) ) {
		return XUI_OK;
	}
	bIcon = __xuiMsgTipHasIcon(pTip);
	fIconBlock = bIcon ? (pTip->tMetrics.fIconSize + pTip->tMetrics.fIconGap) : 0.0f;
	fMaxWidth = pTip->tMetrics.fMaxWidth;
	if ( fMaxWidth < pTip->tMetrics.fMinWidth ) {
		fMaxWidth = pTip->tMetrics.fMinWidth;
	}
	if ( tRoot.fW > 24.0f ) {
		fMaxWidth = __xuiMsgTipMin(fMaxWidth, tRoot.fW - 24.0f);
	}
	if ( fMaxWidth < 48.0f ) {
		fMaxWidth = 48.0f;
	}
	fTextLimit = fMaxWidth - pTip->tMetrics.fPaddingX * 2.0f - fIconBlock;
	if ( fTextLimit < 32.0f ) {
		fTextLimit = 32.0f;
	}
	iRet = __xuiMsgTipMeasureText(pTip, fTextLimit, &tTextSize, &pTip->iWrapLineCount);
	if ( iRet != XUI_OK ) {
		tTextSize.fX = fTextLimit;
		tTextSize.fY = 18.0f;
		pTip->iWrapLineCount = 1;
	}
	fTextW = __xuiMsgTipMin(__xuiMsgTipPixelCeil(tTextSize.fX), fTextLimit);
	fContentW = __xuiMsgTipPixelCeil(fIconBlock + fTextW);
	fContentH = tTextSize.fY;
	if ( bIcon ) {
		fContentH = __xuiMsgTipMax(fContentH, pTip->tMetrics.fIconSize);
	}
	fWidth = fContentW + pTip->tMetrics.fPaddingX * 2.0f;
	fWidth = __xuiMsgTipClamp(fWidth, pTip->tMetrics.fMinWidth, fMaxWidth);
	fWidth = __xuiMsgTipMin(__xuiMsgTipPixelCeil(fWidth), fMaxWidth);
	fTextLimit = fWidth - pTip->tMetrics.fPaddingX * 2.0f - fIconBlock;
	if ( fTextLimit < 32.0f ) {
		fTextLimit = 32.0f;
	}
	iRet = __xuiMsgTipMeasureText(pTip, fTextLimit, &tTextSize, &pTip->iWrapLineCount);
	if ( iRet != XUI_OK ) {
		tTextSize.fX = fTextLimit;
		tTextSize.fY = 18.0f;
		pTip->iWrapLineCount = 1;
	}
	fContentH = tTextSize.fY;
	if ( bIcon ) {
		fContentH = __xuiMsgTipMax(fContentH, pTip->tMetrics.fIconSize);
	}
	fHeight = fContentH + pTip->tMetrics.fPaddingY * 2.0f;
	fHeight = __xuiMsgTipMax(fHeight, pTip->tMetrics.fMinHeight);
	if ( fHeight > tRoot.fH ) {
		fHeight = tRoot.fH;
	}
	tTip.fW = fWidth;
	tTip.fH = fHeight;
	tTip.fX = tRoot.fX + (tRoot.fW - tTip.fW) * 0.5f;
	tTip.fY = tRoot.fY + (tRoot.fH - tTip.fH) * 0.5f + pTip->tMetrics.fOffsetY;
	if ( tTip.fY < tRoot.fY ) {
		tTip.fY = tRoot.fY;
	}
	if ( (tTip.fY + tTip.fH) > (tRoot.fY + tRoot.fH) ) {
		tTip.fY = tRoot.fY + tRoot.fH - tTip.fH;
	}
	tTip = xuiInternalSnapRect(tTip);
	pTip->tTipRect = tTip;
	if ( bIcon ) {
		pTip->tIconRect = xuiInternalSnapRect((xui_rect_t){
			pTip->tMetrics.fPaddingX,
			(tTip.fH - pTip->tMetrics.fIconSize) * 0.5f,
			pTip->tMetrics.fIconSize,
			pTip->tMetrics.fIconSize
		});
	} else {
		pTip->tIconRect = (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	}
	pTip->tTextRect = xuiInternalSnapRect((xui_rect_t){
		pTip->tMetrics.fPaddingX + fIconBlock,
		(tTip.fH - tTextSize.fY) * 0.5f,
		__xuiMsgTipMax(1.0f, tTip.fW - pTip->tMetrics.fPaddingX * 2.0f - fIconBlock),
		tTextSize.fY
	});
	iRet = xuiWidgetSetRect(pTip->pWidget, tTip);
	if ( iRet != XUI_OK ) return iRet;
	return xuiWidgetInvalidate(pTip->pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

static int __xuiMsgTipDrawTextLayout(xui_msgtip pTip, xui_draw_context pDraw)
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

	if ( !__xuiMsgTipValid(pTip) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(pTip->pContext);
	pFont = __xuiMsgTipFont(pTip);
	if ( (pProxy == NULL) || (pProxy->drawText == NULL) || (pDraw == NULL) || (pFont == NULL) ) {
		return XUI_OK;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sText = (pTip->sText != NULL) ? pTip->sText : "";
	tDesc.iTextSize = -1;
	tDesc.pFont = pFont;
	tDesc.fMaxWidth = pTip->tTextRect.fW;
	tDesc.fMaxHeight = pTip->tTextRect.fH;
	tDesc.iWrapMode = XUI_TEXT_WRAP_CHAR;
	tDesc.iFlags = XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP | XUI_TEXT_CLIP;
	tDesc.fLineGap = 2.0f;
	pLayout = NULL;
	iRet = xuiTextLayoutCreate(pTip->pContext, &pLayout, &tDesc);
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
		tLineRect.fX = pTip->tTextRect.fX + tLine.fX;
		tLineRect.fY = pTip->tTextRect.fY + tLine.fY;
		tLineRect.fW = pTip->tTextRect.fW;
		tLineRect.fH = tLine.fH;
		(void)pProxy->drawText(pProxy, pDraw, pFont, sLine, xuiInternalSnapRect(tLineRect), pTip->tColors.iTextColor, XUI_TEXT_ALIGN_LEFT | XUI_TEXT_ALIGN_TOP | XUI_TEXT_CLIP);
		xrtFree(sLine);
	}
	xuiTextLayoutDestroy(pLayout);
	return XUI_OK;
}

static int __xuiMsgTipDrawFallbackIcon(xui_msgtip pTip, xui_draw_context pDraw, xui_proxy pProxy)
{
	xui_rect_t tIcon;
	xui_rect_t tText;
	float fRadius;

	if ( (pProxy == NULL) || (pDraw == NULL) || (pTip == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	tIcon = pTip->tIconRect;
	fRadius = __xuiMsgTipMin(tIcon.fW, tIcon.fH) * 0.5f;
	if ( pProxy->drawCircleFill != NULL ) {
		(void)pProxy->drawCircleFill(pProxy, pDraw, tIcon.fX + tIcon.fW * 0.5f, tIcon.fY + tIcon.fH * 0.5f, fRadius, pTip->tColors.iIconColor);
	}
	if ( (pProxy->drawText != NULL) && (__xuiMsgTipFont(pTip) != NULL) ) {
		tText = tIcon;
		tText.fY -= 1.0f;
		(void)pProxy->drawText(pProxy, pDraw, __xuiMsgTipFont(pTip), __xuiMsgTipIconFallbackText(pTip->iType), tText, XUI_COLOR_WHITE, XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP);
	}
	return XUI_OK;
}

static int __xuiMsgTipDrawBuiltinIcon(xui_msgtip pTip, xui_draw_context pDraw, xui_proxy pProxy)
{
	xui_surface pAtlas;
	xui_rect_t tSrc;
	const char* sName;
	int iRet;

	if ( (pTip == NULL) || (pDraw == NULL) || (pProxy == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	sName = __xuiMsgTipIconName(pTip->iType);
	if ( sName == NULL ) {
		return XUI_OK;
	}
	pAtlas = NULL;
	iRet = xuiBuiltinAssetGetAtlas(pTip->pContext, &pAtlas);
	if ( iRet == XUI_OK ) {
		iRet = xuiBuiltinAssetGetRect(sName, &tSrc);
	}
	if ( (iRet == XUI_OK) && (pAtlas != NULL) && (pProxy->drawSurface != NULL) ) {
		return pProxy->drawSurface(pProxy, pDraw, pAtlas, tSrc, pTip->tIconRect, pTip->tColors.iIconColor, 0);
	}
	return __xuiMsgTipDrawFallbackIcon(pTip, pDraw, pProxy);
}

static int __xuiMsgTipRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_msgtip* ppTip;
	xui_msgtip pTip;
	xui_proxy pProxy;
	xui_rect_t tRect;

	(void)iStateId;
	(void)pWidget;
	ppTip = (xui_msgtip*)pUser;
	pTip = (ppTip != NULL) ? *ppTip : NULL;
	if ( !__xuiMsgTipValid(pTip) || !pTip->bOpen ) {
		return XUI_OK;
	}
	pProxy = xuiInternalContextGetProxy(pTip->pContext);
	if ( (pProxy == NULL) || (pDraw == NULL) ) {
		return XUI_OK;
	}
	tRect = xuiInternalSnapRect((xui_rect_t){0.0f, 0.0f, pTip->tTipRect.fW, pTip->tTipRect.fH});
	if ( pProxy->drawRectFill != NULL ) {
		(void)pProxy->drawRectFill(pProxy, pDraw, tRect, pTip->tColors.iBackgroundColor);
	}
	if ( ((pTip->tColors.iBorderColor & 0xffu) != 0u) && (pProxy->drawRectStroke != NULL) ) {
		(void)pProxy->drawRectStroke(pProxy, pDraw, tRect, 1.0f, pTip->tColors.iBorderColor);
	}
	if ( __xuiMsgTipHasIcon(pTip) ) {
		if ( pTip->bCustomIcon && (pTip->pIconSurface != NULL) && (pProxy->drawSurface != NULL) ) {
			(void)pProxy->drawSurface(pProxy, pDraw, pTip->pIconSurface, pTip->tIconSrc, pTip->tIconRect, XUI_COLOR_WHITE, 0);
		} else {
			(void)__xuiMsgTipDrawBuiltinIcon(pTip, pDraw, pProxy);
		}
	}
	(void)__xuiMsgTipDrawTextLayout(pTip, pDraw);
	return XUI_OK;
}

static int __xuiMsgTipCloseInternal(xui_msgtip pTip, int bExpired)
{
	if ( !__xuiMsgTipValid(pTip) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( !pTip->bOpen ) {
		return XUI_OK;
	}
	pTip->bOpen = 0;
	pTip->fElapsed = 0.0f;
	if ( bExpired ) {
		pTip->iExpireCount++;
	} else {
		pTip->iCloseCount++;
	}
	pTip->iChangeCount++;
	(void)xuiWidgetSetHitTestVisible(pTip->pWidget, 0);
	(void)xuiWidgetSetEnabled(pTip->pWidget, 0);
	(void)xuiWidgetSetVisible(pTip->pWidget, 0);
	(void)xuiWidgetInvalidate(pTip->pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
	if ( pTip->onClose != NULL ) {
		pTip->onClose(pTip, bExpired ? 1 : 0, pTip->pCloseUser);
	}
	return XUI_OK;
}

static int __xuiMsgTipUpdate(xui_widget pWidget, float fDelta, void* pUser)
{
	xui_msgtip* ppTip;
	xui_msgtip pTip;

	(void)pWidget;
	ppTip = (xui_msgtip*)pUser;
	pTip = (ppTip != NULL) ? *ppTip : NULL;
	if ( !__xuiMsgTipValid(pTip) || !pTip->bOpen || (pTip->fDuration <= 0.0f) || (fDelta <= 0.0f) ) {
		return XUI_OK;
	}
	pTip->fElapsed += fDelta;
	if ( pTip->fElapsed >= pTip->fDuration ) {
		return __xuiMsgTipCloseInternal(pTip, 1);
	}
	return XUI_OK;
}

static int __xuiMsgTipEvent(xui_widget pWidget, const xui_event_t* pEvent, void* pUser)
{
	xui_msgtip* ppTip;
	xui_msgtip pTip;

	(void)pWidget;
	ppTip = (xui_msgtip*)pUser;
	pTip = (ppTip != NULL) ? *ppTip : NULL;
	if ( !__xuiMsgTipValid(pTip) || (pEvent == NULL) || !pTip->bOpen ) {
		return XUI_OK;
	}
	if ( (pEvent->iType == XUI_EVENT_POINTER_DOWN) && (pEvent->iButton == XUI_POINTER_BUTTON_LEFT) ) {
		(void)__xuiMsgTipCloseInternal(pTip, 0);
		return XUI_EVENT_DISPATCH_STOP;
	}
	return XUI_OK;
}

static int __xuiMsgTipTypeInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_cache_policy_t tPolicy;
	xui_msgtip* ppTip;

	(void)pUser;
	if ( (pWidget == NULL) || (pTypeData == NULL) || (pCreateData == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	ppTip = (xui_msgtip*)pTypeData;
	*ppTip = (xui_msgtip)pCreateData;
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
	(void)xuiWidgetSetLayer(pWidget, XUI_LAYER_TOOLTIP, 100);
	(void)xuiWidgetSetCachePolicy(pWidget, &tPolicy);
	return xuiWidgetSetEventHandler(pWidget, XUI_EVENT_POINTER_DOWN, __xuiMsgTipEvent, pTypeData);
}

static void __xuiMsgTipTypeDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_msgtip* ppTip;

	(void)pWidget;
	(void)pUser;
	ppTip = (xui_msgtip*)pTypeData;
	if ( ppTip != NULL ) {
		*ppTip = NULL;
	}
}

static xui_widget_type __xuiMsgTipEnsureType(xui_context pContext)
{
	xui_widget_type pType;
	xui_widget_type_desc_t tDesc;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "msgtip");
	if ( pType != NULL ) {
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "msgtip";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iTypeDataSize = sizeof(xui_msgtip);
	tDesc.onInit = __xuiMsgTipTypeInit;
	tDesc.onDestroy = __xuiMsgTipTypeDestroy;
	tDesc.onCacheRender = __xuiMsgTipRender;
	tDesc.onUpdate = __xuiMsgTipUpdate;
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet == XUI_ERROR_ALREADY_INITIALIZED ) {
		return xuiWidgetFindType(pContext, "msgtip");
	}
	return (iRet == XUI_OK) ? pType : NULL;
}

XUI_API int xuiMsgTipCreate(xui_context pContext, xui_msgtip* ppTip, const xui_msgtip_desc_t* pDesc)
{
	xui_msgtip pTip;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) || (ppTip == NULL) || !__xuiMsgTipDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppTip = NULL;
	pType = __xuiMsgTipEnsureType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pTip = (xui_msgtip)xrtCalloc(1, sizeof(*pTip));
	if ( pTip == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pTip->iMagic = XUI_MSGTIP_MAGIC;
	pTip->pContext = pContext;
	pTip->pFont = (pDesc != NULL && pDesc->pFont != NULL) ? pDesc->pFont : xuiGetDefaultFont(pContext);
	pTip->iType = (pDesc != NULL) ? __xuiMsgTipNormalizeType(pDesc->iType) : XUI_MSGTIP_ICON_INFO;
	pTip->fDuration = (pDesc != NULL && pDesc->fDuration > 0.0f) ? pDesc->fDuration : 1.8f;
	__xuiMsgTipDefaultMetrics(&pTip->tMetrics);
	__xuiMsgTipDefaultColors(&pTip->tColors);
	if ( pDesc != NULL && pDesc->bHasMetrics ) pTip->tMetrics = pDesc->tMetrics;
	if ( pDesc != NULL && pDesc->bHasColors ) pTip->tColors = pDesc->tColors;
	pTip->tMetrics.iSize = sizeof(pTip->tMetrics);
	pTip->tColors.iSize = sizeof(pTip->tColors);
	if ( !(pDesc != NULL && pDesc->bHasColors) ) {
		pTip->tColors.iIconColor = __xuiMsgTipIconColor(pTip->iType);
	}
	if ( pDesc != NULL && pDesc->bHasCustomIcon && pDesc->pIconSurface != NULL ) {
		pTip->pIconSurface = pDesc->pIconSurface;
		pTip->tIconSrc = pDesc->tIconSrc;
		pTip->bCustomIcon = 1;
	}
	iRet = __xuiMsgTipReplaceText(&pTip->sText, (pDesc != NULL && pDesc->sText != NULL) ? pDesc->sText : "");
	if ( iRet != XUI_OK ) {
		xrtFree(pTip);
		return iRet;
	}
	iRet = xuiWidgetCreateTyped(pContext, pType, &pTip->pWidget, pTip);
	if ( iRet != XUI_OK ) {
		xuiMsgTipDestroy(pTip);
		return iRet;
	}
	iRet = __xuiMsgTipLayout(pTip);
	if ( iRet != XUI_OK ) {
		xuiMsgTipDestroy(pTip);
		return iRet;
	}
	*ppTip = pTip;
	return XUI_OK;
}

XUI_API void xuiMsgTipDestroy(xui_msgtip pTip)
{
	if ( !__xuiMsgTipValid(pTip) ) {
		return;
	}
	if ( pTip->bOpen ) {
		pTip->bOpen = 0;
		(void)xuiWidgetSetHitTestVisible(pTip->pWidget, 0);
		(void)xuiWidgetSetEnabled(pTip->pWidget, 0);
		(void)xuiWidgetSetVisible(pTip->pWidget, 0);
	}
	if ( pTip->pWidget != NULL ) {
		xuiWidgetDestroy(pTip->pWidget);
		pTip->pWidget = NULL;
	}
	if ( pTip->sText != NULL ) {
		xrtFree(pTip->sText);
		pTip->sText = NULL;
	}
	pTip->iMagic = 0;
	xrtFree(pTip);
}

XUI_API int xuiMsgTipShow(xui_msgtip pTip, int iType, const char* sText, float fDuration)
{
	int iRet;

	if ( !__xuiMsgTipValid(pTip) || (fDuration != fDuration) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiMsgTipReplaceText(&pTip->sText, sText);
	if ( iRet != XUI_OK ) return iRet;
	pTip->iType = __xuiMsgTipNormalizeType(iType);
	if ( !pTip->bCustomIcon ) {
		pTip->tColors.iIconColor = __xuiMsgTipIconColor(pTip->iType);
	}
	pTip->fDuration = (fDuration < 0.0f) ? 0.0f : fDuration;
	pTip->fElapsed = 0.0f;
	pTip->bOpen = 1;
	pTip->iShowCount++;
	pTip->iChangeCount++;
	iRet = xuiWidgetSetVisible(pTip->pWidget, 1);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetEnabled(pTip->pWidget, 1);
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetHitTestVisible(pTip->pWidget, 1);
	if ( iRet == XUI_OK && xuiWidgetGetParent(pTip->pWidget) == NULL ) {
		iRet = xuiOverlayAttach(pTip->pContext, NULL, pTip->pWidget, XUI_LAYER_TOOLTIP, 100);
	}
	if ( iRet == XUI_OK ) iRet = xuiWidgetSetLayer(pTip->pWidget, XUI_LAYER_TOOLTIP, 100);
	if ( iRet == XUI_OK ) iRet = xuiOverlayBringToFront(pTip->pWidget);
	if ( iRet == XUI_OK ) iRet = __xuiMsgTipLayout(pTip);
	return iRet;
}

XUI_API int xuiMsgTipClose(xui_msgtip pTip)
{
	return __xuiMsgTipCloseInternal(pTip, 0);
}

XUI_API int xuiMsgTipIsOpen(xui_msgtip pTip)
{
	return __xuiMsgTipValid(pTip) ? pTip->bOpen : 0;
}

XUI_API int xuiMsgTipSetText(xui_msgtip pTip, const char* sText)
{
	int iRet;

	if ( !__xuiMsgTipValid(pTip) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iRet = __xuiMsgTipReplaceText(&pTip->sText, sText);
	if ( iRet != XUI_OK ) return iRet;
	pTip->iChangeCount++;
	return __xuiMsgTipLayout(pTip);
}

XUI_API const char* xuiMsgTipGetText(xui_msgtip pTip)
{
	return __xuiMsgTipValid(pTip) ? pTip->sText : NULL;
}

XUI_API int xuiMsgTipSetType(xui_msgtip pTip, int iType)
{
	if ( !__xuiMsgTipValid(pTip) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pTip->iType = __xuiMsgTipNormalizeType(iType);
	if ( !pTip->bCustomIcon ) {
		pTip->tColors.iIconColor = __xuiMsgTipIconColor(pTip->iType);
	}
	pTip->iChangeCount++;
	return __xuiMsgTipLayout(pTip);
}

XUI_API int xuiMsgTipGetType(xui_msgtip pTip)
{
	return __xuiMsgTipValid(pTip) ? pTip->iType : XUI_MSGTIP_ICON_NONE;
}

XUI_API int xuiMsgTipSetDuration(xui_msgtip pTip, float fDuration)
{
	if ( !__xuiMsgTipValid(pTip) || (fDuration != fDuration) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pTip->fDuration = (fDuration < 0.0f) ? 0.0f : fDuration;
	pTip->iChangeCount++;
	return XUI_OK;
}

XUI_API float xuiMsgTipGetDuration(xui_msgtip pTip)
{
	return __xuiMsgTipValid(pTip) ? pTip->fDuration : 0.0f;
}

XUI_API int xuiMsgTipSetIconSurface(xui_msgtip pTip, xui_surface pSurface, xui_rect_t tSrc)
{
	if ( !__xuiMsgTipValid(pTip) || (tSrc.fW < 0.0f) || (tSrc.fH < 0.0f) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pTip->pIconSurface = pSurface;
	pTip->tIconSrc = tSrc;
	pTip->bCustomIcon = (pSurface != NULL);
	pTip->iChangeCount++;
	return __xuiMsgTipLayout(pTip);
}

XUI_API int xuiMsgTipUseBuiltinIcon(xui_msgtip pTip)
{
	if ( !__xuiMsgTipValid(pTip) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pTip->pIconSurface = NULL;
	pTip->tIconSrc = (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	pTip->bCustomIcon = 0;
	pTip->tColors.iIconColor = __xuiMsgTipIconColor(pTip->iType);
	pTip->iChangeCount++;
	return __xuiMsgTipLayout(pTip);
}

XUI_API int xuiMsgTipSetMetrics(xui_msgtip pTip, const xui_msgtip_metrics_t* pMetrics)
{
	if ( !__xuiMsgTipValid(pTip) || !__xuiMsgTipMetricsValid(pMetrics) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pTip->tMetrics = *pMetrics;
	pTip->tMetrics.iSize = sizeof(pTip->tMetrics);
	if ( pTip->tMetrics.fMinWidth < 32.0f ) pTip->tMetrics.fMinWidth = 32.0f;
	if ( pTip->tMetrics.fMaxWidth < pTip->tMetrics.fMinWidth ) pTip->tMetrics.fMaxWidth = pTip->tMetrics.fMinWidth;
	if ( pTip->tMetrics.fMinHeight < 24.0f ) pTip->tMetrics.fMinHeight = 24.0f;
	pTip->iChangeCount++;
	return __xuiMsgTipLayout(pTip);
}

XUI_API int xuiMsgTipGetMetrics(xui_msgtip pTip, xui_msgtip_metrics_t* pMetrics)
{
	if ( !__xuiMsgTipValid(pTip) || (pMetrics == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*pMetrics = pTip->tMetrics;
	pMetrics->iSize = sizeof(*pMetrics);
	return XUI_OK;
}

XUI_API int xuiMsgTipSetColors(xui_msgtip pTip, const xui_msgtip_colors_t* pColors)
{
	if ( !__xuiMsgTipValid(pTip) || !__xuiMsgTipColorsValid(pColors) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pTip->tColors = *pColors;
	pTip->tColors.iSize = sizeof(pTip->tColors);
	pTip->iChangeCount++;
	return xuiWidgetInvalidate(pTip->pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiMsgTipGetColors(xui_msgtip pTip, xui_msgtip_colors_t* pColors)
{
	if ( !__xuiMsgTipValid(pTip) || (pColors == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*pColors = pTip->tColors;
	pColors->iSize = sizeof(*pColors);
	return XUI_OK;
}

XUI_API int xuiMsgTipSetClose(xui_msgtip pTip, xui_msgtip_close_proc onClose, void* pUser)
{
	if ( !__xuiMsgTipValid(pTip) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pTip->onClose = onClose;
	pTip->pCloseUser = pUser;
	return XUI_OK;
}

XUI_API xui_widget xuiMsgTipGetWidget(xui_msgtip pTip)
{
	return __xuiMsgTipValid(pTip) ? pTip->pWidget : NULL;
}

XUI_API xui_rect_t xuiMsgTipGetTipRect(xui_msgtip pTip)
{
	return __xuiMsgTipValid(pTip) ? pTip->tTipRect : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API xui_rect_t xuiMsgTipGetIconRect(xui_msgtip pTip)
{
	return __xuiMsgTipValid(pTip) ? pTip->tIconRect : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API xui_rect_t xuiMsgTipGetTextRect(xui_msgtip pTip)
{
	return __xuiMsgTipValid(pTip) ? pTip->tTextRect : (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
}

XUI_API int xuiMsgTipGetWrapLineCount(xui_msgtip pTip)
{
	return __xuiMsgTipValid(pTip) ? pTip->iWrapLineCount : 0;
}

XUI_API int xuiMsgTipGetShowCount(xui_msgtip pTip)
{
	return __xuiMsgTipValid(pTip) ? pTip->iShowCount : 0;
}

XUI_API int xuiMsgTipGetCloseCount(xui_msgtip pTip)
{
	return __xuiMsgTipValid(pTip) ? pTip->iCloseCount : 0;
}

XUI_API int xuiMsgTipGetExpireCount(xui_msgtip pTip)
{
	return __xuiMsgTipValid(pTip) ? pTip->iExpireCount : 0;
}

XUI_API int xuiMsgTipGetChangeCount(xui_msgtip pTip)
{
	return __xuiMsgTipValid(pTip) ? pTip->iChangeCount : 0;
}
