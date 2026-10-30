#include "../include/argon2_native.h"
#include "../../../include/xrt_decl.h"
#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
/* xl run单文件；正式GCC/manifest仍分别编译算法与薄绑定。 */
#include "../argon2_native.c"
#include "../vendor/argon2.c"
#include "../vendor/core.c"
#include "../vendor/encoding.c"
#include "../vendor/ref.c"
#undef G
#include "../vendor/blake2/blake2b.c"
#endif

#define LIT(text) (text), (sizeof(text) - 1u)

int main(void)
{
    /* 低成本与固定旧盐仅用于演示，真实存储使用默认配置并按环境标定。 */
    const char* legacy = "$argon2id$v=16$m=32,t=1,p=1$c29tZXNhbHQ$V5Quaw7ASul1EzygimxtXA";
    const char* unknown = "$argon2id$v=17$m=32,t=1,p=1$c29tZXNhbHQ$UF21A2AEUD+Ys5m/yX+2vw";
    const char* metadata = "$argon2id$v=19$m=32,t=4294967295,p=1$c29tZXNhbHQ$AAAAAA";
    char* upgraded = NULL;
    char* info = NULL;
    char malformed[512];
    unsigned long long cost;
    int result = 1;
    if (!xargon2_verify(LIT("pw"), legacy, strlen(legacy), LIT(""), LIT("")) || xrtGetError()) goto done;
    puts("legacy: verified");
    if (!xargon2_needs_rehash(legacy, strlen(legacy), 1, 32, 1) || xrtGetError()) goto done;
    puts("rehash: required");
    /* 先验证，再生成自动随机盐的新PHC；绝不只替换原字符串的版本字段。 */
    upgraded = xargon2_hash(LIT("pw"), LIT(""), LIT(""), LIT(""), 1, 32, 1, 16, 16, 2);
    if (!upgraded || !xargon2_verify(LIT("pw"), upgraded, strlen(upgraded), LIT(""), LIT("")) ||
        xargon2_needs_rehash(upgraded, strlen(upgraded), 1, 32, 1) || xrtGetError()) goto done;
    puts("upgrade: verified");
    if (xargon2_verify(LIT("pw"), unknown, strlen(unknown), LIT(""), LIT("")) || xrtGetError()) goto done;
    info = xargon2_extract(unknown, strlen(unknown));
    if (!info || *info || xrtGetError()) goto done;
    xrtFree(info);
    info = NULL;
    puts("unknown: rejected");
    size_t size = strlen(upgraded);
    if (size + 5 > sizeof(malformed)) goto done;
    memcpy(malformed, upgraded, size);
    memcpy(malformed + size, "\0tail", 5);
    if (xargon2_verify(LIT("pw"), malformed, size + 5, LIT(""), LIT("")) || xrtGetError()) goto done;
    puts("malformed: rejected");
    /* 只解析成本元数据，不执行高迭代次数的哈希。 */
    info = xargon2_extract(metadata, strlen(metadata));
    if (!info || sscanf(info, "%*u,%llu", &cost) != 1 || cost != 4294967295ULL || xrtGetError()) goto done;
    printf("metadata: %llu\n", cost);
    result = 0;
done:
    xrtFree(info);
    xrtFree(upgraded);
    if (result) fputs("Argon2 PHC policy example failed\n", stderr);
    return result;
}
