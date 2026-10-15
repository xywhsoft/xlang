/* Exercise native completion descriptors through the actual language plan,
 * not through layout-compatible copies or function-pointer casts. */
#define XRT_MODULE_CANCEL
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ATOMIC
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <stdio.h>

_Static_assert(_Generic((const xrtownershipfinishv1*)0,
    const XL6_OwnershipFinishV1*:1, default:0), "one canonical completion ABI");

typedef struct Events { unsigned drops, notifications, finalizers, clears; } Events;
typedef struct Owner {
    volatile int32 refs;
    xcancel* cancel;
    xcancelwatch* watch;
    const void* claim;
    Events* events;
    xrtownershipfinishresult outcome;
    bool cleared, watchFirst;
} Owner;
static const xrtownershipops ownerOps;
static void end(xrtownershipscope* scope) { assert(xrtOwnershipScopeEnd(scope)); }
static void begin(xrtownershipscope* scope) { assert(xrtOwnershipMutationBegin(scope)); }
static xrtownershipref ownerView(Owner* owner) { return (xrtownershipref){owner,&ownerOps}; }
static bool ownerCount(const void* data,size_t* count)
{
    const Owner* owner=data;
    if(!owner || !count || owner->cleared || owner->refs<=0)return false;
    *count=(size_t)owner->refs;return true;
}
static bool ownerTrace(const void* data,xrtownershipvisitor visit,ptr context)
{
    const Owner* owner=data;
    xrtownershipref refs[2]={xrtCancelOwnership(owner->cancel),xrtCancelWatchOwnership(owner->watch)};
    unsigned first=owner->watchFirst?1:0;
    return visit(refs[first],context) && visit(refs[1-first],context);
}
static const xrtownershipops ownerOps={ownerCount,ownerTrace};
static bool ownerHold(const void* data)
{ Owner* owner=(Owner*)data;return !owner->cleared && xrtRefRetain(&owner->refs)>0; }
static void ownerDrop(const void* data)
{
    Owner* owner=(Owner*)data;xrtownershipscope mutation={0};begin(&mutation);
    int32 remaining=xrtRefRelease(&owner->refs);assert(remaining>=0);
    if(!remaining){assert(!owner->cancel && !owner->watch);++owner->events->drops;xrtFree(owner);}
    end(&mutation);
}
static void notify(ptr data)
{ Owner* owner=data;assert(!owner->cleared);++owner->events->notifications; }
static const xcancelwatchownershipv1 watchPolicy={sizeof(watchPolicy),notify,ownerDrop,&ownerOps};
static bool ownerClaim(const void* data,const void* token)
{
    Owner* owner=(Owner*)data;
    if(!token || owner->cleared || (owner->claim && owner->claim!=token))return false;
    owner->claim=token;return true;
}
static void ownerRestore(const void* data,const void* token)
{ Owner* owner=(Owner*)data;assert(owner->claim==token && !owner->cleared);owner->claim=NULL; }
static bool ownerFinalize(const void* data,const void* token)
{
    Owner* owner=(Owner*)data;
    assert(owner->claim==token && !owner->cleared && owner->cancel && owner->watch);
    assert(!xrtCancelRequested(owner->cancel));++owner->events->finalizers;return true;
}
static void ownerClear(const void* data,const void* token)
{
    Owner* owner=(Owner*)data;
    assert(owner->claim==token && !owner->cleared && owner->events->finalizers==1);
    assert(owner->watch && owner->cancel);owner->cleared=true;++owner->events->clears;
}
static void detach(Owner* owner)
{
    xrtownershipscope mutation={0};begin(&mutation);
    xcancelwatch* watch=owner->watch;xcancel* cancel=owner->cancel;
    owner->watch=NULL;owner->cancel=NULL;end(&mutation);
    /* The registration owner's one real right, not a collector Hold. The
     * owner itself can die here on an aborted plan, so do not touch it again. */
    xrtCancelUnwatch(watch);xrtCancelDestroy(cancel);
}
static xrtownershipfinishresult ownerFinish(const void* data,const void* token)
{
    Owner* owner=(Owner*)data;assert(owner->claim==token && owner->cleared);
    if(owner->outcome!=XRT_OWNERSHIP_FINISH_READY)return owner->outcome;
    detach(owner);return XRT_OWNERSHIP_FINISH_READY;
}
static const xrtownershipadapterv1 ownerAdapter={sizeof(ownerAdapter),ownerHold,ownerDrop,
    ownerClaim,ownerRestore,ownerFinalize,ownerClear,NULL};
static const xrtownershipfinishv1 ownerCompletion={sizeof(ownerCompletion),&ownerAdapter,ownerFinish};
static bool resolve(xrtownershipref ref,void* context,const XL6_OwnershipAdapterV1** adapter,
    const xrtownershippreparationv1** preparation,const XL6_OwnershipFinishV1** completion)
{
    (void)context;*preparation=NULL;
    if(ref.Ops==&ownerOps){*adapter=&ownerAdapter;*completion=&ownerCompletion;return true;}
    const xcancelwatchownershipv1* policies[1]={&watchPolicy};
    *adapter=xrtCancelWatchOwnershipAdapterV2(ref,policies,1,completion);
    if(!*adapter)*adapter=xrtCancelOwnershipAdapterV3(ref,policies,1,completion);
    return *adapter!=NULL;
}
static Owner* make(Events* events,xcancel* parent,bool watchFirst)
{
    Owner* owner=xrtCalloc(1,sizeof(*owner));assert(owner);
    owner->refs=1;owner->events=events;owner->watchFirst=watchFirst;
    owner->outcome=XRT_OWNERSHIP_FINISH_BUSY;owner->cancel=xrtCancelChild(parent);assert(owner->cancel);
    owner->watch=xrtCancelWatchOwnedV1(owner->cancel,owner,&watchPolicy);assert(owner->watch);
    /* The initial Data right transferred to Watch; this borrowed anchor is
     * held alive by the genuine Owner -> Watch -> Owner physical cycle. */
    return owner;
}
static void inspect(Owner* owner,bool parent)
{
    xrtownershipscope freeze={0};xrtownershipresult result={0};xrtownershipref root=ownerView(owner);
    assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(xrtOwnershipInspect(&root,1,NULL,0,&result));
    assert(result.NodeCount==(parent?4u:3u) && result.EdgeCount==(parent?5u:4u));
    assert(result.ExternalRootCount==(parent?1u:0u));end(&freeze);
}
static void cycle(bool watchFirst,bool externalParent,bool restore)
{
    Events events={0};xcancel* parent=externalParent?xrtCancelCreate():NULL;assert(!externalParent || parent);
    Owner* owner=make(&events,parent,watchFirst);inspect(owner,externalParent);
    XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreateV3(ownerView(owner),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_FINALIZERS);
    if(restore){
        assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
        assert(!owner->claim && !owner->cleared && !events.finalizers && !events.clears);
        assert(xrtCancelRequest(parent?parent:owner->cancel) && events.notifications==1);
        detach(owner);assert(events.drops==1);xrtCancelDestroy(parent);return;
    }
    assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(events.finalizers==1 && !events.clears && !events.notifications);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(events.clears==1 && owner->cancel && owner->watch && !events.drops);
    xrtSetErrorKind(XERR_STATE);const xerror* primary=xrtGetError();assert(primary);
    assert(xrtMemDebugFailAfter(0));
    for(unsigned i=0;i<8;++i){
        assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_BUSY);
        assert(owner->cancel && owner->watch && !events.drops && !events.notifications);
        assert(xrtGetError()==primary && !xrtMemDebugFailTriggered());
    }
    xrtMemDebugFailClear();xrtClearError();
    /* A callback failure is explicit even when no xerror can be allocated. */
    owner->outcome=XRT_OWNERSHIP_FINISH_FAILED;assert(xrtMemDebugFailAfter(0));
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_FAILED);
    assert(!xrtGetError() && !xrtMemDebugFailTriggered() && !events.drops);
    xrtMemDebugFailClear();
    if(parent)assert(xrtCancelRequest(parent) && !events.notifications);
    owner->outcome=XRT_OWNERSHIP_FINISH_READY;
    assert(xrtMemDebugFailAfter(0));
    XL6_OwnershipStepResult result;
    unsigned turns=0;
    do{result=xl6OwnershipPlanFinishV2(plan);assert(result>=XL6_OWNERSHIP_STEP_BUSY && ++turns<=3);}
    while(result==XL6_OWNERSHIP_STEP_BUSY);
    assert(!xrtGetError() && !xrtMemDebugFailTriggered() && !owner->watch && !owner->cancel);
    assert(!events.drops && xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    xrtMemDebugFailClear();assert(events.drops==1 && !events.notifications);xrtCancelDestroy(parent);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    for(unsigned repeat=0;repeat<25;++repeat)
        for(unsigned flags=0;flags<8;++flags)cycle((flags&1)!=0,(flags&2)!=0,(flags&4)!=0);
    xrtMemDebugSnapshot(&after);
    assert(!xrtGetError() && before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
        before.UseAfterFreeCount==after.UseAfterFreeCount);
    puts("Native completion ABI: 200 physical cycles, 800 BUSY turns, 100 diagnostic-free failures, 100 restored registrations; exact native descriptors, all pins retained, zero live delta");
    return 0;
}
