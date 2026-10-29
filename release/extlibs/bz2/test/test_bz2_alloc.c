#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE
#include "../include/xrt_bz2.h"
#include "../include/bz2_native.h"
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

/* 文本结果直接接管解压缓冲，失败时也不能遗留 Buffer。 */
static void text_transfer(void)
{
    const unsigned char plain[] = {'A', 0, 'B', 0xe4, 0xbd, 0xa0, 0xe5, 0xa5, 0xbd};
    const unsigned char invalid[] = {0xff};
    xmemdebugsnapshot before;
    xbuffer* packed = NULL;
    void* raw;
    void* expected;
    char* text;
    unsigned int index, failuresSeen = 0, successes = 0;
    xrtMemDebugSnapshot(&before);
    assert(xrtBz2Compress(plain, sizeof(plain), NULL, &packed, NULL));
    raw = xbz2_decompress_text(packed->Data, packed->Size, true, 1024, 1024, 1000);
    assert(raw != NULL && xbz2_text_size(raw) == sizeof(plain));
    expected = ((xbuffer*)raw)->Data;
    text = (char*)xbz2_text_take(raw);
    assert(text == expected && memcmp(text, plain, sizeof(plain)) == 0 && text[sizeof(plain)] == 0);
    xrtFree(text);

    for (index = 0; index < 128 && successes < 2; ++index) {
        bool injected;
        assert(xrtMemDebugFailAfter(index));
        raw = xbz2_decompress_text(packed->Data, packed->Size, false, 1024, 1024, 1000);
        injected = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (injected) {
            assert(raw == NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            ++failuresSeen;
        } else {
            assert(raw != NULL);
            xrtFree(xbz2_text_take(raw));
            ++successes;
        }
    }
    assert(failuresSeen > 0 && successes == 2);
    xrtBufferDestroy(packed);
    unchanged(&before);

    assert(xrtBz2Compress(invalid, sizeof(invalid), NULL, &packed, NULL));
    assert(xbz2_decompress_text(packed->Data, packed->Size, false, 1024, 1024, 1000) == NULL);
    assert(xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtBufferDestroy(packed);
    unchanged(&before);

    assert(xrtBz2Compress("", 0, NULL, &packed, NULL));
    raw = xbz2_decompress_text(packed->Data, packed->Size, false, 1024, 1024, 1000);
    assert(raw != NULL && xbz2_text_size(raw) == 0);
    text = (char*)xbz2_text_take(raw);
    assert(text != NULL && text[0] == 0);
    xrtFree(text);
    xrtBufferDestroy(packed);
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
    assert(xrtBz2Compress(data, sizeof(data), NULL, &packed, NULL));
    count = failures((xbytesview){data, sizeof(data)}, xrtBufferView(packed), false);
    count += failures((xbytesview){data, sizeof(data)}, xrtBufferView(packed), true);
    count += constructors(false) + constructors(true);
    drain_retry(xrtBufferView(packed), (xbytesview){data, sizeof(data)});
    xrtBufferDestroy(packed);
    unchanged(&before);
    text_transfer();
    printf("bz2 allocation: rejected=%u, recovery and ownership passed\n", count);
    return 0;
}
