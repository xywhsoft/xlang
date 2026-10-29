/* 固定容量的 Cuckoo Filter：16 位指纹，每桶 4 项，失败插入可回滚。 */
#include "include/cuckoo_native.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#ifndef XRT_MODULE_HASH64
#define XRT_MODULE_HASH64
#endif
#include "xrt.h"

#define XCUCKOO_SLOTS 4
#define XCUCKOO_KICKS 500
#define XCUCKOO_DOMAIN "xlang.extlib.cuckoo"

typedef struct {
    uint16_t *data;
    size_t buckets;
    size_t mask;
    int64_t count;
    int64_t capacity;
} XCuckoo;

typedef struct {
    size_t slot;
    uint16_t previous;
} XCuckooUndo;

static int xcuckoo_fail(int code, const char *message) {
    xrtSetErrorInfo(code, XCUCKOO_DOMAIN, 0, message);
    return 0;
}

static uint16_t xcuckoo_fingerprint(uint64_t hash) {
    uint16_t value = (uint16_t)(hash >> 32);
    return value != 0 ? value : 1;
}

static size_t xcuckoo_alternate(const XCuckoo *filter, size_t index, uint16_t fingerprint) {
    size_t offset = (size_t)((uint32_t)fingerprint * UINT32_C(2654435761)) & filter->mask;
    /* 零偏移会使两个候选桶重合；只在此情况下改用相邻桶。 */
    return index ^ (offset != 0u ? offset : 1u);
}

static uint16_t *xcuckoo_bucket(const XCuckoo *filter, size_t index) {
    return filter->data + index * XCUCKOO_SLOTS;
}

static int xcuckoo_insert_empty(const XCuckoo *filter, size_t index, uint16_t fingerprint) {
    uint16_t *bucket = xcuckoo_bucket(filter, index);
    int i;
    for (i = 0; i < XCUCKOO_SLOTS; ++i) {
        if (bucket[i] == 0) {
            bucket[i] = fingerprint;
            return 1;
        }
    }
    return 0;
}

static int xcuckoo_find(const XCuckoo *filter, size_t index, uint16_t fingerprint) {
    uint16_t *bucket = xcuckoo_bucket(filter, index);
    int i;
    for (i = 0; i < XCUCKOO_SLOTS; ++i) {
        if (bucket[i] == fingerprint) return i;
    }
    return -1;
}

static int xcuckoo_key(const XCuckoo *filter, const char *data, size_t size) {
    xrtClearError();
    if (filter == NULL || data == NULL) return xcuckoo_fail(XERR_ARGUMENT, "filter or key pointer is null");
    (void)size;
    return 1;
}

/* ------------ 生命周期 ------------ */

void *xcuckoo_create(int64_t capacity) {
    XCuckoo *filter;
    size_t buckets = 4, wanted;
    xrtClearError();
    if (capacity <= 0) {
        xcuckoo_fail(XERR_VALUE, "capacity must be positive");
        return NULL;
    }
    wanted = (size_t)((uint64_t)capacity / 3 + ((uint64_t)capacity % 3 != 0));
    if ((uint64_t)capacity > (uint64_t)SIZE_MAX ||
        wanted > SIZE_MAX / (XCUCKOO_SLOTS * sizeof(uint16_t))) {
        xcuckoo_fail(XERR_RANGE, "capacity is too large");
        return NULL;
    }
    while (buckets < wanted) {
        if (buckets > SIZE_MAX / (2 * XCUCKOO_SLOTS * sizeof(uint16_t))) {
            xcuckoo_fail(XERR_RANGE, "capacity is too large");
            return NULL;
        }
        buckets *= 2;
    }
    filter = xrtCalloc(1, sizeof(*filter));
    if (filter == NULL) return NULL;
    filter->data = xrtCalloc(buckets * XCUCKOO_SLOTS, sizeof(uint16_t));
    if (filter->data == NULL) {
        xrtFree(filter);
        return NULL;
    }
    filter->buckets = buckets;
    filter->mask = buckets - 1;
    filter->capacity = capacity;
    return filter;
}

void xcuckoo_destroy(void *handle) {
    XCuckoo *filter = handle;
    if (filter == NULL) return;
    xrtFree(filter->data);
    xrtFree(filter);
}

/* ------------ 增删查及统计 ------------ */

bool xcuckoo_add(void *handle, const char *data, size_t size) {
    XCuckoo *filter = handle;
    XCuckooUndo undo[XCUCKOO_KICKS];
    uint64_t hash, random;
    uint16_t fingerprint, displaced;
    size_t first, alternate, index;
    int kick;
    if (!xcuckoo_key(filter, data, size)) return false;
    if (filter->count >= filter->capacity) {
        xcuckoo_fail(XERR_RANGE, "filter is full");
        return false;
    }
    hash = xrtHash64(data, size);
    fingerprint = xcuckoo_fingerprint(hash);
    first = (size_t)hash & filter->mask;
    alternate = xcuckoo_alternate(filter, first, fingerprint);
    if (xcuckoo_insert_empty(filter, first, fingerprint) ||
        xcuckoo_insert_empty(filter, alternate, fingerprint)) {
        ++filter->count;
        return true;
    }

    /* 每次改写先记旧槽；500 次未找到空位时逆序还原，不能丢掉旧指纹。 */
    index = first;
    displaced = fingerprint;
    random = hash ^ UINT64_C(0x123456789abcdef);
    for (kick = 0; kick < XCUCKOO_KICKS; ++kick) {
        size_t slot;
        random = random * UINT64_C(6364136223846793005) + UINT64_C(1442695040888963407);
        slot = index * XCUCKOO_SLOTS + (size_t)((random >> 33) % XCUCKOO_SLOTS);
        undo[kick].slot = slot;
        undo[kick].previous = filter->data[slot];
        filter->data[slot] = displaced;
        displaced = undo[kick].previous;
        index = xcuckoo_alternate(filter, index, displaced);
        if (xcuckoo_insert_empty(filter, index, displaced)) {
            ++filter->count;
            return true;
        }
    }
    while (kick != 0) {
        --kick;
        filter->data[undo[kick].slot] = undo[kick].previous;
    }
    xcuckoo_fail(XERR_RANGE, "filter cannot place another fingerprint");
    return false;
}

bool xcuckoo_contains(void *handle, const char *data, size_t size) {
    XCuckoo *filter = handle;
    uint64_t hash;
    uint16_t fingerprint;
    size_t first;
    if (!xcuckoo_key(filter, data, size)) return false;
    hash = xrtHash64(data, size);
    fingerprint = xcuckoo_fingerprint(hash);
    first = (size_t)hash & filter->mask;
    return xcuckoo_find(filter, first, fingerprint) >= 0 ||
        xcuckoo_find(filter, xcuckoo_alternate(filter, first, fingerprint), fingerprint) >= 0;
}

bool xcuckoo_remove(void *handle, const char *data, size_t size) {
    XCuckoo *filter = handle;
    uint64_t hash;
    uint16_t fingerprint;
    size_t first, alternate;
    int slot;
    if (!xcuckoo_key(filter, data, size)) return false;
    hash = xrtHash64(data, size);
    fingerprint = xcuckoo_fingerprint(hash);
    first = (size_t)hash & filter->mask;
    alternate = xcuckoo_alternate(filter, first, fingerprint);
    slot = xcuckoo_find(filter, first, fingerprint);
    if (slot >= 0) xcuckoo_bucket(filter, first)[slot] = 0;
    else {
        slot = xcuckoo_find(filter, alternate, fingerprint);
        if (slot < 0) return false;
        xcuckoo_bucket(filter, alternate)[slot] = 0;
    }
    --filter->count;
    return true;
}

void xcuckoo_clear(void *handle) {
    XCuckoo *filter = handle;
    xrtClearError();
    if (filter == NULL) {
        xcuckoo_fail(XERR_ARGUMENT, "filter is null");
        return;
    }
    memset(filter->data, 0, filter->buckets * XCUCKOO_SLOTS * sizeof(uint16_t));
    filter->count = 0;
}

int64_t xcuckoo_count(void *handle) {
    XCuckoo *filter = handle;
    xrtClearError();
    if (filter == NULL) {
        xcuckoo_fail(XERR_ARGUMENT, "filter is null");
        return -1;
    }
    return filter->count;
}

int64_t xcuckoo_buckets(void *handle) {
    XCuckoo *filter = handle;
    xrtClearError();
    if (filter == NULL) {
        xcuckoo_fail(XERR_ARGUMENT, "filter is null");
        return -1;
    }
    return (int64_t)filter->buckets;
}
