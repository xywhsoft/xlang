#ifdef __TINYC__
#include "../xrt_gzip.c"
#else
#include "../include/xrt_gzip.h"
#endif
#include <stdio.h>
#include <string.h>

int main(void)
{
    /* 与 gzip_basic.xl 使用完全相同的 UTF-8 字节及二进制输入。 */
    static const unsigned char text[] = "hello \xe4\xb8\x96\xe7\x95\x8c";
    static const unsigned char binary[] = {0, 255, 128, 65};
    xrtgzipoptions options;
    xrtgziperror error;
    xbuffer* packed = NULL;
    xbuffer* plain = NULL;
    xrtgzipcompressor* encoder = NULL;
    xrtgzipdecompressor* decoder = NULL;
    xbuffer* chunk = NULL;
    int status = 1;
    xrtGzipOptionsInit(&options);
    options.iLevel = 4;
    if (!xrtGzipCompress(text, sizeof(text) - 1, &options, &packed, &error) ||
        !xrtGzipDecompress(xrtBufferView(packed).Data, xrtBufferView(packed).Size,
            NULL, &plain, &error) ||
        xrtBufferView(plain).Size != sizeof(text) - 1 ||
        memcmp(xrtBufferView(plain).Data, text, sizeof(text) - 1) != 0) goto done;
    xrtBufferDestroy(plain);
    xrtBufferDestroy(packed);
    plain = packed = NULL;

    /* 一次性接口转移 Buffer 拥有权；完成后由 C 调用方各释放一次。 */
    if (!xrtGzipCompress(binary, sizeof(binary), &options, &packed, &error) ||
        !xrtGzipDecompress(xrtBufferView(packed).Data, xrtBufferView(packed).Size,
            NULL, &plain, &error) ||
        xrtBufferView(plain).Size != sizeof(binary) ||
        memcmp(xrtBufferView(plain).Data, binary, sizeof(binary)) != 0) goto done;
    xrtBufferDestroy(plain);
    xrtBufferDestroy(packed);
    plain = packed = NULL;

    /* 分块喂入，drain 取走当前输出；finish 后仍需收集最后一段。 */
    encoder = xrtGzipCompressorCreate(&options);
    packed = xrtBufferCreate();
    if (encoder == NULL || packed == NULL ||
        !xrtGzipCompressorUpdate(encoder, "left", 4)) goto done;
    chunk = xrtGzipCompressorDrainBuffer(encoder);
    if (chunk == NULL || !xrtBufferAppend(packed, xrtBufferView(chunk))) goto done;
    xrtBufferDestroy(chunk);
    chunk = NULL;
    if (!xrtGzipCompressorUpdate(encoder, "right", 5) ||
        !xrtGzipCompressorFinish(encoder)) goto done;
    chunk = xrtGzipCompressorTakeBuffer(encoder);
    if (chunk == NULL || !xrtBufferAppend(packed, xrtBufferView(chunk))) goto done;
    xrtBufferDestroy(chunk);
    chunk = NULL;
    decoder = xrtGzipDecompressorCreate(NULL);
    if (decoder == NULL ||
        !xrtGzipDecompressorUpdate(decoder, xrtBufferView(packed).Data, xrtBufferView(packed).Size) ||
        !xrtGzipDecompressorFinish(decoder)) goto done;
    plain = xrtGzipDecompressorTakeBuffer(decoder);
    if (plain == NULL || xrtBufferView(plain).Size != 9 ||
        memcmp(xrtBufferView(plain).Data, "leftright", 9) != 0 ||
        xrtGzipCompressorInputBytes(encoder) != 9 ||
        xrtGzipDecompressorOutputBytes(decoder) != 9) goto done;
    xrtGzipCompressorDestroy(encoder);
    xrtGzipDecompressorDestroy(decoder);
    xrtBufferDestroy(plain);
    xrtBufferDestroy(packed);
    encoder = NULL;
    decoder = NULL;
    plain = packed = NULL;

    if (xrtGzipDecompress(NULL, 0, NULL, &plain, &error) ||
        plain != NULL || error.iCode != XRT_GZIP_ERROR_TRUNCATED) goto done;
    xrtClearError();
    status = 0;
done:
    xrtBufferDestroy(chunk);
    xrtBufferDestroy(plain);
    xrtBufferDestroy(packed);
    xrtGzipCompressorDestroy(encoder);
    xrtGzipDecompressorDestroy(decoder);
    if (status == 0) {
        puts("utf8: ok");
        puts("binary: ok");
        puts("stream: ok");
        puts("truncated: rejected");
    }
    return status;
}
