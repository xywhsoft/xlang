#ifndef XL_EXTLIB_TRIE_NATIVE_H
#define XL_EXTLIB_TRIE_NATIVE_H

#include <stdint.h>
#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* key 是明确长度的字节串，允许内嵌 NUL；句柄只能由本接口创建和销毁。 */
void *xtrie_create(void);
void xtrie_destroy(void *handle);
bool xtrie_insert(void *handle, const char *key, size_t size);
bool xtrie_contains(void *handle, const char *key, size_t size);
bool xtrie_starts_with(void *handle, const char *prefix, size_t size);
int64_t xtrie_count_prefix(void *handle, const char *prefix, size_t size);
bool xtrie_remove(void *handle, const char *key, size_t size);
void xtrie_clear(void *handle);
int64_t xtrie_count(void *handle);

#ifdef __cplusplus
}
#endif
#endif
