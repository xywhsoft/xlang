#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_STRING
#define XRT_MODULE_FUTURE
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <string.h>
static unsigned mode, cleanups, acquired;
static xmemdebugsnapshot before;
static char* payload = "await-owned";
void await_status_begin(unsigned which)
{
    assert(xrtMemDebugEnable(true)); xrtClearError();
    mode = which; cleanups = acquired = 0; xrtMemDebugSnapshot(&before);
}
void await_status_end(void)
{
    xmemdebugsnapshot after;
    assert(cleanups == 2 && acquired == (mode ? 0u : 2u) && xrtGetError() == NULL);
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    assert(after.AllocCount-before.AllocCount == after.FreeCount-before.FreeCount);
}
int64_t await_status_expected(void) { return mode ? -1 : 1; }
void await_status_finally(void) { ++cleanups; }
void await_status_acquired(void) { ++acquired; }
xfuture* await_status_input(void)
{
    xfuture* future = NULL; xpromise* promise = xrtPromiseCreate(&future, NULL);
    assert(promise && xrtPromiseResolve(promise, mode == 1 ? NULL : &payload));
    xrtPromiseDestroy(promise); return future;
}
/* Exact public copy boundary, not a claim of allocator/OS fault injection. */
char* await_status_copy(const char* value)
{
    if (mode == 2 && value == payload) { assert(xrtGetError() == NULL); return NULL; }
    return xrtStrDup(value);
}
