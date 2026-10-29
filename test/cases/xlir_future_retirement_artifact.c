#include "release/include/xl_clock.h"
#include "xlir_future_retirement.generated.h"
#include "xlir_future_retirement.generated.c"
typedef struct FutureEscapeHooks { void (*capture)(xvalue*); void (*dropped)(int64_t); } FutureEscapeHooks;
static FutureEscapeHooks future_escape_hooks;
__declspec(dllexport) void future_escape_bind(const FutureEscapeHooks* hooks) { future_escape_hooks = *hooks; }
__declspec(dllexport) bool future_escape_join(void)
{
    /* The fixture has finished submitting roots. Unlike the removed blocking
     * pool, checked executor teardown refuses still-running logical work.
     * Drain accepted work explicitly, including physical release tails, before
     * asking the generated module to consume its executor owner. */
    XL6_DeadlineV1 limit = xl6DeadlineAfterMillisecondsV1(5000);
    while (!xl6ContinuationExecutorIsIdle(xlir_continuation_executor)) {
        if (xl6DeadlineExpiredV1(limit)) return false;
        xrtThreadYield();
    }
    return xlir_continuation_quiesce(true);
}
void future_escape_capture(xvalue* value) { future_escape_hooks.capture(value); }
void future_escape_dropped(int64_t id) { future_escape_hooks.dropped(id); }
/* The language deliberately rejects async functions declared as Future<T>.
 * Use the native boundary's explicit nested-result ABI, never relax that
 * semantic contract merely to manufacture a retirement fixture. */
xfuture* future_escape_nest(xvalue* value)
{
    xfuture* inner = xrtValueGetFuture(value);
    xfuture* future = NULL; xpromise* promise = xrtPromiseCreate(&future, NULL);
    xfuture** box = xrtMalloc(sizeof(*box));
    if (promise == NULL || box == NULL) { xrtFree(box); xrtPromiseDestroy(promise); xrtFutureDestroy(future); return NULL; }
    *box = xrtFutureRef(inner);
    if (*box == NULL || !xl6PayloadResolveOwned(promise, box, XL6_FUTURE_PAYLOAD_FUTURE)) {
        xrtFutureDestroy(*box); xrtFree(box); xrtPromiseDestroy(promise); xrtFutureDestroy(future); return NULL;
    }
    xrtPromiseDestroy(promise); return future;
}
