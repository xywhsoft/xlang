#include "../include/xrt_bz2.h"
/* 测试操作位于断言中，发布配置也必须执行。 */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void rejected_options(const xrtbz2options* options)
{
    assert(xrtBz2CompressorCreate(options) == NULL);
    assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(xrtBz2DecompressorCreate(options) == NULL);
    assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
}

static void options_and_arguments(void)
{
    xrtbz2options options, bad;
    xrtbz2error error;
    xbuffer* output = NULL;
    xrtbz2compressor* encoder;
    xrtbz2decompressor* decoder;
    unsigned i;
    xrtBz2OptionsInit(NULL);
    xrtBz2ErrorInit(NULL);
    xrtBz2OptionsInit(&options);
    assert(options.iLevel == 9 && options.iWorkFactor == 30 && !options.bSmallMemory);
    /* 归档和树结构限额不属于 bzip2 流合同。 */
    assert(options.tLimits.iMaxItemBytes == 0 && options.tLimits.iMaxEntries == 0 &&
        options.tLimits.iMaxNodes == 0 && options.tLimits.iMaxDepth == 0);
    for (i = 0; i < 5; ++i) {
        bad = options;
        if (i == 0) bad.tLimits.iFlags = 1;
        if (i == 1) bad.tLimits.iMaxItemBytes = 1;
        if (i == 2) bad.tLimits.iMaxEntries = 1;
        if (i == 3) bad.tLimits.iMaxNodes = 1;
        if (i == 4) bad.tLimits.iMaxDepth = 1;
        rejected_options(&bad);
    }
    bad = options; bad.iVersion++; rejected_options(&bad);
    bad = options; bad.iSize--; rejected_options(&bad);
    bad = options; bad.sReserved[6] = 1; rejected_options(&bad);
    bad = options; bad.tLimits.iSize--; rejected_options(&bad);
    bad = options; bad.tLimits.iVersion++; rejected_options(&bad);
    bad = options; bad.tLimits.iReserved = 1; rejected_options(&bad);
    bad = options; bad.iLevel = 0; rejected_options(&bad);
    bad = options; bad.iLevel = 10; rejected_options(&bad);
    bad = options; bad.iWorkFactor = -1; rejected_options(&bad);
    bad = options; bad.iWorkFactor = 251; rejected_options(&bad);
    assert(!xrtBz2Compress(NULL, 1, NULL, &output, &error));
    assert(output == NULL && error.iCode == XRT_BZ2_ERROR_ARGUMENT);
    assert(!xrtBz2Compress("a", 1, NULL, NULL, &error));
    assert(error.iCode == XRT_BZ2_ERROR_ARGUMENT);
    assert(!xrtBz2Decompress(NULL, 0, NULL, &output, &error));
    assert(output == NULL && error.iCode == XRT_BZ2_ERROR_TRUNCATED);
    assert(!xrtBz2CompressorUpdate(NULL, NULL, 0));
    assert(!xrtBz2DecompressorFinish(NULL));
    assert(xrtBz2CompressorError(NULL) == NULL && xrtBz2DecompressorError(NULL) == NULL);
    assert(!xrtBz2CompressorFinished(NULL) && !xrtBz2DecompressorFinished(NULL));
    assert(xrtBz2CompressorInputBytes(NULL) == 0 && xrtBz2DecompressorMembers(NULL) == 0);
    xrtBz2CompressorDestroy(NULL);
    xrtBz2DecompressorDestroy(NULL);

    // 参数误用不消费输入；修正后可继续使用同一对象。
    encoder = xrtBz2CompressorCreate(NULL);
    assert(encoder && !xrtBz2CompressorUpdate(encoder, NULL, 1));
    assert(xrtBz2CompressorInputBytes(encoder) == 0);
    assert(!xrtBz2CompressorTakeBuffer(encoder));
    assert(!xrtBz2CompressorDrain(encoder, NULL));
    assert(xrtBz2CompressorUpdate(encoder, NULL, 0));
    assert(xrtBz2CompressorFinish(encoder) && xrtBz2CompressorFinish(encoder));
    assert(xrtBz2CompressorFinished(encoder));
    assert(!xrtBz2CompressorUpdate(encoder, "late", 4));
    output = xrtBz2CompressorTakeBuffer(encoder);
    assert(output && output->Size > 0);
    assert(!xrtBz2CompressorTakeBuffer(encoder) && !xrtBz2CompressorDrainBuffer(encoder));
    decoder = xrtBz2DecompressorCreate(NULL);
    assert(decoder && xrtBz2DecompressorUpdate(decoder, output->Data, output->Size));
    assert(xrtBz2DecompressorMembers(decoder) == 1 && xrtBz2DecompressorFinished(decoder));
    assert(xrtBz2DecompressorFinish(decoder) && xrtBz2DecompressorFinish(decoder));
    xrtBufferDestroy(output);
    output = xrtBz2DecompressorTakeBuffer(decoder);
    assert(output && output->Size == 0);
    assert(!xrtBz2DecompressorUpdate(decoder, NULL, 0));
    assert(!xrtBz2DecompressorTakeBuffer(decoder));
    xrtBufferDestroy(output);
    xrtBz2CompressorDestroy(encoder);
    xrtBz2DecompressorDestroy(decoder);
    xrtClearError();
}

static void limits_and_failure_state(void)
{
    unsigned char text[131073];
    xrtbz2options options;
    xrtbz2error error;
    xbuffer *packed = NULL, *plain = NULL;
    xrtbz2compressor* encoder;
    xrtbz2decompressor* decoder;
    memset(text, 'a', sizeof(text));
    xrtBz2OptionsInit(&options);
    options.tLimits.iMaxCompressionRatio = 0;
    assert(xrtBz2Compress(text, sizeof(text), &options, &packed, NULL));
    options.tLimits.iMaxInputBytes = packed->Size;
    options.tLimits.iMaxOutputBytes = sizeof(text);
    assert(xrtBz2Decompress(packed->Data, packed->Size, &options, &plain, &error));
    assert(plain->Size == sizeof(text) && memcmp(text, plain->Data, sizeof(text)) == 0);
    xrtBufferDestroy(plain);
    options.tLimits.iMaxInputBytes--;
    assert(!xrtBz2Decompress(packed->Data, packed->Size, &options, &plain, &error));
    assert(plain == NULL && error.iCode == XRT_BZ2_ERROR_LIMIT);
    options.tLimits.iMaxInputBytes = 0;
    options.tLimits.iMaxOutputBytes--;
    assert(!xrtBz2Decompress(packed->Data, packed->Size, &options, &plain, &error));
    assert(plain == NULL && error.iCode == XRT_BZ2_ERROR_LIMIT);
    options.tLimits.iMaxOutputBytes = 0;
    options.tLimits.iMaxCompressionRatio = 1;
    assert(!xrtBz2Decompress(packed->Data, packed->Size, &options, &plain, &error));
    assert(plain == NULL && error.iCode == XRT_BZ2_ERROR_LIMIT);

    xrtBz2OptionsInit(&options);
    options.tLimits.iMaxInputBytes = 2;
    encoder = xrtBz2CompressorCreate(&options);
    assert(encoder && xrtBz2CompressorUpdate(encoder, "ab", 2));
    assert(!xrtBz2CompressorUpdate(encoder, "c", 1));
    assert(xrtBz2CompressorInputBytes(encoder) == 2);
    assert(!xrtBz2CompressorFinish(encoder) && !xrtBz2CompressorDrainBuffer(encoder));
    assert(xrtBz2CompressorError(encoder)->iCode == XRT_BZ2_ERROR_LIMIT);
    assert(!xrtBz2CompressorFinished(encoder));
    xrtBz2CompressorDestroy(encoder);
    options.tLimits.iMaxInputBytes = 0;
    options.tLimits.iMaxOutputBytes = 1;
    encoder = xrtBz2CompressorCreate(&options);
    assert(encoder && !xrtBz2CompressorFinish(encoder));
    assert(xrtBz2CompressorError(encoder)->iCode == XRT_BZ2_ERROR_LIMIT);
    xrtBz2CompressorDestroy(encoder);

    // 终态不能通过清除线程错误或调用 finish 恢复成成功。
    decoder = xrtBz2DecompressorCreate(NULL);
    assert(decoder && !xrtBz2DecompressorUpdate(decoder, "junk", 4));
    assert(xrtBz2DecompressorError(decoder)->iCode == XRT_BZ2_ERROR_DATA);
    xrtClearError();
    assert(!xrtBz2DecompressorUpdate(decoder, packed->Data, packed->Size));
    assert(!xrtBz2DecompressorFinish(decoder) && !xrtBz2DecompressorDrainBuffer(decoder));
    assert(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL);
    xrtBz2DecompressorDestroy(decoder);
    xrtBufferDestroy(packed);
    xrtClearError();
}

static void members_and_chunking(void)
{
    static const unsigned char text[] = "\xe4\xbd\xa0\xe5\xa5\xbd\0\xff";
    xrtbz2options options;
    xbuffer *packed = NULL, *empty = NULL, *chunk, *plain;
    xrtbz2decompressor* decoder;
    size_t i;
    unsigned int small;
    xrtBz2OptionsInit(&options);
    options.iWorkFactor = 0;
    assert(xrtBz2Compress(text, sizeof(text) - 1, &options, &packed, NULL));
    options.iWorkFactor = 250;
    assert(xrtBz2Compress(NULL, 0, &options, &empty, NULL));
    assert(xrtBufferAppend(packed, xrtBufferView(empty)));
    for (small = 0; small < 2; ++small) {
        options.bSmallMemory = small != 0;
        decoder = xrtBz2DecompressorCreate(&options);
        plain = xrtBufferCreate();
        assert(decoder && plain);
        for (i = 0; i < packed->Size; ++i) {
            assert(xrtBz2DecompressorUpdate(decoder, packed->Data + i, 1));
            chunk = xrtBz2DecompressorDrainBuffer(decoder);
            assert(chunk && xrtBufferAppend(plain, xrtBufferView(chunk)));
            xrtBufferDestroy(chunk);
        }
        assert(xrtBz2DecompressorFinish(decoder) && xrtBz2DecompressorMembers(decoder) == 2);
        assert(xrtBz2DecompressorInputBytes(decoder) == packed->Size);
        assert(xrtBz2DecompressorOutputBytes(decoder) == sizeof(text) - 1);
        assert(plain->Size == sizeof(text) - 1 && memcmp(plain->Data, text, plain->Size) == 0);
        xrtBz2DecompressorDestroy(decoder);
        xrtBufferDestroy(plain);
    }
    xrtBufferDestroy(packed);
    xrtBufferDestroy(empty);
}

int main(void)
{
    options_and_arguments();
    limits_and_failure_state();
    members_and_chunking();
    puts("bz2 C boundaries: options, limits, ownership, state and members passed");
    return 0;
}
