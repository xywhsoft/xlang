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
#ifndef XRT_MODULE_IO_FILE
#define XRT_MODULE_IO_FILE
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#endif
#include "xruntime.h"
#include "../include/xrt_sha3.h"
#include "../include/sha3_native.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char* abc = "3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532";
static const char* test_file = "release/extlibs/sha3/xlang.package.json";

// ----------------------------- 普通内存与临时区的独立故障注入 -----------------------------
static xallocator backing;
static int fail_after = -1;
static bool backing_failed;
static bool reject(void)
{
    if (fail_after < 0 || fail_after-- != 0) return false;
    backing_failed = true; return true;
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
    assert(buffer); view = xrtBufferView(buffer);
    assert(view.Size == size && (size == 0 || memcmp(view.Data, expected, size) == 0));
    xrtBufferDestroy(buffer);
}

// ----------------------------- 并发独立对象与临时文本生命周期 -----------------------------
static xatomic32 ready;
static int32 worker(ptr unused)
{
    unsigned i, j;
    void* h;
    (void)unused;
    xrtAtomic32FetchAdd(&ready, 1, XMEMORY_SEQ_CST);
    while (xrtAtomic32Load(&ready, XMEMORY_SEQ_CST) != 4) xrtThreadYield();
    h = xsha3_hasher_create(256, 128); assert(h);
    for (i = 0; i < 200; ++i) {
        const char* saved = xsha3_hash_str_hex("abc", 3, 256);
        assert(!strcmp(saved, abc));
        for (j = 0; j < 16; ++j) {
            assert(strlen(xshake_hash_str_hex("other", 5, 128, 257)) == 514);
            xrtThreadYield(); assert(!strcmp(saved, abc));
        }
        assert(!xsha3_hash_hex(NULL, 1, 256)[0]);
        xrtThreadYield(); assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
        assert(xsha3_hasher_reset(h) && xsha3_hasher_update_str(h, "abc", 3));
        assert(!strcmp(xsha3_hasher_digest_hex(h, 32), abc));
        assert(!xrtGetError() && xrtTempClear());
    }
    xsha3_hasher_destroy(h); xrtTempUnit(xrtTempCurrent()); return 0;
}
static void boundaries(void)
{
    uint8 fixed[32], shake[257];
    void *h, *copy;
    xvalue* bad_callback;
    assert(xsha3_available() && !strcmp(xsha3_version(), "1.0.0"));
    assert(xrtSha3Hash("abc", 3, 256, fixed, 32, NULL));
    assert(xrtShakeHash("abc", 3, 128, shake, 257, NULL));
    assert(!strcmp(xsha3_hash_hex("abc", 3, 256), abc));
    assert(!strcmp(xsha3_hash_str_hex("abc", 3, 256), abc));
    check_buffer(xsha3_hash_bytes("abc", 3, 256), fixed, 32);
    check_buffer(xsha3_hash_str_bytes("abc", 3, 256), fixed, 32);
    check_buffer(xshake_hash_bytes("abc", 3, 128, 257), shake, 257);
    check_buffer(xshake_hash_str_bytes("abc", 3, 128, 257), shake, 257);
    assert(strlen(xshake_hash_hex("abc", 3, 128, 257)) == 514);
    assert(strlen(xshake_hash_str_hex("abc", 3, 128, 257)) == 514);
    {
        char* owned_fixed = (char*)xsha3_hash_str_hex_owned("abc", 3, 256);
        char* owned_xof = (char*)xshake_hash_str_hex_owned("abc", 3, 128, 65536);
        char* empty = (char*)xshake_hash_hex_owned(NULL, 0, 128, 0);
        assert(owned_fixed && !strcmp(owned_fixed, abc));
        assert(owned_xof && strlen(owned_xof) == 131072u);
        assert(empty && empty[0] == '\0');
        assert(xrtTempClear() && !strcmp(owned_fixed, abc) && strlen(owned_xof) == 131072u);
        xrtFree(owned_fixed); xrtFree(owned_xof); xrtFree(empty);
        assert(!xshake_hash_str_hex_owned("abc", 3, 128, SIZE_MAX) &&
            xrtErrorKind(xrtGetError()) == XERR_RANGE);
        xrtClearError();
    }
    check_buffer(xshake_hash_bytes(NULL, 0, 128, 0), NULL, 0);
    assert(!xshake_hash_hex(NULL, 0, 128, 0)[0] && !xrtGetError());
    assert(!xshake_hash_bytes(NULL, 1, 128, 0) && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(!xsha3_hash_hex(NULL, 1, 256)[0] && xsha3_last_error()[0]);
    assert(!xsha3_hash_bytes("abc", 3, 255));
    assert(!xshake_hash_hex("abc", 3, 256, SIZE_MAX)[0] && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    assert(!xshake_hash_bytes("abc", 3, 256, 536870913) && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    assert(!xsha3_file_hex(NULL, 0, 256, NULL)[0] && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(!xsha3_file_bytes("", 0, 256, NULL));
    assert(!xsha3_file_hex("__sha3_missing__", 0, 256, NULL)[0] && xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND);
    assert(!xsha3_file_bytes(test_file, 1, 256, NULL) && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    bad_callback = xrtValueInt(1); assert(bad_callback);
    assert(!xsha3_file_hex(test_file, 0, 256, bad_callback)[0] && xrtGetError());
    xrtValueRelease(bad_callback);
    assert(!xsha3_hasher_adopt(NULL) && xrtErrorKind(xrtGetError()) == XERR_STATE);
    assert(!xsha3_hasher_update(NULL, NULL, 0) && !xsha3_hasher_reset(NULL));
    assert(!xsha3_hasher_digest_hex(NULL, 32)[0] && !xsha3_hasher_digest_bytes(NULL, 32));
    assert(!xsha3_hasher_clone(NULL) && !xsha3_hasher_input_bytes(NULL) && !xsha3_hasher_out_bytes(NULL));
    xsha3_hasher_destroy(NULL);
    assert(!xsha3_shake_hasher_create(224, 0));
    h = xsha3_hasher_create(256, 3); assert(h && xsha3_hasher_adopt(h) == h);
    assert(xsha3_hasher_out_bytes(h) == 32 && xsha3_hasher_update_str(h, "a", 1));
    copy = xsha3_hasher_clone(h); assert(copy && xsha3_hasher_update(copy, "bc", 2));
    assert(xsha3_hasher_input_bytes(h) == 1 && xsha3_hasher_input_bytes(copy) == 3);
    check_buffer(xsha3_hasher_digest_bytes(copy, 32), fixed, 32);
    {
        char* owned = (char*)xsha3_hasher_digest_hex_owned(copy, 32);
        assert(owned && !strcmp(owned, abc));
        assert(xrtTempClear() && !strcmp(owned, abc));
        xrtFree(owned);
    }
    assert(!xsha3_hasher_digest_hex(copy, 64)[0] && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(!strcmp(xsha3_hasher_digest_hex(copy, 32), abc));
    assert(!xsha3_hasher_update(copy, "d", 1) && !xsha3_hasher_clone(copy));
    assert(xsha3_hasher_reset(copy) && xsha3_hasher_update(copy, "abc", 3));
    check_buffer(xsha3_hasher_digest_bytes(copy, 32), fixed, 32);
    xsha3_hasher_destroy(copy); xsha3_hasher_destroy(h);
    h = xsha3_shake_hasher_create(128, 0);
    assert(h && !xsha3_hasher_out_bytes(h) && xsha3_hasher_update_str(h, "abc", 3));
    check_buffer(xsha3_hasher_digest_bytes(h, 257), shake, 257);
    check_buffer(xsha3_hasher_digest_bytes(h, 0), NULL, 0);
    assert(!xsha3_hasher_digest_hex(h, 0)[0] && !xrtGetError());
    assert(!xsha3_hasher_digest_bytes(h, SIZE_MAX) && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xsha3_hasher_destroy(h);
}
static void allocations(void)
{
    xmemdebugsnapshot before, after;
    unsigned kind, i, failures = 0, successes = 0;
    char file_hash[65];
    uint8 fixed[32], shake[257];
    void* source = xsha3_hasher_create(256, 0);
    assert(source && xsha3_hasher_update(source, "abc", 3));
    assert(xrtSha3Hash("abc", 3, 256, fixed, 32, NULL) && xrtShakeHash("abc", 3, 128, shake, 257, NULL));
    strcpy(file_hash, xsha3_file_hex(test_file, 0, 256, NULL));
    assert(file_hash[0] && !xrtGetError() && xrtTempClear() && xrtTempTrim(xrtTempCurrent(), 0));
    xrtMemDebugSnapshot(&before);
    for (kind = 0; kind < 11; ++kind) {
        unsigned bad = 0, good = 0;
        bool ordinary = kind != 1 && kind != 3 && kind != 8;
        for (i = 0; i < 16; ++i) {
            bool ok, injected;
            const char* text = NULL;
            xbuffer* bytes = NULL;
            void* handle = NULL;
            if (ordinary) assert(xrtMemDebugFailAfter(i));
            else { backing_failed = false; fail_after = (int)i; }
            switch (kind) {
            case 0: handle = xsha3_hasher_create(256, 0); ok = handle != NULL; break;
            case 6: handle = xsha3_hasher_clone(source); ok = handle != NULL; break;
            case 4: bytes = xsha3_hash_str_bytes("abc", 3, 256); ok = bytes != NULL; break;
            case 5: bytes = xshake_hash_str_bytes("abc", 3, 128, 257); ok = bytes != NULL; break;
            case 7: bytes = xsha3_hasher_digest_bytes(source, 32); ok = bytes != NULL; break;
            case 9: bytes = xsha3_file_bytes(test_file, 0, 256, NULL); ok = bytes != NULL; break;
            case 10: handle = xsha3_shake_hasher_create(128, 0); ok = handle != NULL; break;
            default:
                text = kind == 1 ? xsha3_hash_str_hex("abc", 3, 256) :
                    kind == 8 ? xsha3_hasher_digest_hex(source, 32) : xsha3_file_hex(test_file, 0, 256, NULL);
                ok = text[0] != 0; break;
            }
            injected = ordinary ? xrtMemDebugFailTriggered() : backing_failed;
            xrtMemDebugFailClear(); fail_after = -1;
            if (ok) {
                assert(!injected && !xrtGetError());
                if (text) assert(!strcmp(text, kind == 2 || kind == 3 ? file_hash : abc));
                if (bytes) {
                    if (kind == 9) { assert(xrtBufferView(bytes).Size == 32); xrtBufferDestroy(bytes); }
                    else check_buffer(bytes, kind == 5 ? shake : fixed, kind == 5 ? 257 : 32);
                }
                ++good; ++successes;
            } else { assert(injected && xrtErrorKind(xrtGetError()) == XERR_MEMORY); ++bad; ++failures; }
            xsha3_hasher_destroy(handle); xrtClearError();
            assert(!strcmp(xsha3_hasher_digest_hex(source, 32), abc));
            assert(xrtTempClear() && xrtTempTrim(xrtTempCurrent(), 0));
            xrtMemDebugSnapshot(&after);
            assert(after.LiveBytes == before.LiveBytes && after.LiveCount == before.LiveCount);
            assert(after.InvalidFreeCount == before.InvalidFreeCount && after.DoubleFreeCount == before.DoubleFreeCount);
            assert(after.OverflowCount == before.OverflowCount && after.UnderflowCount == before.UnderflowCount);
        }
        assert(bad && good);
    }
    xsha3_hasher_destroy(source);
    printf("sha3 allocations: rejected=%u completed=%u\n", failures, successes);
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
    boundaries(); allocations(); xrtClearError(); xrtTempUnit(xrtTempCurrent());
    puts("sha3 native: 30 bindings, 4 threads and owned/temporary/Buffer lifetimes passed");
    return 0;
}
