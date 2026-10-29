/* 与 scene.xl 同一场景；结果由调用者拥有，清空索引不会使其失效。 */
#include "../rtree_native.c"
#include <inttypes.h>
#include <stdio.h>

#define CHECK(test) do { if (!(test)) goto fail; } while (0)

int main(void) {
    void *scene = xrtree_create();
    xvalue *visible = NULL;
    bool large_id = false;
    size_t i;
    CHECK(scene != NULL);
    CHECK(xrtree_insert(scene, 10, 0, 0, 2, 2));
    CHECK(xrtree_insert(scene, 20, 2, 0, 4, 2));
    CHECK(xrtree_insert(scene, 30, 20, 20, 21, 21));
    CHECK(xrtree_insert(scene, INT64_C(4294967297), 1, 1, 1, 1));
    visible = xrtree_search(scene, 0, 0, 2, 2);
    CHECK(visible != NULL && xrtValueType(visible) == XVALUE_ARRAY);
    CHECK(xrtValueCount(visible) == 3 && xrtree_search_count(scene, 0, 0, 2, 2) == 3);
    for (i = 0; i < xrtValueCount(visible); ++i) {
        int64 id;
        CHECK(xrtValueGetInt(xrtValueArrayGet(visible, i), &id));
        if (id == INT64_C(4294967297)) large_id = true;
    }
    CHECK(large_id);
    printf("visible: %zu\n", xrtValueCount(visible));
    CHECK(xrtree_search_count(scene, 2, 0, 2, 2) == 2);
    printf("edge touch: %" PRId64 "\n", xrtree_search_count(scene, 2, 0, 2, 2));
    /* remove+insert 是两次独立修改；不要把它作为失败原子的移动操作。 */
    CHECK(xrtree_remove(scene, 30) && xrtree_insert(scene, 30, .5, .5, 1.5, 1.5));
    CHECK(xrtree_search_count(scene, 0, 0, 2, 2) == 4);
    printf("after move: %" PRId64 "\n", xrtree_search_count(scene, 0, 0, 2, 2));
    CHECK(!xrtree_remove(scene, 999));
    puts("missing remove: false");
    xrtree_clear(scene);
    CHECK(xrtree_count(scene) == 0 && xrtValueCount(visible) == 3);
    printf("saved results: %zu\n", xrtValueCount(visible));
    CHECK(xrtree_insert(scene, -7, 0, 0, 1, 1));
    CHECK(xrtree_insert(scene, -7, 3, 3, 4, 4));
    CHECK(xrtree_remove(scene, -7) && xrtree_count(scene) == 1);
    printf("duplicate remaining: %" PRId64 "\n", xrtree_count(scene));
    xrtValueRelease(visible);
    xrtree_destroy(scene);
    return 0;
fail:
    fputs("R-tree scene example failed\n", stderr);
    xrtValueRelease(visible);
    xrtree_destroy(scene);
    return 1;
}
