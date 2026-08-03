#ifndef XGE_NO_TEXT
typedef struct xge_glyph_run_backend_t {
	int iFontCount;
	int iFontCapacity;
	xge_font* ppFonts;
} xge_glyph_run_backend_t;

static int __xgeGlyphRunKeepFont(xge_glyph_run_backend_t* pBackend, xge_font pFont)
{
	xge_font* ppFonts;
	int iCapacity;
	int i;

	if ( (pBackend == NULL) || (pFont == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < pBackend->iFontCount; i++ ) {
		if ( pBackend->ppFonts[i] == pFont ) return XGE_OK;
	}
	if ( pBackend->iFontCount >= pBackend->iFontCapacity ) {
		iCapacity = (pBackend->iFontCapacity > 0) ? pBackend->iFontCapacity * 2 : 4;
		ppFonts = (xge_font*)xrtRealloc(pBackend->ppFonts, sizeof(*ppFonts) * (size_t)iCapacity);
		if ( ppFonts == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
		pBackend->ppFonts = ppFonts;
		pBackend->iFontCapacity = iCapacity;
	}
	if ( xgeFontAddRef(pFont) <= 0 ) return XGE_ERROR_INVALID_ARGUMENT;
	pBackend->ppFonts[pBackend->iFontCount++] = pFont;
	return XGE_OK;
}

int xgeTextShape(const xge_text_shape_desc_t* pDesc, xge_glyph_run_t* pRun)
{
	xge_glyph_run_backend_t* pBackend;
	xge_glyph_position_t* pPosition;
	xge_glyph_metrics_t tMetrics;
	xge_font pGlyphFont;
	xge_font pPreviousFont;
	stbtt_fontinfo* pInfo;
	const char* sScan;
	const char* sEnd;
	const char* sBefore;
	uint32_t iCodepoint;
	float fLineWidth;
	float fLineRight;
	float fMaxWidth;
	float fGlyphRight;
	float fKerning;
	int iTextSize;
	int iGlyph;
	int iPreviousGlyph;
	int iLineCount;
	int iRet;

	if ( (pDesc == NULL) || (pRun == NULL) || (pDesc->iSize < sizeof(*pDesc)) ||
	     (pDesc->pFont == NULL) || (pDesc->sText == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	memset(pRun, 0, sizeof(*pRun));
	iTextSize = pDesc->iTextSize;
	if ( iTextSize < -1 ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( iTextSize <= 0 ) iTextSize = (int)strlen(pDesc->sText);
	pRun->iSize = sizeof(*pRun);
	pRun->iFlags = pDesc->iFlags;
	pRun->iTextSize = iTextSize;
	pRun->fAscent = pDesc->pFont->fAscent;
	pRun->fDescent = pDesc->pFont->fDescent;
	pRun->fLineHeight = pDesc->pFont->fLineHeight;
	pRun->fHeight = pDesc->pFont->fLineHeight;
	if ( iTextSize == 0 ) return XGE_OK;
	pRun->pGlyphs = (xge_glyph_position_t*)xrtCalloc((size_t)iTextSize, sizeof(*pRun->pGlyphs));
	pBackend = (xge_glyph_run_backend_t*)xrtCalloc(1, sizeof(*pBackend));
	if ( (pRun->pGlyphs == NULL) || (pBackend == NULL) ) {
		xrtFree(pRun->pGlyphs);
		xrtFree(pBackend);
		memset(pRun, 0, sizeof(*pRun));
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pRun->pBackend = pBackend;
	sScan = pDesc->sText;
	sEnd = pDesc->sText + iTextSize;
	fLineWidth = 0.0f;
	fLineRight = 0.0f;
	fMaxWidth = 0.0f;
	iPreviousGlyph = -1;
	pPreviousFont = NULL;
	iLineCount = 1;
	while ( sScan < sEnd ) {
		sBefore = sScan;
		iRet = __xgeTextUTF8DecodeBounded(&sScan, sEnd, &iCodepoint);
		if ( iRet != XGE_OK ) {
			xgeGlyphRunFree(pRun);
			return iRet;
		}
		if ( iCodepoint == '\r' ) {
			if ( (sScan < sEnd) && (*sScan == '\n') ) continue;
			iCodepoint = '\n';
		}
		pPosition = &pRun->pGlyphs[pRun->iGlyphCount];
		pPosition->iCodepoint = iCodepoint;
		pPosition->iCluster = (uint32_t)(sBefore - pDesc->sText);
		if ( iCodepoint == '\n' ) {
			pPosition->iGlyph = -1;
			pPosition->iFlags = XGE_GLYPH_POSITION_LINE_BREAK;
			pPosition->pFont = pDesc->pFont;
			pRun->iGlyphCount++;
			if ( fLineRight > fMaxWidth ) fMaxWidth = fLineRight;
			fLineWidth = 0.0f;
			fLineRight = 0.0f;
			iPreviousGlyph = -1;
			pPreviousFont = NULL;
			iLineCount++;
			continue;
		}
		pGlyphFont = __xgeFontResolveCodepoint(pDesc->pFont, iCodepoint, &iGlyph);
		if ( pGlyphFont == NULL ) continue;
		iRet = xgeFontGlyphGetByIndex(pGlyphFont, iGlyph, &tMetrics);
		if ( iRet != XGE_OK ) continue;
		if ( __xgeGlyphRunKeepFont(pBackend, pGlyphFont) != XGE_OK ) {
			xgeGlyphRunFree(pRun);
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		if ( pGlyphFont->fAscent > pRun->fAscent ) pRun->fAscent = pGlyphFont->fAscent;
		if ( pGlyphFont->fDescent < pRun->fDescent ) pRun->fDescent = pGlyphFont->fDescent;
		if ( pGlyphFont->fLineHeight > pRun->fLineHeight ) pRun->fLineHeight = pGlyphFont->fLineHeight;
		if ( ((pDesc->iFlags & XGE_TEXT_SHAPE_KERNING) != 0) && (iPreviousGlyph >= 0) && (pPreviousFont == pGlyphFont) ) {
			pInfo = __xgeFontInfo(pGlyphFont);
			if ( pInfo != NULL ) {
				fKerning = (float)stbtt_GetGlyphKernAdvance(pInfo, iPreviousGlyph, iGlyph) * pGlyphFont->fScale;
				pRun->pGlyphs[pRun->iGlyphCount - 1].fAdvanceX += fKerning;
				fLineWidth += fKerning;
			}
		}
		pPosition->iGlyph = iGlyph;
		pPosition->pFont = pGlyphFont;
		pPosition->fAdvanceX = tMetrics.fAdvanceX;
		pRun->iGlyphCount++;
		fGlyphRight = fLineWidth + tMetrics.fX1;
		if ( fGlyphRight > fLineRight ) fLineRight = fGlyphRight;
		fLineWidth += tMetrics.fAdvanceX;
		if ( fLineWidth > fLineRight ) fLineRight = fLineWidth;
		iPreviousGlyph = iGlyph;
		pPreviousFont = pGlyphFont;
	}
	if ( fLineRight > fMaxWidth ) fMaxWidth = fLineRight;
	pRun->fWidth = fMaxWidth;
	pRun->fHeight = pRun->fLineHeight * (float)iLineCount;
	return XGE_OK;
}

void xgeGlyphRunFree(xge_glyph_run_t* pRun)
{
	xge_glyph_run_backend_t* pBackend;
	int i;

	if ( pRun == NULL ) return;
	pBackend = (xge_glyph_run_backend_t*)pRun->pBackend;
	if ( pBackend != NULL ) {
		for ( i = 0; i < pBackend->iFontCount; i++ ) xgeFontFree(pBackend->ppFonts[i]);
		xrtFree(pBackend->ppFonts);
		xrtFree(pBackend);
	}
	xrtFree(pRun->pGlyphs);
	memset(pRun, 0, sizeof(*pRun));
}

xge_vec2_t xgeGlyphRunMeasure(const xge_glyph_run_t* pRun)
{
	xge_vec2_t tSize;

	tSize.fX = (pRun != NULL) ? pRun->fWidth : 0.0f;
	tSize.fY = (pRun != NULL) ? pRun->fHeight : 0.0f;
	return tSize;
}

int xgeGlyphRunHitTest(const xge_glyph_run_t* pRun, float fX, float fY, uint32_t* pCluster, int* pTrailing)
{
	const xge_glyph_position_t* pPosition;
	float fPenX;
	int iTargetLine;
	int iLine;
	int i;

	if ( (pRun == NULL) || (pCluster == NULL) || (pTrailing == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	iTargetLine = (pRun->fLineHeight > 0.0f) ? (int)floorf(fY / pRun->fLineHeight) : 0;
	if ( iTargetLine < 0 ) iTargetLine = 0;
	fPenX = 0.0f;
	iLine = 0;
	for ( i = 0; i < pRun->iGlyphCount; i++ ) {
		pPosition = &pRun->pGlyphs[i];
		if ( (pPosition->iFlags & XGE_GLYPH_POSITION_LINE_BREAK) != 0 ) {
			if ( iLine == iTargetLine ) {
				*pCluster = pPosition->iCluster;
				*pTrailing = 0;
				return XGE_OK;
			}
			iLine++;
			fPenX = 0.0f;
			continue;
		}
		if ( iLine != iTargetLine ) continue;
		if ( fX <= (fPenX + pPosition->fAdvanceX * 0.5f) ) {
			*pCluster = pPosition->iCluster;
			*pTrailing = 0;
			return XGE_OK;
		}
		if ( fX <= (fPenX + pPosition->fAdvanceX) ) {
			*pCluster = pPosition->iCluster;
			*pTrailing = 1;
			return XGE_OK;
		}
		fPenX += pPosition->fAdvanceX;
	}
	*pCluster = (uint32_t)pRun->iTextSize;
	*pTrailing = 0;
	return XGE_OK;
}

static void __xgeGlyphRunPrepareAtlas(const xge_glyph_run_t* pRun)
{
	xge_glyph_run_backend_t* pBackend;
	xge_glyph_atlas_page_t* pPages;
	xge_glyph_t tGlyph;
	int i;
	int j;

	for ( i = 0; i < pRun->iGlyphCount; i++ ) {
		if ( (pRun->pGlyphs[i].iFlags & XGE_GLYPH_POSITION_LINE_BREAK) == 0 ) {
			(void)xgeFontGlyphAtlasGetByIndex(pRun->pGlyphs[i].pFont, pRun->pGlyphs[i].iGlyph, &tGlyph);
		}
	}
	pBackend = (xge_glyph_run_backend_t*)pRun->pBackend;
	if ( pBackend == NULL ) return;
	for ( i = 0; i < pBackend->iFontCount; i++ ) {
		pPages = (xge_glyph_atlas_page_t*)pBackend->ppFonts[i]->tAtlas.pPages;
		for ( j = 0; (pPages != NULL) && (j < pBackend->ppFonts[i]->tAtlas.iPageCount); j++ ) {
			if ( pPages[j].bDirty || (pPages[j].tTexture.iBackendId == 0) ) (void)__xgeFontAtlasUploadPage(pBackend->ppFonts[i], j);
		}
	}
}

void xgeGlyphRunDraw(const xge_glyph_run_t* pRun, float fX, float fY, uint32_t iColor, uint32_t iFlags)
{
	const xge_glyph_position_t* pPosition;
	xge_glyph_atlas_page_t* pPages;
	xge_glyph_t tGlyph;
	xge_draw_t tDraw;
	float fPenX;
	float fPenY;
	int i;

	if ( (pRun == NULL) || (pRun->pGlyphs == NULL) ) return;
	__xgeGlyphRunPrepareAtlas(pRun);
	fPenX = fX;
	fPenY = fY + pRun->fAscent;
	if ( (iFlags & XGE_DRAW_SCREEN_SPACE) != 0 ) {
		fPenX = __xgeTextSnapPixel(fPenX);
		fPenY = __xgeTextSnapPixel(fPenY);
	}
	for ( i = 0; i < pRun->iGlyphCount; i++ ) {
		pPosition = &pRun->pGlyphs[i];
		if ( (pPosition->iFlags & XGE_GLYPH_POSITION_LINE_BREAK) != 0 ) {
			fPenX = fX;
			fPenY += pRun->fLineHeight;
			continue;
		}
		if ( xgeFontGlyphAtlasGetByIndex(pPosition->pFont, pPosition->iGlyph, &tGlyph) == XGE_OK &&
		     (tGlyph.iPage >= 0) && (tGlyph.iWidth > 0) && (tGlyph.iHeight > 0) ) {
			pPages = (xge_glyph_atlas_page_t*)pPosition->pFont->tAtlas.pPages;
			memset(&tDraw, 0, sizeof(tDraw));
			tDraw.pTexture = &pPages[tGlyph.iPage].tTexture;
			tDraw.tSrc.fX = (float)tGlyph.iX;
			tDraw.tSrc.fY = (float)tGlyph.iY;
			tDraw.tSrc.fW = (float)tGlyph.iWidth;
			tDraw.tSrc.fH = (float)tGlyph.iHeight;
			tDraw.tDst.fX = fPenX + tGlyph.fOffsetX + pPosition->fOffsetX;
			tDraw.tDst.fY = fPenY + tGlyph.fOffsetY + pPosition->fOffsetY;
			tDraw.tDst.fW = (float)tGlyph.iWidth;
			tDraw.tDst.fH = (float)tGlyph.iHeight;
			tDraw.iColor = iColor;
			tDraw.iFlags = iFlags;
			xgeDrawEx(&tDraw);
		}
		fPenX += pPosition->fAdvanceX;
	}
}

static void __xgeGlyphRunDecorationLine(float fX0, float fX1, float fY, const xge_text_decoration_t* pDecoration, float fDefaultThickness)
{
	float fThickness;
	float fWavelength;
	float fAmplitude;
	float fX;
	float fNext;
	float fDirection;
	uint32_t iColor;
	int bScreenSpace;

	if ( (pDecoration == NULL) || (fX1 <= fX0) ) return;
	iColor = pDecoration->iColor;
	fThickness = (pDecoration->fThickness > 0.0f) ? pDecoration->fThickness : fDefaultThickness;
	bScreenSpace = ((pDecoration->iFlags & XGE_TEXT_DECORATION_SCREEN_SPACE) != 0);
	if ( pDecoration->iType == XGE_TEXT_DECORATION_SQUIGGLE ) {
		fWavelength = (pDecoration->fWavelength > 1.0f) ? pDecoration->fWavelength : 4.0f;
		fAmplitude = (pDecoration->fAmplitude > 0.0f) ? pDecoration->fAmplitude : 1.5f;
		fX = fX0 - fmodf(fabsf(pDecoration->fPhase), fWavelength);
		fDirection = -1.0f;
		while ( fX < fX1 ) {
			fNext = fX + fWavelength * 0.5f;
			if ( fNext > fX1 ) fNext = fX1;
			if ( bScreenSpace ) xgeShapeLinePx(fX, fY + fDirection * fAmplitude, fNext, fY - fDirection * fAmplitude, fThickness, iColor);
			else xgeShapeLine(fX, fY + fDirection * fAmplitude, fNext, fY - fDirection * fAmplitude, fThickness, iColor);
			fDirection = -fDirection;
			fX = fNext;
		}
		return;
	}
	if ( (pDecoration->iType == XGE_TEXT_DECORATION_DOTTED) || (pDecoration->iType == XGE_TEXT_DECORATION_DASHED) ) {
		fWavelength = (pDecoration->fWavelength > 1.0f) ? pDecoration->fWavelength : ((pDecoration->iType == XGE_TEXT_DECORATION_DOTTED) ? 3.0f : 7.0f);
		fX = fX0 - fmodf(fabsf(pDecoration->fPhase), fWavelength);
		while ( fX < fX1 ) {
			fNext = fX + ((pDecoration->iType == XGE_TEXT_DECORATION_DOTTED) ? fThickness : fWavelength * 0.65f);
			if ( fNext > fX1 ) fNext = fX1;
			if ( bScreenSpace ) xgeShapeLinePx(fX, fY, fNext, fY, fThickness, iColor);
			else xgeShapeLine(fX, fY, fNext, fY, fThickness, iColor);
			fX += fWavelength;
		}
		return;
	}
	if ( bScreenSpace ) xgeShapeLinePx(fX0, fY, fX1, fY, fThickness, iColor);
	else xgeShapeLine(fX0, fY, fX1, fY, fThickness, iColor);
}

void xgeGlyphRunDrawDecorated(const xge_glyph_run_t* pRun, float fX, float fY, uint32_t iColor, uint32_t iFlags, const xge_text_decoration_t* pDecorations, int iDecorationCount)
{
	xge_font_metrics_t tMetrics;
	const xge_text_decoration_t* pDecoration;
	const xge_glyph_position_t* pPosition;
	float fPenX;
	float fSegmentStart;
	float fSegmentEnd;
	float fLineTop;
	float fBaseline;
	float fOffset;
	uint32_t iGlyphEnd;
	int iRangeStart;
	int iRangeEnd;
	int bSegment;
	int i;
	int j;

	if ( pRun == NULL ) return;
	xgeGlyphRunDraw(pRun, fX, fY, iColor, iFlags);
	if ( (pDecorations == NULL) || (iDecorationCount <= 0) || (pRun->iGlyphCount <= 0) ) return;
	for ( j = 0; j < iDecorationCount; j++ ) {
		pDecoration = &pDecorations[j];
		if ( (pDecoration->iSize != 0) && (pDecoration->iSize < sizeof(*pDecoration)) ) continue;
		iRangeStart = 0;
		iRangeEnd = pRun->iTextSize;
		if ( (pDecoration->iFlags & XGE_TEXT_DECORATION_RANGE) != 0 ) {
			iRangeStart = pDecoration->iStart;
			iRangeEnd = pDecoration->iEnd;
			if ( iRangeStart < 0 ) iRangeStart = 0;
			if ( iRangeEnd > pRun->iTextSize ) iRangeEnd = pRun->iTextSize;
			if ( iRangeEnd <= iRangeStart ) continue;
		}
		fPenX = fX;
		fLineTop = fY;
		fSegmentStart = fX;
		fSegmentEnd = fX;
		bSegment = 0;
		memset(&tMetrics, 0, sizeof(tMetrics));
		for ( i = 0; i <= pRun->iGlyphCount; i++ ) {
			if ( i == pRun->iGlyphCount ||
			     ((pRun->pGlyphs[i].iFlags & XGE_GLYPH_POSITION_LINE_BREAK) != 0) ) {
				if ( bSegment ) {
					fBaseline = fLineTop + pRun->fAscent;
					if ( pDecoration->iType == XGE_TEXT_DECORATION_OVERLINE ) fOffset = -pRun->fAscent;
					else if ( pDecoration->iType == XGE_TEXT_DECORATION_STRIKE ) fOffset = tMetrics.fStrikePosition;
					else fOffset = tMetrics.fUnderlinePosition;
					if ( pDecoration->fOffset != 0.0f ) fOffset = pDecoration->fOffset;
					__xgeGlyphRunDecorationLine(
						fSegmentStart, fSegmentEnd, fBaseline + fOffset, pDecoration,
						(pDecoration->iType == XGE_TEXT_DECORATION_STRIKE)
							? tMetrics.fStrikeThickness : tMetrics.fUnderlineThickness
					);
					bSegment = 0;
				}
				if ( i < pRun->iGlyphCount ) {
					fPenX = fX;
					fLineTop += pRun->fLineHeight;
				}
				continue;
			}
			pPosition = &pRun->pGlyphs[i];
			iGlyphEnd = (i + 1 < pRun->iGlyphCount)
				? pRun->pGlyphs[i + 1].iCluster : (uint32_t)pRun->iTextSize;
			if ( ((int)iGlyphEnd > iRangeStart) && ((int)pPosition->iCluster < iRangeEnd) ) {
				if ( !bSegment ) {
					fSegmentStart = fPenX;
					memset(&tMetrics, 0, sizeof(tMetrics));
					if ( xgeFontGetMetrics(pPosition->pFont, &tMetrics) != XGE_OK ) {
						tMetrics.fUnderlinePosition = 1.0f;
						tMetrics.fUnderlineThickness = 1.0f;
						tMetrics.fStrikePosition = -pRun->fAscent * 0.35f;
						tMetrics.fStrikeThickness = 1.0f;
					}
					bSegment = 1;
				}
				fSegmentEnd = fPenX + pPosition->fAdvanceX;
			}
			fPenX += pPosition->fAdvanceX;
		}
	}
}
#else

int xgeTextShape(const xge_text_shape_desc_t* pDesc, xge_glyph_run_t* pRun) { (void)pDesc; (void)pRun; return XGE_ERROR_UNSUPPORTED; }
void xgeGlyphRunFree(xge_glyph_run_t* pRun) { if ( pRun != NULL ) memset(pRun, 0, sizeof(*pRun)); }
xge_vec2_t xgeGlyphRunMeasure(const xge_glyph_run_t* pRun) { xge_vec2_t t = {0.0f, 0.0f}; (void)pRun; return t; }
int xgeGlyphRunHitTest(const xge_glyph_run_t* pRun, float fX, float fY, uint32_t* pCluster, int* pTrailing) { (void)pRun; (void)fX; (void)fY; (void)pCluster; (void)pTrailing; return XGE_ERROR_UNSUPPORTED; }
void xgeGlyphRunDraw(const xge_glyph_run_t* pRun, float fX, float fY, uint32_t iColor, uint32_t iFlags) { (void)pRun; (void)fX; (void)fY; (void)iColor; (void)iFlags; }
void xgeGlyphRunDrawDecorated(const xge_glyph_run_t* pRun, float fX, float fY, uint32_t iColor, uint32_t iFlags, const xge_text_decoration_t* pDecorations, int iDecorationCount) { (void)pRun; (void)fX; (void)fY; (void)iColor; (void)iFlags; (void)pDecorations; (void)iDecorationCount; }

#endif
