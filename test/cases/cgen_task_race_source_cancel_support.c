#define XRT_MODULE_FUTURE
#include "lib/xrt/xrt.h"
#include <assert.h>

/* Keep the source producer rights until the language frames have cancelled.
 * Resolve afterwards to prove that stale transport notification cannot revive
 * the outer Futures or replace their terminal cancellation outcomes. */
static xpromise* sources[2];
static size_t count;
static int64_t value;
xfuture* race_pending(void)
{
    xfuture* result = NULL;
    assert(count < 2);
    sources[count] = xrtPromiseCreate(&result, NULL);
    if (sources[count] != NULL) ++count;
    return result;
}
bool race_complete_source(int64_t index)
{
    bool ok;
    if (index < 0 || index >= 2 || sources[index] == NULL) return false;
    ok = xrtPromiseResolve(sources[index], &value);
    xrtPromiseDestroy(sources[index]); sources[index] = NULL;
    return ok;
}
