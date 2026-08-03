#ifndef XGE_NO_TEXT
typedef struct xge_glyph_cache_t {
	xge_glyph_t tGlyph;
	struct xge_glyph_cache_t* pNextGlyph;
	struct xge_glyph_cache_t* pNextAll;
} xge_glyph_cache_t;

typedef struct xge_codepoint_cache_t {
	uint32_t iCodepoint;
	xge_glyph_cache_t* pGlyph;
	struct xge_codepoint_cache_t* pNext;
} xge_codepoint_cache_t;

typedef struct xge_font_cache_t {
	uint32_t iBucketCount;
	uint32_t iGlyphCount;
	uint32_t iCodepointCount;
	xge_glyph_cache_t** ppGlyphBuckets;
	xge_codepoint_cache_t** ppCodepointBuckets;
	xge_glyph_cache_t* pGlyphList;
} xge_font_cache_t;

typedef struct xge_glyph_outline_cache_t {
	int iGlyph;
	int iVertexCount;
	stbtt_vertex* pVertices;
	struct xge_glyph_outline_cache_t* pNext;
} xge_glyph_outline_cache_t;

struct xge_font_face_t {
	int iRefCount;
	unsigned char* pData;
	int iDataSize;
	int iFaceIndex;
	int iFontOffset;
	int iWeight;
	int iStretch;
	int iSlant;
	uint32_t iFlags;
	uint64_t iSourceHash;
	int iUnderlinePosition;
	int iUnderlineThickness;
	int iStrikePosition;
	int iStrikeThickness;
	uint32_t iOutlineBucketCount;
	xge_glyph_outline_cache_t** ppOutlineBuckets;
	stbtt_fontinfo tInfo;
};

typedef struct xge_font_instance_backend_t {
	xge_font_face pFace;
	uint64_t iSourceHash;
	int iFaceIndex;
	int iWeight;
	int iStretch;
	int iSlant;
	float fUnderlinePosition;
	float fUnderlineThickness;
	float fStrikePosition;
	float fStrikeThickness;
} xge_font_instance_backend_t;

struct xge_font_family_t {
	int iRefCount;
	int iFaceCount;
	int iFaceCapacity;
	xge_font_face* ppFaces;
};

typedef struct xge_glyph_atlas_page_t {
	int iCursorX;
	int iCursorY;
	int iRowHeight;
	int bDirty;
	int iDirtyX0;
	int iDirtyY0;
	int iDirtyX1;
	int iDirtyY1;
	unsigned char* pPixels;
	xge_texture_t tTexture;
} xge_glyph_atlas_page_t;

#define XGE_FONT_CACHE_INITIAL_BUCKETS 256u
#define XGE_UNICODE_MAX 0x10FFFFu

static xge_font __xgeFontResolveCodepoint(xge_font pFont, uint32_t iCodepoint, int* pGlyph);

static float __xgeTextSnapPixel(float fValue)
{
	return floorf(fValue + 0.5f);
}

static stbtt_fontinfo* __xgeFontInfo(xge_font pFont)
{
	xge_font_instance_backend_t* pBackend;

	if ( pFont == NULL ) {
		return NULL;
	}
	pBackend = (xge_font_instance_backend_t*)pFont->pBackend;
	if ( (pBackend == NULL) || (pBackend->pFace == NULL) ) {
		return NULL;
	}
	return &pBackend->pFace->tInfo;
}

static xge_font_face __xgeFontFace(xge_font pFont)
{
	xge_font_instance_backend_t* pBackend;

	if ( pFont == NULL ) {
		return NULL;
	}
	pBackend = (xge_font_instance_backend_t*)pFont->pBackend;
	return (pBackend != NULL) ? pBackend->pFace : NULL;
}

static uint32_t __xgeFontHashU32(uint32_t iValue)
{
	iValue ^= iValue >> 16;
	iValue *= 0x7feb352du;
	iValue ^= iValue >> 15;
	iValue *= 0x846ca68bu;
	iValue ^= iValue >> 16;
	return iValue;
}

static int __xgeFontGlyphOutlineGet(xge_font pFont, int iGlyph, const stbtt_vertex** ppVertices, int* pVertexCount)
{
	xge_font_face pFace;
	xge_glyph_outline_cache_t* pEntry;
	uint32_t iBucket;

	if ( (pFont == NULL) || (iGlyph < 0) || (ppVertices == NULL) || (pVertexCount == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppVertices = NULL;
	*pVertexCount = 0;
	pFace = __xgeFontFace(pFont);
	if ( pFace == NULL ) return XGE_ERROR_NOT_INITIALIZED;
	if ( pFace->iOutlineBucketCount == 0 ) {
		pFace->iOutlineBucketCount = XGE_FONT_CACHE_INITIAL_BUCKETS;
		pFace->ppOutlineBuckets = (xge_glyph_outline_cache_t**)xrtCalloc(
			pFace->iOutlineBucketCount, sizeof(*pFace->ppOutlineBuckets)
		);
		if ( pFace->ppOutlineBuckets == NULL ) {
			pFace->iOutlineBucketCount = 0;
			return XGE_ERROR_OUT_OF_MEMORY;
		}
	}
	iBucket = __xgeFontHashU32((uint32_t)iGlyph) & (pFace->iOutlineBucketCount - 1u);
	pEntry = pFace->ppOutlineBuckets[iBucket];
	while ( pEntry != NULL ) {
		if ( pEntry->iGlyph == iGlyph ) {
			*ppVertices = pEntry->pVertices;
			*pVertexCount = pEntry->iVertexCount;
			return XGE_OK;
		}
		pEntry = pEntry->pNext;
	}
	pEntry = (xge_glyph_outline_cache_t*)xrtCalloc(1, sizeof(*pEntry));
	if ( pEntry == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
	pEntry->iGlyph = iGlyph;
	pEntry->iVertexCount = stbtt_GetGlyphShape(&pFace->tInfo, iGlyph, &pEntry->pVertices);
	if ( pEntry->iVertexCount < 0 ) {
		xrtFree(pEntry);
		return XGE_ERROR_RESOURCE_FAILED;
	}
	pEntry->pNext = pFace->ppOutlineBuckets[iBucket];
	pFace->ppOutlineBuckets[iBucket] = pEntry;
	*ppVertices = pEntry->pVertices;
	*pVertexCount = pEntry->iVertexCount;
	return XGE_OK;
}

static xge_font_cache_t* __xgeFontCacheGet(xge_font pFont, int bCreate)
{
	xge_font_cache_t* pCache;

	if ( pFont == NULL ) {
		return NULL;
	}
	pCache = (xge_font_cache_t*)pFont->pGlyphs;
	if ( (pCache != NULL) || !bCreate ) {
		return pCache;
	}
	pCache = (xge_font_cache_t*)xrtCalloc(1, sizeof(*pCache));
	if ( pCache == NULL ) {
		return NULL;
	}
	pCache->iBucketCount = XGE_FONT_CACHE_INITIAL_BUCKETS;
	pCache->ppGlyphBuckets = (xge_glyph_cache_t**)xrtCalloc(pCache->iBucketCount, sizeof(*pCache->ppGlyphBuckets));
	pCache->ppCodepointBuckets = (xge_codepoint_cache_t**)xrtCalloc(pCache->iBucketCount, sizeof(*pCache->ppCodepointBuckets));
	if ( (pCache->ppGlyphBuckets == NULL) || (pCache->ppCodepointBuckets == NULL) ) {
		xrtFree(pCache->ppGlyphBuckets);
		xrtFree(pCache->ppCodepointBuckets);
		xrtFree(pCache);
		return NULL;
	}
	pFont->pGlyphs = pCache;
	return pCache;
}

static int __xgeFontCacheRehash(xge_font_cache_t* pCache, uint32_t iBucketCount)
{
	xge_glyph_cache_t** ppGlyphBuckets;
	xge_codepoint_cache_t** ppCodepointBuckets;
	xge_glyph_cache_t* pGlyph;
	xge_codepoint_cache_t* pCodepoint;
	xge_codepoint_cache_t* pNextCodepoint;
	uint32_t i;
	uint32_t iBucket;

	if ( (pCache == NULL) || (iBucketCount <= pCache->iBucketCount) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	ppGlyphBuckets = (xge_glyph_cache_t**)xrtCalloc(iBucketCount, sizeof(*ppGlyphBuckets));
	ppCodepointBuckets = (xge_codepoint_cache_t**)xrtCalloc(iBucketCount, sizeof(*ppCodepointBuckets));
	if ( (ppGlyphBuckets == NULL) || (ppCodepointBuckets == NULL) ) {
		xrtFree(ppGlyphBuckets);
		xrtFree(ppCodepointBuckets);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pGlyph = pCache->pGlyphList;
	while ( pGlyph != NULL ) {
		iBucket = __xgeFontHashU32((uint32_t)pGlyph->tGlyph.iGlyph) & (iBucketCount - 1u);
		pGlyph->pNextGlyph = ppGlyphBuckets[iBucket];
		ppGlyphBuckets[iBucket] = pGlyph;
		pGlyph = pGlyph->pNextAll;
	}
	for ( i = 0; i < pCache->iBucketCount; i++ ) {
		pCodepoint = pCache->ppCodepointBuckets[i];
		while ( pCodepoint != NULL ) {
			pNextCodepoint = pCodepoint->pNext;
			iBucket = __xgeFontHashU32(pCodepoint->iCodepoint) & (iBucketCount - 1u);
			pCodepoint->pNext = ppCodepointBuckets[iBucket];
			ppCodepointBuckets[iBucket] = pCodepoint;
			pCodepoint = pNextCodepoint;
		}
	}
	xrtFree(pCache->ppGlyphBuckets);
	xrtFree(pCache->ppCodepointBuckets);
	pCache->ppGlyphBuckets = ppGlyphBuckets;
	pCache->ppCodepointBuckets = ppCodepointBuckets;
	pCache->iBucketCount = iBucketCount;
	return XGE_OK;
}

static void __xgeFontCacheGrow(xge_font_cache_t* pCache)
{
	if ( (pCache != NULL) && ((pCache->iGlyphCount + pCache->iCodepointCount) > (pCache->iBucketCount * 3u)) && (pCache->iBucketCount <= (UINT32_MAX / 2u)) ) {
		(void)__xgeFontCacheRehash(pCache, pCache->iBucketCount * 2u);
	}
}

static xge_glyph_cache_t* __xgeFontGlyphFindByIndex(xge_font pFont, int iGlyph)
{
	xge_font_cache_t* pCache;
	xge_glyph_cache_t* pGlyph;
	uint32_t iBucket;

	pCache = __xgeFontCacheGet(pFont, 0);
	if ( (pCache == NULL) || (pCache->iBucketCount == 0) ) {
		return NULL;
	}
	iBucket = __xgeFontHashU32((uint32_t)iGlyph) & (pCache->iBucketCount - 1u);
	pGlyph = pCache->ppGlyphBuckets[iBucket];
	while ( pGlyph != NULL ) {
		if ( pGlyph->tGlyph.iGlyph == iGlyph ) {
			return pGlyph;
		}
		pGlyph = pGlyph->pNextGlyph;
	}
	return NULL;
}

static xge_glyph_cache_t* __xgeFontGlyphFind(xge_font pFont, uint32_t iCodepoint)
{
	xge_font_cache_t* pCache;
	xge_codepoint_cache_t* pCodepoint;
	uint32_t iBucket;

	pCache = __xgeFontCacheGet(pFont, 0);
	if ( (pCache == NULL) || (pCache->iBucketCount == 0) ) {
		return NULL;
	}
	iBucket = __xgeFontHashU32(iCodepoint) & (pCache->iBucketCount - 1u);
	pCodepoint = pCache->ppCodepointBuckets[iBucket];
	while ( pCodepoint != NULL ) {
		if ( pCodepoint->iCodepoint == iCodepoint ) {
			return pCodepoint->pGlyph;
		}
		pCodepoint = pCodepoint->pNext;
	}
	return NULL;
}

static int __xgeFontCacheMapCodepoint(xge_font pFont, uint32_t iCodepoint, xge_glyph_cache_t* pGlyph)
{
	xge_font_cache_t* pCache;
	xge_codepoint_cache_t* pCodepoint;
	uint32_t iBucket;

	if ( (pFont == NULL) || (pGlyph == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pCache = __xgeFontCacheGet(pFont, 1);
	if ( pCache == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	if ( __xgeFontGlyphFind(pFont, iCodepoint) != NULL ) {
		return XGE_OK;
	}
	pCodepoint = (xge_codepoint_cache_t*)xrtMalloc(sizeof(*pCodepoint));
	if ( pCodepoint == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iBucket = __xgeFontHashU32(iCodepoint) & (pCache->iBucketCount - 1u);
	pCodepoint->iCodepoint = iCodepoint;
	pCodepoint->pGlyph = pGlyph;
	pCodepoint->pNext = pCache->ppCodepointBuckets[iBucket];
	pCache->ppCodepointBuckets[iBucket] = pCodepoint;
	pCache->iCodepointCount++;
	__xgeFontCacheGrow(pCache);
	return XGE_OK;
}

static int __xgeFontCacheAddGlyph(xge_font pFont, xge_glyph_cache_t* pGlyph)
{
	xge_font_cache_t* pCache;
	uint32_t iBucket;

	if ( (pFont == NULL) || (pGlyph == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pCache = __xgeFontCacheGet(pFont, 1);
	if ( pCache == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iBucket = __xgeFontHashU32((uint32_t)pGlyph->tGlyph.iGlyph) & (pCache->iBucketCount - 1u);
	pGlyph->pNextGlyph = pCache->ppGlyphBuckets[iBucket];
	pCache->ppGlyphBuckets[iBucket] = pGlyph;
	pGlyph->pNextAll = pCache->pGlyphList;
	pCache->pGlyphList = pGlyph;
	pCache->iGlyphCount++;
	__xgeFontCacheGrow(pCache);
	return XGE_OK;
}

static int __xgeFontFallbackUse(xge_font pFont, float fSize, int iOriginalError)
{
	if ( (pFont == NULL) || (g_xge.tFallbackFont.iRefCount <= 0) ) {
		return iOriginalError;
	}
	if ( xgeFontFallbackGet(pFont, fSize) != XGE_OK ) {
		return iOriginalError;
	}
	return XGE_OK;
}

static void __xgeFontAtlasFree(xge_font pFont)
{
	xge_glyph_atlas_page_t* pPages;
	xge_font_cache_t* pCache;
	xge_glyph_cache_t* pGlyph;
	xge_glyph_cache_t* pNextGlyph;
	xge_codepoint_cache_t* pCodepoint;
	xge_codepoint_cache_t* pNextCodepoint;
	int i;

	if ( pFont == NULL ) {
		return;
	}
	pPages = (xge_glyph_atlas_page_t*)pFont->tAtlas.pPages;
	if ( pPages != NULL ) {
		for ( i = 0; i < pFont->tAtlas.iPageCount; i++ ) {
			xgeTextureFree(&pPages[i].tTexture);
			xrtFree(pPages[i].pPixels);
		}
		xrtFree(pPages);
	}
	pCache = (xge_font_cache_t*)pFont->pGlyphs;
	if ( pCache != NULL ) {
		pGlyph = pCache->pGlyphList;
		while ( pGlyph != NULL ) {
			pNextGlyph = pGlyph->pNextAll;
			xrtFree(pGlyph);
			pGlyph = pNextGlyph;
		}
		for ( i = 0; i < (int)pCache->iBucketCount; i++ ) {
			pCodepoint = pCache->ppCodepointBuckets[i];
			while ( pCodepoint != NULL ) {
				pNextCodepoint = pCodepoint->pNext;
				xrtFree(pCodepoint);
				pCodepoint = pNextCodepoint;
			}
		}
		xrtFree(pCache->ppGlyphBuckets);
		xrtFree(pCache->ppCodepointBuckets);
		xrtFree(pCache);
	}
	pFont->tAtlas.pPages = NULL;
	pFont->tAtlas.iPageCount = 0;
	pFont->pGlyphs = NULL;
}

static int __xgeRangeInsideU64(uint64_t iOffset, uint64_t iSize, int iTotal)
{
	if ( iTotal < 0 ) return 0;
	if ( iOffset > (uint64_t)iTotal ) return 0;
	return iSize <= ((uint64_t)iTotal - iOffset);
}

static xge_glyph_atlas_page_t* __xgeFontAtlasAddPage(xge_font pFont)
{
	xge_glyph_atlas_page_t* pPages;
	xge_glyph_atlas_page_t* pPage;
	int iNewCount;
	size_t iPixelSize;

	if ( (pFont == NULL) || (pFont->tAtlas.iPageWidth <= 0) || (pFont->tAtlas.iPageHeight <= 0) ||
	     ((size_t)pFont->tAtlas.iPageWidth > SIZE_MAX / (size_t)pFont->tAtlas.iPageHeight) ||
	     ((size_t)pFont->tAtlas.iPageWidth * (size_t)pFont->tAtlas.iPageHeight > SIZE_MAX / 4u) ) {
		return NULL;
	}

	iNewCount = pFont->tAtlas.iPageCount + 1;
	pPages = (xge_glyph_atlas_page_t*)xrtRealloc(pFont->tAtlas.pPages, sizeof(*pPages) * (size_t)iNewCount);
	if ( pPages == NULL ) {
		return NULL;
	}
	pFont->tAtlas.pPages = pPages;
	pPage = &pPages[iNewCount - 1];
	memset(pPage, 0, sizeof(*pPage));
	iPixelSize = (size_t)pFont->tAtlas.iPageWidth * (size_t)pFont->tAtlas.iPageHeight * 4u;
	pPage->pPixels = (unsigned char*)xrtMalloc(iPixelSize);
	if ( pPage->pPixels == NULL ) {
		return NULL;
	}
	memset(pPage->pPixels, 0, iPixelSize);
	pPage->bDirty = 1;
	pPage->iDirtyX0 = 0;
	pPage->iDirtyY0 = 0;
	pPage->iDirtyX1 = pFont->tAtlas.iPageWidth;
	pPage->iDirtyY1 = pFont->tAtlas.iPageHeight;
	pFont->tAtlas.iPageCount = iNewCount;
	return pPage;
}

static xge_glyph_atlas_page_t* __xgeFontAtlasPlace(xge_font pFont, int iWidth, int iHeight, int* pPageIndex, int* pX, int* pY)
{
	xge_glyph_atlas_page_t* pPages;
	xge_glyph_atlas_page_t* pPage;
	int i;

	if ( (iWidth > pFont->tAtlas.iPageWidth) || (iHeight > pFont->tAtlas.iPageHeight) ) {
		return NULL;
	}
	pPages = (xge_glyph_atlas_page_t*)pFont->tAtlas.pPages;
	for ( i = 0; i < pFont->tAtlas.iPageCount; i++ ) {
		pPage = &pPages[i];
		if ( pPage->iCursorX + iWidth > pFont->tAtlas.iPageWidth ) {
			pPage->iCursorX = 0;
			pPage->iCursorY += pPage->iRowHeight + 1;
			pPage->iRowHeight = 0;
		}
		if ( pPage->iCursorY + iHeight <= pFont->tAtlas.iPageHeight ) {
			*pPageIndex = i;
			*pX = pPage->iCursorX;
			*pY = pPage->iCursorY;
			pPage->iCursorX += iWidth + 1;
			if ( iHeight > pPage->iRowHeight ) {
				pPage->iRowHeight = iHeight;
			}
			return pPage;
		}
	}
	pPage = __xgeFontAtlasAddPage(pFont);
	if ( pPage == NULL ) {
		return NULL;
	}
	*pPageIndex = pFont->tAtlas.iPageCount - 1;
	*pX = 0;
	*pY = 0;
	pPage->iCursorX = iWidth + 1;
	pPage->iCursorY = 0;
	pPage->iRowHeight = iHeight;
	return pPage;
}

static void __xgeFontAtlasBlit(xge_font pFont, xge_glyph_atlas_page_t* pPage, int iX, int iY, const xge_glyph_bitmap_t* pBitmap)
{
	unsigned char* pDst;
	unsigned char* pSrc;
	int iRow;
	int iCol;
	int iDstPos;

	for ( iRow = 0; iRow < pBitmap->iHeight; iRow++ ) {
		for ( iCol = 0; iCol < pBitmap->iWidth; iCol++ ) {
			pSrc = ((unsigned char*)pBitmap->pPixels) + iRow * pBitmap->iStride + iCol;
			iDstPos = (((iY + iRow) * pFont->tAtlas.iPageWidth) + (iX + iCol)) * 4;
			pDst = pPage->pPixels + iDstPos;
			pDst[0] = *pSrc;
			pDst[1] = *pSrc;
			pDst[2] = *pSrc;
			pDst[3] = *pSrc;
		}
	}
	if ( !pPage->bDirty ) {
		pPage->iDirtyX0 = iX;
		pPage->iDirtyY0 = iY;
		pPage->iDirtyX1 = iX + pBitmap->iWidth;
		pPage->iDirtyY1 = iY + pBitmap->iHeight;
		pPage->bDirty = 1;
	} else {
		if ( iX < pPage->iDirtyX0 ) pPage->iDirtyX0 = iX;
		if ( iY < pPage->iDirtyY0 ) pPage->iDirtyY0 = iY;
		if ( iX + pBitmap->iWidth > pPage->iDirtyX1 ) pPage->iDirtyX1 = iX + pBitmap->iWidth;
		if ( iY + pBitmap->iHeight > pPage->iDirtyY1 ) pPage->iDirtyY1 = iY + pBitmap->iHeight;
	}
}

static int __xgeFontAtlasPageSetPixels(xge_font pFont, int iPage, int iWidth, int iHeight, int iFormat, const unsigned char* pPixels, int iPixelSize)
{
	xge_glyph_atlas_page_t* pPage;
	xge_glyph_atlas_page_t* pPages;
	int i;
	size_t iPixelCount;
	size_t iRGBABytes;

	if ( (pFont == NULL) || (pPixels == NULL) || (iPage < 0) || (iWidth <= 0) || (iHeight <= 0) ||
	     (iPixelSize < 0) || ((size_t)iWidth > SIZE_MAX / (size_t)iHeight) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iPixelCount = (size_t)iWidth * (size_t)iHeight;
	if ( iPixelCount > SIZE_MAX / 4u ) return XGE_ERROR_OUT_OF_MEMORY;
	iRGBABytes = iPixelCount * 4u;

	while ( pFont->tAtlas.iPageCount <= iPage ) {
		if ( __xgeFontAtlasAddPage(pFont) == NULL ) {
			return XGE_ERROR_OUT_OF_MEMORY;
		}
	}
	pPages = (xge_glyph_atlas_page_t*)pFont->tAtlas.pPages;
	pPage = &pPages[iPage];
	if ( (iWidth != pFont->tAtlas.iPageWidth) || (iHeight != pFont->tAtlas.iPageHeight) ) {
		return XGE_ERROR_UNSUPPORTED;
	}
	if ( iFormat == XGE_XRF_PAGE_A8 ) {
		if ( (size_t)iPixelSize < iPixelCount ) {
			return XGE_ERROR_RESOURCE_FAILED;
		}
		memset(pPage->pPixels, 0, (size_t)iRGBABytes);
		for ( i = 0; (size_t)i < iPixelCount; i++ ) {
			pPage->pPixels[i * 4 + 0] = pPixels[i];
			pPage->pPixels[i * 4 + 1] = pPixels[i];
			pPage->pPixels[i * 4 + 2] = pPixels[i];
			pPage->pPixels[i * 4 + 3] = pPixels[i];
		}
	} else if ( iFormat == XGE_XRF_PAGE_RGBA8 ) {
		if ( (size_t)iPixelSize < iRGBABytes ) {
			return XGE_ERROR_RESOURCE_FAILED;
		}
		memcpy(pPage->pPixels, pPixels, (size_t)iRGBABytes);
	} else {
		return XGE_ERROR_UNSUPPORTED;
	}
	pPage->bDirty = 1;
	pPage->iDirtyX0 = 0;
	pPage->iDirtyY0 = 0;
	pPage->iDirtyX1 = iWidth;
	pPage->iDirtyY1 = iHeight;
	return XGE_OK;
}

static int __xgeFontAtlasUploadPage(xge_font pFont, int iPage)
{
	xge_glyph_atlas_page_t* pPages;
	xge_glyph_atlas_page_t* pPage;
	xge_sampler_t tSampler;
	int iDirtyWidth;
	int iDirtyHeight;
	const unsigned char* pDirtyPixels;

	if ( (pFont == NULL) || (iPage < 0) || (iPage >= pFont->tAtlas.iPageCount) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( g_xge.bSokolRunning == 0 ) {
		return XGE_ERROR_NOT_INITIALIZED;
	}
	pPages = (xge_glyph_atlas_page_t*)pFont->tAtlas.pPages;
	pPage = &pPages[iPage];
	if ( pPage->tTexture.iBackendId == 0 ) {
		if ( xgeTextureCreateRGBA(&pPage->tTexture, pFont->tAtlas.iPageWidth, pFont->tAtlas.iPageHeight, pPage->pPixels) != XGE_OK ) {
			return XGE_ERROR_GPU_FAILED;
		}
		tSampler = xgeSamplerDefault();
		tSampler.iMinFilter = XGE_FILTER_NEAREST;
		tSampler.iMagFilter = XGE_FILTER_NEAREST;
		(void)xgeTextureSetSampler(&pPage->tTexture, &tSampler);
		pPage->bDirty = 0;
		return XGE_OK;
	}
	if ( pPage->bDirty ) {
		iDirtyWidth = pPage->iDirtyX1 - pPage->iDirtyX0;
		iDirtyHeight = pPage->iDirtyY1 - pPage->iDirtyY0;
		if ( (iDirtyWidth <= 0) || (iDirtyHeight <= 0) ) {
			pPage->bDirty = 0;
			return XGE_OK;
		}
		pDirtyPixels = pPage->pPixels + (((size_t)pPage->iDirtyY0 * (size_t)pFont->tAtlas.iPageWidth + (size_t)pPage->iDirtyX0) * 4u);
		if ( xgeTextureUpdateRGBA(&pPage->tTexture, pPage->iDirtyX0, pPage->iDirtyY0, iDirtyWidth, iDirtyHeight, pDirtyPixels, pFont->tAtlas.iPageWidth * 4) != XGE_OK ) {
			return XGE_ERROR_GPU_FAILED;
		}
		pPage->bDirty = 0;
	}
	return XGE_OK;
}

int xgeTextUTF8Next(const char** psText, uint32_t* pCodepoint)
{
	const unsigned char* pText;
	uint32_t iCodepoint;
	int iCount;

	if ( (psText == NULL) || (*psText == NULL) || (pCodepoint == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pText = (const unsigned char*)*psText;
	if ( pText[0] == 0 ) {
		return XGE_ERROR;
	}
	if ( pText[0] < 0x80 ) {
		iCodepoint = pText[0];
		iCount = 1;
	} else if ( (pText[0] & 0xE0) == 0xC0 ) {
		if ( (pText[1] & 0xC0) != 0x80 ) {
			return XGE_ERROR_UNSUPPORTED;
		}
		iCodepoint = ((uint32_t)(pText[0] & 0x1F) << 6) | (uint32_t)(pText[1] & 0x3F);
		iCount = 2;
		if ( iCodepoint < 0x80 ) {
			return XGE_ERROR_UNSUPPORTED;
		}
	} else if ( (pText[0] & 0xF0) == 0xE0 ) {
		if ( (pText[1] == 0) || (pText[2] == 0) || ((pText[1] & 0xC0) != 0x80) || ((pText[2] & 0xC0) != 0x80) ) {
			return XGE_ERROR_UNSUPPORTED;
		}
		iCodepoint = ((uint32_t)(pText[0] & 0x0F) << 12) | ((uint32_t)(pText[1] & 0x3F) << 6) | (uint32_t)(pText[2] & 0x3F);
		iCount = 3;
		if ( iCodepoint < 0x800 ) {
			return XGE_ERROR_UNSUPPORTED;
		}
	} else if ( (pText[0] & 0xF8) == 0xF0 ) {
		if ( (pText[1] == 0) || (pText[2] == 0) || (pText[3] == 0) ||
		     ((pText[1] & 0xC0) != 0x80) || ((pText[2] & 0xC0) != 0x80) || ((pText[3] & 0xC0) != 0x80) ) {
			return XGE_ERROR_UNSUPPORTED;
		}
		iCodepoint = ((uint32_t)(pText[0] & 0x07) << 18) | ((uint32_t)(pText[1] & 0x3F) << 12) |
		             ((uint32_t)(pText[2] & 0x3F) << 6) | (uint32_t)(pText[3] & 0x3F);
		iCount = 4;
		if ( iCodepoint < 0x10000 ) {
			return XGE_ERROR_UNSUPPORTED;
		}
	} else {
		return XGE_ERROR_UNSUPPORTED;
	}
	if ( (iCodepoint >= 0xD800 && iCodepoint <= 0xDFFF) || (iCodepoint > XGE_UNICODE_MAX) ) {
		return XGE_ERROR_UNSUPPORTED;
	}
	*pCodepoint = iCodepoint;
	*psText += iCount;
	return XGE_OK;
}

static uint16_t __xgeFontReadU16BE(const unsigned char* pData)
{
	return (uint16_t)(((uint16_t)pData[0] << 8) | (uint16_t)pData[1]);
}

static int16_t __xgeFontReadS16BE(const unsigned char* pData)
{
	return (int16_t)__xgeFontReadU16BE(pData);
}

static uint32_t __xgeFontReadU32BE(const unsigned char* pData)
{
	return ((uint32_t)pData[0] << 24) | ((uint32_t)pData[1] << 16) | ((uint32_t)pData[2] << 8) | (uint32_t)pData[3];
}

static uint64_t __xgeFontHashBytes(const unsigned char* pData, int iSize)
{
	uint64_t iHash;
	int i;

	iHash = UINT64_C(1469598103934665603);
	for ( i = 0; i < iSize; i++ ) {
		iHash ^= (uint64_t)pData[i];
		iHash *= UINT64_C(1099511628211);
	}
	return iHash;
}

static int __xgeFontFaceTable(xge_font_face pFace, uint32_t iTag, const unsigned char** ppTable, uint32_t* pLength)
{
	const unsigned char* pDirectory;
	const unsigned char* pRecord;
	uint32_t iOffset;
	uint32_t iLength;
	uint16_t iTableCount;
	int i;

	if ( (pFace == NULL) || (ppTable == NULL) || (pLength == NULL) || (pFace->iFontOffset < 0) || (pFace->iFontOffset > pFace->iDataSize - 12) ) {
		return 0;
	}
	pDirectory = pFace->pData + pFace->iFontOffset;
	iTableCount = __xgeFontReadU16BE(pDirectory + 4);
	if ( (int)iTableCount > ((pFace->iDataSize - pFace->iFontOffset - 12) / 16) ) {
		return 0;
	}
	for ( i = 0; i < (int)iTableCount; i++ ) {
		pRecord = pDirectory + 12 + (i * 16);
		if ( __xgeFontReadU32BE(pRecord) != iTag ) {
			continue;
		}
		iOffset = __xgeFontReadU32BE(pRecord + 8);
		iLength = __xgeFontReadU32BE(pRecord + 12);
		if ( (iOffset > (uint32_t)pFace->iDataSize) || (iLength > ((uint32_t)pFace->iDataSize - iOffset)) ) {
			return 0;
		}
		*ppTable = pFace->pData + iOffset;
		*pLength = iLength;
		return 1;
	}
	return 0;
}

static void __xgeFontFaceReadMetadata(xge_font_face pFace)
{
	static const int arrStretch[] = { 100, 50, 63, 75, 88, 100, 113, 125, 150, 200 };
	const unsigned char* pTable;
	uint32_t iLength;
	uint16_t iSelection;
	uint16_t iMacStyle;

	pFace->iWeight = XGE_FONT_WEIGHT_NORMAL;
	pFace->iStretch = XGE_FONT_STRETCH_NORMAL;
	pFace->iSlant = XGE_FONT_SLANT_NORMAL;
	pFace->iUnderlinePosition = 0;
	pFace->iUnderlineThickness = 0;
	pFace->iStrikePosition = 0;
	pFace->iStrikeThickness = 0;
	if ( __xgeFontFaceTable(pFace, 0x4F532F32u, &pTable, &iLength) ) {
		if ( iLength >= 8 ) {
			pFace->iWeight = (int)__xgeFontReadU16BE(pTable + 4);
			iSelection = __xgeFontReadU16BE(pTable + 6);
			pFace->iStretch = (iSelection < (uint16_t)(sizeof(arrStretch) / sizeof(arrStretch[0]))) ? arrStretch[iSelection] : XGE_FONT_STRETCH_NORMAL;
		}
		if ( iLength >= 30 ) {
			pFace->iStrikeThickness = (int)__xgeFontReadS16BE(pTable + 26);
			pFace->iStrikePosition = (int)__xgeFontReadS16BE(pTable + 28);
		}
		if ( iLength >= 64 ) {
			iSelection = __xgeFontReadU16BE(pTable + 62);
			if ( (iSelection & 0x0001u) != 0 ) pFace->iSlant = XGE_FONT_SLANT_ITALIC;
			if ( (iSelection & 0x0200u) != 0 ) pFace->iSlant = XGE_FONT_SLANT_OBLIQUE;
		}
	}
	if ( __xgeFontFaceTable(pFace, 0x68656164u, &pTable, &iLength) && (iLength >= 46) ) {
		iMacStyle = __xgeFontReadU16BE(pTable + 44);
		if ( ((iMacStyle & 0x0002u) != 0) && (pFace->iSlant == XGE_FONT_SLANT_NORMAL) ) {
			pFace->iSlant = XGE_FONT_SLANT_ITALIC;
		}
		if ( ((iMacStyle & 0x0001u) != 0) && (pFace->iWeight < XGE_FONT_WEIGHT_BOLD) ) {
			pFace->iWeight = XGE_FONT_WEIGHT_BOLD;
		}
	}
	if ( __xgeFontFaceTable(pFace, 0x706F7374u, &pTable, &iLength) && (iLength >= 12) ) {
		pFace->iUnderlinePosition = (int)__xgeFontReadS16BE(pTable + 8);
		pFace->iUnderlineThickness = (int)__xgeFontReadS16BE(pTable + 10);
	}
}

int xgeFontFaceLoadMemory(xge_font_face* ppFace, const void* pData, int iSize, const xge_font_face_desc_t* pDesc)
{
	xge_font_face pFace;
	int iFaceIndex;
	int iOffset;

	if ( (ppFace == NULL) || (pData == NULL) || (iSize <= 0) ||
	     ((pDesc != NULL) && (pDesc->iSize < sizeof(*pDesc))) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	*ppFace = NULL;
	iFaceIndex = (pDesc != NULL) ? pDesc->iFaceIndex : 0;
	if ( iFaceIndex < 0 ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pFace = (xge_font_face)xrtCalloc(1, sizeof(*pFace));
	if ( pFace == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pFace->pData = (unsigned char*)xrtMalloc((size_t)iSize);
	if ( pFace->pData == NULL ) {
		xrtFree(pFace);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	memcpy(pFace->pData, pData, (size_t)iSize);
	iOffset = stbtt_GetFontOffsetForIndex(pFace->pData, iFaceIndex);
	if ( (iOffset < 0) || (stbtt_InitFont(&pFace->tInfo, pFace->pData, iOffset) == 0) ) {
		xrtFree(pFace->pData);
		xrtFree(pFace);
		return XGE_ERROR_RESOURCE_FAILED;
	}
	pFace->iRefCount = 1;
	pFace->iDataSize = iSize;
	pFace->iFaceIndex = iFaceIndex;
	pFace->iFontOffset = iOffset;
	pFace->iFlags = (pDesc != NULL) ? pDesc->iFlags : 0;
	pFace->iSourceHash = __xgeFontHashBytes(pFace->pData, iSize);
	__xgeFontFaceReadMetadata(pFace);
	*ppFace = pFace;
	return XGE_OK;
}

int xgeFontFaceLoad(xge_font_face* ppFace, const char* sPath, const xge_font_face_desc_t* pDesc)
{
	xge_resource_t tResource;
	int iRet;

	if ( (ppFace == NULL) || (sPath == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = xgeResourceLoad(sPath, &tResource);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = xgeFontFaceLoadMemory(ppFace, tResource.pData, tResource.iSize, pDesc);
	xgeResourceFree(&tResource);
	return iRet;
}

int xgeFontFaceAddRef(xge_font_face pFace)
{
	if ( pFace == NULL ) return 0;
	if ( pFace->iRefCount < INT32_MAX ) pFace->iRefCount++;
	return pFace->iRefCount;
}

void xgeFontFaceFree(xge_font_face pFace)
{
	xge_glyph_outline_cache_t* pOutline;
	xge_glyph_outline_cache_t* pNextOutline;
	uint32_t i;

	if ( pFace == NULL ) return;
	if ( pFace->iRefCount > 1 ) {
		pFace->iRefCount--;
		return;
	}
	for ( i = 0; i < pFace->iOutlineBucketCount; i++ ) {
		pOutline = pFace->ppOutlineBuckets[i];
		while ( pOutline != NULL ) {
			pNextOutline = pOutline->pNext;
			if ( pOutline->pVertices != NULL ) stbtt_FreeShape(&pFace->tInfo, pOutline->pVertices);
			xrtFree(pOutline);
			pOutline = pNextOutline;
		}
	}
	xrtFree(pFace->ppOutlineBuckets);
	xrtFree(pFace->pData);
	memset(pFace, 0, sizeof(*pFace));
	xrtFree(pFace);
}

int xgeFontFaceGetInfo(xge_font_face pFace, xge_font_face_info_t* pInfo)
{
	if ( (pFace == NULL) || (pInfo == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	memset(pInfo, 0, sizeof(*pInfo));
	pInfo->iSize = sizeof(*pInfo);
	pInfo->iFaceIndex = pFace->iFaceIndex;
	pInfo->iWeight = pFace->iWeight;
	pInfo->iStretch = pFace->iStretch;
	pInfo->iSlant = pFace->iSlant;
	pInfo->iSourceHash = pFace->iSourceHash;
	pInfo->iFlags = pFace->iFlags;
	return XGE_OK;
}

int xgeFontFamilyCreate(xge_font_family* ppFamily)
{
	xge_font_family pFamily;

	if ( ppFamily == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
	*ppFamily = NULL;
	pFamily = (xge_font_family)xrtCalloc(1, sizeof(*pFamily));
	if ( pFamily == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
	pFamily->iRefCount = 1;
	*ppFamily = pFamily;
	return XGE_OK;
}

void xgeFontFamilyFree(xge_font_family pFamily)
{
	int i;

	if ( pFamily == NULL ) return;
	if ( pFamily->iRefCount > 1 ) {
		pFamily->iRefCount--;
		return;
	}
	for ( i = 0; i < pFamily->iFaceCount; i++ ) xgeFontFaceFree(pFamily->ppFaces[i]);
	xrtFree(pFamily->ppFaces);
	memset(pFamily, 0, sizeof(*pFamily));
	xrtFree(pFamily);
}

int xgeFontFamilyAddFace(xge_font_family pFamily, xge_font_face pFace)
{
	xge_font_face* ppFaces;
	int iCapacity;
	int i;

	if ( (pFamily == NULL) || (pFace == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	for ( i = 0; i < pFamily->iFaceCount; i++ ) {
		if ( pFamily->ppFaces[i] == pFace ) return XGE_OK;
	}
	if ( pFamily->iFaceCount >= pFamily->iFaceCapacity ) {
		iCapacity = (pFamily->iFaceCapacity > 0) ? pFamily->iFaceCapacity * 2 : 4;
		ppFaces = (xge_font_face*)xrtRealloc(pFamily->ppFaces, sizeof(*ppFaces) * (size_t)iCapacity);
		if ( ppFaces == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
		pFamily->ppFaces = ppFaces;
		pFamily->iFaceCapacity = iCapacity;
	}
	xgeFontFaceAddRef(pFace);
	pFamily->ppFaces[pFamily->iFaceCount++] = pFace;
	return XGE_OK;
}

int xgeFontFamilyResolveEx(xge_font_family pFamily, int iWeight, int iStretch, int iSlant, xge_font_face* ppFace)
{
	xge_font_face pBest;
	int iBestScore;
	int iScore;
	int i;

	if ( (pFamily == NULL) || (ppFace == NULL) || (pFamily->iFaceCount <= 0) ) return XGE_ERROR_INVALID_ARGUMENT;
	*ppFace = NULL;
	if ( iWeight <= 0 ) iWeight = XGE_FONT_WEIGHT_NORMAL;
	if ( iStretch <= 0 ) iStretch = XGE_FONT_STRETCH_NORMAL;
	pBest = NULL;
	iBestScore = INT32_MAX;
	for ( i = 0; i < pFamily->iFaceCount; i++ ) {
		iScore = abs(pFamily->ppFaces[i]->iWeight - iWeight);
		iScore += abs(pFamily->ppFaces[i]->iStretch - iStretch) * 4;
		if ( pFamily->ppFaces[i]->iSlant != iSlant ) iScore += 1000;
		if ( iScore < iBestScore ) {
			iBestScore = iScore;
			pBest = pFamily->ppFaces[i];
		}
	}
	if ( pBest == NULL ) return XGE_ERROR_RESOURCE_FAILED;
	xgeFontFaceAddRef(pBest);
	*ppFace = pBest;
	return XGE_OK;
}

int xgeFontFamilyResolve(xge_font_family pFamily, int iWeight, int iSlant, xge_font_face* ppFace)
{
	return xgeFontFamilyResolveEx(pFamily, iWeight, XGE_FONT_STRETCH_NORMAL, iSlant, ppFace);
}

int xgeFontCreate(xge_font pFont, xge_font_face pFace, const xge_font_instance_desc_t* pDesc)
{
	xge_font_instance_backend_t* pBackend;
	float fSize;
	int iAscent;
	int iDescent;
	int iLineGap;

	if ( (pFont == NULL) || (pFace == NULL) || (pDesc == NULL) ||
	     (pDesc->iSize < sizeof(*pDesc)) || (pDesc->fPixelSize <= 0.0f) ||
	     (pFont->iRefCount > 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	fSize = pDesc->fPixelSize;
	memset(pFont, 0, sizeof(*pFont));
	pBackend = (xge_font_instance_backend_t*)xrtCalloc(1, sizeof(*pBackend));
	if ( pBackend == NULL ) return XGE_ERROR_OUT_OF_MEMORY;
	xgeFontFaceAddRef(pFace);
	pBackend->pFace = pFace;
	stbtt_GetFontVMetrics(&pFace->tInfo, &iAscent, &iDescent, &iLineGap);
	pFont->iRefCount = 1;
	g_xge.iFontCount++;
	pFont->fSize = fSize;
	pFont->fScale = ((pDesc->iFlags & XGE_FONT_SIZE_EM) != 0)
		? stbtt_ScaleForMappingEmToPixels(&pFace->tInfo, fSize)
		: stbtt_ScaleForPixelHeight(&pFace->tInfo, fSize);
	pFont->fAscent = (float)iAscent * pFont->fScale;
	pFont->fDescent = (float)iDescent * pFont->fScale;
	pFont->fLineGap = (float)iLineGap * pFont->fScale;
	pFont->fLineHeight = pFont->fAscent - pFont->fDescent + pFont->fLineGap;
	pBackend->iSourceHash = pFace->iSourceHash;
	pBackend->iFaceIndex = pFace->iFaceIndex;
	pBackend->iWeight = pFace->iWeight;
	pBackend->iStretch = pFace->iStretch;
	pBackend->iSlant = pFace->iSlant;
	pBackend->fUnderlinePosition = -(float)pFace->iUnderlinePosition * pFont->fScale;
	pBackend->fUnderlineThickness = (float)pFace->iUnderlineThickness * pFont->fScale;
	pBackend->fStrikePosition = -(float)pFace->iStrikePosition * pFont->fScale;
	pBackend->fStrikeThickness = (float)pFace->iStrikeThickness * pFont->fScale;
	pFont->iFlags = pDesc->iFlags;
	pFont->pData = pFace->pData;
	pFont->iDataSize = pFace->iDataSize;
	pFont->pBackend = pBackend;
	pFont->tAtlas.iPageWidth = 512;
	pFont->tAtlas.iPageHeight = 512;
	pFont->tAtlas.iFormat = XGE_PIXEL_RGBA8;
	return XGE_OK;
}

int xgeFontLoad(xge_font pFont, const char* sPath, float fSize)
{
	xge_font_face pFace;
	xge_font_instance_desc_t tDesc;
	int iRet;

	if ( (pFont == NULL) || (sPath == NULL) || (fSize <= 0.0f) ) return XGE_ERROR_INVALID_ARGUMENT;
	pFace = NULL;
	iRet = xgeFontFaceLoad(&pFace, sPath, NULL);
	if ( iRet != XGE_OK ) return __xgeFontFallbackUse(pFont, fSize, iRet);
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.fPixelSize = fSize;
	iRet = xgeFontCreate(pFont, pFace, &tDesc);
	xgeFontFaceFree(pFace);
	if ( iRet != XGE_OK ) return __xgeFontFallbackUse(pFont, fSize, iRet);
	return XGE_OK;
}

int xgeFontLoadMemory(xge_font pFont, const void* pData, int iSize, float fSize)
{
	xge_font_face pFace;
	xge_font_instance_desc_t tDesc;
	int iRet;

	if ( (pFont == NULL) || (pData == NULL) || (iSize <= 0) || (fSize <= 0.0f) ) return XGE_ERROR_INVALID_ARGUMENT;
	pFace = NULL;
	iRet = xgeFontFaceLoadMemory(&pFace, pData, iSize, NULL);
	if ( iRet != XGE_OK ) return iRet;
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.fPixelSize = fSize;
	iRet = xgeFontCreate(pFont, pFace, &tDesc);
	xgeFontFaceFree(pFace);
	return iRet;
}

int xgeFontLoadXRF(xge_font pFont, const char* sPath)
{
	xge_resource_t tResource;
	int iRet;

	if ( (pFont == NULL) || (sPath == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = xgeResourceLoad(sPath, &tResource);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = xgeFontLoadXRFMemory(pFont, tResource.pData, tResource.iSize);
	xgeResourceFree(&tResource);
	return iRet;
}

int xgeFontLoadXRFMemory(xge_font pFont, const void* pData, int iSize)
{
	const unsigned char* pBytes;
	const xge_xrf_header_t* pHeader;
	const xge_xrf_header_v2_t* pHeaderV2;
	const xge_xrf_glyph_t* pXGlyphs;
	const xge_xrf_page_t* pXPages;
	const xge_xrf_page_t* pXPage;
	xge_glyph_cache_t* pCache;
	xge_font_instance_backend_t* pBackend;
	uint32_t i;
	int iRet;

	if ( (pFont == NULL) || (pData == NULL) || (iSize < (int)sizeof(xge_xrf_header_t)) ||
	     (pFont->iRefCount > 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pBytes = (const unsigned char*)pData;
	pHeader = (const xge_xrf_header_t*)pBytes;
	if ( (pHeader->iMagic != XGE_XRF_MAGIC) ||
	     ((pHeader->iVersion != XGE_XRF_VERSION_LEGACY) && (pHeader->iVersion != XGE_XRF_VERSION)) ||
	     (pHeader->iHeaderSize < sizeof(xge_xrf_header_t)) || (pHeader->iHeaderSize > iSize) ) {
		return XGE_ERROR_RESOURCE_FAILED;
	}
	if ( (pHeader->iVersion == XGE_XRF_VERSION) && (pHeader->iHeaderSize < sizeof(xge_xrf_header_v2_t)) ) {
		return XGE_ERROR_RESOURCE_FAILED;
	}
	if ( !__xgeRangeInsideU64(pHeader->iGlyphOffset, (uint64_t)pHeader->iGlyphCount * sizeof(xge_xrf_glyph_t), iSize) ) {
		return XGE_ERROR_RESOURCE_FAILED;
	}
	if ( !__xgeRangeInsideU64(pHeader->iPageOffset, (uint64_t)pHeader->iPageCount * sizeof(xge_xrf_page_t), iSize) ) {
		return XGE_ERROR_RESOURCE_FAILED;
	}
	if ( (pHeader->iRangeCount > 0) && !__xgeRangeInsideU64(pHeader->iRangeOffset, (uint64_t)pHeader->iRangeCount * sizeof(xge_xrf_range_t), iSize) ) {
		return XGE_ERROR_RESOURCE_FAILED;
	}
	if ( (pHeader->iKerningCount > 0) && !__xgeRangeInsideU64(pHeader->iKerningOffset, (uint64_t)pHeader->iKerningCount * sizeof(xge_xrf_kerning_t), iSize) ) {
		return XGE_ERROR_RESOURCE_FAILED;
	}
	memset(pFont, 0, sizeof(*pFont));
	pFont->iRefCount = 1;
	pFont->fAscent = pHeader->fAscent;
	pFont->fDescent = pHeader->fDescent;
	pFont->fLineGap = pHeader->fLineGap;
	pFont->fLineHeight = pHeader->fLineHeight;
	pFont->fSize = pHeader->fLineHeight;
	pFont->fScale = 1.0f;
	if ( pHeader->iVersion == XGE_XRF_VERSION ) {
		pHeaderV2 = (const xge_xrf_header_v2_t*)pHeader;
		pFont->fSize = pHeaderV2->fPixelSize;
	}
	pXPages = (const xge_xrf_page_t*)(pBytes + pHeader->iPageOffset);
	if ( pHeader->iPageCount > 0 ) {
		pFont->tAtlas.iPageWidth = pXPages[0].iWidth;
		pFont->tAtlas.iPageHeight = pXPages[0].iHeight;
		pFont->tAtlas.iFormat = XGE_PIXEL_RGBA8;
	}
	for ( i = 0; i < pHeader->iPageCount; i++ ) {
		pXPage = &pXPages[i];
		if ( (pXPage->iWidth <= 0) || (pXPage->iHeight <= 0) ||
		     !__xgeRangeInsideU64((uint64_t)pHeader->iPixelOffset + pXPage->iPixelOffset, pXPage->iPixelSize, iSize) ) {
			__xgeFontAtlasFree(pFont);
			memset(pFont, 0, sizeof(*pFont));
			return XGE_ERROR_RESOURCE_FAILED;
		}
		if ( (pXPage->iWidth != pFont->tAtlas.iPageWidth) || (pXPage->iHeight != pFont->tAtlas.iPageHeight) ) {
			__xgeFontAtlasFree(pFont);
			memset(pFont, 0, sizeof(*pFont));
			return XGE_ERROR_UNSUPPORTED;
		}
		iRet = __xgeFontAtlasPageSetPixels(pFont, (int)i, pXPage->iWidth, pXPage->iHeight, pXPage->iFormat, pBytes + pHeader->iPixelOffset + pXPage->iPixelOffset, (int)pXPage->iPixelSize);
		if ( iRet != XGE_OK ) {
			__xgeFontAtlasFree(pFont);
			memset(pFont, 0, sizeof(*pFont));
			return iRet;
		}
		((xge_glyph_atlas_page_t*)pFont->tAtlas.pPages)[i].iCursorY = pFont->tAtlas.iPageHeight;
	}
	pXGlyphs = (const xge_xrf_glyph_t*)(pBytes + pHeader->iGlyphOffset);
	for ( i = 0; i < pHeader->iGlyphCount; i++ ) {
		pXPage = (pXGlyphs[i].iPage < pHeader->iPageCount) ? &pXPages[pXGlyphs[i].iPage] : NULL;
		if ( pXGlyphs[i].iCodepoint > XGE_UNICODE_MAX || pXPage == NULL ||
		     (uint32_t)pXGlyphs[i].iX + (uint32_t)pXGlyphs[i].iWidth > (uint32_t)pXPage->iWidth ||
		     (uint32_t)pXGlyphs[i].iY + (uint32_t)pXGlyphs[i].iHeight > (uint32_t)pXPage->iHeight ) {
			__xgeFontAtlasFree(pFont);
			memset(pFont, 0, sizeof(*pFont));
			return XGE_ERROR_RESOURCE_FAILED;
		}
		pCache = (xge_glyph_cache_t*)xrtMalloc(sizeof(*pCache));
		if ( pCache == NULL ) {
			__xgeFontAtlasFree(pFont);
			memset(pFont, 0, sizeof(*pFont));
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		memset(pCache, 0, sizeof(*pCache));
		pCache->tGlyph.iCodepoint = pXGlyphs[i].iCodepoint;
		pCache->tGlyph.iGlyph = pXGlyphs[i].iGlyph;
		pCache->tGlyph.iPage = pXGlyphs[i].iPage;
		pCache->tGlyph.iX = pXGlyphs[i].iX;
		pCache->tGlyph.iY = pXGlyphs[i].iY;
		pCache->tGlyph.iWidth = pXGlyphs[i].iWidth;
		pCache->tGlyph.iHeight = pXGlyphs[i].iHeight;
		pCache->tGlyph.fOffsetX = pXGlyphs[i].fOffsetX;
		pCache->tGlyph.fOffsetY = pXGlyphs[i].fOffsetY;
		pCache->tGlyph.fAdvanceX = pXGlyphs[i].fAdvanceX;
		if ( __xgeFontCacheAddGlyph(pFont, pCache) != XGE_OK ) {
			xrtFree(pCache);
			__xgeFontAtlasFree(pFont);
			memset(pFont, 0, sizeof(*pFont));
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		if ( __xgeFontCacheMapCodepoint(pFont, pXGlyphs[i].iCodepoint, pCache) != XGE_OK ) {
			__xgeFontAtlasFree(pFont);
			memset(pFont, 0, sizeof(*pFont));
			return XGE_ERROR_OUT_OF_MEMORY;
		}
	}
	pBackend = (xge_font_instance_backend_t*)xrtCalloc(1, sizeof(*pBackend));
	if ( pBackend == NULL ) {
		__xgeFontAtlasFree(pFont);
		memset(pFont, 0, sizeof(*pFont));
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pBackend->iWeight = XGE_FONT_WEIGHT_NORMAL;
	pBackend->iStretch = XGE_FONT_STRETCH_NORMAL;
	if ( pHeader->iVersion == XGE_XRF_VERSION ) {
		pHeaderV2 = (const xge_xrf_header_v2_t*)pHeader;
		pBackend->iSourceHash = pHeaderV2->iSourceHash;
		pBackend->iFaceIndex = pHeaderV2->iFaceIndex;
		pBackend->iWeight = pHeaderV2->iWeight;
		pBackend->iStretch = pHeaderV2->iStretch;
		pBackend->iSlant = pHeaderV2->iSlant;
		pBackend->fUnderlinePosition = pHeaderV2->fUnderlinePosition;
		pBackend->fUnderlineThickness = pHeaderV2->fUnderlineThickness;
		pBackend->fStrikePosition = pHeaderV2->fStrikePosition;
		pBackend->fStrikeThickness = pHeaderV2->fStrikeThickness;
	}
	pFont->pBackend = pBackend;
	g_xge.iFontCount++;
	return XGE_OK;
}

int xgeFontBuildXRFMemory(xge_font pFont, uint32_t iFirstCodepoint, uint32_t iCount, void** ppData, int* pSize)
{
	xge_xrf_header_t* pHeader;
	xge_xrf_header_v2_t* pHeaderV2;
	xge_font_instance_backend_t* pInstanceBackend;
	xge_xrf_range_t* pRange;
	xge_xrf_glyph_t* pGlyphs;
	xge_xrf_page_t* pPagesOut;
	xge_glyph_atlas_page_t* pPages;
	xge_glyph_cache_t* pCache;
	xge_glyph_t tGlyph;
	xge_glyph_metrics_t tMetrics;
	unsigned char* pBytes;
	unsigned char* pDstPixels;
	uint32_t i;
	uint32_t iGlyphCount;
	uint32_t iPageCount;
	uint32_t iPixelBytes;
	uint32_t iGlyphOffset;
	uint32_t iPageOffset;
	uint32_t iRangeOffset;
	uint32_t iPixelOffset;
	uint32_t iTotalSize;
	uint64_t iLayoutSize;

	if ( (pFont == NULL) || (ppData == NULL) || (pSize == NULL) || (iCount == 0) ||
	     (iFirstCodepoint > XGE_UNICODE_MAX) || ((iCount - 1u) > (XGE_UNICODE_MAX - iFirstCodepoint)) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	for ( i = 0; i < iCount; i++ ) {
		if ( (xgeFontGlyphGet(pFont, iFirstCodepoint + i, &tMetrics) == XGE_OK) &&
		     ((tMetrics.iGlyph != 0) || ((iFirstCodepoint + i) == 0)) ) {
			(void)xgeFontGlyphAtlasGet(pFont, iFirstCodepoint + i, &tGlyph);
		}
	}
	iGlyphCount = 0;
	for ( i = 0; i < iCount; i++ ) {
		pCache = __xgeFontGlyphFind(pFont, iFirstCodepoint + i);
		if ( (pCache != NULL) && (pCache->tGlyph.iPage >= 0) ) {
			iGlyphCount++;
		}
	}
	iPageCount = (uint32_t)pFont->tAtlas.iPageCount;
	if ( (iGlyphCount == 0) || (iPageCount == 0) || (pFont->tAtlas.pPages == NULL) ) {
		return XGE_ERROR_RESOURCE_FAILED;
	}
	if ( (iPageCount > UINT16_MAX) || (pFont->tAtlas.iPageWidth > UINT16_MAX) ||
	     (pFont->tAtlas.iPageHeight > UINT16_MAX) ) return XGE_ERROR_OUT_OF_MEMORY;
	iLayoutSize = (uint64_t)pFont->tAtlas.iPageWidth * (uint64_t)pFont->tAtlas.iPageHeight * 4u;
	if ( (iLayoutSize == 0) || (iLayoutSize > UINT32_MAX) ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iPixelBytes = (uint32_t)iLayoutSize;
	iLayoutSize = sizeof(xge_xrf_header_v2_t) + sizeof(xge_xrf_range_t);
	iGlyphOffset = (uint32_t)iLayoutSize;
	iLayoutSize += (uint64_t)iGlyphCount * sizeof(xge_xrf_glyph_t);
	if ( iLayoutSize > INT32_MAX ) return XGE_ERROR_OUT_OF_MEMORY;
	iPageOffset = (uint32_t)iLayoutSize;
	iLayoutSize += (uint64_t)iPageCount * sizeof(xge_xrf_page_t);
	if ( iLayoutSize > INT32_MAX ) return XGE_ERROR_OUT_OF_MEMORY;
	iPixelOffset = (uint32_t)iLayoutSize;
	iLayoutSize += (uint64_t)iPageCount * iPixelBytes;
	if ( iLayoutSize > INT32_MAX ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	iRangeOffset = (uint32_t)sizeof(xge_xrf_header_v2_t);
	iTotalSize = (uint32_t)iLayoutSize;
	pBytes = (unsigned char*)xrtMalloc(iTotalSize);
	if ( pBytes == NULL ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	memset(pBytes, 0, iTotalSize);
	pHeaderV2 = (xge_xrf_header_v2_t*)pBytes;
	pHeader = &pHeaderV2->tBase;
	pRange = (xge_xrf_range_t*)(pBytes + iRangeOffset);
	pGlyphs = (xge_xrf_glyph_t*)(pBytes + iGlyphOffset);
	pPagesOut = (xge_xrf_page_t*)(pBytes + iPageOffset);
	pHeader->iMagic = XGE_XRF_MAGIC;
	pHeader->iVersion = XGE_XRF_VERSION;
	pHeader->iHeaderSize = (uint16_t)sizeof(xge_xrf_header_v2_t);
	pHeader->iFlags = XGE_XRF_FLAG_SOURCE_INFO;
	pHeader->iGlyphCount = iGlyphCount;
	pHeader->iPageCount = iPageCount;
	pHeader->iRangeCount = 1;
	pHeader->fAscent = pFont->fAscent;
	pHeader->fDescent = pFont->fDescent;
	pHeader->fLineGap = pFont->fLineGap;
	pHeader->fLineHeight = pFont->fLineHeight;
	pHeader->iGlyphOffset = iGlyphOffset;
	pHeader->iPageOffset = iPageOffset;
	pHeader->iRangeOffset = iRangeOffset;
	pHeader->iPixelOffset = iPixelOffset;
	pInstanceBackend = (xge_font_instance_backend_t*)pFont->pBackend;
	pHeaderV2->iSourceHash = (pInstanceBackend != NULL) ? pInstanceBackend->iSourceHash : 0;
	pHeaderV2->fPixelSize = pFont->fSize;
	pHeaderV2->iFaceIndex = (pInstanceBackend != NULL) ? pInstanceBackend->iFaceIndex : 0;
	pHeaderV2->iWeight = (pInstanceBackend != NULL) ? pInstanceBackend->iWeight : XGE_FONT_WEIGHT_NORMAL;
	pHeaderV2->iStretch = (pInstanceBackend != NULL) ? pInstanceBackend->iStretch : XGE_FONT_STRETCH_NORMAL;
	pHeaderV2->iSlant = (pInstanceBackend != NULL) ? pInstanceBackend->iSlant : XGE_FONT_SLANT_NORMAL;
	pHeaderV2->fUnderlinePosition = (pInstanceBackend != NULL) ? pInstanceBackend->fUnderlinePosition : 0.0f;
	pHeaderV2->fUnderlineThickness = (pInstanceBackend != NULL) ? pInstanceBackend->fUnderlineThickness : 0.0f;
	pHeaderV2->fStrikePosition = (pInstanceBackend != NULL) ? pInstanceBackend->fStrikePosition : 0.0f;
	pHeaderV2->fStrikeThickness = (pInstanceBackend != NULL) ? pInstanceBackend->fStrikeThickness : 0.0f;
	pRange->iFirstCodepoint = iFirstCodepoint;
	pRange->iCount = iCount;
	iGlyphCount = 0;
	for ( i = 0; i < iCount; i++ ) {
		pCache = __xgeFontGlyphFind(pFont, iFirstCodepoint + i);
		if ( (pCache != NULL) && (pCache->tGlyph.iPage >= 0) ) {
			if ( (pCache->tGlyph.iGlyph < 0) || (pCache->tGlyph.iGlyph > UINT16_MAX) ||
			     (pCache->tGlyph.iPage > UINT16_MAX) || (pCache->tGlyph.iX < 0) ||
			     (pCache->tGlyph.iY < 0) || (pCache->tGlyph.iWidth < 0) ||
			     (pCache->tGlyph.iHeight < 0) ) {
				xrtFree(pBytes);
				return XGE_ERROR_OUT_OF_MEMORY;
			}
			pGlyphs[iGlyphCount].iCodepoint = iFirstCodepoint + i;
			pGlyphs[iGlyphCount].iGlyph = (uint16_t)pCache->tGlyph.iGlyph;
			pGlyphs[iGlyphCount].iPage = (uint16_t)pCache->tGlyph.iPage;
			pGlyphs[iGlyphCount].iX = (uint16_t)pCache->tGlyph.iX;
			pGlyphs[iGlyphCount].iY = (uint16_t)pCache->tGlyph.iY;
			pGlyphs[iGlyphCount].iWidth = (uint16_t)pCache->tGlyph.iWidth;
			pGlyphs[iGlyphCount].iHeight = (uint16_t)pCache->tGlyph.iHeight;
			pGlyphs[iGlyphCount].fOffsetX = pCache->tGlyph.fOffsetX;
			pGlyphs[iGlyphCount].fOffsetY = pCache->tGlyph.fOffsetY;
			pGlyphs[iGlyphCount].fAdvanceX = pCache->tGlyph.fAdvanceX;
			iGlyphCount++;
		}
	}
	pPages = (xge_glyph_atlas_page_t*)pFont->tAtlas.pPages;
	for ( i = 0; i < iPageCount; i++ ) {
		pPagesOut[i].iWidth = (uint16_t)pFont->tAtlas.iPageWidth;
		pPagesOut[i].iHeight = (uint16_t)pFont->tAtlas.iPageHeight;
		pPagesOut[i].iFormat = XGE_XRF_PAGE_RGBA8;
		pPagesOut[i].iPixelOffset = i * iPixelBytes;
		pPagesOut[i].iPixelSize = iPixelBytes;
		pDstPixels = pBytes + iPixelOffset + pPagesOut[i].iPixelOffset;
		if ( pPages[i].pPixels != NULL ) {
			memcpy(pDstPixels, pPages[i].pPixels, iPixelBytes);
		}
	}
	*ppData = pBytes;
	*pSize = (int)iTotalSize;
	return XGE_OK;
}

int xgeFontSaveXRF(xge_font pFont, const char* sPath, uint32_t iFirstCodepoint, uint32_t iCount)
{
	FILE* pFile;
	char* sFullPath;
	void* pData;
	int iSize;
	int iRet;

	if ( (pFont == NULL) || (sPath == NULL) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pData = NULL;
	iSize = 0;
	iRet = xgeFontBuildXRFMemory(pFont, iFirstCodepoint, iCount, &pData, &iSize);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	sFullPath = __xgePathResolve(sPath);
	if ( sFullPath == NULL ) {
		xrtFree(pData);
		return XGE_ERROR_RESOURCE_FAILED;
	}
	pFile = fopen(sFullPath, "wb");
	xrtFree(sFullPath);
	if ( pFile == NULL ) {
		xrtFree(pData);
		return XGE_ERROR_RESOURCE_FAILED;
	}
	if ( fwrite(pData, 1, (size_t)iSize, pFile) != (size_t)iSize ) {
		fclose(pFile);
		xrtFree(pData);
		return XGE_ERROR_RESOURCE_FAILED;
	}
	fclose(pFile);
	xrtFree(pData);
	return XGE_OK;
}

int xgeFontLoadCached(xge_font pFont, const char* sTTFPath, const char* sXRFPath, float fSize, uint32_t iFirstCodepoint, uint32_t iCount)
{
	xge_font_t tDynamicFont;
	xge_font_instance_backend_t* pCachedBackend;
	xge_font_face pDynamicFace;
	int bCompatible;
	int iRet;

	if ( (pFont == NULL) || (sTTFPath == NULL) || (sXRFPath == NULL) || (pFont->iRefCount > 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = xgeFontLoadXRF(pFont, sXRFPath);
	if ( iRet == XGE_OK ) {
		memset(&tDynamicFont, 0, sizeof(tDynamicFont));
		iRet = xgeFontLoad(&tDynamicFont, sTTFPath, fSize);
		if ( iRet != XGE_OK ) {
			return XGE_OK;
		}
		pCachedBackend = (xge_font_instance_backend_t*)pFont->pBackend;
		pDynamicFace = __xgeFontFace(&tDynamicFont);
		bCompatible = (fabsf(pFont->fLineHeight - tDynamicFont.fLineHeight) <= 0.01f);
		if ( (pCachedBackend != NULL) && (pCachedBackend->iSourceHash != 0) && (pDynamicFace != NULL) ) {
			bCompatible = bCompatible && (pCachedBackend->iSourceHash == pDynamicFace->iSourceHash) && (pCachedBackend->iFaceIndex == pDynamicFace->iFaceIndex);
		}
		if ( bCompatible && (pCachedBackend != NULL) && (pDynamicFace != NULL) ) {
			xgeFontFaceAddRef(pDynamicFace);
			pCachedBackend->pFace = pDynamicFace;
			pFont->pData = pDynamicFace->pData;
			pFont->iDataSize = pDynamicFace->iDataSize;
			pFont->fScale = tDynamicFont.fScale;
			xgeFontFree(&tDynamicFont);
			return XGE_OK;
		}
		xgeFontFree(&tDynamicFont);
		xgeFontFree(pFont);
	}
	iRet = xgeFontLoad(pFont, sTTFPath, fSize);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	(void)xgeFontSaveXRF(pFont, sXRFPath, iFirstCodepoint, iCount);
	return XGE_OK;
}

int xgeFontAddRef(xge_font pFont)
{
	if ( pFont == NULL ) {
		return 0;
	}
	if ( pFont->iRefCount < INT32_MAX ) {
		pFont->iRefCount++;
	}
	return pFont->iRefCount;
}

void xgeFontFree(xge_font pFont)
{
	xge_font_instance_backend_t* pBackend;

	if ( pFont == NULL ) {
		return;
	}
	if ( pFont->iRefCount > 1 ) {
		pFont->iRefCount--;
		return;
	}
	if ( pFont->iRefCount > 0 && g_xge.iFontCount > 0 ) {
		g_xge.iFontCount--;
	}
	if ( pFont->pFallback != NULL && pFont->pFallback != pFont ) {
		xgeFontFree(pFont->pFallback);
		pFont->pFallback = NULL;
	}
	pBackend = (xge_font_instance_backend_t*)pFont->pBackend;
	if ( pBackend != NULL ) {
		xgeFontFaceFree(pBackend->pFace);
		xrtFree(pBackend);
	}
	__xgeFontAtlasFree(pFont);
	memset(pFont, 0, sizeof(*pFont));
}

void xgeFontSetFallback(xge_font pFont, xge_font pFallback)
{
	xge_font pScan;
	int iDepth;

	if ( (pFont == NULL) || (pFallback == pFont) || (pFont->pFallback == pFallback) ) return;
	if ( pFallback != NULL && pFallback->iRefCount <= 0 ) return;
	pScan = pFallback;
	for ( iDepth = 0; pScan != NULL && iDepth < 64; iDepth++ ) {
		if ( pScan == pFont ) return;
		pScan = pScan->pFallback;
	}
	if ( pScan != NULL ) return;
	if ( pFallback != NULL ) xgeFontAddRef(pFallback);
	if ( pFont->pFallback != NULL ) xgeFontFree(pFont->pFallback);
	pFont->pFallback = pFallback;
}

int xgeFontFallbackSet(const char* sPath, float fSize)
{
	xge_resource_t tResource;
	int iRet;

	if ( (sPath == NULL) || (fSize <= 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	iRet = xgeResourceLoad(sPath, &tResource);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = xgeFontFallbackSetMemory(tResource.pData, tResource.iSize, fSize);
	xgeResourceFree(&tResource);
	return iRet;
}

int xgeFontFallbackSetMemory(const void* pData, int iSize, float fSize)
{
	int iRet;

	if ( (pData == NULL) || (iSize <= 0) || (fSize <= 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	xgeFontFallbackClear();
	iRet = xgeFontLoadMemory(&g_xge.tFallbackFont, pData, iSize, fSize);
	if ( iRet != XGE_OK ) {
		memset(&g_xge.tFallbackFont, 0, sizeof(g_xge.tFallbackFont));
		return iRet;
	}
	g_xge.tFallbackFont.iFlags |= XGE_FONT_FALLBACK;
	return XGE_OK;
}

int xgeFontFallbackGetEx(xge_font pFont, float fSize, uint32_t iFlags)
{
	xge_font_face pFace;
	xge_font_instance_desc_t tDesc;
	int iRet;

	if ( (pFont == NULL) || (fSize <= 0.0f) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	if ( g_xge.tFallbackFont.iRefCount <= 0 ) {
		return XGE_ERROR_RESOURCE_FAILED;
	}
	pFace = __xgeFontFace(&g_xge.tFallbackFont);
	if ( pFace == NULL ) return XGE_ERROR_RESOURCE_FAILED;
	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.iSize = sizeof(tDesc);
	tDesc.fPixelSize = fSize;
	tDesc.iFlags = iFlags;
	iRet = xgeFontCreate(pFont, pFace, &tDesc);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pFont->iFlags |= XGE_FONT_FALLBACK;
	return XGE_OK;
}

int xgeFontFallbackGet(xge_font pFont, float fSize)
{
	return xgeFontFallbackGetEx(pFont, fSize, 0u);
}

void xgeFontFallbackClear(void)
{
	if ( g_xge.tFallbackFont.iRefCount > 0 ) {
		xgeFontFree(&g_xge.tFallbackFont);
	}
	memset(&g_xge.tFallbackFont, 0, sizeof(g_xge.tFallbackFont));
}

int xgeFontGetMetrics(xge_font pFont, xge_font_metrics_t* pMetrics)
{
	xge_font_instance_backend_t* pBackend;
	float fDefaultThickness;

	if ( (pFont == NULL) || (pMetrics == NULL) || (pFont->iRefCount <= 0) ) return XGE_ERROR_INVALID_ARGUMENT;
	pBackend = (xge_font_instance_backend_t*)pFont->pBackend;
	fDefaultThickness = (pFont->fSize >= 18.0f) ? 2.0f : 1.0f;
	memset(pMetrics, 0, sizeof(*pMetrics));
	pMetrics->iSize = sizeof(*pMetrics);
	pMetrics->fPixelSize = pFont->fSize;
	pMetrics->fAscent = pFont->fAscent;
	pMetrics->fDescent = pFont->fDescent;
	pMetrics->fLineGap = pFont->fLineGap;
	pMetrics->fLineHeight = pFont->fLineHeight;
	pMetrics->fUnderlinePosition = (pBackend != NULL && pBackend->fUnderlinePosition != 0.0f) ? pBackend->fUnderlinePosition : 1.0f;
	pMetrics->fUnderlineThickness = (pBackend != NULL && pBackend->fUnderlineThickness > 0.0f) ? pBackend->fUnderlineThickness : fDefaultThickness;
	pMetrics->fStrikePosition = (pBackend != NULL && pBackend->fStrikePosition != 0.0f) ? pBackend->fStrikePosition : (-pFont->fAscent * 0.35f);
	pMetrics->fStrikeThickness = (pBackend != NULL && pBackend->fStrikeThickness > 0.0f) ? pBackend->fStrikeThickness : fDefaultThickness;
	return XGE_OK;
}

int xgeFontCacheGetStats(xge_font pFont, xge_font_cache_stats_t* pStats)
{
	xge_font_cache_t* pCache;
	xge_glyph_atlas_page_t* pPages;
	uint64_t iPageBytes;
	int i;

	if ( (pFont == NULL) || (pStats == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	pCache = __xgeFontCacheGet(pFont, 0);
	memset(pStats, 0, sizeof(*pStats));
	pStats->iSize = sizeof(*pStats);
	if ( pCache != NULL ) {
		pStats->iGlyphCount = pCache->iGlyphCount;
		pStats->iCodepointCount = pCache->iCodepointCount;
		pStats->iBucketCount = pCache->iBucketCount;
	}
	pStats->iAtlasPageCount = (uint32_t)pFont->tAtlas.iPageCount;
	iPageBytes = (uint64_t)pFont->tAtlas.iPageWidth * (uint64_t)pFont->tAtlas.iPageHeight * 4u;
	pStats->iAtlasCpuBytes = iPageBytes * (uint64_t)pFont->tAtlas.iPageCount;
	pPages = (xge_glyph_atlas_page_t*)pFont->tAtlas.pPages;
	for ( i = 0; (pPages != NULL) && (i < pFont->tAtlas.iPageCount); i++ ) {
		if ( pPages[i].tTexture.iBackendId != 0 ) pStats->iAtlasGpuBytes += iPageBytes;
	}
	return XGE_OK;
}

void xgeFontCacheClear(xge_font pFont)
{
	if ( (pFont != NULL) && (__xgeFontInfo(pFont) != NULL) ) __xgeFontAtlasFree(pFont);
}

int xgeFontGlyphGet(xge_font pFont, uint32_t iCodepoint, xge_glyph_metrics_t* pMetrics)
{
	stbtt_fontinfo* pInfo;
	xge_glyph_cache_t* pCache;
	int iGlyph;

	if ( (pFont == NULL) || (pMetrics == NULL) || (iCodepoint > XGE_UNICODE_MAX) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pCache = __xgeFontGlyphFind(pFont, iCodepoint);
	if ( pCache != NULL ) {
		memset(pMetrics, 0, sizeof(*pMetrics));
		pMetrics->iCodepoint = iCodepoint;
		pMetrics->iGlyph = pCache->tGlyph.iGlyph;
		pMetrics->fAdvanceX = pCache->tGlyph.fAdvanceX;
		pMetrics->fX0 = pCache->tGlyph.fOffsetX;
		pMetrics->fY0 = pCache->tGlyph.fOffsetY;
		pMetrics->fX1 = pCache->tGlyph.fOffsetX + (float)pCache->tGlyph.iWidth;
		pMetrics->fY1 = pCache->tGlyph.fOffsetY + (float)pCache->tGlyph.iHeight;
		return XGE_OK;
	}
	pInfo = __xgeFontInfo(pFont);
	if ( pInfo == NULL ) {
		if ( pFont->pFallback != NULL ) {
			return xgeFontGlyphGet(pFont->pFallback, iCodepoint, pMetrics);
		}
		return XGE_ERROR_NOT_INITIALIZED;
	}
	iGlyph = stbtt_FindGlyphIndex(pInfo, (int)iCodepoint);
	if ( (iGlyph == 0) && (pFont->pFallback != NULL) ) {
		return xgeFontGlyphGet(pFont->pFallback, iCodepoint, pMetrics);
	}
	if ( xgeFontGlyphGetByIndex(pFont, iGlyph, pMetrics) != XGE_OK ) return XGE_ERROR_RESOURCE_FAILED;
	pMetrics->iCodepoint = iCodepoint;
	return XGE_OK;
}

int xgeFontGlyphGetByIndex(xge_font pFont, int iGlyph, xge_glyph_metrics_t* pMetrics)
{
	stbtt_fontinfo* pInfo;
	xge_glyph_cache_t* pCache;
	int iAdvance;
	int iBearing;
	int iX0;
	int iY0;
	int iX1;
	int iY1;

	if ( (pFont == NULL) || (pMetrics == NULL) || (iGlyph < 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pCache = __xgeFontGlyphFindByIndex(pFont, iGlyph);
	if ( pCache != NULL ) {
		memset(pMetrics, 0, sizeof(*pMetrics));
		pMetrics->iCodepoint = pCache->tGlyph.iCodepoint;
		pMetrics->iGlyph = pCache->tGlyph.iGlyph;
		pMetrics->fAdvanceX = pCache->tGlyph.fAdvanceX;
		pMetrics->fX0 = pCache->tGlyph.fOffsetX;
		pMetrics->fY0 = pCache->tGlyph.fOffsetY;
		pMetrics->fX1 = pCache->tGlyph.fOffsetX + (float)pCache->tGlyph.iWidth;
		pMetrics->fY1 = pCache->tGlyph.fOffsetY + (float)pCache->tGlyph.iHeight;
		return XGE_OK;
	}
	pInfo = __xgeFontInfo(pFont);
	if ( pInfo == NULL ) return XGE_ERROR_NOT_INITIALIZED;
	memset(pMetrics, 0, sizeof(*pMetrics));
	pMetrics->iGlyph = iGlyph;
	stbtt_GetGlyphHMetrics(pInfo, iGlyph, &iAdvance, &iBearing);
	stbtt_GetGlyphBitmapBox(pInfo, iGlyph, pFont->fScale, pFont->fScale, &iX0, &iY0, &iX1, &iY1);
	pMetrics->fAdvanceX = (float)iAdvance * pFont->fScale;
	pMetrics->fLeftSideBearing = (float)iBearing * pFont->fScale;
	pMetrics->fX0 = (float)iX0;
	pMetrics->fY0 = (float)iY0;
	pMetrics->fX1 = (float)iX1;
	pMetrics->fY1 = (float)iY1;
	return XGE_OK;
}

int xgeFontGlyphRasterizeByIndex(xge_font pFont, int iGlyph, xge_glyph_bitmap_t* pBitmap)
{
	stbtt_fontinfo* pInfo;
	unsigned char* pPixels;
	int iWidth;
	int iHeight;
	int iOffsetX;
	int iOffsetY;

	if ( (pFont == NULL) || (pBitmap == NULL) || (iGlyph < 0) ) return XGE_ERROR_INVALID_ARGUMENT;
	pInfo = __xgeFontInfo(pFont);
	if ( pInfo == NULL ) return XGE_ERROR_NOT_INITIALIZED;
	memset(pBitmap, 0, sizeof(*pBitmap));
	pPixels = stbtt_GetGlyphBitmap(pInfo, pFont->fScale, pFont->fScale, iGlyph, &iWidth, &iHeight, &iOffsetX, &iOffsetY);
	if ( (pPixels == NULL) && (iWidth > 0) && (iHeight > 0) ) {
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pBitmap->iWidth = iWidth;
	pBitmap->iHeight = iHeight;
	pBitmap->iStride = iWidth;
	pBitmap->iOffsetX = iOffsetX;
	pBitmap->iOffsetY = iOffsetY;
	pBitmap->iFormat = XGE_PIXEL_A8;
	pBitmap->pPixels = pPixels;
	return XGE_OK;
}

int xgeFontGlyphRasterize(xge_font pFont, uint32_t iCodepoint, xge_glyph_bitmap_t* pBitmap)
{
	stbtt_fontinfo* pInfo;
	int iGlyph;
	int iRet;

	if ( (pFont == NULL) || (pBitmap == NULL) || (iCodepoint > XGE_UNICODE_MAX) ) return XGE_ERROR_INVALID_ARGUMENT;
	pInfo = __xgeFontInfo(pFont);
	if ( pInfo == NULL ) {
		if ( pFont->pFallback != NULL ) return xgeFontGlyphRasterize(pFont->pFallback, iCodepoint, pBitmap);
		return XGE_ERROR_NOT_INITIALIZED;
	}
	iGlyph = stbtt_FindGlyphIndex(pInfo, (int)iCodepoint);
	if ( (iGlyph == 0) && (pFont->pFallback != NULL) ) return xgeFontGlyphRasterize(pFont->pFallback, iCodepoint, pBitmap);
	iRet = xgeFontGlyphRasterizeByIndex(pFont, iGlyph, pBitmap);
	if ( iRet == XGE_OK ) pBitmap->iCodepoint = iCodepoint;
	return iRet;
}

int xgeFontGlyphAtlasGetByIndex(xge_font pFont, int iGlyph, xge_glyph_t* pGlyph)
{
	xge_glyph_cache_t* pCache;
	xge_glyph_bitmap_t tBitmap;
	xge_glyph_metrics_t tMetrics;
	xge_glyph_atlas_page_t* pPage;
	int iPage;
	int iX;
	int iY;
	int iRet;

	if ( (pFont == NULL) || (pGlyph == NULL) || (iGlyph < 0) ) {
		return XGE_ERROR_INVALID_ARGUMENT;
	}
	pCache = __xgeFontGlyphFindByIndex(pFont, iGlyph);
	if ( pCache != NULL ) {
		*pGlyph = pCache->tGlyph;
		return XGE_OK;
	}
	iRet = xgeFontGlyphGetByIndex(pFont, iGlyph, &tMetrics);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	iRet = xgeFontGlyphRasterizeByIndex(pFont, iGlyph, &tBitmap);
	if ( iRet != XGE_OK ) {
		return iRet;
	}
	pCache = (xge_glyph_cache_t*)xrtMalloc(sizeof(*pCache));
	if ( pCache == NULL ) {
		xgeGlyphBitmapFree(&tBitmap);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	memset(pCache, 0, sizeof(*pCache));
	pCache->tGlyph.iCodepoint = 0;
	pCache->tGlyph.iGlyph = tMetrics.iGlyph;
	pCache->tGlyph.fAdvanceX = tMetrics.fAdvanceX;
	pCache->tGlyph.fOffsetX = (float)tBitmap.iOffsetX;
	pCache->tGlyph.fOffsetY = (float)tBitmap.iOffsetY;
	pCache->tGlyph.iWidth = tBitmap.iWidth;
	pCache->tGlyph.iHeight = tBitmap.iHeight;
	pCache->tGlyph.iPage = -1;
	if ( (tBitmap.iWidth > 0) && (tBitmap.iHeight > 0) ) {
		pPage = __xgeFontAtlasPlace(pFont, tBitmap.iWidth, tBitmap.iHeight, &iPage, &iX, &iY);
		if ( pPage == NULL ) {
			xrtFree(pCache);
			xgeGlyphBitmapFree(&tBitmap);
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		__xgeFontAtlasBlit(pFont, pPage, iX, iY, &tBitmap);
		pCache->tGlyph.iPage = iPage;
		pCache->tGlyph.iX = iX;
		pCache->tGlyph.iY = iY;
	}
	if ( __xgeFontCacheAddGlyph(pFont, pCache) != XGE_OK ) {
		xrtFree(pCache);
		xgeGlyphBitmapFree(&tBitmap);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	*pGlyph = pCache->tGlyph;
	xgeGlyphBitmapFree(&tBitmap);
	return XGE_OK;
}

int xgeFontGlyphAtlasGet(xge_font pFont, uint32_t iCodepoint, xge_glyph_t* pGlyph)
{
	xge_glyph_cache_t* pCache;
	xge_glyph_metrics_t tMetrics;
	xge_glyph_bitmap_t tBitmap;
	xge_glyph_atlas_page_t* pPage;
	int iPage;
	int iX;
	int iY;
	int iRet;
	int iGlyph;
	xge_font pGlyphFont;

	if ( (pFont == NULL) || (pGlyph == NULL) || (iCodepoint > XGE_UNICODE_MAX) ) return XGE_ERROR_INVALID_ARGUMENT;
	pGlyphFont = __xgeFontResolveCodepoint(pFont, iCodepoint, &iGlyph);
	if ( pGlyphFont == NULL ) return XGE_ERROR_RESOURCE_FAILED;
	if ( pGlyphFont != pFont ) {
		iRet = xgeFontGlyphAtlasGetByIndex(pGlyphFont, iGlyph, pGlyph);
		if ( iRet == XGE_OK ) pGlyph->iCodepoint = iCodepoint;
		return iRet;
	}
	pCache = __xgeFontGlyphFind(pFont, iCodepoint);
	if ( pCache != NULL ) {
		*pGlyph = pCache->tGlyph;
		pGlyph->iCodepoint = iCodepoint;
		return XGE_OK;
	}
	iRet = xgeFontGlyphGet(pFont, iCodepoint, &tMetrics);
	if ( iRet != XGE_OK ) return iRet;
	pCache = __xgeFontGlyphFindByIndex(pFont, tMetrics.iGlyph);
	if ( (pCache != NULL) && (__xgeFontInfo(pFont) != NULL) ) {
		iRet = __xgeFontCacheMapCodepoint(pFont, iCodepoint, pCache);
		if ( iRet != XGE_OK ) return iRet;
		*pGlyph = pCache->tGlyph;
		pGlyph->iCodepoint = iCodepoint;
		return XGE_OK;
	}
	iRet = xgeFontGlyphRasterize(pFont, iCodepoint, &tBitmap);
	if ( iRet != XGE_OK ) return iRet;
	pCache = (xge_glyph_cache_t*)xrtCalloc(1, sizeof(*pCache));
	if ( pCache == NULL ) {
		xgeGlyphBitmapFree(&tBitmap);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	pCache->tGlyph.iCodepoint = iCodepoint;
	pCache->tGlyph.iGlyph = tMetrics.iGlyph;
	pCache->tGlyph.fAdvanceX = tMetrics.fAdvanceX;
	pCache->tGlyph.fOffsetX = (float)tBitmap.iOffsetX;
	pCache->tGlyph.fOffsetY = (float)tBitmap.iOffsetY;
	pCache->tGlyph.iWidth = tBitmap.iWidth;
	pCache->tGlyph.iHeight = tBitmap.iHeight;
	pCache->tGlyph.iPage = -1;
	if ( (tBitmap.iWidth > 0) && (tBitmap.iHeight > 0) ) {
		pPage = __xgeFontAtlasPlace(pFont, tBitmap.iWidth, tBitmap.iHeight, &iPage, &iX, &iY);
		if ( pPage == NULL ) {
			xrtFree(pCache);
			xgeGlyphBitmapFree(&tBitmap);
			return XGE_ERROR_OUT_OF_MEMORY;
		}
		__xgeFontAtlasBlit(pFont, pPage, iX, iY, &tBitmap);
		pCache->tGlyph.iPage = iPage;
		pCache->tGlyph.iX = iX;
		pCache->tGlyph.iY = iY;
	}
	if ( (__xgeFontCacheAddGlyph(pFont, pCache) != XGE_OK) || (__xgeFontCacheMapCodepoint(pFont, iCodepoint, pCache) != XGE_OK) ) {
		xgeGlyphBitmapFree(&tBitmap);
		return XGE_ERROR_OUT_OF_MEMORY;
	}
	*pGlyph = pCache->tGlyph;
	xgeGlyphBitmapFree(&tBitmap);
	return XGE_OK;
}

void xgeGlyphBitmapFree(xge_glyph_bitmap_t* pBitmap)
{
	if ( pBitmap == NULL ) {
		return;
	}
	if ( pBitmap->pPixels != NULL ) {
		xrtFree(pBitmap->pPixels);
	}
	memset(pBitmap, 0, sizeof(*pBitmap));
}

static int __xgeTextUTF8DecodeBounded(const char** psText, const char* sEnd, uint32_t* pCodepoint)
{
	const unsigned char* pText;
	uint32_t iCodepoint;
	int iCount;

	if ( (psText == NULL) || (*psText == NULL) || (sEnd == NULL) || (pCodepoint == NULL) || (*psText >= sEnd) ) return XGE_ERROR_INVALID_ARGUMENT;
	pText = (const unsigned char*)*psText;
	if ( pText[0] < 0x80 ) {
		iCodepoint = pText[0];
		iCount = 1;
	} else if ( (pText[0] & 0xE0) == 0xC0 ) {
		if ( ((sEnd - *psText) < 2) || ((pText[1] & 0xC0) != 0x80) ) return XGE_ERROR_UNSUPPORTED;
		iCodepoint = ((uint32_t)(pText[0] & 0x1F) << 6) | (uint32_t)(pText[1] & 0x3F);
		iCount = 2;
		if ( iCodepoint < 0x80 ) return XGE_ERROR_UNSUPPORTED;
	} else if ( (pText[0] & 0xF0) == 0xE0 ) {
		if ( ((sEnd - *psText) < 3) || ((pText[1] & 0xC0) != 0x80) || ((pText[2] & 0xC0) != 0x80) ) return XGE_ERROR_UNSUPPORTED;
		iCodepoint = ((uint32_t)(pText[0] & 0x0F) << 12) | ((uint32_t)(pText[1] & 0x3F) << 6) | (uint32_t)(pText[2] & 0x3F);
		iCount = 3;
		if ( iCodepoint < 0x800 ) return XGE_ERROR_UNSUPPORTED;
	} else if ( (pText[0] & 0xF8) == 0xF0 ) {
		if ( ((sEnd - *psText) < 4) || ((pText[1] & 0xC0) != 0x80) || ((pText[2] & 0xC0) != 0x80) || ((pText[3] & 0xC0) != 0x80) ) return XGE_ERROR_UNSUPPORTED;
		iCodepoint = ((uint32_t)(pText[0] & 0x07) << 18) | ((uint32_t)(pText[1] & 0x3F) << 12) |
		             ((uint32_t)(pText[2] & 0x3F) << 6) | (uint32_t)(pText[3] & 0x3F);
		iCount = 4;
		if ( iCodepoint < 0x10000 ) return XGE_ERROR_UNSUPPORTED;
	} else {
		return XGE_ERROR_UNSUPPORTED;
	}
	if ( (iCodepoint >= 0xD800u && iCodepoint <= 0xDFFFu) || (iCodepoint > XGE_UNICODE_MAX) ) return XGE_ERROR_UNSUPPORTED;
	*pCodepoint = iCodepoint;
	*psText += iCount;
	return XGE_OK;
}

static xge_font __xgeFontResolveCodepoint(xge_font pFont, uint32_t iCodepoint, int* pGlyph)
{
	xge_font pScan;
	xge_glyph_cache_t* pCache;
	stbtt_fontinfo* pInfo;
	int iGlyph;
	int iDepth;

	if ( (pFont == NULL) || (pGlyph == NULL) ) return NULL;
	pScan = pFont;
	for ( iDepth = 0; (pScan != NULL) && (iDepth < 32); iDepth++ ) {
		pCache = __xgeFontGlyphFind(pScan, iCodepoint);
		if ( pCache != NULL ) {
			*pGlyph = pCache->tGlyph.iGlyph;
			return pScan;
		}
		pInfo = __xgeFontInfo(pScan);
		if ( pInfo != NULL ) {
			iGlyph = stbtt_FindGlyphIndex(pInfo, (int)iCodepoint);
			if ( (iGlyph != 0) || (pScan->pFallback == NULL) ) {
				*pGlyph = iGlyph;
				return pScan;
			}
		}
		pScan = pScan->pFallback;
	}
	return NULL;
}

static xge_vec2_t __xgeTextMeasureBounded(xge_font pFont, const char* sText, int iSize)
{
	xge_vec2_t tSize;
	xge_glyph_metrics_t tMetrics;
	xge_font pGlyphFont;
	xge_font pPreviousFont;
	stbtt_fontinfo* pInfo;
	const char* sScan;
	const char* sEnd;
	uint32_t iCodepoint;
	float fLineWidth;
	float fLineRight;
	float fGlyphRight;
	float fKerning;
	float fLineHeight;
	int iGlyph;
	int iPreviousGlyph;
	int iLineCount;

	tSize.fX = 0.0f;
	tSize.fY = 0.0f;
	if ( (pFont == NULL) || (sText == NULL) || (iSize < 0) ) return tSize;
	fLineHeight = pFont->fLineHeight;
	iLineCount = 1;
	sScan = sText;
	sEnd = sText + iSize;
	fLineWidth = 0.0f;
	fLineRight = 0.0f;
	iPreviousGlyph = -1;
	pPreviousFont = NULL;
	while ( sScan < sEnd ) {
		if ( __xgeTextUTF8DecodeBounded(&sScan, sEnd, &iCodepoint) != XGE_OK ) break;
		if ( iCodepoint == '\r' ) {
			if ( (sScan < sEnd) && (*sScan == '\n') ) continue;
			iCodepoint = '\n';
		}
		if ( iCodepoint == '\n' ) {
			if ( fLineRight > tSize.fX ) tSize.fX = fLineRight;
			fLineWidth = 0.0f;
			fLineRight = 0.0f;
			iPreviousGlyph = -1;
			pPreviousFont = NULL;
			iLineCount++;
			continue;
		}
		pGlyphFont = __xgeFontResolveCodepoint(pFont, iCodepoint, &iGlyph);
		if ( (pGlyphFont == NULL) || (xgeFontGlyphGetByIndex(pGlyphFont, iGlyph, &tMetrics) != XGE_OK) ) continue;
		if ( pGlyphFont->fLineHeight > fLineHeight ) fLineHeight = pGlyphFont->fLineHeight;
		if ( (iPreviousGlyph >= 0) && (pPreviousFont == pGlyphFont) ) {
			pInfo = __xgeFontInfo(pGlyphFont);
			if ( pInfo != NULL ) {
				fKerning = (float)stbtt_GetGlyphKernAdvance(pInfo, iPreviousGlyph, iGlyph) * pGlyphFont->fScale;
				fLineWidth += fKerning;
			}
		}
		fGlyphRight = fLineWidth + tMetrics.fX1;
		if ( fGlyphRight > fLineRight ) fLineRight = fGlyphRight;
		fLineWidth += tMetrics.fAdvanceX;
		if ( fLineWidth > fLineRight ) fLineRight = fLineWidth;
		iPreviousGlyph = iGlyph;
		pPreviousFont = pGlyphFont;
	}
	if ( fLineRight > tSize.fX ) tSize.fX = fLineRight;
	tSize.fY = fLineHeight * (float)iLineCount;
	return tSize;
}

xge_vec2_t xgeTextMeasure(xge_font pFont, const char* sText)
{
	xge_vec2_t tSize;

	tSize.fX = 0.0f;
	tSize.fY = 0.0f;
	if ( (pFont == NULL) || (sText == NULL) ) return tSize;
	return __xgeTextMeasureBounded(pFont, sText, (int)strlen(sText));
}

static float __xgeTextLineMeasure(xge_font pFont, const char* sText, int iSize)
{
	xge_vec2_t tSize;

	if ( (pFont == NULL) || (sText == NULL) || (iSize <= 0) ) return 0.0f;
	tSize = __xgeTextMeasureBounded(pFont, sText, iSize);
	return tSize.fX;
}

static const char* __xgeTextLineEnd(const char* sText)
{
	const char* sScan;

	sScan = sText;
	while ( (*sScan != 0) && (*sScan != '\n') ) {
		sScan++;
	}
	return sScan;
}

static void __xgeTextDrawRange(xge_font pFont, const char* sText, int iSize, float fX, float fY, uint32_t iColor, uint32_t iDrawFlags)
{
	xge_text_shape_desc_t tDesc;
	xge_glyph_run_t tRun;

	if ( (pFont == NULL) || (sText == NULL) || (iSize <= 0) ) return;
	memset(&tDesc, 0, sizeof(tDesc));
	memset(&tRun, 0, sizeof(tRun));
	tDesc.iSize = sizeof(tDesc);
	tDesc.pFont = pFont;
	tDesc.sText = sText;
	tDesc.iTextSize = iSize;
	tDesc.iFlags = XGE_TEXT_SHAPE_DEFAULT;
	if ( xgeTextShape(&tDesc, &tRun) == XGE_OK ) {
		xgeGlyphRunDraw(&tRun, fX, fY, iColor, iDrawFlags);
		xgeGlyphRunFree(&tRun);
	}
}

void xgeTextDraw(xge_font pFont, const char* sText, float fX, float fY, uint32_t iColor)
{
	const char* sLine;
	const char* sEnd;
	float fPenY;

	if ( (pFont == NULL) || (sText == NULL) ) {
		return;
	}
	sLine = sText;
	fPenY = fY;
	while ( *sLine != 0 ) {
		sEnd = __xgeTextLineEnd(sLine);
		__xgeTextDrawRange(pFont, sLine, (int)(sEnd - sLine), fX, fPenY, iColor, 0);
		if ( *sEnd == '\n' ) {
			sLine = sEnd + 1;
			fPenY += pFont->fLineHeight;
		} else {
			break;
		}
	}
}

void xgeTextDrawRect(xge_font pFont, const char* sText, xge_rect_t tRect, uint32_t iColor, uint32_t iFlags)
{
	const char* sLine;
	const char* sEnd;
	xge_vec2_t tSize;
	xge_font_metrics_t tFontMetrics;
	xge_rect_t tOldClip;
	xge_rect_t tClip;
	float fPenY;
	float fLineX;
	float fLineWidth;
	float fLeft;
	float fTop;
	float fRight;
	float fBottom;
	uint32_t iDrawFlags;
	int iLineSize;
	int bClip;
	int bOldClip;

	if ( (pFont == NULL) || (sText == NULL) || (tRect.fW <= 0.0f) || (tRect.fH <= 0.0f) ) {
		return;
	}
	iDrawFlags = ((iFlags & XGE_TEXT_SCREEN_SPACE) != 0) ? XGE_DRAW_SCREEN_SPACE : 0;
	memset(&tOldClip, 0, sizeof(tOldClip));
	memset(&tClip, 0, sizeof(tClip));
	bOldClip = 0;
	memset(&tFontMetrics, 0, sizeof(tFontMetrics));
	(void)xgeFontGetMetrics(pFont, &tFontMetrics);
	tSize = xgeTextMeasure(pFont, sText);
	fPenY = tRect.fY;
	if ( (iFlags & XGE_TEXT_ALIGN_BOTTOM) == XGE_TEXT_ALIGN_BOTTOM ) {
		fPenY = tRect.fY + tRect.fH - tSize.fY;
	} else if ( (iFlags & XGE_TEXT_ALIGN_MIDDLE) == XGE_TEXT_ALIGN_MIDDLE ) {
		fPenY = tRect.fY + (tRect.fH - tSize.fY) * 0.5f;
	}
	if ( (iFlags & XGE_TEXT_SCREEN_SPACE) != 0 ) {
		fPenY = __xgeTextSnapPixel(fPenY);
	}
	bClip = ((iFlags & XGE_TEXT_CLIP) != 0);
	if ( bClip ) {
		tOldClip = xgeClipGet();
		bOldClip = (tOldClip.fW > 0.0f) && (tOldClip.fH > 0.0f);
		tClip = tRect;
		if ( bOldClip ) {
			fLeft = (tClip.fX > tOldClip.fX) ? tClip.fX : tOldClip.fX;
			fTop = (tClip.fY > tOldClip.fY) ? tClip.fY : tOldClip.fY;
			fRight = ((tClip.fX + tClip.fW) < (tOldClip.fX + tOldClip.fW)) ? (tClip.fX + tClip.fW) : (tOldClip.fX + tOldClip.fW);
			fBottom = ((tClip.fY + tClip.fH) < (tOldClip.fY + tOldClip.fH)) ? (tClip.fY + tClip.fH) : (tOldClip.fY + tOldClip.fH);
			tClip.fX = fLeft;
			tClip.fY = fTop;
			tClip.fW = fRight - fLeft;
			tClip.fH = fBottom - fTop;
		}
		if ( (tClip.fW <= 0.0f) || (tClip.fH <= 0.0f) ) {
			return;
		}
		xgeFlush();
		xgeClipSet(tClip);
	}
	sLine = sText;
	while ( *sLine != 0 ) {
		sEnd = __xgeTextLineEnd(sLine);
		iLineSize = (int)(sEnd - sLine);
		fLineWidth = __xgeTextLineMeasure(pFont, sLine, iLineSize);
		fLineX = tRect.fX;
		if ( (iFlags & XGE_TEXT_ALIGN_RIGHT) == XGE_TEXT_ALIGN_RIGHT ) {
			fLineX = tRect.fX + tRect.fW - fLineWidth;
		} else if ( (iFlags & XGE_TEXT_ALIGN_CENTER) == XGE_TEXT_ALIGN_CENTER ) {
			fLineX = tRect.fX + (tRect.fW - fLineWidth) * 0.5f;
		}
		if ( (iFlags & XGE_TEXT_SCREEN_SPACE) != 0 ) {
			fLineX = __xgeTextSnapPixel(fLineX);
		}
		__xgeTextDrawRange(pFont, sLine, iLineSize, fLineX, fPenY, iColor, iDrawFlags);
		if ( (iFlags & XGE_TEXT_UNDERLINE) != 0 ) {
			xge_rect_t tUnderline;
			tUnderline.fX = fLineX;
			tUnderline.fY = fPenY + pFont->fAscent + tFontMetrics.fUnderlinePosition;
			tUnderline.fW = fLineWidth;
			tUnderline.fH = tFontMetrics.fUnderlineThickness;
			xgeShapeRectFillPx(tUnderline, iColor);
		}
		if ( *sEnd == '\n' ) {
			sLine = sEnd + 1;
			fPenY += pFont->fLineHeight;
			if ( (iFlags & XGE_TEXT_SCREEN_SPACE) != 0 ) {
				fPenY = __xgeTextSnapPixel(fPenY);
			}
		} else {
			break;
		}
	}
	if ( bClip ) {
		xgeFlush();
		if ( bOldClip ) {
			xgeClipSet(tOldClip);
		} else {
			xgeClipClear();
		}
	}
}
#else
int xgeTextUTF8Next(const char** psText, uint32_t* pCodepoint) { (void)psText; (void)pCodepoint; return XGE_ERROR_UNSUPPORTED; }
int xgeFontFaceLoad(xge_font_face* ppFace, const char* sPath, const xge_font_face_desc_t* pDesc) { (void)ppFace; (void)sPath; (void)pDesc; return XGE_ERROR_UNSUPPORTED; }
int xgeFontFaceLoadMemory(xge_font_face* ppFace, const void* pData, int iSize, const xge_font_face_desc_t* pDesc) { (void)ppFace; (void)pData; (void)iSize; (void)pDesc; return XGE_ERROR_UNSUPPORTED; }
int xgeFontFaceAddRef(xge_font_face pFace) { (void)pFace; return 0; }
void xgeFontFaceFree(xge_font_face pFace) { (void)pFace; }
int xgeFontFaceGetInfo(xge_font_face pFace, xge_font_face_info_t* pInfo) { (void)pFace; (void)pInfo; return XGE_ERROR_UNSUPPORTED; }
int xgeFontFamilyCreate(xge_font_family* ppFamily) { (void)ppFamily; return XGE_ERROR_UNSUPPORTED; }
void xgeFontFamilyFree(xge_font_family pFamily) { (void)pFamily; }
int xgeFontFamilyAddFace(xge_font_family pFamily, xge_font_face pFace) { (void)pFamily; (void)pFace; return XGE_ERROR_UNSUPPORTED; }
int xgeFontFamilyResolve(xge_font_family pFamily, int iWeight, int iSlant, xge_font_face* ppFace) { (void)pFamily; (void)iWeight; (void)iSlant; (void)ppFace; return XGE_ERROR_UNSUPPORTED; }
int xgeFontFamilyResolveEx(xge_font_family pFamily, int iWeight, int iStretch, int iSlant, xge_font_face* ppFace) { (void)pFamily; (void)iWeight; (void)iStretch; (void)iSlant; (void)ppFace; return XGE_ERROR_UNSUPPORTED; }
int xgeFontCreate(xge_font pFont, xge_font_face pFace, const xge_font_instance_desc_t* pDesc) { (void)pFont; (void)pFace; (void)pDesc; return XGE_ERROR_UNSUPPORTED; }
int xgeFontLoad(xge_font pFont, const char* sPath, float fSize) { (void)pFont; (void)sPath; (void)fSize; return XGE_ERROR_UNSUPPORTED; }
int xgeFontLoadMemory(xge_font pFont, const void* pData, int iSize, float fSize) { (void)pFont; (void)pData; (void)iSize; (void)fSize; return XGE_ERROR_UNSUPPORTED; }
int xgeFontLoadXRF(xge_font pFont, const char* sPath) { (void)pFont; (void)sPath; return XGE_ERROR_UNSUPPORTED; }
int xgeFontLoadXRFMemory(xge_font pFont, const void* pData, int iSize) { (void)pFont; (void)pData; (void)iSize; return XGE_ERROR_UNSUPPORTED; }
int xgeFontAddRef(xge_font pFont) { (void)pFont; return 0; }
void xgeFontFree(xge_font pFont) { (void)pFont; }
void xgeFontSetFallback(xge_font pFont, xge_font pFallback) { (void)pFont; (void)pFallback; }
int xgeFontFallbackSet(const char* sPath, float fSize) { (void)sPath; (void)fSize; return XGE_ERROR_UNSUPPORTED; }
int xgeFontFallbackSetMemory(const void* pData, int iSize, float fSize) { (void)pData; (void)iSize; (void)fSize; return XGE_ERROR_UNSUPPORTED; }
int xgeFontFallbackGet(xge_font pFont, float fSize) { (void)pFont; (void)fSize; return XGE_ERROR_UNSUPPORTED; }
int xgeFontFallbackGetEx(xge_font pFont, float fSize, uint32_t iFlags) { (void)pFont; (void)fSize; (void)iFlags; return XGE_ERROR_UNSUPPORTED; }
void xgeFontFallbackClear(void) {}
int xgeFontGetMetrics(xge_font pFont, xge_font_metrics_t* pMetrics) { (void)pFont; (void)pMetrics; return XGE_ERROR_UNSUPPORTED; }
int xgeFontCacheGetStats(xge_font pFont, xge_font_cache_stats_t* pStats) { (void)pFont; (void)pStats; return XGE_ERROR_UNSUPPORTED; }
void xgeFontCacheClear(xge_font pFont) { (void)pFont; }
int xgeFontGlyphGet(xge_font pFont, uint32_t iCodepoint, xge_glyph_metrics_t* pMetrics) { (void)pFont; (void)iCodepoint; (void)pMetrics; return XGE_ERROR_UNSUPPORTED; }
int xgeFontGlyphGetByIndex(xge_font pFont, int iGlyph, xge_glyph_metrics_t* pMetrics) { (void)pFont; (void)iGlyph; (void)pMetrics; return XGE_ERROR_UNSUPPORTED; }
int xgeFontGlyphRasterize(xge_font pFont, uint32_t iCodepoint, xge_glyph_bitmap_t* pBitmap) { (void)pFont; (void)iCodepoint; (void)pBitmap; return XGE_ERROR_UNSUPPORTED; }
int xgeFontGlyphRasterizeByIndex(xge_font pFont, int iGlyph, xge_glyph_bitmap_t* pBitmap) { (void)pFont; (void)iGlyph; (void)pBitmap; return XGE_ERROR_UNSUPPORTED; }
int xgeFontGlyphAtlasGet(xge_font pFont, uint32_t iCodepoint, xge_glyph_t* pGlyph) { (void)pFont; (void)iCodepoint; (void)pGlyph; return XGE_ERROR_UNSUPPORTED; }
int xgeFontGlyphAtlasGetByIndex(xge_font pFont, int iGlyph, xge_glyph_t* pGlyph) { (void)pFont; (void)iGlyph; (void)pGlyph; return XGE_ERROR_UNSUPPORTED; }
void xgeGlyphBitmapFree(xge_glyph_bitmap_t* pBitmap) { (void)pBitmap; }
xge_vec2_t xgeTextMeasure(xge_font pFont, const char* sText) { xge_vec2_t tSize; (void)pFont; (void)sText; tSize.fX = 0.0f; tSize.fY = 0.0f; return tSize; }
void xgeTextDraw(xge_font pFont, const char* sText, float fX, float fY, uint32_t iColor) { (void)pFont; (void)sText; (void)fX; (void)fY; (void)iColor; }
void xgeTextDrawRect(xge_font pFont, const char* sText, xge_rect_t tRect, uint32_t iColor, uint32_t iFlags) { (void)pFont; (void)sText; (void)tRect; (void)iColor; (void)iFlags; }
#endif
