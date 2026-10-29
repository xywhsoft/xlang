#ifndef XLANG_SIPHASH_NATIVE_H
#define XLANG_SIPHASH_NATIVE_H
#include <stddef.h>
#include <stdbool.h>
struct xvalue;
#ifdef __cplusplus
extern "C" {
#endif

/* point 对应 void*，string 对应 const char*；Buffer 归调用方，文本借用 XRT 临时区。 */
bool xsiphash_available(void);
const char* xsiphash_version(void);
const char* xsiphash_last_error(void);
unsigned long long xsiphash_hash64(void* data, size_t size, void* key, size_t keySize);
unsigned long long xsiphash_hash64_str(const char* data, size_t size, void* key, size_t keySize);
unsigned long long xsiphash_hash64_key_str(void* data, size_t size, const char* key, size_t keySize);
unsigned long long xsiphash_hash64_str_key_str(const char* data, size_t size, const char* key, size_t keySize);
const char* xsiphash_hash64_hex(void* data, size_t size, void* key, size_t keySize);
const char* xsiphash_hash64_hex_str(const char* data, size_t size, void* key, size_t keySize);
const char* xsiphash_hash64_hex_key_str(void* data, size_t size, const char* key, size_t keySize);
const char* xsiphash_hash64_hex_str_key_str(const char* data, size_t size, const char* key, size_t keySize);
void* xsiphash_hash64_bytes(void* data, size_t size, void* key, size_t keySize);
void* xsiphash_hash64_bytes_str(const char* data, size_t size, void* key, size_t keySize);
void* xsiphash_hash64_bytes_key_str(void* data, size_t size, const char* key, size_t keySize);
void* xsiphash_hash64_bytes_str_key_str(const char* data, size_t size, const char* key, size_t keySize);
const char* xsiphash_hash128_hex(void* data, size_t size, void* key, size_t keySize);
const char* xsiphash_hash128_hex_str(const char* data, size_t size, void* key, size_t keySize);
const char* xsiphash_hash128_hex_key_str(void* data, size_t size, const char* key, size_t keySize);
const char* xsiphash_hash128_hex_str_key_str(const char* data, size_t size, const char* key, size_t keySize);
void* xsiphash_hash128_bytes(void* data, size_t size, void* key, size_t keySize);
void* xsiphash_hash128_bytes_str(const char* data, size_t size, void* key, size_t keySize);
void* xsiphash_hash128_bytes_key_str(void* data, size_t size, const char* key, size_t keySize);
void* xsiphash_hash128_bytes_str_key_str(const char* data, size_t size, const char* key, size_t keySize);
unsigned long long xsiphash_file64(const char* path, unsigned long long maximum, void* key, size_t keySize, struct xvalue* callback);
const char* xsiphash_file_hex(const char* path, unsigned long long maximum, void* key, size_t keySize, int bits, struct xvalue* callback);
void* xsiphash_file_bytes(const char* path, unsigned long long maximum, void* key, size_t keySize, int bits, struct xvalue* callback);
unsigned long long xsiphash_file64_key_str(const char* path, unsigned long long maximum, const char* key, size_t keySize, struct xvalue* callback);
const char* xsiphash_file_hex_key_str(const char* path, unsigned long long maximum, const char* key, size_t keySize, int bits, struct xvalue* callback);
void* xsiphash_file_bytes_key_str(const char* path, unsigned long long maximum, const char* key, size_t keySize, int bits, struct xvalue* callback);
void* xsiphash_hasher_create(void* key, size_t keySize, unsigned long long maximum);
void* xsiphash_hasher_create_key_str(const char* key, size_t keySize, unsigned long long maximum);
void xsiphash_hasher_destroy(void* handle);
bool xsiphash_hasher_update(void* handle, void* data, size_t size);
bool xsiphash_hasher_update_str(void* handle, const char* data, size_t size);
unsigned long long xsiphash_hasher_digest64(void* handle);
const char* xsiphash_hasher_digest64_hex(void* handle);
void* xsiphash_hasher_digest64_bytes(void* handle);
const char* xsiphash_hasher_digest128_hex(void* handle);
void* xsiphash_hasher_digest128_bytes(void* handle);
bool xsiphash_hasher_reset(void* handle);
unsigned long long xsiphash_hasher_input_bytes(void* handle);
void* xsiphash_hasher_adopt(void* handle);
void* xsiphash_hasher_clone(void* handle);

#ifdef __cplusplus
}
#endif
#endif
