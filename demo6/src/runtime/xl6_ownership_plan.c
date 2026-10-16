#define XRT_MODULE_MEMORY
#include "lib/xrt/xrt.h"
#include "xl6_ownership_plan.h"
#include <stdlib.h>
#include <string.h>

typedef struct PlanNode {
    xrtownershipref ref;
    const XL6_OwnershipAdapterV1* adapter;
    const xrtownershippreparationv1* preparation;
    const XL6_OwnershipFinishV1* completion;
    bool held, claimed, finalized, finished, reachable, newClaim, prepared;
} PlanNode;
struct XL6_OwnershipPlan {
    xrtownershipref root;
    XL6_OwnershipResolve resolve;
    XL6_OwnershipResolveV2 resolveV2;
    XL6_OwnershipResolveV3 resolveV3;
    XL6_OwnershipProbeV1 probe;
    XL6_OwnershipDiscoverV1 discover;
    void* context;
    PlanNode* nodes;
    size_t count;
    bool running, committed, finalizerFailed;
};
typedef struct PlanBuild {
    XL6_OwnershipPlan* plan;
    PlanNode* nodes;
    size_t count, capacity;
    /* Snapshot rejection reports STATE. Our admission callback can also
     * fail while allocating its own bookkeeping; retain that diagnostic
     * explicitly across the callback boundary instead of losing MEM/RANGE. */
    xerror* admissionError;
    bool busy;
} PlanBuild;
static bool same_ref(xrtownershipref a,xrtownershipref b)
{ return a.Data==b.Data && a.Ops==b.Ops; }
static void freeze_end(xrtownershipscope* scope)
{ if(!xrtOwnershipScopeEnd(scope))abort(); }
static bool admit(xrtownershipref ref,void* context)
{
    PlanBuild* build=context; XL6_OwnershipPlan* plan=build->plan;
    const XL6_OwnershipAdapterV1* adapter=NULL;
    const xrtownershippreparationv1* preparation=NULL;
    const XL6_OwnershipFinishV1* completion=NULL;
    PlanNode node={0};
    if(plan->probe){
        XL6_OwnershipAdmissionV1 status=plan->probe(ref,plan->context,&adapter,&preparation,&completion);
        if(status==XL6_OWNERSHIP_ADMISSION_BUSY){
            if(adapter || preparation || completion){xrtSetErrorKind(XERR_STATE);return false;}
            build->busy=true;return false;
        }
        if(status!=XL6_OWNERSHIP_ADMISSION_ADMITTED){
            if(status!=XL6_OWNERSHIP_ADMISSION_REFUSED || adapter || preparation || completion)xrtSetErrorKind(XERR_STATE);
            return false;
        }
    }else if(plan->resolveV3){
        if(!plan->resolveV3(ref,plan->context,&adapter,&preparation,&completion))return false;
    }else if(plan->resolveV2){
        if(!plan->resolveV2(ref,plan->context,&adapter,&preparation))return false;
    }else adapter=plan->resolve(ref,plan->context);
    if(!adapter || adapter->size!=sizeof(*adapter) || !adapter->Hold || !adapter->Drop ||
        !adapter->Claim || !adapter->Restore || !adapter->Clear)return false;
    if(preparation && (preparation->size!=sizeof(*preparation) || preparation->Adapter!=adapter ||
        !preparation->Ready || !preparation->Prepare))return false;
    if(completion && (completion->size!=sizeof(*completion) || completion->Adapter!=adapter ||
        !completion->Step))return false;
    node.ref=ref;node.adapter=adapter;node.preparation=preparation;node.completion=completion;
    node.finalized=adapter->Finalize==NULL;node.finished=!completion && adapter->Finish==NULL;
    /* Old held identities are supplied as anchors, in this same order, before
     * traversal discovers new nodes. Descriptor replacement is not accepted. */
    if(build->count<plan->count){
        const PlanNode* old=&plan->nodes[build->count];
        if(!same_ref(old->ref,ref) || old->adapter!=adapter || old->preparation!=preparation ||
            old->completion!=completion)return false;
        node=*old;node.newClaim=false;
    }
    node.prepared=!preparation || preparation->Ready(ref.Data);
    if(build->count==build->capacity){
        size_t capacity=build->capacity?build->capacity*2:16;
        PlanNode* nodes;
        if(capacity<build->capacity || capacity>SIZE_MAX/sizeof(*nodes)){
            xrtSetErrorKind(XERR_RANGE);build->admissionError=xrtTakeError();return false;
        }
        nodes=xrtRealloc(build->nodes,capacity*sizeof(*nodes));
        if(!nodes){build->admissionError=xrtTakeError();return false;}
        build->nodes=nodes;build->capacity=capacity;
    }
    build->nodes[build->count++]=node;return true;
}
/* Failure runs no user callback: newly added pins are not last references,
 * since the unchanged original owning slots still exist under this freeze. */
static void rollback(PlanBuild* build)
{
    for(size_t i=build->count;i--!=0;){
        PlanNode* node=&build->nodes[i];
        if(node->newClaim)node->adapter->Restore(node->ref.Data,build->plan);
        if(i>=build->plan->count && node->held)node->adapter->Drop(node->ref.Data);
    }
    xrtFree(build->nodes);
}
typedef struct PlanAnchors {
    xrtownershipref* refs;
    size_t count, capacity;
} PlanAnchors;
static bool append_anchor(xrtownershipref ref,void* context)
{
    PlanAnchors* anchors=context;
    if(!ref.Data || !ref.Ops){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(anchors->count==anchors->capacity){
        size_t capacity=anchors->capacity?anchors->capacity*2:16;
        if(capacity<anchors->capacity || capacity>SIZE_MAX/sizeof(*anchors->refs)){
            xrtSetErrorKind(XERR_RANGE);return false;
        }
        xrtownershipref* refs=xrtRealloc(anchors->refs,capacity*sizeof(*refs));
        if(!refs)return false;
        anchors->refs=refs;anchors->capacity=capacity;
    }
    anchors->refs[anchors->count++]=ref;return true;
}
static XL6_OwnershipPlanResult rebuild(XL6_OwnershipPlan* plan)
{
    PlanBuild build={.plan=plan};xrtownershipsnapshot* snapshot=NULL;
    PlanAnchors anchors={0};XL6_OwnershipPlanResult result=XL6_OWNERSHIP_PLAN_ERROR;
    bool rootFound=false,rooted=false,pending=false,preparing=false;
    if(plan->probe && plan->discover){
        const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;const xrtownershipfinishv1* completion=NULL;
        XL6_OwnershipAdmissionV1 status=plan->probe(plan->root,plan->context,&adapter,&preparation,&completion);
        if(status==XL6_OWNERSHIP_ADMISSION_BUSY && !adapter && !preparation && !completion)return XL6_OWNERSHIP_PLAN_BUSY;
        if(status!=XL6_OWNERSHIP_ADMISSION_ADMITTED){xrtSetErrorKind(XERR_STATE);return XL6_OWNERSHIP_PLAN_ERROR;}
    }
    if(plan->count){
        if(plan->count>SIZE_MAX/sizeof(*anchors.refs)){xrtSetErrorKind(XERR_RANGE);goto failed;}
        anchors.refs=xrtMalloc(plan->count*sizeof(*anchors.refs));if(!anchors.refs)goto failed;
        anchors.count=anchors.capacity=plan->count;
        for(size_t i=0;i<plan->count;++i)anchors.refs[i]=plan->nodes[i].ref;
    }
    if(plan->discover){
        if(!plan->count && !append_anchor(plan->root,&anchors))goto failed;
        if(!plan->discover(append_anchor,&anchors,plan->context))goto failed;
    }
    if(!xrtOwnershipSnapshotCreate(anchors.count?anchors.refs:&plan->root,anchors.count?anchors.count:1,
        anchors.refs,plan->count,admit,&build,&snapshot))goto failed;
    if(build.count!=xrtOwnershipSnapshotNodeCount(snapshot) || build.count<plan->count)abort();
    for(size_t i=0;i<build.count;++i){
        PlanNode* node=&build.nodes[i];xrtownershipnode observed;
        if(!xrtOwnershipSnapshotNode(snapshot,i,&observed) || !same_ref(node->ref,observed.Reference))abort();
        node->reachable=observed.Reachable;
        if(same_ref(node->ref,plan->root)){rootFound=true;rooted=node->reachable;}
        if(!node->held){
            /* Rollback releases new holds while frozen. A zero-count anchor
             * has no original owner to prevent that from becoming a terminal
             * callback; independent borrowed storage is not a strong slot. */
            if(observed.StrongCount==0){xrtSetErrorKind(XERR_STATE);goto failed;}
            if(!node->adapter->Hold(node->ref.Data))goto failed;
            node->held=true;
        }
    }
    if(!rootFound)abort();
    for(size_t i=0;i<build.count;++i){
        PlanNode* node=&build.nodes[i];
        if(!node->reachable && !node->claimed){
            if(!node->adapter->Claim(node->ref.Data,plan)){result=XL6_OWNERSHIP_PLAN_BUSY;goto failed;}
            node->claimed=true;node->newClaim=true;
        }
        if(!node->reachable && !node->finalized)pending=true;
        if(!node->reachable && !node->prepared)preparing=true;
    }
    /* No fallible work remains. Resurrected nodes regain ordinary admission;
     * the caller receives ROOTED and must not clear the enclosing module. */
    for(size_t i=0;i<build.count;++i){
        PlanNode* node=&build.nodes[i];
        if(node->reachable && node->claimed){node->adapter->Restore(node->ref.Data,plan);node->claimed=false;}
        node->newClaim=false;
    }
    xrtFree(plan->nodes);plan->nodes=build.nodes;plan->count=build.count;
    xrtOwnershipSnapshotDestroy(snapshot);xrtFree(anchors.refs);
    return rooted?XL6_OWNERSHIP_PLAN_ROOTED:preparing?XL6_OWNERSHIP_PLAN_PREPARATIONS:
        pending?XL6_OWNERSHIP_PLAN_FINALIZERS:XL6_OWNERSHIP_PLAN_READY;
failed:
    { xerror* error=build.admissionError?build.admissionError:xrtTakeError();
      if(build.busy){xrtErrorFree(error);error=NULL;result=XL6_OWNERSHIP_PLAN_BUSY;}
      if(build.admissionError)xrtClearError();
      rollback(&build);xrtOwnershipSnapshotDestroy(snapshot);xrtFree(anchors.refs);
      xrtClearError();xrtSetErrorTake(error); }
    return result;
}
static XL6_OwnershipPlanResult create(xrtownershipref root,XL6_OwnershipResolve resolve,
    XL6_OwnershipResolveV2 resolveV2,XL6_OwnershipResolveV3 resolveV3,
    XL6_OwnershipDiscoverV1 discover,bool pinRoot,void* context,XL6_OwnershipPlan** output,XL6_OwnershipProbeV1 probe)
{
    xrtownershipscope freeze={0};XL6_OwnershipPlan* plan;XL6_OwnershipPlanResult result;
    if(!root.Data || !root.Ops || (!resolve && !resolveV2 && !resolveV3 && !probe) || !output){xrtSetErrorKind(XERR_ARGUMENT);return XL6_OWNERSHIP_PLAN_ERROR;}
    if(!xrtOwnershipFreezeTryBegin(&freeze))return XL6_OWNERSHIP_PLAN_BUSY;
    plan=xrtCalloc(1,sizeof(*plan));
    if(!plan){freeze_end(&freeze);return XL6_OWNERSHIP_PLAN_ERROR;}
    plan->root=root;plan->resolve=resolve;plan->resolveV2=resolveV2;plan->resolveV3=resolveV3;
    plan->discover=discover;plan->context=context;plan->probe=probe;
    if(pinRoot){
        /* Acquire a real plan-owned pin before snapshotting a native control
         * record with independent storage lifetime and no incoming lease.
         * Its Hold is recorded as an internal slot, never a made-up count. */
        PlanBuild seed={.plan=plan};
        if(!admit(root,&seed) || !seed.nodes[0].adapter->Hold(root.Data)){
            bool busy=seed.busy;
            xerror* error=seed.admissionError ? seed.admissionError : xrtTakeError();
            if(seed.admissionError)xrtClearError();
            xrtFree(seed.nodes);xrtFree(plan);freeze_end(&freeze);
            if(busy){xrtErrorFree(error);return XL6_OWNERSHIP_PLAN_BUSY;}
            xrtSetErrorTake(error);return XL6_OWNERSHIP_PLAN_ERROR;
        }
        seed.nodes[0].held=true;plan->nodes=seed.nodes;plan->count=1;
    }
    result=rebuild(plan);
    if(result<=XL6_OWNERSHIP_PLAN_BUSY){
        const XL6_OwnershipAdapterV1* adapter=pinRoot ? plan->nodes[0].adapter : NULL;
        xerror* error=xrtTakeError();xrtFree(plan->nodes);xrtFree(plan);freeze_end(&freeze);
        if(adapter)adapter->Drop(root.Data);
        xrtClearError();xrtSetErrorTake(error);return result;
    }
    *output=plan;freeze_end(&freeze);return result;
}
XL6_OwnershipPlanResult xl6OwnershipPlanTryCreate(xrtownershipref root,XL6_OwnershipResolve resolve,
    void* context,XL6_OwnershipPlan** output)
{ return create(root,resolve,NULL,NULL,NULL,false,context,output,NULL); }
XL6_OwnershipPlanResult xl6OwnershipPlanTryCreateV2(xrtownershipref root,XL6_OwnershipResolveV2 resolve,
    void* context,XL6_OwnershipPlan** output)
{ return create(root,NULL,resolve,NULL,NULL,false,context,output,NULL); }
XL6_OwnershipPlanResult xl6OwnershipPlanTryCreateV3(xrtownershipref root,XL6_OwnershipResolveV3 resolve,
    void* context,XL6_OwnershipPlan** output)
{ return create(root,NULL,NULL,resolve,NULL,false,context,output,NULL); }
XL6_OwnershipPlanResult xl6OwnershipPlanTryCreateV4(xrtownershipref root,XL6_OwnershipResolveV3 resolve,
    XL6_OwnershipDiscoverV1 discover,void* context,XL6_OwnershipPlan** output)
{
    if(!resolve || !discover){xrtSetErrorKind(XERR_ARGUMENT);return XL6_OWNERSHIP_PLAN_ERROR;}
    return create(root,NULL,NULL,resolve,discover,false,context,output,NULL);
}
XL6_OwnershipPlanResult xl6OwnershipPlanTryCreateV5(xrtownershipref root,XL6_OwnershipResolveV3 resolve,
    XL6_OwnershipDiscoverV1 discover,void* context,XL6_OwnershipPlan** output)
{
    if(!resolve || !discover){xrtSetErrorKind(XERR_ARGUMENT);return XL6_OWNERSHIP_PLAN_ERROR;}
    return create(root,NULL,NULL,resolve,discover,true,context,output,NULL);
}
XL6_OwnershipPlanResult xl6OwnershipPlanTryCreateV6(xrtownershipref root,XL6_OwnershipProbeV1 probe,
    XL6_OwnershipDiscoverV1 discover,bool pinRoot,void* context,XL6_OwnershipPlan** output)
{
    if(!probe){xrtSetErrorKind(XERR_ARGUMENT);return XL6_OWNERSHIP_PLAN_ERROR;}
    return create(root,NULL,NULL,NULL,discover,pinRoot,context,output,probe);
}
XL6_OwnershipStepResult xl6OwnershipPlanPrepare(XL6_OwnershipPlan* plan)
{
    if(!plan || (!plan->resolveV2 && !plan->resolveV3 && !plan->probe) || plan->running || plan->committed || plan->finalizerFailed){
        xrtSetErrorKind(XERR_STATE);return XL6_OWNERSHIP_STEP_ERROR;
    }
    size_t budget=plan->count;
    for(size_t step=0;step<=budget;++step){
        xrtownershipscope freeze={0};XL6_OwnershipPlanResult result;bool progressed=false;
        if(!xrtOwnershipFreezeTryBegin(&freeze))return XL6_OWNERSHIP_STEP_BUSY;
        result=rebuild(plan);freeze_end(&freeze);
        if(result==XL6_OWNERSHIP_PLAN_ERROR)return XL6_OWNERSHIP_STEP_ERROR;
        if(result==XL6_OWNERSHIP_PLAN_BUSY)return XL6_OWNERSHIP_STEP_BUSY;
        if(result!=XL6_OWNERSHIP_PLAN_PREPARATIONS)return XL6_OWNERSHIP_STEP_READY;
        if(step==budget)return XL6_OWNERSHIP_STEP_BUSY;
        plan->running=true;
        for(size_t i=0;i<plan->count;++i){PlanNode* node=&plan->nodes[i];
            if(node->claimed && !node->prepared){
                xrtownershipprepareresult status=node->preparation->Prepare(node->ref.Data,plan);
                if(status==XRT_OWNERSHIP_PREPARE_BUSY)continue;
                if(status==XRT_OWNERSHIP_PREPARE_READY){progressed=true;break;}
                plan->running=false;
                if(status!=XRT_OWNERSHIP_PREPARE_FAILED)xrtSetErrorKind(XERR_INTERNAL);
                return XL6_OWNERSHIP_STEP_FAILED;
            }
        }
        plan->running=false;
        if(!progressed)return XL6_OWNERSHIP_STEP_BUSY;
    }
    return XL6_OWNERSHIP_STEP_BUSY;
}
XL6_OwnershipPlanResult xl6OwnershipPlanTryRefresh(XL6_OwnershipPlan* plan)
{
    xrtownershipscope freeze={0};XL6_OwnershipPlanResult result;
    if(!plan || plan->running || plan->committed){xrtSetErrorKind(XERR_STATE);return XL6_OWNERSHIP_PLAN_ERROR;}
    if(!xrtOwnershipFreezeTryBegin(&freeze))return XL6_OWNERSHIP_PLAN_BUSY;
    result=rebuild(plan);freeze_end(&freeze);return result;
}
XL6_OwnershipStepResult xl6OwnershipPlanFinalizeV2(XL6_OwnershipPlan* plan)
{
    if(!plan || (!plan->resolveV2 && !plan->resolveV3 && !plan->probe) || plan->running || plan->committed || plan->finalizerFailed){
        xrtSetErrorKind(XERR_STATE);return XL6_OWNERSHIP_STEP_ERROR;
    }
    size_t budget=plan->count;
    for(size_t step=0;step<budget;++step){
        xrtownershipscope freeze={0};XL6_OwnershipPlanResult result;PlanNode* next=NULL;
        if(!xrtOwnershipFreezeTryBegin(&freeze))return XL6_OWNERSHIP_STEP_BUSY;
        result=rebuild(plan);freeze_end(&freeze);
        if(result==XL6_OWNERSHIP_PLAN_ERROR)return XL6_OWNERSHIP_STEP_ERROR;
        if(result==XL6_OWNERSHIP_PLAN_BUSY)return XL6_OWNERSHIP_STEP_BUSY;
        if(result!=XL6_OWNERSHIP_PLAN_FINALIZERS)return XL6_OWNERSHIP_STEP_READY;
        for(size_t i=0;i<plan->count;++i)if(plan->nodes[i].claimed && !plan->nodes[i].finalized){next=&plan->nodes[i];break;}
        if(!next)abort();
        plan->running=true;next->finalized=true;
        bool ok=next->adapter->Finalize(next->ref.Data,plan);plan->running=false;
        if(!ok){plan->finalizerFailed=true;return XL6_OWNERSHIP_STEP_FAILED;}
    }
    return XL6_OWNERSHIP_STEP_BUSY;
}
bool xl6OwnershipPlanFinalize(XL6_OwnershipPlan* plan)
{
    bool ok=true;
    if(!plan || plan->running || plan->committed || plan->finalizerFailed){xrtSetErrorKind(XERR_STATE);return false;}
    if(plan->resolveV2 || plan->resolveV3 || plan->probe)return xl6OwnershipPlanFinalizeV2(plan)>=XL6_OWNERSHIP_STEP_BUSY;
    plan->running=true;
    for(size_t i=0;i<plan->count;++i){
        PlanNode* node=&plan->nodes[i];
        if(node->claimed && !node->finalized){
            node->finalized=true;
            if(!node->adapter->Finalize(node->ref.Data,plan)){plan->finalizerFailed=true;ok=false;break;}
        }
    }
    plan->running=false;return ok;
}
XL6_OwnershipPlanResult xl6OwnershipPlanTryCommit(XL6_OwnershipPlan* plan)
{
    xrtownershipscope freeze={0};XL6_OwnershipPlanResult result;
    if(!plan || plan->running || plan->committed || plan->finalizerFailed){xrtSetErrorKind(XERR_STATE);return XL6_OWNERSHIP_PLAN_ERROR;}
    if(!xrtOwnershipFreezeTryBegin(&freeze))return XL6_OWNERSHIP_PLAN_BUSY;
    result=rebuild(plan);
    if(result==XL6_OWNERSHIP_PLAN_READY){
        for(size_t i=0;i<plan->count;++i){PlanNode* node=&plan->nodes[i];if(node->claimed)node->adapter->Clear(node->ref.Data,plan);}
        plan->committed=true;
    }
    freeze_end(&freeze);return result;
}
XL6_OwnershipStepResult xl6OwnershipPlanFinishV2(XL6_OwnershipPlan* plan)
{
    XL6_OwnershipStepResult result=XL6_OWNERSHIP_STEP_READY;
    if(!plan || plan->running || !plan->committed){xrtSetErrorKind(XERR_STATE);return XL6_OWNERSHIP_STEP_ERROR;}
    plan->running=true;
    for(size_t i=0;i<plan->count;++i){PlanNode* node=&plan->nodes[i];
        if(node->claimed && !node->finished){
            xrtownershipfinishresult status=node->completion
                ? node->completion->Step(node->ref.Data,plan)
                : node->adapter->Finish(node->ref.Data,plan)?XRT_OWNERSHIP_FINISH_READY:XRT_OWNERSHIP_FINISH_FAILED;
            if(status==XRT_OWNERSHIP_FINISH_READY)node->finished=true;
            else if(status==XRT_OWNERSHIP_FINISH_BUSY)result=XL6_OWNERSHIP_STEP_BUSY;
            else {
                if(status!=XRT_OWNERSHIP_FINISH_FAILED)xrtSetErrorKind(XERR_INTERNAL);
                result=XL6_OWNERSHIP_STEP_FAILED;break;
            }
        }
    }
    plan->running=false;return result;
}
bool xl6OwnershipPlanFinish(XL6_OwnershipPlan* plan)
{ return xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY; }
XL6_OwnershipPlanResult xl6OwnershipPlanTryDestroy(XL6_OwnershipPlan* plan)
{
    xrtownershipscope freeze={0};
    if(!plan || plan->running){xrtSetErrorKind(XERR_STATE);return XL6_OWNERSHIP_PLAN_ERROR;}
    if(plan->committed){
        for(size_t i=0;i<plan->count;++i)if(plan->nodes[i].claimed && !plan->nodes[i].finished){xrtSetErrorKind(XERR_STATE);return XL6_OWNERSHIP_PLAN_ERROR;}
    }else{
        if(!xrtOwnershipFreezeTryBegin(&freeze))return XL6_OWNERSHIP_PLAN_BUSY;
        for(size_t i=0;i<plan->count;++i){PlanNode* node=&plan->nodes[i];if(node->claimed)node->adapter->Restore(node->ref.Data,plan);}
        freeze_end(&freeze);
    }
    plan->running=true;
    for(size_t i=plan->count;i--!=0;)plan->nodes[i].adapter->Drop(plan->nodes[i].ref.Data);
    xrtFree(plan->nodes);xrtFree(plan);return XL6_OWNERSHIP_PLAN_READY;
}
