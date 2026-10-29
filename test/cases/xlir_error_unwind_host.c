#include <assert.h>
#include <stdio.h>
#include "../../build/test/xlir_error_unwind_bind.h"
#include "../../build/test/xlir_error_unwind.c"
void unwind_oom(void);
static int mode, cleanup_mode, armed, opened, closed, sequence, current_lane;
static int first_cleanup_pending;
bool unwind_is_normal(void) { return current_lane >= 14 && current_lane != 22; }

static void publish(int kind, int code)
{
    xl6_error_result failure = {false, NULL};
    if (kind == 1 || kind == 4)
        failure.error = xrtErrorCreate(15, "unwind", code, "unwind test failure");
    else if (kind == 3) unwind_oom();
    xl6ErrorScopePropagateFailure(failure);
    if (kind == 4) xrtErrorFree(xrtTakeError());
}

void* unwind_open(int64_t id)
{
    int* resource = (int*)xrtMalloc(sizeof(int));
    assert(resource != NULL); *resource = (int)id; ++opened;
    return resource;
}

void unwind_close(void* pointer)
{
    int id = *(int*)pointer;
    xrtFree(pointer); ++closed;
    sequence = sequence * 10 + id;
    if (armed && cleanup_mode == 5 && id == 2) {
        int saved_mode = mode, saved_sequence = sequence, saved_first_cleanup = first_cleanup_pending;
        int64_t nested_result;
        const xerror* prior_diagnostic = xrtGetError();
        mode = 2; cleanup_mode = 0; armed = 0; sequence = 0;
        nested_result = unwind_caught();
        if (nested_result != 0 || sequence != 21 || xrtGetError() != prior_diagnostic)
            printf("unwind reentry mismatch: lane=%d result=%lld order=%d error=%d\n", current_lane,
                (long long)nested_result, sequence, xrtGetError() != NULL ? xrtErrorCode(xrtGetError()) : 0);
        assert(nested_result == 0 && sequence == 21 && xrtGetError() == prior_diagnostic);
        mode = saved_mode; cleanup_mode = 5; armed = 1; sequence = saved_sequence;
        first_cleanup_pending = saved_first_cleanup;
    }
    if (armed && first_cleanup_pending) {
        first_cleanup_pending = 0;
        publish(mode, 202);
    }
    if (armed && cleanup_mode != 0)
        publish(cleanup_mode == 5 ? 1 : cleanup_mode, id == 2 ? 202 : 303);
}

void unwind_fail(void)
{
    armed = 1;
    publish(mode, 101);
}

void unwind_set(void* pointer, int64_t value)
{
    assert(*(int*)pointer == 2 && value == 7);
    unwind_fail();
}

int64_t unwind_get_at(void* pointer, int64_t index)
{
    assert(pointer != NULL && index == 3);
    return 7;
}

void unwind_set_at(void* pointer, int64_t index, int64_t value)
{
    assert(index == 3);
    unwind_set(pointer, value);
}

static int64_t run_async(void)
{
    xlir_result_future launch = UNWIND_ASYNC_START();
    xfuture* future = launch.value;
    int64_t value;
    assert(launch.ok && future != NULL && xrtFutureWait(future) == XWAIT_OK);
    assert(xrtFutureValue(future) != NULL);
    value = *(const int64_t*)xrtFutureValue(future);
    xrtFutureDestroy(future);
    return value;
}

int unwind_run_case(int original, int cleanup, int lane)
{
    xl6_foreign_error_scope outer = {0};
    xl6_foreign_error_result result;
    int64_t value;
    int normal = lane >= 14 && lane != 22;
    int expected = original == 1 ? (normal ? 202 : 101) : 0;
    int prior_opened = opened, prior_closed = closed;
    int one_owner = lane == 6 || lane == 12 || lane == 13 || lane == 14 || lane == 15 || (lane >= 20 && lane <= 22);
    int acquisitions = (lane == 18 || lane == 23 ? 3 : one_owner ? 1 : 2) + (cleanup == 5 ? 2 : 0);
    int escaping = normal ? lane == 15 || lane == 24 : lane == 1 || (lane >= 4 && lane != 11 && lane != 13 && lane != 22);
    mode = original; cleanup_mode = cleanup; armed = normal; sequence = 0;
    first_cleanup_pending = normal;
    current_lane = lane;
    assert(xl6ForeignCallBegin(&outer));
    if (lane == 5 || lane == 24) {
        xvalue* pending = lane == 5 ? xl6_export_unwind_pending() : xl6_export_unwind_pending_cleanup();
        assert(pending == NULL); value = 0;
    } else value = lane == 0 ? unwind_caught() : lane == 1 ? unwind_escape() :
        lane == 2 ? run_async() : lane == 3 ? unwind_generator() :
        lane == 4 ? unwind_parameter() : lane == 6 ? unwind_property() :
        lane == 7 ? unwind_closure() : lane == 8 ? unwind_operator() :
        lane == 9 ? unwind_constructor() : lane == 10 ? unwind_closure_callee() :
        lane == 11 ? unwind_operator_caught() : lane == 12 ? unwind_setter() : lane == 13 ? unwind_indexed_setter() :
        lane == 14 ? normal_getter() : lane == 15 ? normal_escape() : lane == 16 ? normal_pair() :
        lane == 17 ? normal_closure() : lane == 18 ? normal_owned_result() : lane == 19 ? normal_constructor() :
        lane == 20 ? run_async() : lane == 21 ? unwind_generator() : lane == 22 ? unwind_compound() : normal_compound();
    result = xl6ForeignCallEnd(&outer);
    if ((escaping ? result.ok || value != 0 ||
            (result.error != NULL ? xrtErrorCode(result.error) : 0) != expected :
            !result.ok || result.error != NULL || value != expected) ||
        opened != prior_opened + acquisitions || closed != prior_closed + acquisitions ||
        sequence != (lane == 5 || lane == 24 ? 12 : lane == 18 ? 213 : lane == 23 ? 231 : one_owner ? 2 : 21)) {
        printf("unwind mismatch: lane=%d original=%d cleanup=%d value=%lld ok=%d error=%d order=%d opened=%d closed=%d\n",
            lane, original, cleanup, (long long)value, result.ok,
            result.error != NULL ? xrtErrorCode(result.error) : 0, sequence, opened, closed);
        xrtErrorFree(result.error); return 0;
    }
    xrtErrorFree(result.error);
    assert(xrtGetError() == NULL);
    return 1;
}

void unwind_pending_success(void)
{
    xl6_foreign_error_scope outer = {0};
    xl6_foreign_error_result result;
    xvalue* pending;
    int prior_opened = opened, prior_closed = closed;
    mode = cleanup_mode = armed = first_cleanup_pending = 0; sequence = 0;
    assert(xl6ForeignCallBegin(&outer));
    pending = xl6_export_unwind_pending_cleanup();
    result = xl6ForeignCallEnd(&outer);
    assert(result.ok && result.error == NULL && pending != NULL);
    assert(opened == prior_opened + 2 && closed == prior_closed + 1 && sequence == 1);
    xrtValueRelease(pending);
    assert(closed == prior_closed + 2 && sequence == 12 && xrtGetError() == NULL);
}

void unwind_finish(void) { armed = 0; xlir_module_fini(); assert(xrtGetError() == NULL); }
