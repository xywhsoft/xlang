#include "../include/xrt_bz2.h"
#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
/* 单文件 xl run 编入 bzip2 后端；正式包仍按清单分离编译。 */
#include "../../_bzip2/third_party/bzip2/blocksort.c"
#include "../../_bzip2/third_party/bzip2/huffman.c"
#include "../../_bzip2/third_party/bzip2/crctable.c"
#include "../../_bzip2/third_party/bzip2/randtable.c"
#include "../../_bzip2/third_party/bzip2/compress.c"
#include "../../_bzip2/third_party/bzip2/decompress.c"
#include "../../_bzip2/third_party/bzip2/bzlib.c"
#include "../xrt_bz2.c"
#endif

int main(void)
{
    /* 与 bz2_basic.xl 使用完全相同的 UTF-8 字节及二进制输入。 */
    static const unsigned char text[] = "hello \xe4\xb8\x96\xe7\x95\x8c";
    static const unsigned char binary[] = {0, 255, 128, 65};
    xrtbz2options options;
    xrtbz2error error;
    xbuffer* packed = NULL;
    xbuffer* plain = NULL;
    xrtbz2compressor* encoder = NULL;
    xrtbz2decompressor* decoder = NULL;
    xbuffer* chunk = NULL;
    int status = 1;
    xrtBz2OptionsInit(&options);
    options.iLevel = 4;
    if (!xrtBz2Compress(text, sizeof(text) - 1, &options, &packed, &error) ||
        !xrtBz2Decompress(xrtBufferView(packed).Data, xrtBufferView(packed).Size,
            NULL, &plain, &error) ||
        xrtBufferView(plain).Size != sizeof(text) - 1 ||
        memcmp(xrtBufferView(plain).Data, text, sizeof(text) - 1) != 0) goto done;
    xrtBufferDestroy(plain);
    xrtBufferDestroy(packed);
    plain = packed = NULL;

    /* 一次性接口转移 Buffer 拥有权；完成后由 C 调用方各释放一次。 */
    if (!xrtBz2Compress(binary, sizeof(binary), &options, &packed, &error) ||
        !xrtBz2Decompress(xrtBufferView(packed).Data, xrtBufferView(packed).Size,
            NULL, &plain, &error) ||
        xrtBufferView(plain).Size != sizeof(binary) ||
        memcmp(xrtBufferView(plain).Data, binary, sizeof(binary)) != 0) goto done;
    xrtBufferDestroy(plain);
    xrtBufferDestroy(packed);
    plain = packed = NULL;

    /* 分块喂入，drain 取走当前输出；finish 后仍需收集最后一段。 */
    encoder = xrtBz2CompressorCreate(&options);
    packed = xrtBufferCreate();
    if (encoder == NULL || packed == NULL ||
        !xrtBz2CompressorUpdate(encoder, "left", 4)) goto done;
    chunk = xrtBz2CompressorDrainBuffer(encoder);
    if (chunk == NULL || !xrtBufferAppend(packed, xrtBufferView(chunk))) goto done;
    xrtBufferDestroy(chunk);
    chunk = NULL;
    if (!xrtBz2CompressorUpdate(encoder, "right", 5) ||
        !xrtBz2CompressorFinish(encoder)) goto done;
    chunk = xrtBz2CompressorTakeBuffer(encoder);
    if (chunk == NULL || !xrtBufferAppend(packed, xrtBufferView(chunk))) goto done;
    xrtBufferDestroy(chunk);
    chunk = NULL;
    decoder = xrtBz2DecompressorCreate(NULL);
    if (decoder == NULL ||
        !xrtBz2DecompressorUpdate(decoder, xrtBufferView(packed).Data, xrtBufferView(packed).Size) ||
        !xrtBz2DecompressorFinish(decoder)) goto done;
    plain = xrtBz2DecompressorTakeBuffer(decoder);
    if (plain == NULL || xrtBufferView(plain).Size != 9 ||
        memcmp(xrtBufferView(plain).Data, "leftright", 9) != 0 ||
        xrtBz2CompressorInputBytes(encoder) != 9 ||
        xrtBz2DecompressorOutputBytes(decoder) != 9) goto done;
    xrtBz2CompressorDestroy(encoder);
    xrtBz2DecompressorDestroy(decoder);
    xrtBufferDestroy(plain);
    xrtBufferDestroy(packed);
    encoder = NULL;
    decoder = NULL;
    plain = packed = NULL;

    if (xrtBz2Decompress(NULL, 0, NULL, &plain, &error) ||
        plain != NULL || error.iCode != XRT_BZ2_ERROR_TRUNCATED) goto done;
    xrtClearError();
    status = 0;
done:
    xrtBufferDestroy(chunk);
    xrtBufferDestroy(plain);
    xrtBufferDestroy(packed);
    xrtBz2CompressorDestroy(encoder);
    xrtBz2DecompressorDestroy(decoder);
    if (status == 0) {
        puts("utf8: ok");
        puts("binary: ok");
        puts("stream: ok");
        puts("truncated: rejected");
    }
    return status;
}
