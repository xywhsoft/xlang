#define XRT_MODULE_MEMORY_DEBUG
#include "trie_native.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "trie C test failed at %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

static int test_create_failure(void) {
    unsigned failure;
    for (failure = 0u; failure < 2u; ++failure) {
        CHECK(xrtMemDebugFailAfter(failure));
        CHECK(xtrie_create() == NULL);
        CHECK(xrtMemDebugFailTriggered());
        xrtMemDebugFailClear();
        xrtClearError();
    }
    {
        void *trie = xtrie_create();
        CHECK(trie != NULL);
        xtrie_destroy(trie);
    }
    return 0;
}

static int test_error_lifecycle(void) {
    void *trie = xtrie_create();
    CHECK(trie != NULL);
    CHECK(!xtrie_contains(NULL, "x", 1u) && xrtGetError() != NULL);
    CHECK(!xtrie_contains(trie, "x", 1u) && xrtGetError() == NULL);
    CHECK(!xtrie_starts_with(trie, NULL, 1u) && xrtGetError() != NULL);
    CHECK(!xtrie_starts_with(trie, "x", 1u) && xrtGetError() == NULL);
    CHECK(xtrie_count_prefix(NULL, "x", 1u) == -1 && xrtGetError() != NULL);
    CHECK(xtrie_count_prefix(trie, "x", 1u) == 0 && xrtGetError() == NULL);
    CHECK(!xtrie_insert(NULL, "x", 1u) && xrtGetError() != NULL);
    CHECK(xtrie_insert(trie, "x", 1u) && xrtGetError() == NULL);
    CHECK(!xtrie_remove(NULL, "x", 1u) && xrtGetError() != NULL);
    CHECK(!xtrie_remove(trie, "missing", 7u) && xrtGetError() == NULL);
    xtrie_clear(NULL);
    CHECK(xrtGetError() != NULL);
    xtrie_clear(trie);
    CHECK(xrtGetError() == NULL);
    CHECK(xtrie_count(NULL) == 0 && xrtGetError() != NULL);
    CHECK(xtrie_count(trie) == 0 && xrtGetError() == NULL);
    CHECK(!xtrie_contains(NULL, "x", 1u) && xrtGetError() != NULL);
    {
        void *second = xtrie_create();
        CHECK(second != NULL && xrtGetError() == NULL);
        xtrie_destroy(second);
    }
    xtrie_destroy(trie);
    return 0;
}

static int test_words(void) {
    void *trie = xtrie_create();
    const char binary[] = {'a', 0, 'b'};
    CHECK(trie != NULL);
    CHECK(xtrie_insert(trie, "", 0u));
    CHECK(xtrie_insert(trie, "app", 3u));
    CHECK(xtrie_insert(trie, "apple", 5u));
    CHECK(xtrie_insert(trie, "application", 11u));
    CHECK(xtrie_insert(trie, "\xe4\xbd\xa0\xe5\xa5\xbd", 6u));
    CHECK(xtrie_insert(trie, binary, sizeof(binary)));
    CHECK(xtrie_insert(trie, "apple", 5u));
    CHECK(xtrie_count(trie) == 6);
    CHECK(xtrie_count_prefix(trie, "app", 3u) == 3);
    CHECK(xtrie_count_prefix(trie, "", 0u) == 6);
    CHECK(xtrie_contains(trie, binary, sizeof(binary)));
    CHECK(!xtrie_contains(trie, "a", 1u));
    CHECK(xtrie_starts_with(trie, "\xe4\xbd\xa0", 3u));
    CHECK(!xtrie_starts_with(trie, "missing", 7u));
    CHECK(xtrie_remove(trie, "app", 3u));
    CHECK(!xtrie_contains(trie, "app", 3u));
    CHECK(xtrie_contains(trie, "apple", 5u));
    CHECK(xtrie_count_prefix(trie, "app", 3u) == 2);
    CHECK(!xtrie_remove(trie, "app", 3u));
    CHECK(xtrie_remove(trie, "", 0u));
    CHECK(xtrie_count(trie) == 4);
    xtrie_clear(trie);
    CHECK(xtrie_count(trie) == 0);
    CHECK(xtrie_starts_with(trie, "", 0u));
    CHECK(xtrie_insert(trie, "again", 5u));
    CHECK(xtrie_contains(trie, "again", 5u));
    xtrie_destroy(trie);
    return 0;
}

static int test_all_edges(void) {
    void *trie = xtrie_create();
    unsigned i;
    CHECK(trie != NULL);
    for (i = 256u; i != 0u; --i) {
        char byte = (char)(i - 1u);
        CHECK(xtrie_insert(trie, &byte, 1u));
    }
    CHECK(xtrie_count(trie) == 256);
    for (i = 0u; i < 256u; ++i) {
        char byte = (char)i;
        CHECK(xtrie_contains(trie, &byte, 1u));
        CHECK(xtrie_count_prefix(trie, &byte, 1u) == 1);
        if ((i & 1u) != 0u) CHECK(xtrie_remove(trie, &byte, 1u));
    }
    CHECK(xtrie_count(trie) == 128);
    for (i = 0u; i < 256u; ++i) {
        char byte = (char)i;
        CHECK(xtrie_contains(trie, &byte, 1u) == ((i & 1u) == 0u));
    }
    xtrie_destroy(trie);
    return 0;
}

static int test_deep_and_fault(void) {
    void *trie = xtrie_create();
    char *word = (char *)xrtMalloc(30000u);
    unsigned i;
    int failures = 0;
    CHECK(trie != NULL && word != NULL);
    memset(word, 'q', 30000u);
    CHECK(xtrie_insert(trie, word, 30000u));
    CHECK(xtrie_contains(trie, word, 30000u));
    CHECK(xtrie_count_prefix(trie, word, 20000u) == 1);
    xtrie_clear(trie);
    CHECK(xtrie_count(trie) == 0);
    xrtFree(word);
    CHECK(xtrie_insert(trie, "a", 1u));
    CHECK(xtrie_insert(trie, "base", 4u));
    CHECK(xtrie_insert(trie, "c", 1u));
    CHECK(xtrie_insert(trie, "d", 1u));
    for (i = 0u; i < 18u; ++i) {
        bool inserted;
        CHECK(xrtMemDebugFailAfter(i));
        inserted = xtrie_insert(trie, "newbranch", 9u);
        if (!inserted) failures++;
        xrtMemDebugFailClear();
        xrtClearError();
        CHECK(xtrie_contains(trie, "base", 4u));
        if (inserted) CHECK(xtrie_remove(trie, "newbranch", 9u));
        CHECK(xtrie_count(trie) == 4);
        CHECK(!xtrie_starts_with(trie, "new", 3u));
    }
    CHECK(failures >= 10);
    CHECK(xtrie_insert(trie, "newbranch", 9u));
    CHECK(xtrie_count(trie) == 5);
    xtrie_destroy(trie);
    return 0;
}

static int test_interleaved_against_reference(void) {
    struct Key { unsigned char data[3]; size_t size; bool present; } keys[25] = {0};
    void *trie = xtrie_create();
    uint32_t seed = 0x31a5c749u;
    unsigned step, i, j;
    CHECK(trie != NULL);
    for (i = 1u; i < 25u; ++i) {
        keys[i].data[0] = (unsigned char)('a' + i % 4u);
        keys[i].data[1] = (unsigned char)(i / 4u);
        keys[i].data[2] = (unsigned char)(0x80u + i);
        keys[i].size = (i & 1u) != 0u ? 3u : 2u;
    }
    for (step = 0u; step < 800u; ++step) {
        unsigned count = 0u;
        seed = seed * 1664525u + 1013904223u;
        i = (seed >> 8u) % 25u;
        if ((seed & 1u) != 0u) {
            CHECK(xtrie_insert(trie, (const char *)keys[i].data, keys[i].size));
            keys[i].present = true;
        } else {
            CHECK(xtrie_remove(trie, (const char *)keys[i].data, keys[i].size) == keys[i].present);
            keys[i].present = false;
        }
        if (step % 113u == 112u) {
            xtrie_clear(trie);
            for (j = 0u; j < 25u; ++j) keys[j].present = false;
        }
        for (i = 0u; i < 25u; ++i) {
            size_t prefix;
            if (keys[i].present) count++;
            CHECK(xtrie_contains(trie, (const char *)keys[i].data, keys[i].size) == keys[i].present);
            for (prefix = 0u; prefix <= keys[i].size; ++prefix) {
                unsigned expected = 0u;
                for (j = 0u; j < 25u; ++j) {
                    if (keys[j].present && keys[j].size >= prefix &&
                        memcmp(keys[j].data, keys[i].data, prefix) == 0) expected++;
                }
                CHECK(xtrie_count_prefix(trie, (const char *)keys[i].data, prefix) == expected);
                CHECK(xtrie_starts_with(trie, (const char *)keys[i].data, prefix) ==
                    (prefix == 0u || expected != 0u));
            }
        }
        CHECK(xtrie_count(trie) == count);
    }
    xtrie_destroy(trie);
    return 0;
}

int main(void) {
    CHECK(test_create_failure() == 0);
    CHECK(test_error_lifecycle() == 0);
    CHECK(test_words() == 0);
    CHECK(test_all_edges() == 0);
    CHECK(test_deep_and_fault() == 0);
    CHECK(test_interleaved_against_reference() == 0);
    return 0;
}
