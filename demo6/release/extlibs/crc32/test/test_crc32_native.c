#ifndef XRT_MODULE_THREAD
#define XRT_MODULE_THREAD
#endif
#ifndef XRT_MODULE_ATOMIC
#define XRT_MODULE_ATOMIC
#endif
#ifndef XRT_MODULE_TEMP_MEMORY
#define XRT_MODULE_TEMP_MEMORY
#endif
#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#include "../include/crc32_native.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static xallocator backing;
static int fail_after = -1;
static bool backing_failed;
static bool reject(void)
{
    if (fail_after < 0 || fail_after-- != 0) return false;
    backing_failed = true;
    return true;
}
static ptr allocate(ptr context, size_t size)
{ (void)context; return reject() ? NULL : backing.Alloc(backing.Context, size); }
static ptr reallocate(ptr context, ptr memory, size_t size)
{ (void)context; return reject() ? NULL : backing.Realloc(backing.Context, memory, size); }
static void release(ptr context, ptr memory)
{ (void)context; backing.Free(backing.Context, memory); }

/* 所有线程在任何 CRC 调用之前集合，覆盖首次建表，而不是只测初始化后的热路径。 */
static xatomic32 ready;
static int32 worker(ptr unused)
{
    unsigned int i, j;
    void* h;
    (void)unused;
    xrtAtomic32FetchAdd(&ready, 1, XMEMORY_SEQ_CST);
    while (xrtAtomic32Load(&ready, XMEMORY_SEQ_CST) != 4) xrtThreadYield();
    h = xcrc32_hasher_create(128);
    assert(h);
    for (i = 0; i < 200; ++i) {
        const char* saved = xcrc32_hash_str_hex("123456789", 9);
        assert(strcmp(saved, "cbf43926") == 0);
        for (j = 0; j < 16; ++j) {
            assert(strcmp(xcrc32_hash_str_hex("abc", 3), "352441c2") == 0);
            xrtThreadYield();
            assert(strcmp(saved, "cbf43926") == 0);
        }
        assert(!xcrc32_hash_hex(NULL, 1)[0]);
        xrtThreadYield();
        assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
        assert(xcrc32_hasher_reset(h) && xcrc32_hasher_update_str(h, "123456789", 9));
        assert(strcmp(xcrc32_hasher_digest_hex(h), "cbf43926") == 0);
        assert(xrtGetError() == NULL && xrtTempClear());
    }
    xcrc32_hasher_destroy(h);
    xrtTempUnit(xrtTempCurrent());
    return 0;
}

static void boundaries(void)
{
    void* h;
    assert(xcrc32_available() && strcmp(xcrc32_version(), "1.0.0") == 0);
    assert(xcrc32_hash(NULL, 0) == 0);
    assert(!xcrc32_hash_hex(NULL, 1)[0] && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(xcrc32_last_error()[0]);
    assert(!xcrc32_file_hex(NULL, 0, NULL)[0] && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(!xcrc32_file_hex("", 0, NULL)[0]);
    assert(!xcrc32_file_hex("__crc32_missing_file__", 0, NULL)[0] && xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND);
    assert(!xcrc32_hasher_update(NULL, NULL, 0) && xrtErrorKind(xrtGetError()) == XERR_STATE);
    assert(!xcrc32_hasher_digest_hex(NULL)[0] && !xcrc32_hasher_reset(NULL));
    xcrc32_hasher_destroy(NULL);
    h = xcrc32_hasher_create(3);
    assert(h && xcrc32_hasher_update_str(h, "abc", 3));
    assert(xcrc32_hasher_digest(h) == 0x352441C2LL);
    assert(!xcrc32_hasher_update_str(h, "x", 1) && xcrc32_hasher_input_bytes(h) == 3);
    assert(xcrc32_hasher_reset(h) && xcrc32_hasher_update(h, NULL, 0));
    xcrc32_hasher_destroy(h);
    assert(xcrc32_hash_str("123456789", 9) == 3421780262LL);
    assert(xrtGetError() == NULL);
}

static void allocations(void)
{
    xmemdebugsnapshot before, after;
    unsigned int kind, i, failures = 0, successes = 0;
    char file_hash[9];
    const char* path = "release/extlibs/crc32/xlang.package.json";
    strcpy(file_hash, xcrc32_file_hex(path, 0, NULL));
    assert(file_hash[0] && xrtTempClear() && xrtTempTrim(xrtTempCurrent(), 0));
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    for (kind = 0; kind < 4; ++kind) {
        unsigned int bad = 0, good = 0;
        bool ordinary = kind == 0 || kind == 2;
        for (i = 0; i < 12; ++i) {
            bool ok, injected;
            const char* text = NULL;
            if (ordinary) assert(xrtMemDebugFailAfter(i));
            else { backing_failed = false; fail_after = (int)i; }
            if (kind == 0) {
                void* h = xcrc32_hasher_create(0);
                ok = h != NULL;
                xcrc32_hasher_destroy(h);
            } else {
                text = kind == 1 ? xcrc32_hash_str_hex("abc", 3) : xcrc32_file_hex(path, 0, NULL);
                ok = text[0] != 0;
            }
            injected = ordinary ? xrtMemDebugFailTriggered() : backing_failed;
            xrtMemDebugFailClear();
            fail_after = -1;
            if (ok) {
                assert(!injected && xrtGetError() == NULL);
                if (kind > 0) assert(strcmp(text, kind == 1 ? "352441c2" : file_hash) == 0);
                ++good; ++successes;
            } else {
                assert(injected && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                ++bad; ++failures;
            }
            xrtClearError();
            assert(strcmp(xcrc32_hash_str_hex("abc", 3), "352441c2") == 0);
            assert(xrtTempClear() && xrtTempTrim(xrtTempCurrent(), 0));
            xrtMemDebugSnapshot(&after);
            assert(after.LiveBytes == before.LiveBytes && after.LiveCount == before.LiveCount);
            assert(after.InvalidFreeCount == before.InvalidFreeCount && after.DoubleFreeCount == before.DoubleFreeCount);
            assert(after.OverflowCount == before.OverflowCount && after.UnderflowCount == before.UnderflowCount);
        }
        assert(bad && good);
    }
    printf("crc32 allocations: rejected=%u completed=%u\n", failures, successes);
}

int main(void)
{
    xallocator allocator = {NULL, allocate, reallocate, release};
    xthread* threads[4];
    size_t i;
    xrtGetAllocator(&backing);
    assert(xrtSetAllocator(&allocator) && xrtMemDebugEnable(true));
    xrtAtomic32Init(&ready, 0);
    for (i = 0; i < 4; ++i) { threads[i] = xrtThreadCreate(worker, NULL, 0); assert(threads[i]); }
    for (i = 0; i < 4; ++i) {
        assert(xrtThreadWait(threads[i]) == XWAIT_OK && xrtThreadExitCode(threads[i]) == 0);
        xrtThreadDestroy(threads[i]);
    }
    boundaries();
    allocations();
    xrtClearError();
    xrtTempUnit(xrtTempCurrent());
    puts("crc32 native: concurrent first use, 4 threads, temporary lifetimes passed");
    return 0;
}
