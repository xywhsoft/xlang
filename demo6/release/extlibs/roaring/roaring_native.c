/*
 * roaring_native.c - Simplified Roaring Bitmap (array + bitmap containers)
 */
#include <stdio.h>
#include "../xlang_extlib_error.h"
static char rb_buf[8192];

#define RB_CONTAINER_SIZE 65536
#define RB_ARRAY_THRESHOLD 4096

typedef struct {
    unsigned short *array;    /* sorted array of values */
    int array_count;
    int array_cap;
    unsigned long long *bitmap;  /* 65536 bits = 1024 uint64 */
    int is_bitmap;
    int cardinality;
} Container;

typedef struct {
    unsigned short *keys;     /* high 16 bits */
    Container *containers;
    int count;
    int cap;
} XRoaring;

static Container *container_new(void) {
    Container *c = (Container *)calloc(1, sizeof(Container));
    c->array_cap = 16;
    c->array = (unsigned short *)malloc(sizeof(unsigned short) * c->array_cap);
    return c;
}

static void container_free(Container *c) {
    free(c->array);
    free(c->bitmap);
}

static int container_contains(Container *c, unsigned short val) {
    if (c->is_bitmap) {
        return (c->bitmap[val / 64] >> (val % 64)) & 1;
    }
    /* binary search in array */
    int lo = 0, hi = c->array_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (c->array[mid] == val) return 1;
        if (c->array[mid] < val) lo = mid + 1;
        else hi = mid - 1;
    }
    return 0;
}

static void container_add(Container *c, unsigned short val) {
    if (c->is_bitmap) {
        if (!((c->bitmap[val/64] >> (val%64)) & 1)) {
            c->bitmap[val/64] |= (1ULL << (val%64));
            c->cardinality++;
        }
        return;
    }
    /* check if already present */
    int lo = 0, hi = c->array_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (c->array[mid] == val) return;
        if (c->array[mid] < val) lo = mid + 1;
        else hi = mid - 1;
    }
    /* insert at position lo */
    if (c->array_count >= c->array_cap) {
        c->array_cap *= 2;
        c->array = (unsigned short *)realloc(c->array, sizeof(unsigned short) * c->array_cap);
    }
    memmove(&c->array[lo+1], &c->array[lo], sizeof(unsigned short) * (c->array_count - lo));
    c->array[lo] = val;
    c->array_count++;
    c->cardinality++;
    /* convert to bitmap if too large */
    if (c->array_count > RB_ARRAY_THRESHOLD) {
        int i;
        c->bitmap = (unsigned long long *)calloc(1024, sizeof(unsigned long long));
        for (i = 0; i < c->array_count; i++) c->bitmap[c->array[i]/64] |= (1ULL << (c->array[i]%64));
        free(c->array);
        c->array = NULL;
        c->is_bitmap = 1;
    }
}

static void container_remove(Container *c, unsigned short val) {
    if (c->is_bitmap) {
        if ((c->bitmap[val/64] >> (val%64)) & 1) {
            c->bitmap[val/64] &= ~(1ULL << (val%64));
            c->cardinality--;
        }
        return;
    }
    int lo = 0, hi = c->array_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (c->array[mid] == val) {
            memmove(&c->array[mid], &c->array[mid+1], sizeof(unsigned short) * (c->array_count - mid - 1));
            c->array_count--;
            c->cardinality--;
            return;
        }
        if (c->array[mid] < val) lo = mid + 1;
        else hi = mid - 1;
    }
}

void *xroaring_create(void) {
    XRoaring *rb = (XRoaring *)calloc(1, sizeof(XRoaring));
    if (!rb) { xl6ExtlibError(XL6_EXTERR_MEMORY, "xlang.extlib.roaring", "cannot allocate roaring bitmap"); return NULL; }
    rb->cap = 16;
    rb->keys = (unsigned short *)malloc(sizeof(unsigned short) * rb->cap);
    rb->containers = (Container *)calloc(rb->cap, sizeof(Container));
    return rb;
}

void xroaring_destroy(void *ptr) {
    XRoaring *rb = (XRoaring *)ptr;
    int i;
    if (!rb) return;
    for (i = 0; i < rb->count; i++) container_free(&rb->containers[i]);
    free(rb->keys);
    free(rb->containers);
    free(rb);
}

static Container *rb_get_container(XRoaring *rb, unsigned short key, int create) {
    int lo = 0, hi = rb->count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (rb->keys[mid] == key) return &rb->containers[mid];
        if (rb->keys[mid] < key) lo = mid + 1;
        else hi = mid - 1;
    }
    if (!create) return NULL;
    /* insert at lo */
    if (rb->count >= rb->cap) {
        rb->cap *= 2;
        rb->keys = (unsigned short *)realloc(rb->keys, sizeof(unsigned short) * rb->cap);
        rb->containers = (Container *)realloc(rb->containers, sizeof(Container) * rb->cap);
    }
    memmove(&rb->keys[lo+1], &rb->keys[lo], sizeof(unsigned short) * (rb->count - lo));
    memmove(&rb->containers[lo+1], &rb->containers[lo], sizeof(Container) * (rb->count - lo));
    rb->keys[lo] = key;
    memset(&rb->containers[lo], 0, sizeof(Container));
    rb->containers[lo].array_cap = 16;
    rb->containers[lo].array = (unsigned short *)malloc(sizeof(unsigned short) * 16);
    rb->count++;
    return &rb->containers[lo];
}

void xroaring_add(void *ptr, int value) {
    XRoaring *rb = (XRoaring *)ptr;
    unsigned short hi = (unsigned short)((unsigned int)value >> 16);
    unsigned short lo = (unsigned short)(value & 0xFFFF);
    Container *c;
    if (!rb) return;
    c = rb_get_container(rb, hi, 1);
    container_add(c, lo);
}

void xroaring_remove(void *ptr, int value) {
    XRoaring *rb = (XRoaring *)ptr;
    unsigned short hi = (unsigned short)((unsigned int)value >> 16);
    unsigned short lo = (unsigned short)(value & 0xFFFF);
    Container *c;
    if (!rb) return;
    c = rb_get_container(rb, hi, 0);
    if (c) container_remove(c, lo);
}

int xroaring_contains(void *ptr, int value) {
    XRoaring *rb = (XRoaring *)ptr;
    unsigned short hi = (unsigned short)((unsigned int)value >> 16);
    unsigned short lo = (unsigned short)(value & 0xFFFF);
    Container *c;
    if (!rb) return 0;
    c = rb_get_container(rb, hi, 0);
    if (!c) return 0;
    return container_contains(c, lo);
}

long long xroaring_count(void *ptr) {
    XRoaring *rb = (XRoaring *)ptr;
    long long total = 0;
    int i;
    if (!rb) return 0;
    for (i = 0; i < rb->count; i++) total += rb->containers[i].cardinality;
    return total;
}

void xroaring_clear(void *ptr) {
    XRoaring *rb = (XRoaring *)ptr;
    int i;
    if (!rb) return;
    for (i = 0; i < rb->count; i++) container_free(&rb->containers[i]);
    rb->count = 0;
}

/* Return min value or -1 if empty */
long long xroaring_min(void *ptr) {
    XRoaring *rb = (XRoaring *)ptr;
    int i;
    if (!rb || rb->count == 0) return -1;
    for (i = 0; i < rb->count; i++) {
        Container *c = &rb->containers[i];
        if (c->cardinality == 0) continue;
        if (c->is_bitmap) {
            int j;
            for (j = 0; j < 1024; j++) {
                if (c->bitmap[j]) {
                    int bit = 0;
                    while (!((c->bitmap[j] >> bit) & 1)) bit++;
                    return ((long long)rb->keys[i] << 16) | (j * 64 + bit);
                }
            }
        } else if (c->array_count > 0) {
            return ((long long)rb->keys[i] << 16) | c->array[0];
        }
    }
    return -1;
}

/* Return max value or -1 if empty */
long long xroaring_max(void *ptr) {
    XRoaring *rb = (XRoaring *)ptr;
    int i;
    if (!rb || rb->count == 0) return -1;
    for (i = rb->count - 1; i >= 0; i--) {
        Container *c = &rb->containers[i];
        if (c->cardinality == 0) continue;
        if (c->is_bitmap) {
            int j;
            for (j = 1023; j >= 0; j--) {
                if (c->bitmap[j]) {
                    int bit = 63;
                    while (!((c->bitmap[j] >> bit) & 1)) bit--;
                    return ((long long)rb->keys[i] << 16) | (j * 64 + bit);
                }
            }
        } else if (c->array_count > 0) {
            return ((long long)rb->keys[i] << 16) | c->array[c->array_count - 1];
        }
    }
    return -1;
}
