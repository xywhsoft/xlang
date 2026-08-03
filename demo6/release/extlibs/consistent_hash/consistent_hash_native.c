/*
 * consistent_hash_native.c - Consistent hashing ring with virtual nodes
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
extern void xrtSetError(const void *msg, int bFree);
static char chash_buf[4096];

/* ------------------------------------------------------------------ */
/* FNV-1a hash                                                         */
/* ------------------------------------------------------------------ */
static unsigned long long chash_fnv(const char *s) {
    unsigned long long h = 14695981039346656037ULL;
    while (*s) { h ^= (unsigned char)*s++; h *= 1099511628211ULL; }
    return h;
}

/* ------------------------------------------------------------------ */
/* Ring entry (sorted array of virtual nodes)                          */
/* ------------------------------------------------------------------ */
typedef struct {
    unsigned long long hash;
    char *node;             /* physical node name (shared pointer) */
} VNode;

typedef struct {
    char **nodes;           /* physical node names */
    int node_count;
    int node_cap;
    VNode *ring;            /* sorted virtual node array */
    int ring_count;
    int ring_cap;
    int replicas;           /* virtual nodes per physical node */
} XCHash;

static int vnode_cmp(const void *a, const void *b) {
    const VNode *va = (const VNode *)a;
    const VNode *vb = (const VNode *)b;
    if (va->hash < vb->hash) return -1;
    if (va->hash > vb->hash) return 1;
    return 0;
}

void *xchash_create(int replicas) {
    XCHash *c;
    if (replicas <= 0) replicas = 150;
    c = (XCHash *)calloc(1, sizeof(XCHash));
    if (!c) { xrtSetError("consistent_hash: out of memory", 0); return NULL; }
    c->replicas = replicas;
    c->node_cap = 16;
    c->nodes = (char **)malloc(sizeof(char *) * c->node_cap);
    c->ring_cap = 256;
    c->ring = (VNode *)malloc(sizeof(VNode) * c->ring_cap);
    if (!c->nodes || !c->ring) {
        xrtSetError("consistent_hash: out of memory", 0);
        free(c->nodes);
        free(c->ring);
        free(c);
        return NULL;
    }
    return c;
}

void xchash_destroy(void *ptr) {
    XCHash *c = (XCHash *)ptr;
    int i;
    if (!c) return;
    for (i = 0; i < c->node_count; i++) free(c->nodes[i]);
    free(c->nodes);
    free(c->ring);
    free(c);
}

void xchash_add_node(void *ptr, const char *node) {
    XCHash *c = (XCHash *)ptr;
    int i;
    char vkey[512];
    if (!c || !node || !node[0]) return;

    /* check duplicate */
    for (i = 0; i < c->node_count; i++) {
        if (strcmp(c->nodes[i], node) == 0) return;
    }

    /* grow nodes array */
    if (c->node_count >= c->node_cap) {
        c->node_cap *= 2;
        c->nodes = (char **)realloc(c->nodes, sizeof(char *) * c->node_cap);
    }
    c->nodes[c->node_count] = strdup(node);
    c->node_count++;

    /* ensure ring capacity */
    if (c->ring_count + c->replicas > c->ring_cap) {
        while (c->ring_count + c->replicas > c->ring_cap) c->ring_cap *= 2;
        c->ring = (VNode *)realloc(c->ring, sizeof(VNode) * c->ring_cap);
    }

    /* add virtual nodes */
    for (i = 0; i < c->replicas; i++) {
        snprintf(vkey, sizeof(vkey), "%s#%d", node, i);
        c->ring[c->ring_count].hash = chash_fnv(vkey);
        c->ring[c->ring_count].node = c->nodes[c->node_count - 1];
        c->ring_count++;
    }

    /* re-sort ring */
    qsort(c->ring, c->ring_count, sizeof(VNode), vnode_cmp);
}

void xchash_remove_node(void *ptr, const char *node) {
    XCHash *c = (XCHash *)ptr;
    int i, w;
    char *target = NULL;
    if (!c || !node) return;

    for (i = 0; i < c->node_count; i++) {
        if (strcmp(c->nodes[i], node) == 0) { target = c->nodes[i]; break; }
    }
    if (!target) return;

    /* remove from nodes array */
    for (i = 0; i < c->node_count; i++) {
        if (c->nodes[i] == target) {
            /* shift */
            for (w = i; w < c->node_count - 1; w++) c->nodes[w] = c->nodes[w + 1];
            c->node_count--;
            break;
        }
    }
    free(target);

    /* remove virtual nodes from ring */
    w = 0;
    for (i = 0; i < c->ring_count; i++) {
        if (c->ring[i].node != target) {
            c->ring[w++] = c->ring[i];
        }
    }
    c->ring_count = w;
}

const char *xchash_get_node(void *ptr, const char *key) {
    XCHash *c = (XCHash *)ptr;
    unsigned long long h;
    int lo, hi, mid;
    if (!c || !key || c->ring_count == 0) return "";

    h = chash_fnv(key);

    /* binary search for first vnode with hash >= h */
    lo = 0; hi = c->ring_count - 1;
    if (h > c->ring[hi].hash) {
        /* wrap around */
        strncpy(chash_buf, c->ring[0].node, sizeof(chash_buf) - 1);
        chash_buf[sizeof(chash_buf) - 1] = '\0';
        return chash_buf;
    }
    while (lo < hi) {
        mid = (lo + hi) / 2;
        if (c->ring[mid].hash < h) lo = mid + 1;
        else hi = mid;
    }
    strncpy(chash_buf, c->ring[lo].node, sizeof(chash_buf) - 1);
    chash_buf[sizeof(chash_buf) - 1] = '\0';
    return chash_buf;
}

int xchash_node_count(void *ptr) {
    XCHash *c = (XCHash *)ptr;
    return c ? c->node_count : 0;
}

int xchash_vnode_count(void *ptr) {
    XCHash *c = (XCHash *)ptr;
    return c ? c->ring_count : 0;
}
