#define XRT_MODULE_ERROR
#ifdef XLIR_BOUNDARY_FAULTS
#define XRT_MODULE_MEMORY_DEBUG
#endif
#include "lib/xrt/xrt.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

/* Shared native fixture for real language CLI, standalone and DLL API entry.
 * No generated-C inclusion: these tests exercise artifact symbol boundaries. */
static int mode = -1, opened, closed, mask;
static int parse_int_arg(const char* text)
{
    char* end = NULL;
    long value;
    errno = 0;
    value = strtol(text, &end, 10);
    assert(errno == 0 && end != text && *end == '\0' &&
        value >= INT_MIN && value <= INT_MAX);
    return (int)value;
}
void module_cleanup_configure(int selected)
{
    mode = selected; opened = closed = mask = 0;
    assert(xrtGetError() == NULL);
}
int module_cleanup_complete(void)
{
    int expected = mode == 6 || mode == 7 ? 1 : 4;
    return opened == expected && closed == expected && mask == (expected == 4 ? 30 : 2);
}
static void fail_without_diagnostic(int allocate)
{
    if (allocate) {
#ifdef XLIR_BOUNDARY_FAULTS
        xerror* error;
        assert(xrtMemDebugFailAfter(0));
        error = xrtErrorCreate(XERR_STATE, "module-boundary", 0, "allocation failure");
        assert(error == NULL && xrtMemDebugFailTriggered());
        xrtMemDebugFailClear();
#else
        /* CLI runtime pools cannot enable allocation instrumentation after
         * startup. That lane belongs to the instrumented DLL API host. */
        abort();
#endif
    }
    xl6ErrorScopePropagateFailure((xl6_error_result){false, NULL});
}
void* module_cleanup_open(int64_t id)
{
    int64_t* handle;
    if (mode < 0) {
        const char* selected = getenv("XLIR_MODULE_CLEANUP_CASE");
        module_cleanup_configure(selected != NULL ? parse_int_arg(selected) : 0);
    }
    handle = (int64_t*)xrtMalloc(sizeof(*handle));
    assert(handle != NULL); *handle = id; ++opened; return handle;
}
bool module_cleanup_fail(void)
{
    if (mode == 6) fail_without_diagnostic(0);
    return mode == 7;
}
int64_t module_cleanup_entry(void)
{
    if (mode == 4) fail_without_diagnostic(0);
    if (mode == 5) xrtSetErrorTake(xrtErrorCreate(XERR_STATE,
        "module-boundary", 101, "primary-entry-sentinel"));
    return mode == 1 ? 1 : 0;
}
void module_cleanup_close(void* value)
{
    int id = (int)*(int64_t*)value;
    assert(id >= 1 && id <= 4 && !(mask & (1 << id)));
    mask |= 1 << id; ++closed; xrtFree(value);
    if (mode == 2 || mode == 8) fail_without_diagnostic(mode == 8);
    else if (mode >= 3) xrtSetErrorTake(xrtErrorCreate(XERR_STATE,
        "module-boundary", 200 + id, "secondary-cleanup-sentinel"));
    if (id == 1) {
        assert(module_cleanup_complete());
        puts("module-boundary: all owners closed exactly once");
    }
}
