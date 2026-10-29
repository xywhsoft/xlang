#include <assert.h>
#include <stdio.h>
#include "src/runtime/xl6_continuation_executor.h"
static XL6_ContinuationAdmission source_lease_reserve(XL6_ContinuationExecutor*, XL6_ContinuationTicket*,
    XL6_ContinuationProc, XL6_ContinuationProc, void*, const XL6_CodeOwner*);
#define xl6ContinuationExecutorReserveOwned source_lease_reserve
#include "../../build/test/xlir_async_source_lease.c"
#undef xl6ContinuationExecutorReserveOwned
#include "../../build/test/xlir_async_source_lease.h.bind.h"
#define XLIR_TEST_LAUNCH_FUTURE
#include "xlir_launch_test.h"
extern int source_lease_ambient(void), source_lease_body_mode(void), source_lease_finally_mode(void), source_lease_drop_mode(void);
extern void source_lease_payload_destroy(void*);
extern void source_lease_report_context(void);
extern void source_lease_fault_arm(void), source_lease_fault_disarm(void);
extern int source_lease_fault_active(void);
extern void source_lease_worker_begin(void), source_lease_worker_end(void);
extern void xrtThreadYield(void);
static XL6_ContinuationProc faultRun;
static void source_lease_fault_run(void* data)
{
    source_lease_worker_begin(); faultRun(data); source_lease_worker_end();
}
static XL6_ContinuationAdmission source_lease_reserve(XL6_ContinuationExecutor* executor, XL6_ContinuationTicket* ticket,
    XL6_ContinuationProc run, XL6_ContinuationProc release, void* data, const XL6_CodeOwner* owner)
{
    /* The fault harness serializes launches. Instrument the real executor
     * callback, not generated C, so TLS fault injection runs on its worker. */
    if (source_lease_fault_active()) { faultRun = run; run = source_lease_fault_run; }
    return xl6ContinuationExecutorReserveOwned(executor, ticket, run, release, data, owner);
}
void source_lease_drain(void)
{
    while (!xl6ContinuationExecutorIsIdle(xlir_continuation_executor)) xrtThreadYield();
}
static const int failureCodes[] = {31, 73, 77, 93};
static xerror* failures[4];
static void raise_failure(int mode, int code)
{
    if (mode) {
        unsigned i;
        xl6_error_result failure = {false, NULL};
        for (i = 0; i < 4 && failureCodes[i] != code; ++i) {}
        assert(i < 4);
        /* Fault injection targets generated language allocations, not the
         * test's attempt to manufacture a destructor diagnostic. */
        if (mode == 1) failure.error = xrtErrorRef(failures[i]);
        if (mode == 1) assert(failure.error);
        xl6ErrorScopePropagateFailure(failure);
    }
}
void source_lease_body(void) { raise_failure(source_lease_body_mode(), 73); }
void source_lease_finally(void) { raise_failure(source_lease_finally_mode(), 77); }
void source_lease_payload_free(void* value, void* data)
{
    (void)data; source_lease_payload_destroy(value); raise_failure(source_lease_drop_mode(), 93);
}
static void ambient_begin(xl6_error_scope* scope)
{
    assert(xl6ErrorScopeBegin(scope)); raise_failure(source_lease_ambient(), 31);
}
static void ambient_end(xl6_error_scope* scope)
{
    xl6_error_result outcome = xl6ErrorScopeEnd(scope); int mode = source_lease_ambient();
    assert(outcome.ok == (mode == 0) && !xrtGetError());
    if (mode == 1) assert(outcome.error && xrtErrorCode(outcome.error) == 31);
    else assert(!outcome.error);
    xrtErrorFree(outcome.error);
}
void source_lease_prepare(void) {
    unsigned i;
    for (i = 0; i < 4; ++i) { failures[i] = xrtErrorCreate(XERR_STATE, "source lease", failureCodes[i], "controlled failure"); assert(failures[i]); }
    assert(xlir_module_init() && !xrtGetError());
}
void source_lease_finish(void) {
    unsigned i;
    xlir_module_fini(); assert(!xrtGetError());
    for (i = 0; i < 4; ++i) { xrtErrorFree(failures[i]); failures[i] = NULL; }
}
static xfuture* source_lease_invoke(int kind, xfuture* input)
{
    switch (kind) {
    case 0: return xlir_test_observe_future_launch(SOURCE_LEASE_0(input));
    case 1: return xlir_test_observe_future_launch(SOURCE_LEASE_1());
    case 2: return xlir_test_observe_future_launch(SOURCE_LEASE_2(input));
    case 3: return xlir_test_observe_future_launch(SOURCE_LEASE_3(input));
    case 4: return xlir_test_observe_future_launch(SOURCE_LEASE_4(input));
    case 5: return xlir_test_observe_future_launch(SOURCE_LEASE_5());
    case 6: return xlir_test_observe_future_launch(SOURCE_LEASE_6());
    case 7: return xlir_test_observe_future_launch(SOURCE_LEASE_7());
    default: abort();
    }
}
xfuture* source_lease_launch(int kind, xfuture* input)
{
    xl6_error_scope ambient = {0}, scope = {0}; xl6_error_result outcome; xfuture* result;
    ambient_begin(&ambient); assert(xl6ErrorScopeBegin(&scope)); result = source_lease_invoke(kind, input);
    outcome = xl6ErrorScopeEnd(&scope);
    if (!outcome.ok || outcome.error) {
        source_lease_report_context();
        fprintf(stderr, "launcher outcome: ok=%d code=%d\n", outcome.ok, outcome.error ? xrtErrorCode(outcome.error) : -1);
    }
    assert(outcome.ok && !outcome.error);
    ambient_end(&ambient); return result;
}
xfuture* source_lease_fault_launch(int kind, xfuture* input)
{
    xl6_error_scope ambient = {0}, scope = {0}; xl6_error_result outcome; xfuture* result;
    ambient_begin(&ambient); assert(xl6ErrorScopeBegin(&scope)); source_lease_fault_arm();
    result = source_lease_invoke(kind, input); outcome = xl6ErrorScopeEnd(&scope);
    if (result) { (void)xrtFutureWait(result); source_lease_drain(); }
    source_lease_fault_disarm();
    if (result) assert(outcome.ok && !outcome.error);
    else assert(!outcome.ok && outcome.error && xrtErrorKind(outcome.error) == XERR_MEMORY);
    xrtErrorFree(outcome.error); ambient_end(&ambient); return result;
}
void source_lease_produce(xpromise* promise, unsigned state, void* value, xerror* error)
{
    xl6_error_scope scope = {0}; bool completed;
    ambient_begin(&scope);
    completed = state == 0 ? xrtPromiseResolveOwned(promise, value, source_lease_payload_free, NULL) :
        state == 1 ? xrtPromiseReject(promise, error) : state == 2 ? xrtPromiseCancel(promise) : xrtPromiseClose(promise);
    assert(completed); ambient_end(&scope);
}
void source_lease_release_external(xfuture* future, xpromise* promise, int expected)
{
    xl6_error_scope scope = {0}; xl6_error_result outcome;
    assert(xl6ErrorScopeBegin(&scope)); xrtFutureDestroy(future); xrtPromiseDestroy(promise);
    outcome = xl6ErrorScopeEnd(&scope); assert(outcome.ok == (expected == 0) && !xrtGetError());
    if (expected == 1) assert(outcome.error && xrtErrorCode(outcome.error) == 93);
    else assert(!outcome.error);
    xrtErrorFree(outcome.error);
}

void source_lease_entry_probe(char* first, char* second, xvalue* owned, char* last, int failed)
{
    xl6_error_scope ambient = {0}, scope = {0}; xl6_error_result outer; xlir_result_i64 result; xerror* error;
    ambient_begin(&ambient); assert(xl6ErrorScopeBegin(&scope)); source_lease_fault_arm();
    result = SOURCE_LEASE_ENTRY((xlir_string){first, strlen(first)},
        (xlir_string){second, second ? strlen(second) : 0}, owned,
        (xlir_string){last, strlen(last)});
    error = xrtTakeError();
    source_lease_fault_disarm(); outer = xl6ErrorScopeEnd(&scope);
    assert(outer.ok && !outer.error && result.ok == !failed);
    if (failed) assert(error && xrtErrorKind(error) == XERR_MEMORY);
    else assert(!error && result.value == 42);
    xrtErrorFree(error); ambient_end(&ambient);
}
