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
    XL6_Sequence* owner=xl6SequenceBeginV1(source,LAYOUT);
    if (!owner) return NULL;
    xtypedarray* storage=xl6SequenceStorageV1(owner);
    if (xrtTypedArrayCount(storage) > (size_t)INT64_MAX) {
        xrtSetErrorKind(XERR_RANGE); xl6SequenceEndV1(owner); return NULL;
    }
    xtypedarray* snapshot=xrtTypedArrayClone(storage);
    xl6SequenceEndV1(owner);
    xvalue* result=xl6SequenceWrapV1(snapshot,cursorIdentity,elementIdentity,transfer,size,type);
    if (!result) return NULL;
    owner=xl6SequenceBeginV1(result,cursorIdentity,elementIdentity,transfer,size,type);
    if (!owner) { xrtValueRelease(result); return NULL; }
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
