#define XRUNTIME_MODULE_RUNTIME_CALL
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#include "src/runtime/xl6_stack_callable.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_sequence.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SIGNATURE UINT64_C(10101)
#define ID(N) ((N)?UINT64_C(10202):UINT64_C(10201))
#define ELEMENT(N) ((N)?UINT64_C(10102):SIGNATURE)
#define LAYOUT(N) ID(N),ELEMENT(N),3,sizeof(xvalue*),xrtTypeValue()
typedef struct Owner{unsigned pins,acquired,released;bool closed,poison;}Owner;
static unsigned prefixes;
static int acquire(void* raw){Owner* o=raw;if(o->closed)return 0;++o->pins;++o->acquired;if(o->poison)xrtSetErrorKind(XERR_STATE);return 1;}
static void release(void* raw){Owner* o=raw;assert(o->pins);--o->pins;++o->released;}
static void invoke(void){}
static bool entry(void* raw,const xrtcallframe* frame,xrtcallresult* result)
{
    (void)frame;XL6_Closure* closure=raw;xvalue* value=*(xvalue**)closure->environment;
    assert(value);xvalue* number=xrtValueInt(17);bool ok=number && xrtCallResultSetTake(result,0,&number);
    xrtValueRelease(number);return ok;
}
static const XL6_ClosureCaptureSlot captureSlot={XL6_CLOSURE_CAPTURE_XVALUE,0,sizeof(xvalue*)};
static const XL6_ClosureEnvironmentV1 captureLayout={sizeof(captureLayout),sizeof(xvalue*),1,&captureSlot};
static XL6_Closure* closure(const XL6_CodeOwner* owner,xvalue* value)
{
    xvalue** environment=xrtMalloc(sizeof(*environment));assert(environment);
    *environment=xrtValueRetain(value);assert(*environment);
    XL6_Closure* c=xl6ClosureRuntimeV2Get()->Create(owner,invoke,NULL,environment,&captureLayout,NULL,NULL,0);
    if(!c)xl6ClosureRuntimeV2Get()->DropEnvironment(&captureLayout,environment);
    return c;
}
static xvalue* box(Owner* state)
{
    XL6_CodeOwner owner={sizeof(owner),state,acquire,release};xvalue* value=xrtValueInt(17);assert(value);
    XL6_Closure* c=closure(state?&owner:NULL,value);assert(c);xrtValueRelease(value);
    value=xl6ClosureRuntimeV1Get()->Box(state?&owner:NULL,c,entry,SIGNATURE);xl6ClosureRuntimeV1Get()->Release(c);assert(value);return value;
}
static int64_t projectInvoke(void* environment)
{
    xvalue* value=*(xvalue**)environment;xrtcallframe frame={0};xrtcallresult result=XRT_CALL_RESULT_INIT;int64_t n=0;
    assert(xrtCallableInvoke(xrtValueGetCallable(value),&frame,&result));
    assert(xrtCallResultCount(&result)==1 && xrtValueGetInt(xrtCallResultGet(&result,0),&n));xrtCallResultClear(&result);return n;
}
static void (*projectEntry(void))(void)
{
    int64_t (*typed)(void*)=projectInvoke;void (*address)(void);
    _Static_assert(sizeof(address)==sizeof(typed),"closure function address ABI");memcpy(&address,&typed,sizeof(address));return address;
}
static void call(XL6_Closure* c)
{
    XL6_ClosureScope scope={0};xl6ClosureRuntimeV1Get()->Enter(&scope,c);
    int64_t (*typed)(void*);memcpy(&typed,&c->invoke,sizeof(typed));assert(typed(c->environment)==17);
    xl6ClosureRuntimeV1Get()->Leave(&scope);
}
static xvalue* create(bool nullable)
{return xl6SequenceCallableCreateV1(XL6_SEQUENCE_STACK_V1,ID(nullable),ELEMENT(nullable),SIGNATURE,nullable);}
static bool take(xvalue* stack,bool nullable,bool pop,XL6_Closure** out,bool* found,const XL6_CodeOwner* owner)
{return xl6StackCallableTakeV1(stack,ID(nullable),ELEMENT(nullable),SIGNATURE,nullable,0,pop,owner,projectEntry(),out,found);}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount &&
        after.UseAfterFreeCount==before->UseAfterFreeCount && !xrtGetError());
}
static void vectors(bool nullable)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Owner producer={0},consumer={0};
    XL6_CodeOwner owner={sizeof(owner),&consumer,acquire,release};xvalue* item=box(&producer),*stack=create(nullable);assert(stack);
    assert(xl6StackPushV1(stack,LAYOUT(nullable),&item));xrtValueRelease(item);
    XL6_Closure* out=NULL;bool found=false;
    xvalue* fake=xrtValueObject();assert(fake && xrtValueTypeIdBind(fake,SIGNATURE));
    assert(!xl6StackPushV1(stack,LAYOUT(nullable),&fake));xrtClearError();xrtValueRelease(fake);
    if(nullable){assert(!xl6StackCallableTakeV1(stack,ID(nullable),ELEMENT(nullable),SIGNATURE+99,true,0,true,NULL,projectEntry(),&out,&found) && !out && !found);xrtClearError();}
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(stack,XL6_SEQUENCE_STACK_V1,LAYOUT(nullable));assert(pin);
    xvalue* const* raw=xrtTypedStackConstPeek(xl6SequenceStorageV1(pin),0);xvalue* saved=*raw;xl6SequenceEndV1(pin);
    assert(!take(stack,nullable,false,&out,(bool*)raw,NULL) && !out);xrtClearError();
    pin=xl6SequenceBeginRoleV1(stack,XL6_SEQUENCE_STACK_V1,LAYOUT(nullable));assert(pin);
    assert(*(xvalue* const*)xrtTypedStackConstPeek(xl6SequenceStorageV1(pin),0)==saved);xl6SequenceEndV1(pin);
    consumer.closed=true;assert(!take(stack,nullable,true,&out,&found,&owner) && !out && !found);
    consumer.closed=false;xrtClearError();assert(xl6StackSizeV1(stack,LAYOUT(nullable),false)==1);
    consumer.poison=true;assert(!take(stack,nullable,true,&out,&found,&owner) && !out && !found);
    consumer.poison=false;xrtClearError();assert(xl6StackSizeV1(stack,LAYOUT(nullable),false)==1);
    assert(take(stack,nullable,true,&out,&found,&owner) && found && out);
    xrtValueRelease(stack);assert(producer.pins && consumer.pins);call(out);xl6ClosureRuntimeV1Get()->Release(out);out=NULL;
    assert(!producer.pins && !consumer.pins && producer.acquired==producer.released && consumer.acquired==consumer.released);
    stack=create(nullable);assert(stack);
    assert(take(stack,nullable,false,&out,&found,NULL) && !found && !out);
    assert(!take(stack,nullable,false,&out,NULL,NULL));xrtClearError();
    xvalue* nullValue=xrtValueNull();assert(xl6StackPushV1(stack,LAYOUT(nullable),&nullValue)==nullable);xrtClearError();
    if(nullable){assert(take(stack,nullable,true,&out,&found,NULL) && found && !out);}
    xrtValueRelease(stack);balanced(&before);
}
static void sweep(bool nullable,bool pop)
{
    for(size_t point=0;point<256;++point){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);Owner producer={0},consumer={0};XL6_CodeOwner owner={sizeof(owner),&consumer,acquire,release};
        xvalue* item=box(&producer),*stack=create(nullable);assert(stack && xl6StackPushV1(stack,LAYOUT(nullable),&item));xrtValueRelease(item);
        XL6_Sequence* pin=xl6SequenceBeginRoleV1(stack,XL6_SEQUENCE_STACK_V1,LAYOUT(nullable));assert(pin);
        xarray original=xl6SequenceStorageV1(pin)->Storage;xl6SequenceEndV1(pin);XL6_Closure* out=NULL;bool found=false;
        assert(xrtMemDebugFailAfter(point));bool ok=take(stack,nullable,pop,&out,&found,&owner);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();assert(ok==!hit);++prefixes;
        if(hit){assert(!found && !out);xrtClearError();pin=xl6SequenceBeginRoleV1(stack,XL6_SEQUENCE_STACK_V1,LAYOUT(nullable));assert(pin);
            const xarray* after=&xl6SequenceStorageV1(pin)->Storage;
            assert(after->Count==original.Count && after->Capacity==original.Capacity && after->Data==original.Data && after->Allocation==original.Allocation);
            xl6SequenceEndV1(pin);assert(take(stack,nullable,pop,&out,&found,&owner));}
        assert(found && out);xrtValueRelease(stack);call(out);xl6ClosureRuntimeV1Get()->Release(out);
        assert(!producer.pins && !consumer.pins && producer.acquired==producer.released && consumer.acquired==consumer.released);balanced(&before);
        if(!hit)break;
        assert(point+1<256);
    }
}
static const xrtownershipadapterv1* resolve(xrtownershipref ref,void* context)
{
    (void)context;const xrtownershipadapterv1* a=xl6SequenceOwnershipAdapterV1(ref);
    if(!a)a=xrtValueCallableOwnershipAdapterV1(ref);
    if(!a)a=xl6ClosureOwnershipAdapterV1(ref);
    return a?a:xrtValueOwnershipAdapterV1(ref);
}
static void graph(bool nullable)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);xvalue* stack=create(nullable);assert(stack);
    XL6_Closure* captured=closure(NULL,stack);assert(captured);
    xvalue* item=xl6ClosureRuntimeV1Get()->Box(NULL,captured,entry,SIGNATURE);assert(item);xl6ClosureRuntimeV1Get()->Release(captured);
    assert(xl6StackPushV1(stack,LAYOUT(nullable),&item));xvalue* weak=xrtValueWeakRef(stack);assert(weak);
    xrtValueRelease(item);xrtValueRelease(stack);XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreate(xrtValueOwnership(stack),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY && xl6OwnershipPlanFinish(plan));
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY && xrtValueWeakRefExpired(weak));xrtValueRelease(weak);balanced(&before);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));for(unsigned i=0;i<100;++i){vectors(false);vectors(true);}
    for(unsigned n=0;n<2;++n){sweep(n!=0,false);sweep(n!=0,true);graph(n!=0);}
    printf("callable Stack 200 lifetimes, 2 graphs, projection prefixes=%u balanced\n",prefixes);return 0;
}
