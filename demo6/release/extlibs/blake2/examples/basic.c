#ifndef XRT_MODULE_CODEC_HEX
#define XRT_MODULE_CODEC_HEX
#endif
#include "../include/xrt_blake2.h"
#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
/* xl run 单文件范例；正式包仍分别编译核心与上游源文件。 */
#include "../third_party/blake2.c"
#include "../xrt_blake2.c"
#endif

static bool print_hex(const char* label, const uint8* bytes, size_t size)
{
    char text[129];
    size_t written;
    if (!xrtHexEncode(bytes, size, text, sizeof(text), &written, 0) ||
        written != size * 2) return false;
    printf("%s%s\n", label, text);
    return true;
}

int main(void)
{
    uint8 digest[16], expected[16];
    xrtblake2bhasher* hash = xrtBlake2bHasherCreate(NULL, 16, NULL, 0);
    xrtblake2bhasher* branch = NULL;
    int status = 1;
    /* 十六进制明确固定 UTF-8 字节，与对应 xlang 范例使用相同输入。 */
    const char* text = "\xe4\xbd\xa0\xe5\xa5\xbd xlang";
    const char* key = "\xe5\xaf\x86\xe9\x92\xa5";
    if (hash == NULL || !xrtBlake2bHasherUpdate(hash, "ab", 2)) goto done;
    branch = xrtBlake2bHasherClone(hash);
    if (branch == NULL || !xrtBlake2bHasherUpdate(hash, "c", 1) ||
        !xrtBlake2bHasherUpdate(branch, "d", 1) ||
        !xrtBlake2bHasherDigest(hash, digest, sizeof(digest)) ||
        !xrtBlake2bHash("abc", 3, 16, NULL, 0, expected, NULL) ||
        memcmp(digest, expected, sizeof(digest)) != 0 ||
        !print_hex("b2b/16: ", digest, sizeof(digest))) goto done;

    if (!xrtBlake2bHasherDigest(branch, digest, sizeof(digest)) ||
        !print_hex("clone: ", digest, sizeof(digest))) goto done;

    if (!xrtBlake2bHasherReset(hash) ||
        !xrtBlake2bHasherUpdate(hash, "abc", 3) ||
        !xrtBlake2bHasherDigest(hash, digest, sizeof(digest)) ||
        !print_hex("reset: ", digest, sizeof(digest))) goto done;

    if (!xrtBlake2sHash("abc", 3, 16, NULL, 0, digest, NULL) ||
        !print_hex("b2s/16: ", digest, sizeof(digest))) goto done;
    if (!xrtBlake2bHash(text, strlen(text), 16, key, strlen(key), digest, NULL) ||
        !print_hex("keyed: ", digest, sizeof(digest))) goto done;
    printf("bytes: %llu\n", (unsigned long long)xrtBlake2bHasherInputBytes(hash));
    status = 0;
done:
    xrtBlake2bHasherDestroy(branch);
    xrtBlake2bHasherDestroy(hash);
    return status;
}
