#define XRT_MODULE_ALL
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/lru_native.h"
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "lru C test failed at line %d: %s\n", __LINE__, #condition); \
    return 1; \
} } while (0)

static int same(void* owned, const char* expected)
{
    int result = owned != NULL && strcmp((char*)owned, expected) == 0;
    xrtFree(owned);
    return result;
}

static int test_error_recovery(void)
{
    void* cache;
    CHECK(xlru_create(0, 0) == NULL && xrtGetError() != NULL);
    cache = xlru_create(2, 0);
    CHECK(cache != NULL && xrtGetError() == NULL);
    CHECK(xlru_put(cache, NULL, "x") == 0 && xrtGetError() != NULL);
    CHECK(xlru_put(cache, "a", "one") == 1 && xrtGetError() == NULL);
    CHECK(xlru_get(cache, NULL, "") == NULL && xrtGetError() != NULL);
    CHECK(same(xlru_get(cache, "a", ""), "one") && xrtGetError() == NULL);
    CHECK(xlru_put(cache, NULL, "x") == 0 && xrtGetError() != NULL);
    CHECK(same(xlru_get(cache, "missing", "fallback"), "fallback") &&
        xrtGetError() == NULL);
    CHECK(xlru_contains(cache, NULL) == -1 && xrtGetError() != NULL);
    CHECK(xlru_contains(cache, "a") == 1 && xrtGetError() == NULL);
    CHECK(xlru_remove(cache, NULL) == -1 && xrtGetError() != NULL);
    CHECK(xlru_remove(cache, "missing") == 0 && xrtGetError() == NULL);
    CHECK(xlru_put(cache, NULL, "x") == 0 && xrtGetError() != NULL);
    CHECK(xlru_count(cache) == 1 && xrtGetError() == NULL);
    CHECK(xlru_get(cache, NULL, "") == NULL && xrtGetError() != NULL);
    CHECK(xlru_clear(cache) == 1 && xrtGetError() == NULL);
    CHECK(xlru_put(cache, "", "empty key") == 1);
    CHECK(xlru_contains(cache, "") == 1);
    CHECK(same(xlru_get(cache, "", "missing"), "empty key"));
    CHECK(xlru_remove(cache, "") == 1 && xlru_count(cache) == 0);
    xlru_destroy(cache);
    CHECK(xlru_count(NULL) == -1 && xrtGetError() != NULL);
    return 0;
}

static int test_long_keys(void)
{
    char first[8193], second[8193];
    void* cache = xlru_create(2, 0);
    char* held;

    CHECK(cache != NULL);
    memset(first, 'k', sizeof(first) - 1u);
    memset(second, 'k', sizeof(second) - 1u);
    first[8191] = 'a';
    second[8191] = 'b';
    first[8192] = second[8192] = '\0';
    CHECK(xlru_put(cache, first, "first") == 1);
    CHECK(xlru_contains(cache, second) == 0);
    held = (char*)xlru_get(cache, first, "");
    CHECK(held != NULL && strcmp(held, "first") == 0);
    CHECK(xlru_put(cache, second, "second") == 1);
    CHECK(xlru_put(cache, "third", "third") == 1);
    CHECK(xlru_contains(cache, first) == 0 && xlru_contains(cache, second) == 1);
    CHECK(same(xlru_get(cache, second, ""), "second"));
    CHECK(strcmp(held, "first") == 0);
    xrtFree(held);
    xlru_destroy(cache);
    return 0;
}

static int test_allocation_failure(void)
{
    unsigned failure;
    unsigned rejected = 0u;
    unsigned succeeded = 0u;
    for (failure = 0u; failure < 32u && succeeded < 2u; ++failure) {
        void* cache = xlru_create(1, 0);
        int64_t inserted;
        int triggered;
        CHECK(cache != NULL && xlru_put(cache, "old", "keep") == 1);
        CHECK(xrtMemDebugFailAfter(failure));
        inserted = xlru_put(cache, "new", "value");
        triggered = xrtMemDebugFailTriggered();
        CHECK(triggered ? xrtErrorKind(xrtGetError()) == XERR_MEMORY :
            xrtGetError() == NULL);
        xrtMemDebugFailClear();
        xrtClearError();
        CHECK(xlru_count(cache) == 1);
        if (triggered) {
            CHECK(inserted == 0 && xlru_contains(cache, "old") == 1);
            CHECK(xlru_contains(cache, "new") == 0);
            CHECK(same(xlru_get(cache, "old", ""), "keep"));
            ++rejected;
        } else {
            CHECK(inserted == 1 && xlru_contains(cache, "old") == 0);
            CHECK(same(xlru_get(cache, "new", ""), "value"));
            ++succeeded;
        }
        xlru_destroy(cache);
    }
    CHECK(rejected >= 3u && succeeded == 2u);
    {
        void* cache = xlru_create(1, 0);
        CHECK(cache != NULL && xlru_put(cache, "old", "keep") == 1);
        CHECK(xrtMemDebugFailAfter(0u));
        CHECK(xlru_put(cache, "old", "replace") == 0);
        CHECK(xrtMemDebugFailTriggered());
        CHECK(xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        xrtMemDebugFailClear();
        xrtClearError();
        CHECK(same(xlru_get(cache, "old", ""), "keep"));
        CHECK(xlru_hits(cache) == 1);
        CHECK(xrtMemDebugFailAfter(0u));
        CHECK(xlru_get(cache, "old", "") == NULL);
        CHECK(xrtMemDebugFailTriggered());
        CHECK(xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        xrtMemDebugFailClear();
        xrtClearError();
        CHECK(xlru_hits(cache) == 1 && xlru_misses(cache) == 0);
        xlru_destroy(cache);
    }
    {
        int mode;
        for (mode = 0; mode <= 1; ++mode) {
            void* cache = xlru_create(2, mode);
            CHECK(cache != NULL && xlru_put(cache, "old", "keep") == 1);
            CHECK(xlru_put(cache, "recent", "value") == 1);
            CHECK(xrtMemDebugFailAfter(0u));
            CHECK(xlru_put(cache, "old", "replace") == 0);
            CHECK(xrtMemDebugFailTriggered());
            CHECK(xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            xrtMemDebugFailClear();
            xrtClearError();
            CHECK(xlru_put(cache, "new", "value") == 1);
            CHECK(xlru_contains(cache, "old") == 0);
            CHECK(xlru_contains(cache, "recent") == 1);
            CHECK(xlru_contains(cache, "new") == 1);
            xlru_destroy(cache);
        }
    }
    return 0;
}

typedef struct {
    int present;
    int value;
    unsigned frequency;
    unsigned age;
} model_entry;

static int test_interleaved_operations(int mode)
{
    static const char* keys[] = {"k0", "k1", "k2", "k3", "k4"};
    model_entry model[5] = {{0}};
    void* cache = xlru_create(3, mode);
    uint32_t state = 0x7f4a7c15u;
    unsigned age = 0u;
    unsigned evictions = 0u, overwrites = 0u, clears = 0u;
    unsigned hitChecks = 0u, missChecks = 0u;
    int count = 0, hits = 0, misses = 0;
    unsigned step, i;
    CHECK(cache != NULL);
    for (step = 0u; step < 800u; ++step) {
        unsigned key, operation;
        char value[24], expected[24];
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        key = (state >> 8) % 5u;
        operation = (state >> 16) % 32u;
        if (operation < 16u) {
            int number = (int)(state >> 24);
            snprintf(value, sizeof(value), "v%d", number);
            if (!model[key].present && count == 3) {
                unsigned victim = 0u;
                for (i = 1u; i < 5u; ++i) {
                    if (!model[i].present) continue;
                    if (!model[victim].present ||
                        (mode == 1 && model[i].frequency < model[victim].frequency) ||
                        (mode == 1 && model[i].frequency == model[victim].frequency &&
                         model[i].age < model[victim].age) ||
                        (mode == 0 && model[i].age < model[victim].age)) victim = i;
                }
                model[victim].present = 0;
                --count;
                ++evictions;
            }
            CHECK(xlru_put(cache, keys[key], value) == 1);
            if (!model[key].present) {
                model[key].present = 1;
                model[key].frequency = 1u;
                ++count;
            } else {
                ++model[key].frequency;
                ++overwrites;
            }
            model[key].value = number;
            model[key].age = ++age;
        } else if (operation < 24u) {
            if (model[key].present) {
                snprintf(expected, sizeof(expected), "v%d", model[key].value);
                ++hits;
                ++hitChecks;
                ++model[key].frequency;
                model[key].age = ++age;
            } else {
                snprintf(expected, sizeof(expected), "fallback");
                ++misses;
                ++missChecks;
            }
            CHECK(same(xlru_get(cache, keys[key], "fallback"), expected));
        } else if (operation < 28u) {
            CHECK(xlru_remove(cache, keys[key]) == model[key].present);
            if (model[key].present) { model[key].present = 0; --count; }
        } else if (operation == 28u) {
            CHECK(xlru_clear(cache) == 1);
            memset(model, 0, sizeof(model));
            count = hits = misses = 0;
            ++clears;
        } else {
            CHECK(xlru_contains(cache, keys[key]) == model[key].present);
        }
        CHECK(xlru_count(cache) == count);
        CHECK(xlru_hits(cache) == hits && xlru_misses(cache) == misses);
        for (i = 0u; i < 5u; ++i)
            CHECK(xlru_contains(cache, keys[i]) == model[i].present);
        CHECK(xrtGetError() == NULL);
    }
    CHECK(evictions > 0u && overwrites > 0u && clears > 0u);
    CHECK(hitChecks > 0u && missChecks > 0u);
    xlru_destroy(cache);
    return 0;
}

int main(void)
{
    void* cache = xlru_create(1, 0);
    void* lfu = xlru_create(2, 1);
    void* tied = xlru_create(2, 1);
    char long_value[8193];
    char* held;
    int i;

    CHECK(xlru_create(0, 0) == NULL);
    CHECK(xlru_create(2, 2) == NULL);
    CHECK(cache != NULL && lfu != NULL && tied != NULL);
    memset(long_value, 'v', sizeof(long_value) - 1u);
    long_value[sizeof(long_value) - 1u] = '\0';
    CHECK(xlru_put(cache, "a", long_value) == 1);
    held = (char*)xlru_get(cache, "a", "missing");
    CHECK(held != NULL && strlen(held) == 8192u);
    CHECK(xlru_put(cache, "b", "") == 1);
    CHECK(xlru_count(cache) == 1 && xlru_contains(cache, "a") == 0);
    CHECK(strlen(held) == 8192u);
    xrtFree(held);
    CHECK(xlru_contains(cache, "b") == 1);
    CHECK(same(xlru_get(cache, "b", "fallback"), ""));
    CHECK(same(xlru_get(cache, "a", "fallback"), "fallback"));
    CHECK(xlru_hits(cache) == 2 && xlru_misses(cache) == 1);

    CHECK(xlru_put(cache, "b", "new") == 1);
    CHECK(xlru_count(cache) == 1 && same(xlru_get(cache, "b", ""), "new"));
    held = (char*)xlru_get(cache, "b", "");
    CHECK(held != NULL);
    for (i = 0; i < 128; ++i)
        CHECK(same(xlru_get(cache, "b", ""), "new") && strcmp(held, "new") == 0);
    xrtFree(held);
    CHECK(xlru_remove(cache, "missing") == 0);
    CHECK(xlru_remove(cache, "b") == 1);
    CHECK(xlru_count(cache) == 0);
    CHECK(xlru_put(cache, "c", "value") == 1);
    CHECK(xlru_clear(cache) == 1 && xlru_count(cache) == 0);
    CHECK(xlru_hits(cache) == 0 && xlru_misses(cache) == 0);
    CHECK(xlru_put(cache, NULL, "x") == 0);
    CHECK(xlru_get(cache, NULL, "x") == NULL);

    CHECK(xlru_put(lfu, "hot", "h") == 1);
    CHECK(xlru_put(lfu, "cold", "c") == 1);
    CHECK(same(xlru_get(lfu, "hot", ""), "h"));
    CHECK(same(xlru_get(lfu, "hot", ""), "h"));
    CHECK(xlru_put(lfu, "new", "n") == 1);
    CHECK(xlru_contains(lfu, "hot") == 1 && xlru_contains(lfu, "cold") == 0);

    /* 同频 LFU 选择最久未访问的节点。 */
    CHECK(xlru_put(tied, "first", "1") == 1);
    CHECK(xlru_put(tied, "second", "2") == 1);
    CHECK(xlru_put(tied, "third", "3") == 1);
    CHECK(xlru_contains(tied, "first") == 0 && xlru_contains(tied, "second") == 1);

    xlru_destroy(tied);
    xlru_destroy(lfu);
    xlru_destroy(cache);
    CHECK(test_allocation_failure() == 0);
    CHECK(test_error_recovery() == 0);
    CHECK(test_long_keys() == 0);
    CHECK(test_interleaved_operations(0) == 0);
    CHECK(test_interleaved_operations(1) == 0);
    puts("lru C boundaries: PASS");
    return 0;
}
