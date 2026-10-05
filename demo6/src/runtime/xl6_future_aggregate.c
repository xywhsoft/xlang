#include <limits.h>
#include <stdlib.h>
#include "include/xruntime_decl.h"
#include "src/api/xl_future_bridge.h"
#include "src/runtime/xl6_future_payload.h"
#include "src/runtime/xl6_future_aggregate.h"
#include "src/runtime/xl6_future_aggregate_internal.h"

/* Owned mapper kernel: unchanged XRT producer/cancellation/graph policies.
 * Array inputs live in a separately selected adapter, not a function table. */
typedef struct xl6_future_all_xvalues {
    volatile int32 References;
    xl6_future_payload_box_proc Box;
    XL6_CodeLease* Lease;
    const void* Claim;
    unsigned Mode;
    bool Certified, Cleared;
} xl6_future_all_xvalues;
static xfuture* __xl6FutureAggregateStart(unsigned, xfuture* const*, size_t,
    xl6_future_payload_box_proc, const XL6_CodeOwner*, bool);

static void __xl6FutureAggregateReject(xpromise* output, const char* message)
{
    xerror* error;
    __xl6FutureSetAggregateError(message); error = xrtTakeError();
    if (error == NULL || !xrtPromiseReject(output, error)) (void)xrtPromiseClose(output);
    xrtErrorFree(error);
}
static bool __xl6FutureAggregateCount(const void* data, size_t* count)
{
    const xl6_future_all_xvalues* value = data;
    if (!value || !count || value->Cleared || value->References <= 0) return false;
    *count = (size_t)value->References; return true;
}
static bool __xl6FutureAggregateTrace(const void* data, xrtownershipvisitor visit, void* context)
{
    const xl6_future_all_xvalues* value = data; size_t count;
    return visit && __xl6FutureAggregateCount(data, &count) && (!value->Lease || visit(xl6CodeLeaseOwnership(value->Lease), context));
}
static const xrtownershipops __xl6FutureAggregateOps = {__xl6FutureAggregateCount, __xl6FutureAggregateTrace};
static void __xl6FutureAggregateDrop(const void* data)
{
    xl6_future_all_xvalues* value = (xl6_future_all_xvalues*)data; XL6_CodeLease* lease = NULL;
    xrtownershipscope mutation = {0};
    if (!value) return;
    if (!xrtOwnershipMutationBegin(&mutation)) abort();
    int32 left = xrtRefRelease(&value->References);
    if (left < 0) abort();
    if (!left) { lease = value->Lease; value->Lease = NULL; xrtFree(value); }
    if (!xrtOwnershipScopeEnd(&mutation)) abort();
    xl6CodeLeaseRelease(lease); /* only resident code remains after this release */
}
static void __xl6FutureAggregateContextFree(void* data, void* context)
{ if (context) abort(); __xl6FutureAggregateDrop(data); }
static bool __xl6FutureAggregateContextTrace(const void* data, const void* context,
    xrtownershipvisitor visit, void* visitContext)
{
    return context == NULL && visit != NULL && visit((xrtownershipref){data, &__xl6FutureAggregateOps}, visitContext);
}
static bool __xl6FutureAllForwardNonSuccess(const xfutureall* input, xpromise* output)
{
    if (input == NULL) {
        __xl6FutureAggregateReject(output, "task.all has no source descriptor"); return true;
    }
    for (size_t i = 0; i < input->Count; ++i) {
        if (xrtFutureState(input->Futures[i]) != XFUTURE_RESOLVED) {
            if (!xrtPromiseForward(output, input->Futures[i]))
                __xl6FutureAggregateReject(output, "task.all source outcome forwarding failed");
            return true;
        }
    }
    return false;
}
static void __xl6FutureAllXValuesMap(const xfutureall* input, xpromise* output, void* data)
{
    const xl6_future_all_xvalues* all = data;
    xvalue* array; xvalue** box;
    if (input == NULL || all == NULL || all->Box == NULL) {
        __xl6FutureAggregateReject(output, "task.all has no verified payload boxer"); return;
    }
    /* Preserve the exact failing/cancelled/closed source outcome. No boxing
     * and no legacy metadata allocation is needed to observe those outcomes. */
    if (__xl6FutureAllForwardNonSuccess(input, output)) return;
    array = xrtValueArray();
    if (array == NULL) {
        __xl6FutureAggregateReject(output, "task.all result array allocation failed"); return;
    }
    for (size_t i = 0; i < input->Count; ++i) {
        const void* payload = xrtFutureValue(input->Futures[i]);
        xvalue* item = payload != NULL ? all->Box(payload) : NULL;
        if (item == NULL || !xrtValueArrayAppendNew(array, item)) {
            xrtValueRelease(array);
            __xl6FutureAggregateReject(output, "task.all payload boxing failed"); return;
        }
    }
    box = xrtMalloc(sizeof(*box));
    if (box == NULL) {
        xrtValueRelease(array);
        __xl6FutureAggregateReject(output, "task.all result box allocation failed"); return;
    }
    *box = array;
    if (!xl6PayloadResolveOwned(output, box, XL6_FUTURE_PAYLOAD_XVALUE)) {
        xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_XVALUE)(box, NULL);
        __xl6FutureAggregateReject(output, "task.all result publication failed");
    }
}
XL_API xfuture* xl6FutureAllXValues(xfuture** futures, int count, xl6_future_payload_box_proc box)
{
    if (futures == NULL || count <= 0 || box == NULL) {
        __xl6FutureSetAggregateError("task.all requires a non-empty verified Future payload list"); return NULL;
    }
    return __xl6FutureAggregateStart(XL6_AGGREGATE_ALL, futures, (size_t)count, box, NULL, false);
}
static void __xl6FutureAllVoidMap(const xfutureall* input, xpromise* output, void* data)
{
    (void)data;
    if (__xl6FutureAllForwardNonSuccess(input, output)) return;
    if (!xrtPromiseResolve(output, NULL))
        __xl6FutureAggregateReject(output, "task.all void result publication failed");
}
XL_API xfuture* xl6FutureAllChecked(xfuture** futures, int count)
{
    xfuture* output;
    if (futures == NULL || count <= 0) {
        __xl6FutureSetAggregateError("task.all requires a non-empty Future list"); return NULL;
    }
    output = __xl6FutureAggregateStart(XL6_AGGREGATE_ALL, futures, (size_t)count, NULL, NULL, true);
    if (output == NULL) __xl6FutureSetAggregateError("task.all preparation failed");
    return output;
}
static void __xl6FutureRaceMap(const xfuturepick* input, xpromise* output, void* data)
{
    (void)data;
    if (input == NULL || input->Future == NULL || !xrtPromiseForward(output, input->Future))
        __xl6FutureAggregateReject(output, "task.race source outcome forwarding failed");
}
XL_API xfuture* xl6FutureRaceChecked(xfuture** futures, int count)
{
    xfuture* output;
    if (futures == NULL || count <= 0) {
        __xl6FutureSetAggregateError("task.race requires a non-empty Future payload list"); return NULL;
    }
    output = __xl6FutureAggregateStart(XL6_AGGREGATE_RACE, futures, (size_t)count, NULL, NULL, true);
    if (output == NULL) __xl6FutureSetAggregateError("task.race preparation failed");
    return output;
}
static void __xl6FutureWhenAnyIndexMap(const xfuturepick* input, xpromise* output, void* data)
{
    int64* index; (void)data;
    if (input == NULL || input->Future == NULL || input->Index > (size_t)INT64_MAX) {
        __xl6FutureAggregateReject(output, "task.whenAny has no valid source index"); return;
    }
    index = xrtMalloc(sizeof(*index));
    if (index == NULL) {
        __xl6FutureAggregateReject(output, "task.whenAny result allocation failed"); return;
    }
    *index = (int64)input->Index;
    if (!xl6PayloadResolveOwned(output, index, XL6_FUTURE_PAYLOAD_VALUE)) {
        xrtFree(index); __xl6FutureAggregateReject(output, "task.whenAny result publication failed");
    }
}
XL_API xfuture* xl6FutureWhenAnyIndex(xfuture* const* futures, size_t count)
{
    xfuture* output;
    if (futures == NULL || count == 0 || count > (size_t)INT64_MAX) {
        __xl6FutureSetAggregateError("task.whenAny requires a non-empty Future payload list"); return NULL;
    }
    output = __xl6FutureAggregateStart(XL6_AGGREGATE_ANY, futures, count, NULL, NULL, true);
    if (output == NULL) __xl6FutureSetAggregateError("task.whenAny preparation failed");
    return output;
}

static void __xl6FutureAggregateAllMap(const xfutureall* input, xpromise* output, void* data)
{
    xl6_future_all_xvalues* value = data; XL6_CodeScope execution = {0};
    if (!value || value->Cleared || value->Mode != XL6_AGGREGATE_ALL) abort();
    xl6CodeScopeEnter(&execution, value->Lease);
    if (value->Box) __xl6FutureAllXValuesMap(input, output, value);
    else __xl6FutureAllVoidMap(input, output, value);
    xl6CodeScopeLeave(&execution);
}
static void __xl6FutureAggregatePickMap(const xfuturepick* input, xpromise* output, void* data)
{
    xl6_future_all_xvalues* value = data;
    if (!value || value->Cleared || value->Box || value->Lease) abort();
    if (value->Mode == XL6_AGGREGATE_RACE) __xl6FutureRaceMap(input, output, value);
    else if (value->Mode == XL6_AGGREGATE_ANY) __xl6FutureWhenAnyIndexMap(input, output, value);
    else abort();
}
static const xfuturecombineownershipv1 __xl6FutureAggregatePolicy = {
    sizeof(xfuturecombineownershipv1), __xl6FutureAggregateAllMap, __xl6FutureAggregatePickMap,
    __xl6FutureAggregateDrop, &__xl6FutureAggregateOps
};
const xfuturecombineownershipv1* xl6FutureAggregatePolicyV1Get(void)
{ return &__xl6FutureAggregatePolicy; }
static xfuture* __xl6FutureAggregateStart(unsigned mode, xfuture* const* futures, size_t count,
    xl6_future_payload_box_proc box, const XL6_CodeOwner* owner, bool certified)
{
    xl6_future_all_xvalues* data; XL6_CodeLease* lease = NULL; xfuture* output;
    xrtownershipscope mutation = {0};
    if (!futures || !count || count > (size_t)INT64_MAX ||
        (mode != XL6_AGGREGATE_ANY && count > (size_t)INT_MAX) || mode > XL6_AGGREGATE_ANY ||
        (mode != XL6_AGGREGATE_ALL && box) || (!box && owner) || (!certified && owner)) {
        __xl6FutureSetAggregateError("task aggregate requires a valid non-empty Future list and mapper ABI"); return NULL;
    }
    for (size_t i = 0; i < count; ++i) {
        if (futures[i] == NULL) {
            __xl6FutureSetAggregateError(
                __xl6FutureAggregateNullSourceError(mode));
            return NULL;
        }
    }
    if (box && certified && xl6CodeLeaseAcquireScoped(owner, &lease) != XL6_CODE_LEASE_ACQUIRED) {
        __xl6FutureSetAggregateError("task.all mapper code admission failed"); return NULL;
    }
    if (!xrtOwnershipMutationBegin(&mutation)) { xl6CodeLeaseRelease(lease); return NULL; }
    data = xrtCalloc(1, sizeof(*data));
    if (data) {
        data->References = 1; data->Box = box; data->Lease = lease;
        data->Mode = mode; data->Certified = certified;
    }
    if (!xrtOwnershipScopeEnd(&mutation)) abort();
    if (!data) {
        xerror* error = xrtTakeError(); xl6CodeLeaseRelease(lease); xrtClearError(); xrtSetErrorTake(error);
        __xl6FutureSetAggregateError("task aggregate mapper allocation failed"); return NULL;
    }
    if (!certified) {
        /* Legacy borrowed code retains conservative callback scopes and never
         * matches the new native mapped-policy admission contract. */
        output = xrtFutureAllMapOwnedTraced(futures, count, __xl6FutureAggregateAllMap,
            data, __xl6FutureAggregateContextFree, NULL, __xl6FutureAggregateContextTrace);
    } else if (mode == XL6_AGGREGATE_ALL) output = xrtFutureAllMapOwnedPolicyV1(futures, count, data, &__xl6FutureAggregatePolicy);
    else if (mode == XL6_AGGREGATE_RACE) output = xrtFutureRaceMapOwnedPolicyV1(futures, count, data, &__xl6FutureAggregatePolicy);
    else output = xrtFutureAnyMapOwnedPolicyV1(futures, count, data, &__xl6FutureAggregatePolicy);
    if (!output) {
        xerror* error = xrtTakeError(); __xl6FutureAggregateDrop(data); xrtClearError(); xrtSetErrorTake(error);
        __xl6FutureSetAggregateError("task aggregate preparation failed");
    }
    return output;
}
static bool __xl6FutureAggregateHold(const void* data)
{
    xl6_future_all_xvalues* value = (xl6_future_all_xvalues*)data; xrtownershipscope mutation = {0}; bool held;
    if (!xrtOwnershipMutationBegin(&mutation)) return false;
    held = !value->Cleared && xrtRefRetain(&value->References) > 0;
    if (!xrtOwnershipScopeEnd(&mutation)) abort();
    return held;
}
static bool __xl6FutureAggregateClaim(const void* data, const void* token)
{
    xl6_future_all_xvalues* value = (xl6_future_all_xvalues*)data;
    if (!token || value->Cleared || (value->Claim && value->Claim != token)) return false;
    value->Claim = token; return true;
}
static void __xl6FutureAggregateRestore(const void* data, const void* token)
{
    xl6_future_all_xvalues* value = (xl6_future_all_xvalues*)data;
    if (!token || value->Claim != token || value->Cleared) abort();
    value->Claim = NULL;
}
static void __xl6FutureAggregateClear(const void* data, const void* token)
{
    xl6_future_all_xvalues* value = (xl6_future_all_xvalues*)data;
    if (!token || value->Claim != token || value->Cleared) abort();
    value->Cleared = true;
}
static bool __xl6FutureAggregateFinish(const void* data, const void* token)
{
    xl6_future_all_xvalues* value = (xl6_future_all_xvalues*)data; XL6_CodeLease* lease;
    xrtownershipscope mutation = {0};
    if (!xrtOwnershipMutationBegin(&mutation)) abort();
    if (!token || value->Claim != token || !value->Cleared) abort();
    lease = value->Lease; value->Lease = NULL; value->Box = NULL;
    if (!xrtOwnershipScopeEnd(&mutation)) abort();
    xl6CodeLeaseRelease(lease); return true;
}
const xrtownershipadapterv1* xl6FutureAggregateOwnershipAdapterV1(xrtownershipref ref)
{
    static const xrtownershipadapterv1 adapter = {sizeof(adapter), __xl6FutureAggregateHold,
        __xl6FutureAggregateDrop, __xl6FutureAggregateClaim, __xl6FutureAggregateRestore,
        NULL, __xl6FutureAggregateClear, __xl6FutureAggregateFinish};
    const xl6_future_all_xvalues* data; size_t count;
    if (!ref.Data || ref.Ops != &__xl6FutureAggregateOps) return NULL;
    data = ref.Data;
    return data->Certified && __xl6FutureAggregateCount(data, &count) ? &adapter : NULL;
}
xfuture* xl6FutureAggregateAllV2(xfuture* const* futures, size_t count, XL6_FutureAggregateBoxV1 box, const XL6_CodeOwner* owner)
{ return __xl6FutureAggregateStart(XL6_AGGREGATE_ALL, futures, count, box, owner, true); }
xfuture* xl6FutureAggregateRaceV2(xfuture* const* futures, size_t count)
{ return __xl6FutureAggregateStart(XL6_AGGREGATE_RACE, futures, count, NULL, NULL, true); }
xfuture* xl6FutureAggregateWhenAnyIndexV2(xfuture* const* futures, size_t count)
{ return __xl6FutureAggregateStart(XL6_AGGREGATE_ANY, futures, count, NULL, NULL, true); }

