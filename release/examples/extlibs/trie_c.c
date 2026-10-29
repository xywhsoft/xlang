#define XRT_MODULE_ALL
#include "../../extlibs/trie/include/trie_native.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static int insert(void *trie, const char *text) {
    return xtrie_insert(trie, text, strlen(text));
}

static const char *flag(bool value) { return value ? "true" : "false"; }

int main(void) {
    void *trie = xtrie_create();
    if (trie == NULL || !insert(trie, "hello") || !insert(trie, "help") ||
        !insert(trie, "hero") || !insert(trie, "world")) {
        xtrie_destroy(trie);
        return 1;
    }
    printf("count: %" PRId64 "\n", xtrie_count(trie));
    printf("contains hello: %s\n", flag(xtrie_contains(trie, "hello", 5u)));
    printf("contains hel: %s\n", flag(xtrie_contains(trie, "hel", 3u)));
    printf("startsWith hel: %s\n", flag(xtrie_starts_with(trie, "hel", 3u)));
    printf("countPrefix he: %" PRId64 "\n", xtrie_count_prefix(trie, "he", 2u));
    printf("countPrefix wor: %" PRId64 "\n", xtrie_count_prefix(trie, "wor", 3u));
    if (!xtrie_remove(trie, "help", 4u)) {
        xtrie_destroy(trie);
        return 1;
    }
    printf("after remove help, count: %" PRId64 "\n", xtrie_count(trie));
    printf("contains help: %s\n", flag(xtrie_contains(trie, "help", 4u)));
    puts("trie example: PASS");
    xtrie_destroy(trie);
    return 0;
}

/* xl run 单文件执行时，TCC 不会读取扩展库构建清单。 */
#ifdef __TINYC__
#include "../../extlibs/trie/trie_native.c"
#endif
