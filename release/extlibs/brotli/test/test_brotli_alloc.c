#define XRT_MODULE_MEMORY_DEBUG
#include "../include/xrt_brotli.h"
#include "../include/brotli_native.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void unchanged(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    assert(after.LiveBytes == before->LiveBytes && after.LiveCount == before->LiveCount);
    assert(after.InvalidFreeCount == before->InvalidFreeCount);
    assert(after.DoubleFreeCount == before->DoubleFreeCount);
    assert(after.OverflowCount == before->OverflowCount && after.UnderflowCount == before->UnderflowCount);
}

/* 依次让每个分配点失败；包含后端的自定义分配器，不只检查包装对象。 */
static unsigned int failures(xbytesview plain, xbytesview packed, bool decode, int quality)
{
    xrtbrotlioptions config;
    xmemdebugsnapshot before;
    unsigned int index, bad = 0, good = 0;
    xrtBrotliOptionsInit(&config);
    config.iQuality = quality;
    xrtMemDebugSnapshot(&before);
    for (index = 0; index < 512 && good < 2; ++index) {
        xrtbrotlierror error;
        xbuffer* output = NULL;
        bool ok, injected;
        assert(xrtMemDebugFailAfter(index));
        ok = decode ? xrtBrotliDecompress(packed.Data, packed.Size, &config, &output, &error) :
            xrtBrotliCompress(plain.Data, plain.Size, &config, &output, &error);
        injected = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (injected) {
            assert(!ok && output == NULL && error.iCode == XRT_BROTLI_ERROR_MEMORY);
            ++bad;
        } else {
            assert(ok && output && error.iCode == XRT_BROTLI_OK);
            if (decode) {
                xbytesview actual = xrtBufferView(output);
                assert(actual.Size == plain.Size && memcmp(actual.Data, plain.Data, plain.Size) == 0);
            }
            ++good;
        }
        xrtBufferDestroy(output);
        unchanged(&before);
    }
    assert(good == 2 && bad > 0);
    return bad;
}

/* drain 更换 Buffer 失败不能丢失输出，恢复内存后可重试原操作。 */
static void drain_retry(xbytesview packed, xbytesview plain)
{
    xrtbrotlioptions config;
    xrtbrotlicompressor* c;
    xrtbrotlidecompressor* d;
    xbuffer* output;
    xmemdebugsnapshot before;
    xrtMemDebugSnapshot(&before);
    xrtBrotliOptionsInit(&config);
    config.iQuality = 4;
    c = xrtBrotliCompressorCreate(&config);
    assert(c && xrtBrotliCompressorUpdate(c, plain.Data, plain.Size) && xrtBrotliCompressorFinish(c));
    assert(xrtMemDebugFailAfter(0));
    assert(xrtBrotliCompressorDrainBuffer(c) == NULL && xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    assert(xrtBrotliCompressorError(c)->iCode == XRT_BROTLI_ERROR_MEMORY);
    xrtClearError();
    output = xrtBrotliCompressorDrainBuffer(c);
    assert(output && xrtBufferView(output).Size == xrtBrotliCompressorOutputBytes(c));
    assert(xrtBrotliCompressorError(c)->iCode == XRT_BROTLI_OK);
    xrtBufferDestroy(output);
    xrtBrotliCompressorDestroy(c);
    d = xrtBrotliDecompressorCreate(NULL);
    assert(d && xrtBrotliDecompressorUpdate(d, packed.Data, packed.Size) && xrtBrotliDecompressorFinish(d));
    assert(xrtMemDebugFailAfter(0));
    assert(xrtBrotliDecompressorDrainBuffer(d) == NULL && xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    assert(xrtBrotliDecompressorError(d)->iCode == XRT_BROTLI_ERROR_MEMORY);
    xrtClearError();
    output = xrtBrotliDecompressorDrainBuffer(d);
    assert(output && xrtBufferView(output).Size == plain.Size);
    assert(memcmp(xrtBufferView(output).Data, plain.Data, plain.Size) == 0);
    assert(xrtBrotliDecompressorError(d)->iCode == XRT_BROTLI_OK);
    xrtBufferDestroy(output);
    xrtBrotliDecompressorDestroy(d);
    unchanged(&before);
}

static unsigned int text_failures(void)
{
    xbuffer* packed = NULL;
    xmemdebugsnapshot before;
    unsigned int index, bad = 0, good = 0;
    packed = xbrotli_compress("hello", 5, 4, 22, 0, 0, 0);
    assert(packed);
    xrtMemDebugSnapshot(&before);
    for (index = 0; index < 512 && good < 2; ++index) {
        void* result;
        bool injected;
        assert(xrtMemDebugFailAfter(index));
        result = xbrotli_decompress_text(packed->Data, packed->Size,
            268435456u, 536870912u, 1000u);
        injected = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (injected) {
            assert(result == NULL);
            ++bad;
        } else {
            char* text;
            assert(result != NULL && xbrotli_text_size(result) == 5);
            text = xbrotli_text_take(result);
            assert(text != NULL && memcmp(text, "hello", 6) == 0);
            xrtFree(text);
            ++good;
        }
        unchanged(&before);
    }
    assert(bad > 0 && good == 2);
    xrtBufferDestroy(packed);
    return bad;
}

int main(void)
{
    unsigned char data[16384];
    xrtbrotlioptions config;
    xrtbrotlierror error;
    xbuffer* packed = NULL;
    unsigned int i, count;
    xmemdebugsnapshot before;
    assert(xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
    for (i = 0; i < sizeof(data); ++i) data[i] = (unsigned char)(i * 31u + i / 17u);
    xrtBrotliOptionsInit(&config);
    config.iQuality = 4;
    assert(xrtBrotliCompress(data, sizeof(data), &config, &packed, &error));
    count = failures((xbytesview){data, sizeof(data)}, xrtBufferView(packed), false, 4);
    count += failures((xbytesview){data, sizeof(data)}, xrtBufferView(packed), false, 11);
    count += failures((xbytesview){data, sizeof(data)}, xrtBufferView(packed), true, 4);
    drain_retry(xrtBufferView(packed), (xbytesview){data, sizeof(data)});
    xrtBufferDestroy(packed);
    count += text_failures();
    unchanged(&before);
    printf("brotli allocation: rejected=%u, recovery and ownership passed\n", count);
    return 0;
}
