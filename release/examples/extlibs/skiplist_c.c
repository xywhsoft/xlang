#include "../../extlibs/skiplist/include/skiplist_native.h"
#include "../../extlibs/skiplist/skiplist_native.c"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "skiplist example failed at line %d: %s\n", __LINE__, #test); \
    goto done; \
} } while (0)

static bool put(void *list, const char *key, const char *value) {
    return xskiplist_set(list, key, strlen(key), value, strlen(value));
}

/* 读取失败不是空文本；按字节长度输出，不能用 strlen 截断内嵌 NUL。 */
static bool print_text(const xvalue *value) {
    xstrview text;
    return value != NULL && xrtValueGetString(value, &text) &&
        (text.Size == 0 || fwrite(text.Data, 1, text.Size, stdout) == text.Size);
}

int main(void) {
    void *list = xskiplist_create();
    xvalue *value = NULL, *keys = NULL, *range = NULL;
    int result = 1;
    CHECK(list != NULL && put(list, "user:1001", "Alice") &&
        put(list, "user:1003", "Charlie") && put(list, "user:1002", "Bob"));
    printf("count: %" PRId64 "\n", xskiplist_count(list));
    printf("user:1001 = ");
    value = xskiplist_get(list, "user:1001", 9u);
    CHECK(print_text(value));
    xrtValueRelease(value); value = NULL;
    printf("\nuser:1002 = ");
    value = xskiplist_get(list, "user:1002", 9u);
    CHECK(print_text(value));
    xrtValueRelease(value); value = NULL;
    printf("\ncontains user:1003: %s\n",
        xskiplist_contains(list, "user:1003", 9u) ? "true" : "false");
    keys = xskiplist_keys(list);
    range = xskiplist_range(list, "user:1001", 9u, "user:1002", 9u);
    CHECK(keys != NULL && range != NULL);
    printf("first key: ");
    CHECK(print_text(xrtValueArrayAt(keys, 0)));
    printf("\nrange count: %lld\n", (long long)xrtValueCount(range));
    xrtValueRelease(keys); keys = NULL;
    xrtValueRelease(range); range = NULL;
    CHECK(xskiplist_remove(list, "user:1002", 9u));
    printf("after remove, count: %" PRId64 "\n", xskiplist_count(list));
    value = xskiplist_get(list, "user:1002", 9u);
    CHECK(value != NULL && xrtValueType(value) == XVALUE_NULL);
    printf("get removed: (none)\n");
    puts("skiplist example: PASS");
    CHECK(!ferror(stdout));
    result = 0;
done:
    xrtValueRelease(value);
    xrtValueRelease(keys);
    xrtValueRelease(range);
    xskiplist_destroy(list);
    return result;
}
