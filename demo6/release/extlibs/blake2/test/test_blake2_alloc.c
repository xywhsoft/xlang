#define XRT_MODULE_MEMORY_DEBUG
#ifndef XRT_MODULE_TEMP_MEMORY
#define XRT_MODULE_TEMP_MEMORY
#endif
#ifndef XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_BUFFER
#endif
#include "../include/blake2_native.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

static xallocator backing;
static int fail_after = -1;
static bool backing_failed;
static void* source[2];
static char expected[2][129];

/* 临时区不经过普通堆故障门，另从 backing allocator 注入。 */
static bool reject(void)
{
    if (fail_after < 0 || fail_after-- != 0) return false;
    backing_failed = true;
    return true;
}
static ptr allocate(ptr context, size_t size)
{
    (void)context;
    return reject() ? NULL : backing.Alloc(backing.Context, size);
}
static ptr reallocate(ptr context, ptr memory, size_t size)
{
    (void)context;
    return reject() ? NULL : backing.Realloc(backing.Context, memory, size);
}
static void release(ptr context, ptr memory)
{
    (void)context;
    backing.Free(backing.Context, memory);
}

static bool operation(unsigned int algorithm, unsigned int kind)
{
    void* object = NULL;
    xbuffer* bytes = NULL;
    const char* text;
    if (kind == 0) object = algorithm ? xblake2s_hasher_create(32, 128) : xblake2b_hasher_create(64, 128);
    else if (kind == 1) object = algorithm ? xblake2s_hasher_clone(source[1]) : xblake2b_hasher_clone(source[0]);
    else if (kind == 2) bytes = algorithm ? xblake2s_hash_str_bytes("abc", 3, 32) : xblake2b_hash_str_bytes("abc", 3, 64);
    else if (kind == 3) bytes = algorithm ? xblake2s_hasher_digest_bytes(source[1]) : xblake2b_hasher_digest_bytes(source[0]);
    else {
        if (kind == 4) text = algorithm ? xblake2s_hash_str_hex("abc", 3, 32, "", 0) : xblake2b_hash_str_hex("abc", 3, 64, "", 0);
        else text = algorithm ? xblake2s_hasher_digest_hex(source[1]) : xblake2b_hasher_digest_hex(source[0]);
        if (text[0] == 0) return false;
        assert(strcmp(text, expected[algorithm]) == 0);
        return true;
    }
    if (kind < 2) {
        bool ok = object != NULL;
        if (algorithm) xblake2s_hasher_destroy(object);
        else xblake2b_hasher_destroy(object);
        return ok;
    }
    {
        bool ok = bytes != NULL;
        xrtBufferDestroy(bytes);
        return ok;
    }
}

int main(void)
{
    xallocator allocator = {NULL, allocate, reallocate, release};
    xmemdebugsnapshot before, after;
    unsigned int algorithm, mode, kind, index, failures = 0, successes = 0;
    xrtGetAllocator(&backing);
    assert(xrtSetAllocator(&allocator) && xrtMemDebugEnable(true));
    source[0] = xblake2b_hasher_create(64, 128);
    source[1] = xblake2s_hasher_create(32, 128);
    assert(source[0] && source[1]);
    assert(xblake2b_hasher_update_str(source[0], "abc", 3));
    assert(xblake2s_hasher_update_str(source[1], "abc", 3));
    strcpy(expected[0], xblake2b_hasher_digest_hex(source[0]));
    strcpy(expected[1], xblake2s_hasher_digest_hex(source[1]));
    assert(xrtTempClear() && xrtTempTrim(xrtTempCurrent(), 0));
    xrtClearError();
    xrtMemDebugSnapshot(&before);

    for (algorithm = 0; algorithm < 2; ++algorithm) {
      for (mode = 0; mode < 2; ++mode) {
        for (kind = mode ? 4 : 0; kind < (mode ? 6u : 4u); ++kind) {
            unsigned int good = 0, bad = 0;
            for (index = 0; index < 12; ++index) {
                bool ok, injected;
                if (mode == 0) assert(xrtMemDebugFailAfter(index));
                else { backing_failed = false; fail_after = (int)index; }
                ok = operation(algorithm, kind);
                injected = mode == 0 ? xrtMemDebugFailTriggered() : backing_failed;
                xrtMemDebugFailClear();
                fail_after = -1;
                if (ok) { assert(!injected && xrtGetError() == NULL); ++good; ++successes; }
                else {
                    assert(injected && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                    ++bad;
                    ++failures;
                }
                xrtClearError();
                assert(xblake2b_hasher_input_bytes(source[0]) == 3);
                assert(xblake2s_hasher_input_bytes(source[1]) == 3);
                assert(strcmp(xblake2b_hasher_digest_hex(source[0]), expected[0]) == 0);
                assert(strcmp(xblake2s_hasher_digest_hex(source[1]), expected[1]) == 0);
                assert(xrtTempClear() && xrtTempTrim(xrtTempCurrent(), 0));
                xrtMemDebugSnapshot(&after);
                assert(after.LiveBytes == before.LiveBytes && after.LiveCount == before.LiveCount);
                assert(after.InvalidFreeCount == before.InvalidFreeCount && after.DoubleFreeCount == before.DoubleFreeCount);
                assert(after.OverflowCount == before.OverflowCount && after.UnderflowCount == before.UnderflowCount);
            }
            if (!good || !bad) fprintf(stderr, "algorithm=%u mode=%u kind=%u good=%u bad=%u\n", algorithm, mode, kind, good, bad);
            assert(good && bad);
        }
      }
    }
    xblake2b_hasher_destroy(source[0]);
    xblake2s_hasher_destroy(source[1]);
    xrtClearError();
    xrtTempUnit(xrtTempCurrent());
    printf("blake2 allocation: rejected=%u completed=%u\n", failures, successes);
    return 0;
}
