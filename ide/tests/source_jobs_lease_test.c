#define _WIN32_WINNT 0x0601
#include "services/template_generation.c"
#define copy recoveryCopy
#include "services/recovery.c"
#undef copy
static DWORD owner;
static unsigned checks;
static volatile LONG tracking,allocations,frees,failAllocation,allocationAt,failThread;
static volatile LONG readGate,writeGate,tailGate,readHit,writeHit,tailHit,timeout;
static HANDLE entered,releaseIo,tailEntered,releaseTail;
static xide_source_jobs* reenter;
static int reentered,reentrantAccepted;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"source-jobs:%d:%s; Win32=%lu\n",__LINE__,#v,GetLastError());ExitProcess(1);}}while(0)
void* __real_malloc(size_t);void* __real_calloc(size_t,size_t);void __real_free(void*);
static int allocation(void)
{
 if(GetCurrentThreadId()!=owner)return 0;
 if(tracking)++allocations;
 if(reenter && !reentered){reentered=1;reentrantAccepted=xideSourceJobsSuspend(reenter,NULL)!=0;}
 return failAllocation && ++allocationAt==failAllocation;
}
void* __wrap_malloc(size_t n){return allocation()?NULL:__real_malloc(n);}
void* __wrap_calloc(size_t n,size_t z){return allocation()?NULL:__real_calloc(n,z);}
void __wrap_free(void* p){if(tracking && GetCurrentThreadId()==owner)++frees;__real_free(p);}
extern BOOL(WINAPI* __real___imp_ReadFile)(HANDLE,LPVOID,DWORD,LPDWORD,LPOVERLAPPED);
extern BOOL(WINAPI* __real___imp_WriteFile)(HANDLE,LPCVOID,DWORD,LPDWORD,LPOVERLAPPED);
static void ioGate(volatile LONG* gate,volatile LONG* hit)
{if(GetCurrentThreadId()!=owner && InterlockedCompareExchange(gate,0,1)==1){*hit=1;SetEvent(entered);if(WaitForSingleObject(releaseIo,20000)!=WAIT_OBJECT_0)timeout=1;}}
static BOOL WINAPI readProbe(HANDLE h,LPVOID p,DWORD n,LPDWORD got,LPOVERLAPPED at)
{ioGate(&readGate,&readHit);return __real___imp_ReadFile(h,p,n,got,at);}
static BOOL WINAPI writeProbe(HANDLE h,LPCVOID p,DWORD n,LPDWORD got,LPOVERLAPPED at)
{ioGate(&writeGate,&writeHit);return __real___imp_WriteFile(h,p,n,got,at);}
BOOL(WINAPI* __wrap___imp_ReadFile)(HANDLE,LPVOID,DWORD,LPDWORD,LPOVERLAPPED)=readProbe;
BOOL(WINAPI* __wrap___imp_WriteFile)(HANDLE,LPCVOID,DWORD,LPDWORD,LPOVERLAPPED)=writeProbe;
extern HANDLE(WINAPI* __real___imp_CreateThread)(LPSECURITY_ATTRIBUTES,SIZE_T,LPTHREAD_START_ROUTINE,LPVOID,DWORD,LPDWORD);
typedef struct invocation{LPTHREAD_START_ROUTINE proc;void* data;} invocation;
static DWORD WINAPI workerProbe(void* p)
{
 invocation call=*(invocation*)p;__real_free(p);DWORD result=call.proc(call.data);
 if(tailGate){InterlockedIncrement(&tailHit);SetEvent(tailEntered);if(WaitForSingleObject(releaseTail,20000)!=WAIT_OBJECT_0)timeout=1;}
 return result;
}
static HANDLE WINAPI threadProbe(LPSECURITY_ATTRIBUTES a,SIZE_T stack,LPTHREAD_START_ROUTINE proc,LPVOID data,DWORD flags,LPDWORD id)
{
 if(proc!=generate && proc!=worker)return __real___imp_CreateThread(a,stack,proc,data,flags,id);
 if(failThread){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return NULL;}
 invocation* p=__real_malloc(sizeof(*p));if(!p)return NULL;*p=(invocation){proc,data};HANDLE h=__real___imp_CreateThread(a,stack,workerProbe,p,flags,id);if(!h)__real_free(p);return h;
}
HANDLE(WINAPI* __wrap___imp_CreateThread)(LPSECURITY_ATTRIBUTES,SIZE_T,LPTHREAD_START_ROUTINE,LPVOID,DWORD,LPDWORD)=threadProbe;
static void arm(int read)
{ResetEvent(entered);ResetEvent(releaseIo);ResetEvent(tailEntered);ResetEvent(releaseTail);readHit=writeHit=tailHit=timeout=0;readGate=read;writeGate=!read;tailGate=1;}
static void releaseAll(void)
{readGate=writeGate=tailGate=0;SetEvent(releaseIo);SetEvent(releaseTail);}
static xide_source_jobs_status state(xide_source_jobs* jobs,uint64_t token)
{xide_source_jobs_status s={0};CHECK(xideSourceJobsState(jobs,token,&s));return s;}
static void waitStopped(xide_source_jobs* jobs,uint64_t token)
{ULONGLONG end=GetTickCount64()+20000;while(!state(jobs,token).quiet){CHECK(GetTickCount64()<end);Sleep(1);}CHECK(!timeout);}
static void verify(const char* directory,const char* compiler)
{
 xide_source_jobs jobs={0};xide_model_error e={0};xide_recovery_service* recovery=xideRecoveryServiceCreate(&jobs,NULL);CHECK(recovery);
 tracking=1;allocations=frees=0;uint64_t token=xideSourceJobsSuspend(&jobs,&e);CHECK(token && state(&jobs,token).quiet);
 CHECK(!xideTemplateJobStart(&jobs,(const void*)1,&e) && !allocations && !frees);
 CHECK(!xideRecoveryServiceStart(recovery,XIDE_RECOVERY_GLOBAL,(const void*)1,&e));
 CHECK(!xideRecoveryServiceCreate(&jobs,(const void*)1) && !allocations && !frees);tracking=0;
 CHECK(xideSourceJobsHeld(&jobs) && !xideSourceJobsSuspend(&jobs,&e) && !xideSourceJobsResume(&jobs,token+1,&e) && !xideSourceJobsResume(&jobs,0,&e));
 CHECK(xideSourceJobsResume(&jobs,token,&e) && !xideSourceJobsState(&jobs,token,&(xide_source_jobs_status){0}));
 CHECK(xideSourceJobsBegin(&jobs,&e) && !xideSourceJobsSuspend(&jobs,&e));xideSourceJobsEnd(&jobs);
 xide_source_jobs exhausted={.serial=UINT64_MAX};CHECK(!xideSourceJobsSuspend(&exhausted,&e));
 xide_template_registry registry={0};CHECK(xideTemplateRegistryLoad(&registry,NULL,&e));xvalue* values=xrtValueObject();CHECK(values);
 xide_template_request q={.definition=&registry.items[1],.values=values,.name="source-jobs-project",.location=directory,.compiler=compiler,.scratch=directory,.projectDirectory=1};
 arm(0);xide_template_job* job=xideTemplateJobStart(&jobs,&q,&e);CHECK(job && WaitForSingleObject(entered,15000)==WAIT_OBJECT_0 && writeHit);
 tracking=1;allocations=frees=0;token=xideSourceJobsSuspend(&jobs,&e);CHECK(token && !state(&jobs,token).quiet && !state(&jobs,token).invalidated && !allocations && !frees);tracking=0;
 CHECK(!xideTemplateJobDone(job) && !xideTemplateJobStopped(job));CHECK(SetEvent(releaseIo) && WaitForSingleObject(tailEntered,15000)==WAIT_OBJECT_0);
 CHECK(xideTemplateJobDone(job) && xideTemplateJobSucceeded(job) && !xideTemplateJobStopped(job) && !state(&jobs,token).quiet);
 releaseAll();waitStopped(&jobs,token);CHECK(xideTemplateJobStopped(job) && state(&jobs,token).registered==1);
 CHECK(xideSourceJobsResume(&jobs,token,&e));xideTemplateJobDestroy(job);CHECK(!jobs.count && !jobs.first);
 char* path=xidePathResolve(directory,"state.json");CHECK(path && xideIoWriteAtomic(path,(xbytesview){(const unsigned char*)"{}",2}));
 arm(1);CHECK(xideRecoveryServiceStart(recovery,XIDE_RECOVERY_GLOBAL,path,&e) && WaitForSingleObject(entered,15000)==WAIT_OBJECT_0 && readHit);
 token=xideSourceJobsSuspend(&jobs,&e);CHECK(token && !state(&jobs,token).quiet);xideRecoveryServiceCancel(recovery);
 CHECK(SetEvent(releaseIo) && WaitForSingleObject(tailEntered,15000)==WAIT_OBJECT_0 && !state(&jobs,token).quiet);
 xide_recovery_result result={.job=777};CHECK(!xideRecoveryServiceTake(recovery,&result) && result.job==777);
 releaseAll();waitStopped(&jobs,token);CHECK(!xideRecoveryServiceTake(recovery,&result) && result.job==777 && xideRecoveryServiceBusy(recovery));
 CHECK(xideSourceJobsResume(&jobs,token,&e) && xideRecoveryServiceTake(recovery,&result));xideRecoveryResultClear(&result,1);CHECK(!jobs.count && !jobs.admissions);
 /* Completed owners may be retained or destroyed in any order. Unlinking
  * under a hold revokes ordinary write admission without pretending a worker
  * is still live, so a host can separately settle an already-started restore. */
 q.definition=&registry.items[4];q.name="";xide_template_job* a=xideTemplateJobStart(&jobs,&q,&e),*b=xideTemplateJobStart(&jobs,&q,&e);CHECK(a&&b);
 CHECK(xideRecoveryServiceStart(recovery,XIDE_RECOVERY_GLOBAL,path,&e));token=xideSourceJobsSuspend(&jobs,&e);CHECK(token);waitStopped(&jobs,token);CHECK(state(&jobs,token).registered==3);
 xideTemplateJobDestroy(a);CHECK(state(&jobs,token).quiet && state(&jobs,token).invalidated && state(&jobs,token).registered==2);xideTemplateJobDestroy(b);
 CHECK(xideSourceJobsResume(&jobs,token,&e) && xideRecoveryServiceTake(recovery,&result));xideRecoveryResultClear(&result,1);CHECK(!jobs.first && !jobs.count);
 int failures=0;
 for(int n=1;n<=48;++n){allocationAt=0;failAllocation=n;job=xideTemplateJobStart(&jobs,&q,&e);failAllocation=0;if(job){xideTemplateJobDestroy(job);break;}++failures;CHECK(!jobs.count && !jobs.admissions);token=xideSourceJobsSuspend(&jobs,&e);CHECK(token && state(&jobs,token).quiet && xideSourceJobsResume(&jobs,token,&e));}
 CHECK(failures>0);failThread=1;CHECK(!xideTemplateJobStart(&jobs,&q,&e) && !jobs.count && !jobs.admissions);CHECK(!xideRecoveryServiceStart(recovery,XIDE_RECOVERY_GLOBAL,path,&e) && !jobs.count && !jobs.admissions);failThread=0;
 reenter=&jobs;reentered=reentrantAccepted=0;job=xideTemplateJobStart(&jobs,&q,&e);reenter=NULL;CHECK(job && reentered && !reentrantAccepted);xideTemplateJobDestroy(job);
 CHECK(!jobs.count && !jobs.admissions);xideRecoveryServiceDestroy(recovery);xrtValueRelease(values);xideTemplateRegistryClear(&registry);free(path);
}
int main(int argc,char** argv)
{
 if(argc!=3 || !xideIoCreateDirectories(argv[1])){return 2;}owner=GetCurrentThreadId();entered=CreateEventW(NULL,TRUE,FALSE,NULL);releaseIo=CreateEventW(NULL,TRUE,FALSE,NULL);tailEntered=CreateEventW(NULL,TRUE,FALSE,NULL);releaseTail=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(entered && releaseIo && tailEntered && releaseTail);
 verify(argv[1],argv[2]);CloseHandle(entered);CloseHandle(releaseIo);CloseHandle(tailEntered);CloseHandle(releaseTail);printf("PASS source job lease: %u checks; real template writes,recovery reads,physical worker tails,passive holds,held publication,exact/exhausted tokens,allocation/thread failure,prefix retirement,reentrant construction\n",checks);return 0;
}
