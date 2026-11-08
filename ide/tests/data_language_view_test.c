#include "app/xide_app.h"
#include "lang/languages.h"
#include "ui/editor_view.h"
#include "ui/workspace_view.h"
#include "services/session.h"
#include "platform/path.h"
#include "catalog_ready.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int checks,failed,started;
#define CHECK(e) do{++checks;if(!(e)){fprintf(stderr,"data_language_view_test:%d: %s; %s\n",__LINE__,#e,error.message);failed=1;goto cleanup;}}while(0)
static int settle(xide_app* a)
{xideEditorPoll(a);xideWorkbenchLayout(a);if(xuiDispatchPendingEvents(a->context)!=XUI_OK || xuiLayout(a->context)!=XUI_OK)return 0;xideSessionAfterLayout(a);return !a->frameError;}
static int cached(xui_widget editor)
{
    int count=0;xui_code_token_t tokens[512];
    int result=xuiCodeTokenBufferGetTokens(xuiCodeEditGetTokenBuffer(editor),xuiCodeDocumentGetVersion(xuiCodeEditGetDocument(editor)),tokens,512,&count);
    if(result!=XUI_OK || count<3){printf("cache result=%d count=%d language=%s\n",result,count,xuiCodeEditGetLanguage(editor));return 0;}
    for(int i=0;i<count;++i)if(tokens[i].iKind==XUI_CODE_TOKEN_ERROR){printf("cache error token %d..%d text=%s\n",tokens[i].iStartOffset,tokens[i].iEndOffset,xuiCodeEditGetText(editor));return 0;}
    return 1;
}
static int run(void* raw)
{
    xide_app a={0};xide_model_error error={0};int began=0;char* directory=NULL;char* root=NULL;char* settings=NULL;char* sessions=NULL;char* paths[4]={0};xide_document* docs[4]={0};started=1;
    const char* names[]={"sample.JSON","sample.XLON","sample.jsonl","sample.xlonl"};const char* modes[]={"json","xlon","jsonl","xlonl"};
    const int actions[]={XIDE_ACTION_LANGUAGE_JSON,XIDE_ACTION_LANGUAGE_XLON,XIDE_ACTION_LANGUAGE_JSONL,XIDE_ACTION_LANGUAGE_XLONL};
    const char* text[]={"{\n  \"标题\": \"JSON 中文😀\",\n  \"enabled\": true,\n  \"values\": [1, -2.5e2, null]\n}\n","{\n  \"blob\": bytes(\"AAE=\"),\n  \"when\": time(\"2026-11-08T00:00:00Z\"),\n  \"roles\": set[\"read\", \"write\"],\n  \"ports\": intmap{80: true}\n}\n","{\"标题\":\"first 中文\",\"ok\":true}\n{\"n\":-2.5e2}\n","{\"data\":bytes(\"AAE=\")}\nset[1,2]\n"};
    char* id=xideCreateId("data-languages");directory=id?xidePathResolve(raw,id):NULL;free(id);CHECK(directory && xrtDirCreateAll(directory));
    root=xidePathResolve(directory,"中文 文件");settings=xidePathResolve(directory,"settings.json");sessions=xidePathResolve(directory,"session");CHECK(root && settings && sessions && xrtDirCreateAll(root));
    a.hidden=1;a.disableAutomaticDiagnostics=1;a.settingsPath=settings;a.sessionRoot=sessions;
    CHECK(xideAppCreate(&a)==XUI_OK && xgeBegin()==XGE_OK);began=1;CHECK(xideTestCatalogReady(&a));
    CHECK(xuiMenuGetItemCount(a.languageMenu)==7);
    for(int i=0;i<4;++i){
        paths[i]=xidePathResolve(root,names[i]);CHECK(paths[i] && xrtFileWriteAtomic(paths[i],(xbytesview){(const unsigned char*)text[i],strlen(text[i])}));
        CHECK(xideWorkspaceOpenFile(&a,paths[i]) && settle(&a));docs[i]=xideDocumentsFind(&a.documents,paths[i]);CHECK(docs[i] && !strcmp(docs[i]->language,modes[i]) && !strcmp(xuiCodeEditGetLanguage(docs[i]->editor),modes[i]) && cached(docs[i]->editor));
        CHECK(xideActionQuery(&a,actions[i]).enabled && xideActionQuery(&a,actions[i]).checked && !xideActionQuery(&a,XIDE_ACTION_EDITOR_COMPLETE).enabled);
        CHECK(xideActionExecute(&a,XIDE_ACTION_LANGUAGE_TEXT)==XIDE_ACTION_DISPATCHED && xideActionExecute(&a,actions[i])==XIDE_ACTION_DISPATCHED && settle(&a) && cached(docs[i]->editor));
        CHECK(xuiInputSetModifiers(a.context,0)==XUI_OK && xuiSetFocusWidget(a.context,docs[i]->editor)==XUI_OK && xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(docs[i]->editor),xuiCodeEditGetDocument(docs[i]->editor),(int)strlen(text[i]),0)==XUI_OK);
        int typed=xuiInputTextEx(a.context,' ',NULL),settled=settle(&a),cacheOkay=cached(docs[i]->editor);
        if(typed!=XUI_OK || !settled || !docs[i]->dirty || !cacheOkay)printf("typed=%d settled=%d dirty=%d cached=%d mode=%s readonly=%d text=%s\n",typed,settled,docs[i]->dirty,cacheOkay,modes[i],xuiCodeEditIsReadonly(docs[i]->editor),xuiCodeEditGetText(docs[i]->editor));
        CHECK(typed==XUI_OK && settled && docs[i]->dirty && cacheOkay);
        CHECK(xuiInputKeyDownEx(a.context,'Z',XUI_MOD_CTRL,NULL)==XUI_OK && xuiInputKeyUpEx(a.context,'Z',XUI_MOD_CTRL,NULL)==XUI_OK && settle(&a) && !docs[i]->dirty && !strcmp(xuiCodeEditGetText(docs[i]->editor),text[i]) && cached(docs[i]->editor));
        CHECK(xuiUpdate(a.context,0)==XUI_OK);xui_rect_i_t clip={0,0,a.viewportWidth,a.viewportHeight};CHECK(a.proxy.surfaceClear(&a.proxy,a.target,0xffffffff)==XUI_OK && xuiRender(a.context,a.target,&clip,1)==XUI_OK);
        char shot[32768];snprintf(shot,sizeof(shot),"%s/highlight-%s.png",(const char*)raw,modes[i]);CHECK(xideAppCapture(&a,shot)==XGE_OK);
    }
    CHECK(xuiInputSetModifiers(a.context,0)==XUI_OK && xideEditorShow(&a,docs[2])==XUI_OK && xuiSetFocusWidget(a.context,docs[2]->editor)==XUI_OK);
    CHECK(xuiInputTextEx(a.context,' ',NULL)==XUI_OK && settle(&a) && docs[2]->dirty && xideSessionSave(&a,0,&error));
    xgeEnd();began=0;xideAppDestroy(&a);memset(&a,0,sizeof(a));
    a.hidden=1;a.disableAutomaticDiagnostics=1;a.settingsPath=settings;a.sessionRoot=sessions;
    CHECK(xideAppCreate(&a)==XUI_OK && xgeBegin()==XGE_OK);began=1;CHECK(xideTestCatalogReady(&a) && settle(&a));
    for(int i=0;i<4;++i){docs[i]=xideDocumentsFind(&a.documents,paths[i]);CHECK(docs[i] && !strcmp(docs[i]->language,modes[i]) && cached(docs[i]->editor));CHECK(docs[i]->dirty==(i==2));}
    /* Explicitly changing a normal file's format also survives recovery. */
    xide_text_file baseline={0};CHECK(xideTextFileEncode(text[1],0,0,XUI_CODE_EOL_LF,&baseline,&error));
    xide_document* recovered=xideDocumentRecover(&a.documents,NULL,"恢复 XLON","xlon",&baseline,text[1],&error);xideTextFileClear(&baseline);CHECK(recovered && !strcmp(recovered->language,"xlon") && cached(recovered->editor));
    puts("data_language_view_test: native four-format screenshots, real files, manual modes, cache after edits/undo, no source completion, clean/dirty restart and typed recovery");
cleanup:
    if(began)xgeEnd();
    xideAppDestroy(&a);for(int i=0;i<4;++i)free(paths[i]);free(directory);free(root);free(settings);free(sessions);xgeQuit();
    if(!failed)printf("data_language_view_test passed: %d checks\n",checks);
    return XGE_OK;
}
int main(int argc,char** argv)
{if(argc!=2)return 2;xge_desc_t d={0};d.iWidth=1280;d.iHeight=900;d.sTitle="Structured data highlighting";d.iFlags=XGE_INIT_WINDOW;int result=xgeInit(&d);if(result==XGE_OK)result=xgeRun(run,argv[1]);xgeUnit();return result==XGE_OK && started && !failed?0:1;}
