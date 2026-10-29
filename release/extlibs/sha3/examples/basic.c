#include "../include/xrt_sha3.h"
#include <stdio.h>

static void show(const char* label, const uint8* bytes, size_t size)
{
    size_t i;
    printf("%s=", label);
    for (i = 0; i < size; ++i) printf("%02x", bytes[i]);
    putchar('\n');
}
int main(void)
{
    const int widths[] = {224, 256, 384, 512};
    const uint8 binary[] = {0xe4,0xbd,0xa0,0xe5,0xa5,0xbd,0,0xe4,0xb8,0x96,0xe7,0x95,0x8c};
    uint8 output[257];
    char label[32];
    size_t i;
    xrtsha3hasher *h, *branch;
    for (i = 0; i < 4; ++i) {
        size_t length = xrtSha3DigestSize(widths[i]);
        if (!xrtSha3Hash("abc", 3, widths[i], output, length, NULL)) return 1;
        snprintf(label, sizeof(label), "sha3-%d", widths[i]); show(label, output, length);
    }
    if (!xrtShakeHash("abc", 3, 128, output, 32, NULL)) return 1;
    show("shake128", output, 32);
    if (!xrtShakeHash("abc", 3, 256, output, 64, NULL)) return 1;
    show("shake256", output, 64);
    if (!xrtSha3Hash(binary, sizeof(binary), 256, output, 32, NULL)) return 1;
    show("binary", output, 32);
    h = xrtSha3HasherCreate(NULL, 256);
    if (h == NULL || !xrtSha3HasherUpdate(h, "a", 1)) return 1;
    branch = xrtSha3HasherClone(h);
    if (branch == NULL || !xrtSha3HasherUpdate(h, "bc", 2) ||
        !xrtSha3HasherUpdate(branch, "x", 1)) return 1;
    if (!xrtSha3HasherDigest(h, output, 32)) return 1;
    show("stream", output, 32);
    if (!xrtSha3HasherDigest(branch, output, 32)) return 1;
    show("branch", output, 32);
    printf("input=%llu\n", (unsigned long long)xrtSha3HasherInputBytes(h));
    if (!xrtSha3HasherReset(h) || xrtSha3HasherInputBytes(h) != 0) return 1;
    puts("reset=true");
    xrtSha3HasherDestroy(branch); xrtSha3HasherDestroy(h);
    h = xrtSha3ShakeHasherCreate(NULL, 128);
    if (h == NULL || !xrtSha3HasherUpdate(h, "abc", 3)) return 1;
    if (!xrtSha3HasherDigest(h, NULL, 0)) return 1;
    puts("empty-xof=0");
    if (!xrtSha3HasherDigest(h, output, 257)) return 1;
    puts("long-xof=257");
    xrtSha3HasherDestroy(h);
    return 0;
}
