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
#include "../include/md5_native.h"
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

/* 独立流状态、错误槽与临时文本在四个线程中交错访问。 */
static xatomic32 ready;
static int32 worker(ptr unused)
{
    unsigned int i, j;
    void* h;
    (void)unused;
    xrtAtomic32FetchAdd(&ready, 1, XMEMORY_SEQ_CST);
    while (xrtAtomic32Load(&ready, XMEMORY_SEQ_CST) != 4) xrtThreadYield();
    h = xmd5_hasher_create(128);
    assert(h);
    for (i = 0; i < 200; ++i) {
        const char* saved = xmd5_hash_str_hex("123456789", 9);
        assert(strcmp(saved, "25f9e794323b453885f5181f1b624d0b") == 0);
        for (j = 0; j < 16; ++j) {
            assert(strcmp(xmd5_hash_str_hex("abc", 3), "900150983cd24fb0d6963f7d28e17f72") == 0);
            xrtThreadYield();
            assert(strcmp(saved, "25f9e794323b453885f5181f1b624d0b") == 0);
        }
        assert(!xmd5_hash_hex(NULL, 1)[0]);
        xrtThreadYield();
        assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
        assert(xmd5_hasher_reset(h) && xmd5_hasher_update_str(h, "123456789", 9));
        assert(strcmp(xmd5_hasher_digest_hex(h), "25f9e794323b453885f5181f1b624d0b") == 0);
        assert(xrtGetError() == NULL && xrtTempClear());
    }
    xmd5_hasher_destroy(h);
    xrtTempUnit(xrtTempCurrent());
    return 0;
}

static void boundaries(void)
{
    void* h;
    assert(xmd5_available() && strcmp(xmd5_version(), "1.0.0") == 0);
    assert(strcmp(xmd5_hash_hex(NULL, 0), "d41d8cd98f00b204e9800998ecf8427e") == 0);
    assert(!xmd5_hash_hex(NULL, 1)[0] && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(xmd5_last_error()[0]);
    assert(xmd5_hash_bytes(NULL, 1) == NULL);
    assert(xmd5_file_bytes("", 0, NULL) == NULL);
    assert(xmd5_hasher_digest_bytes(NULL) == NULL);
    assert(!xmd5_file_hex(NULL, 0, NULL)[0] && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(!xmd5_file_hex("", 0, NULL)[0]);
    assert(!xmd5_file_hex("__md5_missing_file__", 0, NULL)[0] && xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND);
    assert(!xmd5_hasher_update(NULL, NULL, 0) && xrtErrorKind(xrtGetError()) == XERR_STATE);
    assert(!xmd5_hasher_digest_hex(NULL)[0] && !xmd5_hasher_reset(NULL));
    xmd5_hasher_destroy(NULL);
    h = xmd5_hasher_create(3);
    assert(h && xmd5_hasher_update_str(h, "abc", 3));
    assert(strcmp(xmd5_hasher_digest_hex(h), "900150983cd24fb0d6963f7d28e17f72") == 0);
    assert(!xmd5_hasher_update_str(h, "x", 1) && xmd5_hasher_input_bytes(h) == 3);
    assert(xmd5_hasher_reset(h) && xmd5_hasher_update(h, NULL, 0));
    {
        xbuffer* bytes = xmd5_hasher_digest_bytes(h);
        assert(bytes && xrtBufferView(bytes).Size == 16);
        xrtBufferDestroy(bytes);
    }
    xmd5_hasher_destroy(h);
    assert(strcmp(xmd5_hash_str_hex("123456789", 9), "25f9e794323b453885f5181f1b624d0b") == 0);
    assert(xrtGetError() == NULL);
}

static void allocations(void)
{
    xmemdebugsnapshot before, after;
    unsigned int kind, i, failures = 0, successes = 0;
    char file_hash[33];
    const char* path = "release/extlibs/md5/xlang.package.json";
    strcpy(file_hash, xmd5_file_hex(path, 0, NULL));
    assert(file_hash[0] && xrtTempClear() && xrtTempTrim(xrtTempCurrent(), 0));
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    for (kind = 0; kind < 6; ++kind) {
        unsigned int bad = 0, good = 0;
        bool ordinary = kind != 1 && kind != 3;
        for (i = 0; i < 12; ++i) {
            bool ok, injected;
            const char* text = NULL;
            xbuffer* bytes = NULL;
            if (ordinary) assert(xrtMemDebugFailAfter(i));
            else { backing_failed = false; fail_after = (int)i; }
            if (kind == 0) {
                void* h = xmd5_hasher_create(0);
                ok = h != NULL;
                xmd5_hasher_destroy(h);
            } else if (kind >= 4) {
                bytes = kind == 4 ? xmd5_hash_str_bytes("abc", 3) : xmd5_file_bytes(path, 0, NULL);
                ok = bytes != NULL;
            } else {
                text = kind == 1 ? xmd5_hash_str_hex("abc", 3) : xmd5_file_hex(path, 0, NULL);
                ok = text[0] != 0;
            }
            injected = ordinary ? xrtMemDebugFailTriggered() : backing_failed;
            xrtMemDebugFailClear();
            fail_after = -1;
            if (ok) {
                assert(!injected && xrtGetError() == NULL);
                if (kind > 0 && kind < 4) assert(strcmp(text, kind == 1 ? "900150983cd24fb0d6963f7d28e17f72" : file_hash) == 0);
                if (bytes != NULL) assert(xrtBufferView(bytes).Size == 16);
                ++good; ++successes;
            } else {
                assert(injected && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                ++bad; ++failures;
            }
            xrtBufferDestroy(bytes);
            xrtClearError();
            assert(strcmp(xmd5_hash_str_hex("abc", 3), "900150983cd24fb0d6963f7d28e17f72") == 0);
            assert(xrtTempClear() && xrtTempTrim(xrtTempCurrent(), 0));
            xrtMemDebugSnapshot(&after);
            assert(after.LiveBytes == before.LiveBytes && after.LiveCount == before.LiveCount);
            assert(after.InvalidFreeCount == before.InvalidFreeCount && after.DoubleFreeCount == before.DoubleFreeCount);
            assert(after.OverflowCount == before.OverflowCount && after.UnderflowCount == before.UnderflowCount);
        }
        assert(bad && good);
    }
    printf("md5 allocations: rejected=%u completed=%u\n", failures, successes);
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
    puts("md5 native: 4 threads, temporary and Buffer lifetimes passed");
    return 0;
}
