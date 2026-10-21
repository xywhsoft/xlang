/* Optional paint envelopes captured by the code editor's geometry visitor.
   The metrics backend is borrowed only during Record; cursor ownership keeps
   fonts/packs alive, and Seed makes an independent retained painting copy. */
typedef struct xge_cursor_ink_t {
    xge_viewport_paint_t tIndex;
    xge_viewport_collect_t tState;
} xge_cursor_ink_t;

static void* __xgeCursorInkCreate(int bytes)
{
    xge_cursor_ink_t* p=xrtCalloc(1,sizeof(*p));
    if(!p)return NULL;
    p->tIndex.iCount=bytes/XGE_VIEWPORT_PAINT_STRIDE+1;
    p->tIndex.pInk=xrtCalloc((size_t)p->tIndex.iCount,sizeof(*p->tIndex.pInk));
    if(!p->tIndex.pInk){xrtFree(p);return NULL;}
    p->tIndex.bValid=1;p->tState.pIndex=&p->tIndex;return p;
}
static void __xgeCursorInkFree(void* value)
{
    xge_cursor_ink_t* p=value;if(!p)return;
    xrtFree(p->tIndex.pInk);xrtFree(p);
}
static void __xgeCursorInkCopyPrefix(void* value,const void* previous,int replay)
{
    xge_cursor_ink_t* p=value;const xge_cursor_ink_t* old=previous;
    int count=replay/XGE_VIEWPORT_PAINT_STRIDE+1;
    if(!p)return;
    if(!old || !old->tIndex.bValid){p->tIndex.bValid=0;return;}
    if(count>p->tIndex.iCount)count=p->tIndex.iCount;
    if(count>old->tIndex.iCount)count=old->tIndex.iCount;
    /* Retaining an old partial-bucket envelope only adds false positives. The
       unchanged prefix remains covered; the replay adds all new suffix ink. */
    memcpy(p->tIndex.pInk,old->tIndex.pInk,(size_t)count*sizeof(*p->tIndex.pInk));
}
static void __xgeCursorInkRecord(void* value,const xge_glyph_position_t* glyph,double pen,const xge_glyph_run_t* metrics)
{
    xge_cursor_ink_t* p=value;float left=0,right=0;int ink=0,bucket;
    if(!p || !p->tIndex.bValid)return;
    p->tIndex.tMetrics.pBackend=metrics->pBackend;
    bucket=(int)(glyph->iCluster/XGE_VIEWPORT_PAINT_STRIDE);
    if((glyph->iFlags&XGE_GLYPH_POSITION_LINE_BREAK) || bucket<0 || bucket>=p->tIndex.iCount ||
       !isfinite(pen) || !__xgeViewportLocalInk(&p->tState,glyph,&left,&right,&ink)) {
        p->tIndex.bValid=0;p->tIndex.tMetrics.pBackend=NULL;return;
    }
    if(ink) {
        xge_viewport_ink_t* b=&p->tIndex.pInk[bucket];
        double l=pen+left-4.0,r=pen+right+4.0;
        if(!b->bInk || l<b->fLeft)b->fLeft=l;
        if(!b->bInk || r>b->fRight)b->fRight=r;
        b->bInk=1;
    }
    p->tIndex.tMetrics.pBackend=NULL;
}

int xgeInternalTextPaintViewportSeed(const xge_text_shape_desc_t* desc,const xge_internal_text_cursor* source,void** output)
{
    const xge_cursor_ink_t* ink;
    xge_viewport_paint_t* p=NULL;xge_internal_text_cursor* copy=NULL;
    if(!desc || desc->iSize<sizeof(*desc) || !desc->sText || !desc->pFont || !source || !output || *output ||
       source->iTextSize!=desc->iTextSize || !__xgeCursorKeyEqual(source,desc) ||
       memcmp(source->sText,desc->sText,(size_t)source->iTextSize))return XGE_ERROR_UNSUPPORTED;
    ink=source->pPaintInk;
    if(!ink || !ink->tIndex.bValid || !source->pRunFonts)return XGE_ERROR_UNSUPPORTED;
    p=xrtCalloc(1,sizeof(*p));copy=__xgeCursorCreate(desc,source->iTextSize);
    if(!p || !copy)goto fail;
    if(source->iCount) {
        copy->pCheckpoints=xrtMalloc((size_t)source->iCount*sizeof(*copy->pCheckpoints));
        if(!copy->pCheckpoints)goto fail;
        memcpy(copy->pCheckpoints,source->pCheckpoints,(size_t)source->iCount*sizeof(*copy->pCheckpoints));
        copy->iCount=copy->iCapacity=source->iCount;copy->iNextOffset=source->iNextOffset;
    }
    copy->pRunFonts=__xgeCursorCloneBackend(source->pRunFonts,source->pRunFonts->iFontCount,source->pRunFonts->pEmojiPack!=NULL);
    if(!copy->pRunFonts)goto fail;
    if(source->pEffectivePack) {
        if(xgeEmojiPackAddRef(source->pEffectivePack)<=0)goto fail;
        copy->pEffectivePack=source->pEffectivePack;copy->iEffectiveRevision=source->iEffectiveRevision;
    }
    p->iCount=ink->tIndex.iCount;p->pInk=xrtMalloc((size_t)p->iCount*sizeof(*p->pInk));
    if(!p->pInk)goto fail;
    memcpy(p->pInk,ink->tIndex.pInk,(size_t)p->iCount*sizeof(*p->pInk));
    p->tMetrics=source->tFinal;
    p->tMetrics.pBackend=__xgeCursorCloneBackend(source->pRunFonts,source->pRunFonts->iFontCount,source->pRunFonts->pEmojiPack!=NULL);
    if(!p->tMetrics.pBackend)goto fail;
    p->pCursor=copy;copy=NULL;p->bValid=1;
    if(!__xgeCursorKeyEqual(p->pCursor,desc))goto fail;
    *output=p;return XGE_OK;
fail:
    xgeInternalTextCursorFree(copy);xgeInternalTextPaintViewportFree(p);return XGE_ERROR_OUT_OF_MEMORY;
}
