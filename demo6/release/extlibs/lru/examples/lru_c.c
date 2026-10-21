/* 单文件范例包含原生实现；正式包仍按清单分别编译源码。 */
#include "../lru_native.c"
#include <stdio.h>

int main(void)
{
    void* cache = xlru_create(3, 0);
    void* lfu = xlru_create(2, 1);
    char* first;
    char* second;
    if (cache == NULL || lfu == NULL) goto fail;
    if (!xlru_put(cache, "user:1", "Alice") ||
        !xlru_put(cache, "user:2", "Bob") ||
        !xlru_put(cache, "user:3", "Carol")) goto fail;
    first = (char*)xlru_get(cache, "user:1", "");
    second = (char*)xlru_get(cache, "user:2", "");
    if (first == NULL || second == NULL) {
        xrtFree(first);
        xrtFree(second);
        goto fail;
    }
    printf("get user:1 = %s\n", first);
    printf("get user:2 = %s\n", second);
    xrtFree(first);
    xrtFree(second);
    printf("count: %lld  capacity: %lld\n",
        (long long)xlru_count(cache), (long long)xlru_capacity(cache));
    if (!xlru_put(cache, "user:4", "Dave")) goto fail;
    printf("after adding user:4, count: %lld\n", (long long)xlru_count(cache));
    printf("contains user:3 (evicted): %s\n",
        xlru_contains(cache, "user:3") ? "true" : "false");
    printf("hits: %lld  misses: %lld\n",
        (long long)xlru_hits(cache), (long long)xlru_misses(cache));
    if (!xlru_put(lfu, "hot", "data") ||
        !xlru_put(lfu, "cold", "data")) goto fail;
    first = (char*)xlru_get(lfu, "hot", "");
    second = (char*)xlru_get(lfu, "hot", "");
    if (first == NULL || second == NULL) {
        xrtFree(first);
        xrtFree(second);
        goto fail;
    }
    xrtFree(first);
    xrtFree(second);
    if (!xlru_put(lfu, "new", "data")) goto fail;
    printf("LFU keeps hot: %s\n", xlru_contains(lfu, "hot") ? "true" : "false");
    printf("LFU evicts cold: %s\n", xlru_contains(lfu, "cold") ? "true" : "false");
    puts("lru example: PASS");
    xlru_destroy(lfu);
    xlru_destroy(cache);
    return 0;
fail:
    xlru_destroy(lfu);
    xlru_destroy(cache);
    return 1;
}
