/* Real resident Class/lease/Stack storage; the native owner models physical
 * acquire/release/pin counts. Actual compiler/TCC module gates are separate. */
#include "src/runtime/xl6_stack.h"
#include "test/support/stack_take_found.h"
#include "src/runtime/xl6_sequence.h"
#include "src/runtime/xl6_code_lease.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REF_ARGS UINT64_C(102),UINT64_C(202),3,sizeof(xvalue*),xrtTypeValue()
typedef struct Owner {
    XL6_CodeOwnerGraph graph;
    size_t acquisitions, pins;
    bool closed, cleared;
    const void* claim;
    unsigned finalizers, discoveries;
    bool spawn, resurrect, refuse;
    xvalue* alias;
    xvalue* peer; /* Borrowed live identity, independently pinned by the plan. */
} Owner;
static Owner* scenario;
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(!xrtGetError() && before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount);
}
static int acquire(void* data)
{ Owner* owner=data;if(owner->closed)return 0;++owner->acquisitions;return 1; }
static void release(void* data)
{ Owner* owner=data;assert(owner->acquisitions);--owner->acquisitions; }
static bool owner_count(const void* data,size_t* count)
{
    const Owner* owner=data;
    if(owner->cleared || xl6CodeOwnerScopeActive(&owner->graph.base))return false;
    *count=owner->acquisitions+owner->pins+!owner->closed;return true;
}
static bool owner_trace(const void* data,xrtownershipvisitor visit,void* context)
{ (void)data;(void)visit;(void)context;return true; }
static const xrtownershipops owner_ops={owner_count,owner_trace};
static bool owner_hold(const void* data)
{ Owner* owner=(Owner*)data;assert(!owner->cleared);++owner->pins;return true; }
static void owner_drop(const void* data)
{ Owner* owner=(Owner*)data;assert(owner->pins);--owner->pins; }
static bool owner_claim(const void* data,const void* token)
{ Owner* owner=(Owner*)data;if(owner->cleared || (owner->claim && owner->claim!=token))return false;owner->claim=token;return true; }
static void owner_restore(const void* data,const void* token)
{ Owner* owner=(Owner*)data;assert(owner->claim==token && !owner->cleared);owner->claim=NULL; }
static void owner_clear(const void* data,const void* token)
{ Owner* owner=(Owner*)data;assert(owner->claim==token && !owner->cleared);owner->cleared=true; }
static const xrtownershipadapterv1 owner_adapter={sizeof(owner_adapter),owner_hold,owner_drop,
    owner_claim,owner_restore,NULL,owner_clear,NULL};
static void initialize(Owner* owner)
{
    memset(owner,0,sizeof(*owner));
    owner->graph=(XL6_CodeOwnerGraph){{sizeof(owner->graph),owner,acquire,release},0x584C36434F574E31ULL,&owner_ops};
}
static bool resolve(xrtownershipref ref,void* context,const xrtownershipadapterv1** adapter,
    const xrtownershippreparationv1** preparation,const XL6_OwnershipFinishV1** completion)
{
    (void)context;*preparation=NULL;*completion=NULL;
    if(ref.Ops==&owner_ops){size_t count;if(!owner_count(ref.Data,&count))return false;*adapter=&owner_adapter;return true;}
    *adapter=xl6ClassOwnershipAdapterV1(ref);
    if(!*adapter)*adapter=xl6CodeLeaseOwnershipAdapterV1(ref);
    if(!*adapter)*adapter=xl6SequenceOwnershipAdapterV1(ref);
    if(!*adapter)*adapter=xrtValueOwnershipAdapterV1(ref);
    return *adapter!=NULL;
}
static bool discover(XL6_ModuleOwnershipVisit visit,void* visitorContext,void* data)
{
    Owner* owner=data;size_t count;
    if(!owner_count(owner,&count))return false;
    ++owner->discoveries;
    if(owner->refuse){xrtSetErrorKind(XERR_RANGE);return false;}
    return xl6ClassOwnershipDiscoverV1(&owner->graph.base,visit,visitorContext);
}
static xvalue* create_cycle(Owner*,int64_t,bool);
static bool finalize(xvalue* object,void* unused)
{
    (void)unused;Owner* owner=scenario;int64 id=0;
    assert(owner && xrtValueGetInt(xrtValueObjectGet(object,XRT_STR_LITERAL("id")),&id));
    xvalue* stack=xrtValueObjectGet(object,XRT_STR_LITERAL("links"));
    assert(stack && xl6StackSizeV1(stack,REF_ARGS,false)==1 && xrtValueCount(object)==2);
    xrtownershipscope freeze={0};assert(xrtOwnershipFreezeTryBegin(&freeze) && xrtOwnershipScopeEnd(&freeze));
    ++owner->finalizers;
    if(id==1 && owner->spawn){
        owner->spawn=false;
        /* Exact accepted callback capability may be reused after Close. The
         * new island has no outgoing path from the old module/plan nodes. */
        xvalue* child=create_cycle(owner,2,true);xrtValueRelease(child);
    }
    if(id==1 && owner->resurrect){
        /* XRT deliberately forbids retaining the borrowed finalizing receiver.
         * Restore a DIFFERENT real, nonfinalizing class island instead. */
        owner->resurrect=false;assert(!owner->alias && owner->peer);
        owner->alias=xrtValueRetain(owner->peer);assert(owner->alias);
    }
    return true;
}
static xvalue* create_cycle(Owner* owner,int64_t id,bool withFinalizer)
{
    const XL6_ClassRuntimeV2* api=xl6ClassRuntimeV2Get();
    xvalue* object=api->Create(&owner->graph.base,withFinalizer?finalize:NULL);
    assert(object && xrtValueObjectSetNew(object,XRT_STR_LITERAL("id"),xrtValueInt(id)));
    xvalue* stack=xl6StackCreateV1(REF_ARGS);assert(stack);
    assert(xrtValueObjectSetNew(object,XRT_STR_LITERAL("links"),stack));
    assert(api->CommitTake(object)==object);
    assert(xl6StackPushV1(stack,REF_ARGS,&object));return object;
}
static XL6_OwnershipPlanResult create_plan(Owner* owner,XL6_OwnershipPlan** plan)
{ return xl6OwnershipPlanTryCreateV4((xrtownershipref){owner,&owner_ops},resolve,discover,owner,plan); }
static void finish_plan(Owner* owner,XL6_OwnershipPlan* plan)
{
    for(unsigned turn=0;turn<32;++turn){
        XL6_OwnershipStepResult finalized=xl6OwnershipPlanFinalizeV2(plan);
        assert(finalized>=XL6_OWNERSHIP_STEP_BUSY);
        XL6_OwnershipPlanResult committed=xl6OwnershipPlanTryCommit(plan);
        if(committed==XL6_OWNERSHIP_PLAN_FINALIZERS)continue;
        assert(committed==XL6_OWNERSHIP_PLAN_READY);
        assert(xl6OwnershipPlanFinish(plan) && xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
        assert(owner->cleared && !owner->pins && !owner->acquisitions);return;
    }
    assert(!"bounded discovery finalization did not finish");
}
static bool count_anchor(xrtownershipref ref,void* data)
{ size_t* count=data;assert(ref.Data && ref.Ops);++*count;return true; }
static size_t anchors(Owner* owner)
{
    xrtownershipscope freeze={0};size_t count=0;assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(xl6ClassOwnershipDiscoverV1(&owner->graph.base,count_anchor,&count));
    assert(xrtOwnershipScopeEnd(&freeze));return count;
}
static void vector(unsigned mode)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Owner owner,foreign;
    initialize(&owner);initialize(&foreign);scenario=&owner;
    xvalue* value=create_cycle(&owner,1,true);
    xvalue* separate=create_cycle(&foreign,3,false);
    xvalue* weak=xrtValueWeakRef(value);assert(weak);
    assert(anchors(&owner)==1 && anchors(&foreign)==1);
    xvalue* alias=mode==1?xrtValueRetain(value):mode==2?xrtValueDeepClone(value):NULL;
    if(mode==2)assert(alias && alias!=value && anchors(&owner)==2);
    owner.spawn=mode==3;owner.resurrect=mode==4;
    if(mode==4){owner.peer=create_cycle(&owner,2,false);xrtValueRelease(owner.peer);}
    xrtValueRelease(value);owner.closed=true;
    XL6_OwnershipPlan* plan=NULL;XL6_OwnershipPlanResult result=create_plan(&owner,&plan);
    assert(plan && result==(alias?XL6_OWNERSHIP_PLAN_ROOTED:XL6_OWNERSHIP_PLAN_FINALIZERS));
    if(alias){
        assert(!owner.finalizers && !owner.cleared && xrtValueCount(alias)==2);
        assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
        xrtValueRelease(alias);plan=NULL;assert(create_plan(&owner,&plan)==XL6_OWNERSHIP_PLAN_FINALIZERS);
    }
    if(mode==4){
        /* Bounded Finalize may stop at its last duty before observing the
         * new root. Commit must freshly discover it and preserve all fields. */
        assert(xl6OwnershipPlanFinalizeV2(plan)>=XL6_OWNERSHIP_STEP_BUSY);
        assert(owner.alias && owner.finalizers==1 && xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_ROOTED);
        assert(xrtValueCount(owner.alias)==2 && xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
        xrtValueRelease(owner.alias);owner.alias=NULL;plan=NULL;assert(create_plan(&owner,&plan)>XL6_OWNERSHIP_PLAN_BUSY);
    }
    finish_plan(&owner,plan);assert(owner.finalizers==(mode==3?2u:1u) && owner.discoveries>=3);
    assert(!anchors(&owner) && anchors(&foreign)==1 && !foreign.finalizers && foreign.acquisitions==1);
    xvalue* expired=xrtValueWeakRefLock(weak);assert(expired && xrtValueType(expired)==XVALUE_NULL);
    xrtValueRelease(expired);xrtValueRelease(weak);
    /* Independent owner must remain intact and independently collectable. */
    xrtValueRelease(separate);foreign.closed=true;plan=NULL;
    /* Physical adapters still have a lifecycle step, even when the class has
     * no semantic destructor. That step must not fabricate a user callback. */
    assert(create_plan(&foreign,&plan)==XL6_OWNERSHIP_PLAN_FINALIZERS);finish_plan(&foreign,plan);
    assert(!anchors(&foreign));scenario=NULL;balanced(&before);
}
static void identity(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Owner owner;initialize(&owner);
    XL6_CodeLease* lease=NULL;assert(xl6CodeLeaseAcquire(&owner.graph.base,&lease)==XL6_CODE_LEASE_ACQUIRED);
    XL6_CodeOwnerGraph wrong=owner.graph;wrong.ops=NULL;
    assert(!xl6CodeLeaseOwnerMatches(lease,&wrong.base));wrong=owner.graph;wrong.base.context=&wrong;
    assert(!xl6CodeLeaseOwnerMatches(lease,&wrong.base));wrong=owner.graph;wrong.base.acquire=NULL;
    assert(!xl6CodeLeaseOwnerMatches(lease,&wrong.base));wrong=owner.graph;wrong.base.release=NULL;
    assert(!xl6CodeLeaseOwnerMatches(lease,&wrong.base));wrong=owner.graph;wrong.abiTag=0;
    assert(!xl6CodeLeaseOwnerMatches(lease,&wrong.base));
    assert(xl6CodeLeaseOwnerMatches(lease,&owner.graph.base));owner.closed=true;
    assert(xl6CodeLeaseOwnerMatches(lease,&owner.graph.base));xl6CodeLeaseRelease(lease);
    assert(!owner.acquisitions);balanced(&before);
}
static void copied_source_ends(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Owner owner;initialize(&owner);
    xvalue* source=create_cycle(&owner,1,false);xvalue* slot=NULL;
    xvalue* stack=xrtValueObjectGet(source,XRT_STR_LITERAL("links"));
    assert(xl6StackFoundTest(stack,REF_ARGS,0,&slot,true) && slot==source);
    xrtValueRelease(slot);
    xvalue* copy=xrtValueDeepClone(source);assert(copy && copy!=source && anchors(&owner)==2);
    xrtValueRelease(source);assert(anchors(&owner)==1 && owner.acquisitions==1);
    stack=xrtValueObjectGet(copy,XRT_STR_LITERAL("links"));assert(xl6StackPushV1(stack,REF_ARGS,&copy));
    xrtValueRelease(copy);owner.closed=true;
    XL6_OwnershipPlan* plan=NULL;assert(create_plan(&owner,&plan)==XL6_OWNERSHIP_PLAN_FINALIZERS);
    finish_plan(&owner,plan);assert(!owner.finalizers && !anchors(&owner));balanced(&before);
}
static void refusal(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Owner owner;initialize(&owner);scenario=&owner;
    xvalue* value=create_cycle(&owner,1,true);xrtValueRelease(value);owner.closed=true;owner.refuse=true;
    XL6_OwnershipPlan* plan=(XL6_OwnershipPlan*)(uintptr_t)1;
    assert(create_plan(&owner,&plan)==XL6_OWNERSHIP_PLAN_ERROR && plan==(XL6_OwnershipPlan*)(uintptr_t)1);
    assert(xrtErrorKind(xrtGetError())==XERR_RANGE && !owner.pins && !owner.finalizers);xrtClearError();
    owner.refuse=false;plan=NULL;assert(create_plan(&owner,&plan)==XL6_OWNERSHIP_PLAN_FINALIZERS);
    owner.refuse=true;assert(xl6OwnershipPlanTryRefresh(plan)==XL6_OWNERSHIP_PLAN_ERROR);
    assert(xrtErrorKind(xrtGetError())==XERR_RANGE && !owner.finalizers && !owner.cleared);xrtClearError();
    owner.refuse=false;finish_plan(&owner,plan);assert(owner.finalizers==1);scenario=NULL;balanced(&before);
}
static int fault(const char* stage,size_t point)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Owner owner;initialize(&owner);scenario=&owner;
    /* Force partial discovery-array growth, not just snapshot allocations. */
    for(int64_t id=1;id<=20;++id){xvalue* value=create_cycle(&owner,id,true);xrtValueRelease(value);}
    owner.closed=true;XL6_OwnershipPlan* plan=NULL;
    if(strcmp(stage,"create"))assert(create_plan(&owner,&plan)==XL6_OWNERSHIP_PLAN_FINALIZERS);
    if(!strcmp(stage,"commit"))assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY);
    xmemdebugsnapshot start;xrtMemDebugSnapshot(&start);unsigned calls=owner.finalizers;
    assert(xrtMemDebugFailAfter(point));XL6_OwnershipPlanResult result;
    if(!strcmp(stage,"create"))result=create_plan(&owner,&plan);
    else if(!strcmp(stage,"refresh"))result=xl6OwnershipPlanTryRefresh(plan);
    else {assert(!strcmp(stage,"commit"));result=xl6OwnershipPlanTryCommit(plan);}
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    if(hit){assert(result==XL6_OWNERSHIP_PLAN_ERROR && xrtErrorKind(xrtGetError())==XERR_MEMORY);
        assert(owner.finalizers==calls && !owner.cleared);xrtClearError();balanced(&start);}
    else assert(result==(!strcmp(stage,"commit")?XL6_OWNERSHIP_PLAN_READY:XL6_OWNERSHIP_PLAN_FINALIZERS));
    if(!plan)assert(create_plan(&owner,&plan)==XL6_OWNERSHIP_PLAN_FINALIZERS);
    if(owner.cleared){assert(xl6OwnershipPlanFinish(plan) && xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);}
    else finish_plan(&owner,plan);
    assert(owner.finalizers==20 && !owner.acquisitions && !owner.pins && !anchors(&owner));
    scenario=NULL;balanced(&before);printf("discovery %s point=%zu hit=%d balanced\n",stage,point,hit);return 0;
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true));
    if(argc==3)return fault(argv[1],(size_t)strtoull(argv[2],NULL,10));
    assert(argc==1);identity();refusal();copied_source_ends();
    for(unsigned round=0;round<50;++round)for(unsigned mode=0;mode<5;++mode)vector(mode);
    puts("Ownership discovery: 250 actual class/Stack island scenarios, foreign owners, rooted alias/deep-copy aborts, new finalizer islands and peer resurrection; original creation shell retired before copied island, real code leases, intact once-only finalizers and exact ledger");return 0;
}
