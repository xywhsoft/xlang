#include "../include/jwt_native.h"
#include <stdio.h>

int main(void)
{
    const char* payload = "{\"sub\":\"user42\",\"role\":\"admin\",\"iat\":1700000000}";
    const char* secret = "super-secret-key";
    const char* token = xjwt_encode(payload, secret);
    if (token[0] == '\0') return 1;

    printf("token: %s\n", token);
    printf("payload: %s\n", xjwt_decode(token));
    printf("header: %s\n", xjwt_header(token));
    printf("verify (correct): %s\n", xjwt_verify(token, secret) ? "true" : "false");
    printf("verify (wrong):   %s\n", xjwt_verify(token, "bad-key") ? "true" : "false");
    puts("jwt example: PASS");
    return 0;
}

#ifdef __TINYC__
#include "../jwt_native.c"
#endif
