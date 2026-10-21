static int kernPrevious__xgeTextShape(const xge_text_shape_desc_t* pDesc, xge_glyph_run_t* pRun,
    xge_internal_text_shape_visit_proc onVisit, void* pUser)
{
	xge_glyph_run_backend_t* pBackend;
    xge_text_shape_lookup_t tLookup[XGE_TEXT_SHAPE_LOOKUP_SLOTS];
    xge_text_shape_lookup_t* pLookup;
	xge_glyph_position_t* pPosition;
    xge_glyph_position_t tCurrent;
    xge_glyph_position_t tPending;
    int bPending = 0;
	xge_glyph_metrics_t tMetrics;
	xge_emoji_match_t tEmojiMatch;
	xge_emoji_pack pEmojiPack;
	xge_font pGlyphFont;
	xge_font pPreviousFont;
	stbtt_fontinfo* pInfo;
	const char* sScan;
	const char* sEnd;
	const char* sBefore;
	uint32_t iCodepoint;
	long double fLineWidth;
	long double fLineRight;
	long double fMaxWidth;
	long double fGlyphRight;
	float fKerning;
	float fEmojiAdvance;
	float fEmojiWidth;
	float fEmojiHeight;
	float fEmojiAbove;
	float fEmojiBelow;
	float fMaxLineGap;
	int iTextSize;
	int iGlyph;
	int iPreviousGlyph;
	int iLineCount;
	int iEmojiPresentation;
	int iEmojiLinePolicy;
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
	iEmojiPresentation = pDesc->iEmojiPresentation;
	if ( (iEmojiPresentation < XGE_EMOJI_PRESENTATION_AUTO) ||
	     (iEmojiPresentation > XGE_EMOJI_PRESENTATION_DISABLED) ) {
		iEmojiPresentation = XGE_EMOJI_PRESENTATION_AUTO;
	}
	iEmojiLinePolicy = (pDesc->iEmojiLinePolicy == XGE_EMOJI_LINE_EXPAND)
		? XGE_EMOJI_LINE_EXPAND : XGE_EMOJI_LINE_STABLE;
	pEmojiPack = pDesc->pEmojiPack;
	if ( iTextSize == 0 ) return XGE_OK;
    pLookup = NULL;
    if ( iTextSize >= 256 ) {
        memset(tLookup, 0, sizeof(tLookup));
        pLookup = tLookup;
    }
    {
        const char* sCount = pDesc->sText;
        const char* sCountEnd = sCount + iTextSize;
        int iCapacity = 0;
        uint32_t iCountCodepoint;
        while ( sCount < sCountEnd ) {
            iRet = __xgeTextUTF8DecodeBounded(&sCount, sCountEnd, &iCountCodepoint);
            if ( iRet != XGE_OK ) { xgeGlyphRunFree(pRun); return iRet; }
            ++iCapacity;
        }
        /* UTF-8 bytes are an upper bound, but waste most capacity for CJK/emoji. */
        if ( onVisit == NULL ) {
            pRun->pGlyphs = (xge_glyph_position_t*)xrtCalloc((size_t)iCapacity, sizeof(*pRun->pGlyphs));
        }
    }
	pBackend = (xge_glyph_run_backend_t*)xrtCalloc(1, sizeof(*pBackend));
	if ( ((onVisit == NULL) && (pRun->pGlyphs == NULL)) || (pBackend == NULL) ) {
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
	fMaxLineGap = pDesc->pFont->fLineGap;
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
        if ( onVisit != NULL ) {
            memset(&tCurrent, 0, sizeof(tCurrent));
            pPosition = &tCurrent;
        } else {
            pPosition = &pRun->pGlyphs[pRun->iGlyphCount];
        }
		pPosition->iCodepoint = iCodepoint;
		pPosition->iCluster = (uint32_t)(sBefore - pDesc->sText);
		pPosition->iClusterEnd = (uint32_t)(sScan - pDesc->sText);
		pPosition->iItemKind = XGE_TEXT_ITEM_GLYPH;
		if ( iCodepoint == '\n' ) {
			pPosition->iGlyph = -1;
			pPosition->iFlags = XGE_GLYPH_POSITION_LINE_BREAK;
			pPosition->pFont = pDesc->pFont;
			if ( onVisit != NULL ) {
                if ( bPending ) {
                    iRet = onVisit(&tPending, pUser);
                    if ( iRet != XGE_OK ) { xgeGlyphRunFree(pRun); return iRet; }
                }
                tPending = *pPosition;
                bPending = 1;
            }
            pRun->iGlyphCount++;
			if ( fLineRight > fMaxWidth ) fMaxWidth = fLineRight;
			fLineWidth = 0.0f;
			fLineRight = 0.0f;
			iPreviousGlyph = -1;
			pPreviousFont = NULL;
			iLineCount++;
			continue;
		}
		if ( ((pDesc->iFlags & XGE_TEXT_SHAPE_EMOJI) != 0) &&
		     (iEmojiPresentation != XGE_EMOJI_PRESENTATION_TEXT) &&
		     (iEmojiPresentation != XGE_EMOJI_PRESENTATION_DISABLED) &&
		     ((pEmojiPack != NULL) || __xgeEmojiMayStart(iCodepoint)) ) {
			if ( pEmojiPack == NULL ) {
				pEmojiPack = __xgeEmojiDefaultAcquire();
				if ( pEmojiPack != NULL ) {
					if ( __xgeGlyphRunKeepEmojiPack(pBackend, pEmojiPack) != XGE_OK ) {
						xgeEmojiPackFree(pEmojiPack);
						xgeGlyphRunFree(pRun);
						return XGE_ERROR_OUT_OF_MEMORY;
					}
					xgeEmojiPackFree(pEmojiPack);
					pEmojiPack = pBackend->pEmojiPack;
				}
			}
		}
		if ( ((pDesc->iFlags & XGE_TEXT_SHAPE_EMOJI) != 0) && (pEmojiPack != NULL) &&
		     (__xgeEmojiMatchForText(pEmojiPack, sBefore, sEnd, iEmojiPresentation, &tEmojiMatch) == XGE_OK) ) {
			if ( __xgeGlyphRunKeepEmojiPack(pBackend, pEmojiPack) != XGE_OK ) {
				xgeGlyphRunFree(pRun);
				return XGE_ERROR_OUT_OF_MEMORY;
			}
			pGlyphFont = __xgeTextShapeResolveCodepoint(pDesc->pFont, iCodepoint, &iGlyph, pLookup);
			if ( (pGlyphFont != NULL) && (__xgeGlyphRunKeepFont(pBackend, pGlyphFont) != XGE_OK) ) {
				xgeGlyphRunFree(pRun);
				return XGE_ERROR_OUT_OF_MEMORY;
			}
			__xgeEmojiResolveLayout(
				pDesc->pFont, &tEmojiMatch.tMetrics, pDesc->fEmojiScale, iEmojiLinePolicy,
				&fEmojiAdvance, &fEmojiWidth, &fEmojiHeight, &fEmojiAbove, &fEmojiBelow
			);
			sScan = sBefore + tEmojiMatch.iTextSize;
			pPosition->iClusterEnd = (uint32_t)(sScan - pDesc->sText);
			pPosition->iItemKind = XGE_TEXT_ITEM_EMOJI;
			pPosition->iGlyph = (pGlyphFont != NULL) ? iGlyph : -1;
			pPosition->pFont = pGlyphFont;
			pPosition->iEmojiId = tEmojiMatch.iEmojiId;
			pPosition->fAdvanceX = fEmojiAdvance;
			pPosition->fEmojiWidth = fEmojiWidth;
			pPosition->fEmojiHeight = fEmojiHeight;
			pPosition->fOffsetX = (fEmojiAdvance - fEmojiWidth) * 0.5f;
			pPosition->fOffsetY = -fEmojiAbove;
			if ( onVisit != NULL ) {
                if ( bPending ) {
                    iRet = onVisit(&tPending, pUser);
                    if ( iRet != XGE_OK ) { xgeGlyphRunFree(pRun); return iRet; }
                }
                tPending = *pPosition;
                bPending = 1;
            }
            pRun->iGlyphCount++;
			fGlyphRight = fLineWidth + pPosition->fOffsetX + fEmojiWidth;
			if ( fGlyphRight > fLineRight ) fLineRight = fGlyphRight;
			fLineWidth += fEmojiAdvance;
			if ( fLineWidth > fLineRight ) fLineRight = fLineWidth;
			if ( iEmojiLinePolicy == XGE_EMOJI_LINE_EXPAND ) {
				if ( fEmojiAbove > pRun->fAscent ) pRun->fAscent = fEmojiAbove;
				if ( -fEmojiBelow < pRun->fDescent ) pRun->fDescent = -fEmojiBelow;
				pRun->fLineHeight = pRun->fAscent - pRun->fDescent + fMaxLineGap;
			}
			iPreviousGlyph = -1;
			pPreviousFont = NULL;
			continue;
		}
		pGlyphFont = __xgeTextShapeResolveCodepoint(pDesc->pFont, iCodepoint, &iGlyph, pLookup);
		if ( pGlyphFont == NULL ) continue;
		iRet = xgeFontGlyphGetByIndex(pGlyphFont, iGlyph, &tMetrics);
		if ( iRet != XGE_OK ) continue;
		if ( __xgeGlyphRunKeepFont(pBackend, pGlyphFont) != XGE_OK ) {
			xgeGlyphRunFree(pRun);
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		if ( pGlyphFont->fAscent > pRun->fAscent ) pRun->fAscent = pGlyphFont->fAscent;
		if ( pGlyphFont->fDescent < pRun->fDescent ) pRun->fDescent = pGlyphFont->fDescent;
		if ( pGlyphFont->fLineGap > fMaxLineGap ) fMaxLineGap = pGlyphFont->fLineGap;
		pRun->fLineHeight = pRun->fAscent - pRun->fDescent + fMaxLineGap;
		if ( ((pDesc->iFlags & XGE_TEXT_SHAPE_KERNING) != 0) && (iPreviousGlyph >= 0) && (pPreviousFont == pGlyphFont) ) {
			pInfo = __xgeFontInfo(pGlyphFont);
			if ( pInfo != NULL ) {
				fKerning = (float)stbtt_GetGlyphKernAdvance(pInfo, iPreviousGlyph, iGlyph) * pGlyphFont->fScale;
                if ( onVisit != NULL ) tPending.fAdvanceX += fKerning;
                else pRun->pGlyphs[pRun->iGlyphCount - 1].fAdvanceX += fKerning;
				fLineWidth += fKerning;
			}
		}
		pPosition->iGlyph = iGlyph;
		pPosition->pFont = pGlyphFont;
		pPosition->fAdvanceX = tMetrics.fAdvanceX;
		if ( onVisit != NULL ) {
                if ( bPending ) {
                    iRet = onVisit(&tPending, pUser);
                    if ( iRet != XGE_OK ) { xgeGlyphRunFree(pRun); return iRet; }
                }
                tPending = *pPosition;
                bPending = 1;
            }
            pRun->iGlyphCount++;
		fGlyphRight = fLineWidth + tMetrics.fX1;
		if ( fGlyphRight > fLineRight ) fLineRight = fGlyphRight;
		fLineWidth += tMetrics.fAdvanceX;
		if ( fLineWidth > fLineRight ) fLineRight = fLineWidth;
		iPreviousGlyph = iGlyph;
		pPreviousFont = pGlyphFont;
	}
    if ( onVisit != NULL && bPending ) {
        iRet = onVisit(&tPending, pUser);
        if ( iRet != XGE_OK ) { xgeGlyphRunFree(pRun); return iRet; }
    }
	if ( fLineRight > fMaxWidth ) fMaxWidth = fLineRight;
	pRun->fWidth = fMaxWidth;
	pRun->fHeight = pRun->fLineHeight * (float)iLineCount;
	return XGE_OK;
}

int kernPreviousxgeTextShape(const xge_text_shape_desc_t* pDesc, xge_glyph_run_t* pRun)
{
    return kernPrevious__xgeTextShape(pDesc, pRun, NULL, NULL);
}

int kernPreviousxgeInternalTextShapeVisit(const xge_text_shape_desc_t* pDesc, xge_glyph_run_t* pRun,
    xge_internal_text_shape_visit_proc onVisit, void* pUser)
{
    if ( onVisit == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
    return kernPrevious__xgeTextShape(pDesc, pRun, onVisit, pUser);
}

