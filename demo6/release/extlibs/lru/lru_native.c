/* LRU/LFU 字符串缓存：XRT 映射负责键查找，双向链表负责淘汰次序。 */
#ifndef XRT_MODULE_MAP
#define XRT_MODULE_MAP
#endif
#ifndef XRT_MODULE_MUTEX
#define XRT_MODULE_MUTEX
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#include "xrt.h"
#include "include/lru_native.h"
#include <limits.h>
#include <string.h>

#define XLRU_DOMAIN "xlang.extlib.lru"

typedef struct xlru_entry {
    char* key;
    char* value;
    int64_t frequency;
    struct xlru_entry* previous;
    struct xlru_entry* next;
} xlru_entry;

typedef struct xlru_cache {
    xmutex guard;
    xmap* items;
    xlru_entry* head;
    xlru_entry* tail;
    int64_t capacity;
    int64_t count;
    int64_t hits;
    int64_t misses;
    int mode;
} xlru_cache;

static void xlru_error(xerrkind kind, const char* message)
{
    xrtSetErrorInfo(kind, XLRU_DOMAIN, 0, message);
}

static xbytesview xlru_key(const char* key)
{
    return (xbytesview){(const unsigned char*)key, strlen(key)};
}

static void xlru_detach(xlru_cache* cache, xlru_entry* entry)
{
    if (entry->previous != NULL) entry->previous->next = entry->next;
    else cache->head = entry->next;
    if (entry->next != NULL) entry->next->previous = entry->previous;
    else cache->tail = entry->previous;
}

static void xlru_front(xlru_cache* cache, xlru_entry* entry)
{
    entry->previous = NULL;
    entry->next = cache->head;
    if (cache->head != NULL) cache->head->previous = entry;
    else cache->tail = entry;
    cache->head = entry;
}

static void xlru_free_entry(xlru_entry* entry)
{
    xrtFree(entry->key);
    xrtFree(entry->value);
    xrtFree(entry);
}

static xlru_entry* xlru_victim(xlru_cache* cache)
{
    xlru_entry* victim = cache->tail;
    xlru_entry* entry;
    if (cache->mode == 0) return victim;
    /* LFU 同频时从尾端选最久未访问者。 */
    for (entry = cache->tail; entry != NULL; entry = entry->previous)
        if (entry->frequency < victim->frequency) victim = entry;
    return victim;
}

static bool xlru_drop(xlru_cache* cache, xlru_entry* entry)
{
    if (!xrtMapRemove(cache->items, xlru_key(entry->key))) return false;
    xlru_detach(cache, entry);
    xlru_free_entry(entry);
    --cache->count;
    return true;
}

void* xlru_create(int64_t capacity, int64_t mode)
{
    xlru_cache* cache;
    xrtClearError();
    if (capacity <= 0 || (mode != 0 && mode != 1)) {
        xlru_error(XERR_ARGUMENT, "capacity must be positive and mode must be LRU or LFU");
        return NULL;
    }
    cache = (xlru_cache*)xrtCalloc(1u, sizeof(*cache));
    if (cache == NULL) return NULL;
    cache->items = xrtMapCreate(sizeof(void*));
    if (cache->items == NULL) { xrtFree(cache); return NULL; }
    if (!xrtMutexInit(&cache->guard)) {
        xrtMapDestroy(cache->items);
        xrtFree(cache);
        return NULL;
    }
    cache->capacity = capacity;
    cache->mode = (int)mode;
    return cache;
}

void xlru_destroy(void* handle)
{
    xlru_cache* cache = (xlru_cache*)handle;
    xlru_entry* entry;
    if (cache == NULL) return;
    /* 销毁前调用方须结束其它线程对句柄的使用。 */
    entry = cache->head;
    while (entry != NULL) {
        xlru_entry* next = entry->next;
        xlru_free_entry(entry);
        entry = next;
    }
    xrtMapDestroy(cache->items);
    xrtMutexUnit(&cache->guard);
    xrtFree(cache);
}

int64_t xlru_put(void* handle, const char* key, const char* value)
{
    xlru_cache* cache = (xlru_cache*)handle;
    xlru_entry* entry;
    xlru_entry* victim;
    char* replacement;
    xrtClearError();
    if (cache == NULL || key == NULL) {
        xlru_error(XERR_ARGUMENT, "cache and key are required");
        return 0;
    }
    if (value == NULL) value = "";
    if (!xrtMutexLock(&cache->guard)) return 0;
    entry = (xlru_entry*)xrtMapGetPtr(cache->items, xlru_key(key));
    if (entry != NULL) {
        replacement = xrtStrDup(value);
        if (replacement == NULL) goto fail;
        xrtFree(entry->value);
        entry->value = replacement;
        if (entry->frequency < INT64_MAX) ++entry->frequency;
        xlru_detach(cache, entry);
        xlru_front(cache, entry);
    } else {
        victim = cache->count == cache->capacity ? xlru_victim(cache) : NULL;
        entry = (xlru_entry*)xrtCalloc(1u, sizeof(*entry));
        if (entry == NULL) goto fail;
        entry->key = xrtStrDup(key);
        entry->value = xrtStrDup(value);
        if (entry->key == NULL || entry->value == NULL ||
            !xrtMapSetPtr(cache->items, xlru_key(key), entry)) {
            xlru_free_entry(entry);
            goto fail;
        }
        entry->frequency = 1;
        xlru_front(cache, entry);
        ++cache->count;
        if (victim != NULL && !xlru_drop(cache, victim)) goto fail;
    }
    xrtMutexUnlock(&cache->guard);
    return 1;
fail:
    xrtMutexUnlock(&cache->guard);
    return 0;
}

void* xlru_get(void* handle, const char* key, const char* fallback)
{
    xlru_cache* cache = (xlru_cache*)handle;
    xlru_entry* entry;
    void* result;
    xrtClearError();
    if (cache == NULL || key == NULL) {
        xlru_error(XERR_ARGUMENT, "cache and key are required");
        return NULL;
    }
    if (!xrtMutexLock(&cache->guard)) return NULL;
    entry = (xlru_entry*)xrtMapGetPtr(cache->items, xlru_key(key));
    result = xrtStrDup(entry != NULL ? entry->value :
        (fallback != NULL ? fallback : ""));
    if (result != NULL) {
        if (entry != NULL) {
            if (cache->hits < INT64_MAX) ++cache->hits;
            if (entry->frequency < INT64_MAX) ++entry->frequency;
            xlru_detach(cache, entry);
            xlru_front(cache, entry);
        } else if (cache->misses < INT64_MAX) {
            ++cache->misses;
        }
    }
    xrtMutexUnlock(&cache->guard);
    return result;
}

int64_t xlru_contains(void* handle, const char* key)
{
    xlru_cache* cache = (xlru_cache*)handle;
    int64_t result;
    xrtClearError();
    if (cache == NULL || key == NULL) {
        xlru_error(XERR_ARGUMENT, "cache and key are required");
        return -1;
    }
    if (!xrtMutexLock(&cache->guard)) return -1;
    result = xrtMapHas(cache->items, xlru_key(key)) ? 1 : 0;
    xrtMutexUnlock(&cache->guard);
    return result;
}

int64_t xlru_remove(void* handle, const char* key)
{
    xlru_cache* cache = (xlru_cache*)handle;
    xlru_entry* entry;
    int64_t result;
    xrtClearError();
    if (cache == NULL || key == NULL) {
        xlru_error(XERR_ARGUMENT, "cache and key are required");
        return -1;
    }
    if (!xrtMutexLock(&cache->guard)) return -1;
    entry = (xlru_entry*)xrtMapGetPtr(cache->items, xlru_key(key));
    result = entry == NULL ? 0 : (xlru_drop(cache, entry) ? 1 : -1);
    xrtMutexUnlock(&cache->guard);
    return result;
}

int64_t xlru_clear(void* handle)
{
    xlru_cache* cache = (xlru_cache*)handle;
    xlru_entry* entry;
    xrtClearError();
    if (cache == NULL) {
        xlru_error(XERR_ARGUMENT, "cache is required");
        return 0;
    }
    if (!xrtMutexLock(&cache->guard)) return 0;
    entry = cache->head;
    while (entry != NULL) {
        xlru_entry* next = entry->next;
        xlru_free_entry(entry);
        entry = next;
    }
    xrtMapClear(cache->items);
    cache->head = cache->tail = NULL;
    cache->count = cache->hits = cache->misses = 0;
    xrtMutexUnlock(&cache->guard);
    return 1;
}

static int64_t xlru_stat(void* handle, int kind)
{
    xlru_cache* cache = (xlru_cache*)handle;
    int64_t result;
    xrtClearError();
    if (cache == NULL) {
        xlru_error(XERR_ARGUMENT, "cache is required");
        return -1;
    }
    if (!xrtMutexLock(&cache->guard)) return -1;
    switch (kind) {
    case 0: result = cache->count; break;
    case 1: result = cache->capacity; break;
    case 2: result = cache->hits; break;
    default: result = cache->misses; break;
    }
    xrtMutexUnlock(&cache->guard);
    return result;
}

int64_t xlru_count(void* handle) { return xlru_stat(handle, 0); }
int64_t xlru_capacity(void* handle) { return xlru_stat(handle, 1); }
int64_t xlru_hits(void* handle) { return xlru_stat(handle, 2); }
int64_t xlru_misses(void* handle) { return xlru_stat(handle, 3); }
