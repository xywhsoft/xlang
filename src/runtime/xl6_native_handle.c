#define XRT_MODULE_VALUE
#include "src/common/xl6_xrt.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"

bool xl6NativeHandleTraceLeaf(const xvalue* value,
    xrtownershipvisitor visitor, void* context)
{
    (void)value; (void)context;
    return visitor != NULL;
}

xvalue* xl6NativeHandleTakeV1(void** slot, const XL6_NativeHandlePolicyV1* policy)
{
    void* owned;
    xvalue* value;
    xl6_error_scope cleanup = {0};
    xl6_error_result outcome;
    if (!slot || !policy || policy->size != sizeof(*policy) ||
        policy->abiTag != XL6_NATIVE_HANDLE_POLICY_V1_TAG ||
        !policy->Ops || !policy->Ops->Drop || !policy->Trace ||
        policy->Ops->Clone || policy->Ops->Hash || policy->Ops->Equal) {
        xrtSetErrorKind(XERR_ARGUMENT);
        return NULL;
    }
    owned = *slot;
    *slot = NULL;
    value = xrtValueHandleTake(&owned, policy->Ops, NULL);
    if (value && xrtValueHandleOwnershipBindPhased(value, policy->Trace))
        return value;
    /* These are independent private owners, never graph pins. XRT's phased
     * last Drop performs the resident callback outside its own mutation. */
    if (!xl6ErrorScopeBeginCleanup(&cleanup, true)) abort();
    if (value) xrtValueRelease(value);
    else if (owned) policy->Ops->Drop(owned, NULL);
    outcome = xl6ErrorScopeEnd(&cleanup);
    /* The failed factory returns NULL even if the first diagnostic is NULL.
     * Cleanup has finished locally; an enclosing language catch may handle it. */
    xrtSetErrorTake(outcome.error);
    return NULL;
}
