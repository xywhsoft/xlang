#include "../include/xrt_gzip.h"
/* 测试操作位于断言中，发布配置也必须执行。 */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void equal(xbuffer* buffer, const void* data, size_t size)
{
    assert(buffer && buffer->Size == size);
    assert(size == 0 || memcmp(buffer->Data, data, size) == 0);
}

static void roundtrips(void)
{
    unsigned char data[100003];
    xrtgzipoptions options;
    xrtgziperror error;
    int level, strategy;
    size_t i;
    for (i = 0; i < sizeof(data); ++i) data[i] = (unsigned char)(i * 31u + i / 17u);
    xrtGzipOptionsInit(&options);
    options.iMTime = 0x12345678u;
    for (level = -1; level <= 9; ++level) {
        for (strategy = XDEFLATE_STRATEGY_DEFAULT; strategy <= XDEFLATE_STRATEGY_FIXED; ++strategy) {
            xbuffer *packed = NULL, *plain = NULL;
            options.iLevel = level;
            options.iStrategy = strategy;
            assert(xrtGzipCompress(data, sizeof(data), &options, &packed, &error));
            assert(error.iCode == XRT_GZIP_OK && packed->Size >= 18);
            assert(memcmp(packed->Data, "\x1f\x8b\x08", 3) == 0);
            assert(memcmp(packed->Data + 4, "\x78\x56\x34\x12", 4) == 0);
            assert(xrtGzipDecompress(packed->Data, packed->Size, NULL, &plain, &error));
            equal(plain, data, sizeof(data));
            xrtBufferDestroy(plain);
            xrtBufferDestroy(packed);
        }
    }
    {
        xbuffer *packed = NULL, *plain = NULL;
        assert(xrtGzipCompress(NULL, 0, NULL, &packed, NULL));
        assert(xrtGzipDecompress(packed->Data, packed->Size, NULL, &plain, NULL));
        equal(plain, NULL, 0);
        xrtBufferDestroy(plain);
        xrtBufferDestroy(packed);
    }
}

/* 每一个截断点、所有头部和尾部关键位、完整首成员后的坏数据。 */
static void malformed(void)
{
    xbuffer *packed = NULL, *plain = NULL, *second = NULL;
    xrtgziperror error;
    size_t i, first;
    assert(xrtGzipCompress("payload", 7, NULL, &packed, NULL));
    for (i = 0; i < packed->Size; ++i) {
        assert(!xrtGzipDecompress(packed->Data, i, NULL, &plain, &error));
        assert(plain == NULL && error.iCode != XRT_GZIP_OK && xrtGetError());
    }
    for (i = 0; i < 12; ++i) {
        size_t position = i < 4 ? i : packed->Size - 12 + i;
        unsigned char bit = i == 3 ? 0x80 : 1;
        packed->Data[position] ^= bit;
        assert(!xrtGzipDecompress(packed->Data, packed->Size, NULL, &plain, &error));
        assert(!plain && error.iCode != XRT_GZIP_OK);
        packed->Data[position] ^= bit;
    }
    assert(xrtGzipCompress("second", 6, NULL, &second, NULL));
    first = packed->Size;
    assert(xrtBufferAppend(packed, xrtBufferView(second)));
    assert(xrtGzipDecompress(packed->Data, packed->Size, NULL, &plain, NULL));
    equal(plain, "payloadsecond", 13);
    xrtBufferDestroy(plain);
    for (i = first + 1; i < packed->Size; ++i) {
        assert(!xrtGzipDecompress(packed->Data, i, NULL, &plain, &error));
        assert(!plain);
    }
    assert(xrtBufferAppend(packed, (xbytesview){(const uint8*)"junk", 4}));
    assert(!xrtGzipDecompress(packed->Data, packed->Size, NULL, &plain, NULL));
    xrtBufferDestroy(packed);
    xrtBufferDestroy(second);
}

static void limits_and_arguments(void)
{
    xrtgzipoptions options;
    xrtgziperror error;
    xbuffer *packed = NULL, *plain = NULL;
    char data[8192] = {0};
    xrtGzipOptionsInit(&options);
    options.iSize--;
    assert(!xrtGzipCompressorCreate(&options) && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    xrtGzipOptionsInit(&options);
    options.iVersion++;
    assert(!xrtGzipDecompressorCreate(&options));
    xrtGzipOptionsInit(&options);
    options.tLimits.iVersion++;
    assert(!xrtGzipCompressorCreate(&options));
    xrtGzipOptionsInit(&options);
    assert(options.tLimits.iMaxItemBytes == 0 && options.tLimits.iMaxEntries == 0 &&
        options.tLimits.iMaxNodes == 0 && options.tLimits.iMaxDepth == 0);
    options.tLimits.iFlags = 1;
    assert(!xrtGzipCompressorCreate(&options) && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(!xrtGzipDecompressorCreate(&options) && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    xrtGzipOptionsInit(&options);
    options.tLimits.iMaxItemBytes = 1;
    assert(!xrtGzipCompressorCreate(&options) && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    xrtGzipOptionsInit(&options);
    options.tLimits.iMaxEntries = 1;
    assert(!xrtGzipDecompressorCreate(&options) && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    xrtGzipOptionsInit(&options);
    options.tLimits.iMaxNodes = 1;
    assert(!xrtGzipCompressorCreate(&options) && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    xrtGzipOptionsInit(&options);
    options.tLimits.iMaxDepth = 1;
    assert(!xrtGzipDecompressorCreate(&options) && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    xrtGzipOptionsInit(&options);
    options.iStrategy = 99;
    assert(!xrtGzipCompressorCreate(&options));
    assert(!xrtGzipCompress(NULL, 1, NULL, &plain, &error) && error.iCode == XRT_GZIP_ERROR_ARGUMENT);
    assert(!xrtGzipCompress("x", 1, NULL, NULL, &error) && error.iCode == XRT_GZIP_ERROR_ARGUMENT);
    assert(!xrtGzipCompressorUpdate(NULL, "x", 1));
    assert(!xrtGzipDecompressorFinish(NULL));
    assert(!xrtGzipCompressStream(NULL, NULL, NULL, NULL, NULL, &error));
    xrtGzipOptionsInit(&options);
    options.tLimits.iMaxInputBytes = 2;
    assert(!xrtGzipCompress("abc", 3, &options, &plain, &error));
    assert(!plain && error.iCode == XRT_GZIP_ERROR_LIMIT);
    assert(xrtGzipCompress(data, sizeof(data), NULL, &packed, NULL));
    xrtGzipOptionsInit(&options);
    options.tLimits.iMaxOutputBytes = sizeof(data) - 1;
    assert(!xrtGzipDecompress(packed->Data, packed->Size, &options, &plain, &error));
    assert(!plain && error.iCode == XRT_GZIP_ERROR_LIMIT);
    options.tLimits.iMaxOutputBytes++;
    assert(xrtGzipDecompress(packed->Data, packed->Size, &options, &plain, NULL));
    xrtBufferDestroy(plain);
    options.tLimits.iMaxCompressionRatio = 2;
    assert(!xrtGzipDecompress(packed->Data, packed->Size, &options, &plain, &error));
    assert(!plain && error.iCode == XRT_GZIP_ERROR_LIMIT);
    xrtBufferDestroy(packed);
}

/* 所有二段切分位置都可解码；输出在每个字节后移交也不丢失头尾或统计。 */
static void incremental(void)
{
    const char text[] = "incremental \xe4\xbd\xa0\xe5\xa5\xbd";
    xrtgzipcompressor* c = xrtGzipCompressorCreate(NULL);
    xbuffer* packed = xrtBufferCreate();
    size_t i;
    assert(c && packed && !xrtGzipCompressorFinished(c));
    assert(!xrtGzipCompressorTakeBuffer(c));
    assert(!xrtGzipCompressorUpdate(c, NULL, 1));
    assert(xrtGzipCompressorUpdate(c, NULL, 0));
    for (i = 0; i < sizeof(text) - 1; ++i) {
        xbuffer* piece;
        assert(xrtGzipCompressorUpdate(c, text + i, 1));
        piece = xrtGzipCompressorDrainBuffer(c);
        assert(piece && xrtBufferAppend(packed, xrtBufferView(piece)));
        xrtBufferDestroy(piece);
    }
    assert(xrtGzipCompressorFinish(c) && xrtGzipCompressorFinish(c));
    assert(xrtGzipCompressorFinished(c));
    {
        xbuffer* piece = xrtGzipCompressorTakeBuffer(c);
        assert(piece && xrtBufferAppend(packed, xrtBufferView(piece)));
        xrtBufferDestroy(piece);
    }
    assert(!xrtGzipCompressorTakeBuffer(c) && !xrtGzipCompressorUpdate(c, "x", 1));
    assert(xrtGzipCompressorInputBytes(c) == sizeof(text) - 1);
    assert(xrtGzipCompressorOutputBytes(c) == packed->Size);
    xrtGzipCompressorDestroy(c);
    for (i = 0; i <= packed->Size; ++i) {
        xrtgzipdecompressor* d = xrtGzipDecompressorCreate(NULL);
        xbuffer *plain, *rest;
        assert(d && xrtGzipDecompressorUpdate(d, packed->Data, i));
        plain = xrtGzipDecompressorDrainBuffer(d);
        assert(plain && xrtGzipDecompressorUpdate(d, packed->Data + i, packed->Size - i));
        assert(xrtGzipDecompressorFinish(d) && xrtGzipDecompressorFinish(d));
        rest = xrtGzipDecompressorTakeBuffer(d);
        assert(rest && xrtBufferAppend(plain, xrtBufferView(rest)));
        equal(plain, text, sizeof(text) - 1);
        assert(xrtGzipDecompressorInputBytes(d) == packed->Size);
        assert(xrtGzipDecompressorOutputBytes(d) == sizeof(text) - 1);
        assert(xrtGzipDecompressorFinished(d));
        xrtGzipDecompressorDestroy(d);
        xrtBufferDestroy(rest);
        xrtBufferDestroy(plain);
    }
    xrtBufferDestroy(packed);
}

int main(void)
{
    roundtrips();
    malformed();
    limits_and_arguments();
    incremental();
    xrtClearError();
    puts("gzip C: roundtrip, malformed, limits, state and split boundaries passed");
    return 0;
}
