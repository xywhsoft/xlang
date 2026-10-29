/* 仅一份 XRT 实现，公开核心不依赖 xlang 或 xruntime。 */
#define XRT_IMPLEMENTATION
#include "../include/xrt_murmur3.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    const uint8 expected[16] = {0xd6,0xf7,0xcf,0xb3,0x9f,0x08,0x85,0x0d,0x30,0x3d,0x35,0x42,0x2b,0x71,0x10,0x75};
    uint8 digest[16];
    xreader* reader = xrtReaderFromMemory((xbytesview){(const uint8*)"abc", 3});
    xrtmurmur3hasher* h = xrtMurmur3HasherCreate(NULL, 42);
    xrtmurmur3hasher* copy;
    assert(h && reader && xrtMurmur3Hash32("abc", 3, 42, NULL) == UINT32_C(1313807976));
    assert(xrtMurmur3Hash128("abc", 3, 42, digest, NULL) && memcmp(digest, expected, 16) == 0);
    assert(xrtMurmur3HasherUpdate(h, "a", 1));
    copy = xrtMurmur3HasherClone(h);
    assert(copy && xrtMurmur3HasherUpdate(copy, "bc", 2));
    assert(xrtMurmur3HasherDigest(copy, digest) && memcmp(digest, expected, 16) == 0);
    assert(xrtMurmur3HasherInputBytes(h) == 1);
    xrtMurmur3HasherDestroy(copy);
    xrtMurmur3HasherDestroy(h);
    assert(xrtMurmur3File128(reader, NULL, 42, digest, NULL, NULL, NULL));
    assert(memcmp(digest, expected, 16) == 0 && xrtReaderDestroy(reader));
    return 0;
}
