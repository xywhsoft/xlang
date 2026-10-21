#include "xge_text_shape_internal.h"
#ifndef XGE_NO_TEXT
typedef struct xge_glyph_run_backend_t {
	int iFontCount;
	int iFontCapacity;
	xge_font* ppFonts;
	xge_emoji_pack pEmojiPack;
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

static int __xgeGlyphRunKeepEmojiPack(xge_glyph_run_backend_t* pBackend, xge_emoji_pack pPack)
{
	if ( (pBackend == NULL) || (pPack == NULL) ) return XGE_ERROR_INVALID_ARGUMENT;
	if ( pBackend->pEmojiPack == pPack ) return XGE_OK;
	if ( pBackend->pEmojiPack != NULL ) return XGE_ERROR_INVALID_STATE;
	if ( xgeEmojiPackAddRef(pPack) <= 0 ) return XGE_ERROR_INVALID_ARGUMENT;
	pBackend->pEmojiPack = pPack;
	return XGE_OK;
}

/* Bounded, call-local lookup reuse. No font state survives the shape call,
   and collisions use the original resolver before replacing this slot. */
#define XGE_TEXT_SHAPE_LOOKUP_SLOTS 128u
typedef struct xge_text_shape_lookup_t {
    uint32_t iCodepointKey;
    int iGlyph;
    xge_font pFont;
} xge_text_shape_lookup_t;

static xge_font __xgeTextShapeResolveCodepoint(xge_font pFont, uint32_t iCodepoint,
    int* pGlyph, xge_text_shape_lookup_t* pLookup)
{
    xge_text_shape_lookup_t* pEntry;
    xge_font pResolved;
    uint32_t iKey = iCodepoint + 1u;
    if ( pLookup == NULL || iKey == 0u ) {
        return __xgeFontResolveCodepoint(pFont, iCodepoint, pGlyph);
    }
    pEntry = &pLookup[(iCodepoint * 0x9e3779b9u) >> 25];
    if ( pEntry->iCodepointKey == iKey ) {
        *pGlyph = pEntry->iGlyph;
        return pEntry->pFont;
    }
    pResolved = __xgeFontResolveCodepoint(pFont, iCodepoint, pGlyph);
    /* A failed lookup leaves the caller's glyph output untouched. */
    if ( pResolved != NULL ) {
        pEntry->iCodepointKey = iKey;
        pEntry->iGlyph = *pGlyph;
        pEntry->pFont = pResolved;
    }
    return pResolved;
}

/* Raw kerning depends on an immutable face and the two glyph indices.
   Keep only a bounded call-local table; apply the live scale at the original
   call site, including when a synchronous visitor changes font scale. */
#define XGE_TEXT_SHAPE_KERN_SLOTS 64u
typedef struct xge_text_shape_kern_t {
    const stbtt_fontinfo* pInfo;
    int iLeft;
    int iRight;
    int iAdvance;
} xge_text_shape_kern_t;

static int __xgeTextShapeKernAdvance(const stbtt_fontinfo* pInfo, int iLeft,
    int iRight, xge_text_shape_kern_t* pTable)
{
    xge_text_shape_kern_t* pEntry;
    uint32_t iHash;
    int iAdvance;
    if ( pTable == NULL ) return stbtt_GetGlyphKernAdvance(pInfo, iLeft, iRight);
    iHash = (uint32_t)((uintptr_t)pInfo >> 4) ^
        (uint32_t)iLeft * 0x9e3779b9u ^ (uint32_t)iRight * 0x85ebca6bu;
    iHash ^= iHash >> 16;
    pEntry = &pTable[iHash & (XGE_TEXT_SHAPE_KERN_SLOTS - 1u)];
    if ( pEntry->pInfo == pInfo && pEntry->iLeft == iLeft && pEntry->iRight == iRight ) {
        return pEntry->iAdvance;
    }
    iAdvance = stbtt_GetGlyphKernAdvance(pInfo, iLeft, iRight);
    pEntry->pInfo = pInfo;
    pEntry->iLeft = iLeft;
    pEntry->iRight = iRight;
    pEntry->iAdvance = iAdvance;
    return iAdvance;
}

#include "xge_text_cursor_internal.inc"

static int __xgeTextShape(const xge_text_shape_desc_t* pDesc, xge_glyph_run_t* pRun,
    xge_internal_text_shape_visit_proc onVisit, void* pUser,
    xge_internal_text_cursor* pCursor, const xge_cursor_checkpoint_t* pRestore, xge_cursor_record_t* pRecord, xge_emoji_pack pPinnedDefault, int bPinDefault)
{
	xge_glyph_run_backend_t* pBackend;
    xge_text_shape_lookup_t tLookup[XGE_TEXT_SHAPE_LOOKUP_SLOTS];
    xge_text_shape_lookup_t* pLookup;
    xge_text_shape_kern_t tKern[XGE_TEXT_SHAPE_KERN_SLOTS];
    xge_text_shape_kern_t* pKern = NULL;
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
	float fLineWidth;
	float fLineRight;
	float fMaxWidth;
	float fGlyphRight;
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
    if ( iTextSize >= 256 && (pDesc->iFlags & XGE_TEXT_SHAPE_KERNING) != 0 ) {
        memset(tKern, 0, sizeof(tKern));
        pKern = tKern;
    }
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
    if ( pRestore != NULL ) {
        const xge_glyph_run_backend_t* pPrefix = pRecord->pPrefixFonts;
        int iFont;
        for ( iFont=0; iFont<pRestore->iFontCount; ++iFont ) {
            if ( __xgeGlyphRunKeepFont(pBackend,pPrefix->ppFonts[iFont]) != XGE_OK ) { xgeGlyphRunFree(pRun); return XGE_ERROR_OUT_OF_MEMORY; }
        }
        if ( pRestore->bEmoji && __xgeGlyphRunKeepEmojiPack(pBackend,pPrefix->pEmojiPack) != XGE_OK ) { xgeGlyphRunFree(pRun); return XGE_ERROR_OUT_OF_MEMORY; }
        *pRun=pRestore->tRun;pRun->pBackend=pBackend;pRun->iTextSize=iTextSize;
        sScan=pDesc->sText+pRestore->iOffset;
        fLineWidth=pRestore->fLineWidth;fLineRight=pRestore->fLineRight;fMaxWidth=pRestore->fMaxWidth;
        fMaxLineGap=pRestore->fMaxLineGap;iLineCount=pRestore->iLineCount;
        iPreviousGlyph=pRestore->iPreviousGlyph;pPreviousFont=pRestore->pPreviousFont;pEmojiPack=pRestore->pEmojiPack;
        bPending=pRestore->bPending;if(bPending)tPending=pRestore->tPending;
        iRet=onVisit(&pRestore->tLastVisited,pUser);
        if(iRet!=XGE_OK){xgeGlyphRunFree(pRun);return iRet;}
    }
	while ( sScan < sEnd ) {
        __xgeCursorCapture(pCursor,(int)(sScan-pDesc->sText),pRun,
            iPreviousGlyph,pPreviousFont,pEmojiPack,fLineWidth,fLineRight,fMaxWidth,fMaxLineGap,
            iLineCount,bPending,&tPending,pRecord);

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
				pEmojiPack = bPinDefault ? pPinnedDefault : __xgeEmojiDefaultAcquire();
				if ( pEmojiPack != NULL ) {
					if ( __xgeGlyphRunKeepEmojiPack(pBackend, pEmojiPack) != XGE_OK ) {
						if (!bPinDefault) xgeEmojiPackFree(pEmojiPack);
						xgeGlyphRunFree(pRun);
						return XGE_ERROR_OUT_OF_MEMORY;
					}
					if (!bPinDefault) xgeEmojiPackFree(pEmojiPack);
					pEmojiPack = pBackend->pEmojiPack;
				}
			}
		}
		if ( ((pDesc->iFlags & XGE_TEXT_SHAPE_EMOJI) != 0) && (pEmojiPack != NULL) &&
		     (__xgeCursorMatchEmoji(pEmojiPack, sBefore, sEnd, iEmojiPresentation, &tEmojiMatch, pRecord, pDesc->sText) == XGE_OK) ) {
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
				fKerning = (float)__xgeTextShapeKernAdvance(pInfo, iPreviousGlyph, iGlyph, pKern) * pGlyphFont->fScale;
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
        __xgeCursorCapture(pCursor,(int)(sScan-pDesc->sText),pRun,
            iPreviousGlyph,pPreviousFont,pEmojiPack,fLineWidth,fLineRight,fMaxWidth,fMaxLineGap,
            iLineCount,bPending,&tPending,pRecord);
    if ( onVisit != NULL && bPending ) {
        iRet = onVisit(&tPending, pUser);
        if ( iRet != XGE_OK ) { xgeGlyphRunFree(pRun); return iRet; }
    }
	if ( fLineRight > fMaxWidth ) fMaxWidth = fLineRight;
	pRun->fWidth = fMaxWidth;
	pRun->fHeight = pRun->fLineHeight * (float)iLineCount;
	return XGE_OK;
}

int xgeTextShape(const xge_text_shape_desc_t* pDesc, xge_glyph_run_t* pRun)
{
    return __xgeTextShape(pDesc, pRun, NULL, NULL, NULL, NULL, NULL, NULL, 0);
}

int xgeInternalTextShapeVisit(const xge_text_shape_desc_t* pDesc, xge_glyph_run_t* pRun,
    xge_internal_text_shape_visit_proc onVisit, void* pUser)
{
    if ( onVisit == NULL ) return XGE_ERROR_INVALID_ARGUMENT;
    return __xgeTextShape(pDesc, pRun, onVisit, pUser, NULL, NULL, NULL, NULL, 0);
}

int xgeInternalTextShapeVisitCached(const xge_text_shape_desc_t* desc,xge_glyph_run_t* run,
    xge_internal_text_shape_visit_proc visit,void* user,const xge_internal_text_cursor* old,int prefix,
    xge_internal_text_cursor** output,xge_internal_text_resume_t* info) {
    xge_internal_text_cursor* cursor;const xge_cursor_checkpoint_t* restore=NULL;
    xge_cursor_record_t record={0};xge_internal_text_resume_t local={0};int size,code,index=-1;
    if(output)*output=NULL;
    if(!info)info=&local;
    memset(info,0,sizeof(*info));
    if(!desc || desc->iSize<sizeof(*desc) || !desc->pFont || !desc->sText || !run || !visit || !output || desc->iTextSize < -1)return XGE_ERROR_INVALID_ARGUMENT;
    size=desc->iTextSize<=0?(int)strlen(desc->sText):desc->iTextSize;
    cursor=__xgeCursorCreate(desc,size);
    if(old && prefix>=0 && prefix<=size && prefix<=old->iTextSize && __xgeCursorKeyEqual(old,desc) && !memcmp(old->sText,desc->sText,(size_t)prefix)) {
        for(int i=0;i<old->iCount;++i) {
            const xge_cursor_checkpoint_t* p=&old->pCheckpoints[i];
            if(p->iOffset<=prefix && p->iRequiredPrefixEnd<=prefix && p->iVisited>0)index=i;
        }
        if(index>=0)restore=&old->pCheckpoints[index];
    }
    record.onVisit=visit;record.pUser=user;record.pPrefixFonts=old?old->pRunFonts:NULL;
    if(restore) {
        record.iVisited=restore->iVisited-1;record.iRequiredPrefixEnd=restore->iRequiredPrefixEnd;
        info->iTextOffset=restore->iOffset;info->iReplayGlyphIndex=record.iVisited;info->iReplayTextOffset=(int)restore->tLastVisited.iCluster;
        if(cursor) {
            cursor->pCheckpoints=xrtMalloc((size_t)(index+1)*sizeof(*cursor->pCheckpoints));
            if(cursor->pCheckpoints){memcpy(cursor->pCheckpoints,old->pCheckpoints,(size_t)(index+1)*sizeof(*cursor->pCheckpoints));cursor->iCount=cursor->iCapacity=index+1;cursor->iNextOffset=(int64_t)restore->iOffset+cursor->iStride;}
            else cursor->bFailed=1;
        }
    }
    code=__xgeTextShape(desc,run,__xgeCursorVisit,&record,cursor,restore,&record,NULL,0);
    if(code!=XGE_OK){xgeInternalTextCursorFree(cursor);return code;}
    if(cursor && !cursor->bFailed && __xgeCursorKeyEqual(cursor,desc)) {
        xge_glyph_run_backend_t* backend=run->pBackend;
        if(backend) {
            cursor->pRunFonts=__xgeCursorCloneBackend(backend,backend->iFontCount,backend->pEmojiPack!=NULL);
            if(!cursor->pRunFonts)cursor->bFailed=1;
            if(backend->pEmojiPack && xgeEmojiPackAddRef(backend->pEmojiPack)>0){cursor->pEffectivePack=backend->pEmojiPack;cursor->iEffectiveRevision=backend->pEmojiPack->iShapeRevision;}
        }
        if(!cursor->bFailed){*output=cursor;info->iCheckpointCount=cursor->iCount;cursor=NULL;}
    }
    xgeInternalTextCursorFree(cursor);return code;
}

void xgeGlyphRunFree(xge_glyph_run_t* pRun)
{
	xge_glyph_run_backend_t* pBackend;
	int i;

	if ( pRun == NULL ) return;
	pBackend = (xge_glyph_run_backend_t*)pRun->pBackend;
	if ( pBackend != NULL ) {
		for ( i = 0; i < pBackend->iFontCount; i++ ) xgeFontFree(pBackend->ppFonts[i]);
		xgeEmojiPackFree(pBackend->pEmojiPack);
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

static void __xgeGlyphRunUploadAtlas(const xge_glyph_run_t* pRun)
{
	xge_glyph_run_backend_t* pBackend;
	xge_glyph_atlas_page_t* pPages;
	int i,j;
	pBackend = (xge_glyph_run_backend_t*)pRun->pBackend;
	if ( pBackend == NULL ) return;
	for ( i = 0; i < pBackend->iFontCount; i++ ) {
		pPages = (xge_glyph_atlas_page_t*)pBackend->ppFonts[i]->tAtlas.pPages;
		for ( j = 0; (pPages != NULL) && (j < pBackend->ppFonts[i]->tAtlas.iPageCount); j++ ) {
			if ( pPages[j].bDirty || (pPages[j].tTexture.iBackendId == 0) ) (void)__xgeFontAtlasUploadPage(pBackend->ppFonts[i], j);
		}
	}
}

static void __xgeGlyphRunPrepareAtlas(const xge_glyph_run_t* pRun)
{
	xge_glyph_t tGlyph;
	int i;

	for ( i = 0; i < pRun->iGlyphCount; i++ ) {
		if ( ((pRun->pGlyphs[i].iFlags & XGE_GLYPH_POSITION_LINE_BREAK) == 0) &&
		     (pRun->pGlyphs[i].iItemKind == XGE_TEXT_ITEM_GLYPH) ) {
			(void)xgeFontGlyphAtlasGetByIndex(pRun->pGlyphs[i].pFont, pRun->pGlyphs[i].iGlyph, &tGlyph);
		}
	}
    __xgeGlyphRunUploadAtlas(pRun);
}

static uint32_t __xgeGlyphRunSpanColor(const xge_glyph_position_t* pPosition,
	uint32_t iColor, const xge_text_paint_span_t* pSpans, int iSpanCount)
{
	uint32_t iClusterEnd;
	int i;

	if ( pPosition == NULL || pSpans == NULL || iSpanCount <= 0 ) return iColor;
	iClusterEnd = pPosition->iClusterEnd;
	if ( iClusterEnd <= pPosition->iCluster ) iClusterEnd = pPosition->iCluster + 1u;
	for ( i = 0; i < iSpanCount; i++ ) {
		const xge_text_paint_span_t* pSpan = &pSpans[i];
		int iStart = pSpan->iStart;
		int iEnd = pSpan->iEnd;
		if ( iEnd <= 0 || iEnd <= iStart ) continue;
		if ( iStart < 0 ) iStart = 0;
		if ( (uint32_t)iEnd <= pPosition->iCluster || (uint32_t)iStart >= iClusterEnd ) continue;
		iColor = pSpan->iColor;
	}
	return iColor;
}

/* Bounded visible paint records. Pens are captured after the original ordered
   float additions, rather than reconstructed by adding skipped advances. */
#define XGE_GLYPH_PAINT_CACHE_MAX 4096
typedef struct xge_glyph_paint_cache_t {
    xge_glyph_run_t tRun;
    xge_vec2_t* pPens;
    int iCapacity;
    int bValid;
    xge_rect_i_t tClip;
} xge_glyph_paint_cache_t;

static void __xgeGlyphPaintCacheAdd(xge_glyph_paint_cache_t* pCache,
    const xge_glyph_position_t* pPosition, float fPenX, float fPenY)
{
    xge_glyph_position_t* pGlyphs;
    xge_vec2_t* pPens;
    int iCapacity;
    int i;
    if ( pCache == NULL || !pCache->bValid ) return;
    i = pCache->tRun.iGlyphCount;
    if ( i >= XGE_GLYPH_PAINT_CACHE_MAX ) { pCache->bValid = 0; return; }
    if ( i >= pCache->iCapacity ) {
        iCapacity = pCache->iCapacity > 0 ? pCache->iCapacity * 2 : 128;
        pGlyphs = (xge_glyph_position_t*)xrtRealloc(pCache->tRun.pGlyphs,
            (size_t)iCapacity * sizeof(*pGlyphs));
        if ( pGlyphs == NULL ) { pCache->bValid = 0; return; }
        pCache->tRun.pGlyphs = pGlyphs;
        pPens = (xge_vec2_t*)xrtRealloc(pCache->pPens, (size_t)iCapacity * sizeof(*pPens));
        if ( pPens == NULL ) { pCache->bValid = 0; return; }
        pCache->pPens = pPens;
        pCache->iCapacity = iCapacity;
    }
    pCache->tRun.pGlyphs[i] = *pPosition;
    pCache->pPens[i].fX = fPenX;
    pCache->pPens[i].fY = fPenY;
    pCache->tRun.iGlyphCount = i + 1;
}

static void __xgeGlyphPaintCacheEmoji(xge_glyph_paint_cache_t* pCache,
    const xge_glyph_position_t* pPosition, float fPenX, float fPenY, int bSvgMayPaint)
{
    xge_glyph_metrics_t tMetrics;
    xge_rect_t tFallbackBounds;
    if ( pCache == NULL || !pCache->bValid ) return;
    if ( !bSvgMayPaint && pPosition->pFont != NULL && pPosition->iGlyph >= 0 ) {
        /* Keep potential fallback ink as well, so a later SVG load failure can
           still draw the same fallback glyph without losing an omitted item. */
        if ( xgeFontGlyphGetByIndex(pPosition->pFont, pPosition->iGlyph, &tMetrics) != XGE_OK ) {
            pCache->bValid = 0;
            return;
        }
        tFallbackBounds.fX = fPenX + tMetrics.fX0;
        tFallbackBounds.fY = fPenY + tMetrics.fY0;
        tFallbackBounds.fW = tMetrics.fX1 - tMetrics.fX0;
        tFallbackBounds.fH = tMetrics.fY1 - tMetrics.fY0;
        bSvgMayPaint = !__xgeTextOutsidePixelClip(tFallbackBounds, pCache->tClip);
    }
    if ( bSvgMayPaint ) __xgeGlyphPaintCacheAdd(pCache, pPosition, fPenX, fPenY);
}

/* Private cross-TU entry points; no public declarations or exports. */
void __xgeGlyphRunPaintCacheFree(void* pOpaque)
{
    xge_glyph_paint_cache_t* pCache = (xge_glyph_paint_cache_t*)pOpaque;
    if ( pCache == NULL ) return;
    xgeGlyphRunFree(&pCache->tRun);
    xrtFree(pCache->pPens);
    xrtFree(pCache);
}

/* A draw-local snapshot avoids four pixel roundings per ordinary glyph.
   Compare the live state on every query: SVG rendering can temporarily change
   the clip, and error paths must not reuse a stale pre-SVG rectangle. */
typedef struct xge_glyph_draw_clip_t {
    xge_rect_t tRect;
    xge_rect_i_t tPixels;
    int bEnabled;
    int bValid;
} xge_glyph_draw_clip_t;

static xge_rect_i_t __xgeGlyphDrawClipPixels(xge_glyph_draw_clip_t* pClip)
{
    if ( !pClip->bValid || pClip->bEnabled != g_xge.bClipEnabled ||
         memcmp(&pClip->tRect, &g_xge.tClipRect, sizeof(pClip->tRect)) != 0 ) {
        pClip->tRect = g_xge.tClipRect;
        pClip->bEnabled = g_xge.bClipEnabled;
        pClip->tPixels = xgeClipGetPixels();
        pClip->bValid = 1;
    }
    return pClip->tPixels;
}

static void __xgeGlyphRunDrawPosition(const xge_glyph_position_t* pPosition,
    xge_glyph_run_backend_t* pBackend, float fPenX, float fPenY, uint32_t iColor,
    uint32_t iFlags, const xge_text_paint_span_t* pSpans, int iSpanCount,
    xge_glyph_paint_cache_t* pCache, xge_glyph_draw_clip_t* pClip)
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
				iEmojiRet = __xgeEmojiDraw(
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
                 !__xgeTextOutsidePixelClip(tDraw.tDst, __xgeGlyphDrawClipPixels(pClip)) ) {
                __xgeGlyphPaintCacheAdd(pCache, pPosition, fPenX, fPenY);
                xgeDrawEx(&tDraw);
            }
		}
        if ( iGlyphRet != XGE_OK && pCache != NULL ) pCache->bValid = 0;
}
static void __xgeGlyphRunDrawSpans(const xge_glyph_run_t* pRun, float fX, float fY,
    uint32_t iColor, uint32_t iFlags, const xge_text_paint_span_t* pSpans, int iSpanCount,
    xge_glyph_paint_cache_t* pCache)
{
    const xge_glyph_position_t* pPosition;
    xge_glyph_run_backend_t* pBackend;
    float fPenX;
    float fPenY;
    int i;
    xge_glyph_draw_clip_t tClip = {0};
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
        __xgeGlyphRunDrawPosition(pPosition, pBackend, fPenX, fPenY, iColor,
            iFlags, pSpans, iSpanCount, pCache, &tClip);
        fPenX += pPosition->fAdvanceX;
    }
}

void* __xgeGlyphRunPaintCacheCaptureDraw(xge_glyph_run_t* pRun, float fX, float fY,
    uint32_t iColor, const xge_text_paint_span_t* pSpans, int iSpanCount)
{
    xge_glyph_paint_cache_t* pCache = NULL;
    xge_rect_i_t tClip = xgeClipGetPixels();
    if ( pRun != NULL && pRun->pBackend != NULL && tClip.iW > 0 && tClip.iH > 0 &&
         isfinite(fX) && isfinite(fY) ) {
        pCache = (xge_glyph_paint_cache_t*)xrtCalloc(1, sizeof(*pCache));
        if ( pCache != NULL ) {
            pCache->tRun = *pRun;
            pCache->tRun.pGlyphs = NULL;
            pCache->tRun.pBackend = NULL;
            pCache->tRun.iGlyphCount = 0;
            pCache->tClip = tClip;
            pCache->bValid = 1;
        }
    }
    __xgeGlyphRunDrawSpans(pRun, fX, fY, iColor, XGE_DRAW_SCREEN_SPACE, pSpans, iSpanCount, pCache);
    if ( pCache != NULL && pCache->bValid ) {
        pCache->tRun.pBackend = pRun->pBackend;
        pRun->pBackend = NULL;
        return pCache;
    }
    __xgeGlyphRunPaintCacheFree(pCache);
    return NULL;
}

void __xgeGlyphRunPaintCacheDraw(void* pOpaque, uint32_t iColor,
    const xge_text_paint_span_t* pSpans, int iSpanCount)
{
    xge_glyph_paint_cache_t* pCache = (xge_glyph_paint_cache_t*)pOpaque;
    xge_glyph_run_backend_t* pBackend;
    int i;
    xge_glyph_draw_clip_t tClip = {0};
    if ( pCache == NULL ) return;
    __xgeGlyphRunPrepareAtlas(&pCache->tRun);
    pBackend = (xge_glyph_run_backend_t*)pCache->tRun.pBackend;
    for ( i = 0; i < pCache->tRun.iGlyphCount; i++ )
        __xgeGlyphRunDrawPosition(&pCache->tRun.pGlyphs[i], pBackend,
            pCache->pPens[i].fX, pCache->pPens[i].fY, iColor,
            XGE_DRAW_SCREEN_SPACE, pSpans, iSpanCount, NULL, &tClip);
}

/* Retain no full glyph array. First complete metrics and atlas preparation;
   then draw the final positions with the original ordered pen additions.
   The first pass's effective default pack is pinned for the second pass while
   preserving automatic activation at the first emoji-capable codepoint. */
static int __xgeTextPaintPreparePosition(const xge_glyph_position_t* pPosition, void* pUser)
{
    xge_glyph_t tGlyph;
    (void)pUser;
    if ((pPosition->iFlags & XGE_GLYPH_POSITION_LINE_BREAK)==0 &&
        pPosition->iItemKind==XGE_TEXT_ITEM_GLYPH)
        (void)xgeFontGlyphAtlasGetByIndex(pPosition->pFont,pPosition->iGlyph,&tGlyph);
    return XGE_OK;
}

typedef struct xge_text_paint_stream_t {
    xge_glyph_run_backend_t* pBackend;
    xge_glyph_paint_cache_t* pCache;
    const xge_text_paint_span_t* pSpans;
    xge_glyph_draw_clip_t tClip;
    float fOriginX,fPenX,fPenY,fLineHeight;
    uint32_t iColor;
    int iSpanCount;
} xge_text_paint_stream_t;

static int __xgeTextPaintDrawPosition(const xge_glyph_position_t* pPosition,void* pUser)
{
    xge_text_paint_stream_t* p=(xge_text_paint_stream_t*)pUser;
    if (pPosition->iFlags & XGE_GLYPH_POSITION_LINE_BREAK) {
        p->fPenX=p->fOriginX;
        p->fPenY+=p->fLineHeight;
        return XGE_OK;
    }
    __xgeGlyphRunDrawPosition(pPosition,p->pBackend,p->fPenX,p->fPenY,p->iColor,
        XGE_DRAW_SCREEN_SPACE,p->pSpans,p->iSpanCount,p->pCache,&p->tClip);
    p->fPenX+=pPosition->fAdvanceX;
    return XGE_OK;
}

int xgeInternalTextPaintStream(const xge_text_shape_desc_t* pDesc,float fX,float fY,
    uint32_t iColor,const xge_text_paint_span_t* pSpans,int iSpanCount,void** ppPaint)
{
    xge_glyph_run_t tMetrics={0},tVisited={0};
    xge_glyph_run_backend_t* pBackend;
    xge_glyph_paint_cache_t* pCache=NULL;
    xge_text_paint_stream_t tState={0};
    xge_rect_i_t tClip=xgeClipGetPixels();
    int iRet;
    if (!ppPaint || iSpanCount<0 || (iSpanCount>0 && !pSpans)) return XGE_ERROR_INVALID_ARGUMENT;
    *ppPaint=NULL;
    iRet=xgeInternalTextShapeVisit(pDesc,&tMetrics,__xgeTextPaintPreparePosition,NULL);
    if (iRet!=XGE_OK) return iRet;
    pBackend=(xge_glyph_run_backend_t*)tMetrics.pBackend;
    __xgeGlyphRunUploadAtlas(&tMetrics);
    if (pBackend && tClip.iW>0 && tClip.iH>0 && isfinite(fX) && isfinite(fY)) {
        pCache=xrtCalloc(1,sizeof(*pCache));
        if (pCache) {
            pCache->tRun=tMetrics;pCache->tRun.pBackend=NULL;pCache->tRun.pGlyphs=NULL;pCache->tRun.iGlyphCount=0;
            pCache->tClip=tClip;pCache->bValid=1;
        }
    }
    tState.pBackend=pBackend;tState.pCache=pCache;tState.pSpans=pSpans;tState.iSpanCount=iSpanCount;
    tState.fOriginX=fX;tState.fPenX=__xgeTextSnapPixel(fX);
    tState.fPenY=__xgeTextSnapPixel(fY+tMetrics.fAscent);tState.fLineHeight=tMetrics.fLineHeight;tState.iColor=iColor;
    iRet=__xgeTextShape(pDesc,&tVisited,__xgeTextPaintDrawPosition,&tState,NULL,NULL,NULL,
        pBackend?pBackend->pEmojiPack:NULL,1);
    if (iRet==XGE_OK && pCache && pCache->bValid) {
        pCache->tRun.pBackend=tMetrics.pBackend;tMetrics.pBackend=NULL;*ppPaint=pCache;pCache=NULL;
    }
    __xgeGlyphRunPaintCacheFree(pCache);xgeGlyphRunFree(&tMetrics);xgeGlyphRunFree(&tVisited);
    return iRet;
}

void xgeGlyphRunDraw(const xge_glyph_run_t* pRun, float fX, float fY, uint32_t iColor, uint32_t iFlags)
{
	__xgeGlyphRunDrawSpans(pRun, fX, fY, iColor, iFlags, NULL, 0, NULL);
}

void xgeGlyphRunDrawSpans(const xge_glyph_run_t* pRun, float fX, float fY, uint32_t iColor,
	uint32_t iFlags, const xge_text_paint_span_t* pSpans, int iSpanCount)
{
	__xgeGlyphRunDrawSpans(pRun, fX, fY, iColor, iFlags, pSpans, iSpanCount, NULL);
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
			iGlyphEnd = (pPosition->iClusterEnd > pPosition->iCluster)
				? pPosition->iClusterEnd : ((i + 1 < pRun->iGlyphCount)
					? pRun->pGlyphs[i + 1].iCluster : (uint32_t)pRun->iTextSize);
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
int xgeInternalTextShapeVisit(const xge_text_shape_desc_t* pDesc, xge_glyph_run_t* pRun, xge_internal_text_shape_visit_proc onVisit, void* pUser) { (void)pDesc; (void)pRun; (void)onVisit; (void)pUser; return XGE_ERROR_UNSUPPORTED; }
void xgeGlyphRunFree(xge_glyph_run_t* pRun) { if ( pRun != NULL ) memset(pRun, 0, sizeof(*pRun)); }
xge_vec2_t xgeGlyphRunMeasure(const xge_glyph_run_t* pRun) { xge_vec2_t t = {0.0f, 0.0f}; (void)pRun; return t; }
int xgeGlyphRunHitTest(const xge_glyph_run_t* pRun, float fX, float fY, uint32_t* pCluster, int* pTrailing) { (void)pRun; (void)fX; (void)fY; (void)pCluster; (void)pTrailing; return XGE_ERROR_UNSUPPORTED; }
void xgeGlyphRunDraw(const xge_glyph_run_t* pRun, float fX, float fY, uint32_t iColor, uint32_t iFlags) { (void)pRun; (void)fX; (void)fY; (void)iColor; (void)iFlags; }
void xgeGlyphRunDrawSpans(const xge_glyph_run_t* pRun, float fX, float fY, uint32_t iColor, uint32_t iFlags, const xge_text_paint_span_t* pSpans, int iSpanCount) { (void)pRun; (void)fX; (void)fY; (void)iColor; (void)iFlags; (void)pSpans; (void)iSpanCount; }
void xgeGlyphRunDrawDecorated(const xge_glyph_run_t* pRun, float fX, float fY, uint32_t iColor, uint32_t iFlags, const xge_text_decoration_t* pDecorations, int iDecorationCount) { (void)pRun; (void)fX; (void)fY; (void)iColor; (void)iFlags; (void)pDecorations; (void)iDecorationCount; }

#endif
