#include "analysis_test_clock.h"
/* Real worker and original core. Only optional session allocation and the
 * existing cancellation predicate are controlled to visit every commit gate. */
#define main dependencySessionFixtureMain
#include "analysis_dependency_session_test.c"
#undef main
#include "analysis/worker.h"
#include "analysis/worker_modules.h"
static xatomic32 faultCalls,faultStop,faultCreate;
static bool dependencyRequested(const xcancel* cancel)
{
 uint32 stop=xrtAtomic32Load(&faultStop,XMEMORY_ACQUIRE),at=xrtAtomic32FetchAdd(&faultCalls,1,XMEMORY_RELAXED)+1;
 return xrtCancelRequested(cancel) || (stop && at>=stop);
}
static xls_dependency_session* dependencyCreate(const xls_session_limits* limits)
{return xrtAtomic32Load(&faultCreate,XMEMORY_ACQUIRE) ? NULL : xlsDependencySessionCreate(limits);}
#define xrtCancelRequested dependencyRequested
#define xlsDependencySessionCreate dependencyCreate
#include "analysis/worker.c"
#undef xrtCancelRequested
#undef xlsDependencySessionCreate
typedef struct outcome {enum xls_query_state state;xls_dependency_capture* value;xls_dependency_session_stats stats;xls_worker_cache_stats cache;uint64_t thread;char error[256];} outcome;
static outcome awaitDependency(xls_query* q)
{
 outcome r={0};uint64_t until=xlsTestClockMicros()+15000000;
 do{r.state=xlsQueryDependencyResult(q,&r.value,&r.stats,&r.thread,r.error,sizeof(r.error));if(r.state!=XLS_QUERY_PENDING)break;xlsTestSleepMicros(100);}while(xlsTestClockMicros()<until);
 xlsQuerySessionCache(q,&r.cache);return r;
}
static outcome queryDependency(xls_worker* w,xls_context* ctx,const xls_analysis_document* docs,size_t count,const char* target)
{xls_query* q=xlsWorkerDependenciesInContext(w,ctx,docs,count,target);outcome r=awaitDependency(q);xlsQueryDestroy(q);return r;}
static void releaseDependency(outcome* r){xlsDependencyCaptureRelease(r->value);r->value=NULL;}
static void wakeDependency(void* p){xrtAtomic32FetchAdd(p,1,XMEMORY_RELAXED);}
typedef struct dependency_reader {xls_dependency_capture* value;int okay;} dependency_reader;
static int32 readDependency(void* p)
{
 dependency_reader* r=p;r->okay=1;
 for(unsigned i=0;i<128;++i){xls_check_source source;if(!xlsDependencyCaptureSource(r->value,i%2,&source) || !source.path || !source.text || !xlsDependencyCaptureInfo(r->value).expanded || xlsModuleGraphInfo(xlsDependencyCaptureModules(r->value)).sources!=2)r->okay=0;}
 xlsDependencyCaptureRelease(r->value);return 0;
}
static int workers(void)
{
 fixture f={0};CHECK(setup(&f));xls_module_store* store=xlsModuleStoreCreate(NULL);CHECK(store);xls_worker* w=xlsWorkerCreateWithModules(NULL,4,NULL,store);CHECK(w);
 xls_query* q=xlsWorkerDependenciesInContext(w,f.context,f.docs,3,"main");CHECK(q);xatomic32 wakes;xrtAtomic32Init(&wakes,0);CHECK(xlsQuerySetWake(q,wakeDependency,&wakes));
 outcome a=awaitDependency(q);CHECK(a.state==XLS_QUERY_READY && a.value && a.thread && a.thread!=xrtThreadCurrentId() && a.stats.builds==1 && a.cache.sessions==1);CHECK(parity(a.value,f.context,f.docs,3,f.entry));
 xls_semantic_check* wrong=NULL;CHECK(xlsQueryCheckResult(q,&wrong,NULL,NULL,NULL,0)==XLS_QUERY_FAILED && !wrong);xlsQueryDestroy(q);CHECK(xrtAtomic32Load(&wakes,XMEMORY_ACQUIRE)==1);
 for(unsigned i=0;i<32;++i){f.docs[0].revision=2+i;outcome b=queryDependency(w,f.context,f.docs,3,"main");CHECK(b.state==XLS_QUERY_READY && b.value==a.value && b.thread==a.thread && b.stats.builds==1 && b.stats.hits==i+1 && b.cache.hits==i+1);releaseDependency(&b);}
 f.docs[0].revision=1;outcome b=queryDependency(w,f.context,f.docs,3,"main");CHECK(b.state==XLS_QUERY_FAILED && !b.value && !b.thread && !b.stats.builds && strstr(b.error,"revision"));
 f.docs[0].revision=34;b=queryDependency(w,f.libraryContext,f.docs,3,"lib");CHECK(b.state==XLS_QUERY_READY && xlsDependencyCaptureInfo(b.value).expanded && b.cache.sessions==2);CHECK(parity(b.value,f.libraryContext,f.docs,3,f.library));releaseDependency(&b);
 /* Revision history belongs to the whole dependency stream,not the selected root. */
 f.docs[0].revision=33;b=queryDependency(w,f.context,f.docs,3,"main");CHECK(b.state==XLS_QUERY_FAILED && !b.value);f.docs[0].revision=35;
 b=queryDependency(w,f.context,f.docs,3,"main");CHECK(b.state==XLS_QUERY_READY && b.value==a.value && b.stats.builds==2);releaseDependency(&b);
 CHECK(!xlsWorkerDependenciesInContext(w,NULL,NULL,0,NULL) && !xlsWorkerDependenciesInContext(w,f.context,NULL,0,"main") && !xlsWorkerDependenciesInContext(w,f.context,NULL,1,NULL));
 CHECK(!xlsWorkerCheckInContext(w,f.context,NULL,0,NULL));
 b=queryDependency(w,f.context,NULL,0,NULL);CHECK(b.state==XLS_QUERY_READY && xlsDependencyCaptureInfo(b.value).expanded && b.stats.documents==0 && parity(b.value,f.context,NULL,0,f.entry));releaseDependency(&b);
 b=queryDependency(w,f.context,NULL,0,NULL);CHECK(b.state==XLS_QUERY_READY && b.stats.builds==3);releaseDependency(&b);
 xlsWorkerDestroy(w);xlsModuleStoreRelease(store);CHECK(parity(a.value,f.context,f.docs,3,f.entry));
 dependency_reader readers[4];xthread* threads[4];for(unsigned i=0;i<4;++i){readers[i]=(dependency_reader){xlsDependencyCaptureRetain(a.value),0};threads[i]=xrtThreadCreate(readDependency,readers+i,0);CHECK(threads[i]);}releaseDependency(&a);for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}
 /* Shared LRU and known-input quota preserve the existing active exception. */
 w=xlsWorkerCreateWithCache(NULL,4,&(xls_worker_cache_limits){1,1});CHECK(w);a=queryDependency(w,f.context,f.docs,3,"main");CHECK(a.state==XLS_QUERY_READY && a.cache.sessions==1 && a.cache.inputBudgetExceeded && a.cache.inputStorageBytes>1);
 b=queryDependency(w,f.libraryContext,f.docs,3,"lib");CHECK(b.state==XLS_QUERY_READY && b.cache.sessions==1 && b.cache.evictions==1);releaseDependency(&b);xlsWorkerDestroy(w);releaseDependency(&a);
 /* Optional allocation failure falls back to an existing same-kind session. */
 w=xlsWorkerCreate(NULL,4);CHECK(w);a=queryDependency(w,f.context,f.docs,3,"main");CHECK(a.state==XLS_QUERY_READY);xrtAtomic32Store(&faultCreate,1,XMEMORY_RELEASE);b=queryDependency(w,f.libraryContext,f.docs,3,"lib");CHECK(b.state==XLS_QUERY_READY && b.cache.allocationFallbacks==1 && b.cache.evictions==1 && b.cache.sessions==1 && parity(b.value,f.libraryContext,f.docs,3,f.library));xrtAtomic32Store(&faultCreate,0,XMEMORY_RELEASE);releaseDependency(&a);releaseDependency(&b);xlsWorkerDestroy(w);
 w=xlsWorkerCreate(NULL,4);CHECK(w);xls_text* broken=xlsTextCreate("func broken(\n",13,1024*1024,NULL);CHECK(broken);f.docs[0].text=broken;f.docs[0].revision=36;b=queryDependency(w,f.context,f.docs,3,"main");CHECK(b.state==XLS_QUERY_READY && b.value && !xlsDependencyCaptureInfo(b.value).expanded && xlsDependencyCaptureInfo(b.value).diagnostics && !*b.error);releaseDependency(&b);xlsWorkerDestroy(w);xlsTextRelease(broken);
 CHECK(!semCalls && !irCalls);clear(&f);return 1;
}
static int cancellationTransactions(void)
{
 fixture f={0};CHECK(setup(&f));unsigned total=0;
 /* Exact hit,used-overlay rebuild and a different entry exercise both
    retained checkpoints and staged sessions,with one/eight retained slots. */
 for(unsigned slots=1;slots<=8;slots+=7)for(unsigned mode=0;mode<3;++mode){
  xls_analysis_document changed[3];memcpy(changed,f.docs,sizeof(changed));changed[1].revision=2;if(mode==1)changed[1].text=f.editText;
  const char* target=mode==2 ? "lib" : "main";xls_context* ctx=mode==2 ? f.libraryContext : f.context;
  xls_worker* w=xlsWorkerCreateWithCache(NULL,4,&(xls_worker_cache_limits){slots,64u*1024u*1024u});CHECK(w);outcome seed=queryDependency(w,f.context,f.docs,3,"main");CHECK(seed.state==XLS_QUERY_READY);releaseDependency(&seed);
  xrtAtomic32Store(&faultCalls,0,XMEMORY_RELEASE);outcome r=queryDependency(w,ctx,changed,3,target);unsigned gates=xrtAtomic32Load(&faultCalls,XMEMORY_ACQUIRE);CHECK(r.state==XLS_QUERY_READY && gates>3);releaseDependency(&r);xlsWorkerDestroy(w);total+=gates;
  for(unsigned stop=1;stop<=gates;++stop){
   w=xlsWorkerCreateWithCache(NULL,4,&(xls_worker_cache_limits){slots,64u*1024u*1024u});CHECK(w);seed=queryDependency(w,f.context,f.docs,3,"main");CHECK(seed.state==XLS_QUERY_READY);
   xrtAtomic32Store(&faultCalls,0,XMEMORY_RELEASE);xrtAtomic32Store(&faultStop,stop,XMEMORY_RELEASE);r=queryDependency(w,ctx,changed,3,target);CHECK(r.state==XLS_QUERY_CANCELLED && !r.value && !r.thread && !r.stats.builds && !r.cache.sessions);
   xrtAtomic32Store(&faultStop,0,XMEMORY_RELEASE);r=queryDependency(w,f.context,f.docs,3,"main");CHECK(r.state==XLS_QUERY_READY && r.value==seed.value && r.cache.sessions==1 && !r.cache.evictions);CHECK(parity(r.value,f.context,f.docs,3,f.entry));releaseDependency(&r);releaseDependency(&seed);xlsWorkerDestroy(w);
  }
 }
 CHECK(!semCalls && !irCalls);printf("Dependency worker cancellation gates=%u;all visited including final post-session commit gate\n",total);clear(&f);return 1;
}
int main(void)
{
 xrtAtomic32Init(&faultCalls,0);xrtAtomic32Init(&faultStop,0);xrtAtomic32Init(&faultCreate,0);
 if(!workers() || !cancellationTransactions())return 1;
 printf("analysis_dependency_worker_test passed: %u checks;real XRT actor,retained lifetimes,wake,per-stream versions,closed entry,shared quota/LRU,allocation fallback,all calibrated cancellation gates,fresh original source/probe comparisons,zero observed semantic/IR builders\n",checks);return 0;
}
