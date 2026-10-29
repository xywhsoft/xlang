#ifndef XL_FOREIGN_NET_H
#define XL_FOREIGN_NET_H
#include <stdbool.h>
struct xvalue;
struct xnetengine;
struct xtlsclientconfig;
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_FOREIGN_NET_API __declspec(dllexport)
#else
#define XL6_FOREIGN_NET_API
#endif
/* Typed, synchronous extension call boundary. Exact managed handle policies
 * are checked before access. Engine gets a real execution scope; both input
 * shells remain retained across callbacks. Native outputs are borrowed until
 * Visit returns; an accepting transport must independently own its inputs.
 * NULL TLS selects plain transport. No legacy raw-handle API is restored. */
typedef bool (*XL6_ForeignNetClientVisitV1)(struct xnetengine*,const struct xtlsclientconfig*,void*);
XL6_FOREIGN_NET_API bool xl6ForeignNetClientVisitV1(const struct xvalue*,const struct xvalue*,
    XL6_ForeignNetClientVisitV1,void*);
#endif
