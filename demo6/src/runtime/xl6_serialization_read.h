#ifndef XL6_SERIALIZATION_READ_H
#define XL6_SERIALIZATION_READ_H
#include "src/runtime/xl6_text_value.h"

/* Nullable readers report malformed input/ordinary I/O as a null value, but
 * exhaustion is a checked language failure. The reader owns `prior` after
 * taking it before the XRT operation. Success preserves it; failure retires
 * both it and any partial owned value without overwriting the memory error.
 * A true result leaves ownership of value with the caller, false consumes it.
 * This synchronous error policy is shared by default and configured readers. */
static inline bool xl6SerializeReadFinishV1(xvalue* value, xerror* prior)
{
    if (xrtErrorKind(xrtGetError()) == XERR_MEMORY) {
        xerror* failure = xrtTakeError();
        xrtValueRelease(value);
        xrtErrorFree(prior);
        xrtClearError();
        xrtSetErrorTake(failure);
        return false;
    }
    xrtClearError();
    xrtSetErrorTake(prior);
    return true;
}

/* Typed restore adapters share the nullable reader policy, with physical
 * failure independent of the class/null output. `source` and `prior` are
 * consumed; target and the synchronous assignment callback are borrowed. */
static inline bool xl6SerializeRestoreReadFinishV1(xvalue* source, xvalue* target,
    bool (*assign)(xvalue*, const xvalue*), xerror* prior, xvalue** output)
{
    xvalue* result;
    xerror* failure;
    if (output != NULL) *output = NULL;
    if (output == NULL || assign == NULL) {
        failure = xrtTakeError();
        xrtValueRelease(source); xrtErrorFree(prior); xrtErrorFree(failure);
        xrtClearError(); xrtSetErrorKind(XERR_ARGUMENT); return false;
    }
    result = source != NULL && target != NULL && assign(target, source)
        ? xrtValueRetain(target) : xrtValueRetain(xrtValueNull());
    failure = xrtTakeError();
    xrtValueRelease(source);
    xrtClearError(); xrtSetErrorTake(failure);
    if (!xl6SerializeReadFinishV1(result, prior)) return false;
    *output = result;
    return true;
}
#endif
