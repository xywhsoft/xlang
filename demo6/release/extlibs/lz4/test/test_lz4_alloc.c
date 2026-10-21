#define XRT_MODULE_MEMORY_DEBUG
#include "../include/xrt_lz4.h"
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
        xrtlz4error error;
        xbuffer* output = NULL;
        bool ok, injected;
        assert(xrtMemDebugFailAfter(index));
        ok = decode ? xrtLz4Decompress(packed.Data, packed.Size, NULL, &output, &error) :
            xrtLz4Compress(plain.Data, plain.Size, NULL, &output, &error);
        injected = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (injected) {
            assert(!ok && output == NULL && error.iCode == XRT_LZ4_ERROR_MEMORY);
            ++bad;
        } else {
            assert(ok && output && error.iCode == XRT_LZ4_OK);
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
    xrtlz4compressor* c;
    xrtlz4decompressor* d;
    xbuffer* output;
    xmemdebugsnapshot before;
    xrtMemDebugSnapshot(&before);
    c = xrtLz4CompressorCreate(NULL);
    assert(c && xrtLz4CompressorUpdate(c, plain.Data, plain.Size) && xrtLz4CompressorFinish(c));
    assert(xrtMemDebugFailAfter(0));
    assert(xrtLz4CompressorDrainBuffer(c) == NULL && xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    assert(xrtLz4CompressorError(c)->iCode == XRT_LZ4_ERROR_MEMORY);
    output = xrtLz4CompressorDrainBuffer(c);
    assert(output && output->Size == xrtLz4CompressorOutputBytes(c));
    assert(xrtLz4CompressorError(c)->iCode == XRT_LZ4_OK);
    xrtBufferDestroy(output);
    xrtLz4CompressorDestroy(c);
    d = xrtLz4DecompressorCreate(NULL);
    assert(d && xrtLz4DecompressorUpdate(d, packed.Data, packed.Size) && xrtLz4DecompressorFinish(d));
    assert(xrtMemDebugFailAfter(0));
    assert(xrtLz4DecompressorDrainBuffer(d) == NULL && xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    assert(xrtLz4DecompressorError(d)->iCode == XRT_LZ4_ERROR_MEMORY);
    output = xrtLz4DecompressorDrainBuffer(d);
    assert(output && output->Size == plain.Size && memcmp(output->Data, plain.Data, plain.Size) == 0);
    assert(xrtLz4DecompressorError(d)->iCode == XRT_LZ4_OK);
    xrtBufferDestroy(output);
    xrtLz4DecompressorDestroy(d);
    unchanged(&before);
}

/* 失败的构造必须返回 NULL，且每个中间分配都被回收。 */
static unsigned int constructors(bool decode)
{
    xmemdebugsnapshot before;
    unsigned int index, bad = 0, good = 0;
    xrtMemDebugSnapshot(&before);
    for (index = 0; index < 64 && good < 2; ++index) {
        void* object;
        bool injected;
        assert(xrtMemDebugFailAfter(index));
        object = decode ? (void*)xrtLz4DecompressorCreate(NULL) :
            (void*)xrtLz4CompressorCreate(NULL);
        injected = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (injected) {
            assert(object == NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            bad++;
        } else {
            assert(object && xrtGetError() == NULL);
            good++;
        }
        if (decode) xrtLz4DecompressorDestroy(object);
        else xrtLz4CompressorDestroy(object);
        unchanged(&before);
    }
    assert(good == 2 && bad > 0);
    return bad;
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
    assert(xrtLz4Compress(data, sizeof(data), NULL, &packed, NULL));
    count = failures((xbytesview){data, sizeof(data)}, xrtBufferView(packed), false);
    count += failures((xbytesview){data, sizeof(data)}, xrtBufferView(packed), true);
    count += constructors(false) + constructors(true);
    drain_retry(xrtBufferView(packed), (xbytesview){data, sizeof(data)});
    xrtBufferDestroy(packed);
    unchanged(&before);
    printf("lz4 allocation: rejected=%u, recovery and ownership passed\n", count);
    return 0;
}
