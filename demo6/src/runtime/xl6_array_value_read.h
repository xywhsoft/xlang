#ifndef XL6_ARRAY_VALUE_READ_H
#define XL6_ARRAY_VALUE_READ_H
#include <stdint.h>
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
/* Open dynamic index dispatch: integer Array/list or exact byte string object
 * keys; unsupported key/carrier and missing items yield owned canonical null.
 * Recognized native Array failures remain failures. No callback/context escapes. */
XL6_ARRAY_VALUE_READ_API xvalue* xl6ValueIndexReadCopyV1(const xvalue*,const xvalue*);
#endif
