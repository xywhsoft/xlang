/* XRT 路径元数据驱动的轮询监视；路径始终按 UTF-8 处理。 */
#include <stdint.h>
#include <string.h>

#ifndef XRT_MODULE_DIR
#define XRT_MODULE_DIR
#endif
#ifndef XRT_MODULE_PATH
#define XRT_MODULE_PATH
#endif
#ifndef XRT_MODULE_HASH64
#define XRT_MODULE_HASH64
#endif
#include "xrt.h"
#include "watch_native.h"

#define XWATCH_DOMAIN "xlang.extlib.watch"

typedef struct {
    char *path;      /* 事件与 path() 保留调用方写法。 */
    char *absolute;  /* 文件扫描固定在 add() 时的目标。 */
    xfileinfo info;
    bool exists;
    uint64_t children;
    size_t child_count;
} XWatchEntry;

typedef struct {
    XWatchEntry *entries;
    size_t length;
    size_t capacity;
    size_t active;
    size_t cursor;
} XWatcher;

static void xwatch_error(xerrkind kind, const char *message) {
    xrtSetErrorInfo(kind, XWATCH_DOMAIN, 0, message);
}

/* 缺失是合法的监视状态；其他文件系统错误仍交给调用方。 */
static bool xwatch_stat(const char *path, bool *exists, xfileinfo *info) {
    const xerror *error;
    if (xrtPathStat(path, true, info)) {
        *exists = true;
        return true;
    }
    error = xrtGetError();
    if (error && xrtErrorKind(error) == XERR_NOT_FOUND) {
        xrtClearError();
        *exists = false;
        memset(info, 0, sizeof(*info));
        return true;
    }
    return false;
}

static bool xwatch_changed(const xfileinfo *before, const xfileinfo *after) {
    if (before->Type != after->Type || before->Available != after->Available)
        return true;
    if ((before->Available & XFILE_INFO_IDENTITY) &&
        (before->Device != after->Device ||
         before->Identity != after->Identity)) return true;
    if ((before->Available & XFILE_INFO_SIZE) && before->Size != after->Size)
        return true;
    if ((before->Available & XFILE_INFO_CREATE_TIME) &&
        before->Created != after->Created) return true;
    if ((before->Available & XFILE_INFO_MODIFY_TIME) &&
        before->Modified != after->Modified) return true;
    if ((before->Available & XFILE_INFO_CHANGE_TIME) &&
        before->Changed != after->Changed) return true;
    return false;
}

/* 目录元数据并不保证因子项变化而更新；按名称与元数据汇总直接子项。 */
static bool xwatch_children(const char *path, uint64_t *digest, size_t *count) {
    xdir dir = xrtDirOpen(path, XDIR_STAT);
    xdirentry item;
    xdirnext next;
    uint64_t hash = 0;
    size_t total = 0;
    if (!dir) return false;
    while ((next = xrtDirNext(dir, &item)) == XDIR_NEXT_ITEM) {
        const xfileinfo *info = &item.Info;
        /* 字段有序哈希，不能先 XOR，让常常相同的 mtime/ctime 互相抵消。 */
        const uint64_t fields[] = {
            (uint64_t)info->Type, info->Available,
            (info->Available & XFILE_INFO_SIZE) ? info->Size : 0,
            (info->Available & XFILE_INFO_MODIFY_TIME) ? (uint64_t)info->Modified : 0,
            (info->Available & XFILE_INFO_CHANGE_TIME) ? (uint64_t)info->Changed : 0,
            (info->Available & XFILE_INFO_CREATE_TIME) ? (uint64_t)info->Created : 0,
            (info->Available & XFILE_INFO_IDENTITY) ? info->Device : 0,
            (info->Available & XFILE_INFO_IDENTITY) ? info->Identity : 0
        };
        uint64_t seed = xrtHash64(fields, sizeof(fields));
        hash ^= xrtHash64Seed(item.Name.Data, item.Name.Size, seed);
        total++;
    }
    if (next == XDIR_NEXT_ERROR) {
        xerror *failure = xrtTakeError();
        (void)xrtDirClose(dir);
        xrtSetErrorTake(failure);
        return false;
    }
    if (!xrtDirClose(dir)) return false;
    *digest = hash;
    *count = total;
    return true;
}

/* 目录可能在 stat 后、枚举前消失；只对这种竞争重新取样一次。 */
static bool xwatch_snapshot(const char *path, bool *exists, xfileinfo *info,
                            uint64_t *children, size_t *child_count) {
    unsigned attempt;
    for (attempt = 0; attempt < 2; ++attempt) {
        const xerror *error;
        if (!xwatch_stat(path, exists, info)) return false;
        *children = 0;
        *child_count = 0;
        if (!*exists || info->Type != XFILE_TYPE_DIRECTORY) {
            xrtClearError();
            return true;
        }
        if (xwatch_children(path, children, child_count)) {
            xrtClearError();
            return true;
        }
        error = xrtGetError();
        if (attempt != 0 || !error ||
            (xrtErrorKind(error) != XERR_NOT_FOUND &&
             xrtErrorKind(error) != XERR_TYPE)) return false;
    }
    return false;
}

static char *xwatch_event(const char *kind, const char *path) {
    size_t kind_len = strlen(kind);
    size_t path_len = strlen(path);
    char *result;
    if (path_len > SIZE_MAX - kind_len - 2) {
        xwatch_error(XERR_RANGE, "watch event length exceeds size range");
        return NULL;
    }
    result = xrtMalloc(kind_len + path_len + 2);
    if (!result) return NULL;
    memcpy(result, kind, kind_len);
    result[kind_len] = ':';
    memcpy(result + kind_len + 1, path, path_len + 1);
    return result;
}

void *xwatch_create(void) {
    xrtClearError();
    return xrtCalloc(1, sizeof(XWatcher));
}

void xwatch_destroy(void *handle) {
    XWatcher *watcher = handle;
    size_t i;
    if (!watcher) return;
    for (i = 0; i < watcher->length; ++i) {
        xrtFree(watcher->entries[i].path);
        xrtFree(watcher->entries[i].absolute);
    }
    xrtFree(watcher->entries);
    xrtFree(watcher);
}

int64_t xwatch_add(void *handle, const char *path) {
    XWatcher *watcher = handle;
    XWatchEntry *entry;
    xfileinfo info;
    bool exists;
    size_t index;
    char *owned;
    char *absolute;
    uint64_t children = 0;
    size_t child_count = 0;
    xrtClearError();
    if (!watcher || !path || !*path) {
        xwatch_error(XERR_ARGUMENT, "watch path or instance is empty");
        return -1;
    }
    absolute = xrtPathAbs(path);
    if (!absolute) return -1;
    if (!xwatch_snapshot(absolute, &exists, &info, &children, &child_count))
        goto failed;
    owned = xrtStrDup(path);
    if (!owned) goto failed;
    for (index = 0; index < watcher->length; index++) {
        if (!watcher->entries[index].path) break;
    }
    if (index == watcher->length) {
        if (watcher->length >= INT64_MAX) {
            xwatch_error(XERR_RANGE, "too many watched paths");
            goto failed_owned;
        }
        if (watcher->length == watcher->capacity) {
            size_t capacity = watcher->capacity ? watcher->capacity * 2 : 8;
            XWatchEntry *grown;
            if (capacity < watcher->capacity ||
                capacity > SIZE_MAX / sizeof(XWatchEntry)) {
                xwatch_error(XERR_RANGE, "too many watched paths");
                goto failed_owned;
            }
            grown = xrtRealloc(watcher->entries, capacity * sizeof(XWatchEntry));
            if (!grown) goto failed_owned;
            memset(grown + watcher->capacity, 0,
                (capacity - watcher->capacity) * sizeof(XWatchEntry));
            watcher->entries = grown;
            watcher->capacity = capacity;
        }
        watcher->length++;
    }
    entry = &watcher->entries[index];
    entry->path = owned;
    entry->absolute = absolute;
    entry->info = info;
    entry->exists = exists;
    entry->children = children;
    entry->child_count = child_count;
    watcher->active++;
    return (int64_t)index;
failed_owned:
    xrtFree(owned);
failed:
    xrtFree(absolute);
    return -1;
}

bool xwatch_remove(void *handle, int64_t index) {
    XWatcher *watcher = handle;
    XWatchEntry *entry;
    xrtClearError();
    if (!watcher || index < 0 || (uint64_t)index >= watcher->length) return false;
    entry = &watcher->entries[index];
    if (!entry->path) return false;
    xrtFree(entry->path);
    xrtFree(entry->absolute);
    memset(entry, 0, sizeof(*entry));
    watcher->active--;
    return true;
}

/* 每次只返回一个独立拥有的事件，扫描游标避免前面的活跃路径饿死后面的。 */
char *xwatch_poll(void *handle) {
    XWatcher *watcher = handle;
    size_t i;
    xrtClearError();
    if (!watcher) {
        xwatch_error(XERR_ARGUMENT, "watch instance is null");
        return NULL;
    }
    for (i = 0; i < watcher->length; i++) {
        size_t index = (watcher->cursor + i) % watcher->length;
        XWatchEntry *entry = &watcher->entries[index];
        xfileinfo info;
        bool exists;
        uint64_t children = 0;
        size_t child_count = 0;
        const char *kind;
        char *event;
        if (!entry->path) continue;
        if (!xwatch_snapshot(entry->absolute, &exists, &info,
                             &children, &child_count)) return NULL;
        if (entry->exists == exists &&
            (!exists || (!xwatch_changed(&entry->info, &info) &&
                entry->children == children &&
                entry->child_count == child_count))) continue;
        kind = !entry->exists ? "CREATED" : !exists ? "DELETED" : "MODIFIED";
        event = xwatch_event(kind, entry->path);
        if (!event) return NULL;
        entry->exists = exists;
        entry->info = info;
        entry->children = children;
        entry->child_count = child_count;
        watcher->cursor = (index + 1) % watcher->length;
        return event;
    }
    return xrtStrDup("");
}

int64_t xwatch_count(void *handle) {
    XWatcher *watcher = handle;
    xrtClearError();
    return watcher ? (int64_t)watcher->active : 0;
}

const char *xwatch_path(void *handle, int64_t index) {
    XWatcher *watcher = handle;
    xrtClearError();
    if (!watcher || index < 0 || (uint64_t)index >= watcher->length ||
        !watcher->entries[index].path) return "";
    return watcher->entries[index].path;
}

bool xwatch_exists(const char *path) {
    xrtClearError();
    return path && xrtPathExists(path);
}
