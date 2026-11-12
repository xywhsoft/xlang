#define _WIN32_WINNT 0x0601
#include "ui/language_view.c"
#include "ui/completion_view.h"
#include "ui/editor_view.h"
#include "services/source_barrier.h"
#include "services/recovery.h"
#include "services/template_generation.h"
#include "catalog_ready.h"
#include <tlhelp32.h>
static unsigned checks;static DWORD uiThread;static int failAt=-1,failResumeLanguage,failMalloc;static unsigned watchResumes;
static HANDLE releaseTail;static volatile LONG captureNext,tailCount,tailTimeout;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"source-barrier:%d:%s\n",__LINE__,#v);ExitProcess(1);}}while(0)
static uint64_t rejected(xide_model_error* e){if(e){e->code=XIDE_MODEL_CONFLICT;snprintf(e->message,sizeof(e->message),"Deliberate participant admission refusal.");}return 0;}
extern uint64_t __real_xideDocumentsSuspendSources(xide_document_store*,xide_model_error*);
uint64_t __wrap_xideDocumentsSuspendSources(xide_document_store* s,xide_model_error* e){return failAt==0?rejected(e):__real_xideDocumentsSuspendSources(s,e);}
extern uint64_t __real_xideSourceJobsSuspend(xide_source_jobs*,xide_model_error*);
uint64_t __wrap_xideSourceJobsSuspend(xide_source_jobs* s,xide_model_error* e){return failAt==1?rejected(e):__real_xideSourceJobsSuspend(s,e);}
extern uint64_t __real_xideTasksSuspend(xide_tasks*,xide_model_error*);
uint64_t __wrap_xideTasksSuspend(xide_tasks* s,xide_model_error* e){return failAt==2?rejected(e):__real_xideTasksSuspend(s,e);}
extern uint64_t __real_xideToolchainsSuspend(xide_toolchains*,xide_model_error*);
uint64_t __wrap_xideToolchainsSuspend(xide_toolchains* s,xide_model_error* e){return failAt==3?rejected(e):__real_xideToolchainsSuspend(s,e);}
extern uint64_t __real_xideFileSyncSuspend(xide_app*,xide_model_error*);
uint64_t __wrap_xideFileSyncSuspend(xide_app* a,xide_model_error* e){return failAt==4?rejected(e):__real_xideFileSyncSuspend(a,e);}
extern uint64_t __real_xideLanguageSourcesSuspend(xide_app*,xide_model_error*);
uint64_t __wrap_xideLanguageSourcesSuspend(xide_app* a,xide_model_error* e){return failAt==5?rejected(e):__real_xideLanguageSourcesSuspend(a,e);}
extern int __real_xideFileSyncResume(xide_app*,uint64_t,xide_model_error*);
int __wrap_xideFileSyncResume(xide_app* a,uint64_t t,xide_model_error* e){++watchResumes;return __real_xideFileSyncResume(a,t,e);}
extern int __real_xideLanguageSourcesRelease(xide_app*,uint64_t);
int __wrap_xideLanguageSourcesRelease(xide_app* a,uint64_t t){if(failResumeLanguage){failResumeLanguage=0;return 0;}return __real_xideLanguageSourcesRelease(a,t);}
void* __real_malloc(size_t);
void* __wrap_malloc(size_t n){if(GetCurrentThreadId()==uiThread&&failMalloc){failMalloc=0;return NULL;}return __real_malloc(n);}
extern HANDLE(WINAPI* __real___imp_CreateThread)(LPSECURITY_ATTRIBUTES,SIZE_T,LPTHREAD_START_ROUTINE,LPVOID,DWORD,LPDWORD);
typedef struct invocation{LPTHREAD_START_ROUTINE proc;void* data;}invocation;
static DWORD WINAPI tail(void* p){invocation c=*(invocation*)p;free(p);DWORD result=c.proc(c.data);InterlockedIncrement(&tailCount);if(WaitForSingleObject(releaseTail,60000)!=WAIT_OBJECT_0)tailTimeout=1;return result;}
static HANDLE WINAPI createProbe(LPSECURITY_ATTRIBUTES a,SIZE_T n,LPTHREAD_START_ROUTINE proc,LPVOID data,DWORD flags,LPDWORD id){if(GetCurrentThreadId()!=uiThread||!InterlockedCompareExchange(&captureNext,0,1))return __real___imp_CreateThread(a,n,proc,data,flags,id);invocation* p=malloc(sizeof(*p));if(!p)return NULL;*p=(invocation){proc,data};HANDLE h=__real___imp_CreateThread(a,n,tail,p,flags,id);if(!h)free(p);return h;}
HANDLE(WINAPI* __wrap___imp_CreateThread)(LPSECURITY_ATTRIBUTES,SIZE_T,LPTHREAD_START_ROUTINE,LPVOID,DWORD,LPDWORD)=createProbe;
static int lsWrite(const char* p,const char* body){return xrtFileWriteAtomic(p,(xbytesview){(const unsigned char*)body,strlen(body)});}
static int lsFrame(xide_app* a){if(xuiDispatchPendingEvents(a->context)!=XUI_OK)return 0;xideEditorPoll(a);xideFileSyncPoll(a);xideTaskViewPoll(a);xideLanguageViewPoll(a);xideCompletionViewPoll(a);xideWorkbenchLayout(a);return xuiLayout(a->context)==XUI_OK&&!a->frameError;}
static int lsReady(xide_app* a){ULONGLONG until=GetTickCount64()+15000;do{if(!lsFrame(a))return 0;xide_language_view* v=a->languageView;if(v->initialized&&v->autoEnabled&&v->autoView->syncAdopted&&!v->syncMode&&xideLanguageClientCanSuspendAnalysis(v->client)&&xideCompletionSourcePreflight(a))return 1;Sleep(2);}while(GetTickCount64()<until);return 0;}
const char* xideTestCompletionSourceState(xide_app*);
void xideTestCompletionSourceStop(xide_app*);
static int lsCompletion(xide_app* a,xide_document* d,const char* body){int offset=(int)(strstr(body,"dep.value")-body)+4;xui_code_selection_t s={.iCaretOffset=offset,.iAnchorOffset=offset};if(xideEditorShow(a,d)!=XUI_OK || xuiSetFocusWidget(a->context,d->editor)!=XUI_OK || xuiCodeSelectionSetState(xuiCodeEditGetSelection(d->editor),xuiCodeEditGetDocument(d->editor),&s)!=XUI_OK)return 0;xideCompletionViewRequest(a,d->editor,offset,1);int retries=0;ULONGLONG until=GetTickCount64()+15000;do{if(!lsFrame(a))return 0;if(xideCompletionViewHasName(a,d->editor,"value"))return 1;if(!xideCompletionViewActive(a)&&retries++<4)xideCompletionViewRequest(a,d->editor,offset,1);Sleep(2);}while(GetTickCount64()<until);fprintf(stderr,"completion timeout: %s\n",xideTestCompletionSourceState(a));return 0;}
static size_t lsPids(DWORD* ids){size_t count=0;HANDLE h=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(h==INVALID_HANDLE_VALUE)return SIZE_MAX;PROCESSENTRY32W p={.dwSize=sizeof(p)};if(Process32FirstW(h,&p))do{if(!wcscmp(p.szExeFile,L".xlang-ls-source-barrier.exe")&&p.th32ParentProcessID==GetCurrentProcessId()){if(count<4)ids[count]=p.th32ProcessID;++count;}}while(Process32NextW(h,&p));CloseHandle(h);for(size_t i=0;i<count&&i<4;++i)for(size_t j=i+1;j<count&&j<4;++j)if(ids[i]>ids[j]){DWORD t=ids[i];ids[i]=ids[j];ids[j]=t;}return count;}
static int lsSamePids(const DWORD* before){DWORD now[4];return lsPids(now)==2 && now[0]==before[0]&&now[1]==before[1];}
static int lsKey(xide_app* a,int key,uint32_t mods){return xuiInputKeyDown(a->context,key,mods)==XUI_OK&&xuiInputKeyUp(a->context,key,mods)==XUI_OK&&lsFrame(a);}
static int lsIme(xide_app* a,const char* text,int active){xui_ime_composition_t c={.iSize=sizeof(c),.sText=text,.iTextSize=(int)strlen(text),.bActive=active};c.iCursor=c.iSelectionStart=c.iSelectionEnd=c.iTextSize;return xuiInputImeCompositionEx(a->context,&c)==XUI_OK&&lsFrame(a);}
static int lsCapture(xide_app* a,const char* path){xui_rect_i_t r={0,0,a->viewportWidth,a->viewportHeight};return lsFrame(a)&&xuiUpdate(a->context,0)==XUI_OK&&a->proxy.surfaceClear(&a->proxy,a->target,0xffeeeeeeu)==XUI_OK&&xuiRender(a->context,a->target,&r,1)==XUI_OK&&xideAppCapture(a,path)==XGE_OK;}

typedef struct fixture{const char *directory,*compiler;HDESK desktop;int started;} fixture;
static void reopened(xide_app* a){CHECK(!xideDocumentsSourcesHeld(&a->documents)&&!xideSourceJobsHeld(&a->sourceJobs)&&!xideTasksAdmissionHeld(a->tasks)&&!xideToolchainsAdmissionHeld(xideTasksToolchains(a->tasks)));}
static void releaseBarrier(xide_source_barrier* b,uint64_t token){xide_model_error e={0};CHECK(xideSourceBarrierResume(b,token,&e));ULONGLONG until=GetTickCount64()+15000;for(;;){CHECK(lsFrame(b->app));int result=xideSourceBarrierRelease(b,token,&e);CHECK(result>=0);if(result==1)break;CHECK(GetTickCount64()<until);Sleep(1);}}
static void waitBarrier(xide_source_barrier* b,uint64_t token,xide_source_barrier_status* s){ULONGLONG until=GetTickCount64()+15000;do{CHECK(lsFrame(b->app)&&xideSourceBarrierState(b,token,s)&&!s->lost&&!s->failed);if(s->quiet)return;CHECK(GetTickCount64()<until);Sleep(1);}while(1);}
static int verify(void* user)
{
 fixture* f=user;if(f->started)return XGE_OK;f->started=1;uiThread=GetCurrentThreadId();xide_app app={.hidden=1,.noSession=1,.compilerPath=f->compiler};xide_model_error error={0};xide_source_barrier barrier={0};xide_source_barrier_status state={0};
 char *entry=xidePathResolveLexical(f->directory,"main.xl"),*dep=xidePathResolveLexical(f->directory,"dep.xl"),*native=xidePathResolveLexical(f->directory,"main.c"),*settings=xidePathResolveLexical(f->directory,"settings.json"),*session=xidePathResolveLexical(f->directory,"session"),*diagnostics=xidePathResolveLexical(f->directory,"diagnostics"),*picture=xidePathResolveLexical(f->directory,"barrier-input.png");
 const char* body="import dep\nfunc main(...) as int\n return dep.value()\nend\n";const char* good="export func value() as int\n return 7\nend\n";const char* cBody="int main(void) { return 0; }\n";
 CHECK(entry&&dep&&native&&settings&&session&&diagnostics&&picture&&lsWrite(entry,body)&&lsWrite(dep,good)&&lsWrite(native,cBody));app.settingsPath=settings;app.sessionRoot=session;app.diagnosticRoot=diagnostics;CHECK(xideAppCreate(&app)==XUI_OK&&xgeBegin()==XGE_OK&&xideTestCatalogReady(&app));
 /* Every constructor-failure prefix remains explicitly owned; a failed Begin
  * never authorizes a write and never steals a preexisting participant hold. */
 for(failAt=0;failAt<6;++failAt){uint64_t token=xideSourceBarrierBegin(&barrier,&app,&error);CHECK(error.code);if(!failAt){CHECK(!token);reopened(&app);continue;}CHECK(token&&xideSourceBarrierState(&barrier,token,&state)&&state.failed&&!state.ready&&state.held==((1u<<failAt)-1));int saved=failAt;failAt=-1;releaseBarrier(&barrier,token);reopened(&app);failAt=saved;}
 failAt=-1;uint64_t foreign=xideTasksSuspend(app.tasks,&error);CHECK(foreign);uint64_t token=xideSourceBarrierBegin(&barrier,&app,&error);CHECK(token&&xideSourceBarrierState(&barrier,token,&state)&&state.failed&&state.held==(XIDE_SOURCE_DOCUMENTS|XIDE_SOURCE_JOBS));releaseBarrier(&barrier,token);CHECK(xideTasksSourceLeaseCurrent(app.tasks,foreign)&&xideTasksResume(app.tasks,foreign,&error));reopened(&app);
 CHECK(xideEditorOpen(&app,native,&error));xide_document* c=app.activeDocument;CHECK(xideEditorOpen(&app,entry,&error));xide_document* d=app.activeDocument;CHECK(lsReady(&app)&&lsCompletion(&app,d,body)&&lsReady(&app));DWORD pids[4];CHECK(lsPids(pids)==2);
 /* Three real workers publish their result but remain in physical thread tails.
  * All are independently required by the actual group State. */
 captureNext=1;xide_toolchain_request* query=xideToolchainRequest(xideTasksToolchains(app.tasks),f->compiler,f->directory,1,&error);CHECK(query&&!captureNext);
 xide_recovery_service* recovery=xideRecoveryServiceCreate(&app.sourceJobs,NULL);CHECK(recovery);captureNext=1;CHECK(xideRecoveryServiceStart(recovery,XIDE_RECOVERY_GLOBAL,settings,&error)&&!captureNext);
 captureNext=1;xide_task* task=xideTasksBuildFileGuarded(app.tasks,native,f->compiler,0,NULL,&error);CHECK(task&&!captureNext);
 CHECK(lsReady(&app));token=xideSourceBarrierBegin(&barrier,&app,&error);CHECK(token);CHECK(!xideSourceBarrierBegin(&barrier,&app,&error)&&xideSourceBarrierRelease(&barrier,token,&error)==-1&&!xideSourceBarrierResume(&barrier,token+1,&error));state=(xide_source_barrier_status){.ready=1,.held=99};CHECK(!xideSourceBarrierState(&barrier,token+1,&state)&&!state.ready&&!state.held);
 ULONGLONG until=GetTickCount64()+45000;do{CHECK(lsFrame(&app)&&xideSourceBarrierState(&barrier,token,&state)&&!state.failed&&!state.invalidated);if(tailCount==3&&!(state.pending&(XIDE_SOURCE_WATCH|XIDE_SOURCE_LANGUAGE)))break;CHECK(GetTickCount64()<until);Sleep(1);}while(1);
 CHECK(xideTasksSourceLeaseCurrent(app.tasks,barrier.leases[2])&&!xideTasksSourceLeaseCurrent(app.tasks,barrier.leases[2]+1)&&xideToolchainsSourceLeaseCurrent(xideTasksToolchains(app.tasks),barrier.leases[3])&&!xideToolchainsSourceLeaseCurrent(xideTasksToolchains(app.tasks),barrier.leases[3]+1)&&xideFileSyncSourceLeaseCurrent(&app,barrier.leases[4])&&!xideFileSyncSourceLeaseCurrent(&app,barrier.leases[4]+1));
 CHECK(!tailTimeout&&state.held==XIDE_SOURCE_ALL&&!state.quiet&&!state.ready&&state.pending==(XIDE_SOURCE_JOBS|XIDE_SOURCE_TASKS|XIDE_SOURCE_TOOLCHAINS));xide_task_info info={0};xideTaskInfo(task,&info);CHECK(info.state>=XIDE_TASK_SUCCEEDED&&!xideToolchainRequestStopped(query));
 CHECK(!xideDocumentOpen(&app.documents,native,&error)&&!xideDocumentSave(c,NULL,XIDE_DOCUMENT_OVERWRITE_EXTERNAL,&error)&&!xideRecoveryServiceStart(recovery,XIDE_RECOVERY_GLOBAL,settings,&error)&&!xideToolchainRequest(xideTasksToolchains(app.tasks),f->compiler,f->directory,0,&error)&&!xideTasksBuildFileGuarded(app.tasks,native,f->compiler,0,NULL,&error)&&!xideTemplateJobStart(&app.sourceJobs,(const xide_template_request*)(uintptr_t)1,&error));
 CHECK(SetEvent(releaseTail));waitBarrier(&barrier,token,&state);CHECK(state.ready&&lsSamePids(pids));
 /* User edits remain available and revoke request validity permanently, even
  * when Undo restores the original bytes. */
 xide_document* docs[]={c,d};const char* bodies[]={cBody,body};
 for(int i=0;i<2;++i){xide_document* doc=docs[i];CHECK(xideEditorShow(&app,doc)==XUI_OK&&lsFrame(&app));xui_rect_t r=xuiWidgetGetWorldRect(doc->editor);float x=r.fX+100,y=r.fY+12;CHECK(xuiInputPointerMove(app.context,x,y,0)==XUI_OK&&xuiInputPointerDown(app.context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK&&xuiInputPointerUp(app.context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK&&lsFrame(&app)&&xuiGetFocusWidget(app.context)==doc->editor);xui_code_document core=xuiCodeEditGetDocument(doc->editor);int end=xuiCodeDocumentGetLength(core);CHECK(xuiCodeSelectionSetRange(xuiCodeEditGetSelection(doc->editor),core,end,end)==XUI_OK&&xuiInputKeyDown(app.context,'Z',0)==XUI_OK&&xuiInputText(app.context,'z')==XUI_OK&&xuiInputKeyUp(app.context,'Z',0)==XUI_OK&&lsFrame(&app));CHECK(strstr(xuiCodeDocumentGetText(core),"z")&&lsKey(&app,XUI_KEY_BACKSPACE,0)&&!strcmp(xuiCodeDocumentGetText(core),bodies[i]));CHECK(lsKey(&app,'Z',XUI_MOD_CTRL)&&strstr(xuiCodeDocumentGetText(core),"z")&&lsKey(&app,'Z',XUI_MOD_CTRL)&&!strcmp(xuiCodeDocumentGetText(core),bodies[i]));CHECK(lsIme(&app,"中文😀",1)&&lsIme(&app,"中文😀",0)&&strstr(xuiCodeDocumentGetText(core),"中文😀")&&lsKey(&app,'Z',XUI_MOD_CTRL)&&!strcmp(xuiCodeDocumentGetText(core),bodies[i]));}
 CHECK(xideSourceBarrierState(&barrier,token,&state)&&state.quiet&&state.invalidated&&!state.ready&&!state.failed&&lsCapture(&app,picture));xideRecoveryServiceDestroy(recovery);CHECK(xideSourceBarrierState(&barrier,token,&state)&&state.invalidated);xideToolchainRequestDestroy(query);
 CHECK(xideSourceBarrierResume(&barrier,token,&error)&&xideSourceBarrierResume(&barrier,token,&error));until=GetTickCount64()+15000;do{CHECK(lsFrame(&app)&&xideSourceBarrierState(&barrier,token,&state));if(!(state.pending&(XIDE_SOURCE_WATCH|XIDE_SOURCE_LANGUAGE)))break;CHECK(GetTickCount64()<until);Sleep(1);}while(1);
 /* Actual allocation refusal in monitor resubscription cannot reopen another
  * gate. A later failure after successful monitor resume is retried without
  * repeating that successful prefix. */
 unsigned before=watchResumes;failMalloc=1;CHECK(xideSourceBarrierRelease(&barrier,token,&error)==-1&&!failMalloc&&watchResumes==before+1&&xideSourceBarrierState(&barrier,token,&state)&&state.held==XIDE_SOURCE_ALL);failResumeLanguage=1;CHECK(xideSourceBarrierRelease(&barrier,token,&error)==-1&&!failResumeLanguage&&watchResumes==before+2&&xideSourceBarrierState(&barrier,token,&state)&&state.held==(XIDE_SOURCE_ALL&~XIDE_SOURCE_WATCH));CHECK(xideSourceBarrierRelease(&barrier,token,&error)==1&&watchResumes==before+2);reopened(&app);CHECK(!xideSourceBarrierState(&barrier,token,&state)&&!xideSourceBarrierResume(&barrier,token,&error)&&lsReady(&app)&&lsCompletion(&app,d,body)&&lsSamePids(pids));
 /* A dead peer is never write authority even after its physical shutdown. The
  * healthy peer retains its cache/identity while explicit resume rebuilds only
  * the failed completion connection. */
 CHECK(lsReady(&app));token=xideSourceBarrierBegin(&barrier,&app,&error);CHECK(token);waitBarrier(&barrier,token,&state);xide_language_client* healthy=app.languageView->client;xideTestCompletionSourceStop(&app);until=GetTickCount64()+15000;do{CHECK(lsFrame(&app)&&xideSourceBarrierState(&barrier,token,&state));if(state.failed&&state.quiet)break;CHECK(GetTickCount64()<until);Sleep(1);}while(1);CHECK(state.failed&&!state.ready);releaseBarrier(&barrier,token);reopened(&app);CHECK(lsReady(&app)&&lsCompletion(&app,d,body)&&app.languageView->client==healthy);
 /* Early abandonment still waits for asynchronous monitor/language handshakes,
  * but does not join or cancel a physically retained recovery worker. */
 ResetEvent(releaseTail);LONG tails=tailCount;recovery=xideRecoveryServiceCreate(&app.sourceJobs,NULL);CHECK(recovery);captureNext=1;CHECK(xideRecoveryServiceStart(recovery,XIDE_RECOVERY_GLOBAL,settings,&error));until=GetTickCount64()+15000;while(tailCount==tails){CHECK(lsFrame(&app)&&GetTickCount64()<until);Sleep(1);}CHECK(lsReady(&app));token=xideSourceBarrierBegin(&barrier,&app,&error);CHECK(token&&xideSourceBarrierState(&barrier,token,&state)&&!state.ready);releaseBarrier(&barrier,token);CHECK(!tailTimeout&&xideRecoveryServiceBusy(recovery));CHECK(SetEvent(releaseTail));xideRecoveryServiceDestroy(recovery);reopened(&app);
 barrier.serial=UINT64_MAX;CHECK(!xideSourceBarrierBegin(&barrier,&app,&error));reopened(&app);CHECK(!tailTimeout);xgeEnd();xideAppDestroy(&app);free(entry);free(dep);free(native);free(settings);free(session);free(diagnostics);free(picture);xgeQuit();return XGE_OK;
}
static DWORD WINAPI gui(void* raw){fixture* f=raw;if(!SetThreadDesktop(f->desktop))return 1;xge_desc_t d={.iWidth=1280,.iHeight=800,.sTitle="Source barrier",.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI,.iRunMode=XGE_RUN_GAME_LOOP};int result=xgeInit(&d);if(result==XGE_OK)result=xgeRun(verify,f);xgeUnit();return result==XGE_OK&&f->started?0:1;}
int main(int argc,char** argv){if(argc!=3||!xideIoCreateDirectories(argv[1]))return 2;releaseTail=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(releaseTail);wchar_t name[80];swprintf(name,80,L"SourceBarrier-%lu",GetCurrentProcessId());fixture f={.directory=argv[1],.compiler=argv[2]};f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);CHECK(f.desktop);HANDLE t=CreateThread(NULL,0,gui,&f,0,NULL);CHECK(t&&WaitForSingleObject(t,180000)==WAIT_OBJECT_0);DWORD code=1;CHECK(GetExitCodeThread(t,&code)&&!code);CloseHandle(t);if(!CloseDesktop(f.desktop))CHECK(GetLastError()==ERROR_BUSY);CloseHandle(releaseTail);printf("PASS source barrier: %u checks; six real participants, physical tails, failed prefixes, foreign tokens, retryable resume, dead peer, C/xlang input/delete/undo/IME/focus\n",checks);return 0;}
