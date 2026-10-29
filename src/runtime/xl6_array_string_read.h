#ifndef XL6_ARRAY_STRING_READ_H
#define XL6_ARRAY_STRING_READ_H
#include "xl6_text_value.h"

/* Borrowed receiver and key, independent owned UTF-8 result. Native ARRAY
 * identity and the exact resident string descriptor are checked under one
 * operation pin. No boxed intermediate or whole-container snapshot. Explicit
 * wire inputs use the same negative-index rules. NULL Data is failure (also
 * if allocation of a diagnostic fails); present empty text has non-NULL Data.
 * The caller drops the successful result with xrtFree(result.Data). */
XL6_TEXT_API xstrview xl6ArrayReadStringV1(const xvalue*, uint64_t, uint64_t, int64_t);
#endif
