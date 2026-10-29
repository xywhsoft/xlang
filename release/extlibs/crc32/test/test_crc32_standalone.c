/* 普通 C 应用：公开头选择必要的 XRT 模块，不依赖 xlang 或测试运行时。 */
#define XRT_IMPLEMENTATION
#include "../include/xrt_crc32.h"
#include <assert.h>

int main(void)
{
    uint32 crc = 42;
    xrtcrc32hasher* h;
    xreader* reader;
    assert(xrtCrc32Hash("123456789", 9, &crc, NULL) && crc == 0xCBF43926u);
    h = xrtCrc32HasherCreate(NULL);
    assert(h && xrtCrc32HasherUpdate(h, "abc", 3));
    assert(xrtCrc32HasherDigest(h, &crc) && crc == 0x352441C2u);
    xrtCrc32HasherDestroy(h);
    reader = xrtReaderFromMemory((xbytesview){(const uint8*)"abc", 3});
    assert(reader && xrtCrc32File(reader, NULL, &crc, NULL, NULL, NULL));
    assert(crc == 0x352441C2u && xrtReaderDestroy(reader));
    return 0;
}
