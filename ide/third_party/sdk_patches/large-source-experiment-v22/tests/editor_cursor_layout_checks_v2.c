static unsigned long long layoutCursorAssertions,layoutCursorCases,layoutCursorReuses,layoutCursorCallbacks,layoutCursorWidgets,layoutCursorCancelCases;
#define LC_CHECK(e) do {++layoutCursorAssertions;if(!(e)){fprintf(stderr,"cursor layout check %d: %s\n",__LINE__,#e);exit(1);}}while(0)
typedef struct layout_cursor_stats_t {int lastOffset,lastReplay,lastCallbacks;} layout_cursor_stats_t;
typedef struct layout_cursor_visit_t {xui_text_cluster_visit_proc visit;void* user;layout_cursor_stats_t* stats;} layout_cursor_visit_t;
static int layoutCursorCount(const xui_text_cluster_t* cluster,void* user) {
    layout_cursor_visit_t* s=user;++s->stats->lastCallbacks;++layoutCursorCallbacks;return s->visit(cluster,s->user);
}
static int layoutCursorProvider(xui_proxy proxy,xui_font font,const char* text,int n,uint32_t flags,
    xui_text_cluster_visit_proc visit,void* user,xui_text_shape_t* shape,xui_text_shape_cursor old,int common,
    xui_text_shape_cursor* fresh,xui_text_shape_resume_t* resume,void* providerUser) {
    layout_cursor_stats_t* stats=providerUser;*stats=(layout_cursor_stats_t){0};layout_cursor_visit_t state={visit,user,stats};
    int code=xideDpiVisitTextShapeCached(proxy,font,text,n,flags,layoutCursorCount,&state,shape,old,common,fresh,resume,NULL);
    stats->lastOffset=resume->iTextOffset;stats->lastReplay=resume->iReplayTextOffset;
    if(stats->lastOffset>0)++layoutCursorReuses;
    return code;
}
static int layoutCursorUnsupported(xui_proxy p,xui_font f,const char* t,int n,uint32_t flags,
    xui_text_cluster_visit_proc v,void* user,xui_text_shape_t* shape,xui_text_shape_cursor old,int common,
    xui_text_shape_cursor* fresh,xui_text_shape_resume_t* resume,void* state) {return XUI_ERROR_UNSUPPORTED;}
static double layoutCursorOldOffset(const xui_code_edit_line_layout_t* p,int offset) {
    if(!p || p->iStopCount<=0 || offset<=0)return 0;
    for(int i=0;i<p->iStopCount;++i){if(offset==p->pStops[i].iOffset)return p->pStops[i].fX;if(offset<p->pStops[i].iOffset)return i>0?p->pStops[i-1].fX:0;}
    return p->fWidth;
}
static xui_code_edit_line_layout_t* layoutCursorCompare(xui_proxy proxy,xui_font font,xui_code_edit_data_t* a,
    xui_code_edit_data_t* b,const char* text,int n) {
    xui_code_edit_line_layout_t* old=diagPreviousLineLayoutEnsure(proxy,font,a,text,n,12.0f);
    xui_code_edit_line_layout_t* now=__xuiCodeEditLineLayoutEnsure(proxy,font,b,text,n,12.0f);
    LC_CHECK(old && now && old->iStopCount==now->iStopCount && old->iSegmentCount==now->iSegmentCount && old->fWidth==now->fWidth);
    { int equal=1;for(int i=0;i<old->iStopCount;++i)if(old->pStops[i].iOffset!=now->pStops[i].iOffset || old->pStops[i].fX!=now->pStops[i].fX){equal=0;break;}LC_CHECK(equal); }
    for(int i=0;i<old->iSegmentCount;++i) {
        xui_code_edit_text_segment_t* x=&old->pSegments[i];xui_code_edit_text_segment_t* y=&now->pSegments[i];
        LC_CHECK(x->iStart==y->iStart && x->iEnd==y->iEnd && x->fX==y->fX);
        xui_text_shape_t xs=x->tShape,ys=y->tShape;xs.pClusters=ys.pClusters=NULL;LC_CHECK(!memcmp(&xs,&ys,sizeof(xs)));
    }
    for(int i=-1;i<=64;++i){int offset=n*(i<0?0:i)/64;LC_CHECK(layoutCursorOldOffset(old,offset)==__xuiCodeEditLineLayoutOffsetX(now,offset));}
    if(n<=32768) for(int w=0;w<2;++w) {
        float width=w?8192.0f:64.5f;LC_CHECK(__xuiCodeEditLineLayoutBuildWraps(old,width)==__xuiCodeEditLineLayoutBuildWraps(now,width));
        LC_CHECK(old->iWrapOffsetCount==now->iWrapOffsetCount && !memcmp(old->pWrapOffsets,now->pWrapOffsets,(size_t)old->iWrapOffsetCount*sizeof(int)));
    }
    LC_CHECK(__xuiCodeEditLineLayoutEnsure(proxy,font,b,text,n,12.0f)==now);return now;
}
static void layoutCursorCase(xui_proxy proxy,xui_font font,const char* motif,int n,int middle) {
    char* before=malloc((size_t)n+1);char* after=malloc((size_t)n+2);int m=(int)strlen(motif),used=0;
    LC_CHECK(before && after);while(used+m<=n){memcpy(before+used,motif,(size_t)m);used+=m;}memset(before+used,'A',(size_t)(n-used));before[n]=0;
    int at=middle?n/2:n;while(at>0 && ((unsigned char)before[at]&0xc0u)==0x80u)--at;
    memcpy(after,before,(size_t)at);after[at]='V';memcpy(after+at+1,before+at,(size_t)(n-at)+1);
    xui_code_document document=NULL;LC_CHECK(xuiCodeDocumentCreate(&document)==XUI_OK);LC_CHECK(xuiCodeDocumentSetText(document,before)==XUI_OK);
    layout_cursor_stats_t stats={0};xui_code_edit_data_t a={0},b={0};a.pDocument=b.pDocument=document;a.iTabColumns=b.iTabColumns=4;
    b.onShapeVisit=xideDpiVisitTextShape;b.onCursorVisit=layoutCursorProvider;b.onCursorFree=xuiProxyXgeTextShapeCursorFree;b.pCursorVisitUser=&stats;
    layoutCursorCompare(proxy,font,&a,&b,before,n);
    LC_CHECK(xuiCodeDocumentReplace(document,at,at,"V")==XUI_OK);
    layoutCursorCompare(proxy,font,&a,&b,after,n+1);
    printf("cursor layout edit diagnostic: size=%d at=%d middle=%d tabs=%d resume=%d callbacks=%d\n",n,at,middle,strchr(before,'\t')!=NULL,stats.lastOffset,stats.lastCallbacks);fflush(stdout);
    if(!strchr(before,'\t')) {
        LC_CHECK(stats.lastOffset<=at);
        /* A UTF-8 boundary just before byte 4096 has no earlier checkpoint. */
        if(!middle || at>=8192)LC_CHECK(stats.lastOffset>0);
        if(stats.lastOffset>0)LC_CHECK(stats.lastCallbacks<n);
    }
    LC_CHECK(xuiCodeDocumentUndo(document)==XUI_OK);layoutCursorCompare(proxy,font,&a,&b,before,n);
    if(!strchr(before,'\t')) {LC_CHECK(stats.lastOffset<=at);if(!middle || at>=8192)LC_CHECK(stats.lastOffset>0);}
    LC_CHECK(xuiCodeDocumentRedo(document)==XUI_OK);layoutCursorCompare(proxy,font,&a,&b,after,n+1);
    ++layoutCursorCases;__xuiCodeEditLineLayoutsClear(&a);__xuiCodeEditLineLayoutsClear(&b);xuiCodeDocumentDestroy(document);free(before);free(after);
}
static int layoutCursorNoop(const xui_text_cluster_t* c,void* u) {return XUI_OK;}
typedef struct layout_cursor_cancel_t {int at,failAt;} layout_cursor_cancel_t;
static int layoutCursorCancel(const xui_text_cluster_t* c,void* user) {
    layout_cursor_cancel_t* s=user;
    if(s->at++==s->failAt)return XUI_ERROR_OUT_OF_MEMORY;
    return XUI_OK;
}
static void verifyCursorLayout(char** args) {
    const float scales[]={1,1.25f,1.5f,2};const char* motifs[]={"AVATAR To WA. ","AV 中文😀 é 👩‍💻 終 ","AV\t中文😀\t終 "};int sizes[]={8192,32768};
    LC_CHECK(g_xuiProxyXgeCursors==NULL);
    for(int scale=0;scale<4;++scale) {
        xide_dpi_proxy* state=NULL;xui_proxy_t proxy={0};xui_font font=NULL,other=NULL;xui_context context=NULL;
        LC_CHECK(xideDpiProxyCreate(&state,&proxy,scales[scale])==XUI_OK);
        LC_CHECK(proxy.fontLoadFile(&proxy,&font,args[2],20,0)==XUI_OK);LC_CHECK(proxy.fontLoadFile(&proxy,&other,args[2],24,0)==XUI_OK);
        for(int motif=0;motif<3;++motif)for(int size=0;size<2;++size)for(int edit=0;edit<2;++edit)layoutCursorCase(&proxy,font,motifs[motif],sizes[size],edit);
        if(scale==1)for(int motif=0;motif<2;++motif)for(int edit=0;edit<2;++edit)layoutCursorCase(&proxy,font,motifs[motif],2097152,edit);
        LC_CHECK(xuiCreate(&context)==XUI_OK && xuiSetProxy(context,&proxy)==XUI_OK && xuiSetDefaultFont(context,font)==XUI_OK);
        LC_CHECK(xuiInputViewport(context,640,480)==XUI_OK);
        char text[8193];memset(text,'A',8192);text[8192]=0;
        for(int language=0;language<2;++language) {
            xui_widget widget=NULL;xui_code_edit_desc_t d={0};layout_cursor_stats_t stats={0};d.iSize=sizeof(d);d.pFont=font;d.sText=text;d.sLanguage=language?"xlang":"c";d.iTabColumns=4;
            LC_CHECK(xideDpiCodeEditCreate(context,&widget,&d)==XUI_OK);xui_code_edit_data_t* data=__xuiCodeEditGetData(widget);LC_CHECK(data && data->onCursorVisit==xideDpiVisitTextShapeCached);
            LC_CHECK(xuiCodeEditSetTextShapeCursorVisitor(widget,layoutCursorProvider,NULL,&stats)==XUI_ERROR_INVALID_ARGUMENT);
            LC_CHECK(xuiCodeEditSetTextShapeCursorVisitor(widget,layoutCursorProvider,xuiProxyXgeTextShapeCursorFree,&stats)==XUI_OK);
            xui_code_edit_line_layout_t* layout=__xuiCodeEditLineLayoutEnsure(&proxy,font,data,text,8192,12);LC_CHECK(layout && layout->pCursor);
            xui_code_edit_caret_stop_t* stops=layout->pStops;
            LC_CHECK(xuiCodeDocumentReplace(data->pDocument,8192,8192,"V")==XUI_OK);
            char changed[8194];memcpy(changed,text,8192);changed[8192]='V';changed[8193]=0;
            layout=__xuiCodeEditLineLayoutEnsure(&proxy,font,data,changed,8193,12);LC_CHECK(layout && stats.lastOffset>0 && layout->pStops==stops);
            LC_CHECK(xuiCodeEditSetFont(widget,other)==XUI_OK && data->iLineLayoutCount==0);
            layout=__xuiCodeEditLineLayoutEnsure(&proxy,other,data,changed,8193,12);LC_CHECK(layout && stats.lastOffset==0);
            LC_CHECK(xuiCodeEditSetTextShapeCursorVisitor(widget,layoutCursorUnsupported,xuiProxyXgeTextShapeCursorFree,NULL)==XUI_OK);
            layout=__xuiCodeEditLineLayoutEnsure(&proxy,other,data,changed,8193,12);LC_CHECK(layout && !layout->pCursor && !layout->pSegments[0].tShape.pClusters);
            LC_CHECK(xuiCodeEditSetTextShapeCursorVisitor(widget,NULL,NULL,NULL)==XUI_OK && data->iLineLayoutCount==0);
            xuiWidgetDestroy(widget);++layoutCursorWidgets;
        }
        xuiDestroy(context);proxy.fontDestroy(&proxy,other);proxy.fontDestroy(&proxy,font);xideDpiProxyDestroy(state);LC_CHECK(g_xuiProxyXgeCursors==NULL);
    }
    /* A caller may free the SDK font before disposing its cache wrapper. */
    xui_proxy_t raw=xuiProxyXge();xui_font font=NULL;xui_text_shape_cursor cache=NULL;xui_text_shape_resume_t resume={0};xui_text_shape_t shape={0};char longText[8193];memset(longText,'A',8192);longText[8192]=0;resume.iSize=sizeof(resume);
    LC_CHECK(raw.fontLoadFile(&raw,&font,args[2],20,0)==XUI_OK);
    LC_CHECK(xuiProxyXgeVisitTextShapeCached(&raw,font,longText,8192,XUI_TEXT_SHAPE_DEFAULT,layoutCursorNoop,NULL,&shape,NULL,0,&cache,&resume,NULL)==XUI_OK && cache);
    xuiTextShapeFree(&shape);
    char appended[8194];memcpy(appended,longText,8192);appended[8192]='V';appended[8193]=0;
    xui_text_shape_cursor fresh=NULL;layout_cursor_cancel_t cancellation={0,-1};resume.iSize=sizeof(resume);
    LC_CHECK(xuiProxyXgeVisitTextShapeCached(&raw,font,appended,8193,XUI_TEXT_SHAPE_DEFAULT,layoutCursorCancel,&cancellation,&shape,cache,8192,&fresh,&resume,NULL)==XUI_OK);
    LC_CHECK(fresh && resume.iTextOffset>0 && cancellation.at>0);int tail=cancellation.at;xuiTextShapeFree(&shape);xuiProxyXgeTextShapeCursorFree(fresh,NULL);
    for(int point=0;point<3;++point) {
        cancellation=(layout_cursor_cancel_t){0,point==0?0:(point==1?tail/2:tail-1)};fresh=NULL;resume.iSize=sizeof(resume);
        LC_CHECK(xuiProxyXgeVisitTextShapeCached(&raw,font,appended,8193,XUI_TEXT_SHAPE_DEFAULT,layoutCursorCancel,&cancellation,&shape,cache,8192,&fresh,&resume,NULL)==XUI_ERROR_OUT_OF_MEMORY);
        LC_CHECK(fresh==NULL && resume.iTextOffset>0);xuiTextShapeFree(&shape);
        /* The borrowed old cache remains usable after cancellation. */
        resume.iSize=sizeof(resume);
        LC_CHECK(xuiProxyXgeVisitTextShapeCached(&raw,font,appended,8193,XUI_TEXT_SHAPE_DEFAULT,layoutCursorNoop,NULL,&shape,cache,8192,&fresh,&resume,NULL)==XUI_OK && fresh);
        xuiTextShapeFree(&shape);xuiProxyXgeTextShapeCursorFree(fresh,NULL);++layoutCursorCancelCases;
    }
    raw.fontDestroy(&raw,font);LC_CHECK(!((xui_proxy_xge_cursor_t*)cache)->pCursor);xuiProxyXgeTextShapeCursorFree(cache,NULL);LC_CHECK(g_xuiProxyXgeCursors==NULL);
    printf("cursor layout comparison passed: %llu assertions; %llu edit cases; %llu resumed calls; %llu callbacks; %llu real widget cases; %llu cancellation cases\n",layoutCursorAssertions,layoutCursorCases,layoutCursorReuses,layoutCursorCallbacks,layoutCursorWidgets,layoutCursorCancelCases);fflush(stdout);
}
