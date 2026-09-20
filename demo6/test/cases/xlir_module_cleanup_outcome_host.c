#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../build/test/xlir_module_cleanup_outcome.c"

void module_cleanup_oom(void);
extern void* xrtMalloc(size_t);
extern void xrtFree(void*);
static int initialization, cleanup_mode, reentry, armed;
static int opened, closed, mask, first_id, last_id, reentries;
int64_t module_cleanup_entry(void) { return 0; }

static void publish(int mode, int code)
{
    xl6_error_result outcome = {false, NULL};
    if (mode == 1) {
        outcome.error = xrtErrorCreate(15, "module-cleanup", code, "cleanup diagnostic");
        assert(outcome.error != NULL);
        xrtSetErrorTake(outcome.error); /* Native diagnostic-only boundary. */
    } else if (mode != 0) {
        if (mode == 3) module_cleanup_oom();
        xl6ErrorScopePropagateFailure(outcome);
    }
}

void* module_cleanup_open(int64_t id)
{
    int64_t* value = (int64_t*)xrtMalloc(sizeof(*value));
    assert(value != NULL); *value = id; ++opened; return value;
}

bool module_cleanup_fail(void)
{
    if (initialization == 0) return false;
    armed = 1;
    if (initialization >= 2) publish(initialization == 3 ? 3 : 2, 0);
    return initialization == 1;
}

void module_cleanup_close(void* handle)
{
    int id = (int)*(int64_t*)handle;
    int before = closed;
    assert(handle != NULL && id >= 1 && id <= 4 && (mask & (1 << id)) == 0);
    mask |= 1 << id; if (closed == 0) first_id = id; last_id = id;
    xrtFree(handle); ++closed;
    if (reentry) {
        xl_script_module_unit();
        assert(xl_script_module_init(NULL) == 0 && closed == before + 1);
        ++reentries;
    }
    if (armed) publish(cleanup_mode, 200 + id);
}

int module_cleanup_case(int init_mode, int pending_mode, int cleanup, int reenter)
{
    xl6_error_scope outer = {0};
    xl6_error_result result = {true, NULL};
    xerror* original = NULL;
    int initialized, count = init_mode == 0 ? 4 : 1;
    int scoped = pending_mode != 4;
    initialization = init_mode; cleanup_mode = cleanup; reentry = reenter; armed = 0;
    opened = closed = mask = first_id = last_id = reentries = 0;
    assert(xrtGetError() == NULL);
    if (scoped) assert(xl6ErrorScopeBegin(&outer));
    initialized = xl_script_module_init(NULL);
    if (init_mode == 0) {
        assert(initialized && opened == 4 && closed == 0);
        if (pending_mode == 1 || pending_mode == 3) {
            original = xrtErrorCreate(15, "module-cleanup", 101, "incoming failure");
            assert(original != NULL);
        }
        if (pending_mode == 1) xrtSetErrorTake(original);
        if (pending_mode == 2 || pending_mode == 3) {
            xl6ErrorScopePropagateFailure((xl6_error_result){false, original});
            xrtClearError(); /* The incoming failure bit must survive consumption. */
        }
        armed = 1;
        if (scoped) xl_script_module_unit();
        else result.ok = xl_script_module_unit_checked() != 0;
    } else assert(!initialized);
    assert(opened == count && closed == count && last_id == 1 &&
        first_id == (count == 4 ? 4 : 1) && mask == (count == 4 ? 30 : 2) &&
        reentries == (reenter ? count : 0));
    xl_script_module_unit();
    assert(closed == count); /* An already-finalized phase is idempotent. */
    if (scoped) result = xl6ErrorScopeEnd(&outer);
    else {
        if (init_mode != 0) result.ok = initialized != 0;
        result.error = xrtTakeError();
    }
    if (init_mode != 0) {
        assert(!result.ok);
        if (init_mode == 1) assert(result.error != NULL &&
            strcmp(xrtErrorMessage(result.error), "primary initialization failure") == 0);
        else assert(result.error == NULL);
    } else if (pending_mode != 0 && pending_mode != 4) {
        assert(!result.ok && result.error == original);
    } else if (cleanup != 0) {
        assert(!result.ok);
        if (cleanup == 1) assert(result.error != NULL && xrtErrorCode(result.error) == 204);
        else assert(result.error == NULL);
    } else assert(result.ok && result.error == NULL);
    xrtErrorFree(result.error);
    assert(xrtGetError() == NULL && xlir_module_initialized == 0);
    return 1;
}
