static int diagFailCollectorCalloc,diagFailCollectorRealloc;
static void* diagPaintRealloc(void* p,size_t size) {
    if(diagFailCollectorRealloc && --diagFailCollectorRealloc==0)return NULL;
    return xrtRealloc(p,size);
}
static void* diagPaintCalloc(size_t count,size_t size) {
    if(diagFailCollectorCalloc){diagFailCollectorCalloc=0;return NULL;}
    return xrtCalloc(count,size);
}

#define XUI_PROXY_XGE_SURFACE_MAGIC	0x58554953u

#define XUI_PROXY_XGE_FONT_MAGIC	0x58554946u

#define XUI_PROXY_XGE_DRAW_MAGIC	0x58554944u

struct xui_surface_t {
	uint32_t iMagic;
	int iKind;
	int iFormat;
	uint32_t iFlags;
	uint32_t iGeneration;
	xge_texture_t tTexture;
	xge_render_target_t tTarget;
};

struct xui_font_t {
	uint32_t iMagic;
	uint32_t iFlags;
	xge_font_t tFont;
	xge_font_t tFallbackFont;
	int bHasFallback;
};

struct xui_draw_context_t {
	uint32_t iMagic;
	xui_surface pTarget;
	xge_pass_t tPass;
	int bDirty;
};

static unsigned long long diagShapeCalls;
static int diagFailCopy,diagFailShape;
static void* diagLastGlyphs;
static void* diagLastBackend;
static xge_emoji_pack diagToggleCustom,diagToggleBuiltin;
static void* diagCopyAlloc(size_t size) {
    if(diagFailCopy){diagFailCopy=0;return NULL;}return xrtMalloc(size);
}
static int diagShape(const xge_text_shape_desc_t* desc,xge_glyph_run_t* run) {
    ++diagShapeCalls;if(diagFailShape){diagFailShape=0;return XGE_ERROR_OUT_OF_MEMORY;}
    if(diagToggleCustom)xgeEmojiPackSetDefault(diagToggleCustom);
    int code=xgeTextShape(desc,run);
    if(diagToggleCustom){xgeEmojiPackSetDefault(diagToggleBuiltin);diagToggleCustom=NULL;diagToggleBuiltin=NULL;}
    if(code==XGE_OK){diagLastGlyphs=run->pGlyphs;diagLastBackend=run->pBackend;}return code;
}

#if (defined(_WIN32) || defined(_WIN64)) && defined(XGE_XRT_PROFILE_IDE) && !defined(XGE_NO_TEXT)
#define XUI_PROXY_XGE_DRAW_CACHE_MIN 4096
#define XUI_PROXY_XGE_DRAW_CACHE_MAX (16 * 1024 * 1024)
extern xge_emoji_pack __xgeEmojiDrawCacheBuiltinAcquire(uint64_t* pRevision);
extern void __xgeGlyphRunPaintCacheFree(void* pCache);
extern void* __xgeGlyphRunPaintCacheCaptureDraw(xge_glyph_run_t* pRun, float fX, float fY,
    uint32_t iColor, const xge_text_paint_span_t* pSpans, int iSpanCount);
extern void __xgeGlyphRunPaintCacheDraw(void* pCache, uint32_t iColor,
    const xge_text_paint_span_t* pSpans, int iSpanCount);
typedef struct xui_proxy_xge_draw_cache_t {
    xui_font pFont;
    char* sText;
    int iTextSize;
    xge_emoji_pack pEmojiPack;
    uint64_t iEmojiRevision;
    xge_rect_i_t tClip;
    float fX;
    float fY;
    void* pPaint;
} xui_proxy_xge_draw_cache_t;
static xui_proxy_xge_draw_cache_t g_xuiProxyXgeDrawCache;
static SRWLOCK g_xuiProxyXgeDrawCacheLock = SRWLOCK_INIT;

static void __xuiProxyXgeDrawCacheClearLocked(void)
{
    __xgeGlyphRunPaintCacheFree(g_xuiProxyXgeDrawCache.pPaint);
    xrtFree(g_xuiProxyXgeDrawCache.sText);
    xgeEmojiPackFree(g_xuiProxyXgeDrawCache.pEmojiPack);
    memset(&g_xuiProxyXgeDrawCache, 0, sizeof(g_xuiProxyXgeDrawCache));
}

static void __xuiProxyXgeDrawCacheClear(xui_font pFont)
{
    AcquireSRWLockExclusive(&g_xuiProxyXgeDrawCacheLock);
    if ( pFont == NULL || g_xuiProxyXgeDrawCache.pFont == pFont )
        __xuiProxyXgeDrawCacheClearLocked();
    ReleaseSRWLockExclusive(&g_xuiProxyXgeDrawCacheLock);
}

static int __xuiProxyXgeDrawCacheTextIsLarge(const char* sText)
{
    int i;
    for ( i = 0; i < XUI_PROXY_XGE_DRAW_CACHE_MIN; i++ )
        if ( sText[i] == 0 ) return 0;
    return 1;
}

static int __xuiProxyXgeDrawCachePaint(xui_font pFont, const xge_text_shape_desc_t* pDesc,
    xui_rect_t tRect, uint32_t iColor, const xge_text_paint_span_t* pSpans, int iSpanCount)
{
    xge_emoji_pack pBefore;
    xge_emoji_pack pAfter;
    uint64_t iBefore;
    uint64_t iAfter;
    xge_rect_i_t tClip;
    xge_glyph_run_t tRun;
    void* pPaint;
    char* sCopy;
    int iRet;
    int bDrawn;
    AcquireSRWLockExclusive(&g_xuiProxyXgeDrawCacheLock);
    tClip = xgeClipGetPixels();
    pBefore = __xgeEmojiDrawCacheBuiltinAcquire(&iBefore);
    if ( pBefore != NULL && g_xuiProxyXgeDrawCache.pEmojiPack == pBefore &&
         g_xuiProxyXgeDrawCache.iEmojiRevision == iBefore &&
         g_xuiProxyXgeDrawCache.pFont == pFont && g_xuiProxyXgeDrawCache.pPaint != NULL &&
         g_xuiProxyXgeDrawCache.iTextSize == pDesc->iTextSize &&
         g_xuiProxyXgeDrawCache.fX == tRect.fX && g_xuiProxyXgeDrawCache.fY == tRect.fY &&
         g_xuiProxyXgeDrawCache.tClip.iX == tClip.iX && g_xuiProxyXgeDrawCache.tClip.iY == tClip.iY &&
         g_xuiProxyXgeDrawCache.tClip.iW == tClip.iW && g_xuiProxyXgeDrawCache.tClip.iH == tClip.iH &&
         g_xuiProxyXgeDrawCache.sText != NULL &&
         memcmp(g_xuiProxyXgeDrawCache.sText, pDesc->sText, (size_t)pDesc->iTextSize) == 0 ) {
        __xgeGlyphRunPaintCacheDraw(g_xuiProxyXgeDrawCache.pPaint, iColor, pSpans, iSpanCount);
        iRet = XGE_OK;
    } else {
        __xuiProxyXgeDrawCacheClearLocked();
        memset(&tRun, 0, sizeof(tRun));
        iRet = diagShape(pDesc, &tRun);
        bDrawn = 0;
        if ( iRet == XGE_OK && pBefore != NULL && pDesc->iTextSize <= XUI_PROXY_XGE_DRAW_CACHE_MAX &&
             tClip.iW > 0 && tClip.iH > 0 ) {
            sCopy = (char*)diagCopyAlloc((size_t)pDesc->iTextSize);
            if ( sCopy != NULL ) {
                memcpy(sCopy, pDesc->sText, (size_t)pDesc->iTextSize);
                pPaint = __xgeGlyphRunPaintCacheCaptureDraw(&tRun, tRect.fX, tRect.fY, iColor, pSpans, iSpanCount);
                bDrawn = 1;
                pAfter = __xgeEmojiDrawCacheBuiltinAcquire(&iAfter);
                if ( pPaint != NULL && pAfter == pBefore && iAfter == iBefore ) {
                    g_xuiProxyXgeDrawCache.pFont = pFont;
                    g_xuiProxyXgeDrawCache.sText = sCopy;
                    sCopy = NULL;
                    g_xuiProxyXgeDrawCache.iTextSize = pDesc->iTextSize;
                    g_xuiProxyXgeDrawCache.pEmojiPack = pBefore;
                    pBefore = NULL;
                    g_xuiProxyXgeDrawCache.iEmojiRevision = iBefore;
                    g_xuiProxyXgeDrawCache.tClip = tClip;
                    g_xuiProxyXgeDrawCache.fX = tRect.fX;
                    g_xuiProxyXgeDrawCache.fY = tRect.fY;
                    g_xuiProxyXgeDrawCache.pPaint = pPaint;
                    pPaint = NULL;
                }
                __xgeGlyphRunPaintCacheFree(pPaint);
                xgeEmojiPackFree(pAfter);
                xrtFree(sCopy);
            }
        }
        if ( iRet == XGE_OK && !bDrawn )
            xgeGlyphRunDrawSpans(&tRun, tRect.fX, tRect.fY, iColor,
                XGE_DRAW_SCREEN_SPACE, pSpans, iSpanCount);
        xgeGlyphRunFree(&tRun);
    }
    xgeEmojiPackFree(pBefore);
    ReleaseSRWLockExclusive(&g_xuiProxyXgeDrawCacheLock);
    return iRet;
}
#endif



static int __xuiProxyXgeFontValid(xui_font pFont)
{
	return (pFont != NULL) &&
	       (pFont->iMagic == XUI_PROXY_XGE_FONT_MAGIC) &&
	       (pFont->tFont.iRefCount > 0);
}

static int __xuiProxyXgeSurfaceValid(xui_surface pSurface)
{
	xge_texture pTexture;

	if ( (pSurface == NULL) ||
	     (pSurface->iMagic != XUI_PROXY_XGE_SURFACE_MAGIC) ||
	     (pSurface->iKind != XUI_SURFACE_KIND_TEXTURE) ||
	     (pSurface->iFormat != XUI_SURFACE_FORMAT_RGBA8) ) {
		return 0;
	}
	if ( (pSurface->iFlags & XUI_SURFACE_USAGE_TARGET) != 0 ) {
		pTexture = xgeRenderTargetTexture(&pSurface->tTarget);
		return (pTexture != NULL) && (pTexture->iRefCount > 0);
	}
	return (pSurface != NULL) &&
	       (pSurface->iMagic == XUI_PROXY_XGE_SURFACE_MAGIC) &&
	       (pSurface->iKind == XUI_SURFACE_KIND_TEXTURE) &&
	       (pSurface->iFormat == XUI_SURFACE_FORMAT_RGBA8) &&
	       (pSurface->tTexture.iRefCount > 0);
}

static int __xuiProxyXgeSurfaceTargetValid(xui_surface pSurface)
{
	return __xuiProxyXgeSurfaceValid(pSurface) &&
	       ((pSurface->iFlags & XUI_SURFACE_USAGE_TARGET) != 0) &&
	       ((pSurface->tTarget.iFlags & XGE_RENDER_TARGET_TEXTURE) != 0);
}

static int __xuiProxyXgeDrawValid(xui_draw_context pDraw)
{
	return (pDraw != NULL) &&
	       (pDraw->iMagic == XUI_PROXY_XGE_DRAW_MAGIC) &&
	       (pDraw->tPass.bActive != 0) &&
	       __xuiProxyXgeSurfaceTargetValid(pDraw->pTarget);
}

static void __xuiProxyXgeDrawMarkDirty(xui_draw_context pDraw)
{
	if ( __xuiProxyXgeDrawValid(pDraw) ) {
		pDraw->bDirty = 1;
	}
}

static void __xuiProxyXgeFontDestroy(xui_proxy pProxy, xui_font pFont)
{
	if ( pProxy == NULL ) {
		return;
	}
	(void)pProxy;
	if ( !__xuiProxyXgeFontValid(pFont) ) {
		return;
	}
#if (defined(_WIN32) || defined(_WIN64)) && defined(XGE_XRT_PROFILE_IDE) && !defined(XGE_NO_TEXT)
    /* Run backend font pointers refer to this wrapper's embedded fonts. */
    __xuiProxyXgeDrawCacheClear(pFont);
#endif
	xgeFontFree(&pFont->tFont);
	if ( pFont->bHasFallback ) {
		xgeFontFree(&pFont->tFallbackFont);
	}
	pFont->iMagic = 0;
	xrtFree(pFont);
}

static int __xuiProxyXgeTextMeasure(xui_proxy pProxy, xui_font pFont, const char* sText, xui_vec2_t* pSize)
{
	xge_vec2_t tSize;

	if ( (pProxy == NULL) || !__xuiProxyXgeFontValid(pFont) || (sText == NULL) || (pSize == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	(void)pProxy;
#if (defined(_WIN32) || defined(_WIN64)) && defined(XGE_XRT_PROFILE_IDE) && !defined(XGE_NO_TEXT)
    if ( __xuiProxyXgeDrawCacheTextIsLarge(sText) ) __xuiProxyXgeDrawCacheClear(NULL);
#endif
	tSize = xgeTextMeasure(&pFont->tFont, sText);
	pSize->fX = (float)xuiInternalPixelCeil(tSize.fX);
	pSize->fY = (float)xuiInternalPixelCeil(tSize.fY);
	return XGE_OK;
}

static int __xuiProxyXgeTextShape(xui_proxy pProxy, xui_font pFont, const char* sText,
	int iTextSize, uint32_t iFlags, xui_text_shape_t* pShape)
{
	xge_text_shape_desc_t tDesc;
	xge_glyph_run_t tRun;
	xui_text_cluster_t* pCluster;
	uint32_t iXgeFlags = 0;
	int i;
	int iCount;
	int iRet;

	if ( (pProxy == NULL) || !__xuiProxyXgeFontValid(pFont) || (sText == NULL) ||
	     (iTextSize < -1) || (pShape == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	(void)pProxy;
	if ( iTextSize < 0 ) iTextSize = (int)strlen(sText);
#if (defined(_WIN32) || defined(_WIN64)) && defined(XGE_XRT_PROFILE_IDE) && !defined(XGE_NO_TEXT)
    /* Logical layout rebuilds must not overlap the retained physical draw run. */
    if ( iTextSize >= XUI_PROXY_XGE_DRAW_CACHE_MIN ) __xuiProxyXgeDrawCacheClear(NULL);
#endif
	memset(pShape, 0, sizeof(*pShape));
	pShape->iSize = sizeof(*pShape);
	pShape->iFlags = iFlags;
	pShape->iTextSize = iTextSize;
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.pFont = &pFont->tFont;
	tDesc.sText = sText;
	tDesc.iTextSize = iTextSize;
	if ( (iFlags & XUI_TEXT_SHAPE_KERNING) != 0 ) iXgeFlags |= XGE_TEXT_SHAPE_KERNING;
	if ( (iFlags & XUI_TEXT_SHAPE_EMOJI) != 0 ) iXgeFlags |= XGE_TEXT_SHAPE_EMOJI;
	tDesc.iFlags = iXgeFlags;
	tDesc.iEmojiPresentation = XGE_EMOJI_PRESENTATION_AUTO;
	tDesc.iEmojiLinePolicy = XGE_EMOJI_LINE_STABLE;
	tDesc.fEmojiScale = 1.0f;
	memset(&tRun, 0, sizeof(tRun));
	iRet = xgeTextShape(&tDesc, &tRun);
	if ( iRet != XGE_OK ) return iRet;
	pShape->fWidth = tRun.fWidth;
	pShape->fHeight = tRun.fHeight;
	pShape->fAscent = tRun.fAscent;
	pShape->fDescent = tRun.fDescent;
	pShape->fLineHeight = tRun.fLineHeight;
	iCount = 0;
	for ( i = 0; i < tRun.iGlyphCount; i++ ) {
		if ( i == 0 || tRun.pGlyphs[i].iCluster != tRun.pGlyphs[i - 1].iCluster ) iCount++;
	}
	if ( iCount > 0 ) {
		pShape->pClusters = (xui_text_cluster_t*)xrtCalloc((size_t)iCount, sizeof(*pShape->pClusters));
		if ( pShape->pClusters == NULL ) {
			xgeGlyphRunFree(&tRun);
			return XGE_ERROR_OUT_OF_MEMORY;
		}
	}
	pShape->iClusterCount = iCount;
	iCount = -1;
	for ( i = 0; i < tRun.iGlyphCount; i++ ) {
		xge_glyph_position_t* pGlyph = &tRun.pGlyphs[i];
		if ( i == 0 || pGlyph->iCluster != tRun.pGlyphs[i - 1].iCluster ) {
			iCount++;
			pCluster = &pShape->pClusters[iCount];
			pCluster->iSize = sizeof(*pCluster);
			pCluster->iTextStart = (int)pGlyph->iCluster;
			pCluster->iTextEnd = (int)pGlyph->iClusterEnd;
			pCluster->fOffsetX = pGlyph->fOffsetX;
			pCluster->fOffsetY = pGlyph->fOffsetY;
		} else {
			pCluster = &pShape->pClusters[iCount];
			if ((int)pGlyph->iClusterEnd > pCluster->iTextEnd) pCluster->iTextEnd = (int)pGlyph->iClusterEnd;
		}
		pCluster->fAdvance += pGlyph->fAdvanceX;
		if ( (pGlyph->iFlags & XGE_GLYPH_POSITION_LINE_BREAK) != 0 ) pCluster->iFlags |= XUI_TEXT_CLUSTER_LINE_BREAK;
		if ( pGlyph->iItemKind == XGE_TEXT_ITEM_EMOJI ) pCluster->iFlags |= XUI_TEXT_CLUSTER_EMOJI;
	}
	xgeGlyphRunFree(&tRun);
	return XUI_OK;
}

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
		diagOldGlyphRunDrawSpans(&tRun, tRect.fX, tRect.fY, iColor,
			XGE_DRAW_SCREEN_SPACE, pXgeSpans, iSpanCount);
		xgeGlyphRunFree(&tRun);
		__xuiProxyXgeDrawMarkDirty(pDraw);
	}
	if ( pXgeSpans != sSmall ) xrtFree(pXgeSpans);
	(void)iFlags;
	return iRet;
}

static int __xuiProxyXgeDrawTextSpans(xui_proxy pProxy, xui_draw_context pDraw, xui_font pFont,
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
#if (defined(_WIN32) || defined(_WIN64)) && defined(XGE_XRT_PROFILE_IDE) && !defined(XGE_NO_TEXT)
    if ( iTextSize >= XUI_PROXY_XGE_DRAW_CACHE_MIN ) {
        iRet = __xuiProxyXgeDrawCachePaint(pFont, &tDesc, tRect, iColor, pXgeSpans, iSpanCount);
        if ( iRet == XGE_OK ) __xuiProxyXgeDrawMarkDirty(pDraw);
    } else
#endif
    {
	iRet = xgeTextShape(&tDesc, &tRun);
	if ( iRet == XGE_OK ) {
		xgeGlyphRunDrawSpans(&tRun, tRect.fX, tRect.fY, iColor,
			XGE_DRAW_SCREEN_SPACE, pXgeSpans, iSpanCount);
		xgeGlyphRunFree(&tRun);
		__xuiProxyXgeDrawMarkDirty(pDraw);
	}
	}
	if ( pXgeSpans != sSmall ) xrtFree(pXgeSpans);
	(void)iFlags;
	return iRet;
}

