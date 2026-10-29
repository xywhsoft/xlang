#ifndef XLIR_PROPERTY_AWAIT_BARRIER_H
#define XLIR_PROPERTY_AWAIT_BARRIER_H
/* Async roots run on the ready queue. Observe the real pending source Watch
 * before the host replaces receiver/key globals; merely receiving an output
 * Future does not prove that its getter has executed. The host owns both
 * Future references throughout this query. No private frame layout is read. */
static void xlirTestWaitForPropertyAwait(xfuture* input, xfuture* output, bool failingGetter)
{
    assert(input && output && !xrtFutureDone(input));
    if (failingGetter) {
        assert(xrtFutureWaitFor(output, 5000000) == XWAIT_OK);
        return;
    }
    for (unsigned attempt = 0; attempt < 10000; ++attempt) {
        xfuturedebugsnapshot observed;
        assert(xrtFutureDebugSnapshot(input, &observed));
        if (observed.PendingWatches) {
            assert(!xrtFutureDone(input) && !xrtFutureDone(output));
            return;
        }
        xwaitresult status = xrtFutureWaitFor(output, 1000);
        if (status == XWAIT_OK) {
            const xerror* error = xrtFutureError(output);
            fprintf(stderr, "property completed before await registration: %s at %d\n",
                error ? xrtErrorMessage(error) : "success", error ? xrtErrorLine(error) : 0);
        }
        assert(status == XWAIT_TIMEOUT);
    }
    assert(!"property await registration timed out");
}
#endif
