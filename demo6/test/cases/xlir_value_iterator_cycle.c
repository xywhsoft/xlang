#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE_GRAPH
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_value_iterator.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <stdio.h>
#include <windows.h>

typedef struct Stats {
    unsigned mode,finalizers,drops,probes;
    xvaluecursor* native; /* Borrowed from the real language iterator. */
    xvaluecursor* extra;  /* An actual independently retained external owner. */
} Stats;
typedef struct Context {Stats* stats;XL6_ValueIterator* iterator;} Context;
static const XL6_ValueIteratorRuntimeV1* api;
static unsigned graphs,rooted,refusals,ooms,budgets,finalizers,drops,probes;
static void freeze(xrtownershipscope* scope)
{ULONGLONG stop=GetTickCount64()+5000;while(!xrtOwnershipFreezeTryBegin(scope)){assert(GetTickCount64()<stop);SwitchToThread();}}
static DWORD WINAPI freeze_thread(void* unused)
{(void)unused;xrtownershipscope scope={0};freeze(&scope);assert(xrtOwnershipScopeEnd(&scope));return 0;}
static void cross_thread(Stats* stats)
{
    HANDLE thread=CreateThread(NULL,0,freeze_thread,NULL,0,NULL);
    assert(thread&&WaitForSingleObject(thread,10000)==WAIT_OBJECT_0&&CloseHandle(thread));++stats->probes;
}
static bool empty_trace(const void* data,xrtownershipvisitor visit,void* context)
{(void)data;(void)visit;(void)context;return true;}
static void empty_drop(void* data){assert(!data);}
static bool trace_context(const void* data,xrtownershipvisitor visit,void* unused)
{
    const Context* context=data;
    return !context->iterator||visit(api->Ownership(context->iterator),unused);
}
static void drop_context(void* data)
{
    Context* context=data;Stats* stats=context->stats;
    assert(stats->finalizers==1&&!stats->drops);++stats->drops;cross_thread(stats);
    XL6_ValueIterator* iterator=context->iterator;context->iterator=NULL;
    api->Destroy(iterator);xrtFree(context);
}
static bool finalize_checked(xvalue* object,void* data)
{
    Context* context=data;Stats* stats=context->stats;
    assert(!stats->finalizers&&!stats->drops);++stats->finalizers;
    const XL6_ValueIteratorView* view=api->View(context->iterator);
    assert(view&&view->item==xrtValueObjectGet(object,XRT_STR_LITERAL("item")));
    int64 item=0;assert(xrtValueGetInt(view->item,&item)&&item==42);
    cross_thread(stats);
    if(stats->mode==2){stats->extra=xrtValueCursorRetain(stats->native);assert(stats->extra==stats->native);}
    if(stats->mode==1){xrtSetErrorKind(XERR_STATE);return false;}
    return true;
}
static void finalize_terminal(xvalue* object,void* data){(void)finalize_checked(object,data);}
static const xvalueobjectownershipv1 policy={sizeof(policy),finalize_terminal,trace_context,drop_context,empty_trace,empty_drop,finalize_checked};
static bool resolve(xrtownershipref ref,void* denyNative,const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation)
{
    *preparation=NULL;
    if((*adapter=xl6ValueIteratorOwnershipAdapterV1(ref)))return true;
    if((*adapter=xrtValueCursorOwnershipAdapterV1(ref)))return denyNative==NULL;
    if((*adapter=xrtValueObjectOwnershipAdapterV1(ref,&policy)))return true;
    *adapter=xrtValueOwnershipAdapterV1(ref);return *adapter!=NULL;
}
static bool native_child(xrtownershipref child,void* data)
{
    Stats* stats=data;assert(!stats->native&&xrtValueCursorOwnershipAdapterV1(child));
    stats->native=(xvaluecursor*)child.Data;return true;
}
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&
        before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static void transaction(unsigned mode)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Stats stats={0};stats.mode=mode;
    Context* context=xrtCalloc(1,sizeof(*context));xvalue* object=xrtValueObject();assert(context&&object);context->stats=&stats;
    assert(xrtValueObjectSetNew(object,XRT_STR_LITERAL("item"),xrtValueInt(42))&&
        xrtValueObjectLifetimeBindOwned(object,NULL,empty_trace,empty_drop)&&
        xrtValueObjectFinalizerPrepareOwned(object,finalize_terminal,context,trace_context,drop_context)&&
        xrtValueObjectOwnershipBindV1(object,&policy)&&xrtValueObjectFinalizerCommit(object));
    XL6_ValueIterator* iterator=api->Create(object);assert(iterator&&api->Advance(iterator));
    xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation));
    context->iterator=iterator;assert(xrtOwnershipScopeEnd(&mutation)); /* Transfer the actual creation reference. */
    xrtownershipref root=api->Ownership(iterator);xrtownershipscope scope={0};freeze(&scope);
    assert(root.Ops->Trace(root.Data,native_child,&stats));
    xrtownershipresult graph={0};assert(xrtOwnershipInspect(&root,1,NULL,0,&graph));
    if(!graphs)printf("Iterator cycle: nodes=%llu edges=%llu roots=%llu\n",(unsigned long long)graph.NodeCount,
        (unsigned long long)graph.EdgeCount,(unsigned long long)graph.ExternalRootCount);
    assert(graph.NodeCount==6&&graph.EdgeCount==7&&graph.ExternalRootCount==1);assert(xrtOwnershipScopeEnd(&scope));
    XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreateV2(root,resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ROOTED&&plan);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_ROOTED&&!stats.finalizers);
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);plan=NULL;++rooted;
    xrtValueRelease(object);freeze(&scope);
    assert(xrtOwnershipInspect(&root,1,NULL,0,&graph)&&graph.NodeCount==6&&graph.EdgeCount==7&&!graph.ExternalRootCount);
    assert(xrtOwnershipScopeEnd(&scope));++graphs;
    assert(xl6OwnershipPlanTryCreateV2(root,resolve,&stats,&plan)==XL6_OWNERSHIP_PLAN_ERROR&&!plan);
    ++refusals;xrtClearError();
    for(unsigned fault=0;fault<24;++fault){
        xmemdebugsnapshot scratch;xrtMemDebugSnapshot(&scratch);assert(xrtMemDebugFailAfter(fault));
        XL6_OwnershipPlanResult status=xl6OwnershipPlanTryCreateV2(root,resolve,NULL,&plan);
        bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++budgets;
        if(triggered){assert(status==XL6_OWNERSHIP_PLAN_ERROR&&!plan);++ooms;}
        else{assert(status==XL6_OWNERSHIP_PLAN_FINALIZERS&&plan);assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);plan=NULL;}
        assert(!stats.finalizers&&!stats.drops);balance(&scratch);
    }
    assert(xl6OwnershipPlanTryCreateV2(root,resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_FINALIZERS&&plan);
    assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);
    XL6_OwnershipStepResult step=xl6OwnershipPlanFinalizeV2(plan);
    assert(stats.finalizers==1&&!stats.drops);
    if(mode==1){
        assert(step==XL6_OWNERSHIP_STEP_FAILED&&xrtErrorKind(xrtGetError())==XERR_STATE);
        assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);plan=NULL;xrtClearError();
        assert(xl6OwnershipPlanTryCreateV2(root,resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_FINALIZERS&&plan);
        assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY&&stats.finalizers==1);
    }else assert(step==XL6_OWNERSHIP_STEP_READY);
    if(mode==2){
        assert(stats.extra&&xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_ROOTED);++rooted;
        assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);plan=NULL;
        xrtValueCursorRelease(stats.extra);stats.extra=NULL;
        assert(xl6OwnershipPlanTryCreateV2(root,resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_FINALIZERS&&plan);
        assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY&&stats.finalizers==1);
    }
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY&&!stats.drops);
    assert(xl6OwnershipPlanFinish(plan)&&xl6OwnershipPlanFinish(plan));
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(stats.finalizers==1&&stats.drops==1&&stats.probes==2);
    finalizers+=stats.finalizers;drops+=stats.drops;probes+=stats.probes;balance(&before);
}
int main(void)
{
    api=xl6ValueIteratorRuntimeV1Get();assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_MEMORY);xrtClearError();
    for(unsigned round=0;round<25;++round)for(unsigned mode=0;mode<3;++mode)transaction(mode);
    assert(graphs==75&&rooted==100&&refusals==75&&budgets==1800&&ooms&&finalizers==75&&drops==75&&probes==150);
    printf("Managed iterator cycles: 75 complete six-node/seven-edge graphs, 100 real-root deferrals, 75 missing-child refusals, 1800 OOM budgets and %u actual refusals, 75 finalizers, 75 context drops and 150 cross-thread freezes; failure retry and native resurrection without duplicate finalization, exact memory balance\n",ooms);return 0;
}
