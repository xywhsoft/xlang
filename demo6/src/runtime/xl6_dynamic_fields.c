#include "xl6_dynamic_fields.h"
#include "src/api/xl_foreign_error.h"
#include <limits.h>

/* Trace is a pure physical edge query. The boxed handle owns one REAL XRT
 * object strong reference; that object in turn owns one Value per field slot.
 * Do not flatten slots, call a language method, or label this handle a leaf. */
static bool trace(const xvalue* value, xrtownershipvisitor visit, void* context)
{
    const XL6_NativeHandlePolicyV1* policy = xl6DynamicFieldsHandlePolicyV1Get();
    ptr payload = NULL, user = NULL;
    const xvaluehandleops* ops = NULL;
    if (!visit || !xrtValueGetHandle(value, &payload, &ops, &user) ||
        ops != policy->Ops || user || !payload) return false;
    xrtownershipref child = xrtDynamicFieldsOwnership(payload);
    /* Exact native admission before the visitor can invoke Count/Trace. */
    return xrtDynamicFieldsOwnershipAdapterV1(child) && visit(child, context);
}

void xl6_xrt_dynamic_fields_destroy(void* payload)
{
    xl6_error_scope cleanup = {0};
    if (!payload) return;
    if (!xl6ErrorScopeBeginCleanup(&cleanup, false)) abort();
    xrtDynamicFieldsUnref(payload);
    xl6_error_result outcome = xl6ErrorScopeEnd(&cleanup);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
}
static void drop(void* payload, void* user)
{
    if (user) abort();
    xl6_xrt_dynamic_fields_destroy(payload);
}
const XL6_NativeHandlePolicyV1* xl6DynamicFieldsHandlePolicyV1Get(void)
{
    static const xvaluehandleops ops = {NULL, drop, NULL, NULL};
    static const XL6_NativeHandlePolicyV1 policy = {sizeof(policy),
        XL6_NATIVE_HANDLE_POLICY_V1_TAG, &ops, trace};
    return &policy;
}
void* xl6_xrt_dynamic_fields_create(void)
{ return xrtDynamicFieldsCreate(); }
void* xl6_xrt_dynamic_fields_wrap(void* payload)
{
    xrtownershipscope freeze = {0};
    if (!xrtOwnershipFreezeTryBegin(&freeze)) {
        xrtSetErrorKind(XERR_STATE); return NULL;
    }
    bool valid = xrtDynamicFieldsOwnershipAdapterV1(xrtDynamicFieldsOwnership(payload)) != NULL;
    if (!xrtOwnershipScopeEnd(&freeze)) abort();
    if (!valid) { xrtSetErrorKind(XERR_TYPE); return NULL; }
    return payload;
}
void* xl6_xrt_dynamic_fields_native_handle(void* payload)
{ return payload; }
static int64_t countResult(size_t count)
{
    if (count > INT64_MAX) { xrtSetErrorKind(XERR_RANGE); return 0; }
    return (int64_t)count;
}
int64_t xl6_xrt_dynamic_fields_count(void* payload)
{ return countResult(xrtDynamicFieldsCount(payload)); }
int64_t xl6_xrt_dynamic_fields_capacity(void* payload)
{ return countResult(xrtDynamicFieldsCapacity(payload)); }
bool xl6_xrt_dynamic_fields_reserve(void* payload, int64_t capacity)
{
    if (capacity < 0 || (uint64_t)capacity > SIZE_MAX) {
        xrtSetErrorKind(XERR_RANGE); return false;
    }
    return xrtDynamicFieldsReserve(payload, (size_t)capacity);
}
bool xl6_xrt_dynamic_fields_trim(void* payload)
{ return xrtDynamicFieldsTrim(payload); }
bool xl6_xrt_dynamic_fields_clear(void* payload)
{ return xrtDynamicFieldsClear(payload); }
bool xl6_xrt_dynamic_fields_has(void* payload, xstrview name)
{ return xrtDynamicFieldsHas(payload, name); }
/* Absent is an ordinary canonical null; errors are never silently converted
 * to a successful null. No strlen: names always carry their exact byte size. */
static xvalue* lookup(void* payload, xstrview name,
    xvalue* (*read)(const xrtdynamicfields*, xstrview))
{
    xl6_error_scope call = {0};
    if (!xl6ErrorScopeBegin(&call)) return NULL;
    xvalue* value = read(payload, name);
    xl6_error_result outcome = xl6ErrorScopeEnd(&call);
    if (!outcome.ok) {
        xl6ErrorScopePropagateFailure(outcome);
        return NULL;
    }
    return value ? value : xrtValueNull();
}
xvalue* xl6_xrt_dynamic_fields_get(void* payload, xstrview name)
{ return lookup(payload, name, xrtDynamicFieldsGetRef); }
bool xl6_xrt_dynamic_fields_set(void* payload, xstrview name, const xvalue* value)
{ return xrtDynamicFieldsSetRef(payload, name, value); }
bool xl6_xrt_dynamic_fields_remove(void* payload, xstrview name)
{ return xrtDynamicFieldsRemove(payload, name); }
xvalue* xl6_xrt_dynamic_fields_take(void* payload, xstrview name)
{
    xl6_error_scope call = {0};
    if (!xl6ErrorScopeBegin(&call)) return NULL;
    xvalue* value = xrtDynamicFieldsTake(payload, name);
    xl6_error_result outcome = xl6ErrorScopeEnd(&call);
    if (!outcome.ok) { xl6ErrorScopePropagateFailure(outcome); return NULL; }
    return value ? value : xrtValueNull();
}
xvalue* xl6_xrt_dynamic_fields_keys(void* payload)
{ return xrtDynamicFieldsKeys(payload); }
