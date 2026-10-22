#ifndef XLANG_ARGON2_NATIVE_H
#define XLANG_ARGON2_NATIVE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 操作入口清除旧错误；返回字符串借用 XRT 临时区，失败详情见 xrtGetError。 */
const char *xargon2_hash(const char *password, size_t passwordSize,
    const char *salt, size_t saltSize, const char *secret, size_t secretSize,
    const char *ad, size_t adSize, long long timeCost, long long memoryCost,
    long long parallelism, long long hashLength, long long saltLength, long long type);
const char *xargon2_hash_raw(const char *password, size_t passwordSize,
    const char *salt, size_t saltSize, const char *secret, size_t secretSize,
    const char *ad, size_t adSize, long long timeCost, long long memoryCost,
    long long parallelism, long long hashLength, long long saltLength, long long type);
long long xargon2_verify(const char *password, size_t passwordSize,
    const char *encoded, size_t encodedSize, const char *secret, size_t secretSize,
    const char *ad, size_t adSize);
long long xargon2_needs_rehash(const char *encoded, size_t encodedSize, long long timeCost,
    long long memoryCost, long long parallelism);
const char *xargon2_extract(const char *encoded, size_t encodedSize);
const char *xargon2_generate_salt(long long length);
long long xargon2_recommended_time(void);
long long xargon2_recommended_memory(void);
long long xargon2_recommended_parallelism(void);
long long xargon2_recommended_hashlen(void);
long long xargon2_recommended_saltlen(void);

#ifdef __cplusplus
}
#endif
#endif
