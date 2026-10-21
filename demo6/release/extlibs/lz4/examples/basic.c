#include "../include/xrt_lz4.h"
#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
/* 单文件 xl run 使用包内实现；正式包仍按清单分别编译。 */
#include "../lz4_hash.c"
#include "../third_party/lz4.c"
#include "../third_party/lz4hc.c"
#include "../lz4_frame.c"
#include "../xrt_lz4.c"
#endif

int main(void)
{
    /* 与 lz4_basic.xl 使用完全相同的 UTF-8 字节及二进制输入。 */
    static const unsigned char text[] = "hello \xe4\xb8\x96\xe7\x95\x8c";
    static const unsigned char binary[] = {0, 255, 128, 65};
    xrtlz4options options;
    xrtlz4error error;
    xbuffer* packed = NULL;
    xbuffer* plain = NULL;
    xrtlz4compressor* encoder = NULL;
    xrtlz4decompressor* decoder = NULL;
    xbuffer* chunk = NULL;
    int status = 1;
    xrtLz4OptionsInit(&options);
    options.iAcceleration = 4;
    if (!xrtLz4Compress(text, sizeof(text) - 1, &options, &packed, &error) ||
        !xrtLz4Decompress(xrtBufferView(packed).Data, xrtBufferView(packed).Size,
            NULL, &plain, &error) ||
        xrtBufferView(plain).Size != sizeof(text) - 1 ||
        memcmp(xrtBufferView(plain).Data, text, sizeof(text) - 1) != 0) goto done;
    xrtBufferDestroy(plain);
    xrtBufferDestroy(packed);
    plain = packed = NULL;

    /* 一次性接口转移 Buffer 拥有权；完成后由 C 调用方各释放一次。 */
    if (!xrtLz4Compress(binary, sizeof(binary), &options, &packed, &error) ||
        !xrtLz4Decompress(xrtBufferView(packed).Data, xrtBufferView(packed).Size,
            NULL, &plain, &error) ||
        xrtBufferView(plain).Size != sizeof(binary) ||
        memcmp(xrtBufferView(plain).Data, binary, sizeof(binary)) != 0) goto done;
    xrtBufferDestroy(plain);
    xrtBufferDestroy(packed);
    plain = packed = NULL;

    /* 分块喂入，drain 取走当前输出；finish 后仍需收集最后一段。 */
    encoder = xrtLz4CompressorCreate(&options);
    packed = xrtBufferCreate();
    if (encoder == NULL || packed == NULL ||
        !xrtLz4CompressorUpdate(encoder, "left", 4)) goto done;
    chunk = xrtLz4CompressorDrainBuffer(encoder);
    if (chunk == NULL || !xrtBufferAppend(packed, xrtBufferView(chunk))) goto done;
    xrtBufferDestroy(chunk);
    chunk = NULL;
    if (!xrtLz4CompressorUpdate(encoder, "right", 5) ||
        !xrtLz4CompressorFinish(encoder)) goto done;
    chunk = xrtLz4CompressorTakeBuffer(encoder);
    if (chunk == NULL || !xrtBufferAppend(packed, xrtBufferView(chunk))) goto done;
    xrtBufferDestroy(chunk);
    chunk = NULL;
    decoder = xrtLz4DecompressorCreate(NULL);
    if (decoder == NULL ||
        !xrtLz4DecompressorUpdate(decoder, xrtBufferView(packed).Data, xrtBufferView(packed).Size) ||
        !xrtLz4DecompressorFinish(decoder)) goto done;
    plain = xrtLz4DecompressorTakeBuffer(decoder);
    if (plain == NULL || xrtBufferView(plain).Size != 9 ||
        memcmp(xrtBufferView(plain).Data, "leftright", 9) != 0 ||
        xrtLz4CompressorInputBytes(encoder) != 9 ||
        xrtLz4DecompressorOutputBytes(decoder) != 9) goto done;
    xrtLz4CompressorDestroy(encoder);
    xrtLz4DecompressorDestroy(decoder);
    xrtBufferDestroy(plain);
    xrtBufferDestroy(packed);
    encoder = NULL;
    decoder = NULL;
    plain = packed = NULL;

    if (xrtLz4Decompress(NULL, 0, NULL, &plain, &error) ||
        plain != NULL || error.iCode != XRT_LZ4_ERROR_TRUNCATED) goto done;
    xrtClearError();
    status = 0;
done:
    xrtBufferDestroy(chunk);
    xrtBufferDestroy(plain);
    xrtBufferDestroy(packed);
    xrtLz4CompressorDestroy(encoder);
    xrtLz4DecompressorDestroy(decoder);
    if (status == 0) {
        puts("utf8: ok");
        puts("binary: ok");
        puts("stream: ok");
        puts("truncated: rejected");
    }
    return status;
}
