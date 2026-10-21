#define XRT_MODULE_MEMORY_DEBUG
#include "../include/xrt_bz2.h"
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
        xrtbz2error error;
        xbuffer* output = NULL;
        bool ok, injected;
        assert(xrtMemDebugFailAfter(index));
        ok = decode ? xrtBz2Decompress(packed.Data, packed.Size, NULL, &output, &error) :
            xrtBz2Compress(plain.Data, plain.Size, NULL, &output, &error);
        injected = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (injected) {
            assert(!ok && output == NULL && error.iCode == XRT_BZ2_ERROR_MEMORY);
            ++bad;
        } else {
            assert(ok && output && error.iCode == XRT_BZ2_OK);
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
    xrtbz2compressor* c;
    xrtbz2decompressor* d;
    xbuffer* output;
    xmemdebugsnapshot before;
    xrtMemDebugSnapshot(&before);
    c = xrtBz2CompressorCreate(NULL);
    assert(c && xrtBz2CompressorUpdate(c, plain.Data, plain.Size) && xrtBz2CompressorFinish(c));
    assert(xrtMemDebugFailAfter(0));
    assert(xrtBz2CompressorDrainBuffer(c) == NULL && xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    assert(xrtBz2CompressorError(c)->iCode == XRT_BZ2_ERROR_MEMORY);
    output = xrtBz2CompressorDrainBuffer(c);
    assert(output && output->Size == xrtBz2CompressorOutputBytes(c));
    assert(xrtBz2CompressorError(c)->iCode == XRT_BZ2_OK);
    xrtBufferDestroy(output);
    xrtBz2CompressorDestroy(c);
    d = xrtBz2DecompressorCreate(NULL);
    assert(d && xrtBz2DecompressorUpdate(d, packed.Data, packed.Size) && xrtBz2DecompressorFinish(d));
    assert(xrtMemDebugFailAfter(0));
    assert(xrtBz2DecompressorDrainBuffer(d) == NULL && xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    assert(xrtBz2DecompressorError(d)->iCode == XRT_BZ2_ERROR_MEMORY);
    output = xrtBz2DecompressorDrainBuffer(d);
    assert(output && output->Size == plain.Size && memcmp(output->Data, plain.Data, plain.Size) == 0);
    assert(xrtBz2DecompressorError(d)->iCode == XRT_BZ2_OK);
    xrtBufferDestroy(output);
    xrtBz2DecompressorDestroy(d);
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
        object = decode ? (void*)xrtBz2DecompressorCreate(NULL) :
            (void*)xrtBz2CompressorCreate(NULL);
        injected = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (injected) {
            assert(object == NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            bad++;
        } else {
            assert(object && xrtGetError() == NULL);
            good++;
        }
        if (decode) xrtBz2DecompressorDestroy(object);
        else xrtBz2CompressorDestroy(object);
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
    assert(xrtBz2Compress(data, sizeof(data), NULL, &packed, NULL));
    count = failures((xbytesview){data, sizeof(data)}, xrtBufferView(packed), false);
    count += failures((xbytesview){data, sizeof(data)}, xrtBufferView(packed), true);
    count += constructors(false) + constructors(true);
    drain_retry(xrtBufferView(packed), (xbytesview){data, sizeof(data)});
    xrtBufferDestroy(packed);
    unchanged(&before);
    printf("bz2 allocation: rejected=%u, recovery and ownership passed\n", count);
    return 0;
}
