/* Private, bounded viewport replay. Whole-line metrics and font/emoji lifetime
   remain owned by the cache. An ink envelope covers every glyph in each byte
   bucket; nonmonotonic pens and SVG/fallback overflow do not justify skipping
   a bucket. Unsupported or uncertain bounds use the original complete scan. */
#define XGE_VIEWPORT_PAINT_STRIDE 4096
#define XGE_VIEWPORT_PAINT_MEMO 64
typedef struct xge_viewport_ink_t {double fLeft,fRight;int bInk;} xge_viewport_ink_t;
typedef struct xge_viewport_memo_t {
    xge_font pFont;
    uint32_t iKind,iEmoji;
    int iGlyph,bSet,bInk;
    float fOffsetX,fWidth,fHeight,fLeft,fRight;
} xge_viewport_memo_t;
typedef struct xge_viewport_paint_t {
    xge_internal_text_cursor* pCursor;
    xge_glyph_run_t tMetrics;
    xge_viewport_ink_t* pInk;
    int iCount,bValid;
} xge_viewport_paint_t;
typedef struct xge_viewport_collect_t {
    xge_viewport_paint_t* pIndex;
    xge_text_paint_collect_t tCollect;
    xge_viewport_memo_t tMemo[XGE_VIEWPORT_PAINT_MEMO];
    double fOrigin;
    int iStart,iEnd,bStop;
} xge_viewport_collect_t;

void xgeInternalTextPaintViewportFree(void* pValue)
{
    xge_viewport_paint_t* p=pValue;
    if (!p) return;
    xgeInternalTextCursorFree(p->pCursor);
    xgeGlyphRunFree(&p->tMetrics);
    xrtFree(p->pInk);xrtFree(p);
}

int xgeInternalTextPaintViewportValid(void* pValue,const xge_text_shape_desc_t* pDesc)
{
    xge_viewport_paint_t* p=pValue;
    return p && p->bValid && p->pCursor && p->pCursor->iTextSize==pDesc->iTextSize &&
        __xgeCursorKeyEqual(p->pCursor,pDesc);
}

static int __xgeViewportLocalInk(xge_viewport_collect_t* p,const xge_glyph_position_t* g,
    float* pLeft,float* pRight,int* pInk)
{
    unsigned int hash=(unsigned int)g->iGlyph*31u+g->iEmojiId*7u+(unsigned int)((uintptr_t)g->pFont>>4);
    xge_viewport_memo_t* m=&p->tMemo[hash&(XGE_VIEWPORT_PAINT_MEMO-1u)];
    float left=0,right=0;int ink=0;
    if (m->bSet && m->pFont==g->pFont && m->iGlyph==g->iGlyph && m->iKind==g->iItemKind &&
        m->iEmoji==g->iEmojiId && m->fOffsetX==g->fOffsetX && m->fWidth==g->fEmojiWidth && m->fHeight==g->fEmojiHeight) {
        *pLeft=m->fLeft;*pRight=m->fRight;*pInk=m->bInk;return 1;
    }
    if (g->iItemKind==XGE_TEXT_ITEM_GLYPH) {
        xge_glyph_t glyph;
        if (xgeFontGlyphAtlasGetByIndex(g->pFont,g->iGlyph,&glyph)!=XGE_OK) return 0;
        ink=glyph.iPage>=0 && glyph.iWidth>0 && glyph.iHeight>0;
        left=glyph.fOffsetX+g->fOffsetX;right=left+(float)glyph.iWidth;
    } else if (g->iItemKind==XGE_TEXT_ITEM_EMOJI) {
        xge_glyph_run_backend_t* backend=p->pIndex->tMetrics.pBackend;
        xge_rect_t rect={0},bounds={0};
        rect.fX=g->fOffsetX;rect.fW=g->fEmojiWidth;rect.fH=g->fEmojiHeight;
        if (!backend || !__xgeEmojiHorizontalPaintBounds(backend->pEmojiPack,g->iEmojiId,rect,&bounds)) return 0;
        left=bounds.fX;right=bounds.fX+bounds.fW;ink=bounds.fW>0;
        /* A failed SVG draw may paint the fallback even when its SVG is outside. */
        if (g->pFont && g->iGlyph>=0) {
            xge_glyph_t fallback;
            if (xgeFontGlyphAtlasGetByIndex(g->pFont,g->iGlyph,&fallback)!=XGE_OK) return 0;
            if (fallback.iPage>=0 && fallback.iWidth>0 && fallback.iHeight>0) {
                float l=fallback.fOffsetX,r=l+(float)fallback.iWidth;
                if (!ink || l<left) left=l;
                if (!ink || r>right) right=r;
                ink=1;
            }
        }
    } else return 0;
    if (!isfinite(left) || !isfinite(right) || right<left) return 0;
    m->bSet=1;m->pFont=g->pFont;m->iGlyph=g->iGlyph;m->iKind=g->iItemKind;m->iEmoji=g->iEmojiId;
    m->fOffsetX=g->fOffsetX;m->fWidth=g->fEmojiWidth;m->fHeight=g->fEmojiHeight;
    m->fLeft=left;m->fRight=right;m->bInk=ink;
    *pLeft=left;*pRight=right;*pInk=ink;return 1;
}

static int __xgeViewportIndexPosition(const xge_glyph_position_t* g,void* user)
{
    xge_viewport_collect_t* p=user;xge_viewport_paint_t* index=p->pIndex;
    float left=0,right=0;int ink=0;
    int bucket=(int)(g->iCluster/XGE_VIEWPORT_PAINT_STRIDE);
    if (index->bValid && ((g->iFlags&XGE_GLYPH_POSITION_LINE_BREAK) || bucket<0 || bucket>=index->iCount ||
        !isfinite(p->tCollect.fPenX) || !__xgeViewportLocalInk(p,g,&left,&right,&ink))) index->bValid=0;
    if (index->bValid && ink) {
        xge_viewport_ink_t* b=&index->pInk[bucket];
        double x=p->tCollect.fPenX-p->fOrigin;
        double l=x+left-4.0,r=x+right+4.0;
        if (!b->bInk || l<b->fLeft) b->fLeft=l;
        if (!b->bInk || r>b->fRight) b->fRight=r;
        b->bInk=1;
    }
    return __xgeTextPaintCollectPosition(g,&p->tCollect);
}

static int __xgeViewportReplayPosition(const xge_glyph_position_t* g,void* user)
{
    xge_viewport_collect_t* p=user;
    if (g->iCluster>=(uint32_t)p->iEnd) {p->bStop=1;return XGE_ERROR_UNSUPPORTED;}
    if (g->iCluster<(uint32_t)p->iStart) {p->tCollect.fPenX+=g->fAdvanceX;return XGE_OK;}
    return __xgeTextPaintCollectPosition(g,&p->tCollect);
}

static int __xgeViewportBucketVisible(const xge_viewport_ink_t* b,double origin,xge_rect_i_t clip)
{
    return b->bInk && origin+b->fRight>=(double)clip.iX-2.0 && origin+b->fLeft<=(double)clip.iX+clip.iW+2.0;
}

int xgeInternalTextPaintViewport(const xge_text_shape_desc_t* desc,float x,float y,uint32_t color,
    const xge_text_paint_span_t* spans,int spanCount,void** ppPaint,void** ppIndex)
{
    xge_viewport_paint_t* index;
    xge_viewport_collect_t state={0};
    xge_glyph_paint_cache_t* paint=NULL;
    xge_rect_i_t clip=xgeClipGetPixels();
    int code=XGE_OK;
    if (!ppPaint || !ppIndex || !desc || desc->iTextSize<=0 || spanCount<0 || (spanCount && !spans)) return XGE_ERROR_INVALID_ARGUMENT;
    *ppPaint=NULL;index=*ppIndex;
    if (clip.iW<=0 || clip.iH<=0 || !isfinite(x) || !isfinite(y))
        return xgeInternalTextPaintStream(desc,x,y,color,spans,spanCount,ppPaint);
    if (index && (!xgeInternalTextPaintViewportValid(index,desc) || memcmp(index->pCursor->sText,desc->sText,(size_t)desc->iTextSize))) {
        xgeInternalTextPaintViewportFree(index);index=NULL;*ppIndex=NULL;
    }
    paint=xrtCalloc(1,sizeof(*paint));
    if (!paint) return xgeInternalTextPaintStream(desc,x,y,color,spans,spanCount,ppPaint);
    paint->bValid=1;paint->tClip=clip;
    state.fOrigin=__xgeTextSnapPixel(x);state.tCollect.fPenX=state.fOrigin;
    state.tCollect.pCandidates=paint;state.tCollect.tClip=clip;
    if (!index) {
        index=xrtCalloc(1,sizeof(*index));
        if (index) {
            index->iCount=desc->iTextSize/XGE_VIEWPORT_PAINT_STRIDE+1;
            index->pInk=xrtCalloc((size_t)index->iCount,sizeof(*index->pInk));
        }
        if (!index || !index->pInk) {xgeInternalTextPaintViewportFree(index);__xgeGlyphRunPaintCacheFree(paint);return xgeInternalTextPaintStream(desc,x,y,color,spans,spanCount,ppPaint);}
        index->bValid=1;state.pIndex=index;state.tCollect.pMetrics=&index->tMetrics;
        code=xgeInternalTextShapeVisitCached(desc,&index->tMetrics,__xgeViewportIndexPosition,&state,NULL,0,&index->pCursor,NULL);
        if (code!=XGE_OK || !index->pCursor || !index->bValid) {
            xgeInternalTextPaintViewportFree(index);index=NULL;
        } else *ppIndex=index;
    } else {
        state.pIndex=index;state.tCollect.pMetrics=&index->tMetrics;
        for (int at=0;at<index->iCount && code==XGE_OK;) {
            int begin=at,end;
            const xge_cursor_checkpoint_t* restore=NULL;
            xge_glyph_run_t visited={0};xge_cursor_record_t record={0};
            if (!__xgeViewportBucketVisible(&index->pInk[at],state.fOrigin,clip)) {++at;continue;}
            while (++at<index->iCount && __xgeViewportBucketVisible(&index->pInk[at],state.fOrigin,clip)) {}
            end=at*XGE_VIEWPORT_PAINT_STRIDE;
            if (end>desc->iTextSize) end=desc->iTextSize;
            for (int cp=0;cp<index->pCursor->iCount;++cp) {
                const xge_cursor_checkpoint_t* candidate=&index->pCursor->pCheckpoints[cp];
                if (candidate->iOffset<=begin*XGE_VIEWPORT_PAINT_STRIDE && candidate->iVisited>0) restore=candidate;
                else if (candidate->iOffset>begin*XGE_VIEWPORT_PAINT_STRIDE) break;
            }
            state.iStart=begin*XGE_VIEWPORT_PAINT_STRIDE;state.iEnd=end;state.bStop=0;state.tCollect.fPenX=state.fOrigin+(restore?restore->fReplayPenX:0.0);
            record.bValidatedInput=1;record.pPrefixFonts=index->pCursor->pRunFonts;
            code=__xgeTextShape(desc,&visited,__xgeViewportReplayPosition,&state,NULL,restore,&record,index->pCursor->pEffectivePack,1);
            if (state.bStop && code==XGE_ERROR_UNSUPPORTED) code=XGE_OK;
            xgeGlyphRunFree(&visited);
        }
    }
    if (code!=XGE_OK || !index || state.tCollect.bFailed || !paint->bValid || !isfinite(y+index->tMetrics.fAscent)) {
        __xgeGlyphRunPaintCacheFree(paint);
        if (code!=XGE_OK) return code;
        return xgeInternalTextPaintStream(desc,x,y,color,spans,spanCount,ppPaint);
    }
    {
        xge_glyph_run_backend_t* backend=index->tMetrics.pBackend;
        int count=paint->tRun.iGlyphCount;
        xge_glyph_position_t* positions=paint->tRun.pGlyphs;
        xge_glyph_draw_clip_t drawClip={0};float penY=__xgeTextSnapPixel(y+index->tMetrics.fAscent);
        __xgeGlyphRunUploadAtlas(&index->tMetrics);
        paint->tRun=index->tMetrics;paint->tRun.pGlyphs=positions;paint->tRun.pBackend=NULL;paint->tRun.iGlyphCount=0;
        for (int i=0;i<count;++i) {
            xge_glyph_position_t position=positions[i];float penX=paint->pPens[i].fX;
            __xgeGlyphRunDrawPosition(&position,backend,penX,penY,color,XGE_DRAW_SCREEN_SPACE,spans,spanCount,paint,&drawClip);
        }
        if (paint->bValid) {
            paint->tRun.pBackend=__xgeCursorCloneBackend(backend,backend->iFontCount,backend->pEmojiPack!=NULL);
            if (paint->tRun.pBackend) {*ppPaint=paint;paint=NULL;}
        }
    }
    __xgeGlyphRunPaintCacheFree(paint);return XGE_OK;
}
