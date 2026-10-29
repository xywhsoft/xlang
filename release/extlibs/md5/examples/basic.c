#include "../include/xrt_md5.h"
#include <assert.h>
#include <stdio.h>

/* 只把摘要转换为展示文本；算法和资源管理全部使用 XRT。 */
static void show(const char* label, const uint8 digest[16])
{
    unsigned i;
    printf("%s=", label);
    for (i = 0; i < 16; ++i) printf("%02x", digest[i]);
    puts("");
}
int main(void)
{
    uint8 digest[16];
    xrtmd5hasher* h;
    assert(xrtMd5Hash(NULL, 0, digest, NULL));
    show("empty", digest);
    assert(xrtMd5Hash("abc", 3, digest, NULL));
    show("abc", digest);
    h = xrtMd5HasherCreate(NULL);
    assert(h && xrtMd5HasherUpdate(h, "a", 1) && xrtMd5HasherUpdate(h, "bc", 2));
    assert(xrtMd5HasherDigest(h, digest));
    show("stream", digest);
    printf("bytes=%llu\n", (unsigned long long)xrtMd5HasherInputBytes(h));
    assert(xrtMd5HasherReset(h) && xrtMd5HasherDigest(h, digest));
    show("reset", digest);
    xrtMd5HasherDestroy(h);
    return 0;
}
