/* Real resident handle/domain admission; no language-frame claim is made here. */
#define XRT_MODULE_ALL
#define XRUNTIME_MODULE_ALL
#include "lib/xruntime/xruntime.h"
#include "src/api/xl_task_group_bridge.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>

typedef struct Owner {
    volatile int32 refs;
    struct Owner* self;
    xvalue *handle,*child;
    xfuture *source,*parent;
    xpromise* parentPromise;
    const void* claim;
    unsigned *dropped,*finalized;
    bool cleared;
} Owner;
static const xrtownershipops ownerOps;
static xrtownershipref view(const Owner* owner) { return (xrtownershipref){owner,&ownerOps}; }
static void end(xrtownershipscope* scope) { assert(xrtOwnershipScopeEnd(scope)); }
static void begin(xrtownershipscope* scope) { assert(xrtOwnershipMutationBegin(scope)); }
static xl6_task_group* group(const xvalue* handle)
{ void* raw=NULL;assert(xrtValueGetHandle(handle,&raw,NULL,NULL) && raw);return raw; }
static xvalue* wrap(xl6_task_group* raw)
{ void* owned=raw;assert(owned);xvalue* handle=xl6NativeHandleTakeV1(&owned,xl6TaskGroupHandlePolicyV1Get());assert(handle && !owned);return handle; }
static bool count(const void* data,size_t* refs)
{ const Owner* owner=data;if(!owner || owner->cleared || owner->refs<=0)return false;*refs=(size_t)owner->refs;return true; }
static bool trace(const void* data,xrtownershipvisitor visit,ptr context)
{
    const Owner* owner=data;
    return visit(view(owner->self),context) && visit(xrtValueOwnership(owner->handle),context) &&
        (!owner->child || visit(xrtValueOwnership(owner->child),context)) &&
        visit(xrtFutureOwnership(owner->source),context) &&
        (!owner->parent || visit(xrtFutureOwnership(owner->parent),context)) &&
        (!owner->parentPromise || visit(xrtPromiseOwnership(owner->parentPromise),context));
}
static const xrtownershipops ownerOps={count,trace};
static bool hold(const void* data)
{ Owner* owner=(Owner*)data;xrtownershipscope scope={0};begin(&scope);bool held=!owner->cleared && xrtRefRetain(&owner->refs)>0;end(&scope);return held; }
static void drop(const void* data)
{
    Owner* owner=(Owner*)data;xrtownershipscope scope={0};begin(&scope);
    int32 refs=xrtRefRelease(&owner->refs);assert(refs>=0);
    if(!refs){assert(!owner->self && !owner->handle && !owner->child && !owner->source && !owner->parent && !owner->parentPromise);
        ++*owner->dropped;xrtFree(owner);}
    end(&scope);
}
static bool claim(const void* data,const void* token)
{ Owner* owner=(Owner*)data;if(!token || owner->cleared || (owner->claim && owner->claim!=token))return false;owner->claim=token;return true; }
static void restore(const void* data,const void* token)
{ Owner* owner=(Owner*)data;assert(owner->claim==token && !owner->cleared);owner->claim=NULL; }
static bool finalize(const void* data,const void* token)
{
    const Owner* owner=data;assert(owner->claim==token && !owner->cleared);
    assert(xl6TaskGroupCount(group(owner->handle))==0);
    if(owner->child)assert(xl6TaskGroupCount(group(owner->child))==0);
    xfuture* done=xl6TaskGroupFuture(group(owner->handle));assert(done && xrtFutureState(done)==XFUTURE_RESOLVED);xrtFutureDestroy(done);
    assert(!xl6TaskGroupBindParent(group(owner->handle),owner->source));
    assert(xrtGetError() && xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    ++*owner->finalized;return true;
}
static void clear(const void* data,const void* token)
{ Owner* owner=(Owner*)data;assert(owner->claim==token && !owner->cleared);owner->cleared=true; }
static void dispose(Owner* owner)
{
    xrtownershipscope scope={0};begin(&scope);
    Owner* self=owner->self;xvalue *handle=owner->handle,*child=owner->child;
    xfuture *source=owner->source,*parent=owner->parent;xpromise* parentPromise=owner->parentPromise;
    owner->self=NULL;owner->handle=owner->child=NULL;owner->source=owner->parent=NULL;owner->parentPromise=NULL;end(&scope);
    xrtValueRelease(child);xrtValueRelease(handle);xrtFutureDestroy(source);xrtPromiseDestroy(parentPromise);xrtFutureDestroy(parent);drop(self);
}
static bool finish(const void* data,const void* token)
{ Owner* owner=(Owner*)data;assert(owner->claim==token && owner->cleared);dispose(owner);return true; }
static const xrtownershipadapterv1 ownerAdapter={sizeof(ownerAdapter),hold,drop,claim,restore,finalize,clear,finish};
static bool resolve(xrtownershipref ref,void* catalog,const xrtownershipadapterv1** adapter,
    const xrtownershippreparationv1** preparation,const xrtownershipfinishv1** completion)
{
    *adapter=NULL;*preparation=NULL;*completion=NULL;
    if(ref.Ops==&ownerOps){*adapter=&ownerAdapter;return true;}
    return xl6OwnershipCatalogResolveV1(ref,catalog,adapter,preparation,completion);
}
static Owner* make(unsigned flags,unsigned* dropped,unsigned* finalized,xpromise** producer)
{
    Owner* owner=xrtCalloc(1,sizeof(*owner));assert(owner);owner->refs=1;owner->self=owner;owner->dropped=dropped;owner->finalized=finalized;
    owner->handle=wrap(xl6TaskGroupCreate());
    if(flags&2)owner->child=wrap(xl6TaskGroupChild(group(owner->handle)));
    if(flags&1){owner->parentPromise=xrtPromiseCreate(&owner->parent,NULL);assert(owner->parentPromise && owner->parent);
        assert(xl6TaskGroupBindParent(group(owner->handle),owner->parent));}
    *producer=xrtPromiseCreate(&owner->source,NULL);assert(*producer && owner->source);
    xl6_task_group* target=group(owner->child?owner->child:owner->handle);
    assert(xl6TaskGroupAdd(target,owner->source) && xl6TaskGroupAdd(target,owner->source));
    return owner;
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(!xrtGetError() && before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount && before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static void cycle(unsigned flags,XL6_OwnershipCatalog* catalog)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);unsigned dropped=0,finalized=0;xpromise* producer=NULL;
    Owner* owner=make(flags,&dropped,&finalized,&producer);
    xfuture* externalDone=(flags&4)?xl6TaskGroupFuture(group(owner->handle)):NULL;
    xrtownershipref root=view(owner);xrtownershipscope scope={0};xrtownershipresult graph={0};
    assert(xrtOwnershipFreezeTryBegin(&scope) && xrtOwnershipInspect(&root,1,NULL,0,&graph));
    /* A child contributes its language handle as a separate physical owner,
     * including the extra handle -> wrapper edge (not a borrowed shortcut). */
    const size_t nodes=12u+((flags&1)?3u:0u)+((flags&2)?8u:0u),edges=22u+((flags&1)?6u:0u)+((flags&2)?16u:0u);
    if(graph.NodeCount!=nodes || graph.EdgeCount!=edges || graph.ExternalRootCount!=1u+((flags&4)?1u:0u))
        fprintf(stderr,"flags=%u nodes=%zu edges=%zu roots=%zu\n",flags,graph.NodeCount,graph.EdgeCount,graph.ExternalRootCount);
    assert(graph.NodeCount==nodes && graph.EdgeCount==edges && graph.ExternalRootCount==1u+((flags&4)?1u:0u));end(&scope);
    XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreateV3(root,resolve,catalog,&plan)==XL6_OWNERSHIP_PLAN_PREPARATIONS);
    if(flags&8){
        assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
        assert(!owner->claim && !owner->cleared && !finalized);
        if(owner->parent){assert(xrtFutureCancel(owner->parent));xcancel* token=xrtFutureCancelToken(owner->source);
            assert(token && xrtCancelRequested(token));xrtCancelDestroy(token);}
        assert(xrtPromiseCancel(producer));dispose(owner);
    }else{
        assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_BUSY);
        assert(!finalized && !dropped && xrtFutureState(owner->source)==XFUTURE_PENDING);
        xcancel* token=xrtFutureCancelToken(owner->source);assert(token && xrtCancelRequested(token));xrtCancelDestroy(token);
        /* Only the actual external producer acknowledges cancellation. The
         * collector cannot fabricate completion or erase a genuine root. */
        assert(xrtPromiseCancel(producer));
        XL6_OwnershipStepResult result;unsigned turns=0;
        do{result=xl6OwnershipPlanPrepare(plan);assert(++turns<32 && result>=XL6_OWNERSHIP_STEP_BUSY);}while(result==XL6_OWNERSHIP_STEP_BUSY);
        assert(!finalized && xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY && finalized==1);
        assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
        assert(xrtMemDebugFailAfter(0));turns=0;
        do{result=xl6OwnershipPlanFinishV2(plan);assert(++turns<32 && result>=XL6_OWNERSHIP_STEP_BUSY);}while(result==XL6_OWNERSHIP_STEP_BUSY);
        assert(!xrtMemDebugFailTriggered() && !dropped && xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
        assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);xrtMemDebugFailClear();
    }
    assert(dropped==1 && finalized==((flags&8)?0u:1u));
    if(externalDone)assert(xrtFutureState(externalDone)==XFUTURE_RESOLVED);
    xrtPromiseDestroy(producer);xrtFutureDestroy(externalDone);balanced(&before);
}
static unsigned unknownTraceCalls;
static bool unknownTrace(const xvalue* value,xrtownershipvisitor visit,ptr context)
{ (void)value;(void)visit;(void)context;++unknownTraceCalls;return false; }
static void rejectUnknown(XL6_OwnershipCatalog* catalog)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    const XL6_NativeHandlePolicyV1* known=xl6TaskGroupHandlePolicyV1Get();
    xvaluehandleops copiedOps=*known->Ops;
    XL6_NativeHandlePolicyV1 unknown={sizeof(unknown),XL6_NATIVE_HANDLE_POLICY_V1_TAG,&copiedOps,unknownTrace};
    void* raw=xl6TaskGroupCreate();assert(raw);
    xvalue* handle=xl6NativeHandleTakeV1(&raw,&unknown);assert(handle && !raw);
    const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;const xrtownershipfinishv1* completion=NULL;
    xrtownershipscope scope={0};assert(xrtOwnershipFreezeTryBegin(&scope));
    assert(!xl6OwnershipCatalogResolveV1(xrtValueOwnership(handle),catalog,&adapter,&preparation,&completion));
    assert(!adapter && !preparation && !completion && !unknownTraceCalls);end(&scope);
    xrtValueRelease(handle);balanced(&before);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainResourceTaskGroupV1Get(),xl6OwnershipDomainNativeFutureV1Get()};
    for(size_t domainCount=1;domainCount<=2;++domainCount){
        XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,domainCount);assert(catalog);
        rejectUnknown(catalog);
        for(unsigned round=0;round<4;++round)for(unsigned flags=0;flags<16;++flags)cycle(flags,catalog);
        xl6OwnershipCatalogDestroyV1(catalog);
    }
    balanced(&before);
    puts("TaskGroup catalog: 128 real handle graphs, exact external roots, cooperative preparation, abort restoration, unknown-policy refusal, zero-allocation finish, full ledger balanced");return 0;
}
