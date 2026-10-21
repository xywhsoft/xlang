#ifndef XL_MURMUR3_NATIVE_H
#define XL_MURMUR3_NATIVE_H
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#include "xrt.h"
struct xvalue;
#ifdef __cplusplus
extern "C" {
#endif
bool xmurmur3_available(void);
const char* xmurmur3_version(void);
const char* xmurmur3_last_error(void);
long long xmurmur3_hash32(void* data, size_t size, unsigned int seed);
long long xmurmur3_hash32_str(const char* data, size_t size, unsigned int seed);
const char* xmurmur3_hash32_hex(void* data, size_t size, unsigned int seed);
const char* xmurmur3_hash32_str_hex(const char* data, size_t size, unsigned int seed);
void* xmurmur3_hash32_bytes(void* data, size_t size, unsigned int seed);
void* xmurmur3_hash32_str_bytes(const char* data, size_t size, unsigned int seed);
const char* xmurmur3_hash128_hex(void* data, size_t size, unsigned int seed);
const char* xmurmur3_hash128_str_hex(const char* data, size_t size, unsigned int seed);
void* xmurmur3_hash128_bytes(void* data, size_t size, unsigned int seed);
void* xmurmur3_hash128_str_bytes(const char* data, size_t size, unsigned int seed);
long long xmurmur3_file32(const char* path, unsigned long long maximum, unsigned int seed, struct xvalue* callback);
const char* xmurmur3_file128_hex(const char* path, unsigned long long maximum, unsigned int seed, struct xvalue* callback);
void* xmurmur3_hasher_create(unsigned int seed, unsigned long long maximum);
void xmurmur3_hasher_destroy(void* handle);
bool xmurmur3_hasher_update(void* handle, void* data, size_t size);
bool xmurmur3_hasher_update_str(void* handle, const char* data, size_t size);
const char* xmurmur3_hasher_digest_hex(void* handle);
void* xmurmur3_hasher_digest_bytes(void* handle);
bool xmurmur3_hasher_reset(void* handle);
unsigned long long xmurmur3_hasher_input_bytes(void* handle);
void* xmurmur3_hasher_adopt(void* handle);
void* xmurmur3_hasher_clone(void* handle);
#ifdef __cplusplus
}
#endif
#endif
