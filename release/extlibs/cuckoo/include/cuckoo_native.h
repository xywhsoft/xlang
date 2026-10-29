#ifndef XL_EXTLIB_CUCKOO_NATIVE_H
#define XL_EXTLIB_CUCKOO_NATIVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void *xcuckoo_create(int64_t capacity);
void xcuckoo_destroy(void *handle);
bool xcuckoo_add(void *handle, const char *data, size_t size);
bool xcuckoo_contains(void *handle, const char *data, size_t size);
bool xcuckoo_remove(void *handle, const char *data, size_t size);
void xcuckoo_clear(void *handle);
int64_t xcuckoo_count(void *handle);
int64_t xcuckoo_buckets(void *handle);

#ifdef __cplusplus
}
#endif

#endif
