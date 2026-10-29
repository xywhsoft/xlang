#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../../build/test/xlir_class_cleanup_outcome.c"

void class_cleanup_oom(void);
extern void* xrtMalloc(size_t);
extern void xrtFree(void*);
static int mode, reentry, sequence, steps, opened, closed, reentered;
void* class_cleanup_open(void)
{
    int* resource = (int*)xrtMalloc(sizeof(*resource));
    assert(resource != NULL); *resource = 4; ++opened; return resource;
}
static void publish(int id)
{
    xl6_error_result outcome = {false, NULL};
    if (mode == 0 || (mode == 5 && id == 3)) return;
    if ((mode == 2 || mode == 3) && id == 3) {
        if (mode == 3) class_cleanup_oom();
        xl6ErrorScopePropagateFailure(outcome);
    } else if ((mode == 4 && id != 3) || (mode == 5 && id == 2)) {
        xl6ErrorScopePropagateFailure(outcome);
    } else {
        outcome.error = xrtErrorCreate(15, "class-cleanup", 100 + id, "class cleanup failed");
        assert(outcome.error != NULL); xrtSetErrorTake(outcome.error);
    }
}
void class_cleanup_step(int64_t id)
{
    int prior = steps;
    assert(id >= 1 && id <= 3);
    sequence = sequence * 10 + (int)id; ++steps;
    if (reentry && id == 3) {
        xl_script_module_unit();
        assert(xl_script_module_init(NULL) == 0 && steps == prior + 1);
        ++reentered;
    }
    publish((int)id);
}
void class_cleanup_close(void* value)
{
    assert(value != NULL && *(int*)value == 4);
    xrtFree(value); ++closed; sequence = sequence * 10 + 4;
    publish(4);
}
void class_cleanup_case(int pending, int fault, int reenter)
{
    xl6_error_scope outer = {0};
    xl6_error_result result = {true, NULL};
    xerror* original = NULL;
    int scoped = pending != 4;
    mode = reentry = sequence = steps = opened = closed = reentered = 0;
    assert(xrtGetError() == NULL);
    assert(xl_script_module_init(NULL) && opened == 1 && steps == 0);
    if (scoped) assert(xl6ErrorScopeBegin(&outer));
    if (pending == 1 || pending == 3) {
        original = xrtErrorCreate(15, "class-cleanup", 99, "incoming failure");
        assert(original != NULL);
    }
    if (pending == 1) xrtSetErrorTake(original);
    if (pending == 2 || pending == 3) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false, original});
        xrtClearError();
    }
    mode = fault; reentry = reenter;
    if (scoped) xl_script_module_unit();
    else result.ok = xl_script_module_unit_checked() != 0;
    assert(steps == 3 && closed == 1 && sequence == 3214 && reentered == reenter);
    xl_script_module_unit();
    assert(steps == 3 && closed == 1);
    if (scoped) result = xl6ErrorScopeEnd(&outer);
    else result.error = xrtTakeError();
    if (pending > 0 && pending < 4) assert(!result.ok && result.error == original);
    else if (fault == 0) assert(result.ok && result.error == NULL);
    else {
        assert(!result.ok);
        if (fault == 1 || fault == 4) assert(result.error != NULL && xrtErrorCode(result.error) == 103);
        else assert(result.error == NULL);
    }
    xrtErrorFree(result.error);
    assert(xrtGetError() == NULL && xlir_module_initialized == 0);
}

int class_cleanup_local_probe(int scope)
{
    int64_t result;
    mode = reentry = sequence = steps = opened = closed = reentered = 0;
    assert(xl_script_module_init(NULL));
    mode = 2;
    result = scope ? class_cleanup_scope() : class_cleanup_temporary();
    printf("class cleanup %s probe: value=%lld, steps=%d, order=%d, diagnostic=%d\n",
        scope ? "scope" : "temporary", (long long)result, steps, sequence, xrtGetError() != NULL);
    assert(steps == 3 && sequence == 321);
    xrtClearError(); mode = 0; xl_script_module_unit(); xrtClearError();
    assert(closed == 1 && steps == 6);
    return result == 202;
}
