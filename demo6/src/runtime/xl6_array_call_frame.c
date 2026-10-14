#include "src/runtime/xl6_array_call_frame.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_call_frame_expansion_internal.h"
#include "src/api/xl_foreign_error.h"
#include <limits.h>
#include <stdlib.h>

static xvalue* failed(xvalue* owner)
{
    xl6_error_scope scope = {0};
    if (!xl6ErrorScopeBeginCleanup(&scope, true)) abort();
    xrtValueRelease(owner);
    if (xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome = xl6ErrorScopeEnd(&scope);
    xrtSetErrorTake(outcome.error);
    return NULL;
}
xvalue* xl6ArrayCallFramePackV1(uint64_t identity, uint64_t elementIdentity,
    const xrtcallframe* frame, size_t fixed)
{
    if (!identity || !elementIdentity || !frame ||
        (frame->ArgumentCount && !frame->Arguments)) {
        xrtSetErrorKind(XERR_ARGUMENT); return NULL;
    }
    size_t count = frame->ArgumentCount > fixed ? frame->ArgumentCount - fixed : 0;
    if (count > (size_t)INT64_MAX || count > SIZE_MAX / sizeof(xvalue*)) {
        xrtSetErrorKind(XERR_RANGE); return NULL;
    }
    for (size_t i = 0; i < count; ++i) {
        if (!frame->Arguments[fixed + i]) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    }
    xvalue* owner = xl6SequenceReferenceCreateV1(XL6_SEQUENCE_ARRAY_V1, identity,
        elementIdentity, XL6_VALUE_ADMIT_ANY, false, elementIdentity);
    if (!owner) return NULL;
    if (!xl6ArrayReserveV1(owner, identity, elementIdentity, 3, sizeof(xvalue*),
        xrtTypeValue(), (int64_t)count)) return failed(owner);
    owner = xl6ArrayProjectionTakeV1(owner, identity, elementIdentity, 3,
        sizeof(xvalue*), xrtTypeValue(), NULL);
    if (!owner) return NULL;
    for (size_t i = 0; i < count; ++i) {
        xvalue* item = xrtValueRetain(frame->Arguments[fixed + i]);
        if (!item || !xl6ArrayValueAppendTakeV1(owner, item)) {
            if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
            return failed(owner);
        }
    }
    return owner;
}

bool xl6ArrayCallFrameExpandV1(uint64_t identity, uint64_t elementIdentity,
    const xvalue* tail, const xrtcallframe* fixed, xrtcallframe* output, void** state)
{
    if (!identity || !elementIdentity || !xl6FrameExpansionInputs(tail,fixed,output,state)) {
        xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    XL6_FrameExpansion* owner=xrtCalloc(1,sizeof(*owner)); if (!owner) return false;
    owner->snapshot=xl6ArrayEraseV1(tail,identity,elementIdentity,3,sizeof(xvalue*),xrtTypeValue(),NULL);
    if (!owner->snapshot) goto fail;
    int64_t size=xl6ArrayErasedSizeV1(owner->snapshot);
    if (size<0) goto fail;
    if ((uint64_t)size>SIZE_MAX) {
        xrtSetErrorKind(XERR_RANGE); goto fail;
    }
    size_t count=(size_t)size;
    if (!xl6FrameExpansionFixed(owner,fixed,count)) goto fail;
    for (size_t i=0; i<count; ++i) {
        xvalue* value=xl6ArrayErasedReadV1(owner->snapshot,(int64_t)i);
        if (!value) goto fail;
        owner->arguments[owner->argumentCount++]=value;
    }
    xl6FrameExpansionPublish(owner,fixed,output,state); return true;
fail: return xl6FrameExpansionFailed(owner);
}
