#include "xl6_execution_resource_internal.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

typedef XL6_ExecutionSourceV1 Source;
typedef XL6_ExecutionResourceV1 Resource;
/* One short resident metadata lock orders source links and receiver pins.
 * No native allocation, event call, callback, wait or join occurs inside it.
 * In particular, a ticket/source pointer is retained before releasing this
 * lock, not read lock-free and retained after a possible unregistration. */
static xatomic64 metadataGate={0};
struct XL6_ExecutionSourceV1 {
    xevent* changed;
    xthread* thread;
    Resource* resources;
    size_t references,entries,count;
    uint64_t workerId,cycle;
    xerror* error;
    const void* claim;
    bool starting,initialized,closed,active,parked,exited,joining,joined,graphStop,stop,cleared,retired,creator,failed;
};
static const xrtownershipops sourceOps,resourceOps;
static void begin(xrtownershipscope* scope)
{
    if(!xrtOwnershipMutationBegin(scope))abort();
    for(;;){uint64 available=0;
        if(xrtAtomic64CompareExchange(&metadataGate,&available,1,XMEMORY_ACQUIRE,XMEMORY_RELAXED))return;
        xrtThreadYield();}
}
static void end(xrtownershipscope* scope)
{xrtAtomic64Store(&metadataGate,0,XMEMORY_RELEASE);if(!xrtOwnershipScopeEnd(scope))abort();}
static bool sourceEnterLocked(Source* source,bool terminal)
{
    bool ok=source&&source->references&&source->references<SIZE_MAX&&source->entries<SIZE_MAX&&
        source->creator&&!source->joining&&(!source->cleared||terminal)&&(!source->retired||terminal);
    if(ok){++source->references;++source->entries;}return ok;
}
static void sourceDrop(Source* source)
{
    xrtownershipscope mutation={0};begin(&mutation);if(!source->references)abort();
    bool last=--source->references==0;
    if(last&&(!source->retired||source->creator||source->resources||source->thread||source->changed||source->entries||source->active||
        (source->claim&&!source->cleared)))abort();
    xerror* error=last?source->error:NULL;if(last)source->error=NULL;end(&mutation);
    if(last){xrtErrorFree(error);xrtFree(source);}
}
static void sourceLeave(Source* source)
{xrtownershipscope mutation={0};begin(&mutation);if(!source->entries)abort();--source->entries;end(&mutation);sourceDrop(source);}
static void resourceDrop(Resource* resource)
{
    xrtownershipscope mutation={0};begin(&mutation);if(!resource->references)abort();bool last=--resource->references==0;
    if(last&&(resource->owners||resource->entries||resource->source||resource->family->Present(resource)||resource->retiring||resource->control||
        (resource->claim&&!resource->cleared)))abort();
    end(&mutation);
    if(last){resource->family->Drop(resource);xrtFree(resource);}
}
static Source* wakeHoldLocked(Resource* resource)
{
    Source* source=resource->source;
    if(source&&(source->cleared||source->retired||source->graphStop))return NULL;
    if(source&&!sourceEnterLocked(source,false))abort();
    return source;
}
static void wakeDrop(Source* source)
{if(source){if(!xrtEventSet(source->changed))abort();sourceLeave(source);}}
static void sourceFailureLocked(Source* source,xerror** error)
{if(!source->failed){source->failed=true;source->error=*error;*error=NULL;}}
static bool waitChanged(Source* source,bool finite)
{
    xwaitresult waited=finite?xrtEventWaitFor(source->changed,UINT64_C(10000)):xrtEventWait(source->changed);
    if(waited!=XWAIT_ERROR){xrtClearError();return true;}
    xerror* error=xrtTakeError();xrtownershipscope mutation={0};begin(&mutation);sourceFailureLocked(source,&error);
    source->exited=true;source->workerId=0;end(&mutation);xrtErrorFree(error);return false;
}
static bool nativePresent(const Resource* resource)
{return resource->family->Present(resource);}
/* Transfer registry and source-back-reference slots to the caller's locals.
 * Both are still REAL owners until the caller drops them outside the gate. */
static Source* unregisterLocked(Resource* resource)
{
    Source* source=resource->source;if(!source||!source->count||source->references<2)abort();
    if(resource->previous)resource->previous->next=resource->next;else source->resources=resource->next;
    if(resource->next)resource->next->previous=resource->previous;
    resource->previous=resource->next=NULL;resource->source=NULL;--source->count;return source;
}
static int32_t worker(void* raw)
{
    Source* source=raw;bool busy=false;uint64_t cycle=0;
    for(;;){
        xrtownershipscope mutation={0};begin(&mutation);source->workerId=xrtThreadCurrentId();
        if(!source->starting&&(source->stop||(!source->graphStop&&source->closed&&!source->resources))){
            source->exited=true;source->workerId=0;end(&mutation);return 0;
        }
        if(source->graphStop){
            /* Prepare pauses the control worker; it does not terminate it.
             * Restore can therefore resume ordinary retirement without a
             * forbidden allocation/thread creation under graph Freeze. Keep
             * parked persistent even across a pending mechanical wake. */
            source->parked=true;end(&mutation);
            if(!waitChanged(source,false))return 1;
            cycle=0;continue;
        }
        source->parked=false;
        if(!cycle){
            if(source->cycle==UINT64_MAX){for(Resource* r=source->resources;r;r=r->next)r->cycle=0;source->cycle=0;}
            cycle=++source->cycle;busy=false;
        }
        Resource* selected=NULL;
        if(!source->starting&&!source->claim){
            for(Resource* r=source->resources;r;r=r->next){
                if(r->requested&&!r->owners&&!r->stalled&&!r->claim&&r->cycle!=cycle){
                    r->cycle=cycle;
                    if(r->entries||r->retiring){busy=true;continue;}
                    selected=r;r->retiring=true;source->active=true;break;
                }
            }
        }
        if(selected){
            end(&mutation);XL6_ModulePrepareResult result=XL6_MODULE_PREPARE_FAILED;
            xl6_error_scope cleanup={0};xl6_error_result outcome={false,NULL};
            bool begun=xl6ErrorScopeBegin(&cleanup);
            if(begun){
                result=selected->family->Retire(selected);
                if(result==XL6_MODULE_PREPARE_FAILED)xl6ErrorScopeCaptureFailure(&cleanup);
            }else outcome.error=xrtTakeError();
            begin(&mutation);Source* detached=NULL;
            if(result==XL6_MODULE_PREPARE_READY){
                selected->executor=NULL;selected->pool=NULL;selected->engine=NULL;selected->resolver=NULL;detached=unregisterLocked(selected);
            }else if(result==XL6_MODULE_PREPARE_FAILED)selected->stalled=true;
            else busy=true;
            selected->retiring=false;end(&mutation);
            /* Registry removal is not the end of cleanup. The actual last
             * resource/ledger/error-context release can still run language
             * destructors; keep the checked scope and active tail through it.
             * A separate inspection pin may defer its own final drop. */
            if(detached){sourceDrop(detached);resourceDrop(selected);}
            if(begun)outcome=xl6ErrorScopeEnd(&cleanup);
            xerror* error=outcome.error;begin(&mutation);
            if(!outcome.ok)sourceFailureLocked(source,&error);
            source->active=false;end(&mutation);xrtErrorFree(error);continue;
        }
        source->parked=true;end(&mutation);cycle=0;
        if(!waitChanged(source,busy))return 1;
    }
}
Source* xl6ExecutionSourceCreateV1(void)
{
    Source* source=xrtCalloc(1,sizeof(*source));if(!source)return NULL;
    source->references=1;source->creator=true;source->starting=true;source->changed=xrtEventCreate(false,false);
    if(source->changed)source->thread=xrtThreadCreate(worker,source,0);
    if(!source->thread){xerror* error=xrtTakeError();if(source->changed&&!xrtEventDestroy(source->changed))abort();xrtFree(source);xrtClearError();xrtSetErrorTake(error);return NULL;}
    xrtownershipscope mutation={0};begin(&mutation);source->initialized=true;source->starting=false;end(&mutation);
    if(!xrtEventSet(source->changed))abort();
    return source;
}
bool xl6ExecutionSourceCloseV1(Source* source)
{
    xrtownershipscope mutation={0};begin(&mutation);bool ok=sourceEnterLocked(source,true);
    if(ok)source->closed=true;
    bool wake=ok&&!source->cleared&&!source->retired&&!source->graphStop&&!source->exited;
    end(&mutation);
    if(!ok){xrtSetErrorKind(XERR_STATE);return false;}
    if(wake)wakeDrop(source);else sourceLeave(source);
    return true;
}
bool xl6ExecutionSourceCheckV1(Source* source)
{
    xrtownershipscope mutation={0};begin(&mutation);bool entered=sourceEnterLocked(source,false),failed=entered&&source->failed;
    const xerror* original=failed?source->error:NULL;end(&mutation);
    if(!entered){xrtSetErrorKind(XERR_STATE);return false;}
    xerror* error=failed?xrtErrorRef(original):NULL;sourceLeave(source);if(failed)xrtSetErrorTake(error);return !failed;
}
bool xl6ExecutionSourceRetryV1(Source* source)
{
    xrtownershipscope mutation={0};begin(&mutation);bool entered=sourceEnterLocked(source,false);
    if(entered)for(Resource* r=source->resources;r;r=r->next)if(!r->claim)r->stalled=false;
    end(&mutation);if(!entered){xrtSetErrorKind(XERR_STATE);return false;}
    wakeDrop(source);return true;
}
bool xl6ExecutionSourceSnapshotV1Get(Source* source,XL6_ExecutionSourceSnapshotV1* output)
{
    if(!output||output->size!=sizeof(*output)){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    xrtownershipscope mutation={0};begin(&mutation);bool ok=sourceEnterLocked(source,false);
    if(ok)*output=(XL6_ExecutionSourceSnapshotV1){sizeof(*output),source->count,source->entries-1,source->closed,source->active,source->exited,source->failed};
    end(&mutation);if(ok)sourceLeave(source);else xrtSetErrorKind(XERR_STATE);return ok;
}
Resource* xl6ExecutionCreateInternalV1(Source* source,const XL6_ExecutionFamilyV1* family,const XL6_ExecutionCreateOptionsV1* options)
{
    if(!family||family->size!=sizeof(*family)||family->abiTag!=XL6_EXECUTION_FAMILY_TAG_V1||
        !family->Create||!family->Rollback||!family->Retire||!family->Finish||!family->Drop||!family->Trace||!family->Close||!family->Cancel||!family->Present||
        family->instanceBytes<sizeof(Resource)||!options||options->kind!=family->kind||
        (family->kind<XL6_EXECUTION_EXECUTOR_V1||family->kind>XL6_EXECUTION_UDP_SOCKET_V1)){
        xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    xrtownershipscope mutation={0};begin(&mutation);bool entered=sourceEnterLocked(source,false);
    bool admitted=entered&&!source->closed&&!source->claim;end(&mutation);
    /* Separate entry acquisition from admission: a close race must still
     * balance the acquired reference even when creation is refused. */
    if(!admitted){
        if(entered)sourceLeave(source);
        xrtSetErrorKind(XERR_STATE);return NULL;
    }
    Resource* resource=xrtCalloc(1,family->instanceBytes);bool created=false;
    if(resource){
        resource->references=1;resource->owners=1;resource->family=family;resource->kind=family->kind;
        created=family->Create(resource,options);
    }
    bool ready=created&&nativePresent(resource);begin(&mutation);
    bool published=ready&&!source->closed&&!source->claim&&source->count<SIZE_MAX&&source->references<SIZE_MAX;
    if(published){
        resource->source=source;resource->next=source->resources;if(resource->next)resource->next->previous=resource;
        source->resources=resource;++source->count;++source->references;++resource->references;
    }
    end(&mutation);
    if(!published){
        /* A rejected publication is our StateError; an unsuccessful native
         * factory's failure carrier is authoritative even without a diagnostic.
         * Never manufacture a replacement MemoryError for that NULL outcome. */
        if(ready)xrtSetErrorKind(XERR_STATE);
        xerror* error=xrtTakeError();
        if(resource){
            if(!family->Rollback(resource))abort();
            family->Drop(resource);xrtFree(resource);
        }
        sourceLeave(source);xrtClearError();xrtSetErrorTake(error);return NULL;
    }
    sourceLeave(source);return resource;
}
Resource* xl6ExecutionResourceRetainV1(Resource* resource)
{
    xrtownershipscope mutation={0};begin(&mutation);bool ok=resource&&resource->references&&resource->references<SIZE_MAX&&
        resource->owners&&resource->owners<SIZE_MAX&&!resource->cleared;
    if(ok){++resource->references;++resource->owners;}end(&mutation);if(!ok)xrtSetErrorKind(XERR_STATE);return ok?resource:NULL;
}
void xl6ExecutionResourceReleaseV1(Resource* resource)
{
    if(!resource)return;
    xrtownershipscope mutation={0};begin(&mutation);if(!resource->owners)abort();
    --resource->owners;Source* source=NULL;
    if(!resource->owners){resource->requested=true;source=wakeHoldLocked(resource);}end(&mutation);
    wakeDrop(source);resourceDrop(resource);
}
static bool request(Resource* resource,bool retry)
{
    XL6_ExecutionScopeV1 scope={0};if(!xl6ExecutionResourceEnterV1(resource,false,&scope))return false;
    xrtownershipscope mutation={0};begin(&mutation);resource->requested=true;if(retry)resource->stalled=false;end(&mutation);
    bool ok=resource->family->Close(resource);xl6ExecutionResourceLeaveV1(&scope);return ok;
}
bool xl6ExecutionResourceCloseV1(Resource* r){return request(r,false);}
bool xl6ExecutionResourceRetryV1(Resource* r){return request(r,true);}
bool xl6ExecutionResourceSnapshotV1Get(Resource* resource,XL6_ExecutionResourceSnapshotV1* output)
{
    if(!output||output->size!=sizeof(*output)){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    xrtownershipscope mutation={0};begin(&mutation);bool ok=resource&&resource->references&&!resource->cleared;
    if(ok)*output=(XL6_ExecutionResourceSnapshotV1){sizeof(*output),resource->owners,resource->entries,resource->kind,resource->requested,nativePresent(resource),resource->stalled};
    end(&mutation);if(!ok)xrtSetErrorKind(XERR_STATE);return ok;
}
static bool resourceEnter(Resource* resource,bool submit,bool control,XL6_ExecutionScopeV1* scope)
{
    if(!scope||scope->resource){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    xrtownershipscope mutation={0};begin(&mutation);bool ok=resource&&resource->owners&&resource->references<SIZE_MAX&&
        resource->entries<SIZE_MAX&&!resource->cleared&&!resource->claim&&!resource->retiring&&!resource->control&&
        (!control||!resource->entries)&&(!submit||(!resource->requested&&nativePresent(resource)));
    if(ok){
        ++resource->references;++resource->entries;resource->control=control;
        *scope=(XL6_ExecutionScopeV1){.resource=resource,.executor=resource->executor,.pool=resource->pool,
            .thread=xrtThreadCurrentId(),.engine=resource->engine,.resolver=resource->resolver,.control=control};
    }
    end(&mutation);if(!ok)xrtSetErrorKind(XERR_STATE);return ok;
}
bool xl6ExecutionResourceEnterV1(Resource* r,bool submit,XL6_ExecutionScopeV1* s){return resourceEnter(r,submit,false,s);}
bool xl6ExecutionResourceControlV1(Resource* r,bool submit,XL6_ExecutionScopeV1* s){return resourceEnter(r,submit,true,s);}
void xl6ExecutionResolverConsumedInternalV1(XL6_ExecutionScopeV1* scope)
{
    if(!scope||!scope->resource||!scope->control||scope->thread!=xrtThreadCurrentId())abort();
    Resource* resource=scope->resource;xrtownershipscope mutation={0};begin(&mutation);
    if(resource->kind!=XL6_EXECUTION_NET_RESOLVER_V1||!resource->control||resource->entries!=1||
        resource->resolver!=scope->resolver)abort();
    resource->resolver=NULL;scope->resolver=NULL;end(&mutation);
}
void xl6ExecutionResourceLeaveV1(XL6_ExecutionScopeV1* scope)
{
    if(!scope||!scope->resource||scope->thread!=xrtThreadCurrentId())abort();
    Resource* resource=scope->resource;xrtownershipscope mutation={0};begin(&mutation);if(!resource->entries)abort();
    if(scope->control){if(!resource->control||resource->entries!=1)abort();resource->control=false;}
    --resource->entries;Source* source=resource->requested?wakeHoldLocked(resource):NULL;*scope=(XL6_ExecutionScopeV1){0};end(&mutation);
    wakeDrop(source);resourceDrop(resource);
}
bool xl6ExecutionResourceCancelV1(Resource* resource)
{
    XL6_ExecutionScopeV1 scope={0};if(!xl6ExecutionResourceEnterV1(resource,false,&scope))return false;
    xrtownershipscope mutation={0};begin(&mutation);resource->requested=true;end(&mutation);
    bool ok=resource->family->Cancel(resource);
    xl6ExecutionResourceLeaveV1(&scope);return ok;
}

static bool sourceCount(const void* raw,size_t* output)
{
    const Source* s=raw;if(!s||!output||!s->initialized||!s->references||s->cleared||s->entries||s->active||s->joining||
        (!s->parked&&!s->exited))return false;
    *output=s->references;return true;
}
static bool sourceTrace(const void* raw,xrtownershipvisitor visit,void* context)
{
    const Source* s=raw;size_t refs;if(!visit||!sourceCount(raw,&refs))return false;
    if(s->error&&!visit(xrtErrorOwnership(s->error),context))return false;
    for(const Resource* r=s->resources;r;r=r->next)if(!visit(xl6ExecutionResourceOwnershipV1(r),context))return false;
    return true;
}
static const xrtownershipops sourceOps={sourceCount,sourceTrace};
static bool resourceCount(const void* raw,size_t* output)
{const Resource* r=raw;if(!r||!output||!r->references||r->cleared||r->entries||r->retiring)return false;*output=r->references;return true;}
static bool resourceTrace(const void* raw,xrtownershipvisitor visit,void* context)
{
    const Resource* r=raw;size_t refs;if(!visit||!resourceCount(raw,&refs))return false;
    if(r->source&&!visit(xl6ExecutionSourceOwnershipV1(r->source),context))return false;
    return r->family->Trace(r,visit,context);
}
static const xrtownershipops resourceOps={resourceCount,resourceTrace};
xrtownershipref xl6ExecutionSourceOwnershipV1(const Source* s){return (xrtownershipref){s,s?&sourceOps:NULL};}
xrtownershipref xl6ExecutionResourceOwnershipV1(const Resource* r){return (xrtownershipref){r,r?&resourceOps:NULL};}
static bool sourceBusy(const void* raw)
{const Source* s=raw;return s&&s->references&&!s->cleared&&(s->entries||s->active||s->joining||(!s->parked&&!s->exited));}
static bool resourceBusy(const void* raw)
{const Resource* r=raw;return r&&r->references&&!r->cleared&&(r->entries||r->retiring);}
const xrtownershipquiescencev1* xl6ExecutionSourceQuiescenceV1Get(void)
{static const xrtownershipquiescencev1 policy={sizeof(policy),&sourceOps,sourceBusy};return &policy;}
const xrtownershipquiescencev1* xl6ExecutionResourceQuiescenceV1Get(void)
{static const xrtownershipquiescencev1 policy={sizeof(policy),&resourceOps,resourceBusy};return &policy;}
static bool sourceHold(const void* raw)
{Source* s=(Source*)raw;xrtownershipscope mutation={0};begin(&mutation);bool ok=s->references&&s->references<SIZE_MAX&&!s->cleared;if(ok)++s->references;end(&mutation);return ok;}
static bool resourceHold(const void* raw)
{Resource* r=(Resource*)raw;xrtownershipscope mutation={0};begin(&mutation);bool ok=r->references&&r->references<SIZE_MAX&&!r->cleared;if(ok)++r->references;end(&mutation);return ok;}
static void sourceUnpin(const void* raw){sourceDrop((Source*)raw);}
static void resourceUnpin(const void* raw){resourceDrop((Resource*)raw);}
static bool sourceClaim(const void* raw,const void* token)
{Source* s=(Source*)raw;if(!token||s->cleared||(s->claim&&s->claim!=token))return false;s->claim=token;return true;}
static bool resourceClaim(const void* raw,const void* token)
{Resource* r=(Resource*)raw;if(!token||r->cleared||(r->claim&&r->claim!=token))return false;r->claim=token;return true;}
static void sourceRestore(const void* raw,const void* token)
{Source* s=(Source*)raw;if(!token||s->claim!=token||s->cleared)abort();s->claim=NULL;s->graphStop=false;}
static void resourceRestore(const void* raw,const void* token)
{Resource* r=(Resource*)raw;if(!token||r->claim!=token||r->cleared)abort();r->claim=NULL;}
static bool sourceReady(const void* raw)
{const Source* s=raw;return s->closed&&(s->joined||s->exited||(s->graphStop&&s->parked))&&!s->joining&&!s->entries&&!s->active;}
static bool resourceReady(const void* raw)
{const Resource* r=raw;return r->requested&&!r->entries&&!r->retiring;}
static xrtownershipprepareresult sourcePrepare(const void* raw,const void* token)
{
    Source* s=(Source*)raw;xrtownershipscope mutation={0};begin(&mutation);
    if(!token||s->claim!=token||s->cleared)abort();
    s->closed=true;s->graphStop=true;bool ready=sourceReady(s);end(&mutation);
    if(ready)return XRT_OWNERSHIP_PREPARE_READY;
    if(!xrtEventSet(s->changed))return XRT_OWNERSHIP_PREPARE_FAILED;
    return XRT_OWNERSHIP_PREPARE_BUSY;
}
static xrtownershipprepareresult resourcePrepare(const void* raw,const void* token)
{
    Resource* r=(Resource*)raw;xrtownershipscope mutation={0};begin(&mutation);
    if(!token||r->claim!=token||r->cleared)abort();
    r->requested=true;bool ready=resourceReady(r);end(&mutation);
    /* Native children have independent graph preparations. Do not destroy a
     * claimed child or force cancellation to manufacture local readiness. */
    return ready?XRT_OWNERSHIP_PREPARE_READY:XRT_OWNERSHIP_PREPARE_BUSY;
}
static void sourceClear(const void* raw,const void* token)
{Source* s=(Source*)raw;if(!token||s->claim!=token||s->cleared||!sourceReady(s))abort();s->cleared=true;}
static void resourceClear(const void* raw,const void* token)
{Resource* r=(Resource*)raw;if(!token||r->claim!=token||r->cleared||!resourceReady(r))abort();r->cleared=true;}
static bool sourceFinish(const void* raw,const void* token)
{
    Source* s=(Source*)raw;xrtownershipscope mutation={0};begin(&mutation);
    if(!token||s->claim!=token||!s->cleared||!sourceReady(s))abort();
    if(s->retired){end(&mutation);return true;}
    xthread* thread=s->thread;xevent* event=s->changed;bool alreadyJoined=s->joined;s->joining=true;s->stop=true;end(&mutation);
    if(!alreadyJoined&&!xrtEventSet(event)){
        xerror* error=xrtTakeError();xerror* diagnostic=xrtErrorRef(error);
        begin(&mutation);s->joining=false;sourceFailureLocked(s,&error);end(&mutation);xrtErrorFree(error);xrtSetErrorTake(diagnostic);return false;
    }
    bool joined=alreadyJoined||xrtThreadWait(thread)==XWAIT_OK;
    if(joined&&!alreadyJoined)xrtThreadDestroy(thread);
    bool ok=joined&&xrtEventDestroy(event);begin(&mutation);
    if(joined){s->thread=NULL;s->joined=true;}
    if(ok){s->changed=NULL;s->retired=true;}s->joining=false;end(&mutation);return ok;
}
static bool resourceFinish(const void* raw,const void* token)
{
    Resource* r=(Resource*)raw;xrtownershipscope mutation={0};begin(&mutation);
    if(!token||r->claim!=token||!r->cleared||!resourceReady(r))abort();
    r->retiring=true;end(&mutation);
    /* Children are ALL cleared before any Finish. Native Destroy consumes the
     * actual creator; explicit native/job pins preserve either Finish order. */
    bool ok=r->family->Finish(r);
    begin(&mutation);Source* source=NULL;
    if(ok){r->executor=NULL;r->pool=NULL;r->engine=NULL;r->resolver=NULL;if(r->source)source=unregisterLocked(r);}r->retiring=false;end(&mutation);
    if(source){sourceDrop(source);resourceDrop(r);}return ok;
}
const xrtownershipadapterv1* xl6ExecutionOwnershipAdapterV1(xrtownershipref ref,
    const XL6_ExecutionFamilyV1* const* families,size_t count,const xrtownershippreparationv1** preparation)
{
    static const xrtownershipadapterv1 sources={sizeof(sources),sourceHold,sourceUnpin,sourceClaim,sourceRestore,NULL,sourceClear,sourceFinish};
    static const xrtownershipadapterv1 resources={sizeof(resources),resourceHold,resourceUnpin,resourceClaim,resourceRestore,NULL,resourceClear,resourceFinish};
    static const xrtownershippreparationv1 sourcePolicy={sizeof(sourcePolicy),&sources,sourceReady,sourcePrepare};
    static const xrtownershippreparationv1 resourcePolicy={sizeof(resourcePolicy),&resources,resourceReady,resourcePrepare};
    size_t refs;if(!preparation)return NULL;
    if(ref.Ops==&sourceOps&&sourceCount(ref.Data,&refs)){*preparation=&sourcePolicy;return &sources;}
    if(ref.Ops==&resourceOps&&ref.Data&&(!count||families)){
        const Resource* r=ref.Data;bool known=false;
        for(size_t i=0;i<count;++i)if(families[i]&&families[i]==r->family){known=true;break;}
        if(known&&r->family->size==sizeof(*r->family)&&r->family->abiTag==XL6_EXECUTION_FAMILY_TAG_V1&&
            r->kind==r->family->kind&&resourceCount(ref.Data,&refs)){*preparation=&resourcePolicy;return &resources;}
    }
    return NULL;
}
XL6_ExecutionRetireResultV1 xl6ExecutionSourceDestroyV1(Source** input)
{
    XL6_ExecutionRetireResultV1 result={false,true};if(!input){xrtSetErrorKind(XERR_ARGUMENT);result.ok=false;return result;}
    Source* s=*input;if(!s){result.ready=true;return result;}
    xrtownershipscope mutation={0};begin(&mutation);bool entered=sourceEnterLocked(s,true);
    bool ready=entered&&s->closed&&!s->resources&&!s->active&&s->entries==1&&s->exited&&!s->joining&&
        (!s->claim||s->cleared)&&s->workerId!=xrtThreadCurrentId();
    bool retired=s->retired;if(ready&&!retired)s->joining=true;result.ok=entered&&!s->failed;end(&mutation);
    if(!entered){xrtSetErrorKind(XERR_STATE);return result;}
    if(!ready){
        begin(&mutation);bool wake=s->closed&&!s->resources&&!s->retired&&!s->cleared;
        if(wake)s->stop=true;
        end(&mutation);
        if(wake&&!xrtEventSet(s->changed))abort();
    }
    if(ready&&!retired){
        xwaitresult joined=s->joined?XWAIT_OK:xrtThreadWaitFor(s->thread,0);
        bool releasedThread=joined==XWAIT_OK;
        if(releasedThread){xrtThreadDestroy(s->thread);ready=xrtEventDestroy(s->changed);}
        else ready=false;
        xerror* failedError=!ready&&joined!=XWAIT_TIMEOUT?xrtTakeError():NULL;
        begin(&mutation);s->joining=false;
        if(releasedThread){s->thread=NULL;s->joined=true;}
        if(ready){s->changed=NULL;s->retired=true;}
        else if(joined!=XWAIT_TIMEOUT)sourceFailureLocked(s,&failedError);
        end(&mutation);xrtErrorFree(failedError);
    }
    begin(&mutation);result.ok=!s->failed;const xerror* original=s->error;end(&mutation);
    xerror* error=original?xrtErrorRef(original):NULL;
    if(ready){begin(&mutation);if(!s->creator||s->entries!=1)abort();s->creator=false;*input=NULL;end(&mutation);sourceDrop(s);result.ready=true;}
    sourceLeave(s);if(!result.ok)xrtSetErrorTake(error);else xrtErrorFree(error);return result;
}
bool xl6ExecutionRequestedInternalV1(const Resource* resource)
{xrtownershipscope mutation={0};begin(&mutation);bool requested=resource->requested;end(&mutation);return requested;}
