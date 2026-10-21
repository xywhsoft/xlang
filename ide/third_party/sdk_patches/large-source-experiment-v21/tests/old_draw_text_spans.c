static int diagOldDrawTextSpans(xui_proxy pProxy, xui_draw_context pDraw, xui_font pFont,
	const char* sText, int iTextSize, xui_rect_t tRect, uint32_t iColor, uint32_t iFlags,
	const xui_text_paint_span_t* pSpans, int iSpanCount)
{
	xge_text_shape_desc_t tDesc;
	xge_glyph_run_t tRun;
	xge_text_paint_span_t sSmall[16];
	xge_text_paint_span_t* pXgeSpans;
	int i;
	int iRet;

	if ( (pProxy == NULL) || !__xuiProxyXgeDrawValid(pDraw) || !__xuiProxyXgeFontValid(pFont) ||
	     (sText == NULL) || (iTextSize < 0) || (iSpanCount < 0) ||
	     ((iSpanCount > 0) && (pSpans == NULL)) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( iTextSize == 0 || tRect.fW <= 0.0f || tRect.fH <= 0.0f ) return XGE_OK;
	pXgeSpans = sSmall;
	if ( iSpanCount > (int)(sizeof(sSmall) / sizeof(sSmall[0])) ) {
		pXgeSpans = (xge_text_paint_span_t*)xrtMalloc(sizeof(*pXgeSpans) * (size_t)iSpanCount);
		if ( pXgeSpans == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
	}
	for ( i = 0; i < iSpanCount; i++ ) {
		pXgeSpans[i].iSize = sizeof(*pXgeSpans);
		pXgeSpans[i].iStart = pSpans[i].iStart;
		pXgeSpans[i].iEnd = pSpans[i].iEnd;
		pXgeSpans[i].iColor = pSpans[i].iColor;
	}
	memset(&tDesc, 0, sizeof(tDesc));
	memset(&tRun, 0, sizeof(tRun));
	tDesc.iSize = sizeof(tDesc);
	tDesc.pFont = &pFont->tFont;
	tDesc.sText = sText;
	tDesc.iTextSize = iTextSize;
	tDesc.iFlags = XGE_TEXT_SHAPE_DEFAULT;
	tDesc.iEmojiPresentation = XGE_EMOJI_PRESENTATION_AUTO;
	tDesc.iEmojiLinePolicy = XGE_EMOJI_LINE_STABLE;
	tDesc.fEmojiScale = 1.0f;
	iRet = xgeTextShape(&tDesc, &tRun);
	if ( iRet == XGE_OK ) {
		xgeGlyphRunDrawSpans(&tRun, tRect.fX, tRect.fY, iColor,
			XGE_DRAW_SCREEN_SPACE, pXgeSpans, iSpanCount);
		xgeGlyphRunFree(&tRun);
		__xuiProxyXgeDrawMarkDirty(pDraw);
	}
	if ( pXgeSpans != sSmall ) xrtFree(pXgeSpans);
	(void)iFlags;
	return iRet;
}
