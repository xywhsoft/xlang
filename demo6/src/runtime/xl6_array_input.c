#include "src/runtime/xl6_array_input.h"
#include "src/runtime/xl6_sequence_projection.h"
#include <limits.h>

void xl6ArrayInputReleaseV1(XL6_ArrayInputV1* input)
{
    if (!input) return;
    XL6_ArrayInputV1 owned=*input; *input=(XL6_ArrayInputV1){0};
    xerror* prior=xrtTakeError();
    for (size_t i=0;i<owned.count;++i) {
        xrtValueRelease(owned.items[i]);
        xerror* cleanup=xrtTakeError();
        if (!prior) prior=cleanup;
        else xrtErrorFree(cleanup);
    }
    xrtFree(owned.items);
    xrtClearError(); xrtSetErrorTake(prior);
}
bool xl6ArrayInputAcquireV1(const xvalue* source, XL6_ArrayInputV1* output)
{
    if (!output || output->items || output->count) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    XL6_Sequence* owner=NULL; XL6_SequenceLayoutV1 layout;
    const xvalueconverter* converter=NULL;
    int status=xl6SequenceBeginErasedArrayV1(source,&owner,&layout,&converter);
    if (status<0) return false;
    xvalue* wire=NULL; size_t count;
    if (status) count=xrtTypedArrayCount(xl6SequenceStorageV1(owner));
    else {
        if (!source || xrtValueType(source)!=XVALUE_ARRAY) {
            xrtSetErrorKind(XERR_TYPE); return false;
        }
        wire=xrtValueRetain(source);
        if (!wire) return false;
        count=xrtValueCount(wire);
    }
    XL6_ArrayInputV1 prepared={0}; bool ok=false;
    if (count>INT64_MAX || count>SIZE_MAX/sizeof(*prepared.items)) {
        xrtSetErrorKind(XERR_RANGE); goto done;
    }
    prepared.items=count ? xrtMalloc(count*sizeof(*prepared.items)) : NULL;
    if (count && !prepared.items) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
        goto done;
    }
    for (size_t i=0;i<count;++i) {
        xvalue* item=owner ? xl6SequenceProjectionTakeV1(owner,
            xrtTypedArrayGetValue(xl6SequenceStorageV1(owner),i,converter)) :
            xrtValueRetain(xrtValueArrayGet(wire,i));
        if (!item) { if (!xrtGetError()) xrtSetErrorKind(XERR_STATE); goto done; }
        prepared.items[prepared.count++]=item;
    }
    ok=true;
done:
    /* End may drop the last owner after a reentrant allocator released the
     * source shell. Prepared elements already own their independent edges. */
    if (owner) xl6SequenceEndV1(owner);
    xerror* error=xrtTakeError(); xrtValueRelease(wire);
    if (error) { xrtClearError(); xrtSetErrorTake(error); }
    else if (xrtGetError()) ok=false;
    if (!ok) { xl6ArrayInputReleaseV1(&prepared); return false; }
    *output=prepared; return true;
}
