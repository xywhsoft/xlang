#include "src/runtime/xl6_sequence_projection_schema.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
static xvalue* factory(unsigned role,unsigned rule)
{ return xl6SequencePrimitiveCreateV1(role,9101,9102,1,sizeof(int64_t),xrtTypeInt64(),rule,false,9102); }
static unsigned fault;
extern bool __real_xl6SequenceSchemaCopyV1(const xvalue*,XL6_SequenceRoleV1*,XL6_SequenceLayoutV1*,XL6_SequenceAdmissionV3*);
bool __wrap_xl6SequenceSchemaCopyV1(const xvalue* value,XL6_SequenceRoleV1* role,
    XL6_SequenceLayoutV1* layout,XL6_SequenceAdmissionV3* admission)
{
    if(fault==1)return false; /* Real failure without a diagnostic. */
    bool ok=__real_xl6SequenceSchemaCopyV1(value,role,layout,admission);
    if(ok && fault==4) { xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL}); return false; }
    return ok;
}
extern void __real_xl6SequenceAdmissionDropV3(XL6_SequenceAdmissionV3*);
void __wrap_xl6SequenceAdmissionDropV3(XL6_SequenceAdmissionV3* admission)
{
    bool owned=admission->ops!=NULL;
    __real_xl6SequenceAdmissionDropV3(admission);
    if(owned && fault==2)xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    if(owned && (fault==3 || fault==4))xrtSetErrorKind(XERR_STATE);
}
static void dropFake(ptr value,ptr user) { (void)value; (void)user; }
static const xvaluehandleops fakeOps={NULL,dropFake,NULL,NULL};
static void reject(const XL6_SequenceProjectionSchemaV1* schema,const xvalue* source)
{
    xvalue* output=NULL;
    assert(!xl6SequenceProjectionApplyV1(schema,source,false,&output) && !output);
    assert(xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError();
    assert(xl6SequenceProjectionApplyV1(schema,source,true,&output));
    assert(output && xrtValueType(output)==XVALUE_NULL); xrtValueRelease(output);
}
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    XL6_SequenceProjectionSchemaV1 schema={0};
    xvalue* witness=factory(XL6_SEQUENCE_ARRAY_V1,XL6_VALUE_ADMIT_I8); assert(witness);
    assert(xl6SequenceProjectionPrepareV1(witness,XL6_SEQUENCE_ARRAY_V1,&schema));
    xrtValueRelease(witness); /* Metadata survives its originating owner. */
    xvalue* source=factory(XL6_SEQUENCE_ARRAY_V1,XL6_VALUE_ADMIT_I8); assert(source);
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(source,XL6_SEQUENCE_ARRAY_V1,
        9101,9102,1,sizeof(int64_t),xrtTypeInt64()); assert(pin);
    int64_t number=7; assert(xrtTypedArrayPush(xl6SequenceStorageV1(pin),&number));
    xvalue* output=NULL;
    assert(xrtMemDebugFailAfter(0));
    assert(xl6SequenceProjectionApplyV1(&schema,source,false,&output) && output==source);
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    xrtValueRelease(output); output=NULL; xl6SequenceEndV1(pin);
    xvalue* sameLayoutWrongPolicy=factory(XL6_SEQUENCE_ARRAY_V1,XL6_VALUE_ADMIT_I64); assert(sameLayoutWrongPolicy);
    reject(&schema,sameLayoutWrongPolicy); xrtValueRelease(sameLayoutWrongPolicy);
    xvalue* wrongRole=factory(XL6_SEQUENCE_STACK_V1,XL6_VALUE_ADMIT_I8); assert(wrongRole);
    reject(&schema,wrongRole); xrtValueRelease(wrongRole);
    xvalue* wire=xrtValueArray(); assert(wire && xrtValueTypeIdBind(wire,9101));
    reject(&schema,wire); xrtValueRelease(wire);
    /* A spoofed TypeId must not authorize dereferencing a foreign pointer. */
    ptr pointer=(ptr)(uintptr_t)1;
    xvalue* fake=xrtValueHandleTake(&pointer,&fakeOps,NULL); assert(fake && !pointer);
    assert(xrtValueTypeIdBind(fake,9101)); reject(&schema,fake); xrtValueRelease(fake);
    reject(&schema,xrtValueNull());
    assert(!xl6SequenceProjectionApplyV1(&schema,NULL,true,&output) && !output); xrtClearError();
    output=xrtValueRetain(source);
    assert(!xl6SequenceProjectionApplyV1(&schema,source,false,&output) && output==source);
    xrtClearError(); xrtValueRelease(output); output=NULL;
    assert(xl6SequenceProjectionWitnessTakeV1(factory(XL6_SEQUENCE_ARRAY_V1,XL6_VALUE_ADMIT_I8),
        XL6_SEQUENCE_ARRAY_V1,source,false,&output) && output==source);
    xrtValueRelease(output); output=NULL;
    assert(xl6SequenceProjectionDisposeV1(&schema) && !schema.admission.ops && !schema.layout.identity);
    assert(xl6SequenceProjectionDisposeV1(&schema)); xrtValueRelease(source);
    /* Capture failure must not publish or consume an existing caller record. */
    witness=factory(XL6_SEQUENCE_ARRAY_V1,XL6_VALUE_ADMIT_I8); assert(witness);
    assert(!xl6SequenceProjectionPrepareV1(witness,XL6_SEQUENCE_STACK_V1,&schema) && !schema.admission.ops);
    xrtClearError(); xrtValueRelease(witness);
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    for(unsigned mode=1;mode<=4;++mode) {
        fault=mode;
        xl6ProductMemoryBegin(SIZE_MAX);
        source=factory(XL6_SEQUENCE_ARRAY_V1,XL6_VALUE_ADMIT_I8); assert(source);
        assert(!xl6SequenceProjectionWitnessTakeV1(factory(XL6_SEQUENCE_ARRAY_V1,XL6_VALUE_ADMIT_I8),
            XL6_SEQUENCE_ARRAY_V1,source,false,&output) && !output);
        assert(fault==3 ? xrtErrorKind(xrtGetError())==XERR_STATE : !xrtGetError());
        xrtClearError();
        fault=0; xrtValueRelease(source); assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    }
    /* Every synchronous allocation prefix, including the first successful one. */
    fault=0; size_t point;
    for(point=0;point<1000;++point) {
        xl6ProductMemoryBegin(point); output=NULL;
        source=factory(XL6_SEQUENCE_ARRAY_V1,XL6_VALUE_ADMIT_I8);
        witness=source ? factory(XL6_SEQUENCE_ARRAY_V1,XL6_VALUE_ADMIT_I8) : NULL;
        bool ok=witness && xl6SequenceProjectionWitnessTakeV1(witness,XL6_SEQUENCE_ARRAY_V1,source,false,&output);
        bool hit=xl6ProductMemoryEnd(); xerror* failure=xrtTakeError();
        assert(hit ? !ok && !output && xrtErrorKind(failure)==XERR_MEMORY : ok && output==source && !failure);
        xrtValueRelease(output); xrtValueRelease(source); xrtErrorFree(failure); xrtClearError();
        xl6ProductMemoryVerify(); if(!hit)break;
    }
    assert(point<1000);
    printf("complete schema/role/policy/identity/no-allocation/busy/independent-metadata balanced; native prefixes=%zu\n",point+1);
    return 0;
}
