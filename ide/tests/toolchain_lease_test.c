#define _WIN32_WINNT 0x0601
#include "services/toolchain.c"
#include "ui/toolchain_view.c"
#include "ui/editor_view.h"
#include "services/file_sync.h"
#include "catalog_ready.h"

static DWORD owner;
static volatile LONG tracking,allocations,frees,failAllocation,allocationAt,failThread;
static volatile LONG readGate,readHit,tailGate,tailHit,tailTimeout;
static HANDLE readEntered,readRelease,tailEntered,tailRelease;
static BY_HANDLE_FILE_INFORMATION readIdentity;
static unsigned checks;static xui_code_document lastDocument;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"toolchain-lease:%d:%s;%s\n",__LINE__,#v,lastDocument?xuiCodeDocumentGetText(lastDocument):"");ExitProcess(1);}}while(0)
void* __real_malloc(size_t);void* __real_calloc(size_t,size_t);void __real_free(void*);
static int rejectAllocation(void)
{return GetCurrentThreadId()==owner && failAllocation && InterlockedIncrement(&allocationAt)==failAllocation;}
void* __wrap_malloc(size_t n){if(tracking && GetCurrentThreadId()==owner)++allocations;return rejectAllocation()?NULL:__real_malloc(n);}
void* __wrap_calloc(size_t n,size_t z){if(tracking && GetCurrentThreadId()==owner)++allocations;return rejectAllocation()?NULL:__real_calloc(n,z);}
void __wrap_free(void* p){if(tracking && GetCurrentThreadId()==owner)++frees;__real_free(p);}
extern BOOL (WINAPI* __real___imp_ReadFile)(HANDLE,LPVOID,DWORD,LPDWORD,LPOVERLAPPED);
static BOOL WINAPI readProbe(HANDLE h,LPVOID data,DWORD n,LPDWORD got,LPOVERLAPPED at)
{
 if(readGate && GetCurrentThreadId()!=owner){BY_HANDLE_FILE_INFORMATION info={0};
  if(GetFileInformationByHandle(h,&info) && info.dwVolumeSerialNumber==readIdentity.dwVolumeSerialNumber && info.nFileIndexHigh==readIdentity.nFileIndexHigh && info.nFileIndexLow==readIdentity.nFileIndexLow && InterlockedCompareExchange(&readGate,0,1)==1){
   readHit=1;SetEvent(readEntered);if(WaitForSingleObject(readRelease,15000)!=WAIT_OBJECT_0){SetLastError(ERROR_TIMEOUT);return FALSE;}
  }
 }
 return __real___imp_ReadFile(h,data,n,got,at);
}
BOOL (WINAPI* __wrap___imp_ReadFile)(HANDLE,LPVOID,DWORD,LPDWORD,LPOVERLAPPED)=readProbe;
extern HANDLE (WINAPI* __real___imp_CreateThread)(LPSECURITY_ATTRIBUTES,SIZE_T,LPTHREAD_START_ROUTINE,LPVOID,DWORD,LPDWORD);
typedef struct invocation {LPTHREAD_START_ROUTINE fn;void* user;} invocation;
static DWORD WINAPI workerProbe(void* p)
{
 invocation call=*(invocation*)p;__real_free(p);DWORD result=call.fn(call.user);
 if(tailGate){InterlockedIncrement(&tailHit);SetEvent(tailEntered);if(WaitForSingleObject(tailRelease,15000)!=WAIT_OBJECT_0)tailTimeout=1;}
 return result;
}
static HANDLE WINAPI createProbe(LPSECURITY_ATTRIBUTES a,SIZE_T stack,LPTHREAD_START_ROUTINE fn,LPVOID user,DWORD flags,LPDWORD id)
{
 if(fn!=queryWorker)return __real___imp_CreateThread(a,stack,fn,user,flags,id);
 if(failThread){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return NULL;}
 invocation* p=__real_malloc(sizeof(*p));if(!p)return NULL;*p=(invocation){fn,user};HANDLE h=__real___imp_CreateThread(a,stack,workerProbe,p,flags,id);if(!h)__real_free(p);return h;
}
HANDLE (WINAPI* __wrap___imp_CreateThread)(LPSECURITY_ATTRIBUTES,SIZE_T,LPTHREAD_START_ROUTINE,LPVOID,DWORD,LPDWORD)=createProbe;
static void armRead(const char* path)
{
 wchar_t* w=wide(path);CHECK(w);HANDLE h=CreateFileW(w,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);free(w);CHECK(h!=INVALID_HANDLE_VALUE && GetFileInformationByHandle(h,&readIdentity));CloseHandle(h);
 ResetEvent(readEntered);ResetEvent(readRelease);readHit=0;readGate=1;
}
static void armTail(void){ResetEvent(tailEntered);ResetEvent(tailRelease);tailHit=tailTimeout=0;tailGate=1;}
static void releaseAll(void){readGate=tailGate=0;SetEvent(readRelease);SetEvent(tailRelease);}
static void waitStopped(xide_toolchain_request* q)
{uint64_t end=GetTickCount64()+20000;while(!xideToolchainRequestStopped(q)){CHECK(GetTickCount64()<end);Sleep(2);}CHECK(!tailTimeout);}
static size_t registered(xide_toolchains* c){size_t n=0;for(xide_toolchain_request* q=c->requests;q;q=q->next){CHECK(q->cache==c);if(q->next)CHECK(q->next->previous==q);++n;}return n;}
static void service(const char* self,const char* directory)
{
 xide_model_error e={0};xide_toolchains* c=xideToolchainsCreate();CHECK(c);
 allocations=frees=0;tracking=1;uint64_t t=xideToolchainsSuspend(c,&e);CHECK(t && xideToolchainsSuspended(c,t) && !allocations && !frees);CHECK(!xideToolchainRequest(c,"?:bad","?:bad",1,&e) && !allocations && !frees);tracking=0;
 CHECK(xideToolchainsAdmissionHeld(c) && !xideToolchainsSuspend(c,&e) && !xideToolchainsResume(c,0,&e) && !xideToolchainsResume(c,t+1,&e) && xideToolchainsResume(c,t,&e));
 CHECK(!xideToolchainsSuspended(c,t) && !xideToolchainsAdmissionHeld(c));
 armRead(self);armTail();xide_toolchain_request* first=xideToolchainRequest(c,self,directory,1,&e);CHECK(first && WaitForSingleObject(readEntered,10000)==WAIT_OBJECT_0 && readHit);
 allocations=frees=0;tracking=1;t=xideToolchainsSuspend(c,&e);CHECK(t && !allocations && !frees && !xideToolchainsSuspended(c,t));CHECK(!xideToolchainRequest(c,self,directory,0,&e) && !allocations && !frees);tracking=0;
 CHECK(!cancelled(first) && xideToolchainsResume(c,t,&e));t=xideToolchainsSuspend(c,&e);CHECK(t && registered(c)==1 && !xideToolchainsSuspended(c,t));
 CHECK(SetEvent(readRelease) && WaitForSingleObject(tailEntered,15000)==WAIT_OBJECT_0 && xideToolchainRequestState(first,NULL,0)==XIDE_TOOL_QUERY_READY && !xideToolchainRequestStopped(first) && !xideToolchainsSuspended(c,t));
 xide_toolchain_profile profile={0};CHECK(xideToolchainRequestResult(first,&profile) && !profile.cached && (profile.commands&XIDE_TOOL_CHECK));xideToolchainProfileClear(&profile);
 releaseAll();waitStopped(first);CHECK(xideToolchainsSuspended(c,t) && xideToolchainsResume(c,t,&e));
 /* A completed request kept by its caller is not a live reader. Multiple
    requests, destruction out of order and cache hits retain their ownership. */
 xide_toolchain_request* second=xideToolchainRequest(c,self,directory,0,&e);CHECK(second);waitStopped(second);CHECK(xideToolchainRequestResult(second,&profile) && profile.cached);xideToolchainProfileClear(&profile);
 xide_toolchain_request* third=xideToolchainRequest(c,self,directory,1,&e);CHECK(third);waitStopped(third);CHECK(registered(c)==3);
 xideToolchainRequestDestroy(second);CHECK(registered(c)==2 && c->requests==third && third->next==first);xideToolchainRequestDestroy(first);CHECK(registered(c)==1 && third->next==NULL);xideToolchainRequestDestroy(third);CHECK(registered(c)==0);
 /* An active real snapshot can be cancelled during a passive hold, but the
    held read and physical cleanup must still leave before acknowledgement. */
 armRead(self);first=xideToolchainRequest(c,self,directory,1,&e);CHECK(first && WaitForSingleObject(readEntered,10000)==WAIT_OBJECT_0);t=xideToolchainsSuspend(c,&e);CHECK(t);xideToolchainRequestCancel(first);CHECK(!xideToolchainsSuspended(c,t));releaseAll();waitStopped(first);CHECK(xideToolchainRequestState(first,NULL,0)==XIDE_TOOL_QUERY_CANCELLED && xideToolchainsSuspended(c,t));xideToolchainRequestDestroy(first);CHECK(xideToolchainsResume(c,t,&e));
 /* Fail real caller allocations and thread creation; no failed prefix may
    remain registered as an immortal reader. */
 int failures=0;
 for(int n=1;n<=32;++n){allocationAt=0;failAllocation=n;first=xideToolchainRequest(c,self,directory,1,&e);failAllocation=0;if(first){waitStopped(first);xideToolchainRequestDestroy(first);break;}++failures;CHECK(!registered(c));t=xideToolchainsSuspend(c,&e);CHECK(t && xideToolchainsSuspended(c,t) && xideToolchainsResume(c,t,&e));}
 CHECK(failures>=3);failThread=1;CHECK(!xideToolchainRequest(c,self,directory,1,&e) && !registered(c));failThread=0;
 t=xideToolchainsSuspend(c,&e);CHECK(t);c->holdSerial=UINT64_MAX;CHECK(xideToolchainsResume(c,t,&e) && !xideToolchainsSuspend(c,&e));xideToolchainsDestroy(c);
 CHECK(!xideToolchainsSuspend(NULL,&e) && !xideToolchainsSuspended(NULL,1) && !xideToolchainsResume(NULL,1,&e) && !xideToolchainsAdmissionHeld(NULL) && xideToolchainRequestStopped(NULL));
}
typedef struct fixture {const char *directory,*compiler;HDESK desktop;int started;} fixture;
static int frame(xide_app* a)
{if(xuiDispatchPendingEvents(a->context)!=XUI_OK)return 0;xideEditorPoll(a);xideFileSyncPoll(a);xideTaskViewPoll(a);xideToolchainViewPoll(a);xideWorkbenchLayout(a);return xuiLayout(a->context)==XUI_OK && !a->frameError;}
static int key(xide_app* a,int code,uint32_t mods){return xuiInputKeyDown(a->context,code,mods)==XUI_OK && xuiInputKeyUp(a->context,code,mods)==XUI_OK && frame(a);}
static int ime(xide_app* a,const char* text,int active){xui_ime_composition_t c={.iSize=sizeof(c),.sText=text,.iTextSize=(int)strlen(text),.bActive=active};c.iCursor=c.iSelectionStart=c.iSelectionEnd=c.iTextSize;return xuiInputImeCompositionEx(a->context,&c)==XUI_OK && frame(a);}
static int verify(void* user)
{
 fixture* f=user;if(f->started)return XGE_OK;f->started=1;owner=GetCurrentThreadId();xide_model_error e={0};xide_app a={.hidden=1,.noSession=1,.disableAutomaticDiagnostics=1,.compilerPath=f->compiler};
 char *settings=xidePathResolveLexical(f->directory,"settings.json"),*session=xidePathResolveLexical(f->directory,"sessions"),*c=xidePathResolveLexical(f->directory,"main.c"),*xl=xidePathResolveLexical(f->directory,"main.xl"),*picture=xidePathResolveLexical(f->directory,"held-toolchain-input.png");CHECK(settings && session && c && xl && picture);
 a.settingsPath=settings;a.sessionRoot=session;const char* bodies[]={"int value;\n","func value()\nend\n"};CHECK(xrtFileWriteAtomic(c,(xbytesview){(const unsigned char*)bodies[0],strlen(bodies[0])}) && xrtFileWriteAtomic(xl,(xbytesview){(const unsigned char*)bodies[1],strlen(bodies[1])}));
 CHECK(xideAppCreate(&a)==XUI_OK && xgeBegin()==XGE_OK && xideTestCatalogReady(&a));CHECK(xideEditorOpen(&a,c,&e));xide_document* docs[2]={a.activeDocument,NULL};CHECK(xideEditorOpen(&a,xl,&e));docs[1]=a.activeDocument;
 armTail();CHECK(xideToolchainViewOpen(&a) && WaitForSingleObject(tailEntered,15000)==WAIT_OBJECT_0);xide_toolchains* cache=xideTasksToolchains(a.tasks);xide_toolchain_request* q=a.toolchainView->request;CHECK(q && xideToolchainRequestState(q,NULL,0)==XIDE_TOOL_QUERY_READY && !xideToolchainRequestStopped(q));
 uint64_t t=xideToolchainsSuspend(cache,&e);CHECK(t && !xideToolchainsSuspended(cache,t));
 /* This Poll must return while the actual worker is still in its tail. */
 xideToolchainViewPoll(&a);CHECK(!tailTimeout && a.toolchainView->request==q && !xideToolchainRequestStopped(q) && xideToolchainViewBusy(&a));
 CHECK(!xideToolchainViewCanOpen(&a) && !xideToolchainViewOpen(&a) && !xuiWidgetGetEnabled(xideToolchainViewControl(&a,XIDE_TOOL_REFRESH)));xideDialogSetOpen(xideToolchainViewWindow(&a),0);
 for(int i=0;i<2;++i){xide_document* d=docs[i];CHECK(xideEditorShow(&a,d)==XUI_OK && frame(&a));xui_rect_t rect=xuiWidgetGetWorldRect(d->editor);float x=rect.fX+100,y=rect.fY+12;CHECK(xuiInputPointerMove(a.context,x,y,0)==XUI_OK && xuiInputPointerDown(a.context,x,y,XUI_POINTER_BUTTON_LEFT,XUI_POINTER_BUTTON_LEFT)==XUI_OK && xuiInputPointerUp(a.context,x,y,XUI_POINTER_BUTTON_LEFT,0)==XUI_OK && frame(&a) && xuiGetFocusWidget(a.context)==d->editor);
  xui_code_document core=xuiCodeEditGetDocument(d->editor);lastDocument=core;int end=xuiCodeDocumentGetLength(core);CHECK(xuiCodeSelectionSetRange(xuiCodeEditGetSelection(d->editor),core,end,end)==XUI_OK && xuiInputKeyDown(a.context,'Z',0)==XUI_OK && xuiInputText(a.context,'z')==XUI_OK && xuiInputKeyUp(a.context,'Z',0)==XUI_OK && frame(&a));CHECK(strstr(xuiCodeDocumentGetText(core),"z"));CHECK(key(&a,XUI_KEY_BACKSPACE,0));CHECK(!strcmp(xuiCodeDocumentGetText(core),bodies[i]));CHECK(key(&a,'Z',XUI_MOD_CTRL) && strstr(xuiCodeDocumentGetText(core),"z") && key(&a,'Z',XUI_MOD_CTRL) && !strcmp(xuiCodeDocumentGetText(core),bodies[i]));
  CHECK(ime(&a,"中文",1) && ime(&a,"中文",0) && frame(&a) && strstr(xuiCodeDocumentGetText(core),"中文") && key(&a,'Z',XUI_MOD_CTRL) && !strcmp(xuiCodeDocumentGetText(core),bodies[i]));
 }
 CHECK(a.toolchainView->request==q && !xideToolchainsSuspended(cache,t));releaseAll();waitStopped(q);CHECK(xideToolchainsSuspended(cache,t));xideToolchainViewPoll(&a);CHECK(!xideToolchainViewBusy(&a) && !a.toolchainView->request && !xuiWidgetGetEnabled(xideToolchainViewControl(&a,XIDE_TOOL_REFRESH)) && xideToolchainsSuspended(cache,t));
 CHECK(!xideToolchainViewCanOpen(&a) && xideToolchainsResume(cache,t,&e));xideToolchainViewPoll(&a);CHECK(xuiWidgetGetEnabled(xideToolchainViewControl(&a,XIDE_TOOL_REFRESH)) && xideToolchainViewCanOpen(&a));
 CHECK(xideToolchainViewOpen(&a));uint64_t end=GetTickCount64()+15000;while(xideToolchainViewBusy(&a)){CHECK(frame(&a) && GetTickCount64()<end);Sleep(2);}CHECK(a.toolchainView->profile.cached && a.toolchainView->profile.help);xideDialogSetOpen(xideToolchainViewWindow(&a),0);
 xui_rect_i_t rect={0,0,a.viewportWidth,a.viewportHeight};CHECK(frame(&a) && xuiUpdate(a.context,0)==XUI_OK && a.proxy.surfaceClear(&a.proxy,a.target,0xffeeeeeeu)==XUI_OK && xuiRender(a.context,a.target,&rect,1)==XUI_OK && xideAppCapture(&a,picture)==XGE_OK);
 lastDocument=NULL;xgeEnd();xideAppDestroy(&a);free(settings);free(session);free(c);free(xl);free(picture);xgeQuit();return XGE_OK;
}
static DWORD WINAPI gui(void* p)
{fixture* f=p;if(!SetThreadDesktop(f->desktop))return 1;xge_desc_t d={.iWidth=1280,.iHeight=800,.sTitle="Toolchain source lease",.iFlags=XGE_INIT_WINDOW|XGE_INIT_HIGHDPI,.iRunMode=XGE_RUN_GAME_LOOP};int result=xgeInit(&d);if(result==XGE_OK)result=xgeRun(verify,f);xgeUnit();if(result!=XGE_OK || !f->started)fprintf(stderr,"toolchain-gui result=%d started=%d\n",result,f->started);return result==XGE_OK && f->started?0:1;}
int main(int argc,char** argv)
{
 if(argc==2 && !strcmp(argv[1],"--help")){puts("xl build <file> -o out\nxl run <file>\nxl check <file> --json --source-overlay");return 0;}
 if(argc!=3 || !xrtDirCreateAll(argv[1])){return 2;}
 owner=GetCurrentThreadId();readEntered=CreateEventW(NULL,TRUE,FALSE,NULL);readRelease=CreateEventW(NULL,TRUE,FALSE,NULL);tailEntered=CreateEventW(NULL,TRUE,FALSE,NULL);tailRelease=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(readEntered && readRelease && tailEntered && tailRelease);
 wchar_t exe[32768];CHECK(GetModuleFileNameW(NULL,exe,32768));int n=WideCharToMultiByte(CP_UTF8,0,exe,-1,NULL,0,NULL,NULL);char* self=malloc(n);CHECK(self && WideCharToMultiByte(CP_UTF8,0,exe,-1,self,n,NULL,NULL));service(self,argv[1]);free(self);
 wchar_t name[100];swprintf(name,100,L"ToolchainLease-%lu",GetCurrentProcessId());fixture f={.directory=argv[1],.compiler=argv[2]};f.desktop=CreateDesktopW(name,NULL,NULL,0,DESKTOP_CREATEWINDOW|DESKTOP_READOBJECTS|DESKTOP_WRITEOBJECTS|DESKTOP_ENUMERATE,NULL);CHECK(f.desktop);HANDLE thread=CreateThread(NULL,0,gui,&f,0,NULL);CHECK(thread && WaitForSingleObject(thread,120000)==WAIT_OBJECT_0);DWORD code=1;CHECK(GetExitCodeThread(thread,&code));CHECK(!code);CHECK(CloseHandle(thread));/* SDK background threads may retain this test-only desktop until process exit. */if(!CloseDesktop(f.desktop))CHECK(GetLastError()==ERROR_BUSY);CloseHandle(readEntered);CloseHandle(readRelease);CloseHandle(tailEntered);CloseHandle(tailRelease);
 printf("PASS toolchain source lease: %u checks; real snapshot I/O and worker tail, passive hold, cache retention, registered request lifetimes, OOM/thread failure, exact/exhausted tokens, nonblocking UI Poll and C/xlang input/delete/undo/IME/focus\n",checks);return 0;
}
