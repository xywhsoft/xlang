/* 完整小查询逐分配点回滚；只在夹具中观察 RNG，不暴露私有节点 ABI。 */
#define XRT_MODULE_MEMORY_DEBUG
#include "../skiplist_native.c"
#include <stdio.h>

#define CHECK(test) do { ++checks; if (!(test)) { \
    fprintf(stderr, "skiplist snapshot line %d: %s\n", __LINE__, #test); return 1; \
} } while (0)

typedef struct { const char *key, *value; size_t key_size, value_size; } Record;
static char large[8192];
static const Record records[] = {
    {"", "", 0, 0}, {"\0a", "\0v", 2, 2}, {"a", large, 1, sizeof(large)},
    {"a\0b", "v\0x", 3, 3},
    {"b00", "row", 3, 3}, {"b01", "row", 3, 3}, {"b02", "row", 3, 3},
    {"b03", "row", 3, 3}, {"b04", "row", 3, 3}, {"b05", "row", 3, 3},
    {"b06", "row", 3, 3}, {"b07", "row", 3, 3}, {"b08", "row", 3, 3},
    {"b09", "row", 3, 3}, {"b10", "row", 3, 3}, {"b11", "row", 3, 3},
    {"\x80", "high", 1, 4},
    {"\xe4\xbd\xa0", "\xe5\x80\xbc", 3, 3}
};
#define RECORD_COUNT (sizeof(records) / sizeof(records[0]))
static unsigned checks;

static xmemdebugsnapshot memory(void) {
    xmemdebugsnapshot value;
    xrtMemDebugSnapshot(&value);
    return value;
}

static int balanced(xmemdebugsnapshot a, xmemdebugsnapshot b) {
    int result = a.Enabled && b.Enabled && a.LiveCount == b.LiveCount &&
        a.LiveBytes == b.LiveBytes && a.InvalidFreeCount == b.InvalidFreeCount &&
        a.DoubleFreeCount == b.DoubleFreeCount && a.OverflowCount == b.OverflowCount &&
        a.UnderflowCount == b.UnderflowCount;
    if (!result)
        fprintf(stderr, "memory: live=%llu/%llu bytes=%llu/%llu invalid=%llu/%llu double=%llu/%llu overflow=%llu/%llu underflow=%llu/%llu\n",
            (unsigned long long)a.LiveCount, (unsigned long long)b.LiveCount,
            (unsigned long long)a.LiveBytes, (unsigned long long)b.LiveBytes,
            (unsigned long long)a.InvalidFreeCount, (unsigned long long)b.InvalidFreeCount,
            (unsigned long long)a.DoubleFreeCount, (unsigned long long)b.DoubleFreeCount,
            (unsigned long long)a.OverflowCount, (unsigned long long)b.OverflowCount,
            (unsigned long long)a.UnderflowCount, (unsigned long long)b.UnderflowCount);
    return result;
}

static int equal_text(const xvalue *value, const char *data, size_t size) {
    xstrview view;
    return value != NULL && xrtValueGetString(value, &view) && view.Size == size &&
        (size == 0 || memcmp(view.Data, data, size) == 0);
}

static xvalue *query(void *list, unsigned operation) {
    if (operation == 0) return xskiplist_get(list, "a\0b", 3);
    if (operation == 1) return xskiplist_get(list, "missing", 7);
    if (operation == 2) return xskiplist_keys(list);
    if (operation == 3) return xskiplist_range(list, "", 0, "\xe4\xbd\xa0", 3);
    if (operation == 4) return xskiplist_range(list, "a", 1, "a\0b", 3);
    if (operation == 5) return xskiplist_range(list, "z", 1, "a", 1);
    return xskiplist_range(list, "c", 1, "d", 1);
}

static int verify(const xvalue *result, unsigned operation) {
    size_t i, first = operation == 4 ? 2 : 0;
    size_t count = operation == 4 ? 2 : (operation >= 5 ? 0 : RECORD_COUNT);
    CHECK(result != NULL);
    if (operation == 0) { CHECK(equal_text(result, "v\0x", 3)); return 0; }
    if (operation == 1) { CHECK(xrtValueType(result) == XVALUE_NULL); return 0; }
    CHECK(xrtValueType(result) == XVALUE_ARRAY && xrtValueCount(result) == count);
    for (i = 0; i < count; ++i) {
        xvalue *item = xrtValueArrayGet(result, i);
        const Record *expected = &records[first + i];
        if (operation == 2) CHECK(equal_text(item, expected->key, expected->key_size));
        else {
            CHECK(item != NULL && xrtValueType(item) == XVALUE_OBJECT && xrtValueCount(item) == 2);
            CHECK(equal_text(xrtValueObjectGet(item, (xstrview){"key", 3}), expected->key, expected->key_size));
            CHECK(equal_text(xrtValueObjectGet(item, (xstrview){"value", 5}), expected->value, expected->value_size));
        }
    }
    return 0;
}

static int verify_list(void *list) {
    size_t i;
    CHECK(xskiplist_count(list) == (int64_t)RECORD_COUNT);
    for (i = 0; i < RECORD_COUNT; ++i) {
        const Record *record = &records[i];
        xvalue *value = xskiplist_get(list, record->key, record->key_size);
        CHECK(equal_text(value, record->value, record->value_size));
        xrtValueRelease(value);
    }
    return 0;
}

static int read_failures(void *list) {
    unsigned operation;
    xvalue *held[7] = {0};
    for (operation = 0; operation < 7; ++operation) {
        unsigned budget;
        held[operation] = query(list, operation);
        CHECK(verify(held[operation], operation) == 0);
        /* 直到首次成功，而非只截取大查询的前 64 个分配点。 */
        for (budget = 0; budget < 512; ++budget) {
            xmemdebugsnapshot before = memory();
            xvalue *result;
            int triggered, kind;
            CHECK(xrtMemDebugFailAfter(budget));
            result = query(list, operation);
            triggered = xrtMemDebugFailTriggered();
            kind = xrtErrorKind(xrtGetError());
            xrtMemDebugFailClear();
            if (triggered) {
                CHECK(result == NULL && kind == XERR_MEMORY);
            } else {
                CHECK(xrtGetError() == NULL && verify(result, operation) == 0);
            }
            xrtValueRelease(result);
            /* 已核对错误类别；释放线程错误槽拥有的诊断后再比较存活内存。 */
            xrtClearError();
            CHECK(balanced(before, memory()));
            CHECK(verify_list(list) == 0);
            /* 已持有的结果在其他查询失败后也必须保持完整。 */
            CHECK(verify(held[operation], operation) == 0);
            if (!triggered) break;
        }
        CHECK(budget < 512);
        printf("skiplist query %u: %u failure points, then success\n", operation, budget);
    }
    xskiplist_clear(list);
    xskiplist_destroy(list);
    /* 释放索引后再读完整键值，非仅检查结果长度；交错次序释放结果。 */
    for (operation = 0; operation < 7; ++operation)
        CHECK(verify(held[operation], operation) == 0);
    for (operation = 0; operation < 7; ++operation)
        xrtValueRelease(held[(operation * 3) % 7]);
    return 0;
}

static int write_failures(XSkipList *list) {
    unsigned budget;
    xrng old_rng = list->rng;
    for (budget = 0; budget < 3; ++budget) {
        xmemdebugsnapshot before = memory();
        CHECK(xrtMemDebugFailAfter(budget));
        CHECK(!xskiplist_set(list, "new", 3, "value", 5));
        CHECK(xrtMemDebugFailTriggered() && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        xrtMemDebugFailClear();
        xrtClearError();
        CHECK(memcmp(&old_rng, &list->rng, sizeof(old_rng)) == 0);
        CHECK(balanced(before, memory()));
        CHECK(verify_list(list) == 0 && !xskiplist_contains(list, "new", 3));
    }
    CHECK(xrtMemDebugFailAfter(0));
    CHECK(!xskiplist_set(list, "a", 1, "changed", 7));
    CHECK(xrtMemDebugFailTriggered() && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    xrtMemDebugFailClear();
    CHECK(memcmp(&old_rng, &list->rng, sizeof(old_rng)) == 0 && verify_list(list) == 0);
    CHECK(xrtMemDebugFailAfter(3));
    CHECK(xskiplist_set(list, "new", 3, "value", 5));
    CHECK(!xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    CHECK(memcmp(&old_rng, &list->rng, sizeof(old_rng)) != 0);
    CHECK(xskiplist_remove(list, "new", 3) && verify_list(list) == 0);
    return 0;
}

int main(void) {
    xmemdebugsnapshot before;
    XSkipList *list;
    size_t i;
    memset(large, 'Q', sizeof(large));
    xrtSetErrorInfo(XERR_VALUE, "skiplist.test", 0, "warm diagnostic slot");
    xrtClearError();
    list = xskiplist_create();
    CHECK(list != NULL);
    xskiplist_destroy(list);
    before = memory();
    list = xskiplist_create();
    CHECK(list != NULL);
    /* 固定布局仅用于回滚夹具；正式 create 仍由系统随机源播种。 */
    xrtRngSeed(&list->rng, 123, 456);
    for (i = 0; i < RECORD_COUNT; ++i)
        CHECK(xskiplist_set(list, records[i].key, records[i].key_size,
            records[i].value, records[i].value_size));
    CHECK(write_failures(list) == 0);
    CHECK(read_failures(list) == 0);
    CHECK(balanced(before, memory()));
    printf("skiplist complete snapshots: %u checks, PASS\n", checks);
    return 0;
}
