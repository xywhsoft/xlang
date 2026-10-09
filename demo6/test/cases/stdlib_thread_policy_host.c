/* Real resident Thread + closed, immutable Sequence policy. No provider stubs. */
#include "release/include/xrt_decl.h"
#include "src/runtime/xl6_thread.h"
#include "src/runtime/xl6_thread_admission.h"
#include "src/runtime/xl6_stack.h"
#include "src/runtime/xl6_array.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
enum { THREAD_ID=401, OPTIONAL_ID=402, STACK_ID=701, ARRAY_ID=702 };
static bool invoke(void* environment,int64_t* output)
{ (void)environment; *output=42; return true; }
static void fakeDrop(void* raw,void* context) { (void)raw; (void)context; }
static void vectors(void)
{
    XL6_Closure* closure=xl6ClosureRuntimeV2Get()->Create(NULL,(void (*)(void))invoke,
        NULL,NULL,NULL,NULL,NULL,0); assert(closure);
    xvalue* thread=xl6ThreadCreateV1(closure,THREAD_ID); assert(thread);
    xl6ClosureRuntimeV1Get()->Release(closure); assert(xl6ThreadWaitV1(thread,THREAD_ID));
    xvalue* stack=xl6SequenceThreadCreateV1(XL6_SEQUENCE_STACK_V1,STACK_ID,THREAD_ID,THREAD_ID,false);
    assert(stack && xl6StackPushV1(stack,STACK_ID,THREAD_ID,3,sizeof(xvalue*),xrtTypeValue(),&thread));
    XL6_Sequence* pin=xl6SequenceBeginV1(stack,STACK_ID,THREAD_ID,3,sizeof(xvalue*),xrtTypeValue()); assert(pin);
    XL6_SequenceAdmissionV3 policy={0}; assert(xl6SequenceAdmissionCopyV3(pin,&policy)); xl6SequenceEndV1(pin);
    assert(policy.ops==xl6ArrayThreadAdmissionOpsV2Get());
    static const xvaluehandleops foreignOps={NULL,fakeDrop,NULL,NULL};
    void* invalid=(void*)(uintptr_t)1; xvalue* foreign=xrtValueHandleTake(&invalid,&foreignOps,NULL); assert(foreign);
    assert(xrtValueTypeIdBind(foreign,THREAD_ID));
    xvalue* scalar=xrtValueInt(1); assert(scalar && xrtValueTypeIdBind(scalar,THREAD_ID));
    xrtSetErrorKind(XERR_TIMEOUT); const xerror* prior=xrtGetError();
    assert(xrtMemDebugFailAfter(0));
    assert(policy.ops->Value(policy.data,thread) && policy.ops->Slot(policy.data,&thread));
    assert(!policy.ops->Value(policy.data,foreign) && !policy.ops->Slot(policy.data,&foreign));
    assert(!policy.ops->Value(policy.data,scalar) && !policy.ops->Value(policy.data,xrtValueNull()));
    assert(!policy.ops->Value(policy.data,NULL) && !policy.ops->Defaultable(policy.data));
    assert(!xrtMemDebugFailTriggered() && xrtGetError()==prior); xrtMemDebugFailClear(); xrtClearError();
    assert(!xl6StackPushV1(stack,STACK_ID,THREAD_ID,3,sizeof(xvalue*),xrtTypeValue(),&foreign)); xrtClearError();
    assert(xl6StackSizeV1(stack,STACK_ID,THREAD_ID,3,sizeof(xvalue*),xrtTypeValue(),false)==1);
    xvalue* moved=NULL; assert(xl6StackTakeV1(stack,STACK_ID,THREAD_ID,3,sizeof(xvalue*),xrtTypeValue(),0,&moved,true));
    xrtValueRelease(stack);
    /* Metadata owns no shell/module edge; it remains valid after its owner. */
    assert(policy.ops->Value(policy.data,moved) && xl6ThreadExitCodeV1(moved,THREAD_ID)==42);
    xl6SequenceAdmissionDropV3(&policy); xrtValueRelease(moved);
    xvalue* optional=xl6SequenceThreadCreateV1(XL6_SEQUENCE_ARRAY_V1,ARRAY_ID,OPTIONAL_ID,THREAD_ID,true); assert(optional);
    assert(xl6ArrayResizeV1(optional,ARRAY_ID,OPTIONAL_ID,3,sizeof(xvalue*),xrtTypeValue(),3));
    xvalue* absent=NULL; assert(xl6ArrayReadV1(optional,ARRAY_ID,OPTIONAL_ID,3,sizeof(xvalue*),xrtTypeValue(),1,&absent));
    assert(!absent || xrtValueType(absent)==XVALUE_NULL); xrtValueRelease(absent);
    assert(xl6ArraySetV1(optional,ARRAY_ID,OPTIONAL_ID,3,sizeof(xvalue*),xrtTypeValue(),1,&thread));
    xvalue* nullValue=xrtValueNull();
    assert(xl6ArraySetV1(optional,ARRAY_ID,OPTIONAL_ID,3,sizeof(xvalue*),xrtTypeValue(),1,&nullValue));
    xrtValueRelease(optional); xrtValueRelease(thread); xrtValueRelease(scalar); xrtValueRelease(foreign);
}
static void oom(void)
{
    unsigned hits=0; bool complete=false;
    for (size_t point=0;point<32;++point) {
        xl6ProductMemoryBegin(point);
        xvalue* stack=xl6SequenceThreadCreateV1(XL6_SEQUENCE_STACK_V1,STACK_ID,OPTIONAL_ID,THREAD_ID,true);
        bool hit=xl6ProductMemoryEnd();
        if (hit) { assert(!stack); ++hits; } else { assert(stack); complete=true; }
        xrtValueRelease(stack); xrtClearError(); xl6ProductMemoryVerify(); if(complete) break;
    }
    assert(complete && hits); printf("Thread policy: %u complete construction allocation prefixes\n",hits);
}
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX); vectors(); assert(!xrtGetError() && !xl6ProductMemoryEnd());
    xl6ProductMemoryVerify(); oom(); puts("actual Thread policy: provenance, precise nulls and lifetimes balanced"); return 0;
}
