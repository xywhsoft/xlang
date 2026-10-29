/* Generated C and target binding are preincluded in this TU only. */
#include <assert.h>
#include <stdio.h>
extern xwaitresult xrtFutureWaitFor(xfuture*, uint64_t);
extern uint64_t xrtDeadlineAfter(uint64_t);
extern bool xrtDeadlineExpired(uint64_t);
extern void xrtThreadYield(void);
extern bool xrtPromiseResolve(xpromise*, void*);
extern void dispatch_memory_begin(void);
extern void dispatch_memory_end(void);
extern void dispatch_arm_watch(int, int);
extern void dispatch_wait_watches(int);
extern void dispatch_enable_blocking(void);
extern void dispatch_wait_blocked(void);
extern int dispatch_cleanup_count(void);
bool dispatch_should_run(void) { return false; }
xfuture* dispatch_ready_input(void) { assert(0); return NULL; }
static void resolved(xfuture* output, int64_t expected)
{
    xfutureresult result = {0};
    assert(xrtFutureWaitFor(output, 5000000) == XWAIT_OK && xrtFutureResult(output, &result));
    assert(result.State == XFUTURE_RESOLVED && result.Value && *(int64_t*)result.Value == expected);
}
static void shutdown(void)
{
    uint64_t deadline = xrtDeadlineAfter(5000000);
    xrtClearError();
    while (!xlir_module_fini_body().ok) {
        assert(xrtGetError() == NULL && !xrtDeadlineExpired(deadline)); xrtThreadYield();
    }
    assert(xlir_continuation_executor == NULL && xlir_module_initialized == 0);
}
int main(int argc, char** argv)
{
    unsigned round, mode, refused = 0;
    unsigned selected = argc == 2 ? (unsigned)strtoul(argv[1], NULL, 10) : 7;
    for (round = 0; round < 25; ++round) for (mode = 0; mode < 7; ++mode) {
        if (selected != 7 && mode != selected) continue;
        xfuture *a, *b, *c, *d; xpromise *pa, *pb, *pc, *pd;
        xlir_result_future one, two = {0}, three = {0};
        int64_t va = 21, vb = 21, vc = 2, vd = 0;
        dispatch_memory_begin(); assert(xlir_module_init());
        pa = xrtPromiseCreate(&a, NULL); pb = xrtPromiseCreate(&b, NULL);
        pc = xrtPromiseCreate(&c, NULL); pd = xrtPromiseCreate(&d, NULL);
        assert(pa && pb && pc && pd);
        if (mode == 0) { assert(xrtPromiseResolve(pa, &va)); assert(xrtPromiseResolve(pb, &vb)); }
        if (mode == 2 || mode == 3) dispatch_arm_watch(1, mode == 3);
        if (mode == 4) dispatch_arm_watch(2, 0);
        if (mode == 6) {
            va = 1; vb = 0; dispatch_enable_blocking();
            assert(xrtPromiseResolve(pa, &va) && xrtPromiseResolve(pb, &vb));
            assert(xrtPromiseResolve(pc, &vc) && xrtPromiseResolve(pd, &vd));
        }
        one = XLIR_DISPATCH_CHAIN(a, b);
        if (mode == 2 || mode == 3) {
            xfutureresult result = {0};
            /* The language body failed after launch: its outcome belongs to the
             * returned Future, independently of whether a diagnostic exists. */
            assert(one.ok && one.value != NULL && xrtGetError() == NULL);
            assert(xrtFutureWaitFor(one.value, 5000000) == 0 && xrtFutureResult(one.value, &result));
            assert(result.State == XFUTURE_FAILED && result.Value == NULL);
            if (mode == 3) assert(result.Error && xrtErrorCode(result.Error) == 62);
            else assert(result.Error && xrtErrorKind(result.Error) == XERR_INTERNAL);
            xrtClearError();
        } else {
            assert(one.ok && one.value);
            if (mode == 5) {
                two = XLIR_DISPATCH_CHAIN(c, d); assert(two.ok && two.value);
                /* A full ready-entry domain refuses launch before the body
                 * starts. Body/Watch failures in modes 2/3/4 still belong to
                 * an accepted Future and must execute their finally block. */
                three = XLIR_DISPATCH_CHAIN(a, b);
                assert(!three.ok && three.value == NULL && xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_RANGE);
                ++refused;
                xrtClearError();
            }
            if (mode == 6) {
                dispatch_wait_blocked(); two = XLIR_DISPATCH_CHAIN(c, d); assert(two.ok && two.value);
            }
            /* Accepted successor frames must survive Close without a new root. */
            if (mode == 1 || mode == 5) xl6ContinuationExecutorClose(xlir_continuation_executor);
            if (mode != 0 && mode != 6) {
                assert(xrtPromiseResolve(pa, &va));
                if (mode == 1 || mode == 4) dispatch_wait_watches(2);
                assert(xrtPromiseResolve(pb, &vb));
            }
            if (mode == 4) {
                xfutureresult result = {0};
                assert(xrtFutureWaitFor(one.value, 5000000) == 0 && xrtFutureResult(one.value, &result));
                assert(result.State == XFUTURE_FAILED && result.Error && xrtErrorKind(result.Error) == XERR_INTERNAL);
            } else resolved(one.value, mode == 6 ? 1 : 42);
            if (mode == 5) { assert(xrtPromiseResolve(pc, &vc) && xrtPromiseResolve(pd, &vd)); }
            if (two.value) resolved(two.value, 2);
        }
        xrtFutureDestroy(one.value); xrtFutureDestroy(two.value); xrtFutureDestroy(three.value);
        xrtFutureDestroy(a); xrtFutureDestroy(b); xrtFutureDestroy(c); xrtFutureDestroy(d);
        xrtPromiseDestroy(pa); xrtPromiseDestroy(pb); xrtPromiseDestroy(pc); xrtPromiseDestroy(pd);
        shutdown();
        assert(dispatch_cleanup_count() == (mode == 5 || mode == 6 ? 2 : 1));
        dispatch_memory_end();
    }
    assert(refused == (selected == 5 || selected == 7 ? 25u : 0u));
    printf("generated dispatch: mode=%u cases=%u balanced memory\n", selected, selected == 7 ? 175 : 25);
    return 0;
}
