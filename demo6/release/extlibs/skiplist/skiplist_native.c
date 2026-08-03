/*
 * skiplist_native.c - Skip List ordered key-value store
 * O(log n) insert/lookup/delete with ordered traversal.
 */
#include <stdlib.h>
#include <string.h>
extern void xrtSetError(const void *msg, int bFree);
static char sl_buf[4096];

#define SL_MAX_LEVEL 32
#define SL_PROB 0.5

typedef struct SLNode {
    char *key;
    char *value;
    int level;
    struct SLNode *forward[];
} SLNode;

typedef struct {
    SLNode *header;
    int level;
    long long count;
    unsigned long long rng;
} XSkipList;

static SLNode *sl_new_node(int level, const char *key, const char *value) {
    SLNode *n = (SLNode *)calloc(1, sizeof(SLNode) + level * sizeof(SLNode *));
    if (!n) return NULL;
    n->level = level;
    if (key) { n->key = strdup(key); }
    if (value) { n->value = strdup(value); }
    return n;
}

static void sl_free_node(SLNode *n) {
    if (n) { free(n->key); free(n->value); free(n); }
}

static int sl_random_level(XSkipList *sl) {
    int lvl = 1;
    while (lvl < SL_MAX_LEVEL) {
        sl->rng = sl->rng * 6364136223846793005ULL + 1442695040888963407ULL;
        if ((sl->rng >> 33) & 1) break;
        lvl++;
    }
    return lvl;
}

void *xskiplist_create(void) {
    XSkipList *sl = (XSkipList *)calloc(1, sizeof(XSkipList));
    if (!sl) { xrtSetError("skiplist: out of memory", 0); return NULL; }
    sl->header = sl_new_node(SL_MAX_LEVEL, NULL, NULL);
    if (!sl->header) { free(sl); xrtSetError("skiplist: out of memory", 0); return NULL; }
    sl->level = 1;
    sl->count = 0;
    sl->rng = 12345678901234567ULL;
    return sl;
}

void xskiplist_destroy(void *ptr) {
    XSkipList *sl = (XSkipList *)ptr;
    if (!sl) return;
    SLNode *cur = sl->header->forward[0];
    while (cur) { SLNode *next = cur->forward[0]; sl_free_node(cur); cur = next; }
    sl_free_node(sl->header);
    free(sl);
}

void xskiplist_set(void *ptr, const char *key, const char *value) {
    XSkipList *sl = (XSkipList *)ptr;
    SLNode *update[SL_MAX_LEVEL];
    SLNode *cur;
    int i, lvl;

    if (!sl || !key) return;
    cur = sl->header;
    for (i = sl->level - 1; i >= 0; i--) {
        while (cur->forward[i] && strcmp(cur->forward[i]->key, key) < 0)
            cur = cur->forward[i];
        update[i] = cur;
    }
    cur = cur->forward[0];

    if (cur && strcmp(cur->key, key) == 0) {
        free(cur->value);
        cur->value = strdup(value ? value : "");
        return;
    }

    lvl = sl_random_level(sl);
    if (lvl > sl->level) {
        for (i = sl->level; i < lvl; i++) update[i] = sl->header;
        sl->level = lvl;
    }

    {
        SLNode *node = sl_new_node(lvl, key, value ? value : "");
        if (!node) { xrtSetError("skiplist: out of memory", 0); return; }
        for (i = 0; i < lvl; i++) {
            node->forward[i] = update[i]->forward[i];
            update[i]->forward[i] = node;
        }
    }
    sl->count++;
}

/* Returns string from thread-local buffer, or empty string if not found */
const char *xskiplist_get(void *ptr, const char *key) {
    XSkipList *sl = (XSkipList *)ptr;
    SLNode *cur;
    int i;
    if (!sl || !key) return "";
    cur = sl->header;
    for (i = sl->level - 1; i >= 0; i--) {
        while (cur->forward[i] && strcmp(cur->forward[i]->key, key) < 0)
            cur = cur->forward[i];
    }
    cur = cur->forward[0];
    if (cur && strcmp(cur->key, key) == 0) {
        strncpy(sl_buf, cur->value, sizeof(sl_buf) - 1);
        sl_buf[sizeof(sl_buf) - 1] = '\0';
        return sl_buf;
    }
    return "";
}

int xskiplist_contains(void *ptr, const char *key) {
    XSkipList *sl = (XSkipList *)ptr;
    SLNode *cur;
    int i;
    if (!sl || !key) return 0;
    cur = sl->header;
    for (i = sl->level - 1; i >= 0; i--) {
        while (cur->forward[i] && strcmp(cur->forward[i]->key, key) < 0)
            cur = cur->forward[i];
    }
    cur = cur->forward[0];
    return (cur && strcmp(cur->key, key) == 0) ? 1 : 0;
}

int xskiplist_remove(void *ptr, const char *key) {
    XSkipList *sl = (XSkipList *)ptr;
    SLNode *update[SL_MAX_LEVEL];
    SLNode *cur;
    int i;
    if (!sl || !key) return 0;
    cur = sl->header;
    for (i = sl->level - 1; i >= 0; i--) {
        while (cur->forward[i] && strcmp(cur->forward[i]->key, key) < 0)
            cur = cur->forward[i];
        update[i] = cur;
    }
    cur = cur->forward[0];
    if (!cur || strcmp(cur->key, key) != 0) return 0;
    for (i = 0; i < sl->level; i++) {
        if (update[i]->forward[i] != cur) break;
        update[i]->forward[i] = cur->forward[i];
    }
    sl_free_node(cur);
    while (sl->level > 1 && sl->header->forward[sl->level - 1] == NULL)
        sl->level--;
    sl->count--;
    return 1;
}

void xskiplist_clear(void *ptr) {
    XSkipList *sl = (XSkipList *)ptr;
    SLNode *cur, *next;
    int i;
    if (!sl) return;
    cur = sl->header->forward[0];
    while (cur) { next = cur->forward[0]; sl_free_node(cur); cur = next; }
    for (i = 0; i < SL_MAX_LEVEL; i++) sl->header->forward[i] = NULL;
    sl->level = 1;
    sl->count = 0;
}

long long xskiplist_count(void *ptr) {
    XSkipList *sl = (XSkipList *)ptr;
    return sl ? sl->count : 0;
}

/* Collect keys in order. Returns count written. Caller provides array. */
long long xskiplist_keys(void *ptr, char **out, long long max) {
    XSkipList *sl = (XSkipList *)ptr;
    SLNode *cur;
    long long n = 0;
    if (!sl || !out) return 0;
    cur = sl->header->forward[0];
    while (cur && n < max) {
        out[n] = cur->key;
        n++;
        cur = cur->forward[0];
    }
    return n;
}

/* Range query: keys where start <= key <= end */
long long xskiplist_range(void *ptr, const char *start, const char *end, char **out_keys, char **out_vals, long long max) {
    XSkipList *sl = (XSkipList *)ptr;
    SLNode *cur;
    int i;
    long long n = 0;
    if (!sl || !out_keys) return 0;
    cur = sl->header;
    if (start) {
        for (i = sl->level - 1; i >= 0; i--) {
            while (cur->forward[i] && strcmp(cur->forward[i]->key, start) < 0)
                cur = cur->forward[i];
        }
        cur = cur->forward[0];
    } else {
        cur = cur->forward[0];
    }
    while (cur && n < max) {
        if (end && strcmp(cur->key, end) > 0) break;
        out_keys[n] = cur->key;
        if (out_vals) out_vals[n] = cur->value;
        n++;
        cur = cur->forward[0];
    }
    return n;
}
