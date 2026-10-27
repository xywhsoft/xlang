#ifndef XCH_NATIVE_H
#define XCH_NATIVE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void* xchash_create(int64_t replicas);
void xchash_destroy(void* ring);
int64_t xchash_add_node(void* ring, const char* name);
int64_t xchash_remove_node(void* ring, const char* name);
/* 返回独立的 UTF-8 字符串，调用方以 xrtFree 释放。空环返回空串。 */
char* xchash_get_node(void* ring, const char* key);
int64_t xchash_node_count(void* ring);
int64_t xchash_vnode_count(void* ring);

#ifdef __cplusplus
}
#endif
#endif
