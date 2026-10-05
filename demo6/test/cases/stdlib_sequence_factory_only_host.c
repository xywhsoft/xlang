#include "src/runtime/xl6_array_admission.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    assert(xrtMemDebugEnable(true)); xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
    for (unsigned cycle=0;cycle<100;++cycle) {
        xvalue* value;
        unsigned int transfer=3; const xrttype* type=xrtTypeValue(); uint64_t element=3001;
#if XL6_FACTORY_PROFILE == 0
        value=xl6ArrayReferenceCreateV2(1001,element,XL6_VALUE_ADMIT_ANY,false,0);
#elif XL6_FACTORY_PROFILE == 1
        const uint64_t identities[]={3001};
        XL6_NominalAdmissionV1 schema={.kind=XL6_NOMINAL_ADMIT_EXACT_ID,.identities=identities,.count=1};
        value=xl6ArrayNominalCreateV2(1001,element,&schema,false,element);
#elif XL6_FACTORY_PROFILE == 2
        transfer=1; type=xrtTypeInt64();
        value=xl6ArrayPrimitiveCreateV2(1001,element,transfer,type->Size,type,XL6_VALUE_ADMIT_I8,false,element);
#elif XL6_FACTORY_PROFILE == 3
        transfer=1; type=xrtTypeUInt16();
        value=xl6ArrayCScalarCreateV1(1001,element,type,element);
#elif XL6_FACTORY_PROFILE == 4
        value=xl6ArraySequenceCreateV1(1001,element,element,false);
#elif XL6_FACTORY_PROFILE == 5
        transfer=1; type=xl6NullableNativeU16TypeV1();
        value=xl6ArrayNullableCScalarCreateV1(1001,element,type,xrtTypeUInt16(),4001);
#elif XL6_FACTORY_PROFILE == 6
        transfer=1; type=xrtTypeInt64();
        value=xl6ArrayNumericEnumCreateV1(1001,element,type,XL6_VALUE_ADMIT_I8,false,element);
#elif XL6_FACTORY_PROFILE == 7
        value=xl6ArrayCallableCreateV1(1001,element,element,false);
#elif XL6_FACTORY_PROFILE == 8
        transfer=XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1; type=xrtTypeFuture();
        value=xl6ArrayFutureCreateV1(1001,element,element,false);
#else
#error Require one exact resident factory profile
#endif
        assert(value && xl6SequenceAdmitsArrayV1(value,1001));
        XL6_Sequence* owner=xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,1001,element,transfer,type->Size,type);
        XL6_SequenceAdmissionV3 policy={0}; assert(owner && xl6SequenceAdmissionCopyV3(owner,&policy));
        xl6SequenceEndV1(owner); xrtValueRelease(value);
        /* Only the owned metadata remains. Role does not require another
         * container implementation and clone must preserve that metadata. */
        value=xl6SequenceCreateAdmittedV1(XL6_SEQUENCE_STACK_V1,2001,&policy); assert(value && !policy.ops && !policy.data);
        assert(!xl6SequenceAdmitsArrayV1(value,2001));
        xvalue* clone=xl6SequenceCloneRoleV1(value,XL6_SEQUENCE_STACK_V1,2001,element,transfer,type->Size,type);
        assert(clone && xl6SequenceEqualsRoleV1(value,clone,XL6_SEQUENCE_STACK_V1,2001,element,transfer,type->Size,type));
        xrtValueRelease(value); xrtValueRelease(clone); assert(!xrtGetError());
    }
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before.LiveCount && after.LiveBytes==before.LiveBytes &&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
        after.InvalidFreeCount==before.InvalidFreeCount && after.DoubleFreeCount==before.DoubleFreeCount &&
        after.UseAfterFreeCount==before.UseAfterFreeCount);
    printf("factory %u: 100 publication/clone cycles balanced\n",XL6_FACTORY_PROFILE); return 0;
}
