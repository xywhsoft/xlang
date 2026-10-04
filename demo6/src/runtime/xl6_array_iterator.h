#ifndef XL6_ARRAY_ITERATOR_H
#define XL6_ARRAY_ITERATOR_H
#include "src/runtime/xl6_sequence.h"

/* Compiler-private typed snapshot cursor. Create borrows the source and owns
 * an independent container snapshot; managed elements retain their reference
 * semantics. Its xvalue shell and wrapper use the existing Sequence graph.
 * Read copies into an initialized expression-result slot, never exposes a
 * borrowed element or a language byref. Advance/Read/Ordinal serialize use;
 * BUSY rejects reentry/concurrency rather than promising thread safety.
 * Destroy is the normal xrtValueRelease, including before the first advance. */
#define XL6_ARRAY_ITERATOR_LAYOUT uint64_t identity, uint64_t elementIdentity, unsigned int transfer, size_t size, const xrttype* type
XL6_TEXT_API xvalue* xl6ArrayIteratorCreateV1(const xvalue*, XL6_ARRAY_ITERATOR_LAYOUT, uint64_t cursorIdentity);
XL6_TEXT_API bool xl6ArrayIteratorAdvanceV1(const xvalue*, XL6_ARRAY_ITERATOR_LAYOUT);
XL6_TEXT_API bool xl6ArrayIteratorReadV1(const xvalue*, XL6_ARRAY_ITERATOR_LAYOUT, void*);
XL6_TEXT_API int64_t xl6ArrayIteratorOrdinalV1(const xvalue*, XL6_ARRAY_ITERATOR_LAYOUT);
#undef XL6_ARRAY_ITERATOR_LAYOUT
#endif
