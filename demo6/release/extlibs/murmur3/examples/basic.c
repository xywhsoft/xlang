#include "../include/xrt_murmur3.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* 与 murmur3_basic.xl 相同输入、seed 和输出；C 直接调用公共核心。 */
static void show(const char* label, const uint8 digest[16])
{
    unsigned i;
    printf("%s=", label);
    for (i = 0; i < 16; ++i) printf("%02x", digest[i]);
    puts("");
}
int main(void)
{
    const char* text = "你好🌍尾";
    uint8 digest[16], data[256];
    uint32 narrow;
    unsigned i;
    xrtmurmur3options options;
    xrtmurmur3hasher *h, *branch;
    printf("empty32=%u\n", xrtMurmur3Hash32(NULL, 0, 0, NULL));
    narrow = xrtMurmur3Hash32("abc", 3, 0, NULL);
    printf("abc32=%u\nabc32hex=", narrow);
    for (i = 0; i < 4; ++i) printf("%02x", (unsigned)((narrow >> (i * 8)) & 255));
    puts("");
    assert(xrtMurmur3Hash128("abc", 3, 42, digest, NULL));
    show("abc128", digest);
    assert(xrtMurmur3Hash128(text, strlen(text), 0, digest, NULL));
    show("utf8", digest);
    for (i = 0; i < sizeof(data); ++i) data[i] = (uint8)(i * 37 + 11);
    printf("binary32=%u\n", xrtMurmur3Hash32(data, sizeof(data), 42, NULL));
    assert(xrtMurmur3Hash128(data, sizeof(data), 42, digest, NULL));
    show("binary128", digest);
    xrtMurmur3OptionsInit(&options);
    options.tLimits.iMaxInputBytes = 1024;
    h = xrtMurmur3HasherCreate(&options, 42);
    assert(h && xrtMurmur3HasherUpdate(h, "a", 1));
    branch = xrtMurmur3HasherClone(h);
    assert(branch && xrtMurmur3HasherUpdate(h, "bc", 2));
    assert(xrtMurmur3HasherReset(branch) && xrtMurmur3HasherUpdate(branch, "foo", 3));
    assert(xrtMurmur3HasherDigest(h, digest));
    show("stream", digest);
    assert(xrtMurmur3HasherDigest(branch, digest));
    show("clone", digest);
    printf("bytes=%llu\n", (unsigned long long)xrtMurmur3HasherInputBytes(h));
    assert(xrtMurmur3HasherReset(h) && xrtMurmur3HasherDigest(h, digest));
    show("reset", digest);
    xrtMurmur3HasherDestroy(branch);
    xrtMurmur3HasherDestroy(h);
    return 0;
}
