#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
static unsigned long long streamAssertions,streamPositionCases,streamPositions,streamErrorCases;
static unsigned long long layoutCases,layoutAssertions,proxyCases;
#define STREAM_CHECK(e) do { ++streamAssertions; if(!(e)){fprintf(stderr,"stream check %d: %s\n",__LINE__,#e);exit(1);} }while(0)
#define LAYOUT_CHECK(e) do { ++layoutAssertions; if(!(e)){fprintf(stderr,"layout check %d: %s\n",__LINE__,#e);exit(1);} }while(0)
typedef struct stream_reference_t {
    const xge_glyph_run_t* pReference;
    int iAt;
    int iFailAt;
} stream_reference_t;
static int comparePosition(const xge_glyph_position_t* position,void* user) {
    stream_reference_t* state=user;
    STREAM_CHECK(state->iAt<state->pReference->iGlyphCount);
    STREAM_CHECK(!memcmp(position,&state->pReference->pGlyphs[state->iAt],sizeof(*position)));
    if(state->iAt==state->iFailAt)return XGE_ERROR_INVALID_STATE;
    ++state->iAt;++streamPositions;return XGE_OK;
}
static void compareStream(const xge_text_shape_desc_t* desc,const xge_glyph_run_t* old,int oldCode) {
    xge_glyph_run_t result={0};stream_reference_t state={old,0,-1};
    int code=xgeInternalTextShapeVisit(desc,&result,comparePosition,&state);STREAM_CHECK(code==oldCode);++streamPositionCases;
    if(code==XGE_OK) {
        STREAM_CHECK(result.pGlyphs==NULL);
        STREAM_CHECK(result.iSize==old->iSize && result.iFlags==old->iFlags && result.iTextSize==old->iTextSize && result.iGlyphCount==old->iGlyphCount);
        STREAM_CHECK(result.fWidth==old->fWidth && result.fHeight==old->fHeight && result.fAscent==old->fAscent && result.fDescent==old->fDescent && result.fLineHeight==old->fLineHeight);
        STREAM_CHECK(state.iAt==old->iGlyphCount);
        const xge_glyph_run_backend_t* a=old->pBackend;const xge_glyph_run_backend_t* b=result.pBackend;
        STREAM_CHECK((a==NULL)==(b==NULL));
        if(a){STREAM_CHECK(a->iFontCount==b->iFontCount && a->pEmojiPack==b->pEmojiPack);for(int i=0;i<a->iFontCount;++i)STREAM_CHECK(a->ppFonts[i]==b->ppFonts[i]);}
    } else STREAM_CHECK(state.iAt==0 && result.pGlyphs==NULL && result.pBackend==NULL);
    xgeGlyphRunFree(&result);
    if(code==XGE_OK && old->iGlyphCount>0 && old->iGlyphCount<=64) {
        int points[]={0,old->iGlyphCount/2,old->iGlyphCount-1};
        for(int i=0;i<3;++i) {
            state.iAt=0;state.iFailAt=points[i];
            STREAM_CHECK(xgeInternalTextShapeVisit(desc,&result,comparePosition,&state)==XGE_ERROR_INVALID_STATE);
            xge_glyph_run_t empty={0};STREAM_CHECK(!memcmp(&result,&empty,sizeof(result)));++streamErrorCases;
            xgeGlyphRunFree(&result);
        }
    }
}
typedef struct cluster_reference_t {
    const xui_text_shape_t* reference;
    int at;
    int failAt;
} cluster_reference_t;
static int compareCluster(const xui_text_cluster_t* cluster,void* user) {
    cluster_reference_t* state=user;
    LAYOUT_CHECK(state->at<state->reference->iClusterCount);
    LAYOUT_CHECK(!memcmp(cluster,&state->reference->pClusters[state->at],sizeof(*cluster)));
    if(state->at==state->failAt)return XUI_ERROR_OUT_OF_MEMORY;
    ++state->at;return XUI_OK;
}
static int customCalls;
static void verifyDpiLayoutGeometry(char** args);
static int customShape(xui_proxy proxy,xui_font font,const char* text,int n,uint32_t flags,xui_text_shape_t* shape) {
    ++customCalls;return __xuiProxyXgeTextShape(proxy,font,text,n,flags,shape);
}
static void proxyCompare(xui_proxy proxy,xui_font font,const char* text,int n,uint32_t flags) {
    xui_text_shape_t old={0},now={0};int references=font->tFont.iRefCount;
    int code=proxy->textShape(proxy,font,text,n,flags,&old);
    cluster_reference_t state={&old,0,-1};
    LAYOUT_CHECK(xuiInternalProxyXgeVisitTextShape(proxy,font,text,n,flags,compareCluster,&state,&now)==code);++proxyCases;
    if(code==XUI_OK) {
        LAYOUT_CHECK(state.at==old.iClusterCount && now.pClusters==NULL);
        xui_text_cluster_t* stored=old.pClusters;old.pClusters=NULL;
        LAYOUT_CHECK(!memcmp(&old,&now,sizeof(old)));old.pClusters=stored;
        if(old.iClusterCount>0 && old.iClusterCount<=128) {
            for(int fail=0;fail<3;++fail) {
                state.at=0;state.failAt=fail==0 ? 0 : (fail==1 ? old.iClusterCount/2 : old.iClusterCount-1);
                xuiTextShapeFree(&now);
                LAYOUT_CHECK(xuiInternalProxyXgeVisitTextShape(proxy,font,text,n,flags,compareCluster,&state,&now)==XUI_ERROR_OUT_OF_MEMORY);
                LAYOUT_CHECK(now.pClusters==NULL);LAYOUT_CHECK(font->tFont.iRefCount==references);
            }
        }
    } else LAYOUT_CHECK(state.at==0 && now.pClusters==NULL);
    xuiTextShapeFree(&old);xuiTextShapeFree(&now);LAYOUT_CHECK(font->tFont.iRefCount==references);
}
static void geometryCompare(xui_proxy proxy,xui_font font,xui_code_edit_data_t* a,xui_code_edit_data_t* b,const char* text,int n,float column) {
    xui_code_edit_line_layout_t* old=diagPreviousLineLayoutEnsure(proxy,font,a,text,n,column);
    xui_code_edit_line_layout_t* now=__xuiCodeEditLineLayoutEnsure(proxy,font,b,text,n,column);
    LAYOUT_CHECK(old && now);++layoutCases;
    LAYOUT_CHECK(old->iStopCount==now->iStopCount && old->iSegmentCount==now->iSegmentCount && old->fWidth==now->fWidth);
    { int equal=1;for(int i=0;i<old->iStopCount;++i)if(old->pStops[i].iOffset!=now->pStops[i].iOffset || old->pStops[i].fX!=now->pStops[i].fX){equal=0;break;}LAYOUT_CHECK(equal); }
    LAYOUT_CHECK(!memcmp(old->sText,now->sText,(size_t)n+1));
    for(int i=0;i<old->iSegmentCount;++i) {
        xui_code_edit_text_segment_t* x=&old->pSegments[i];xui_code_edit_text_segment_t* y=&now->pSegments[i];
        LAYOUT_CHECK(x->iStart==y->iStart && x->iEnd==y->iEnd && x->fX==y->fX);
        xui_text_cluster_t* xc=x->tShape.pClusters;xui_text_cluster_t* yc=y->tShape.pClusters;
        x->tShape.pClusters=y->tShape.pClusters=NULL;LAYOUT_CHECK(!memcmp(&x->tShape,&y->tShape,sizeof(x->tShape)));x->tShape.pClusters=xc;y->tShape.pClusters=yc;
        if(proxy->textShape==__xuiProxyXgeTextShape)LAYOUT_CHECK(yc==NULL);
    }
    int step=n>4096 ? n/32 : 1;
    for(int at=-2;at<n+3;at+=step)LAYOUT_CHECK(__xuiCodeEditLineLayoutOffsetX(old,at)==__xuiCodeEditLineLayoutOffsetX(now,at));
    for(int i=0;i<65;++i){float x=old->fWidth*(float)i/64;LAYOUT_CHECK(__xuiCodeEditLineLayoutOffsetFromX(old,x)==__xuiCodeEditLineLayoutOffsetFromX(now,x));}
    const float wraps[]={1.0f,8.25f,64.5f,256.0f,8192.0f};
    for(int i=0;i<5;++i) {
        if(n>4096 && i<3)continue;
        LAYOUT_CHECK(__xuiCodeEditLineLayoutBuildWraps(old,wraps[i])==__xuiCodeEditLineLayoutBuildWraps(now,wraps[i]));
        LAYOUT_CHECK(old->iWrapOffsetCount==now->iWrapOffsetCount);
        LAYOUT_CHECK(!memcmp(old->pWrapOffsets,now->pWrapOffsets,sizeof(*old->pWrapOffsets)*(size_t)old->iWrapOffsetCount));
    }
    LAYOUT_CHECK(__xuiCodeEditLineLayoutEnsure(proxy,font,b,text,n,column)==now);
}
static void verifyLayoutGeometry(char** args) {
    xui_proxy_t proxy=xuiProxyXge();xui_code_document document=NULL;
    LAYOUT_CHECK(xuiCodeDocumentCreate(&document)==XUI_OK);
    xui_code_edit_data_t a={0},b={0};a.pDocument=b.pDocument=document;
    const char* corpus[]={"","A","AVATAR Wa. ii 0123","\tA\t中文\t終","中文😀 é 👩🏽‍💻 🇨🇳🇫🇷","á́́‍b","각각 क्ष क्‍ष","مرحبا אבג","A\rB\nC\r\nD","\t\t\t","\xe2\x80\x8d","\xe2\x80\x8b","🏳️‍🌈👨‍👩‍👧‍👦","終\t"};
    const float sizes[]={8.0f,16.0f,20.0f,32.0f};const int tabs[]={1,4,8};
    for(int size=0;size<4;++size) {
        xui_font latin=NULL,cjk=NULL;LAYOUT_CHECK(proxy.fontLoadFile(&proxy,&latin,args[2],sizes[size],0)==XUI_OK);
        LAYOUT_CHECK(proxy.fontLoadFile(&proxy,&cjk,args[3],sizes[size],0)==XUI_OK);
        for(int chain=0;chain<3;++chain) {
            xui_font font=chain==2 ? cjk : latin;latin->tFont.pFallback=chain==1 ? &cjk->tFont : NULL;
            for(size_t j=0;j<sizeof(corpus)/sizeof(*corpus);++j) {
                const char* text=corpus[j];int n=(int)strlen(text);LAYOUT_CHECK(xuiCodeDocumentSetText(document,text)==XUI_OK);
                for(int flags=0;flags<4;++flags)proxyCompare(&proxy,font,text,n,(uint32_t)flags);
                for(int tab=0;tab<3;++tab) {
                    a.iTabColumns=b.iTabColumns=tabs[tab];geometryCompare(&proxy,font,&a,&b,text,n,9.25f);
                }
            }
            const char nul[]={'A',0,'V','\n',(char)0xe4,(char)0xb8,(char)0xad,0};proxyCompare(&proxy,font,nul,7,XUI_TEXT_SHAPE_DEFAULT);
            proxyCompare(&proxy,font,"\xff",1,XUI_TEXT_SHAPE_DEFAULT);
            xui_proxy_t custom=proxy;custom.textShape=customShape;
            for(int i=0;i<3;++i) {
                const char* text=corpus[i+2];LAYOUT_CHECK(xuiCodeDocumentSetText(document,text)==XUI_OK);
                int before=customCalls;geometryCompare(&custom,font,&a,&b,text,(int)strlen(text),9.25f);LAYOUT_CHECK(customCalls>before);
            }
        }
        __xuiCodeEditLineLayoutsClear(&a);__xuiCodeEditLineLayoutsClear(&b);latin->tFont.pFallback=NULL;
        proxy.fontDestroy(&proxy,latin);proxy.fontDestroy(&proxy,cjk);
    }
    xui_font font=NULL;LAYOUT_CHECK(proxy.fontLoadFile(&proxy,&font,args[2],20,0)==XUI_OK);
    size_t n=1048576;char* text=malloc(n+1);LAYOUT_CHECK(text);const char* motif="AVATAR 中文😀 é\t";size_t m=strlen(motif),used=0;
    while(used+m<=n){memcpy(text+used,motif,m);used+=m;}memset(text+used,'A',n-used);text[n]=0;
    LAYOUT_CHECK(xuiCodeDocumentSetText(document,text)==XUI_OK);a.iTabColumns=b.iTabColumns=4;
    geometryCompare(&proxy,font,&a,&b,text,(int)n,9.25f);
    __xuiCodeEditLineLayoutsClear(&a);__xuiCodeEditLineLayoutsClear(&b);free(text);proxy.fontDestroy(&proxy,font);xuiCodeDocumentDestroy(document);
    verifyDpiLayoutGeometry(args);
    printf("stream layout comparison passed: %llu position assertions; %llu position cases; %llu positions; %llu callback failures; %llu layout assertions; %llu layout cases; %llu proxy cases\n",streamAssertions,streamPositionCases,streamPositions,streamErrorCases,layoutAssertions,layoutCases,proxyCases);fflush(stdout);
}
static int providerUnsupported(xui_proxy proxy,xui_font font,const char* text,int size,uint32_t flags,
    xui_text_cluster_visit_proc visitor,void* user,xui_text_shape_t* shape,void* providerUser) {
    (void)proxy;(void)font;(void)text;(void)size;(void)flags;(void)visitor;(void)user;(void)shape;
    ++*(int*)providerUser;return XUI_ERROR_UNSUPPORTED;
}
static void verifyDpiLayoutGeometry(char** args) {
    const float scales[]={1.0f,1.25f,1.5f,2.0f};
    const char* corpus[]={"","AVATAR Wa.","中文😀 é 👩🏽‍💻 🇨🇳","á́́","각각 क्ष","مرحبا אבג","A\t中文\t終","🏳️‍🌈👨‍👩‍👧‍👦","终\t"};
    xui_code_document document=NULL;LAYOUT_CHECK(xuiCodeDocumentCreate(&document)==XUI_OK);
    for(int scale=0;scale<4;++scale) {
        xide_dpi_proxy* state=NULL;xui_proxy_t proxy={0};
        LAYOUT_CHECK(xideDpiProxyCreate(&state,&proxy,scales[scale])==XUI_OK);
        xui_font font=NULL;LAYOUT_CHECK(proxy.fontLoadFile(&proxy,&font,args[2],20,0)==XUI_OK);
        xui_code_edit_data_t a={0},b={0};a.pDocument=b.pDocument=document;a.iTabColumns=b.iTabColumns=4;
        b.onShapeVisit=xideDpiVisitTextShape;
        for(size_t i=0;i<sizeof(corpus)/sizeof(*corpus);++i) {
            const char* text=corpus[i];LAYOUT_CHECK(xuiCodeDocumentSetText(document,text)==XUI_OK);
            geometryCompare(&proxy,font,&a,&b,text,(int)strlen(text),9.25f);
            for(int j=0;j<b.iLineLayoutCount;++j)
                for(int k=0;k<b.pLineLayouts[j].iSegmentCount;++k)LAYOUT_CHECK(b.pLineLayouts[j].pSegments[k].tShape.pClusters==NULL);
        }
        LAYOUT_CHECK(xideDpiProxySetScale(state,scales[(scale+1)%4])==XUI_OK);
        LAYOUT_CHECK(xuiCodeDocumentSetText(document,"AV 中文😀\t端")==XUI_OK);
        geometryCompare(&proxy,font,&a,&b,"AV 中文😀\t端",(int)strlen("AV 中文😀\t端"),9.25f);
        __xuiCodeEditLineLayoutsClear(&a);__xuiCodeEditLineLayoutsClear(&b);
        int unsupportedCalls=0;b.onShapeVisit=providerUnsupported;b.pShapeVisitUser=&unsupportedCalls;
        LAYOUT_CHECK(xuiCodeDocumentSetText(document,"fallback AV")==XUI_OK);
        geometryCompare(&proxy,font,&a,&b,"fallback AV",11,9.25f);LAYOUT_CHECK(unsupportedCalls==1);
        __xuiCodeEditLineLayoutsClear(&a);__xuiCodeEditLineLayoutsClear(&b);
        proxy.fontDestroy(&proxy,font);xideDpiProxyDestroy(state);
    }
    xuiCodeDocumentDestroy(document);
}
