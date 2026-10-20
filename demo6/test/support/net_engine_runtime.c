#include "net-engine-modules.h"
#define XRT_MODULE_MEMORY_DEBUG
#define XRUNTIME_IMPLEMENTATION
#include "lib/xruntime/xruntime.h"
#include "src/runtime/xl6_execution_resource_internal.h"
#include "src/runtime/xl6_execution_net_engine.h"
#include "src/runtime/xl6_detached_work.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef XL6_ExecutionResourceV1 Resource;
typedef struct Context {xatomic32 runs,drops,started,resume,control;unsigned fail; xrtownershipref native;bool inspect,reenter;} Context;
typedef struct Environment {Context* context;Resource* owned;} Environment;
static const int layout=3;
static XL6_OwnershipCatalog* catalog;
static void waitFor(xatomic32* value,uint32 target)
{xdeadline end=xrtDeadlineAfter(UINT64_C(5000000));while(xrtAtomic32Load(value,XMEMORY_ACQUIRE)<target){assert(!xrtDeadlineExpired(end));xrtThreadYield();}}
static void freeze(xrtownershipscope* scope)
{xdeadline end=xrtDeadlineAfter(UINT64_C(5000000));while(!xrtOwnershipFreezeTryBegin(scope)){assert(!xrtDeadlineExpired(end));xrtThreadYield();}}
static void balanced(const xmemdebugsnapshot* before)
{xrtClearError();xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&
    after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&before->InvalidFreeCount==after.InvalidFreeCount&&
    before->DoubleFreeCount==after.DoubleFreeCount&&before->UseAfterFreeCount==after.UseAfterFreeCount);}
static void destroy(XL6_ExecutionSourceV1** source,bool expected)
{
    assert(xl6ExecutionSourceCloseV1(*source));xdeadline end=xrtDeadlineAfter(UINT64_C(5000000));
    while(*source){XL6_ExecutionRetireResultV1 result=xl6ExecutionSourceDestroyV1(source);assert(result.ok==expected);
        if(result.ready)break;
        assert(!xrtDeadlineExpired(end));xrtClearError();xrtThreadYield();}
}
static void drain(XL6_ExecutionSourceV1* source)
{
    xdeadline end=xrtDeadlineAfter(UINT64_C(5000000));
    for(;;){XL6_ExecutionSourceSnapshotV1 state={.size=sizeof(state)};assert(xl6ExecutionSourceSnapshotV1Get(source,&state));
        if(!state.resources&&!state.active)return;
        assert(!xrtDeadlineExpired(end));xrtThreadYield();}
}
static bool validate(const void* descriptor,const void* raw,size_t bytes)
{return descriptor==&layout&&raw&&bytes==sizeof(Environment);}
static bool trace(const void* descriptor,const void* raw,size_t bytes,XL6_ModuleOwnershipVisit visit,void* user)
{const Environment* env=raw;return validate(descriptor,raw,bytes)&&(!env->owned||visit(xl6ExecutionResourceOwnershipV1(env->owned),user));}
static bool drop(const void* descriptor,void* raw,size_t bytes)
{
    assert(validate(descriptor,raw,bytes));Environment* env=raw;Context* c=env->context;
    xl6ExecutionResourceReleaseV1(env->owned);xrtFree(env);xrtAtomic32FetchAdd(&c->drops,1,XMEMORY_ACQ_REL);return true;
}
static const XL6_ClosureEnvironmentOpsV1 environmentOps={sizeof(environmentOps),validate,trace,drop};
static bool body(void* raw)
{
    Environment* env=raw;Context* c=env->context;xrtAtomic32Store(&c->started,1,XMEMORY_RELEASE);
    waitFor(&c->resume,1);
    if(c->inspect){
        xrtownershipscope frozen={0};freeze(&frozen);const xrtownershipadapterv1* adapter=NULL;
        const xrtownershippreparationv1* prep=NULL;const XL6_OwnershipFinishV1* completion=NULL;
        assert(xl6OwnershipCatalogProbeV1(c->native,catalog,&adapter,&prep,&completion)==XL6_OWNERSHIP_ADMISSION_BUSY);
        assert(!adapter&&!prep&&!completion&&xrtOwnershipScopeEnd(&frozen));
    }
    if(c->reenter){
        waitFor(&c->control,1);bool ready=true;
        assert(!xl6ExecutionNetEngineTryStopV1(env->owned,&ready)&&!ready&&xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
        XL6_ExecutionFailureSnapshotV1 snapshot={.size=sizeof(snapshot)};
        assert(!xl6ExecutionNetEngineFailuresV1(env->owned,&snapshot)&&xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    }
    xrtAtomic32FetchAdd(&c->runs,1,XMEMORY_ACQ_REL);
    if(c->fail==1){xrtSetErrorTake(xrtErrorCreate(XERR_ARGUMENT,"engine-fixture",701,"engine first"));return false;}
    if(c->fail==2)return false;
    return true;
}
static XL6_Closure* make(Context* c,Resource* owned)
{
    Environment* env=xrtMalloc(sizeof(*env));assert(env);*env=(Environment){c,owned?xl6ExecutionResourceRetainV1(owned):NULL};
    XL6_Closure* callback=xl6ClosureCreateEnvironmentV1(NULL,(void (*)(void))body,NULL,env,sizeof(*env),&layout,&environmentOps,NULL,NULL,0);assert(callback);return callback;
}
static Resource* create(XL6_ExecutionSourceV1* source)
{xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;config.Backend=XNET_PORT_SELECT;return xl6ExecutionNetEngineCreateV1(source,&config);}
static uint64_t failures(Resource* r)
{XL6_ExecutionFailureSnapshotV1 value={.size=sizeof(value)};assert(xl6ExecutionNetEngineFailuresV1(r,&value)&&!value.saturated);return value.count;}
static void lifecycle(unsigned mode,bool lastOwner)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Context c={0};c.fail=mode;c.inspect=true;xrtAtomic32Store(&c.resume,1,XMEMORY_RELEASE);
    XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();Resource* r=create(source);assert(source&&r);c.native=xrtNetEngineOwnership(r->engine);
    XL6_Closure* callback=make(&c,lastOwner?r:NULL);assert(xl6ExecutionNetEngineStartV1(r));
    assert(xl6ExecutionNetEnginePostV1(r,0,callback));xl6ClosureRuntimeV1Get()->Release(callback);
    if(lastOwner){xl6ExecutionResourceReleaseV1(r);drain(source);assert(xl6ExecutionSourceCheckV1(source)==!mode);xrtClearError();destroy(&source,!mode);}
    else{
        assert(xl6ExecutionResourceCloseV1(r));bool ready=false;
        assert(xl6ExecutionNetEngineWaitV1(r,5000,&ready)==!mode&&ready&&failures(r)==(mode?1u:0u));
        if(mode)assert(mode==1?xrtErrorCode(xrtGetError())==701:!xrtGetError());
        xrtClearError();
        assert(xl6ExecutionNetEngineCheckV1(r)==!mode);xrtClearError();
        xl6ExecutionResourceReleaseV1(r);drain(source);assert(xl6ExecutionSourceCheckV1(source));destroy(&source,true);
    }
    assert(xrtAtomic32Load(&c.runs,XMEMORY_ACQUIRE)==1&&xrtAtomic32Load(&c.drops,XMEMORY_ACQUIRE)==1);balanced(&before);
}
static void timers(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();Resource* r=create(source);assert(r);
    Context c={0};xrtAtomic32Store(&c.resume,1,XMEMORY_RELEASE);XL6_Closure* callback=make(&c,NULL);assert(xl6ExecutionNetEngineStartV1(r));
    uint64_t id=0;assert(xl6ExecutionNetEngineAfterV1(r,0,0,callback,&id)&&id);waitFor(&c.runs,1);
    assert(xl6ExecutionNetEngineAfterV1(r,0,60000,callback,&id)&&xl6ExecutionNetEngineCancelTimerV1(r,id));
    assert(xl6ExecutionNetEngineAfterV1(r,0,60000,callback,&id));xl6ClosureRuntimeV1Get()->Release(callback);
    assert(xl6ExecutionNetEngineStopV1(r)&&failures(r)==0&&xrtAtomic32Load(&c.runs,XMEMORY_ACQUIRE)==1);
    assert(xrtAtomic32Load(&c.drops,XMEMORY_ACQUIRE)==1&&xl6ExecutionNetEngineStartV1(r));
    xnetportbackendinfo info;assert(xl6ExecutionNetEngineBackendV1(r,0,&info)&&info.Backend==XNET_PORT_SELECT);
    assert(xl6ExecutionNetEngineStopV1(r));xl6ExecutionResourceReleaseV1(r);drain(source);destroy(&source,true);balanced(&before);
}
typedef struct Control {Resource* resource;Context* context;} Control;
static int32 controlThread(void* raw)
{
    Control* data=raw;XL6_ExecutionScopeV1 scope={0};assert(xl6ExecutionResourceControlV1(data->resource,false,&scope));
    xrtAtomic32Store(&data->context->control,1,XMEMORY_RELEASE);
    assert(xrtNetEngineStop(scope.engine));xl6ExecutionResourceLeaveV1(&scope);return 0;
}
static void competingControl(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();Resource* r=create(source);assert(r);
    Context c={0};c.reenter=true;XL6_Closure* callback=make(&c,r);assert(xl6ExecutionNetEngineStartV1(r)&&xl6ExecutionNetEnginePostV1(r,0,callback));
    xl6ClosureRuntimeV1Get()->Release(callback);waitFor(&c.started,1);Control control={r,&c};xthread* thread=xrtThreadCreate(controlThread,&control,0);assert(thread);
    waitFor(&c.control,1);xrtAtomic32Store(&c.resume,1,XMEMORY_RELEASE);assert(xrtThreadWait(thread)==XWAIT_OK);xrtThreadDestroy(thread);
    assert(failures(r)==0&&xl6ExecutionNetEngineStartV1(r)&&xl6ExecutionNetEngineStopV1(r));xl6ExecutionResourceReleaseV1(r);drain(source);destroy(&source,true);balanced(&before);
}
static void producerFailure(bool diagnostic)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_ExecutionFailuresV1* state=xl6ExecutionFailuresCreateV1();Context c={0};
    XL6_Closure* callback=make(&c,NULL);XL6_DetachedWorkV1* work=xl6DetachedWorkCreateV1(state,callback);assert(work);
    xl6ClosureRuntimeV1Get()->Release(callback);
    if(diagnostic)xrtSetErrorTake(xrtErrorCreate(XERR_ARGUMENT,"engine-fixture",702,"producer first"));
    /* Controlled result-adapter unit check, not a claimed native OOM path. */
    xl6NetEngineTimerPolicyV1Get()->Proc(NULL,1,XNET_RESULT_ERROR,work);xl6NetEngineTimerPolicyV1Get()->Drop(work);
    assert(!xl6ExecutionFailuresCheckV1(state));assert(diagnostic?xrtErrorCode(xrtGetError())==702:!xrtGetError());xrtClearError();
    XL6_ExecutionFailureSnapshotV1 result={.size=sizeof(result)};assert(xl6ExecutionFailuresSnapshotV1(state,&result)&&result.count==1);
    assert(!xrtAtomic32Load(&c.runs,XMEMORY_ACQUIRE)&&xrtAtomic32Load(&c.drops,XMEMORY_ACQUIRE)==1);xl6ExecutionFailuresReleaseV1(state);balanced(&before);
}
enum {LIMIT=16};
typedef struct Node {xrtownershipref ref;const xrtownershipadapterv1* adapter;const xrtownershippreparationv1* prepare;size_t refs,incoming;} Node;
typedef struct Graph {Node nodes[LIMIT];size_t count,edges;} Graph;
static bool edge(xrtownershipref ref,void* raw)
{Graph* g=raw;size_t i=0;while(i<g->count&&(g->nodes[i].ref.Data!=ref.Data||g->nodes[i].ref.Ops!=ref.Ops))++i;
    if(i==g->count){assert(g->count<LIMIT);g->nodes[g->count++].ref=ref;}++g->nodes[i].incoming;++g->edges;return true;}
static bool collect(Graph* g,XL6_ExecutionSourceV1* source)
{
    *g=(Graph){0};g->nodes[0].ref=xl6ExecutionSourceOwnershipV1(source);g->count=1;
    for(size_t i=0;i<g->count;++i){Node* n=&g->nodes[i];const XL6_OwnershipFinishV1* finish=NULL;
        XL6_OwnershipAdmissionV1 status=xl6OwnershipCatalogProbeV1(n->ref,catalog,&n->adapter,&n->prepare,&finish);
        if(status==XL6_OWNERSHIP_ADMISSION_BUSY)return false;
        assert(status==XL6_OWNERSHIP_ADMISSION_ADMITTED&&n->adapter&&!finish&&n->ref.Ops->Count(n->ref.Data,&n->refs));
        assert(n->ref.Ops->Trace(n->ref.Data,edge,g));}
    xrtownershipresult actual={0};assert(xrtOwnershipInspect(&g->nodes[0].ref,1,NULL,0,&actual));
    assert(actual.NodeCount==g->count&&actual.EdgeCount==g->edges);return true;
}
static void graph(unsigned order,bool restore,bool timer)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();Resource* r=create(source);assert(r);
    Context c={0};if(timer){XL6_Closure* callback=make(&c,r);uint64_t id=0;assert(xl6ExecutionNetEngineStartV1(r)&&
        xl6ExecutionNetEngineAfterV1(r,0,60000,callback,&id));xl6ClosureRuntimeV1Get()->Release(callback);}
    Graph g;xrtownershipscope frozen={0};xdeadline deadline=xrtDeadlineAfter(UINT64_C(5000000));
    for(;;){freeze(&frozen);if(collect(&g,source))break;assert(xrtOwnershipScopeEnd(&frozen)&&!xrtDeadlineExpired(deadline));xrtThreadYield();}
    assert(g.count==(timer?6u:4u));int token=0;
    for(size_t i=0;i<g.count;++i)assert(g.nodes[i].adapter->Hold(g.nodes[i].ref.Data)&&g.nodes[i].adapter->Claim(g.nodes[i].ref.Data,&token));
    assert(xrtOwnershipScopeEnd(&frozen));bool ready[LIMIT]={false};size_t pending=g.count;
    while(pending){for(size_t step=0;step<g.count;++step){size_t i=order&1?g.count-1-step:step;Node* n=&g.nodes[i];if(ready[i])continue;
        xrtownershipprepareresult result=n->prepare?n->prepare->Prepare(n->ref.Data,&token):XRT_OWNERSHIP_PREPARE_READY;
        assert(result!=XRT_OWNERSHIP_PREPARE_FAILED);if(result==XRT_OWNERSHIP_PREPARE_READY){ready[i]=true;--pending;}}
        assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    freeze(&frozen);for(size_t i=0;i<g.count;++i)assert(!g.nodes[i].prepare||g.nodes[i].prepare->Ready(g.nodes[i].ref.Data));
    for(size_t i=0;i<g.count;++i){if(restore)g.nodes[i].adapter->Restore(g.nodes[i].ref.Data,&token);else g.nodes[i].adapter->Clear(g.nodes[i].ref.Data,&token);}
    assert(xrtOwnershipScopeEnd(&frozen));
    if(!restore){assert(xrtMemDebugFailAfter(0));for(unsigned pass=0;pass<2;++pass)for(size_t step=0;step<g.count;++step){
        size_t i=order&2?g.count-1-step:step;Node* n=&g.nodes[i];assert(!n->adapter->Finish||n->adapter->Finish(n->ref.Data,&token));}
        assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();}
    if(restore)for(size_t step=0;step<g.count;++step){size_t i=order&4?g.count-1-step:step;g.nodes[i].adapter->Drop(g.nodes[i].ref.Data);}
    xl6ExecutionResourceReleaseV1(r);if(restore)drain(source);destroy(&source,true);
    if(!restore)for(size_t step=0;step<g.count;++step){size_t i=order&4?g.count-1-step:step;g.nodes[i].adapter->Drop(g.nodes[i].ref.Data);}
    if(timer)assert(!xrtAtomic32Load(&c.runs,XMEMORY_ACQUIRE)&&xrtAtomic32Load(&c.drops,XMEMORY_ACQUIRE)==1);
    balanced(&before);
}
static unsigned nullDrops;
static bool forbiddenTrace(const Resource* resource,xrtownershipvisitor visit,void* user)
{(void)resource;(void)visit;(void)user;abort();}
static void exactFamily(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();assert(source);
    XL6_ExecutionCreateOptionsV1 wrong={.kind=XL6_EXECUTION_TASK_POOL_V1};
    assert(!xl6ExecutionCreateInternalV1(source,xl6ExecutionNetEngineFamilyV1Get(),&wrong)&&xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
    Resource* r=create(source);assert(r);const XL6_ExecutionFamilyV1* actual=r->family;XL6_ExecutionFamilyV1 copied=*actual;copied.Trace=forbiddenTrace;
    xrtownershipscope frozen={0};freeze(&frozen);r->family=&copied;
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* prepare=NULL;const XL6_OwnershipFinishV1* finish=NULL;
    assert(xl6OwnershipCatalogProbeV1(xl6ExecutionResourceOwnershipV1(r),catalog,&adapter,&prepare,&finish)==XL6_OWNERSHIP_ADMISSION_REFUSED);
    assert(!adapter&&!prepare&&!finish);r->family=actual;assert(xrtOwnershipScopeEnd(&frozen));
    xl6ExecutionResourceReleaseV1(r);drain(source);destroy(&source,true);balanced(&before);
}
static bool nullValidate(const void* descriptor,const void* raw,size_t bytes)
{return descriptor==&layout&&!raw&&!bytes;}
static bool nullTrace(const void* descriptor,const void* raw,size_t bytes,XL6_ModuleOwnershipVisit visit,void* user)
{(void)visit;(void)user;return nullValidate(descriptor,raw,bytes);}
static bool nullDrop(const void* descriptor,void* raw,size_t bytes)
{assert(nullValidate(descriptor,raw,bytes));++nullDrops;return true;}
static bool nullBody(void* raw){assert(!raw);return true;}
static void nullEnvironment(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    const XL6_ClosureEnvironmentOpsV1 ops={sizeof(ops),nullValidate,nullTrace,nullDrop};
    XL6_Closure* closure=xl6ClosureCreateEnvironmentV1(NULL,(void (*)(void))nullBody,NULL,NULL,0,&layout,&ops,NULL,NULL,0);assert(closure);
    xrtownershipref ref=xl6ClosureRuntimeV1Get()->Ownership(closure);xrtownershipscope frozen={0};freeze(&frozen);
    const xrtownershipadapterv1* adapter=xl6ClosureOwnershipAdapterV1(ref);int token=0;assert(adapter&&adapter->Hold(ref.Data)&&adapter->Claim(ref.Data,&token));
    adapter->Clear(ref.Data,&token);assert(xrtOwnershipScopeEnd(&frozen));unsigned previous=nullDrops;
    assert(adapter->Finish(ref.Data,&token)&&adapter->Finish(ref.Data,&token)&&nullDrops==previous+1);
    xl6ClosureRuntimeV1Get()->Release(closure);adapter->Drop(ref.Data);assert(nullDrops==previous+1);balanced(&before);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();
    xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));xl6_error_result result=xl6ErrorScopeEnd(&scope);assert(result.ok);xrtErrorFree(result.error);
    const XL6_OwnershipDomainV1* domains[]={
        #include "net-engine-domains.h"
    };catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(*domains));assert(catalog);
    exactFamily();
    for(unsigned i=0;i<4;++i){for(unsigned mode=0;mode<3;++mode){lifecycle(mode,false);lifecycle(mode,true);}
        timers();competingControl();producerFailure(false);producerFailure(true);nullEnvironment();}
    for(unsigned order=0;order<8;++order)for(unsigned flags=0;flags<4;++flags)graph(order,flags&1,flags&2);
    xl6OwnershipCatalogDestroyV1(catalog);
    puts("Engine runtime: 24 owned completion/error tails, 4 timer/restart and exclusive-control rounds, 8 producer adapter failures, 32 exact graphs/restore/finish orders; balanced memory");return 0;
}
