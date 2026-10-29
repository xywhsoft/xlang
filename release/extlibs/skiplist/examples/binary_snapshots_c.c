/* 与 binary_snapshots.xl 同一过程；xvalue 字符串必须按明确长度访问。 */
#include "../skiplist_native.c"
#include <inttypes.h>
#include <stdio.h>

#define CHECK(test) do { if (!(test)) goto fail; } while (0)

static bool text_is(const xvalue *value, const char *expected, size_t size) {
    xstrview view;
    return value != NULL && xrtValueGetString(value, &view) && view.Size == size &&
        (size == 0 || memcmp(view.Data, expected, size) == 0);
}

int main(void) {
    void *table = xskiplist_create();
    xvalue *binary = NULL, *keys = NULL, *rows = NULL, *value = NULL;
    CHECK(table != NULL);
    CHECK(xskiplist_set(table, "", 0, "", 0));
    CHECK(xskiplist_set(table, "a", 1, "plain", 5));
    CHECK(xskiplist_set(table, "a\0b", 3, "v\0x", 3));
    CHECK(xskiplist_set(table, "\xe4\xbd\xa0", 3, "\xe5\x80\xbc", 3));
    CHECK(xskiplist_count(table) == 4 && xskiplist_contains(table, "a\0b", 3));
    binary = xskiplist_get(table, "a\0b", 3);
    keys = xskiplist_keys(table);
    rows = xskiplist_range(table, "a", 1, "a\0b", 3);
    CHECK(text_is(binary, "v\0x", 3));
    CHECK(keys != NULL && xrtValueCount(keys) == 4);
    CHECK(text_is(xrtValueArrayGet(keys, 0), "", 0));
    CHECK(text_is(xrtValueArrayGet(keys, 2), "a\0b", 3));
    CHECK(rows != NULL && xrtValueCount(rows) == 2);
    puts("binary bytes: 3");
    value = xskiplist_get(table, "", 0);
    CHECK(text_is(value, "", 0));
    xrtValueRelease(value); value = NULL;
    puts("empty stored: true");
    value = xskiplist_get(table, "missing", 7);
    CHECK(value != NULL && xrtValueType(value) == XVALUE_NULL);
    /* C 层用 NULL 值表示未命中，调用者在这里选择与 XLang 一样的缺省值。 */
    puts("missing default: fallback");
    xrtValueRelease(value); value = NULL;
    printf("range rows: %zu\n", xrtValueCount(rows));
    CHECK(xskiplist_set(table, "a", 1, "changed", 7));
    CHECK(xskiplist_remove(table, "a\0b", 3) && !xskiplist_contains(table, "a\0b", 3));
    xskiplist_clear(table);
    CHECK(xskiplist_count(table) == 0);
    CHECK(xskiplist_set(table, "reuse", 5, "ok", 2));
    value = xskiplist_get(table, "reuse", 5);
    CHECK(text_is(value, "ok", 2) && xskiplist_count(table) == 1);
    xskiplist_destroy(table); table = NULL;
    /* 先销毁原索引，再核对快照正文；不是仅检查数组长度。 */
    CHECK(text_is(binary, "v\0x", 3) && xrtValueCount(keys) == 4);
    CHECK(text_is(xrtValueObjectGet(xrtValueArrayGet(rows, 0), (xstrview){"value", 5}), "plain", 5));
    CHECK(text_is(xrtValueObjectGet(xrtValueArrayGet(rows, 1), (xstrview){"value", 5}), "v\0x", 3));
    printf("snapshot rows: %zu\n", xrtValueCount(rows));
    puts("reused count: 1");
    xrtValueRelease(value);
    xrtValueRelease(binary);
    xrtValueRelease(keys);
    xrtValueRelease(rows);
    return 0;
fail:
    fputs("SkipList binary snapshot example failed\n", stderr);
    xrtValueRelease(value);
    xrtValueRelease(binary);
    xrtValueRelease(keys);
    xrtValueRelease(rows);
    xskiplist_destroy(table);
    return 1;
}
