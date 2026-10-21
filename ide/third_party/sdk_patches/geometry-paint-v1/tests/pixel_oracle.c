#include "xge.c"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Private functions are deliberately absent from the public consumer headers. */
int xgeInternalTextShapeVisit(const xge_text_shape_desc_t*,xge_glyph_run_t*,int (*)(const xge_glyph_position_t*,void*),void*);
int xgeInternalTextPaintStream(const xge_text_shape_desc_t*,float,float,uint32_t,const xge_text_paint_span_t*,int,void**);
int xgeInternalTextPaintViewport(const xge_text_shape_desc_t*,float,float,uint32_t,const xge_text_paint_span_t*,int,void**,void**);
int xgeInternalTextPaintViewportValid(void*,const xge_text_shape_desc_t*);
void xgeInternalTextPaintViewportFree(void*);
void __xgeGlyphRunPaintCacheFree(void*);
void __xgeGlyphRunPaintCacheDraw(void*,uint32_t,const xge_text_paint_span_t*,int);

#define CHECK(v) do {++checks;if(!(v)){fprintf(stderr,"viewport paint:%d: %s\n",__LINE__,#v);fflush(stderr);exit(1);}} while(0)
static unsigned int checks,pairs,replays,retained,started;
static unsigned char pixels[2][512*128*4];
static char** arguments;

static int ignoreGlyph(const xge_glyph_position_t* glyph,void* user)
{ (void)glyph;(void)user;return XGE_OK; }

static void samePixels(void)
{
    if(memcmp(pixels[0],pixels[1],sizeof(pixels[0]))) {
        unsigned int differences=0;
        for(size_t i=0;i<sizeof(pixels[0]);++i)if(pixels[0][i]!=pixels[1][i])++differences;
        fprintf(stderr,"pixel pair %u differs in %u channels\n",pairs,differences);
        for(int mode=0;mode<2;++mode) {
            FILE* file=fopen(mode?"mismatch-new.rgba":"mismatch-old.rgba","wb");
            if(file){fwrite(pixels[mode],1,sizeof(pixels[mode]),file);fclose(file);}
        }
    }
    CHECK(!memcmp(pixels[0],pixels[1],sizeof(pixels[0])));++pairs;
}

static void compareCarry(xge_render_target target,const xge_text_shape_desc_t* desc,const float* origins,int count,int expectedIndex,void** carry)
{
    xge_text_paint_span_t spans[]={
        {sizeof(xge_text_paint_span_t),-3,4,0xff0088ffu},
        {sizeof(xge_text_paint_span_t),2,17,0xff990077u},
        {sizeof(xge_text_paint_span_t),20,20000000,0xff994400u},
        {sizeof(xge_text_paint_span_t),3,7,0xff33aa55u}};
    void* index=carry?*carry:NULL;
    int refs=desc->pFont->iRefCount;
    int fallback=desc->pFont->pFallback?desc->pFont->pFallback->iRefCount:0;
    int emoji=desc->pEmojiPack?desc->pEmojiPack->iRefCount:0;
    if(!index && desc->iTextSize>=65536) {
        xge_glyph_run_t measured={0};xge_internal_text_cursor* geometry=NULL;
        CHECK(xgeInternalTextShapeVisitPaintCached(desc,&measured,ignoreGlyph,NULL,NULL,0,&geometry,NULL)==XGE_OK);
        CHECK(geometry && xgeInternalTextPaintViewportSeed(desc,geometry,&index)==XGE_OK);
        xgeGlyphRunFree(&measured);xgeInternalTextCursorFree(geometry);
    }
    for(int at=0;at<count;++at) {
        xge_rect_i_t clip={8+(at%3),8,496-(at%3),112};
        void* paint[2]={NULL,NULL};int code[2]={0,0};void* previous=index;
        printf("{\"event\":\"pair\",\"pair\":%u,\"bytes\":%d,\"origin\":%.9g,\"step\":%d}\n",pairs,desc->iTextSize,(double)origins[at],at);fflush(stdout);
        for(int mode=0;mode<2;++mode) {
            xge_pass_t pass={0};xgePassInit(&pass,target,XGE_PASS_CLEAR_COLOR,0xffefefefu);
            CHECK(xgePassBegin(&pass)==XGE_OK);xgeClipSetPixels(clip);
            code[mode]=mode?xgeInternalTextPaintViewport(desc,origins[at],9.5f,0xff102030u,spans,4,paint+mode,&index):
                xgeInternalTextPaintStream(desc,origins[at],9.5f,0xff102030u,spans,4,paint+mode);
            CHECK(xgePassEnd(&pass)==XGE_OK);xgeClipClear();
            CHECK(xgeRenderTargetReadPixels(target,pixels[mode],512*4)==XGE_OK);
        }
        CHECK(code[0]==code[1]);samePixels();
        if(expectedIndex) {
            CHECK(index && xgeInternalTextPaintViewportValid(index,desc));
            if(previous && (expectedIndex==1 || at>0)){CHECK(index==previous);++retained;}
        }
        if(paint[0] && paint[1]) {
            for(int mode=0;mode<2;++mode) {
                xge_pass_t pass={0};xgePassInit(&pass,target,XGE_PASS_CLEAR_COLOR,0xffefefefu);
                CHECK(xgePassBegin(&pass)==XGE_OK);xgeClipSetPixels(clip);
                __xgeGlyphRunPaintCacheDraw(paint[mode],0xff779911u,spans+1,3);
                CHECK(xgePassEnd(&pass)==XGE_OK);xgeClipClear();
                CHECK(xgeRenderTargetReadPixels(target,pixels[mode],512*4)==XGE_OK);
            }
            samePixels();++replays;
        }
        __xgeGlyphRunPaintCacheFree(paint[0]);__xgeGlyphRunPaintCacheFree(paint[1]);
    }
    if(carry)*carry=index;
    else {
        xgeInternalTextPaintViewportFree(index);
        CHECK(desc->pFont->iRefCount==refs);
        CHECK(!desc->pFont->pFallback || desc->pFont->pFallback->iRefCount==fallback);
        CHECK(!desc->pEmojiPack || desc->pEmojiPack->iRefCount==emoji);
    }
}

static void compare(xge_render_target target,const xge_text_shape_desc_t* desc,const float* origins,int count,int expectedIndex)
{ compareCarry(target,desc,origins,count,expectedIndex,NULL); }

static void transitions(xge_render_target target,char* text,xge_emoji_pack normal,xge_emoji_pack tiny,const char* svg)
{
    xge_font_t font={0},firstFallback={0},secondFallback={0};
    CHECK(xgeFontLoad(&font,arguments[1],16.25f)==XGE_OK);
    CHECK(xgeFontLoad(&firstFallback,arguments[2],28.5f)==XGE_OK);
    CHECK(xgeFontLoad(&secondFallback,arguments[2],39.75f)==XGE_OK);
    font.pFallback=&firstFallback;
    int fontRefs=font.iRefCount,firstRefs=firstFallback.iRefCount,secondRefs=secondFallback.iRefCount;
    CHECK(xgeEmojiPackSetDefault(normal)==XGE_OK);int packRefs=normal->iRefCount;
    memset(text,'V',65536);memcpy(text+65528,"中😀A",8);text[65536]=0;
    xge_text_shape_desc_t desc={0};desc.iSize=sizeof(desc);desc.pFont=&font;desc.sText=text;desc.iTextSize=65536;
    desc.iFlags=XGE_TEXT_SHAPE_DEFAULT;desc.iEmojiLinePolicy=XGE_EMOJI_LINE_EXPAND;desc.fEmojiScale=1.25f;
    void* index=NULL;
    for(int change=0;change<9;++change) {
        if(change==1) {text[4094]=(char)0xc3;text[4095]=(char)0xa9;}
        if(change==2) {desc.iFlags^=XGE_TEXT_SHAPE_KERNING;CHECK(!xgeInternalTextPaintViewportValid(index,&desc));}
        if(change==3) {font.fScale*=1.05f;CHECK(!xgeInternalTextPaintViewportValid(index,&desc));}
        if(change==4) {font.pFallback=&secondFallback;CHECK(!xgeInternalTextPaintViewportValid(index,&desc));}
        if(change==5) {CHECK(xgeEmojiPackSetDefault(tiny)==XGE_OK);CHECK(!xgeInternalTextPaintViewportValid(index,&desc));}
        if(change==6) {desc.pEmojiPack=normal;CHECK(!xgeInternalTextPaintViewportValid(index,&desc));memcpy(text+65528,"中🧪A",8);}
        if(change==7) {
            xge_emoji_metrics_t metrics={0};metrics.iSize=sizeof(metrics);metrics.fAdvanceEm=metrics.fWidthEm=2;
            metrics.fHeightEm=3;metrics.fBaselineRatio=.9f;uint32_t id=0;
            CHECK(xgeEmojiPackAddSvgMemory(normal,"🧪",svg,(int)strlen(svg),&metrics,&id)==XGE_OK);
            CHECK(!xgeInternalTextPaintViewportValid(index,&desc));
        }
        if(change==8) {memcpy(text+65536,"AV",2);text[65538]=0;desc.iTextSize=65538;CHECK(!xgeInternalTextPaintViewportValid(index,&desc));}
        xge_glyph_run_t shape={0};CHECK(xgeInternalTextShapeVisit(&desc,&shape,ignoreGlyph,NULL)==XGE_OK);
        float origins[]={12.25f,-9999.25f,480.0f-shape.fWidth};xgeGlyphRunFree(&shape);
        compareCarry(target,&desc,origins,3,2,&index);
    }
    /* Equal bytes at a different address retain the cache; pointer identity does not. */
    char* copy=malloc((size_t)desc.iTextSize+1u);CHECK(copy);memcpy(copy,text,(size_t)desc.iTextSize+1u);desc.sText=copy;
    float origins[]={12.25f,-9999.25f};compareCarry(target,&desc,origins,2,1,&index);
    free(copy);xgeInternalTextPaintViewportFree(index);
    CHECK(font.iRefCount==fontRefs && firstFallback.iRefCount==firstRefs && secondFallback.iRefCount==secondRefs);
    CHECK(xgeEmojiPackSetDefault(normal)==XGE_OK && normal->iRefCount==packRefs);
    font.pFallback=NULL;xgeFontFree(&font);xgeFontFree(&firstFallback);xgeFontFree(&secondFallback);
}

static int run(void* unused)
{
    (void)unused;if(started)return XGE_OK;started=1;
    CHECK(xgeBegin()==XGE_OK);
    xge_render_target_t target={0};CHECK(xgeRenderTargetCreate(&target,512,128)==XGE_OK);
    xge_emoji_pack saved=NULL,normal=NULL,bad=NULL,tiny=NULL;
    CHECK(xgeEmojiPackGetDefault(&saved)==XGE_OK);
    CHECK(xgeEmojiPackCreate(&normal)==XGE_OK && xgeEmojiPackCreate(&bad)==XGE_OK && xgeEmojiPackCreate(&tiny)==XGE_OK);
    xge_emoji_metrics_t metrics={0};metrics.iSize=sizeof(metrics);metrics.fAdvanceEm=metrics.fWidthEm=1;metrics.fHeightEm=2;metrics.fBaselineRatio=.9f;
    const char* svg="<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32' transform='translate(-16 0)'><rect width='48' height='32' fill='#ff8800'/></svg>";
    uint32_t id=0;
    CHECK(xgeEmojiPackAddSvgMemory(normal,"😀",svg,(int)strlen(svg),&metrics,&id)==XGE_OK);
    CHECK(xgeEmojiPackAddSvgMemory(normal,"A",svg,(int)strlen(svg),&metrics,&id)==XGE_OK);
    metrics.fAdvanceEm=.000001f;
    CHECK(xgeEmojiPackAddSvgMemory(tiny,"😀",svg,(int)strlen(svg),&metrics,&id)==XGE_OK);
    const char* broken="this is not SVG";
    CHECK(xgeEmojiPackAddSvgMemory(bad,"😀",broken,(int)strlen(broken),&metrics,&id)==XGE_OK);
    CHECK(xgeEmojiPackSetDefault(normal)==XGE_OK);
    const char* motif="AVATAR office 中文😀 é 0123456789 ";int unit=(int)strlen(motif);
    char* text=malloc(16777217);CHECK(text);
    for(int size=0;size<3;++size) {
        float sizes[]={8.5f,16,39.75f};xge_font_t font={0},fallback={0};
        CHECK(xgeFontLoad(&font,arguments[1],sizes[size])==XGE_OK);
        CHECK(xgeFontLoad(&fallback,arguments[2],sizes[size]*1.8f)==XGE_OK);font.pFallback=&fallback;
        int length=size==2?2097152:65536;
        int used=0;while(used+unit+7<=length){memcpy(text+used,motif,(size_t)unit);used+=unit;}
        memset(text+used,'V',(size_t)(length-used-7));memcpy(text+length-7,"中😀",7);text[length]=0;
        xge_text_shape_desc_t desc={0};desc.iSize=sizeof(desc);desc.pFont=&font;desc.sText=text;desc.iTextSize=length;
        desc.iFlags=XGE_TEXT_SHAPE_DEFAULT;desc.iEmojiLinePolicy=XGE_EMOJI_LINE_EXPAND;desc.fEmojiScale=1.25f;
        xge_glyph_run_t shape={0};CHECK(xgeInternalTextShapeVisit(&desc,&shape,ignoreGlyph,NULL)==XGE_OK);
        float origins[]={12.25f,-25.75f,-9999.25f,480.0f-shape.fWidth,-10400.5f,12.25f};xgeGlyphRunFree(&shape);
        compare(&target,&desc,origins,6,1);
        desc.pEmojiPack=normal;compare(&target,&desc,origins,6,1);
        /* UTF-8 errors and multiline/embedded-NUL fallback preserve old results. */
        const char* controls[]={"AV\r\nTA\nR\tB", "A\xff", "A\xf0\x9f", "A\0V\nB"};
        int lengths[]={11,2,3,5};
        for(int control=0;control<4;++control){desc.sText=controls[control];desc.iTextSize=lengths[control];compare(&target,&desc,origins,2,0);}
        desc.sText="A😀";desc.iTextSize=5;desc.pEmojiPack=bad;compare(&target,&desc,origins,2,0);
        for(int at=0;at<5000;++at)memcpy(text+at*4,"😀",4);
        desc.sText=text;desc.iTextSize=20000;desc.pEmojiPack=tiny;compare(&target,&desc,origins,3,0);
        font.pFallback=NULL;xgeFontFree(&font);xgeFontFree(&fallback);
    }
    /* A 16 MiB noninteger-advance tail catches precision lost by replay seeds. */
    xge_font_t kern={0};CHECK(xgeFontLoad(&kern,arguments[3],16.25f)==XGE_OK);
    memset(text,'V',16777216);text[16777216]=0;
    xge_text_shape_desc_t desc={0};desc.iSize=sizeof(desc);desc.pFont=&kern;desc.sText=text;desc.iTextSize=16777216;
    desc.iFlags=XGE_TEXT_SHAPE_KERNING;desc.fEmojiScale=1;
    xge_glyph_run_t shape={0};CHECK(xgeInternalTextShapeVisit(&desc,&shape,ignoreGlyph,NULL)==XGE_OK);
    float origins[]={12.25f,480.0f-shape.fWidth,-1234567.25f,12.25f};xgeGlyphRunFree(&shape);
    compare(&target,&desc,origins,4,1);xgeFontFree(&kern);
    transitions(&target,text,normal,tiny,svg);free(text);
    if(saved)CHECK(xgeEmojiPackSetDefault(saved)==XGE_OK);else xgeEmojiPackClearDefault();
    xgeEmojiPackFree(saved);xgeEmojiPackFree(normal);xgeEmojiPackFree(bad);xgeEmojiPackFree(tiny);xgeRenderTargetFree(&target);
    {
        void* paint=NULL;void* index=NULL;xge_text_shape_desc_t invalid={0};
        invalid.iSize=sizeof(invalid);invalid.iTextSize=4;
        CHECK(xgeInternalTextPaintViewport(&invalid,0,0,0,NULL,0,&paint,&index)==XGE_ERROR_INVALID_ARGUMENT);
        CHECK(!paint && !index && !xgeInternalTextPaintViewportValid(NULL,NULL));
    }
    CHECK(pairs>=100 && retained>=30 && replays>=30);
    printf("{\"event\":\"completed\",\"checks\":%u,\"pixelPairs\":%u,\"paintReplays\":%u,\"retainedViewportRepositions\":%u}\n",checks,pairs,replays,retained);fflush(stdout);
    CHECK(xgeEnd()==XGE_OK);xgeQuit();return XGE_OK;
}

int main(int count,char** values)
{
    if(count!=4)return 2;
    arguments=values;
    xge_desc_t desc={0};desc.iWidth=600;desc.iHeight=200;desc.sTitle="Private viewport paint pixel oracle";
    desc.iFlags=XGE_INIT_WINDOW;desc.iRunMode=XGE_RUN_GAME_LOOP;
    int code=xgeInit(&desc);if(code==XGE_OK)code=xgeRun(run,NULL);xgeUnit();return code==XGE_OK && started?0:1;
}
