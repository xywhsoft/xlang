#ifndef XL6_VALUE_NAMED_WRITE_H
#define XL6_VALUE_NAMED_WRITE_H
#include <stdbool.h>
#include <stddef.h>
typedef struct xvalue xvalue;
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_VALUE_NAMED_WRITE_API __declspec(dllexport)
#else
#define XL6_VALUE_NAMED_WRITE_API
#endif
/* Borrow receiver/key span; consume item on BOTH paths. Native Dict enforces
 * its immutable logical/physical domain; Object uses authenticated field
 * routing. The first checked outcome survives item Drop, including silence.
 * A destructor can report failure after the underlying write committed. */
XL6_VALUE_NAMED_WRITE_API bool xl6ValueNameSetTakeV1(xvalue*,const char*,size_t,xvalue*);
#endif
