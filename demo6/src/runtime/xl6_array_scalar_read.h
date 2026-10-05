#ifndef XL6_ARRAY_SCALAR_READ_H
#define XL6_ARRAY_SCALAR_READ_H
#include <stdbool.h>
#include <stdint.h>
typedef struct xvalue xvalue;
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_ARRAY_SCALAR_READ_API __declspec(dllexport)
#else
#define XL6_ARRAY_SCALAR_READ_API
#endif

/* Compiler-private borrowed receiver/index, trivial copied result. Native
 * ARRAY identity, element identity and exact physical descriptor are checked
 * under its real operation pin. No slot callbacks or boxing are performed.
 * Explicit wire Array inputs retain their existing XRT scalar-get semantics.
 * Zero/false is a valid result; failure is reported through the XRT error
 * channel, consumed by the IR's FOREIGN_SCOPE edge, never by abort(). */
XL6_ARRAY_SCALAR_READ_API int64_t xl6ArrayReadI64V1(const xvalue*, uint64_t, uint64_t, int64_t);
XL6_ARRAY_SCALAR_READ_API uint64_t xl6ArrayReadU64V1(const xvalue*, uint64_t, uint64_t, int64_t);
XL6_ARRAY_SCALAR_READ_API bool xl6ArrayReadBoolV1(const xvalue*, uint64_t, uint64_t, int64_t);
XL6_ARRAY_SCALAR_READ_API double xl6ArrayReadF64V1(const xvalue*, uint64_t, uint64_t, int64_t);
#endif
