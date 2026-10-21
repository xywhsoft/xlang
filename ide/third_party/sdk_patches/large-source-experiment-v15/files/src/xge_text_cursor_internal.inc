/* Private checkpointed shaping. Visitors must keep font/emoji state stable.
   The caller owns both caches; an old cache stays valid until explicitly freed. */
typedef struct xge_cursor_font_key_t {
    xge_font pFont;
    xge_font pFallback;
    void* pBackend;
    void* pData;
    int iDataSize;
    uint32_t iFlags;
    float fSize,fScale,fAscent,fDescent,fLineGap,fLineHeight;
} xge_cursor_font_key_t;
typedef struct xge_cursor_checkpoint_t {
    int iOffset,iVisited,iFontCount,bEmoji,bPending;
    int iPreviousGlyph,iLineCount;
    int64_t iRequiredPrefixEnd;
    xge_font pPreviousFont;
    xge_emoji_pack pEmojiPack;
    xge_glyph_run_t tRun;
    xge_glyph_position_t tPending,tLastVisited;
    float fLineWidth,fLineRight,fMaxWidth,fMaxLineGap;
} xge_cursor_checkpoint_t;
struct xge_internal_text_cursor_t {
    xge_text_shape_desc_t tDesc;
    char* sText;
    int iTextSize,iStride,iCount,iCapacity,bFailed;
    int64_t iNextOffset;
    uint64_t iDefaultRevision,iExplicitRevision,iEffectiveRevision;
    xge_emoji_pack pExplicitPack,pEffectivePack;
    xge_glyph_run_backend_t* pKeyFonts;
    xge_glyph_run_backend_t* pRunFonts;
    xge_cursor_font_key_t* pFontKeys;
    int iFontKeyCount;
    xge_cursor_checkpoint_t* pCheckpoints;
};
typedef struct xge_cursor_record_t {
    xge_internal_text_shape_visit_proc onVisit;
    void* pUser;
    const xge_glyph_run_backend_t* pPrefixFonts;
    int iVisited,iInvocations;
    int64_t iRequiredPrefixEnd;
    xge_glyph_position_t tLastVisited;
} xge_cursor_record_t;
static uint64_t __xgeCursorDefaultRevision(void) {
    uint64_t value;__xgeEmojiGlobalLock();value=g_xgeEmojiDefaultRevision;__xgeEmojiGlobalUnlock();return value;
}
static void __xgeCursorBackendFree(xge_glyph_run_backend_t* backend) {
    xge_glyph_run_t run={0};run.pBackend=backend;xgeGlyphRunFree(&run);
}
void xgeInternalTextCursorFree(xge_internal_text_cursor* cursor) {
    if(!cursor)return;
    __xgeCursorBackendFree(cursor->pKeyFonts);__xgeCursorBackendFree(cursor->pRunFonts);
    xgeEmojiPackFree(cursor->pExplicitPack);xgeEmojiPackFree(cursor->pEffectivePack);
    xrtFree(cursor->sText);xrtFree(cursor->pFontKeys);xrtFree(cursor->pCheckpoints);xrtFree(cursor);
}
static xge_cursor_font_key_t __xgeCursorFontKey(xge_font font) {
    xge_cursor_font_key_t k={0};k.pFont=font;k.pFallback=font->pFallback;k.pBackend=font->pBackend;k.pData=font->pData;
    k.iDataSize=font->iDataSize;k.iFlags=font->iFlags;k.fSize=font->fSize;k.fScale=font->fScale;
    k.fAscent=font->fAscent;k.fDescent=font->fDescent;k.fLineGap=font->fLineGap;k.fLineHeight=font->fLineHeight;return k;
}
static int __xgeCursorFontsEqual(const xge_internal_text_cursor* cursor) {
    for(int i=0;i<cursor->iFontKeyCount;++i){xge_cursor_font_key_t k=__xgeCursorFontKey(cursor->pFontKeys[i].pFont);if(memcmp(&k,&cursor->pFontKeys[i],sizeof(k)))return 0;}return 1;
}
static xge_internal_text_cursor* __xgeCursorCreate(const xge_text_shape_desc_t* desc,int size) {
    xge_internal_text_cursor* c=xrtCalloc(1,sizeof(*c));xge_font font;
    if(!c)return NULL;
    c->tDesc=*desc;c->iTextSize=size;c->iStride=size/8192+1;if(c->iStride<4096)c->iStride=4096;c->iNextOffset=c->iStride;
    c->sText=xrtMalloc((size_t)size+1u);c->pKeyFonts=xrtCalloc(1,sizeof(*c->pKeyFonts));c->pFontKeys=xrtCalloc(64,sizeof(*c->pFontKeys));
    if(!c->sText || !c->pKeyFonts || !c->pFontKeys)goto fail;
    memcpy(c->sText,desc->sText,(size_t)size);c->sText[size]=0;c->tDesc.sText=c->sText;
    for(font=desc->pFont;font;font=font->pFallback) {
        int found=0;for(int i=0;i<c->iFontKeyCount;++i)if(c->pFontKeys[i].pFont==font){found=1;break;}if(found)break;
        if(c->iFontKeyCount>=64 || __xgeGlyphRunKeepFont(c->pKeyFonts,font)!=XGE_OK)goto fail;
        c->pFontKeys[c->iFontKeyCount++]=__xgeCursorFontKey(font);
    }
    c->iDefaultRevision=__xgeCursorDefaultRevision();
    if(desc->pEmojiPack){if(xgeEmojiPackAddRef(desc->pEmojiPack)<=0)goto fail;c->pExplicitPack=desc->pEmojiPack;c->iExplicitRevision=desc->pEmojiPack->iShapeRevision;}
    return c;
fail:xgeInternalTextCursorFree(c);return NULL;
}
static int __xgeCursorKeyEqual(const xge_internal_text_cursor* c,const xge_text_shape_desc_t* d) {
    const xge_text_shape_desc_t* a=&c->tDesc;
    if(a->pFont!=d->pFont || a->pEmojiPack!=d->pEmojiPack || a->iFlags!=d->iFlags ||
       a->iEmojiPresentation!=d->iEmojiPresentation || a->iEmojiLinePolicy!=d->iEmojiLinePolicy || a->fEmojiScale!=d->fEmojiScale || !__xgeCursorFontsEqual(c))return 0;
    if(c->iDefaultRevision==UINT64_MAX || c->iDefaultRevision!=__xgeCursorDefaultRevision())return 0;
    if(c->pExplicitPack && (c->iExplicitRevision==UINT64_MAX || c->iExplicitRevision!=c->pExplicitPack->iShapeRevision))return 0;
    if(c->pEffectivePack && (c->iEffectiveRevision==UINT64_MAX || c->iEffectiveRevision!=c->pEffectivePack->iShapeRevision))return 0;
    return 1;
}
static int __xgeCursorVisit(const xge_glyph_position_t* glyph,void* user) {
    xge_cursor_record_t* r=user;int code;xge_glyph_position_t saved=*glyph;
    ++r->iInvocations;code=r->onVisit(glyph,r->pUser);if(code==XGE_OK){r->tLastVisited=saved;++r->iVisited;}return code;
}
static int __xgeCursorMatchEmoji(xge_emoji_pack pack,const char* text,const char* end,int presentation,
    xge_emoji_match_t* match,xge_cursor_record_t* record,const char* origin) {
    xge_emoji_read_extent_t read={0};int code;
    if(!record)return __xgeEmojiMatchForText(pack,text,end,presentation,match);
    code=__xgeEmojiMatchForTextCursor(pack,text,end,presentation,match,&read);
    int64_t required=(int64_t)(text-origin)+read.iRead+(read.bEnd?1:0);
    if(required>record->iRequiredPrefixEnd)record->iRequiredPrefixEnd=required;
    return code;
}
static void __xgeCursorCapture(xge_internal_text_cursor* c,int offset,const xge_glyph_run_t* run,
    int previousGlyph,xge_font previousFont,xge_emoji_pack pack,float width,float right,float maximum,float gap,
    int lines,int pending,const xge_glyph_position_t* glyph,const xge_cursor_record_t* r) {
    xge_cursor_checkpoint_t* p;xge_glyph_run_backend_t* backend=run->pBackend;
    if(!c || c->bFailed || offset<c->iNextOffset)return;
    if(c->iCount==c->iCapacity){int capacity=c->iCapacity?c->iCapacity*2:16;
        p=xrtRealloc(c->pCheckpoints,(size_t)capacity*sizeof(*p));if(!p){c->bFailed=1;return;}c->pCheckpoints=p;c->iCapacity=capacity;}
    p=&c->pCheckpoints[c->iCount++];memset(p,0,sizeof(*p));p->iOffset=offset;p->iVisited=r->iVisited;p->iRequiredPrefixEnd=r->iRequiredPrefixEnd;
    p->iFontCount=backend->iFontCount;p->bEmoji=backend->pEmojiPack!=NULL;p->bPending=pending;
    p->iPreviousGlyph=previousGlyph;p->pPreviousFont=previousFont;p->pEmojiPack=pack;p->iLineCount=lines;
    p->fLineWidth=width;p->fLineRight=right;p->fMaxWidth=maximum;p->fMaxLineGap=gap;
    p->tRun=*run;p->tRun.pBackend=NULL;p->tRun.pGlyphs=NULL;if(pending)p->tPending=*glyph;if(r->iVisited)p->tLastVisited=r->tLastVisited;
    c->iNextOffset=(int64_t)offset+c->iStride;
}
static xge_glyph_run_backend_t* __xgeCursorCloneBackend(const xge_glyph_run_backend_t* source,int fonts,int emoji) {
    xge_glyph_run_backend_t* result=xrtCalloc(1,sizeof(*result));if(!result)return NULL;
    for(int i=0;i<fonts;++i)if(__xgeGlyphRunKeepFont(result,source->ppFonts[i])!=XGE_OK)goto fail;
    if(emoji && source->pEmojiPack && __xgeGlyphRunKeepEmojiPack(result,source->pEmojiPack)!=XGE_OK)goto fail;
    return result;
fail:__xgeCursorBackendFree(result);return NULL;
}
