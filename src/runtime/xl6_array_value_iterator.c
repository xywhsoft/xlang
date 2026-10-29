#include "src/runtime/xl6_array_value_iterator.h"
#include "src/runtime/xl6_sequence_projection.h"

int xl6ArrayIteratorCreateErasedV1(const xvalue* source, xvalue** target,
    XL6_SequenceLayoutV1* targetLayout, const xvalueconverter** targetProjection)
{
    if (!target || !targetLayout || !targetProjection) { xrtSetErrorKind(XERR_ARGUMENT); return -1; }
    XL6_Sequence* owner=NULL; XL6_SequenceLayoutV1 layout;
    const xvalueconverter* projection=NULL;
    int status=xl6SequenceBeginErasedArrayV1(source,&owner,&layout,&projection);
    if (status!=1) return status;
    xvalue* result=xl6ArrayIteratorSnapshotV1(owner,layout.identity,layout.elementIdentity,
        layout.transfer,layout.size,layout.type,layout.identity);
    if (!result) return -1;
    *target=result; *targetLayout=layout; *targetProjection=projection; return 1;
}
bool xl6ArrayIteratorAdvanceValueV1(const xvalue* value, uint64_t identity,
    uint64_t element, unsigned int transfer, size_t size, const xrttype* type,
    const xvalueconverter* converter, bool* found, xvalue** item, size_t* ordinal)
{
    if (found) *found=false;
    if (item) *item=NULL;
    if (ordinal) *ordinal=0;
    if (!found || !item || !ordinal || (converter && converter->Context)) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    XL6_Sequence* owner=xl6SequenceIteratorBeginV1(value,identity,element,transfer,size,type);
    if (!owner) return false;
    XL6_SequenceIteratorStateV1* state=xl6SequenceIteratorStateV1(owner);
    xtypedarray* storage=xl6SequenceStorageV1(owner);
    bool ok=!state->failed;
    state->currentValid=false;
    if (!ok) xrtSetErrorKind(XERR_STATE);
    else if (!state->ended && state->next<xrtTypedArrayCount(storage)) {
        xvalue* projected=xl6SequenceProjectionTakeV1(owner,
            xrtTypedArrayGetValue(storage,state->next,converter));
        ok=projected!=NULL;
        if (!ok && !xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
        if (ok) {
            state->current=state->next++; state->currentValid=true;
            *item=projected; *ordinal=state->current; *found=true;
        }
    } else state->ended=true;
    if (!ok) { state->failed=true; state->ended=true; }
    xl6SequenceEndV1(owner); return ok;
}
