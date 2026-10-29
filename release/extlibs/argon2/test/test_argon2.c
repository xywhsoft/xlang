#include <stdio.h>
#include <string.h>
/* 哈希与校验操作位于断言中，发布配置也必须执行。 */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include "../include/argon2_native.h"
#include "../../../include/xrt_decl.h"

#define LIT(text) (text), (sizeof(text) - 1u)

static void expect_error(xerrkind kind)
{
    assert(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == kind);
    xrtClearError();
}

int main(void)
{
    /* RFC 9106 第 5 节：三种算法共享输入，分别校验独立公布的 32 字节 Tag。 */
    {
        char password[32], salt[16], secret[8], ad[12];
        static const struct {
            long long type;
            const char* tag;
        } vectors[] = {
            { 0, "512b391b6f1162975371d30919734294f868e3be3984f3c1a13a4db9fabe4acb" },
            { 1, "c814d9d1dc7f37aa13f0d77f2494bda1c8de6b016dd388d29952a4c4672b6ce8" },
            { 2, "0d640df58d78766c08c037a34a8b53c9d01ef0452d75b65eb52520e96b01e659" }
        };
        size_t i;
        memset(password, 1, sizeof(password));
        memset(salt, 2, sizeof(salt));
        memset(secret, 3, sizeof(secret));
        memset(ad, 4, sizeof(ad));
        for (i = 0; i < sizeof(vectors) / sizeof(vectors[0]); ++i) {
            char* actual = xargon2_hash_raw(password, sizeof(password),
                salt, sizeof(salt), secret, sizeof(secret), ad, sizeof(ad),
                3, 32, 4, 32, 16, vectors[i].type);
            if (actual == NULL || strcmp(actual, vectors[i].tag) != 0) {
                fprintf(stderr, "test_argon2: RFC 9106 vector %lld failed: %s\n",
                    vectors[i].type, actual != NULL ? actual : "(null)");
                xrtFree(actual);
                return 1;
            }
            xrtFree(actual);
        }
    }
    char* encoded = xargon2_hash(
        LIT("xlang-password"), LIT("0123456789abcdef"), LIT(""), LIT(""),
        2, 64, 1, 32, 16, 2
    );

    if ( encoded == NULL || strncmp(encoded, "$argon2id$", 10u) != 0 ||
         !xargon2_verify(LIT("xlang-password"), encoded, strlen(encoded), LIT(""), LIT("")) ||
         xargon2_verify(LIT("wrong-password"), encoded, strlen(encoded), LIT(""), LIT("")) ) {
        fputs("test_argon2: hash or verify failed\n", stderr);
        return 1;
    }
    /* 每次公开操作只留下本次错误；正常不匹配不能继承旧故障。 */
    {
        char *value;
        xrtSetErrorInfo(XERR_IO, "test", 0, "old error");
        value = xargon2_hash(LIT("pw"), LIT("0123456789abcdef"), LIT(""), LIT(""),
            1, 64, 1, 32, 16, 2);
        assert(value && value[0] != '\0' && xrtGetError() == NULL);
        xrtFree(value);

        xrtSetErrorInfo(XERR_IO, "test", 0, "old error");
        value = xargon2_hash_raw(LIT("pw"), LIT("0123456789abcdef"), LIT(""), LIT(""),
            1, 64, 1, 32, 16, 2);
        assert(value && value[0] != '\0' && xrtGetError() == NULL);
        xrtFree(value);

        xrtSetErrorInfo(XERR_IO, "test", 0, "old error");
        assert(!xargon2_verify(LIT("wrong"), encoded, strlen(encoded), LIT(""), LIT("")));
        assert(xrtGetError() == NULL);

        xrtSetErrorInfo(XERR_IO, "test", 0, "old error");
        assert(xargon2_needs_rehash(encoded, strlen(encoded), 1, 64, 1));
        assert(xrtGetError() == NULL);

        xrtSetErrorInfo(XERR_IO, "test", 0, "old error");
        value = xargon2_extract(encoded, strlen(encoded));
        assert(value && value[0] != '\0' && xrtGetError() == NULL);
        xrtFree(value);

        xrtSetErrorInfo(XERR_IO, "test", 0, "old error");
        value = xargon2_generate_salt(16);
        assert(value && strlen(value) == 32u && xrtGetError() == NULL);
        xrtFree(value);

        xrtSetErrorInfo(XERR_IO, "test", 0, "old error");
        assert(!xargon2_verify(LIT("pw"), LIT("invalid"), LIT(""), LIT("")));
        assert(xrtGetError() == NULL);

        xrtSetErrorInfo(XERR_IO, "test", 0, "old error");
        assert(xargon2_needs_rehash(LIT("invalid"), 3, 65536, 4));
        assert(xrtGetError() == NULL);

        xrtSetErrorInfo(XERR_IO, "test", 0, "old error");
        value = xargon2_extract(LIT("invalid"));
        assert(value && value[0] == '\0' && xrtGetError() == NULL);
        xrtFree(value);
    }
    {
        char *withNul = xargon2_hash(LIT("pw\0tail"), LIT("0123456789abcdef"),
            LIT(""), LIT(""), 1, 64, 1, 32, 16, 2);
        char *withoutNul = xargon2_hash(LIT("pw"), LIT("0123456789abcdef"),
            LIT(""), LIT(""), 1, 64, 1, 32, 16, 2);
        assert(withNul && withoutNul);
        if ( strcmp(withNul, withoutNul) == 0 ) {
            fputs("test_argon2: password was truncated at NUL\n", stderr);
            return 1;
        }
        assert(xargon2_verify(LIT("pw\0tail"), withNul, strlen(withNul), LIT(""), LIT("")));
        assert(!xargon2_verify(LIT("pw"), withNul, strlen(withNul), LIT(""), LIT("")));
        xrtFree(withNul);
        xrtFree(withoutNul);
    }
    {
        char malformed[512];
        size_t fullLength;
        char *fullSalt = xargon2_hash(LIT("pw"), LIT("salt1234\0tail"),
            LIT("key\0tail"), LIT("tenant\0tail"), 1, 64, 1, 32, 16, 2);
        char *shortSalt = xargon2_hash(LIT("pw"), LIT("salt1234"),
            LIT("key\0tail"), LIT("tenant\0tail"), 1, 64, 1, 32, 16, 2);
        char *rawFull = xargon2_hash_raw(LIT("pw"), LIT("salt1234\0tail"),
            LIT("key\0tail"), LIT("tenant\0tail"), 1, 64, 1, 32, 16, 2);
        char *rawShort = xargon2_hash_raw(LIT("pw"), LIT("salt1234"),
            LIT("key\0tail"), LIT("tenant\0tail"), 1, 64, 1, 32, 16, 2);
        assert(fullSalt && shortSalt && rawFull && rawShort);
        assert(strcmp(fullSalt, shortSalt) != 0);
        assert(strcmp(rawFull, rawShort) != 0);
        assert(xargon2_verify(LIT("pw"), fullSalt, strlen(fullSalt),
            LIT("key\0tail"), LIT("tenant\0tail")));
        assert(!xargon2_verify(LIT("pw"), fullSalt, strlen(fullSalt),
            LIT("key"), LIT("tenant\0tail")));
        assert(!xargon2_verify(LIT("pw"), fullSalt, strlen(fullSalt),
            LIT("key\0tail"), LIT("tenant")));
        fullLength = strlen(fullSalt);
        assert(fullLength + 5u < sizeof(malformed));
        memcpy(malformed, fullSalt, fullLength);
        memcpy(malformed + fullLength, "\0tail", 5u);
        assert(!xargon2_verify(LIT("pw"), malformed, fullLength + 5u,
            LIT("key\0tail"), LIT("tenant\0tail")));
        assert(xrtGetError() == NULL);
        xrtFree(fullSalt);
        xrtFree(shortSalt);
        xrtFree(rawFull);
        xrtFree(rawShort);
    }
    if ( xargon2_hash(LIT("x"), LIT("0123456789abcdef"), LIT(""), LIT(""),
                      1, 2097153, 1, 32, 16, 2) != NULL ) {
        fputs("test_argon2: excessive memory cost was accepted\n", stderr);
        return 1;
    }
    expect_error(XERR_VALUE);

    /* 错误密码/损坏 PHC 是普通不匹配；资源上限和分配失败不是不匹配。 */
    {
        char *contextual = xargon2_hash(
            LIT("pw"), LIT("0123456789abcdef"), LIT("server-key"), LIT("tenant-42"),
            1, 64, 1, 32, 16, 2
        );
        assert(contextual && contextual[0] != '\0');
        assert(xargon2_verify(LIT("pw"), contextual, strlen(contextual), LIT("server-key"), LIT("tenant-42")));
        assert(!xargon2_verify(LIT("pw"), contextual, strlen(contextual), LIT(""), LIT("")));
        assert(!xargon2_verify(LIT("pw"), contextual, strlen(contextual), LIT("wrong-key"), LIT("tenant-42")));
        assert(!xargon2_verify(LIT("pw"), contextual, strlen(contextual), LIT("server-key"), LIT("wrong-tenant")));
        assert(xrtGetError() == NULL);
        xrtFree(contextual);
    }
    assert(!xargon2_verify(LIT("pw"), LIT("invalid"), LIT(""), LIT("")));
    assert(xrtGetError() == NULL);
    assert(!xargon2_verify(LIT("pw"), LIT("$argon2id$v=19$m=2097153,t=1,p=1$c29tZXNhbHQ$AAAAAA"), LIT(""), LIT("")));
    expect_error(XERR_VALUE);
    assert(xargon2_generate_salt(7) == NULL);
    expect_error(XERR_VALUE);
    assert(xargon2_needs_rehash(encoded, strlen(encoded), 0, 64, 1));
    expect_error(XERR_VALUE);
    assert(xargon2_hash(LIT("x"), LIT("short"), LIT(""), LIT(""), 1, 64, 1, 32, 16, 2) == NULL);
    expect_error(XERR_VALUE);
    assert(xargon2_hash_raw(LIT("x"), LIT("somesalt"), LIT(""), LIT(""), 1, 64, 1, 3, 16, 2) == NULL);
    expect_error(XERR_VALUE);
    assert(xargon2_hash(LIT("x"), LIT("somesalt"), LIT(""), LIT(""), 1, 64, 1, 32, 16, 3) == NULL);
    expect_error(XERR_VALUE);
    /* 结果不再借用临时区，清理后原 PHC 仍可验证。 */
    xrtTempUnit(xrtTempCurrent());
    assert(xargon2_verify(LIT("xlang-password"), encoded, strlen(encoded), LIT(""), LIT("")));
    xrtFree(encoded);
    puts("test_argon2: ok");
    return 0;
}
