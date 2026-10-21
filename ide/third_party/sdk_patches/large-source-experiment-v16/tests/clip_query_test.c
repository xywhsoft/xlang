/* Compiled after the real engine and independently extracted SDK29 renderer. */
static volatile unsigned long long diagClipConversions;
static unsigned long long clipChecks,clipStates,clipPixelCases;
static int diagClipMutationMode,diagClipDrawCalls,diagClipEmojiCalls;
#define CLIP_CHECK(e) do {++clipChecks;if(!(e)){fprintf(stderr,"clip state %d: %s\n",__LINE__,#e);exit(1);}} while(0)
static xge_rect_i_t diagClipPixels(void) {
    ++diagClipConversions;return xgeClipGetPixels();
}
static void diagClipMutation(int n) {
    if(!diagClipMutationMode)return;
    switch(n%6) {
    case 1:xgeClipSet((xge_rect_t){40.49f,8.5f,240.51f,112.0f});break;
    case 2:xgeClipClear();break;
    case 3:xgeClipSet((xge_rect_t){8.25f,8.25f,300.75f,111.75f});break;
    case 4:xgeClipSet((xge_rect_t){20.5f,10.5f,496.0f,110.0f});break;
    case 5:xgeClipSetPixels((xge_rect_i_t){8,8,496,112});break;
    default:xgeClipSet((xge_rect_t){0,0,0,0});break;
    }
}
static void diagClipDrawEx(const xge_draw_t* pDraw) {
    xgeDrawEx(pDraw);
    diagClipMutation(++diagClipDrawCalls);
}
static int diagClipEmojiDraw(xge_emoji_pack pack,uint32_t id,xge_rect_t dst,int screen,int* mayPaint) {
    int ret=__xgeEmojiDraw(pack,id,dst,screen,mayPaint);
    ++diagClipEmojiCalls;
    if(diagClipMutationMode==2) {
        /* An error path that changes live clipping before fallback. */
        diagClipMutation(diagClipEmojiCalls);
        return XGE_ERROR_RESOURCE_FAILED;
    }
    if(diagClipMutationMode==3)diagClipMutation(diagClipEmojiCalls);
    return ret;
}
static void clipCompareQuery(xge_glyph_draw_clip_t* state) {
    xge_rect_i_t a=__xgeGlyphDrawClipPixels(state),b=xgeClipGetPixels();
    CLIP_CHECK(!memcmp(&a,&b,sizeof(a)));++clipStates;
}
static void clipStateTests(void) {
    xge_rect_t saved=g_xge.tClipRect;int enabled=g_xge.bClipEnabled;
    xge_glyph_draw_clip_t state={0};
    unsigned long long before=diagClipConversions;
    g_xge.bClipEnabled=1;g_xge.tClipRect=(xge_rect_t){12.5f,-9.5f,350.75f,92.25f};
    for(int i=0;i<1000000;++i)clipCompareQuery(&state);
    CLIP_CHECK(diagClipConversions==before+1);
    const uint32_t bits[]={0,0x80000000u,0x3effffffu,0x3f000000u,0x3f000001u,
        0x3f800000u,0xbf800000u,0x4effffffu,0x4f000000u,0xcf000000u,0x7f7fffffu,
        0xff7fffffu,0x7f800000u,0xff800000u,0x7fc00000u,0x7fc00001u,0x7fa00001u,1,0x80000001u};
    for(size_t a=0;a<sizeof(bits)/sizeof(*bits);++a)
        for(size_t b=0;b<sizeof(bits)/sizeof(*bits);++b)
            for(int field=0;field<4;++field)for(int flag=0;flag<4;++flag) {
                uint32_t rect[]={bits[a],bits[b],bits[b],bits[a]};
                rect[field]=bits[(a+b)%19];memcpy(&g_xge.tClipRect,rect,sizeof(rect));
                g_xge.bClipEnabled=flag ? flag-2 : 0;
                before=diagClipConversions;clipCompareQuery(&state);
                CLIP_CHECK(diagClipConversions<=before+1);
                before=diagClipConversions;clipCompareQuery(&state);CLIP_CHECK(diagClipConversions==before);
            }
    uint32_t random=0x68a4bd39u;
    for(int i=0;i<262144;++i) {
        uint32_t rect[4];
        for(int f=0;f<4;++f){random^=random<<13;random^=random>>17;random^=random<<5;rect[f]=random;}
        memcpy(&g_xge.tClipRect,rect,sizeof(rect));g_xge.bClipEnabled=(i%5)-2;
        clipCompareQuery(&state);before=diagClipConversions;clipCompareQuery(&state);CLIP_CHECK(diagClipConversions==before);
    }
    /* Actual APIs normalize nonfinite/negative extents; compare after each setter. */
    for(int i=0;i<1024;++i) {
        xgeClipSet((xge_rect_t){i*.125f,-i*.25f,240.5f-i*.5f,92.5f});clipCompareQuery(&state);
        xgeClipClear();clipCompareQuery(&state);
        xgeClipSetPixels((xge_rect_i_t){i-512,17,512-i,124});clipCompareQuery(&state);
    }
    g_xge.tClipRect=saved;g_xge.bClipEnabled=enabled;
    printf("clip queries passed: %llu assertions; %llu exact state queries; %llu conversions; unchanged million queries use one conversion\n",clipChecks,clipStates,(unsigned long long)diagClipConversions);fflush(stdout);
}
static void clipRenderTests(xui_font f) {
    const char* texts[]={"ABCDEFGHIJK 中文 é 😀 AVATAR", "😀ABC😀DEF😀中文😀XYZ", "AVATAR\n中文 😀 é\n123456789"};
    for(int t=0;t<3;++t)for(int mode=0;mode<4;++mode)for(int world=0;world<2;++world) {
        xge_text_shape_desc_t d={0};xge_glyph_run_t run={0};
        d.iSize=sizeof(d);d.pFont=&f->tFont;d.sText=texts[t];d.iTextSize=(int)strlen(texts[t]);
        d.iFlags=XGE_TEXT_SHAPE_DEFAULT;d.fEmojiScale=1;
        CLIP_CHECK(xgeTextShape(&d,&run)==XGE_OK);
        unsigned long long conversions[2]={0};int draws[2]={0},emojis[2]={0};
        for(int old=0;old<2;++old) {
            xge_pass_t pass={0};xgePassInit(&pass,&surface.tTarget,XGE_PASS_CLEAR_COLOR,0xffefefefu);
            CLIP_CHECK(xgePassBegin(&pass)==XGE_OK);xgeClipSetPixels((xge_rect_i_t){8,8,496,112});
            diagClipDrawCalls=diagClipEmojiCalls=0;diagClipMutationMode=mode;
            unsigned long long before=diagClipConversions;
            if(old)diagPreviousDrawSpans(&run,12.25f,12.25f,0xff102030u,world ? 0 : XGE_DRAW_SCREEN_SPACE,NULL,0,NULL);
            else xgeGlyphRunDrawSpans(&run,12.25f,12.25f,0xff102030u,world ? 0 : XGE_DRAW_SCREEN_SPACE,NULL,0);
            conversions[old]=diagClipConversions-before;draws[old]=diagClipDrawCalls;emojis[old]=diagClipEmojiCalls;
            diagClipMutationMode=0;
            CLIP_CHECK(xgePassEnd(&pass)==XGE_OK);xgeClipClear();
            CLIP_CHECK(xgeRenderTargetReadPixels(&surface.tTarget,old ? oldPixels : newPixels,512*4)==XGE_OK);
        }
        CLIP_CHECK(draws[0]==draws[1] && emojis[0]==emojis[1]);
        CLIP_CHECK(!memcmp(oldPixels,newPixels,sizeof(oldPixels)));++clipPixelCases;
        if(world)CLIP_CHECK(conversions[0]==0);
        else if(mode==0)CLIP_CHECK(conversions[0]<=1);
        xgeGlyphRunFree(&run);
    }
    printf("clip drawing passed: %llu matching pixel cases; live draw and SVG success or failure clip mutation\n",clipPixelCases);fflush(stdout);
}
