#include "services/language_check_internal.h"
#include <stdio.h>
#include <string.h>
static HANDLE entered,resume;
static volatile LONG gated,cancels;
void __real_xlsQueryCancel(xls_query* query);
void __wrap_xlsQueryCancel(xls_query* query)
{if(query)InterlockedIncrement(&cancels);__real_xlsQueryCancel(query);}
static int observedWake(xls_query* query,void (*wake)(void*),void* user)
{
 int okay=xlsQuerySetWake(query,wake,user);
 if(InterlockedCompareExchange(&gated,0,1)==1){SetEvent(entered);WaitForSingleObject(resume,30000);}
 return okay;
}
#define xlsQuerySetWake observedWake
#include "../src/services/language_snapshot.c"
#undef xlsQuerySetWake
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"language_native_cancel_test:%d:%s\n",__LINE__,#v);return 1;}}while(0)
static int finish(xide_language_check* check)
{enum xide_check_state state=XIDE_CHECK_RUNNING;for(unsigned i=0;i<30000 && state==XIDE_CHECK_RUNNING;++i){state=xideLanguageCheckState(check,NULL,0);if(state==XIDE_CHECK_RUNNING)xrtSleep(1);}return state!=XIDE_CHECK_RUNNING;}
int main(void)
{
 char *directory=xrtPathAbs("build/language-native-cancel"),*entry=xrtPathJoin(directory,"main.xl"),*compiler=xrtPathJoin(directory,"absent-xl.exe");const char* body="func main(...) as int\n return 1\nend\n";CHECK(directory && entry && compiler && xrtDirCreateAll(directory) && xrtFileWriteAtomic(entry,(xbytesview){(const unsigned char*)body,strlen(body)}));
 xls_context_spec spec={.kind=XLS_CONTEXT_XLANG,.entry=entry,.directory=directory,.analysisProfile="xlang-ls/compiler-xrt-v1",.targetKind="exe"};xls_context* context=xlsContextCreate(&spec,16u*1024u*1024u,NULL);xls_worker* actor=xlsWorkerCreate(NULL,8);xide_toolchains* cache=xideToolchainsCreate();xide_language_analysis_options options={.nativeBackend=1};entered=CreateEventW(NULL,TRUE,FALSE,NULL);resume=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(context && actor && cache && entered && resume);
 for(unsigned round=0;round<3;++round){CHECK(ResetEvent(entered) && ResetEvent(resume));InterlockedExchange(&gated,1);LONG before=InterlockedCompareExchange(&cancels,0,0);xide_language_check* check=xideLanguageCheckStartDeclarationCapture(context,cache,compiler,actor,NULL,0,&options,NULL);CHECK(check && WaitForSingleObject(entered,30000)==WAIT_OBJECT_0);
  EnterCriticalSection(&check->lock);int bound=check->activeNativeQuery!=NULL;LeaveCriticalSection(&check->lock);CHECK(bound);
  /* The helper is pinned inside SetWake, so its normal stop-event path cannot
     forward cancellation. This call must reach the real native future now. */
  xideLanguageCheckCancel(check);CHECK(InterlockedCompareExchange(&cancels,0,0)>before);CHECK(SetEvent(resume) && finish(check));
  EnterCriticalSection(&check->lock);bound=check->activeNativeQuery!=NULL;LeaveCriticalSection(&check->lock);CHECK(!bound);before=InterlockedCompareExchange(&cancels,0,0);xideLanguageCheckCancel(check);CHECK(InterlockedCompareExchange(&cancels,0,0)==before);xideLanguageCheckDestroy(check);
 }
 InterlockedExchange(&gated,0);xide_language_check* check=xideLanguageCheckStartDeclarationCapture(context,cache,compiler,actor,NULL,0,&options,NULL);CHECK(check && finish(check) && xideLanguageCheckState(check,NULL,0)==XIDE_CHECK_READY);CHECK(!check->activeNativeQuery);xideLanguageCheckDestroy(check);
 CloseHandle(entered);CloseHandle(resume);xideToolchainsDestroy(cache);xlsWorkerDestroy(actor);xlsContextRelease(context);xrtFree(directory);xrtFree(entry);xrtFree(compiler);printf("language_native_cancel_test passed: %u checks;actual future cancellation before pinned helper resumes,unbind before destruction,repeated cancellation and actor recovery.\n",checks);return 0;
}
