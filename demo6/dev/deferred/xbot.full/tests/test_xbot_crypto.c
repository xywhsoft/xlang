/* xbot crypto smoke: the native helper delegates to XRT primitives. */
#define XRT_MODULE_ALL
#include "xrt.h"

#include <stdio.h>
#include <string.h>

const char* xxbot_sha256_hex(const char* sData);
const char* xxbot_hmac_sha256_hex(const char* sKey, const char* sData);

int main(void)
{
    if (strcmp(xxbot_sha256_hex("abc"),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") != 0 ||
        strcmp(xxbot_hmac_sha256_hex("key", "The quick brown fox jumps over the lazy dog"),
            "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8") != 0) {
        fputs("xbot crypto smoke failed\n", stderr);
        return 1;
    }
    puts("xbot crypto smoke: PASS");
    return 0;
}

#include "../native/xbot_crypto.c"
