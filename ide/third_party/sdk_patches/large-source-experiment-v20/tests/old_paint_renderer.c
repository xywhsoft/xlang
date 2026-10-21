static int diagOldEmojiDraw(xge_emoji_pack pPack, uint32_t iEmojiId, xge_rect_t tDst,
	int bScreenSpace)
{
	xge_svg pSvg;
	int iEntry;
	int iRet;

	if ( (pPack == NULL) || (iEmojiId == 0) || (iEmojiId > (uint32_t)pPack->iEntryCount) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iEntry = (int)iEmojiId - 1;
	pSvg = NULL;
	xrtMutexLock(pPack->pCacheMutex);
	iRet = __xgeEmojiEntryLoadLocked(pPack, iEntry);
	if ( iRet == XGE_OK ) {
		pSvg = pPack->pEntries[iEntry].pSvg;
		iRet = xgeSvgAddRef(pSvg);
	}
	xrtMutexUnlock(pPack->pCacheMutex);
	if ( iRet != XGE_OK ) return iRet;
    if ( bScreenSpace ) {
        xge_rect_i_t tPixelClip = xgeClipGetPixels();
        if ( __xgeTextOutsidePixelClip(tDst, tPixelClip) ) {
            xge_rect_t tPaintBounds;
            /* The SVG renderer clips all content to its hierarchy-adjusted
               outer viewport. Check that exact conservative rectangle first;
               a runtime root transform may move it beyond the nominal box. */
            if ( __xgeSvgDrawOuterClipBounds(pSvg, tDst, &tPaintBounds) == XGE_OK &&
                 __xgeTextOutsidePixelClip(tPaintBounds, tPixelClip) ) {
                xgeSvgDestroy(pSvg);
                return XGE_OK;
            }
            /* A custom SVG may paint outside its nominal viewport. */
            if ( xgeSvgGetDrawBounds(pSvg, tDst, 0.35f, &tPaintBounds) == XGE_OK &&
                 __xgeTextOutsidePixelClip(tPaintBounds, tPixelClip) ) {
                xgeSvgDestroy(pSvg);
                return XGE_OK;
            }
        }
    }
	iRet = bScreenSpace ? xgeSvgDrawPx(pSvg, tDst, 0.35f) : xgeSvgDraw(pSvg, tDst, 0.35f);
	xgeSvgDestroy(pSvg);
	return iRet;
}

static void diagOldGlyphRunDrawSpans(const xge_glyph_run_t* pRun, float fX, float fY,
	uint32_t iColor, uint32_t iFlags, const xge_text_paint_span_t* pSpans, int iSpanCount)
{
	const xge_glyph_position_t* pPosition;
	xge_glyph_atlas_page_t* pPages;
	xge_glyph_t tGlyph;
	xge_draw_t tDraw;
	float fPenX;
	float fPenY;
	xge_glyph_run_backend_t* pBackend;
	int i;

	if ( (pRun == NULL) || (pRun->pGlyphs == NULL) ) return;
	pBackend = (xge_glyph_run_backend_t*)pRun->pBackend;
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
		if ( pPosition->iItemKind == XGE_TEXT_ITEM_EMOJI ) {
			xge_rect_t tEmojiRect;
			int iEmojiRet = XGE_ERROR_RESOURCE_FAILED;
			tEmojiRect.fX = fPenX + pPosition->fOffsetX;
			tEmojiRect.fY = fPenY + pPosition->fOffsetY;
			tEmojiRect.fW = pPosition->fEmojiWidth;
			tEmojiRect.fH = pPosition->fEmojiHeight;
			if ( (pBackend != NULL) && (pBackend->pEmojiPack != NULL) ) {
				iEmojiRet = diagOldEmojiDraw(
					pBackend->pEmojiPack, pPosition->iEmojiId, tEmojiRect,
					((iFlags & XGE_DRAW_SCREEN_SPACE) != 0)
				);
			}
			if ( iEmojiRet == XGE_OK ) {
				fPenX += pPosition->fAdvanceX;
				continue;
			}
		}
		if ( xgeFontGlyphAtlasGetByIndex(pPosition->pFont, pPosition->iGlyph, &tGlyph) == XGE_OK &&
		     (tGlyph.iPage >= 0) && (tGlyph.iWidth > 0) && (tGlyph.iHeight > 0) ) {
			pPages = (xge_glyph_atlas_page_t*)pPosition->pFont->tAtlas.pPages;
			if ( (pPosition->iItemKind == XGE_TEXT_ITEM_EMOJI) &&
			     (pPages[tGlyph.iPage].bDirty || (pPages[tGlyph.iPage].tTexture.iBackendId == 0)) ) {
				(void)__xgeFontAtlasUploadPage(pPosition->pFont, tGlyph.iPage);
			}
			memset(&tDraw, 0, sizeof(tDraw));
			tDraw.pTexture = &pPages[tGlyph.iPage].tTexture;
			tDraw.tSrc.fX = (float)tGlyph.iX;
			tDraw.tSrc.fY = (float)tGlyph.iY;
			tDraw.tSrc.fW = (float)tGlyph.iWidth;
			tDraw.tSrc.fH = (float)tGlyph.iHeight;
			tDraw.tDst.fX = fPenX + tGlyph.fOffsetX +
				((pPosition->iItemKind == XGE_TEXT_ITEM_GLYPH) ? pPosition->fOffsetX : 0.0f);
			tDraw.tDst.fY = fPenY + tGlyph.fOffsetY +
				((pPosition->iItemKind == XGE_TEXT_ITEM_GLYPH) ? pPosition->fOffsetY : 0.0f);
			tDraw.tDst.fW = (float)tGlyph.iWidth;
			tDraw.tDst.fH = (float)tGlyph.iHeight;
			tDraw.iColor = __xgeGlyphRunSpanColor(pPosition, iColor, pSpans, iSpanCount);
			tDraw.iFlags = iFlags;
            if ( (iFlags & XGE_DRAW_SCREEN_SPACE) == 0 ||
                 !__xgeTextOutsidePixelClip(tDraw.tDst, xgeClipGetPixels()) ) {
                xgeDrawEx(&tDraw);
            }
		}
		fPenX += pPosition->fAdvanceX;
	}
}

