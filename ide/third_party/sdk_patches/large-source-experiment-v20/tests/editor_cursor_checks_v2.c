#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned long long cursorAssertions,cursorCases,cursorPositions,cursorReusedCases,cursorReusedGlyphs;
#define CURSOR_CHECK(e) do{++cursorAssertions;if(!(e)){fprintf(stderr,"cursor check %d: %s\n",__LINE__,#e);exit(1);}}while(0)
typedef struct cursor_compare_t {
    const xge_glyph_run_t* reference;
    const xge_glyph_run_t* base;
    xge_internal_text_resume_t* info;
    int at,started,failAt;
} cursor_compare_t;
static int cursorComparePosition(const xge_glyph_position_t* glyph,void* user) {
    cursor_compare_t* c=user;
    if(!c->started){c->started=1;c->at=c->info->iReplayGlyphIndex;CURSOR_CHECK(c->at>=0 && c->at<=c->reference->iGlyphCount && c->at<=c->base->iGlyphCount);
        for(int i=0;i<c->at;++i)CURSOR_CHECK(!memcmp(&c->base->pGlyphs[i],&c->reference->pGlyphs[i],sizeof(*glyph)));}
    CURSOR_CHECK(c->at<c->reference->iGlyphCount);
    CURSOR_CHECK(!memcmp(glyph,&c->reference->pGlyphs[c->at],sizeof(*glyph)));++cursorPositions;
    if(c->at==c->failAt)return XGE_ERROR_INVALID_STATE;
    ++c->at;return XGE_OK;
}
static void cursorMetadata(const xge_glyph_run_t* a,const xge_glyph_run_t* b) {
    CURSOR_CHECK(a->iSize==b->iSize && a->iFlags==b->iFlags && a->iTextSize==b->iTextSize && a->iGlyphCount==b->iGlyphCount && b->pGlyphs==NULL);
    CURSOR_CHECK(a->fWidth==b->fWidth && a->fHeight==b->fHeight && a->fAscent==b->fAscent && a->fDescent==b->fDescent && a->fLineHeight==b->fLineHeight);
    const xge_glyph_run_backend_t* x=a->pBackend;const xge_glyph_run_backend_t* y=b->pBackend;CURSOR_CHECK((x==NULL)==(y==NULL));
    if(x){CURSOR_CHECK(x->iFontCount==y->iFontCount && x->pEmojiPack==y->pEmojiPack);for(int i=0;i<x->iFontCount;++i)CURSOR_CHECK(x->ppFonts[i]==y->ppFonts[i]);}
}
static int cursorCommon(const char* a,int an,const char* b,int bn) {int n=an<bn?an:bn,i=0;while(i<n && a[i]==b[i])++i;return i;}
static void cursorCase(xge_font font,xge_emoji_pack pack,const char* before,int bn,const char* after,int an,uint32_t flags,int expected,int boundary) {
    xge_text_shape_desc_t d={0};d.iSize=sizeof(d);d.pFont=font;d.pEmojiPack=pack;d.sText=before;d.iTextSize=bn;d.iFlags=flags;d.iEmojiPresentation=XGE_EMOJI_PRESENTATION_AUTO;d.iEmojiLinePolicy=XGE_EMOJI_LINE_EXPAND;d.fEmojiScale=1.25f;
    xge_glyph_run_t old={0},captured={0},reference={0},now={0};xge_internal_text_cursor* cache=NULL,*fresh=NULL;xge_internal_text_resume_t info={0};
    int refs=font->iRefCount,packRefs=pack->iRefCount;
    xge_font fb=font->pFallback;int fbRefs=fb?fb->iRefCount:0;
    CURSOR_CHECK(diagOldTextShape(&d,&old)==XGE_OK);
    cursor_compare_t c={&old,&old,&info,0,0,-1};
    CURSOR_CHECK(xgeInternalTextShapeVisitCached(&d,&captured,cursorComparePosition,&c,NULL,0,&cache,&info)==XGE_OK);
    CURSOR_CHECK(cache!=NULL && c.at==old.iGlyphCount);cursorMetadata(&old,&captured);xgeGlyphRunFree(&captured);
    d.sText=after;d.iTextSize=an;int expectedCode=diagOldTextShape(&d,&reference),common=cursorCommon(before,bn,after,an);
    c=(cursor_compare_t){&reference,&old,&info,0,0,-1};
    int code=xgeInternalTextShapeVisitCached(&d,&now,cursorComparePosition,&c,cache,common,&fresh,&info);CURSOR_CHECK(code==expectedCode);
    if(code==XGE_OK){CURSOR_CHECK(c.at==reference.iGlyphCount);cursorMetadata(&reference,&now);
        if(expected>0)CURSOR_CHECK(info.iTextOffset>0 && info.iReplayGlyphIndex>0);
        if(expected<0)CURSOR_CHECK(info.iTextOffset==0);
        if(boundary>=0)CURSOR_CHECK(info.iTextOffset<=boundary);
        if(info.iReplayGlyphIndex>0){++cursorReusedCases;cursorReusedGlyphs+=(unsigned)info.iReplayGlyphIndex;}
        CURSOR_CHECK(fresh!=NULL && fresh->iCount<=8193);
    }else{CURSOR_CHECK(!c.started && fresh==NULL);xge_glyph_run_t zero={0};CURSOR_CHECK(!memcmp(&now,&zero,sizeof(now)));}
    ++cursorCases;xgeGlyphRunFree(&now);xgeGlyphRunFree(&reference);xgeGlyphRunFree(&old);xgeInternalTextCursorFree(cache);xgeInternalTextCursorFree(fresh);
    CURSOR_CHECK(font->iRefCount==refs && pack->iRefCount==packRefs && (!fb || fb->iRefCount==fbRefs));
}

static void cursorKeyGuard(xge_font font,xge_font fallback,xge_emoji_pack pack,int mode) {
    char text[12290];memset(text,'A',12288);text[12288]='V';text[12289]=0;
    memcpy(text+8190,"😀",4);
    xge_text_shape_desc_t d={0};d.iSize=sizeof(d);d.pFont=font;d.pEmojiPack=pack;d.sText=text;d.iTextSize=12288;
    d.iFlags=XGE_TEXT_SHAPE_KERNING;d.iEmojiPresentation=XGE_EMOJI_PRESENTATION_AUTO;d.iEmojiLinePolicy=XGE_EMOJI_LINE_EXPAND;d.fEmojiScale=1.25f;
    xge_internal_text_cursor *old=NULL,*fresh=NULL; xge_internal_text_resume_t info={0};
    xge_glyph_run_t initial={0},captured={0},reference={0},now={0};
    int refs=font->iRefCount,fallbackRefs=fallback->iRefCount,packRefs=pack->iRefCount;
    float scale=font->fScale,fs=fallback->fScale; xge_font fb=font->pFallback;
    CURSOR_CHECK(diagOldTextShape(&d,&initial)==XGE_OK);
    cursor_compare_t c={&initial,&initial,&info,0,0,-1};
    CURSOR_CHECK(xgeInternalTextShapeVisitCached(&d,&captured,cursorComparePosition,&c,NULL,0,&old,&info)==XGE_OK && old);
    xgeGlyphRunFree(&captured);d.iTextSize=12289;
    switch(mode) {
    case 0:d.iFlags^=XGE_TEXT_SHAPE_KERNING;break;
    case 1:d.fEmojiScale=1.5f;break;
    case 2:d.iEmojiPresentation=XGE_EMOJI_PRESENTATION_TEXT;break;
    case 3:d.iEmojiLinePolicy=XGE_EMOJI_LINE_STABLE;break;
    case 4:font->fScale*=1.25f;break;
    case 5:fallback->fScale*=1.25f;break;
    case 6:font->pFallback=NULL;break;
    case 7:{xge_emoji_metrics_t m={0};m.iSize=sizeof(m);m.fAdvanceEm=1.5f;m.fWidthEm=1.1f;m.fHeightEm=1.2f;m.fBaselineRatio=.7f;
        const char* svg="<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'><rect width='30' height='30' fill='#cc8844'/></svg>";
        CURSOR_CHECK(xgeEmojiPackAddSvgMemory(pack,"😀",svg,(int)strlen(svg),&m,NULL)==XGE_OK);break;}
    case 8:xgeEmojiPackClearDefault();break;
    default:CURSOR_CHECK(0);
    }
    CURSOR_CHECK(diagOldTextShape(&d,&reference)==XGE_OK);c=(cursor_compare_t){&reference,&initial,&info,0,0,-1};
    CURSOR_CHECK(xgeInternalTextShapeVisitCached(&d,&now,cursorComparePosition,&c,old,12288,&fresh,&info)==XGE_OK);
    CURSOR_CHECK(info.iTextOffset==0 && info.iReplayGlyphIndex==0 && fresh && c.at==reference.iGlyphCount);
    cursorMetadata(&reference,&now);++cursorCases;
    font->fScale=scale;fallback->fScale=fs;font->pFallback=fb;
    xgeGlyphRunFree(&initial);xgeGlyphRunFree(&reference);xgeGlyphRunFree(&now);xgeInternalTextCursorFree(old);xgeInternalTextCursorFree(fresh);
    CURSOR_CHECK(font->iRefCount==refs && fallback->iRefCount==fallbackRefs && pack->iRefCount==packRefs);
}

static void verifyCursorCheckpoints(char** args) {
    xge_font_t font={0},fallback={0};char path[4096];CURSOR_CHECK(snprintf(path,sizeof(path),"%s/kern.ttf",args[1])>0);
    CURSOR_CHECK(xgeFontLoad(&font,path,20)==XGE_OK);CURSOR_CHECK(xgeFontLoad(&fallback,args[3],20)==XGE_OK);font.pFallback=&fallback;
    xge_emoji_pack pack=NULL;CURSOR_CHECK(xgeEmojiPackCreate(&pack)==XGE_OK);xge_emoji_metrics_t em={0};em.iSize=sizeof(em);em.fAdvanceEm=1.1f;em.fWidthEm=1.3f;em.fHeightEm=1.2f;em.fBaselineRatio=.8f;
    const char* svg="<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'><circle cx='16' cy='16' r='14' fill='#4488cc'/></svg>";
    const char* sequences[]={"😀","👩","👩‍💻"};
    for(int i=0;i<3;++i)CURSOR_CHECK(xgeEmojiPackAddSvgMemory(pack,sequences[i],svg,(int)strlen(svg),&em,NULL)==XGE_OK);
    char* before=malloc(2097153);char* after=malloc(2097200);CURSOR_CHECK(before && after);
    const char* motifs[]={"AVATAR To WA. ","AV 中文😀 é 👩‍💻 \t\r\n終","A\0V"};int sizes[]={8192,32768};
    for(int motif=0;motif<3;++motif)for(int size=0;size<2;++size)for(int flag=0;flag<4;++flag) {
        int n=sizes[size],m=motif==2?3:(int)strlen(motifs[motif]),used=0;
        while(used+m<=n){memcpy(before+used,motifs[motif],(size_t)m);used+=m;}memset(before+used,'A',(size_t)(n-used));before[n]=0;
        memcpy(after,before,(size_t)n);after[n]='V';after[n+1]=0;cursorCase(&font,pack,before,n,after,n+1,(uint32_t)flag,1,-1);
        int at=n/2;while(at>0 && ((unsigned char)before[at]&0xc0u)==0x80u)--at;
        memcpy(after,before,(size_t)at);after[at]='V';memcpy(after+at+1,before+at,(size_t)(n-at));after[n+1]=0;cursorCase(&font,pack,before,n,after,n+1,(uint32_t)flag,0,at);
    }
    memset(before,'A',8188);memcpy(before+8188,"👩",4);before[8192]=0;
    memcpy(after,before,8192);memcpy(after+8192,"‍💻",7);after[8199]=0;cursorCase(&font,pack,before,8192,after,8199,XGE_TEXT_SHAPE_DEFAULT,1,8188);
    memcpy(after,before,8192);memcpy(after+8192,"︎",3);after[8195]=0;cursorCase(&font,pack,before,8192,after,8195,XGE_TEXT_SHAPE_DEFAULT,1,8188);
    int n=3000;memset(before,'A',3000);memcpy(before+n,"👩",4);n+=4;
    for(int i=0;i<10000;++i){memcpy(before+n,"🏽",4);n+=4;}memset(before+n,'B',8000);n+=8000;before[n]=0;
    memcpy(after,before,(size_t)n+1);int change=3004+9999*4;after[change]='C';memmove(after+change+1,after+change+4,(size_t)(n-change-4+1));cursorCase(&font,pack,before,n,after,n-3,XGE_TEXT_SHAPE_DEFAULT,-1,-1);
    memset(before,'A',2097152);before[2097152]=0;memcpy(after,before,2097152);after[2097152]='V';after[2097153]=0;cursorCase(&font,pack,before,2097152,after,2097153,XGE_TEXT_SHAPE_KERNING,1,-1);
    after[1048576]='V';cursorCase(&font,pack,before,2097152,after,2097153,XGE_TEXT_SHAPE_KERNING,1,1048576);
    memset(before,'A',8192);before[8192]=0;memcpy(after,before,8192);after[8192]=(char)0xff;after[8193]=0;cursorCase(&font,pack,before,8192,after,8193,XGE_TEXT_SHAPE_DEFAULT,0,-1);
    for(int mode=0;mode<9;++mode)cursorKeyGuard(&font,&fallback,pack,mode);
    free(before);free(after);font.pFallback=NULL;xgeFontFree(&font);xgeFontFree(&fallback);xgeEmojiPackFree(pack);
    printf("cursor checkpoint comparison passed: %llu assertions; %llu cases; %llu visited positions; %llu reused cases; %llu reused prefix glyphs\n",cursorAssertions,cursorCases,cursorPositions,cursorReusedCases,cursorReusedGlyphs);fflush(stdout);
}
