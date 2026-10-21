#ifndef XRT_MODULE_THREAD
#define XRT_MODULE_THREAD
#endif
#ifndef XRT_MODULE_TEMP_MEMORY
#define XRT_MODULE_TEMP_MEMORY
#endif
#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#include "../include/xxhash_native.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
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

static void boundaries(void)
{
    void* h;
    assert(xxxhash_available() && strcmp(xxxhash_version(), "0.8.3") == 0);
    assert(xxxhash_hash64(NULL, 0, 0) == UINT64_C(0xef46db3751d8e999));
    assert(xxxhash_hash32(NULL, 0, 0) == 0x02cc5d05U);
    assert(!xxxhash_hash64_hex(NULL, 1, 0)[0] && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(!xxxhash_hash32_hex(NULL, 1, 0)[0] && xxxhash_last_error()[0]);
    assert(!xxxhash_file64_hex(NULL, 0, 0, NULL)[0] && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(!xxxhash_file64_hex("", 0, 0, NULL)[0]);
    assert(!xxxhash_file64_hex("__xxhash_missing_file__", 0, 0, NULL)[0] && xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND);
    assert(!xxxhash_hasher_update(NULL, NULL, 0) && xrtErrorKind(xrtGetError()) == XERR_STATE);
    assert(!xxxhash_hasher_digest_hex(NULL)[0] && !xxxhash_hasher_reset(NULL));
    xrtClearError();
    assert(xxxhash_hasher_input_bytes(NULL) == 0 && xrtErrorKind(xrtGetError()) == XERR_STATE);
    xxxhash_hasher_destroy(NULL);
    h = xxxhash_hasher_create(UINT64_MAX, 3);
    assert(h && xxxhash_hasher_update_str(h, "abc", 3));
    assert(xxxhash_hasher_digest(h) == xxxhash_hash64_str("abc", 3, UINT64_MAX));
    assert(!xxxhash_hasher_update_str(h, "x", 1) && xxxhash_hasher_input_bytes(h) == 3);
    assert(xrtGetError() == NULL);
    assert(xxxhash_hasher_reset(h) && xxxhash_hasher_update(h, NULL, 0));
    xxxhash_hasher_destroy(h);
    assert(xxxhash_hash32_str("abc", 3, UINT32_MAX) == xxxhash_hash32((void*)"abc", 3, UINT32_MAX));
    assert(strcmp(xxxhash_hash32_str_hex("abc", 3, 0), "32d153ff") == 0);
    assert(xrtGetError() == NULL);
}

static void allocations(void)
{
    xmemdebugsnapshot before, after;
    unsigned int kind, i, failures = 0, successes = 0;
    char file_hash[17];
    const char* path = "release/extlibs/xxhash/test/vectors.json";
    strcpy(file_hash, xxxhash_file64_hex(path, 0, 0, NULL));
    assert(file_hash[0] && xrtTempClear() && xrtTempTrim(xrtTempCurrent(), 0));
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    for (kind = 0; kind < 5; ++kind) {
        unsigned int bad = 0, good = 0;
        bool ordinary = kind == 0 || kind == 3;
        for (i = 0; i < 12; ++i) {
            bool ok, injected;
            const char* text = NULL;
            if (ordinary) assert(xrtMemDebugFailAfter(i));
            else { backing_failed = false; fail_after = (int)i; }
            if (kind == 0) {
                void* h = xxxhash_hasher_create(0, 0);
                ok = h != NULL;
                xxxhash_hasher_destroy(h);
            } else {
                if (kind == 1) text = xxxhash_hash64_str_hex("abc", 3, 0);
                else if (kind == 2) text = xxxhash_hash32_str_hex("abc", 3, 0);
                else text = xxxhash_file64_hex(path, 0, 0, NULL);
                ok = text[0] != 0;
            }
            injected = ordinary ? xrtMemDebugFailTriggered() : backing_failed;
            xrtMemDebugFailClear();
            fail_after = -1;
            if (ok) {
                assert(!injected && xrtGetError() == NULL);
                if (kind > 0) assert(strcmp(text, kind == 1 ? "44bc2cf5ad770999" : kind == 2 ? "32d153ff" : file_hash) == 0);
                ++good; ++successes;
            } else {
                assert(injected && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                ++bad; ++failures;
            }
            xrtClearError();
            assert(strcmp(xxxhash_hash64_str_hex("abc", 3, 0), "44bc2cf5ad770999") == 0);
            assert(xrtTempClear() && xrtTempTrim(xrtTempCurrent(), 0));
            xrtMemDebugSnapshot(&after);
            assert(after.LiveBytes == before.LiveBytes && after.LiveCount == before.LiveCount);
            assert(after.InvalidFreeCount == before.InvalidFreeCount && after.DoubleFreeCount == before.DoubleFreeCount);
            assert(after.OverflowCount == before.OverflowCount && after.UnderflowCount == before.UnderflowCount);
        }
        assert(bad && good);
    }
    printf("xxhash allocations: rejected=%u completed=%u\n", failures, successes);
}

typedef struct worker { char text[24], expected[17]; } worker;
static int32 run_worker(ptr data)
{
    worker* w = data;
    unsigned int i, j;
    void* h = xxxhash_hasher_create(0, 128);
    assert(h);
    for (i = 0; i < 200; ++i) {
        const char* saved = xxxhash_hash64_str_hex(w->text, strlen(w->text), 0);
        for (j = 0; j < 16; ++j) {
            assert(xxxhash_hash32_str_hex("other", 5, j)[0]);
            xrtThreadYield();
            assert(strcmp(saved, w->expected) == 0);
        }
        assert(!xxxhash_hash64_hex(NULL, 1, 0)[0]);
        xrtThreadYield();
        assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
        assert(xxxhash_hasher_reset(h) && xxxhash_hasher_update_str(h, w->text, strlen(w->text)));
        assert(strcmp(xxxhash_hasher_digest_hex(h), w->expected) == 0);
        assert(xrtGetError() == NULL && xrtTempClear());
    }
    xxxhash_hasher_destroy(h);
    xrtTempUnit(xrtTempCurrent());
    return 0;
}

int main(void)
{
    xallocator allocator = {NULL, allocate, reallocate, release};
    worker workers[4];
    xthread* threads[4];
    size_t i;
    xrtGetAllocator(&backing);
    assert(xrtSetAllocator(&allocator) && xrtMemDebugEnable(true));
    boundaries();
    allocations();
    for (i = 0; i < 4; ++i) {
        snprintf(workers[i].text, sizeof(workers[i].text), "worker-%u", (unsigned)i);
        strcpy(workers[i].expected, xxxhash_hash64_str_hex(workers[i].text, strlen(workers[i].text), 0));
        threads[i] = xrtThreadCreate(run_worker, &workers[i], 0);
        assert(threads[i]);
    }
    for (i = 0; i < 4; ++i) {
        assert(xrtThreadWait(threads[i]) == XWAIT_OK && xrtThreadExitCode(threads[i]) == 0);
        xrtThreadDestroy(threads[i]);
    }
    xrtClearError();
    xrtTempUnit(xrtTempCurrent());
    puts("xxhash native: 4 independent threads and temporary lifetimes passed");
    return 0;
}
