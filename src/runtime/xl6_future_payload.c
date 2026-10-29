#define XRT_MODULE_MEMORY
#define XRT_MODULE_VALUE
#define XRT_MODULE_FUTURE
#define XRUNTIME_MODULE_RUNTIME_VALUE_FUTURE
#include "src/common/xl6_xrt.h"
#include "xl6_future_payload.h"
#include "xl6_closure.h"
#include "xl6_generator.h"
#include "src/api/xl_foreign_error.h"

bool xl6PayloadReadFutureV1(const xvalue* source, uint64_t identity,
    unsigned int mode, xfuture** output)
{
    xl6_error_scope scope={0}; xfuture* owned=NULL; bool ok=false;
    if (!xl6ErrorScopeBegin(&scope)) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()}); return false;
    }
    if (!identity || !output || *output || mode>XL6_FUTURE_VALUE_TRY) xrtSetErrorKind(XERR_ARGUMENT);
    else if (source && mode==XL6_FUTURE_VALUE_NULLABLE && xrtValueType(source)==XVALUE_NULL) ok=true;
    else if (!source || xrtValueTypeId(source)!=identity || !xrtValueIsFuture(source))
        { if (source && mode==XL6_FUTURE_VALUE_TRY) ok=true; else xrtSetErrorKind(XERR_TYPE); }
    else {
        xfuture* borrowed=xrtValueGetFuture(source);
        if (borrowed) owned=xrtFutureRef(borrowed);
        ok=owned!=NULL;
        if (!ok && !xrtGetError()) xrtSetErrorKind(XERR_STATE);
    }
    if (!ok || xrtGetError()) xl6ErrorScopeCaptureFailure(&scope);
    if (scope.failed) { xrtFutureDestroy(owned); owned=NULL; }
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) { xl6ErrorScopePropagateFailure(outcome); return false; }
    *output=owned; return true;
}

xvalue* xl6PayloadBoxFutureV1(xfuture* future, uint64_t identity)
{
    if (!identity) { xrtSetErrorKind(XERR_ARGUMENT); return NULL; }
    if (!future) return xrtValueRetain(xrtValueNull());
    xvalue* value = xrtValueFuture(future);
    if (!value) {
        if (!xrtGetError()) xrtSetErrorKind(XERR_MEMORY);
        return NULL;
    }
    if (xrtValueTypeIdBind(value, identity)) return value;
    if (!xrtGetError()) xrtSetErrorKind(XERR_TYPE);
    xerror* failure = xrtTakeError();
    xrtValueRelease(value); xrtClearError(); xrtSetErrorTake(failure);
    return NULL;
}

static void xl6FuturePayloadDropValue(void* value, void* context)
{ (void)context; xrtFree(value); }
static void xl6FuturePayloadDropString(void* value, void* context)
{
    (void)context;
    if (value != NULL) { xrtFree((void*)((xstrview*)value)->Data); xrtFree(value); }
}
static void xl6FuturePayloadDropXValue(void* value, void* context)
{
    (void)context;
    if (value != NULL) { xrtValueRelease(*(xvalue**)value); xrtFree(value); }
}
static void xl6FuturePayloadDropError(void* value, void* context)
{
    (void)context;
    if (value != NULL) { xrtErrorFree(*(xerror**)value); xrtFree(value); }
}
static void xl6FuturePayloadDropFuture(void* value, void* context)
{
    (void)context;
    if (value != NULL) { xrtFutureDestroy(*(xfuture**)value); xrtFree(value); }
}
static void xl6FuturePayloadDropClosure(void* value, void* context)
{ xl6ClosureRuntimeV1Get()->PayloadDrop(value, context); }
static void xl6FuturePayloadDropGenerator(void* value, void* context)
{
    (void)context;
    if (value != NULL) {
        xl6GeneratorRuntimeV1Get()->Destroy(*(XL6_Generator**)value);
        xrtFree(value);
    }
}

static bool xl6PayloadTraceLeaf(const void* value, const void* data, xrtownershipvisitor visit, void* context)
{ (void)value; (void)context; return data == NULL && visit != NULL; }
static bool xl6PayloadTraceXValue(const void* value, const void* data, xrtownershipvisitor visit, void* context)
{
    return data == NULL && visit != NULL && (value == NULL ||
        visit(xrtValueOwnership(*(xvalue* const*)value), context));
}
static bool xl6PayloadTraceError(const void* value, const void* data, xrtownershipvisitor visit, void* context)
{
    return data == NULL && visit != NULL && (value == NULL ||
        visit(xrtErrorOwnership(*(xerror* const*)value), context));
}
static bool xl6PayloadTraceFuture(const void* value, const void* data, xrtownershipvisitor visit, void* context)
{
    return data == NULL && visit != NULL && (value == NULL ||
        visit(xrtFutureOwnership(*(xfuture* const*)value), context));
}
static bool xl6PayloadTraceClosure(const void* value, const void* data, xrtownershipvisitor visit, void* context)
{ return xl6ClosureRuntimeV1Get()->PayloadTrace(value, data, visit, context); }
static bool xl6PayloadTraceGenerator(const void* value, const void* data, xrtownershipvisitor visit, void* context)
{
    return data == NULL && visit != NULL && (value == NULL ||
        visit(xl6GeneratorRuntimeV1Get()->Ownership(*(XL6_Generator* const*)value), context));
}
static const xfuturepayloadownershipv1 payloadPolicies[] = {
    {sizeof(xfuturepayloadownershipv1), xl6FuturePayloadDropValue, xl6PayloadTraceLeaf},
    {sizeof(xfuturepayloadownershipv1), xl6FuturePayloadDropString, xl6PayloadTraceLeaf},
    {sizeof(xfuturepayloadownershipv1), xl6FuturePayloadDropXValue, xl6PayloadTraceXValue},
    {sizeof(xfuturepayloadownershipv1), xl6FuturePayloadDropError, xl6PayloadTraceError},
    {sizeof(xfuturepayloadownershipv1), xl6FuturePayloadDropFuture, xl6PayloadTraceFuture},
    {sizeof(xfuturepayloadownershipv1), xl6FuturePayloadDropClosure, xl6PayloadTraceClosure},
    {sizeof(xfuturepayloadownershipv1), xl6FuturePayloadDropGenerator, xl6PayloadTraceGenerator}
};
_Static_assert(
    sizeof(payloadPolicies) / sizeof(payloadPolicies[0]) ==
        XL6_FUTURE_PAYLOAD_COUNT - XL6_FUTURE_PAYLOAD_VALUE,
    "future payload kind table is incomplete");
const xfuturepayloadownershipv1* xl6PayloadPolicyGet(XL6_FuturePayloadKind kind)
{
    return kind >= XL6_FUTURE_PAYLOAD_VALUE && kind < XL6_FUTURE_PAYLOAD_COUNT
        ? &payloadPolicies[kind - 1] : NULL;
}
XL6_FuturePayloadDrop xl6PayloadDropGet(XL6_FuturePayloadKind kind)
{ const xfuturepayloadownershipv1* policy = xl6PayloadPolicyGet(kind); return policy ? policy->Drop : NULL; }
XL6_FuturePayloadTrace xl6PayloadTraceGet(XL6_FuturePayloadKind kind)
{ const xfuturepayloadownershipv1* policy = xl6PayloadPolicyGet(kind); return policy ? policy->Trace : NULL; }
bool xl6PayloadResolveOwned(xpromise* promise, void* value, XL6_FuturePayloadKind kind)
{
    const xfuturepayloadownershipv1* policy = xl6PayloadPolicyGet(kind);
    if (policy == NULL) { xrtSetErrorKind(XERR_ARGUMENT); return false; }
    return xrtPromiseResolveOwnedPolicyV1(promise, value, policy);
}
const xrtownershipadapterv1* xl6PayloadOwnershipAdapterV1(xrtownershipref ref)
{
    static const xfuturepayloadownershipv1* const allowed[] = {
        &payloadPolicies[0], &payloadPolicies[1], &payloadPolicies[2],
        &payloadPolicies[3], &payloadPolicies[4], &payloadPolicies[5],
        &payloadPolicies[6]
    };
    _Static_assert(
        sizeof(allowed) / sizeof(allowed[0]) ==
            XL6_FUTURE_PAYLOAD_COUNT - XL6_FUTURE_PAYLOAD_VALUE,
        "future ownership admission omits a resident payload kind");
    return xrtFutureOwnershipAdapterV1(ref, allowed, sizeof(allowed) / sizeof(allowed[0]));
}

bool xl6PayloadObserveOutcome(int state, const xerror* error)
{
    switch (state) {
    case XFUTURE_RESOLVED: xrtClearError(); return true;
    case XFUTURE_FAILED:
        if (error != NULL) xrtSetErrorTake(xrtErrorRef(error));
        else xrtSetErrorKind(XERR_INTERNAL);
        break;
    case XFUTURE_CANCELLED: {
        xerrordesc desc = {XERR_CANCELLED, 0, 0, "xlang", "await",
            "future cancel requested.", "CancelledError", NULL};
        xerror* cancelled = xrtErrorBuild(&desc);
        if (cancelled != NULL) xrtSetErrorTake(cancelled);
        else xrtSetErrorKind(XERR_CANCELLED);
        break;
    }
    case XFUTURE_CLOSED: xrtSetErrorKind(XERR_CLOSED); break;
    default: xrtSetErrorKind(XERR_STATE); break;
    }
    return false;
}
