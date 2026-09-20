#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE_CONTAINER
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <stdio.h>
#include <windows.h>

/* The native owner has one actual self-owning slot and two real COW shells.
 * No fake refcounts, tracing roots, or direct access to XRT private layouts. */
typedef struct Owner {
    volatile int32 refs;
    struct Owner* self;
    xvalue *first,*second;
    const void* claim;
    bool cleared;
} Owner;
static unsigned retired,transactions,promotions,refusedPromotions;
static bool hold(const void* data){return xrtRefRetain(&((Owner*)data)->refs)>0;}
static void drop(const void* data)
{
    Owner* owner=(Owner*)data;Owner* self;xvalue *first,*second;
    xrtownershipscope mutation={0};if(!owner)return;
    assert(xrtOwnershipMutationBegin(&mutation));int32 refs=xrtRefRelease(&owner->refs);assert(refs>=0);
    if(refs){assert(xrtOwnershipScopeEnd(&mutation));return;}
    self=owner->self;first=owner->first;second=owner->second;
    xrtFree(owner);++retired;assert(xrtOwnershipScopeEnd(&mutation));
    drop(self);xrtValueRelease(second);xrtValueRelease(first);
}
static bool count(const void* data,size_t* result)
{const Owner* owner=data;if(owner->refs<=0 || owner->cleared)return false;*result=(size_t)owner->refs;return true;}
static bool trace(const void*,xrtownershipvisitor,void*);
static const xrtownershipops ops={count,trace};
static xrtownershipref view(const Owner* owner){return (xrtownershipref){owner,owner?&ops:NULL};}
static bool trace(const void* data,xrtownershipvisitor visit,void* context)
{const Owner* owner=data;size_t refs;return count(data,&refs) && visit(view(owner->self),context) &&
    visit(xrtValueOwnership(owner->first),context) && visit(xrtValueOwnership(owner->second),context);}
static bool claim(const void* data,const void* token)
{Owner* owner=(Owner*)data;if(owner->claim && owner->claim!=token)return false;owner->claim=token;return true;}
static void restore(const void* data,const void* token)
{Owner* owner=(Owner*)data;assert(owner->claim==token && !owner->cleared);owner->claim=NULL;}
static void clear(const void* data,const void* token)
{
    Owner* owner=(Owner*)data;Owner* self=owner->self;xvalue *first=owner->first,*second=owner->second;
    assert(owner->claim==token && !owner->cleared);owner->self=NULL;owner->first=owner->second=NULL;owner->cleared=true;
    drop(self);xrtValueRelease(second);xrtValueRelease(first);
}
static const XL6_OwnershipAdapterV1 adapter={sizeof(adapter),hold,drop,claim,restore,NULL,clear,NULL};
static const XL6_OwnershipAdapterV1* resolve(xrtownershipref ref,void* context)
{(void)context;return ref.Ops==&ops?&adapter:xrtValueOwnershipAdapterV1(ref);}
static Owner* make(unsigned kind,xvalue** weak)
{
    Owner* owner=xrtCalloc(1,sizeof(*owner));assert(owner);owner->refs=1;
    xvalue* scalar=xrtValueInt(42);assert(scalar);*weak=xrtValueWeakRef(scalar);assert(*weak);
    switch(kind){
    case 0:owner->first=xrtValueArray();assert(owner->first);
        assert(xrtValueArrayAppendNew(owner->first,xrtValueRetain(scalar)) &&
            xrtValueArrayAppendNew(owner->first,xrtValueRetain(scalar)));break;
    case 1:owner->first=xrtValueIntMap();assert(owner->first);
        assert(xrtValueIntMapSetNew(owner->first,1,xrtValueRetain(scalar)) &&
            xrtValueIntMapSetNew(owner->first,2,xrtValueRetain(scalar)));break;
    case 2:owner->first=xrtValueObject();assert(owner->first);
        assert(xrtValueObjectSetNew(owner->first,XRT_STR_LITERAL("a"),xrtValueRetain(scalar)) &&
            xrtValueObjectSetNew(owner->first,XRT_STR_LITERAL("b"),xrtValueRetain(scalar)));break;
    default:owner->first=xrtValueSet();assert(owner->first);
        assert(xrtValueSetAddNew(owner->first,xrtValueRetain(scalar)));break;
    }
    owner->second=xrtValueClone(owner->first);assert(owner->second && owner->first!=owner->second);
    xrtValueRelease(scalar);assert(hold(owner));owner->self=owner;drop(owner);return owner;
}
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
}
static void check_value(xvalue* weak,bool available)
{
    int64 integer=0;xvalue* value=xrtValueWeakRefLock(weak);assert(value);
    if(available){assert(xrtValueType(value)==XVALUE_INT && xrtValueGetInt(value,&integer) && integer==42);++promotions;}
    else{assert(xrtValueType(value)==XVALUE_NULL);++refusedPromotions;}
    xrtValueRelease(value);
}
static DWORD WINAPI denied_worker(void* weak)
{for(unsigned i=0;i<100;++i)check_value(weak,false);return 0;}
static void end_plan(XL6_OwnershipPlan* plan)
{assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);}
static void transaction(unsigned kind,unsigned mode)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);xvalue* weak=NULL;
    Owner* owner=make(kind,&weak);XL6_OwnershipPlan* plan=NULL;xvalue* external=NULL;
    if(mode==1){external=xrtValueClone(owner->first);assert(external);}
    if(mode==2)assert(hold(owner));
    XL6_OwnershipPlanResult ready=xl6OwnershipPlanTryCreate(view(owner),resolve,NULL,&plan);
    assert(ready==(mode==2?XL6_OWNERSHIP_PLAN_ROOTED:XL6_OWNERSHIP_PLAN_READY));
    check_value(weak,mode==1 || mode==2);assert(!xrtValueWeakRefExpired(weak));
    if(mode==2 || mode==3){
        xrtownershipscope busy={0};assert(xrtOwnershipMutationBegin(&busy));
        assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_BUSY && plan);
        assert(xrtOwnershipScopeEnd(&busy));end_plan(plan);plan=NULL;
        check_value(weak,true);assert(owner->self==owner && !owner->claim);
        if(mode==2)drop(owner);
        assert(xl6OwnershipPlanTryCreate(view(owner),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_READY);
    }
    if(mode==0){
        HANDLE thread=CreateThread(NULL,0,denied_worker,weak,0,NULL);
        assert(thread && WaitForSingleObject(thread,3000)==WAIT_OBJECT_0 && CloseHandle(thread));
    }
    /* No allocation or foreign finalizer can occur in physical field clear;
     * preparation still allocates and is separately fault-swept below. */
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanFinish(plan));end_plan(plan);
    if(external){
        xvalueiter iterator={0};xvalue* item=NULL;unsigned items=0;
        assert(xrtValueIterBegin(external,&iterator));
        while(xrtValueIterAdvance(&iterator,NULL,&item)==XVALUE_ITER_ITEM){int64 integer=0;assert(xrtValueGetInt(item,&integer) && integer==42);++items;}
        xrtValueIterEnd(&iterator);assert(items==(kind==3?1u:2u));check_value(weak,true);xrtValueRelease(external);
    }
    assert(xrtValueWeakRefExpired(weak));check_value(weak,false);xrtValueRelease(weak);++transactions;balance(&before);
}
static unsigned fault_sweep(void)
{
    unsigned faults=0;
    for(unsigned stage=0;stage<3;++stage)for(unsigned kind=0;kind<4;++kind)for(unsigned point=0;point<64;++point){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);xvalue* weak=NULL;
        Owner* owner=make(kind,&weak);XL6_OwnershipPlan* plan=NULL;
        if(stage)assert(xl6OwnershipPlanTryCreate(view(owner),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_READY);
        assert(xrtMemDebugFailAfter(point));XL6_OwnershipPlanResult result=stage==0?
            xl6OwnershipPlanTryCreate(view(owner),resolve,NULL,&plan):stage==1?
            xl6OwnershipPlanTryRefresh(plan):xl6OwnershipPlanTryCommit(plan);
        bool failed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(failed){
            assert(result==XL6_OWNERSHIP_PLAN_ERROR && !owner->cleared && owner->self==owner);++faults;
            check_value(weak,stage==0);xrtClearError();
        }else assert(result==XL6_OWNERSHIP_PLAN_READY);
        if(stage==2 && !failed){assert(xl6OwnershipPlanFinish(plan));end_plan(plan);}
        else{
            if(plan)end_plan(plan);
            plan=NULL;check_value(weak,true);
            assert(xl6OwnershipPlanTryCreate(view(owner),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_READY);
            assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY && xl6OwnershipPlanFinish(plan));end_plan(plan);
        }
        assert(xrtValueWeakRefExpired(weak));xrtValueRelease(weak);balance(&before);
        if(!failed)break;
        assert(point<63);
    }
    return faults;
}
int main(void)
{
    xmemdebugsnapshot before;assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();xrtMemDebugSnapshot(&before);
    for(unsigned i=0;i<100;++i)for(unsigned kind=0;kind<4;++kind)for(unsigned mode=0;mode<4;++mode)transaction(kind,mode);
    unsigned faults=fault_sweep();balance(&before);assert(transactions==1600 && faults>0 && refusedPromotions>=40000);
    printf("Value ownership plans: 1600 physical Array/IntMap/Object/Set COW transactions, 40000 concurrent quarantined promotions, %u preparation/refresh/commit faults, %u real native owners retired; zero live delta; custom policies remain refused\n",faults,retired);return 0;
}
