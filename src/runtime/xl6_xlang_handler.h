#ifndef XL6_XLANG_HANDLER_H
#define XL6_XLANG_HANDLER_H
#include "xl6_native_handle.h"
#include "xl6_ownership_catalog.h"
#if !defined(XRT_SINGLE_HEADER) && !defined(XRUNTIME_DECLARATIONS_H)
#include "include/xrt_decl.h"
#endif
/* The physical handler stays opaque. Its module, context, lock and native
 * registry are actual host storage, not a generated mirror of that layout.
 * Input views borrow xlang strings, including their trailing NUL outside Size;
 * embedded NUL is rejected before passing them to the public host C API. */
XL6_CODE_LEASE_API void* xl6_xlang_handler_wrap(void*);
XL6_CODE_LEASE_API void* xl6_xlang_handler_create(xstrview,xstrview);
XL6_CODE_LEASE_API void xl6_xlang_handler_destroy(void*);
XL6_CODE_LEASE_API bool xl6_xlang_handler_run_args(void*,struct xvalue*);
XL6_CODE_LEASE_API void* xl6_xlang_handler_last_error_copy(void*);
XL6_CODE_LEASE_API bool xl6XlangHandlerTraceV1(const struct xvalue*,bool (*)(struct xrtownershipref,void*),void*);
XL6_CODE_LEASE_API const XL6_OwnershipBoundaryV1* xl6XlangHandlerBoundaryV1Get(void);
XL6_CODE_LEASE_API const struct xrtownershipquiescencev1* xl6XlangModuleQuiescenceV1Get(void);
XL6_CODE_LEASE_API bool xl6XlangHandlerResolveV1(struct xrtownershipref,const struct xrtownershipadapterv1**,
    const struct xrtownershippreparationv1**,const XL6_OwnershipFinishV1**);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6XlangHandlerHandlePolicyV1Get(void);
#endif
