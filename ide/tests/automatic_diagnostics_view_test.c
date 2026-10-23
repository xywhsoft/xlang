#define _WIN32_WINNT 0x0601
#include "app/xide_app.h"
#include "ui/editor_view.h"
#include "ui/task_view.h"
#include "services/file_sync.h"
#include "platform/path.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/ui/language_view.c"
typedef struct fixture {const char *root,*compiler;HDESK desktop;int started,failed,checks,legacy;} fixture;
#define CHECK(e) do{++f->checks;if(!(e)){fprintf(stderr,"automatic_view:%d %s; %s; enabled=%d sync=%d receipt=%d\n",__LINE__,#e,xideLanguageViewMessage(&app),app.languageView ? app.languageView->autoEnabled : 0,app.languageView ? app.languageView->syncMode : 0,app.languageView ? app.languageView->receiptReady : 0);f->failed=1;goto cleanup;}}while(0)
static int writeText(const char* path,const char* text) {return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)});}
static int settle(xide_app* app)
{if(xuiDispatchPendingEvents(app->context)!=XUI_OK) return 0;xideEditorPoll(app);xideFileSyncPoll(app);xideTaskViewPoll(app);xideLanguageViewPoll(app);xideWorkbenchLayout(app);return xuiLayout(app->context)==XUI_OK && !app->frameError;}
static automatic_group* group(xide_app* app,const char* path)
{xide_automatic_diagnostics* a=app->languageView->autoView;wchar_t* key=xidePathNativeWide(path);if(!a || !key) {free(key);return NULL;}size_t at=automaticFind(a,key);int found=at<a->groupCount && !xidePathNativeKeyCompare(key,a->groups[at].notice->key);free(key);return found ? a->groups+at : NULL;}
static int waitGroup(xide_app* app,const char* path,size_t count)
{
 uint64_t until=GetTickCount64()+15000;
 do {if(!settle(app)) return 0;automatic_group* g=group(app,path);
  if(g && g->valid && g->applied && (count==SIZE_MAX ? g->notice->count>0 : g->notice->count==count) && !app->languageView->syncMode && app->languageView->receiptReady) return 1;
  Sleep(2);
 }while(GetTickCount64()<until);
 xide_language_view* v=app->languageView;xide_automatic_diagnostics* a=v->autoView;automatic_group* g=group(app,path);
 fprintf(stderr,"waitGroup failed: %s count=%zu client=%p enabled=%d initialized=%d pause=%d serial=%llu epoch=%llu groups=%zu target=%p\n",path,count,(void*)v->client,v->autoEnabled,v->initialized,a ? a->paused : -1,a ? (unsigned long long)a->sentSerial : 0,a ? (unsigned long long)a->epoch : 0,a ? a->groupCount : 0,(void*)g);
 if(g) fprintf(stderr,"group valid=%d applied=%d count=%zu mapped=%d overlays=%d matched=%d disk=%d serial=%llu bindings=%zu\n",g->valid,g->applied,g->notice->count,g->notice->mapped,g->notice->hasOverlays,g->notice->overlaysMatched,g->notice->diskValidated,(unsigned long long)g->notice->inputSerial,g->notice->bindingCount);
 return 0;
}
static int marks(xide_document* d) {return xuiCodeAnnotationGetDiagnosticCount(xuiCodeEditGetAnnotations(d->editor));}
static int capture(xide_app* app,const char* root,const char* name)
{char* path=xidePathResolve(root,name);xui_rect_i_t clip={0,0,app->viewportWidth,app->viewportHeight};int okay=path && settle(app) && xuiUpdate(app->context,0)==XUI_OK && app->proxy.surfaceClear(&app->proxy,app->target,0xffeeeeeeu)==XUI_OK && xuiRender(app->context,app->target,&clip,1)==XUI_OK && xideAppCapture(app,path)==XGE_OK;free(path);return okay;}
typedef struct stream_gate {HANDLE entered,release;LONG done;} stream_gate;
static void blockWorker(void* value) {stream_gate* gate=value;SetEvent(gate->entered);WaitForSingleObject(gate->release,15000);InterlockedExchange(&gate->done,1);}
static int injectOwned(xvalue* value,size_t bytes,void* user) {return automaticReceive(user,value,bytes);}
static int inertJump(xide_app* app,size_t row,void* user) {(void)app;(void)row;(void)user;return 0;}
static int waitLegacy(xide_app* app,int count)
{uint64_t until=GetTickCount64()+15000;do {if(!settle(app)) return 0;xide_language_view* v=app->languageView;if(count<0 ? v->initialized && !v->waitingInitialize : !v->busy && v->snapshotResult && xideLanguageViewDiagnosticCount(app)==(size_t)count) return 1;Sleep(2);}while(GetTickCount64()<until);return 0;}
static int run(void* user)
{
 fixture* f=user;if(f->started) return XGE_OK;f->started=1;xide_app app={0};xide_model_error error={0};int began=0;char *entry=NULL,*side=NULL,*entryUri=NULL,*importer=NULL,*shared=NULL,*negative=NULL,*optional=NULL,*cfile=NULL;char* frame=NULL;stream_gate gate={0};xide_lsp_stream injection={0};xvalue* packet=NULL;
 const char *bad="func main(...) as int\n return absentName\nend\n",*good="func main(...) as int\n return 0\nend\n",*other="func main(...) as int\n return otherMissing\nend\n";
 entry=xidePathResolve(f->root,"auto😀.xl");side=xidePathResolve(f->root,"independent.xl");CHECK(entry && side && writeText(entry,bad) && writeText(side,good));
 app.hidden=1;app.noSession=1;app.compilerPath=f->compiler;CHECK(xideAppCreate(&app)==XUI_OK && xgeBegin()==XGE_OK);began=1;
 CHECK(xideEditorOpen(&app,entry,&error));xide_document* a=app.activeDocument;
 if(f->legacy) {
  CHECK(waitLegacy(&app,-1) && !app.languageView->autoEnabled && !app.languageView->autoView->groupCount);
  CHECK(xideActionExecute(&app,XIDE_ACTION_CHECK_CURRENT)==XIDE_ACTION_DISPATCHED && waitLegacy(&app,1) && marks(a)==1);
  CHECK(xuiCodeEditSetText(a->editor,good)==XUI_OK && waitLegacy(&app,0) && !marks(a) && capture(&app,f->root,"manual-fallback.png"));
  printf("automatic_diagnostics_view_test passed: %d checks; unsupported conditional auto capability stays OFF, actual explicit manual diagnostic fallback and dirty recheck remain usable.\n",f->checks);goto cleanup;
 }
 CHECK(waitGroup(&app,entry,1) && app.languageView->autoEnabled && marks(a)==1 && xideLanguageViewDiagnosticCount(&app)==1);
 CHECK(group(&app,entry)->notice->hasSerial && group(&app,entry)->notice->overlaysMatched && capture(&app,f->root,"automatic-open.png"));
 CHECK(xideEditorOpen(&app,side,&error));xide_document* b=app.activeDocument;CHECK(waitGroup(&app,side,0) && waitGroup(&app,entry,1));
 uint64_t independent=group(&app,entry)->token;
 CHECK(xuiCodeEditSetText(b->editor,other)==XUI_OK && waitGroup(&app,side,1) && group(&app,entry)->valid && group(&app,entry)->token==independent && marks(a)==1 && marks(b)==1 && xideLanguageViewDiagnosticCount(&app)==2);
 CHECK(xuiCodeEditSetText(a->editor,good)==XUI_OK && waitGroup(&app,entry,0) && group(&app,side)->valid && marks(a)==0 && marks(b)==1 && xideLanguageViewDiagnosticCount(&app)==1);
 CHECK(xideEditorShow(&app,a)==XUI_OK && xideActionExecute(&app,XIDE_ACTION_CHECK_CURRENT)==XIDE_ACTION_DISPATCHED);
 uint64_t until=GetTickCount64()+15000;do {CHECK(settle(&app));if(!app.languageView->busy) break;Sleep(2);}while(GetTickCount64()<until);
 CHECK(!app.languageView->busy && waitGroup(&app,entry,0) && marks(b)==1);
 CHECK(xideActionExecute(&app,XIDE_ACTION_CANCEL_CHECK)==XIDE_ACTION_DISPATCHED && !app.languageView->client && !app.languageView->autoEnabled);
 for(int i=0;i<10;++i) {CHECK(settle(&app));Sleep(2);}CHECK(!app.languageView->client && !marks(a) && !marks(b));
 CHECK(xuiCodeEditSetText(a->editor,bad)==XUI_OK && waitGroup(&app,entry,1) && waitGroup(&app,side,1));
 entryUri=xideLspPathToUri(entry,NULL);CHECK(entryUri);int previousWire=xlsSyncVersion(app.languageView->sync,entryUri);uint64_t oldToken=group(&app,entry)->token;
 CHECK(xideEditorClose(&app,a,1,&error));for(int i=0;i<20;++i) {CHECK(settle(&app) && group(&app,side) && group(&app,side)->valid && marks(b)==1);Sleep(2);}
 CHECK(xideEditorOpen(&app,entry,&error) && waitGroup(&app,entry,1) && waitGroup(&app,side,1));
 CHECK(xlsSyncVersion(app.languageView->sync,entryUri)>previousWire && !xideLanguageViewJump(&app,AUTO_ROUTE_BASE+(size_t)oldToken*4096u) && capture(&app,f->root,"automatic-reopen.png"));
 CHECK(xideLanguageViewJump(&app,AUTO_ROUTE_BASE+(size_t)group(&app,entry)->token*4096u) && app.activeDocument==a);

 importer=xidePathResolve(f->root,"imports.xl");shared=xidePathResolve(f->root,"shared.xl");negative=xidePathResolve(f->root,"negative.xl");optional=xidePathResolve(f->root,"optional_mod.xl");cfile=xidePathResolve(f->root,"foreign.c");
 const char* imported="import shared\nfunc main(...) as int\n return shared.value()\nend\n";
 const char* sharedBad="export func value() as int\n return \"bad\"\nend\n";const char* sharedGood="export func value() as int\n return 7\nend\n";
 CHECK(importer && shared && negative && optional && cfile && writeText(importer,imported) && writeText(shared,sharedBad));
 CHECK(xideEditorOpen(&app,importer,&error) && waitGroup(&app,shared,1) && !group(&app,shared)->notice->overlay && group(&app,shared)->notice->diskValidated);
 size_t diskToken=AUTO_ROUTE_BASE+(size_t)group(&app,shared)->token*4096u;CHECK(writeText(shared,sharedBad) && !xideLanguageViewJump(&app,diskToken));
 CHECK(waitGroup(&app,shared,1) && group(&app,shared)->token!=(diskToken-AUTO_ROUTE_BASE)/4096u);
 CHECK(xideLanguageViewJump(&app,AUTO_ROUTE_BASE+(size_t)group(&app,shared)->token*4096u) && xidePathLexicalEqual(app.activeDocument->path,shared));xide_document* dependency=app.activeDocument;
 CHECK(waitGroup(&app,shared,1) && group(&app,shared)->notice->overlay && xuiCodeEditSetText(dependency->editor,sharedGood)==XUI_OK && waitGroup(&app,shared,0) && waitGroup(&app,importer,0) && marks(b)==1);
 const char* missing="import optional_mod\nfunc main(...) as int\n return optional_mod.value()\nend\n";
 CHECK(writeText(negative,missing) && xideEditorOpen(&app,negative,&error) && waitGroup(&app,negative,SIZE_MAX));
 CHECK(writeText(optional,sharedGood) && waitGroup(&app,negative,0));

 CHECK(writeText(cfile,"int main(void) { return 0; }\n") && xideEditorOpen(&app,cfile,&error));xide_document* foreign=app.activeDocument;
 xui_code_diagnostic_t cmark={.iSize=sizeof(cmark),.tRange={0,3},.iSeverity=XUI_CODE_DIAGNOSTIC_WARNING,.sMessage="foreign diagnostic",.sSource="clangd",.iUserData=17};
 CHECK(xuiCodeAnnotationSetDiagnostics(xuiCodeEditGetAnnotations(foreign->editor),&cmark,1)==XUI_OK);int foreignOwner=0;const char* titles[]={"retained C problem"};
 CHECK(xideTaskViewShowLanguageProblemsRouted(&app,"C analysis",titles,1,0,&foreignOwner,inertJump)==XUI_OK);
 CHECK(xideEditorShow(&app,b)==XUI_OK && xuiCodeEditSetText(b->editor,good)==XUI_OK && waitGroup(&app,side,0) && marks(foreign)==1 && xideTaskViewLanguageProblemsOwner(&app)==&foreignOwner);
 CHECK(xuiCodeEditSetText(b->editor,other)==XUI_OK && waitGroup(&app,side,1));

 /* Gate the REAL background stream and fill its two job slots, pending batch
  * and one held root. Each injected message passes actual framing/owned JSON
  * parsing; no transport/root lifetime shortcuts or per-file workers. */
 gate.entered=CreateEventW(NULL,TRUE,FALSE,NULL);gate.release=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(gate.entered && gate.release);
 uint64_t gateUntil=GetTickCount64()+10000;int submitted=0;
 do {submitted=xideDiagnosticStreamRetire(app.languageView->autoView->stream,&gate,blockWorker);if(!submitted) {CHECK(settle(&app));Sleep(1);}}while(!submitted && GetTickCount64()<gateUntil);
 CHECK(submitted && WaitForSingleObject(gate.entered,10000)==WAIT_OBJECT_0);
 packet=xrtValueObject();CHECK(packet && put(packet,"jsonrpc",xrtValueString(sv("2.0"))) && put(packet,"method",xrtValueString(sv("textDocument/publishDiagnostics"))) && put(packet,"params",xrtValueDeepClone(group(&app,side)->notice->params)));
 size_t frameBytes=0;frame=xideLspFrame(packet,&frameBytes,&error);xrtValueRelease(packet);packet=NULL;CHECK(frame);
 for(size_t i=0;i<385;++i) CHECK(xideLspStreamFeedOwned(&injection,frame,frameBytes,injectOwned,app.languageView,&error));
 xide_diagnostic_stream_stats load;CHECK(xideLspStreamEnd(&injection,&error) && xideDiagnosticStreamLoad(app.languageView->autoView->stream,&load) && load.outstanding==2 && app.languageView->autoView->held && xideDiagnosticBatchCount(app.languageView->autoView->batch)==128);
 CHECK(xuiCodeEditSetText(b->editor,good)==XUI_OK && settle(&app) && !marks(b) && marks(a)==1 && marks(foreign)==1 && !InterlockedCompareExchange(&gate.done,0,0));
 SetEvent(gate.release);CHECK(waitGroup(&app,side,0) && waitGroup(&app,entry,1) && marks(foreign)==1 && xideTaskViewLanguageProblemsOwner(&app)==&foreignOwner);
 free(frame);frame=NULL;xideLspStreamClear(&injection);

 /* A malformed source serial must poison/pause the connection and remove
  * owned ranges; a foreign provider's annotations and problem owner survive. */
 packet=xrtValueObject();xvalue* broken=xrtValueDeepClone(group(&app,entry)->notice->params);CHECK(packet && broken && put(get(broken,"xlang"),"serial",xrtValueString(sv("01"))) && put(packet,"jsonrpc",xrtValueString(sv("2.0"))) && put(packet,"method",xrtValueString(sv("textDocument/publishDiagnostics"))) && put(packet,"params",broken));
 frame=xideLspFrame(packet,&frameBytes,&error);xrtValueRelease(packet);packet=NULL;CHECK(frame && xideLspStreamFeedOwned(&injection,frame,frameBytes,injectOwned,app.languageView,&error));
 uint64_t badUntil=GetTickCount64()+10000;do {CHECK(settle(&app));if(app.languageView->autoView->paused) break;Sleep(1);}while(GetTickCount64()<badUntil);
 CHECK(app.languageView->autoView->paused && !app.languageView->client && !marks(a) && marks(foreign)==1 && xideTaskViewLanguageProblemsOwner(&app)==&foreignOwner);
 CHECK(xuiCodeEditSetText(a->editor,good)==XUI_OK && waitGroup(&app,entry,0) && waitGroup(&app,side,0));
 CHECK(capture(&app,f->root,"automatic-dependencies.png"));
 printf("automatic_diagnostics_view_test passed: %d checks; default open/edit, dependency/disk proof and identity jump, missing candidate recovery, unrelated groups, explicit checks, pause/resume, close/reopen routes, foreign provider ownership, 385 framed messages under gated backpressure, typing while blocked, stale-source drain and malformed serial isolation; max-owner-observed=%.6fms (functional fixture, not a P95 benchmark).\n",f->checks,app.languageView->autoView->maxOwnerMs);
cleanup:
 if(gate.release) {SetEvent(gate.release);}if(began) {xgeEnd();}xideAppDestroy(&app);if(gate.entered) {CloseHandle(gate.entered);}if(gate.release) {CloseHandle(gate.release);}free(frame);xrtValueRelease(packet);xideLspStreamClear(&injection);free(entry);free(side);free(entryUri);free(importer);free(shared);free(negative);free(optional);free(cfile);xgeQuit();return XGE_OK;
}
static DWORD WINAPI worker(void* user)
{fixture* f=user;if(!SetThreadDesktop(f->desktop)) return 1;xge_desc_t d={0};d.iWidth=1600;d.iHeight=1000;d.sTitle="Default automatic diagnostic integration";d.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI;d.iRunMode=XGE_RUN_GAME_LOOP;int result=xgeInit(&d);if(result==XGE_OK) result=xgeRun(run,f);xgeUnit();return result==XGE_OK && f->started && !f->failed ? 0 : 1;}
int main(int argc,char** argv)
{
 if((argc!=3 && !(argc==4 && !strcmp(argv[3],"legacy"))) || !xrtDirCreateAll(argv[1])) {return 2;}fixture f={.root=argv[1],.compiler=argv[2],.legacy=argc==4};wchar_t name[100];swprintf(name,100,L"AutomaticDiagnostic-%lu",GetCurrentProcessId());f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);if(!f.desktop) return 1;
 HANDLE thread=CreateThread(NULL,0,worker,&f,0,NULL);DWORD code=1;if(thread) {if(WaitForSingleObject(thread,120000)!=WAIT_OBJECT_0) ExitProcess(1);GetExitCodeThread(thread,&code);CloseHandle(thread);}CloseDesktop(f.desktop);return (int)code;
}
