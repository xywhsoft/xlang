#include "xui_internal.h"

#include <string.h>

#define XUI_TEXT_LAYOUT_MAGIC 0x58554954u

struct xui_text_layout_t {
	uint32_t iMagic;
	xui_context pContext;
	xui_text_layout_desc_t tDesc;
	xui_font_metrics_t tMetrics;
	char* sText;
	int iTextSize;
	xui_text_line_t* pLines;
	int iLineCount;
	int iLineCapacity;
	xui_vec2_t tSize;
	int bTruncated;
	float fNextY;
	char* pScratch;
	int iScratchCapacity;
};

static int __xuiTextLayoutValid(xui_text_layout pLayout)
{
	return (pLayout != NULL) && (pLayout->iMagic == XUI_TEXT_LAYOUT_MAGIC);
}

static int __xuiTextFloatValid(float fValue)
{
	return (fValue == fValue) && (fValue >= 0.0f) && (fValue <= XUI_LAYOUT_UNBOUNDED);
}

static int __xuiTextWrapModeValid(int iWrapMode)
{
	return (iWrapMode == XUI_TEXT_WRAP_NONE) ||
	       (iWrapMode == XUI_TEXT_WRAP_WORD) ||
	       (iWrapMode == XUI_TEXT_WRAP_CHAR);
}

static int __xuiTextMaxFinite(float fValue)
{
	return (fValue > 0.0f) && (fValue < XUI_LAYOUT_UNBOUNDED);
}

static float __xuiTextMaxFloat(float fA, float fB)
{
	return (fA > fB) ? fA : fB;
}

static int __xuiTextDescValid(const xui_text_layout_desc_t* pDesc)
{
	if ( pDesc == NULL ) {
		return 0;
	}
	if ( pDesc->iSize < sizeof(*pDesc) ) {
		return 0;
	}
	if ( (pDesc->sText == NULL) || (pDesc->pFont == NULL) || (pDesc->iTextSize < -1) ) {
		return 0;
	}
	if ( !__xuiTextWrapModeValid(pDesc->iWrapMode) ||
	     !__xuiTextFloatValid(pDesc->fMaxWidth) ||
	     !__xuiTextFloatValid(pDesc->fMaxHeight) ||
	     !__xuiTextFloatValid(pDesc->fLineGap) ||
	     !__xuiTextFloatValid(pDesc->fParagraphGap) ) {
		return 0;
	}
	return 1;
}

static void __xuiTextLayoutClear(xui_text_layout pLayout)
{
	if ( pLayout->sText != NULL ) {
		xrtFree(pLayout->sText);
		pLayout->sText = NULL;
	}
	if ( pLayout->pLines != NULL ) {
		xrtFree(pLayout->pLines);
		pLayout->pLines = NULL;
	}
	pLayout->iTextSize = 0;
	pLayout->iLineCount = 0;
	pLayout->iLineCapacity = 0;
	pLayout->tSize.fX = 0.0f;
	pLayout->tSize.fY = 0.0f;
	pLayout->bTruncated = 0;
	pLayout->fNextY = 0.0f;
}

static int __xuiTextLineReserve(xui_text_layout pLayout, int iCapacity)
{
	xui_text_line_t* pLines;

	if ( iCapacity <= pLayout->iLineCapacity ) {
		return XUI_OK;
	}
	if ( iCapacity < (pLayout->iLineCapacity * 2) ) {
		iCapacity = pLayout->iLineCapacity * 2;
	}
	if ( iCapacity < 8 ) {
		iCapacity = 8;
	}
	pLines = (xui_text_line_t*)xrtRealloc(pLayout->pLines, sizeof(*pLines) * (size_t)iCapacity);
	if ( pLines == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pLayout->pLines = pLines;
	pLayout->iLineCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiTextScratchReserve(xui_text_layout pLayout, int iCapacity)
{
	char* pScratch;

	if ( iCapacity <= pLayout->iScratchCapacity ) {
		return XUI_OK;
	}
	pScratch = (char*)xrtRealloc(pLayout->pScratch, (size_t)iCapacity);
	if ( pScratch == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pLayout->pScratch = pScratch;
	pLayout->iScratchCapacity = iCapacity;
	return XUI_OK;
}

static int __xuiTextMeasureRange(xui_text_layout pLayout, const char* sStart, const char* sEnd, xui_vec2_t* pSize)
{
	xui_proxy pProxy;
	int iSize;
	int iRet;

	pSize->fX = 0.0f;
	pSize->fY = pLayout->tMetrics.fLineHeight;
	if ( sEnd <= sStart ) {
		return XUI_OK;
	}
	iSize = (int)(sEnd - sStart);
	iRet = __xuiTextScratchReserve(pLayout, iSize + 1);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	memcpy(pLayout->pScratch, sStart, (size_t)iSize);
	pLayout->pScratch[iSize] = 0;
	pProxy = xuiInternalContextGetProxy(pLayout->pContext);
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	return pProxy->textMeasure(pProxy, pLayout->tDesc.pFont, pLayout->pScratch, pSize);
}

static const char* __xuiTextPrevUtf8(const char* sBegin, const char* sAt)
{
	const unsigned char* p;

	if ( sAt <= sBegin ) {
		return sBegin;
	}
	p = (const unsigned char*)sAt - 1;
	while ( ((const char*)p > sBegin) && ((*p & 0xC0u) == 0x80u) ) {
		p--;
	}
	return (const char*)p;
}

static int __xuiTextAsciiSpaceAt(const char* sAt, const char* sEnd)
{
	if ( sAt >= sEnd ) {
		return 0;
	}
	return (*sAt == ' ') || (*sAt == '\t');
}

static const char* __xuiTextTrimEndSpaces(const char* sBegin, const char* sEnd)
{
	const char* sPrev;

	while ( sEnd > sBegin ) {
		sPrev = __xuiTextPrevUtf8(sBegin, sEnd);
		if ( !__xuiTextAsciiSpaceAt(sPrev, sEnd) ) {
			break;
		}
		sEnd = sPrev;
	}
	return sEnd;
}

static const char* __xuiTextSkipSpaces(const char* sAt, const char* sEnd)
{
	while ( (sAt < sEnd) && __xuiTextAsciiSpaceAt(sAt, sEnd) ) {
		sAt++;
	}
	return sAt;
}

static int __xuiTextUtf8Next(const char** psScan, const char* sEnd, uint32_t* pCodepoint)
{
	const unsigned char* pText;
	uint32_t iCodepoint;

	if ( (psScan == NULL) || (*psScan == NULL) || (pCodepoint == NULL) || (*psScan >= sEnd) ) {
		return 0;
	}
	pText = (const unsigned char*)*psScan;
	if ( pText[0] < 0x80u ) {
		*pCodepoint = (uint32_t)pText[0];
		*psScan += 1;
		return 1;
	}
	if ( ((pText[0] & 0xE0u) == 0xC0u) && ((*psScan + 1) < sEnd) && ((pText[1] & 0xC0u) == 0x80u) ) {
		iCodepoint = ((uint32_t)(pText[0] & 0x1Fu) << 6) | (uint32_t)(pText[1] & 0x3Fu);
		if ( iCodepoint >= 0x80u ) {
			*pCodepoint = iCodepoint;
			*psScan += 2;
			return 1;
		}
	}
	if ( ((pText[0] & 0xF0u) == 0xE0u) && ((*psScan + 2) < sEnd) &&
	     ((pText[1] & 0xC0u) == 0x80u) && ((pText[2] & 0xC0u) == 0x80u) ) {
		iCodepoint = ((uint32_t)(pText[0] & 0x0Fu) << 12) |
		             ((uint32_t)(pText[1] & 0x3Fu) << 6) |
		             (uint32_t)(pText[2] & 0x3Fu);
		if ( (iCodepoint >= 0x800u) && !((iCodepoint >= 0xD800u) && (iCodepoint <= 0xDFFFu)) ) {
			*pCodepoint = iCodepoint;
			*psScan += 3;
			return 1;
		}
	}
	if ( ((pText[0] & 0xF8u) == 0xF0u) && ((*psScan + 3) < sEnd) &&
	     ((pText[1] & 0xC0u) == 0x80u) && ((pText[2] & 0xC0u) == 0x80u) && ((pText[3] & 0xC0u) == 0x80u) ) {
		iCodepoint = ((uint32_t)(pText[0] & 0x07u) << 18) |
		             ((uint32_t)(pText[1] & 0x3Fu) << 12) |
		             ((uint32_t)(pText[2] & 0x3Fu) << 6) |
		             (uint32_t)(pText[3] & 0x3Fu);
		if ( (iCodepoint >= 0x10000u) && (iCodepoint <= 0x10FFFFu) ) {
			*pCodepoint = iCodepoint;
			*psScan += 4;
			return 1;
		}
	}
	*pCodepoint = 0xFFFDu;
	*psScan += 1;
	return 1;
}

static const char* __xuiTextFindNewline(const char* sStart, const char* sEnd, const char** psAfter)
{
	const char* sScan;

	for ( sScan = sStart; sScan < sEnd; sScan++ ) {
		if ( *sScan == '\n' ) {
			*psAfter = sScan + 1;
			return sScan;
		}
		if ( *sScan == '\r' ) {
			if ( ((sScan + 1) < sEnd) && (sScan[1] == '\n') ) {
				*psAfter = sScan + 2;
			} else {
				*psAfter = sScan + 1;
			}
			return sScan;
		}
	}
	*psAfter = sEnd;
	return sEnd;
}

static int __xuiTextAddLine(xui_text_layout pLayout, const char* sStart, const char* sEnd, int iBreakType)
{
	xui_text_line_t tLine;
	xui_vec2_t tMeasure;
	float fLineBottom;
	int iRet;

	if ( pLayout->bTruncated ) {
		return XUI_OK;
	}
	if ( sEnd < sStart ) {
		sEnd = sStart;
	}
	fLineBottom = pLayout->fNextY + pLayout->tMetrics.fLineHeight;
	if ( __xuiTextMaxFinite(pLayout->tDesc.fMaxHeight) && (fLineBottom > pLayout->tDesc.fMaxHeight) ) {
		pLayout->bTruncated = 1;
		return XUI_OK;
	}
	iRet = __xuiTextMeasureRange(pLayout, sStart, sEnd, &tMeasure);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	iRet = __xuiTextLineReserve(pLayout, pLayout->iLineCount + 1);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	memset(&tLine, 0, sizeof(tLine));
	tLine.iSize = sizeof(tLine);
	tLine.iTextOffset = (int)(sStart - pLayout->sText);
	tLine.iTextSize = (int)(sEnd - sStart);
	tLine.iBreakType = iBreakType;
	tLine.fY = pLayout->fNextY;
	tLine.fW = tMeasure.fX;
	tLine.fH = pLayout->tMetrics.fLineHeight;
	tLine.fBaseline = pLayout->tMetrics.fAscent;
	pLayout->pLines[pLayout->iLineCount++] = tLine;
	pLayout->tSize.fX = __xuiTextMaxFloat(pLayout->tSize.fX, tLine.fW);
	pLayout->tSize.fY = tLine.fY + tLine.fH;
	pLayout->fNextY = pLayout->tSize.fY + ((iBreakType == XUI_TEXT_BREAK_NEWLINE) ? pLayout->tDesc.fParagraphGap : pLayout->tDesc.fLineGap);
	return XUI_OK;
}

static int __xuiTextLayoutNoWrapParagraph(xui_text_layout pLayout, const char* sStart, const char* sEnd, int iBreakType)
{
	return __xuiTextAddLine(pLayout, sStart, __xuiTextTrimEndSpaces(sStart, sEnd), iBreakType);
}

static int __xuiTextLayoutWrappedParagraph(xui_text_layout pLayout, const char* sStart, const char* sEnd, int iBreakType)
{
	const char* sLineStart;
	const char* sScan;
	const char* sNext;
	const char* sMeasureEnd;
	const char* sLastBreakEnd;
	const char* sLastBreakNext;
	xui_vec2_t tMeasure;
	uint32_t iCodepoint;
	int iRet;

	if ( sStart >= sEnd ) {
		return __xuiTextAddLine(pLayout, sStart, sStart, iBreakType);
	}
	if ( !__xuiTextMaxFinite(pLayout->tDesc.fMaxWidth) ) {
		return __xuiTextLayoutNoWrapParagraph(pLayout, sStart, sEnd, iBreakType);
	}
	sLineStart = sStart;
	sScan = sStart;
	sLastBreakEnd = NULL;
	sLastBreakNext = NULL;
	while ( sScan < sEnd ) {
		sNext = sScan;
		if ( !__xuiTextUtf8Next(&sNext, sEnd, &iCodepoint) ) {
			break;
		}
		sMeasureEnd = __xuiTextTrimEndSpaces(sLineStart, sNext);
		iRet = __xuiTextMeasureRange(pLayout, sLineStart, sMeasureEnd, &tMeasure);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		if ( (tMeasure.fX > pLayout->tDesc.fMaxWidth) && (sLineStart < sScan) ) {
			if ( (pLayout->tDesc.iWrapMode == XUI_TEXT_WRAP_WORD) &&
			     (sLastBreakEnd != NULL) && (sLastBreakNext != NULL) &&
			     (sLastBreakNext > sLineStart) ) {
				iRet = __xuiTextAddLine(pLayout, sLineStart, sLastBreakEnd, XUI_TEXT_BREAK_WRAP);
				if ( iRet != XUI_OK ) {
					return iRet;
				}
				sLineStart = sLastBreakNext;
			} else {
				iRet = __xuiTextAddLine(pLayout, sLineStart, __xuiTextTrimEndSpaces(sLineStart, sScan), XUI_TEXT_BREAK_WRAP);
				if ( iRet != XUI_OK ) {
					return iRet;
				}
				sLineStart = sScan;
			}
			if ( pLayout->bTruncated ) {
				return XUI_OK;
			}
			sScan = sLineStart;
			sLastBreakEnd = NULL;
			sLastBreakNext = NULL;
			continue;
		}
		if ( (tMeasure.fX > pLayout->tDesc.fMaxWidth) && (sLineStart == sScan) ) {
			iRet = __xuiTextAddLine(pLayout, sLineStart, sNext, XUI_TEXT_BREAK_WRAP);
			if ( iRet != XUI_OK ) {
				return iRet;
			}
			if ( pLayout->bTruncated ) {
				return XUI_OK;
			}
			sLineStart = sNext;
			sScan = sLineStart;
			sLastBreakEnd = NULL;
			sLastBreakNext = NULL;
			continue;
		}
		if ( __xuiTextAsciiSpaceAt(sScan, sEnd) ) {
			sLastBreakEnd = __xuiTextTrimEndSpaces(sLineStart, sScan);
			sLastBreakNext = __xuiTextSkipSpaces(sNext, sEnd);
		}
		sScan = sNext;
	}
	if ( !pLayout->bTruncated && (sLineStart <= sEnd) ) {
		return __xuiTextAddLine(pLayout, sLineStart, __xuiTextTrimEndSpaces(sLineStart, sEnd), iBreakType);
	}
	return XUI_OK;
}

static int __xuiTextLayoutParagraph(xui_text_layout pLayout, const char* sStart, const char* sEnd, int iBreakType)
{
	if ( pLayout->tDesc.iWrapMode == XUI_TEXT_WRAP_NONE ) {
		return __xuiTextLayoutNoWrapParagraph(pLayout, sStart, sEnd, iBreakType);
	}
	return __xuiTextLayoutWrappedParagraph(pLayout, sStart, sEnd, iBreakType);
}

static void __xuiTextLayoutAlignLines(xui_text_layout pLayout)
{
	uint32_t iHorizontal;
	int i;

	iHorizontal = pLayout->tDesc.iFlags & (XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_RIGHT);
	for ( i = 0; i < pLayout->iLineCount; i++ ) {
		if ( iHorizontal == XUI_TEXT_ALIGN_RIGHT ) {
			pLayout->pLines[i].fX = pLayout->tSize.fX - pLayout->pLines[i].fW;
		} else if ( iHorizontal == XUI_TEXT_ALIGN_CENTER ) {
			pLayout->pLines[i].fX = (pLayout->tSize.fX - pLayout->pLines[i].fW) * 0.5f;
		} else {
			pLayout->pLines[i].fX = 0.0f;
		}
	}
}

static int __xuiTextLayoutBuild(xui_text_layout pLayout)
{
	const char* sScan;
	const char* sEnd;
	const char* sParagraphEnd;
	const char* sAfterNewline;
	int iRet;
	int iBreakType;

	sScan = pLayout->sText;
	sEnd = pLayout->sText + pLayout->iTextSize;
	while ( sScan < sEnd ) {
		sParagraphEnd = __xuiTextFindNewline(sScan, sEnd, &sAfterNewline);
		iBreakType = (sParagraphEnd < sEnd) ? XUI_TEXT_BREAK_NEWLINE : XUI_TEXT_BREAK_END;
		iRet = __xuiTextLayoutParagraph(pLayout, sScan, sParagraphEnd, iBreakType);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		if ( pLayout->bTruncated ) {
			break;
		}
		sScan = sAfterNewline;
		if ( (sParagraphEnd < sEnd) && (sScan == sEnd) ) {
			iRet = __xuiTextAddLine(pLayout, sScan, sScan, XUI_TEXT_BREAK_END);
			if ( iRet != XUI_OK ) {
				return iRet;
			}
			break;
		}
	}
	__xuiTextLayoutAlignLines(pLayout);
	return XUI_OK;
}

static uint32_t __xuiTextMergeFlags(uint32_t iBaseFlags, uint32_t iOverrideFlags)
{
	uint32_t iHorizontal;
	uint32_t iVertical;
	uint32_t iFlags;

	iHorizontal = iOverrideFlags & (XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_RIGHT);
	if ( iHorizontal == 0 ) {
		iHorizontal = iBaseFlags & (XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_RIGHT);
	}
	iVertical = iOverrideFlags & (XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_ALIGN_BOTTOM);
	if ( iVertical == 0 ) {
		iVertical = iBaseFlags & (XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_ALIGN_BOTTOM);
	}
	iFlags = (iBaseFlags | iOverrideFlags) & ~(XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_RIGHT | XUI_TEXT_ALIGN_MIDDLE | XUI_TEXT_ALIGN_BOTTOM);
	return iFlags | iHorizontal | iVertical;
}

static float __xuiTextVerticalOffset(xui_text_layout pLayout, xui_rect_t tRect, uint32_t iFlags)
{
	if ( (iFlags & XUI_TEXT_ALIGN_BOTTOM) == XUI_TEXT_ALIGN_BOTTOM ) {
		return tRect.fH - pLayout->tSize.fY;
	}
	if ( (iFlags & XUI_TEXT_ALIGN_MIDDLE) == XUI_TEXT_ALIGN_MIDDLE ) {
		return (tRect.fH - pLayout->tSize.fY) * 0.5f;
	}
	return 0.0f;
}

XUI_API int xuiTextLayoutCreate(xui_context pContext, xui_text_layout* ppLayout, const xui_text_layout_desc_t* pDesc)
{
	xui_text_layout pLayout;
	int iRet;

	if ( !xuiInternalContextIsValid(pContext) || (ppLayout == NULL) || !__xuiTextDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*ppLayout = NULL;
	if ( xuiInternalContextGetProxy(pContext) == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	pLayout = (xui_text_layout)xrtCalloc(1, sizeof(*pLayout));
	if ( pLayout == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	pLayout->iMagic = XUI_TEXT_LAYOUT_MAGIC;
	pLayout->pContext = pContext;
	iRet = xuiTextLayoutReset(pLayout, pDesc);
	if ( iRet != XUI_OK ) {
		xuiTextLayoutDestroy(pLayout);
		return iRet;
	}
	*ppLayout = pLayout;
	return XUI_OK;
}

XUI_API void xuiTextLayoutDestroy(xui_text_layout pLayout)
{
	if ( !__xuiTextLayoutValid(pLayout) ) {
		return;
	}
	__xuiTextLayoutClear(pLayout);
	if ( pLayout->pScratch != NULL ) {
		xrtFree(pLayout->pScratch);
		pLayout->pScratch = NULL;
	}
	pLayout->iMagic = 0;
	xrtFree(pLayout);
}

XUI_API int xuiTextLayoutReset(xui_text_layout pLayout, const xui_text_layout_desc_t* pDesc)
{
	xui_proxy pProxy;
	int iTextSize;
	int iRet;

	if ( !__xuiTextLayoutValid(pLayout) || !__xuiTextDescValid(pDesc) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	if ( !xuiInternalContextIsValid(pLayout->pContext) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(pLayout->pContext);
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	iTextSize = pDesc->iTextSize;
	if ( iTextSize < 0 ) {
		iTextSize = (int)strlen(pDesc->sText);
	}
	__xuiTextLayoutClear(pLayout);
	pLayout->tDesc = *pDesc;
	pLayout->tDesc.iSize = sizeof(pLayout->tDesc);
	pLayout->iTextSize = iTextSize;
	pLayout->sText = (char*)xrtMalloc((size_t)iTextSize + 1u);
	if ( pLayout->sText == NULL ) {
		return XUI_ERROR_OUT_OF_MEMORY;
	}
	memcpy(pLayout->sText, pDesc->sText, (size_t)iTextSize);
	pLayout->sText[iTextSize] = 0;
	iRet = pProxy->fontGetMetrics(pProxy, pLayout->tDesc.pFont, &pLayout->tMetrics);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	if ( pLayout->tMetrics.fLineHeight <= 0.0f ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	return __xuiTextLayoutBuild(pLayout);
}

XUI_API xui_vec2_t xuiTextLayoutGetSize(xui_text_layout pLayout)
{
	xui_vec2_t tSize;

	tSize.fX = 0.0f;
	tSize.fY = 0.0f;
	if ( __xuiTextLayoutValid(pLayout) ) {
		tSize = pLayout->tSize;
	}
	return tSize;
}

XUI_API int xuiTextLayoutGetLineCount(xui_text_layout pLayout)
{
	return __xuiTextLayoutValid(pLayout) ? pLayout->iLineCount : 0;
}

XUI_API int xuiTextLayoutGetLine(xui_text_layout pLayout, int iIndex, xui_text_line_t* pLine)
{
	if ( !__xuiTextLayoutValid(pLayout) || (pLine == NULL) || (iIndex < 0) || (iIndex >= pLayout->iLineCount) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	*pLine = pLayout->pLines[iIndex];
	return XUI_OK;
}

XUI_API const char* xuiTextLayoutGetText(xui_text_layout pLayout)
{
	if ( !__xuiTextLayoutValid(pLayout) ) {
		return NULL;
	}
	return pLayout->sText;
}

XUI_API int xuiTextLayoutGetTruncated(xui_text_layout pLayout)
{
	return __xuiTextLayoutValid(pLayout) ? pLayout->bTruncated : 0;
}

XUI_API int xuiTextLayoutDraw(xui_text_layout pLayout, xui_surface pTarget, xui_rect_t tRect, uint32_t iColor, uint32_t iFlags)
{
	xui_proxy pProxy;
	xui_text_line_t* pLine;
	xui_rect_t tLineRect;
	uint32_t iMergedFlags;
	uint32_t iLineFlags;
	float fOffsetY;
	float fLineBottom;
	int i;
	int iRet;

	if ( !__xuiTextLayoutValid(pLayout) || (pTarget == NULL) || (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pProxy = xuiInternalContextGetProxy(pLayout->pContext);
	if ( pProxy == NULL ) {
		return XUI_ERROR_NOT_INITIALIZED;
	}
	iMergedFlags = __xuiTextMergeFlags(pLayout->tDesc.iFlags, iFlags);
	iLineFlags = (iMergedFlags & (XUI_TEXT_ALIGN_CENTER | XUI_TEXT_ALIGN_RIGHT | XUI_TEXT_CLIP | XUI_TEXT_UNDERLINE)) | XUI_TEXT_ALIGN_TOP;
	fOffsetY = __xuiTextVerticalOffset(pLayout, tRect, iMergedFlags);
	for ( i = 0; i < pLayout->iLineCount; i++ ) {
		pLine = &pLayout->pLines[i];
		if ( pLine->iTextSize <= 0 ) {
			continue;
		}
		iRet = __xuiTextScratchReserve(pLayout, pLine->iTextSize + 1);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
		memcpy(pLayout->pScratch, pLayout->sText + pLine->iTextOffset, (size_t)pLine->iTextSize);
		pLayout->pScratch[pLine->iTextSize] = 0;
		tLineRect.fX = tRect.fX;
		tLineRect.fY = tRect.fY + fOffsetY + pLine->fY;
		tLineRect.fW = tRect.fW;
		tLineRect.fH = pLine->fH;
		if ( (iMergedFlags & XUI_TEXT_CLIP) != 0 ) {
			fLineBottom = tLineRect.fY + tLineRect.fH;
			if ( (fLineBottom <= tRect.fY) || (tLineRect.fY >= (tRect.fY + tRect.fH)) ) {
				continue;
			}
			if ( tLineRect.fY < tRect.fY ) {
				tLineRect.fH -= tRect.fY - tLineRect.fY;
				tLineRect.fY = tRect.fY;
			}
			if ( (tLineRect.fY + tLineRect.fH) > (tRect.fY + tRect.fH) ) {
				tLineRect.fH = (tRect.fY + tRect.fH) - tLineRect.fY;
			}
			if ( tLineRect.fH <= 0.0f ) {
				continue;
			}
		}
		iRet = pProxy->textDraw(pProxy, pTarget, pLayout->tDesc.pFont, pLayout->pScratch, tLineRect, iColor, iLineFlags);
		if ( iRet != XUI_OK ) {
			return iRet;
		}
	}
	return XUI_OK;
}

XUI_API int xuiTextMeasureLayout(xui_context pContext, const xui_text_layout_desc_t* pDesc, xui_vec2_t* pSize)
{
	xui_text_layout pLayout;
	int iRet;

	if ( (pSize == NULL) ) {
		return XUI_ERROR_INVALID_ARGUMENT;
	}
	pSize->fX = 0.0f;
	pSize->fY = 0.0f;
	pLayout = NULL;
	iRet = xuiTextLayoutCreate(pContext, &pLayout, pDesc);
	if ( iRet != XUI_OK ) {
		return iRet;
	}
	*pSize = xuiTextLayoutGetSize(pLayout);
	xuiTextLayoutDestroy(pLayout);
	return XUI_OK;
}
