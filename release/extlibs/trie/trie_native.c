/* 按字节建立前缀树：适用于 UTF-8 文本，也保留 C 侧二进制键能力。 */
#include <limits.h>
#include <string.h>

#include "include/trie_native.h"

#define TRIE_DOMAIN "xlang.extlib.trie"

typedef struct TrieNode TrieNode;
typedef struct {
    TrieNode *child;
    unsigned char key;
} TrieEdge;

struct TrieNode {
    TrieNode *parent;
    TrieEdge *edges;
    unsigned short length;
    unsigned short capacity;
    long long words;
    bool terminal;
};

typedef struct {
    TrieNode *root;
    long long count;
} XTrie;

static void trie_error(int kind, const char *message) {
    xrtSetErrorInfo(kind, TRIE_DOMAIN, 0, message);
}

static TrieNode *trie_node(TrieNode *parent) {
    TrieNode *node = (TrieNode *)xrtCalloc(1u, sizeof(*node));
    if (node != NULL) node->parent = parent;
    return node;
}

/* 按父指针回溯，极长单词的销毁也不消耗 C 调用栈。 */
static void trie_free_tree(TrieNode *root) {
    TrieNode *node = root;
    while (node != NULL) {
        TrieNode *parent;
        if (node->length != 0u) {
            node = node->edges[--node->length].child;
            continue;
        }
        parent = node->parent;
        xrtFree(node->edges);
        xrtFree(node);
        node = parent;
    }
}

static unsigned trie_slot(const TrieNode *node, unsigned char key, bool *found) {
    unsigned low = 0u, high = node->length;
    while (low < high) {
        unsigned middle = low + (high - low) / 2u;
        if (node->edges[middle].key < key) low = middle + 1u;
        else high = middle;
    }
    *found = low < node->length && node->edges[low].key == key;
    return low;
}

static TrieNode *trie_child(const TrieNode *node, unsigned char key) {
    bool found;
    unsigned pos = trie_slot(node, key, &found);
    return found ? node->edges[pos].child : NULL;
}

/* 一个 edge 数组只做一次 realloc；失败时原节点保持原样。 */
static bool trie_attach(TrieNode *parent, unsigned char key, TrieNode *child) {
    bool found;
    unsigned pos = trie_slot(parent, key, &found);
    if (found || parent->length == 256u) return false;
    if (parent->length == parent->capacity) {
        unsigned capacity = parent->capacity == 0u ? 4u : parent->capacity * 2u;
        TrieEdge *edges;
        if (capacity > 256u) capacity = 256u;
        edges = (TrieEdge *)xrtRealloc(parent->edges, capacity * sizeof(*edges));
        if (edges == NULL) return false;
        parent->edges = edges;
        parent->capacity = (unsigned short)capacity;
    }
    memmove(parent->edges + pos + 1u, parent->edges + pos,
        (parent->length - pos) * sizeof(*parent->edges));
    parent->edges[pos].key = key;
    parent->edges[pos].child = child;
    parent->length++;
    child->parent = parent;
    return true;
}

static bool trie_input(const XTrie *trie, const char *data, size_t size) {
    xrtClearError();
    if (trie != NULL && (data != NULL || size == 0u)) return true;
    trie_error(XERR_ARGUMENT, "invalid trie or key buffer");
    return false;
}

void *xtrie_create(void) {
    xrtClearError();
    XTrie *trie = (XTrie *)xrtCalloc(1u, sizeof(*trie));
    if (trie == NULL) {
        trie_error(XERR_MEMORY, "cannot allocate trie");
        return NULL;
    }
    trie->root = trie_node(NULL);
    if (trie->root == NULL) {
        xrtFree(trie);
        trie_error(XERR_MEMORY, "cannot allocate trie root");
        return NULL;
    }
    return trie;
}

void xtrie_destroy(void *handle) {
    XTrie *trie = (XTrie *)handle;
    if (trie == NULL) return;
    trie_free_tree(trie->root);
    xrtFree(trie);
}

bool xtrie_insert(void *handle, const char *data, size_t size) {
    XTrie *trie = (XTrie *)handle;
    TrieNode *node, *branch = NULL, *tail;
    size_t i = 0u, j;
    if (!trie_input(trie, data, size)) return false;
    node = trie->root;
    while (i < size) {
        TrieNode *next = trie_child(node, (unsigned char)data[i]);
        if (next == NULL) break;
        node = next;
        i++;
    }
    if (i == size && node->terminal) return true;
    if (trie->count == LLONG_MAX) {
        trie_error(XERR_VALUE, "trie word count overflow");
        return false;
    }
    if (i < size) {
        branch = trie_node(NULL);
        if (branch == NULL) goto memory_failure;
        tail = branch;
        for (j = i + 1u; j < size; ++j) {
            TrieNode *next = trie_node(NULL);
            if (next == NULL) goto memory_failure;
            if (!trie_attach(tail, (unsigned char)data[j], next)) {
                xrtFree(next);
                goto memory_failure;
            }
            tail = next;
        }
        tail->terminal = true;
        while (tail != NULL) {
            tail->words = 1;
            tail = tail->parent;
        }
        if (!trie_attach(node, (unsigned char)data[i], branch)) goto memory_failure;
    } else {
        node->terminal = true;
    }
    /* 接入新分支后才更改计数；失败不会改变已发布的树。 */
    node = trie->root;
    node->words++;
    for (j = 0u; j < i; ++j) {
        node = trie_child(node, (unsigned char)data[j]);
        node->words++;
    }
    trie->count++;
    return true;
memory_failure:
    trie_free_tree(branch);
    trie_error(XERR_MEMORY, "cannot allocate trie node");
    return false;
}

bool xtrie_contains(void *handle, const char *data, size_t size) {
    XTrie *trie = (XTrie *)handle;
    TrieNode *node;
    size_t i;
    if (!trie_input(trie, data, size)) return false;
    node = trie->root;
    for (i = 0u; i < size; ++i) {
        node = trie_child(node, (unsigned char)data[i]);
        if (node == NULL) return false;
    }
    return node->terminal;
}

bool xtrie_starts_with(void *handle, const char *data, size_t size) {
    XTrie *trie = (XTrie *)handle;
    TrieNode *node;
    size_t i;
    if (!trie_input(trie, data, size)) return false;
    node = trie->root;
    for (i = 0u; i < size; ++i) {
        node = trie_child(node, (unsigned char)data[i]);
        if (node == NULL) return false;
    }
    return true;
}

int64_t xtrie_count_prefix(void *handle, const char *data, size_t size) {
    XTrie *trie = (XTrie *)handle;
    TrieNode *node;
    size_t i;
    if (!trie_input(trie, data, size)) return -1;
    node = trie->root;
    for (i = 0u; i < size; ++i) {
        node = trie_child(node, (unsigned char)data[i]);
        if (node == NULL) return 0;
    }
    return node->words;
}

bool xtrie_remove(void *handle, const char *data, size_t size) {
    XTrie *trie = (XTrie *)handle;
    TrieNode *node;
    size_t i;
    if (!trie_input(trie, data, size) ||
        !xtrie_contains(handle, data, size)) return false;
    node = trie->root;
    node->words--;
    for (i = 0u; i < size; ++i) {
        bool found;
        unsigned pos = trie_slot(node, (unsigned char)data[i], &found);
        TrieNode *child = node->edges[pos].child;
        if (child->words == 1) {
            memmove(node->edges + pos, node->edges + pos + 1u,
                (node->length - pos - 1u) * sizeof(*node->edges));
            node->length--;
            child->parent = NULL;
            trie_free_tree(child);
            trie->count--;
            return true;
        }
        child->words--;
        node = child;
    }
    node->terminal = false;
    trie->count--;
    return true;
}

void xtrie_clear(void *handle) {
    XTrie *trie = (XTrie *)handle;
    TrieNode *root;
    xrtClearError();
    if (trie == NULL) {
        trie_error(XERR_ARGUMENT, "invalid trie handle");
        return;
    }
    root = trie->root;
    while (root->length != 0u) {
        TrieNode *child = root->edges[--root->length].child;
        child->parent = NULL;
        trie_free_tree(child);
    }
    xrtFree(root->edges);
    root->edges = NULL;
    root->capacity = 0u;
    root->words = 0;
    root->terminal = false;
    trie->count = 0;
}

int64_t xtrie_count(void *handle) {
    XTrie *trie = (XTrie *)handle;
    xrtClearError();
    if (trie == NULL) trie_error(XERR_ARGUMENT, "invalid trie handle");
    return trie != NULL ? trie->count : 0;
}
