/* Preinclude generated C and the canonical async target binding. No sockets,
 * delays or network worker scheduling are needed to expose producer execution. */
#include "release/include/xl_clock.h"
#include <assert.h>
#include <stdio.h>
typedef struct xthread xthread;
extern xthread* xrtThreadCreate(int32_t (*)(void*), void*, size_t);
extern void xrtThreadDestroy(xthread*);
extern uint64_t xrtThreadCurrentId(void);
extern bool xrtFutureDone(const xfuture*);
extern bool xrtPromiseResolve(xpromise*, void*);
extern uint64_t xl6DeadlineAfterMillisecondsV1(uint64_t);
extern bool xl6DeadlineExpiredV1(uint64_t);
extern void xrtThreadYield(void);

static int64_t payload = 41;
static uint64_t resume_thread, finally_thread;
static unsigned probe_count, finally_count;

int64_t resume_isolation_probe(int64_t value)
{ resume_thread = xrtThreadCurrentId(); ++probe_count; return value + 1; }
void resume_isolation_finally(void)
{ finally_thread = xrtThreadCurrentId(); ++finally_count; }
xfuture* resume_isolation_ready(void)
{
    xfuture* future = NULL;
    xpromise* promise = xrtPromiseCreate(&future, NULL);
    assert(promise && future && xrtPromiseResolve(promise, &payload));
    xrtPromiseDestroy(promise);
    return future;
}
typedef struct production {
    xpromise* promise;
    xerror* error;
    unsigned outcome;
    uint64_t thread;
    int diagnostic_preserved;
} production;
static int32_t produce(void* data)
{
    production* source = (production*)data;
    xerror* ambient = xrtErrorCreate(XERR_STATE, "producer", 91, "producer diagnostic");
    bool ok;
    assert(ambient);
    source->thread = xrtThreadCurrentId();
    xrtSetErrorTake(ambient);
    ok = source->outcome == 0 ? xrtPromiseResolve(source->promise, &payload) :
         source->outcome == 1 ? xrtPromiseReject(source->promise, source->error) :
         source->outcome == 2 ? xrtPromiseCancel(source->promise) : xrtPromiseClose(source->promise);
    source->diagnostic_preserved = xrtGetError() == ambient;
    assert(ok);
    xrtClearError();
    return 0;
}
int main(void)
{
    unsigned round, ready, outcome, cases = 0, isolation_failures = 0;
    unsigned semantic_failures = 0, diagnostic_failures = 0;
    assert(xlir_module_init());
    for (round = 0; round < 50; ++round)
    for (ready = 0; ready < 2; ++ready)
    for (outcome = 0; outcome < 4; ++outcome) {
        xfuture *input = NULL, *output;
        xpromise* promise = xrtPromiseCreate(&input, NULL);
        xerror* error = xrtErrorCreate(XERR_STATE, "input", 17, "source failed");
        production source = { promise, error, outcome, 0, 0 };
        xfutureresult result = {0};
        xlir_result_future launched;
        assert(promise && input && error);
        if (ready) produce(&source);
        resume_thread = finally_thread = 0;
        probe_count = finally_count = 0;
        launched = XLIR_ISOLATED_RESUME(input);
        assert(launched.ok && launched.value);
        output = launched.value;
        if (!ready) {
            xthread* thread;
            assert(!xrtFutureDone(output));
            thread = xrtThreadCreate(produce, &source, 0);
            assert(thread && xrtThreadWait(thread) == 0);
            xrtThreadDestroy(thread);
        }
        assert(xrtFutureWaitFor(output, 1000000) == 0);
        assert(xrtFutureResult(output, &result));
        if (result.State != XFUTURE_RESOLVED || !result.Value ||
            *(int64_t*)result.Value != (outcome == 0 ? 42 : 0) ||
            probe_count != 1 || finally_count != 1) ++semantic_failures;
        if (!source.diagnostic_preserved) ++diagnostic_failures;
        if (resume_thread == source.thread || finally_thread == source.thread) ++isolation_failures;
        ++cases;
        xrtFutureDestroy(output); xrtFutureDestroy(input);
        xrtPromiseDestroy(promise); xrtErrorFree(error); xrtClearError();
    }
    {
        uint64_t deadline = xl6DeadlineAfterMillisecondsV1(5000);
        while (!xlir_module_fini_body().ok) {
            assert(xrtGetError() == NULL && !xl6DeadlineExpiredV1(deadline)); xrtThreadYield();
        }
        assert(xlir_module_initialized == 0 && xlir_continuation_executor == NULL);
    }
    printf("resume-isolation cases=%u pending=%u ready=%u producer-execution-failures=%u semantic-failures=%u diagnostic-failures=%u\n",
        cases, cases / 2, cases / 2, isolation_failures, semantic_failures, diagnostic_failures);
    return isolation_failures || semantic_failures || diagnostic_failures ? 1 : 0;
}
