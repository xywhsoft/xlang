#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE_GRAPH
#include "lib/xrt/xrt.h"
#include "src/api/xl_foreign_error.h"
#include "src/runtime/xl6_generator.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <stdio.h>
#include "xlir_lifetime_platform.h"

extern XL6_Generator* managed_generator_create(xvalue*);
typedef struct Stats {
    unsigned mode,marks,finalizers,drops,probes;
    XL6_Generator* generator; /* Borrowed; the finalizer context owns it. */
    void* frame;             /* Borrowed except for the explicit extraFrame. */
    XL6_Generator* extra;
    void* extraFrame;
    xrtownershipref frameRef;
} Stats;
typedef struct Context {Stats* stats;XL6_Generator* generator;} Context;
static Stats* current;
static const XL6_GeneratorRuntimeV1* api;
static const XL6_GeneratorRuntimeV2* managed;
static unsigned transactions,marks,finalizers,drops,probes,rooted,refusals,budgets,ooms,prepareFailures,finalizerFailures;
static void freeze(xrtownershipscope* scope)
{uint64_t stop=xlirTestMilliseconds()+5000;while(!xrtOwnershipFreezeTryBegin(scope)){assert(xlirTestMilliseconds()<stop);xlirTestYield();}}
static XLIR_TEST_THREAD_RESULT XLIR_TEST_THREAD_CALL freeze_thread(void* unused)
{(void)unused;xrtownershipscope scope={0};freeze(&scope);assert(xrtOwnershipScopeEnd(&scope));return 0;}
static void cross_thread(Stats* stats)
{
    XLIR_TestThread thread; assert(xlirTestThreadStart(&thread,freeze_thread,NULL));
    xlirTestThreadJoin(thread);++stats->probes;
}
void managed_generator_mark(int64_t step)
{
    Stats* stats=current;assert(stats&&step==(int64_t)stats->marks+1&&!stats->finalizers);++stats->marks;
    cross_thread(stats);
    xrtownershipscope scope={0};freeze(&scope);size_t count=999;
    xrtownershipref control=api->Ownership(stats->generator);
    assert(!control.Ops->Count(control.Data,&count)&&count==999);
    assert(!stats->frameRef.Ops->Count(stats->frameRef.Data,&count)&&count==999);
    assert(xrtOwnershipScopeEnd(&scope));
    /* Independently retained references can be released while the source is
     * running; the resident invocation still owns its actual lifetime pin. */
    assert(managed->Retain(stats->generator)==stats->generator);api->Destroy(stats->generator);
    assert(managed->FrameRetain(stats->frame)==stats->frame);managed->FrameRelease(stats->frame);
    if(step==1&&stats->mode==3){stats->extra=managed->Retain(stats->generator);assert(stats->extra);}
    if(step==1&&stats->mode==4){stats->extraFrame=managed->FrameRetain(stats->frame);assert(stats->extraFrame);}
}
void managed_generator_fail(void)
{
    if(current->mode==1||current->mode==2){
        xerror* error=current->mode==1?xrtErrorCreate(XERR_STATE,"generator preparation",189,"controlled failure"):NULL;
        assert(current->mode==2||error);xl6ErrorScopePropagateFailure((xl6_error_result){false,error});
    }
}
static bool empty_trace(const void* data,xrtownershipvisitor visit,void* context)
{(void)data;(void)visit;(void)context;return true;}
static void empty_drop(void* data){assert(!data);}
static bool trace_context(const void* data,xrtownershipvisitor visit,void* context)
{const Context* owner=data;return !owner->generator||visit(api->Ownership(owner->generator),context);}
static void drop_context(void* data)
{
    Context* context=data;Stats* stats=context->stats;
    assert(stats->marks==2&&stats->finalizers==1&&!stats->drops);++stats->drops;cross_thread(stats);
    XL6_Generator* generator=context->generator;context->generator=NULL;api->Destroy(generator);xrtFree(context);
}
static bool finalize_checked(xvalue* object,void* data)
{
    Context* context=data;Stats* stats=context->stats;int64 item=0;
    assert(stats->marks==2&&!stats->finalizers&&!stats->drops);++stats->finalizers;
    assert(xrtValueGetInt(xrtValueObjectGet(object,XRT_STR_LITERAL("item")),&item)&&item==42);
    assert(context->generator->done&&!context->generator->frame&&!context->generator->has_item);cross_thread(stats);
    if(stats->mode==6){stats->extra=managed->Retain(context->generator);assert(stats->extra);}
    if(stats->mode==5){xrtSetErrorKind(XERR_STATE);return false;}
    return true;
}
static void finalize_terminal(xvalue* object,void* data){(void)finalize_checked(object,data);}
static const xvalueobjectownershipv1 policy={sizeof(policy),finalize_terminal,trace_context,drop_context,empty_trace,empty_drop,finalize_checked};
static bool resolve(xrtownershipref ref,void* deny,const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** preparation)
{
    *preparation=NULL;
    if((*adapter=xl6GeneratorOwnershipAdapterV1(ref,preparation)))return !deny||ref.Data!=current->frameRef.Data;
    if((*adapter=xrtValueObjectOwnershipAdapterV1(ref,&policy)))return true;
    *adapter=xrtValueOwnershipAdapterV1(ref);return *adapter!=NULL;
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
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Stats stats={0};stats.mode=mode;current=&stats;
    Context* context=xrtCalloc(1,sizeof(*context));xvalue* object=xrtValueObject();assert(context&&object);context->stats=&stats;
    assert(xrtValueObjectSetNew(object,XRT_STR_LITERAL("item"),xrtValueInt(42))&&
        xrtValueObjectLifetimeBindOwned(object,NULL,empty_trace,empty_drop)&&
        xrtValueObjectFinalizerPrepareOwned(object,finalize_terminal,context,trace_context,drop_context)&&
        xrtValueObjectOwnershipBindV1(object,&policy)&&xrtValueObjectFinalizerCommit(object));
    XL6_Generator* generator=managed_generator_create(object);assert(generator);stats.generator=generator;stats.frame=generator->frame;
    stats.frameRef=managed->FrameOwnership(generator->frame);
    xrtownershipscope scope={0};assert(xrtOwnershipMutationBegin(&scope));context->generator=generator;assert(xrtOwnershipScopeEnd(&scope));
    XL6_GeneratorStep step=api->Next(generator);assert(step.ok&&step.value&&!stats.marks);
    xrtownershipref root=api->Ownership(generator);xrtownershipresult graph={0};freeze(&scope);
    assert(xrtOwnershipInspect(&root,1,NULL,0,&graph));
    if(!transactions)printf("Generator cycle: nodes=%llu edges=%llu roots=%llu\n",(unsigned long long)graph.NodeCount,
        (unsigned long long)graph.EdgeCount,(unsigned long long)graph.ExternalRootCount);
    assert(graph.NodeCount==6&&graph.EdgeCount==7&&graph.ExternalRootCount==1);assert(xrtOwnershipScopeEnd(&scope));
    XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreateV2(root,resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ROOTED&&plan);
    assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY&&!stats.marks&&!stats.finalizers);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_ROOTED);
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);plan=NULL;++rooted;
    xrtValueRelease(object);freeze(&scope);
    assert(xrtOwnershipInspect(&root,1,NULL,0,&graph)&&graph.NodeCount==6&&graph.EdgeCount==7&&!graph.ExternalRootCount);
    assert(xrtOwnershipScopeEnd(&scope));
    assert(xl6OwnershipPlanTryCreateV2(root,resolve,&stats,&plan)==XL6_OWNERSHIP_PLAN_ERROR&&!plan);++refusals;xrtClearError();
    for(unsigned fault=0;fault<24;++fault){
        xmemdebugsnapshot scratch;xrtMemDebugSnapshot(&scratch);assert(xrtMemDebugFailAfter(fault));
        XL6_OwnershipPlanResult status=xl6OwnershipPlanTryCreateV2(root,resolve,NULL,&plan);
        bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();++budgets;
        if(triggered){assert(status==XL6_OWNERSHIP_PLAN_ERROR&&!plan);++ooms;}
        else{assert(status==XL6_OWNERSHIP_PLAN_PREPARATIONS&&plan);assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);plan=NULL;}
        assert(!stats.marks&&!stats.finalizers&&!stats.drops);balance(&scratch);
    }
    assert(xl6OwnershipPlanTryCreateV2(root,resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_PREPARATIONS&&plan);
    assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY&&!stats.finalizers); /* Preparation cannot be skipped. */
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_PREPARATIONS);
    XL6_OwnershipStepResult prepared=xl6OwnershipPlanPrepare(plan);
    assert(stats.marks==2&&!stats.finalizers&&!stats.drops);
    if(mode==1||mode==2){
        assert(prepared==XL6_OWNERSHIP_STEP_FAILED);
        assert(mode==1?(xrtGetError()&&xrtErrorCode(xrtGetError())==189):!xrtGetError());++prepareFailures;xrtClearError();
        assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY&&stats.marks==2);
    }else assert(prepared==XL6_OWNERSHIP_STEP_READY);
    if(mode==3){
        assert(stats.extra&&xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_ROOTED);++rooted;
        assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY&&!stats.finalizers);
        api->Destroy(stats.extra);stats.extra=NULL;
        assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY&&stats.marks==2);
    }
    XL6_OwnershipStepResult finalized=xl6OwnershipPlanFinalizeV2(plan);assert(stats.finalizers==1&&!stats.drops);
    if(mode==5){
        assert(finalized==XL6_OWNERSHIP_STEP_FAILED&&xrtErrorKind(xrtGetError())==XERR_STATE);++finalizerFailures;xrtClearError();
        /* Abort retains completed source/finalizer duties, not deleted graph
         * edges reconstructed solely to make a synthetic retry possible. */
        stats.extra=managed->Retain(generator);assert(stats.extra);
        assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);plan=NULL;
        assert(stats.drops==1&&stats.marks==2);
        xl6_error_result closed=api->Close(stats.extra);assert(closed.ok&&!closed.error);api->Destroy(stats.extra);stats.extra=NULL;
    }else{
        assert(finalized==XL6_OWNERSHIP_STEP_READY);
        if(mode==6){assert(stats.extra&&xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_ROOTED);++rooted;api->Destroy(stats.extra);stats.extra=NULL;}
        assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY&&!stats.drops);
        assert(xl6OwnershipPlanFinish(plan)&&xl6OwnershipPlanFinish(plan));
        assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    }
    if(mode==4){
        size_t count=0;freeze(&scope);assert(stats.frameRef.Ops->Count(stats.frameRef.Data,&count)&&count==1);
        assert(xrtOwnershipScopeEnd(&scope));managed->FrameRelease(stats.extraFrame);stats.extraFrame=NULL;
    }
    assert(stats.marks==2&&stats.finalizers==1&&stats.drops==1&&stats.probes==4);
    ++transactions;marks+=stats.marks;finalizers+=stats.finalizers;drops+=stats.drops;probes+=stats.probes;current=NULL;balance(&before);
}
int main(void)
{
    api=xl6GeneratorRuntimeV1Get();managed=xl6GeneratorRuntimeV2Get();assert(xrtMemDebugEnable(true));
    xl6_error_scope warm={0};assert(xl6ErrorScopeBegin(&warm));assert(xl6ErrorScopeEnd(&warm).ok);xrtSetErrorKind(XERR_MEMORY);xrtClearError();
    for(unsigned n=0;n<20;++n)for(unsigned mode=0;mode<7;++mode)transaction(mode);
    assert(transactions==140&&marks==280&&finalizers==140&&drops==140&&probes==560&&rooted==180&&
        refusals==140&&budgets==3360&&ooms&&prepareFailures==40&&finalizerFailures==20);
    printf("Generated generator cycles: 140 complete six-node/seven-edge transactions, 280 finally steps, 140 finalizers, 140 context drops, 560 cross-thread freezes, 180 real-root deferrals, 140 missing-frame refusals, 3360 OOM budgets and %u actual refusals; 40 preparation failures, 20 finalizer failures, native resurrection and independently pinned frames; exact memory balance\n",ooms);return 0;
}
