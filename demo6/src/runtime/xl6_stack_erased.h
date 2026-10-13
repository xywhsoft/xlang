#ifndef XL6_STACK_ERASED_H
#define XL6_STACK_ERASED_H
#include "xl6_sequence.h"

/* Compiler-private explicit Stack -> dynamic Value boundary. Source is
 * borrowed; success returns one independent owner of the SAME Stack. The
 * exact physical tuple is supplied by verified IR. Projection is immutable,
 * resident and module-independent. Typed-only construction need not bind it.
 * Transfer 3 selects the shared retaining reference converter, not graph copy. */
XL6_TEXT_API xvalue* xl6StackEraseV1(const xvalue*, uint64_t, uint64_t,
    unsigned int, size_t, const xrttype*, const xvalueconverter*);
/* Nullable source uses the canonical Value null sentinel, never a failed
 * NULL native result. The exact tuple is still required for absent input. */
XL6_TEXT_API xvalue* xl6StackEraseV2(const xvalue*, uint64_t, uint64_t,
    unsigned int, size_t, const xrttype*, const xvalueconverter*, bool);
/* Source is consumed on success AND failure. Lowering acquires an independent
 * typed owner first when its source is borrowed. NULL denotes failed output. */
XL6_TEXT_API xvalue* xl6StackEraseTakeV1(xvalue*, uint64_t, uint64_t,
    unsigned int, size_t, const xrttype*, const xvalueconverter*, bool);
/* Borrowed receiver/item. Admission and conversion complete before append;
 * an error leaves count/content unchanged (capacity may have grown). */
XL6_TEXT_API bool xl6StackErasedPushV1(const xvalue*, const xvalue*);
/* Borrowed receiver, independently owned result. Depth is measured from the
 * top; pop only accepts depth 0. Empty/bad depth raises RANGE, not null data.
 * Encode/admit the result before removing storage. An encoding failure,
 * including an explicit failure with no diagnostic, does not pop the item.
 * Once removal starts, a failing Drop may report committed mutation. */
XL6_TEXT_API xvalue* xl6StackErasedReadV1(const xvalue*, int64_t, bool);
#endif
