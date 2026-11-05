#ifndef XL6_NATIVE_CALLBACK_H
#define XL6_NATIVE_CALLBACK_H
#include "xl6_foreign_view.h"
#include "xl6_closure.h"
struct xl6_foreign_error_result;
typedef struct XL6_NativeCallback XL6_NativeCallback;
/* Native publisher MUST retain one real userdata reference before calling
 * its generated typed thunk and release it AFTER that thunk has returned.
 * The resident Enter/Leave below do not protect the thunk's own return tail.
 * Signature is the compiler-sealed physical callback identity; the optional
 * owner is an implicit context binding pin, never a language self-capture.
 * Native storage is the declaration's exact typed root call record. */
typedef struct XL6_NativeCallbackScopeV1 {
    struct XL6_NativeCallbackScopeV1* self;
    XL6_NativeCallback* record;
    XL6_ForeignCallbackScopeV1 callback;
    XL6_CodeScope adapter;
    XL6_ClosureScope closure;
    XL6_ForeignViewCallV1 owner;
} XL6_NativeCallbackScopeV1;
typedef struct XL6_NativeCallbackRuntimeV1 {
    XL6_NativeCallback* (*Create)(const XL6_CodeOwner*,XL6_Closure*,XL6_ForeignView*,uint64_t);
    XL6_NativeCallback* (*Retain)(XL6_NativeCallback*);
    /* Consuming, checked result; ordinary callers own the failure edge. */
    bool (*Release)(XL6_NativeCallback*);
    struct xrtownershipref (*Ownership)(const XL6_NativeCallback*);
    bool (*Enter)(XL6_NativeCallback*,uint64_t,XL6_NativeCallbackScopeV1*,void*,size_t);
    struct xl6_foreign_error_result (*Leave)(XL6_NativeCallbackScopeV1*,struct xl6_foreign_error_result);
    /* Exact C publisher callbacks. Hold is an RC-only mutation; no user code.
     * Drop consumes and also publishes failure to the immediate foreign scope,
     * including NULL diagnostics, because this is a native cleanup callback.
     * The publisher uses the bool for its own checked status propagation. */
    bool (*HoldNative)(const void*);
    bool (*DropNative)(const void*);
} XL6_NativeCallbackRuntimeV1;
XL6_CODE_LEASE_API const XL6_NativeCallbackRuntimeV1* xl6NativeCallbackRuntimeV1Get(void);
/* Consume failure into the immediate checked boundary, or the pinned root
 * when called by a native publisher outside language execution. */
XL6_CODE_LEASE_API bool xl6NativeCallbackReportFailureV1(XL6_NativeCallback*,struct xl6_foreign_error_result);
XL6_CODE_LEASE_API const struct XL6_OwnershipDomainV1* xl6OwnershipDomainNativeCallbackV1Get(void);
#endif
