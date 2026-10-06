#include "src/runtime/xl6_array_algebra.h"
#include "src/api/xl_foreign_error.h"
#include <stdlib.h>

static void releasePreservingError(xvalue* value)
{
    xerror* prior=xrtTakeError();
    xrtValueRelease(value);
    if (prior) { xrtClearError(); xrtSetErrorTake(prior); }
}

xvalue* xl6ArrayConcatV1(const xvalue* left, const xvalue* right)
{
    xerror* incoming=xrtTakeError();
    xl6_error_scope operation={0};
    if (!xl6ErrorScopeBegin(&operation)) abort();
    XL6_Sequence* a=NULL; XL6_Sequence* b=NULL;
    XL6_SequenceLayoutV1 layout; const xvalueconverter* converter=NULL;
    xvalue* heldLeft=NULL; xvalue* heldRight=NULL; xvalue* result=NULL;
    int status=xl6SequenceBeginErasedArrayV1(left,&a,&layout,&converter);
    if (status<0) goto done;
    if (status) {
        b=xl6SequenceBeginOtherV1(right,layout.identity,layout.elementIdentity,
            layout.transfer,layout.size,layout.type,a);
        if (b) result=xl6SequenceConcatPinnedV1(a,b);
    } else {
        if (!left || !right || xrtValueType(left)!=XVALUE_ARRAY || xrtValueType(right)!=XVALUE_ARRAY) {
            xrtSetErrorKind(XERR_TYPE); goto done;
        }
        /* Keep both shells alive across reentrant allocation/cleanup hooks. */
        heldLeft=xrtValueRetain(left); heldRight=xrtValueRetain(right);
        if (heldLeft && heldRight) result=xrtValueArrayConcat(heldLeft,heldRight);
        if (!result && !xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
    }
done:
    if (!result || xrtGetError()) xl6ErrorScopeCaptureFailure(&operation);
    if (b && b!=a) xl6SequenceEndV1(b);
    if (a) xl6SequenceEndV1(a);
    releasePreservingError(heldRight); releasePreservingError(heldLeft);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&operation);
    if (operation.failed) { releasePreservingError(result); result=NULL; }
    xl6_error_result outcome=xl6ErrorScopeEnd(&operation);
    if (!outcome.ok) { xrtErrorFree(incoming); xl6ErrorScopePropagateFailure(outcome); }
    else xrtSetErrorTake(incoming);
    return result;
}
