#define XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include "src/runtime/xl6_code_lease.h"
#include "xlir_class_storage_dll.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

typedef struct Owner {
    HMODULE library;
    const void* address;
    XL6_CodeOwnerGraph capability;
    XL6_CodeFinalizeProc finalize;
    ClassStorageHooks hooks;
    xvalue* child;
    xvalueiter cursor;
    unsigned pins, acquires, drops, finalized, unmapped, expectedFinalized;
    unsigned mode;
    bool closed;
} Owner;
static const XL6_ClassRuntimeV1* api;
static void retire(Owner* owner)
{
    MEMORY_BASIC_INFORMATION info;
    assert(owner->closed && !owner->pins && !owner->unmapped && !owner->child);
    assert(owner->finalized == owner->expectedFinalized && owner->hooks.returned == owner->finalized);
    assert(FreeLibrary(owner->library)); owner->library = NULL;
    assert(VirtualQuery(owner->address, &info, sizeof(info)) == sizeof(info) && info.State == MEM_FREE);
    ++owner->unmapped;
}
static int acquire(void* data)
{
    Owner* owner = data;
    if (owner->closed) return false;
    ++owner->pins; ++owner->acquires; return true;
}
static void release(void* data)
{
    Owner* owner = data;
    assert(owner->pins); --owner->pins; ++owner->drops;
    if (!owner->pins && owner->closed) retire(owner);
}
static bool owner_count(const void* data, size_t* count)
{
    const Owner* owner = data;
    if (!owner || !count || owner->unmapped) return false;
    *count = owner->pins + (owner->closed ? 0u : 1u); return *count != 0;
}
static bool leaf(const void* data, xrtownershipvisitor visit, void* context)
{ (void)data; (void)visit; (void)context; return true; }
static const xrtownershipops owner_ops = {owner_count, leaf};
static void finalize(xvalue* value, void* data)
{
    Owner* owner = data;
    assert(owner->closed && owner->pins == 1 && !owner->unmapped && !owner->finalized++);
    if (owner->mode == 6) assert(xrtValueIterBegin(value, &owner->cursor));
    if (owner->mode == 7) {
        owner->child = api->Create(&owner->capability.base, NULL);
        assert(owner->child && api->CommitTake(owner->child) == owner->child && owner->acquires == 1);
    }
}
static void load(Owner* owner, const char* path)
{
    void (*bind)(ClassStorageHooks*); FARPROC address;
    owner->library = LoadLibraryA(path); assert(owner->library);
    address = GetProcAddress(owner->library, "class_storage_bind"); assert(address);
    memcpy(&bind, &address, sizeof(bind));
    address = GetProcAddress(owner->library, "class_storage_finalize"); assert(address);
    memcpy(&owner->finalize, &address, sizeof(owner->finalize));
    memcpy(&owner->address, &address, sizeof(owner->address));
    owner->hooks = (ClassStorageHooks){owner, finalize, 0}; bind(&owner->hooks);
    owner->capability = (XL6_CodeOwnerGraph){{sizeof(XL6_CodeOwnerGraph), owner, acquire, release},
        0x584C36434F574E31ULL, &owner_ops};
}
static void close_owner(Owner* owner)
{ owner->closed = true; if (!owner->pins) retire(owner); }
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes &&
        after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount &&
        before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount &&
        before->UseAfterFreeCount == after.UseAfterFreeCount);
}
static void graph(xvalue* value)
{
    xrtownershipref anchor = xrtValueOwnership(value); xrtownershipresult stats;
    assert(xrtOwnershipInspect(&anchor, 1, NULL, 0, &stats));
    /* shell -> backing -> shared lifetime -> CodeLease -> host. The unique
     * finalizer borrows that lease; it must not add a fictitious owning edge. */
    assert(stats.NodeCount == 5 && stats.EdgeCount == 4 && stats.ExternalRootCount == 1 && stats.ReachableAnchorCount == 1);
}
static DWORD WINAPI release_alias(LPVOID data)
{ xrtValueRelease(data); return 0; }
static void lifetimes(const char* path)
{
    for (unsigned round = 0; round < 25; ++round) for (unsigned mode = 0; mode < 9; ++mode) {
        Owner owner = {0}; xmemdebugsnapshot before;
        xvalue *value, *alias = NULL; xvalueiter cursor = {0};
        HANDLE threads[4] = {0};
        xrtMemDebugSnapshot(&before); load(&owner, path); owner.mode = mode;
        owner.expectedFinalized = (mode != 1 && mode != 5);
        value = api->Create(&owner.capability.base, mode == 5 ? NULL : owner.finalize); assert(value && owner.pins == 1);
        if (mode == 2) alias = xrtValueRetain(value);
        if (mode == 3) alias = xrtValueClone(value);
        if (mode == 4) assert(xrtValueIterBegin(value, &cursor));
        close_owner(&owner); assert(!owner.unmapped);
        if (mode <= 1 || mode >= 5) graph(value);
        if (mode == 8) for (unsigned i = 0; i < 4; ++i) {
            xvalue* copied = xrtValueClone(value); assert(copied);
            threads[i] = CreateThread(NULL, 0, release_alias, copied, 0, NULL); assert(threads[i]);
        }
        assert(xrtMemDebugFailAfter(0));
        if (mode != 1) assert(api->CommitTake(value) == value);
        assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
        if (mode == 8) {
            assert(WaitForMultipleObjects(4, threads, TRUE, 10000) == WAIT_OBJECT_0);
            for (unsigned i = 0; i < 4; ++i) assert(CloseHandle(threads[i]));
        }
        xrtValueRelease(value);
        if (mode >= 2 && mode <= 4) assert(!owner.unmapped && !owner.finalized);
        xrtValueRelease(alias); xrtValueIterEnd(&cursor);
        if (mode == 6) { assert(!owner.unmapped && owner.hooks.returned == 1); xrtValueIterEnd(&owner.cursor); }
        if (mode == 7) {
            xvalue* child = owner.child; assert(child && !owner.unmapped); graph(child);
            owner.child = NULL; xrtValueRelease(child);
        }
        assert(owner.unmapped == 1 && owner.pins == 0 && owner.acquires == 1 && owner.drops == 1 && !xrtGetError());
        balanced(&before);
    }
}
static unsigned faults(const char* path)
{
    unsigned rejected = 0, allocationPoints = 0;
    for (unsigned round = 0; round < 25; ++round) {
        bool succeeded = false;
        for (unsigned point = 0; point < 32; ++point) {
            Owner owner = {0}; xmemdebugsnapshot before; xvalue* value; xerror* primary;
            xrtMemDebugSnapshot(&before); load(&owner, path);
            primary = (round & 1) ? xrtErrorCreate(XERR_IO, "primary", 1, "earlier error") : NULL;
            if (round & 1) assert(primary);
            xrtSetErrorTake(primary); assert(xrtMemDebugFailAfter(point));
            value = api->Create(&owner.capability.base, owner.finalize);
            if (value) { assert(!xrtMemDebugFailTriggered()); succeeded = true; }
            else { assert(xrtMemDebugFailTriggered()); ++rejected; }
            xrtMemDebugFailClear();
            assert(primary ? xrtGetError() == primary : value ? !xrtGetError() : xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            xrtClearError(); xrtValueRelease(value); close_owner(&owner);
            assert(owner.unmapped == 1 && !owner.pins && owner.acquires == owner.drops && !owner.finalized);
            balanced(&before);
            if (succeeded) {
                if (round == 0) allocationPoints = point;
                assert(point == allocationPoints && point == 5); break;
            }
        }
        assert(succeeded);
    }
    assert(rejected == allocationPoints * 25); return rejected;
}
int main(int argc, char** argv)
{
    unsigned refused;
    assert(argc == 2 && xrtMemDebugEnable(true)); api = xl6ClassRuntimeV1Get();
    lifetimes(argv[1]); refused = faults(argv[1]);
    printf("Resident class storage: 225 real DLL lifetimes, native aliases/cursors, 25 destructor-free code owners, 25 abandoned constructions, 25 concurrent alias cohorts, 25 inherited children, exact graph/unmap and allocator balance; %u actual allocation refusals\n", refused);
    assert(xrtMemDebugReset()); return 0;
}
