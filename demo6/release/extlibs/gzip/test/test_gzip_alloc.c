#define XRT_MODULE_MEMORY_DEBUG
#include "../include/xrt_gzip.h"
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

/* 逐个分配点注入失败；失败不交付残缺结果，恢复后仍可创建独立对象。 */
static unsigned int failures(xbytesview plain, xbytesview packed, bool decode)
{
    xmemdebugsnapshot before;
    unsigned int index, bad = 0, good = 0;
    xrtMemDebugSnapshot(&before);
    for (index = 0; index < 256 && good < 2; ++index) {
        xrtgziperror error;
        xbuffer* output = NULL;
        bool ok, injected;
        assert(xrtMemDebugFailAfter(index));
        ok = decode ? xrtGzipDecompress(packed.Data, packed.Size, NULL, &output, &error) :
            xrtGzipCompress(plain.Data, plain.Size, NULL, &output, &error);
        injected = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (injected) {
            assert(!ok && output == NULL && error.iCode == XRT_GZIP_ERROR_MEMORY);
            ++bad;
        } else {
            assert(ok && output && error.iCode == XRT_GZIP_OK);
            if (decode) assert(output->Size == plain.Size && memcmp(output->Data, plain.Data, plain.Size) == 0);
            ++good;
        }
        xrtBufferDestroy(output);
        unchanged(&before);
    }
    assert(good == 2 && bad > 0);
    return bad;
}

static void drain_retry(xbytesview packed, xbytesview plain)
{
    xrtgzipcompressor* c;
    xrtgzipdecompressor* d;
    xbuffer* output;
    xmemdebugsnapshot before;
    xrtMemDebugSnapshot(&before);
    c = xrtGzipCompressorCreate(NULL);
    assert(c && xrtGzipCompressorUpdate(c, plain.Data, plain.Size) && xrtGzipCompressorFinish(c));
    assert(xrtMemDebugFailAfter(0));
    assert(xrtGzipCompressorDrainBuffer(c) == NULL && xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    assert(xrtGzipCompressorError(c)->iCode == XRT_GZIP_ERROR_MEMORY);
    output = xrtGzipCompressorDrainBuffer(c);
    assert(output && output->Size == xrtGzipCompressorOutputBytes(c));
    assert(xrtGzipCompressorError(c)->iCode == XRT_GZIP_OK);
    xrtBufferDestroy(output);
    xrtGzipCompressorDestroy(c);
    d = xrtGzipDecompressorCreate(NULL);
    assert(d && xrtGzipDecompressorUpdate(d, packed.Data, packed.Size) && xrtGzipDecompressorFinish(d));
    assert(xrtMemDebugFailAfter(0));
    assert(xrtGzipDecompressorDrainBuffer(d) == NULL && xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    assert(xrtGzipDecompressorError(d)->iCode == XRT_GZIP_ERROR_MEMORY);
    output = xrtGzipDecompressorDrainBuffer(d);
    assert(output && output->Size == plain.Size && memcmp(output->Data, plain.Data, plain.Size) == 0);
    assert(xrtGzipDecompressorError(d)->iCode == XRT_GZIP_OK);
    xrtBufferDestroy(output);
    xrtGzipDecompressorDestroy(d);
    unchanged(&before);
}

int main(void)
{
    unsigned char data[16384];
    xbuffer* packed = NULL;
    unsigned int i, count;
    xmemdebugsnapshot before;
    assert(xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
    for (i = 0; i < sizeof(data); ++i) data[i] = (unsigned char)(i * 31u + i / 17u);
    assert(xrtGzipCompress(data, sizeof(data), NULL, &packed, NULL));
    count = failures((xbytesview){data, sizeof(data)}, xrtBufferView(packed), false);
    count += failures((xbytesview){data, sizeof(data)}, xrtBufferView(packed), true);
    drain_retry(xrtBufferView(packed), (xbytesview){data, sizeof(data)});
    xrtBufferDestroy(packed);
    unchanged(&before);
    printf("gzip allocation: rejected=%u, recovery and ownership passed\n", count);
    return 0;
}
