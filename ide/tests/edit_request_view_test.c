#define _WIN32_WINNT 0x0601
#include "ui/language_view.c"
#include "ui/completion_view.h"
#include "ui/editor_view.h"
#include "ui/workspace_view.h"
#include "services/source_barrier.h"
#include "services/execution_target.h"
#include "catalog_ready.h"
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"edit-request-view:%d:%s\n",__LINE__,#v);ExitProcess(1);}}while(0)
typedef struct fixture {const char* directory;const char* compiler;HDESK desktop;int started;} fixture;
static int frame(xide_app* a){return xuiDispatchPendingEvents(a->context)==XUI_OK&&(xideEditorPoll(a),xideFileSyncPoll(a),xideTaskViewPoll(a),xideLanguageViewPoll(a),xideCompletionViewPoll(a),xideWorkbenchLayout(a),xuiLayout(a->context)==XUI_OK)&&!a->frameError;}
static xide_edit_request* capture(xide_app* a,xide_document* doc)
{
 ULONGLONG until=GetTickCount64()+20000;xide_model_error e={0};
 do{if(!frame(a))return NULL;xide_language_view* v=a->languageView;
  if(v->sourceToken||v->editRequest)return NULL;
  if(!v->syncMode&&!v->receiptRequest&&!v->receiptReady)prepareSymbolsDocument(a,doc);
  xide_edit_request* p=xideLanguageEditRequestCapture(a,&e);if(p)return p;Sleep(2);
 }while(GetTickCount64()<until);xide_language_view* v=a->languageView;fprintf(stderr,"capture: %s; %s; initialized=%d native=%d ondemand=%d receipt=%d request=%d mode=%d source=%llu drain=%d busy=%d current=%d adopted=%d serial=%llu/%llu watches=%d\n",e.message,v->message,v->initialized,v->nativeBackend,v->onDemandSupported,v->receiptReady,v->receiptRequest,v->syncMode,(unsigned long long)v->sourceToken,v->sourceDrain,v->busy,snapshotCurrent(v),v->autoView->syncAdopted,(unsigned long long)v->autoView->receiptSerial,(unsigned long long)v->autoView->sentSerial,automaticWatchPending(v));for(size_t i=0;i<v->inputCount;++i){xls_sync_binding b={0};xlsSyncBindingAt(v->sync,i,&b);fprintf(stderr,"binding %zu: id=%llu/%llu rev=%llu/%llu wire=%d/%d\n",i,(unsigned long long)b.identity,(unsigned long long)v->inputs[i].id,(unsigned long long)b.revision,(unsigned long long)v->inputs[i].revision,b.version,v->inputs[i].wireVersion);}return NULL;
}
static void waitBarrier(xide_app* a,xide_source_barrier* b,uint64_t t,xide_source_barrier_status* s)
{ULONGLONG until=GetTickCount64()+20000;do{CHECK(frame(a)&&xideSourceBarrierState(b,t,s));if(s->quiet)return;CHECK(GetTickCount64()<until);Sleep(2);}while(1);}
static void releaseBarrier(xide_app* a,xide_source_barrier* b,uint64_t t)
{xide_model_error e={0};CHECK(xideSourceBarrierResume(b,t,&e));ULONGLONG until=GetTickCount64()+20000;int done;do{CHECK(frame(a));done=xideSourceBarrierRelease(b,t,&e);CHECK(done>=0&&GetTickCount64()<until);if(!done)Sleep(2);}while(!done);CHECK(!b->request);}
static void end(xide_app* a,xide_edit_request* p){CHECK(xideLanguageEditRequestEnd(a,p)&&!xideLanguageEditRequestEnd(a,p)&&xideEditRequestInvalidated(p));xideEditRequestRelease(p);}
static int key(xide_app* a,int code,unsigned mods){return xuiInputKeyDown(a->context,code,mods)==XUI_OK&&xuiInputKeyUp(a->context,code,mods)==XUI_OK&&frame(a);}
static int ime(xide_app* a,const char* text,int active){xui_ime_composition_t c={.iSize=sizeof(c),.sText=text,.iTextSize=(int)strlen(text),.bActive=active};c.iCursor=c.iSelectionStart=c.iSelectionEnd=c.iTextSize;return xuiInputImeCompositionEx(a->context,&c)==XUI_OK&&frame(a);}
static void input(xide_app* a,xide_document* d,const char* original)
{
 CHECK(xideEditorShow(a,d)==XUI_OK&&frame(a));xui_rect_t r=xuiWidgetGetWorldRect(d->editor);float x=r.fX+120,y=r.fY+12;
 CHECK(xuiInputPointerMove(a->context,x,y,0)==XUI_OK&&xuiInputPointerDown(a->context,x,y,1,1)==XUI_OK&&xuiInputPointerUp(a->context,x,y,1,0)==XUI_OK&&frame(a)&&xuiGetFocusWidget(a->context)==d->editor);
 xui_code_document doc=xuiCodeEditGetDocument(d->editor);int n=xuiCodeDocumentGetLength(doc);CHECK(xuiCodeSelectionSetRange(xuiCodeEditGetSelection(d->editor),doc,n,n)==XUI_OK&&xuiInputKeyDown(a->context,'Z',0)==XUI_OK&&xuiInputText(a->context,'z')==XUI_OK&&xuiInputKeyUp(a->context,'Z',0)==XUI_OK&&frame(a));
 CHECK(strstr(xuiCodeDocumentGetText(doc),"z"));CHECK(key(a,XUI_KEY_BACKSPACE,0));if(strcmp(xuiCodeDocumentGetText(doc),original))fprintf(stderr,"after backspace expected=[%s] actual=[%s]\n",original,xuiCodeDocumentGetText(doc));CHECK(!strcmp(xuiCodeDocumentGetText(doc),original));CHECK(key(a,'Z',XUI_MOD_CTRL)&&strstr(xuiCodeDocumentGetText(doc),"z")&&key(a,'Z',XUI_MOD_CTRL)&&!strcmp(xuiCodeDocumentGetText(doc),original));
 CHECK(ime(a,"中文😀",1)&&ime(a,"中文😀",0)&&strstr(xuiCodeDocumentGetText(doc),"中文😀")&&key(a,'Z',XUI_MOD_CTRL)&&!strcmp(xuiCodeDocumentGetText(doc),original));
}
typedef struct retired {xide_edit_request* request;int okay;} retired;
static DWORD WINAPI retire(void* raw)
{retired* r=raw;xls_sync_binding b={0};const xls_sync* sync=xideEditRequestSync(r->request);r->okay=xlsSyncBindingAt(sync,0,&b)&&b.identity&&b.revision&&xideEditRequestCatalog(r->request);xls_text* text=r->okay?xlsSyncRetainText(sync,b.uri,b.version):NULL;r->okay=r->okay&&text&&strstr(xlsTextData(text),"func main");xlsTextRelease(text);xideEditRequestRelease(r->request);return r->okay?0:1;}
static int verify(void* raw)
{
 fixture* f=raw;if(f->started)return XGE_OK;f->started=1;xide_app app={.hidden=1,.noSession=1,.compilerPath=f->compiler};xide_model_error error={0};xide_source_barrier barrier={0};xide_source_barrier_status state={0};
 char *entry=xidePathResolve(f->directory,"main.xl"),*native=xidePathResolve(f->directory,"other.c"),*settings=xidePathResolve(f->directory,"absent-settings.json"),*session=xidePathResolve(f->directory,"session"),*diagnostics=xidePathResolve(f->directory,"diagnostics");
 const char* xlBody="func main(...) as int\n return 0\nend\n";const char* cBody="int main(void) { return 0; }\n";CHECK(entry&&native&&settings&&session&&diagnostics&&xrtFileWriteAtomic(entry,(xbytesview){(const unsigned char*)xlBody,strlen(xlBody)})&&xrtFileWriteAtomic(native,(xbytesview){(const unsigned char*)cBody,strlen(cBody)}));
 app.settingsPath=settings;app.sessionRoot=session;app.diagnosticRoot=diagnostics;CHECK(xideAppCreate(&app)==XUI_OK&&xgeBegin()==XGE_OK&&xideTestCatalogReady(&app));CHECK(!xideLanguageEditRequestCapture(&app,&error));
 CHECK(xideWorkspaceCreate(&app.workspaceModel,"Request scope",&error));app.workspaceModel.projects=calloc(2,sizeof(xide_project));CHECK(app.workspaceModel.projects);app.workspaceModel.projectCount=app.workspaceModel.projectCapacity=2;
 for(int i=0;i<2;++i){char* manifest=xidePathResolve(f->directory,i?"B.xlp":"A.xlp");xide_project* p=app.workspaceModel.projects+i;CHECK(manifest&&xideProjectCreate(p,manifest,i?"B":"A","main.xl",&error));p->id=copy(i?"B":"A");CHECK(p->id);xvalue* configs=xrtJsonParse(xrtStrView("[{\"id\":\"development\"},{\"id\":\"release\"}]"));CHECK(configs&&xideProjectApplyConfigurations(p,configs,"development",&error));xrtValueRelease(configs);free(manifest);}
 CHECK(xideWorkspaceViewRefresh(&app)==XUI_OK&&xideWorkspaceViewSelectProject(&app,"A"));CHECK(xideEditorOpen(&app,native,&error));xide_document* c=app.activeDocument;CHECK(xideEditorOpen(&app,entry,&error));xide_document* d=app.activeDocument;
 xide_edit_request* p=capture(&app,d);CHECK(p&&xideLanguageEditRequestCurrent(&app,p)&&xideLanguageEditRequestOwned(&app,p));CHECK(xideEditRequestId(p)==app.languageView->nextId&&xideEditRequestToken(p)==app.languageView->editSerial&&!xideLanguageEditRequestCapture(&app,&error));
 xide_edit_request* foreign=xideEditRequestCreate(99,99,xideEditRequestSync(p),(xvalue*)xideEditRequestCatalog(p),0,NULL,NULL,&error);CHECK(foreign&&!xideLanguageEditRequestEnd(&app,foreign)&&!xideSourceBarrierBeginEdit(&barrier,&app,foreign,&error));xideEditRequestRelease(foreign);
 /* Display metadata/caret changes keep the exact semantic scope and client. */
 xide_language_client* client=app.languageView->client;CHECK(xideProjectSetMetadata(app.workspaceModel.projects,"Renamed display","Description","exe",&error)&&xideWorkspaceViewRefresh(&app)==XUI_OK&&xideLanguageEditRequestCurrent(&app,p)&&app.languageView->client==client);
 uint64_t token=xideSourceBarrierBeginEdit(&barrier,&app,p,&error);CHECK(token&&barrier.request==p);waitBarrier(&app,&barrier,token,&state);CHECK(state.ready&&!state.invalidated&&xideLanguageEditRequestCurrent(&app,p));
 xideEditRequestInvalidate(p,XIDE_EDIT_REQUEST_CANCELLED);CHECK(xideSourceBarrierState(&barrier,token,&state)&&state.quiet&&!state.ready&&state.invalidated);releaseBarrier(&app,&barrier,token);end(&app,p);
 p=capture(&app,d);CHECK(p);token=xideSourceBarrierBeginEdit(&barrier,&app,p,&error);CHECK(token);waitBarrier(&app,&barrier,token,&state);CHECK(state.ready&&xideLanguageEditRequestEnd(&app,p)&&!xideLanguageEditRequestCapture(&app,&error));CHECK(xideSourceBarrierState(&barrier,token,&state)&&state.quiet&&state.invalidated&&!state.ready);releaseBarrier(&app,&barrier,token);xideEditRequestRelease(p);
 /* All editor input stays usable while the request's barrier is held. Undo
  * restores bytes, never the old request identity. */
 xide_document* docs[]={c,d};const char* bodies[]={cBody,xlBody};
 for(int i=0;i<2;++i){p=capture(&app,d);CHECK(p);token=xideSourceBarrierBeginEdit(&barrier,&app,p,&error);CHECK(token);waitBarrier(&app,&barrier,token,&state);CHECK(state.ready);input(&app,docs[i],bodies[i]);CHECK((xideEditRequestInvalidated(p)&XIDE_EDIT_REQUEST_DOCUMENT)&&!xideLanguageEditRequestCurrent(&app,p)&&xideSourceBarrierState(&barrier,token,&state)&&state.quiet&&state.invalidated&&!state.ready);releaseBarrier(&app,&barrier,token);end(&app,p);}
 /* A suppressed model callback still advances revision; the explicit full
  * Current fence detects it before admission/publication. */
 p=capture(&app,d);CHECK(p);end(&app,p);xide_language_view* view=app.languageView;uint64_t oldReceipt=view->autoView->receiptSerial;
 CHECK(view->receiptReady&&automaticFence(view,entry,0,1,2));automaticWatches(view);CHECK(view->syncMode==6);ULONGLONG deadline=GetTickCount64()+10000;
 while(view->syncMode){pollSyncPlan(&app);CHECK(GetTickCount64()<deadline);Sleep(2);}CHECK(!view->receiptReady&&!view->receiptRequest&&view->autoView->sentSerial>oldReceipt);
 p=capture(&app,d);CHECK(p&&view->receiptReady&&view->autoView->receiptSerial==view->autoView->sentSerial);d->suppressChange=1;xui_code_document doc=xuiCodeEditGetDocument(d->editor);CHECK(xuiCodeDocumentReplace(doc,0,0," ")==XUI_OK&&xideLanguageEditRequestOwned(&app,p));d->suppressChange=0;CHECK(!xideLanguageEditRequestCurrent(&app,p)&&xuiCodeDocumentUndo(doc)==XUI_OK&&!strcmp(xuiCodeDocumentGetText(doc),xlBody)&&!xideLanguageEditRequestCurrent(&app,p));end(&app,p);
 p=capture(&app,d);CHECK(p&&xideWorkspaceViewSelectProject(&app,"B")&&xideWorkspaceViewSelectProject(&app,"A")&&(xideEditRequestInvalidated(p)&XIDE_EDIT_REQUEST_CONFIGURATION)&&!xideLanguageEditRequestCurrent(&app,p));end(&app,p);
 p=capture(&app,d);CHECK(p&&xideWorkspaceViewSelectConfiguration(&app,"release",&error)&&xideWorkspaceViewSelectConfiguration(&app,"development",&error)&&(xideEditRequestInvalidated(p)&XIDE_EDIT_REQUEST_CONFIGURATION)&&!xideLanguageEditRequestCurrent(&app,p));end(&app,p);
 p=capture(&app,d);CHECK(p);xideLanguageViewDiskChanged(&app,entry,2);CHECK((xideEditRequestInvalidated(p)&XIDE_EDIT_REQUEST_DISK)&&!xideSourceBarrierBeginEdit(&barrier,&app,p,&error));end(&app,p);
 /* Explicit validation also covers direct host/environment changes before an
  * ordinary notification or frame. Restoring the value cannot revive p. */
 p=capture(&app,d);CHECK(p);char* old=app.workspaceModel.projects[0].entry;app.workspaceModel.projects[0].entry=xidePathResolve(f->directory,"different-entry.xl");CHECK(app.workspaceModel.projects[0].entry&&!xideLanguageEditRequestCurrent(&app,p));free(app.workspaceModel.projects[0].entry);app.workspaceModel.projects[0].entry=old;CHECK(!xideLanguageEditRequestCurrent(&app,p));end(&app,p);
 p=capture(&app,d);CHECK(p);wchar_t previous[32768];SetLastError(ERROR_SUCCESS);DWORD length=GetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",previous,32768);int present=length||GetLastError()==ERROR_SUCCESS;CHECK(length<32768&&SetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",L"edit-request-other-package"));int rejected=!xideLanguageEditRequestCurrent(&app,p);CHECK(SetEnvironmentVariableW(L"XLANG_PACKAGE_PATH",present?previous:NULL)&&rejected&&!xideLanguageEditRequestCurrent(&app,p));end(&app,p);
 p=capture(&app,d);CHECK(p);uint64_t held=xideTasksSuspend(app.tasks,&error);CHECK(held);token=xideSourceBarrierBeginEdit(&barrier,&app,p,&error);CHECK(token&&barrier.request==p&&xideSourceBarrierState(&barrier,token,&state)&&state.failed&&!state.ready);releaseBarrier(&app,&barrier,token);CHECK(xideTasksSourceLeaseCurrent(app.tasks,held)&&xideTasksResume(app.tasks,held,&error));end(&app,p);
 p=capture(&app,d);CHECK(p);xideLanguageViewSaved(&app,d,1);CHECK(xideEditRequestInvalidated(p)&XIDE_EDIT_REQUEST_DOCUMENT);end(&app,p);
 p=capture(&app,d);CHECK(p);xideLanguageViewStop(&app);CHECK((xideEditRequestInvalidated(p)&XIDE_EDIT_REQUEST_CONNECTION)&&!xideLanguageEditRequestCurrent(&app,p));end(&app,p);
 p=capture(&app,d);CHECK(p);token=xideEditRequestToken(p);end(&app,p);p=capture(&app,d);CHECK(p&&xideEditRequestToken(p)>token);end(&app,p);
 uint64_t serial=app.languageView->editSerial;app.languageView->editSerial=UINT64_MAX;CHECK(!xideLanguageEditRequestCapture(&app,&error));app.languageView->editSerial=serial;int id=app.languageView->nextId;app.languageView->nextId=INT_MAX;CHECK(!xideLanguageEditRequestCapture(&app,&error));app.languageView->nextId=id;
 /* Final teardown invalidates the caller's independent immutable lease. Its
  * exact text/catalog remain readable and are retired on a real worker. */
 p=capture(&app,d);CHECK(p);xgeEnd();xideAppDestroy(&app);CHECK((xideEditRequestInvalidated(p)&XIDE_EDIT_REQUEST_ENDED)&&!xideLanguageEditRequestCurrent(&app,p));retired retiredScope={p,0};HANDLE worker=CreateThread(NULL,0,retire,&retiredScope,0,NULL);CHECK(worker&&WaitForSingleObject(worker,10000)==WAIT_OBJECT_0);DWORD code=1;CHECK(GetExitCodeThread(worker,&code)&&!code&&retiredScope.okay);CloseHandle(worker);
 free(entry);free(native);free(settings);free(session);free(diagnostics);xgeQuit();return XGE_OK;
}
static DWORD WINAPI gui(void* raw){fixture* f=raw;if(!SetThreadDesktop(f->desktop))return 1;xge_desc_t d={.iWidth=1280,.iHeight=800,.sTitle="Edit request scope",.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI,.iRunMode=XGE_RUN_GAME_LOOP};int result=xgeInit(&d);if(result==XGE_OK)result=xgeRun(verify,f);xgeUnit();return result==XGE_OK&&f->started?0:1;}
int main(int argc,char** argv){if(argc!=3||!xideIoCreateDirectories(argv[1]))return 2;wchar_t name[80];swprintf(name,80,L"EditRequest-%lu",GetCurrentProcessId());fixture f={.directory=argv[1],.compiler=argv[2]};f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);CHECK(f.desktop);HANDLE t=CreateThread(NULL,0,gui,&f,0,NULL);CHECK(t&&WaitForSingleObject(t,240000)==WAIT_OBJECT_0);DWORD code=1;CHECK(GetExitCodeThread(t,&code)&&!code);CloseHandle(t);if(!CloseDesktop(f.desktop))CHECK(GetLastError()==ERROR_BUSY);printf("PASS edit request view: %u checks; actual acknowledged native input, semantic config ABA, model revision, two editors input/delete/undo/IME/focus, exact request ownership, cancellation and six-participant barrier, failed prefix, environment, stop/restart, exhaustion and worker-owned teardown\n",checks);return 0;}
