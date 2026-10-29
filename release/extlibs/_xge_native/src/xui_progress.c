#include "xui_internal.h"

#include <string.h>

typedef struct xui_progress_data_t {
	char* sTextTemplate;
	char* sDisplayText;
	xui_font pFont;
	float fMin;
	float fMax;
	float fValue;
	uint32_t iTrackColor;
	uint32_t iFillColor;
	uint32_t iTextColor;
	uint32_t iFillTextColor;
	uint32_t iTextFlags;
	int iFillDirection;
	int iFillPatchMode;
	xui_nine_patch_t tTrackPatch;
	xui_nine_patch_t tFillPatch;
	int bHasTrackPatch;
	int bHasFillPatch;
	int bTemplateString;
} xui_progress_data_t;

typedef struct xui_progress_resolved_t {
	xui_font pFont;
	uint32_t iTrackColor;
	uint32_t iFillColor;
	uint32_t iTextColor;
	uint32_t iFillTextColor;
	uint32_t iTextFlags;
	int iFillDirection;
	int iFillPatchMode;
} xui_progress_resolved_t;

static uint32_t __xuiProgressColorAlpha(uint32_t iColor)
{
	return iColor & 0xffu;
}

static float __xuiProgressClampFloat(float fValue, float fMin, float fMax)
{
	if ( fValue < fMin ) {
		return fMin;
	}
	if ( fValue > fMax ) {
		return fMax;
	}
	return fValue;
}

static int __xuiProgressDirectionValid(int iDirection)
{
	return (iDirection == XUI_PROGRESS_LEFT_TO_RIGHT) ||
	       (iDirection == XUI_PROGRESS_RIGHT_TO_LEFT) ||
	       (iDirection == XUI_PROGRESS_BOTTOM_TO_TOP) ||
	       (iDirection == XUI_PROGRESS_TOP_TO_BOTTOM);
}

static int __xuiProgressPatchModeValid(int iMode)
{
	return (iMode == XUI_PROGRESS_FILL_STRETCH) ||
	       (iMode == XUI_PROGRESS_FILL_REVEAL);
}

static int __xuiProgressDescValid(const xui_progress_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 1;
	}
	if ( (pDesc->iSize != 0) && (pDesc->iSize < sizeof(*pDesc)) ) {
		return 0;
	}
	if ( (pDesc->iFillDirection != 0) && !__xuiProgressDirectionValid(pDesc->iFillDirection) ) {
		return 0;
	}
	if ( (pDesc->iFillPatchMode != 0) && !__xuiProgressPatchModeValid(pDesc->iFillPatchMode) ) {
		return 0;
	}
	return 1;
}

static void __xuiProgressNormalizeRange(float* pMin, float* pMax)
{
	float fSwap;

	if ( (*pMax) < (*pMin) ) {
		fSwap = *pMin;
		*pMin = *pMax;
		*pMax = fSwap;
	}
	if ( (*pMax) == (*pMin) ) {
		*pMax = (*pMin) + 1.0f;
	}
}

static float __xuiProgressRateData(const xui_progress_data_t* pData)
{
	if ( (pData == NULL) || (pData->fMax == pData->fMin) ) {
		return 0.0f;
	}
	return __xuiProgressClampFloat((pData->fValue - pData->fMin) / (pData->fMax - pData->fMin), 0.0f, 1.0f);
}

static int __xuiProgressTemplateIsDynamic(const char* sTextTemplate)
{
	int i;

	if ( sTextTemplate == NULL ) {
		return 0;
	}
	for ( i = 0; sTextTemplate[i] != 0; i++ ) {
		if ( sTextTemplate[i] != '%' ) {
			continue;
		}
		if ( sTextTemplate[i + 1] == '%' ) {
			i++;
			continue;
		}
		return 1;
	}
	return 0;
}

static char* __xuiProgressCopyText(const char* sText)
{
	char* sCopy;
	size_t iNeed;

	if ( sText == NULL ) {
		return NULL;
	}
	iNeed = strlen(sText) + 1u;
	sCopy = (char*)xrtMalloc(iNeed);
	if ( sCopy == NULL ) {
		return NULL;
	}
	memcpy(sCopy, sText, iNeed);
	return sCopy;
}

static int __xuiProgressSetDisplayText(xui_progress_data_t* pData)
{
	char* sNew;
	double fPercent;

	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->sDisplayText != NULL ) {
		xrtFree(pData->sDisplayText);
		pData->sDisplayText = NULL;
	}
	if ( pData->sTextTemplate == NULL ) {
		return XUI_OK;
	}
	if ( pData->bTemplateString ) {
		fPercent = (double)__xuiProgressRateData(pData) * 100.0;
		sNew = (char*)xrtFormat(pData->sTextTemplate, fPercent);
	} else {
		sNew = __xuiProgressCopyText(pData->sTextTemplate);
	}
	if ( sNew == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pData->sDisplayText = sNew;
	return XUI_OK;
}

static int __xuiProgressSetTextTemplateData(xui_progress_data_t* pData, const char* sTextTemplate)
{
	char* sNewTemplate;
	int iRet;

	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	sNewTemplate = __xuiProgressCopyText(sTextTemplate);
	if ( (sTextTemplate != NULL) && (sNewTemplate == NULL) ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	if ( pData->sTextTemplate != NULL ) {
		xrtFree(pData->sTextTemplate);
	}
	pData->sTextTemplate = sNewTemplate;
	pData->bTemplateString = __xuiProgressTemplateIsDynamic(pData->sTextTemplate);
	iRet = __xuiProgressSetDisplayText(pData);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return XUI_OK;
}

static xui_progress_data_t* __xuiProgressGetData(xui_widget pWidget)
{
	xui_context pContext;
	xui_widget_type pType;

	if ( !xuiInternalWidgetIsValid(pWidget) ) {
		return NULL;
	}
	pContext = xuiWidgetGetContext(pWidget);
	pType = xuiWidgetFindType(pContext, "progress");
	if ( (pType == NULL) || !xuiWidgetIsType(pWidget, pType) ) {
		return NULL;
	}
	return (xui_progress_data_t*)xuiWidgetGetTypeData(pWidget);
}

static int __xuiProgressStyleColor(xui_widget pWidget, const char* sName, uint32_t* pColor)
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

static int __xuiProgressStyleInt(xui_widget pWidget, const char* sName, int* pValue)
{
	xui_style_property_t tProperty;
	int iRet;

	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, sName, &tProperty);
	if ( (iRet == XUI_OK) &&
	     ((tProperty.tValue.iType == XUI_STYLE_VALUE_INT) || (tProperty.tValue.iType == XUI_STYLE_VALUE_BOOL)) ) {
		*pValue = tProperty.tValue.iInt;
		return 1;
	}
	return 0;
}

static xui_font __xuiProgressStyleFont(xui_widget pWidget, xui_font pBaseFont)
{
	xui_style_property_t tProperty;
	xui_context pContext;
	xui_font pFont;
	int iRet;

	memset(&tProperty, 0, sizeof(tProperty));
	tProperty.iSize = sizeof(tProperty);
	iRet = xuiWidgetGetResolvedStyleProperty(pWidget, "font.name", &tProperty);
	if ( (iRet == XUI_OK) &&
	     (tProperty.tValue.iType == XUI_STYLE_VALUE_STRING) &&
	     (tProperty.tValue.sText != NULL) ) {
		pContext = xuiWidgetGetContext(pWidget);
		pFont = xuiFindFont(pContext, tProperty.tValue.sText);
		if ( pFont != NULL ) {
			return pFont;
		}
	}
	return pBaseFont;
}

static void __xuiProgressResolve(xui_widget pWidget, const xui_progress_data_t* pData, xui_progress_resolved_t* pResolved)
{
	int iValue;

	memset(pResolved, 0, sizeof(*pResolved));
	pResolved->pFont = (pData->pFont != NULL) ? pData->pFont : xuiGetDefaultFont(xuiWidgetGetContext(pWidget));
	pResolved->iTrackColor = pData->iTrackColor;
	pResolved->iFillColor = pData->iFillColor;
	pResolved->iTextColor = pData->iTextColor;
	pResolved->iFillTextColor = pData->iFillTextColor;
	pResolved->iTextFlags = pData->iTextFlags | XUI_TEXT_CLIP;
	pResolved->iFillDirection = pData->iFillDirection;
	pResolved->iFillPatchMode = pData->iFillPatchMode;

	(void)__xuiProgressStyleColor(pWidget, "text.color", &pResolved->iTextColor);
	(void)__xuiProgressStyleColor(pWidget, "progress.track_color", &pResolved->iTrackColor);
	(void)__xuiProgressStyleColor(pWidget, "progress.fill_color", &pResolved->iFillColor);
	(void)__xuiProgressStyleColor(pWidget, "progress.text_color", &pResolved->iTextColor);
	(void)__xuiProgressStyleColor(pWidget, "progress.fill_text_color", &pResolved->iFillTextColor);
	if ( __xuiProgressStyleInt(pWidget, "text.flags", &iValue) ) {
		pResolved->iTextFlags = (uint32_t)iValue;
	}
	if ( __xuiProgressStyleInt(pWidget, "progress.text_flags", &iValue) ) {
		pResolved->iTextFlags = (uint32_t)iValue;
	}
	if ( __xuiProgressStyleInt(pWidget, "progress.fill_direction", &iValue) && __xuiProgressDirectionValid(iValue) ) {
		pResolved->iFillDirection = iValue;
	}
	if ( __xuiProgressStyleInt(pWidget, "progress.fill_patch_mode", &iValue) && __xuiProgressPatchModeValid(iValue) ) {
		pResolved->iFillPatchMode = iValue;
	}
	pResolved->pFont = __xuiProgressStyleFont(pWidget, pResolved->pFont);
	pResolved->iTextFlags |= XUI_TEXT_CLIP;
	if ( !__xuiProgressDirectionValid(pResolved->iFillDirection) ) {
		pResolved->iFillDirection = XUI_PROGRESS_LEFT_TO_RIGHT;
	}
	if ( !__xuiProgressPatchModeValid(pResolved->iFillPatchMode) ) {
		pResolved->iFillPatchMode = XUI_PROGRESS_FILL_STRETCH;
	}
}

static xui_rect_t __xuiProgressSurfaceSrc(xui_proxy pProxy, xui_surface pSurface, xui_rect_t tSrc)
{
	xui_surface_desc_t tDesc;

	if ( (tSrc.fW <= 0.0f) || (tSrc.fH <= 0.0f) ) {
		memset(&tDesc, 0, sizeof(tDesc));
		if ( (pProxy != NULL) &&
		     (pProxy->surfaceGetDesc != NULL) &&
		     (pProxy->surfaceGetDesc(pProxy, pSurface, &tDesc) == XUI_OK) ) {
			tSrc.fX = 0.0f;
			tSrc.fY = 0.0f;
			tSrc.fW = (float)tDesc.iWidth;
			tSrc.fH = (float)tDesc.iHeight;
		}
	}
	return tSrc;
}

static void __xuiProgressScaleFixed(float fTotal, float* pA, float* pB, float* pMid)
{
	float fSum;

	fSum = *pA + *pB;
	if ( fSum > fTotal ) {
		if ( fSum > 0.0f ) {
			*pA = (*pA) * fTotal / fSum;
			*pB = fTotal - *pA;
		} else {
			*pA = 0.0f;
			*pB = 0.0f;
		}
		*pMid = 0.0f;
	} else {
		*pMid = fTotal - fSum;
	}
}

static int __xuiProgressDrawSurfacePiece(xui_proxy pProxy, xui_draw_context pDraw, xui_surface pSurface, xui_rect_t tSrc, xui_rect_t tDst, uint32_t iColor)
{
	if ( (tSrc.fW <= 0.0f) || (tSrc.fH <= 0.0f) || (tDst.fW <= 0.0f) || (tDst.fH <= 0.0f) ||
	     (__xuiProgressColorAlpha(iColor) == 0) ) {
		return XUI_OK;
	}
	return pProxy->drawSurface(pProxy, pDraw, pSurface, tSrc, tDst, iColor, 0);
}

static int __xuiProgressDrawTilePiece(xui_proxy pProxy, xui_draw_context pDraw, xui_surface pSurface, xui_rect_t tSrc, xui_rect_t tDst, uint32_t iColor)
{
	xui_rect_t tTileSrc;
	xui_rect_t tTileDst;
	float fX;
	float fY;
	float fW;
	float fH;
	int iRet;

	if ( (tSrc.fW <= 0.0f) || (tSrc.fH <= 0.0f) || (tDst.fW <= 0.0f) || (tDst.fH <= 0.0f) ) {
		return XUI_OK;
	}
	for ( fY = 0.0f; fY < tDst.fH; fY += tSrc.fH ) {
		fH = tSrc.fH;
		if ( fY + fH > tDst.fH ) {
			fH = tDst.fH - fY;
		}
		for ( fX = 0.0f; fX < tDst.fW; fX += tSrc.fW ) {
			fW = tSrc.fW;
			if ( fX + fW > tDst.fW ) {
				fW = tDst.fW - fX;
			}
			tTileSrc = tSrc;
			tTileDst = tDst;
			tTileSrc.fW = fW;
			tTileSrc.fH = fH;
			tTileDst.fX += fX;
			tTileDst.fY += fY;
			tTileDst.fW = fW;
			tTileDst.fH = fH;
			iRet = __xuiProgressDrawSurfacePiece(pProxy, pDraw, pSurface, tTileSrc, tTileDst, iColor);
			if ( iRet != XUI_OK ) {
				return iRet;
			}
		}
	}
	return XUI_OK;
}

static int __xuiProgressDrawNinePatch(xui_proxy pProxy, xui_draw_context pDraw, const xui_nine_patch_t* pPatch, xui_rect_t tDst)
{
	xui_rect_t tSrc;
	xui_rect_t arrSrc[9];
	xui_rect_t arrDst[9];
	float arrSrcX[4];
	float arrSrcY[4];
	float arrDstX[4];
	float arrDstY[4];
	float fLeft;
	float fTop;
	float fRight;
	float fBottom;
	float fMidW;
	float fMidH;
	int x;
	int y;
	int iIndex;
	int bTile;
	int iRet;

	if ( (pProxy == NULL) || (pDraw == NULL) || (pPatch == NULL) || (pPatch->pSurface == NULL) ||
	     (tDst.fW <= 0.0f) || (tDst.fH <= 0.0f) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	tSrc = __xuiProgressSurfaceSrc(pProxy, pPatch->pSurface, pPatch->tSrc);
	if ( (tSrc.fW <= 0.0f) || (tSrc.fH <= 0.0f) ) {
		return XUI_OK;
	}
	fLeft = pPatch->tSlice.fLeft;
	fTop = pPatch->tSlice.fTop;
	fRight = pPatch->tSlice.fRight;
	fBottom = pPatch->tSlice.fBottom;
	if ( fLeft + fRight > tSrc.fW ) {
		fRight = tSrc.fW - fLeft;
		if ( fRight < 0.0f ) {
			fRight = 0.0f;
			fLeft = tSrc.fW;
		}
	}
	if ( fTop + fBottom > tSrc.fH ) {
		fBottom = tSrc.fH - fTop;
		if ( fBottom < 0.0f ) {
			fBottom = 0.0f;
			fTop = tSrc.fH;
		}
	}
	__xuiProgressScaleFixed(tDst.fW, &fLeft, &fRight, &fMidW);
	__xuiProgressScaleFixed(tDst.fH, &fTop, &fBottom, &fMidH);
	arrSrcX[0] = tSrc.fX;
	arrSrcX[1] = tSrc.fX + pPatch->tSlice.fLeft;
	arrSrcX[2] = tSrc.fX + tSrc.fW - pPatch->tSlice.fRight;
	arrSrcX[3] = tSrc.fX + tSrc.fW;
	arrSrcY[0] = tSrc.fY;
	arrSrcY[1] = tSrc.fY + pPatch->tSlice.fTop;
	arrSrcY[2] = tSrc.fY + tSrc.fH - pPatch->tSlice.fBottom;
	arrSrcY[3] = tSrc.fY + tSrc.fH;
	arrDstX[0] = tDst.fX;
	arrDstX[1] = tDst.fX + fLeft;
	arrDstX[2] = tDst.fX + tDst.fW - fRight;
	arrDstX[3] = tDst.fX + tDst.fW;
	arrDstY[0] = tDst.fY;
	arrDstY[1] = tDst.fY + fTop;
	arrDstY[2] = tDst.fY + tDst.fH - fBottom;
	arrDstY[3] = tDst.fY + tDst.fH;
	for ( y = 0; y < 3; y++ ) {
		for ( x = 0; x < 3; x++ ) {
			iIndex = y * 3 + x;
			arrSrc[iIndex] = (xui_rect_t){arrSrcX[x], arrSrcY[y], arrSrcX[x + 1] - arrSrcX[x], arrSrcY[y + 1] - arrSrcY[y]};
			arrDst[iIndex] = (xui_rect_t){arrDstX[x], arrDstY[y], arrDstX[x + 1] - arrDstX[x], arrDstY[y + 1] - arrDstY[y]};
		}
	}
	for ( iIndex = 0; iIndex < 9; iIndex++ ) {
		bTile = (pPatch->iMode == XUI_NINE_PATCH_TILE) && (iIndex != 0) && (iIndex != 2) && (iIndex != 6) && (iIndex != 8);
		if ( bTile ) {
			iRet = __xuiProgressDrawTilePiece(pProxy, pDraw, pPatch->pSurface, arrSrc[iIndex], arrDst[iIndex], pPatch->iColor);
		} else {
			iRet = __xuiProgressDrawSurfacePiece(pProxy, pDraw, pPatch->pSurface, arrSrc[iIndex], arrDst[iIndex], pPatch->iColor);
		}
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return XUI_OK;
}

static int __xuiProgressPatchIsRevealSimple(const xui_nine_patch_t* pPatch)
{
	return (pPatch != NULL) &&
	       (pPatch->tSlice.fLeft <= 0.0f) &&
	       (pPatch->tSlice.fTop <= 0.0f) &&
	       (pPatch->tSlice.fRight <= 0.0f) &&
	       (pPatch->tSlice.fBottom <= 0.0f);
}

static int __xuiProgressDrawRevealPatch(xui_proxy pProxy, xui_draw_context pDraw, const xui_nine_patch_t* pPatch, xui_rect_t tFill, int iDirection, float fRate)
{
	xui_rect_t tSrc;

	if ( (pPatch == NULL) || (pPatch->pSurface == NULL) || (tFill.fW <= 0.0f) || (tFill.fH <= 0.0f) || (fRate <= 0.0f) ) {
		return XUI_OK;
	}
	tSrc = __xuiProgressSurfaceSrc(pProxy, pPatch->pSurface, pPatch->tSrc);
	if ( (tSrc.fW <= 0.0f) || (tSrc.fH <= 0.0f) ) {
		return XUI_OK;
	}
	if ( iDirection == XUI_PROGRESS_RIGHT_TO_LEFT ) {
		tSrc.fX += tSrc.fW * (1.0f - fRate);
		tSrc.fW *= fRate;
	} else if ( iDirection == XUI_PROGRESS_BOTTOM_TO_TOP ) {
		tSrc.fY += tSrc.fH * (1.0f - fRate);
		tSrc.fH *= fRate;
	} else if ( iDirection == XUI_PROGRESS_TOP_TO_BOTTOM ) {
		tSrc.fH *= fRate;
	} else {
		tSrc.fW *= fRate;
	}
	return __xuiProgressDrawSurfacePiece(pProxy, pDraw, pPatch->pSurface, tSrc, tFill, pPatch->iColor);
}

static void __xuiProgressNormalizePatch(xui_nine_patch_t* pPatch)
{
	if ( pPatch == NULL ) {
		return;
	}
	if ( pPatch->iSize == 0 ) {
		pPatch->iSize = sizeof(*pPatch);
	}
	if ( pPatch->iColor == 0 ) {
		pPatch->iColor = XUI_COLOR_WHITE;
	}
}

static xui_rect_t __xuiProgressContentRect(xui_widget pWidget)
{
	xui_rect_t tContent;

	tContent = xuiWidgetGetContentRect(pWidget);
	if ( (tContent.fW <= 0.0f) || (tContent.fH <= 0.0f) ) {
		tContent = xuiWidgetGetRect(pWidget);
		tContent.fX = 0.0f;
		tContent.fY = 0.0f;
	}
	return xuiInternalSnapRect(tContent);
}

static xui_rect_t __xuiProgressFillRectFromData(xui_widget pWidget, const xui_progress_data_t* pData, int iFillDirection)
{
	xui_rect_t tContent;
	xui_rect_t tFill;
	float fRate;
	float fEdge;

	tContent = __xuiProgressContentRect(pWidget);
	tFill = tContent;
	fRate = __xuiProgressRateData(pData);
	if ( (tContent.fW <= 0.0f) || (tContent.fH <= 0.0f) || (fRate <= 0.0f) ) {
		return (xui_rect_t){tContent.fX, tContent.fY, 0.0f, 0.0f};
	}
	if ( fRate >= 1.0f ) {
		return tContent;
	}
	if ( iFillDirection == XUI_PROGRESS_RIGHT_TO_LEFT ) {
		fEdge = xuiInternalSnapPixel(tContent.fX + tContent.fW * (1.0f - fRate));
		tFill.fX = fEdge;
		tFill.fW = (tContent.fX + tContent.fW) - fEdge;
	} else if ( iFillDirection == XUI_PROGRESS_BOTTOM_TO_TOP ) {
		fEdge = xuiInternalSnapPixel(tContent.fY + tContent.fH * (1.0f - fRate));
		tFill.fY = fEdge;
		tFill.fH = (tContent.fY + tContent.fH) - fEdge;
	} else if ( iFillDirection == XUI_PROGRESS_TOP_TO_BOTTOM ) {
		fEdge = xuiInternalSnapPixel(tContent.fY + tContent.fH * fRate);
		tFill.fH = fEdge - tContent.fY;
	} else {
		fEdge = xuiInternalSnapPixel(tContent.fX + tContent.fW * fRate);
		tFill.fW = fEdge - tContent.fX;
	}
	if ( tFill.fW < 0.0f ) {
		tFill.fW = 0.0f;
	}
	if ( tFill.fH < 0.0f ) {
		tFill.fH = 0.0f;
	}
	return tFill;
}

static int __xuiProgressRectContains(xui_rect_t tOuter, xui_rect_t tInner)
{
	return (tInner.fX >= tOuter.fX) &&
	       (tInner.fY >= tOuter.fY) &&
	       ((tInner.fX + tInner.fW) <= (tOuter.fX + tOuter.fW)) &&
	       ((tInner.fY + tInner.fH) <= (tOuter.fY + tOuter.fH));
}

static xui_rect_t __xuiProgressTextVisualRect(xui_proxy pProxy, xui_font pFont, const char* sText, xui_rect_t tContent, uint32_t iFlags)
{
	xui_vec2_t tSize;
	xui_rect_t tText;

	tText = tContent;
	memset(&tSize, 0, sizeof(tSize));
	if ( (pProxy == NULL) || (pProxy->textMeasure == NULL) ||
	     (pProxy->textMeasure(pProxy, pFont, sText, &tSize) != XUI_OK) ||
	     (tSize.fX <= 0.0f) || (tSize.fY <= 0.0f) ) {
		return tText;
	}
	if ( tSize.fX > tContent.fW ) {
		tSize.fX = tContent.fW;
	}
	if ( tSize.fY > tContent.fH ) {
		tSize.fY = tContent.fH;
	}
	tText.fW = tSize.fX;
	tText.fH = tSize.fY;
	if ( (iFlags & XUI_TEXT_ALIGN_RIGHT) == XUI_TEXT_ALIGN_RIGHT ) {
		tText.fX = tContent.fX + tContent.fW - tText.fW;
	} else if ( (iFlags & XUI_TEXT_ALIGN_CENTER) == XUI_TEXT_ALIGN_CENTER ) {
		tText.fX = tContent.fX + (tContent.fW - tText.fW) * 0.5f;
	} else {
		tText.fX = tContent.fX;
	}
	if ( (iFlags & XUI_TEXT_ALIGN_BOTTOM) == XUI_TEXT_ALIGN_BOTTOM ) {
		tText.fY = tContent.fY + tContent.fH - tText.fH;
	} else if ( (iFlags & XUI_TEXT_ALIGN_MIDDLE) == XUI_TEXT_ALIGN_MIDDLE ) {
		tText.fY = tContent.fY + (tContent.fH - tText.fH) * 0.5f;
	} else {
		tText.fY = tContent.fY;
	}
	return xuiInternalSnapRect(tText);
}

static int __xuiProgressContentMeasure(xui_widget pWidget, xui_vec2_t tConstraint, xui_vec2_t* pSize, void* pUser)
{
	xui_progress_data_t* pData;
	xui_progress_resolved_t tResolved;

	(void)tConstraint;
	if ( (pWidget == NULL) || (pSize == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = (xui_progress_data_t*)pUser;
	if ( pData == NULL ) {
		pData = __xuiProgressGetData(pWidget);
	}
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiProgressResolve(pWidget, pData, &tResolved);
	if ( (tResolved.iFillDirection == XUI_PROGRESS_BOTTOM_TO_TOP) ||
	     (tResolved.iFillDirection == XUI_PROGRESS_TOP_TO_BOTTOM) ) {
		pSize->fX = 12.0f;
		pSize->fY = 120.0f;
	} else {
		pSize->fX = 120.0f;
		pSize->fY = 12.0f;
	}
	return XUI_OK;
}

static int __xuiProgressCacheRender(xui_widget pWidget, xui_draw_context pDraw, uint32_t iStateId, void* pUser)
{
	xui_progress_data_t* pData;
	xui_progress_resolved_t tResolved;
	xui_proxy pProxy;
	xui_rect_t tContent;
	xui_rect_t tFill;
	float fRate;
	int iRet;
	int bFillVisible;

	(void)iStateId;
	(void)pUser;
	if ( (pWidget == NULL) || (pDraw == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiProgressGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(xuiWidgetGetContext(pWidget));
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	__xuiProgressResolve(pWidget, pData, &tResolved);
	tContent = __xuiProgressContentRect(pWidget);
	if ( (tContent.fW <= 0.0f) || (tContent.fH <= 0.0f) ) {
		return XUI_OK;
	}
	if ( pData->bHasTrackPatch ) {
		iRet = __xuiProgressDrawNinePatch(pProxy, pDraw, &pData->tTrackPatch, tContent);
	} else if ( __xuiProgressColorAlpha(tResolved.iTrackColor) != 0 ) {
		iRet = pProxy->drawRectFill(pProxy, pDraw, tContent, tResolved.iTrackColor);
	} else {
		iRet = XUI_OK;
	}
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	fRate = __xuiProgressRateData(pData);
	tFill = __xuiProgressFillRectFromData(pWidget, pData, tResolved.iFillDirection);
	bFillVisible = (tFill.fW > 0.0f) && (tFill.fH > 0.0f);
	if ( pData->bHasFillPatch && bFillVisible ) {
		if ( (tResolved.iFillPatchMode == XUI_PROGRESS_FILL_REVEAL) && __xuiProgressPatchIsRevealSimple(&pData->tFillPatch) ) {
			iRet = __xuiProgressDrawRevealPatch(pProxy, pDraw, &pData->tFillPatch, tFill, tResolved.iFillDirection, fRate);
		} else {
			iRet = __xuiProgressDrawNinePatch(pProxy, pDraw, &pData->tFillPatch, tFill);
		}
	} else if ( bFillVisible && (__xuiProgressColorAlpha(tResolved.iFillColor) != 0) ) {
		iRet = pProxy->drawRectFill(pProxy, pDraw, tFill, tResolved.iFillColor);
	}
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( (tResolved.pFont != NULL) && (pProxy->drawText != NULL) &&
	     (pData->sDisplayText != NULL) && (pData->sDisplayText[0] != '\0') &&
	     (__xuiProgressColorAlpha(tResolved.iTextColor) != 0) ) {
		iRet = pProxy->drawText(pProxy, pDraw, tResolved.pFont, pData->sDisplayText, tContent, tResolved.iTextColor, tResolved.iTextFlags);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	if ( (tResolved.pFont != NULL) && (pProxy->drawText != NULL) &&
	     (pData->sDisplayText != NULL) && (pData->sDisplayText[0] != '\0') &&
	     bFillVisible && (__xuiProgressColorAlpha(tResolved.iFillTextColor) != 0) &&
	     (pData->bHasFillPatch || (__xuiProgressColorAlpha(tResolved.iFillColor) != 0)) ) {
		xui_rect_t tTextRect;
		tTextRect = __xuiProgressTextVisualRect(pProxy, tResolved.pFont, pData->sDisplayText, tContent, tResolved.iTextFlags);
		if ( __xuiProgressRectContains(tFill, tTextRect) ) {
			iRet = pProxy->drawText(pProxy, pDraw, tResolved.pFont, pData->sDisplayText, tContent, tResolved.iFillTextColor, tResolved.iTextFlags);
			if ( iRet != XUI_OK ) {
				return iRet;
			}
		}
	}
	return XUI_OK;
}

static int __xuiProgressInit(xui_widget pWidget, void* pTypeData, const void* pCreateData, void* pUser)
{
	xui_progress_data_t* pData;
	const xui_progress_desc_t* pDesc;
	xui_theme_t tTheme;
	xui_context pContext;
	float fMin;
	float fMax;
	int iRet;

	(void)pUser;
	pData = (xui_progress_data_t*)pTypeData;
	pDesc = (const xui_progress_desc_t*)pCreateData;
	if ( (pWidget == NULL) || (pData == NULL) || !__xuiProgressDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pContext = xuiWidgetGetContext(pWidget);
	memset(&tTheme, 0, sizeof(tTheme));
	tTheme.iSize = sizeof(tTheme);
	(void)xuiGetTheme(pContext, &tTheme);
	fMin = (pDesc != NULL) ? pDesc->fMin : 0.0f;
	fMax = (pDesc != NULL) ? pDesc->fMax : 1.0f;
	if ( (pDesc == NULL) || ((fMin == 0.0f) && (fMax == 0.0f)) ) {
		fMin = 0.0f;
		fMax = 1.0f;
	}
	__xuiProgressNormalizeRange(&fMin, &fMax);
	pData->pFont = (pDesc != NULL && pDesc->pFont != NULL) ? pDesc->pFont : xuiGetDefaultFont(pContext);
	pData->fMin = fMin;
	pData->fMax = fMax;
	pData->fValue = __xuiProgressClampFloat((pDesc != NULL) ? pDesc->fValue : 0.0f, pData->fMin, pData->fMax);
	pData->iTrackColor = (pDesc != NULL && pDesc->iTrackColor != 0) ? pDesc->iTrackColor : XUI_COLOR_RGBA(216, 236, 248, 255);
	pData->iFillColor = (pDesc != NULL && pDesc->iFillColor != 0) ? pDesc->iFillColor : XUI_COLOR_RGBA(46, 124, 214, 255);
	pData->iTextColor = (pDesc != NULL && pDesc->iTextColor != 0) ? pDesc->iTextColor : ((tTheme.iTextColor != 0) ? tTheme.iTextColor : XUI_COLOR_RGBA(36, 52, 70, 255));
	pData->iFillTextColor = (pDesc != NULL && pDesc->iFillTextColor != 0) ? pDesc->iFillTextColor : XUI_COLOR_WHITE;
	pData->iTextFlags = ((pDesc != NULL && pDesc->iTextFlags != 0) ? pDesc->iTextFlags : (XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_CLIP)) | XUI_TEXT_CLIP;
	pData->iFillDirection = (pDesc != NULL && pDesc->iFillDirection != 0) ? pDesc->iFillDirection : XUI_PROGRESS_LEFT_TO_RIGHT;
	pData->iFillPatchMode = (pDesc != NULL && pDesc->iFillPatchMode != 0) ? pDesc->iFillPatchMode : XUI_PROGRESS_FILL_STRETCH;
	if ( (pDesc != NULL) && pDesc->bHasTrackPatch ) {
		pData->tTrackPatch = pDesc->tTrackPatch;
		__xuiProgressNormalizePatch(&pData->tTrackPatch);
		pData->bHasTrackPatch = 1;
	}
	if ( (pDesc != NULL) && pDesc->bHasFillPatch ) {
		pData->tFillPatch = pDesc->tFillPatch;
		__xuiProgressNormalizePatch(&pData->tFillPatch);
		pData->bHasFillPatch = 1;
	}
	iRet = __xuiProgressSetTextTemplateData(pData, (pDesc != NULL) ? pDesc->sText : NULL);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	(void)xuiWidgetSetFocusable(pWidget, 0);
	(void)xuiWidgetSetTabStop(pWidget, 0);
	return XUI_OK;
}

static void __xuiProgressDestroy(xui_widget pWidget, void* pTypeData, void* pUser)
{
	xui_progress_data_t* pData;

	(void)pWidget;
	(void)pUser;
	pData = (xui_progress_data_t*)pTypeData;
	if ( pData == NULL ) {
		return;
	}
	if ( pData->sTextTemplate != NULL ) {
		xrtFree(pData->sTextTemplate);
	}
	if ( pData->sDisplayText != NULL ) {
		xrtFree(pData->sDisplayText);
	}
	memset(pData, 0, sizeof(*pData));
}

static void __xuiProgressDefaultLayout(xui_layout_t* pLayout)
{
	memset(pLayout, 0, sizeof(*pLayout));
	pLayout->iLayoutType = XUI_LAYOUT_MANUAL;
	pLayout->iWidthMode = XUI_SIZE_CONTENT;
	pLayout->iHeightMode = XUI_SIZE_CONTENT;
	pLayout->iFlowMode = XUI_FLOW_BLOCK;
	pLayout->iDock = XUI_DOCK_FILL;
	pLayout->iOverflow = XUI_OVERFLOW_VISIBLE;
	pLayout->iAlignX = XUI_ALIGN_START;
	pLayout->iAlignY = XUI_ALIGN_START;
	pLayout->iTableRowSpan = 1;
	pLayout->iTableColumnSpan = 1;
	pLayout->iGridColumnCount = 1;
	pLayout->fMaxWidth = XUI_LAYOUT_UNBOUNDED;
	pLayout->fMaxHeight = XUI_LAYOUT_UNBOUNDED;
	pLayout->fShrink = 1.0f;
}

static void __xuiProgressDefaultCachePolicy(xui_cache_policy_t* pPolicy)
{
	memset(pPolicy, 0, sizeof(*pPolicy));
	pPolicy->iSize = sizeof(*pPolicy);
	pPolicy->iPolicy = XUI_CACHE_POLICY_SELF;
	pPolicy->iFlags = XUI_CACHE_CLEAR_ON_UPDATE;
	pPolicy->iClearColor = XUI_COLOR_RGBA(0, 0, 0, 0);
}

static void __xuiProgressRegisterStyleProperty(xui_context pContext, xui_widget_type pType, const char* sName, int iValueType, uint32_t iDirtyFlags, uint32_t iFlags)
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

static void __xuiProgressRegisterStyleProperties(xui_context pContext, xui_widget_type pType)
{
	__xuiProgressRegisterStyleProperty(pContext, pType, "text.color", XUI_STYLE_VALUE_COLOR, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER, XUI_STYLE_PROPERTY_INHERITED);
	__xuiProgressRegisterStyleProperty(pContext, pType, "text.flags", XUI_STYLE_VALUE_INT, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER, XUI_STYLE_PROPERTY_INHERITED);
	__xuiProgressRegisterStyleProperty(pContext, pType, "font.name", XUI_STYLE_VALUE_STRING, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER, XUI_STYLE_PROPERTY_INHERITED);
	__xuiProgressRegisterStyleProperty(pContext, pType, "progress.track_color", XUI_STYLE_VALUE_COLOR, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER, 0);
	__xuiProgressRegisterStyleProperty(pContext, pType, "progress.fill_color", XUI_STYLE_VALUE_COLOR, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER, 0);
	__xuiProgressRegisterStyleProperty(pContext, pType, "progress.text_color", XUI_STYLE_VALUE_COLOR, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER, 0);
	__xuiProgressRegisterStyleProperty(pContext, pType, "progress.fill_text_color", XUI_STYLE_VALUE_COLOR, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER, 0);
	__xuiProgressRegisterStyleProperty(pContext, pType, "progress.text_flags", XUI_STYLE_VALUE_INT, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER, 0);
	__xuiProgressRegisterStyleProperty(pContext, pType, "progress.fill_direction", XUI_STYLE_VALUE_INT, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER, 0);
	__xuiProgressRegisterStyleProperty(pContext, pType, "progress.fill_patch_mode", XUI_STYLE_VALUE_INT, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER, 0);
}

XUI_API xui_widget_type xuiProgressGetType(xui_context pContext)
{
	xui_widget_type_desc_t tDesc;
	xui_widget_type pType;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) ) {
		return NULL;
	}
	pType = xuiWidgetFindType(pContext, "progress");
	if ( pType != NULL ) {
		__xuiProgressRegisterStyleProperties(pContext, pType);
		return pType;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.sName = "progress";
	tDesc.pParent = xuiWidgetGetBaseType();
	tDesc.iFlags = XUI_WIDGET_TYPE_DEFAULT_LAYOUT | XUI_WIDGET_TYPE_DEFAULT_CACHE_POLICY;
	tDesc.iTypeDataSize = sizeof(xui_progress_data_t);
	tDesc.onInit = __xuiProgressInit;
	tDesc.onDestroy = __xuiProgressDestroy;
	tDesc.onContentMeasure = __xuiProgressContentMeasure;
	tDesc.onCacheRender = __xuiProgressCacheRender;
	__xuiProgressDefaultLayout(&tDesc.tLayout);
	__xuiProgressDefaultCachePolicy(&tDesc.tCachePolicy);
	iRet = xuiWidgetRegisterType(pContext, &pType, &tDesc);
	if ( iRet != XUI_OK ) {
		return NULL;
	}
	__xuiProgressRegisterStyleProperties(pContext, pType);
	return pType;
}

XUI_API int xuiProgressCreate(xui_context pContext, xui_widget* ppWidget, const xui_progress_desc_t* pDesc)
{
	xui_widget_type pType;

	if ( (ppWidget == NULL) || !__xuiProgressDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppWidget = NULL;
	pType = xuiProgressGetType(pContext);
	if ( pType == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return xuiWidgetCreateTyped(pContext, pType, ppWidget, pDesc);
}

XUI_API int xuiProgressSetRange(xui_widget pWidget, float fMin, float fMax)
{
	xui_progress_data_t* pData;
	int iRet;

	pData = __xuiProgressGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	__xuiProgressNormalizeRange(&fMin, &fMax);
	pData->fMin = fMin;
	pData->fMax = fMax;
	pData->fValue = __xuiProgressClampFloat(pData->fValue, pData->fMin, pData->fMax);
	iRet = __xuiProgressSetDisplayText(pData);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiProgressGetRange(xui_widget pWidget, float* pMin, float* pMax)
{
	xui_progress_data_t* pData;

	pData = __xuiProgressGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pMin != NULL ) {
		*pMin = pData->fMin;
	}
	if ( pMax != NULL ) {
		*pMax = pData->fMax;
	}
	return XUI_OK;
}

XUI_API int xuiProgressSetValue(xui_widget pWidget, float fValue)
{
	xui_progress_data_t* pData;
	int iRet;

	pData = __xuiProgressGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	fValue = __xuiProgressClampFloat(fValue, pData->fMin, pData->fMax);
	if ( pData->fValue == fValue ) {
		return XUI_OK;
	}
	pData->fValue = fValue;
	iRet = __xuiProgressSetDisplayText(pData);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API float xuiProgressGetValue(xui_widget pWidget)
{
	xui_progress_data_t* pData;

	pData = __xuiProgressGetData(pWidget);
	return (pData != NULL) ? pData->fValue : 0.0f;
}

XUI_API float xuiProgressGetRate(xui_widget pWidget)
{
	xui_progress_data_t* pData;

	pData = __xuiProgressGetData(pWidget);
	return __xuiProgressRateData(pData);
}

XUI_API int xuiProgressSetText(xui_widget pWidget, xui_font pFont, const char* sText)
{
	int iRet;

	iRet = xuiProgressSetFont(pWidget, pFont);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return xuiProgressSetTextTemplate(pWidget, sText);
}

XUI_API int xuiProgressSetTextTemplate(xui_widget pWidget, const char* sTextTemplate)
{
	xui_progress_data_t* pData;
	int iRet;

	pData = __xuiProgressGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( ((pData->sTextTemplate == NULL) && (sTextTemplate == NULL)) ||
	     ((pData->sTextTemplate != NULL) && (sTextTemplate != NULL) && (strcmp(pData->sTextTemplate, sTextTemplate) == 0)) ) {
		return XUI_OK;
	}
	iRet = __xuiProgressSetTextTemplateData(pData, sTextTemplate);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API const char* xuiProgressGetTextTemplate(xui_widget pWidget)
{
	xui_progress_data_t* pData;

	pData = __xuiProgressGetData(pWidget);
	return (pData != NULL) ? pData->sTextTemplate : NULL;
}

XUI_API const char* xuiProgressGetDisplayText(xui_widget pWidget)
{
	xui_progress_data_t* pData;

	pData = __xuiProgressGetData(pWidget);
	return (pData != NULL) ? pData->sDisplayText : NULL;
}

XUI_API int xuiProgressSetFont(xui_widget pWidget, xui_font pFont)
{
	xui_progress_data_t* pData;

	pData = __xuiProgressGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->pFont == pFont ) {
		return XUI_OK;
	}
	pData->pFont = pFont;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API xui_font xuiProgressGetFont(xui_widget pWidget)
{
	xui_progress_data_t* pData;

	pData = __xuiProgressGetData(pWidget);
	return (pData != NULL) ? pData->pFont : NULL;
}

XUI_API int xuiProgressSetTextColor(xui_widget pWidget, uint32_t iColor)
{
	xui_progress_data_t* pData;

	pData = __xuiProgressGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->iTextColor == iColor ) {
		return XUI_OK;
	}
	pData->iTextColor = iColor;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiProgressGetTextColor(xui_widget pWidget)
{
	xui_progress_data_t* pData;

	pData = __xuiProgressGetData(pWidget);
	return (pData != NULL) ? pData->iTextColor : 0;
}

XUI_API int xuiProgressSetFillTextColor(xui_widget pWidget, uint32_t iColor)
{
	xui_progress_data_t* pData;

	pData = __xuiProgressGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->iFillTextColor == iColor ) {
		return XUI_OK;
	}
	pData->iFillTextColor = iColor;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiProgressGetFillTextColor(xui_widget pWidget)
{
	xui_progress_data_t* pData;

	pData = __xuiProgressGetData(pWidget);
	return (pData != NULL) ? pData->iFillTextColor : 0;
}

XUI_API int xuiProgressSetTextFlags(xui_widget pWidget, uint32_t iTextFlags)
{
	xui_progress_data_t* pData;

	pData = __xuiProgressGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	iTextFlags |= XUI_TEXT_CLIP;
	if ( pData->iTextFlags == iTextFlags ) {
		return XUI_OK;
	}
	pData->iTextFlags = iTextFlags;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiProgressGetTextFlags(xui_widget pWidget)
{
	xui_progress_data_t* pData;

	pData = __xuiProgressGetData(pWidget);
	return (pData != NULL) ? pData->iTextFlags : 0;
}

XUI_API int xuiProgressSetColors(xui_widget pWidget, uint32_t iTrack, uint32_t iFill)
{
	xui_progress_data_t* pData;

	pData = __xuiProgressGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( (pData->iTrackColor == iTrack) && (pData->iFillColor == iFill) ) {
		return XUI_OK;
	}
	pData->iTrackColor = iTrack;
	pData->iFillColor = iFill;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API uint32_t xuiProgressGetTrackColor(xui_widget pWidget)
{
	xui_progress_data_t* pData;

	pData = __xuiProgressGetData(pWidget);
	return (pData != NULL) ? pData->iTrackColor : 0;
}

XUI_API uint32_t xuiProgressGetFillColor(xui_widget pWidget)
{
	xui_progress_data_t* pData;

	pData = __xuiProgressGetData(pWidget);
	return (pData != NULL) ? pData->iFillColor : 0;
}

XUI_API int xuiProgressSetFillDirection(xui_widget pWidget, int iFillDirection)
{
	xui_progress_data_t* pData;

	if ( !__xuiProgressDirectionValid(iFillDirection) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiProgressGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->iFillDirection == iFillDirection ) {
		return XUI_OK;
	}
	pData->iFillDirection = iFillDirection;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_LAYOUT | XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiProgressGetFillDirection(xui_widget pWidget)
{
	xui_progress_data_t* pData;

	pData = __xuiProgressGetData(pWidget);
	return (pData != NULL) ? pData->iFillDirection : XUI_PROGRESS_LEFT_TO_RIGHT;
}

XUI_API int xuiProgressSetTrackPatch(xui_widget pWidget, const xui_nine_patch_t* pPatch)
{
	xui_progress_data_t* pData;

	pData = __xuiProgressGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pPatch != NULL ) {
		pData->tTrackPatch = *pPatch;
		__xuiProgressNormalizePatch(&pData->tTrackPatch);
		pData->bHasTrackPatch = 1;
	} else {
		memset(&pData->tTrackPatch, 0, sizeof(pData->tTrackPatch));
		pData->bHasTrackPatch = 0;
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiProgressClearTrackPatch(xui_widget pWidget)
{
	return xuiProgressSetTrackPatch(pWidget, NULL);
}

XUI_API int xuiProgressHasTrackPatch(xui_widget pWidget)
{
	xui_progress_data_t* pData;

	pData = __xuiProgressGetData(pWidget);
	return (pData != NULL) ? pData->bHasTrackPatch : 0;
}

XUI_API int xuiProgressGetTrackPatch(xui_widget pWidget, xui_nine_patch_t* pPatch)
{
	xui_progress_data_t* pData;

	pData = __xuiProgressGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pPatch != NULL ) {
		if ( pData->bHasTrackPatch ) {
			*pPatch = pData->tTrackPatch;
		} else {
			memset(pPatch, 0, sizeof(*pPatch));
		}
	}
	return XUI_OK;
}

XUI_API int xuiProgressSetFillPatch(xui_widget pWidget, const xui_nine_patch_t* pPatch)
{
	xui_progress_data_t* pData;

	pData = __xuiProgressGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pPatch != NULL ) {
		pData->tFillPatch = *pPatch;
		__xuiProgressNormalizePatch(&pData->tFillPatch);
		pData->bHasFillPatch = 1;
	} else {
		memset(&pData->tFillPatch, 0, sizeof(pData->tFillPatch));
		pData->bHasFillPatch = 0;
	}
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiProgressClearFillPatch(xui_widget pWidget)
{
	return xuiProgressSetFillPatch(pWidget, NULL);
}

XUI_API int xuiProgressHasFillPatch(xui_widget pWidget)
{
	xui_progress_data_t* pData;

	pData = __xuiProgressGetData(pWidget);
	return (pData != NULL) ? pData->bHasFillPatch : 0;
}

XUI_API int xuiProgressGetFillPatch(xui_widget pWidget, xui_nine_patch_t* pPatch)
{
	xui_progress_data_t* pData;

	pData = __xuiProgressGetData(pWidget);
	if ( pData == NULL ) return XUI_ERROR_INVALID_ARGUMENT;
	if ( pPatch != NULL ) {
		if ( pData->bHasFillPatch ) {
			*pPatch = pData->tFillPatch;
		} else {
			memset(pPatch, 0, sizeof(*pPatch));
		}
	}
	return XUI_OK;
}

XUI_API int xuiProgressSetFillPatchMode(xui_widget pWidget, int iMode)
{
	xui_progress_data_t* pData;

	if ( !__xuiProgressPatchModeValid(iMode) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pData = __xuiProgressGetData(pWidget);
	if ( pData == NULL ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( pData->iFillPatchMode == iMode ) {
		return XUI_OK;
	}
	pData->iFillPatchMode = iMode;
	return xuiWidgetInvalidate(pWidget, XUI_WIDGET_DIRTY_CACHE | XUI_WIDGET_DIRTY_RENDER);
}

XUI_API int xuiProgressGetFillPatchMode(xui_widget pWidget)
{
	xui_progress_data_t* pData;

	pData = __xuiProgressGetData(pWidget);
	return (pData != NULL) ? pData->iFillPatchMode : XUI_PROGRESS_FILL_STRETCH;
}

XUI_API xui_rect_t xuiProgressGetFillRect(xui_widget pWidget)
{
	xui_progress_data_t* pData;
	xui_progress_resolved_t tResolved;

	pData = __xuiProgressGetData(pWidget);
	if ( pData == NULL ) {
		return (xui_rect_t){0.0f, 0.0f, 0.0f, 0.0f};
	}
	__xuiProgressResolve(pWidget, pData, &tResolved);
	return __xuiProgressFillRectFromData(pWidget, pData, tResolved.iFillDirection);
}
