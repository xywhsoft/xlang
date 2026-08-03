/*
 * trie_native.c - Trie (prefix tree) for string set operations
 * Supports insert, search, prefix search, delete, and prefix counting.
 * Uses compact child arrays (sorted by byte value).
 */
#include <stdlib.h>
#include <string.h>
extern void xrtSetError(const void *msg, int bFree);
typedef struct TrieNode {
    struct TrieNode **children;
    unsigned char *keys;
    int child_count;
    int child_cap;
    int is_end;
} TrieNode;

typedef struct {
    TrieNode *root;
    long long count;
} XTrie;

static TrieNode *trie_new_node(void) {
    TrieNode *n = (TrieNode *)calloc(1, sizeof(TrieNode));
    return n;
}

static void trie_free_node(TrieNode *n) {
    int i;
    if (!n) return;
    for (i = 0; i < n->child_count; i++) {
        trie_free_node(n->children[i]);
    }
    free(n->children);
    free(n->keys);
    free(n);
}

static TrieNode *trie_find_child(TrieNode *n, unsigned char c) {
    int i;
    for (i = 0; i < n->child_count; i++) {
        if (n->keys[i] == c) return n->children[i];
    }
    return NULL;
}

static TrieNode *trie_add_child(TrieNode *n, unsigned char c) {
    TrieNode *child;
    int pos = 0, i;

    /* Find insertion position (sorted) */
    while (pos < n->child_count && n->keys[pos] < c) pos++;
    if (pos < n->child_count && n->keys[pos] == c) return n->children[pos];

    child = trie_new_node();
    if (!child) return NULL;

    if (n->child_count >= n->child_cap) {
        int newcap = n->child_cap == 0 ? 4 : n->child_cap * 2;
        TrieNode **nc = (TrieNode **)realloc(n->children, newcap * sizeof(TrieNode *));
        unsigned char *nk = (unsigned char *)realloc(n->keys, newcap);
        if (!nc || !nk) { free(child); return NULL; }
        n->children = nc;
        n->keys = nk;
        n->child_cap = newcap;
    }

    /* Shift elements */
    for (i = n->child_count; i > pos; i--) {
        n->children[i] = n->children[i-1];
        n->keys[i] = n->keys[i-1];
    }
    n->children[pos] = child;
    n->keys[pos] = c;
    n->child_count++;
    return child;
}

void *xtrie_create(void) {
    XTrie *t = (XTrie *)calloc(1, sizeof(XTrie));
    if (!t) { xrtSetError("trie: out of memory", 0); return NULL; }
    t->root = trie_new_node();
    if (!t->root) { free(t); xrtSetError("trie: out of memory", 0); return NULL; }
    t->count = 0;
    return t;
}

void xtrie_destroy(void *ptr) {
    XTrie *t = (XTrie *)ptr;
    if (!t) return;
    trie_free_node(t->root);
    free(t);
}

void xtrie_insert(void *ptr, const char *word) {
    XTrie *t = (XTrie *)ptr;
    TrieNode *cur;
    const unsigned char *s;
    if (!t || !word) return;
    cur = t->root;
    for (s = (const unsigned char *)word; *s; s++) {
        cur = trie_add_child(cur, *s);
        if (!cur) { xrtSetError("trie: out of memory", 0); return; }
    }
    if (!cur->is_end) {
        cur->is_end = 1;
        t->count++;
    }
}

int xtrie_contains(void *ptr, const char *word) {
    XTrie *t = (XTrie *)ptr;
    TrieNode *cur;
    const unsigned char *s;
    if (!t || !word) return 0;
    cur = t->root;
    for (s = (const unsigned char *)word; *s; s++) {
        cur = trie_find_child(cur, *s);
        if (!cur) return 0;
    }
    return cur->is_end;
}

int xtrie_starts_with(void *ptr, const char *prefix) {
    XTrie *t = (XTrie *)ptr;
    TrieNode *cur;
    const unsigned char *s;
    if (!t || !prefix) return 0;
    cur = t->root;
    for (s = (const unsigned char *)prefix; *s; s++) {
        cur = trie_find_child(cur, *s);
        if (!cur) return 0;
    }
    return 1;
}

static long long trie_count_under(TrieNode *n) {
    long long total = 0;
    int i;
    if (!n) return 0;
    if (n->is_end) total++;
    for (i = 0; i < n->child_count; i++) {
        total += trie_count_under(n->children[i]);
    }
    return total;
}

long long xtrie_count_prefix(void *ptr, const char *prefix) {
    XTrie *t = (XTrie *)ptr;
    TrieNode *cur;
    const unsigned char *s;
    if (!t || !prefix) return 0;
    cur = t->root;
    for (s = (const unsigned char *)prefix; *s; s++) {
        cur = trie_find_child(cur, *s);
        if (!cur) return 0;
    }
    return trie_count_under(cur);
}

static int trie_remove_helper(TrieNode *n, const unsigned char *s) {
    if (*s == '\0') {
        if (!n->is_end) return 0;
        n->is_end = 0;
        return 1;
    }
    {
        TrieNode *child = trie_find_child(n, *s);
        if (!child) return 0;
        if (!trie_remove_helper(child, s + 1)) return 0;
        /* Prune empty leaf */
        if (child->child_count == 0 && !child->is_end) {
            int i, pos = -1;
            for (i = 0; i < n->child_count; i++) {
                if (n->keys[i] == *s) { pos = i; break; }
            }
            if (pos >= 0) {
                trie_free_node(child);
                for (i = pos; i < n->child_count - 1; i++) {
                    n->children[i] = n->children[i+1];
                    n->keys[i] = n->keys[i+1];
                }
                n->child_count--;
            }
        }
        return 1;
    }
}

int xtrie_remove(void *ptr, const char *word) {
    XTrie *t = (XTrie *)ptr;
    if (!t || !word) return 0;
    if (trie_remove_helper(t->root, (const unsigned char *)word)) {
        t->count--;
        return 1;
    }
    return 0;
}

void xtrie_clear(void *ptr) {
    XTrie *t = (XTrie *)ptr;
    if (!t) return;
    trie_free_node(t->root);
    t->root = trie_new_node();
    t->count = 0;
}

long long xtrie_count(void *ptr) {
    XTrie *t = (XTrie *)ptr;
    return t ? t->count : 0;
}
