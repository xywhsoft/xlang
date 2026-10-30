/* 与 XLang 相同的二进制分段输入；流对象和 drain 缓冲分别拥有。 */
#include "../include/xrt_zstd.h"
#include <stdio.h>
#include <string.h>
#ifdef __TINYC__
#include "../third_party/zstd.c"
#include "../xrt_zstd.c"
#endif

int main(void)
{
    const unsigned char source[] = {'o','n','e',0,'t','w','o'};
    xrtzstdcompressor* encoder = xrtZstdCompressorCreate(NULL);
    xrtzstddecompressor* decoder = NULL;
    xbuffer *packed = NULL, *tail = NULL, *plain = NULL, *retry = NULL, *recovered = NULL;
    xrtzstdoptions options;
    xrtzstderror error;
    int ok = 0;
    if (!encoder || !xrtZstdCompressorUpdate(encoder, source, 4)) goto done;
    packed = xrtZstdCompressorDrainBuffer(encoder);
    if (!packed || !xrtZstdCompressorUpdate(encoder, source + 4, 3) ||
        !xrtZstdCompressorFinish(encoder)) goto done;
    tail = xrtZstdCompressorDrainBuffer(encoder);
    if (!tail || !xrtBufferAppend(packed, xrtBufferView(tail))) goto done;
    decoder = xrtZstdDecompressorCreate(NULL);
    if (!decoder || !xrtZstdDecompressorUpdate(decoder, packed->Data, packed->Size) ||
        !xrtZstdDecompressorFinish(decoder) || !xrtZstdDecompressorFinished(decoder) ||
        xrtZstdDecompressorMembers(decoder) != 1) goto done;
    plain = xrtZstdDecompressorDrainBuffer(decoder);
    if (!plain || plain->Size != sizeof(source) || memcmp(plain->Data, source, sizeof(source))) goto done;
    xrtZstdOptionsInit(&options);
    options.tLimits.iMaxOutputBytes = 1;
    if (xrtZstdCompress(plain->Data, plain->Size, &options, &retry, &error) || retry ||
        error.iCode != XRT_ZSTD_ERROR_LIMIT) goto done;
    if (!xrtZstdCompress(plain->Data, plain->Size, NULL, &retry, &error) ||
        !xrtZstdDecompress(retry->Data, retry->Size, NULL, &recovered, &error) ||
        recovered->Size != sizeof(source) || memcmp(recovered->Data, source, sizeof(source))) goto done;
    puts("source bytes: 7\ncompleted frames: 1\nbinary round trip: true\noutput limit rejected: true\nzstd bounded stream: OK");
    ok = 1;
done:
    xrtBufferDestroy(recovered);
    xrtBufferDestroy(retry);
    xrtBufferDestroy(plain);
    xrtBufferDestroy(tail);
    xrtBufferDestroy(packed);
    xrtZstdDecompressorDestroy(decoder);
    xrtZstdCompressorDestroy(encoder);
    xrtClearError();
    return ok ? 0 : 1;
}
