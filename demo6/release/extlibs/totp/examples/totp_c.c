#define XRT_MODULE_CRYPTO_SHA1
#define XRT_MODULE_TIME
#include "xrt.h"
#include "../include/totp_native.h"
#include <stdio.h>

int main(void)
{
    const char* secret = "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ";
    char* hotp = (char*)xtotp_hotp(secret, 32, 0, 6);
    char* totp = (char*)xtotp_generate_at(secret, 32, 59, 30, 8);
    char* live = (char*)xtotp_generate(secret, 32, 30, 6);
    int valid = hotp != NULL && totp != NULL &&
        live != NULL &&
        xtotp_verify_at(secret, 32, totp, 8, 59, 30, 8, 0) == 1 &&
        xtotp_verify(secret, 32, live, 6, 30, 6, 1) == 1;
    if (valid) {
        printf("HOTP(0): %s\n", hotp);
        printf("TOTP(59): %s\n", totp);
        puts("valid: true");
        puts("live valid: true");
    }
    xrtFree(hotp);
    xrtFree(totp);
    xrtFree(live);
    return valid ? 0 : 1;
}

/* 单文件 TCC 运行包含实现；普通 C 构建仍单独链接。 */
#ifdef __TINYC__
#include "../totp_native.c"
#endif
