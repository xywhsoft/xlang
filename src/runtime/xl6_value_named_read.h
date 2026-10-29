#ifndef XL6_VALUE_NAMED_READ_H
#define XL6_VALUE_NAMED_READ_H
#include <stdbool.h>
#include <stddef.h>
typedef struct xvalue xvalue;
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_VALUE_NAMED_READ_API __declspec(dllexport)
#else
#define XL6_VALUE_NAMED_READ_API
#endif
/* Borrow receiver/exact key span; return one independent owner. Dict reads
 * authenticate the resident family and project under its operation pin.
 * Object reads preserve declared-before-open routing. Absence is owned null;
 * recognized native failure is never a miss. Named access requires a record;
 * indexing alone permits unsupported source kinds to be successful absence.
 * NULL/Value-null receivers are absence. No key boxing, TLS result or borrow. */
XL6_VALUE_NAMED_READ_API xvalue* xl6ValueNameReadCopyV1(const xvalue*,const char*,size_t,bool);
#endif
