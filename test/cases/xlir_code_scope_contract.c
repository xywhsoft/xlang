#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_THREAD
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_code_lease.h"
#include "xlir_code_scope_drop.h"
#include <assert.h>
#include <stdio.h>
#include <windows.h>

typedef struct Owner {
    XL6_CodeOwnerGraph descriptor;
    XL6_CodeLease* slot;
    bool closed;
    unsigned pins, acquired, released, unmapped;
    HMODULE library;
    void* address;
    ScopeDropHooks hooks;
    xerror* releaseError;
} Owner;
static int acquire(void* data)
{
    Owner* owner = (Owner*)data;
    if (owner->closed) return 0;
    ++owner->acquired; ++owner->pins; return 1;
}
static int different_acquire(void* data) { return acquire(data); }
static void release(void* data)
{
    Owner* owner = (Owner*)data;
    assert(owner->pins); --owner->pins; ++owner->released;
    if (!owner->pins && owner->library) {
        MEMORY_BASIC_INFORMATION memory;
        assert(owner->closed && owner->hooks.returned == 1);
        assert(FreeLibrary(owner->library));
        assert(VirtualQuery(owner->address, &memory, sizeof(memory)) == sizeof(memory) && memory.State == MEM_FREE);
        ++owner->unmapped;
    }
    if (owner->releaseError) xrtSetErrorTake(xrtErrorRef(owner->releaseError));
}
static void different_release(void* data) { release(data); }
static bool count(const void* data, size_t* result)
{
    const Owner* owner = (const Owner*)data;
    *result = owner->pins + !owner->closed; return true;
}
static bool trace(const void* data, xrtownershipvisitor visit, void* context)
{
    const Owner* owner = (const Owner*)data;
    return !owner->slot || visit(xl6CodeLeaseOwnership(owner->slot), context);
}
static const xrtownershipops ops = {count, trace}, otherOps = {count, trace};
static void prepare(Owner* owner)
{
    owner->descriptor = (XL6_CodeOwnerGraph){{sizeof(owner->descriptor), owner, acquire, release},
        0x584C36434F574E31ULL, &ops};
}
static void graph(Owner* owner, size_t roots, size_t refs)
{
    xrtownershipref anchor = {owner, &ops}, lease = xl6CodeLeaseOwnership(owner->slot);
    xrtownershipresult result; size_t actual;
    assert(lease.Ops->Count(lease.Data, &actual) && actual == refs);
    assert(xrtOwnershipInspect(&anchor, 1, NULL, 0, &result));
    assert(result.NodeCount == 2 && result.EdgeCount == 2 && result.ExternalRootCount == roots && result.ReachableAnchorCount == roots);
}
static void during(void* data)
{
    Owner* owner = (Owner*)data;
    XL6_CodeLease* child = NULL;
    XL6_CodeScope nested = {0};
    XL6_CodeOwnerGraph copy = owner->descriptor;
    assert(owner->closed && owner->pins == 1 && !owner->released);
    assert(xl6CodeLeaseAcquire(&copy.base, &child) == XL6_CODE_LEASE_CLOSED && !child);
    assert(xrtMemDebugFailAfter(0));
    assert(xl6CodeLeaseAcquireScoped(&copy.base, &child) == XL6_CODE_LEASE_ACQUIRED && child);
    xl6CodeScopeEnter(&nested, child);
    xl6CodeLeaseRelease(child); child = NULL;
    assert(xl6CodeLeaseAcquireScoped(&copy.base, &child) == XL6_CODE_LEASE_ACQUIRED && child);
    xl6CodeLeaseRelease(child); xl6CodeScopeLeave(&nested);
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    assert(owner->acquired == 1 && !owner->released);
}
typedef struct Worker {
    Owner* owner;
    XL6_CodeLease* lease;
    HANDLE start;
    bool rejectOnly;
} Worker;
static DWORD WINAPI work(void* data)
{
    Worker* worker = (Worker*)data;
    XL6_CodeLease* child = NULL;
    XL6_CodeScope scope = {0};
    assert(WaitForSingleObject(worker->start, 5000) == WAIT_OBJECT_0);
    if (worker->rejectOnly) {
        assert(xl6CodeLeaseAcquireScoped(&worker->owner->descriptor.base, &child) == XL6_CODE_LEASE_CLOSED && !child);
    } else {
        /* This native thread has never entered XRT before. The main thread
         * has armed its allocator: scope admission and inheritance allocate 0. */
        xl6CodeScopeEnter(&scope, worker->lease);
        for (unsigned i = 0; i < 1000; ++i) {
            assert(xl6CodeLeaseAcquireScoped(&worker->owner->descriptor.base, &child) == XL6_CODE_LEASE_ACQUIRED && child == worker->lease);
            xl6CodeLeaseRelease(child);
        }
        xl6CodeScopeLeave(&scope);
    }
    return 0;
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes &&
        after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount &&
        before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount &&
        before->UseAfterFreeCount == after.UseAfterFreeCount);
}
int main(int argc, char** argv)
{
    assert(argc == 2 && xrtMemDebugEnable(true));
    for (unsigned round = 0; round < 50; ++round) {
        Owner owner = {0}; XL6_CodeLease* parent = NULL; XL6_CodeDropProc drop;
        XL6_CodeDrop* record; FARPROC symbol; xmemdebugsnapshot before;
        xrtClearError(); xrtMemDebugSnapshot(&before); prepare(&owner);
        owner.library = LoadLibraryA(argv[1]); assert(owner.library);
        symbol = GetProcAddress(owner.library, "scope_generated_drop"); memcpy(&drop, &symbol, sizeof(drop)); assert(drop);
        owner.address = (void*)symbol; owner.hooks = (ScopeDropHooks){during, &owner, 0};
        owner.releaseError = xrtErrorCreate(XERR_STATE, "host", 7, "release diagnostic"); assert(owner.releaseError);
        assert(xl6CodeLeaseAcquire(&owner.descriptor.base, &parent) == XL6_CODE_LEASE_ACQUIRED);
        record = xl6CodeDropCreate(parent, drop, &owner.hooks); assert(record);
        owner.closed = true; xl6CodeLeaseRelease(parent); xl6CodeDropInvoke(NULL, record);
        assert(owner.acquired == 1 && owner.released == 1 && owner.unmapped == 1 && !xrtGetError());
        xrtErrorFree(owner.releaseError); balanced(&before);
    }
    for (unsigned round = 0; round < 25; ++round) {
        Owner owner = {0}, other = {0}; XL6_CodeScope outer = {0}, nested = {0}, external = {0};
        XL6_CodeLease *child = NULL, *foreign = NULL;
        xmemdebugsnapshot before; HANDLE threads[4], start; Worker jobs[4];
        xrtMemDebugSnapshot(&before); prepare(&owner); prepare(&other);
        assert(xl6CodeLeaseAcquire(&owner.descriptor.base, &owner.slot) == XL6_CODE_LEASE_ACQUIRED);
        assert(xl6CodeLeaseAcquire(&other.descriptor.base, &foreign) == XL6_CODE_LEASE_ACQUIRED);
        owner.closed = other.closed = true; graph(&owner, 0, 1);
        xl6CodeScopeEnter(&outer, owner.slot); graph(&owner, 1, 2);
        xl6CodeScopeEnter(&nested, foreign); xl6CodeScopeEnter(&external, NULL);
        assert(xl6CodeLeaseAcquireScoped(&owner.descriptor.base, &child) == XL6_CODE_LEASE_ACQUIRED && child == owner.slot);
        xl6CodeLeaseRelease(child); xl6CodeScopeLeave(&external); xl6CodeScopeLeave(&nested);
        for (unsigned mode = 0; mode < 4; ++mode) {
            XL6_CodeOwnerGraph copy = owner.descriptor;
            if (mode == 0) copy.base.acquire = different_acquire;
            if (mode == 1) copy.base.release = different_release;
            if (mode == 2) copy.ops = &otherOps;
            if (mode == 3) copy.base.context = &other;
            assert(xl6CodeLeaseAcquireScoped(&copy.base, &child) == XL6_CODE_LEASE_CLOSED && !child);
        }
        for (unsigned mode = 0; mode < 2; ++mode) {
            start = CreateEventA(NULL, TRUE, FALSE, NULL); assert(start);
            for (unsigned i = 0; i < 4; ++i) {
                jobs[i] = (Worker){&owner, owner.slot, start, mode == 0};
                threads[i] = CreateThread(NULL, 0, work, &jobs[i], 0, NULL); assert(threads[i]);
            }
            if (mode == 1) assert(xrtMemDebugFailAfter(0));
            assert(SetEvent(start));
            assert(WaitForMultipleObjects(4, threads, TRUE, 10000) == WAIT_OBJECT_0);
            if (mode == 1) { assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); }
            for (unsigned i = 0; i < 4; ++i) assert(CloseHandle(threads[i]));
            assert(CloseHandle(start));
        }
        graph(&owner, 1, 2); xl6CodeScopeLeave(&outer); graph(&owner, 0, 1);
        assert(xl6CodeLeaseAcquireScoped(&owner.descriptor.base, &child) == XL6_CODE_LEASE_CLOSED && !child);
        child = owner.slot; owner.slot = NULL; xl6CodeLeaseRelease(child); xl6CodeLeaseRelease(foreign);
        assert(owner.acquired == 1 && owner.released == 1 && other.acquired == 1 && other.released == 1);
        balanced(&before);
    }
    puts("Code scopes: 50 real DLL return-before-unmap lifetimes, 25 cyclic graph root transitions, 100000 allocation-free native-thread inheritances, 100 cross-thread and 100 identity refusals");
    assert(xrtMemDebugReset()); return 0;
}
