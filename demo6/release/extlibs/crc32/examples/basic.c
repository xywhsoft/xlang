#include "../include/xrt_crc32.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    uint32 crc;
    xrtcrc32hasher* h;
    /* C 与 xlang 使用相同核心；输出可逐行对照。 */
    assert(xrtCrc32Hash(NULL, 0, &crc, NULL));
    printf("empty=%08x\n", (unsigned int)crc);
    assert(xrtCrc32Hash("123456789", 9, &crc, NULL));
    printf("check=%u\nhex=%08x\n", (unsigned int)crc, (unsigned int)crc);
    h = xrtCrc32HasherCreate(NULL);
    assert(h && xrtCrc32HasherUpdate(h, "1234", 4));
    assert(xrtCrc32HasherUpdate(h, "56789", 5) && xrtCrc32HasherDigest(h, &crc));
    printf("stream=%08x\nbytes=%llu\n", (unsigned int)crc, (unsigned long long)xrtCrc32HasherInputBytes(h));
    assert(xrtCrc32HasherReset(h) && xrtCrc32HasherDigest(h, &crc));
    printf("reset=%08x\n", (unsigned int)crc);
    xrtCrc32HasherDestroy(h);
    return 0;
}
