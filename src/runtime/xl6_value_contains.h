#ifndef XL6_VALUE_CONTAINS_H
#define XL6_VALUE_CONTAINS_H
#include <stdbool.h>
typedef struct xvalue xvalue;
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_VALUE_CONTAINS_API __declspec(dllexport)
#else
#define XL6_VALUE_CONTAINS_API
#endif
/* Borrowed operands. Array values, List values, Set keys, Object string keys,
 * or exact-length string substring. Absence is false; callback/allocation
 * failure is reported through the shared checked error boundary. */
XL6_VALUE_CONTAINS_API bool xl6ValueContainsV1(const xvalue*,const xvalue*);
/* Same checked outcome, restricted to explicit wire Set/Object/text/IntMap.
 * Declared separately so closed Dict/Set queries retain no sequence family. */
XL6_VALUE_CONTAINS_API bool xl6ValueWireContainsV1(const xvalue*,const xvalue*);
#endif
