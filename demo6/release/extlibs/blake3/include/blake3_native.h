#ifndef XLANG_BLAKE3_NATIVE_H
#define XLANG_BLAKE3_NATIVE_H
#include "xrt.h"
/* 仅借用回调指针，不要求包含头文件时启用整个 value 模块。 */
struct xvalue;
#ifdef __cplusplus
extern "C" {
#endif

/* point 的物理 ABI 是 void*；只在进入核心接口时转换为 const 借用。 */
bool xblake3_available(void);
const char* xblake3_version(void);
const char* xblake3_last_error(void);
const char* xblake3_hash_hex(void* data, size_t size, size_t outlen);
const char* xblake3_hash_str_hex(const char* data, size_t size, size_t outlen);
void* xblake3_hash_bytes(void* data, size_t size, size_t outlen);
void* xblake3_hash_str_bytes(const char* data, size_t size, size_t outlen);
const char* xblake3_hash_mac_hex(void* data, size_t size, void* key, size_t keylen, size_t outlen);
const char* xblake3_hash_mac_str_hex(const char* data, size_t size, const char* key, size_t keylen, size_t outlen);
const char* xblake3_hash_kdf_hex(void* data, size_t size, const char* context, size_t context_len, size_t outlen);
const char* xblake3_hash_kdf_str_hex(const char* data, size_t size, const char* context, size_t context_len, size_t outlen);
const char* xblake3_file_hex(const char* path, unsigned long long maximum, size_t outlen, struct xvalue* callback);
void* xblake3_hasher_create(unsigned long long maximum);
void* xblake3_hasher_create_keyed(void* key, size_t keylen, unsigned long long maximum);
/* 借用 bytes 值并校验真实长度为 32；密钥复制进新状态。 */
void* xblake3_hasher_create_keyed_bytes(struct xvalue* key, unsigned long long maximum);
void* xblake3_hasher_create_kdf(const char* context, size_t context_len, unsigned long long maximum);
void xblake3_hasher_destroy(void* handle);
bool xblake3_hasher_update(void* handle, void* data, size_t size);
bool xblake3_hasher_update_str(void* handle, const char* data, size_t size);
void* xblake3_hasher_digest_bytes(void* handle, size_t outlen);
const char* xblake3_hasher_digest_hex(void* handle, size_t outlen);
bool xblake3_hasher_reset(void* handle);
unsigned long long xblake3_hasher_input_bytes(void* handle);
void* xblake3_hasher_adopt(void* handle);
void* xblake3_hasher_clone(void* handle);

#ifdef __cplusplus
}
#endif
#endif
