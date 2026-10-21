/* 32 位整数集合：按高 16 位分桶，低 16 位使用有序数组或位图。 */
#include "include/roaring_native.h"

#include <limits.h>
#include <string.h>

#include "xrt.h"

#define XR_ARRAY_LIMIT 4096u
#define XR_BITMAP_WORDS 1024u
#define XR_DOMAIN "xlang.extlib.roaring"

typedef struct {
    uint16_t key;
    uint16_t *array;
    uint64_t *bitmap;
    uint32_t count;
    uint32_t capacity;
} XRContainer;

typedef struct {
    XRContainer *items;
    size_t count;
    size_t capacity;
    uint64_t total;
} XRBitmap;

static int xr_fail(int code, const char *message) {
    xrtSetErrorInfo(code, XR_DOMAIN, 0, message);
    return 0;
}

static int xr_value(int64_t value, uint32_t *out) {
    if (value < 0 || value > UINT32_MAX) return xr_fail(XERR_RANGE, "value must be uint32");
    *out = (uint32_t)value;
    return 1;
}

static size_t xr_bucket_index(const XRBitmap *set, uint16_t key, int *found) {
    size_t lo = 0, hi = set->count;
    while (lo < hi) {
        size_t middle = lo + (hi - lo) / 2;
        if (set->items[middle].key < key) lo = middle + 1;
        else hi = middle;
    }
    *found = lo < set->count && set->items[lo].key == key;
    return lo;
}

static size_t xr_array_index(const XRContainer *bucket, uint16_t value, int *found) {
    size_t lo = 0, hi = bucket->count;
    while (lo < hi) {
        size_t middle = lo + (hi - lo) / 2;
        if (bucket->array[middle] < value) lo = middle + 1;
        else hi = middle;
    }
    *found = lo < bucket->count && bucket->array[lo] == value;
    return lo;
}

static int xr_create_bucket(XRBitmap *set, size_t position, uint16_t key) {
    uint16_t *values = xrtMalloc(16u * sizeof(*values));
    if (values == NULL) return 0;
    if (set->count == set->capacity) {
        size_t capacity = set->capacity != 0 ? set->capacity * 2 : 8;
        XRContainer *grown;
        if (capacity < set->capacity || capacity > SIZE_MAX / sizeof(*grown)) {
            xrtFree(values);
            return xr_fail(XERR_RANGE, "too many containers");
        }
        grown = xrtMalloc(capacity * sizeof(*grown));
        if (grown == NULL) {
            xrtFree(values);
            return 0;
        }
        if (set->count != 0) memcpy(grown, set->items, set->count * sizeof(*grown));
        xrtFree(set->items);
        set->items = grown;
        set->capacity = capacity;
    }
    memmove(set->items + position + 1, set->items + position,
        (set->count - position) * sizeof(*set->items));
    memset(set->items + position, 0, sizeof(*set->items));
    set->items[position].key = key;
    set->items[position].array = values;
    set->items[position].capacity = 16;
    ++set->count;
    return 1;
}

static int xr_insert(XRContainer *bucket, uint16_t value, int *added) {
    int found;
    size_t position;
    *added = 0;
    if (bucket->bitmap != NULL) {
        uint64_t mask = UINT64_C(1) << (value & 63);
        uint64_t *word = bucket->bitmap + value / 64;
        if ((*word & mask) == 0) {
            *word |= mask;
            ++bucket->count;
            *added = 1;
        }
        return 1;
    }
    position = xr_array_index(bucket, value, &found);
    if (found) return 1;
    if (bucket->count == XR_ARRAY_LIMIT) {
        uint64_t *bitmap = xrtCalloc(XR_BITMAP_WORDS, sizeof(*bitmap));
        uint32_t i;
        if (bitmap == NULL) return 0;
        for (i = 0; i < bucket->count; ++i) {
            uint16_t item = bucket->array[i];
            bitmap[item / 64] |= UINT64_C(1) << (item & 63);
        }
        bitmap[value / 64] |= UINT64_C(1) << (value & 63);
        xrtFree(bucket->array);
        bucket->array = NULL;
        bucket->bitmap = bitmap;
        bucket->capacity = 0;
        ++bucket->count;
        *added = 1;
        return 1;
    }
    if (bucket->count == bucket->capacity) {
        uint32_t capacity = bucket->capacity * 2;
        uint16_t *grown = xrtMalloc((size_t)capacity * sizeof(*grown));
        if (grown == NULL) return 0;
        memcpy(grown, bucket->array, (size_t)bucket->count * sizeof(*grown));
        xrtFree(bucket->array);
        bucket->array = grown;
        bucket->capacity = capacity;
    }
    memmove(bucket->array + position + 1, bucket->array + position,
        (bucket->count - position) * sizeof(*bucket->array));
    bucket->array[position] = value;
    ++bucket->count;
    *added = 1;
    return 1;
}

static int xr_has(const XRContainer *bucket, uint16_t value) {
    int found;
    if (bucket->bitmap != NULL) {
        return (bucket->bitmap[value / 64] & (UINT64_C(1) << (value & 63))) != 0;
    }
    xr_array_index(bucket, value, &found);
    return found;
}

static int xr_erase(XRContainer *bucket, uint16_t value) {
    int found;
    size_t position;
    if (bucket->bitmap != NULL) {
        uint64_t mask = UINT64_C(1) << (value & 63);
        uint64_t *word = bucket->bitmap + value / 64;
        if ((*word & mask) == 0) return 0;
        *word &= ~mask;
    } else {
        position = xr_array_index(bucket, value, &found);
        if (!found) return 0;
        memmove(bucket->array + position, bucket->array + position + 1,
            (bucket->count - position - 1) * sizeof(*bucket->array));
    }
    --bucket->count;
    return 1;
}

/* ------------ 公开生命周期与集合操作 ------------ */

void *xroaring_create(void) {
    xrtClearError();
    return xrtCalloc(1, sizeof(XRBitmap));
}

void xroaring_destroy(void *handle) {
    XRBitmap *set = handle;
    size_t i;
    if (set == NULL) return;
    for (i = 0; i < set->count; ++i) {
        xrtFree(set->items[i].array);
        xrtFree(set->items[i].bitmap);
    }
    xrtFree(set->items);
    xrtFree(set);
}

bool xroaring_add(void *handle, int64_t value) {
    XRBitmap *set = handle;
    uint32_t number;
    uint16_t key, low;
    size_t position;
    int found, added;
    xrtClearError();
    if (set == NULL) {
        xr_fail(XERR_ARGUMENT, "set is null");
        return false;
    }
    if (!xr_value(value, &number)) return false;
    key = (uint16_t)(number >> 16);
    low = (uint16_t)number;
    position = xr_bucket_index(set, key, &found);
    if (!found && !xr_create_bucket(set, position, key)) return false;
    if (!xr_insert(set->items + position, low, &added)) return false;
    set->total += (uint64_t)added;
    return true;
}

bool xroaring_remove(void *handle, int64_t value) {
    XRBitmap *set = handle;
    uint32_t number;
    size_t position;
    int found;
    XRContainer *bucket;
    xrtClearError();
    if (set == NULL) {
        xr_fail(XERR_ARGUMENT, "set is null");
        return false;
    }
    if (!xr_value(value, &number)) return false;
    position = xr_bucket_index(set, (uint16_t)(number >> 16), &found);
    if (!found) return false;
    bucket = set->items + position;
    if (!xr_erase(bucket, (uint16_t)number)) return false;
    --set->total;
    if (bucket->count == 0) {
        xrtFree(bucket->array);
        xrtFree(bucket->bitmap);
        memmove(bucket, bucket + 1, (set->count - position - 1) * sizeof(*bucket));
        --set->count;
    }
    return true;
}

bool xroaring_contains(void *handle, int64_t value) {
    XRBitmap *set = handle;
    uint32_t number;
    size_t position;
    int found;
    xrtClearError();
    if (set == NULL) {
        xr_fail(XERR_ARGUMENT, "set is null");
        return false;
    }
    if (!xr_value(value, &number)) return false;
    position = xr_bucket_index(set, (uint16_t)(number >> 16), &found);
    return found && xr_has(set->items + position, (uint16_t)number);
}

int64_t xroaring_count(void *handle) {
    XRBitmap *set = handle;
    xrtClearError();
    if (set == NULL) {
        xr_fail(XERR_ARGUMENT, "set is null");
        return -1;
    }
    return (int64_t)set->total;
}

void xroaring_clear(void *handle) {
    XRBitmap *set = handle;
    size_t i;
    xrtClearError();
    if (set == NULL) {
        xr_fail(XERR_ARGUMENT, "set is null");
        return;
    }
    for (i = 0; i < set->count; ++i) {
        xrtFree(set->items[i].array);
        xrtFree(set->items[i].bitmap);
    }
    set->count = 0;
    set->total = 0;
}

static int64_t xr_extreme(void *handle, int maximum) {
    XRBitmap *set = handle;
    XRContainer *bucket;
    uint32_t low;
    int index;
    xrtClearError();
    if (set == NULL) {
        xr_fail(XERR_ARGUMENT, "set is null");
        return -1;
    }
    if (set->count == 0) return -1;
    bucket = set->items + (maximum ? set->count - 1 : 0);
    if (bucket->bitmap == NULL) low = bucket->array[maximum ? bucket->count - 1 : 0];
    else {
        int word = maximum ? XR_BITMAP_WORDS - 1 : 0;
        for (;;) {
            uint64_t bits = bucket->bitmap[word];
            if (bits != 0) {
                index = maximum ? 63 : 0;
                while ((bits & (UINT64_C(1) << index)) == 0) index += maximum ? -1 : 1;
                low = (uint32_t)(word * 64 + index);
                break;
            }
            word += maximum ? -1 : 1;
        }
    }
    return ((int64_t)bucket->key << 16) | low;
}

int64_t xroaring_min(void *handle) { return xr_extreme(handle, 0); }
int64_t xroaring_max(void *handle) { return xr_extreme(handle, 1); }
