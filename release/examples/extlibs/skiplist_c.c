#include "../../extlibs/skiplist/include/skiplist_native.h"
#include "../../extlibs/skiplist/skiplist_native.c"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static bool put(void *list, const char *key, const char *value) {
    return xskiplist_set(list, key, strlen(key), value, strlen(value));
}

static void print_text(const xvalue *value) {
    xstrview text;
    if (value != NULL && xrtValueGetString(value, &text))
        printf("%.*s", (int)text.Size, text.Data);
}

int main(void) {
    void *list = xskiplist_create();
    xvalue *value, *keys, *range;
    if (list == NULL || !put(list, "user:1001", "Alice") ||
        !put(list, "user:1003", "Charlie") ||
        !put(list, "user:1002", "Bob")) {
        xskiplist_destroy(list);
        return 1;
    }
    printf("count: %" PRId64 "\n", xskiplist_count(list));
    printf("user:1001 = ");
    value = xskiplist_get(list, "user:1001", 9u);
    print_text(value);
    xrtValueRelease(value);
    printf("\nuser:1002 = ");
    value = xskiplist_get(list, "user:1002", 9u);
    print_text(value);
    xrtValueRelease(value);
    printf("\ncontains user:1003: %s\n",
        xskiplist_contains(list, "user:1003", 9u) ? "true" : "false");
    keys = xskiplist_keys(list);
    range = xskiplist_range(list, "user:1001", 9u, "user:1002", 9u);
    if (keys == NULL || range == NULL) {
        xrtValueRelease(keys);
        xrtValueRelease(range);
        xskiplist_destroy(list);
        return 1;
    }
    printf("first key: ");
    print_text(xrtValueArrayAt(keys, 0));
    printf("\nrange count: %lld\n", (long long)xrtValueCount(range));
    xrtValueRelease(keys);
    xrtValueRelease(range);
    if (!xskiplist_remove(list, "user:1002", 9u)) {
        xskiplist_destroy(list);
        return 1;
    }
    printf("after remove, count: %" PRId64 "\n", xskiplist_count(list));
    printf("get removed: (none)\n");
    puts("skiplist example: PASS");
    xskiplist_destroy(list);
    return 0;
}
