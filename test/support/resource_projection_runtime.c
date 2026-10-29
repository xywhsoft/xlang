#include "src/runtime/xl6_value_domain_projection.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_concurrent_queue.h"
#include "src/runtime/xl6_wait_queue_abi.h"
#include "src/runtime/xl6_channel_abi.h"
#include "src/runtime/xl6_slot_map.h"
#include "src/runtime/xl6_resource_reference_admission.h"
#include "src/runtime/xl6_value_admission.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
typedef xvalue* (*Resource)(uint64_t,xvalue*,int64_t);
typedef xvalue* (*Domain)(unsigned int,uint64_t,uint64_t,uint64_t,xvalue*,bool);
static const Resource resources[]={xl6SpscQueueCreateTakeV1,xl6MpscQueueCreateTakeV1,
    xl6MpmcQueueCreateTakeV1,xl6WaitQueueCreateTakeV1,xl6ChannelCreateTakeV1};
static const Domain domains[]={xl6SequenceSpscQueueCreateTakeV1,xl6SequenceMpscQueueCreateTakeV1,
    xl6SequenceMpmcQueueCreateTakeV1,xl6SequenceWaitQueueCreateTakeV1,xl6SequenceChannelCreateTakeV1,
    xl6SequenceSlotMapCreateTakeV1};
static unsigned getterFault;
extern bool __real_xrtValueGetFloat(const xvalue*,double*);
bool __wrap_xrtValueGetFloat(const xvalue* value,double* output)
{
    if(getterFault) { if(getterFault==2)xrtSetErrorKind(XERR_STATE); return false; }
    return __real_xrtValueGetFloat(value,output);
}
static xvalue* seed(unsigned rule)
{ return xl6SequencePrimitiveCreateV1(1,7400,7410,1,sizeof(int64_t),xrtTypeInt64(),rule,false,7410); }
static xvalue* resource(unsigned family,unsigned rule)
{
    xvalue* child=seed(rule); if(!child)return NULL;
    if(family<5)return resources[family](7500,child,2);
    XL6_SlotMapLayoutV1 layout={7500,{7400,7410,1,sizeof(int64_t),xrtTypeInt64()}};
    return xl6SlotMapCreateTakeV1(&layout,child);
}
static xvalue* witness(unsigned family)
{
    xvalue* child=seed(XL6_VALUE_ADMIT_I8); if(!child)return NULL;
    return domains[family](1,7600,7500,7500,child,false);
}
static void fakeDrop(ptr value,ptr user) { (void)value;(void)user; }
static const xvaluehandleops fakeOps={NULL,fakeDrop,NULL,NULL};
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX); assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    for(unsigned family=0;family<6;++family) {
        xvalue* source=resource(family,XL6_VALUE_ADMIT_I8);
        xvalue* metadata=witness(family); xvalue* output=NULL; assert(source && metadata);
        xl6ProductMemoryBegin(0);
        assert(xl6ValueDomainProjectionWitnessTakeV1(metadata,source,false,&output) && output==source);
        assert(!xl6ProductMemoryEnd());
        xrtValueRelease(output); xrtValueRelease(source);
    }
    xl6ProductMemoryBegin(SIZE_MAX);
    for(unsigned family=0;family<6;++family) {
        xvalue* source=resource(family,XL6_VALUE_ADMIT_I8); assert(source); xvalue* output=NULL;
        assert(xl6ValueDomainProjectionWitnessTakeV1(witness(family),source,false,&output) && output==source);
        xrtValueRelease(output); output=NULL;
        xvalue* other=resource((family+1)%6,XL6_VALUE_ADMIT_I8); assert(other);
        assert(xl6ValueDomainProjectionWitnessTakeV1(witness(family),other,true,&output) && xrtValueType(output)==XVALUE_NULL);
        xrtValueRelease(output); output=NULL; xrtValueRelease(other);
        xvalue* wrong=resource(family,XL6_VALUE_ADMIT_I64); assert(wrong);
        assert(!xl6ValueDomainProjectionWitnessTakeV1(witness(family),wrong,false,&output) && !output);
        assert(xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError();
        assert(xl6ValueDomainProjectionWitnessTakeV1(witness(family),wrong,true,&output) && xrtValueType(output)==XVALUE_NULL);
        xrtValueRelease(output); output=NULL; xrtValueRelease(wrong);
        ptr pointer=(ptr)(uintptr_t)1; wrong=xrtValueHandleTake(&pointer,&fakeOps,NULL); assert(wrong);
        assert(xrtValueTypeIdBind(wrong,7500));
        assert(xl6ValueDomainProjectionWitnessTakeV1(witness(family),wrong,true,&output) && xrtValueType(output)==XVALUE_NULL);
        xrtValueRelease(output); output=NULL; xrtValueRelease(wrong);
        assert(!xl6ValueDomainProjectionWitnessTakeV1(witness(family),NULL,true,&output) && !output); xrtClearError();
        xrtValueRelease(source);
    }
    xvalue* value=xrtValueFloat(1.5); assert(value); float number=77; bool present=true;
    assert(xl6ValueReadF32V1(value,7700,&number) && number==1.5f);
    assert(xl6ValueTryReadF32V1(value,7700,&present,&number) && present && number==1.5f);
    for(getterFault=1;getterFault<=2;++getterFault) {
        number=77; present=true;
        assert(!xl6ValueTryReadF32V1(value,7700,&present,&number) && present && number==77);
        assert(getterFault==2 ? xrtErrorKind(xrtGetError())==XERR_STATE : !xrtGetError()); xrtClearError();
        assert(!xl6ValueReadF32V1(value,7700,&number) && number==77);
        assert(getterFault==2 ? xrtErrorKind(xrtGetError())==XERR_STATE : !xrtGetError()); xrtClearError();
    }
    getterFault=0; xrtValueRelease(value);
    value=xrtValueFloat(1.1); assert(value); number=77; present=true;
    assert(xl6ValueTryReadF32V1(value,7700,&present,&number) && !present && number==77); xrtValueRelease(value);
    value=xrtValueFloat(INFINITY); assert(value);
    assert(xl6ValueTryReadF32V1(value,7700,&present,&number) && present && isinf(number)); xrtValueRelease(value);
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    for(unsigned family=0;family<6;++family) {
        size_t point;
        for(point=0;point<1000;++point) {
            xl6ProductMemoryBegin(point); xvalue* output=NULL;
            xvalue* source=resource(family,XL6_VALUE_ADMIT_I8); xvalue* schema=source ? witness(family) : NULL;
            bool ok=schema && xl6ValueDomainProjectionWitnessTakeV1(schema,source,false,&output);
            bool hit=xl6ProductMemoryEnd(); xerror* failure=xrtTakeError();
            assert(hit ? !ok && !output && xrtErrorKind(failure)==XERR_MEMORY : ok && output==source && !failure);
            xrtValueRelease(output); xrtValueRelease(source); xrtErrorFree(failure); xrtClearError(); xl6ProductMemoryVerify();
            if(!hit)break;
        }
        assert(point<1000); printf("resource family=%u prefixes=%zu balanced\n",family,point+1);
    }
    puts("resource domains and F32 lossless/null-diagnostic outputs balanced"); return 0;
}
