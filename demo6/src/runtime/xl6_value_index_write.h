#ifndef XL6_VALUE_INDEX_WRITE_H
#define XL6_VALUE_INDEX_WRITE_H
#include <stdbool.h>
typedef struct xvalue xvalue;
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_VALUE_INDEX_WRITE_API __declspec(dllexport)
#else
#define XL6_VALUE_INDEX_WRITE_API
#endif
/* Borrow receiver/key, consume item exactly once on every path. Integer keys
 * select authenticated native Array/List or their explicit wire inputs; string
 * keys use exact-length objects. Native List enforces its immutable admission
 * domain before decoding; no conversion, rebind or whole-container copy.
 * Missing List keys insert; invalid Array bounds raise. Destructor failure can
 * follow a committed write, but may not replace the operation's first outcome. */
XL6_VALUE_INDEX_WRITE_API bool xl6ValueIndexSetTakeV1(xvalue*,const xvalue*,xvalue*);
#endif
