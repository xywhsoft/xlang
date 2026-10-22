#define XRT_MODULE_HASH64
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "cuckoo_native.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "cuckoo C test failed at %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

static int test_collision_rollback(void) {
    char keys[9][32];
    uint16_t fingerprints[9];
    void *filter = xcuckoo_create(16);
    int found = 0, candidate, i;
    CHECK(filter != NULL && xcuckoo_buckets(filter) == 8);
    for (candidate = 0; candidate < 100000 && found < 9; ++candidate) {
        char key[32];
        uint64_t hash;
        uint16_t fp;
        size_t first, alternate;
        int n = snprintf(key, sizeof(key), "collision:%d", candidate);
        CHECK(n > 0 && (size_t)n < sizeof(key));
        hash = xrtHash64(key, (size_t)n);
        fp = (uint16_t)(hash >> 32);
        if (fp == 0) fp = 1;
        first = (size_t)hash & 7;
        alternate = (first ^ ((uint32_t)fp * UINT32_C(2654435761))) & 7;
        if (first != 0 || alternate != 1) continue;
        for (i = 0; i < found && fingerprints[i] != fp; ++i) { }
        if (i != found) continue;
        strcpy(keys[found], key);
        fingerprints[found++] = fp;
    }
    CHECK(found == 9);
    for (i = 0; i < 8; ++i) CHECK(xcuckoo_add(filter, keys[i], strlen(keys[i])));
    CHECK(xcuckoo_count(filter) == 8);
    CHECK(!xcuckoo_add(filter, keys[8], strlen(keys[8])));
    CHECK(xcuckoo_count(filter) == 8);
    for (i = 0; i < 8; ++i) CHECK(xcuckoo_contains(filter, keys[i], strlen(keys[i])));
    CHECK(!xcuckoo_contains(filter, keys[8], strlen(keys[8])));
    CHECK(xcuckoo_remove(filter, keys[0], strlen(keys[0])));
    CHECK(xcuckoo_add(filter, keys[8], strlen(keys[8])));
    CHECK(xcuckoo_contains(filter, keys[8], strlen(keys[8])));
    xcuckoo_destroy(filter);
    return 0;
}

static int test_distinct_alternate_bucket(void) {
    char keys[5][32];
    uint16_t fingerprints[5];
    void *filter = xcuckoo_create(8);
    int found = 0, candidate, i;
    CHECK(filter != NULL && xcuckoo_buckets(filter) == 4);
    for (candidate = 0; candidate < 100000 && found < 5; ++candidate) {
        char key[32];
        uint64_t hash;
        uint16_t fp;
        int n = snprintf(key, sizeof(key), "self-alt:%d", candidate);
        CHECK(n > 0 && (size_t)n < sizeof(key));
        hash = xrtHash64(key, (size_t)n);
        fp = (uint16_t)(hash >> 32);
        if (fp == 0) fp = 1;
        if ((hash & 3u) != 0u || (fp & 3u) != 0u) continue;
        for (i = 0; i < found && fingerprints[i] != fp; ++i) { }
        if (i != found) continue;
        strcpy(keys[found], key);
        fingerprints[found++] = fp;
    }
    CHECK(found == 5);
    for (i = 0; i < found; ++i) CHECK(xcuckoo_add(filter, keys[i], strlen(keys[i])));
    CHECK(xcuckoo_count(filter) == 5);
    for (i = 0; i < found; ++i) CHECK(xcuckoo_contains(filter, keys[i], strlen(keys[i])));
    xcuckoo_destroy(filter);
    return 0;
}

static int test_bounds(void) {
    const char binary[] = {'a', 0, 'b'};
    void *filter = xcuckoo_create(1);
    CHECK(filter != NULL);
    CHECK(xcuckoo_add(filter, binary, sizeof(binary)));
    CHECK(xcuckoo_contains(filter, binary, sizeof(binary)));
    CHECK(!xcuckoo_contains(filter, binary, 1));
    CHECK(!xcuckoo_add(filter, "other", 5));
    CHECK(xrtGetError() != NULL);
    CHECK(!xcuckoo_contains(filter, "other", 5) && xrtGetError() == NULL);
    CHECK(xcuckoo_count(filter) == 1);
    CHECK(xcuckoo_remove(filter, binary, sizeof(binary)));
    CHECK(xcuckoo_count(filter) == 0);
    CHECK(xcuckoo_add(filter, "", 0));
    CHECK(xcuckoo_contains(filter, "", 0));
    xcuckoo_clear(filter);
    CHECK(xcuckoo_count(filter) == 0 && !xcuckoo_contains(filter, "", 0));
    xcuckoo_destroy(filter);
    CHECK(xcuckoo_create(0) == NULL);
    return 0;
}

static int test_high_load(void) {
    void *filter = xcuckoo_create(3000);
    unsigned int inserted = 0u;
    unsigned int i;
    char key[32];
    CHECK(filter != NULL);
    for (i = 0u; i <= 3000u; ++i) {
        int n = snprintf(key, sizeof(key), "stress:%u", i);
        CHECK(n > 0 && (size_t)n < sizeof(key));
        if (!xcuckoo_add(filter, key, (size_t)n)) {
            CHECK(xrtGetError() != NULL);
            break;
        }
        ++inserted;
    }
    CHECK(inserted > 2500u && inserted <= 3000u);
    CHECK(xcuckoo_count(filter) == (int64_t)inserted);
    for (i = 0u; i < inserted; ++i) {
        int n = snprintf(key, sizeof(key), "stress:%u", i);
        CHECK(n > 0 && xcuckoo_contains(filter, key, (size_t)n));
    }
    CHECK(xcuckoo_count(filter) == (int64_t)inserted);
    xcuckoo_destroy(filter);
    return 0;
}

static int test_error_recovery(void) {
    void *filter;
    xrtClearError();
    CHECK(xcuckoo_create(0) == NULL && xrtGetError());
    filter = xcuckoo_create(2);
    CHECK(filter != NULL && xrtGetError() == NULL);
    CHECK(!xcuckoo_add(NULL, "x", 1) && xrtGetError());
    CHECK(xcuckoo_add(filter, "x", 1) && xrtGetError() == NULL);
    CHECK(!xcuckoo_contains(NULL, "x", 1) && xrtGetError());
    CHECK(xcuckoo_contains(filter, "x", 1) && xrtGetError() == NULL);
    CHECK(!xcuckoo_remove(NULL, "x", 1) && xrtGetError());
    CHECK(!xcuckoo_remove(filter, "missing", 7) && xrtGetError() == NULL);
    CHECK(xcuckoo_count(NULL) == -1 && xrtGetError());
    CHECK(xcuckoo_count(filter) == 1 && xrtGetError() == NULL);
    CHECK(xcuckoo_buckets(NULL) == -1 && xrtGetError());
    CHECK(xcuckoo_buckets(filter) > 0 && xrtGetError() == NULL);
    xcuckoo_clear(NULL);
    CHECK(xrtGetError());
    xcuckoo_clear(filter);
    CHECK(xcuckoo_count(filter) == 0 && xrtGetError() == NULL);
    xcuckoo_destroy(filter);
    return 0;
}

static int test_create_allocation_failures(void) {
    unsigned int allocation;
    for (allocation = 0; allocation < 2; ++allocation) {
        void *failed;
        xrtClearError();
        CHECK(xrtMemDebugFailAfter(allocation));
        failed = xcuckoo_create(64);
        CHECK(xrtMemDebugFailTriggered());
        xrtMemDebugFailClear();
        CHECK(failed == NULL && xrtGetError() != NULL &&
            xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        failed = xcuckoo_create(64);
        CHECK(failed != NULL && xrtGetError() == NULL);
        xcuckoo_destroy(failed);
    }
    return 0;
}

int main(void) {
    if (test_collision_rollback() || test_distinct_alternate_bucket() ||
        test_bounds() || test_high_load() ||
        test_error_recovery() || test_create_allocation_failures()) return 1;
    puts("cuckoo C: PASS");
    return 0;
}
