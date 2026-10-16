#include "src/runtime/xl6_array_algebra.h"
#include "src/runtime/xl6_array_input.h"
#include "src/api/xl_foreign_error.h"
#include <limits.h>
#include <stdlib.h>

static void releasePreservingError(xvalue* value)
{
    xerror* prior=xrtTakeError();
    xrtValueRelease(value);
    if (prior) { xrtClearError(); xrtSetErrorTake(prior); }
}

bool xl6ArrayIsValueV1(const xvalue* value)
{
    return value && (xrtValueType(value)==XVALUE_ARRAY ||
        xl6SequenceAdmitsArrayV1(value,xrtValueTypeId(value)));
}
xvalue* xl6ArrayConcatDynamicV1(const xvalue* left, const xvalue* right)
{
    if (left && right && xrtValueType(left)==XVALUE_ARRAY && xrtValueType(right)==XVALUE_ARRAY)
        return xl6ArrayConcatV1(left,right);
    xerror* incoming=xrtTakeError();
    xl6_error_scope operation={0};
    if (!xl6ErrorScopeBegin(&operation)) abort();
    XL6_ArrayInputV1 a={0}, b={0}; xvalue* result=NULL;
    if (!xl6ArrayInputAcquireV1(left,&a) || !xl6ArrayInputAcquireV1(right,&b)) goto done;
    if (a.count>SIZE_MAX-b.count || b.count>(size_t)INT64_MAX ||
        a.count>(size_t)INT64_MAX-b.count) {
        xrtSetErrorKind(XERR_RANGE); goto done;
    }
    result=xrtValueArray();
    if (!result) { if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY); goto done; }
    XL6_ArrayInputV1* inputs[2]={&a,&b};
    for (size_t n=0;n<2;++n) for (size_t i=0;i<inputs[n]->count;++i)
        if (!xrtValueArrayAppendTake(result,&inputs[n]->items[i])) {
            if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
            goto done;
        }
done:
    if (!result || xrtGetError()) xl6ErrorScopeCaptureFailure(&operation);
    xl6ArrayInputReleaseV1(&b); xl6ArrayInputReleaseV1(&a);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&operation);
    if (operation.failed) { releasePreservingError(result); result=NULL; }
    xl6_error_result outcome=xl6ErrorScopeEnd(&operation);
    if (!outcome.ok) { xrtErrorFree(incoming); xrtSetErrorTake(outcome.error); }
    else xrtSetErrorTake(incoming);
    return result;
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
    if (!outcome.ok) { xrtErrorFree(incoming); xrtSetErrorTake(outcome.error); }
    else xrtSetErrorTake(incoming);
    return result;
}
