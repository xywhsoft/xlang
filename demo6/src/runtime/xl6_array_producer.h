#ifndef XL6_ARRAY_PRODUCER_H
#define XL6_ARRAY_PRODUCER_H
#include "xl6_array_admission.h"
#include "src/api/xl_foreign_error.h"

/* A private construction transaction, never a published/reference alias.
 * Only closed nonnullable STRING/CHAR/I64 profiles are accepted. Begin is
 * followed by Finish even when it returns false. Push copies the actual
 * physical slot; it does not create per-element dynamic Value wrappers.
 * No partial owner escapes, including failures without an allocated error. */
typedef struct XL6_ArrayProducerV1 {
    xl6_error_scope transaction;
    xvalue* result;
    XL6_Sequence* pin;
} XL6_ArrayProducerV1;
/* The closed producer supplies its own resident descriptor/codec, so an
 * int-only producer does not select string/char implementation islands. */
XL6_TEXT_API bool xl6ArrayProducerBeginV1(XL6_ArrayProducerV1*, uint64_t, uint64_t, unsigned int, const xrttype*, const xvalueconverter*);
XL6_TEXT_API bool xl6ArrayProducerReserveV1(XL6_ArrayProducerV1*, size_t);
XL6_TEXT_API bool xl6ArrayProducerPushV1(XL6_ArrayProducerV1*, const void*);
XL6_TEXT_API xvalue* xl6ArrayProducerFinishV1(XL6_ArrayProducerV1*, bool);
#endif
