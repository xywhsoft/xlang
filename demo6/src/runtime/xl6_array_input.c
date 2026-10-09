#include "src/runtime/xl6_array_input.h"
#include "src/runtime/xl6_sequence_projection.h"
#include "src/api/xl_foreign_error.h"
#include <limits.h>
#include <stdlib.h>

void xl6ArrayInputReleaseV1(XL6_ArrayInputV1* input)
{
    if (!input) return;
    XL6_ArrayInputV1 owned=*input; *input=(XL6_ArrayInputV1){0};
    xl6_error_scope cleanup={0};
    xerror* incoming=xrtErrorRef(xrtGetError());
    if(!xl6ErrorScopeBeginCleanup(&cleanup,false)) {
        xerror* first=incoming ? incoming : xrtTakeError();
        xrtClearError(); xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtErrorRef(first)});
        for(size_t i=0;i<owned.count;++i) { xrtClearError(); xrtValueRelease(owned.items[i]); }
        xrtFree(owned.items); xrtClearError(); xrtSetErrorTake(first); return;
    }
    xrtErrorFree(incoming);
    for (size_t i=0;i<owned.count;++i) {
        xrtValueRelease(owned.items[i]);
        if(xrtGetError()) xl6ErrorScopeCaptureFailure(&cleanup);
    }
    xrtFree(owned.items);
    xl6_error_result outcome=xl6ErrorScopeEnd(&cleanup);
    if(!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    else xrtErrorFree(outcome.error);
}
bool xl6ArrayInputAcquireV1(const xvalue* source, XL6_ArrayInputV1* output)
{
    if (!output || output->items || output->count) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    xl6_error_scope transaction={0};
    if(!xl6ErrorScopeBegin(&transaction)) return false;
    XL6_Sequence* owner=NULL; XL6_SequenceLayoutV1 layout;
    const xvalueconverter* converter=NULL;
    int status=xl6SequenceBeginErasedArrayV1(source,&owner,&layout,&converter);
    if (status<0) {
        xl6ErrorScopeCaptureFailure(&transaction);
        xl6ErrorScopePropagateFailure(xl6ErrorScopeEnd(&transaction));
        return false;
    }
    xvalue* wire=NULL; size_t count;
    if (status) count=xrtTypedArrayCount(xl6SequenceStorageV1(owner));
    else {
        if (!source || xrtValueType(source)!=XVALUE_ARRAY) {
            xrtSetErrorKind(XERR_TYPE);
            xl6ErrorScopeCaptureFailure(&transaction);
            xl6ErrorScopePropagateFailure(xl6ErrorScopeEnd(&transaction));
            return false;
        }
        wire=xrtValueRetain(source);
        if (!wire) {
            xl6ErrorScopeCaptureFailure(&transaction);
            xl6ErrorScopePropagateFailure(xl6ErrorScopeEnd(&transaction));
            return false;
        }
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
    if(!ok) xl6ErrorScopeCaptureFailure(&transaction);
    /* End may drop the last owner after a reentrant allocator released the
     * source shell. Prepared elements already own their independent edges. */
    if (owner) xl6SequenceEndV1(owner);
    xrtValueRelease(wire);
    if(xrtGetError()) xl6ErrorScopeCaptureFailure(&transaction);
    if(transaction.failed) xl6ArrayInputReleaseV1(&prepared);
    xl6_error_result outcome=xl6ErrorScopeEnd(&transaction);
    if(!outcome.ok) { xl6ErrorScopePropagateFailure(outcome); return false; }
    *output=prepared; return true;
}
