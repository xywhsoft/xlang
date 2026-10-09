#ifndef XL6_SEQUENCE_PROJECTION_H
#define XL6_SEQUENCE_PROJECTION_H
#include "xl6_sequence.h"

/* Consumes an independently owned encoded primitive on both paths. Reference
 * policies leave its actual identity intact. Scalar identities are bound and
 * re-admitted before a take removes storage or a snapshot is published.
 * XRT builtin conversion deliberately precedes custom converters; metadata
 * must not rely on a converter intercepting that builtin fast path. */
static inline xvalue* xl6SequenceProjectionTakeV1(XL6_Sequence* owner,xvalue* value)
{
    if (!value) return NULL;
    uint64_t identity=xl6SequenceBoxedIdentityV1(owner);
    if (!identity) return value;
    /* XRT's canonical bools (like null) are immutable process-lifetime
     * singletons. Their tag already fixes the one bool language domain; do
     * not allocate a replacement or mutate shared metadata just to box bool. */
    xvaluetype tag=xrtValueType(value);
    if ((tag==XVALUE_NULL || tag==XVALUE_BOOL || xrtValueTypeIdBind(value,identity)) &&
        xl6SequenceAdmitValueV1(owner,value)) return value;
    xerror* error=xrtTakeError(); xrtValueRelease(value);
    xrtClearError(); xrtSetErrorTake(error); return NULL;
}
#endif
