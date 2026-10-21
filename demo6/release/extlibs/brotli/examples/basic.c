#include "../include/xrt_brotli.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    /* 与 brotli_basic.xl 使用完全相同的 UTF-8 字节及二进制输入。 */
    static const unsigned char text[] = "hello \xe4\xb8\x96\xe7\x95\x8c";
    static const unsigned char binary[] = {0, 255, 128, 65};
    xrtbrotlioptions options;
    xrtbrotlierror error;
    xbuffer* packed = NULL;
    xbuffer* plain = NULL;
    xrtbrotlicompressor* encoder = NULL;
    xrtbrotlidecompressor* decoder = NULL;
    xbuffer* chunk = NULL;
    int status = 1;
    xrtBrotliOptionsInit(&options);
    options.iQuality = 4;
    if (!xrtBrotliCompress(text, sizeof(text) - 1, &options, &packed, &error) ||
        !xrtBrotliDecompress(xrtBufferView(packed).Data, xrtBufferView(packed).Size,
            NULL, &plain, &error) ||
        xrtBufferView(plain).Size != sizeof(text) - 1 ||
        memcmp(xrtBufferView(plain).Data, text, sizeof(text) - 1) != 0) goto done;
    xrtBufferDestroy(plain);
    xrtBufferDestroy(packed);
    plain = packed = NULL;

    /* 一次性接口转移 Buffer 拥有权；完成后由 C 调用方各释放一次。 */
    if (!xrtBrotliCompress(binary, sizeof(binary), &options, &packed, &error) ||
        !xrtBrotliDecompress(xrtBufferView(packed).Data, xrtBufferView(packed).Size,
            NULL, &plain, &error) ||
        xrtBufferView(plain).Size != sizeof(binary) ||
        memcmp(xrtBufferView(plain).Data, binary, sizeof(binary)) != 0) goto done;
    xrtBufferDestroy(plain);
    xrtBufferDestroy(packed);
    plain = packed = NULL;

    /* 分块喂入，drain 取走当前输出；finish 后仍需收集最后一段。 */
    encoder = xrtBrotliCompressorCreate(&options);
    packed = xrtBufferCreate();
    if (encoder == NULL || packed == NULL ||
        !xrtBrotliCompressorUpdate(encoder, "left", 4)) goto done;
    chunk = xrtBrotliCompressorDrainBuffer(encoder);
    if (chunk == NULL || !xrtBufferAppend(packed, xrtBufferView(chunk))) goto done;
    xrtBufferDestroy(chunk);
    chunk = NULL;
    if (!xrtBrotliCompressorUpdate(encoder, "right", 5) ||
        !xrtBrotliCompressorFinish(encoder)) goto done;
    chunk = xrtBrotliCompressorTakeBuffer(encoder);
    if (chunk == NULL || !xrtBufferAppend(packed, xrtBufferView(chunk))) goto done;
    xrtBufferDestroy(chunk);
    chunk = NULL;
    decoder = xrtBrotliDecompressorCreate(NULL);
    if (decoder == NULL ||
        !xrtBrotliDecompressorUpdate(decoder, xrtBufferView(packed).Data, xrtBufferView(packed).Size) ||
        !xrtBrotliDecompressorFinish(decoder)) goto done;
    plain = xrtBrotliDecompressorTakeBuffer(decoder);
    if (plain == NULL || xrtBufferView(plain).Size != 9 ||
        memcmp(xrtBufferView(plain).Data, "leftright", 9) != 0 ||
        xrtBrotliCompressorInputBytes(encoder) != 9 ||
        xrtBrotliDecompressorMembers(decoder) != 1) goto done;
    xrtBrotliCompressorDestroy(encoder);
    xrtBrotliDecompressorDestroy(decoder);
    xrtBufferDestroy(plain);
    xrtBufferDestroy(packed);
    encoder = NULL;
    decoder = NULL;
    plain = packed = NULL;

    if (xrtBrotliDecompress(NULL, 0, NULL, &plain, &error) ||
        plain != NULL || error.iCode != XRT_BROTLI_ERROR_TRUNCATED) goto done;
    xrtClearError();
    status = 0;
done:
    xrtBufferDestroy(chunk);
    xrtBufferDestroy(plain);
    xrtBufferDestroy(packed);
    xrtBrotliCompressorDestroy(encoder);
    xrtBrotliDecompressorDestroy(decoder);
    if (status == 0) {
        puts("utf8: ok");
        puts("binary: ok");
        puts("stream: ok");
        puts("truncated: rejected");
    }
    return status;
}
