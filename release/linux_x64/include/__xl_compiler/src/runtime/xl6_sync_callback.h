#ifndef XL6_SYNC_CALLBACK_H
#define XL6_SYNC_CALLBACK_H
#include <xrt_decl.h>
#include "xl6_closure.h"
#include "xl6_foreign_callback_scope.h"
struct xl6_foreign_error_result;

/* Synchronous, nonescaping native callbacks only. Zero initialized caller
 * stack storage, never copied while live. The native API joins all callback
 * attempts before returning and never keeps either function or userdata.
 * The caller independently owns adapter code residency through End. No heap
 * adapter, ownership slot, or copied argument payload is created here.
 * Fields are runtime-private; failure publication is serialized because a
 * misbehaving native worker may attempt a callback on the wrong thread. */
typedef struct XL6_SyncCallbackContextV1 {
    struct XL6_SyncCallbackContextV1* self;
    XL6_ForeignCallbackScopeV1 boundary;
    XL6_CodeActivityV1 activity;
    XL6_ClosureScope closure;
    struct XL6_SyncCallbackScopeV1* active;
    uint64_t signature;
    xatomic32 failed;
    bool completed;
    xerror* firstError;
} XL6_SyncCallbackContextV1;
typedef struct XL6_SyncCallbackScopeV1 {
    struct XL6_SyncCallbackScopeV1* self;
    XL6_SyncCallbackContextV1* context;
    struct XL6_SyncCallbackScopeV1* previous;
    XL6_Closure* closure; /* Borrowed from the complete synchronous invocation. */
} XL6_SyncCallbackScopeV1;
XL6_CODE_LEASE_API bool xl6SyncCallbackBeginV1(XL6_SyncCallbackContextV1*,const XL6_CodeOwner*,XL6_Closure*,uint64_t);
XL6_CODE_LEASE_API bool xl6SyncCallbackEnterV1(XL6_SyncCallbackContextV1*,uint64_t,XL6_SyncCallbackScopeV1*);
/* Leave preserves the callback checked result. Closure/code activity spans
 * the whole C invocation; End performs its cleanup and handles preparation
 * aborts that did not reach Complete. */
XL6_CODE_LEASE_API struct xl6_foreign_error_result xl6SyncCallbackLeaveV1(XL6_SyncCallbackScopeV1*,struct xl6_foreign_error_result);
XL6_CODE_LEASE_API void xl6SyncCallbackReportV1(XL6_SyncCallbackContextV1*,struct xl6_foreign_error_result);
/* Producer completion precedes result admission. Consumes the first callback
 * failure once; contexts and code remain live until the checked cleanup. */
XL6_CODE_LEASE_API struct xl6_foreign_error_result xl6SyncCallbackCompleteV1(XL6_SyncCallbackContextV1*);
XL6_CODE_LEASE_API struct xl6_foreign_error_result xl6SyncCallbackEndV1(XL6_SyncCallbackContextV1*);
#endif
