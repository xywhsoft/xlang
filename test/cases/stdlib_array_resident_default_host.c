#include "test_numeric_arg.h"
#if XL6_FACTORY_PROFILE == 7
#define XRUNTIME_MODULE_RUNTIME_CALL
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#endif
#if XL6_FACTORY_PROFILE == 8
#define XRUNTIME_MODULE_RUNTIME_TYPE_FUTURE
#define XRUNTIME_MODULE_RUNTIME_VALUE_FUTURE
#endif
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_default.h"
#include "src/runtime/xl6_array_reference_value.h"
#include "src/runtime/xl6_string_type.h"
#include "src/runtime/xl6_nullable_string.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static XL6_SequenceLayoutV1 layout;
static bool forbidden(void)
{ return XL6_FACTORY_PROFILE==1 || XL6_FACTORY_PROFILE==4 || XL6_FACTORY_PROFILE==7 || XL6_FACTORY_PROFILE==8; }
static xvalue* make(unsigned role,bool nullable)
{
    layout=(XL6_SequenceLayoutV1){1001,nullable ? 3002 : 3001,3,sizeof(xvalue*),xrtTypeValue()};
    xvalue* result;
#if XL6_FACTORY_PROFILE == 0
    result=xl6SequenceReferenceCreateV1(role,layout.identity,layout.elementIdentity,XL6_VALUE_ADMIT_ANY,nullable,0);
#elif XL6_FACTORY_PROFILE == 1
    const uint64_t ids[]={3001};
    XL6_NominalAdmissionV1 schema={.kind=XL6_NOMINAL_ADMIT_EXACT_ID,.identities=ids,.count=1};
    result=xl6SequenceNominalCreateV1(role,layout.identity,layout.elementIdentity,&schema,nullable,3001);
#elif XL6_FACTORY_PROFILE == 2
    assert(!nullable); layout.transfer=1; layout.type=xrtTypeInt64(); layout.size=layout.type->Size;
    result=xl6SequencePrimitiveCreateV1(role,layout.identity,layout.elementIdentity,1,layout.size,layout.type,XL6_VALUE_ADMIT_I8,false,3001);
#elif XL6_FACTORY_PROFILE == 3
    assert(!nullable); layout.transfer=1; layout.type=xrtTypeUInt16(); layout.size=layout.type->Size;
    result=xl6SequenceCScalarCreateV1(role,layout.identity,layout.elementIdentity,layout.type,3001);
#elif XL6_FACTORY_PROFILE == 4
    result=xl6SequenceNestedCreateTakeV2(role,layout.identity,layout.elementIdentity,
        xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,3001,4001,1,sizeof(int64_t),
            xrtTypeInt64(),XL6_VALUE_ADMIT_I8,false,4001),nullable);
#elif XL6_FACTORY_PROFILE == 5
    assert(!nullable); layout.transfer=1; layout.type=xl6NullableNativeU16TypeV1(); layout.size=layout.type->Size;
    result=xl6SequenceNullableCScalarCreateV1(role,layout.identity,layout.elementIdentity,layout.type,xrtTypeUInt16(),4001);
#elif XL6_FACTORY_PROFILE == 6
    assert(!nullable); layout.transfer=1; layout.type=xrtTypeInt64(); layout.size=layout.type->Size;
    result=xl6SequenceNumericEnumCreateV1(role,layout.identity,layout.elementIdentity,layout.type,XL6_VALUE_ADMIT_I8,false,3001);
#elif XL6_FACTORY_PROFILE == 7
    result=xl6SequenceCallableCreateV1(role,layout.identity,layout.elementIdentity,3001,nullable);
#elif XL6_FACTORY_PROFILE == 8
    layout.transfer=XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1; layout.type=xrtTypeFuture(); layout.size=layout.type->Size;
    result=xl6SequenceFutureCreateV1(role,layout.identity,layout.elementIdentity,3001,nullable);
#elif XL6_FACTORY_PROFILE == 9 || XL6_FACTORY_PROFILE == 11
    assert(!nullable); layout.transfer=2;
    layout.type=XL6_FACTORY_PROFILE==11 ? xl6NullableStringTypeV1() : xl6StringTypeV1(); layout.size=layout.type->Size;
    result=xl6SequencePrimitiveCreateV1(role,layout.identity,layout.elementIdentity,2,layout.size,layout.type,XL6_VALUE_ADMIT_STRING,XL6_FACTORY_PROFILE==11,4001);
#elif XL6_FACTORY_PROFILE == 10
    assert(!nullable);
    result=xl6SequenceReferenceCreateV1(role,layout.identity,layout.elementIdentity,XL6_VALUE_ADMIT_BYTES,false,3001);
#else
#error Require exact resident default profile
#endif
    assert(result); return result;
}
static XL6_Sequence* pin(xvalue* value,unsigned role)
{
    XL6_Sequence* owner=xl6SequenceBeginRoleV1(value,(XL6_SequenceRoleV1)role,
        layout.identity,layout.elementIdentity,layout.transfer,layout.size,layout.type);
    assert(owner); return owner;
}
static void bind(xvalue* value)
{
    XL6_Sequence* owner=pin(value,XL6_SEQUENCE_ARRAY_V1);
    const xvalueconverter* converter=NULL;
    if (layout.transfer==3) converter=xl6ArrayReferenceConverterV1();
#if XL6_FACTORY_PROFILE == 5
    converter=xl6NullableNativeU16ConverterV1();
#elif XL6_FACTORY_PROFILE == 9
    converter=xl6StringConverterV1();
#elif XL6_FACTORY_PROFILE == 11
    converter=xl6NullableStringConverterV1();
#endif
    assert(xl6SequenceBindProjectionV1(owner,converter)); xl6SequenceEndV1(owner);
}
static void error(xerrkind kind)
{ assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind); xrtClearError(); }
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount && !xrtGetError());
}
static void defaults(xvalue* value,size_t expected)
{
    XL6_Sequence* owner=pin(value,XL6_SEQUENCE_ARRAY_V1);
    xtypedarray* storage=xl6SequenceStorageV1(owner); assert(xrtTypedArrayCount(storage)==expected);
    for (size_t i=0;i<expected;++i) {
        const void* item=xrtTypedArrayConstGet(storage,i); assert(xl6SequenceAdmitSlotV1(owner,item));
#if XL6_FACTORY_PROFILE == 9 || XL6_FACTORY_PROFILE == 11
        const xstrview* text=item; assert(text->Size==0);
        assert((text->Data!=NULL)==(XL6_FACTORY_PROFILE==9));
#elif XL6_FACTORY_PROFILE == 10
        xbytesview bytes={0}; const xvalue* data=*(xvalue* const*)item;
        assert(data && xrtValueGetBytes(data,&bytes) && bytes.Size==0);
#else
        for (size_t b=0;b<layout.size;++b) assert(((const unsigned char*)item)[b]==0);
#endif
    }
    xl6SequenceEndV1(owner);
}
static void vectors(bool nullable)
{
    xvalue* value=make(XL6_SEQUENCE_ARRAY_V1,nullable); bind(value);
    xvalue* alias=xrtValueRetain(value); assert(alias==value);
    bool allowed=!forbidden() || nullable;
    assert(xrtMemDebugFailAfter(0));
    assert(xl6ArrayResizeDefaultV1(value,0,XL6_VALUE_DEFAULT_FORBIDDEN));
    if (!allowed) { assert(!xl6ArrayResizeDefaultV1(value,1,XL6_VALUE_DEFAULT_NULL)); error(XERR_TYPE); }
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    if (allowed) {
        /* A wire hint must never initialize the wrong native slot domain. */
        assert(xl6ArrayResizeDefaultV1(value,3,XL6_VALUE_DEFAULT_TIME)); defaults(alias,3);
        xvalue* snapshot=xl6SequenceCloneRoleV1(value,XL6_SEQUENCE_ARRAY_V1,layout.identity,
            layout.elementIdentity,layout.transfer,layout.size,layout.type); assert(snapshot);
        assert(xl6ArrayResizeDefaultV1(value,19,XL6_VALUE_DEFAULT_FORBIDDEN)); defaults(alias,19); defaults(snapshot,3);
        assert(xl6ArrayResizeDefaultV1(value,1,XL6_VALUE_DEFAULT_INT)); defaults(alias,1); defaults(snapshot,3);
        xrtValueRelease(snapshot);
    }
    assert(!xl6ArrayResizeDefaultV1(value,-1,XL6_VALUE_DEFAULT_NULL)); error(XERR_RANGE);
    assert(!xl6ArrayResizeDefaultV1(value,0,XL6_VALUE_DEFAULT_INVALID)); error(XERR_ARGUMENT);
    XL6_Sequence* owner=pin(value,XL6_SEQUENCE_ARRAY_V1);
    assert(!xl6ArrayResizeDefaultV1(value,0,XL6_VALUE_DEFAULT_NULL)); error(XERR_STATE); xl6SequenceEndV1(owner);
    assert(xl6ArrayResizeDefaultV1(value,0,XL6_VALUE_DEFAULT_FORBIDDEN)); defaults(alias,0);
    xrtValueRelease(alias); xrtValueRelease(value);
}
static void fault(bool nullable,bool spare,size_t point)
{
    assert(!forbidden() || nullable);
    xvalue* value=make(XL6_SEQUENCE_ARRAY_V1,nullable); bind(value);
    assert(xl6ArrayResizeDefaultV1(value,1,XL6_VALUE_DEFAULT_NULL));
    xvalue* alias=xrtValueRetain(value),*snapshot=xl6SequenceCloneRoleV1(value,XL6_SEQUENCE_ARRAY_V1,
        layout.identity,layout.elementIdentity,layout.transfer,layout.size,layout.type); assert(alias && snapshot);
    XL6_Sequence* owner=pin(value,XL6_SEQUENCE_ARRAY_V1); xtypedarray* storage=xl6SequenceStorageV1(owner);
    if (spare) assert(xrtTypedArrayReserve(storage,65));
    size_t capacity=xrtTypedArrayCapacity(storage); unsigned char first[sizeof(xstrview)]; assert(layout.size<=sizeof(first));
    memcpy(first,xrtTypedArrayConstGet(storage,0),layout.size); xl6SequenceEndV1(owner);
    assert(xrtMemDebugFailAfter(point));
    bool ok=xl6ArrayResizeDefaultV1(value,spare ? 9 : 65,XL6_VALUE_DEFAULT_INT);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if (!ok) {
        assert(hit && xrtGetError());
        /* XRT can wrap failed element Init as STATE with MEMORY as its cause. */
        const xerror* failure=xrtGetError();
        while (failure && xrtErrorKind(failure)!=XERR_MEMORY) failure=xrtErrorCause(failure);
        assert(failure);
        xrtClearError(); defaults(value,1);
        owner=pin(value,XL6_SEQUENCE_ARRAY_V1); storage=xl6SequenceStorageV1(owner);
        assert(xrtTypedArrayCapacity(storage)==capacity && !memcmp(first,xrtTypedArrayConstGet(storage,0),layout.size));
        xl6SequenceEndV1(owner);
    } else { assert(!hit && !xrtGetError()); defaults(alias,spare ? 9 : 65); }
    defaults(snapshot,1); xrtValueRelease(snapshot); xrtValueRelease(alias); xrtValueRelease(value);
    printf("native-default profile=%u nullable=%u spare=%u point=%zu hit=%u balanced\n",XL6_FACTORY_PROFILE,nullable,spare,point,hit);
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true)); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    if (argc==4) fault(xlTestIntArg(argv[1])!=0,xlTestIntArg(argv[2])!=0,(size_t)strtoull(argv[3],NULL,10));
    else {
        assert(argc==1);
        for (unsigned i=0;i<100;++i) { vectors(false); if (forbidden()) vectors(true); }
        assert(!xl6ArrayResizeDefaultV1(NULL,1,XL6_VALUE_DEFAULT_NULL)); error(XERR_ARGUMENT);
        xvalue* unbound=make(XL6_SEQUENCE_ARRAY_V1,false);
        assert(!xl6ArrayResizeDefaultV1(unbound,0,XL6_VALUE_DEFAULT_NULL)); error(XERR_STATE); xrtValueRelease(unbound);
        xvalue* stack=make(XL6_SEQUENCE_STACK_V1,false);
        assert(!xl6ArrayResizeDefaultV1(stack,0,XL6_VALUE_DEFAULT_NULL)); error(XERR_TYPE); xrtValueRelease(stack);
        puts("native defaults: 100 alias/snapshot/policy cycles balanced");
    }
    balanced(&before); return 0;
}
