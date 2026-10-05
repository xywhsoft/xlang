#ifndef XL6_STACK_BATCH_H
#define XL6_STACK_BATCH_H
#include "src/runtime/xl6_sequence.h"

/* Private native Array boundary, NOT a legacy boxed-array adapter. Both
 * identities are verified separately; only element/physical layout is shared.
 * Output is an already-owned empty expression result with final admission
 * metadata, never a user byref argument. Negative sizes/depths are errors. */
#define XL6_STACK_BATCH_LAYOUT uint64_t, uint64_t, unsigned int, size_t, const xrttype*
XL6_TEXT_API bool xl6StackPushBatchV1(const xvalue*, XL6_STACK_BATCH_LAYOUT, const xvalue*, uint64_t);
XL6_TEXT_API bool xl6StackExtractBatchV1(const xvalue*, XL6_STACK_BATCH_LAYOUT, const xvalue*, uint64_t,
    int64_t, int64_t, bool);
#undef XL6_STACK_BATCH_LAYOUT
#endif
