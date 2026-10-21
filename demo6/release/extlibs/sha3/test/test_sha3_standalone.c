/* 公开 C 核心只依赖一份 XRT，不依赖 xlang/xruntime。 */
#define XRT_IMPLEMENTATION
#include "../include/xrt_sha3.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    uint8 digest[32], expected[32];
    xreader* reader = xrtReaderFromMemory((xbytesview){(const uint8*)"abc", 3});
    xrtsha3hasher *h = xrtSha3HasherCreate(NULL, 256), *copy;
    assert(h && reader && xrtSha3Hash("abc", 3, 256, expected, 32, NULL));
    assert(xrtSha3HasherUpdate(h, "a", 1));
    copy = xrtSha3HasherClone(h);
    assert(copy && xrtSha3HasherUpdate(copy, "bc", 2));
    assert(xrtSha3HasherDigest(copy, digest, 32) && !memcmp(digest, expected, 32));
    assert(xrtSha3HasherInputBytes(h) == 1);
    xrtSha3HasherDestroy(copy); xrtSha3HasherDestroy(h);
    assert(xrtSha3File(reader, NULL, 256, digest, 32, NULL, NULL, NULL));
    assert(!memcmp(digest, expected, 32) && xrtReaderDestroy(reader));
    return 0;
}
