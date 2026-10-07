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
#endif
