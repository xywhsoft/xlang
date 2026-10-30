/* 与 framed_records.xl 同一条标准线格式；C 和 XLang 不另实现协议。 */
#ifdef __TINYC__
#include "../third_party/xrt_snappy.c"
#include "../xrt_snappy.c"
#else
#include "xrt_snappy.h"
#endif
#include <stdio.h>
#include <string.h>

int main(void) {
    static const unsigned char wire[] = {
        255,6,0,0,'s','N','a','P','p','Y',
        1,7,0,0,110,87,241,33,'a','b','c',
        1,4,0,0,216,234,130,162,
        0,5,0,0,216,234,130,162,0,
        255,6,0,0,'s','N','a','P','p','Y',
        1,7,0,0,110,87,241,33,'a','b','c'
    };
    xrtsnappydecompressor *decoder = xrtSnappyDecompressorCreate(NULL);
    xbuffer *plain = xrtBufferCreate(), *part = NULL;
    int result = 1;
    if (!decoder || !plain) goto done;
    /* 每次排空返回独立 Buffer，由调用方释放。空块不意味着 EOF。 */
    for (size_t i = 0; i < sizeof(wire); ++i) {
        if (!xrtSnappyDecompressorUpdate(decoder, wire + i, 1)) goto done;
        part = xrtSnappyDecompressorDrainBuffer(decoder);
        if (!part || !xrtBufferAppend(plain, xrtBufferView(part))) goto done;
        xrtBufferDestroy(part); part = NULL;
    }
    if (!xrtSnappyDecompressorFinish(decoder) || !xrtSnappyDecompressorFinished(decoder) ||
        xrtSnappyDecompressorMembers(decoder) != 2 ||
        xrtSnappyDecompressorInputBytes(decoder) != sizeof(wire) ||
        xrtSnappyDecompressorOutputBytes(decoder) != 6 || plain->Size != 6 ||
        memcmp(plain->Data, "abcabc", 6)) goto done;
    printf("text: %.*s\nmembers: 2\nbytes: 6\n", (int)plain->Size, (const char *)plain->Data);
    result = ferror(stdout) ? 1 : 0;
done:
    xrtBufferDestroy(part); xrtBufferDestroy(plain); xrtSnappyDecompressorDestroy(decoder);
    return result;
}
