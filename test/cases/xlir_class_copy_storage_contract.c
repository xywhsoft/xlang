#define XRT_MODULE_VALUE_COLLECTION
#define main unused_class_storage_main
#include "xlir_class_storage_contract.c"
#undef main

static xvalue* make_copyable(Owner* owner, unsigned* calls)
{
    xvalueidentityhash hash; xvalueidentityequal equal; void (*bind_counter)(unsigned*); FARPROC symbol;
    xvalue* value = api->Create(&owner->capability.base, NULL); assert(value);
    symbol = GetProcAddress(owner->library, "class_copy_hash"); assert(symbol); memcpy(&hash, &symbol, sizeof(hash));
    symbol = GetProcAddress(owner->library, "class_copy_equal"); assert(symbol); memcpy(&equal, &symbol, sizeof(equal));
    symbol = GetProcAddress(owner->library, "class_copy_bind_counter"); assert(symbol); memcpy(&bind_counter, &symbol, sizeof(bind_counter));
    bind_counter(calls);
    /* Match generated class identity's NULL context. Arbitrary native identity
     * user data is an opaque graph boundary, not silently declared a leaf. */
    assert(xrtValueTypeIdBind(value, 0x4c49) && xrtValueIdentityBind(value, hash, equal, NULL));
    return value;
}
static void invoke_hash(xvalue* value, unsigned* calls)
{
    uint64 hash; unsigned before = *calls;
    uint64 payload = UINT64_C(0x4c494645);
    uint64 expected = payload ^ (UINT64_C(0x4c49) + UINT64_C(0x9E3779B97F4A7C15) + (payload << 6) + (payload >> 2));
    assert(xrtValueHash(value, &hash) && hash == expected && *calls == before + 1);
}
static void copy_lifetimes(const char* path)
{
    for (unsigned round = 0; round < 25; ++round) for (unsigned mode = 0; mode < 4; ++mode) {
        Owner owner = {0}; unsigned calls = 0; xmemdebugsnapshot before; xvalue *source, *copy;
        xvalueiter cursor = {0}; xrtMemDebugSnapshot(&before); load(&owner, path);
        source = make_copyable(&owner, &calls);
        if (mode != 3) assert(api->CommitTake(source) == source);
        if (mode == 0 || mode == 3) {
            copy = xrtValueClone(source); assert(copy && xrtValueReserve(source, 8));
        } else if (mode == 1) {
            copy = xrtValueDeepClone(source); assert(copy);
        } else {
            xvalue* values = xrtValueArray(); xvalue* deep;
            assert(values && xrtValueArrayAppend(values, source) && xrtValueArrayAppend(values, source));
            deep = xrtValueDeepClone(values); assert(deep);
            assert(xrtValueArrayGet(deep, 0) == xrtValueArrayGet(deep, 1));
            copy = xrtValueRetain(xrtValueArrayGet(deep, 0)); assert(copy);
            xrtValueRelease(deep); xrtValueRelease(values);
        }
        if (mode == 3) assert(api->CommitTake(source) == source);
        close_owner(&owner); assert(owner.acquires == 1 && owner.pins == 1 && !owner.unmapped);
        xrtValueRelease(source); graph(copy); invoke_hash(copy, &calls);
        assert(xrtValueIterBegin(copy, &cursor)); xrtValueRelease(copy);
        assert(!owner.unmapped && owner.pins == 1); xrtValueIterEnd(&cursor);
        assert(owner.unmapped == 1 && owner.acquires == 1 && owner.drops == 1 && !xrtGetError()); balanced(&before);
    }
}
static void foreign_empty_merge(const char* first, const char* second)
{
    for (unsigned round = 0; round < 25; ++round) {
        Owner a = {0}, b = {0}; unsigned callsA = 0, callsB = 0; xvalue *target, *source;
        xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); load(&a, first); load(&b, second);
        target = make_copyable(&a, &callsA); source = make_copyable(&b, &callsB);
        assert(api->CommitTake(source) == source && xrtValueObjectSetNew(source, XRT_STR_LITERAL("field"), xrtValueInt(42)));
        assert(xrtValueObjectMerge(target, source, XVALUE_MERGE_REPLACE) && api->CommitTake(target) == target);
        close_owner(&a); close_owner(&b); xrtValueRelease(source);
        assert(b.unmapped == 1 && !a.unmapped && a.pins == 1);
        invoke_hash(target, &callsA); assert(!callsB);
        xrtValueRelease(target); assert(a.unmapped == 1 && a.acquires == 1 && b.acquires == 1); balanced(&before);
    }
}
static void concurrent_copies(const char* path)
{
    for (unsigned round = 0; round < 25; ++round) {
        Owner owner = {0}; unsigned calls = 0; HANDLE threads[4]; xvalue* copies[4]; xvalue* source;
        xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); load(&owner, path);
        source = make_copyable(&owner, &calls); assert(api->CommitTake(source) == source);
        for (unsigned i = 0; i < 4; ++i) { copies[i] = xrtValueDeepClone(source); assert(copies[i]); }
        close_owner(&owner); xrtValueRelease(source); assert(!owner.unmapped && owner.pins == 1);
        for (unsigned i = 0; i < 4; ++i) { threads[i] = CreateThread(NULL, 0, release_alias, copies[i], 0, NULL); assert(threads[i]); }
        assert(WaitForMultipleObjects(4, threads, TRUE, 10000) == WAIT_OBJECT_0);
        for (unsigned i = 0; i < 4; ++i) assert(CloseHandle(threads[i]));
        assert(owner.unmapped == 1 && owner.acquires == 1 && owner.drops == 1 && !xrtGetError()); balanced(&before);
    }
}
int main(int argc, char** argv)
{
    assert(argc == 3 && xrtMemDebugEnable(true)); api = xl6ClassRuntimeV1Get();
    copy_lifetimes(argv[1]); foreign_empty_merge(argv[1], argv[2]); concurrent_copies(argv[1]);
    puts("Resident class copies: 175 real DLL lifetimes, 100 shallow/COW/deep/precommit/cursor families, 25 foreign-module empty merges, 25 four-thread copied-backing releases, generated hash after Close, actual unmap/graph/allocator balance");
    assert(xrtMemDebugReset()); return 0;
}
