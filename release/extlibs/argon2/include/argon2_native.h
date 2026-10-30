#ifndef XLANG_ARGON2_NATIVE_H
#define XLANG_ARGON2_NATIVE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 操作入口清除旧错误；文本结果以 xrtFree 释放，失败返回 NULL 并设置 XRT 错误。 */
char *xargon2_hash(const char *password, size_t passwordSize,
    const char *salt, size_t saltSize, const char *secret, size_t secretSize,
    const char *ad, size_t adSize, long long timeCost, long long memoryCost,
    long long parallelism, long long hashLength, long long saltLength, long long type);
char *xargon2_hash_raw(const char *password, size_t passwordSize,
    const char *salt, size_t saltSize, const char *secret, size_t secretSize,
    const char *ad, size_t adSize, long long timeCost, long long memoryCost,
    long long parallelism, long long hashLength, long long saltLength, long long type);
/* PHC仅接受版本16/19；省略版本表示16。未知版本与损坏编码同为普通不匹配。 */
long long xargon2_verify(const char *password, size_t passwordSize,
    const char *encoded, size_t encodedSize, const char *secret, size_t secretSize,
    const char *ad, size_t adSize);
long long xargon2_needs_rehash(const char *encoded, size_t encodedSize, long long timeCost,
    long long memoryCost, long long parallelism);
/* 无效 PHC 返回拥有的空文本且不留错误，不等于资源失败的 NULL。 */
char *xargon2_extract(const char *encoded, size_t encodedSize);
char *xargon2_generate_salt(long long length);
long long xargon2_recommended_time(void);
long long xargon2_recommended_memory(void);
long long xargon2_recommended_parallelism(void);
long long xargon2_recommended_hashlen(void);
long long xargon2_recommended_saltlen(void);

#ifdef __cplusplus
}
#endif
#endif
