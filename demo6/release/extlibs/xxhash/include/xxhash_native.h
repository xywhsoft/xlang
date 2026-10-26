#ifndef XLANG_XXHASH_NATIVE_H
#define XLANG_XXHASH_NATIVE_H
#include "xrt.h"
struct xvalue;
#ifdef __cplusplus
extern "C" {
#endif

bool xxxhash_available(void);
const char* xxxhash_version(void);
const char* xxxhash_last_error(void);
unsigned long long xxxhash_hash64(void* data, size_t size, unsigned long long seed);
unsigned long long xxxhash_hash64_str(const char* data, size_t size, unsigned long long seed);
const char* xxxhash_hash64_hex(void* data, size_t size, unsigned long long seed);
const char* xxxhash_hash64_str_hex(const char* data, size_t size, unsigned long long seed);
unsigned int xxxhash_hash32(void* data, size_t size, unsigned int seed);
unsigned int xxxhash_hash32_str(const char* data, size_t size, unsigned int seed);
const char* xxxhash_hash32_hex(void* data, size_t size, unsigned int seed);
const char* xxxhash_hash32_str_hex(const char* data, size_t size, unsigned int seed);
unsigned long long xxxhash_hash3_64(void* data, size_t size, unsigned long long seed);
unsigned long long xxxhash_hash3_64_str(const char* data, size_t size, unsigned long long seed);
const char* xxxhash_hash3_64_hex(void* data, size_t size, unsigned long long seed);
const char* xxxhash_hash3_64_str_hex(const char* data, size_t size, unsigned long long seed);
const char* xxxhash_hash3_128_hex(void* data, size_t size, unsigned long long seed);
const char* xxxhash_hash3_128_str_hex(const char* data, size_t size, unsigned long long seed);
unsigned long long xxxhash_file64(const char* path, unsigned long long seed, unsigned long long maximum, struct xvalue* callback);
const char* xxxhash_file64_hex(const char* path, unsigned long long seed, unsigned long long maximum, struct xvalue* callback);
void* xxxhash_hasher_create(unsigned long long seed, unsigned long long maximum);
void xxxhash_hasher_destroy(void* handle);
bool xxxhash_hasher_update(void* handle, void* data, size_t size);
bool xxxhash_hasher_update_str(void* handle, const char* data, size_t size);
unsigned long long xxxhash_hasher_digest(void* handle);
const char* xxxhash_hasher_digest_hex(void* handle);
bool xxxhash_hasher_reset(void* handle);
unsigned long long xxxhash_hasher_input_bytes(void* handle);

#ifdef __cplusplus
}
#endif
#endif
