/*
 * lru_native.c - LRU and LFU cache implementations
 * Hash map + doubly linked list for O(1) get/put/evict.
 */
#include <stdlib.h>
#include <string.h>
extern void xrtSetError(const void *msg, int bFree);
static char lru_buf[4096];

/* ------------------------------------------------------------------ */
/* Simple hash map (open addressing) for key -> node lookup            */
/* ------------------------------------------------------------------ */

#define LRU_HASH_SIZE 4096

typedef struct LruEntry {
    char *key;
    char *value;
    long long freq;             /* for LFU */
    struct LruEntry *prev;      /* LRU list */
    struct LruEntry *next;
    struct LruEntry *hash_next; /* hash chain */
} LruEntry;

typedef struct {
    LruEntry *buckets[LRU_HASH_SIZE];
    LruEntry *head;             /* most recently used */
    LruEntry *tail;             /* least recently used */
    long long capacity;
    long long count;
    long long hits;
    long long misses;
    int mode;                   /* 0=LRU, 1=LFU */
} XLru;

static unsigned long long lru_hash(const char *s) {
    unsigned long long h = 14695981039346656037ULL;
    while (*s) { h ^= (unsigned char)*s++; h *= 1099511628211ULL; }
    return h;
}

static int lru_bucket(unsigned long long h) {
    return (int)(h % LRU_HASH_SIZE);
}

static LruEntry *lru_find(XLru *c, const char *key) {
    int b = lru_bucket(lru_hash(key));
    LruEntry *e = c->buckets[b];
    while (e) {
        if (strcmp(e->key, key) == 0) return e;
        e = e->hash_next;
    }
    return NULL;
}

static void lru_unlink(XLru *c, LruEntry *e) {
    if (e->prev) e->prev->next = e->next;
    else c->head = e->next;
    if (e->next) e->next->prev = e->prev;
    else c->tail = e->prev;
    e->prev = e->next = NULL;
}

static void lru_push_front(XLru *c, LruEntry *e) {
    e->prev = NULL;
    e->next = c->head;
    if (c->head) c->head->prev = e;
    c->head = e;
    if (!c->tail) c->tail = e;
}

static void lru_hash_insert(XLru *c, LruEntry *e) {
    int b = lru_bucket(lru_hash(e->key));
    e->hash_next = c->buckets[b];
    c->buckets[b] = e;
}

static void lru_hash_remove(XLru *c, LruEntry *e) {
    int b = lru_bucket(lru_hash(e->key));
    LruEntry **pp = &c->buckets[b];
    while (*pp) {
        if (*pp == e) { *pp = e->hash_next; return; }
        pp = &(*pp)->hash_next;
    }
}

static void lru_evict(XLru *c) {
    LruEntry *victim = c->tail;
    if (!victim) return;

    if (c->mode == 1) {
        /* LFU: find min frequency entry (scan from tail) */
        LruEntry *cur = c->tail;
        victim = cur;
        while (cur) {
            if (cur->freq < victim->freq) victim = cur;
            cur = cur->prev;
        }
    }

    lru_unlink(c, victim);
    lru_hash_remove(c, victim);
    free(victim->key);
    free(victim->value);
    free(victim);
    c->count--;
}

void *xlru_create(long long capacity, int mode) {
    XLru *c;
    if (capacity <= 0) { xrtSetError("lru: capacity must be positive", 0); return NULL; }
    c = (XLru *)calloc(1, sizeof(XLru));
    if (!c) { xrtSetError("lru: out of memory", 0); return NULL; }
    c->capacity = capacity;
    c->mode = mode;
    return c;
}

void xlru_destroy(void *ptr) {
    XLru *c = (XLru *)ptr;
    LruEntry *e, *next;
    if (!c) return;
    e = c->head;
    while (e) { next = e->next; free(e->key); free(e->value); free(e); e = next; }
    free(c);
}

void xlru_put(void *ptr, const char *key, const char *value) {
    XLru *c = (XLru *)ptr;
    LruEntry *e;
    if (!c || !key) return;

    e = lru_find(c, key);
    if (e) {
        free(e->value);
        e->value = strdup(value ? value : "");
        e->freq++;
        lru_unlink(c, e);
        lru_push_front(c, e);
        return;
    }

    if (c->count >= c->capacity) lru_evict(c);

    e = (LruEntry *)calloc(1, sizeof(LruEntry));
    if (!e) { xrtSetError("lru: out of memory", 0); return; }
    e->key = strdup(key);
    e->value = strdup(value ? value : "");
    e->freq = 1;
    lru_hash_insert(c, e);
    lru_push_front(c, e);
    c->count++;
}

const char *xlru_get(void *ptr, const char *key) {
    XLru *c = (XLru *)ptr;
    LruEntry *e;
    if (!c || !key) return "";
    e = lru_find(c, key);
    if (!e) { c->misses++; return ""; }
    c->hits++;
    e->freq++;
    lru_unlink(c, e);
    lru_push_front(c, e);
    strncpy(lru_buf, e->value, sizeof(lru_buf) - 1);
    lru_buf[sizeof(lru_buf) - 1] = '\0';
    return lru_buf;
}

int xlru_contains(void *ptr, const char *key) {
    XLru *c = (XLru *)ptr;
    if (!c || !key) return 0;
    return lru_find(c, key) != NULL;
}

int xlru_remove(void *ptr, const char *key) {
    XLru *c = (XLru *)ptr;
    LruEntry *e;
    if (!c || !key) return 0;
    e = lru_find(c, key);
    if (!e) return 0;
    lru_unlink(c, e);
    lru_hash_remove(c, e);
    free(e->key);
    free(e->value);
    free(e);
    c->count--;
    return 1;
}

void xlru_clear(void *ptr) {
    XLru *c = (XLru *)ptr;
    LruEntry *e, *next;
    int i;
    if (!c) return;
    e = c->head;
    while (e) { next = e->next; free(e->key); free(e->value); free(e); e = next; }
    for (i = 0; i < LRU_HASH_SIZE; i++) c->buckets[i] = NULL;
    c->head = c->tail = NULL;
    c->count = 0;
    c->hits = 0;
    c->misses = 0;
}

long long xlru_count(void *ptr) {
    XLru *c = (XLru *)ptr;
    return c ? c->count : 0;
}

long long xlru_capacity(void *ptr) {
    XLru *c = (XLru *)ptr;
    return c ? c->capacity : 0;
}

long long xlru_hits(void *ptr) {
    XLru *c = (XLru *)ptr;
    return c ? c->hits : 0;
}

long long xlru_misses(void *ptr) {
    XLru *c = (XLru *)ptr;
    return c ? c->misses : 0;
}
