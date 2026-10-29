#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_THREAD
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_STRING
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#include "lib/xruntime/xruntime.h"
#include "src/runtime/xl6_closure.h"
#include "src/runtime/xl6_cell.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* The synthetic host is single-thread/quiescent, and owns real module-like
 * global slots. Native worker admission is tested separately below. Actual
 * generated classes/modules remain covered by their independent hard gates. */
typedef struct ClassOwner {
    XL6_CodeOwnerGraph descriptor;
    unsigned refs;
    bool closed, cleared;
    const void* claim;
    xvalue *roots, *retired;
    xerror *error, *retiredError;
} ClassOwner;
static unsigned freed, transactions, faults, finalizations, late_finalizations;
static unsigned mode, calls, late_calls;
static ClassOwner* current;
static xvalue *receiver, *published;
static uint64_t identity_hash(const xvalue* value, void* unused)
{ (void)unused; return (uint64_t)(uintptr_t)value; }
static bool identity_equal(const xvalue* a, const xvalue* b, void* unused)
{ (void)unused; return a == b; }
static int acquire(void* data)
{ ClassOwner* o=data; if(o->closed)return 0; ++o->refs; return 1; }
static void release(void* data)
{
    ClassOwner* o=data; assert(o->refs); if(--o->refs)return;
    assert(!o->roots && !o->retired && !o->error && !o->retiredError); xrtFree(o); ++freed;
}
static bool count(const void* data,size_t* result)
{ const ClassOwner* o=data; if(o->cleared)return false; *result=o->refs; return true; }
static bool trace(const void* data,xrtownershipvisitor visit,void* context)
{ const ClassOwner* o=data; return visit(xrtValueOwnership(o->roots),context) && visit(xrtErrorOwnership(o->error),context); }
static const xrtownershipops owner_ops={count,trace};
static bool hold(const void* data) { ++((ClassOwner*)data)->refs; return true; }
static void drop(const void* data) { release((void*)data); }
static bool claim(const void* data,const void* token)
{ ClassOwner* o=(ClassOwner*)data; if(o->claim && o->claim!=token)return false; o->claim=token; return true; }
static void restore(const void* data,const void* token)
{ ClassOwner* o=(ClassOwner*)data; assert(o->claim==token && !o->cleared); o->claim=NULL; }
static void clear(const void* data,const void* token)
{ ClassOwner* o=(ClassOwner*)data; assert(o->claim==token && !o->cleared); o->retired=o->roots; o->roots=NULL; o->retiredError=o->error; o->error=NULL; o->cleared=true; }
static bool finish(const void* data,const void* token)
{
    ClassOwner* o=(ClassOwner*)data; xvalue* retired=o->retired;
    assert(o->claim==token && o->cleared); o->retired=NULL; xrtValueRelease(retired);
    xrtErrorFree(o->retiredError); o->retiredError=NULL; return true;
}
static const xrtownershipadapterv1 owner_adapter={sizeof(owner_adapter),hold,drop,claim,restore,NULL,clear,finish};
static const xrtownershipadapterv1* resolve(xrtownershipref ref,void* unused)
{
    const xrtownershipadapterv1* adapter; (void)unused;
    if(ref.Ops==&owner_ops)return &owner_adapter;
    if((adapter=xl6ClosureOwnershipAdapterV1(ref)))return adapter;
    if((adapter=xl6CellOwnershipAdapterV1(ref)))return adapter;
    if((adapter=xl6CodeLeaseOwnershipAdapterV1(ref)))return adapter;
    if((adapter=xl6ClassOwnershipAdapterV1(ref)))return adapter;
    if((adapter=xrtValueCallableOwnershipAdapterV1(ref)))return adapter;
    if((adapter=xrtErrorOwnershipAdapterV1(ref)))return adapter;
    return xrtValueOwnershipAdapterV1(ref);
}
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; assert(xrtGetError()==NULL); xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
}
static bool late_finalize(xvalue* value,void* unused)
{
    (void)unused; assert(value && current->roots && !current->cleared && !late_calls++);
    ++late_finalizations; return true;
}
static bool finalize(xvalue* value,void* unused)
{
    xvalue* item; int64_t number=0; xrtownershipscope freeze={0}; bool updated;
    (void)unused; assert(value==receiver && current->roots && !current->cleared && !calls++);
    assert(xrtValueGetInt(xrtValueObjectGet(value,xrtStrView("n")),&number) && number==42);
    /* No inherited mutation/freeze spans semantic user code. A physical
     * finalizing object refuses BEFORE its Count/Trace or any identity call. */
    assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(!xl6ClassOwnershipAdapterV1(xrtValueOwnership(value)));
    assert(xrtOwnershipScopeEnd(&freeze));
    assert(!xrtValueRetain(value) && xrtErrorKind(xrtGetError())==XERR_STATE); xrtClearError();
    assert(!xrtValueClone(value) && xrtErrorKind(xrtGetError())==XERR_STATE); xrtClearError();
    xrtValueRelease(value); assert(xrtErrorKind(xrtGetError())==XERR_STATE); xrtClearError();
    assert(xl6ClassRuntimeV2Get()->BindIdentity(value,identity_hash,identity_equal));
    item=xrtValueInt(43); assert(item); updated=xrtValueObjectSet(value,xrtStrView("n"),item); xrtValueRelease(item);
    /* One REAL plan backing pin must not cause COW. A second actual shell
     * still has the original shared-finalizer mutation refusal semantics. */
    if(mode==2){assert(!updated && xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();}
    else assert(updated && !xrtGetError());
    if(mode==4){published=xrtValueRetain(xrtValueArrayGet(current->roots,1));assert(published);}
    if(mode==5){
        item=xl6ClassRuntimeV2Get()->Create(&current->descriptor.base,late_finalize);assert(item);
        item=xl6ClassRuntimeV2Get()->CommitTake(item);assert(item);
        assert(xrtValueObjectSetNew(value,xrtStrView("late"),item));
    }
    ++finalizations;
    if(mode==3)xrtSetErrorKind(XERR_RANGE);
    return mode!=3 && mode!=7; /* Mode 7 fails WITHOUT any diagnostic. */
}
static void entry(void) {}
static bool boxed_entry(void* data,const xrtcallframe* frame,xrtcallresult* result)
{
    XL6_Closure* closure=data; (void)frame; xvalue* object=*(xvalue**)closure->environment;
    return xrtCallResultPush(result,xrtValueObjectGet(object,xrtStrView("n")));
}
static ClassOwner* make(xvalue** weak)
{
    static const XL6_ClosureCaptureSlot slot={XL6_CLOSURE_CAPTURE_XVALUE,0,sizeof(xvalue*)};
    static const XL6_ClosureEnvironmentV1 layout={sizeof(layout),sizeof(xvalue*),1,&slot};
    ClassOwner* o=xrtCalloc(1,sizeof(*o)); xvalue* object; xvalue** environment; XL6_Closure* closure;
    assert(o); o->refs=1; o->descriptor=(XL6_CodeOwnerGraph){{sizeof(o->descriptor),o,acquire,release},0x584C36434F574E31ULL,&owner_ops};
    current=o; calls=late_calls=0; published=NULL;
    object=xl6ClassRuntimeV2Get()->Create(&o->descriptor.base,finalize); assert(object);
    assert(xrtValueTypeIdBind(object,0x177) && xl6ClassRuntimeV2Get()->BindIdentity(object,identity_hash,identity_equal));
    assert(xrtValueObjectSetNew(object,xrtStrView("n"),xrtValueInt(42)));
    object=xl6ClassRuntimeV2Get()->CommitTake(object); assert(object); receiver=object;
    *weak=xrtValueWeakRef(object); assert(*weak);
    environment=xrtMalloc(sizeof(*environment)); assert(environment); *environment=xrtValueRetain(object);
    closure=xl6ClosureRuntimeV2Get()->Create(&o->descriptor.base,entry,NULL,environment,&layout,NULL,NULL,0);assert(closure);
    o->roots=xrtValueArray(); assert(o->roots && xrtValueArrayAppendNew(o->roots,object));
    xvalue* boxed=xl6ClosureRuntimeV1Get()->Box(&o->descriptor.base,closure,boxed_entry,0x178);assert(boxed);
    xl6ClosureRuntimeV1Get()->Release(closure); assert(xrtValueArrayAppendNew(o->roots,boxed));
    if(mode==2)assert(xrtValueArrayAppendNew(o->roots,xrtValueClone(object)));
    if(mode==8){
        xerror* cause=xrtErrorCreate(XERR_RANGE,"class-plan",11,"immutable cause");
        xerror* error=xrtErrorWrap(cause,XERR_STATE,"class-plan",12,"immutable error");
        assert(cause && error);
        o->error=error; xrtErrorFree(cause);
    }
    xrtcallresult result=XRT_CALL_RESULT_INIT; assert(xrtValueInvoke(boxed,NULL,&result));
    int64_t actual=0;assert(xrtCallResultCount(&result)==1 && xrtValueGetInt(xrtCallResultGet(&result,0),&actual) && actual==42);
    xrtCallResultClear(&result); o->closed=true; release(o); assert(o->refs==3);
    xrtownershipref root={o,&owner_ops}; xrtownershipresult graph={0};
    assert(xrtOwnershipInspect(&root,1,NULL,0,&graph));
    assert(graph.NodeCount==(mode==2?14u:mode==8?15u:13u) &&
        graph.EdgeCount==(mode==2?18u:mode==8?18u:16u) && !graph.ExternalRootCount && !graph.ReachableAnchorCount);
    return o;
}
static XL6_OwnershipPlan* prepare(ClassOwner* o)
{
    XL6_OwnershipPlan* plan=NULL; XL6_OwnershipPlanResult result=xl6OwnershipPlanTryCreate((xrtownershipref){o,&owner_ops},resolve,NULL,&plan);
    if(result!=XL6_OWNERSHIP_PLAN_FINALIZERS)fprintf(stderr,"class prepare mode=%u result=%d error=%s\n",mode,(int)result,xrtGetError()?xrtErrorMessage(xrtGetError()):"none");
    assert(result==XL6_OWNERSHIP_PLAN_FINALIZERS && plan); return plan;
}
static void abort_plan(XL6_OwnershipPlan* plan)
{ assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY); }
static void complete(XL6_OwnershipPlan* plan)
{
    XL6_OwnershipPlanResult result;
    do { assert(xl6OwnershipPlanFinalize(plan)); result=xl6OwnershipPlanTryCommit(plan); }
    while(result==XL6_OWNERSHIP_PLAN_FINALIZERS);
    assert(result==XL6_OWNERSHIP_PLAN_READY && xl6OwnershipPlanFinish(plan)); abort_plan(plan);
}
static void transaction(unsigned choice)
{
    xmemdebugsnapshot before; xvalue* weak=NULL; XL6_OwnershipPlan* plan=NULL;
    mode=choice; xrtMemDebugSnapshot(&before); ClassOwner* o=make(&weak);
    if(mode==1){
        xvalue* external=xrtValueClone(o->roots); assert(external);
        assert(xl6OwnershipPlanTryCreate((xrtownershipref){o,&owner_ops},resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ROOTED);
        assert(!calls);abort_plan(plan);xrtValueRelease(external);
    }
    plan=prepare(o); xvalue* locked=xrtValueWeakRefLock(weak);
    assert(locked && xrtValueType(locked)==XVALUE_NULL);xrtValueRelease(locked);
    if(mode==3 || mode==7){
        assert(!xl6OwnershipPlanFinalize(plan) &&
            xrtErrorKind(xrtGetError())==(mode==3?XERR_RANGE:XERR_NONE) && calls==1);
        xrtClearError();abort_plan(plan);plan=prepare(o);
    }else if(mode==4){
        assert(xl6OwnershipPlanFinalize(plan) && published && calls==1);
        assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_ROOTED && !o->cleared);
        abort_plan(plan);xrtValueRelease(published);published=NULL;plan=prepare(o);
    }else if(mode==6){
        assert(xl6OwnershipPlanFinalize(plan) && xl6OwnershipPlanFinalize(plan) && calls==1);
        abort_plan(plan);plan=prepare(o);
    }
    xerror* alias=mode==8?xrtErrorRef(o->error):NULL;
    complete(plan); assert(calls==1 && late_calls==(mode==5?1u:0u)); xrtValueRelease(weak);
    if(alias){assert(xrtErrorCode(alias)==12 && xrtErrorCode(xrtErrorCause(alias))==11);xrtErrorFree(alias);}
    current=NULL;receiver=NULL;balance(&before);++transactions;
}
static void plan_fault(unsigned point,bool commit)
{
    xmemdebugsnapshot before; xvalue* weak=NULL; XL6_OwnershipPlan* plan=NULL; XL6_OwnershipPlanResult result;
    mode=0;xrtMemDebugSnapshot(&before);ClassOwner* o=make(&weak);
    if(commit){plan=prepare(o);assert(xl6OwnershipPlanFinalize(plan));}
    assert(xrtMemDebugFailAfter(point));
    result=commit?xl6OwnershipPlanTryCommit(plan):xl6OwnershipPlanTryCreate((xrtownershipref){o,&owner_ops},resolve,NULL,&plan);
    bool failed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xrtClearError();
    if(failed){++faults;assert(result==XL6_OWNERSHIP_PLAN_ERROR && !o->cleared);if(plan)abort_plan(plan);plan=NULL;}
    if(!plan)plan=prepare(o);
    if(!commit || failed)complete(plan);
    else{assert(result==XL6_OWNERSHIP_PLAN_READY && xl6OwnershipPlanFinish(plan));abort_plan(plan);}
    assert(calls==1);xrtValueRelease(weak);current=NULL;receiver=NULL;balance(&before);
}
static unsigned legacy_finalized,legacy_traced;
static bool poison_trace(const void* data,xrtownershipvisitor visitor,void* context)
{ (void)data;(void)visitor;(void)context;++legacy_traced;abort(); }
static void legacy_finalize(xvalue* value,void* data) { (void)value;(void)data;++legacy_finalized; }
static void legacy_release(void* data) { (void)data; }
static void invalid_policies(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);xvalue* value=xrtValueObject();XL6_OwnershipPlan* plan=NULL;
    assert(value && xrtValueObjectFinalizerBindOwned(value,legacy_finalize,&legacy_finalized,poison_trace,legacy_release));
    assert(xl6OwnershipPlanTryCreate(xrtValueOwnership(value),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ERROR && !plan && !legacy_traced);
    xrtClearError();xrtValueRelease(value);assert(legacy_finalized==1);balance(&before);
}
typedef struct Active { xatomic32 entered,leave; xrtcallable* callable; } Active;
static bool active_entry(void* data,const xrtcallframe* frame,xrtcallresult* result)
{
    Active* a=data;(void)frame;(void)result;xrtAtomic32Store(&a->entered,1,XMEMORY_RELEASE);
    /* The pinned snapshot uses microseconds. Keep a five-second handshake
     * budget rather than a scheduler-sensitive five-millisecond deadline. */
    xdeadline deadline=xrtDeadlineAfter(5*XRT_TIME_SECOND);
    while(!xrtAtomic32Load(&a->leave,XMEMORY_ACQUIRE)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    return true;
}
static void active_drop(void* data) { (void)data; }
static const xrtcallableownershipv1 active_policy={sizeof(active_policy),poison_trace,active_drop};
static const xrtownershipadapterv1* active_resolve(xrtownershipref ref,void* unused)
{ (void)unused;return xrtCallableOwnershipAdapterV1(ref,&active_policy); }
static int32 worker(void* data)
{
    Active* a=data;xrtcallresult result=XRT_CALL_RESULT_INIT;
    assert(xrtCallableInvoke(a->callable,NULL,&result));xrtCallResultClear(&result);return 0;
}
static void active_native(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Active a={0};xrtownershipscope freeze={0};XL6_OwnershipPlan* plan=NULL;
    a.callable=xrtCallableCreate(NULL,active_entry,&a,active_drop);assert(a.callable && xrtCallableOwnershipBindV1(a.callable,&active_policy));
    xthread* thread=xrtThreadCreate(worker,&a,0);assert(thread);xdeadline deadline=xrtDeadlineAfter(5*XRT_TIME_SECOND);
    while(!xrtAtomic32Load(&a.entered,XMEMORY_ACQUIRE)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    while(!xrtOwnershipFreezeTryBegin(&freeze)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
    assert(!xrtCallableOwnershipAdapterV1(xrtCallableOwnership(a.callable),&active_policy));
    assert(xrtOwnershipScopeEnd(&freeze));
    assert(xl6OwnershipPlanTryCreate(xrtCallableOwnership(a.callable),active_resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ERROR && !plan && !legacy_traced);xrtClearError();
    xrtAtomic32Store(&a.leave,1,XMEMORY_RELEASE);assert(xrtThreadWaitFor(thread,5*XRT_TIME_SECOND)==XWAIT_OK);
    xrtThreadDestroy(thread);xrtCallableUnref(a.callable);balance(&before);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    for(unsigned round=0;round<50;++round)for(unsigned choice=0;choice<9;++choice)transaction(choice);
    for(unsigned point=0;point<48;++point){plan_fault(point,false);plan_fault(point,true);}
    invalid_policies();for(unsigned round=0;round<50;++round)active_native();
    assert(transactions==450 && freed==546 && finalizations==546 && late_finalizations==50 && faults>0 && !legacy_traced);
    printf("Class/callable plans: 450 exact physical graphs, 546 once-only receiver finalizers, 50 late duties, 96 preparation/commit probes with %u real failures, 50 native active pre-trace refusals; real COW aliases, weak quarantine, diagnostic-free failure, immutable error causes, child publication and abort preserve state; zero live delta\n",faults);
    return 0;
}
