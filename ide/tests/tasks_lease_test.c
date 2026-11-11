#include "../src/services/tasks.c"
#include "app/xide_app.h"
#include "services/execution_target.h"
#include "ui/editor_view.h"
#include "ui/task_view.h"
#include "theme/theme.h"
#include <shellapi.h>
static volatile LONG assertions,holdExit,trackOwner;static DWORD owner;
static size_t ownerAllocations,ownerFrees;static HANDLE workerFinished,workerRelease;
#define CHECK(v) do{InterlockedIncrement(&assertions);if(!(v)){fprintf(stderr,"tasks-lease:%d: %s\n",__LINE__,#v);exit(1);}}while(0)
void* __real_malloc(size_t);void* __real_calloc(size_t,size_t);void __real_free(void*);
void* __wrap_malloc(size_t n){if(InterlockedCompareExchange(&trackOwner,0,0)&&GetCurrentThreadId()==owner)++ownerAllocations;return __real_malloc(n);}
void* __wrap_calloc(size_t n,size_t z){if(InterlockedCompareExchange(&trackOwner,0,0)&&GetCurrentThreadId()==owner)++ownerAllocations;return __real_calloc(n,z);}
void __wrap_free(void* p){if(InterlockedCompareExchange(&trackOwner,0,0)&&GetCurrentThreadId()==owner)++ownerFrees;__real_free(p);}
extern HANDLE (WINAPI* __real___imp_CreateThread)(LPSECURITY_ATTRIBUTES,SIZE_T,LPTHREAD_START_ROUTINE,LPVOID,DWORD,LPDWORD);
typedef struct invocation {LPTHREAD_START_ROUTINE routine;void* user;} invocation;
static DWORD WINAPI wrappedWorker(void* user)
{
 invocation* call=user;LPTHREAD_START_ROUTINE fn=call->routine;void* data=call->user;__real_free(call);DWORD result=fn(data);
 /* The exact real worker has published DONE, but its actual thread is still
  * alive. No fabricated task state or worker completion result is installed. */
 if(InterlockedCompareExchange(&holdExit,0,1)==1)CHECK(SetEvent(workerFinished)&&WaitForSingleObject(workerRelease,30000)==WAIT_OBJECT_0);
 return result;
}
static HANDLE WINAPI createProbe(LPSECURITY_ATTRIBUTES a,SIZE_T stack,LPTHREAD_START_ROUTINE fn,LPVOID user,DWORD flags,LPDWORD id)
{
 if(fn!=taskWorker)return __real___imp_CreateThread(a,stack,fn,user,flags,id);
 invocation* call=__real_malloc(sizeof(*call));if(!call)return NULL;*call=(invocation){fn,user};HANDLE h=__real___imp_CreateThread(a,stack,wrappedWorker,call,flags,id);if(!h)__real_free(call);return h;
}
HANDLE (WINAPI* __wrap___imp_CreateThread)(LPSECURITY_ATTRIBUTES,SIZE_T,LPTHREAD_START_ROUTINE,LPVOID,DWORD,LPDWORD)=createProbe;
static char* utf8(const wchar_t* s)
{int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s,-1,NULL,0,NULL,NULL);char* p=n?malloc(n):NULL;if(p&&!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s,-1,p,n,NULL,NULL)){free(p);p=NULL;}return p;}
static void emit(const char* s){DWORD n=0;CHECK(WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,(DWORD)strlen(s),&n,NULL)&&n==strlen(s));}
static int compilerHelper(int argc,char** argv)
{
 if(argc==2&&!strcmp(argv[1],"--help")){emit("xl build <file.xlp> -o <output>\n");return 0;}
 if(argc>=5&&!strcmp(argv[1],"build")){
  /* The build process intentionally receives build-only environment. Read our
   * fixture controls from the actual immutable request manifest, rather than
   * changing production environment semantics or global process variables. */
  xvalue* root=xrtJsonParseFile(argv[2]);CHECK(root);const xvalue* ide=xrtValueObjectGet(root,xrtStrView("ide"));const xvalue* configs=xrtValueObjectGet(ide,xrtStrView("configurations"));const xvalue* run=xrtValueObjectGet(xrtValueArrayGet(configs,0),xrtStrView("run"));const xvalue* environment=xrtValueObjectGet(run,xrtStrView("environment"));xstrview r={0},q={0},p={0};CHECK(xrtValueGetString(xrtValueObjectGet(environment,xrtStrView("XIDE_TASK_LEASE_READY")),&r)&&xrtValueGetString(xrtValueObjectGet(environment,xrtStrView("XIDE_TASK_LEASE_RELEASE")),&q)&&xrtValueGetString(xrtValueObjectGet(environment,xrtStrView("XIDE_TASK_LEASE_PROGRAM")),&p));
  wchar_t readyName[256],releaseName[256],program[32768];CHECK(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,r.Data,-1,readyName,256)&&MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,q.Data,-1,releaseName,256)&&MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,p.Data,-1,program,32768));xrtValueRelease(root);
  HANDLE ready=OpenEventW(EVENT_MODIFY_STATE,FALSE,readyName),release=OpenEventW(SYNCHRONIZE,FALSE,releaseName);CHECK(ready&&release&&SetEvent(ready)&&WaitForSingleObject(release,30000)==WAIT_OBJECT_0);CHECK(CloseHandle(ready)&&CloseHandle(release));wchar_t* target=xidePathNativeWide(argv[4]);int okay=target&&CopyFileW(program,target,FALSE);free(target);return okay?0:9;
 }
 return -1;
}
static void putEnvironment(xvalue* v,const char* key,const char* text)
{CHECK(xrtValueObjectSetNew(v,xrtStrView(key),xrtValueString(xrtStrView(text))));}
static void writeText(const char* path,const char* text){CHECK(xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)}));}
static xide_task_info info(xide_task* task){xide_task_info value={0};xideTaskInfo(task,&value);return value;}
static void waitRun(xide_tasks* tasks,xide_task* task)
{uint64_t until=GetTickCount64()+12000;do{xideTasksPoll(tasks);if(xideTaskInputOpen(task)&&info(task).phase==XIDE_TASK_RUN)return;CHECK(info(task).state<XIDE_TASK_SUCCEEDED);Sleep(5);}while(GetTickCount64()<until);CHECK(0);}
static void waitSuspended(xide_tasks* tasks,uint64_t token)
{uint64_t until=GetTickCount64()+12000;do{xideTasksPoll(tasks);if(xideTasksSuspended(tasks,token))return;Sleep(5);}while(GetTickCount64()<until);CHECK(0);}
static void waitDone(xide_tasks* tasks,xide_task* task)
{uint64_t until=GetTickCount64()+12000;do{xideTasksPoll(tasks);if(info(task).state>=XIDE_TASK_SUCCEEDED&&completedWorker(task))return;Sleep(5);}while(GetTickCount64()<until);CHECK(0);}
static void guiAdmission(const char* root,const char* compiler)
{
 xide_app app={.hidden=1,.noSession=1,.disableAutomaticDiagnostics=1,.viewportWidth=1280,.viewportHeight=800,.compilerPath=compiler};xide_model_error error={0};char* path=xidePathResolveLexical(root,"ui-main.c");CHECK(path);writeText(path,"int before;\n");CHECK(xuiCreate(&app.context)==XUI_OK&&ideThemeInit(app.context,IDE_THEME_CLASSIC)==XUI_OK&&xuiInputViewport(app.context,1280,800)==XUI_OK&&xideWorkbenchCreate(&app)==XUI_OK&&xideEditorOpen(&app,path,&error));
 xide_document* doc=app.activeDocument;CHECK(xuiCodeDocumentInsert(xuiCodeEditGetDocument(doc->editor),0,"// user\n")==XUI_OK&&doc->dirty);uint64_t token=xideTasksSuspend(app.tasks,&error);CHECK(token&&xideTasksSuspended(app.tasks,token)&&!xideTaskViewCanBuildFile(&app));
 for(int action=XIDE_ACTION_BUILD;action<=XIDE_ACTION_RUN;++action)CHECK(!xideExecutionQuery(&app,action).enabled);
 CHECK(!xideExecutionQuery(&app,XIDE_ACTION_BUILD_FILE).enabled&&!xideExecutionQuery(&app,XIDE_ACTION_RUN_FILE).enabled&&!xideExecutionQuery(&app,XIDE_ACTION_TEST).enabled);
 CHECK(xideTaskViewAction(&app,XIDE_ACTION_BUILD_FILE)&&doc->dirty&&!xideTasksCount(app.tasks));size_t n=0;unsigned char* disk=xrtFileReadAll(path,&n);CHECK(disk&&n==12&&!memcmp(disk,"int before;\n",12));xrtFree(disk);
 CHECK(xideTaskViewAction(&app,XIDE_ACTION_STOP_ALL)&&xideTasksResume(app.tasks,token,&error)&&xideTaskViewCanBuildFile(&app));xideAppDestroy(&app);free(path);
}
int main(void)
{
 int argc=0;wchar_t** wideArgs=CommandLineToArgvW(GetCommandLineW(),&argc);CHECK(wideArgs);char** argv=calloc((size_t)argc+1,sizeof(*argv));CHECK(argv);for(int i=0;i<argc;++i){argv[i]=utf8(wideArgs[i]);CHECK(argv[i]);}LocalFree(wideArgs);int helper=compilerHelper(argc,argv);if(helper>=0)return helper;
 CHECK(argc==3&&xrtDirCreateAll(argv[1]));owner=GetCurrentThreadId();xide_model_error error={0};char* manifest=xidePathResolveLexical(argv[1],"lease.xlp");char* entry=xidePathResolveLexical(argv[1],"main.xl");char* cfile=xidePathResolveLexical(argv[1],"helper.c");CHECK(manifest&&entry&&cfile);writeText(entry,"func value()\nend\n");writeText(cfile,"int value;\n");xide_project project={0};CHECK(xideProjectCreate(&project,manifest,"Lease",entry,&error));
 xide_configuration* config=project.configurations;char readyName[128],releaseName[128];snprintf(readyName,sizeof(readyName),"Local\\xide-lease-ready-%lu",(unsigned long)GetCurrentProcessId());snprintf(releaseName,sizeof(releaseName),"Local\\xide-lease-release-%lu",(unsigned long)GetCurrentProcessId());wchar_t readyWide[128],releaseWide[128];CHECK(MultiByteToWideChar(CP_UTF8,0,readyName,-1,readyWide,128)&&MultiByteToWideChar(CP_UTF8,0,releaseName,-1,releaseWide,128));HANDLE ready=CreateEventW(NULL,TRUE,FALSE,readyWide),release=CreateEventW(NULL,TRUE,FALSE,releaseWide);workerFinished=CreateEventW(NULL,TRUE,FALSE,NULL);workerRelease=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(ready&&release&&workerFinished&&workerRelease);
 putEnvironment(config->environment,"XIDE_TASK_LEASE_READY",readyName);putEnvironment(config->environment,"XIDE_TASK_LEASE_RELEASE",releaseName);putEnvironment(config->environment,"XIDE_TASK_LEASE_PROGRAM",argv[2]);putEnvironment(config->environment,"XIDE_TASK_MODE","");CHECK(xideProjectSave(&project,NULL,&error));xide_tasks* tasks=xideTasksCreate();CHECK(tasks);InterlockedExchange(&holdExit,1);
 xide_task* first=xideTasksBuild(tasks,&project,config->id,argv[0],1,&error);CHECK(first&&WaitForSingleObject(ready,12000)==WAIT_OBJECT_0);xide_task* queued=xideTasksBuild(tasks,&project,config->id,argv[0],0,&error);CHECK(queued&&queued!=first&&info(queued).state==XIDE_TASK_QUEUED);
 ownerAllocations=ownerFrees=0;InterlockedExchange(&trackOwner,1);uint64_t abandoned=xideTasksSuspend(tasks,&error);CHECK(abandoned&&!ownerAllocations&&!ownerFrees);InterlockedExchange(&trackOwner,0);CHECK(!xideTasksSuspended(tasks,abandoned)&&xideTasksResume(tasks,abandoned,&error));uint64_t token=xideTasksSuspend(tasks,&error);CHECK(token>abandoned&&!xideTasksResume(tasks,abandoned,&error)&&!xideTasksSuspend(tasks,&error));
 const char* paths[]={entry};xide_strings pending={(char**)paths,1,1};size_t count=xideTasksCount(tasks);ownerAllocations=ownerFrees=0;InterlockedExchange(&trackOwner,1);CHECK(!xideTasksBuild(tasks,&project,config->id,argv[0],1,&error)&&error.code==XIDE_MODEL_CONFLICT);CHECK(!xideTasksBuildGuarded(tasks,&project,config->id,argv[0],0,&pending,&error));CHECK(!xideTasksBuildFileGuarded(tasks,cfile,argv[0],1,&pending,&error));CHECK(!xideTasksTest(tasks,&project,config->id,&error)&&xideTasksCount(tasks)==count);InterlockedExchange(&trackOwner,0);CHECK(!ownerAllocations&&!ownerFrees);EnterCriticalSection(&first->lock);int unchanged=!first->invalidated&&!first->changedPaths.count;LeaveCriticalSection(&first->lock);CHECK(unchanged);
 CHECK(SetEvent(release));waitRun(tasks,first);CHECK(!xideTasksSuspended(tasks,token)&&info(queued).state==XIDE_TASK_QUEUED);CHECK(xideTaskWrite(first,"still usable\n",13));xideTaskCloseInput(first);CHECK(WaitForSingleObject(workerFinished,12000)==WAIT_OBJECT_0);CHECK(info(first).state==XIDE_TASK_SUCCEEDED&&!xideTasksSuspended(tasks,token)&&xideTasksBusy(tasks));
 xideTasksPoll(tasks);CHECK(info(queued).state==XIDE_TASK_QUEUED);xideTaskCancel(queued);CHECK(info(queued).state==XIDE_TASK_CANCELLED&&xideTasksBusy(tasks));CHECK(xideTasksResume(tasks,token,&error));queued=xideTasksBuild(tasks,&project,config->id,argv[0],0,&error);CHECK(queued&&info(queued).state==XIDE_TASK_QUEUED);token=xideTasksSuspend(tasks,&error);CHECK(token);CHECK(SetEvent(workerRelease));waitSuspended(tasks,token);CHECK(info(queued).state==XIDE_TASK_QUEUED&&xideTasksBusy(tasks));
 /* Input invalidation remains a separate explicit responsibility of the holder. */
 writeText(entry,"func changed()\nend\n");xideTasksInvalidateInput(tasks,entry);CHECK(xideTasksResume(tasks,token,&error));waitDone(tasks,queued);CHECK(info(queued).state==XIDE_TASK_FAILED&&info(queued).invalidated&&!info(queued).built&&!info(queued).ran);
 xide_task* fresh=xideTasksBuild(tasks,&project,config->id,argv[0],0,&error);CHECK(fresh&&fresh!=queued);waitDone(tasks,fresh);CHECK(info(fresh).state==XIDE_TASK_SUCCEEDED&&info(fresh).built);CHECK(!xideTasksAdmissionHeld(tasks));
 token=xideTasksSuspend(tasks,&error);CHECK(token&&xideTasksSuspended(tasks,token)&&!xideTasksBusy(tasks));tasks->nextLease=UINT64_MAX;CHECK(xideTasksResume(tasks,token,&error)&&!xideTasksSuspend(tasks,&error));xideTasksDestroy(tasks);CHECK(CloseHandle(ready)&&CloseHandle(release)&&CloseHandle(workerFinished)&&CloseHandle(workerRelease));xideProjectClear(&project);guiAdmission(argv[1],argv[0]);free(manifest);free(entry);free(cfile);for(int i=0;i<argc;++i)free(argv[i]);free(argv);
 printf("tasks_lease_test passed: %ld checks; real compiler/run workers and terminal-state-before-thread-exit, passive active work, frozen queue, guarded submissions and UI pre-save admission, early abandonment, input invalidation, exact/exhausted tokens\n",assertions);return 0;
}
