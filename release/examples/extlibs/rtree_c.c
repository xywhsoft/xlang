#include "../../extlibs/rtree/include/rtree_native.h"
#include "../../extlibs/rtree/rtree_native.c"
#include <inttypes.h>
#include <stdio.h>

int main(void) {
    void *tree = xrtree_create();
    struct xvalue *hits;
    if (tree == NULL ||
        !xrtree_insert(tree, 1, 100, 200, 110, 210) ||
        !xrtree_insert(tree, 2, 150, 250, 160, 260) ||
        !xrtree_insert(tree, 3, 105, 205, 115, 215) ||
        !xrtree_insert(tree, 4, 500, 500, 510, 510)) {
        xrtree_destroy(tree);
        return 1;
    }
    hits = xrtree_search(tree, 99, 199, 112, 212);
    if (hits == NULL || xrtValueType(hits) != XVALUE_ARRAY) {
        xrtValueRelease(hits);
        xrtree_destroy(tree);
        return 1;
    }
    printf("hits: %zu\n", xrtValueCount(hits));
    xrtValueRelease(hits);
    xrtree_remove(tree, 1);
    printf("remaining: %" PRId64 "\nrtree example: PASS\n", xrtree_count(tree));
    xrtree_destroy(tree);
    return 0;
}
