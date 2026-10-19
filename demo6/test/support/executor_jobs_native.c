#include "executor-jobs-modules.h"
#define XRT_MODULE_MEMORY_DEBUG
#define XRUNTIME_IMPLEMENTATION
#include "lib/xruntime/xruntime.h"
#include "src/runtime/xl6_executor_jobs.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>

typedef struct Context {
    xatomic32 runs,drops,started,resume,dropStarted,dropResume,hostAcquires,hostReleases;
    int mode,dropMode;
    XL6_ExecutorFailuresV1* borrowedState;
    xrtownershipref inspectedJob,inspectedSlot;
} Context;
typedef struct Environment {Context* context;} Environment;
typedef struct CodeHost {xatomic32 references,closed;Context* context;} CodeHost;
static const int layout=17;
static XL6_OwnershipCatalog* catalog;
static xexecutordataownershipv1 copiedPolicy;
static bool loadCopy(XL6_OwnershipCatalog* target)
{return xl6OwnershipCatalogAddExecutorV1(target,&copiedPolicy);}
static const XL6_OwnershipDomainV1 copiedDomain={sizeof(copiedDomain),loadCopy,NULL,NULL};
static void freeze(xrtownershipscope* scope)
{
    xdeadline end=xrtDeadlineAfter(UINT64_C(5000000));
    while(!xrtOwnershipFreezeTryBegin(scope)){assert(!xrtDeadlineExpired(end));xrtThreadYield();}
}
static void waitFor(xatomic32* value,uint32_t expected)
{
    xdeadline end=xrtDeadlineAfter(UINT64_C(5000000));
    while(xrtAtomic32Load(value,XMEMORY_ACQUIRE)<expected){assert(!xrtDeadlineExpired(end));xrtThreadYield();}
}
static void contextInit(Context* c,int mode,int dropMode)
{*c=(Context){0};c->mode=mode;c->dropMode=dropMode;xrtAtomic32Init(&c->resume,1);xrtAtomic32Init(&c->dropResume,1);}
static void callbackFreeze(Context* context)
{
    xrtownershipscope frozen={0};freeze(&frozen);
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;
    const XL6_OwnershipFinishV1* completion=NULL;
    if(context->inspectedJob.Data){
        assert(xl6OwnershipCatalogProbeV1(context->inspectedJob,catalog,&adapter,&preparation,&completion)==XL6_OWNERSHIP_ADMISSION_BUSY);
        assert(!adapter&&!preparation&&!completion);
        assert(xl6OwnershipCatalogProbeV1(context->inspectedSlot,catalog,&adapter,&preparation,&completion)==XL6_OWNERSHIP_ADMISSION_BUSY);
        assert(!adapter&&!preparation&&!completion);
    }
    assert(xrtOwnershipScopeEnd(&frozen));
}
static int acquire(void* data)
{
    CodeHost* host=data;if(xrtAtomic32Load(&host->closed,XMEMORY_ACQUIRE))return 0;
    (void)xrtAtomic32FetchAdd(&host->references,1,XMEMORY_ACQ_REL);
    (void)xrtAtomic32FetchAdd(&host->context->hostAcquires,1,XMEMORY_ACQ_REL);return 1;
}
static void release(void* data)
{
    CodeHost* host=data;assert(xrtAtomic32Load(&host->context->drops,XMEMORY_ACQUIRE));
    (void)xrtAtomic32FetchAdd(&host->context->hostReleases,1,XMEMORY_ACQ_REL);
    uint32 refs=xrtAtomic32FetchSub(&host->references,1,XMEMORY_ACQ_REL);assert(refs>1);
}
static bool hostCount(const void* data,size_t* count)
{const CodeHost* host=data;*count=xrtAtomic32Load(&host->references,XMEMORY_ACQUIRE);return *count!=0;}
static bool hostTrace(const void* data,xrtownershipvisitor visit,void* context)
{(void)data;(void)visit;(void)context;return true;}
static const xrtownershipops hostOps={hostCount,hostTrace};
static bool validate(const void* descriptor,const void* data,size_t bytes)
{return descriptor==&layout&&data&&bytes==sizeof(Environment);}
static bool trace(const void* descriptor,const void* data,size_t bytes,XL6_ModuleOwnershipVisit visit,void* context)
{(void)visit;(void)context;return validate(descriptor,data,bytes);}
static bool drop(const void* descriptor,void* data,size_t bytes)
{
    assert(validate(descriptor,data,bytes));Environment* env=data;Context* c=env->context;
    xrtAtomic32Store(&c->dropStarted,1,XMEMORY_RELEASE);waitFor(&c->dropResume,1);
    callbackFreeze(c);
    (void)xrtAtomic32FetchAdd(&c->drops,1,XMEMORY_ACQ_REL);xrtFree(env);
    if(c->dropMode==1){xrtSetErrorTake(xrtErrorCreate(XERR_STATE,"executor-fixture",902,"capture drop"));return false;}
    if(c->dropMode==2){
        assert(xrtMemDebugFailAfter(0)&&!xrtMalloc(1)&&xrtMemDebugFailTriggered());
        xrtClearError();return false;
    }
    return true;
}
static const XL6_ClosureEnvironmentOpsV1 environmentOps={sizeof(environmentOps),validate,trace,drop};
static bool body(void* raw)
{
    Context* c=((Environment*)raw)->context;(void)xrtAtomic32FetchAdd(&c->runs,1,XMEMORY_ACQ_REL);
    xrtAtomic32Store(&c->started,1,XMEMORY_RELEASE);waitFor(&c->resume,1);
    callbackFreeze(c);
    if(c->borrowedState){XL6_ExecutorFailureSnapshotV1 state={.size=sizeof(state)};assert(xl6ExecutorFailuresSnapshotV1(c->borrowedState,&state));}
    if(c->mode==1){xrtSetErrorTake(xrtErrorCreate(XERR_ARGUMENT,"executor-fixture",901,"body first"));return false;}
    if(c->mode==2)return false;
    if(c->mode==3){xrtSetErrorKind(XERR_INTERNAL);xrtClearError();}
    return true;
}
static XL6_Closure* make(Context* context,CodeHost* host)
{
    Environment* env=xrtMalloc(sizeof(*env));assert(env);env->context=context;
    XL6_CodeOwnerGraph owner={{sizeof(owner),host,acquire,release},UINT64_C(0x584C36434F574E31),&hostOps};
    XL6_Closure* callback=xl6ClosureCreateEnvironmentV1(host?&owner.base:NULL,(void (*)(void))body,NULL,
        env,sizeof(*env),&layout,&environmentOps,NULL,NULL,0);assert(callback);return callback;
}
static xexecutor* create(unsigned threads,size_t capacity)
{xexecutorconfig config={threads,capacity,0};xexecutor* executor=xrtExecutorCreate(&config);assert(executor);return executor;}
static uint64_t failures(XL6_ExecutorFailuresV1* state)
{XL6_ExecutorFailureSnapshotV1 result={.size=sizeof(result)};assert(xl6ExecutorFailuresSnapshotV1(state,&result)&&!result.saturated);return result.count;}
static void balanced(const xmemdebugsnapshot* before)
{
    xrtClearError();xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount&&after.LiveBytes==before->LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        after.InvalidFreeCount==before->InvalidFreeCount&&after.DoubleFreeCount==before->DoubleFreeCount&&
        after.UseAfterFreeCount==before->UseAfterFreeCount);
}
static bool report(XL6_ExecutorFailuresV1* state,int code,bool diagnostic)
{
    xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));bool ok=xl6ExecutorThrowIfFailedV1(state);
    if(!ok)xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result result=xl6ErrorScopeEnd(&scope);assert(result.ok==ok);
    if(!ok){
        if((result.error!=NULL)!=diagnostic)fprintf(stderr,"diagnostic mismatch: expected=%d kind=%d code=%d\n",diagnostic,
            result.error?(int)xrtErrorKind(result.error):-1,result.error?(int)xrtErrorCode(result.error):-1);
        assert((result.error!=NULL)==diagnostic);if(diagnostic)assert(xrtErrorCode(result.error)==code);
    }
    xrtErrorFree(result.error);return ok;
}
static void outcomeRound(int mode,int dropMode)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Context context;contextInit(&context,mode,dropMode);
    CodeHost* host=xrtCalloc(1,sizeof(*host));assert(host);host->context=&context;xrtAtomic32Init(&host->references,1);
    XL6_Closure* callback=make(&context,host);XL6_ExecutorFailuresV1* state=xl6ExecutorFailuresCreateV1();assert(state);
    xexecutor* executor=create(1,8);xrtownershipscope frozen={0};freeze(&frozen);
    assert(xl6ExecutorSubmitClosureV1(executor,state,callback));xl6ClosureRuntimeV1Get()->Release(callback);
    xrtAtomic32Store(&host->closed,1,XMEMORY_RELEASE);assert(xrtOwnershipScopeEnd(&frozen));assert(xrtExecutorClose(executor));
    XL6_ExecutorWaitStatusV1 status;bool expected=mode!=1&&mode!=2&&!dropMode;
    assert(xl6ExecutorWaitV1(executor,state,5000,&status)==expected&&status==XL6_EXECUTOR_READY_V1);
    xrtClearError();assert(failures(state)==(expected?0u:1u));
    bool diagnostic=mode==1||(mode!=2&&dropMode==1);
    int code=mode==1?901:902;
    assert(report(state,code,diagnostic)==expected);
    assert(report(state,code,diagnostic)==expected);
    assert(xrtExecutorDestroy(executor));xl6ExecutorFailuresReleaseV1(state);
    assert(xrtAtomic32Load(&context.runs,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&context.drops,XMEMORY_ACQUIRE)==1);
    assert(xrtAtomic32Load(&context.hostAcquires,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&context.hostReleases,XMEMORY_ACQUIRE)==1);
    assert(xrtAtomic32Load(&host->references,XMEMORY_ACQUIRE)==1);xrtFree(host);balanced(&before);
}
typedef struct Edges {xrtownershipref refs[8];size_t count;} Edges;
static bool edge(xrtownershipref ref,void* raw)
{Edges* list=raw;if(ref.Data){assert(list->count<8);list->refs[list->count++]=ref;}return true;}
static Edges edges(xrtownershipref ref)
{Edges list={0};size_t count;assert(ref.Ops->Count(ref.Data,&count)&&ref.Ops->Trace(ref.Data,edge,&list));return list;}
static void graphRound(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Context context;contextInit(&context,1,1);
    xrtAtomic32Store(&context.resume,0,XMEMORY_RELEASE);xrtAtomic32Store(&context.dropResume,0,XMEMORY_RELEASE);
    XL6_Closure* callback=make(&context,NULL);XL6_ExecutorFailuresV1* state=xl6ExecutorFailuresCreateV1();assert(state);
    const XL6_OwnershipDomainV1* copyDomains[]={xl6OwnershipDomainNativeExecutorV1Get(),&copiedDomain};
    XL6_OwnershipCatalog* copied=xl6OwnershipCatalogCreateV1(copyDomains,2);assert(copied);
    xexecutor* executor=create(1,8);xrtownershipscope frozen={0};freeze(&frozen);
    assert(xl6ExecutorSubmitClosureV1(executor,state,callback));xl6ClosureRuntimeV1Get()->Release(callback);
    xrtownershipref root=xrtExecutorOwnership(executor);Edges slots=edges(root);assert(slots.count==1);
    Edges children=edges(slots.refs[0]);assert(children.count==2);
    xrtownershipref job=children.refs[0].Data==root.Data?children.refs[1]:children.refs[0];
    Edges data=edges(job);assert(data.count==2&&data.refs[1].Data==state);
    size_t references;assert(job.Ops->Count(job.Data,&references)&&references==1);
    assert(data.refs[1].Ops->Count(state,&references)&&references==2);
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;
    const XL6_OwnershipFinishV1* completion=NULL;
    assert(xl6OwnershipCatalogProbeV1(slots.refs[0],copied,&adapter,&preparation,&completion)==XL6_OWNERSHIP_ADMISSION_REFUSED);
    assert(!adapter&&!preparation&&!completion);
    assert(xl6OwnershipCatalogProbeV1(slots.refs[0],catalog,&adapter,&preparation,&completion)==XL6_OWNERSHIP_ADMISSION_ADMITTED);
    assert(adapter&&preparation&&!completion);
    assert(xl6OwnershipCatalogProbeV1(job,catalog,&adapter,&preparation,&completion)==XL6_OWNERSHIP_ADMISSION_ADMITTED);
    assert(adapter&&preparation&&!completion&&adapter->Hold(job.Data));
    int token=0;assert(adapter->Claim(job.Data,&token)&&!preparation->Ready(job.Data));
    context.inspectedJob=job;context.inspectedSlot=slots.refs[0];
    assert(xrtOwnershipScopeEnd(&frozen));waitFor(&context.started,1);assert(xrtExecutorClose(executor));
    XL6_ExecutorWaitStatusV1 status;assert(xl6ExecutorWaitV1(executor,state,0,&status)&&status==XL6_EXECUTOR_TIMEOUT_V1);
    assert(failures(state)==0&&report(state,0,false));xrtAtomic32Store(&context.resume,1,XMEMORY_RELEASE);
    waitFor(&context.dropStarted,1);assert(failures(state)==0&&report(state,0,false));
    assert(xl6ExecutorWaitV1(executor,state,0,&status)&&status==XL6_EXECUTOR_TIMEOUT_V1);
    xrtAtomic32Store(&context.dropResume,1,XMEMORY_RELEASE);
    assert(!xl6ExecutorWaitV1(executor,state,5000,&status)&&status==XL6_EXECUTOR_READY_V1);xrtClearError();
    assert(failures(state)==1&&!report(state,901,true));assert(xrtExecutorDestroy(executor));
    freeze(&frozen);assert(job.Ops->Count(job.Data,&references)&&references==1&&edges(job).count==0);
    assert(preparation->Ready(job.Data));assert(xrtOwnershipScopeEnd(&frozen));
    assert(preparation->Prepare(job.Data,&token)==XRT_OWNERSHIP_PREPARE_READY);
    freeze(&frozen);adapter->Clear(job.Data,&token);assert(xrtOwnershipScopeEnd(&frozen));adapter->Drop(job.Data);
    xl6ExecutorFailuresReleaseV1(state);xl6OwnershipCatalogDestroyV1(copied);balanced(&before);
}
static void parallelRound(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_ExecutorFailuresV1* state=xl6ExecutorFailuresCreateV1();assert(state);
    Context first;contextInit(&first,2,0);XL6_Closure* callback=make(&first,NULL);xexecutor* executor=create(1,8);
    assert(xl6ExecutorSubmitClosureV1(executor,state,callback));xl6ClosureRuntimeV1Get()->Release(callback);assert(xrtExecutorClose(executor));
    XL6_ExecutorWaitStatusV1 status;assert(!xl6ExecutorWaitV1(executor,state,5000,&status)&&status==XL6_EXECUTOR_READY_V1);xrtClearError();
    assert(xrtExecutorDestroy(executor)&&failures(state)==1&&!report(state,0,false));
    enum {N=32};Context contexts[N];XL6_Closure* callbacks[N];executor=create(4,64);
    for(size_t i=0;i<N;++i){contextInit(&contexts[i],i%4?1:3,0);callbacks[i]=make(&contexts[i],NULL);}
    assert(xl6ExecutorSubmitClosuresV1(executor,state,callbacks,N));
    for(size_t i=0;i<N;++i)xl6ClosureRuntimeV1Get()->Release(callbacks[i]);
    assert(xrtExecutorClose(executor));assert(!xl6ExecutorWaitV1(executor,state,5000,&status)&&status==XL6_EXECUTOR_READY_V1);xrtClearError();
    assert(failures(state)==25&&!report(state,0,false));assert(xrtExecutorDestroy(executor));
    for(size_t i=0;i<N;++i)assert(xrtAtomic32Load(&contexts[i].runs,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&contexts[i].drops,XMEMORY_ACQUIRE)==1);
    xl6ExecutorFailuresReleaseV1(state);balanced(&before);
}
static void cancellationRound(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_ExecutorFailuresV1* state=xl6ExecutorFailuresCreateV1();assert(state);
    Context running;contextInit(&running,0,0);xrtAtomic32Store(&running.resume,0,XMEMORY_RELEASE);
    XL6_Closure* callback=make(&running,NULL);xexecutor* executor=create(1,8);
    assert(xl6ExecutorSubmitClosureV1(executor,state,callback));xl6ClosureRuntimeV1Get()->Release(callback);waitFor(&running.started,1);
    Context queued[3];XL6_Closure* callbacks[3];
    for(size_t i=0;i<3;++i){contextInit(&queued[i],1,i==1?1:0);callbacks[i]=make(&queued[i],NULL);}
    assert(xl6ExecutorSubmitClosuresV1(executor,state,callbacks,3));
    for(size_t i=0;i<3;++i)xl6ClosureRuntimeV1Get()->Release(callbacks[i]);
    xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));assert(xrtExecutorCancel(executor));
    xl6_error_result result=xl6ErrorScopeEnd(&scope);assert(result.ok&&!result.error);
    assert(failures(state)==1&&!report(state,902,true));
    for(size_t i=0;i<3;++i)assert(!xrtAtomic32Load(&queued[i].runs,XMEMORY_ACQUIRE)&&xrtAtomic32Load(&queued[i].drops,XMEMORY_ACQUIRE)==1);
    xrtAtomic32Store(&running.resume,1,XMEMORY_RELEASE);XL6_ExecutorWaitStatusV1 status;
    assert(!xl6ExecutorWaitV1(executor,state,5000,&status)&&status==XL6_EXECUTOR_READY_V1);xrtClearError();
    assert(failures(state)==1&&xrtExecutorDestroy(executor));xl6ExecutorFailuresReleaseV1(state);balanced(&before);
}
static void callerPrefixes(void)
{
    unsigned failed=0,succeeded=0;
    for(unsigned budget=0;budget<8;++budget){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_ExecutorFailuresV1* state=xl6ExecutorFailuresCreateV1();assert(state);
        Context contexts[3];XL6_Closure* callbacks[3];xexecutor* executor=create(1,8);
        for(size_t i=0;i<3;++i){contextInit(&contexts[i],0,0);callbacks[i]=make(&contexts[i],NULL);}
        assert(xrtMemDebugFailAfter(budget));bool accepted=xl6ExecutorSubmitClosuresV1(executor,state,callbacks,3),injected=xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();xrtClearError();if(accepted){assert(!injected);++succeeded;}else{assert(injected);++failed;}
        for(size_t i=0;i<3;++i)xl6ClosureRuntimeV1Get()->Release(callbacks[i]);
        assert(xrtExecutorClose(executor));XL6_ExecutorWaitStatusV1 status;
        assert(xl6ExecutorWaitV1(executor,state,5000,&status)&&status==XL6_EXECUTOR_READY_V1&&failures(state)==0);
        assert(xrtExecutorDestroy(executor));xl6ExecutorFailuresReleaseV1(state);
        for(size_t i=0;i<3;++i)assert(xrtAtomic32Load(&contexts[i].runs,XMEMORY_ACQUIRE)==(accepted?1u:0u)&&xrtAtomic32Load(&contexts[i].drops,XMEMORY_ACQUIRE)==1);
        balanced(&before);
    }
    assert(failed&&succeeded);
}
static void rejectionRound(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_ExecutorFailuresV1* state=xl6ExecutorFailuresCreateV1();assert(state);
    Context contexts[2];XL6_Closure* callbacks[2];xexecutor* executor=create(1,1);
    for(size_t i=0;i<2;++i){contextInit(&contexts[i],0,0);callbacks[i]=make(&contexts[i],NULL);}
    XL6_Closure* invalid[]={callbacks[0],NULL};assert(!xl6ExecutorSubmitClosuresV1(executor,state,invalid,2));xrtClearError();
    assert(failures(state)==0);
    xrtownershipscope frozen={0};freeze(&frozen);assert(xl6ExecutorSubmitClosureV1(executor,state,callbacks[0]));
    assert(!xl6ExecutorSubmitClosuresV1(executor,state,callbacks,2));xrtClearError();
    assert(!xl6ExecutorSubmitClosureV1(executor,state,callbacks[1]));xrtClearError();
    xexecutorstats stats;assert(xrtExecutorGet(executor,&stats)&&stats.Submitted==1&&stats.Queued==1);
    xrtownershipref ledger=xl6ExecutorFailuresOwnershipV1(state);size_t refs;assert(ledger.Ops->Count(ledger.Data,&refs)&&refs==2);
    assert(!xrtAtomic32Load(&contexts[0].runs,XMEMORY_ACQUIRE)&&!xrtAtomic32Load(&contexts[1].runs,XMEMORY_ACQUIRE));
    XL6_ExecutorWaitStatusV1 status=XL6_EXECUTOR_READY_V1;
    assert(!xl6ExecutorWaitV1(executor,state,-2,&status)&&status==XL6_EXECUTOR_TIMEOUT_V1);xrtClearError();
    assert(!xl6ExecutorWaitV1(executor,state,INT64_MAX,&status)&&status==XL6_EXECUTOR_TIMEOUT_V1);xrtClearError();
    assert(!xl6ExecutorWaitV1(NULL,state,0,&status)&&status==XL6_EXECUTOR_TIMEOUT_V1);xrtClearError();
    assert(xrtOwnershipScopeEnd(&frozen));assert(xrtExecutorClose(executor));
    assert(xl6ExecutorWaitV1(executor,state,5000,&status)&&status==XL6_EXECUTOR_READY_V1&&failures(state)==0);
    assert(xrtExecutorDestroy(executor));for(size_t i=0;i<2;++i)xl6ClosureRuntimeV1Get()->Release(callbacks[i]);
    assert(xrtAtomic32Load(&contexts[0].runs,XMEMORY_ACQUIRE)==1&&!xrtAtomic32Load(&contexts[1].runs,XMEMORY_ACQUIRE));
    xl6ExecutorFailuresReleaseV1(state);balanced(&before);
}
static void abandonedReaderRound(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_ExecutorFailuresV1* state=xl6ExecutorFailuresCreateV1();assert(state);
    Context context;contextInit(&context,0,0);context.borrowedState=state;XL6_Closure* callback=make(&context,NULL);
    xexecutor* executor=create(1,8);xrtownershipscope frozen={0};freeze(&frozen);
    assert(xl6ExecutorSubmitClosureV1(executor,state,callback));xl6ClosureRuntimeV1Get()->Release(callback);xl6ExecutorFailuresReleaseV1(state);
    xrtownershipref ledger=xl6ExecutorFailuresOwnershipV1(state);size_t refs;assert(ledger.Ops->Count(ledger.Data,&refs)&&refs==1);
    assert(xrtOwnershipScopeEnd(&frozen));assert(xrtExecutorClose(executor));
    assert(xrtExecutorWaitFor(executor,UINT64_C(5000000))==XWAIT_OK&&xrtExecutorDestroy(executor));
    assert(xrtAtomic32Load(&context.runs,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&context.drops,XMEMORY_ACQUIRE)==1);balanced(&before);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();assert(xrtTemp(8)&&xrtTempClear());
    xl6_error_scope warm={0};assert(xl6ErrorScopeBegin(&warm));xl6_error_result ready=xl6ErrorScopeEnd(&warm);assert(ready.ok);xrtErrorFree(ready.error);
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainExecutorJobsV1Get(),xl6OwnershipDomainNativeExecutorV1Get(),
        xl6OwnershipDomainClosureV1Get(),xl6OwnershipDomainCodeLeaseV1Get(),xl6OwnershipDomainCellV1Get()};
    catalog=xl6OwnershipCatalogCreateV1(domains,5);assert(catalog);
    copiedPolicy=*xl6ExecutorClosureDataPolicyV1Get();
    for(unsigned i=0;i<8;++i){outcomeRound(0,0);outcomeRound(1,0);outcomeRound(2,0);outcomeRound(3,0);
        outcomeRound(1,1);outcomeRound(2,1);outcomeRound(0,1);outcomeRound(0,2);
        graphRound();parallelRound();cancellationRound();rejectionRound();abandonedReaderRound();}
    callerPrefixes();
    xl6OwnershipCatalogDestroyV1(catalog);
    puts("Executor jobs: 64 checked body/drop and code-lease outcomes, 8 pinned graphs, 264 parallel jobs, 24 cancelled captures, 8 rejection/abandoned-reader rounds, 8 caller fault prefixes; exact memory balance");return 0;
}
