#ifndef XL6_ERROR_VALUE_H
#define XL6_ERROR_VALUE_H
#include <stdint.h>
#if !defined(XRT_SINGLE_HEADER) && !defined(XRUNTIME_DECLARATIONS_H)
#include "include/xruntime_decl.h"
#endif
#ifndef XL6_TEXT_API
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_TEXT_API __declspec(dllexport)
#else
#define XL6_TEXT_API
#endif
#endif

/* Resident immutable Error boxes. A logical TypeId alone is never permission
 * to interpret a native handle as xerror: both the resident Ops and Trace
 * identities are authenticated. No generated code address is retained. */
XL6_TEXT_API xvalue* xl6ErrorValueRefV1(const xerror*, uint64_t identity);
/* Nullable null succeeds with a NULL reference. On failure output is untouched. */
XL6_TEXT_API bool xl6ErrorValueReadRefV1(const xvalue*, uint64_t identity, xerror**);
/* Non-errors return NULL without changing the diagnostic; borrowed result. */
XL6_TEXT_API const xerror* xl6ErrorValueBorrowV1(const xvalue*);
XL6_TEXT_API const xrtownershipadapterv1* xl6ErrorValueOwnershipAdapterV1(xrtownershipref);

/* Owned UTF-8 text, exact byte length, {NULL,0} on failure. Primitive strings
 * retain embedded NUL. Error text is its message, just like e.message; this is
 * display/conversion, NOT persistence. Non-error structured values keep the
 * existing XLON representation and its explicit unsupported-value failures. */
XL6_TEXT_API xstrview xl6ValueTextV1(const xvalue*);
#endif
