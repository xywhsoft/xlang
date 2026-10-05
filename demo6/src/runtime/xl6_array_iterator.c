#include "src/runtime/xl6_array_iterator.h"
#include <limits.h>

#define PARAMS uint64_t identity, uint64_t elementIdentity, unsigned int transfer, size_t size, const xrttype* type
#define LAYOUT identity,elementIdentity,transfer,size,type
static bool allocation(bool ok)
{ if (!ok && !xrtGetError()) xrtSetErrorKind(XERR_MEMORY); return ok; }
static XL6_Sequence* begin(const xvalue* value, PARAMS)
{ return xl6SequenceIteratorBeginV1(value,LAYOUT); }
xvalue* xl6ArrayIteratorCreateV1(const xvalue* source, PARAMS, uint64_t cursorIdentity)
{
    if (!cursorIdentity || cursorIdentity==identity) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    XL6_Sequence* owner=xl6SequenceBeginRoleV1(source,XL6_SEQUENCE_ARRAY_V1,LAYOUT);
    if (!owner) return NULL;
    xtypedarray* storage=xl6SequenceStorageV1(owner);
    if (xrtTypedArrayCount(storage) > (size_t)INT64_MAX) {
        xrtSetErrorKind(XERR_RANGE); xl6SequenceEndV1(owner); return NULL;
    }
    xtypedarray* snapshot=xrtTypedArrayClone(storage);
    XL6_SequenceAdmissionV2 admission={0};
    if (snapshot && !xl6SequenceAdmissionCopyV2(owner,&admission)) {
        xerror* error=xrtTakeError(); xrtTypedArrayDestroy(snapshot); snapshot=NULL;
        xrtClearError(); xrtSetErrorTake(error);
    }
    xl6SequenceEndV1(owner);
    xvalue* result=xl6SequenceWrapAsV1(snapshot,XL6_SEQUENCE_CURSOR_V1,cursorIdentity,elementIdentity,transfer,size,type);
    if (!result) { xl6SequenceAdmissionDropV2(&admission); return NULL; }
    owner=xl6SequenceBeginRoleV1(result,XL6_SEQUENCE_CURSOR_V1,cursorIdentity,elementIdentity,transfer,size,type);
    if (!owner || !xl6SequenceAdmissionTakeV2(owner,&admission)) {
        if (owner) xl6SequenceEndV1(owner);
        xerror* error=xrtTakeError(); xrtValueRelease(result);
        xrtClearError(); xrtSetErrorTake(error); xl6SequenceAdmissionDropV2(&admission); return NULL;
    }
    *xl6SequenceIteratorStateV1(owner)=(XL6_SequenceIteratorStateV1){.prepared=true};
    xl6SequenceEndV1(owner); return result;
}
bool xl6ArrayIteratorAdvanceV1(const xvalue* value, PARAMS)
{
    XL6_Sequence* owner=begin(value,LAYOUT);
    if (!owner) return false;
    XL6_SequenceIteratorStateV1* state=xl6SequenceIteratorStateV1(owner);
    size_t count=xrtTypedArrayCount(xl6SequenceStorageV1(owner));
    bool found=!state->ended && state->next<count;
    if (found) { state->current=state->next++; state->currentValid=true; }
    else { state->currentValid=false; state->ended=true; }
    xl6SequenceEndV1(owner); return found;
}
bool xl6ArrayIteratorReadV1(const xvalue* value, PARAMS, void* target)
{
    if (!target) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    XL6_Sequence* owner=begin(value,LAYOUT);
    if (!owner) return false;
    XL6_SequenceIteratorStateV1* state=xl6SequenceIteratorStateV1(owner);
    bool ok=false;
    if (!state->currentValid) xrtSetErrorKind(XERR_STATE);
    else {
        xtypedarray* storage=xl6SequenceStorageV1(owner);
        const void* item=xrtTypedArrayConstGet(storage,state->current);
        if (item) ok=allocation(xrtTypeCopyValue(xrtTypedArrayItemType(storage),target,item));
    }
    xl6SequenceEndV1(owner); return ok;
}
int64_t xl6ArrayIteratorOrdinalV1(const xvalue* value, PARAMS)
{
    XL6_Sequence* owner=begin(value,LAYOUT);
    if (!owner) return -1;
    XL6_SequenceIteratorStateV1* state=xl6SequenceIteratorStateV1(owner);
    int64_t result=-1;
    if (!state->currentValid) xrtSetErrorKind(XERR_STATE);
    else if (state->current > (size_t)INT64_MAX) xrtSetErrorKind(XERR_RANGE);
    else result=(int64_t)state->current;
    xl6SequenceEndV1(owner); return result;
}
