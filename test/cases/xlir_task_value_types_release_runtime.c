/* Deterministic public-XRT contract: independent owners may release a nonfinal
 * reference while a lookup is using the value. No generated code or private
 * xvalue layout is involved. Semaphore order keeps the lookup active until the
 * other owner has attempted its release; this is not a concurrent mutation. */
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_SEM
#define XRT_MODULE_THREAD
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
static xsem *entered, *released;
static xvalue* workerOwner;
static xerror* workerError;
static int armed;
static uint64 hash_key(const xvalue* value, void* user)
{
    (void)value; (void)user;
    if (armed) {
        armed = 0;
        assert(xrtSemPost(entered));
        assert(xrtSemWaitFor(released, 15000000) == XWAIT_OK);
    }
    return 42;
}
static bool equal_key(const xvalue* left, const xvalue* right, void* user)
{ (void)user; return left == right; }
static int32 release_owner(void* data)
{
    (void)data;
    assert(xrtSemWaitFor(entered, 15000000) == XWAIT_OK);
    xrtValueRelease(workerOwner);
    workerError = xrtTakeError();
    assert(xrtSemPost(released));
    return 0;
}
int main(void)
{
    xvalue *set, *key; xthread* worker; xmemdebugsnapshot before, after; int releaseFailed;
    assert(xrtMemDebugEnable(true)); xrtMemDebugSnapshot(&before);
    entered = xrtSemCreate(0, 1); released = xrtSemCreate(0, 1);
    assert(entered && released);
    set = xrtValueSet(); key = xrtValueObject(); assert(set && key);
    assert(xrtValueTypeIdBind(key, 0xABCULL));
    assert(xrtValueIdentityBind(key, hash_key, equal_key, NULL));
    assert(xrtValueSetAdd(set, key));
    workerOwner = xrtValueRetain(set); assert(workerOwner);
    worker = xrtThreadCreate(release_owner, NULL, 0); assert(worker);
    armed = 1;
    assert(xrtValueSetHas(set, key));
    assert(xrtThreadWait(worker) == XWAIT_OK);
    xrtThreadDestroy(worker);
    fprintf(stderr, "nonfinal release during set lookup: error=%s\n",
        workerError ? xrtErrorMessage(workerError) : "none");
    releaseFailed = workerError != NULL;
    xrtErrorFree(workerError); workerError = NULL;
    xrtValueRelease(key); xrtValueRelease(set);
    xrtSemDestroy(entered); xrtSemDestroy(released);
    xrtMemDebugSnapshot(&after);
    fprintf(stderr, "nonfinal release balance: live=%llu bytes=%llu allocations=%llu frees=%llu\n",
        (unsigned long long)(after.LiveCount - before.LiveCount),
        (unsigned long long)(after.LiveBytes - before.LiveBytes),
        (unsigned long long)(after.AllocCount - before.AllocCount),
        (unsigned long long)(after.FreeCount - before.FreeCount));
    return releaseFailed || after.LiveCount != before.LiveCount ||
        after.LiveBytes != before.LiveBytes ||
        after.AllocCount - before.AllocCount != after.FreeCount - before.FreeCount ||
        after.InvalidFreeCount != before.InvalidFreeCount || after.DoubleFreeCount != before.DoubleFreeCount ||
        after.UseAfterFreeCount != before.UseAfterFreeCount ? 1 : 0;
}
