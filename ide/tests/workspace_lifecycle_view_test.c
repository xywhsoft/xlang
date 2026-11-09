#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "ui/workspace_view.h"
#include "services/file_sync.h"
#include "services/language_client.h"
#include "platform/path.h"
#include "catalog_ready.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Observe only this test's stats reply;ordinary messages keep the real view
 * dispatch path,including diagnostics,source sync and background worker wakes. */
static xide_language_client* statsClient;static int statsId;static xvalue* statsReply;
static xvalue* observeNext(xide_language_client* client,size_t* bytes)
{
 xvalue* value;while((value=xideLanguageClientNextSized(client,bytes))){int64 id=0;if(client==statsClient && statsId && xrtValueGetInt(xrtValueObjectGet(value,xrtStrView("id")),&id) && id==statsId){xrtValueRelease(statsReply);statsReply=value;continue;}return value;}return NULL;
}
#define xideLanguageClientNextSized observeNext
#include "../src/ui/language_view.c"
#undef xideLanguageClientNextSized
typedef struct fixture {const char *directory,*compiler;HDESK desktop;int started,failed,checks;} fixture;
#define CHECK(v) do{++f->checks;if(!(v)){fprintf(stderr,"workspace_lifecycle_view_test:%d:%s;%s;%s\n",__LINE__,#v,error.message,app.languageView?app.languageView->message:"");f->failed=1;goto cleanup;}}while(0)
static int writeText(const char* path,const char* text){return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)});}
static int settle(xide_app* app){if(xuiDispatchPendingEvents(app->context)!=XUI_OK)return 0;xideEditorPoll(app);xideFileSyncPoll(app);xideTaskViewPoll(app);xideLanguageViewPoll(app);xideWorkbenchLayout(app);return xuiLayout(app->context)==XUI_OK && !app->frameError;}
static const xvalue* stats(xide_app* app)
{
 xide_language_view* v=app->languageView;if(!v || !v->initialized || v->syncMode || v->waitingInitialize || !v->autoEnabled || !v->autoView->syncAdopted)return NULL;
 xrtValueRelease(statsReply);statsReply=NULL;statsClient=v->client;statsId=++v->nextId;if(!sendMessage(v,"xlang/workspaceSymbolStats",xrtValueObject(),statsId))return NULL;
 uint64_t until=GetTickCount64()+10000;while(!statsReply && GetTickCount64()<until){if(!settle(app))return NULL;Sleep(1);}statsId=0;return statsReply?get(get(statsReply,"result"),"background"):NULL;
}
static int64 count(const xvalue* value,const char* key){int64 n=-1;return xrtValueGetInt(get(value,key),&n)?n:-1;}
static int flag(const xvalue* value,const char* key){bool b=false;return xrtValueGetBool(get(value,key),&b) && b;}
static int waitBackground(xide_app* app,int64 complete,int64 failed)
{
 uint64_t until=GetTickCount64()+20000;do{if(!settle(app))return 0;const xvalue* value=stats(app);if(value && count(value,"completed")>=complete && count(value,"failed")>=failed && !flag(value,"running") && !flag(value,"pending"))return 1;Sleep(5);}while(GetTickCount64()<until);return 0;
}
static int waitFiles(xide_app* app){uint64_t until=GetTickCount64()+10000;do{if(!settle(app))return 0;if(xideFileSyncSettled(app))return 1;Sleep(5);}while(GetTickCount64()<until);return 0;}
static int search(xide_app* app,const char* query,size_t expected,int failure)
{
 if(!xideLanguageViewWorkspaceSymbolsIsOpen(app) && xideActionExecute(app,XIDE_ACTION_WORKSPACE_SYMBOLS)!=XIDE_ACTION_DISPATCHED)return 0;
 if(xuiEditSetText(app->languageView->workspaceSearch->input,query)!=XUI_OK || !workspaceBegin(app,0,0))return 0;
 uint64_t until=GetTickCount64()+20000;
 do{if(!settle(app))return 0;struct xide_workspace_search_view* r=app->languageView->workspaceSearch;if(!strcmp(r->query,query) && !r->stage && !r->due){if(failure && !r->usable && *r->message)return 1;if(!failure && r->usable && r->result && r->result->count==expected)return 1;}Sleep(5);}while(GetTickCount64()<until);return 0;
}
static int quiet(xide_app* app,unsigned milliseconds){uint64_t end=GetTickCount64()+milliseconds;do{if(!settle(app))return 0;Sleep(5);}while(GetTickCount64()<end);return 1;}
static int verify(void* user)
{
 fixture* f=user;if(f->started)return XGE_OK;f->started=1;xide_app app={0};xide_model_error error={0};xide_project project={0};int began=0;
 char *root=NULL,*entry=NULL,*helper=NULL,*orphan=NULL,*native=NULL,*a=NULL,*b=NULL,*workspace=NULL,*settings=NULL;
 const char* mainBody="import helper\nfunc main(...) as int\n return helper.value()\nend\n";
 const char* library="/* 中文😀 */\nclass Payload\n int number\nend\nexport func value() as int\n return 1\nend\n";
 const char* changed="/* 中文😀 */\nclass Payload\n int amount\nend\nexport func value() as int\n return 1\nend\n";
 const char* dirty="/* 中文😀 */\nclass Payload\n int edited\nend\nexport func value() as int\n return 1\nend\n";
 const char* unopened="func orphanWork() as int\n return 7\nend\nfunc main(...) as int\n return orphanWork()\nend\n";
 root=xidePathResolve(f->directory,"中文😀 lifecycle");entry=xidePathResolve(root,"main.xl");helper=xidePathResolve(root,"helper.xl");orphan=xidePathResolve(root,"未打开.xl");native=xidePathResolve(root,"native.c");a=xidePathResolve(root,"A.xlp");b=xidePathResolve(root,"B.xlp");workspace=xidePathResolve(root,"test.xlw");settings=xidePathResolve(root,"settings.json");
 CHECK(root && entry && helper && orphan && native && a && b && workspace && settings && xrtDirCreateAll(root));CHECK(writeText(entry,mainBody) && writeText(helper,library) && writeText(orphan,unopened) && writeText(native,"int main(void) { return 0; }\n"));
 app.hidden=1;app.noSession=1;app.compilerPath=f->compiler;app.settingsPath=settings;CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK);began=1;CHECK(xideTestCatalogReady(&app) && !app.languageView->client);
 CHECK(xideWorkspaceCreate(&app.workspaceModel,"自动索引",&error));
 for(size_t i=0;i<2;++i){CHECK(xideProjectCreate(&project,i?b:a,i?"B":"A",entry,&error) && xideProjectAddFile(&project,helper,"xlang",&error) && xideProjectAddFile(&project,orphan,"xlang",&error) && xideProjectSave(&project,project.manifest,&error));xideProjectClear(&project);CHECK(xideWorkspaceAddProject(&app.workspaceModel,i?b:a,i?"B":"A",&error));}
 CHECK(xideWorkspaceSave(&app.workspaceModel,workspace,&error) && xideWorkspaceViewRefresh(&app)==XUI_OK);xideLanguageViewConfigurationChanged(&app);xideFileSyncRefresh(&app);CHECK(waitFiles(&app) && waitBackground(&app,1,0));
 xide_language_view* v=app.languageView;xide_language_client* original=v->client;CHECK(!app.activeDocument && !app.documents.count && !current(v) && count(get(get(statsReply,"result"),"background"),"index.rows")==12 && count(get(get(statsReply,"result"),"background"),"captured")==4);
 /* A prepared old watch batch must be retired before admission when this
    same path changes again. This exercises the actual view's send boundary. */
 uint64_t serial=v->autoView->sentSerial,session=v->highlightSession;xideLanguageViewDiskChanged(&app,helper,2);automaticWatches(v);CHECK(v->syncMode==6);pollSyncPlan(&app);CHECK(v->syncSubmitted);
 uint64_t until=GetTickCount64()+10000;while(!v->syncReady && GetTickCount64()<until){v->syncReady=xideSyncPlannerTake(v->syncPlanner);if(!v->syncReady)Sleep(1);}CHECK(v->syncReady && v->syncReady->valid && v->syncReady->workerThreadId!=xrtThreadCurrentId());
 xideLanguageViewDiskChanged(&app,helper,2);pollSyncPlan(&app);CHECK(!v->syncMode && v->autoView->sentSerial==serial && v->highlightSession==session && v->client==original);CHECK(waitBackground(&app,2,0));
 CHECK(v->autoView->sentSerial==serial+1 && v->client==original);
 uint64_t revision=xideFileSyncRevision(&app);int64 completed=count(get(get(statsReply,"result"),"background"),"completed");CHECK(writeText(helper,changed) && waitBackground(&app,completed+1,0));CHECK(xideFileSyncRevision(&app)>revision && !app.documents.count && v->client==original && count(get(get(statsReply,"result"),"background"),"captured")==2 && count(get(get(statsReply,"result"),"background"),"reused")==2);
 CHECK(search(&app,"Payload amount",2,0));completed=count(stats(&app),"completed");CHECK(completed>=0 && writeText(helper,library) && waitBackground(&app,completed+1,0));CHECK(xideLanguageViewWorkspaceSymbolsIsOpen(&app) && !xideLanguageViewWorkspaceSymbolCount(&app) && search(&app,"Payload number",2,0));workspaceClose(NULL,&app);
 completed=count(stats(&app),"completed");int64 failed=count(get(get(statsReply,"result"),"background"),"failed");CHECK(completed>=0 && failed>=0 && xrtFileDelete(orphan) && waitBackground(&app,completed,failed+1));CHECK(search(&app,"orphanWork",0,1));workspaceClose(NULL,&app);int64 started=count(stats(&app),"started");CHECK(quiet(&app,700) && count(stats(&app),"started")==started);
 CHECK(writeText(orphan,unopened) && waitBackground(&app,completed+1,failed+1) && search(&app,"orphanWork",2,0));workspaceClose(NULL,&app);
 CHECK(xideEditorOpen(&app,helper,&error) && waitFiles(&app) && waitBackground(&app,0,failed+1));xide_document* document=app.activeDocument;completed=count(stats(&app),"completed");CHECK(completed>=0 && xuiCodeDocumentReplace(xuiCodeEditGetDocument(document->editor),0,xuiCodeDocumentGetLength(xuiCodeEditGetDocument(document->editor)),dirty)==XUI_OK && waitFiles(&app));
 CHECK(waitBackground(&app,completed+1,failed+1) && search(&app,"Payload edited",2,0) && document->dirty && xuiCodeDocumentCanUndo(xuiCodeEditGetDocument(document->editor)));workspaceClose(NULL,&app);
 CHECK(writeText(helper,changed) && waitFiles(&app));until=GetTickCount64()+10000;while(!document->externalState && GetTickCount64()<until){CHECK(settle(&app));Sleep(5);}CHECK(document->dirty && document->externalState && !strcmp(xuiCodeEditGetText(document->editor),dirty) && xuiCodeDocumentCanUndo(xuiCodeEditGetDocument(document->editor)));CHECK(xideEditorClose(&app,document,1,&error) && writeText(helper,library) && waitFiles(&app) && waitBackground(&app,0,0) && search(&app,"Payload number",2,0));workspaceClose(NULL,&app);
 uint64_t resyncs=v->autoView->resyncs;session=v->highlightSession;
 for(int i=0;i<132;++i){char name[80];snprintf(name,sizeof(name),"monitor-%03d.xl",i);char* path=xidePathResolve(root,name);CHECK(path && writeText(path,"func main(...) as int\n return 0\nend\n"));xideLanguageViewDiskChanged(&app,path,2);free(path);}
 CHECK(v->autoView->restarting && v->autoView->resyncs==resyncs+1 && v->highlightSession==session+1 && !v->client);CHECK(waitBackground(&app,1,0) && !app.activeDocument && count(get(get(statsReply,"result"),"background"),"index.rows")==12);
 /* Clearing every project stops the zero-open main connection. A standalone
    C editor continues to work with the default C-LSP macro disabled. */
 CHECK(xideWorkspaceRemoveProject(&app.workspaceModel,"A",&error) && xideWorkspaceRemoveProject(&app.workspaceModel,"B",&error) && xideWorkspaceViewRefresh(&app)==XUI_OK);xideLanguageViewConfigurationChanged(&app);CHECK(xideEditorOpen(&app,native,&error) && quiet(&app,700) && !v->client && !strcmp(app.activeDocument->language,"c"));
 CHECK(!app.frameError);printf("workspace_lifecycle_view_test passed: %d checks;real zero-open two-project startup/prebuilt index,stale prepared-batch rejection,unchanged connection during watch updates,modal equal-length edits,delete/create failure recovery,dirty overlay/undo preservation,132-path coalesced resync,workspace removal and C isolation.\n",f->checks);
cleanup:
 xrtValueRelease(statsReply);statsReply=NULL;statsClient=NULL;statsId=0;if(began)xgeEnd();xideProjectClear(&project);xideAppDestroy(&app);free(root);free(entry);free(helper);free(orphan);free(native);free(a);free(b);free(workspace);free(settings);xgeQuit();return XGE_OK;
}
static DWORD WINAPI worker(void* p){fixture* f=p;if(!SetThreadDesktop(f->desktop))return 1;xge_desc_t desc={.iWidth=1600,.iHeight=1000,.sTitle="Workspace lifecycle verification",.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI,.iRunMode=XGE_RUN_GAME_LOOP};int code=xgeInit(&desc);if(code==XGE_OK)code=xgeRun(verify,f);xgeUnit();return code==XGE_OK && f->started && !f->failed?0:1;}
int main(int argc,char** argv){if(argc!=3 || !xrtDirCreateAll(argv[1]))return 2;fixture f={.directory=argv[1],.compiler=argv[2]};wchar_t name[100];swprintf(name,100,L"WorkspaceLifecycle-%lu",GetCurrentProcessId());f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop)return 1;HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=1;if(thread){if(WaitForSingleObject(thread,180000)!=WAIT_OBJECT_0)ExitProcess(1);GetExitCodeThread(thread,&code);CloseHandle(thread);}CloseDesktop(f.desktop);return (int)code;}
