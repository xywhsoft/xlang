#ifndef XRT_MODULE_CODEC_HEX
#define XRT_MODULE_CODEC_HEX
#endif
#include "../include/xrt_blake3.h"
#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
/* xl run 单文件范例；正式包仍分别编译便携后端与核心源。 */
#include "../blake3_portable.c"
#include "../xrt_blake3.c"
#endif

static bool print_hex(const char* label, const uint8* bytes, size_t size)
{
    char text[263];
    size_t written;
    if (!xrtHexEncode(bytes, size, text, sizeof(text), &written, 0) ||
        written != size * 2) return false;
    printf("%s%s\n", label, text);
    return true;
}

int main(void)
{
    const uint8 input[] = {0, 1};
    const uint8 key[] = "whats the Elvish word for friend";
    const char* context = "BLAKE3 2019-12-27 16:29:52 test vectors context";
    uint8 digest[65];
    xrtblake3hasher* h = xrtBlake3HasherCreate(NULL);
    xrtblake3hasher* branch = NULL;
    int status = 1;
    /* 使用公开向量中的二进制输入，让两种语言都能与独立答案比较。 */
    if (h == NULL || !xrtBlake3HasherUpdate(h, input, 1)) goto done;
    branch = xrtBlake3HasherClone(h);
    if (branch == NULL || !xrtBlake3HasherUpdate(h, input + 1, 1) ||
        !xrtBlake3HasherDigest(h, digest, 16) ||
        !print_hex("hash: ", digest, 16) ||
        !xrtBlake3HasherDigest(branch, digest, 16) ||
        !print_hex("clone: ", digest, 16)) goto done;
    printf("bytes: %llu\n", (unsigned long long)xrtBlake3HasherInputBytes(h));
    if (!xrtBlake3HasherReset(h) ||
        !xrtBlake3HasherDigest(h, digest, 16) ||
        !print_hex("reset: ", digest, 16) ||
        !xrtBlake3HashKeyed(input, 1, key, digest, 16, NULL) ||
        !print_hex("keyed: ", digest, 16) ||
        !xrtBlake3HashKdf(input, 1, context, strlen(context), digest, 16, NULL) ||
        !print_hex("kdf: ", digest, 16) ||
        !xrtBlake3HasherDigest(branch, digest, 65) ||
        !print_hex("xof/65: ", digest, 65)) goto done;
    status = 0;
done:
    xrtBlake3HasherDestroy(branch);
    xrtBlake3HasherDestroy(h);
    return status;
}
