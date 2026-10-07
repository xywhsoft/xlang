#ifndef XL6_PERSISTENCE_RESTORE_ARRAY_H
#define XL6_PERSISTENCE_RESTORE_ARRAY_H
#include "src/runtime/xl6_text_value.h"
#include "src/runtime/xl6_array_import.h"

/* Both owners are consumed. The seed was constructed from verified IR with
 * its full resident admission/projection, not fabricated by binding a TypeId
 * to a wire array. Converted children already have their actual native ABI.
 * This synchronous call retains no generated restore callback or schema. */
static inline xvalue* xl6PersistenceRestoreArrayTakeV1(xvalue* seed, xvalue* wire)
{
    xvalue* result = NULL;
    if (seed != NULL && wire != NULL) result = xl6ArrayImportTakeV1(seed, wire);
    else {
        xerror* failure = xrtTakeError();
        xrtValueRelease(seed);
        xrtClearError(); xrtSetErrorTake(failure);
    }
    xerror* failure = xrtTakeError();
    xrtValueRelease(wire);
    xrtClearError(); xrtSetErrorTake(failure);
    return result;
}
#endif
