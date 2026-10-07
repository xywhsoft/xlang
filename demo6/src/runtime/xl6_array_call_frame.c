#include "src/runtime/xl6_array_call_frame.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_erased.h"
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
