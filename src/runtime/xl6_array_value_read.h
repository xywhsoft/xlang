#ifndef XL6_ARRAY_VALUE_READ_H
#define XL6_ARRAY_VALUE_READ_H
#include <stdint.h>
#include "xl6_value_index_read.h"
typedef struct xvalue xvalue;
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_ARRAY_VALUE_READ_API __declspec(dllexport)
#else
#define XL6_ARRAY_VALUE_READ_API
#endif
/* Borrow receiver/key; return one independent owned Value reference. Native
 * slots project under the same operation pin; wire items retain, never clone.
 * NULL is FAILURE even without a diagnostic. Safe misses return the canonical
 * null Value, not NULL. Invalid native domains/BUSY/projection failures are not
 * misses and never fall back. A safe nullable receiver may itself be absent. */
XL6_ARRAY_VALUE_READ_API xvalue* xl6ArrayValueReadCopyV1(const xvalue*,int64_t);
XL6_ARRAY_VALUE_READ_API xvalue* xl6ArrayValueReadOrNullCopyV1(const xvalue*,int64_t);
/* Resident dynamic dispatcher uses this family-specific query. Unrelated
 * carriers and absent items return owned canonical null; an authenticated
 * native Array's failure remains failure. No List dependency is linked. */
XL6_ARRAY_VALUE_READ_API xvalue* xl6ArrayValueReadIfArrayCopyV1(const xvalue*,int64_t);
#endif
