#ifndef XLANG_BLAKE2_NATIVE_H
#define XLANG_BLAKE2_NATIVE_H
#ifndef XRT_MODULE_VALUE
#define XRT_MODULE_VALUE
#endif
#include "xrt.h"
#ifdef __cplusplus
extern "C" {
#endif

void* xblake2b_hasher_create_keyed_span(size_t outlen, xbytesview key,
    unsigned long long maximum);
void* xblake2s_hasher_create_keyed_span(size_t outlen, xbytesview key,
    unsigned long long maximum);

/* 薄绑定物理 ABI；核心 const 数据接口见 xrt_blake2.h。 */
bool xblake2_available(void);
const char* xblake2_version(void);
const char* xblake2_last_error(void);
const char* xblake2b_hash_hex(void* data, size_t size,
    size_t outlen, void* key, size_t keylen);
const char* xblake2b_hash_str_hex(const char* data, size_t size,
    size_t outlen, const char* key, size_t keylen);
void* xblake2b_hash_mac_bytes(void* data, size_t size,
    void* key, size_t keylen, size_t outlen);
void* xblake2b_hash_bytes(void* data, size_t size, size_t outlen);
void* xblake2b_hash_str_bytes(const char* data, size_t size, size_t outlen);
void* xblake2b_hash_mac_str_bytes(const char* data, size_t size,
    const char* key, size_t keylen, size_t outlen);
const char* xblake2b_file_hex(const char* path, unsigned long long maximum,
    size_t outlen, xvalue* callback);
void* xblake2b_hasher_create_keyed(size_t outlen, void* key,
    size_t keylen, unsigned long long maximum);
/* 借用 bytes 值并读取真实长度；不会保留调用方的值。 */
void* xblake2b_hasher_create_keyed_bytes(size_t outlen, xvalue* key,
    unsigned long long maximum);
void* xblake2b_hasher_create(size_t outlen, unsigned long long maximum);
void xblake2b_hasher_destroy(void* handle);
bool xblake2b_hasher_update(void* handle, void* data, size_t size);
bool xblake2b_hasher_update_str(void* handle, const char* text, size_t size);
const char* xblake2b_hasher_digest_hex(void* handle);
void* xblake2b_hasher_digest_bytes(void* handle);
bool xblake2b_hasher_reset(void* handle);
unsigned long long xblake2b_hasher_input_bytes(void* handle);
void* xblake2b_hasher_adopt(void* handle);
void* xblake2b_hasher_clone(void* handle);
const char* xblake2s_hash_hex(void* data, size_t size,
    size_t outlen, void* key, size_t keylen);
const char* xblake2s_hash_str_hex(const char* data, size_t size,
    size_t outlen, const char* key, size_t keylen);
void* xblake2s_hash_mac_bytes(void* data, size_t size,
    void* key, size_t keylen, size_t outlen);
void* xblake2s_hash_bytes(void* data, size_t size, size_t outlen);
void* xblake2s_hash_str_bytes(const char* data, size_t size, size_t outlen);
void* xblake2s_hash_mac_str_bytes(const char* data, size_t size,
    const char* key, size_t keylen, size_t outlen);
const char* xblake2s_file_hex(const char* path, unsigned long long maximum,
    size_t outlen, xvalue* callback);
void* xblake2s_hasher_create_keyed(size_t outlen, void* key,
    size_t keylen, unsigned long long maximum);
/* 与 BLAKE2b 入口一致，长度上限由现有核心校验。 */
void* xblake2s_hasher_create_keyed_bytes(size_t outlen, xvalue* key,
    unsigned long long maximum);
void* xblake2s_hasher_create(size_t outlen, unsigned long long maximum);
void xblake2s_hasher_destroy(void* handle);
bool xblake2s_hasher_update(void* handle, void* data, size_t size);
bool xblake2s_hasher_update_str(void* handle, const char* text, size_t size);
const char* xblake2s_hasher_digest_hex(void* handle);
void* xblake2s_hasher_digest_bytes(void* handle);
bool xblake2s_hasher_reset(void* handle);
unsigned long long xblake2s_hasher_input_bytes(void* handle);
void* xblake2s_hasher_adopt(void* handle);
void* xblake2s_hasher_clone(void* handle);

#ifdef __cplusplus
}
#endif
#endif
