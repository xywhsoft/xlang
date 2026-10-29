/* 单文件范例包含原生实现；正式包仍按清单分别编译源码。 */
#include "../consistent_hash_native.c"
#include <stdio.h>

static int show(void* ring, const char* label, const char* key)
{
    char* node = (char*)xchash_get_node(ring, key);
    if (node == NULL) return 0;
    printf("%s -> %s\n", label, node);
    xrtFree(node);
    return 1;
}

int main(void)
{
    void* ring = xchash_create(150);
    if (ring == NULL) return 1;
    if (!xchash_add_node(ring, "node-1") ||
        !xchash_add_node(ring, "node-2") ||
        !xchash_add_node(ring, "node-3")) goto fail;

    printf("nodes: %lld  vnodes: %lld\n",
        (long long)xchash_node_count(ring), (long long)xchash_vnode_count(ring));
    if (!show(ring, "user:100", "user:100") ||
        !show(ring, "user:200", "user:200") ||
        !show(ring, "user:300", "user:300") ||
        !show(ring, "order:42 ", "order:42")) goto fail;

    if (!xchash_remove_node(ring, "node-2")) goto fail;
    puts("after removing node-2:");
    if (!show(ring, "user:100", "user:100") ||
        !show(ring, "user:200", "user:200")) goto fail;
    puts("consistent_hash example: PASS");
    xchash_destroy(ring);
    return 0;
fail:
    xchash_destroy(ring);
    return 1;
}
