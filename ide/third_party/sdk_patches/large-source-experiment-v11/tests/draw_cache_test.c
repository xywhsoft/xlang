#include <stdio.h>
#include <stdlib.h>
static unsigned long long assertions, pixelCases, reuseCases, errorCases;
static int started;
#define CHECK(e) do { ++assertions; if (!(e)) { fprintf(stderr,"draw cache check %d: %s\n",__LINE__,#e); fflush(stderr); exit(1); } } while (0)

static xui_font fontLoad(const char* latin, const char* cjk, float size) {
    xui_font f = xrtCalloc(1, sizeof(*f)); CHECK(f);
    CHECK(xgeFontLoad(&f->tFont, latin, size) == XGE_OK);
    CHECK(xgeFontLoad(&f->tFallbackFont, cjk, size) == XGE_OK);
    xgeFontSetFallback(&f->tFont, &f->tFallbackFont);
    f->bHasFallback = 1; f->iMagic = XUI_PROXY_XGE_FONT_MAGIC;
    return f;
}
static void runsEqual(const xge_glyph_run_t* a, const xge_glyph_run_t* b) {
    CHECK(a->iFlags == b->iFlags && a->iTextSize == b->iTextSize && a->iGlyphCount == b->iGlyphCount);
    CHECK(a->fWidth == b->fWidth && a->fHeight == b->fHeight && a->fAscent == b->fAscent && a->fDescent == b->fDescent && a->fLineHeight == b->fLineHeight);
    for (int i=0; i<a->iGlyphCount; ++i) CHECK(!memcmp(a->pGlyphs+i,b->pGlyphs+i,sizeof(*a->pGlyphs)));
}
static unsigned char oldPixels[512*128*4], newPixels[512*128*4];
static xui_proxy_t proxy;
static struct xui_surface_t surface;
static int paint(int old, xui_font f, const char* text, int length, float x, uint32_t color, uint32_t flags,
                 const xui_text_paint_span_t* spans, int count, unsigned char* pixels) {
    struct xui_draw_context_t draw = {0}; draw.iMagic = XUI_PROXY_XGE_DRAW_MAGIC; draw.pTarget = &surface;
    xgePassInit(&draw.tPass, &surface.tTarget, XGE_PASS_CLEAR_COLOR, 0xffefefefu);
    CHECK(xgePassBegin(&draw.tPass) == XGE_OK);
    xgeClipSetPixels((xge_rect_i_t){8,8,496,112});
    xui_rect_t rect = {x,12,512,128};
    int code = old ? diagOldDrawTextSpans(&proxy,&draw,f,text,length,rect,color,flags,spans,count)
                   : __xuiProxyXgeDrawTextSpans(&proxy,&draw,f,text,length,rect,color,flags,spans,count);
    CHECK(draw.bDirty == (code == XGE_OK && length > 0));
    CHECK(xgePassEnd(&draw.tPass) == XGE_OK); xgeClipClear();
    CHECK(xgeRenderTargetReadPixels(&surface.tTarget,pixels,512*4) == XGE_OK);
    return code;
}
static int compare(xui_font f, const char* text, int length, float x, uint32_t color, uint32_t flags,
                    const xui_text_paint_span_t* spans, int count) {
    int a = paint(1,f,text,length,x,color,flags,spans,count,oldPixels);
    int b = paint(0,f,text,length,x,color,flags,spans,count,newPixels);
    CHECK(a == b); CHECK(!memcmp(oldPixels,newPixels,sizeof(oldPixels)));
    if(a==XGE_OK) ++pixelCases; else ++errorCases;
    return a;
}
static void verifyRun(xui_font f, const char* text, int length) {
    CHECK(g_xuiProxyXgeDrawCache.pFont == f && g_xuiProxyXgeDrawCache.iTextSize == length);
    CHECK(g_xuiProxyXgeDrawCache.sText != text && !memcmp(g_xuiProxyXgeDrawCache.sText,text,(size_t)length));
    CHECK(g_xuiProxyXgeDrawCache.tRun.pGlyphs == diagLastGlyphs); /* Moved, never copied. */
    xge_text_shape_desc_t desc = {0}; xge_glyph_run_t run = {0};
    desc.iSize=sizeof(desc); desc.pFont=&f->tFont; desc.sText=text; desc.iTextSize=length;
    desc.iFlags=XGE_TEXT_SHAPE_DEFAULT; desc.iEmojiPresentation=XGE_EMOJI_PRESENTATION_AUTO;
    desc.iEmojiLinePolicy=XGE_EMOJI_LINE_STABLE; desc.fEmojiScale=1;
    CHECK(xgeTextShape(&desc,&run)==XGE_OK); runsEqual(&run,&g_xuiProxyXgeDrawCache.tRun); xgeGlyphRunFree(&run);
}
static int verify(void* user) {
    if(started) return XGE_OK;
    started=1; char** args=user;
    CHECK(xgeBegin()==XGE_OK); int originalFonts=g_xge.iFontCount;
    surface.iMagic=XUI_PROXY_XGE_SURFACE_MAGIC; surface.iKind=XUI_SURFACE_KIND_TEXTURE;
    surface.iFormat=XUI_SURFACE_FORMAT_RGBA8; surface.iFlags=XUI_SURFACE_USAGE_TARGET;
    CHECK(xgeRenderTargetCreate(&surface.tTarget,512,128)==XGE_OK);
    xge_emoji_pack builtin=NULL; CHECK(xgeEmojiPackLoadBuiltin(&builtin)==XGE_OK);
    CHECK(xgeEmojiPackSetDefault(builtin)==XGE_OK); int builtinRefs=builtin->iRefCount;
    char* text=malloc(32769); CHECK(text);
    const char* patterns[]={"AVATAR 0123456789 \t", "中文😀 é 👩‍💻 ", "\r\nA\tB\n", "\xff\xc0\xaf\xed\xa0\x80\xf0\x9f", "אב العربية क्", "\xf4\x8f\xbf\xbf ", "A\0B"};
    int lengths[]={4095,4096,8192,32768}; float sizes[]={16,20,24,32};
    xui_text_paint_span_t spans[20]; memset(spans,0,sizeof(spans));
    for(int i=0;i<20;++i) {spans[i].iStart=i*3;spans[i].iEnd=i*3+7;spans[i].iColor=0xffaa3300u+(uint32_t)i*1021u;}
    for(int sz=0;sz<4;++sz) {
        xui_font f=fontLoad(args[2],args[3],sizes[sz]);
        compare(f,"AVATAR",6,12,0xff102030u,0,NULL,0);
        unsigned int nonempty=0;
        for(size_t i=0;i<sizeof(oldPixels);i+=4)if(memcmp(oldPixels+i,oldPixels,4))++nonempty;
        CHECK(nonempty>100);
        for(int pattern=0;pattern<7;++pattern) for(int limit=0;limit<4;++limit) {
            int length=lengths[limit], n=pattern==6 ? 3 : (int)strlen(patterns[pattern]);
            int used=0; while(used+n<=length) {memcpy(text+used,patterns[pattern],(size_t)n);used+=n;}
            memset(text+used,'A',(size_t)(length-used));text[length]=0;
            __xuiProxyXgeDrawCacheClear(NULL); int refs=f->tFont.iRefCount;
            for(int variant=0;variant<3;++variant) {
                float x=variant==2 ? -100.5f : 12.25f; uint32_t color=variant ? 0xffaa0022u : 0xff102030u;
                unsigned long long calls=diagShapeCalls;
                int code=compare(f,text,length,x,color,(uint32_t)variant,spans,variant==0 ? 0 : variant==1 ? 3 : 20);
                if(code!=XGE_OK) {
                    CHECK(g_xuiProxyXgeDrawCache.pFont==NULL);
                    if(length>=4096)CHECK(diagShapeCalls==calls+1);
                    CHECK(f->tFont.iRefCount==refs);
                } else if(length>=4096) {
                    CHECK(diagShapeCalls==calls+(variant==0 ? 1u : 0u));
                    if(variant==0)verifyRun(f,text,length); else ++reuseCases;
                    CHECK(f->tFont.iRefCount>=refs);
                } else CHECK(g_xuiProxyXgeDrawCache.pFont==NULL);
            }
            __xuiProxyXgeDrawCacheClear(NULL);CHECK(f->tFont.iRefCount==refs);
        }
        __xuiProxyXgeFontDestroy(&proxy,f);
    }
    xui_font f=fontLoad(args[2],args[3],20), other=fontLoad(args[2],args[3],24);
    memset(text,'A',8192);text[8192]=0;
    compare(f,text,8192,12,0xff102030u,0,NULL,0);verifyRun(f,text,8192);
    unsigned long long calls=diagShapeCalls; text[0]='V'; text[8191]='B';
    compare(f,text,8192,12,0xff102030u,0,NULL,0);CHECK(diagShapeCalls==calls+1);verifyRun(f,text,8192);
    void* glyphs=g_xuiProxyXgeDrawCache.tRun.pGlyphs;calls=diagShapeCalls;
    compare(f,"small",5,12,0xff102030u,0,NULL,0);
    CHECK(g_xuiProxyXgeDrawCache.tRun.pGlyphs==glyphs && diagShapeCalls==calls);
    /* Live physical-font replacement without a preceding explicit clear. */
    compare(other,text,8192,12,0xff102030u,0,NULL,0);verifyRun(other,text,8192);
    CHECK(g_xuiProxyXgeDrawCache.pFont==other);
    compare(f,text,8192,12,0xff102030u,0,NULL,0);verifyRun(f,text,8192);
    glyphs=g_xuiProxyXgeDrawCache.tRun.pGlyphs;
    __xuiProxyXgeFontDestroy(&proxy,other);CHECK(g_xuiProxyXgeDrawCache.tRun.pGlyphs==glyphs);
    xui_text_shape_t shape={0};CHECK(__xuiProxyXgeTextShape(&proxy,f,text,8192,XUI_TEXT_SHAPE_DEFAULT,&shape)==XGE_OK);
    CHECK(g_xuiProxyXgeDrawCache.pFont==NULL);xrtFree(shape.pClusters);
    compare(f,text,8192,12,0xff102030u,0,NULL,0);xui_vec2_t measured;
    CHECK(__xuiProxyXgeTextMeasure(&proxy,f,text,&measured)==XGE_OK);CHECK(g_xuiProxyXgeDrawCache.pFont==NULL);
    compare(f,text,8192,12,0xff102030u,0,NULL,0);glyphs=g_xuiProxyXgeDrawCache.tRun.pGlyphs;
    CHECK(__xuiProxyXgeTextMeasure(&proxy,f,"M",&measured)==XGE_OK);CHECK(g_xuiProxyXgeDrawCache.tRun.pGlyphs==glyphs);
    __xuiProxyXgeDrawCacheClear(NULL); diagFailCopy=1;
    compare(f,text,8192,12,0xff102030u,0,NULL,0);CHECK(diagFailCopy==0 && g_xuiProxyXgeDrawCache.pFont==NULL);
    compare(f,text,8192,12,0xff102030u,0,NULL,0);verifyRun(f,text,8192);
    text[1]='Z'; diagFailShape=1;
    CHECK(paint(0,f,text,8192,12,0xff102030u,0,NULL,0,newPixels)==XGE_ERROR_OUT_OF_MEMORY);
    CHECK(g_xuiProxyXgeDrawCache.pFont==NULL && diagFailShape==0);
    compare(f,text,8192,12,0xff102030u,0,NULL,0);verifyRun(f,text,8192);
    diagFailShape=1; CHECK(paint(0,f,text,XUI_PROXY_XGE_DRAW_CACHE_MAX+1,12,0xff102030u,0,NULL,0,newPixels)==XGE_ERROR_OUT_OF_MEMORY);
    CHECK(g_xuiProxyXgeDrawCache.pFont==NULL); /* Oversize preallocation eviction, injected shape failure. */
    xgeEmojiPackClearDefault();memset(text,'A',8192);
    compare(f,text,8192,12,0xff102030u,0,NULL,0);CHECK(g_xgeDefaultEmojiPack==NULL && g_xuiProxyXgeDrawCache.pFont==NULL);
    memcpy(text,"😀",4);
    /* First new draw initializes the built-in default but has no pre-shape key. */
    CHECK(paint(0,f,text,8192,12,0xff102030u,0,NULL,0,newPixels)==XGE_OK);
    CHECK(g_xgeDefaultEmojiPack!=NULL && g_xuiProxyXgeDrawCache.pFont==NULL);
    compare(f,text,8192,12,0xff102030u,0,NULL,0);
    verifyRun(f,text,8192);
    xge_emoji_pack custom=NULL;CHECK(xgeEmojiPackCreate(&custom)==XGE_OK);CHECK(xgeEmojiPackSetDefault(custom)==XGE_OK);
    const char* svg="<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'><circle cx='16' cy='16' r='14' fill='#ff8800'/></svg>";
    xge_emoji_metrics_t metrics={0};metrics.iSize=sizeof(metrics);metrics.fAdvanceEm=metrics.fWidthEm=metrics.fHeightEm=1;metrics.fBaselineRatio=.82f;
    uint32_t id=0;CHECK(xgeEmojiPackAddSvgMemory(custom,"😀",svg,(int)strlen(svg),&metrics,&id)==XGE_OK);
    compare(f,text,8192,12,0xff102030u,0,NULL,0);CHECK(g_xuiProxyXgeDrawCache.pFont==NULL);
    metrics.fAdvanceEm=2;CHECK(xgeEmojiPackAddSvgMemory(custom,"😀",svg,(int)strlen(svg),&metrics,&id)==XGE_OK);
    compare(f,text,8192,12,0xff102030u,0,NULL,0);CHECK(g_xuiProxyXgeDrawCache.pFont==NULL);
    CHECK(xgeEmojiPackAddSvgMemory(builtin,"😀",svg,(int)strlen(svg),&metrics,&id)==XGE_ERROR_INVALID_ARGUMENT);
    CHECK(xgeEmojiPackSetDefault(builtin)==XGE_OK);compare(f,text,8192,12,0xff102030u,0,NULL,0);verifyRun(f,text,8192);
    CHECK(diagOldDrawTextSpans(NULL,NULL,NULL,NULL,-1,(xui_rect_t){0},0,0,NULL,0)==__xuiProxyXgeDrawTextSpans(NULL,NULL,NULL,NULL,-1,(xui_rect_t){0},0,0,NULL,0));
    __xuiProxyXgeFontDestroy(&proxy,f);CHECK(g_xuiProxyXgeDrawCache.pFont==NULL && g_xge.iFontCount==originalFonts);
    CHECK(builtin->iRefCount==builtinRefs);xgeEmojiPackClearDefault();xgeEmojiPackFree(custom);xgeEmojiPackFree(builtin);
    xgeRenderTargetFree(&surface.tTarget);free(text);CHECK(xgeEnd()==XGE_OK);
    printf("draw cache comparison passed: %llu assertions; %llu successful pixel cases; %llu matching error cases; %llu unchanged long run reuse cases; %llu cache-path shape calls; one moved run; failure fallback and font cleanup\n",assertions,pixelCases,errorCases,reuseCases,diagShapeCalls);fflush(stdout);xgeQuit();return XGE_OK;
}
int main(int argc,char** argv) {
    if(argc!=4)return 2;
    xge_desc_t d={0};d.iWidth=512;d.iHeight=192;d.sTitle="Private draw cache comparison";d.iFlags=XGE_INIT_WINDOW;d.iRunMode=XGE_RUN_GAME_LOOP;
    int code=xgeInit(&d);if(code==XGE_OK)code=xgeRun(verify,argv);xgeUnit();return code==XGE_OK && started ? 0 : 1;
}
