#ifndef XL6_LIST_TYPED_READ_H
#define XL6_LIST_TYPED_READ_H
#include <stdbool.h>
#include <stdint.h>
#include "xl6_text_value.h"
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_LIST_TYPED_READ_API __declspec(dllexport)
#else
#define XL6_LIST_TYPED_READ_API
#endif
/* Borrowed source; compiler-declared owner, schema and element identities.
 * Authentic native slots use their exact descriptor and operation pin, without
 * Value boxing or schema copies. IntMap is an explicit migration input only.
 * Missing typed keys are checked RANGE errors; scalar zero/false is valid.
 * String returns an independent length-bearing owner, including embedded NUL.
 * Scalar payloads latch the enclosing checked call scope when no diagnostic
 * is available. String instead returns NULL Data on failure and publishes the
 * owned diagnostic locally; a language catch may consume that outcome. */
XL6_LIST_TYPED_READ_API int64_t xl6ListReadI64V1(const xvalue*,uint64_t,uint64_t,uint64_t,int64_t);
XL6_LIST_TYPED_READ_API uint64_t xl6ListReadU64V1(const xvalue*,uint64_t,uint64_t,uint64_t,int64_t);
XL6_LIST_TYPED_READ_API bool xl6ListReadBoolV1(const xvalue*,uint64_t,uint64_t,uint64_t,int64_t);
XL6_LIST_TYPED_READ_API double xl6ListReadF64V1(const xvalue*,uint64_t,uint64_t,uint64_t,int64_t);
XL6_LIST_TYPED_READ_API xstrview xl6ListReadStringV1(const xvalue*,uint64_t,uint64_t,uint64_t,int64_t);
#endif
