#ifndef XL_EXTLIB_SKIPLIST_NATIVE_H
#define XL_EXTLIB_SKIPLIST_NATIVE_H

#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#include <stdint.h>
#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 所有键和值均为明确长度的字节串；返回的 xvalue 由调用方释放。 */
void *xskiplist_create(void);
void xskiplist_destroy(void *handle);
bool xskiplist_set(void *handle, const char *key, size_t keySize,
    const char *value, size_t valueSize);
xvalue *xskiplist_get(void *handle, const char *key, size_t keySize);
bool xskiplist_contains(void *handle, const char *key, size_t keySize);
bool xskiplist_remove(void *handle, const char *key, size_t keySize);
void xskiplist_clear(void *handle);
int64_t xskiplist_count(void *handle);
xvalue *xskiplist_keys(void *handle);
xvalue *xskiplist_range(void *handle, const char *start, size_t startSize,
    const char *end, size_t endSize);

#ifdef __cplusplus
}
#endif
#endif
