/* 与 argon2_basic.xl 同结果；低成本仅用于演示，真实密码存储使用随机盐和适当成本。 */
#if !defined(_WIN32) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE 1
#endif
#include "../../extlibs/argon2/argon2_native.c"
#if defined(__GNUC__) && !defined(__TINYC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
#endif
#include "../../extlibs/argon2/vendor/argon2.c"
#include "../../extlibs/argon2/vendor/core.c"
#include "../../extlibs/argon2/vendor/encoding.c"
#include "../../extlibs/argon2/vendor/ref.c"
/* 单文件演示中隔离上游编译单元的私有宏。 */
#undef G
#include "../../extlibs/argon2/vendor/blake2/blake2b.c"
#if defined(__GNUC__) && !defined(__TINYC__)
#pragma GCC diagnostic pop
#endif

#define LIT(text) (text), (sizeof(text) - 1u)

int main(void)
{
    const char *encoded = xargon2_hash(LIT("password"), LIT("somesalt12345678"), LIT(""), LIT(""), 2, 64, 1, 32, 16, 2);
    if (!encoded[0] || xrtGetError()) return 1;
    printf("verified: %s\n", xargon2_verify(LIT("password"), encoded, strlen(encoded), LIT(""), LIT("")) ? "true" : "false");
    printf("wrong password: %s\n", xargon2_verify(LIT("wrong"), encoded, strlen(encoded), LIT(""), LIT("")) ? "true" : "false");
    const char *contextual = xargon2_hash(LIT("password"), LIT("somesalt12345678"), LIT("server-key"), LIT("tenant-42"), 2, 64, 1, 32, 16, 2);
    if (!contextual[0] || xrtGetError()) return 1;
    printf("context verified: %s\n", xargon2_verify(LIT("password"), contextual, strlen(contextual), LIT("server-key"), LIT("tenant-42")) ? "true" : "false");
    printf("wrong context: %s\n", xargon2_verify(LIT("password"), contextual, strlen(contextual), LIT("server-key"), LIT("other")) ? "true" : "false");
    printf("needs rehash: %s\n", xargon2_needs_rehash(encoded, strlen(encoded), 3, 65536, 4) ? "true" : "false");
    printf("salt hex length: %zu\n", strlen(xargon2_generate_salt(16)));
    xrtTempUnit(xrtTempCurrent());
    return xrtGetError() ? 1 : 0;
}
