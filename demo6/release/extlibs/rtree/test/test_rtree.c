#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE_CONTAINER
#include "../include/rtree_native.h"
#include "xrt.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define ITEMS 2048
#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "rtree C test failed at line %d: %s\n", __LINE__, #test); \
    return 1; \
} } while (0)

typedef struct {
    double x0, y0, x1, y1;
    int alive;
} TestRect;

static unsigned int seed = 0x58242u;
static unsigned int next_random(void) {
    seed = seed * 1664525u + 1013904223u;
    return seed;
}

static int intersects(TestRect r, double x0, double y0, double x1, double y1) {
    return r.x0 <= x1 && r.x1 >= x0 && r.y0 <= y1 && r.y1 >= y0;
}

static int check_query(void *tree, const TestRect *rects,
    double x0, double y0, double x1, double y1) {
    struct xvalue *matches = xrtree_search(tree, x0, y0, x1, y1);
    unsigned char seen[ITEMS] = {0};
    long long count = xrtree_search_count(tree, x0, y0, x1, y1);
    long long expected = 0;
    size_t index;
    int item;
    CHECK(matches != NULL && xrtValueType(matches) == XVALUE_ARRAY);
    for (item = 0; item < ITEMS; ++item) {
        if (rects[item].alive && intersects(rects[item], x0, y0, x1, y1))
            ++expected;
    }
    CHECK(count == expected && xrtValueCount(matches) == (size_t)expected);
    for (index = 0; index < xrtValueCount(matches); ++index) {
        int64 id;
        CHECK(xrtValueGetInt(xrtValueArrayGet(matches, index), &id));
        CHECK(id >= 0 && id < ITEMS && !seen[id]);
        CHECK(rects[id].alive && intersects(rects[id], x0, y0, x1, y1));
        seen[id] = 1;
    }
    xrtValueRelease(matches);
    return 0;
}

static int test_extreme_coordinates(void) {
    void *tree = xrtree_create();
    xvalue *matches;
    int seen[16] = {0};
    int id;
    CHECK(tree != NULL);
    for (id = 0; id < 16; ++id)
        CHECK(xrtree_insert(tree, id, -1.0e308, -1.0e308,
            1.0e308, 1.0e308));
    CHECK(xrtree_count(tree) == 16);
    CHECK(xrtree_search_count(tree, -1.0e308, -1.0e308,
        1.0e308, 1.0e308) == 16);
    matches = xrtree_search(tree, -1.0e308, -1.0e308, 1.0e308, 1.0e308);
    CHECK(matches != NULL && xrtValueType(matches) == XVALUE_ARRAY &&
        xrtValueCount(matches) == 16);
    for (id = 0; id < 16; ++id) {
        int64 value;
        CHECK(xrtValueGetInt(xrtValueArrayAt(matches, id), &value));
        CHECK(value >= 0 && value < 16 && !seen[value]);
        seen[value] = 1;
    }
    xrtValueRelease(matches);
    for (id = 0; id < 16; ++id) CHECK(xrtree_remove(tree, id));
    CHECK(xrtree_count(tree) == 0);
    xrtree_destroy(tree);
    return 0;
}

static int test_error_contract(void) {
    void *tree;
    xvalue *matches;
    int failure;
    int saw_failure = 0;
    int saw_later_failure = 0;
    int saw_success = 0;
    xrtSetErrorInfo(XERR_VALUE, "rtree.test", 1, "stale");
    tree = xrtree_create();
    CHECK(tree != NULL && xrtGetError() == NULL);

    xrtSetErrorInfo(XERR_VALUE, "rtree.test", 1, "stale");
    CHECK(xrtree_count(tree) == 0 && xrtGetError() == NULL);
    xrtSetErrorInfo(XERR_VALUE, "rtree.test", 1, "stale");
    CHECK(xrtree_insert(tree, 1, 0, 0, 1, 1) && xrtGetError() == NULL);
    xrtSetErrorInfo(XERR_VALUE, "rtree.test", 1, "stale");
    CHECK(xrtree_search_count(tree, 0, 0, 1, 1) == 1 && xrtGetError() == NULL);
    xrtSetErrorInfo(XERR_VALUE, "rtree.test", 1, "stale");
    matches = xrtree_search(tree, 0, 0, 1, 1);
    CHECK(matches != NULL && xrtValueType(matches) == XVALUE_ARRAY &&
        xrtGetError() == NULL);
    xrtValueRelease(matches);
    xrtSetErrorInfo(XERR_VALUE, "rtree.test", 1, "stale");
    CHECK(!xrtree_remove(tree, 99) && xrtGetError() == NULL);
    xrtSetErrorInfo(XERR_VALUE, "rtree.test", 1, "stale");
    xrtree_clear(tree);
    CHECK(xrtGetError() == NULL && xrtree_count(tree) == 0);
    CHECK(xrtree_insert(tree, 1, 0, 0, 1, 1));
    for (failure = 0; failure < 16; ++failure) {
        int triggered;
        CHECK(xrtMemDebugFailAfter((uint64)failure));
        matches = xrtree_search(tree, 0, 0, 1, 1);
        triggered = xrtMemDebugFailTriggered();
        if (triggered) {
            CHECK(matches != NULL && xrtValueType(matches) == XVALUE_NULL &&
                xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            ++saw_failure;
            if (failure > 0) ++saw_later_failure;
        } else {
            CHECK(matches != NULL && xrtValueType(matches) == XVALUE_ARRAY &&
                xrtValueCount(matches) == 1 && xrtGetError() == NULL);
            ++saw_success;
        }
        xrtValueRelease(matches);
        xrtMemDebugFailClear();
    }
    CHECK(saw_failure > 0 && saw_later_failure > 0 && saw_success > 0);
    matches = xrtree_search(tree, 0, 0, 1, 1);
    CHECK(matches != NULL && xrtValueType(matches) == XVALUE_ARRAY &&
        xrtValueCount(matches) == 1 && xrtGetError() == NULL);
    xrtValueRelease(matches);
    xrtree_destroy(tree);
    return 0;
}

int main(void) {
    TestRect rects[ITEMS];
    void *tree;
    void *fault_tree;
    int index;
    CHECK(test_error_contract() == 0);
    for (index = 0; index < 2; ++index) {
        CHECK(xrtMemDebugFailAfter((uint64)index));
        CHECK(xrtree_create() == NULL);
        CHECK(xrtMemDebugFailTriggered());
        xrtMemDebugFailClear();
        xrtClearError();
    }
    tree = xrtree_create();
    CHECK(tree != NULL);
    memset(rects, 0, sizeof(rects));
    CHECK(xrtree_count(tree) == 0);
    for (index = 0; index < ITEMS; ++index) {
        double x = (double)(next_random() % 1000u);
        double y = (double)(next_random() % 1000u);
        rects[index] = (TestRect){x, y, x + (index % 19), y + (index % 13), 1};
        CHECK(xrtree_insert(tree, index, rects[index].x0, rects[index].y0,
            rects[index].x1, rects[index].y1));
    }
    CHECK(xrtree_count(tree) == ITEMS);
    CHECK(check_query(tree, rects, -1, -1, 2000, 2000) == 0);
    for (index = 0; index < 500; ++index) {
        double x = (double)(next_random() % 1000u);
        double y = (double)(next_random() % 1000u);
        CHECK(check_query(tree, rects, x, y, x + 70, y + 70) == 0);
    }
    for (index = 0; index < ITEMS; index += 3) {
        CHECK(xrtree_remove(tree, index));
        rects[index].alive = 0;
    }
    CHECK(!xrtree_remove(tree, -999));
    for (index = 0; index < 500; ++index) {
        double x = (double)(next_random() % 1000u);
        double y = (double)(next_random() % 1000u);
        CHECK(check_query(tree, rects, x, y, x + 70, y + 70) == 0);
    }
    for (index = 0; index < ITEMS; index += 3) {
        CHECK(xrtree_insert(tree, index, rects[index].x0, rects[index].y0,
            rects[index].x1, rects[index].y1));
        rects[index].alive = 1;
    }
    CHECK(check_query(tree, rects, -1, -1, 2000, 2000) == 0);

    CHECK(!xrtree_insert(tree, 9999, 2, 2, 1, 3));
    xrtClearError();
    CHECK(!xrtree_insert(tree, 9999, NAN, 0, 1, 1));
    xrtClearError();
    CHECK(xrtree_search_count(tree, 2, 2, 1, 3) == -1);
    xrtClearError();
    {
        struct xvalue *bad = xrtree_search(tree, 2, 2, 1, 3);
        CHECK(bad != NULL && xrtValueType(bad) == XVALUE_NULL);
        xrtValueRelease(bad);
        xrtClearError();
    }
    CHECK(xrtree_count(tree) == ITEMS);

    for (index = ITEMS - 1; index >= 0; --index) {
        CHECK(xrtree_remove(tree, index));
        rects[index].alive = 0;
    }
    CHECK(xrtree_count(tree) == 0);
    CHECK(check_query(tree, rects, -1, -1, 2000, 2000) == 0);
    CHECK(xrtree_insert(tree, 42, 10, 10, 11, 11));
    CHECK(xrtree_search_count(tree, 10, 10, 11, 11) == 1);

    xrtree_clear(tree);
    CHECK(xrtree_count(tree) == 0);
    CHECK(xrtree_insert(tree, -7, 0, 0, 1, 1));
    CHECK(xrtree_insert(tree, -7, 2, 2, 3, 3));
    CHECK(xrtree_count(tree) == 2);
    CHECK(xrtree_remove(tree, -7) && xrtree_count(tree) == 1);
    CHECK(xrtree_remove(tree, -7) && xrtree_count(tree) == 0);
    xrtree_destroy(tree);

    /* 根节点分裂需要两个新节点；任一预分配失败都不能改变原树。 */
    fault_tree = xrtree_create();
    CHECK(fault_tree != NULL);
    for (index = 0; index < 8; ++index)
        CHECK(xrtree_insert(fault_tree, index, index, 0, index + 1, 1));
    for (index = 0; index < 2; ++index) {
        CHECK(xrtMemDebugFailAfter((uint64)index));
        CHECK(!xrtree_insert(fault_tree, 8, 8, 0, 9, 1));
        CHECK(xrtMemDebugFailTriggered());
        CHECK(xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        xrtMemDebugFailClear();
        xrtClearError();
        CHECK(xrtree_count(fault_tree) == 8);
        CHECK(xrtree_search_count(fault_tree, -1, -1, 20, 2) == 8);
    }
    CHECK(xrtree_insert(fault_tree, 8, 8, 0, 9, 1));
    CHECK(xrtree_count(fault_tree) == 9);
    xrtree_destroy(fault_tree);
    CHECK(test_extreme_coordinates() == 0);
    puts("rtree C oracle: PASS");
    return 0;
}
