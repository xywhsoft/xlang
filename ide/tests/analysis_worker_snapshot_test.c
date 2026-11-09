#include "analysis/worker.h"
#include "analysis/session_state.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static unsigned nativeCalls,checks;
static xls_semantic_check* observedCheck(xls_check_session* s,xls_context* c,const xls_input_batch* h,
 const xls_analysis_document* d,size_t n,const char* t,int index,int (*cancel)(void*),void* user,char* error,size_t cap)
{++nativeCalls;return xlsCheckSessionCheckWithHistory(s,c,h,d,n,t,index,cancel,user,error,cap);}
#define xlsCheckSessionCheckWithHistory observedCheck
#include "../src/analysis/worker.c"
#undef xlsCheckSessionCheckWithHistory
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_worker_snapshot_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
typedef struct observed {size_t count;unsigned calls;xls_check_stats work;xls_worker_cache_stats cache;xls_input_batch* history;void* first;} observed;
static observed observe(xls_worker* w)
{return (observed){w->cachedCount,nativeCalls,w->checkTotals,w->cacheStats,w->history[0],w->cachedCount ? w->cached[0].value.check : NULL};}
static int unchanged(xls_worker* w,observed before)
{observed now=observe(w);return now.count==before.count && now.calls==before.calls && now.history==before.history && now.first==before.first && !memcmp(&now.work,&before.work,sizeof(now.work)) && !memcmp(&now.cache,&before.cache,sizeof(now.cache));}
static void wake(void* p){xrtEventSet(p);}
static enum xls_query_state get(xls_query* q,xls_semantic_check** result,uint64_t* thread)
{
 xevent event;*result=NULL;if(!q || !xrtEventInit(&event,false,false))return XLS_QUERY_FAILED;
 char error[256];enum xls_query_state state=XLS_QUERY_FAILED;
 if(xlsQuerySetWake(q,wake,&event) && xrtEventWaitFor(&event,30000000)==XWAIT_OK)state=xlsQueryCheckResult(q,result,NULL,thread,error,sizeof(error));
 xlsQueryDestroy(q);xrtEventUnit(&event);return state;
}
static int miss(xls_worker* w,xls_context* c,const xls_analysis_document* docs,size_t count,const char* target)
{
 observed before=observe(w);xls_semantic_check* result=NULL;
 CHECK(get(xlsWorkerCachedCheckInContext(w,c,docs,count,target),&result,NULL)==XLS_QUERY_FAILED && !result);
 CHECK(unchanged(w,before));return 1;
}
typedef struct blocked {xevent entered,release;} blocked;
static xtaskoutcome block(xcancel* cancel,void* input,xtaskvalue* output)
{(void)cancel;(void)output;blocked* b=input;xrtEventSet(&b->entered);xrtEventWaitFor(&b->release,30000000);return XTASK_SUCCESS;}
static int run(void)
{
 char *dir=xrtPathAbs("build/worker-snapshot"),*entry=xrtPathJoin(dir,"main.xl"),*helper=xrtPathJoin(dir,"helper.xl"),*other=xrtPathJoin(dir,"other.xl");
 CHECK(dir && entry && helper && other && xrtDirCreateAll(dir));
 const char *body="import helper\nfunc main(...) as int\n return helper.value()\nend\n",*library="export func value() as int\n return 3\nend\n",*changed="export func value() as int\n return 4\nend\n";
 CHECK(xrtFileWriteAtomic(entry,(xbytesview){(const unsigned char*)body,strlen(body)}) && xrtFileWriteAtomic(helper,(xbytesview){(const unsigned char*)library,strlen(library)}));
 xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project="snapshot.xlp",.configuration="Debug",.entry=entry,.directory=dir,.analysisProfile="xlang-ls/compiler-xrt-v1",.targetKind="exe"};
 xls_context* context=xlsContextCreate(&spec,1024*1024,NULL);spec.configuration="Release";xls_context* alternate=xlsContextCreate(&spec,1024*1024,NULL);spec.configuration="Debug";spec.entry=helper;xls_context* wrongEntry=xlsContextCreate(&spec,1024*1024,NULL);
 xls_worker* worker=xlsWorkerCreate(NULL,8);xls_text* text=xlsTextCreate(body,strlen(body),1024*1024,NULL);xls_text* equal=xlsTextCreate(body,strlen(body),1024*1024,NULL);
 xls_analysis_document docs[2]={{"entry",entry,text,10,1},{"extra",other,equal,20,1}};CHECK(context && alternate && wrongEntry && worker && text && equal);
 CHECK(miss(worker,context,docs,1,NULL) && !worker->cachedCount && !worker->history[0] && !nativeCalls);
 xls_semantic_check *warm=NULL,*snapshot=NULL;uint64_t thread=0,snapshotThread=0;
 CHECK(get(xlsWorkerCheckInContext(worker,context,docs,1,"entry"),&warm,&thread)==XLS_QUERY_READY && xlsSemanticCheckInfo(warm).accepted && nativeCalls==1);
 observed before=observe(worker);
 CHECK(get(xlsWorkerCachedCheckInContext(worker,context,docs,1,NULL),&snapshot,&snapshotThread)==XLS_QUERY_READY && snapshot==warm && snapshotThread==thread && thread!=xrtThreadCurrentId());xlsSemanticCheckDestroy(snapshot);snapshot=NULL;
 CHECK(unchanged(worker,before));
 CHECK(miss(worker,alternate,docs,1,NULL) && miss(worker,wrongEntry,docs,1,NULL));
 docs[0].revision=2;CHECK(miss(worker,context,docs,1,NULL));docs[0].revision=1;
 docs[0].identity=11;CHECK(miss(worker,context,docs,1,NULL));docs[0].identity=10;
 docs[0].text=equal;CHECK(miss(worker,context,docs,1,NULL));docs[0].text=text;
 docs[0].key="renamed";CHECK(miss(worker,context,docs,1,NULL));docs[0].key="entry";
 docs[0].path=other;CHECK(miss(worker,context,docs,1,NULL));docs[0].path=entry;
 CHECK(miss(worker,context,docs,2,NULL) && miss(worker,context,NULL,0,NULL));
 /* A future-revision miss must not become revision authority. */
 CHECK(get(xlsWorkerCheckInContext(worker,context,docs,1,"entry"),&snapshot,NULL)==XLS_QUERY_READY && snapshot==warm && nativeCalls==2);xlsSemanticCheckDestroy(snapshot);snapshot=NULL;
 /* Deterministic queued cancellation cannot create/promote a session. */
 blocked b;CHECK(xrtEventInit(&b.entered,false,false) && xrtEventInit(&b.release,false,false));
 xfuture* gate=xrtTaskSubmit(worker->pool,block,&b,NULL);CHECK(gate && xrtEventWaitFor(&b.entered,30000000)==XWAIT_OK);
 before=observe(worker);xls_query* cancelledQuery=xlsWorkerCachedCheckInContext(worker,context,docs,1,NULL);CHECK(cancelledQuery);xlsQueryCancel(cancelledQuery);xrtEventSet(&b.release);
 CHECK(get(cancelledQuery,&snapshot,NULL)==XLS_QUERY_CANCELLED && !snapshot && unchanged(worker,before));xrtFutureDestroy(gate);xrtEventUnit(&b.entered);xrtEventUnit(&b.release);
 /* Snapshots intentionally perform no disk validation; host proof must reject
  * a historical candidate when a real imported source has changed. */
 CHECK(xrtFileWriteAtomic(helper,(xbytesview){(const unsigned char*)changed,strlen(changed)}));before=observe(worker);
 CHECK(get(xlsWorkerCachedCheckInContext(worker,context,docs,1,NULL),&snapshot,NULL)==XLS_QUERY_READY && snapshot==warm && unchanged(worker,before));
 CHECK(!xlsSemanticCheckValidate(snapshot));xlsSemanticCheckDestroy(snapshot);snapshot=NULL;
 CHECK(get(xlsWorkerCheckInContext(worker,context,docs,1,NULL),&snapshot,NULL)==XLS_QUERY_READY && snapshot!=warm && nativeCalls==3 && xlsSemanticCheckValidate(snapshot));
 xlsSemanticCheckDestroy(warm);warm=snapshot;snapshot=NULL;
 CHECK(get(xlsWorkerCachedCheckInContext(worker,context,docs,1,"entry"),&snapshot,NULL)==XLS_QUERY_READY && snapshot==warm);
 xlsWorkerDestroy(worker);CHECK(xlsSemanticCheckInfo(snapshot).accepted && xlsSemanticCheckValidate(snapshot));xlsSemanticCheckDestroy(snapshot);xlsSemanticCheckDestroy(warm);
 xlsTextRelease(text);xlsTextRelease(equal);xlsContextRelease(context);xlsContextRelease(alternate);xlsContextRelease(wrongEntry);xrtFree(dir);xrtFree(entry);xrtFree(helper);xrtFree(other);return 1;
}
int main(void){if(!run())return 1;printf("analysis_worker_snapshot_test passed: %u checks;exact immutable inputs/context/entry,zero native calls/history/residency changes,conservative misses,queued cancellation,disk proof rejection and detached owner lifetime.\n",checks);return 0;}
