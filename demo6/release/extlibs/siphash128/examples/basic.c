#include "../include/xrt_siphash.h"
#include <assert.h>
#include <stdio.h>

static void show(const char* label, const uint8* bytes, size_t size)
{
    size_t i;
    printf("%s=", label);
    for (i = 0; i < size; ++i) printf("%02x", bytes[i]);
    putchar('\n');
}
static void show64(const char* label, uint64 value)
{
    uint8 bytes[8];
    size_t i;
    for (i = 0; i < 8; ++i) bytes[i] = (uint8)(value >> (i * 8));
    show(label, bytes, sizeof(bytes));
}
int main(void)
{
    // 固定密钥仅用于复现；真实的带密钥哈希需要保密的随机密钥。
    const uint8 key[] = "0123456789abcdef";
    const uint8 utf8Key[] = "你好世界啊a";
    const uint8 binary[] = {0xe4,0xbd,0xa0,0xe5,0xa5,0xbd,0,0xe4,0xb8,0x96,0xe7,0x95,0x8c};
    uint8 digest[16];
    uint64 value = xrtSipHash64("abc", 3, key, NULL);
    xrtsiphasher *h, *branch;
    assert(!xrtGetError());
    printf("number=%llu\n", (unsigned long long)value);
    show64("tag64", value);
    assert(xrtSipHash128("abc", 3, key, digest, NULL)); show("tag128", digest, 16);
    assert(xrtSipHash128(binary, sizeof(binary), key, digest, NULL)); show("binary", digest, 16);
    show64("utf8-key", xrtSipHash64("abc", 3, utf8Key, NULL));

    // 摘要为可重复快照；clone 创建独立分支，reset 保留密钥、清空数据。
    h = xrtSipHasherCreate(NULL, key); assert(h && xrtSipHasherUpdate(h, "a", 1));
    branch = xrtSipHasherClone(h); assert(branch);
    assert(xrtSipHasherUpdate(h, "bc", 2) && xrtSipHasherUpdate(branch, "x", 1));
    show64("stream", xrtSipHasherDigest64(h));
    show64("branch", xrtSipHasherDigest64(branch));
    printf("input=%llu\n", (unsigned long long)xrtSipHasherInputBytes(h));
    assert(xrtSipHasherReset(h) && xrtSipHasherInputBytes(h) == 0); puts("reset=true");
    show64("empty", xrtSipHasherDigest64(h));
    xrtSipHasherDestroy(branch); xrtSipHasherDestroy(h);
    return 0;
}
