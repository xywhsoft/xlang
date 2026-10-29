/* 与 snapshots.xl 使用相同操作；读取结果是调用者拥有的独立快照。 */
#include "../lru_native.c"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(value) do { if (!(value)) goto fail; } while (0)

int main(void)
{
    void* cache = xlru_create(2, 0);
    void* lfu = xlru_create(2, 1);
    char* saved = NULL;
    char* text = NULL;
    int64_t stored, removed, retained;
    CHECK(cache != NULL && lfu != NULL);
    CHECK(xlru_put(cache, "a", "old"));
    saved = xlru_get(cache, "a", "");
    CHECK(saved != NULL && strcmp(saved, "old") == 0);
    CHECK(xlru_put(cache, "a", "new"));
    text = xlru_get(cache, "a", "");
    CHECK(text != NULL && strcmp(text, "new") == 0 && strcmp(saved, "old") == 0);
    printf("saved: %s\ncurrent: %s\n", saved, text);
    xrtFree(text); text = NULL;
    CHECK(xlru_clear(cache));
    CHECK(strcmp(saved, "old") == 0);
    printf("after clear: %s\n", saved);
    text = xlru_get(cache, "missing", "fallback");
    CHECK(text != NULL && strcmp(text, "fallback") == 0 && xlru_misses(cache) == 1);
    printf("missing: %s\n", text);
    xrtFree(text); text = NULL;
    CHECK(xlru_put(cache, "", ""));
    stored = xlru_contains(cache, "");
    CHECK(stored == 1);
    text = xlru_get(cache, "", "fallback");
    CHECK(text != NULL && strcmp(text, "") == 0);
    xrtFree(text); text = NULL;
    printf("empty stored: %s\n", stored ? "true" : "false");
    removed = xlru_remove(cache, "");
    CHECK(removed == 1);
    printf("empty removed: %s\n", removed ? "true" : "false");
    CHECK(xlru_clear(cache) && xlru_count(cache) == 0 && xlru_hits(cache) == 0 && xlru_misses(cache) == 0);
    CHECK(xlru_capacity(cache) == 2);
    printf("reset: %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 "\n",
        xlru_count(cache), xlru_hits(cache), xlru_misses(cache), xlru_capacity(cache));
    CHECK(xlru_put(lfu, "hot", "h") && xlru_put(lfu, "cold", "c"));
    text = xlru_get(lfu, "hot", "");
    CHECK(text != NULL && strcmp(text, "h") == 0);
    xrtFree(text); text = NULL;
    CHECK(xlru_put(lfu, "new", "n"));
    retained = xlru_contains(lfu, "hot");
    CHECK(retained == 1 && xlru_contains(lfu, "new") == 1 && xlru_contains(lfu, "cold") == 0);
    printf("LFU retains hot: %s\n", retained ? "true" : "false");
    xrtFree(saved);
    xlru_destroy(lfu);
    xlru_destroy(cache);
    return 0;
fail:
    fprintf(stderr, "LRU snapshot example failed\n");
    xrtFree(saved);
    xrtFree(text);
    xlru_destroy(lfu);
    xlru_destroy(cache);
    return 1;
}
