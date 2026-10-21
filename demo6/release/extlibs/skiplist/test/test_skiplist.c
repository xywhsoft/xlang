#define XRT_MODULE_MEMORY_DEBUG
#include "skiplist_native.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "skiplist C test failed at %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

static xstrview text(const xvalue *value) {
    xstrview result = {NULL, 0};
    (void)xrtValueGetString(value, &result);
    return result;
}

static int test_create_failure(void) {
    unsigned failure;
    for (failure = 0u; failure < 2u; ++failure) {
        CHECK(xrtMemDebugFailAfter(failure));
        CHECK(xskiplist_create() == NULL);
        CHECK(xrtMemDebugFailTriggered());
        CHECK(xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        xrtMemDebugFailClear();
        xrtClearError();
    }
    {
        void *list = xskiplist_create();
        CHECK(list != NULL);
        xskiplist_destroy(list);
    }
    return 0;
}

static int test_order_and_range(void) {
    void *list = xskiplist_create();
    xvalue *keys, *range;
    char key[16];
    int i;
    CHECK(list != NULL);
    for (i = 999; i >= 0; --i) {
        int n = snprintf(key, sizeof(key), "k%04d", i);
        CHECK(n == 5 && xskiplist_set(list, key, 5u, key, 5u));
    }
    CHECK(xskiplist_count(list) == 1000);
    keys = xskiplist_keys(list);
    CHECK(keys != NULL && xrtValueCount(keys) == 1000);
    for (i = 0; i < 1000; ++i) {
        int n = snprintf(key, sizeof(key), "k%04d", i);
        xstrview view = text(xrtValueArrayAt(keys, i));
        CHECK(n == 5 && view.Size == 5u && memcmp(view.Data, key, 5u) == 0);
    }
    xrtValueRelease(keys);
    range = xskiplist_range(list, "k0100", 5u, "k0199", 5u);
    CHECK(range != NULL && xrtValueCount(range) == 100);
    {
        xvalue *first = xrtValueArrayAt(range, 0);
        xvalue *last = xrtValueArrayAt(range, 99);
        xstrview a = text(xrtValueObjectGet(first, (xstrview){"key", 3}));
        xstrview b = text(xrtValueObjectGet(last, (xstrview){"value", 5}));
        CHECK(a.Size == 5u && memcmp(a.Data, "k0100", 5u) == 0);
        CHECK(b.Size == 5u && memcmp(b.Data, "k0199", 5u) == 0);
    }
    xrtValueRelease(range);
    CHECK(xskiplist_remove(list, "k0100", 5u));
    CHECK(!xskiplist_contains(list, "k0100", 5u));
    CHECK(xskiplist_count(list) == 999);
    xskiplist_clear(list);
    CHECK(xskiplist_count(list) == 0);
    CHECK(xskiplist_set(list, "again", 5u, "yes", 3u));
    xskiplist_destroy(list);
    return 0;
}

static int test_binary_and_long(void) {
    void *list = xskiplist_create();
    const char key[] = {'x', 0, 'y'};
    const char binary[] = {'v', 0, 'w'};
    char longValue[5000];
    xvalue *value;
    xstrview view;
    CHECK(list != NULL);
    memset(longValue, 'Q', sizeof(longValue));
    CHECK(xskiplist_set(list, key, sizeof(key), binary, sizeof(binary)));
    CHECK(xskiplist_contains(list, key, sizeof(key)));
    CHECK(!xskiplist_contains(list, "x", 1u));
    value = xskiplist_get(list, key, sizeof(key));
    CHECK(value != NULL);
    view = text(value);
    CHECK(view.Size == sizeof(binary) && memcmp(view.Data, binary, sizeof(binary)) == 0);
    xrtValueRelease(value);
    CHECK(xskiplist_set(list, key, sizeof(key), longValue, sizeof(longValue)));
    value = xskiplist_get(list, key, sizeof(key));
    CHECK(value != NULL);
    view = text(value);
    CHECK(view.Size == sizeof(longValue) && memcmp(view.Data, longValue, sizeof(longValue)) == 0);
    xrtValueRelease(value);
    CHECK(xskiplist_set(list, "", 0u, "empty", 5u));
    CHECK(xskiplist_set(list, "empty-value", 11u, "", 0u));
    CHECK(xskiplist_count(list) == 3);
    value = xskiplist_get(list, "empty-value", 11u);
    CHECK(value != NULL && xrtValueType(value) == XVALUE_STRING &&
        text(value).Size == 0u);
    xrtValueRelease(value);
    value = xskiplist_get(list, "missing", 7u);
    CHECK(value != NULL && xrtValueType(value) == XVALUE_NULL);
    xrtValueRelease(value);
    xskiplist_destroy(list);
    return 0;
}

static int test_atomic_failure(void) {
    void *list = xskiplist_create();
    xvalue *value;
    unsigned failure;
    CHECK(list != NULL);
    CHECK(xskiplist_set(list, "old", 3u, "keep", 4u));
    CHECK(xrtMemDebugFailAfter(0u));
    CHECK(!xskiplist_set(list, "old", 3u, "replace", 7u));
    CHECK(xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    xrtMemDebugFailClear();
    xrtClearError();
    value = xskiplist_get(list, "old", 3u);
    CHECK(value != NULL && text(value).Size == 4u &&
        memcmp(text(value).Data, "keep", 4u) == 0);
    xrtValueRelease(value);
    for (failure = 0u; failure < 3u; ++failure) {
        CHECK(xrtMemDebugFailAfter(failure));
        CHECK(!xskiplist_set(list, "new", 3u, "data", 4u));
        CHECK(xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        xrtMemDebugFailClear();
        xrtClearError();
        CHECK(xskiplist_count(list) == 1);
        CHECK(!xskiplist_contains(list, "new", 3u));
    }
    CHECK(xskiplist_set(list, "new", 3u, "data", 4u));
    CHECK(xskiplist_count(list) == 2);
    xskiplist_destroy(list);
    return 0;
}

static int test_error_state(void) {
    void *list;
    xvalue *value;
    xrtSetErrorInfo(XERR_VALUE, "skiplist.test", 0, "stale error");
    list = xskiplist_create();
    CHECK(list != NULL && xrtGetError() == NULL);

    CHECK(!xskiplist_set(list, NULL, 1u, "v", 1u));
    CHECK(xrtGetError() != NULL);
    CHECK(!xskiplist_contains(list, "missing", 7u));
    CHECK(xrtGetError() == NULL);

    CHECK(!xskiplist_remove(list, NULL, 1u));
    CHECK(xrtGetError() != NULL);
    CHECK(xskiplist_set(list, "a", 1u, "v", 1u));
    CHECK(xrtGetError() == NULL);

    CHECK(xskiplist_get(list, NULL, 1u) == NULL);
    CHECK(xrtGetError() != NULL);
    value = xskiplist_get(list, "a", 1u);
    CHECK(value != NULL && xrtGetError() == NULL);
    xrtValueRelease(value);

    CHECK(xskiplist_range(list, NULL, 1u, "z", 1u) == NULL);
    CHECK(xrtGetError() != NULL);
    value = xskiplist_keys(list);
    CHECK(value != NULL && xrtGetError() == NULL);
    xrtValueRelease(value);

    CHECK(!xskiplist_set(list, NULL, 1u, "v", 1u));
    CHECK(xrtGetError() != NULL);
    CHECK(xskiplist_count(list) == 1 && xrtGetError() == NULL);
    CHECK(!xskiplist_set(list, NULL, 1u, "v", 1u));
    CHECK(xrtGetError() != NULL);
    xskiplist_clear(list);
    CHECK(xrtGetError() == NULL && xskiplist_count(list) == 0);
    xskiplist_destroy(list);
    CHECK(xskiplist_count(NULL) == 0 && xrtGetError() != NULL &&
        xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    xskiplist_clear(NULL);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    xrtClearError();
    return 0;
}

static int test_interleaved_against_reference(void) {
    struct Entry { char key[2]; char value[3]; size_t keySize; bool present; } entries[25] = {0};
    void *list = xskiplist_create();
    uint32_t seed = 0x58d321a7u;
    unsigned step, i, j;
    CHECK(list != NULL);
    for (i = 1u; i < 25u; ++i) {
        entries[i].key[0] = (char)(i / 4u);
        entries[i].key[1] = (char)(i % 4u);
        entries[i].keySize = 2u;
    }
    for (step = 0u; step < 400u; ++step) {
        unsigned count = 0u;
        xvalue *keys;
        seed = seed * 1664525u + 1013904223u;
        i = (seed >> 8u) % 25u;
        if ((seed & 1u) != 0u) {
            entries[i].value[0] = (char)('A' + step % 26u);
            entries[i].value[1] = 0;
            entries[i].value[2] = (char)i;
            CHECK(xskiplist_set(list, entries[i].key, entries[i].keySize,
                entries[i].value, sizeof(entries[i].value)));
            entries[i].present = true;
        } else {
            CHECK(xskiplist_remove(list, entries[i].key, entries[i].keySize) ==
                entries[i].present);
            entries[i].present = false;
        }
        if (step % 89u == 88u) {
            xskiplist_clear(list);
            for (j = 0u; j < 25u; ++j) entries[j].present = false;
        }
        keys = xskiplist_keys(list);
        CHECK(keys != NULL);
        for (i = 0u; i < 25u; ++i) {
            xvalue *value = xskiplist_get(list, entries[i].key, entries[i].keySize);
            CHECK(value != NULL);
            CHECK(xskiplist_contains(list, entries[i].key, entries[i].keySize) ==
                entries[i].present);
            if (entries[i].present) {
                xstrview key = text(xrtValueArrayAt(keys, count));
                xstrview data = text(value);
                CHECK(key.Size == entries[i].keySize &&
                    memcmp(key.Data, entries[i].key, key.Size) == 0);
                CHECK(data.Size == sizeof(entries[i].value) &&
                    memcmp(data.Data, entries[i].value, data.Size) == 0);
                count++;
            } else CHECK(xrtValueType(value) == XVALUE_NULL);
            xrtValueRelease(value);
        }
        CHECK(xskiplist_count(list) == count && xrtValueCount(keys) == count);
        xrtValueRelease(keys);
        if (step % 7u == 0u) {
            unsigned first = (seed >> 16u) % 25u;
            unsigned last = (seed >> 24u) % 25u;
            unsigned position = 0u;
            xvalue *range = xskiplist_range(list, entries[first].key,
                entries[first].keySize, entries[last].key, entries[last].keySize);
            CHECK(range != NULL);
            for (j = first; j <= last; ++j) {
                if (entries[j].present) {
                    xvalue *item = xrtValueArrayAt(range, position++);
                    xstrview key = text(xrtValueObjectGet(item, (xstrview){"key", 3}));
                    xstrview data = text(xrtValueObjectGet(item, (xstrview){"value", 5}));
                    CHECK(key.Size == entries[j].keySize &&
                        memcmp(key.Data, entries[j].key, key.Size) == 0);
                    CHECK(data.Size == sizeof(entries[j].value) &&
                        memcmp(data.Data, entries[j].value, data.Size) == 0);
                }
            }
            CHECK(xrtValueCount(range) == position);
            xrtValueRelease(range);
        }
    }
    xskiplist_destroy(list);
    return 0;
}

int main(void) {
    CHECK(test_create_failure() == 0);
    CHECK(test_order_and_range() == 0);
    CHECK(test_binary_and_long() == 0);
    CHECK(test_atomic_failure() == 0);
    CHECK(test_error_state() == 0);
    CHECK(test_interleaved_against_reference() == 0);
    return 0;
}
