#ifndef XL_MD5_NATIVE_H
#define XL_MD5_NATIVE_H
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#include "xrt.h"
struct xvalue;
#ifdef __cplusplus
extern "C" {
#endif
bool xmd5_available(void);
const char* xmd5_version(void);
const char* xmd5_last_error(void);
const char* xmd5_hash_hex(void* data, size_t size);
const char* xmd5_hash_str_hex(const char* data, size_t size);
void* xmd5_hash_bytes(void* data, size_t size);
void* xmd5_hash_str_bytes(const char* data, size_t size);
const char* xmd5_file_hex(const char* path, unsigned long long maximum, struct xvalue* callback);
void* xmd5_file_bytes(const char* path, unsigned long long maximum, struct xvalue* callback);
void* xmd5_hasher_create(unsigned long long maximum);
void xmd5_hasher_destroy(void* handle);
bool xmd5_hasher_update(void* handle, void* data, size_t size);
bool xmd5_hasher_update_str(void* handle, const char* data, size_t size);
const char* xmd5_hasher_digest_hex(void* handle);
void* xmd5_hasher_digest_bytes(void* handle);
bool xmd5_hasher_reset(void* handle);
unsigned long long xmd5_hasher_input_bytes(void* handle);
#ifdef __cplusplus
}
#endif
#endif
