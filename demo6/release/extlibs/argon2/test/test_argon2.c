#include <stdio.h>
#include <string.h>

extern const char* xargon2_hash(
    const char* password, const char* salt, const char* secret, const char* ad,
    long long timeCost, long long memoryCost, long long parallelism,
    long long hashLength, long long saltLength, long long type
);
extern long long xargon2_verify(const char* password, const char* encoded);

int main(void)
{
    const char* encoded = xargon2_hash(
        "xlang-password", "0123456789abcdef", "", "",
        2, 64, 1, 32, 16, 2
    );

    if ( encoded == NULL || strncmp(encoded, "$argon2id$", 10u) != 0 ||
         !xargon2_verify("xlang-password", encoded) ||
         xargon2_verify("wrong-password", encoded) ) {
        fputs("test_argon2: hash or verify failed\n", stderr);
        return 1;
    }
    if ( xargon2_hash("x", "0123456789abcdef", "", "",
                      1, 2097153, 1, 32, 16, 2)[0] != '\0' ) {
        fputs("test_argon2: excessive memory cost was accepted\n", stderr);
        return 1;
    }
    puts("test_argon2: ok");
    return 0;
}
