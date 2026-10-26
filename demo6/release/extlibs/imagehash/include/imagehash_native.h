#ifndef XL_EXTLIB_IMAGEHASH_NATIVE_H
#define XL_EXTLIB_IMAGEHASH_NATIVE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

bool xihash_available(void);
const char *xihash_version(void);
const char *xihash_last_error(void);
/* 哈希字符串由 XRT 分配，C 调用方以 xrtFree 释放。 */
char *xihash_ahash(const char *path);
char *xihash_dhash(const char *path);
char *xihash_phash(const char *path);
int xihash_hamming(const char *left, size_t left_length,
    const char *right, size_t right_length);
bool xihash_valid(const char *hash, size_t length);

#ifdef __cplusplus
}
#endif

#endif
