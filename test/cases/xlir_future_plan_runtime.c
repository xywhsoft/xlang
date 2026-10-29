/* Reuse the established physical class/callable fixture, without replacing
 * its original 450-graph test population or changing its resolver. */
#define XRT_MODULE_FUTURE
#define XRUNTIME_MODULE_RUNTIME_VALUE_FUTURE
#define main original_class_callable_main
#include "xlir_class_callable_plan_runtime.c"
#undef main
#include "src/runtime/xl6_future_payload.h"

typedef struct ProducerBox { xvalue* roots; xfuture* future; xpromise* promise; } ProducerBox;
static unsigned producer_drops, refusals, future_faults, future_transactions;
static void producer_drop(void* value, void* unused)
{
    ProducerBox* box=value; xrtownershipscope freeze={0}; assert(!unused);
    assert(xrtOwnershipFreezeTryBegin(&freeze)); assert(xrtOwnershipScopeEnd(&freeze));
    assert(xrtFutureState(box->future)==XFUTURE_CLOSED);
    ++producer_drops; xrtValueRelease(box->roots);
    xrtPromiseDestroy(box->promise); xrtFutureDestroy(box->future); xrtFree(box);
}
static bool producer_trace(const void* value,const void* unused,xrtownershipvisitor visit,void* context)
{
    const ProducerBox* box=value; assert(!unused);
    return visit(xrtValueOwnership(box->roots),context) && visit(xrtFutureOwnership(box->future),context) &&
        visit(xrtPromiseOwnership(box->promise),context);
}
static const xfuturepayloadownershipv1 producer_policy={sizeof(producer_policy),producer_drop,producer_trace};
static const xfuturepayloadownershipv1* const producer_allowed[]={&producer_policy};
static const xrtownershipadapterv1* future_resolve(xrtownershipref ref,void* unused)
{
    const xrtownershipadapterv1* adapter;
    if((adapter=xl6PayloadOwnershipAdapterV1(ref)))return adapter;
    if((adapter=xrtFutureOwnershipAdapterV1(ref,producer_allowed,1)))return adapter;
    if((adapter=xrtCancelOwnershipAdapterV1(ref)))return adapter;
    if((adapter=xrtValueFutureOwnershipAdapterV1(ref)))return adapter;
    return resolve(ref,unused);
}
static XL6_OwnershipPlan* future_prepare(ClassOwner* o)
{
    XL6_OwnershipPlan* plan=NULL;
    XL6_OwnershipPlanResult result=xl6OwnershipPlanTryCreate((xrtownershipref){o,&owner_ops},future_resolve,NULL,&plan);
    if(result!=XL6_OWNERSHIP_PLAN_FINALIZERS)fprintf(stderr,"Future prepare result=%d error=%s\n",result,xrtGetError()?xrtErrorMessage(xrtGetError()):"none");
    assert(result==XL6_OWNERSHIP_PLAN_FINALIZERS && plan);return plan;
}
static ClassOwner* future_make(unsigned kind,xvalue** weak,xfuture** pending,xcancel** token)
{
    static const XL6_ClosureCaptureSlot slot={XL6_CLOSURE_CAPTURE_XVALUE,0,sizeof(xvalue*)};
    static const XL6_ClosureEnvironmentV1 layout={sizeof(layout),sizeof(xvalue*),1,&slot};
    mode=0; ClassOwner* o=make(weak); xfuture* future=NULL;
    xpromise* promise=xrtPromiseCreate(&future,NULL);assert(promise); *pending=NULL; *token=NULL;
    if(kind>=4){
        ProducerBox* box=xrtCalloc(1,sizeof(*box));assert(box);box->roots=o->roots;
        xcancel* parent=xrtCancelCreate();assert(parent);box->promise=xrtPromiseCreate(&box->future,parent);assert(box->promise);
        *token=parent;*pending=box->future;
        assert(xrtPromiseResolveOwnedPolicyV1(promise,box,&producer_policy));
    }else if(kind==3){
        xvalue** env=xrtMalloc(sizeof(*env));assert(env);*env=o->roots;
        XL6_Closure** box=xrtMalloc(sizeof(*box));assert(box);
        *box=xl6ClosureRuntimeV2Get()->Create(NULL,entry,NULL,env,&layout,NULL,NULL,0);assert(*box);
        assert(xl6PayloadResolveOwned(promise,box,XL6_FUTURE_PAYLOAD_CLOSURE));
    }else{
        xvalue** box=xrtMalloc(sizeof(*box));assert(box);*box=o->roots;
        assert(xl6PayloadResolveOwned(promise,box,XL6_FUTURE_PAYLOAD_XVALUE));
    }
    xrtPromiseDestroy(promise);
    if(kind==1 || kind==2){
        xfuture* outer=NULL;promise=xrtPromiseCreate(&outer,NULL);assert(promise);
        if(kind==1){assert(xrtPromiseForward(promise,future));xrtFutureDestroy(future);}
        else{xfuture** box=xrtMalloc(sizeof(*box));assert(box);*box=future;assert(xl6PayloadResolveOwned(promise,box,XL6_FUTURE_PAYLOAD_FUTURE));}
        xrtPromiseDestroy(promise);future=outer;
    }
    o->roots=xrtValueFutureTake(&future);assert(o->roots && !future);return o;
}
static void refused_watch(void* unused) { (void)unused; assert(!"retirement invoked an unknown observer"); }
static void future_transaction(unsigned kind)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);xvalue* weak=NULL;xfuture* pending=NULL;xcancel* parent=NULL;
    ClassOwner* o=future_make(kind,&weak,&pending,&parent);XL6_OwnershipPlan* plan=future_prepare(o);
    assert(xl6OwnershipPlanFinalize(plan) && calls==1);
    if(kind==5 || kind==6){
        xfuturewatch watch={0};xcancelwatch* observer=NULL;
        if(kind==5){assert(xrtFutureWatchInitPhased(&watch,refused_watch,legacy_release,NULL,poison_trace));assert(xrtFutureWatchAdd(pending,&watch)==XFUTURE_WATCH_PENDING);}
        else{observer=xrtCancelWatch(parent,refused_watch,NULL);assert(observer);}
        assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_ERROR && !o->cleared);++refusals;xrtClearError();
        assert(xrtFutureState(pending)==XFUTURE_PENDING && !xrtCancelRequested(parent));
        if(observer)xrtCancelUnwatch(observer);else xrtFutureWatchRemove(pending,&watch);
        abort_plan(plan);plan=future_prepare(o);
    }else if(kind==7){
        xfuture* alias=xrtFutureRef(xrtValueGetFuture(o->roots));assert(alias);
        assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_ROOTED && !o->cleared);
        abort_plan(plan);xrtFutureDestroy(alias);plan=future_prepare(o);
    }else if(kind==8){abort_plan(plan);plan=future_prepare(o);}
    xcancel* child=pending?xrtFutureCancelToken(pending):NULL;
    complete(plan);assert(calls==1);xrtValueRelease(weak);
    if(parent){assert(xrtCancelRequested(child) && !xrtCancelRequested(parent));xrtCancelDestroy(child);xrtCancelDestroy(parent);}
    current=NULL;receiver=NULL;balance(&before);++future_transactions;
}
static void future_fault(unsigned point,bool commit)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);xvalue* weak=NULL;xfuture* pending=NULL;xcancel* parent=NULL;
    ClassOwner* o=future_make(4,&weak,&pending,&parent);XL6_OwnershipPlan* plan=NULL;
    if(commit){plan=future_prepare(o);assert(xl6OwnershipPlanFinalize(plan));}
    assert(xrtMemDebugFailAfter(point));
    XL6_OwnershipPlanResult result=commit?xl6OwnershipPlanTryCommit(plan):xl6OwnershipPlanTryCreate((xrtownershipref){o,&owner_ops},future_resolve,NULL,&plan);
    bool failed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xrtClearError();
    if(failed){++future_faults;assert(result==XL6_OWNERSHIP_PLAN_ERROR && !o->cleared && xrtFutureState(pending)==XFUTURE_PENDING);if(plan)abort_plan(plan);plan=NULL;}
    if(!plan)plan=future_prepare(o);
    if(!commit || failed)complete(plan);
    else{assert(result==XL6_OWNERSHIP_PLAN_READY && xl6OwnershipPlanFinish(plan));abort_plan(plan);}
    assert(calls==1);xrtValueRelease(weak);xrtCancelDestroy(parent);current=NULL;receiver=NULL;balance(&before);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    for(unsigned round=0;round<25;++round)for(unsigned kind=0;kind<9;++kind)future_transaction(kind);
    for(unsigned point=0;point<32;++point){future_fault(point,false);future_fault(point,true);}
    assert(future_transactions==225 && freed==289 && finalizations==289 && producer_drops==189 && refusals==50 && future_faults>0 && !legacy_traced);
    printf("Future plans: 225 physical graphs, 289 once-only finalizers, 189 producer closes, 50 pre-trace observer refusals, 64 OOM probes with %u real failures; aliases, forwarding, nested and closure payloads, cancel parents and retry; zero live delta\n",future_faults);
    return 0;
}
