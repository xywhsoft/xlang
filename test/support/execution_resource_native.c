#include "execution-resource-modules.h"
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_THREAD_KEY
#define XRT_MODULE_FILE_TEMP
#define XRUNTIME_IMPLEMENTATION
#ifndef XRT_MODULE_WAIT
#define XRT_MODULE_WAIT
#endif
#include "lib/xruntime/xruntime_features.h"
#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)
#define XRT_IMPLEMENTATION
#endif
#include "lib/xrt/xrt.h"
#include "lib/xruntime/xruntime.h"
#include "release/include/xl_clock.h"
#include "src/runtime/xl6_execution_resource.h"
#include "src/runtime/xl6_execution_resource_internal.h"
#include "src/api/xl_foreign_error.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/runtime/xl6_thread_exit.h"
#include "src/runtime/xl6_task_pool_exit.h"
#include "src/runtime/xl6_task_pool_internal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static xatomic64 allocationThread={0};
static xatomic32 allocationArmed={0},allocationSeen={0},allocationResume={0};
static void waitFor(xatomic32*,uint32_t);
static void allocationHook(void)
{
    uint32_t armed=1;if(xrtAtomic64Load(&allocationThread,XMEMORY_ACQUIRE)!=xrtThreadCurrentId()||
        !xrtAtomic32CompareExchange(&allocationArmed,&armed,2,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE))return;
    xrtAtomic32Store(&allocationSeen,1,XMEMORY_RELEASE);waitFor(&allocationResume,1);
}
static void* allocate(void* context,size_t bytes){(void)context;allocationHook();return malloc(bytes);}
static void* resize(void* context,void* raw,size_t bytes){(void)context;allocationHook();return realloc(raw,bytes);}
static void release(void* context,void* raw){(void)context;free(raw);}
typedef struct Coordination {xatomic32 bodyStarted,bodyResume,dropStarted,dropResume;uint64_t bodyThread,dropThread;unsigned failure;} Coordination;
typedef struct Environment {XL6_ExecutionResourceV1* owned;Coordination* coordination;} Environment;
static const int layout=1;
static void waitFor(xatomic32* value,uint32_t target)
{XL6_DeadlineV1 end=xl6DeadlineAfterMillisecondsV1(5000);while(xrtAtomic32Load(value,XMEMORY_ACQUIRE)<target){assert(!xl6DeadlineExpiredV1(end));xrtThreadYield();}}
static void freeze(xrtownershipscope* scope)
{XL6_DeadlineV1 end=xl6DeadlineAfterMillisecondsV1(5000);while(!xrtOwnershipFreezeTryBegin(scope)){assert(!xl6DeadlineExpiredV1(end));xrtThreadYield();}}
static void balanced(const xmemdebugsnapshot* before)
{
    xrtClearError();xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount&&after.LiveBytes==before->LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        after.InvalidFreeCount==before->InvalidFreeCount&&after.DoubleFreeCount==before->DoubleFreeCount&&after.UseAfterFreeCount==before->UseAfterFreeCount);
}
static XL6_ExecutionSourceSnapshotV1 snapshot(XL6_ExecutionSourceV1* source)
{XL6_ExecutionSourceSnapshotV1 result={.size=sizeof(result)};assert(xl6ExecutionSourceSnapshotV1Get(source,&result));return result;}
static void drain(XL6_ExecutionSourceV1* source)
{XL6_DeadlineV1 end=xl6DeadlineAfterMillisecondsV1(5000);for(;;){XL6_ExecutionSourceSnapshotV1 state=snapshot(source);if(!state.resources&&!state.active)break;assert(!xl6DeadlineExpiredV1(end));xrtThreadYield();}}
static void destroyClosed(XL6_ExecutionSourceV1** source,bool success)
{
    XL6_DeadlineV1 end=xl6DeadlineAfterMillisecondsV1(5000);
    while(*source){XL6_ExecutionRetireResultV1 result=xl6ExecutionSourceDestroyV1(source);assert(result.ok==success);
        if(result.ready)break;
        assert(!xl6DeadlineExpiredV1(end));xrtClearError();xrtThreadYield();}
    assert(!*source);
}
static void destroy(XL6_ExecutionSourceV1** source,bool success)
{assert(xl6ExecutionSourceCloseV1(*source));destroyClosed(source,success);}
static const xrtownershipadapterv1* hold(XL6_ExecutionResourceV1* resource)
{
    const XL6_ExecutionFamilyV1* families[]={xl6ExecutionExecutorFamilyV1Get(),xl6ExecutionTaskPoolFamilyV1Get()};
    const xrtownershippreparationv1* prepare=NULL;xrtownershipscope frozen={0};freeze(&frozen);
    const xrtownershipadapterv1* adapter=xl6ExecutionOwnershipAdapterV1(xl6ExecutionResourceOwnershipV1(resource),families,2,&prepare);
    assert(adapter&&prepare&&adapter->Hold(resource));assert(xrtOwnershipScopeEnd(&frozen));return adapter;
}
static bool validate(const void* schema,const void* data,size_t bytes)
{return schema==&layout&&data&&bytes==sizeof(Environment);}
static bool trace(const void* schema,const void* raw,size_t bytes,XL6_ModuleOwnershipVisit visit,void* context)
{const Environment* env=raw;return validate(schema,raw,bytes)&&visit(xl6ExecutionResourceOwnershipV1(env->owned),context);}
static bool drop(const void* schema,void* raw,size_t bytes)
{
    assert(validate(schema,raw,bytes));Environment* env=raw;Coordination* c=env->coordination;c->dropThread=xrtThreadCurrentId();
    XL6_ExecutionResourceV1* owned=env->owned;env->owned=NULL;xl6ExecutionResourceReleaseV1(owned);
    xrtAtomic32Store(&c->dropStarted,1,XMEMORY_RELEASE);waitFor(&c->dropResume,1);xrtFree(env);return true;
}
static const XL6_ClosureEnvironmentOpsV1 environmentOps={sizeof(environmentOps),validate,trace,drop};
static bool body(void* raw)
{
    Coordination* c=((Environment*)raw)->coordination;c->bodyThread=xrtThreadCurrentId();
    xrtAtomic32Store(&c->bodyStarted,1,XMEMORY_RELEASE);waitFor(&c->bodyResume,1);
    if(c->failure==1)xrtSetErrorKind(XERR_ARGUMENT);
    if(c->failure==3)xrtSetErrorTake(xrtErrorCreate(XERR_ARGUMENT,"executor-graph",901,"graph first failure"));
    return !c->failure;
}
static xtaskoutcome taskBody(xcancel* cancel,void* raw,xtaskvalue* result)
{(void)cancel;(void)result;assert(body(raw));return XTASK_SUCCESS;}
static void taskDrop(void* raw,void* context)
{assert(!context&&drop(&layout,raw,sizeof(Environment)));}
static void selfReleaseRound(bool pool,unsigned failure)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Coordination coordination={0};coordination.failure=failure;
    XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();assert(source);
    XL6_ExecutionResourceV1* resource=pool?xl6ExecutionTaskPoolCreateV1(source,1,8,0):xl6ExecutionExecutorCreateV1(source,1,8,0);assert(resource);
    const xrtownershipadapterv1* pin=hold(resource);Environment* env=xrtMalloc(sizeof(*env));assert(env);
    env->owned=xl6ExecutionResourceRetainV1(resource);assert(env->owned);env->coordination=&coordination;
    xfuture* future=NULL;
    if(!pool){
        XL6_Closure* callback=xl6ClosureCreateEnvironmentV1(NULL,(void (*)(void))body,NULL,env,sizeof(*env),&layout,&environmentOps,NULL,NULL,0);assert(callback);
        assert(xl6ExecutionExecutorSubmitV1(resource,callback));xl6ClosureRuntimeV1Get()->Release(callback);
    }else{
        /* This legacy raw task is used only to exercise a real worker's Drop
         * tail. It is never admitted as an ownership-graph participant. */
        XL6_ExecutionScopeV1 entry={0};XL6_TaskNativeScopeV1 native={0};
        assert(xl6ExecutionResourceEnterV1(resource,true,&entry)&&xl6TaskNativeBeginV1(entry.pool,&native));
        xtaskargs args={0};args.Destroy=taskDrop;future=xrtTaskSubmit(native.native,taskBody,env,&args);assert(future);
        xl6TaskNativeEndV1(&native);xl6ExecutionResourceLeaveV1(&entry);
    }
    waitFor(&coordination.bodyStarted,1);xl6ExecutionResourceReleaseV1(resource);
    xrtAtomic32Store(&coordination.bodyResume,1,XMEMORY_RELEASE);waitFor(&coordination.dropStarted,1);
    assert(coordination.bodyThread==coordination.dropThread&&coordination.dropThread!=xrtThreadCurrentId());
    assert(snapshot(source).resources==1);
    xrtAtomic32Store(&coordination.dropResume,1,XMEMORY_RELEASE);drain(source);
    assert(xl6ExecutionSourceCheckV1(source)==!failure);
    if(failure)assert(failure==1?xrtErrorKind(xrtGetError())==XERR_ARGUMENT:!xrtGetError());
    xrtClearError();destroy(&source,!failure);
    xrtownershipscope frozen={0};freeze(&frozen);xrtownershipref ref=xl6ExecutionResourceOwnershipV1(resource);size_t count;
    assert(ref.Ops->Count(ref.Data,&count)&&count==1);assert(xrtOwnershipScopeEnd(&frozen));pin->Drop(resource);xrtFutureDestroy(future);balanced(&before);
}
static void stalledRound(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();assert(source);
    XL6_ExecutionResourceV1* r=xl6ExecutionExecutorCreateV1(source,1,8,0);assert(r);
    const xrtownershipadapterv1* pin=hold(r);XL6_ExecutionScopeV1 entry={0};assert(xl6ExecutionResourceEnterV1(r,false,&entry));
    xexecutor* native=entry.executor;xl6ExecutionResourceLeaveV1(&entry);
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* prepare=NULL;xrtownershipscope frozen={0};
    int token=0;XL6_DeadlineV1 end=xl6DeadlineAfterMillisecondsV1(5000);
    const xexecutorworkerexitownershipv1* known[]={xl6ExecutorWorkerExitPolicyV1Get()};
    while(!adapter){freeze(&frozen);adapter=xrtExecutorOwnershipAdapterV2(xrtExecutorOwnership(native),known,1,&prepare);
        if(adapter)assert(adapter->Hold(native)&&adapter->Claim(native,&token));
        assert(xrtOwnershipScopeEnd(&frozen)&&!xl6DeadlineExpiredV1(end));xrtThreadYield();}
    xl6ExecutionResourceReleaseV1(r);XL6_ExecutionResourceSnapshotV1 state={.size=sizeof(state)};
    do{assert(xl6ExecutionResourceSnapshotV1Get(r,&state)&&!xl6DeadlineExpiredV1(end));xrtThreadYield();}while(!state.stalled||snapshot(source).active);
    assert(state.nativePresent&&snapshot(source).resources==1&&!xl6ExecutionSourceCheckV1(source));
    assert(xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    freeze(&frozen);adapter->Restore(native,&token);assert(xrtOwnershipScopeEnd(&frozen));adapter->Drop(native);
    assert(xl6ExecutionSourceRetryV1(source));drain(source);
    assert(!xl6ExecutionSourceCheckV1(source)&&xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    destroy(&source,false);pin->Drop(r);balanced(&before);
}
static XL6_ExecutionResourceV1* createResource(XL6_ExecutionSourceV1* source,bool pool)
{XL6_ExecutionResourceV1* r=pool?xl6ExecutionTaskPoolCreateV1(source,1,8,0):xl6ExecutionExecutorCreateV1(source,1,8,0);assert(r);return r;}
static void entryRound(bool pool)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();assert(source);
    XL6_ExecutionResourceV1* r=createResource(source,pool);const xrtownershipadapterv1* pin=hold(r);
    XL6_ExecutionScopeV1 entry={0};assert(xl6ExecutionResourceEnterV1(r,true,&entry));
    xl6ExecutionResourceReleaseV1(r);
    XL6_ExecutionResourceSnapshotV1 state={.size=sizeof(state)};assert(xl6ExecutionResourceSnapshotV1Get(r,&state));
    assert(!state.owners&&state.entries==1&&state.closing&&state.nativePresent&&snapshot(source).resources==1);
    /* Retirement cannot consume the native owner while this actual method
     * entry is still borrowing it, even though every user owner is gone. */
    assert(pool?xl6TaskPoolControlV1Get()->Start(entry.pool):xrtExecutorClose(entry.executor));
    xl6ExecutionResourceLeaveV1(&entry);drain(source);destroy(&source,true);pin->Drop(r);balanced(&before);
}
static void closedRound(bool pool)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();assert(source);
    XL6_ExecutionResourceV1* r=createResource(source,pool);assert(xl6ExecutionSourceCloseV1(source));
    assert(!(pool?xl6ExecutionTaskPoolCreateV1(source,1,8,0):xl6ExecutionExecutorCreateV1(source,1,8,0)));
    assert(xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    assert(xl6ExecutionResourceCloseV1(r)&&xl6ExecutionResourceCloseV1(r));
    XL6_ExecutionScopeV1 entry={0};assert(!xl6ExecutionResourceEnterV1(r,true,&entry));xrtClearError();
    assert(xl6ExecutionResourceEnterV1(r,false,&entry));
    if(pool){XL6_TaskPoolWaitStatusV1 status;assert(xl6TaskPoolControlV1Get()->WaitFor(entry.pool,0,NULL,&status)&&status==XL6_TASK_POOL_READY_V1);}
    else {XL6_ExecutorWaitStatusV1 status;xexecutorstats stats;
        assert(xl6ExecutionExecutorWaitV1(r,5000,&status)&&status==XL6_EXECUTOR_READY_V1&&xl6ExecutionExecutorStatsV1(r,&stats));}
    xl6ExecutionResourceLeaveV1(&entry);assert(snapshot(source).resources==1);
    xl6ExecutionResourceReleaseV1(r);drain(source);destroy(&source,true);balanced(&before);
}
typedef struct Creator {XL6_ExecutionSourceV1* source;XL6_ExecutionResourceV1* result;bool pool,failed;} Creator;
static int32_t creatorThread(void* raw)
{
    Creator* creator=raw;xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation)&&xrtOwnershipScopeEnd(&mutation));
    xrtSetErrorKind(XERR_STATE);xrtClearError();
    xrtAtomic64Store(&allocationThread,xrtThreadCurrentId(),XMEMORY_RELEASE);xrtAtomic32Store(&allocationArmed,1,XMEMORY_RELEASE);
    creator->result=creator->pool?xl6ExecutionTaskPoolCreateV1(creator->source,1,8,0):xl6ExecutionExecutorCreateV1(creator->source,1,8,0);
    creator->failed=xrtGetError()&&xrtErrorKind(xrtGetError())==XERR_STATE;xrtClearError();return 0;
}
static void creationCloseRound(bool pool)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();assert(source);
    xrtAtomic32Store(&allocationSeen,0,XMEMORY_RELEASE);xrtAtomic32Store(&allocationResume,0,XMEMORY_RELEASE);
    Creator creator={.source=source,.pool=pool};xthread* thread=xrtThreadCreate(creatorThread,&creator,0);assert(thread);
    waitFor(&allocationSeen,1);assert(snapshot(source).entries==1&&xl6ExecutionSourceCloseV1(source));
    xrtAtomic32Store(&allocationResume,1,XMEMORY_RELEASE);assert(xrtThreadWaitFor(thread,UINT64_C(5000000))==XWAIT_OK);xrtThreadDestroy(thread);
    assert(!creator.result&&creator.failed&&!snapshot(source).resources);destroy(&source,true);balanced(&before);
}
static void allocationPrefixes(void)
{
    for(unsigned kind=0;kind<3;++kind){unsigned failed=0,succeeded=0;
        for(unsigned budget=0;budget<40;++budget){xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
            XL6_ExecutionSourceV1* source=kind?xl6ExecutionSourceCreateV1():NULL;assert(!kind||source);
            assert(xrtMemDebugFailAfter(budget));XL6_ExecutionResourceV1* r=NULL;bool ok;
            if(!kind){source=xl6ExecutionSourceCreateV1();ok=source!=NULL;}
            else {r=kind==1?xl6ExecutionExecutorCreateV1(source,1,8,0):xl6ExecutionTaskPoolCreateV1(source,1,8,0);ok=r!=NULL;}
            bool injected=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xrtClearError();
            if(ok){assert(!injected);++succeeded;}else{assert(injected);++failed;}
            if(r)xl6ExecutionResourceReleaseV1(r);
            if(source){drain(source);destroy(&source,true);}balanced(&before);
        }assert(failed&&succeeded);
    }
}

static bool forbiddenTrace(const XL6_ExecutionResourceV1* resource,xrtownershipvisitor visit,void* context)
{(void)resource;(void)visit;(void)context;abort();}
static void identityRefusal(XL6_OwnershipCatalog* catalog,bool pool)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();assert(source);
    XL6_ExecutionResourceV1* r=createResource(source,pool);const XL6_ExecutionFamilyV1* actual=r->family;
    XL6_ExecutionFamilyV1 copied=*actual;copied.Trace=forbiddenTrace;xrtownershipscope frozen={0};freeze(&frozen);r->family=&copied;
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* prepare=NULL;const XL6_OwnershipFinishV1* finish=NULL;
    assert(xl6OwnershipCatalogProbeV1(xl6ExecutionResourceOwnershipV1(r),catalog,&adapter,&prepare,&finish)==XL6_OWNERSHIP_ADMISSION_REFUSED);
    assert(!adapter&&!prepare&&!finish);r->family=actual;
    assert(xl6OwnershipCatalogProbeV1(xl6ExecutionResourceOwnershipV1(r),catalog,&adapter,&prepare,&finish)==XL6_OWNERSHIP_ADMISSION_ADMITTED);
    assert(adapter&&prepare&&!finish&&xrtOwnershipScopeEnd(&frozen));
    xl6ExecutionResourceReleaseV1(r);drain(source);destroy(&source,true);balanced(&before);
}

enum {GRAPH_LIMIT=16};
typedef struct Node {xrtownershipref ref;const xrtownershipadapterv1* adapter;const xrtownershippreparationv1* preparation;size_t refs,incoming;} Node;
typedef struct Graph {Node nodes[GRAPH_LIMIT];size_t count,edges;} Graph;
static bool remember(xrtownershipref ref,void* raw)
{
    Graph* g=raw;size_t i=0;assert(ref.Data&&ref.Ops);
    while(i<g->count&&(g->nodes[i].ref.Data!=ref.Data||g->nodes[i].ref.Ops!=ref.Ops))++i;
    if(i==g->count){assert(g->count<GRAPH_LIMIT);g->nodes[g->count++].ref=ref;}
    ++g->nodes[i].incoming;++g->edges;return true;
}
/* Query the exact catalog BEFORE Count/Trace; never inspect an unknown or
 * BUSY record merely because a table has superficially similar callbacks. */
static bool collect(Graph* g,XL6_OwnershipCatalog* catalog,XL6_ExecutionSourceV1* source)
{
    *g=(Graph){0};g->count=1;g->nodes[0].ref=xl6ExecutionSourceOwnershipV1(source);
    for(size_t i=0;i<g->count;++i){Node* n=&g->nodes[i];const XL6_OwnershipFinishV1* finish=NULL;
        XL6_OwnershipAdmissionV1 admission=xl6OwnershipCatalogProbeV1(n->ref,catalog,&n->adapter,&n->preparation,&finish);
        if(admission==XL6_OWNERSHIP_ADMISSION_BUSY)return false;
        if(admission!=XL6_OWNERSHIP_ADMISSION_ADMITTED||!n->adapter||finish){
            fprintf(stderr,"graph refused node %zu/%zu: admission=%d adapter=%d prepare=%d finish=%d\n",i,g->count,(int)admission,n->adapter!=NULL,n->preparation!=NULL,finish!=NULL);
            abort();}
        assert(n->ref.Ops->Count(n->ref.Data,&n->refs)&&n->ref.Ops->Trace(n->ref.Data,remember,g));
    }
    xrtownershipresult inspection={0};assert(xrtOwnershipInspect(&g->nodes[0].ref,1,NULL,0,&inspection));
    size_t roots=0;for(size_t i=0;i<g->count;++i){assert(g->nodes[i].refs>=g->nodes[i].incoming);roots+=g->nodes[i].refs>g->nodes[i].incoming;}
    assert(inspection.NodeCount==g->count&&inspection.EdgeCount==g->edges&&inspection.ExternalRootCount==roots);return true;
}
static void graphRound(XL6_OwnershipCatalog* catalog,bool pool,unsigned empty,unsigned order,bool restore)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();assert(source);
    bool noResource=empty==1||empty==2;
    XL6_ExecutionResourceV1* r=noResource?NULL:createResource(source,pool);
    if(empty==2){assert(xl6ExecutionSourceCloseV1(source));XL6_DeadlineV1 exited=xl6DeadlineAfterMillisecondsV1(5000);
        while(!snapshot(source).exited){assert(!xl6DeadlineExpiredV1(exited));xrtThreadYield();}}
    if(pool&&!empty){XL6_ExecutionScopeV1 entry={0};assert(xl6ExecutionResourceEnterV1(r,true,&entry));
        assert(!xl6TaskPoolReleaseClearedOwnerV1(entry.pool));assert(xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
        assert(xl6TaskPoolControlV1Get()->Start(entry.pool));xl6ExecutionResourceLeaveV1(&entry);}
    Graph g;xrtownershipscope frozen={0};XL6_DeadlineV1 end=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){freeze(&frozen);if(collect(&g,catalog,source))break;
        assert(xrtOwnershipScopeEnd(&frozen)&&!xl6DeadlineExpiredV1(end));xrtThreadYield();}
    assert(g.count==(noResource?1u:pool?(empty==3?4u:5u):4u));
    if(pool&&!noResource){
        xrtownershipref failures=xl6ExecutionFailuresOwnershipV1(r->failures);size_t i=0;
        while(i<g.count&&(g.nodes[i].ref.Data!=failures.Data||g.nodes[i].ref.Ops!=failures.Ops))++i;
        /* Exactly the resource and the lazy wrapper OR real native pool own
         * this ledger. Native creation transfers, never duplicates, that ref. */
        assert(i<g.count&&g.nodes[i].refs==2&&g.nodes[i].incoming==2);
    }
    int token=0,conflict=1;
    for(size_t i=0;i<g.count;++i){Node* n=&g.nodes[i];assert(n->adapter->Hold(n->ref.Data)&&n->adapter->Claim(n->ref.Data,&token));
        assert(!n->adapter->Claim(n->ref.Data,&conflict));}
    assert(xrtOwnershipScopeEnd(&frozen));
    bool prepared[GRAPH_LIMIT]={false};size_t remaining=g.count;
    while(remaining){for(size_t step=0;step<g.count;++step){size_t i=(order&1)?g.count-1-step:step;if(prepared[i])continue;
        Node* n=&g.nodes[i];xrtownershipprepareresult result=n->preparation?n->preparation->Prepare(n->ref.Data,&token):XRT_OWNERSHIP_PREPARE_READY;
        assert(result!=XRT_OWNERSHIP_PREPARE_FAILED);if(result==XRT_OWNERSHIP_PREPARE_READY){prepared[i]=true;--remaining;}}
        assert(!xl6DeadlineExpiredV1(end));xrtThreadYield();}
    freeze(&frozen);for(size_t i=0;i<g.count;++i)assert(!g.nodes[i].preparation||g.nodes[i].preparation->Ready(g.nodes[i].ref.Data));
    for(size_t step=0;step<g.count;++step){size_t i=(order&1)?g.count-1-step:step;Node* n=&g.nodes[i];
        if(restore)n->adapter->Restore(n->ref.Data,&token);else n->adapter->Clear(n->ref.Data,&token);}
    assert(xrtOwnershipScopeEnd(&frozen));
    if(!restore){
        assert(xrtMemDebugFailAfter(0));
        for(unsigned pass=0;pass<2;++pass)for(size_t step=0;step<g.count;++step){size_t i=(order&2)?g.count-1-step:step;
            Node* n=&g.nodes[i];assert(!n->adapter->Finish||n->adapter->Finish(n->ref.Data,&token));}
        assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    }
    /* Restore must leave the controller able to retire the same actual
     * native instance after the last user release (without a new thread). */
    xl6ExecutionResourceReleaseV1(r);if(restore)drain(source);
    assert(xl6ExecutionSourceCloseV1(source));
    destroyClosed(&source,true);
    for(size_t step=0;step<g.count;++step){size_t i=(order&4)?g.count-1-step:step;g.nodes[i].adapter->Drop(g.nodes[i].ref.Data);}
    balanced(&before);
}
static void failureGraphRound(XL6_OwnershipCatalog* catalog,bool nullError,bool observed,bool graph)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();assert(source);
    XL6_ExecutionResourceV1* r=createResource(source,false);Coordination c={0};c.failure=nullError?2:3;
    xrtAtomic32Store(&c.bodyResume,1,XMEMORY_RELEASE);xrtAtomic32Store(&c.dropResume,1,XMEMORY_RELEASE);
    Environment* env=xrtMalloc(sizeof(*env));assert(env);*env=(Environment){xl6ExecutionResourceRetainV1(r),&c};assert(env->owned);
    XL6_Closure* callback=xl6ClosureCreateEnvironmentV1(NULL,(void (*)(void))body,NULL,env,sizeof(*env),&layout,&environmentOps,NULL,NULL,0);assert(callback);
    assert(xl6ExecutionExecutorSubmitV1(r,callback));xl6ClosureRuntimeV1Get()->Release(callback);
    /* Wait on native physical completion only: this is deliberately NOT an
     * explicit language failure report, so an unobserved error stays pending. */
    assert(xl6ExecutionResourceCloseV1(r));
    XL6_ExecutionScopeV1 entry={0};assert(xl6ExecutionResourceEnterV1(r,false,&entry));
    assert(xrtExecutorWaitFor(entry.executor,UINT64_C(5000000))==XWAIT_OK);xl6ExecutionResourceLeaveV1(&entry);
    if(observed){assert(!xl6ExecutionExecutorThrowIfFailedV1(r));
        assert(nullError?!xrtGetError():xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();}
    XL6_ExecutionFailureSnapshotV1 failure={.size=sizeof(failure)};assert(xl6ExecutionExecutorFailuresV1(r,&failure)&&failure.count==1);
    if(!graph){
        xl6ExecutionResourceReleaseV1(r);drain(source);assert(xl6ExecutionSourceCheckV1(source)==observed);
        if(!observed){assert(nullError?!xrtGetError():xrtErrorKind(xrtGetError())==XERR_ARGUMENT);
            if(!nullError)assert(xrtErrorCode(xrtGetError())==901&&!strcmp(xrtErrorMessage(xrtGetError()),"graph first failure"));}
        xrtClearError();destroy(&source,observed);balanced(&before);return;
    }
    Graph g;xrtownershipscope frozen={0};XL6_DeadlineV1 deadline=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){freeze(&frozen);if(collect(&g,catalog,source))break;
        assert(xrtOwnershipScopeEnd(&frozen)&&!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}
    int token=0;for(size_t i=0;i<g.count;++i){Node* n=&g.nodes[i];assert(n->adapter->Hold(n->ref.Data)&&n->adapter->Claim(n->ref.Data,&token));}
    assert(xrtOwnershipScopeEnd(&frozen));
    for(size_t i=0;i<g.count;++i){Node* n=&g.nodes[i];if(n->preparation)for(;;){
        xrtownershipprepareresult result=n->preparation->Prepare(n->ref.Data,&token);
        if(result==XRT_OWNERSHIP_PREPARE_READY)break;
        assert(result==XRT_OWNERSHIP_PREPARE_BUSY&&!xl6DeadlineExpiredV1(deadline));xrtThreadYield();}}
    freeze(&frozen);for(size_t i=0;i<g.count;++i)g.nodes[i].adapter->Clear(g.nodes[i].ref.Data,&token);
    assert(xrtOwnershipScopeEnd(&frozen));unsigned reported=0;
    for(unsigned pass=0;pass<2;++pass)for(size_t i=0;i<g.count;++i){Node* n=&g.nodes[i];
        if(n->adapter->Finish&&!n->adapter->Finish(n->ref.Data,&token)){
            assert(!pass&&!observed&&++reported==1);
            assert(nullError?!xrtGetError():xrtErrorKind(xrtGetError())==XERR_ARGUMENT);
            if(!nullError)assert(xrtErrorCode(xrtGetError())==901&&!strcmp(xrtErrorMessage(xrtGetError()),"graph first failure"));
            xrtClearError();}}
    assert(reported==!observed);xl6ExecutionResourceReleaseV1(r);assert(xl6ExecutionSourceCloseV1(source));destroyClosed(&source,true);
    for(size_t i=g.count;i>0;--i)g.nodes[i-1].adapter->Drop(g.nodes[i-1].ref.Data);
    balanced(&before);
}
static xthreadkey* exitModeKey;
static bool forbiddenExitCount(const void* raw,size_t* count)
{(void)raw;(void)count;abort();}
static bool forbiddenExitTrace(const void* raw,xrtownershipvisitor visit,void* context)
{(void)raw;(void)visit;(void)context;abort();}
static void lazyExitIdentity(XL6_OwnershipCatalog* catalog)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    XL6_ExecutionFailuresV1* failures=xl6ExecutionFailuresCreateV1();assert(failures);
    xtaskpoolworkerexitownershipv1 copy=*xl6TaskPoolWorkerExitPolicyV1Get();
    const xrtownershipops forbidden={forbiddenExitCount,forbiddenExitTrace};copy.Ops=&forbidden;
    XL6_TaskPoolConfig config={1,8,0};assert(xl6ExecutionFailuresRetainV1(failures));
    XL6_TaskPool* pool=xl6TaskPoolCreateOwnedExitInternalV1(&config,failures,&copy);assert(pool);
    xrtownershipscope frozen={0};freeze(&frozen);
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;
    const XL6_OwnershipFinishV1* finish=NULL;xrtownershipref ref=xl6TaskPoolOwnership(pool);
    assert(xl6OwnershipCatalogProbeV1(ref,catalog,&adapter,&preparation,&finish)==XL6_OWNERSHIP_ADMISSION_REFUSED);
    assert(!adapter&&!preparation&&!finish);
    preparation=(const void*)pool;
    assert(!xl6TaskPoolOwnershipAdapterV1(ref,&preparation)&&preparation==(const void*)pool);
    assert(xrtOwnershipScopeEnd(&frozen));
    /* Known Drop is safe, but unknown Ops must NEVER have been executed. */
    assert(xl6TaskPoolControlV1Get()->Destroy(pool));xl6ExecutionFailuresReleaseV1(failures);balanced(&before);
}
static void lazyStartPrefixes(void)
{
    unsigned failed=0,succeeded=0;
    for(unsigned budget=0;budget<32;++budget){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();assert(source);
        XL6_ExecutionResourceV1* resource=createResource(source,true);XL6_ExecutionScopeV1 entry={0};
        assert(xl6ExecutionResourceEnterV1(resource,true,&entry));
        assert(xrtMemDebugFailAfter(budget));bool ok=xl6TaskPoolControlV1Get()->Start(entry.pool);
        bool injected=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xrtClearError();
        if(ok){assert(!injected);++succeeded;}else{assert(injected);++failed;}
        xl6ExecutionResourceLeaveV1(&entry);
        xrtownershipscope frozen={0};freeze(&frozen);xrtownershipref ledger=xl6ExecutionFailuresOwnershipV1(resource->failures);size_t refs;
        assert(ledger.Ops->Count(ledger.Data,&refs)&&refs==2);assert(xrtOwnershipScopeEnd(&frozen));
        xl6ExecutionResourceReleaseV1(resource);drain(source);destroy(&source,true);balanced(&before);
    }
    assert(failed&&succeeded);
}
static void registeredResourceTail(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();assert(source);
    XL6_ExecutionResourceV1* r=createResource(source,true);XL6_ExecutionScopeV1 entry={0};
    assert(xl6ExecutionResourceEnterV1(r,true,&entry));
    str path=NULL;xfile raw=xrtFileTemp(NULL,"xl6-pool-exit-",".bin",&path);assert(raw&&path);
    XL6_AsyncFile* file=xl6AsyncFileAdoptV1(entry.pool,raw);assert(file);
    xfuture* write=xl6AsyncFileWriteV1(file,0,(const uint8*)"a\0b",3);assert(write);
    xl6ExecutionResourceLeaveV1(&entry);assert(xl6ExecutionResourceCloseV1(r));
    assert(xl6ExecutionResourceEnterV1(r,false,&entry));XL6_TaskPoolWaitStatusV1 status=XL6_TASK_POOL_TIMEOUT_V1;
    /* Keep the FILE OWNER alive through actual worker exit. Only the real
     * registry close protocol can release its native credit before join. */
    assert(xl6TaskPoolControlV1Get()->WaitFor(entry.pool,5000,NULL,&status)&&status==XL6_TASK_POOL_READY_V1);
    assert(xl6AsyncFileClosedV1(file));xl6ExecutionResourceLeaveV1(&entry);
    assert(xrtFutureWaitFor(write,0)==XWAIT_OK);xfutureresult result={0};assert(xrtFutureResult(write,&result));
    assert(result.State==XFUTURE_RESOLVED&&((xfilechange*)result.Value)->Size==3);
    xrtFutureDestroy(write);xl6AsyncFileDestroyV1(file);
    assert(xrtFileDelete(path));xrtFree(path);
    xl6ExecutionResourceReleaseV1(r);drain(source);destroy(&source,true);balanced(&before);
}
static xatomic32 exitCalls;
static bool checkedWorkerExit(void)
{
    (void)xrtAtomic32FetchAdd(&exitCalls,1,XMEMORY_RELEASE);
    const unsigned* mode=xrtThreadKeyGet(exitModeKey);assert(mode);
    if(*mode==2)xrtSetErrorTake(xrtErrorCreate(XERR_ARGUMENT,"worker-exit",902,"original exit failure"));
    return *mode==0;
}
static void markWorkerExit(void* raw){assert(xrtThreadKeySet(exitModeKey,raw));}
static xtaskoutcome markPoolWorkerExit(xcancel* cancel,void* raw,xtaskvalue* output)
{(void)cancel;(void)output;markWorkerExit(raw);return XTASK_SUCCESS;}
static void exitFailureRound(bool pool,unsigned mode,bool observed)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);uint32_t calls=xrtAtomic32Load(&exitCalls,XMEMORY_ACQUIRE);
    XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();assert(source);
    XL6_ExecutionResourceV1* r=createResource(source,pool);XL6_ExecutionScopeV1 entry={0};
    assert(xl6ExecutionResourceEnterV1(r,true,&entry));
    if(pool){
        XL6_TaskNativeScopeV1 native={0};assert(xl6TaskNativeBeginV1(entry.pool,&native));
        for(unsigned i=0;i<3;++i){
            xfuture* future=xrtTaskSubmit(native.native,markPoolWorkerExit,&mode,NULL);assert(future);
            assert(xrtFutureWaitFor(future,UINT64_C(5000000))==XWAIT_OK);xrtFutureDestroy(future);
            assert(xrtAtomic32Load(&exitCalls,XMEMORY_ACQUIRE)==calls); /* not per-job */
        }
        xl6TaskNativeEndV1(&native);
    }else assert(xrtExecutorSubmit(entry.executor,markWorkerExit,&mode,NULL,NULL));
    xl6ExecutionResourceLeaveV1(&entry);
    assert(xl6ExecutionResourceCloseV1(r)&&xl6ExecutionResourceEnterV1(r,false,&entry));
    if(pool){
        XL6_TaskPoolWaitStatusV1 status=XL6_TASK_POOL_TIMEOUT_V1;
        assert(xl6TaskPoolControlV1Get()->WaitFor(entry.pool,5000,NULL,&status)&&status==XL6_TASK_POOL_READY_V1);
        XL6_TaskPoolStatsV1 stats={.size=sizeof(stats)};assert(xl6TaskPoolControlV1Get()->Snapshot(entry.pool,&stats));
        assert(stats.submitted==3&&stats.succeeded==3&&!stats.failed); /* exit != task failure */
    }else assert(xrtExecutorWaitFor(entry.executor,UINT64_C(5000000))==XWAIT_OK);
    xl6ExecutionResourceLeaveV1(&entry);
    assert(xrtAtomic32Load(&exitCalls,XMEMORY_ACQUIRE)==calls+1);
    XL6_ExecutionFailureSnapshotV1 state={.size=sizeof(state)};
    assert((pool?xl6ExecutionTaskPoolExitFailuresV1(r,&state):xl6ExecutionExecutorFailuresV1(r,&state))&&state.count==(mode?1u:0u));
    if(observed){
        if(pool)assert(xl6ExecutionTaskPoolCheckExitV1(r)==!mode);
        else {XL6_ExecutorWaitStatusV1 status=XL6_EXECUTOR_TIMEOUT_V1;
            assert(xl6ExecutionExecutorWaitV1(r,0,&status)==!mode&&status==XL6_EXECUTOR_READY_V1);}
        if(mode)assert(mode==1?!xrtGetError():xrtErrorCode(xrtGetError())==902);
        xrtClearError();assert((pool?xl6ExecutionTaskPoolCheckExitV1(r):xl6ExecutionExecutorThrowIfFailedV1(r))==!mode);xrtClearError();}
    xl6ExecutionResourceReleaseV1(r);drain(source);
    bool ok=!mode||observed;assert(xl6ExecutionSourceCheckV1(source)==ok);
    if(!ok)assert(mode==1?!xrtGetError():xrtErrorCode(xrtGetError())==902);
    xrtClearError();destroy(&source,ok);balanced(&before);
}
int main(void)
{
    const xallocator allocator={NULL,allocate,resize,release};assert(xrtSetAllocator(&allocator));
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();assert(xrtTemp(8)&&xrtTempClear());
    xl6_error_scope warm={0};assert(xl6ErrorScopeBegin(&warm));xl6_error_result clean=xl6ErrorScopeEnd(&warm);assert(clean.ok);xrtErrorFree(clean.error);
    for(unsigned i=0;i<8;++i)for(unsigned pool=0;pool<2;++pool){selfReleaseRound(pool!=0,0);entryRound(pool!=0);closedRound(pool!=0);creationCloseRound(pool!=0);}
    for(unsigned i=0;i<8;++i){selfReleaseRound(false,1);selfReleaseRound(false,2);stalledRound();}
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainExecutionSourceV1Get(),xl6OwnershipDomainExecutionExecutorV1Get(),
        xl6OwnershipDomainExecutionPoolV1Get(),xl6OwnershipDomainNativeExecutorV1Get(),xl6OwnershipDomainTaskPoolV1Get(),xl6OwnershipDomainExecutorJobsV1Get(),
        xl6OwnershipDomainExecutionFailuresV1Get(),xl6OwnershipDomainTaskPoolExitV1Get()};
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(*domains));assert(catalog);
    identityRefusal(catalog,false);identityRefusal(catalog,true);
    lazyExitIdentity(catalog);
    for(unsigned order=0;order<8;++order){
        for(unsigned pool=0;pool<2;++pool){graphRound(catalog,pool!=0,0,order,false);graphRound(catalog,pool!=0,0,order,true);}
        graphRound(catalog,true,3,order,false);graphRound(catalog,true,3,order,true);
        for(unsigned empty=1;empty<3;++empty){graphRound(catalog,false,empty,order,false);graphRound(catalog,false,empty,order,true);}}
    for(unsigned i=0;i<8;++i)for(unsigned flags=0;flags<8;++flags)failureGraphRound(catalog,(flags&1)!=0,(flags&2)!=0,(flags&4)!=0);
    puts("Execution resource error retirement: 64 observed/unobserved and NULL/allocated first failures; graph and ordinary checked notifications, no repeated handled error");
    xl6OwnershipCatalogDestroyV1(catalog);
    allocationPrefixes();
    lazyStartPrefixes();
    for(unsigned i=0;i<16;++i)registeredResourceTail();
    exitModeKey=xrtThreadKeyCreate(NULL);assert(exitModeKey&&xl6ThreadExitRegisterLocalV1(checkedWorkerExit));
    for(unsigned i=0;i<8;++i)for(unsigned mode=0;mode<3;++mode){
        exitFailureRound(false,mode,false);exitFailureRound(false,mode,true);
        exitFailureRound(true,mode,false);exitFailureRound(true,mode,true);
    }
    assert(xrtThreadKeyDestroy(exitModeKey));
    puts("Executor worker exit: 48 real-thread normal/NULL/allocated failures; explicit wait/history and unobserved checked retirement, exact allocation balance");
    puts("TaskPool worker exit: 48 real-thread normal/NULL/allocated failures, 144 persistent jobs, 16 lazy owned graphs, 32 lazy start prefixes, unknown exit Ops refused before execution; exact context transfer, checked history/retirement and native stats");
    puts("TaskPool resource-before-exit: 16 actual asynchronous files retained across join, accepted writes delivered, registry released native credits, exact memory balance");
    puts("Execution resources: 32 worker last-owner tails (16 failed), 16 held entries, 16 owned close/waits, 16 real creation-close races, 8 explicit retries, 80 exact graphs (32 empty) with restore/arbitrary finish order, 120 allocation prefixes, identity refusal and balanced lifetimes");return 0;
}
