#ifndef XL6_SEQUENCE_H
#define XL6_SEQUENCE_H
#include "src/runtime/xl6_text_value.h"

/* Native-only shared owner for contiguous typed storage. The opaque token
 * pins one wrapper and closes its graph while an operation runs. No raw
 * element borrows or parameter write-back are exposed to xlang. */
typedef struct XL6_Sequence XL6_Sequence;
/* Native-only cursor state belongs to the counted Sequence wrapper, not a
 * second owning node. Access is valid only inside a Begin/End operation pin.
 * No slot pointer is cached: snapshot storage is the sole element owner. */
typedef struct XL6_SequenceIteratorStateV1 {
    size_t next, current;
    bool prepared, ended, currentValid;
} XL6_SequenceIteratorStateV1;
XL6_TEXT_API XL6_SequenceIteratorStateV1* xl6SequenceIteratorStateV1(XL6_Sequence*);
/* A nullable managed carrier may use the C ABI's NULL slot or XRT's immortal
 * null value. They denote one absent value; never send a NULL C pointer to
 * xrtValueEqual, which requires two actual xvalue instances. */
static inline bool xl6SequenceReferenceEqualV1(const xvalue* left, const xvalue* right)
{
    if (left == right) return true;
    if (!left || !right) return xrtValueType(left ? left : right) == XVALUE_NULL;
    return xrtValueEqual(left,right);
}
XL6_TEXT_API bool xl6SequenceTupleV1(uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
XL6_TEXT_API const xrttype* xl6SequenceStorageTypeV1(unsigned int, const xrttype*);
/* Consumes valid owned storage on both success and failure. */
XL6_TEXT_API xvalue* xl6SequenceWrapV1(xtypedarray*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
XL6_TEXT_API XL6_Sequence* xl6SequenceBeginV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
XL6_TEXT_API XL6_Sequence* xl6SequenceIteratorBeginV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
XL6_TEXT_API XL6_Sequence* xl6SequenceBeginOtherV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*, XL6_Sequence*);
XL6_TEXT_API xtypedarray* xl6SequenceStorageV1(XL6_Sequence*);
XL6_TEXT_API void xl6SequenceEndV1(XL6_Sequence*);
XL6_TEXT_API bool xl6SequenceEqualsV1(const xvalue*, const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
XL6_TEXT_API const xrtownershipadapterv1* xl6SequenceOwnershipAdapterV1(xrtownershipref);
#endif
