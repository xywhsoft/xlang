#include <limits.h>
#include "include/xruntime_decl.h"
#include "src/runtime/xl6_future_aggregate.h"
#include "src/runtime/xl6_array_input.h"
#include "src/runtime/xl6_future_aggregate_internal.h"

/* Synchronous owned unpacking only. Start pins each native Future before
 * releasing the temporary input; no Array/slot borrow survives the call. */
static xfuture* __xl6FutureAggregateArray(unsigned mode, const xvalue* array, XL6_FutureAggregateBoxV1 box, const XL6_CodeOwner* owner)
{
    xfuture** futures; xfuture* output; size_t count;
    XL6_ArrayInputV1 input={0};
    if (!xl6ArrayInputAcquireV1(array,&input)) return NULL;
    if (!(count = input.count) || count > SIZE_MAX / sizeof(*futures) ||
        count > (size_t)INT64_MAX || (mode != XL6_AGGREGATE_ANY && count > (size_t)INT_MAX)) {
        __xl6FutureSetAggregateError(mode == XL6_AGGREGATE_ALL
            ? "task.all requires a non-empty future array"
            : mode == XL6_AGGREGATE_RACE
                ? "task.race requires a non-empty future array"
                : "task.whenAny requires a non-empty future array");
        xl6ArrayInputReleaseV1(&input); return NULL;
    }
    futures = xrtMalloc(count * sizeof(*futures));
    if (!futures) { __xl6FutureSetAggregateError("task aggregate Future array allocation failed"); xl6ArrayInputReleaseV1(&input); return NULL; }
    for (size_t i = 0; i < count; ++i) {
        const xvalue* item = input.items[i];
        futures[i] = item != NULL && xrtValueType(item) != XVALUE_NULL
            ? xrtValueGetFuture(item) : NULL;
        if (!futures[i]) {
            xrtFree(futures);
            __xl6FutureSetAggregateError(
                __xl6FutureAggregateNullSourceError(mode));
            xl6ArrayInputReleaseV1(&input); return NULL;
        }
    }
    output = mode == XL6_AGGREGATE_ALL ? xl6FutureAggregateAllV2(futures,count,box,owner) :
        mode == XL6_AGGREGATE_RACE ? xl6FutureAggregateRaceV2(futures,count) :
        xl6FutureAggregateWhenAnyIndexV2(futures,count);
    xrtFree(futures); xl6ArrayInputReleaseV1(&input); return output;
}
xfuture* xl6FutureAggregateAllArrayV2(const xvalue* array, XL6_FutureAggregateBoxV1 box, const XL6_CodeOwner* owner)
{ return __xl6FutureAggregateArray(XL6_AGGREGATE_ALL, array, box, owner); }
xfuture* xl6FutureAggregateRaceArrayV2(const xvalue* array)
{ return __xl6FutureAggregateArray(XL6_AGGREGATE_RACE, array, NULL, NULL); }
xfuture* xl6FutureAggregateWhenAnyIndexArrayV2(const xvalue* array)
{ return __xl6FutureAggregateArray(XL6_AGGREGATE_ANY, array, NULL, NULL); }

