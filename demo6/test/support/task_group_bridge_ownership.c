#define XRT_MODULE_TASK_GROUP
#define XRT_MODULE_THREAD
#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include "src/api/xl_task_group_bridge.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <stdio.h>

typedef struct Owner {
    volatile int32 refs;
    struct Owner* self;
    xl6_task_group *group,*child;
    xfuture *parent,*source;
    xpromise *parentPromise,*sourcePromise;
    const void* claim;
    unsigned* dropped;
    bool cleared;
} Owner;
static const xrtownershipops ownerOps;
static xrtownershipref view(Owner* owner) { return (xrtownershipref){owner,&ownerOps}; }
static void end(xrtownershipscope* scope) { assert(xrtOwnershipScopeEnd(scope)); }
static void begin(xrtownershipscope* scope) { assert(xrtOwnershipMutationBegin(scope)); }
static bool count(const void* data,size_t* count)
{ const Owner* owner=data;if(!owner || owner->cleared || owner->refs<=0)return false;*count=(size_t)owner->refs;return true; }
static bool trace(const void* data,xrtownershipvisitor visit,ptr context)
{
    const Owner* owner=data;
    return visit(view(owner->self),context) && visit(xl6TaskGroupOwnershipV1(owner->group),context) &&
        (!owner->child || visit(xl6TaskGroupOwnershipV1(owner->child),context)) &&
        visit(xrtFutureOwnership(owner->parent),context) && visit(xrtPromiseOwnership(owner->parentPromise),context) &&
        (!owner->source || visit(xrtFutureOwnership(owner->source),context)) &&
        (!owner->sourcePromise || visit(xrtPromiseOwnership(owner->sourcePromise),context));
}
static const xrtownershipops ownerOps={count,trace};
static void drop(const void* data)
{
    Owner* owner=(Owner*)data;xrtownershipscope scope={0};begin(&scope);
    int32 refs=xrtRefRelease(&owner->refs);assert(refs>=0);
    if(!refs){assert(!owner->self && !owner->group && !owner->child && !owner->parent && !owner->source);
        ++*owner->dropped;xrtFree(owner);}
    end(&scope);
}
static bool hold(const void* data) { return xrtRefRetain(&((Owner*)data)->refs)>0; }
static bool claim(const void* data,const void* token)
{ Owner* owner=(Owner*)data;if(!token || owner->cleared || (owner->claim && owner->claim!=token))return false;owner->claim=token;return true; }
static void restore(const void* data,const void* token)
{ Owner* owner=(Owner*)data;assert(owner->claim==token && !owner->cleared);owner->claim=NULL; }
static bool finalize(const void* data,const void* token)
{
    const Owner* owner=data;assert(owner->claim==token && !owner->cleared);
    assert(xl6TaskGroupCount(owner->group)==(owner->child?1:owner->source?1:0));
    xcancel* cancel=xrtFutureCancelToken(owner->parent);assert(cancel && !xrtCancelRequested(cancel));xrtCancelDestroy(cancel);
    return true;
}
static void clear(const void* data,const void* token)
{ Owner* owner=(Owner*)data;assert(owner->claim==token && !owner->cleared);owner->cleared=true; }
static void dispose(Owner* owner)
{
    xrtownershipscope scope={0};begin(&scope);
    Owner* self=owner->self;xl6_task_group *group=owner->group,*child=owner->child;
    xfuture *parent=owner->parent,*source=owner->source;xpromise *parentPromise=owner->parentPromise,*sourcePromise=owner->sourcePromise;
    owner->self=NULL;owner->group=owner->child=NULL;owner->parent=owner->source=NULL;
    owner->parentPromise=owner->sourcePromise=NULL;end(&scope);
    xl6TaskGroupDestroy(child);xl6TaskGroupDestroy(group);
    xrtPromiseDestroy(sourcePromise);xrtFutureDestroy(source);xrtPromiseDestroy(parentPromise);xrtFutureDestroy(parent);drop(self);
}
static bool finish(const void* data,const void* token)
{ Owner* owner=(Owner*)data;assert(owner->claim==token && owner->cleared);dispose(owner);return true; }
static const xrtownershipadapterv1 ownerAdapter={sizeof(ownerAdapter),hold,drop,claim,restore,finalize,clear,finish};
static bool resolve(xrtownershipref ref,void* context,const xrtownershipadapterv1** adapter,
    const xrtownershippreparationv1** preparation,const xrtownershipfinishv1** completion)
{
    (void)context;*preparation=NULL;
    if(ref.Ops==&ownerOps){*adapter=&ownerAdapter;return true;}
    *adapter=xl6TaskGroupOwnershipAdapterV1(ref,completion);
    if(!*adapter)*adapter=xrtTaskGroupOwnershipAdapterV1(ref,completion);
    const xfuturewatchownershipv1* watches[]={xrtTaskGroupWatchPolicyV1Get()};
    const xcancelwatchownershipv1* cancels[]={xl6TaskGroupParentPolicyV1Get(),xrtTaskGroupCancelPolicyV1Get()};
    const xfutureownershipadmissionv1 admission={sizeof(admission),NULL,0,NULL,0,watches,1,NULL,0,cancels,2};
    if(!*adapter)*adapter=xrtFutureOwnershipAdapterV5(ref,&admission,completion);
    if(!*adapter)*adapter=xrtCancelWatchOwnershipAdapterV2(ref,cancels,2,completion);
    if(!*adapter)*adapter=xrtCancelOwnershipAdapterV3(ref,cancels,2,completion);
    return *adapter!=NULL;
}
static Owner* make(unsigned flags,unsigned* dropped)
{
    Owner* owner=xrtCalloc(1,sizeof(*owner));assert(owner);owner->refs=1;owner->dropped=dropped;
    owner->self=owner;assert(hold(owner));drop(owner); /* One genuine self slot. */
    owner->group=xl6TaskGroupCreate();assert(owner->group);
    if(flags&1){owner->child=xl6TaskGroupChild(owner->group);assert(owner->child);}
    owner->parentPromise=xrtPromiseCreate(&owner->parent,NULL);assert(owner->parentPromise && owner->parent);
    assert(xl6TaskGroupBindParent(owner->group,owner->parent));
    if(flags&2){owner->sourcePromise=xrtPromiseCreate(&owner->source,NULL);assert(owner->sourcePromise && owner->source);
        assert(xl6TaskGroupAdd(owner->child?owner->child:owner->group,owner->source));}
    return owner;
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(!xrtGetError() && before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static void cycle(unsigned flags)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);unsigned dropped=0;Owner* owner=make(flags,&dropped);
    xfuture* externalParent=(flags&4)?xrtFutureRef(owner->parent):NULL;
    xfuture* externalDone=(flags&4)?xl6TaskGroupFuture(owner->group):NULL;
    xrtownershipref root=view(owner);xrtownershipscope scope={0};xrtownershipresult graph={0};
    assert(xrtOwnershipFreezeTryBegin(&scope) && xrtOwnershipInspect(&root,1,NULL,0,&graph));
    assert(graph.NodeCount==10u+((flags&1)?7u:0u)+((flags&2)?3u:0u));
    assert(graph.EdgeCount==16u+((flags&1)?14u:0u)+((flags&2)?7u:0u));
    assert(graph.ExternalRootCount==((flags&4)?2u:0u));end(&scope);
    XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreateV3(root,resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_FINALIZERS);
    if(flags&8){
        assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
        assert(!owner->claim && !owner->cleared && xl6TaskGroupCount(owner->group)==((flags&1)?1:(flags&2)?1:0));
        assert(xrtFutureCancel(owner->parent));dispose(owner);
    }else{
        assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);
        assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY);
        assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
        if(externalParent)assert(xrtFutureCancel(externalParent));
        assert(xrtMemDebugFailAfter(0));unsigned turns=0;XL6_OwnershipStepResult result;
        do{result=xl6OwnershipPlanFinishV2(plan);assert(result>=XL6_OWNERSHIP_STEP_BUSY && ++turns<32);}
        while(result==XL6_OWNERSHIP_STEP_BUSY);
        assert(!xrtMemDebugFailTriggered() && !dropped);
        assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
        assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);xrtMemDebugFailClear();
        if(externalDone)assert(xrtFutureState(externalDone)==XFUTURE_RESOLVED);
    }
    assert(dropped==1);xrtFutureDestroy(externalParent);xrtFutureDestroy(externalDone);balanced(&before);
}
typedef struct Reentry { xl6_task_group* group; unsigned calls; bool destroy; } Reentry;
static void reentered(ptr data)
{
    Reentry* entry=data;++entry->calls;assert(xl6TaskGroupCount(entry->group)==1);
    if(entry->destroy){xl6TaskGroupDestroy(entry->group);entry->group=NULL;}
}
static void reentrant(bool destroy,bool synchronous)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Reentry entry={xl6TaskGroupCreate(),0,destroy};assert(entry.group);
    xfuture *parent=NULL,*source=NULL;xpromise *one=xrtPromiseCreate(&parent,NULL),*two=xrtPromiseCreate(&source,NULL);assert(one && two);
    assert(xl6TaskGroupAdd(entry.group,source));xcancel* token=xrtFutureCancelToken(source);assert(token);
    xcancelwatch* watch=xrtCancelWatch(token,reentered,&entry);assert(watch);
    if(synchronous)assert(xrtFutureCancel(parent));
    bool bound=xl6TaskGroupBindParent(entry.group,parent);
    assert(bound==!(destroy && synchronous));
    if(!bound){assert(xrtGetError() && xrtErrorKind(xrtGetError())==XERR_CLOSED);xrtClearError();}
    if(!synchronous)assert(xrtFutureCancel(parent));
    assert(entry.calls==1 && xrtCancelUnwatchChecked(watch));xrtCancelDestroy(token);
    assert(xrtPromiseCancel(two));xl6TaskGroupDestroy(entry.group);
    xrtPromiseDestroy(one);xrtPromiseDestroy(two);xrtFutureDestroy(parent);xrtFutureDestroy(source);balanced(&before);
}
typedef struct Waiting { xl6_task_group* group; bool done; } Waiting;
static int32 waiter(ptr data)
{ Waiting* waiting=data;waiting->done=xl6TaskGroupJoin(waiting->group);return 0; }
static void concurrent(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Waiting waiting={xl6TaskGroupCreate(),false};assert(waiting.group);
    xfuture* source=NULL;xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise && source);
    assert(xl6TaskGroupAdd(waiting.group,source));xthread* thread=xrtThreadCreate(waiter,&waiting,0);assert(thread);
    double deadline=xrtTimer()+5;bool busy=false;
    while(!busy){
        xrtownershipscope scope={0};
        if(xrtOwnershipFreezeTryBegin(&scope)){
            const xrtownershipfinishv1* completion=NULL;
            busy=xl6TaskGroupOwnershipAdapterV1(xl6TaskGroupOwnershipV1(waiting.group),&completion)==NULL;end(&scope);
        }
        assert(xrtTimer()<deadline);
    }
    xl6TaskGroupDestroy(waiting.group);assert(xrtPromiseResolve(promise,NULL));
    assert(xrtThreadWaitFor(thread,UINT64_C(5000000))==XWAIT_OK && waiting.done);xrtThreadDestroy(thread);
    xrtPromiseDestroy(promise);xrtFutureDestroy(source);balanced(&before);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();
    for(unsigned repeat=0;repeat<10;++repeat)for(unsigned flags=0;flags<16;++flags)cycle(flags);
    for(unsigned repeat=0;repeat<20;++repeat){
        reentrant(false,false);reentrant(true,false);reentrant(false,true);reentrant(true,true);concurrent();
    }
    puts("TaskGroup bridge ownership: 160 real cycles, 80 restored registrations, 80 reentrant callbacks, 20 concurrent wait/destroy tails; exact physical roots and full ledger balanced");return 0;
}
