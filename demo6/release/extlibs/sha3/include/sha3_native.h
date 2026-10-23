#ifndef XLANG_SHA3_NATIVE_H
#define XLANG_SHA3_NATIVE_H
#include <stddef.h>
#include <stdbool.h>
struct xvalue;
#ifdef __cplusplus
extern "C" {
#endif

bool xsha3_available(void);
const char* xsha3_version(void);
const char* xsha3_last_error(void);
const char* xsha3_hash_hex(void* data, size_t size, int bits);
const char* xsha3_hash_str_hex(const char* data, size_t size, int bits);
/* _owned 文本由调用方使用 xrtFree 释放；原 hex 接口仍借用 XRT 临时区。 */
void* xsha3_hash_hex_owned(void* data, size_t size, int bits);
void* xsha3_hash_str_hex_owned(const char* data, size_t size, int bits);
void* xsha3_hash_bytes(void* data, size_t size, int bits);
void* xsha3_hash_str_bytes(const char* data, size_t size, int bits);
const char* xshake_hash_hex(void* data, size_t size, int strength, size_t length);
const char* xshake_hash_str_hex(const char* data, size_t size, int strength, size_t length);
void* xshake_hash_hex_owned(void* data, size_t size, int strength, size_t length);
void* xshake_hash_str_hex_owned(const char* data, size_t size, int strength, size_t length);
void* xshake_hash_bytes(void* data, size_t size, int strength, size_t length);
void* xshake_hash_str_bytes(const char* data, size_t size, int strength, size_t length);
const char* xsha3_file_hex(const char* path, unsigned long long maximum, int bits, struct xvalue* callback);
void* xsha3_file_bytes(const char* path, unsigned long long maximum, int bits, struct xvalue* callback);
void* xsha3_hasher_create(int bits, unsigned long long maximum);
void* xsha3_shake_hasher_create(int strength, unsigned long long maximum);
void xsha3_hasher_destroy(void* handle);
bool xsha3_hasher_update(void* handle, void* data, size_t size);
bool xsha3_hasher_update_str(void* handle, const char* data, size_t size);
const char* xsha3_hasher_digest_hex(void* handle, size_t length);
void* xsha3_hasher_digest_hex_owned(void* handle, size_t length);
void* xsha3_hasher_digest_bytes(void* handle, size_t length);
bool xsha3_hasher_reset(void* handle);
unsigned long long xsha3_hasher_input_bytes(void* handle);
int xsha3_hasher_out_bytes(void* handle);
void* xsha3_hasher_adopt(void* handle);
void* xsha3_hasher_clone(void* handle);

#ifdef __cplusplus
}
#endif
#endif
