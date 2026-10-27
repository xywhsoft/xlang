#ifndef XRT_MODULE_THREAD
#define XRT_MODULE_THREAD
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

static void expect_text(char* text, const char* expected)
{
    assert(text && strcmp(text, expected) == 0);
    xrtFree(text);
}

static void boundaries(void)
{
    void* h;
    char *text, *other;
    assert(xxxhash_available() && strcmp(xxxhash_version(), "0.8.3") == 0);
    assert(xxxhash_hash64(NULL, 0, 0) == UINT64_C(0xef46db3751d8e999));
    assert(xxxhash_hash32(NULL, 0, 0) == 0x02cc5d05U);
    assert(xxxhash_hash3_64(NULL, 0, 0) == UINT64_C(0x2d06800538d394c2));
    expect_text(xxxhash_hash3_64_str_hex("abc", 3, 0), "78af5f94892f3950");
    expect_text(xxxhash_hash3_128_hex(NULL, 0, 0), "99aa06d3014798d86001c324468d497f");
    assert(xxxhash_hash3_64_str("a\0b", 3, 1) ==
        xxxhash_hash3_64((void*)"a\0b", 3, 1));
    text = xxxhash_hash3_128_str_hex("a\0b", 3, 1);
    other = xxxhash_hash3_128_hex((void*)"a\0b", 3, 1);
    assert(text && other && text != other && strcmp(text, other) == 0);
    xrtFree(text);
    xrtFree(other);
    assert(xxxhash_hash64_hex(NULL, 1, 0) == NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(xxxhash_hash32_hex(NULL, 1, 0) == NULL && xxxhash_last_error()[0]);
    assert(xxxhash_hash3_64_hex(NULL, 1, 0) == NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(xxxhash_hash3_128_hex(NULL, 1, 0) == NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(xxxhash_file64_hex(NULL, 0, 0, NULL) == NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(xxxhash_file64_hex("", 0, 0, NULL) == NULL);
    assert(xxxhash_file64_hex("__xxhash_missing_file__", 0, 0, NULL) == NULL && xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND);
    assert(!xxxhash_hasher_update(NULL, NULL, 0) && xrtErrorKind(xrtGetError()) == XERR_STATE);
    assert(xxxhash_hasher_digest_hex(NULL) == NULL && !xxxhash_hasher_reset(NULL));
    xrtClearError();
    assert(xxxhash_hasher_input_bytes(NULL) == 0 && xrtErrorKind(xrtGetError()) == XERR_STATE);
    xxxhash_hasher_destroy(NULL);
    h = xxxhash_hasher_create(UINT64_MAX, 3);
    assert(h && xxxhash_hasher_update_str(h, "abc", 3));
    assert(xxxhash_hasher_digest(h) == xxxhash_hash64_str("abc", 3, UINT64_MAX));
    text = xxxhash_hasher_digest_hex(h);
    other = xxxhash_hash64_str_hex("abc", 3, UINT64_MAX);
    assert(text && other && strcmp(text, other) == 0);
    assert(!xxxhash_hasher_update_str(h, "x", 1) && xxxhash_hasher_input_bytes(h) == 3);
    assert(xrtGetError() == NULL);
    assert(xxxhash_hasher_reset(h) && xxxhash_hasher_update(h, NULL, 0));
    xxxhash_hasher_destroy(h);
    /* 摘要不借用 hasher；对象销毁后仍可读取并分别释放。 */
    assert(strcmp(text, other) == 0);
    xrtFree(text);
    xrtFree(other);
    assert(xxxhash_hash32_str("abc", 3, UINT32_MAX) == xxxhash_hash32((void*)"abc", 3, UINT32_MAX));
    expect_text(xxxhash_hash32_str_hex("abc", 3, 0), "32d153ff");
    assert(xrtGetError() == NULL);
}

static char* hex_result(unsigned int kind, const char* path, void* hasher)
{
    switch (kind) {
    case 0: return xxxhash_hash64_hex(NULL, 0, 0);
    case 1: return xxxhash_hash64_str_hex("abc", 3, 0);
    case 2: return xxxhash_hash32_hex(NULL, 0, 0);
    case 3: return xxxhash_hash32_str_hex("abc", 3, 0);
    case 4: return xxxhash_hash3_64_hex(NULL, 0, 0);
    case 5: return xxxhash_hash3_64_str_hex("abc", 3, 0);
    case 6: return xxxhash_hash3_128_hex(NULL, 0, 0);
    case 7: return xxxhash_hash3_128_str_hex("abc", 3, 0);
    case 8: return xxxhash_file64_hex(path, 0, 0, NULL);
    default: return xxxhash_hasher_digest_hex(hasher);
    }
}

static void allocations(void)
{
    xmemdebugsnapshot before, after;
    unsigned int kind, i, failures = 0, successes = 0;
    char file_hash[17];
    const char* path = "release/extlibs/xxhash/test/vectors.json";
    const char* expected[] = { "ef46db3751d8e999", "44bc2cf5ad770999",
        "02cc5d05", "32d153ff", "2d06800538d394c2", "78af5f94892f3950",
        "99aa06d3014798d86001c324468d497f", "06b05ab6733a618578af5f94892f3950",
        file_hash, "44bc2cf5ad770999" };
    void* hasher = xxxhash_hasher_create(0, 0);
    char* initial = xxxhash_file64_hex(path, 0, 0, NULL);
    assert(hasher && xxxhash_hasher_update_str(hasher, "abc", 3) && initial);
    strcpy(file_hash, initial);
    xrtFree(initial);
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    for (kind = 0; kind < 11; ++kind) {
        unsigned int bad = 0, good = 0;
        for (i = 0; i < 32; ++i) {
            bool ok, injected;
            char* text = NULL;
            assert(xrtMemDebugFailAfter(i));
            if (kind == 0) {
                void* h = xxxhash_hasher_create(0, 0);
                ok = h != NULL;
                xxxhash_hasher_destroy(h);
            } else {
                text = hex_result(kind - 1, path, hasher);
                ok = text != NULL;
            }
            injected = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (ok) {
                assert(!injected && xrtGetError() == NULL);
                if (kind > 0) assert(strcmp(text, expected[kind - 1]) == 0);
                ++good; ++successes;
            } else {
                assert(injected && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                ++bad; ++failures;
            }
            xrtFree(text);
            xrtClearError();
            expect_text(xxxhash_hash64_str_hex("abc", 3, 0), "44bc2cf5ad770999");
            xrtMemDebugSnapshot(&after);
            assert(after.LiveBytes == before.LiveBytes && after.LiveCount == before.LiveCount);
            assert(after.InvalidFreeCount == before.InvalidFreeCount && after.DoubleFreeCount == before.DoubleFreeCount);
            assert(after.OverflowCount == before.OverflowCount && after.UnderflowCount == before.UnderflowCount);
        }
        assert(bad && good);
    }
    xxxhash_hasher_destroy(hasher);
    printf("xxhash allocations: rejected=%u completed=%u\n", failures, successes);
}

typedef struct worker { char text[24], expected[17]; char* retained; } worker;
static int32 run_worker(ptr data)
{
    worker* w = data;
    unsigned int i, j;
    void* h = xxxhash_hasher_create(0, 128);
    assert(h);
    for (i = 0; i < 200; ++i) {
        char* saved = xxxhash_hash64_str_hex(w->text, strlen(w->text), 0);
        assert(saved);
        for (j = 0; j < 16; ++j) {
            char* other = xxxhash_hash32_str_hex("other", 5, j);
            assert(other);
            xrtFree(other);
            xrtThreadYield();
            assert(strcmp(saved, w->expected) == 0);
        }
        assert(xxxhash_hash64_hex(NULL, 1, 0) == NULL);
        xrtThreadYield();
        assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
        assert(xxxhash_hasher_reset(h) && xxxhash_hasher_update_str(h, w->text, strlen(w->text)));
        expect_text(xxxhash_hasher_digest_hex(h), w->expected);
        assert(xrtGetError() == NULL);
        xrtFree(saved);
    }
    w->retained = xxxhash_hasher_digest_hex(h);
    assert(w->retained);
    xxxhash_hasher_destroy(h);
    return 0;
}

int main(void)
{
    worker workers[4];
    xthread* threads[4];
    size_t i;
    assert(xrtMemDebugEnable(true));
    boundaries();
    allocations();
    for (i = 0; i < 4; ++i) {
        snprintf(workers[i].text, sizeof(workers[i].text), "worker-%u", (unsigned)i);
        char* expected;
        expected = xxxhash_hash64_str_hex(workers[i].text, strlen(workers[i].text), 0);
        assert(expected);
        strcpy(workers[i].expected, expected);
        xrtFree(expected);
        threads[i] = xrtThreadCreate(run_worker, &workers[i], 0);
        assert(threads[i]);
    }
    for (i = 0; i < 4; ++i) {
        assert(xrtThreadWait(threads[i]) == XWAIT_OK && xrtThreadExitCode(threads[i]) == 0);
        xrtThreadDestroy(threads[i]);
        assert(strcmp(workers[i].retained, workers[i].expected) == 0);
        xrtFree(workers[i].retained);
    }
    xrtClearError();
    puts("xxhash native: 4 independent threads and owned lifetimes passed");
    return 0;
}
