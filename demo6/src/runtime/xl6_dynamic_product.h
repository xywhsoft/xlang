#ifndef XL6_DYNAMIC_PRODUCT_H
#define XL6_DYNAMIC_PRODUCT_H
#include "xl6_text_value.h"

/* Borrows callable and array storage, consumes every positional/keyword value
 * on both outcomes. Exact N>=2 empty distinct SSA output slots; success owns
 * all N values, failure preserves all slots. This is private product ABI,
 * never public byref syntax. Count mismatch raises, never drops extra values. */
XL6_TEXT_API bool xl6DynamicFunctionProductTakeV1(const xvalue*,size_t,xvalue**,size_t,
    const xstrview*,xvalue**,size_t,xvalue***);
#endif
