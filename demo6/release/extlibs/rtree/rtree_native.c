/* 平面矩形 R-tree：每节点最多 8 项，二次分裂；不同树没有共享可变状态。 */
#include <float.h>
#include <limits.h>
#include <string.h>

#ifndef XRT_MODULE_MATH
#define XRT_MODULE_MATH
#endif
#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#include "xrt.h"
#include "include/rtree_native.h"

#define XRTREE_DOMAIN "xlang.extlib.rtree"
#define XRTREE_MAX_ENTRIES 8
#define XRTREE_MIN_ENTRIES 4
#define XRTREE_MAX_HEIGHT 64

typedef struct {
    double minx, miny, maxx, maxy;
} XrtreeRect;

typedef struct XrtreeNode XrtreeNode;
typedef struct {
    XrtreeRect rect;
    long long id;
    XrtreeNode *child;
} XrtreeEntry;

struct XrtreeNode {
    int leaf;
    int count;
    XrtreeEntry entries[XRTREE_MAX_ENTRIES + 1];
};

typedef struct {
    XrtreeNode *root;
    long long size;
} Xrtree;

static void xrtree_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XRTREE_DOMAIN, 0, message);
}

static int xrtree_rect_valid(XrtreeRect rect) {
    return xrtMathIsFinite(rect.minx) && xrtMathIsFinite(rect.miny) &&
        xrtMathIsFinite(rect.maxx) && xrtMathIsFinite(rect.maxy) &&
        rect.minx <= rect.maxx && rect.miny <= rect.maxy;
}

static XrtreeRect xrtree_combine(XrtreeRect a, XrtreeRect b) {
    XrtreeRect result = {
        a.minx < b.minx ? a.minx : b.minx,
        a.miny < b.miny ? a.miny : b.miny,
        a.maxx > b.maxx ? a.maxx : b.maxx,
        a.maxy > b.maxy ? a.maxy : b.maxy
    };
    return result;
}

static long double xrtree_area(XrtreeRect rect) {
    long double width = (long double)rect.maxx - rect.minx;
    long double height = (long double)rect.maxy - rect.miny;
    /* 面积只用于分裂启发式；使用所有目标均能表示的上限避免 inf - inf。 */
    if (width == 0 || height == 0) return 0;
    if (width > DBL_MAX || height > DBL_MAX ||
        width > (long double)DBL_MAX / height) return DBL_MAX;
    return width * height;
}

static long double xrtree_enlargement(XrtreeRect bounds, XrtreeRect item) {
    return xrtree_area(xrtree_combine(bounds, item)) - xrtree_area(bounds);
}

static int xrtree_intersects(XrtreeRect a, XrtreeRect b) {
    return a.minx <= b.maxx && a.maxx >= b.minx &&
        a.miny <= b.maxy && a.maxy >= b.miny;
}

static XrtreeNode *xrtree_node_new(int leaf) {
    XrtreeNode *node = (XrtreeNode *)xrtMalloc(sizeof(*node));
    if (node == NULL) return NULL;
    memset(node, 0, sizeof(*node));
    node->leaf = leaf;
    return node;
}

static void xrtree_node_free(XrtreeNode *node) {
    int index;
    if (node == NULL) return;
    if (!node->leaf) {
        for (index = 0; index < node->count; ++index)
            xrtree_node_free(node->entries[index].child);
    }
    xrtFree(node);
}

static XrtreeRect xrtree_bounds(const XrtreeNode *node) {
    XrtreeRect bounds = node->entries[0].rect;
    int index;
    for (index = 1; index < node->count; ++index)
        bounds = xrtree_combine(bounds, node->entries[index].rect);
    return bounds;
}

static int xrtree_choose(const XrtreeNode *node, XrtreeRect rect) {
    int index, best = 0;
    long double bestGrowth = xrtree_enlargement(node->entries[0].rect, rect);
    long double bestArea = xrtree_area(node->entries[0].rect);
    for (index = 1; index < node->count; ++index) {
        long double growth = xrtree_enlargement(node->entries[index].rect, rect);
        long double area = xrtree_area(node->entries[index].rect);
        if (growth < bestGrowth || (growth == bestGrowth && area < bestArea)) {
            best = index;
            bestGrowth = growth;
            bestArea = area;
        }
    }
    return best;
}

/* 9 项分成至少 4+4；节点已预分配，因此分裂不会在树更新中途失败。 */
static void xrtree_split(XrtreeNode *node, XrtreeNode *sibling) {
    XrtreeEntry items[XRTREE_MAX_ENTRIES + 1];
    int assigned[XRTREE_MAX_ENTRIES + 1] = {0};
    int first = 0, second = 1, remaining = XRTREE_MAX_ENTRIES - 1;
    long double worst = -1.0e300L;
    int i, j;
    memcpy(items, node->entries, sizeof(items));
    sibling->leaf = node->leaf;
    sibling->count = 0;
    for (i = 0; i < XRTREE_MAX_ENTRIES + 1; ++i) {
        for (j = i + 1; j < XRTREE_MAX_ENTRIES + 1; ++j) {
            long double waste = xrtree_area(xrtree_combine(items[i].rect,
                items[j].rect)) - xrtree_area(items[i].rect) -
                xrtree_area(items[j].rect);
            if (waste > worst) { worst = waste; first = i; second = j; }
        }
    }
    node->count = 0;
    node->entries[node->count++] = items[first];
    sibling->entries[sibling->count++] = items[second];
    assigned[first] = assigned[second] = 1;
    while (remaining > 0) {
        int pick = -1, group = 0;
        long double biggestDifference = -1;
        XrtreeRect left = xrtree_bounds(node);
        XrtreeRect right = xrtree_bounds(sibling);
        for (i = 0; i < XRTREE_MAX_ENTRIES + 1; ++i) {
            long double leftGrowth, rightGrowth, difference;
            if (assigned[i]) continue;
            leftGrowth = xrtree_enlargement(left, items[i].rect);
            rightGrowth = xrtree_enlargement(right, items[i].rect);
            difference = leftGrowth > rightGrowth ?
                leftGrowth - rightGrowth : rightGrowth - leftGrowth;
            if (difference > biggestDifference) {
                biggestDifference = difference;
                pick = i;
                group = leftGrowth < rightGrowth ? 0 :
                    (rightGrowth < leftGrowth ? 1 :
                    (xrtree_area(left) <= xrtree_area(right) ? 0 : 1));
            }
        }
        if (node->count + remaining == XRTREE_MIN_ENTRIES) group = 0;
        if (sibling->count + remaining == XRTREE_MIN_ENTRIES) group = 1;
        if (group == 0) node->entries[node->count++] = items[pick];
        else sibling->entries[sibling->count++] = items[pick];
        assigned[pick] = 1;
        --remaining;
    }
}

static XrtreeNode *xrtree_insert_node(XrtreeNode *node, XrtreeRect rect,
    long long id, XrtreeNode **spares, int *used) {
    if (node->leaf) {
        XrtreeEntry item = {rect, id, NULL};
        node->entries[node->count++] = item;
    } else {
        int index = xrtree_choose(node, rect);
        XrtreeNode *sibling = xrtree_insert_node(node->entries[index].child,
            rect, id, spares, used);
        node->entries[index].rect = xrtree_bounds(node->entries[index].child);
        if (sibling != NULL) {
            XrtreeEntry item = {xrtree_bounds(sibling), 0, sibling};
            node->entries[node->count++] = item;
        }
    }
    if (node->count <= XRTREE_MAX_ENTRIES) return NULL;
    xrtree_split(node, spares[*used]);
    return spares[(*used)++];
}

static int xrtree_remove_node(XrtreeNode *node, long long id) {
    int index;
    for (index = 0; index < node->count; ++index) {
        if (node->leaf ? node->entries[index].id == id :
            xrtree_remove_node(node->entries[index].child, id)) {
            if (!node->leaf && node->entries[index].child->count != 0) {
                node->entries[index].rect =
                    xrtree_bounds(node->entries[index].child);
            } else {
                if (!node->leaf) xrtree_node_free(node->entries[index].child);
                memmove(&node->entries[index], &node->entries[index + 1],
                    (size_t)(node->count - index - 1) * sizeof(node->entries[0]));
                --node->count;
            }
            return 1;
        }
    }
    return 0;
}

static long long xrtree_count_matches(const XrtreeNode *node,
    XrtreeRect query) {
    long long count = 0;
    int index;
    for (index = 0; index < node->count; ++index) {
        if (!xrtree_intersects(node->entries[index].rect, query)) continue;
        count += node->leaf ? 1 :
            xrtree_count_matches(node->entries[index].child, query);
    }
    return count;
}

static int xrtree_collect(const XrtreeNode *node, XrtreeRect query,
    xvalue *results) {
    int index;
    for (index = 0; index < node->count; ++index) {
        if (!xrtree_intersects(node->entries[index].rect, query)) continue;
        if (node->leaf) {
            if (!xrtValueArrayAppendNew(results,
                xrtValueInt(node->entries[index].id))) return 0;
        } else if (!xrtree_collect(node->entries[index].child, query,
            results)) return 0;
    }
    return 1;
}

/* 公开 C 接口：句柄属于调用方；查询数组用 xrtValueRelease 释放。 */
void *xrtree_create(void) {
    Xrtree *tree;
    xrtClearError();
    tree = (Xrtree *)xrtMalloc(sizeof(*tree));
    if (tree == NULL) return NULL;
    tree->root = xrtree_node_new(1);
    tree->size = 0;
    if (tree->root == NULL) { xrtFree(tree); return NULL; }
    return tree;
}

void xrtree_destroy(void *handle) {
    Xrtree *tree = (Xrtree *)handle;
    if (tree == NULL) return;
    xrtree_node_free(tree->root);
    xrtFree(tree);
}

bool xrtree_insert(void *handle, int64_t id,
    double minx, double miny, double maxx, double maxy) {
    Xrtree *tree = (Xrtree *)handle;
    XrtreeRect rect = {minx, miny, maxx, maxy};
    XrtreeNode *path[XRTREE_MAX_HEIGHT];
    XrtreeNode *spares[XRTREE_MAX_HEIGHT + 1];
    XrtreeNode *node;
    int depth = 0, need = 0, carry = 1, used = 0, index;
    xrtClearError();
    if (tree == NULL || !xrtree_rect_valid(rect) || tree->size == LLONG_MAX) {
        xrtree_error("invalid tree, rectangle, or entry count");
        return false;
    }
    node = tree->root;
    while (node != NULL) {
        if (depth == XRTREE_MAX_HEIGHT) {
            xrtree_error("R-tree height limit exceeded");
            return false;
        }
        path[depth++] = node;
        if (node->leaf) break;
        node = node->entries[xrtree_choose(node, rect)].child;
    }
    for (index = depth - 1; index >= 0 && carry; --index) {
        if (path[index]->count == XRTREE_MAX_ENTRIES) ++need;
        else carry = 0;
    }
    if (carry) {
        if (depth == XRTREE_MAX_HEIGHT) {
            xrtree_error("R-tree height limit exceeded");
            return false;
        }
        ++need;
    }
    for (index = 0; index < need; ++index) {
        spares[index] = xrtree_node_new(0);
        if (spares[index] == NULL) {
            while (index > 0) xrtFree(spares[--index]);
            return false;
        }
    }
    node = xrtree_insert_node(tree->root, rect, id, spares, &used);
    if (node != NULL) {
        XrtreeNode *root = spares[used++];
        root->entries[0] = (XrtreeEntry){xrtree_bounds(tree->root), 0, tree->root};
        root->entries[1] = (XrtreeEntry){xrtree_bounds(node), 0, node};
        root->count = 2;
        tree->root = root;
    }
    while (used < need) xrtFree(spares[used++]);
    ++tree->size;
    return true;
}

struct xvalue *xrtree_search(void *handle,
    double minx, double miny, double maxx, double maxy) {
    Xrtree *tree = (Xrtree *)handle;
    XrtreeRect query = {minx, miny, maxx, maxy};
    xvalue *results;
    xrtClearError();
    if (tree == NULL || !xrtree_rect_valid(query)) {
        xrtree_error("invalid tree or query rectangle");
        return xrtValueNull();
    }
    results = xrtValueArray();
    if (results == NULL) return xrtValueNull();
    if (!xrtree_collect(tree->root, query, results)) {
        xrtValueRelease(results);
        return xrtValueNull();
    }
    return results;
}

int64_t xrtree_search_count(void *handle,
    double minx, double miny, double maxx, double maxy) {
    Xrtree *tree = (Xrtree *)handle;
    XrtreeRect query = {minx, miny, maxx, maxy};
    xrtClearError();
    if (tree == NULL || !xrtree_rect_valid(query)) {
        xrtree_error("invalid tree or query rectangle");
        return -1;
    }
    return xrtree_count_matches(tree->root, query);
}

bool xrtree_remove(void *handle, int64_t id) {
    Xrtree *tree = (Xrtree *)handle;
    xrtClearError();
    if (tree == NULL) { xrtree_error("invalid tree handle"); return false; }
    if (!xrtree_remove_node(tree->root, id)) return false;
    --tree->size;
    while (!tree->root->leaf && tree->root->count == 1) {
        XrtreeNode *old = tree->root;
        tree->root = old->entries[0].child;
        xrtFree(old);
    }
    if (tree->root->count == 0) tree->root->leaf = 1;
    return true;
}

int64_t xrtree_count(void *handle) {
    Xrtree *tree = (Xrtree *)handle;
    xrtClearError();
    if (tree != NULL) return tree->size;
    xrtree_error("invalid tree handle");
    return -1;
}

void xrtree_clear(void *handle) {
    Xrtree *tree = (Xrtree *)handle;
    int index;
    xrtClearError();
    if (tree == NULL) { xrtree_error("invalid tree handle"); return; }
    if (!tree->root->leaf) {
        for (index = 0; index < tree->root->count; ++index)
            xrtree_node_free(tree->root->entries[index].child);
    }
    memset(tree->root, 0, sizeof(*tree->root));
    tree->root->leaf = 1;
    tree->size = 0;
}
