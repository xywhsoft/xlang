#ifndef XL6_LIST_VALUE_READ_H
#define XL6_LIST_VALUE_READ_H
#include <stdint.h>
typedef struct xvalue xvalue;
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_LIST_VALUE_READ_API __declspec(dllexport)
#else
#define XL6_LIST_VALUE_READ_API
#endif
/* Borrowed receiver, exact signed sparse key; one independent owned Value.
 * Missing returns the canonical null Value, not failure. Native projection and
 * admission run under the owner's pin. Failure, including without diagnostics,
 * returns NULL and publishes its owned diagnostic without failing the caller's
 * enclosing callback scope. Legacy IntMap is an
 * explicit migration input, not a native handle or a schema fallback. */
XL6_LIST_VALUE_READ_API xvalue* xl6ListValueReadCopyV1(const xvalue*,int64_t);
/* Safe nullable receiver: C NULL / canonical null is a normal owned miss;
 * invalid present owners and projection failures still fail checked. */
XL6_LIST_VALUE_READ_API xvalue* xl6ListValueReadOrNullCopyV1(const xvalue*,int64_t);
/* Ordinary nonnullable typed element: a missing sparse key is RANGE, not an
 * unrelated projection TYPE failure. A present canonical null stays a value. */
XL6_LIST_VALUE_READ_API xvalue* xl6ListValueReadRequiredCopyV1(const xvalue*,int64_t);
#endif
