#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#ifndef XRT_MODULE_TEMP_MEMORY
#define XRT_MODULE_TEMP_MEMORY
#endif
#ifndef XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_BUFFER
#endif
#include "../include/blake3_native.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char key[] = "whats the Elvish word for friend";
static xallocator backing;
static int fail_after = -1;
static bool backing_failed;
static void* source;
static char expected[3][263];
static const char* input_file = "release/extlibs/blake3/test/test_vectors.json";
static char expected_file[65];

/* 临时区使用 backing allocator，不遗漏普通堆门禁之外的分配。 */
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

static bool operation(unsigned int kind)
{
    void* h = NULL;
    xbuffer* bytes = NULL;
    const char* text;
    unsigned int mode = 0;
    if (kind == 0) h = xblake3_hasher_create(128);
    else if (kind == 1) h = xblake3_hasher_create_keyed((void*)key, 32, 128);
    else if (kind == 2) h = xblake3_hasher_create_kdf("test", 4, 128);
    else if (kind == 3) h = xblake3_hasher_clone(source);
    else if (kind == 4) bytes = xblake3_hash_str_bytes("abc", 3, 131);
    else if (kind == 5) bytes = xblake3_hasher_digest_bytes(source, 131);
    else if (kind >= 10) {
        text = xblake3_file_hex(input_file, 1048576, 32, NULL);
        if (!text[0]) return false;
        assert(strcmp(text, expected_file) == 0);
        return true;
    }
    else {
        if (kind == 6) text = xblake3_hash_str_hex("abc", 3, 131);
        else if (kind == 7) { mode = 1; text = xblake3_hash_mac_str_hex("abc", 3, key, 32, 131); }
        else if (kind == 8) { mode = 2; text = xblake3_hash_kdf_str_hex("abc", 3, "test", 4, 131); }
        else text = xblake3_hasher_digest_hex(source, 131);
        if (!text[0]) return false;
        assert(strcmp(text, expected[mode]) == 0);
        return true;
    }
    if (kind < 4) { bool ok = h != NULL; xblake3_hasher_destroy(h); return ok; }
    { bool ok = bytes != NULL; xrtBufferDestroy(bytes); return ok; }
}

int main(void)
{
    xallocator allocator = {NULL, allocate, reallocate, release};
    xmemdebugsnapshot before, after;
    unsigned int kind, index, failures = 0, successes = 0;
    xrtGetAllocator(&backing);
    assert(xrtSetAllocator(&allocator) && xrtMemDebugEnable(true));
    source = xblake3_hasher_create(128);
    assert(source && xblake3_hasher_update_str(source, "abc", 3));
    strcpy(expected[0], xblake3_hasher_digest_hex(source, 131));
    strcpy(expected[1], xblake3_hash_mac_str_hex("abc", 3, key, 32, 131));
    strcpy(expected[2], xblake3_hash_kdf_str_hex("abc", 3, "test", 4, 131));
    strcpy(expected_file, xblake3_file_hex(input_file, 1048576, 32, NULL));
    assert(strlen(expected_file) == 64);
    assert(xrtTempClear() && xrtTempTrim(xrtTempCurrent(), 0));
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    for (kind = 0; kind < 12; ++kind) {
        unsigned int good = 0, bad = 0;
        bool ordinary = kind < 6 || kind == 10;
        for (index = 0; index < 12; ++index) {
            bool ok, injected;
            if (ordinary) assert(xrtMemDebugFailAfter(index));
            else { backing_failed = false; fail_after = (int)index; }
            ok = operation(kind);
            injected = ordinary ? xrtMemDebugFailTriggered() : backing_failed;
            xrtMemDebugFailClear();
            fail_after = -1;
            if (ok) { assert(!injected && xrtGetError() == NULL); ++good; ++successes; }
            else { assert(injected && xrtErrorKind(xrtGetError()) == XERR_MEMORY); ++bad; ++failures; }
            xrtClearError();
            assert(xblake3_hasher_input_bytes(source) == 3);
            assert(strcmp(xblake3_hasher_digest_hex(source, 131), expected[0]) == 0);
            assert(xrtTempClear() && xrtTempTrim(xrtTempCurrent(), 0));
            xrtMemDebugSnapshot(&after);
            assert(after.LiveBytes == before.LiveBytes && after.LiveCount == before.LiveCount);
            assert(after.InvalidFreeCount == before.InvalidFreeCount && after.DoubleFreeCount == before.DoubleFreeCount);
            assert(after.OverflowCount == before.OverflowCount && after.UnderflowCount == before.UnderflowCount);
        }
        assert(good && bad);
    }
    xblake3_hasher_destroy(source);
    xrtClearError();
    xrtTempUnit(xrtTempCurrent());
    printf("blake3 allocation: rejected=%u completed=%u\n", failures, successes);
    return 0;
}
