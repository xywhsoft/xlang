/* 公开头选择必要 XRT 模块，不链接 xlang 或 xruntime。 */
#define XRT_IMPLEMENTATION
#include "../include/xrt_md5.h"
#include <assert.h>
#include <string.h>
int main(void)
{
    const uint8 expected[16] = {0x90,0x01,0x50,0x98,0x3c,0xd2,0x4f,0xb0,0xd6,0x96,0x3f,0x7d,0x28,0xe1,0x7f,0x72};
    uint8 digest[16];
    xreader* reader = xrtReaderFromMemory((xbytesview){(const uint8*)"abc", 3});
    xrtmd5hasher* h = xrtMd5HasherCreate(NULL);
    assert(h && reader && xrtMd5Hash("abc", 3, digest, NULL));
    assert(memcmp(digest, expected, 16) == 0);
    assert(xrtMd5HasherUpdate(h, "abc", 3) && xrtMd5HasherDigest(h, digest));
    assert(memcmp(digest, expected, 16) == 0);
    xrtMd5HasherDestroy(h);
    assert(xrtMd5File(reader, NULL, digest, NULL, NULL, NULL));
    assert(memcmp(digest, expected, 16) == 0 && xrtReaderDestroy(reader));
    return 0;
}
