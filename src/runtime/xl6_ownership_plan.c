#define XRT_MODULE_MEMORY
#include "src/common/xl6_xrt.h"
#include "xl6_ownership_plan.h"
#include "xl6_ownership_completion.h"
#include "xl6_ownership_finalization.h"
#include "xl6_callback_failure.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>
#include <string.h>

typedef struct PlanNode {
    xrtownershipref ref;
    const XL6_OwnershipAdapterV1* adapter;
    const xrtownershippreparationv1* preparation;
    const XL6_OwnershipFinishV1* completion;
    const xrtownershipfinalizationv1* finalization;
    bool finalizationStarted;
    bool held, claimed, finalized, finished, reachable, newClaim, prepared, preparationStarted;
} PlanNode;
struct XL6_OwnershipPlan {
    xrtownershipref root;
    const XL6_OwnershipBoundaryV1* boundary;
    XL6_OwnershipResolve resolve;
    XL6_OwnershipResolveV2 resolveV2;
    XL6_OwnershipResolveV3 resolveV3;
    XL6_OwnershipProbeV1 probe;
    XL6_OwnershipDiscoverV1 discover;
    XL6_OwnershipFinalizationResolveV1 resolveFinalization;
    void* context;
    PlanNode* nodes;
    size_t count;
    bool running, committed, finalizerFailed, semanticRetryRequired;
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
static bool freeze_begin(const XL6_OwnershipBoundaryV1* boundary,xrtownershipscope* scope)
{
    if(boundary&&!boundary->Enter(boundary->context))return false;
    if(xrtOwnershipFreezeTryBegin(scope))return true;
    if(boundary)boundary->Leave(boundary->context);
    return false;
}
static void freeze_end(const XL6_OwnershipBoundaryV1* boundary,xrtownershipscope* scope)
{
    if(!xrtOwnershipScopeEnd(scope))abort();
    if(boundary)boundary->Leave(boundary->context);
}
static bool admit(xrtownershipref ref,void* context)
{
    PlanBuild* build=context; XL6_OwnershipPlan* plan=build->plan;
    const XL6_OwnershipAdapterV1* adapter=NULL;
    const xrtownershippreparationv1* preparation=NULL;
    const XL6_OwnershipFinishV1* completion=NULL;
    const xrtownershipfinalizationv1* finalization=NULL;
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
    if(!xl6OwnershipCompletionValid(completion,adapter))return false;
    if(plan->resolveFinalization &&
       !plan->resolveFinalization(ref,plan->context,adapter,&finalization))return false;
    if(!xl6OwnershipFinalizationValidV1(finalization,adapter))return false;
    const xrtownershipfinishv2* checked=xl6OwnershipCompletionV2Get(completion);
    if(checked && !checked->DropReady(ref.Data)){build->busy=true;return false;}
    node.ref=ref;node.adapter=adapter;node.preparation=preparation;node.completion=completion;node.finalization=finalization;
    node.finalized=!finalization && adapter->Finalize==NULL;node.finished=!completion && adapter->Finish==NULL;
    /* Old held identities are supplied as anchors, in this same order, before
     * traversal discovers new nodes. Descriptor replacement is not accepted. */
    if(build->count<plan->count){
        const PlanNode* old=&plan->nodes[build->count];
        if(!same_ref(old->ref,ref) || old->adapter!=adapter || old->preparation!=preparation ||
            old->completion!=completion || old->finalization!=finalization)return false;
        node=*old;node.newClaim=false;
    }
    node.prepared=!preparation || preparation->Ready(ref.Data);
    if(node.prepared)node.preparationStarted=false;
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
static XL6_OwnershipPlanResult rebuild_body(XL6_OwnershipPlan* plan)
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
    if((plan->discover||(plan->boundary&&plan->boundary->Discover))&&!plan->count&&
       !append_anchor(plan->root,&anchors))goto failed;
    if(plan->discover&&!plan->discover(append_anchor,&anchors,plan->context))goto failed;
    if(plan->boundary&&plan->boundary->Discover&&
       !plan->boundary->Discover(append_anchor,&anchors,plan->boundary->context))goto failed;
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
static XL6_OwnershipPlanResult rebuild(XL6_OwnershipPlan* plan)
{
    /* A refused admission can make SnapshotCreate publish a STATE error.
     * Keep the caller's earlier diagnostic independent of this read/claim
     * transaction, especially when a pure DropReady refusal maps to BUSY. */
    xerror* previous=xrtTakeError();
    XL6_OwnershipPlanResult result=rebuild_body(plan);
    if(previous){xrtClearError();xrtSetErrorTake(previous);}
    return result;
}
static XL6_OwnershipPlanResult create(xrtownershipref root,XL6_OwnershipResolve resolve,
    XL6_OwnershipResolveV2 resolveV2,XL6_OwnershipResolveV3 resolveV3,
    XL6_OwnershipDiscoverV1 discover,bool pinRoot,void* context,XL6_OwnershipPlan** output,XL6_OwnershipProbeV1 probe,const XL6_OwnershipBoundaryV1* boundary,
    XL6_OwnershipFinalizationResolveV1 finalization)
{
    xrtownershipscope freeze={0};XL6_OwnershipPlan* plan;XL6_OwnershipPlanResult result;
    if(!root.Data || !root.Ops || (!resolve && !resolveV2 && !resolveV3 && !probe) || !output){xrtSetErrorKind(XERR_ARGUMENT);return XL6_OWNERSHIP_PLAN_ERROR;}
    if(boundary&&(boundary->size!=sizeof(*boundary)||!boundary->Enter||!boundary->Leave)){
        xrtSetErrorKind(XERR_ARGUMENT);return XL6_OWNERSHIP_PLAN_ERROR;
    }
    if(!freeze_begin(boundary,&freeze))return XL6_OWNERSHIP_PLAN_BUSY;
    plan=xrtCalloc(1,sizeof(*plan));
    if(!plan){freeze_end(boundary,&freeze);return XL6_OWNERSHIP_PLAN_ERROR;}
    plan->boundary=boundary;plan->root=root;plan->resolve=resolve;plan->resolveV2=resolveV2;plan->resolveV3=resolveV3;
    plan->discover=discover;plan->context=context;plan->probe=probe;plan->resolveFinalization=finalization;
    if(pinRoot){
        /* Acquire a real plan-owned pin before snapshotting a native control
         * record with independent storage lifetime and no incoming lease.
         * Its Hold is recorded as an internal slot, never a made-up count. */
        PlanBuild seed={.plan=plan};
        if(!admit(root,&seed) || !seed.nodes[0].adapter->Hold(root.Data)){
            bool busy=seed.busy;
            xerror* error=seed.admissionError ? seed.admissionError : xrtTakeError();
            if(seed.admissionError)xrtClearError();
            xrtFree(seed.nodes);xrtFree(plan);freeze_end(boundary,&freeze);
            if(busy){xrtSetErrorTake(error);return XL6_OWNERSHIP_PLAN_BUSY;}
            xrtSetErrorTake(error);return XL6_OWNERSHIP_PLAN_ERROR;
        }
        seed.nodes[0].held=true;plan->nodes=seed.nodes;plan->count=1;
    }
    result=rebuild(plan);
    if(result<=XL6_OWNERSHIP_PLAN_BUSY){
        const XL6_OwnershipAdapterV1* adapter=pinRoot ? plan->nodes[0].adapter : NULL;
        const xrtownershipfinishv2* completion=pinRoot
            ? xl6OwnershipCompletionV2Get(plan->nodes[0].completion) : NULL;
        bool primaryFailed=result==XL6_OWNERSHIP_PLAN_ERROR;
        xerror* error=xrtTakeError();xrtFree(plan->nodes);xrtFree(plan);freeze_end(boundary,&freeze);
        if(completion){
            if(!completion->DropChecked(root.Data))result=XL6_OWNERSHIP_PLAN_ERROR;
        }else if(adapter)adapter->Drop(root.Data);
        xerror* cleanup=xrtTakeError();
        if(cleanup)result=XL6_OWNERSHIP_PLAN_ERROR;
        if(primaryFailed || error)xrtErrorFree(cleanup);else error=cleanup;
        xrtSetErrorTake(error);return result;
    }
    *output=plan;freeze_end(boundary,&freeze);return result;
}
XL6_OwnershipPlanResult xl6OwnershipPlanTryCreate(xrtownershipref root,XL6_OwnershipResolve resolve,
    void* context,XL6_OwnershipPlan** output)
{ return create(root,resolve,NULL,NULL,NULL,false,context,output,NULL,NULL,NULL); }
XL6_OwnershipPlanResult xl6OwnershipPlanTryCreateV2(xrtownershipref root,XL6_OwnershipResolveV2 resolve,
    void* context,XL6_OwnershipPlan** output)
{ return create(root,NULL,resolve,NULL,NULL,false,context,output,NULL,NULL,NULL); }
XL6_OwnershipPlanResult xl6OwnershipPlanTryCreateV3(xrtownershipref root,XL6_OwnershipResolveV3 resolve,
    void* context,XL6_OwnershipPlan** output)
{ return create(root,NULL,NULL,resolve,NULL,false,context,output,NULL,NULL,NULL); }
XL6_OwnershipPlanResult xl6OwnershipPlanTryCreateV4(xrtownershipref root,XL6_OwnershipResolveV3 resolve,
    XL6_OwnershipDiscoverV1 discover,void* context,XL6_OwnershipPlan** output)
{
    if(!resolve || !discover){xrtSetErrorKind(XERR_ARGUMENT);return XL6_OWNERSHIP_PLAN_ERROR;}
    return create(root,NULL,NULL,resolve,discover,false,context,output,NULL,NULL,NULL);
}
XL6_OwnershipPlanResult xl6OwnershipPlanTryCreateV5(xrtownershipref root,XL6_OwnershipResolveV3 resolve,
    XL6_OwnershipDiscoverV1 discover,void* context,XL6_OwnershipPlan** output)
{
    if(!resolve || !discover){xrtSetErrorKind(XERR_ARGUMENT);return XL6_OWNERSHIP_PLAN_ERROR;}
    return create(root,NULL,NULL,resolve,discover,true,context,output,NULL,NULL,NULL);
}
XL6_OwnershipPlanResult xl6OwnershipPlanTryCreateV6(xrtownershipref root,XL6_OwnershipProbeV1 probe,
    XL6_OwnershipDiscoverV1 discover,bool pinRoot,void* context,XL6_OwnershipPlan** output)
{
    if(!probe){xrtSetErrorKind(XERR_ARGUMENT);return XL6_OWNERSHIP_PLAN_ERROR;}
    return create(root,NULL,NULL,NULL,discover,pinRoot,context,output,probe,NULL,NULL);
}
XL6_OwnershipPlanResult xl6OwnershipPlanTryCreateV7(xrtownershipref root,XL6_OwnershipProbeV1 probe,
    XL6_OwnershipDiscoverV1 discover,bool pinRoot,void* context,
    const XL6_OwnershipBoundaryV1* boundary,XL6_OwnershipPlan** output)
{
    if(!probe){xrtSetErrorKind(XERR_ARGUMENT);return XL6_OWNERSHIP_PLAN_ERROR;}
    return create(root,NULL,NULL,NULL,discover,pinRoot,context,output,probe,boundary,NULL);
}
XL6_OwnershipPlanResult xl6OwnershipPlanTryCreateV8(xrtownershipref root,XL6_OwnershipProbeV1 probe,
    XL6_OwnershipDiscoverV1 discover,XL6_OwnershipFinalizationResolveV1 finalization,
    bool pinRoot,void* context,const XL6_OwnershipBoundaryV1* boundary,XL6_OwnershipPlan** output)
{
    if(!probe || !finalization){xrtSetErrorKind(XERR_ARGUMENT);return XL6_OWNERSHIP_PLAN_ERROR;}
    return create(root,NULL,NULL,NULL,discover,pinRoot,context,output,probe,boundary,finalization);
}
XL6_OwnershipStepResult xl6OwnershipPlanPrepare(XL6_OwnershipPlan* plan)
{
    if(!plan || (!plan->resolveV2 && !plan->resolveV3 && !plan->probe) || plan->running || plan->committed || plan->finalizerFailed){
        xrtSetErrorKind(XERR_STATE);return XL6_OWNERSHIP_STEP_ERROR;
    }
    size_t budget=plan->count;
    for(size_t step=0;step<=budget;++step){
        xrtownershipscope freeze={0};XL6_OwnershipPlanResult result;bool progressed=false;
        if(!freeze_begin(plan->boundary,&freeze))return XL6_OWNERSHIP_STEP_BUSY;
        result=rebuild(plan);
        /* An already authorized asynchronous preparation can itself make
         * Count/Trace BUSY until its continuation completes. Its REAL pin,
         * claim and immutable preparation capability survive that temporary
         * read refusal. Only its separate read-only Ready predicate is queried
         * here, under freeze; no Count/Trace/admission exception is granted. */
        bool continuing=result==XL6_OWNERSHIP_PLAN_BUSY;
        if(continuing)for(size_t i=0;i<plan->count;++i){PlanNode* node=&plan->nodes[i];
            if(node->held&&node->claimed&&node->preparationStarted&&!node->prepared&&node->preparation&&
                node->preparation->Ready(node->ref.Data)){
                node->prepared=true;node->preparationStarted=false;progressed=true;
            }
        }
        freeze_end(plan->boundary,&freeze);
        if(result==XL6_OWNERSHIP_PLAN_ERROR)return XL6_OWNERSHIP_STEP_ERROR;
        if(!continuing&&result!=XL6_OWNERSHIP_PLAN_PREPARATIONS)return XL6_OWNERSHIP_STEP_READY;
        if(step==budget)return XL6_OWNERSHIP_STEP_BUSY;
        plan->running=true;
        for(size_t i=0;i<plan->count;++i){PlanNode* node=&plan->nodes[i];
            /* A BUSY snapshot never authorizes a NEW preparation. Resume only
             * a duty begun after a previously complete unreachable graph proof. */
            if(node->held&&node->claimed&&!node->prepared&&node->preparation&&(!continuing||node->preparationStarted)){
                node->preparationStarted=true;
                xrtownershipprepareresult status=node->preparation->Prepare(node->ref.Data,plan);
                if(status==XRT_OWNERSHIP_PREPARE_BUSY)continue;
                if(status==XRT_OWNERSHIP_PREPARE_READY){
                    node->prepared=true;node->preparationStarted=false;progressed=true;break;
                }
                plan->running=false;
                if(status!=XRT_OWNERSHIP_PREPARE_FAILED)xrtSetErrorKind(XERR_INTERNAL);
                return XL6_OWNERSHIP_STEP_FAILED;
            }
        }
        plan->running=false;
        /* Refresh before ANY further semantic duty, after a READY callback may
         * have changed edges/resurrected roots. Completed work is not repeated
         * even if that refresh fails allocation. Commit still needs full proof. */
        if(!progressed)return XL6_OWNERSHIP_STEP_BUSY;
    }
    return XL6_OWNERSHIP_STEP_BUSY;
}
XL6_OwnershipPlanResult xl6OwnershipPlanTryRefresh(XL6_OwnershipPlan* plan)
{
    xrtownershipscope freeze={0};XL6_OwnershipPlanResult result;
    if(!plan || plan->running || plan->committed){xrtSetErrorKind(XERR_STATE);return XL6_OWNERSHIP_PLAN_ERROR;}
    if(!freeze_begin(plan->boundary,&freeze))return XL6_OWNERSHIP_PLAN_BUSY;
    result=rebuild(plan);freeze_end(plan->boundary,&freeze);return result;
}
bool xl6OwnershipPlanRetryFinalizationV1(XL6_OwnershipPlan* plan)
{
    if(!plan || plan->running || plan->committed || plan->finalizerFailed){xrtSetErrorKind(XERR_STATE);return false;}
    plan->semanticRetryRequired=false;return true;
}
XL6_OwnershipStepResult xl6OwnershipPlanFinalizeStepV1(XL6_OwnershipPlan* plan,bool* semanticFailed)
{
    if(!semanticFailed){xrtSetErrorKind(XERR_ARGUMENT);return XL6_OWNERSHIP_STEP_ERROR;}
    *semanticFailed=false;
    if(!plan || (!plan->resolveV2 && !plan->resolveV3 && !plan->probe) ||
       plan->running || plan->committed || plan->finalizerFailed){
        xrtSetErrorKind(XERR_STATE);return XL6_OWNERSHIP_STEP_ERROR;
    }
    /* An incomplete failed semantic duty is never replayed by automatic
     * retirement or by the timeout loop of the same checked drain. */
    if(plan->semanticRetryRequired)return XL6_OWNERSHIP_STEP_WAITING;
    xerror* ambient=xrtTakeError();xl6_error_result first={true,NULL};
    XL6_OwnershipStepResult result=XL6_OWNERSHIP_STEP_READY;
    /* Every old identity is an anchor in EXACT original order in rebuild();
     * admit() rejects any identity/descriptor mismatch before publication.
     * Index+initial count therefore bounds a turn without admitting a new
     * callback-created peer or invoking a waiting peer twice. */
    size_t initial=plan->count;bool waiting=false,progressed=false;
    for(size_t index=0;index<initial;++index){
        xrtownershipscope freeze={0};
        if(!freeze_begin(plan->boundary,&freeze)){result=(waiting||progressed)?XL6_OWNERSHIP_STEP_WAITING:XL6_OWNERSHIP_STEP_BUSY;goto done;}
        XL6_OwnershipPlanResult status=rebuild(plan);
        if(status==XL6_OWNERSHIP_PLAN_ERROR){freeze_end(plan->boundary,&freeze);result=XL6_OWNERSHIP_STEP_ERROR;goto done;}
        bool continuing=status==XL6_OWNERSHIP_PLAN_BUSY;
        if(!continuing && status!=XL6_OWNERSHIP_PLAN_FINALIZERS){
            freeze_end(plan->boundary,&freeze);goto done;
        }
        PlanNode* node=&plan->nodes[index];
        bool invoke=node->held && node->claimed && !node->reachable && !node->finalized;
        if(continuing)invoke=invoke && node->finalization && node->finalizationStarted;
        if(invoke && node->finalization && !node->finalization->CanStep(node->ref.Data,plan)){
            invoke=false;waiting=true;
        }
        freeze_end(plan->boundary,&freeze);
        if(!invoke){if(continuing)waiting=true;continue;}
        xl6_error_scope errors={0};
        if(!xl6ErrorScopeBegin(&errors)){result=XL6_OWNERSHIP_STEP_ERROR;goto done;}
        plan->running=true;
        if(node->finalization){
            node->finalizationStarted=true;progressed=true;bool complete=false;
            bool ok=node->finalization->Step(node->ref.Data,plan,&complete);
            plan->running=false;node->finalized=complete;
            if(!ok || xrtGetError())xl6ErrorScopeCaptureFailure(&errors);
            xl6_error_result outcome=xl6ErrorScopeEnd(&errors);
            if(!outcome.ok){
                /* A separate failed bit retains even NULL. A later snapshot
                 * OOM or peer cleanup cannot replace this first outcome. */
                first=outcome;
                if(!complete){plan->semanticRetryRequired=true;result=XL6_OWNERSHIP_STEP_WAITING;goto done;}
                /* This duty is definitely consumed. Revalidate before Clear,
                 * but neither replay it nor run another semantic callback in
                 * the same failed turn. */
                break;
            }
            xrtErrorFree(outcome.error);if(!complete)waiting=true;
        }else{
            /* The V1 synchronous finalizer is still attempted at most once.
             * Its false result is terminal, never a resumable wait. */
            node->finalized=true;
            bool ok=!node->adapter->Finalize || node->adapter->Finalize(node->ref.Data,plan);
            plan->running=false;
            if(!ok || xrtGetError())xl6ErrorScopeCaptureFailure(&errors);
            xl6_error_result outcome=xl6ErrorScopeEnd(&errors);
            if(!outcome.ok){plan->finalizerFailed=true;first=outcome;result=XL6_OWNERSHIP_STEP_FAILED;goto done;}
            xrtErrorFree(outcome.error);
        }
    }
    { xrtownershipscope freeze={0};
      if(!freeze_begin(plan->boundary,&freeze)){result=(waiting||progressed)?XL6_OWNERSHIP_STEP_WAITING:XL6_OWNERSHIP_STEP_BUSY;goto done;}
      XL6_OwnershipPlanResult status=rebuild(plan);freeze_end(plan->boundary,&freeze);
      if(status==XL6_OWNERSHIP_PLAN_ERROR)result=XL6_OWNERSHIP_STEP_ERROR;
      else if(status==XL6_OWNERSHIP_PLAN_BUSY)result=(waiting||progressed)?XL6_OWNERSHIP_STEP_WAITING:XL6_OWNERSHIP_STEP_BUSY;
      else if(status==XL6_OWNERSHIP_PLAN_FINALIZERS)result=XL6_OWNERSHIP_STEP_WAITING;
    }
done:
    *semanticFailed=!first.ok;
    if(!first.ok){xrtErrorFree(ambient);xrtClearError();xrtSetErrorTake(first.error);}
    else if(ambient){xrtClearError();xrtSetErrorTake(ambient);}
    return result;
}
XL6_OwnershipStepResult xl6OwnershipPlanFinalizeV2(XL6_OwnershipPlan* plan)
{
    bool failed=false;XL6_OwnershipStepResult result=xl6OwnershipPlanFinalizeStepV1(plan,&failed);
    /* The old result cannot honestly describe a side-effecting WAIT. Require
     * explicit consumers to use the new orthogonal API; retain actual owners. */
    return failed || result==XL6_OWNERSHIP_STEP_WAITING ? XL6_OWNERSHIP_STEP_FAILED:result;
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
    if(!freeze_begin(plan->boundary,&freeze))return XL6_OWNERSHIP_PLAN_BUSY;
    result=rebuild(plan);
    if(result==XL6_OWNERSHIP_PLAN_READY && plan->semanticRetryRequired)result=XL6_OWNERSHIP_PLAN_FINALIZERS;
    if(result==XL6_OWNERSHIP_PLAN_READY){
        for(size_t i=0;i<plan->count;++i){PlanNode* node=&plan->nodes[i];if(node->claimed)node->adapter->Clear(node->ref.Data,plan);}
        plan->committed=true;
    }
    freeze_end(plan->boundary,&freeze);return result;
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
static void publish_drop_failure(xl6_error_scope* scope)
{
    bool enclosing=scope->previous!=NULL;
    xl6_error_result failure=xl6ErrorScopeEnd(scope);
    if(failure.ok)return;
    xerror* previous=xrtTakeError();
    if(enclosing && previous){
        /* The caller's already escaping diagnostic has priority over a later
         * cleanup error. A NULL first failure in its scope is preserved too. */
        xrtErrorFree(failure.error);failure.error=previous;previous=NULL;
    }
    if(!xl6ErrorScopeTryPropagateFailureV1(&failure)){
        /* No active checked caller: retain the independent failure bit even
         * if OOM prevented a diagnostic. Bare C still sees an available error. */
        xerror* diagnostic=previous ? previous : xrtErrorRef(failure.error);
        xl6CallbackFailureRecordV1(failure);xrtSetErrorTake(diagnostic);
    }
}
XL6_OwnershipPlanResult xl6OwnershipPlanTryDestroy(XL6_OwnershipPlan* plan)
{
    xrtownershipscope freeze={0};
    if(!plan || plan->running){xrtSetErrorKind(XERR_STATE);return XL6_OWNERSHIP_PLAN_ERROR;}
    XL6_CodeLeaseReleaseScopeV1 releases={0};
    xl6_error_scope errors={0};bool checked=false;
    for(size_t i=0;i<plan->count;++i){
        PlanNode* node=&plan->nodes[i];
        if(!xl6OwnershipCompletionValid(node->completion,node->adapter)){
            xrtSetErrorKind(XERR_STATE);return XL6_OWNERSHIP_PLAN_ERROR;
        }
        if(xl6OwnershipCompletionV2Get(node->completion))checked=true;
    }
    if(plan->committed){
        for(size_t i=0;i<plan->count;++i)if(plan->nodes[i].claimed && !plan->nodes[i].finished){xrtSetErrorKind(XERR_STATE);return XL6_OWNERSHIP_PLAN_ERROR;}
    }
    if(!xl6CodeLeaseReleaseScopeBeginV1(&releases))return XL6_OWNERSHIP_PLAN_ERROR;
    if(checked && !xl6ErrorScopeBegin(&errors)){
        xl6CodeLeaseReleaseScopeEndV1(&releases);return XL6_OWNERSHIP_PLAN_ERROR;
    }
    if(!plan->committed || checked){
        if(!freeze_begin(plan->boundary,&freeze)){
            xl6CodeLeaseReleaseScopeEndV1(&releases);
            if(checked)publish_drop_failure(&errors);
            return XL6_OWNERSHIP_PLAN_BUSY;
        }
        /* Pure admission of the WHOLE batch precedes any restore or release.
         * The plan can have moved threads since creation, and an aborted
         * snapshot can hold the last creator-affine native reference. */
        for(size_t i=0;i<plan->count;++i){
            PlanNode* node=&plan->nodes[i];
            const xrtownershipfinishv2* v2=xl6OwnershipCompletionV2Get(node->completion);
            if(v2 && !v2->DropReady(node->ref.Data)){
                freeze_end(plan->boundary,&freeze);
                xl6CodeLeaseReleaseScopeEndV1(&releases);
                publish_drop_failure(&errors);return XL6_OWNERSHIP_PLAN_BUSY;
            }
        }
        if(!plan->committed)
            for(size_t i=0;i<plan->count;++i){PlanNode* node=&plan->nodes[i];if(node->claimed)node->adapter->Restore(node->ref.Data,plan);}
        freeze_end(plan->boundary,&freeze);
    }
    plan->running=true;
    for(size_t i=plan->count;i--!=0;){
        PlanNode* node=&plan->nodes[i];
        const xrtownershipfinishv2* v2=xl6OwnershipCompletionV2Get(node->completion);
        if(v2){
            /* Both outcomes consume this one real snapshot reference. Never
             * retry it or follow it with the legacy void Drop. */
            if(!v2->DropChecked(node->ref.Data))xl6ErrorScopeCaptureFailure(&errors);
        }else node->adapter->Drop(node->ref.Data);
        if(checked && xrtGetError())xl6ErrorScopeCaptureFailure(&errors);
    }
    /* Host release can reenter retirement while the caller still refers to
     * this plan. Keep its running guard and storage alive through every such
     * release; no native table is read after ending the physical drop batch. */
    xl6CodeLeaseReleaseScopeEndV1(&releases);
    if(checked)publish_drop_failure(&errors);
    xrtFree(plan->nodes);xrtFree(plan);return XL6_OWNERSHIP_PLAN_READY;
}
