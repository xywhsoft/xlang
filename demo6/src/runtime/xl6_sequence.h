#ifndef XL6_SEQUENCE_H
#define XL6_SEQUENCE_H
#include "src/runtime/xl6_text_value.h"

/* Native-only shared owner for contiguous typed storage. The opaque token
 * pins one wrapper and closes its graph while an operation runs. No raw
 * element borrows or parameter write-back are exposed to xlang. */
typedef struct XL6_Sequence XL6_Sequence;
XL6_TEXT_API bool xl6SequenceTupleV1(uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
XL6_TEXT_API const xrttype* xl6SequenceStorageTypeV1(unsigned int, const xrttype*);
/* Consumes valid owned storage on both success and failure. */
XL6_TEXT_API xvalue* xl6SequenceWrapV1(xtypedarray*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
XL6_TEXT_API XL6_Sequence* xl6SequenceBeginV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
XL6_TEXT_API XL6_Sequence* xl6SequenceBeginOtherV1(const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*, XL6_Sequence*);
XL6_TEXT_API xtypedarray* xl6SequenceStorageV1(XL6_Sequence*);
XL6_TEXT_API void xl6SequenceEndV1(XL6_Sequence*);
XL6_TEXT_API bool xl6SequenceEqualsV1(const xvalue*, const xvalue*, uint64_t, uint64_t, unsigned int, size_t, const xrttype*);
XL6_TEXT_API const xrtownershipadapterv1* xl6SequenceOwnershipAdapterV1(xrtownershipref);
#endif
