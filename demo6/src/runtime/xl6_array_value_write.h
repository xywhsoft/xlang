#ifndef XL6_ARRAY_VALUE_WRITE_H
#define XL6_ARRAY_VALUE_WRITE_H
#include <stdbool.h>
#include <stdint.h>
#include "xl6_value_index_write.h"
typedef struct xvalue xvalue;
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_ARRAY_VALUE_WRITE_API __declspec(dllexport)
#else
#define XL6_ARRAY_VALUE_WRITE_API
#endif
/* Compiler-private borrowed receiver/key, consumed item on EVERY path.
 * Native writes validate the resident domain and mutate its sole storage;
 * wire writes delegate to XRT. Failure preserves the receiver and first error,
 * including during input destruction. No implicit snapshot or constructor
 * conversion. Open index dispatch has its own resident component/header. */
XL6_ARRAY_VALUE_WRITE_API bool xl6ArrayValueAppendTakeV1(xvalue*,xvalue*);
XL6_ARRAY_VALUE_WRITE_API bool xl6ArrayValueSetTakeV1(xvalue*,int64_t,xvalue*);
#endif
