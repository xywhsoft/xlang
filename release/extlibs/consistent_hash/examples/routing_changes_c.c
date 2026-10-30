/* 与 routing_changes.xl 同一分片扩容；每份查询结果都由调用者 xrtFree。 */
#include "../consistent_hash_native.c"
#include <inttypes.h>
#include <stdio.h>

#define CHECK(test) do { if (!(test)) goto done; } while (0)

int main(void) {
    void *ring = xchash_create(64);
    char *before[128] = {0}, *current = NULL, *held = NULL;
    int i, moved = 0, result = 1;
    char key[32];
    CHECK(ring != NULL && xchash_add_node(ring, "east") && xchash_add_node(ring, "west"));
    for (i = 0; i < 128; ++i) {
        snprintf(key, sizeof(key), "request-%d", i);
        before[i] = xchash_get_node(ring, key);
        CHECK(before[i] != NULL);
    }
    CHECK(xchash_add_node(ring, "central") && xchash_add_node(ring, "central"));
    CHECK(xchash_node_count(ring) == 3 && xchash_vnode_count(ring) == 192);
    for (i = 0; i < 128; ++i) {
        snprintf(key, sizeof(key), "request-%d", i);
        current = xchash_get_node(ring, key);
        CHECK(current != NULL);
        if (strcmp(current, before[i]) != 0) {
            CHECK(strcmp(current, "central") == 0);
            ++moved;
        }
        xrtFree(current); current = NULL;
    }
    CHECK(moved > 0 && moved < 128);
    printf("nodes: %" PRId64 "\nmoved: %d\n", xchash_node_count(ring), moved);
    CHECK(xchash_remove_node(ring, "central"));
    for (i = 0; i < 128; ++i) {
        snprintf(key, sizeof(key), "request-%d", i);
        current = xchash_get_node(ring, key);
        CHECK(current != NULL && strcmp(current, before[i]) == 0);
        xrtFree(current); current = NULL;
    }
    puts("restored: true");
    held = xchash_get_node(ring, "saved");
    CHECK(held != NULL && xchash_remove_node(ring, "east") && xchash_remove_node(ring, "west"));
    CHECK(xchash_remove_node(ring, "missing") && xchash_node_count(ring) == 0 && xchash_vnode_count(ring) == 0);
    current = xchash_get_node(ring, "key");
    CHECK(current != NULL && current[0] == 0);
    xchash_destroy(ring); ring = NULL;
    CHECK(strcmp(held, "east") == 0 || strcmp(held, "west") == 0);
    puts("snapshot survives: true\nempty ring: true");
    result = 0;
done:
    for (i = 0; i < 128; ++i) xrtFree(before[i]);
    xrtFree(current);
    xrtFree(held);
    xchash_destroy(ring);
    return result;
}
