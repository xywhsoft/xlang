#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_reference_value.h"
#include "src/runtime/xl6_array_default.h"
#include <assert.h>
#include <stdio.h>

static xvalue* receiver;
static unsigned drops;
static void drop(void* data,void* user)
{
    assert(data==&drops && !user);
    /* The operation pin remains live during a committed shrink's Drop. */
    assert(!xl6ArrayResizeDefaultV1(receiver,0,XL6_VALUE_DEFAULT_NULL));
    assert(xrtGetError() && xrtErrorKind(xrtGetError())==XERR_STATE); xrtClearError();
    ++drops; xrtSetErrorKind(XERR_RANGE);
}
static const xvaluehandleops ops={NULL,drop,NULL,NULL};
static void vectors(bool pending)
{
    receiver=xl6SequenceReferenceCreateV1(XL6_SEQUENCE_ARRAY_V1,1001,3001,XL6_VALUE_ADMIT_ANY,false,0);
    assert(receiver);
    XL6_Sequence* owner=xl6SequenceBeginRoleV1(receiver,XL6_SEQUENCE_ARRAY_V1,1001,3001,3,sizeof(xvalue*),xrtTypeValue());
    assert(owner && xl6SequenceBindProjectionV1(owner,xl6ArrayReferenceConverterV1()));
    void* raw=&drops; xvalue* item=xrtValueHandleTake(&raw,&ops,NULL); assert(item && !raw);
    assert(xl6SequenceAdmitSlotV1(owner,&item) && xrtTypedArrayPush(xl6SequenceStorageV1(owner),&item));
    xl6SequenceEndV1(owner); xrtValueRelease(item);
    const xerror* prior=NULL;
    if (pending) { xrtSetErrorKind(XERR_ARGUMENT); prior=xrtGetError(); }
    unsigned before=drops;
    assert(!xl6ArrayResizeDefaultV1(receiver,0,XL6_VALUE_DEFAULT_NULL));
    assert(drops==before+1 && xrtGetError());
    assert(pending ? xrtGetError()==prior : xrtErrorKind(xrtGetError())==XERR_RANGE); xrtClearError();
    owner=xl6SequenceBeginRoleV1(receiver,XL6_SEQUENCE_ARRAY_V1,1001,3001,3,sizeof(xvalue*),xrtTypeValue());
    assert(owner && xrtTypedArrayCount(xl6SequenceStorageV1(owner))==0); xl6SequenceEndV1(owner);
    assert(xl6ArrayResizeDefaultV1(receiver,0,XL6_VALUE_DEFAULT_FORBIDDEN));
    xrtValueRelease(receiver); receiver=NULL;
}
int main(void)
{
    assert(xrtMemDebugEnable(true)); xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
    for (unsigned i=0;i<100;++i) { vectors(false); vectors(true); }
    xrtMemDebugSnapshot(&after);
    assert(drops==200 && !xrtGetError() && before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
        before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
        before.UseAfterFreeCount==after.UseAfterFreeCount);
    puts("native resize committed shrink: 200 drops, BUSY refusal, first error, balanced"); return 0;
}
