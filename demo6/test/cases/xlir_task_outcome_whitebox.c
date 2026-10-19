/* White-box transport coverage migrated intact from the former generated
 * adapter: 24 ready outcomes and one registration failure. This does not stand
 * in for admitted-job, code-owner or graph coverage in xlir_task_storage_runtime. */
#include "src/runtime/xl6_task.c"
#include <assert.h>
static TaskFrame* probe_watch(void)
{
    TaskFrame* frame=xrtCalloc(1,sizeof(*frame)); assert(frame);
    frame->references=1; /* one real direct notification/release caller */
    XL6_TaskPool* pool=xl6TaskPoolCreateInternalV1(NULL);assert(pool&&xl6TaskPoolHoldInternalV1(pool));
    frame->pool=pool; /* actual frame owner, including after physical release */
    assert(xl6TaskPoolDestroyInternalV1(pool)); /* consume Create's owner of this idle shell */
    return frame;
}
static unsigned watch_drops;
static int watch_drop_raises;
static void pool_watch_value_free(void* value, void* data)
{
    (void)data; ++watch_drops; xrtFree(value);
    if (watch_drop_raises) xrtSetErrorKind(XERR_INTERNAL);
}
typedef struct CleanupEdge {xrtownershipref error;unsigned count;} CleanupEdge;
static bool cleanup_edge(xrtownershipref edge,void* raw)
{
    CleanupEdge* expected=raw;
    if(edge.Data==expected->error.Data){assert(edge.Ops==expected->error.Ops);++expected->count;}
    return true;
}
static void cleanup_error_owner_probe(void)
{
    TaskFrame* frame=probe_watch();xfuture *source=NULL,*output=NULL;
    xpromise* producer=xrtPromiseCreate(&source,NULL);assert(producer&&xrtPromiseResolve(producer,NULL));
    xrtPromiseDestroy(producer);frame->source=source;frame->output=xrtPromiseCreate(&output,NULL);assert(frame->output);
    frame->committed=true;frame->cleanupFailed=true;
    frame->cleanupError=xrtErrorCreate(XERR_STATE,"task.checked",902,"checked cleanup owner");assert(frame->cleanupError);
    CleanupEdge expected={xrtErrorOwnership(frame->cleanupError),0};assert(hold_frame(frame));
    xrtownershipscope frozen={0};while(!xrtOwnershipFreezeTryBegin(&frozen))xrtThreadYield();
    size_t count=0;assert(frame_count(frame,&count)&&count==2&&frame_trace(frame,cleanup_edge,&expected)&&expected.count==1);
    assert(xrtOwnershipScopeEnd(&frozen));
    outcome_notify(frame);assert(!frame->cleanupError&&xrtFutureState(output)==XFUTURE_FAILED);
    assert(xrtErrorCode(xrtFutureError(output))==902);outcome_release(frame);
    /* Only the genuine inspection pin remains; a retired frame cannot keep
     * an untraced error owner or require that pin's own release to prepare. */
    assert(frame->references==1&&prepared_frame(frame)&&!frame->cleanupError);
    xrtFutureDestroy(output);producer_drop(frame);xrtClearError();
}
unsigned pool_outcome_ready_watch_probe(void)
{
    unsigned mode, early, raises, count = 0;
    cleanup_error_owner_probe();
    {
        TaskFrame* watch = probe_watch();
        xfuture *source = NULL, *output = NULL;
        xpromise* destination = xrtPromiseCreate(&output, NULL);
        xcancel* parent = xrtPromiseCancelToken(destination);
        xpromise* producer = xrtPromiseCreate(&source, parent);
        xcancel* child = xrtPromiseCancelToken(producer);
        assert(watch && destination && parent && producer && child);
        watch->source = source; watch->output = destination;
        xrtSetErrorKind(XERR_STATE);
        outcome_registration_failed(watch, output);
        assert(xrtCancelRequested(child) && xrtFutureState(output) == XFUTURE_FAILED);
        assert(xrtFutureError(output) && xrtErrorKind(xrtFutureError(output)) == XERR_STATE);
        assert(!xrtGetError());
        xrtCancelDestroy(child); xrtCancelDestroy(parent);
        xrtPromiseDestroy(producer); xrtFutureDestroy(output); ++count;
    }
    for (mode = 1; mode <= 6; ++mode)
    for (early = 0; early < 2; ++early)
    for (raises = 0; raises < 2; ++raises) {
        TaskFrame* watch = probe_watch();
        xfuture *source = NULL, *output = NULL;
        xpromise* producer = xrtPromiseCreate(&source, NULL);
        xpromise* destination = xrtPromiseCreate(&output, NULL);
        const xerror* previous;
        int64_t* value = NULL;
        assert(watch && producer && destination);
        watch_drops = 0; watch_drop_raises = (int)raises;
        if (mode == 1) {
            value = xrtMalloc(sizeof(*value)); assert(value); *value = 42;
            assert(xrtPromiseResolveOwned(producer, value, pool_watch_value_free, NULL));
        } else if (mode == 3) assert(xrtPromiseCancel(producer));
        else if (mode == 4) assert(xrtPromiseClose(producer));
        else {
            xrtSetErrorKind(mode == 2 ? XERR_STATE : mode == 5 ? XERR_CANCELLED : XERR_CLOSED);
            assert(xrtPromiseReject(producer, xrtGetError())); xrtClearError();
        }
        xrtPromiseDestroy(producer);
        watch->source = source; watch->output = destination;
        assert(xrtFutureWatchInit(&watch->watch, outcome_notify, outcome_release, watch));
        assert(xrtFutureWatchAdd(source, &watch->watch) == XFUTURE_WATCH_READY);
        if (early) xrtFutureDestroy(output);
        xrtSetErrorKind(XERR_STATE); previous = xrtGetError(); assert(previous);
        outcome_notify(watch);
        assert(xrtGetError() == previous);
        outcome_release(watch);
        assert(xrtGetError() == previous);
        xrtClearError();
        if (!early) {
            xfuturestate expected = mode == 1 ? XFUTURE_RESOLVED : mode == 2 ? XFUTURE_FAILED
                : mode == 3 || mode == 5 ? XFUTURE_CANCELLED : XFUTURE_CLOSED;
            assert(xrtFutureState(output) == expected);
            if (mode == 1) assert(xrtFutureValue(output) == value && !watch_drops);
            xrtFutureDestroy(output);
            /* A user-initiated final release is outside the callback's isolated
             * context; this probe only requires callback Notify/Release isolation. */
            xrtClearError();
        }
        assert(watch_drops == (mode == 1 ? 1u : 0u)); ++count;
    }
    return count;
}
