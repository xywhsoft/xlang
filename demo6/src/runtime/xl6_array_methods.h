#ifndef XL6_ARRAY_METHODS_H
#define XL6_ARRAY_METHODS_H
#include <stdbool.h>
#include <stdint.h>
typedef struct xvalue xvalue;
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_ARRAY_METHODS_API __declspec(dllexport)
#else
#define XL6_ARRAY_METHODS_API
#endif
/* Borrowed receiver/items. A recognized native Array must be bound and idle;
 * failures never fall back to wire storage. Methods use nonnegative indices,
 * distinct from the relative indexing expression ABI. Invalid extents raise
 * RangeError; absent indexOf/removeValue are normal -1/false results.
 * Native conversion/equality runs under the same owner pin as the mutation.
 * Clear/shrink-like removal may commit before a destructor reports an error.
 * These are checked resident calls, not no-raise generated helper bodies. */
XL6_ARRAY_METHODS_API bool xl6ArrayMethodClearV1(xvalue*);
XL6_ARRAY_METHODS_API int64_t xl6ArrayMethodCapacityV1(const xvalue*);
XL6_ARRAY_METHODS_API bool xl6ArrayMethodReserveV1(xvalue*,int64_t);
XL6_ARRAY_METHODS_API bool xl6ArrayMethodTrimV1(xvalue*);
XL6_ARRAY_METHODS_API bool xl6ArrayMethodSwapV1(xvalue*,int64_t,int64_t);
XL6_ARRAY_METHODS_API bool xl6ArrayMethodInsertV1(xvalue*,int64_t,const xvalue*);
XL6_ARRAY_METHODS_API int64_t xl6ArrayMethodIndexOfV1(const xvalue*,const xvalue*);
XL6_ARRAY_METHODS_API bool xl6ArrayMethodReverseV1(xvalue*);
XL6_ARRAY_METHODS_API bool xl6ArrayMethodRemoveValueV1(xvalue*,const xvalue*);
XL6_ARRAY_METHODS_API bool xl6ArrayMethodRemoveV1(xvalue*,int64_t,int64_t);
/* Owned element, or NULL on failure. pop ignores index (which must be 0).
 * Projection/admission complete under the operation pin before removal. */
XL6_ARRAY_METHODS_API xvalue* xl6ArrayMethodTakeV1(xvalue*,int64_t,bool);
#endif
