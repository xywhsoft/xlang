#ifndef XL6_SERIALIZATION_READ_H
#define XL6_SERIALIZATION_READ_H
#include "xl6_text_value.h"

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

/* Parsing/ordinary I/O use the nullable reader policy. Declaration factories
 * and assignment are language execution: ALL their failures propagate, not
 * just exhaustion. Keep the two error boundaries separate so an original
 * initializer/destructor exception cannot become a successful null result.
 * `source` and `prior` are consumed; target and assign are borrowed. */
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
    if (!xl6SerializeReadFinishV1(source, prior)) return false;
    prior = xrtTakeError();
    result = source != NULL && target != NULL && assign(target, source)
        ? xrtValueRetain(target) : xrtValueRetain(xrtValueNull());
    failure = xrtTakeError();
    xrtValueRelease(source);
    if (failure != NULL) {
        xrtValueRelease(result);
        xrtErrorFree(prior);
        xrtClearError(); xrtSetErrorTake(failure);
        return false;
    }
    xrtClearError(); xrtSetErrorTake(prior);
    *output = result;
    return true;
}

/* Transitional pending-family adapter only. The new language RuntimeOp owns
 * its construction target even while a family still uses legacy assignment.
 * Do not retain a second language owner or omit target retirement on refusal. */
XL6_TEXT_API bool xl6SerializeRestoreReadFinishTakeV1(xvalue*,xvalue*,
    bool (*)(xvalue*,const xvalue*),xerror*,xvalue**);
#endif
