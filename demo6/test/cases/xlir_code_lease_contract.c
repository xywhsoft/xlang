#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_FUTURE
#define XRT_MODULE_THREAD
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_code_lease.h"
#include "xlir_code_lease_drop.h"
#include <assert.h>
#include <stdatomic.h>
#include <windows.h>
#include <stdio.h>
typedef struct Owner {
    HMODULE library;
    unsigned pins, acquired, released, invoked, unloaded, expected;
    bool closed;
    XL6_CodeLease* borrowed;
    xerror* release_error;
} Owner;
static bool smoke;
static void unmap(Owner* owner)
{
    assert(!owner->unloaded && !owner->pins && owner->invoked == owner->expected);
    assert(FreeLibrary(owner->library));
    assert(!GetModuleHandleA("xlir_code_lease_drop.dll")); ++owner->unloaded;
}
static int acquire(void* data)
{
    Owner* owner = data;
    if (owner->closed) return 0;
    ++owner->acquired; ++owner->pins; return 1;
}
static void release(void* data)
{
    Owner* owner = data;
    assert(owner->pins); --owner->pins; ++owner->released;
    if (owner->closed && !owner->pins) unmap(owner);
    /* Deliberate hostile diagnostic mutation to verify resident isolation. */
    if (owner->release_error) xrtSetErrorTake(xrtErrorRef(owner->release_error));
}
static void close_owner(Owner* owner)
{ owner->closed = true; if (!owner->pins) unmap(owner); }
static void never_drop(void* value, void* data) { (void)value; (void)data; assert(0); }
static void dispose(void* value, xerror* error)
{ xrtFree(value); if (error) xrtSetErrorTake(xrtErrorRef(error)); }
static void inspect(void* data)
{
    Owner* owner = data; XL6_CodeDrop* nested;
    assert(!owner->unloaded && owner->pins == 1 && owner->closed);
    /* Forking the accepted capability after Close is not a host acquisition.
     * An unaccepted nested callback releases its child without running code. */
    nested = xl6CodeDropCreate(owner->borrowed, never_drop, NULL); assert(nested);
    xl6CodeDropAbort(nested); assert(owner->acquired == 1 && owner->released == 0);
    ++owner->invoked;
}
static int32_t share(void* data)
{
    XL6_CodeLease* owned = data; unsigned i;
    for (i = 0; i < 1000; ++i) {
        XL6_CodeLease* copy = xl6CodeLeaseRetain(owned); assert(copy == owned);
        xl6CodeLeaseRelease(copy);
    }
    xl6CodeLeaseRelease(owned); return 0;
}
static void balanced(const xmemdebugsnapshot* before)
{
    if (smoke) return; /* C-host runtime may already own the debug allocator. */
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
    assert(after.AllocCount-before->AllocCount == after.FreeCount-before->FreeCount);
}
int main(int argc, char** argv)
{
    unsigned round, mode;
    smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    assert(argc == 1 || smoke);
    if (!smoke) assert(xrtMemDebugEnable(true));
    {
        XL6_CodeLease* invalid_output = (XL6_CodeLease*)(uintptr_t)1;
        XL6_CodeOwner invalid = {sizeof(invalid) - 1, NULL, acquire, release};
        assert(xl6CodeLeaseAcquire(&invalid, &invalid_output) == XL6_CODE_LEASE_INVALID && !invalid_output);
        invalid.size = sizeof(invalid); invalid.acquire = NULL;
        assert(xl6CodeLeaseAcquire(&invalid, &invalid_output) == XL6_CODE_LEASE_INVALID && !invalid_output);
        assert(xl6CodeLeaseAcquire(NULL, NULL) == XL6_CODE_LEASE_INVALID);
        assert(xl6CodeLeaseAcquire(NULL, &invalid_output) == XL6_CODE_LEASE_ACQUIRED && !invalid_output);
        assert(!xl6CodeLeaseRetain(NULL) && !xl6CodeDropCreate(NULL, NULL, NULL));
        xl6CodeLeaseRelease(NULL); xl6CodeDropAbort(NULL);
    }
    for (round = 0; round < (smoke ? 2u : 50u); ++round) for (mode = 0; mode < (smoke ? 5u : 7u); ++mode) {
        Owner owner = {0}; XL6_CodeOwner descriptor = {sizeof(descriptor), &owner, acquire, release};
        XL6_CodeLease* root = NULL; XL6_CodeDrop* record; XL6_CodeDropProc drop;
        xmemdebugsnapshot before; FARPROC symbol; void* value = NULL;
        LeaseDropHooks hooks = {inspect, &owner, NULL, dispose}; xerror* prior;
        xrtClearError(); xrtMemDebugSnapshot(&before);
        owner.library = LoadLibraryA("build/test/xlir_code_lease_drop.dll"); assert(owner.library);
        symbol = GetProcAddress(owner.library, "lease_generated_drop");
        _Static_assert(sizeof(symbol) == sizeof(drop), "Windows function ABI"); memcpy(&drop, &symbol, sizeof(drop)); assert(drop);
        owner.release_error = xrtErrorCreate(XERR_STATE, "host", 99, "host release diagnostic"); assert(owner.release_error);
        prior = round & 1 ? xrtErrorCreate(XERR_STATE, "prior", 61, "prior diagnostic") : NULL;
        if (round & 1) assert(prior);
        if (mode == 5) {
            xrtSetErrorTake(prior); assert(xrtMemDebugFailAfter(0));
            assert(xl6CodeLeaseAcquire(&descriptor, &root) == XL6_CODE_LEASE_RESOURCE_FAILURE && !root);
            assert(xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
            assert(prior ? xrtGetError() == prior : xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            assert(owner.acquired == 1 && owner.released == 1 && !owner.unloaded);
            xrtClearError(); close_owner(&owner);
        } else {
            assert(xl6CodeLeaseAcquire(&descriptor, &root) == XL6_CODE_LEASE_ACQUIRED && root);
            owner.borrowed = root; owner.expected = mode <= 2 ? 1 : 0;
            close_owner(&owner);
            { XL6_CodeLease* denied = root; xrtSetErrorTake(prior);
              assert(xl6CodeLeaseAcquire(&descriptor, &denied) == XL6_CODE_LEASE_CLOSED && !denied);
              assert(xrtGetError() == prior); xrtClearError(); }
            if (mode == 4) {
                xthread* threads[4]; unsigned i;
                for (i = 0; i < 4; ++i) { threads[i] = xrtThreadCreate(share, xl6CodeLeaseRetain(root), 0); assert(threads[i]); }
                xl6CodeLeaseRelease(root);
                for (i = 0; i < 4; ++i) { assert(xrtThreadWaitFor(threads[i], 5000000) == XWAIT_OK); xrtThreadDestroy(threads[i]); }
                assert(owner.released == 1 && owner.unloaded == 1);
            } else if (mode == 6) {
                xerror* earlier = round & 1 ? xrtErrorCreate(XERR_STATE, "drop-create", 62, "earlier failure") : NULL;
                if (round & 1) assert(earlier);
                xrtSetErrorTake(earlier);
                assert(xrtMemDebugFailAfter(0)); record = xl6CodeDropCreate(root, drop, &hooks);
                assert(!record && xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
                assert(earlier ? xrtGetError() == earlier : xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                assert(owner.invoked == 0 && owner.released == 0); xrtClearError(); xl6CodeLeaseRelease(root);
            } else {
                record = xl6CodeDropCreate(root, drop, &hooks); assert(record);
                if (mode == 3) { xl6CodeDropAbort(record); xl6CodeLeaseRelease(root); }
                else {
                    value = xrtMalloc(32); assert(value);
                    if (mode == 2) {
                        hooks.error = xrtErrorCreate(XERR_STATE, "drop", 67, "destructor failed"); assert(hooks.error);
                        xl6CodeLeaseRelease(root); xl6CodeDropInvoke(value, record);
                        assert(xrtGetError() == hooks.error); xrtClearError(); xrtErrorFree(hooks.error);
                    } else {
                        xfuture* future = NULL; xpromise* promise = xrtPromiseCreate(&future, NULL); assert(promise);
                        if (mode == 1) {
                            assert(xrtPromiseResolve(promise, NULL));
                            assert(!xrtPromiseResolveOwned(promise, value, xl6CodeDropInvoke, record));
                            xrtClearError(); xl6CodeDropAbort(record); drop(value, &hooks);
                            xl6CodeLeaseRelease(root);
                        } else {
                            assert(xrtPromiseResolveOwned(promise, value, xl6CodeDropInvoke, record));
                            xl6CodeLeaseRelease(root); assert(!owner.unloaded && owner.pins == 1);
                        }
                        xrtPromiseDestroy(promise); xrtFutureDestroy(future);
                    }
                }
            }
        }
        assert(owner.unloaded == 1 && owner.acquired == 1 && owner.released == 1);
        xrtErrorFree(owner.release_error); xrtClearError(); balanced(&before);
    }
    puts(smoke ? "code lease C-host smoke: 10 real DLL lifetimes, 8000 retain-release pairs, close/inherit/drop diagnostics; no allocator fault/balance claim" :
        "code lease: 350 real DLL lifetimes, close/inherit/Future/drop/abort/rejected-publication, 200000 concurrent retain-release pairs, 100 real allocation failures, diagnostic and memory balance");
    return 0;
}
