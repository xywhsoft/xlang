#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "ui/task_view.h"
#include "ui/goto_view.h"
#include "services/file_sync.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* White-box acceptance checks augment actual server/CodeEdit integration. */
#include "../src/ui/language_view.c"
typedef struct fixture { const char *root,*compiler;HDESK desktop;int started,failed,checks,legacy; } fixture;
#define CHECK(e) do {++f->checks;if(!(e)){fprintf(stderr,"sync_view:%d %s; %s; %s; request=%d decode=%d applied=%zu receipt=%d invalid=%d\n",__LINE__,#e,error.message,xideLanguageViewMessage(&app),app.languageView ? app.languageView->highlightRequest : 0,app.languageView ? app.languageView->highlightDecoding : 0,app.languageView ? app.languageView->highlightApplied : 0,app.languageView ? app.languageView->receiptReady : 0,app.languageView ? app.languageView->invalidated : 0);f->failed=1;goto cleanup;}}while(0)
/* Explicit manual compatibility oracle. Default automatic mode has its own real UI fixture. */
static int writeText(const char* path,const char* text)
{ return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)}); }
static int settle(xide_app* app)
{ if(xuiDispatchPendingEvents(app->context)!=XUI_OK) return 0;xideEditorPoll(app);xideFileSyncPoll(app);xideTaskViewPoll(app);xideLanguageViewPoll(app);xideWorkbenchLayout(app);return xuiLayout(app->context)==XUI_OK && !app->frameError; }
static int point(xide_app* app,xide_document* d,int offset)
{ return xideEditorShow(app,d)==XUI_OK && xuiCodeSelectionGotoOffset(xuiCodeEditGetSelection(d->editor),xuiCodeEditGetDocument(d->editor),offset,0)==XUI_OK && xuiSetFocusWidget(app->context,d->editor)==XUI_OK; }
static int key(xide_app* app,int code)
{ return xuiInputKeyDownEx(app->context,code,0,NULL)==XUI_OK && xuiInputKeyUpEx(app->context,code,0,NULL)==XUI_OK && settle(app); }
static int waitHighlights(xide_app* app,size_t count)
{
 uint64_t until=GetTickCount64()+15000;
 do {if(!settle(app)) return 0;xide_language_view* v=app->languageView;
  if(v->highlightApplied==count && !v->highlightAt && !v->highlightRequest && !v->highlightDecoding && !v->highlightResult && v->highlight.document==app->activeDocument) return 1;
  Sleep(5);
 }while(GetTickCount64()<until);return 0;
}
static int marked(xide_document* d,int offset,int id)
{
 xui_code_indicator_t items[16];int count=0;
 if(xuiCodeAnnotationGetIndicatorsAt(xuiCodeEditGetAnnotations(d->editor),offset,items,16,&count)!=XUI_OK) return -1;
 int result=0;for(int i=0;i<count && i<16;++i) if(items[i].iIndicator==id) ++result;return result;
}
/* Keep a real completed plan owner-held until the test introduces a stale
   model/session. No transport batch has been admitted at this point. */
static int holdPlan(xide_language_view* v)
{
 if(!v->syncMode || v->syncSubmitted || v->syncReady) return 0;
 xide_sync_input inputs[64];for(size_t i=0;i<v->inputCount;++i) inputs[i]=(xide_sync_input){v->inputs[i].uri,v->inputs[i].textValue,v->inputs[i].id,v->inputs[i].version};
 if(xideSyncPlannerSubmit(v->syncPlanner,v->syncToken,v->sync,inputs,v->inputCount)!=1) return 0;
 v->syncSubmitted=1;v->syncDeadline=0;uint64_t until=GetTickCount64()+10000;
 do {v->syncReady=xideSyncPlannerTake(v->syncPlanner);if(v->syncReady) return v->syncReady->valid && v->syncReady->token==v->syncToken;Sleep(1);}while(GetTickCount64()<until);return 0;
}
static int waitChecked(xide_app* app)
{uint64_t until=GetTickCount64()+15000;do {if(!settle(app)) return 0;if(!app->languageView->busy && app->languageView->snapshotResult) return 1;Sleep(5);}while(GetTickCount64()<until);return 0;}
static int capture(xide_app* app,const char* root,const char* name)
{
 char* path=xidePathResolve(root,name);xui_rect_i_t clip={0,0,app->viewportWidth,app->viewportHeight};
 int okay=path && settle(app) && xuiUpdate(app->context,0)==XUI_OK && app->proxy.surfaceClear(&app->proxy,app->target,0xffeeeeeeu)==XUI_OK && xuiRender(app->context,app->target,&clip,1)==XUI_OK && xideAppCapture(app,path)==XGE_OK;free(path);return okay;
}
static int run(void* user)
{
 fixture* f=user;if(f->started) return XGE_OK;f->started=1;xide_app app={0};xide_model_error error={0};int began=0;char *entry=NULL,*native=NULL;xvalue* fake=NULL;xstrbuf large;xrtStrBufInit(&large);

 const char* saved="// initial\nfunc main(...) as int\n int result = 7\n return result\nend\n";
 entry=xidePathResolve(f->root,"sync😀.xl");CHECK(entry && writeText(entry,saved));
 app.hidden=1;app.noSession=1; app.disableAutomaticDiagnostics=1;app.compilerPath=f->compiler;CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK);began=1;
 CHECK(xideEditorOpen(&app,entry,&error));xide_document* xl=app.activeDocument;xui_code_document document=xuiCodeEditGetDocument(xl->editor);xide_language_view* v=app.languageView;
 int declaration=(int)(strstr(saved,"result")-saved),use=(int)(strstr(saved+declaration+6,"result")-saved);
 CHECK(point(&app,xl,declaration) && waitHighlights(&app,2) && capture(&app,f->root,"sync-current.png") && key(&app,XUI_KEY_ESCAPE));
 char* uri=xideLspPathToUri(entry,NULL);CHECK(uri && xlsSyncVersion(v->sync,uri)==1);
 xls_text* original=xlsSyncRetainText(v->sync,uri,1);CHECK(original);
 CHECK(xuiCodeDocumentReplace(document,2,3,"a")==XUI_OK && settle(&app) && prepareSymbols(&app) && v->syncMode==2 && !v->syncSubmitted && !v->receiptReady);
 CHECK(xideActionExecute(&app,XIDE_ACTION_CANCEL_CHECK)==XIDE_ACTION_DISPATCHED && !v->syncMode && !v->busy && !v->receiptReady && !xlsSyncVersion(v->sync,uri));
 CHECK(xuiCodeDocumentReplace(document,2,3,"b")==XUI_OK && settle(&app) && prepareSymbols(&app) && holdPlan(v) && !xlsSyncVersion(v->sync,uri));
 uint64_t token=v->syncToken;CHECK(v->syncReady->token==token && v->syncReady->versions[0]==1);
 CHECK(xuiCodeDocumentReplace(document,2,3,"c")==XUI_OK && settle(&app) && !v->syncMode && !v->receiptReady && !xlsSyncVersion(v->sync,uri) && !v->receiptRequest);
 suppressHighlights(v);CHECK(prepareSymbols(&app) && holdPlan(v));
 uint64_t session=v->highlightSession;xideLanguageViewStop(&app);CHECK(v->highlightSession!=session && !v->client && !v->sync && !v->syncMode && !v->receiptReady);
 uint64_t retireUntil=GetTickCount64()+5000;do {pollSyncPlan(&app);if(v->syncReady) Sleep(1);}while(v->syncReady && GetTickCount64()<retireUntil);CHECK(!v->syncReady);
 CHECK(point(&app,xl,use) && waitHighlights(&app,2) && v->syncSerial>token && v->receiptReady && xlsSyncVersion(v->sync,uri)==1 && marked(xl,declaration,HIGHLIGHT_INDICATOR)==1);
 CHECK(xuiCodeDocumentReplace(document,2,3,"d")==XUI_OK && settle(&app) && xideActionExecute(&app,XIDE_ACTION_CHECK_CURRENT)==XIDE_ACTION_DISPATCHED && v->syncMode==1 && v->busy && !v->requestId);
 CHECK(xideActionExecute(&app,XIDE_ACTION_CANCEL_CHECK)==XIDE_ACTION_DISPATCHED && !v->busy && !v->syncMode && !v->requestId && !v->receiptReady);
 CHECK(xideActionExecute(&app,XIDE_ACTION_CHECK_CURRENT)==XIDE_ACTION_DISPATCHED && waitChecked(&app) && v->snapshotResult && !v->syncMode && !xideLanguageViewDiagnosticCount(&app));
 CHECK(xideActionExecute(&app,XIDE_ACTION_CANCEL_CHECK)==XIDE_ACTION_DISPATCHED && !v->busy);
 CHECK(prepareSymbols(&app) && holdPlan(v));
 CHECK(xideEditorClose(&app,xl,1,&error) && settle(&app) && !v->syncMode && !v->receiptReady && !v->document);
 CHECK(!strcmp(xlsTextData(original),saved));xlsTextRelease(original);free(uri);
 printf("sync_view_test passed: %d native assertions; real input receipt, pre-submit manual/on-demand cancellation, completed stale edit/session/close rejection, reconnect and exact latest-source recovery.\n",f->checks);

cleanup:
 xrtValueRelease(fake);if(began) xgeEnd();xideAppDestroy(&app);xrtStrBufFree(&large);free(entry);free(native);xgeQuit();return XGE_OK;
}
static DWORD WINAPI worker(void* user)
{ fixture* f=user;if(!SetThreadDesktop(f->desktop)) return 1;xge_desc_t d={0};d.iWidth=1600;d.iHeight=1000;d.sTitle="Highlight integration verification";d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI;d.iRunMode=XGE_RUN_GAME_LOOP;int result=xgeInit(&d);if(result==XGE_OK) result=xgeRun(run,f);xgeUnit();return result==XGE_OK && f->started && !f->failed ? 0 : 1; }
int main(int argc,char** argv)
{
 if(argc<3 || argc>4 || (argc==4 && strcmp(argv[3],"legacy")) || !xrtDirCreateAll(argv[1])) return 2;
 fixture f={.root=argv[1],.compiler=argv[2],.legacy=argc==4};wchar_t name[100];swprintf(name,100,L"SyncIntegration-%lu",GetCurrentProcessId());f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop) return 1;
 HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=1;if(thread) {if(WaitForSingleObject(thread,120000)!=WAIT_OBJECT_0) ExitProcess(1);GetExitCodeThread(thread,&code);CloseHandle(thread);}CloseDesktop(f.desktop);return (int)code;
}
