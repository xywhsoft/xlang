#ifndef CRC32_NATIVE_H
#define CRC32_NATIVE_H
#include "xrt.h"
struct xvalue;
#ifdef __cplusplus
extern "C" {
#endif

/* 与生成的 C 原型逐项对照；核心只读接口仍接受 const void*。 */
bool xcrc32_available(void);
const char* xcrc32_version(void);
const char* xcrc32_last_error(void);
long long xcrc32_hash(void* data, size_t size);
long long xcrc32_hash_str(const char* data, size_t size);
const char* xcrc32_hash_hex(void* data, size_t size);
const char* xcrc32_hash_str_hex(const char* data, size_t size);
long long xcrc32_file(const char* path, unsigned long long maximum, struct xvalue* callback);
const char* xcrc32_file_hex(const char* path, unsigned long long maximum, struct xvalue* callback);
void* xcrc32_hasher_create(unsigned long long maximum);
void xcrc32_hasher_destroy(void* handle);
bool xcrc32_hasher_update(void* handle, void* data, size_t size);
bool xcrc32_hasher_update_str(void* handle, const char* data, size_t size);
long long xcrc32_hasher_digest(void* handle);
const char* xcrc32_hasher_digest_hex(void* handle);
bool xcrc32_hasher_reset(void* handle);
unsigned long long xcrc32_hasher_input_bytes(void* handle);

#ifdef __cplusplus
}
#endif
#endif
