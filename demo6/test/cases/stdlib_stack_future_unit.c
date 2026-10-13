#define XRUNTIME_MODULE_RUNTIME_TYPE_FUTURE
#define XRUNTIME_MODULE_RUNTIME_VALUE_FUTURE
#include "src/runtime/xl6_stack.h"
#include "src/runtime/xl6_stack_batch.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_ownership_plan.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define BASE UINT64_C(9701)
#define ELEMENT(N) ((N)?UINT64_C(9702):BASE)
#define ID(N) ((N)?UINT64_C(9802):UINT64_C(9801))
#define ARRAY_ID(N) (ID(N)+100)
#define PHYSICAL(N) ELEMENT(N),XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1,sizeof(xfuture*),xrtTypeFuture()
#define STACK(N) ID(N),PHYSICAL(N)
#define ARRAY(N) ARRAY_ID(N),PHYSICAL(N)
static unsigned prefixes;
static bool failCopy;
extern bool __real_xrtTypeCopyValue(const xrttype*,void*,const void*);
bool __wrap_xrtTypeCopyValue(const xrttype* type,void* out,const void* source)
{
    if(failCopy && type==xrtTypeFuture()){xrtSetErrorKind(XERR_MEMORY);return false;}
    return __real_xrtTypeCopyValue(type,out,source);
}
static xfuture* ready(void)
{
    xfuture* future=NULL;xpromise* promise=xrtPromiseCreate(&future,NULL);assert(promise && future);
    assert(xrtPromiseResolve(promise,NULL));xrtPromiseDestroy(promise);return future;
}
static xvalue* create(bool nullable,XL6_SequenceRoleV1 role)
{return xl6SequenceFutureCreateV1(role,role==XL6_SEQUENCE_STACK_V1?ID(nullable):ARRAY_ID(nullable),ELEMENT(nullable),BASE,nullable);}
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
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    xfuture* input=ready(),*out=NULL,*absent=NULL;xvalue* stack=create(nullable,XL6_SEQUENCE_STACK_V1);assert(stack);
    assert(xl6StackPushV1(stack,STACK(nullable),&input));
    assert(xl6StackPushV1(stack,STACK(nullable),&absent)==nullable);xrtClearError();
    bool found=false;
    if(nullable){assert(xl6StackTryTakeV2(stack,STACK(nullable),0,&out,true,&found) && found && !out);}
    failCopy=true;assert(!xl6StackTryTakeV2(stack,STACK(nullable),0,&out,false,&found) && !found && !out);
    failCopy=false;xrtClearError();assert(xl6StackSizeV1(stack,STACK(nullable),false)==1);
    assert(xl6StackTryTakeV2(stack,STACK(nullable),9,&out,false,&found) && !found && !out);
    assert(!xl6StackTryTakeV2(stack,STACK(nullable),-1,&out,false,&found) && !found);xrtClearError();
    xvalue* clone=xl6StackCloneV1(stack,STACK(nullable));assert(clone && xl6StackEqualsV1(stack,clone,STACK(nullable)));
    assert(xl6StackTakeV1(stack,STACK(nullable),0,&out,true) && out==input);
    xrtFutureDestroy(input);assert(xrtFutureDone(out));xrtFutureDestroy(out);out=NULL;
    assert(xl6StackTryTakeV2(stack,STACK(nullable),0,&out,true,&found) && !found && !out);
    assert(!xl6StackTakeV1(stack,STACK(nullable),0,&out,true));xrtClearError();
    assert(xl6StackTakeV1(clone,STACK(nullable),0,&out,false));xrtValueRelease(clone);
    assert(xrtFutureDone(out));xrtFutureDestroy(out);
    assert(xl6StackClearV1(stack,STACK(nullable)) && xl6StackTrimV1(stack,STACK(nullable)));
    xrtValueRelease(stack);balanced(&before);
}
static void sweep(bool nullable,const char* operation)
{
    for(size_t point=0;point<256;++point){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        xfuture* input=ready();xvalue* stack=create(nullable,XL6_SEQUENCE_STACK_V1),*array=create(nullable,XL6_SEQUENCE_ARRAY_V1),*result=NULL;
        assert(stack && array && xl6StackPushV1(stack,STACK(nullable),&input) && xl6ArrayPushV1(array,ARRAY(nullable),&input));
        XL6_Sequence* pin=xl6SequenceBeginRoleV1(stack,XL6_SEQUENCE_STACK_V1,STACK(nullable));assert(pin);
        xarray original=xl6SequenceStorageV1(pin)->Storage;xl6SequenceEndV1(pin);
        assert(xrtMemDebugFailAfter(point));bool ok=false;
        if(!strcmp(operation,"create")){result=create(nullable,XL6_SEQUENCE_STACK_V1);ok=result!=NULL;}
        else if(!strcmp(operation,"clone")){result=xl6StackCloneV1(stack,STACK(nullable));ok=result!=NULL;}
        else if(!strcmp(operation,"reserve"))ok=xl6StackReserveV1(stack,STACK(nullable),128);
        else if(!strcmp(operation,"pushBatch"))ok=xl6StackPushBatchV1(stack,STACK(nullable),array,ARRAY_ID(nullable));
        else{result=xl6StackExtractArrayV1(stack,STACK(nullable),ARRAY_ID(nullable),0,1,!strcmp(operation,"popBatch"),NULL);ok=result!=NULL;}
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();assert(ok==!hit);++prefixes;xrtClearError();
        if(hit){pin=xl6SequenceBeginRoleV1(stack,XL6_SEQUENCE_STACK_V1,STACK(nullable));assert(pin);
            const xarray* after=&xl6SequenceStorageV1(pin)->Storage;
            assert(after->Count==original.Count && after->Capacity==original.Capacity &&
                after->Data==original.Data && after->Allocation==original.Allocation);xl6SequenceEndV1(pin);}
        if(result && (!strcmp(operation,"popBatch") || !strcmp(operation,"peekBatch"))){
            xfuture* out=NULL;assert(xl6ArrayReadV1(result,ARRAY(nullable),0,&out) && out==input);xrtFutureDestroy(out);
        }
        xrtValueRelease(result);xrtValueRelease(array);xrtValueRelease(stack);xrtFutureDestroy(input);balanced(&before);
        if(!hit)break;
        assert(point+1<256);
    }
}
static void payloadDrop(void* payload,void* context){(void)context;xrtValueRelease(payload);}
static bool payloadTrace(const void* payload,const void* metadata,xrtownershipvisitor visit,void* context)
{(void)metadata;return visit(xrtValueOwnership(payload),context);}
static const xfuturepayloadownershipv1 payloadPolicy={sizeof(payloadPolicy),payloadDrop,payloadTrace};
static const xrtownershipadapterv1* resolve(xrtownershipref ref,void* context)
{
    const XL6_SequenceAdmissionOpsV2* policies[]={xl6ArrayFutureAdmissionOpsV2Get()};
    const xfuturepayloadownershipv1* payloads[]={&payloadPolicy};
    const xrtownershipadapterv1* adapter=context?xl6SequenceOwnershipAdapterV1(ref):xl6SequenceOwnershipAdapterV2(ref,policies,1);
    if(!adapter)adapter=xrtFutureOwnershipAdapterV1(ref,payloads,1);
    if(!adapter)adapter=xrtValueFutureOwnershipAdapterV1(ref);
    if(!adapter)adapter=xrtCancelOwnershipAdapterV1(ref);
    if(!adapter)adapter=xrtErrorOwnershipAdapterV1(ref);
    return adapter?adapter:xrtValueOwnershipAdapterV1(ref);
}
static void graph(bool nullable)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    xfuture* future=NULL;xpromise* promise=xrtPromiseCreate(&future,NULL);assert(promise && future);
    xvalue* stack=create(nullable,XL6_SEQUENCE_STACK_V1);assert(stack && xl6StackPushV1(stack,STACK(nullable),&future));
    xvalue* weak=xrtValueWeakRef(stack),*payload=xrtValueRetain(stack);assert(weak && payload);
    assert(xrtPromiseResolveOwnedPolicyV1(promise,payload,&payloadPolicy));xrtPromiseDestroy(promise);
    xrtValueRelease(stack);xrtFutureDestroy(future);XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreate(xrtFutureOwnership(future),resolve,(void*)1,&plan)==XL6_OWNERSHIP_PLAN_ERROR && !plan);
    xrtClearError();assert(!xrtValueWeakRefExpired(weak));
    assert(xrtFutureRef(future));
    assert(xl6OwnershipPlanTryCreate(xrtFutureOwnership(future),resolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ROOTED);
    xrtFutureDestroy(future);assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY && xl6OwnershipPlanFinish(plan));
    assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY && xrtValueWeakRefExpired(weak));
    xrtValueRelease(weak);balanced(&before);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xrtClearError();
    for(unsigned i=0;i<100;++i){vectors(false);vectors(true);}
    const char* operations[]={"create","clone","reserve","pushBatch","peekBatch","popBatch"};
    for(unsigned nullable=0;nullable<2;++nullable){graph(nullable!=0);for(size_t i=0;i<sizeof operations/sizeof *operations;++i)sweep(nullable!=0,operations[i]);}
    printf("Future Stack 200 lifetimes, 2 graphs, allocation prefixes=%u balanced\n",prefixes);return 0;
}
