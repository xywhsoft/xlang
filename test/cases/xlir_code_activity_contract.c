/* Actual resident registry, with native OS threads that have no XRT context.
 * Stack activity never grants residency or accepted-lease inheritance. */
#include "release/include/xrt_decl.h"
#include "src/runtime/xl6_code_lease.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

static unsigned acquires, releases;
static int closed(void* ignored) { (void)ignored; ++acquires; return 0; }
static void release(void* ignored) { (void)ignored; ++releases; }
static int identity;
static XL6_CodeOwner owner = {sizeof(owner), &identity, closed, release};
static xatomic64 ready = {0}, proceed = {0};

static void inspect(bool active, bool foreign)
{
    xrtownershipscope freeze = {0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(xl6CodeOwnerScopeActive(&owner) == active);
    assert(xl6CodeOwnerScopeForeign(&owner) == foreign);
    XL6_CodeOwner other = owner; other.context = &other;
    assert(!xl6CodeOwnerScopeActive(&other) && !xl6CodeOwnerScopeForeign(&other));
    assert(xrtOwnershipScopeEnd(&freeze));
}
#ifdef _WIN32
static DWORD WINAPI worker(void* ignored)
#else
static void* worker(void* ignored)
#endif
{
    XL6_CodeActivityV1 activity = {0}; (void)ignored;
    xl6CodeActivityEnterV1(&activity, &owner);
    xrtAtomic64Store(&ready, 1, XMEMORY_RELEASE);
    while (!xrtAtomic64Load(&proceed, XMEMORY_ACQUIRE)) xrtThreadYield();
    xl6CodeActivityLeaveV1(&activity);
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX); inspect(false, false);
    xerror* diagnostic = xrtErrorCreate(XERR_STATE, "activity", 162, "preserved"); assert(diagnostic);
    xrtSetErrorTake(diagnostic);
    xmemdebugsnapshot before, after; xrtMemDebugSnapshot(&before);
    assert(xrtMemDebugFailAfter(0));
    for (unsigned i = 0; i < 200; ++i) {
        XL6_CodeActivityV1 outer = {0}, inner = {0}, empty = {0};
        xl6CodeActivityEnterV1(&outer, &owner); inspect(true, false);
        assert(outer.self == &outer && outer.identity.size == sizeof(XL6_CodeOwner));
        xl6CodeActivityEnterV1(&inner, &owner);
        xl6CodeActivityEnterV1(&empty, NULL); xl6CodeActivityLeaveV1(&empty);
        assert(!empty.self && xrtGetError() == diagnostic);
        xl6CodeActivityLeaveV1(&inner); inspect(true, false);
        xl6CodeActivityLeaveV1(&outer); inspect(false, false);
        assert(!outer.self && !outer.identity.size && xrtGetError() == diagnostic);
    }
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    xrtMemDebugSnapshot(&after); assert(after.AllocCount == before.AllocCount && after.FreeCount == before.FreeCount);
    assert(!acquires && !releases);
    XL6_CodeActivityV1 borrowed = {0}; XL6_CodeLease* lease = NULL;
    xl6CodeActivityEnterV1(&borrowed, &owner);
    assert(xl6CodeLeaseAcquireScoped(&owner, &lease) == XL6_CODE_LEASE_CLOSED && !lease);
    assert(acquires == 1 && !releases); xl6CodeActivityLeaveV1(&borrowed);
    xrtClearError();
    /* Only native OS thread APIs below; registration on a cold thread must
     * allocate zero XRT blocks even when allocator failure is armed. */
    xrtMemDebugSnapshot(&before); assert(xrtMemDebugFailAfter(0));
#ifdef _WIN32
    HANDLE thread = CreateThread(NULL, 0, worker, NULL, 0, NULL); assert(thread);
#else
    pthread_t thread; assert(!pthread_create(&thread, NULL, worker, NULL));
#endif
    while (!xrtAtomic64Load(&ready, XMEMORY_ACQUIRE)) xrtThreadYield();
    inspect(true, true);
    xl6CodeActivityEnterV1(&borrowed, &owner); inspect(true, true);
    xl6CodeActivityLeaveV1(&borrowed);
    xrtAtomic64Store(&proceed, 1, XMEMORY_RELEASE);
#ifdef _WIN32
    assert(WaitForSingleObject(thread, 10000) == WAIT_OBJECT_0); assert(CloseHandle(thread));
#else
    assert(!pthread_join(thread, NULL));
#endif
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    xrtMemDebugSnapshot(&after); assert(after.AllocCount == before.AllocCount && after.FreeCount == before.FreeCount);
    inspect(false, false); xl6ProductMemoryVerify();
    puts("borrowed activity: 200 nested scopes, closed non-capability, cold foreign OS thread, zero XRT allocations and balanced heap");
    return 0;
}
