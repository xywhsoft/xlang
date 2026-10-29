/* 只在测试中观察节点：逐层分裂失败必须逐字节保留原树与存活分配。 */
#define XRT_MODULE_MEMORY_DEBUG
#include "../rtree_native.c"
#include <stdio.h>

#define LIMIT 4096
#define CHECK(test) do { ++checks; if (!(test)) { \
    fprintf(stderr, "cascade line %d: %s\n", __LINE__, #test); return 1; \
} } while (0)

typedef struct {
    XrtreeNode *address;
    XrtreeNode value;
} SavedNode;

static unsigned checks;
static SavedNode *saved;
static size_t saved_count;

static int save_nodes(XrtreeNode *node) {
    int i;
    CHECK(saved_count < LIMIT);
    saved[saved_count].address = node;
    memcpy(&saved[saved_count++].value, node, sizeof(*node));
    if (!node->leaf)
        for (i = 0; i < node->count; ++i)
            CHECK(save_nodes(node->entries[i].child) == 0);
    return 0;
}

/* 独立验证结构约束，不用待测的 bounds/count/collect 来证明它们自己。 */
static int inspect(const XrtreeNode *node, int root, int depth,
    int *leaf_depth, int64_t *entries) {
    int i;
    CHECK(node->count >= (root ? 0 : 1) && node->count <= 8);
    if (node->leaf) {
        if (*leaf_depth < 0) *leaf_depth = depth;
        CHECK(*leaf_depth == depth);
        *entries += node->count;
        return 0;
    }
    CHECK(node->count >= (root ? 2 : 1));
    for (i = 0; i < node->count; ++i) {
        const XrtreeNode *child = node->entries[i].child;
        XrtreeRect bounds;
        int j;
        CHECK(child != NULL && child->count > 0);
        bounds = child->entries[0].rect;
        for (j = 1; j < child->count; ++j) {
            XrtreeRect rect = child->entries[j].rect;
            if (rect.minx < bounds.minx) bounds.minx = rect.minx;
            if (rect.miny < bounds.miny) bounds.miny = rect.miny;
            if (rect.maxx > bounds.maxx) bounds.maxx = rect.maxx;
            if (rect.maxy > bounds.maxy) bounds.maxy = rect.maxy;
        }
        CHECK(memcmp(&bounds, &node->entries[i].rect, sizeof(bounds)) == 0);
        CHECK(inspect(child, 0, depth + 1, leaf_depth, entries) == 0);
    }
    return 0;
}

static int check_ids(Xrtree *tree, int count) {
    unsigned char seen[LIMIT] = {0};
    xvalue *all = xrtree_search(tree, -1, -1, LIMIT + 1, 2);
    size_t i;
    CHECK(all != NULL && xrtValueType(all) == XVALUE_ARRAY);
    CHECK(xrtValueCount(all) == (size_t)count);
    for (i = 0; i < xrtValueCount(all); ++i) {
        int64 id;
        CHECK(xrtValueGetInt(xrtValueArrayGet(all, i), &id));
        CHECK(id >= 0 && id < count && !seen[id]);
        seen[id] = 1;
    }
    xrtValueRelease(all);
    return 0;
}

static xmemdebugsnapshot memory(void) {
    xmemdebugsnapshot result;
    xrtMemDebugSnapshot(&result);
    return result;
}

static int same_memory(xmemdebugsnapshot a, xmemdebugsnapshot b) {
    return a.LiveCount == b.LiveCount && a.LiveBytes == b.LiveBytes &&
        a.InvalidFreeCount == b.InvalidFreeCount &&
        a.DoubleFreeCount == b.DoubleFreeCount &&
        a.OverflowCount == b.OverflowCount &&
        a.UnderflowCount == b.UnderflowCount;
}

static int exercise(int spread) {
    Xrtree *tree = xrtree_create();
    unsigned tested[5] = {0};
    int count;
    CHECK(tree != NULL);
    for (count = 0; count < LIMIT; ++count) {
        XrtreeRect rect = {spread ? count : 0, 0, spread ? count + 1 : 1, 1};
        XrtreeNode *path[64], *node = tree->root;
        int depth = 0, need = 0, carry = 1, i;
        while (node != NULL) {
            CHECK(depth < 64);
            path[depth++] = node;
            if (node->leaf) break;
            node = node->entries[xrtree_choose(node, rect)].child;
        }
        for (i = depth - 1; i >= 0 && carry; --i) {
            if (path[i]->count == 8) ++need;
            else carry = 0;
        }
        if (carry) ++need;
        if (need >= 2 && need <= 4 && tested[need] < 2) {
            Xrtree old = *tree;
            int budget;
            saved_count = 0;
            CHECK(save_nodes(tree->root) == 0);
            for (budget = 0; budget < need; ++budget) {
                xmemdebugsnapshot before = memory();
                int triggered, kind;
                CHECK(before.Enabled && xrtMemDebugFailAfter((uint64)budget));
                CHECK(!xrtree_insert(tree, count, rect.minx, 0, rect.maxx, 1));
                triggered = xrtMemDebugFailTriggered();
                kind = xrtErrorKind(xrtGetError());
                xrtMemDebugFailClear();
                CHECK(triggered && kind == XERR_MEMORY);
                CHECK(memcmp(&old, tree, sizeof(old)) == 0);
                for (i = 0; i < (int)saved_count; ++i)
                    CHECK(memcmp(saved[i].address, &saved[i].value, sizeof(XrtreeNode)) == 0);
                CHECK(same_memory(before, memory()));
                CHECK(check_ids(tree, count) == 0);
            }
            /* 同一插入立即恢复；预算恰好够用时也必须成功且不触发故障。 */
            CHECK(xrtMemDebugFailAfter((uint64)need));
            CHECK(xrtree_insert(tree, count, rect.minx, 0, rect.maxx, 1));
            CHECK(!xrtMemDebugFailTriggered());
            xrtMemDebugFailClear();
            CHECK(check_ids(tree, count + 1) == 0);
            ++tested[need];
        } else {
            CHECK(xrtree_insert(tree, count, rect.minx, 0, rect.maxx, 1));
        }
        {
            int leaf_depth = -1;
            int64_t entries = 0;
            CHECK(inspect(tree->root, 1, 0, &leaf_depth, &entries) == 0);
            CHECK(entries == count + 1 && tree->size == entries);
        }
        if (tested[2] == 2 && tested[3] == 2 && tested[4] == 2) break;
    }
    CHECK(count < LIMIT);
    printf("cascade %s: allocations 2/3/4, two events each, %d entries\n",
        spread ? "ordered" : "overlapping", count + 1);
    xrtree_destroy(tree);
    return 0;
}

static int wide_ids(void) {
    static const int64_t ids[] = {INT64_MIN, INT64_MAX, -1, 0,
        INT64_C(4294967297), INT64_MIN, INT64_MAX, -1, 0};
    void *tree = xrtree_create();
    xvalue *held;
    size_t i, j;
    CHECK(tree != NULL);
    for (i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i)
        CHECK(xrtree_insert(tree, ids[i], 0, 0, 1, 1));
    held = xrtree_search(tree, 0, 0, 1, 1);
    CHECK(held != NULL && xrtValueType(held) == XVALUE_ARRAY && xrtValueCount(held) == 9);
    for (i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i) {
        int expected = 0, actual = 0;
        for (j = 0; j < sizeof(ids) / sizeof(ids[0]); ++j) expected += ids[j] == ids[i];
        for (j = 0; j < xrtValueCount(held); ++j) {
            int64 id;
            CHECK(xrtValueGetInt(xrtValueArrayGet(held, j), &id));
            actual += id == ids[i];
        }
        CHECK(actual == expected);
    }
    /* 删除不分配，重复 ID 每次仅删一项；结果独立于树的后续生命周期。 */
    CHECK(xrtMemDebugFailAfter(0));
    for (i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i) {
        CHECK(xrtree_remove(tree, ids[i]));
        CHECK(xrtree_count(tree) == (int64_t)(8 - i));
    }
    CHECK(!xrtree_remove(tree, INT64_MIN) && xrtGetError() == NULL);
    CHECK(xrtree_search_count(tree, 0, 0, 1, 1) == 0);
    xrtree_clear(tree);
    xrtree_destroy(tree);
    CHECK(!xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    CHECK(xrtValueCount(held) == 9);
    xrtValueRelease(held);
    return 0;
}

int main(void) {
    xmemdebugsnapshot before, after;
    /* 先预热线程错误槽；快照缓冲在注入窗口之前分配，不干扰被测预算。 */
    xrtSetErrorInfo(XERR_VALUE, "rtree.test", 0, "warm");
    xrtClearError();
    saved = (SavedNode *)xrtMalloc(sizeof(*saved) * LIMIT);
    CHECK(saved != NULL);
    before = memory();
    CHECK(before.Enabled);
    CHECK(exercise(0) == 0);
    CHECK(exercise(1) == 0);
    CHECK(wide_ids() == 0);
    after = memory();
    CHECK(same_memory(before, after));
    xrtFree(saved);
    printf("rtree cascade rollback: %u checks, PASS\n", checks);
    return 0;
}
