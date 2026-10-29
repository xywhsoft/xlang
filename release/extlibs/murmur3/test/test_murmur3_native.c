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
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#endif
#include "xruntime.h"
#include "../include/murmur3_native.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char* abc128 = "d6f7cfb39f08850d303d35422b711075";
static const uint8 abc32bytes[4] = {0xfa,0x93,0xdd,0xb3};
static const uint8 abc128bytes[16] = {0xd6,0xf7,0xcf,0xb3,0x9f,0x08,0x85,0x0d,0x30,0x3d,0x35,0x42,0x2b,0x71,0x10,0x75};
static const char* test_file = "release/extlibs/murmur3/xlang.package.json";

// ----------------------------- 两条分配路径的故障注入 -----------------------------

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

static void check_buffer(xbuffer* buffer, const uint8* expected, size_t size)
{
    xbytesview view;
    assert(buffer);
    view = xrtBufferView(buffer);
    assert(view.Size == size && memcmp(view.Data, expected, size) == 0);
    xrtBufferDestroy(buffer);
}

// ----------------------------- 独立线程、临时文本和错误槽 -----------------------------

static xatomic32 ready;
static int32 worker(ptr unused)
{
    unsigned i, j;
    void* h;
    (void)unused;
    xrtAtomic32FetchAdd(&ready, 1, XMEMORY_SEQ_CST);
    while (xrtAtomic32Load(&ready, XMEMORY_SEQ_CST) != 4) xrtThreadYield();
    h = xmurmur3_hasher_create(42, 128);
    assert(h);
    for (i = 0; i < 200; ++i) {
        const char* saved = xmurmur3_hash128_str_hex("123456789", 9, 42);
        assert(strcmp(saved, "d91c7e2ab66d52113fcca17c019c9a68") == 0);
        for (j = 0; j < 16; ++j) {
            assert(strcmp(xmurmur3_hash128_str_hex("abc", 3, 42), abc128) == 0);
            xrtThreadYield();
            assert(strcmp(saved, "d91c7e2ab66d52113fcca17c019c9a68") == 0);
        }
        assert(!xmurmur3_hash128_hex(NULL, 1, 42)[0]);
        xrtThreadYield();
        assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
        assert(xmurmur3_hasher_reset(h) && xmurmur3_hasher_update_str(h, "abc", 3));
        assert(strcmp(xmurmur3_hasher_digest_hex(h), abc128) == 0);
        assert(xrtGetError() == NULL && xrtTempClear());
    }
    xmurmur3_hasher_destroy(h);
    xrtTempUnit(xrtTempCurrent());
    return 0;
}

// ----------------------------- 全部薄绑定入口及所有权 -----------------------------

static void boundaries(void)
{
    void *h, *copy;
    xvalue* invalid_callback;
    assert(xmurmur3_available() && strcmp(xmurmur3_version(), "3.0.0") == 0);
    assert(xmurmur3_hash32(NULL, 0, 0) == 0 && xrtGetError() == NULL);
    assert(xmurmur3_hash32(NULL, 1, 0) == 0 && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(xmurmur3_last_error()[0]);
    assert(xmurmur3_hash32_str("abc", 3, UINT32_MAX) == INT64_C(4236296879));
    assert(strcmp(xmurmur3_hash32_hex("abc", 3, 0), "fa93ddb3") == 0);
    assert(strcmp(xmurmur3_hash32_str_hex("abc", 3, 0), "fa93ddb3") == 0);
    check_buffer(xmurmur3_hash32_bytes("abc", 3, 0), abc32bytes, 4);
    check_buffer(xmurmur3_hash32_str_bytes("abc", 3, 0), abc32bytes, 4);
    check_buffer(xmurmur3_hash128_bytes("abc", 3, 42), abc128bytes, 16);
    check_buffer(xmurmur3_hash128_str_bytes("abc", 3, 42), abc128bytes, 16);
    assert(!xmurmur3_hash32_hex(NULL, 1, 0)[0] && !xmurmur3_hash128_hex(NULL, 1, 0)[0]);
    assert(!xmurmur3_hash32_bytes(NULL, 1, 0) && !xmurmur3_hash128_bytes(NULL, 1, 0));
    assert(!xmurmur3_file128_hex(NULL, 0, 0, NULL)[0] && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(xmurmur3_file32("", 0, 0, NULL) == 0 && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(!xmurmur3_file128_hex("__murmur3_missing_file__", 0, 0, NULL)[0]);
    assert(xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND);
    assert(!xmurmur3_file128_hex(test_file, 1, 0, NULL)[0] && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    invalid_callback = xrtValueInt(1);
    assert(invalid_callback);
    assert(!xmurmur3_file128_hex(test_file, 0, 0, invalid_callback)[0] && xrtGetError());
    xrtValueRelease(invalid_callback);
    assert(!xmurmur3_hasher_adopt(NULL) && xrtErrorKind(xrtGetError()) == XERR_STATE);
    assert(!xmurmur3_hasher_update(NULL, NULL, 0) && !xmurmur3_hasher_reset(NULL));
    assert(!xmurmur3_hasher_digest_hex(NULL)[0] && !xmurmur3_hasher_digest_bytes(NULL));
    assert(!xmurmur3_hasher_clone(NULL) && xmurmur3_hasher_input_bytes(NULL) == 0);
    xmurmur3_hasher_destroy(NULL);
    h = xmurmur3_hasher_create(42, 3);
    assert(h && xmurmur3_hasher_adopt(h) == h);
    assert(xmurmur3_hasher_update_str(h, "a", 1));
    copy = xmurmur3_hasher_clone(h);
    assert(copy && xmurmur3_hasher_update(copy, "bc", 2));
    assert(strcmp(xmurmur3_hasher_digest_hex(copy), abc128) == 0);
    check_buffer(xmurmur3_hasher_digest_bytes(copy), abc128bytes, 16);
    assert(xmurmur3_hasher_input_bytes(h) == 1 && xmurmur3_hasher_input_bytes(copy) == 3);
    xmurmur3_hasher_destroy(copy);
    assert(!xmurmur3_hasher_update_str(h, "xxx", 3));
    assert(!xmurmur3_hasher_clone(h) && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    assert(xmurmur3_hasher_reset(h) && xmurmur3_hasher_update(h, "abc", 3));
    assert(strcmp(xmurmur3_hasher_digest_hex(h), abc128) == 0);
    xmurmur3_hasher_destroy(h);
    assert(strcmp(xmurmur3_hash128_str_hex("abc", 3, 42), abc128) == 0 && xrtGetError() == NULL);
}

static void allocations(void)
{
    xmemdebugsnapshot before, after;
    unsigned kind, i, failures = 0, successes = 0;
    char file_hash[33];
    long long file32;
    void* source = xmurmur3_hasher_create(42, 0);
    assert(source && xmurmur3_hasher_update(source, "abc", 3));
    strcpy(file_hash, xmurmur3_file128_hex(test_file, 0, 42, NULL));
    file32 = xmurmur3_file32(test_file, 0, 42, NULL);
    assert(file_hash[0] && xrtGetError() == NULL && xrtTempClear() && xrtTempTrim(xrtTempCurrent(), 0));
    xrtMemDebugSnapshot(&before);
    for (kind = 0; kind < 10; ++kind) {
        unsigned bad = 0, good = 0;
        bool ordinary = kind != 1 && kind != 3 && kind != 8;
        for (i = 0; i < 12; ++i) {
            bool ok, injected;
            const char* text = NULL;
            xbuffer* bytes = NULL;
            void* handle = NULL;
            long long number = 0;
            if (ordinary) assert(xrtMemDebugFailAfter(i));
            else { backing_failed = false; fail_after = (int)i; }
            if (kind == 0 || kind == 6) {
                handle = kind == 0 ? xmurmur3_hasher_create(42, 0) : xmurmur3_hasher_clone(source);
                ok = handle != NULL;
            } else if (kind == 4 || kind == 5 || kind == 7) {
                bytes = kind == 4 ? xmurmur3_hash128_str_bytes("abc", 3, 42) :
                    kind == 5 ? xmurmur3_hash32_str_bytes("abc", 3, 0) : xmurmur3_hasher_digest_bytes(source);
                ok = bytes != NULL;
            } else if (kind == 9) {
                number = xmurmur3_file32(test_file, 0, 42, NULL);
                ok = xrtGetError() == NULL;
            } else {
                text = kind == 1 ? xmurmur3_hash128_str_hex("abc", 3, 42) :
                    kind == 8 ? xmurmur3_hasher_digest_hex(source) : xmurmur3_file128_hex(test_file, 0, 42, NULL);
                ok = text[0] != 0;
            }
            injected = ordinary ? xrtMemDebugFailTriggered() : backing_failed;
            xrtMemDebugFailClear();
            fail_after = -1;
            if (ok) {
                assert(!injected && xrtGetError() == NULL);
                if (text) assert(strcmp(text, kind == 2 || kind == 3 ? file_hash : abc128) == 0);
                if (bytes) check_buffer(bytes, kind == 5 ? abc32bytes : abc128bytes, kind == 5 ? 4 : 16);
                if (kind == 9) assert(number == file32);
                if (kind == 6) assert(strcmp(xmurmur3_hasher_digest_hex(handle), abc128) == 0);
                ++good; ++successes;
            } else {
                assert(injected && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                ++bad; ++failures;
            }
            xmurmur3_hasher_destroy(handle);
            xrtClearError();
            assert(strcmp(xmurmur3_hasher_digest_hex(source), abc128) == 0);
            assert(xrtTempClear() && xrtTempTrim(xrtTempCurrent(), 0));
            xrtMemDebugSnapshot(&after);
            assert(after.LiveBytes == before.LiveBytes && after.LiveCount == before.LiveCount);
            assert(after.InvalidFreeCount == before.InvalidFreeCount && after.DoubleFreeCount == before.DoubleFreeCount);
            assert(after.OverflowCount == before.OverflowCount && after.UnderflowCount == before.UnderflowCount);
        }
        assert(bad && good);
    }
    xmurmur3_hasher_destroy(source);
    printf("murmur3 allocations: rejected=%u completed=%u\n", failures, successes);
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
    boundaries(); allocations();
    xrtClearError();
    xrtTempUnit(xrtTempCurrent());
    puts("murmur3 native: all 25 bindings, 4 threads and temporary/Buffer lifetimes passed");
    return 0;
}
