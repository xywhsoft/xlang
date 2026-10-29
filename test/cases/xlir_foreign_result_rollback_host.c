#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include "src/api/xl_foreign_error.h"
#include "../../build/test/xlir_foreign_result_rollback_bind.h"
static bool rollback_test_begin(xl6_foreign_error_scope* scope);
#define xl6ForeignCallBegin rollback_test_begin
#include "../../build/test/xlir_foreign_result_rollback.c"
#undef xl6ForeignCallBegin

/* 0: success, 1: diagnostic failure, 2: failure with no diagnostic.
 * Failure is independent of the nullable native result and diagnostic. */
static int original_mode, cleanup_mode, null_result, reentering;
static int opened, closed, reentries;
static int borrowed_storage;
static int skip_scope;
static int backend_lane;
void rollback_inject_error_oom(void);
void rollback_set_lane(int lane) { backend_lane = lane; }
extern int xrtFutureWait(xfuture*);
extern void* xrtFutureValue(const xfuture*);

static int64_t rollback_async(void)
{
    xfuture* future = ROLLBACK_ASYNC_START();
    int64_t result;
    assert(future != NULL && xrtFutureWait(future) == 0);
    assert(xrtFutureValue(future) != NULL);
    result = *(int64_t*)xrtFutureValue(future);
    xrtFutureDestroy(future);
    return result;
}

static bool rollback_test_begin(xl6_foreign_error_scope* scope)
{
    if (skip_scope > 0 && --skip_scope == 0) {
        xrtSetErrorTake(xrtErrorCreate(15, "factory", 101, "injected scope entry failure"));
        return false;
    }
    return xl6ForeignCallBegin(scope);
}

static void fail_original(void)
{
    if (original_mode == 1)
        xrtSetErrorTake(xrtErrorCreate(15, "factory", 101, "original factory failure"));
    else if (original_mode >= 2) {
        xl6_foreign_error_result failure = {false, NULL};
        if (original_mode == 3) rollback_inject_error_oom();
        xl6ErrorScopePropagateFailure(failure);
    }
}

void* rollback_owned_open(void)
{
    int* resource = null_result ? NULL : (int*)malloc(sizeof(int));
    if (!null_result) { assert(resource != NULL); *resource = 42; ++opened; }
    fail_original();
    return resource;
}

void* rollback_borrowed_open(void)
{
    fail_original();
    return null_result ? NULL : &borrowed_storage;
}

void rollback_owned_close(void* handle)
{
    assert(handle != NULL && handle != &borrowed_storage && *(int*)handle == 42);
    *(int*)handle = 0;
    free(handle);
    ++closed;
    /* Reentry executes a second factory failure and catches it inside the
     * destructor. It must neither repeat this disposal nor alter the caller's
     * pending error, and its own failed-result resource must be closed. */
    if (cleanup_mode == 4 && !reentering) {
        int saved_mode = original_mode;
        reentering = 1; original_mode = 1;
        assert(rollback_caught() == 101);
        ++reentries;
        original_mode = saved_mode; reentering = 0;
    }
    if (cleanup_mode == 1)
        xrtSetErrorTake(xrtErrorCreate(15, "destructor", 202, "secondary cleanup failure"));
    else if (cleanup_mode == 2) {
        xl6_foreign_error_result failure = {false, NULL};
        xl6ErrorScopePropagateFailure(failure);
    } else if (cleanup_mode == 3) {
        xrtSetErrorTake(xrtErrorCreate(15, "destructor", 202, "secondary callback failure"));
        xl6ForeignCallbackFail();
        xrtErrorFree(xrtTakeError()); /* Native diagnostic consumption is not handling. */
    }
}

void rollback_run_case(int original, int cleanup, int empty, int borrowed, int escape)
{
    xl6_foreign_error_scope outer = {0};
    xl6_foreign_error_result outcome;
    int prior_opened = opened, prior_closed = closed, prior_reentries = reentries;
    int64_t result;
    original_mode = original; cleanup_mode = cleanup; null_result = empty;
    /* Public entries now own their own error transaction. Inject at the
     * factory scope, not at that earlier export entry. Async/private calls
     * in this fixture bypass the public entry and need only one Begin. */
    skip_scope = original == 4 ? (escape || backend_lane == 1 ? 1 : 2) : 0;
    assert(xl6ForeignCallBegin(&outer));
    if (escape) {
        /* Observe the private language result carrier directly. The separate
         * --export-probe checks that the public entry also preserves failure
         * when there is no diagnostic, without conflating the two ABIs. */
        xlir_result_i64 escaped = ROLLBACK_ESCAPE();
        xerror* error = xrtTakeError();
        assert(escaped.ok == (original == 0));
        if (original == 1 || original == 4)
            assert(error != NULL && xrtErrorCode(error) == 101);
        else
            assert(error == NULL);
        xrtErrorFree(error);
        result = escaped.value;
    } else {
        result = borrowed ? rollback_borrowed_caught() :
            backend_lane == 1 ? rollback_async() : backend_lane == 2 ? rollback_generator() : rollback_caught();
    }
    outcome = xl6ForeignCallEnd(&outer);
    assert(outcome.ok && outcome.error == NULL);
    if (!escape || original == 0)
        assert(result == (original == 0 ? -1 : (original == 1 || original == 4) ? 101 : 0));
    assert(xrtGetError() == NULL && opened == closed && !skip_scope);
    if (borrowed || empty || original == 4) {
        assert(opened == prior_opened && closed == prior_closed && reentries == prior_reentries);
    } else {
        int count = cleanup == 4 ? 2 : 1;
        assert(opened == prior_opened + count && closed == prior_closed + count);
        assert(reentries == prior_reentries + (cleanup == 4));
    }
}

void rollback_finish(void)
{
    xlir_module_fini();
    assert(opened == closed && xrtGetError() == NULL);
}

int rollback_export_missing_failure_probe(void)
{
    xl6_foreign_error_scope outer = {0};
    xl6_foreign_error_result outcome;
    int64_t result;
    original_mode = 2; cleanup_mode = 0; null_result = 0; skip_scope = 0;
    assert(xl6ForeignCallBegin(&outer));
    result = rollback_escape();
    outcome = xl6ForeignCallEnd(&outer);
    printf("raw scalar export probe: result=%lld outer.ok=%d opened=%d closed=%d\n",
        (long long)result, (int)outcome.ok, opened, closed);
    xrtErrorFree(outcome.error); rollback_finish();
    return outcome.ok ? 1 : 0;
}
