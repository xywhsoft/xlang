/* 确定性虚拟节点环；节点变更在同一把锁内一次性提交。 */
#ifndef XRT_MODULE_HASH64
#define XRT_MODULE_HASH64
#endif
#ifndef XRT_MODULE_MUTEX
#define XRT_MODULE_MUTEX
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#include "xrt.h"
#include "include/consistent_hash_native.h"
#include <stdlib.h>
#include <string.h>

#define XCH_DOMAIN "xlang.extlib.consistent_hash"
#define XCH_MAX_VNODES 1000000u

typedef struct xch_vnode {
    uint64 hash;
    char* name;
    uint32 replica;
} xch_vnode;

typedef struct xch_ring {
    xmutex guard;
    size_t replicas;
    char** nodes;
    size_t node_count;
    size_t node_capacity;
    xch_vnode* vnodes;
    size_t vnode_count;
    size_t vnode_capacity;
} xch_ring;

static int xch_compare(const void* left, const void* right)
{
    const xch_vnode* a = (const xch_vnode*)left;
    const xch_vnode* b = (const xch_vnode*)right;
    int name_order;
    if (a->hash != b->hash) return a->hash < b->hash ? -1 : 1;
    name_order = strcmp(a->name, b->name);
    if (name_order != 0) return name_order;
    return (a->replica > b->replica) - (a->replica < b->replica);
}

static void xch_error(xerrkind kind, const char* message)
{
    xrtSetErrorInfo(kind, XCH_DOMAIN, 0, message);
}

static bool xch_name_valid(const char* name)
{
    size_t size = 0u;
    if (name == NULL) return false;
    /* 超限名称只读到第 4097 字节，不扫描完整个不受限输入。 */
    while (size <= 4096u && name[size] != '\0') ++size;
    return size > 0u && size <= 4096u &&
        xrtUtf8Valid((xstrview){name, size}, NULL);
}

void* xchash_create(int64_t replicas)
{
    xch_ring* ring;
    xrtClearError();
    if (replicas < 1 || replicas > 10000) {
        xch_error(XERR_RANGE, "replicas must be 1..10000");
        return NULL;
    }
    ring = (xch_ring*)xrtCalloc(1u, sizeof(*ring));
    if (ring == NULL) return NULL;
    if (!xrtMutexInit(&ring->guard)) {
        xrtFree(ring);
        return NULL;
    }
    ring->replicas = (size_t)replicas;
    return ring;
}

void xchash_destroy(void* handle)
{
    xch_ring* ring = (xch_ring*)handle;
    size_t i;
    if (ring == NULL) return;
    /* 调用方应先结束其它线程对这个句柄的使用。 */
    for (i = 0u; i < ring->node_count; ++i) xrtFree(ring->nodes[i]);
    xrtFree(ring->nodes);
    xrtFree(ring->vnodes);
    xrtMutexUnit(&ring->guard);
    xrtFree(ring);
}

int64_t xchash_add_node(void* handle, const char* name)
{
    xch_ring* ring = (xch_ring*)handle;
    char** grown_nodes;
    xch_vnode* grown_vnodes;
    char* owned_name;
    size_t i, needed, capacity, name_size;
    xrtClearError();
    if (ring == NULL || !xch_name_valid(name)) {
        xch_error(XERR_ARGUMENT, "node name must be 1..4096 valid UTF-8 bytes");
        return 0;
    }
    name_size = strlen(name);
    if (!xrtMutexLock(&ring->guard)) return 0;
    for (i = 0u; i < ring->node_count; ++i) {
        if (strcmp(ring->nodes[i], name) == 0) {
            xrtMutexUnlock(&ring->guard);
            return 1;
        }
    }
    if (ring->vnode_count > XCH_MAX_VNODES - ring->replicas) {
        xch_error(XERR_RANGE, "virtual node limit exceeded");
        goto fail;
    }
    owned_name = xrtStrDup(name);
    if (owned_name == NULL) goto fail;
    if (ring->node_count == ring->node_capacity) {
        capacity = ring->node_capacity == 0u ? 4u : ring->node_capacity * 2u;
        grown_nodes = (char**)xrtRealloc(ring->nodes, capacity * sizeof(char*));
        if (grown_nodes == NULL) goto fail_name;
        ring->nodes = grown_nodes;
        ring->node_capacity = capacity;
    }
    needed = ring->vnode_count + ring->replicas;
    if (needed > ring->vnode_capacity) {
        capacity = ring->vnode_capacity == 0u ? ring->replicas : ring->vnode_capacity;
        while (capacity < needed) capacity = capacity > XCH_MAX_VNODES / 2u ?
            XCH_MAX_VNODES : capacity * 2u;
        grown_vnodes = (xch_vnode*)xrtRealloc(ring->vnodes,
            capacity * sizeof(xch_vnode));
        if (grown_vnodes == NULL) goto fail_name;
        ring->vnodes = grown_vnodes;
        ring->vnode_capacity = capacity;
    }
    for (i = 0u; i < ring->replicas; ++i) {
        xch_vnode* vnode = &ring->vnodes[ring->vnode_count + i];
        vnode->hash = xrtHash64Seed(name, name_size, (uint64)i);
        vnode->name = owned_name;
        vnode->replica = (uint32)i;
    }
    ring->nodes[ring->node_count++] = owned_name;
    ring->vnode_count = needed;
    qsort(ring->vnodes, ring->vnode_count, sizeof(xch_vnode), xch_compare);
    xrtMutexUnlock(&ring->guard);
    return 1;
fail_name:
    xrtFree(owned_name);
fail:
    xrtMutexUnlock(&ring->guard);
    return 0;
}

int64_t xchash_remove_node(void* handle, const char* name)
{
    xch_ring* ring = (xch_ring*)handle;
    char* target = NULL;
    size_t i, write = 0u;
    xrtClearError();
    if (ring == NULL || !xch_name_valid(name)) {
        xch_error(XERR_ARGUMENT, "node name must be 1..4096 valid UTF-8 bytes");
        return 0;
    }
    if (!xrtMutexLock(&ring->guard)) return 0;
    for (i = 0u; i < ring->node_count; ++i) {
        if (strcmp(ring->nodes[i], name) == 0) {
            target = ring->nodes[i];
            if (i + 1u < ring->node_count)
                memmove(&ring->nodes[i], &ring->nodes[i + 1u],
                    (ring->node_count - i - 1u) * sizeof(char*));
            --ring->node_count;
            break;
        }
    }
    if (target != NULL) {
        for (i = 0u; i < ring->vnode_count; ++i)
            if (ring->vnodes[i].name != target) ring->vnodes[write++] = ring->vnodes[i];
        ring->vnode_count = write;
        xrtFree(target);
    }
    xrtMutexUnlock(&ring->guard);
    return 1;
}

void* xchash_get_node(void* handle, const char* key)
{
    xch_ring* ring = (xch_ring*)handle;
    uint64 hash;
    size_t lo, hi, mid;
    void* result;
    xrtClearError();
    if (ring == NULL || key == NULL) {
        xch_error(XERR_ARGUMENT, "ring and key are required");
        return NULL;
    }
    /* 键哈希只依赖调用方输入，不占用保护节点环的锁。 */
    hash = xrtHash64(key, strlen(key));
    if (!xrtMutexLock(&ring->guard)) return NULL;
    if (ring->vnode_count == 0u) {
        result = xrtStrDup("");
    } else {
        lo = 0u;
        hi = ring->vnode_count;
        while (lo < hi) {
            mid = lo + (hi - lo) / 2u;
            if (ring->vnodes[mid].hash < hash) lo = mid + 1u;
            else hi = mid;
        }
        result = xrtStrDup(ring->vnodes[lo == ring->vnode_count ? 0u : lo].name);
    }
    xrtMutexUnlock(&ring->guard);
    return result;
}

int64_t xchash_node_count(void* handle)
{
    xch_ring* ring = (xch_ring*)handle;
    int64_t count;
    xrtClearError();
    if (ring == NULL) {
        xch_error(XERR_ARGUMENT, "ring is required");
        return -1;
    }
    if (!xrtMutexLock(&ring->guard)) return -1;
    count = (int64_t)ring->node_count;
    xrtMutexUnlock(&ring->guard);
    return count;
}

int64_t xchash_vnode_count(void* handle)
{
    xch_ring* ring = (xch_ring*)handle;
    int64_t count;
    xrtClearError();
    if (ring == NULL) {
        xch_error(XERR_ARGUMENT, "ring is required");
        return -1;
    }
    if (!xrtMutexLock(&ring->guard)) return -1;
    count = (int64_t)ring->vnode_count;
    xrtMutexUnlock(&ring->guard);
    return count;
}
