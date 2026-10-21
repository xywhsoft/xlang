static unsigned long long widgetAssertions, widgetCases;
#define WIDGET_CHECK(e) do { ++widgetAssertions; if(!(e)){fprintf(stderr,"stream widget check %d: %s\n",__LINE__,#e);exit(1);} }while(0)
static void verifyLayoutProviderLifecycle(char** args)
{
    const float scales[]={1.0f,1.25f,1.5f,2.0f};
    for(int scale=0;scale<4;++scale) {
        xide_dpi_proxy* state=NULL;xui_proxy_t proxy={0};xui_context context=NULL;
        xui_font font=NULL,other=NULL;xui_widget plain=NULL;
        WIDGET_CHECK(xideDpiProxyCreate(&state,&proxy,scales[scale])==XUI_OK);
        WIDGET_CHECK(proxy.fontLoadFile(&proxy,&font,args[2],20,0)==XUI_OK);
        WIDGET_CHECK(proxy.fontLoadFile(&proxy,&other,args[2],24,0)==XUI_OK);
        WIDGET_CHECK(xuiCreate(&context)==XUI_OK);
        WIDGET_CHECK(xuiSetProxy(context,&proxy)==XUI_OK);
        WIDGET_CHECK(xuiSetDefaultFont(context,font)==XUI_OK);
        WIDGET_CHECK(xuiInputViewport(context,640,480)==XUI_OK);
        WIDGET_CHECK(xuiWidgetCreate(context,&plain)==XUI_OK);
        WIDGET_CHECK(xuiCodeEditSetTextShapeVisitor(NULL,xideDpiVisitTextShape,NULL)==XUI_ERROR_INVALID_ARGUMENT);
        WIDGET_CHECK(xuiCodeEditSetTextShapeVisitor(plain,xideDpiVisitTextShape,NULL)==XUI_ERROR_INVALID_ARGUMENT);
        for(int language=0;language<2;++language) {
            xui_code_edit_desc_t desc={0};xui_widget widget=NULL;
            desc.iSize=sizeof(desc);desc.pFont=font;desc.sText="AVATAR 中文😀 é\tlong text for wrapping\nsecond line";
            desc.sLanguage=language ? "xlang" : "c";desc.bWordWrap=1;desc.iTabColumns=4;
            WIDGET_CHECK(xideDpiCodeEditCreate(context,&widget,&desc)==XUI_OK);
            xui_code_edit_data_t* data=__xuiCodeEditGetData(widget);WIDGET_CHECK(data);
            WIDGET_CHECK(data->onShapeVisit==xideDpiVisitTextShape && data->pShapeVisitUser==NULL);
            WIDGET_CHECK(__xuiCodeEditWrapPrepare(widget,data,64.5f)==XUI_OK);
            xui_code_edit_line_layout_t* layout=NULL;
            WIDGET_CHECK(__xuiCodeEditWrapEnsureLine(widget,data,0,64.5f,&layout)==XUI_OK && layout);
            WIDGET_CHECK(data->iWrapIndexCount>0 && data->pWrapRows && data->pWrapTree && data->pWrapMeasured);
            WIDGET_CHECK(data->iLineLayoutCount>0 && layout->iSegmentCount>0 && layout->pSegments[0].tShape.pClusters==NULL);
            int* rows=data->pWrapRows;int count=data->iLineLayoutCount;
            WIDGET_CHECK(xuiCodeEditSetTextShapeVisitor(widget,xideDpiVisitTextShape,NULL)==XUI_OK);
            WIDGET_CHECK(data->pWrapRows==rows && data->iLineLayoutCount==count);
            int unsupported=0;
            WIDGET_CHECK(xuiCodeEditSetTextShapeVisitor(widget,providerUnsupported,&unsupported)==XUI_OK);
            WIDGET_CHECK(data->iWrapIndexCount==0 && data->pWrapRows==NULL && data->pWrapTree==NULL && data->pWrapMeasured==NULL && data->iLineLayoutCount==0);
            WIDGET_CHECK(data->iWrapDocumentVersion==0 && data->pWrapLayoutFont==NULL);
            WIDGET_CHECK(__xuiCodeEditWrapPrepare(widget,data,64.5f)==XUI_OK);
            WIDGET_CHECK(__xuiCodeEditWrapEnsureLine(widget,data,0,64.5f,&layout)==XUI_OK && layout);
            WIDGET_CHECK(unsupported>0 && layout->pSegments[0].tShape.pClusters!=NULL);
            WIDGET_CHECK(xuiCodeEditSetTextShapeVisitor(widget,xideDpiVisitTextShape,NULL)==XUI_OK);
            WIDGET_CHECK(data->iWrapIndexCount==0 && data->iLineLayoutCount==0);
            WIDGET_CHECK(__xuiCodeEditWrapPrepare(widget,data,64.5f)==XUI_OK);
            WIDGET_CHECK(__xuiCodeEditWrapEnsureLine(widget,data,0,64.5f,&layout)==XUI_OK && layout);
            WIDGET_CHECK(layout->pSegments[0].tShape.pClusters==NULL);
            WIDGET_CHECK(xuiCodeEditSetFont(widget,other)==XUI_OK);
            WIDGET_CHECK(data->onShapeVisit==xideDpiVisitTextShape && xuiCodeEditGetFont(widget)==other);
            WIDGET_CHECK(__xuiCodeEditWrapPrepare(widget,data,64.5f)==XUI_OK);
            WIDGET_CHECK(__xuiCodeEditWrapEnsureLine(widget,data,0,64.5f,&layout)==XUI_OK && layout);
            WIDGET_CHECK(layout->pSegments[0].tShape.pClusters==NULL && data->pWrapLayoutFont==other);
            WIDGET_CHECK(xuiCodeEditSetTextShapeVisitor(widget,NULL,NULL)==XUI_OK);
            WIDGET_CHECK(data->iWrapIndexCount==0 && data->iLineLayoutCount==0);
            WIDGET_CHECK(__xuiCodeEditWrapPrepare(widget,data,64.5f)==XUI_OK);
            WIDGET_CHECK(__xuiCodeEditWrapEnsureLine(widget,data,0,64.5f,&layout)==XUI_OK && layout);
            WIDGET_CHECK(layout->pSegments[0].tShape.pClusters!=NULL);
            xuiWidgetDestroy(widget);++widgetCases;
        }
        xuiWidgetDestroy(plain);
        xuiDestroy(context);proxy.fontDestroy(&proxy,other);proxy.fontDestroy(&proxy,font);xideDpiProxyDestroy(state);
    }
    printf("stream widget lifecycle passed: %llu assertions; %llu cases\n",widgetAssertions,widgetCases);fflush(stdout);
}
