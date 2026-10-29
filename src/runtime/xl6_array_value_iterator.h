#ifndef XL6_ARRAY_VALUE_ITERATOR_H
#define XL6_ARRAY_VALUE_ITERATOR_H
#include "xl6_array_iterator.h"
/* Sealed erasure probe: 0 is another carrier (no error/output writes), -1 is
 * failure, 1 publishes an owned native snapshot and immutable resident codec.
 * No whole-storage wire conversion. The private CURSOR role separates this
 * snapshot from an Array while keeping the original layout identity. */
XL6_TEXT_API int xl6ArrayIteratorCreateErasedV1(const xvalue*, xvalue**,
    XL6_SequenceLayoutV1*, const xvalueconverter**);
/* A step transfers one owned projection. false is failure, never EOF; outputs
 * are cleared first. Projection failure permanently invalidates this cursor. */
XL6_TEXT_API bool xl6ArrayIteratorAdvanceValueV1(const xvalue*, uint64_t, uint64_t,
    unsigned int, size_t, const xrttype*, const xvalueconverter*, bool*, xvalue**, size_t*);
#endif
