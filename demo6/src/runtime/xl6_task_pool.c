#define XRT_MODULE_TASK_POOL
#define XRT_MODULE_MEMORY
#define XRT_MODULE_ATOMIC
#include "lib/xrt/xrt.h"
#include "xl6_task.h"
#include "xl6_ownership_scope.h"
#include "src/api/xl_foreign_error.h"
#include "xl6_concurrent_deadline.h"
#include <stddef.h>

#include "xl6_task_pool_internal.h"

struct XL6_TaskPool {
    xmutex* gate;
    xcond* changed;
    xtaskpool* native;
    void* exitData; /* one real owner until native lazy creation succeeds */
    const xtaskpoolworkerexitownershipv1* exitPolicy; /* immutable resident */
    xtaskpoolconfig config;
    XL6_TaskResourceV1* resources;
    size_t references, entries, activities, submitting, controls;
    xtaskpoolstats retiredStats;
    const void* claim;
    bool ownerHeld, closing, cancelling, quiescing, creating, retiring, cleared, retiredStatsValid;
    /* A nonblocking stop owns a pending duty even before a graph can admit
     * the native pool. Joined completion is monotonic after closed admission. */
    bool quiescencePending, exitComplete;
};
static void pool_close_resources(XL6_TaskPool*);
static xatomic64 activity_lock={0};
static TaskActivity* activities;
static void lock_activity(void)
{
    for(;;) { uint64 expected=0;
        if(xrtAtomic64CompareExchange(&activity_lock,&expected,1,XMEMORY_ACQUIRE,XMEMORY_RELAXED))return;
        xrtThreadYield();
    }
}
static void unlock_activity(void) { xrtAtomic64Store(&activity_lock,0,XMEMORY_RELEASE); }
static void mutation_begin(xrtownershipscope* scope) { if(!xrtOwnershipMutationBegin(scope))abort(); }
static void mutation_end(xrtownershipscope* scope) { if(!xrtOwnershipScopeEnd(scope))abort(); }
static void lock_pool(const XL6_TaskPool* pool) { if(!xrtMutexLock(pool->gate))abort(); }
static void unlock_pool(const XL6_TaskPool* pool) { if(!xrtMutexUnlock(pool->gate))abort(); }
void xl6TaskPoolDropInternalV1(const void* data)
{
    XL6_TaskPool* pool=(XL6_TaskPool*)data; xrtownershipscope mutation={0};
    if(!pool)return;
    mutation_begin(&mutation);lock_pool(pool);
    if(!pool->references)abort();
    bool last=--pool->references==0;
    if(last && (pool->ownerHeld||pool->native||pool->entries||pool->activities||
        pool->submitting||pool->controls||pool->creating||pool->quiescing||pool->retiring||pool->resources||pool->exitData))abort();
    unlock_pool(pool);
    if(last){
        if(!xrtCondDestroy(pool->changed)||!xrtMutexDestroy(pool->gate))abort();
        xrtFree(pool);
    }
    mutation_end(&mutation);
}
bool xl6TaskPoolHoldInternalV1(const void* data)
{
    XL6_TaskPool* pool=(XL6_TaskPool*)data;xrtownershipscope mutation={0};bool held=false;
    if(!pool)return false;
    mutation_begin(&mutation);lock_pool(pool);
    if(pool->references&&pool->references<SIZE_MAX&&!pool->cleared){++pool->references;held=true;}
    unlock_pool(pool);mutation_end(&mutation);return held;
}
/* Callers already own the receiver. An entry protects the complete native
 * call, including private bridge allocation and callback/rollback tails. */
void xl6TaskPoolEnterInternalV1(XL6_TaskPool* pool)
{
    xrtownershipscope mutation={0};mutation_begin(&mutation);lock_pool(pool);
    if(!pool->references||pool->references==SIZE_MAX||pool->entries==SIZE_MAX)abort();
    ++pool->references;++pool->entries;unlock_pool(pool);mutation_end(&mutation);
}
void xl6TaskPoolLeaveInternalV1(XL6_TaskPool* pool)
{
    xrtownershipscope mutation={0};mutation_begin(&mutation);lock_pool(pool);
    if(!pool->entries)abort();
    --pool->entries;unlock_pool(pool);xl6TaskPoolDropInternalV1(pool);mutation_end(&mutation);
}
void xl6TaskActivityEnterInternalV1(TaskActivity* activity,XL6_TaskPool* pool)
{
    xrtownershipscope mutation={0}; mutation_begin(&mutation);
    lock_pool(pool);
    if(!pool->references||pool->references==SIZE_MAX||pool->activities==SIZE_MAX||pool->cleared)abort();
    ++pool->references;++pool->activities;unlock_pool(pool);
    activity->pool=pool; activity->thread=xrtThreadCurrentId(); lock_activity();
    activity->next=activities; if(activities)activities->previous=activity; activities=activity;
    unlock_activity(); mutation_end(&mutation);
}
void xl6TaskActivityLeaveInternalV1(TaskActivity* activity)
{
    xrtownershipscope mutation={0}; mutation_begin(&mutation); lock_activity();
    if(activity->previous)activity->previous->next=activity->next;else activities=activity->next;
    if(activity->next)activity->next->previous=activity->previous;
    unlock_activity();XL6_TaskPool* pool=activity->pool;*activity=(TaskActivity){0};
    lock_pool(pool);if(!pool->activities)abort();--pool->activities;unlock_pool(pool);
    xl6TaskPoolDropInternalV1(pool);mutation_end(&mutation);
}
static bool activity_current(const XL6_TaskPool* pool)
{
    uint64 thread=xrtThreadCurrentId(); bool found=false; lock_activity();
    for(TaskActivity* item=activities;item;item=item->next)if(item->pool==pool && item->thread==thread){found=true;break;}
    unlock_activity(); return found;
}
static void error_set(xerrkind kind,const char* name,const char* message)
{
    if(xrtGetError())return;
    xerrordesc desc={kind,0,0,"xlang","throw",message,name,NULL};
    xerror* error=xrtErrorBuild(&desc); if(error)xrtSetErrorTake(error);else xrtSetErrorKind(kind);
}
static XL6_TaskPool* pool_create_body(const XL6_TaskPoolConfig* config)
{
    XL6_TaskPool* pool=xrtMalloc(sizeof(*pool));
    if(!pool)return NULL;
    *pool=(XL6_TaskPool){0};
    if(config)pool->config=(xtaskpoolconfig){config->threads,config->queueLimit,config->stackSize};
    pool->gate=xrtMutexCreate(); if(!pool->gate){xrtFree(pool);return NULL;}
    pool->changed=xrtCondCreate();
    if(!pool->changed){xrtMutexDestroy(pool->gate);xrtFree(pool);return NULL;}
    pool->references=1;pool->ownerHeld=true;
    return pool;
}
XL6_TaskPool* xl6TaskPoolCreateInternalV1(const XL6_TaskPoolConfig* config) { XL6_OWNERSHIP_RETURN(XL6_TaskPool*,pool_create_body(config)); }
XL6_TaskPool* xl6TaskPoolCreateOwnedExitInternalV1(const XL6_TaskPoolConfig* config,void* data,
    const xtaskpoolworkerexitownershipv1* policy)
{
    if(!data||!policy||policy->size!=sizeof(*policy)||!policy->Exit||!policy->Drop||
        !policy->Ops||!policy->Ops->Count||!policy->Ops->Trace){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    XL6_TaskPool* pool=xl6TaskPoolCreateInternalV1(config);
    if(pool){pool->exitData=data;pool->exitPolicy=policy;} /* private, unpublished */
    return pool;
}
bool xl6TaskPoolQuiesceInternalV1(XL6_TaskPool* pool)
{
    xrtownershipscope mutation={0}; xtaskpool* native; bool ok;xtaskpoolstats stats={0};
    if(!pool)return true;
    xl6TaskPoolEnterInternalV1(pool);
    mutation_begin(&mutation); bool self=activity_current(pool); lock_pool(pool);
    if(pool->retiring||(pool->claim&&!pool->cleared)){
        unlock_pool(pool);mutation_end(&mutation);xl6TaskPoolLeaveInternalV1(pool);return false;
    }
    /* Preserve V1: a self-quiesce closes new admission but never joins itself. */
    pool->closing=true;
    if(pool->quiescing || self) {
        unlock_pool(pool); mutation_end(&mutation);
        error_set(XERR_STATE,"StateError",self?"task activity cannot join its own module pool":"module task quiescence is already running");
        xl6TaskPoolLeaveInternalV1(pool);return false;
    }
    pool->quiescing=true; pool->quiescencePending=true; unlock_pool(pool); mutation_end(&mutation);
    /* A synchronous cancellation/pruning callback can still be returning from
     * Submit. Wait for its real submission credit, with no mutation held. */
    lock_pool(pool);
    while(pool->submitting)if(xrtCondWait(pool->changed,pool->gate)!=XWAIT_OK)abort();
    native=pool->native; unlock_pool(pool);
    pool_close_resources(pool);
    /* Native finalizers may still be inside a pool resource-release entry.
     * Destroy requires exclusive external entry, so settle native jobs and
     * finalizer tails before requesting destruction, never by retrying it. */
    ok=!native || xrtTaskPoolClose(native);
    if(ok){
        lock_pool(pool);
        while(pool->controls)if(xrtCondWait(pool->changed,pool->gate)!=XWAIT_OK)abort();
        unlock_pool(pool);
        ok=!native || (xrtTaskPoolWait(native)==XWAIT_OK && xrtTaskPoolGet(native,&stats) && xrtTaskPoolDestroy(native));
    }
    mutation_begin(&mutation); lock_pool(pool);
    if(ok){pool->native=NULL;pool->quiescencePending=false;if(native){pool->retiredStats=stats;pool->retiredStatsValid=true;}}
    pool->quiescing=false; unlock_pool(pool); mutation_end(&mutation);xl6TaskPoolLeaveInternalV1(pool);return ok;
}
XL6_ModulePrepareResult xl6TaskPoolTryQuiesceV1(XL6_TaskPool* pool)
{
    xrtownershipscope mutation={0}; xtaskpool* native;xtaskpoolstats stats={0};
    XL6_ModulePrepareResult result=XL6_MODULE_PREPARE_READY;
    if(!pool)return result;
    xl6TaskPoolEnterInternalV1(pool);
    mutation_begin(&mutation); bool self=activity_current(pool); lock_pool(pool);
    if(pool->retiring||(pool->claim&&!pool->cleared)){
        unlock_pool(pool);mutation_end(&mutation);xl6TaskPoolLeaveInternalV1(pool);return XL6_MODULE_PREPARE_BUSY;
    }
    pool->closing=true;
    if(pool->quiescing || pool->submitting || pool->controls || self) {
        unlock_pool(pool); mutation_end(&mutation);xl6TaskPoolLeaveInternalV1(pool);return XL6_MODULE_PREPARE_BUSY;
    }
    pool->quiescing=true; pool->quiescencePending=true; native=pool->native; unlock_pool(pool); mutation_end(&mutation);
    pool_close_resources(pool);
    if(native) {
        if(!xrtTaskPoolClose(native))result=XL6_MODULE_PREPARE_FAILED;
        else {
            /* Job drain is not thread completion: the native thread wrapper
             * can still run ThreadKey destructors after the worker body.
             * TryJoin also waits for actual resource credits without making
             * this single resource-control thread block in Destroy. */
            xwaitresult state=xrtTaskPoolTryJoinV1(native);
            if(state==XWAIT_TIMEOUT)result=XL6_MODULE_PREPARE_BUSY;
            else if(state!=XWAIT_OK || !xrtTaskPoolGet(native,&stats) || !xrtTaskPoolDestroy(native))result=XL6_MODULE_PREPARE_FAILED;
        }
    }
    mutation_begin(&mutation); lock_pool(pool);
    if(result==XL6_MODULE_PREPARE_READY){pool->native=NULL;pool->quiescencePending=false;if(native){pool->retiredStats=stats;pool->retiredStatsValid=true;}}
    pool->quiescing=false; unlock_pool(pool); mutation_end(&mutation);xl6TaskPoolLeaveInternalV1(pool);return result;
}
XL6_ModulePrepareResult xl6TaskPoolModulePrepareV1(XL6_TaskPool* pool,bool teardown,bool wait)
{
    if(!pool)return XL6_MODULE_PREPARE_READY;
    /* Preflight must not start draining graph-owned work. It does, however,
     * finish a stop that earlier one-shot teardown already requested. Native
     * Count/Trace correctly stays BUSY until its physical join is complete;
     * no snapshot can be used to resume a duty that prevents that snapshot. */
    xl6TaskPoolEnterInternalV1(pool);
    lock_pool(pool);bool pending=pool->quiescencePending,cleared=pool->claim&&pool->cleared;unlock_pool(pool);
    /* Commit already proved every physical preparation. The graph owns
     * native Finish now; normal native control on a cleared node is invalid.
     * Keep this observation separate from initiating/retrying a stop. */
    if(cleared){xl6TaskPoolLeaveInternalV1(pool);return XL6_MODULE_PREPARE_READY;}
    if(teardown&&wait){
        xl6TaskPoolLeaveInternalV1(pool);
        return xl6TaskPoolQuiesceInternalV1(pool)?XL6_MODULE_PREPARE_READY:XL6_MODULE_PREPARE_FAILED;
    }
    XL6_ModulePrepareResult result=teardown||pending?xl6TaskPoolTryQuiesceV1(pool):XL6_MODULE_PREPARE_READY;
    xl6TaskPoolLeaveInternalV1(pool);return result;
}
bool xl6TaskPoolIsQuiescentInternalV1(const XL6_TaskPool* pool)
{
    bool ok; if(!pool)return true;
    xl6TaskPoolEnterInternalV1((XL6_TaskPool*)pool);
    lock_pool(pool);ok=!pool->native&&!pool->submitting&&!pool->controls&&!pool->creating&&!pool->quiescing&&!pool->retiring;
    unlock_pool(pool);xl6TaskPoolLeaveInternalV1((XL6_TaskPool*)pool);return ok;
}
bool xl6TaskPoolIsClosingInternalV1(const XL6_TaskPool* pool)
{
    bool closing; if(!pool)return true;
    xl6TaskPoolEnterInternalV1((XL6_TaskPool*)pool);lock_pool(pool);closing=pool->closing;
    unlock_pool(pool);xl6TaskPoolLeaveInternalV1((XL6_TaskPool*)pool);return closing;
}
static bool pool_release_owner(XL6_TaskPool* pool,bool clearedOnly)
{
    if(!pool){if(clearedOnly)xrtSetErrorKind(XERR_ARGUMENT);return !clearedOnly;}
    xl6TaskPoolEnterInternalV1(pool);xrtownershipscope mutation={0};mutation_begin(&mutation);lock_pool(pool);
    bool graph=pool->claim&&pool->cleared;
    bool ready=pool->entries==1&&!pool->submitting&&!pool->controls&&!pool->creating&&!pool->activities&&
        !pool->quiescing&&!pool->retiring&&(graph ? pool->ownerHeld&&pool->references>=3 :
            !clearedOnly&&!pool->claim&&!pool->cleared&&!pool->native);
    bool owner=ready&&pool->ownerHeld;void* exitData=owner&&!graph?pool->exitData:NULL;
    const xtaskpoolworkerexitownershipv1* policy=pool->exitPolicy;
    /* An ordinary owner released after graph Commit consumes only its own
     * reference. The actual plan pin and outgoing native/exit owners survive
     * until shared Finish, irrespective of parent-before-child ordering. */
    if(owner){pool->ownerHeld=false;pool->closing=true;if(!graph)pool->exitData=NULL;}
    unlock_pool(pool);if(owner)xl6TaskPoolDropInternalV1(pool);mutation_end(&mutation);
    if(exitData)policy->Drop(exitData,NULL);
    if(clearedOnly&&!ready)xrtSetErrorKind(XERR_STATE);
    xl6TaskPoolLeaveInternalV1(pool);return ready;
}
bool xl6TaskPoolDestroyInternalV1(XL6_TaskPool* pool) { return pool_release_owner(pool,false); }
bool xl6TaskPoolReleaseClearedOwnerV1(XL6_TaskPool* pool)
{ return pool_release_owner(pool,true); }
static bool pool_count(const void* data,size_t* count)
{
    const XL6_TaskPool* pool=data;
    if(!pool||!count||!pool->references||pool->entries||pool->activities||pool->submitting||pool->controls||
        pool->creating||pool->quiescing||pool->retiring||pool->cleared||pool->resources)return false;
    *count=pool->references;return true;
}
static bool pool_trace(const void* data,xrtownershipvisitor visit,void* context)
{
    const XL6_TaskPool* pool=data;size_t count;
    return visit&&pool_count(data,&count)&&(!pool->native||visit(xrtTaskPoolOwnership(pool->native),context))&&
        (!pool->exitData||visit((xrtownershipref){pool->exitData,pool->exitPolicy->Ops},context));
}
static const xrtownershipops pool_ops={pool_count,pool_trace};
static bool pool_busy(const void* data)
{
    const XL6_TaskPool* pool=data;
    if(!pool || !pool->references || pool->cleared)return false;
    return pool->entries || pool->activities || pool->submitting || pool->controls || pool->creating || pool->quiescing || pool->retiring;
}
const xrtownershipquiescencev1* xl6TaskPoolQuiescenceV1Get(void)
{
    static const xrtownershipquiescencev1 policy={sizeof(policy),&pool_ops,pool_busy};return &policy;
}
xrtownershipref xl6TaskPoolOwnership(const XL6_TaskPool* pool)
{return (xrtownershipref){pool,pool?&pool_ops:NULL};}
static bool pool_claim(const void* data,const void* token)
{
    XL6_TaskPool* pool=(XL6_TaskPool*)data;
    if(!token||pool->cleared||(pool->claim&&pool->claim!=token))return false;
    pool->claim=token;return true;
}
static void pool_restore(const void* data,const void* token)
{
    XL6_TaskPool* pool=(XL6_TaskPool*)data;
    if(!token||pool->claim!=token||pool->cleared)abort();
    pool->claim=NULL;
}
static bool pool_prepared(const void* data)
{
    const XL6_TaskPool* pool=data;
    /* Only this wrapper's admission/entries settle here. The independently
     * traced native pool MUST be resolved and prepared (including join) by
     * the same whole graph before ANY node may Clear/Finish. */
    return pool->closing&&!pool->entries&&!pool->activities&&!pool->submitting&&!pool->controls&&
        !pool->creating&&!pool->quiescing&&!pool->retiring&&!pool->resources;
}
static xrtownershipprepareresult pool_prepare(const void* data,const void* token)
{
    XL6_TaskPool* pool=(XL6_TaskPool*)data;xrtownershipscope mutation={0};
    mutation_begin(&mutation);lock_pool(pool);
    if(!token||pool->claim!=token||pool->cleared)abort();
    pool->closing=true;bool ready=pool_prepared(pool);
    unlock_pool(pool);mutation_end(&mutation);
    return ready?XRT_OWNERSHIP_PREPARE_READY:XRT_OWNERSHIP_PREPARE_BUSY;
}
static void pool_clear(const void* data,const void* token)
{
    XL6_TaskPool* pool=(XL6_TaskPool*)data;
    if(!token||pool->claim!=token||pool->cleared||!pool_prepared(pool))abort();
    pool->cleared=true;
}
static bool pool_finish(const void* data,const void* token)
{
    XL6_TaskPool* pool=(XL6_TaskPool*)data;xrtownershipscope mutation={0};
    mutation_begin(&mutation);lock_pool(pool);
    if(!token||pool->claim!=token||!pool->cleared)abort();
    if(pool->entries||pool->activities||pool->controls||pool->retiring){unlock_pool(pool);mutation_end(&mutation);return false;}
    xtaskpool* native=pool->native;pool->retiring=true;unlock_pool(pool);mutation_end(&mutation);
    /* All participants are already cleared; consume the actual native owner
     * outside freeze. Its plan pin survives either Finish ordering. */
    bool ready=xrtTaskPoolDestroy(native);
    mutation_begin(&mutation);lock_pool(pool);
    void* exitData=ready?pool->exitData:NULL;const xtaskpoolworkerexitownershipv1* policy=pool->exitPolicy;
    if(ready){pool->native=NULL;pool->exitData=NULL;}
    pool->retiring=false;unlock_pool(pool);mutation_end(&mutation);
    if(exitData)policy->Drop(exitData,NULL);
    return ready;
}
const xrtownershipadapterv1* xl6TaskPoolOwnershipAdapterV2(xrtownershipref ref,
    const xtaskpoolworkerexitownershipv1* const* policies,size_t policyCount,const xrtownershippreparationv1** preparation)
{
    static const xrtownershipadapterv1 adapter={sizeof(adapter),xl6TaskPoolHoldInternalV1,xl6TaskPoolDropInternalV1,
        pool_claim,pool_restore,NULL,pool_clear,pool_finish};
    static const xrtownershippreparationv1 prepare={sizeof(prepare),&adapter,pool_prepared,pool_prepare};
    size_t count;
    if(!preparation||ref.Ops!=&pool_ops||!ref.Data||(policyCount&&!policies))return NULL;
    const XL6_TaskPool* pool=ref.Data;
    if(pool->exitData){
        bool known=false;for(size_t i=0;i<policyCount;++i)if(policies[i]==pool->exitPolicy){known=true;break;}
        if(!known)return NULL;
    }
    if(!pool_count(ref.Data,&count))return NULL;
    *preparation=&prepare;return &adapter;
}
const xrtownershipadapterv1* xl6TaskPoolOwnershipAdapterV1(xrtownershipref ref,
    const xrtownershippreparationv1** preparation)
{return xl6TaskPoolOwnershipAdapterV2(ref,NULL,0,preparation);}
xtaskpool* xl6TaskSubmitBeginInternalV1(XL6_TaskPool* pool)
{
    xrtownershipscope mutation={0};xtaskpool* native;
    mutation_begin(&mutation);lock_pool(pool);
    if(pool->closing||pool->submitting==SIZE_MAX){
        unlock_pool(pool);mutation_end(&mutation);
        error_set(XERR_STATE,"StateError","module task resource is closing");return NULL;
    }
    ++pool->submitting;unlock_pool(pool);mutation_end(&mutation);
    /* Reserve admission before lazy creation. Quiesce waits for every real
     * reservation. Creation/failed-create worker joins and condition waits
     * are outside both ownership mutation and the wrapper resource gate. */
    for(;;){
        mutation_begin(&mutation);lock_pool(pool);
        if(pool->native){native=pool->native;unlock_pool(pool);mutation_end(&mutation);return native;}
        if(!pool->creating){
            pool->creating=true;unlock_pool(pool);mutation_end(&mutation);
            native=pool->exitData?xrtTaskPoolCreateOwnedExitV1(&pool->config,pool->exitData,pool->exitPolicy):
                xrtTaskPoolCreate(&pool->config);
            mutation_begin(&mutation);lock_pool(pool);
            if(!pool->creating||pool->native||!pool->submitting)abort();
            if(native)pool->exitData=NULL; /* successful native ownership transfer */
            /* Cancel can linearize while creation is outside the wrapper gate.
             * Keep the native pointer PRIVATE until the latch is applied, so
             * another reserved caller cannot receive an uncancelled pool.
             * Native cancellation/cleanup runs outside the wrapper gate and
             * mutation; creating and the real submission credit stay live. */
            bool cancelled=true;
            if(native&&pool->cancelling){
                unlock_pool(pool);mutation_end(&mutation);cancelled=xrtTaskPoolCancel(native);
                mutation_begin(&mutation);lock_pool(pool);
                if(!pool->creating||pool->native||!pool->submitting)abort();
            }
            pool->native=native;pool->creating=false;
            if(!native||!cancelled)--pool->submitting;
            if(!xrtCondBroadcast(pool->changed))abort();
            unlock_pool(pool);mutation_end(&mutation);return cancelled?native:NULL;
        }
        unlock_pool(pool);mutation_end(&mutation);
        lock_pool(pool);
        while(pool->creating)if(xrtCondWait(pool->changed,pool->gate)!=XWAIT_OK)abort();
        unlock_pool(pool);
    }
}
void xl6TaskSubmitEndInternalV1(XL6_TaskPool* pool)
{
    xrtownershipscope mutation={0};mutation_begin(&mutation);lock_pool(pool);
    if(!pool->submitting)abort();
    --pool->submitting;
    if(!pool->submitting && !xrtCondBroadcast(pool->changed))abort();
    unlock_pool(pool);mutation_end(&mutation);
}
bool xl6TaskNativeBeginV1(XL6_TaskPool* pool, XL6_TaskNativeScopeV1* scope)
{
    if(!pool || !scope){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    *scope=(XL6_TaskNativeScopeV1){0};xl6TaskPoolEnterInternalV1(pool);xl6TaskActivityEnterInternalV1(scope,pool);
    xtaskpool* native=xl6TaskSubmitBeginInternalV1(pool);
    if(!native){xl6TaskActivityLeaveInternalV1(scope);xl6TaskPoolLeaveInternalV1(pool);return false;}
    scope->native=native;return true;
}
void xl6TaskNativeEndV1(XL6_TaskNativeScopeV1* scope)
{
    if(!scope || !scope->pool || scope->thread!=xrtThreadCurrentId())abort();
    XL6_TaskPool* pool=scope->pool;xl6TaskActivityLeaveInternalV1(scope);xl6TaskSubmitEndInternalV1(pool);xl6TaskPoolLeaveInternalV1(pool);
}
void xl6TaskResourceInitV1(XL6_TaskResourceV1* resource,const XL6_TaskNativeScopeV1* scope,
    void* context,void (*close)(void*),void (*drop)(void*))
{
    if(!resource || !scope || !scope->pool || !scope->native ||
        scope->thread!=xrtThreadCurrentId() || !close || !drop)abort();
    XL6_TaskPool* pool=scope->pool;
    *resource=(XL6_TaskResourceV1){0};resource->pool=pool;resource->context=context;
    resource->Close=close;resource->Drop=drop;if(!xl6TaskPoolHoldInternalV1(pool))abort();
    xrtownershipscope mutation={0};mutation_begin(&mutation);lock_pool(pool);
    resource->next=pool->resources;if(resource->next)resource->next->previous=resource;
    pool->resources=resource;resource->registered=true;unlock_pool(pool);mutation_end(&mutation);
}
bool xl6TaskResourceRemoveV1(XL6_TaskResourceV1* resource)
{
    XL6_TaskPool* pool=resource->pool;xrtownershipscope mutation={0};
    mutation_begin(&mutation);lock_pool(pool);bool removed=resource->registered;
    if(removed){
        if(resource->previous)resource->previous->next=resource->next;else pool->resources=resource->next;
        if(resource->next)resource->next->previous=resource->previous;
        resource->previous=resource->next=NULL;resource->registered=false;
    }
    unlock_pool(pool);mutation_end(&mutation);return removed;
}
void xl6TaskResourceReleaseV1(XL6_TaskResourceV1* resource)
{ if(!resource || !resource->pool || resource->registered)abort();xl6TaskPoolDropInternalV1(resource->pool);*resource=(XL6_TaskResourceV1){0}; }
static void pool_close_resources(XL6_TaskPool* pool)
{
    for(;;){
        xrtownershipscope mutation={0};mutation_begin(&mutation);lock_pool(pool);
        XL6_TaskResourceV1* resource=pool->resources;
        if(resource){pool->resources=resource->next;if(resource->next)resource->next->previous=NULL;
            resource->previous=resource->next=NULL;resource->registered=false;}
        unlock_pool(pool);mutation_end(&mutation);
        if(!resource)return;
        resource->Close(resource->context);resource->Drop(resource->context);
    }
}
/* Control scopes retain the wrapper AND exclude native retirement. They are
 * not submission rights and do not create a pool on read/close/wait. */
static bool control_begin(XL6_TaskPool* pool,TaskActivity* activity,xtaskpool** native)
{
    if(!pool){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    xl6TaskPoolEnterInternalV1(pool);xrtownershipscope mutation={0};mutation_begin(&mutation);lock_pool(pool);
    bool ok=pool->ownerHeld&&!pool->cleared&&!pool->claim&&!pool->retiring&&!pool->quiescing&&pool->controls<SIZE_MAX;
    if(ok){++pool->controls;*native=pool->native;}
    unlock_pool(pool);mutation_end(&mutation);
    if(!ok){error_set(XERR_STATE,"StateError","task pool control is unavailable during retirement");xl6TaskPoolLeaveInternalV1(pool);return false;}
    xl6TaskActivityEnterInternalV1(activity,pool);return true;
}
static void control_end(XL6_TaskPool* pool,TaskActivity* activity)
{
    xl6TaskActivityLeaveInternalV1(activity);xrtownershipscope mutation={0};mutation_begin(&mutation);lock_pool(pool);
    if(!pool->controls)abort();
    --pool->controls;if(!pool->controls&&!xrtCondBroadcast(pool->changed))abort();
    unlock_pool(pool);mutation_end(&mutation);xl6TaskPoolLeaveInternalV1(pool);
}
static bool control_start(XL6_TaskPool* pool)
{
    XL6_TaskNativeScopeV1 scope={0};if(!xl6TaskNativeBeginV1(pool,&scope))return false;
    xl6TaskNativeEndV1(&scope);return true;
}
static bool control_close(XL6_TaskPool* pool)
{
    TaskActivity activity={0};xtaskpool* native=NULL;
    if(!control_begin(pool,&activity,&native))return false;
    xrtownershipscope mutation={0};mutation_begin(&mutation);lock_pool(pool);
    pool->closing=true;unlock_pool(pool);mutation_end(&mutation);control_end(pool,&activity);return true;
}
static bool control_cancel(XL6_TaskPool* pool)
{
    TaskActivity activity={0};xtaskpool* native=NULL;
    if(!control_begin(pool,&activity,&native))return false;
    xrtownershipscope mutation={0};mutation_begin(&mutation);lock_pool(pool);
    pool->closing=true;pool->cancelling=true;native=pool->native;
    unlock_pool(pool);mutation_end(&mutation);
    bool ok=!native||xrtTaskPoolCancel(native);control_end(pool,&activity);return ok;
}
static void control_wake(void* data)
{
    XL6_TaskPool* pool=data;lock_pool(pool);
    if(!xrtCondBroadcast(pool->changed))abort();
    unlock_pool(pool);
}
static xwaitresult control_join(XL6_TaskPool* pool,xtaskpool* native,xdeadline deadline,xcancel* cancel)
{
    /* Work and accepted capture tails are already drained. Close registered
     * asynchronous resources BEFORE requesting native stop, so a TLS value
     * owning such a resource cannot deadlock its worker's exit. Close is the
     * existing allocation-free registry protocol, not arbitrary resource loss. */
    pool_close_resources(pool);
    for(;;){
        xwaitresult state=xrtTaskPoolTryJoinV1(native);
        if(state!=XWAIT_TIMEOUT){
            if(state==XWAIT_OK){
                xrtownershipscope mutation={0};mutation_begin(&mutation);lock_pool(pool);
                pool->exitComplete=true;unlock_pool(pool);mutation_end(&mutation);
            }
            return state;
        }
        lock_pool(pool);bool complete=pool->exitComplete;unlock_pool(pool);
        if(complete)return XWAIT_OK; /* another waiter published physical completion */
        if(cancel&&xrtCancelRequested(cancel))return XWAIT_CANCELLED;
        if(xrtDeadlineExpired(deadline))return XWAIT_TIMEOUT;
        /* Explicit synchronous wait only. The native zero-time join remains
         * nonblocking for the resource source. A bounded cond heartbeat avoids
         * spinning on TLS/native wrapper tails without adding a new timer,
         * thread or callback ownership edge. Completion wins the next probe. */
        xdeadline heartbeat=xrtDeadlineAfter(1000);if(deadline<heartbeat)heartbeat=deadline;
        lock_pool(pool);state=xrtCondWaitUntil(pool->changed,pool->gate,heartbeat);unlock_pool(pool);
        if(state==XWAIT_ERROR)return state;
    }
}
static bool control_wait(XL6_TaskPool* pool,xdeadline deadline,xcancel* cancel,XL6_TaskPoolWaitStatusV1* status)
{
    if(!status){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    *status=XL6_TASK_POOL_TIMEOUT_V1;
    if(!pool){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    /* A synchronous cancelled-submit destructor is also a same-pool activity,
     * not just a worker. Never wait on its own unreturned submission credit. */
    if(activity_current(pool)){error_set(XERR_STATE,"StateError","task pool activity cannot wait on itself");return false;}
    TaskActivity activity={0};xtaskpool* native=NULL;
    if(!control_begin(pool,&activity,&native))return false;
    lock_pool(pool);bool closed=pool->closing,submitting=pool->submitting!=0,complete=pool->exitComplete;unlock_pool(pool);
    if(!closed){error_set(XERR_STATE,"StateError","close the task pool before waiting");control_end(pool,&activity);return false;}
    /* Only a successful full native join publishes this fact. Closed wrapper
     * admission cannot restart it. A later exclusive TryJoin may be BUSY due
     * to unrelated ownership mutation/control; that is not an undone exit.
     * This proves wait completion only, never permission to retire or trace. */
    if(complete){*status=XL6_TASK_POOL_READY_V1;control_end(pool,&activity);return true;}
    /* Trusted native scope users can submit callbacks without language-frame
     * activity metadata. The native zero-time probe rejects their own worker
     * before we could wait for that worker's still-live submitting scope. */
    if(native&&(!xrtTaskPoolClose(native)||xrtTaskPoolWaitFor(native,0)==XWAIT_ERROR)){
        control_end(pool,&activity);return false;
    }
    lock_pool(pool);submitting=pool->submitting!=0;
    bool cancelled=submitting&&cancel&&xrtCancelRequested(cancel);
    bool expired=submitting&&xrtDeadlineExpired(deadline);unlock_pool(pool);
    if(cancelled||expired){
        if(cancelled)*status=XL6_TASK_POOL_CANCELLED_V1;
        control_end(pool,&activity);return true;
    }
    xcancelwatch* watch=submitting&&cancel ? xrtCancelWatch(cancel,control_wake,pool) : NULL;
    if(submitting&&cancel&&!watch){control_end(pool,&activity);return false;}
    xwaitresult result=XWAIT_OK;
    lock_pool(pool);
    while(pool->submitting){
        if(cancel&&xrtCancelRequested(cancel)){result=XWAIT_CANCELLED;break;}
        if(xrtDeadlineExpired(deadline)){result=XWAIT_TIMEOUT;break;}
        result=xrtCondWaitUntil(pool->changed,pool->gate,deadline);
        if(result!=XWAIT_OK)break;
    }
    if(result==XWAIT_TIMEOUT&&!pool->submitting)result=XWAIT_OK;
    native=pool->native;bool ownedExit=pool->exitPolicy!=NULL;unlock_pool(pool);xrtCancelUnwatch(watch);
    if(result==XWAIT_OK){
        if(native){
            if(!xrtTaskPoolClose(native))result=XWAIT_ERROR;
            else result=xrtTaskPoolWaitUntilCancel(native,deadline,cancel);
            if(result==XWAIT_OK&&ownedExit)result=control_join(pool,native,deadline,cancel);
        }
    }
    control_end(pool,&activity);
    if(result==XWAIT_OK)*status=XL6_TASK_POOL_READY_V1;
    else if(result==XWAIT_CANCELLED)*status=XL6_TASK_POOL_CANCELLED_V1;
    else if(result!=XWAIT_TIMEOUT)return false;
    return true;
}
static bool control_wait_for(XL6_TaskPool* pool,int64_t milliseconds,xcancel* cancel,XL6_TaskPoolWaitStatusV1* status)
{
    if(status)*status=XL6_TASK_POOL_TIMEOUT_V1;
    xdeadline deadline;if(!xl6ConcurrentDeadline(milliseconds,&deadline))return false;
    return control_wait(pool,deadline,cancel,status);
}
static bool control_wait_until(XL6_TaskPool* pool,uint64_t deadline,xcancel* cancel,XL6_TaskPoolWaitStatusV1* status)
{
    if(status)*status=XL6_TASK_POOL_TIMEOUT_V1;
    if(deadline==XRT_DEADLINE_NEVER){xrtSetErrorKind(XERR_RANGE);return false;}
    return control_wait(pool,deadline,cancel,status);
}
static bool control_snapshot(XL6_TaskPool* pool,XL6_TaskPoolStatsV1* output)
{
    if(!output||output->size!=sizeof(*output)){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    *output=(XL6_TaskPoolStatsV1){.size=sizeof(*output)};
    TaskActivity activity={0};xtaskpool* native=NULL;
    if(!control_begin(pool,&activity,&native))return false;
    lock_pool(pool);bool initialized=native||pool->retiredStatsValid,closed=pool->closing,cancelling=pool->cancelling;
    xtaskpoolstats stats=pool->retiredStats;unlock_pool(pool);
    bool ok=!native||xrtTaskPoolGet(native,&stats);control_end(pool,&activity);
    if(!ok)return false;
    *output=(XL6_TaskPoolStatsV1){sizeof(*output),stats.Threads,stats.QueueLimit,stats.Queued,stats.Running,
        stats.Submitted,stats.Completed,stats.Succeeded,stats.Failed,stats.Cancelled,stats.Rejected,
        initialized,closed||stats.Closed,cancelling||stats.Cancelling};return true;
}
const XL6_TaskPoolControlV1* xl6TaskPoolControlV1Get(void)
{
    static const XL6_TaskPoolControlV1 api={sizeof(api),xl6TaskPoolCreateInternalV1,control_start,control_close,control_cancel,
        control_wait_for,control_wait_until,control_snapshot,xl6TaskPoolQuiesceInternalV1,xl6TaskPoolTryQuiesceV1,xl6TaskPoolDestroyInternalV1};return &api;
}
