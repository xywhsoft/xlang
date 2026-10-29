#ifndef XL6_ARRAY_ENTRY_H
#define XL6_ARRAY_ENTRY_H
#include "xl6_text_value.h"
/* Borrowed host Array data -> owned canonical ANY Array. NULL denotes no
 * arguments. Exact native policies are retained without copying storage;
 * wire/other admitted Array data is explicitly imported, never retagged. */
XL6_TEXT_API xvalue* xl6ArrayEntryAcquireV1(uint64_t, uint64_t, const xvalue*);
/* C/POSIX UTF-8 byte argv is NUL-terminated by its C contract. Count/pointers are validated
 * before allocation; output owns its strings and one reserved native Array. */
XL6_TEXT_API xvalue* xl6ArrayArgvCreateV1(uint64_t, uint64_t, int, char* const*);
#endif
