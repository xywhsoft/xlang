#include "xui.h"
#include "xui_test_proxy.h"
#include <stdio.h>
#include <string.h>
static unsigned checks;static xui_context context;static xui_code_document docs[2];
static xui_code_document_batch current;static int notices,callbackError,closeOwner;
static int closeStatus;static bool closeConsumed;
static const char *before[]={"int alpha = 1;\nint keep = 2;\n","func main()\n    print(\"before\")\nend\n"};
static const char *after[]={"int 中文 = 2;\nint keep = 3;\n","func main()\n    print(\"😀 after\")\nend\n"};
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"xui_document_batch_dll_test:%d:%s\n",__LINE__,#v);failed=1;goto cleanup;}}while(0)
static void changed(xui_code_document document,xui_code_range_t range,uint32_t version,void* user)
{
    (void)document;(void)range;(void)version;(void)user;++notices;
    for(size_t i=0;i<2;++i)if(strcmp(xuiCodeDocumentGetText(docs[i]),after[i]))callbackError=1;
    if(xuiCodeDocumentInsert(docs[1],0,"forbidden")!=XUI_ERROR_BUSY || xuiCodeDocumentBatchDestroy(current)!=XUI_ERROR_BUSY)callbackError=1;
    if(closeOwner){closeStatus=xuiDestroyOwnerChecked(context,&closeConsumed);if(closeConsumed)context=NULL;}
}
int main(void)
{
    int failed=0;xui_test_proxy_state_t proxy;xui_widget editors[2]={0};xui_font font=NULL;xui_surface surface=NULL;xui_rect_i_t damage={0,0,640,240};
    xuiTestProxyInit(&proxy);CHECK(xuiCreate(&context)==XUI_OK && xuiSetProxy(context,&proxy.tProxy)==XUI_OK);
    CHECK(proxy.tProxy.fontLoadFile(&proxy.tProxy,&font,"test.ttf",14,0)==XUI_OK && xuiSetDefaultFont(context,font)==XUI_OK);
    CHECK(xuiInputViewport(context,640,240)==XUI_OK && xuiTestSurfaceCreate(&proxy,&surface,640,240,XUI_SURFACE_USAGE_TARGET)==XUI_OK);
    for(size_t i=0;i<2;++i){
        xui_code_edit_desc_t desc={0};desc.iSize=sizeof(desc);desc.pFont=font;desc.sText=before[i];desc.sLanguage=i?"xlang":"c";
        CHECK(xuiCodeEditCreate(context,editors+i,&desc)==XUI_OK);docs[i]=xuiCodeEditGetDocument(editors[i]);CHECK(docs[i]);
        CHECK(xuiSetRootWidget(context,editors[i])==XUI_OK && xuiWidgetSetRect(editors[i],(xui_rect_t){0,0,640,240})==XUI_OK);
        CHECK(xuiLayout(context)==XUI_OK && xuiRender(context,surface,&damage,1)==XUI_OK);
    }
    xui_code_document_batch_item_t rows[2];for(size_t i=0;i<2;++i)rows[i]=(xui_code_document_batch_item_t){docs[i],xuiCodeDocumentGetChangeVersion(docs[i]),{before[i],strlen(before[i])},{after[i],strlen(after[i])}};
    CHECK(xuiCodeDocumentBatchPrepare(rows,2,0,&current)==XUI_OK && current);
    for(size_t i=0;i<2;++i){CHECK(!strcmp(xuiCodeEditGetText(editors[i]),before[i]));CHECK(xuiCodeDocumentSetChangeCallback(docs[i],changed,NULL)==XUI_OK);}
    bool applied=false;CHECK(xuiCodeDocumentBatchCommit(current,&applied)==XUI_OK && applied && !callbackError && notices==2);
    CHECK(xuiCodeDocumentBatchDestroy(current)==XUI_OK);current=NULL;
    for(size_t i=0;i<2;++i){
        CHECK(xuiCodeDocumentSetChangeCallback(docs[i],NULL,NULL)==XUI_OK && xuiCodeDocumentCanUndo(docs[i]));
        CHECK(!strcmp(xuiCodeEditGetText(editors[i]),after[i]));CHECK(xuiSetRootWidget(context,editors[i])==XUI_OK && xuiLayout(context)==XUI_OK && xuiRender(context,surface,&damage,1)==XUI_OK);
        CHECK(xuiCodeDocumentUndo(docs[i])==XUI_OK && !strcmp(xuiCodeEditGetText(editors[i]),before[i]));
    }
    /* Real CodeEdit-owned documents and Context destruction during the first
     * notice. Both payloads must already be present; cleanup can return error. */
    for(size_t i=0;i<2;++i){rows[i].iChangeVersion=xuiCodeDocumentGetChangeVersion(docs[i]);CHECK(xuiCodeDocumentSetChangeCallback(docs[i],changed,NULL)==XUI_OK);}
    CHECK(xuiCodeDocumentBatchPrepare(rows,2,0,&current)==XUI_OK);closeOwner=1;notices=0;applied=false;
    int status=xuiCodeDocumentBatchCommit(current,&applied);CHECK(status==XUI_OK && applied && !callbackError && notices==1 && context && closeStatus==XUI_STATUS_DEFERRED && !closeConsumed);
    bool consumed=false;status=xuiDestroyOwnerChecked(context,&consumed);if(consumed)context=NULL;CHECK(status==XUI_OK && consumed && !context);
    CHECK(xuiCodeDocumentBatchDestroy(current)==XUI_OK);current=NULL;
cleanup:
    if(current)xuiCodeDocumentBatchDestroy(current);
    if(context)xuiDestroy(context);
    if(surface)proxy.tProxy.surfaceDestroy(&proxy.tProxy,surface);
    if(font)proxy.tProxy.fontDestroy(&proxy.tProxy,font);
    if(!failed)printf("xui_document_batch_dll_test passed: %u checks; actual DLL, real CodeEdit observers/caches/rendering, C/xlang, Unicode, grouped undo and Context close after all payloads publish\n",checks);
    return failed?1:0;
}
