static void diagPreviousDrawPosition(const xge_glyph_position_t* pPosition,
    xge_glyph_run_backend_t* pBackend, float fPenX, float fPenY, uint32_t iColor,
    uint32_t iFlags, const xge_text_paint_span_t* pSpans, int iSpanCount,
    xge_glyph_paint_cache_t* pCache)
{
    xge_glyph_atlas_page_t* pPages;
    xge_glyph_t tGlyph;
    xge_draw_t tDraw;
    int iGlyphRet;
		if ( pPosition->iItemKind == XGE_TEXT_ITEM_EMOJI ) {
			xge_rect_t tEmojiRect;
			int iEmojiRet = XGE_ERROR_RESOURCE_FAILED;
			int bMayPaint = 1;
			tEmojiRect.fX = fPenX + pPosition->fOffsetX;
			tEmojiRect.fY = fPenY + pPosition->fOffsetY;
			tEmojiRect.fW = pPosition->fEmojiWidth;
			tEmojiRect.fH = pPosition->fEmojiHeight;
			if ( (pBackend != NULL) && (pBackend->pEmojiPack != NULL) ) {
				iEmojiRet = diagClipEmojiDraw(
					pBackend->pEmojiPack, pPosition->iEmojiId, tEmojiRect,
					((iFlags & XGE_DRAW_SCREEN_SPACE) != 0), &bMayPaint
				);
			}
			if ( iEmojiRet == XGE_OK ) {
				__xgeGlyphPaintCacheEmoji(pCache, pPosition, fPenX, fPenY, bMayPaint);
				return;
			}
			if ( pCache != NULL ) pCache->bValid = 0;
		}
		iGlyphRet = xgeFontGlyphAtlasGetByIndex(pPosition->pFont, pPosition->iGlyph, &tGlyph);
		if ( iGlyphRet == XGE_OK &&
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
                __xgeGlyphPaintCacheAdd(pCache, pPosition, fPenX, fPenY);
                diagClipDrawEx(&tDraw);
            }
		}
        if ( iGlyphRet != XGE_OK && pCache != NULL ) pCache->bValid = 0;
}

static void diagPreviousDrawSpans(const xge_glyph_run_t* pRun, float fX, float fY,
    uint32_t iColor, uint32_t iFlags, const xge_text_paint_span_t* pSpans, int iSpanCount,
    xge_glyph_paint_cache_t* pCache)
{
    const xge_glyph_position_t* pPosition;
    xge_glyph_run_backend_t* pBackend;
    float fPenX;
    float fPenY;
    int i;
    if ( pRun == NULL || pRun->pGlyphs == NULL ) return;
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
        diagPreviousDrawPosition(pPosition, pBackend, fPenX, fPenY, iColor,
            iFlags, pSpans, iSpanCount, pCache);
        fPenX += pPosition->fAdvanceX;
    }
}
