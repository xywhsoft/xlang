/* 与 streaming.xl 等价；Writer 由调用方持有，drain 不接管或关闭它。 */
#include "../include/xrt_miniz.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    xrtminizcompressor* encoder = xrtMinizCompressorCreate(NULL);
    xrtminizdecompressor* decoder = NULL;
    xbuffer* packed = xrtBufferCreate();
    xbuffer* plain = xrtBufferCreate();
    xwriter* packed_writer = packed ? xrtWriterFromBuffer(packed) : NULL;
    xwriter* plain_writer = plain ? xrtWriterFromBuffer(plain) : NULL;
    int result = 1;
    if (!encoder || !packed_writer || !plain_writer) goto done;
    if (!xrtMinizCompressorUpdate(encoder, "hello ", 6) ||
        !xrtMinizCompressorDrain(encoder, packed_writer) ||
        !xrtMinizCompressorUpdate(encoder, "stream", 6) ||
        !xrtMinizCompressorDrain(encoder, packed_writer) ||
        !xrtMinizCompressorFinish(encoder) ||
        !xrtMinizCompressorDrain(encoder, packed_writer)) goto done;
    if (xrtMinizCompressorInputBytes(encoder) != 12 ||
        xrtMinizCompressorOutputBytes(encoder) != packed->Size) goto done;

    decoder = xrtMinizDecompressorCreate(NULL);
    if (!decoder) goto done;
    size_t split = packed->Size / 2;
    if (!xrtMinizDecompressorUpdate(decoder, packed->Data, split) ||
        !xrtMinizDecompressorDrain(decoder, plain_writer) ||
        !xrtMinizDecompressorUpdate(decoder, packed->Data + split, packed->Size - split) ||
        !xrtMinizDecompressorDrain(decoder, plain_writer) ||
        !xrtMinizDecompressorFinish(decoder) ||
        !xrtMinizDecompressorDrain(decoder, plain_writer)) goto done;
    if (!xrtMinizDecompressorFinished(decoder) || xrtMinizDecompressorInputBytes(decoder) != packed->Size ||
        xrtMinizDecompressorOutputBytes(decoder) != 12 || plain->Size != 12 ||
        memcmp(plain->Data, "hello stream", 12) != 0) goto done;
    printf("%.*s\n%llu / %llu\n", (int)plain->Size, (const char*)plain->Data,
        (unsigned long long)xrtMinizCompressorInputBytes(encoder),
        (unsigned long long)xrtMinizDecompressorOutputBytes(decoder));
    result = 0;
done:
    if (!xrtWriterDestroy(packed_writer)) result = 1;
    if (!xrtWriterDestroy(plain_writer)) result = 1;
    xrtBufferDestroy(plain);
    xrtBufferDestroy(packed);
    xrtMinizDecompressorDestroy(decoder);
    xrtMinizCompressorDestroy(encoder);
    return result;
}

#ifdef __TINYC__
#include "../xrt_miniz.c"
#endif
