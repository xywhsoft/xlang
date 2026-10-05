#ifndef XL6_ARRAY_INPUT_H
#define XL6_ARRAY_INPUT_H
#include "src/runtime/xl6_sequence.h"

/* Synchronous standard-library input, NOT a language Array/var conversion.
 * Owns one reference per item (including duplicates). It releases the native
 * operation pin before returning, so consumers may accept aliased inputs and
 * invoke native callbacks without borrowing slots or pinning a mutable Array.
 * Wire/value Arrays remain accepted at the explicit data/host boundary. Other
 * containers, foreign handles, unbound native Arrays and BUSY owners fail.
 * Initialize to zero; acquire publishes only a complete input. Release resets
 * first and preserves an existing failure over cleanup. No module callbacks,
 * contexts or raw element borrows escape from this private resident ABI. */
typedef struct XL6_ArrayInputV1 {
    xvalue** items;
    size_t count;
} XL6_ArrayInputV1;
XL6_TEXT_API bool xl6ArrayInputAcquireV1(const xvalue*, XL6_ArrayInputV1*);
XL6_TEXT_API void xl6ArrayInputReleaseV1(XL6_ArrayInputV1*);
#endif
