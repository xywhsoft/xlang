/* rtree_native.c - Simplified R-tree spatial index */
#include <stdlib.h>
#include <string.h>

#define MAX_ENTRIES 8
#define MIN_ENTRIES 3
#define MAX_DEPTH 16

typedef struct {
    double minx, miny, maxx, maxy;
} Rect;

typedef struct Entry {
    Rect rect;
    long long id;       /* for leaf entries */
    struct Node *child; /* for internal entries */
} Entry;

typedef struct Node {
    int is_leaf;
    int count;
    Entry entries[MAX_ENTRIES + 1]; /* +1 for overflow during split */
} Node;

typedef struct {
    Node *root;
    long long size;
} RTree;

static Rect combine_rect(Rect a, Rect b) {
    Rect r;
    r.minx = a.minx < b.minx ? a.minx : b.minx;
    r.miny = a.miny < b.miny ? a.miny : b.miny;
    r.maxx = a.maxx > b.maxx ? a.maxx : b.maxx;
    r.maxy = a.maxy > b.maxy ? a.maxy : b.maxy;
    return r;
}

static double rect_area(Rect r) {
    return (r.maxx - r.minx) * (r.maxy - r.miny);
}

static int rect_intersects(Rect a, Rect b) {
    return a.minx <= b.maxx && a.maxx >= b.minx &&
           a.miny <= b.maxy && a.maxy >= b.miny;
}

static int rect_contains(Rect outer, Rect inner) {
    return outer.minx <= inner.minx && outer.maxx >= inner.maxx &&
           outer.miny <= inner.miny && outer.maxy >= inner.maxy;
}

static Node *node_create(int is_leaf) {
    Node *n = (Node *)calloc(1, sizeof(Node));
    n->is_leaf = is_leaf;
    n->count = 0;
    return n;
}

static void node_free(Node *n) {
    int i;
    if (!n) return;
    if (!n->is_leaf) {
        for (i = 0; i < n->count; i++)
            node_free(n->entries[i].child);
    }
    free(n);
}

static Rect node_bounds(Node *n) {
    Rect r = n->entries[0].rect;
    int i;
    for (i = 1; i < n->count; i++)
        r = combine_rect(r, n->entries[i].rect);
    return r;
}

/* Choose subtree: pick child needing least enlargement */
static int choose_subtree(Node *n, Rect r) {
    int best = 0, i;
    double best_enlarge = 1e300;
    for (i = 0; i < n->count; i++) {
        Rect combined = combine_rect(n->entries[i].rect, r);
        double enlarge = rect_area(combined) - rect_area(n->entries[i].rect);
        if (enlarge < best_enlarge) {
            best_enlarge = enlarge;
            best = i;
        }
    }
    return best;
}

/* Split node using linear split */
static Node *node_split(Node *n) {
    Node *newn = node_create(n->is_leaf);
    int total = n->count;

    /* Simple split: first half stays, second half moves */
    int mid = total / 2;
    newn->count = total - mid;
    int i;
    for (i = 0; i < newn->count; i++)
        newn->entries[i] = n->entries[mid + i];
    n->count = mid;
    return newn;
}

/* Insert into subtree, returns new sibling if split occurred */
static Node *insert_internal(Node *n, Rect r, long long id, int depth) {
    if (n->is_leaf) {
        n->entries[n->count].rect = r;
        n->entries[n->count].id = id;
        n->entries[n->count].child = NULL;
        n->count++;
        if (n->count > MAX_ENTRIES)
            return node_split(n);
        return NULL;
    }

    int idx = choose_subtree(n, r);
    Node *sibling = insert_internal(n->entries[idx].child, r, id, depth + 1);
    n->entries[idx].rect = node_bounds(n->entries[idx].child);

    if (sibling) {
        n->entries[n->count].rect = node_bounds(sibling);
        n->entries[n->count].child = sibling;
        n->entries[n->count].id = 0;
        n->count++;
        if (n->count > MAX_ENTRIES)
            return node_split(n);
    }
    return NULL;
}

/* Search: collect IDs of entries intersecting query rect */
static int search_internal(Node *n, Rect query, long long *results, int max_results, int count) {
    int i;
    if (n->is_leaf) {
        for (i = 0; i < n->count && count < max_results; i++) {
            if (rect_intersects(n->entries[i].rect, query))
                results[count++] = n->entries[i].id;
        }
    } else {
        for (i = 0; i < n->count && count < max_results; i++) {
            if (rect_intersects(n->entries[i].rect, query))
                count = search_internal(n->entries[i].child, query, results, max_results, count);
        }
    }
    return count;
}

/* Delete entry by id from leaf */
static int delete_internal(Node *n, long long id) {
    int i;
    if (n->is_leaf) {
        for (i = 0; i < n->count; i++) {
            if (n->entries[i].id == id) {
                memmove(&n->entries[i], &n->entries[i+1],
                        (n->count - i - 1) * sizeof(Entry));
                n->count--;
                return 1;
            }
        }
        return 0;
    }
    for (i = 0; i < n->count; i++) {
        if (delete_internal(n->entries[i].child, id)) {
            if (n->entries[i].child->count == 0) {
                node_free(n->entries[i].child);
                memmove(&n->entries[i], &n->entries[i+1],
                        (n->count - i - 1) * sizeof(Entry));
                n->count--;
            } else {
                n->entries[i].rect = node_bounds(n->entries[i].child);
            }
            return 1;
        }
    }
    return 0;
}

static long long count_internal(Node *n) {
    long long c = 0;
    int i;
    if (n->is_leaf) return n->count;
    for (i = 0; i < n->count; i++)
        c += count_internal(n->entries[i].child);
    return c;
}

/* === Exported API === */

void *xrtree_create(void) {
    RTree *t = (RTree *)calloc(1, sizeof(RTree));
    t->root = node_create(1);
    t->size = 0;
    return t;
}

void xrtree_destroy(void *handle) {
    RTree *t = (RTree *)handle;
    if (!t) return;
    node_free(t->root);
    free(t);
}

void xrtree_insert(void *handle, long long id, double minx, double miny, double maxx, double maxy) {
    RTree *t = (RTree *)handle;
    Rect r = { minx, miny, maxx, maxy };
    Node *sibling = insert_internal(t->root, r, id, 0);
    if (sibling) {
        Node *newroot = node_create(0);
        newroot->entries[0].rect = node_bounds(t->root);
        newroot->entries[0].child = t->root;
        newroot->entries[1].rect = node_bounds(sibling);
        newroot->entries[1].child = sibling;
        newroot->count = 2;
        t->root = newroot;
    }
    t->size++;
}

/* Search returns count, fills results buffer (comma-separated IDs) */
static char search_buf[4096];
const char *xrtree_search(void *handle, double minx, double miny, double maxx, double maxy) {
    RTree *t = (RTree *)handle;
    Rect query = { minx, miny, maxx, maxy };
    long long results[256];
    int n = search_internal(t->root, query, results, 256, 0);
    int i, pos = 0;
    search_buf[0] = '\0';
    for (i = 0; i < n; i++) {
        if (i > 0) search_buf[pos++] = ',';
        pos += sprintf(search_buf + pos, "%lld", results[i]);
    }
    search_buf[pos] = '\0';
    return search_buf;
}

long long xrtree_search_count(void *handle, double minx, double miny, double maxx, double maxy) {
    RTree *t = (RTree *)handle;
    Rect query = { minx, miny, maxx, maxy };
    long long results[256];
    return search_internal(t->root, query, results, 256, 0);
}

long long xrtree_remove(void *handle, long long id) {
    RTree *t = (RTree *)handle;
    if (delete_internal(t->root, id)) {
        t->size--;
        /* Shrink root if needed */
        while (!t->root->is_leaf && t->root->count == 1) {
            Node *old = t->root;
            t->root = old->entries[0].child;
            free(old);
        }
        return 1;
    }
    return 0;
}

long long xrtree_count(void *handle) {
    RTree *t = (RTree *)handle;
    return t->size;
}

void xrtree_clear(void *handle) {
    RTree *t = (RTree *)handle;
    node_free(t->root);
    t->root = node_create(1);
    t->size = 0;
}
