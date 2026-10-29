/* 有序字节串键值表；每个节点只持有自己的一份 key/value。 */
#include <limits.h>
#include <string.h>

#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#include "include/skiplist_native.h"

#define SL_MAX_LEVEL 32
#define SL_DOMAIN "xlang.extlib.skiplist"

typedef struct SLNode {
    char *key;
    char *value;
    size_t keySize;
    size_t valueSize;
    int level;
    struct SLNode *forward[];
} SLNode;

typedef struct {
    SLNode *head;
    int level;
    long long count;
    xrng rng;
} XSkipList;

static void sl_error(int kind, const char *message) {
    xrtSetErrorInfo(kind, SL_DOMAIN, 0, message);
}

static bool sl_input(const XSkipList *list, const char *text, size_t size) {
    xrtClearError();
    if (list != NULL && (text != NULL || size == 0u)) return true;
    sl_error(XERR_ARGUMENT, "invalid skiplist or text buffer");
    return false;
}

static char *sl_copy(const char *text, size_t size) {
    char *copy;
    if (size == SIZE_MAX) return NULL;
    copy = (char *)xrtMalloc(size + 1u);
    if (copy == NULL) return NULL;
    if (size != 0u) memcpy(copy, text, size);
    copy[size] = 0;
    return copy;
}

static SLNode *sl_new_node(int level, const char *key, size_t keySize,
    const char *value, size_t valueSize) {
    SLNode *node = (SLNode *)xrtCalloc(1u,
        sizeof(*node) + (size_t)level * sizeof(node->forward[0]));
    if (node == NULL) return NULL;
    node->level = level;
    if (key == NULL) return node; /* 头节点没有键和值。 */
    node->key = sl_copy(key, keySize);
    node->value = sl_copy(value, valueSize);
    if (node->key == NULL || node->value == NULL) {
        xrtFree(node->key);
        xrtFree(node->value);
        xrtFree(node);
        return NULL;
    }
    node->keySize = keySize;
    node->valueSize = valueSize;
    return node;
}

static void sl_free_node(SLNode *node) {
    if (node == NULL) return;
    xrtFree(node->key);
    xrtFree(node->value);
    xrtFree(node);
}

static int sl_compare(const char *left, size_t leftSize,
    const char *right, size_t rightSize) {
    size_t common = leftSize < rightSize ? leftSize : rightSize;
    int result = common != 0u ? memcmp(left, right, common) : 0;
    if (result != 0) return result;
    return (leftSize > rightSize) - (leftSize < rightSize);
}

static SLNode *sl_find(XSkipList *list, const char *key, size_t keySize,
    SLNode **update) {
    SLNode *node = list->head;
    int i;
    for (i = list->level - 1; i >= 0; --i) {
        while (node->forward[i] != NULL &&
            sl_compare(node->forward[i]->key, node->forward[i]->keySize,
                key, keySize) < 0) node = node->forward[i];
        if (update != NULL) update[i] = node;
    }
    return node->forward[0];
}

static int sl_random_level(xrng *state) {
    int level = 1;
    uint32 bits = xrtRng32(state);
    while (level < SL_MAX_LEVEL && (bits & 1u) == 0u) {
        level++;
        bits >>= 1u;
    }
    return level;
}

void *xskiplist_create(void) {
    uint64 seeds[2];
    xrtClearError();
    XSkipList *list = (XSkipList *)xrtCalloc(1u, sizeof(*list));
    if (list == NULL) {
        sl_error(XERR_MEMORY, "cannot allocate skiplist");
        return NULL;
    }
    list->head = sl_new_node(SL_MAX_LEVEL, NULL, 0u, NULL, 0u);
    if (list->head == NULL) {
        xrtFree(list);
        sl_error(XERR_MEMORY, "cannot allocate skiplist head");
        return NULL;
    }
    list->level = 1;
    /* 每个实例独立播种，避免外部输入按固定层级序列构造退化索引。 */
    if (!xrtSecureRandom(seeds, sizeof(seeds))) {
        sl_free_node(list->head);
        xrtFree(list);
        return NULL;
    }
    xrtRngSeed(&list->rng, seeds[0], seeds[1]);
    return list;
}

void xskiplist_destroy(void *handle) {
    XSkipList *list = (XSkipList *)handle;
    SLNode *node, *next;
    if (list == NULL) return;
    node = list->head->forward[0];
    while (node != NULL) {
        next = node->forward[0];
        sl_free_node(node);
        node = next;
    }
    sl_free_node(list->head);
    xrtFree(list);
}

bool xskiplist_set(void *handle, const char *key, size_t keySize,
    const char *value, size_t valueSize) {
    XSkipList *list = (XSkipList *)handle;
    SLNode *update[SL_MAX_LEVEL], *node;
    xrng nextRng;
    int level, i;
    if (!sl_input(list, key, keySize) || !sl_input(list, value, valueSize)) return false;
    if (key == NULL) key = "";
    if (value == NULL) value = "";
    node = sl_find(list, key, keySize, update);
    if (node != NULL && sl_compare(node->key, node->keySize, key, keySize) == 0) {
        char *replacement = sl_copy(value, valueSize);
        char *old;
        if (replacement == NULL) goto memory_failure;
        old = node->value;
        node->value = replacement;
        node->valueSize = valueSize;
        xrtFree(old);
        return true;
    }
    if (list->count == LLONG_MAX) {
        sl_error(XERR_VALUE, "skiplist item count overflow");
        return false;
    }
    nextRng = list->rng;
    level = sl_random_level(&nextRng);
    node = sl_new_node(level, key, keySize, value, valueSize);
    if (node == NULL) goto memory_failure;
    for (i = list->level; i < level; ++i) update[i] = list->head;
    for (i = 0; i < level; ++i) {
        node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = node;
    }
    if (level > list->level) list->level = level;
    list->rng = nextRng;
    list->count++;
    return true;
memory_failure:
    sl_error(XERR_MEMORY, "cannot allocate skiplist node or value");
    return false;
}

xvalue *xskiplist_get(void *handle, const char *key, size_t keySize) {
    XSkipList *list = (XSkipList *)handle;
    SLNode *node;
    if (!sl_input(list, key, keySize)) return NULL;
    node = sl_find(list, key, keySize, NULL);
    if (node == NULL || sl_compare(node->key, node->keySize, key, keySize) != 0)
        return xrtValueNull();
    return xrtValueString((xstrview){node->value, node->valueSize});
}

bool xskiplist_contains(void *handle, const char *key, size_t keySize) {
    XSkipList *list = (XSkipList *)handle;
    SLNode *node;
    if (!sl_input(list, key, keySize)) return false;
    node = sl_find(list, key, keySize, NULL);
    return node != NULL && sl_compare(node->key, node->keySize, key, keySize) == 0;
}

bool xskiplist_remove(void *handle, const char *key, size_t keySize) {
    XSkipList *list = (XSkipList *)handle;
    SLNode *update[SL_MAX_LEVEL], *node;
    int i;
    if (!sl_input(list, key, keySize)) return false;
    node = sl_find(list, key, keySize, update);
    if (node == NULL || sl_compare(node->key, node->keySize, key, keySize) != 0)
        return false;
    for (i = 0; i < node->level; ++i) update[i]->forward[i] = node->forward[i];
    sl_free_node(node);
    while (list->level > 1 && list->head->forward[list->level - 1] == NULL)
        list->level--;
    list->count--;
    return true;
}

void xskiplist_clear(void *handle) {
    XSkipList *list = (XSkipList *)handle;
    SLNode *node, *next;
    xrtClearError();
    if (list == NULL) {
        sl_error(XERR_ARGUMENT, "invalid skiplist");
        return;
    }
    node = list->head->forward[0];
    while (node != NULL) {
        next = node->forward[0];
        sl_free_node(node);
        node = next;
    }
    memset(list->head->forward, 0, SL_MAX_LEVEL * sizeof(list->head->forward[0]));
    list->level = 1;
    list->count = 0;
}

int64_t xskiplist_count(void *handle) {
    XSkipList *list = (XSkipList *)handle;
    xrtClearError();
    if (list == NULL) {
        sl_error(XERR_ARGUMENT, "invalid skiplist");
        return 0;
    }
    return list->count;
}

/* New 容器 API 消费传入的临时值，失败时只释放外层容器。 */
static bool sl_append_entry(xvalue *result, const SLNode *node, bool includeValue) {
    xvalue *item;
    if (!includeValue) {
        return xrtValueArrayAppendNew(result,
            xrtValueString((xstrview){node->key, node->keySize}));
    }
    item = xrtValueObject();
    if (item == NULL) return false;
    if (!xrtValueObjectSetNew(item, (xstrview){"key", 3},
            xrtValueString((xstrview){node->key, node->keySize})) ||
        !xrtValueObjectSetNew(item, (xstrview){"value", 5},
            xrtValueString((xstrview){node->value, node->valueSize}))) {
        xrtValueRelease(item);
        return false;
    }
    return xrtValueArrayAppendNew(result, item);
}

xvalue *xskiplist_keys(void *handle) {
    XSkipList *list = (XSkipList *)handle;
    xvalue *result;
    SLNode *node;
    xrtClearError();
    if (list == NULL) {
        sl_error(XERR_ARGUMENT, "invalid skiplist");
        return NULL;
    }
    result = xrtValueArray();
    if (result == NULL) return NULL;
    for (node = list->head->forward[0]; node != NULL; node = node->forward[0]) {
        if (!sl_append_entry(result, node, false)) {
            xrtValueRelease(result);
            return NULL;
        }
    }
    return result;
}

xvalue *xskiplist_range(void *handle, const char *start, size_t startSize,
    const char *end, size_t endSize) {
    XSkipList *list = (XSkipList *)handle;
    xvalue *result;
    SLNode *node;
    if (!sl_input(list, start, startSize) || !sl_input(list, end, endSize)) return NULL;
    result = xrtValueArray();
    if (result == NULL) return NULL;
    if (sl_compare(start, startSize, end, endSize) > 0) return result;
    node = sl_find(list, start, startSize, NULL);
    while (node != NULL && sl_compare(node->key, node->keySize, end, endSize) <= 0) {
        if (!sl_append_entry(result, node, true)) {
            xrtValueRelease(result);
            return NULL;
        }
        node = node->forward[0];
    }
    return result;
}
