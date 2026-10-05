#define XRUNTIME_MODULE_RUNTIME_CALL
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_array_value.h"
#include "src/runtime/xl6_array_iterator.h"
#include "src/runtime/xl6_closure.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REF(id,element) id,element,3,sizeof(xvalue*),xrtTypeValue()
typedef struct Spec { uint64_t owner,element,signature; bool nullable; } Spec;
typedef struct Owner { unsigned pins,acquired,released; bool closed; } Owner;
typedef struct Reentry { xvalue* array; xvalue* self; } Reentry;
static Spec spec(unsigned profile)
{
    assert(profile<4); uint64_t signature=1001+profile/2;
    return (Spec){2001+profile,profile%2 ? 3001+profile : signature,signature,!!(profile%2)};
}
#define LAYOUT(s) REF((s)->owner,(s)->element)
static int acquire(void* opaque)
{ Owner* owner=opaque; if (owner->closed) return 0; ++owner->pins; ++owner->acquired; return 1; }
static void release(void* opaque)
{ Owner* owner=opaque; assert(owner->pins); --owner->pins; ++owner->released; }
static void error(xerrkind kind)
{ assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind); xrtClearError(); }
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount &&
        after.UseAfterFreeCount==before->UseAfterFreeCount && !xrtGetError());
}
static void invoke(void) { }
static bool entry(void* opaque,const xrtcallframe* frame,xrtcallresult* result)
{
    XL6_Closure* closure=opaque;
    xvalue* capture=*(xvalue**)closure->environment;
    assert(capture);
    if (frame->Context) {
        Reentry* reentry=frame->Context;
        /* Domain admission must not reject an actively invoked callable. */
        assert(xl6ArrayErasedPushV1(reentry->array,reentry->self));
    }
    xstrview text; assert(xrtValueGetString(capture,&text) && text.Size==5 &&
        !memcmp(text.Data,"a\0你",5));
    xvalue* value=xrtValueInt((int64_t)text.Size);
    bool ok=value && xrtCallResultSetTake(result,0,&value); xrtValueRelease(value); return ok;
}
static const XL6_ClosureCaptureSlot captureSlot={XL6_CLOSURE_CAPTURE_XVALUE,0,sizeof(xvalue*)};
static const XL6_ClosureEnvironmentV1 captureLayout={sizeof(captureLayout),sizeof(xvalue*),1,&captureSlot};
static XL6_Closure* closure(const XL6_CodeOwner* owner,xvalue* capture)
{
    xvalue** environment=xrtMalloc(sizeof(*environment)); assert(environment);
    *environment=xrtValueRetain(capture); assert(*environment);
    XL6_Closure* result=xl6ClosureRuntimeV2Get()->Create(owner,invoke,NULL,environment,
        &captureLayout,NULL,NULL,0);
    if (!result) xl6ClosureRuntimeV2Get()->DropEnvironment(&captureLayout,environment);
    return result;
}
static xvalue* callable(uint64_t signature,Owner* state)
{
    XL6_CodeOwner owner={sizeof(owner),state,acquire,release};
    xvalue* text=xrtValueString((xstrview){"a\0你",5}); assert(text);
    XL6_Closure* source=closure(state ? &owner : NULL,text); assert(source); xrtValueRelease(text);
    xvalue* result=xl6ClosureRuntimeV1Get()->Box(state ? &owner : NULL,source,entry,signature);
    xl6ClosureRuntimeV1Get()->Release(source); assert(result); return result;
}
static xvalue* create(const Spec* s)
{ return xl6ArrayCallableCreateV1(s->owner,s->element,s->signature,s->nullable); }
static int64_t projectInvoke(void* environment)
{
    xvalue* value=*(xvalue**)environment; xrtcallframe frame={0};
    xrtcallresult result=XRT_CALL_RESULT_INIT; int64_t number=0;
    assert(xrtCallableInvoke(xrtValueGetCallable(value),&frame,&result));
    assert(xrtCallResultCount(&result)==1 && xrtValueGetInt(xrtCallResultGet(&result,0),&number));
    xrtCallResultClear(&result); return number;
}
static void (*projectEntry(void))(void)
{
    /* As in the typed invocation ABI, encode a function address without
     * inventing a data-pointer cast or an array-specific callback shim. */
    int64_t (*typed)(void*)=projectInvoke; void (*address)(void);
    _Static_assert(sizeof(address)==sizeof(typed),"closure function address ABI");
    memcpy(&address,&typed,sizeof(address)); return address;
}
static bool projectBoxEntry(void* opaque,const xrtcallframe* frame,xrtcallresult* result)
{
    (void)frame; XL6_Closure* source=opaque;
    xvalue* number=xrtValueInt(projectInvoke(source->environment));
    bool ok=number && xrtCallResultSetTake(result,0,&number); xrtValueRelease(number); return ok;
}
static void project(xvalue* value)
{
    XL6_Closure* typed=xl6ClosureRuntimeV2Get()->CreateValue(NULL,projectEntry(),value); assert(typed);
    XL6_ClosureScope scope={0}; xl6ClosureRuntimeV1Get()->Enter(&scope,typed);
    int64_t (*invokeTyped)(void*); memcpy(&invokeTyped,&typed->invoke,sizeof(invokeTyped));
    assert(invokeTyped(typed->environment)==5);
    xl6ClosureRuntimeV1Get()->Release(typed); /* Invocation owns the return tail. */
    xl6ClosureRuntimeV1Get()->Leave(&scope);
}
static void call(xvalue* value,Reentry* context)
{
    assert(xrtValueIsCallable(value));
    xrtcallframe frame={.Context=context}; xrtcallresult result=XRT_CALL_RESULT_INIT;
    assert(xrtCallableInvoke(xrtValueGetCallable(value),&frame,&result));
    int64_t number=0;
    assert(xrtCallResultCount(&result)==1 && xrtValueGetInt(xrtCallResultGet(&result,0),&number) && number==5);
    xrtCallResultClear(&result);
}
static void refused(const Spec* s,xvalue* array,xvalue* wrong)
{
    int64_t size=xl6ArrayErasedSizeV1(array);
    assert(xrtMemDebugFailAfter(0));
    assert(!xl6ArrayPushV1(array,LAYOUT(s),&wrong)); error(XERR_TYPE);
    assert(!xl6ArraySetV1(array,LAYOUT(s),0,&wrong)); error(XERR_TYPE);
    assert(!xl6ArrayInsertV1(array,LAYOUT(s),0,&wrong)); error(XERR_TYPE);
    if (wrong) {
        assert(!xl6ArrayErasedPushV1(array,wrong)); error(XERR_TYPE);
        assert(!xl6ArrayErasedSetV1(array,0,wrong)); error(XERR_TYPE);
        assert(!xl6ArrayErasedInsertV1(array,0,wrong)); error(XERR_TYPE);
    }
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    assert(xl6ArrayErasedSizeV1(array)==size);
}
static unsigned foreignDrops;
static bool unboundEntry(void* opaque,const xrtcallframe* frame,xrtcallresult* result)
{ (void)opaque; (void)frame; xrtCallResultUnit(result); return true; }
static void foreignDrop(void* data,void* context)
{ (void)context; assert(data==(void*)(uintptr_t)1); ++foreignDrops; }
static void vectors(const Spec* s)
{
    assert(!xl6ArrayCallableCreateV1(0,s->element,s->signature,s->nullable)); error(XERR_ARGUMENT);
    assert(!xl6ArrayCallableCreateV1(s->owner,0,s->signature,s->nullable)); error(XERR_ARGUMENT);
    assert(!xl6ArrayCallableCreateV1(s->owner,s->element,0,s->nullable)); error(XERR_ARGUMENT);
    assert(!xl6ArrayCallableCreateV1(s->owner,s->signature,s->signature,true)); error(XERR_ARGUMENT);
    assert(!xl6ArrayCallableCreateV1(s->owner,s->signature+10,s->signature,false)); error(XERR_ARGUMENT);
    Owner state={0}; xvalue* good=callable(s->signature,&state),*array=create(s); assert(array);
    assert(state.pins==2 && state.acquired==2 && !state.released);
    assert(xl6ArrayPushV1(array,LAYOUT(s),&good));
    xvalue* alias=xl6ArrayEraseV1(array,LAYOUT(s),NULL); assert(alias==array);
    xvalue* wrong=callable(s->signature+10,NULL); refused(s,alias,wrong); xrtValueRelease(wrong);
    xrtcallable* unbound=xrtCallableCreate(NULL,unboundEntry,NULL,NULL); assert(unbound);
    wrong=xrtValueCallableTake(&unbound); assert(wrong && !unbound);
    refused(s,alias,wrong); xrtValueRelease(wrong);
    wrong=xrtValueInt(5); assert(wrong && xrtValueTypeIdBind(wrong,s->signature));
    refused(s,alias,wrong); xrtValueRelease(wrong);
    static const xvaluehandleops foreign={.Drop=foreignDrop};
    void* poison=(void*)(uintptr_t)1;
    wrong=xrtValueHandleTake(&poison,&foreign,NULL); assert(wrong && xrtValueTypeIdBind(wrong,s->signature));
    unsigned drops=foreignDrops; refused(s,alias,wrong); assert(foreignDrops==drops);
    xrtValueRelease(wrong); assert(foreignDrops==drops+1);
    if (!s->nullable) { refused(s,alias,NULL); refused(s,alias,xrtValueNull()); }
    else {
        xvalue* absent=NULL;
        assert(xl6ArrayPushV1(array,LAYOUT(s),&absent) && xl6ArrayErasedPushV1(alias,xrtValueNull()));
    }
    Reentry reentry={array,good}; call(good,&reentry);
    xvalue* copy=xl6ArrayErasedCloneV1(alias),*snapshot=xl6ArrayToValueV1(array,LAYOUT(s),NULL);
    xvalue* cursor=xl6ArrayIteratorCreateV1(array,LAYOUT(s),999); assert(copy && snapshot && cursor);
    assert(xrtValueArrayAt(snapshot,0)==good);
    state.closed=true; xrtValueRelease(good); xrtValueRelease(alias); xrtValueRelease(array);
    assert(state.pins==2 && state.acquired==2);
    xvalue* read=xl6ArrayErasedReadV1(copy,0); assert(read==good); call(read,NULL); project(read);
    assert(xl6ArrayIteratorAdvanceV1(cursor,REF(999,s->element)));
    xvalue* item=NULL; assert(xl6ArrayIteratorReadV1(cursor,REF(999,s->element),&item) && item==good);
    call(item,NULL); xrtValueRelease(item);
    bool grown=xl6ArrayResizeV1(copy,LAYOUT(s),9);
    if (s->nullable) {
        assert(grown); item=xl6ArrayErasedReadV1(copy,8);
        assert(item && xrtValueType(item)==XVALUE_NULL); xrtValueRelease(item);
    } else { assert(!grown); error(XERR_TYPE); }
    item=xl6ArrayErasedTakeV1(copy,0); assert(item==good); call(item,NULL); xrtValueRelease(item);
    xrtValueRelease(cursor); xrtValueRelease(snapshot); xrtValueRelease(copy);
    assert(state.pins==2); xrtValueRelease(read);
    assert(!state.pins && state.acquired==state.released);
}
static const xrtownershipadapterv1* resolve(xrtownershipref ref,void* context)
{
    (void)context;
    const xrtownershipadapterv1* a=xl6SequenceOwnershipAdapterV1(ref);
    if (!a) a=xrtValueCallableOwnershipAdapterV1(ref);
    if (!a) a=xl6ClosureOwnershipAdapterV1(ref);
    return a ? a : xrtValueOwnershipAdapterV1(ref);
}
static void graph(void)
{
    Spec s=spec(0); xvalue* array=create(&s); assert(array);
    XL6_Closure* captured=closure(NULL,array); assert(captured);
    xvalue* function=xl6ClosureRuntimeV1Get()->Box(NULL,captured,entry,s.signature); assert(function);
    xl6ClosureRuntimeV1Get()->Release(captured);
    assert(xl6ArrayPushV1(array,LAYOUT(&s),&function));
    xvalue* wa=xrtValueWeakRef(array),*wf=xrtValueWeakRef(function); assert(wa && wf);
    xrtValueRelease(function); xrtValueRelease(array);
    XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreate(xrtValueOwnership(array),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY && xl6OwnershipPlanFinish(plan));
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xrtValueWeakRefExpired(wa) && xrtValueWeakRefExpired(wf));
    xrtValueRelease(wa); xrtValueRelease(wf);
}
static void fault(const Spec* s,const char* operation,size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    Owner state={0}; xvalue* good=callable(s->signature,&state),*array=NULL,*alias=NULL,*result=NULL;
    if (strcmp(operation,"create")) {
        array=create(s); assert(array && xl6ArrayPushV1(array,LAYOUT(s),&good));
        alias=xl6ArrayEraseV1(array,LAYOUT(s),NULL); assert(alias);
    }
    XL6_Closure* projection=NULL;
    XL6_Closure* source=!strcmp(operation,"box") ?
        xl6ClosureRuntimeV2Get()->CreateValue(NULL,projectEntry(),good) : NULL;
    if (!strcmp(operation,"box")) assert(source);
    assert(xrtMemDebugFailAfter(point)); bool ok;
    if (!strcmp(operation,"create")) { result=create(s); ok=result!=NULL; }
    else if (!strcmp(operation,"clone")) { result=xl6ArrayCloneV1(array,LAYOUT(s)); ok=result!=NULL; }
    else if (!strcmp(operation,"erased-clone")) { result=xl6ArrayErasedCloneV1(alias); ok=result!=NULL; }
    else if (!strcmp(operation,"cursor")) { result=xl6ArrayIteratorCreateV1(array,LAYOUT(s),999); ok=result!=NULL; }
    else if (!strcmp(operation,"snapshot")) { result=xl6ArrayToValueV1(array,LAYOUT(s),NULL); ok=result!=NULL; }
    else if (!strcmp(operation,"read")) { result=xl6ArrayErasedReadV1(alias,0); ok=result!=NULL; }
    else if (!strcmp(operation,"take")) { result=xl6ArrayErasedTakeV1(alias,0); ok=result!=NULL; }
    else if (!strcmp(operation,"push")) ok=xl6ArrayErasedPushV1(alias,good);
    else if (!strcmp(operation,"set")) ok=xl6ArrayErasedSetV1(alias,0,good);
    else if (!strcmp(operation,"insert")) ok=xl6ArrayErasedInsertV1(alias,0,good);
    else if (!strcmp(operation,"reserve")) ok=xl6ArrayReserveV1(array,LAYOUT(s),257);
    else if (!strcmp(operation,"project")) {
        projection=xl6ClosureRuntimeV2Get()->CreateValue(NULL,projectEntry(),good); ok=projection!=NULL;
    } else if (!strcmp(operation,"box")) {
        XL6_CodeOwner owner={sizeof(owner),&state,acquire,release};
        result=xl6ClosureRuntimeV1Get()->Box(&owner,source,projectBoxEntry,s->signature); ok=result!=NULL;
    }
    else { assert(!strcmp(operation,"resize") && s->nullable); ok=xl6ArrayResizeV1(array,LAYOUT(s),257); }
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if (hit) {
        assert(!ok); error(XERR_MEMORY);
        if (array) {
            assert(xl6ArrayErasedSizeV1(array)==1);
            xvalue* read=xl6ArrayErasedReadV1(alias,0); assert(read==good); call(read,NULL); xrtValueRelease(read);
        }
    } else assert(ok && !xrtGetError());
    state.closed=true;
    xl6ClosureRuntimeV1Get()->Release(source); xl6ClosureRuntimeV1Get()->Release(projection);
    xrtValueRelease(result); xrtValueRelease(alias); xrtValueRelease(array); xrtValueRelease(good);
    assert(!state.pins && state.acquired==state.released);
    balanced(&before); printf("operation=%s point=%zu hit=%d balanced\n",operation,point,(int)hit);
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true));
    if (argc==4) { Spec s=spec((unsigned)strtoul(argv[1],NULL,10)); fault(&s,argv[2],(size_t)strtoull(argv[3],NULL,10)); return 0; }
    assert(argc==1);
    for (unsigned p=0;p<4;++p) {
        Spec s=spec(p); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); vectors(&s); balanced(&before);
    }
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); graph(); balanced(&before);
    puts("4 callable Array domains, exact signature, active invocation, code leases and cyclic captures balanced"); return 0;
}
