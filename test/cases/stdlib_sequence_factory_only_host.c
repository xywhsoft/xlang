#include "src/runtime/xl6_array_admission.h"
#include <assert.h>
#include <stdio.h>

static xvalue* factory(unsigned int role,const xrttype** descriptor,unsigned int* mode)
{
        xvalue* value;
        unsigned int transfer=3; const xrttype* type=xrtTypeValue(); uint64_t element=3001;
#if XL6_FACTORY_PROFILE == 0
        value=xl6SequenceReferenceCreateV1(role,1001,element,XL6_VALUE_ADMIT_ANY,false,0);
#elif XL6_FACTORY_PROFILE == 1
        const uint64_t identities[]={3001};
        XL6_NominalAdmissionV1 schema={.kind=XL6_NOMINAL_ADMIT_EXACT_ID,.identities=identities,.count=1};
        value=xl6SequenceNominalCreateV1(role,1001,element,&schema,false,element);
#elif XL6_FACTORY_PROFILE == 2
        transfer=1; type=xrtTypeInt64();
        value=xl6SequencePrimitiveCreateV1(role,1001,element,transfer,type->Size,type,XL6_VALUE_ADMIT_I8,false,element);
#elif XL6_FACTORY_PROFILE == 3
        transfer=1; type=xrtTypeUInt16();
        value=xl6SequenceCScalarCreateV1(role,1001,element,type,element);
#elif XL6_FACTORY_PROFILE == 4
        value=xl6SequenceNestedCreateTakeV2(role,1001,element,
            xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,element,4001,1,sizeof(int64_t),
                xrtTypeInt64(),XL6_VALUE_ADMIT_I8,false,4001),false);
#elif XL6_FACTORY_PROFILE == 5
        transfer=1; type=xl6NullableNativeU16TypeV1();
        value=xl6SequenceNullableCScalarCreateV1(role,1001,element,type,xrtTypeUInt16(),4001);
#elif XL6_FACTORY_PROFILE == 6
        transfer=1; type=xrtTypeInt64();
        value=xl6SequenceNumericEnumCreateV1(role,1001,element,type,XL6_VALUE_ADMIT_I8,false,element);
#elif XL6_FACTORY_PROFILE == 7
        value=xl6SequenceCallableCreateV1(role,1001,element,element,false);
#elif XL6_FACTORY_PROFILE == 8
        transfer=XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1; type=xrtTypeFuture();
        value=xl6SequenceFutureCreateV1(role,1001,element,element,false);
#else
#error Require one exact resident factory profile
#endif
        *descriptor=type; *mode=transfer; return value;
}

int main(void)
{
    assert(xrtMemDebugEnable(true)); xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
    for (unsigned role=XL6_SEQUENCE_ARRAY_V1;role<=XL6_SEQUENCE_STACK_V1;++role)
    for (unsigned cycle=0;cycle<100;++cycle) {
        const xrttype* type; unsigned int transfer; uint64_t element=3001;
        xvalue* value=factory(role,&type,&transfer);
        assert(value && xl6SequenceAdmitsArrayV1(value,1001)==(role==XL6_SEQUENCE_ARRAY_V1));
        assert(xl6SequenceAdmitsRoleV1(value,role,1001,element,transfer,type->Size,type));
        assert(!xl6SequenceAdmitsRoleV1(value,role,1001,element+1,transfer,type->Size,type));
        assert(!xl6SequenceAdmitsRoleV1(value,role==1 ? 2 : 1,1001,element,transfer,type->Size,type));
        XL6_Sequence* owner=xl6SequenceBeginRoleV1(value,(XL6_SequenceRoleV1)role,1001,element,transfer,type->Size,type);
        XL6_SequenceAdmissionV3 policy={0}; assert(owner && xl6SequenceAdmissionCopyV3(owner,&policy));
        /* The probe observes sealed immutable metadata, even while BUSY, and
         * does not replace an error owned by its caller. */
        xrtSetErrorKind(XERR_ARGUMENT); const xerror* pending=xrtGetError();
        assert(xl6SequenceAdmitsRoleV1(value,role,1001,element,transfer,type->Size,type));
        assert(!xl6SequenceAdmitsRoleV1(value,role,1001,element,transfer,type->Size+1,type));
        assert(xrtGetError()==pending); xrtClearError();
        xl6SequenceEndV1(owner);
        xvalue* directClone=xl6SequenceCloneRoleV1(value,(XL6_SequenceRoleV1)role,1001,element,transfer,type->Size,type);
        assert(directClone && xl6SequenceEqualsRoleV1(value,directClone,(XL6_SequenceRoleV1)role,1001,element,transfer,type->Size,type));
        xrtValueRelease(directClone);
        xrtValueRelease(value);
        /* Only the owned metadata remains. Role does not require another
         * container implementation and clone must preserve that metadata. */
        value=xl6SequenceCreateAdmittedV1(XL6_SEQUENCE_STACK_V1,2001,&policy); assert(value && !policy.ops && !policy.data);
        assert(!xl6SequenceAdmitsArrayV1(value,2001));
        xvalue* clone=xl6SequenceCloneRoleV1(value,XL6_SEQUENCE_STACK_V1,2001,element,transfer,type->Size,type);
        assert(clone && xl6SequenceEqualsRoleV1(value,clone,XL6_SEQUENCE_STACK_V1,2001,element,transfer,type->Size,type));
        xrtValueRelease(value); xrtValueRelease(clone); assert(!xrtGetError());
    }
    const unsigned int invalid[]={0,XL6_SEQUENCE_CURSOR_V1,UINT32_MAX};
    for (unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);++i) {
        const xrttype* type; unsigned int transfer;
        assert(!factory(invalid[i],&type,&transfer));
        assert(xrtGetError() && xrtErrorKind(xrtGetError())==XERR_ARGUMENT); xrtClearError();
    }
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before.LiveCount && after.LiveBytes==before.LiveBytes &&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
        after.InvalidFreeCount==before.InvalidFreeCount && after.DoubleFreeCount==before.DoubleFreeCount &&
        after.UseAfterFreeCount==before.UseAfterFreeCount);
    printf("factory %u: 100 ARRAY/STACK direct publication/clone cycles balanced\n",XL6_FACTORY_PROFILE); return 0;
}
