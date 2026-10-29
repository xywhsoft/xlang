#include "../include/xrt_lz4.h"
/* 测试调用位于断言中，发布配置也必须执行。 */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void invalid_options(const xrtlz4options* options)
{
    assert(xrtLz4CompressorCreate(options) == NULL);
    assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(xrtLz4DecompressorCreate(options) == NULL);
    assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
}

static void arguments_and_state(void)
{
    xrtlz4options options, bad;
    xrtlz4error error;
    xbuffer *packed = NULL, *plain = NULL;
    xrtlz4compressor* c;
    xrtlz4decompressor* d;
    unsigned i;
    xrtLz4OptionsInit(NULL);
    xrtLz4ErrorInit(NULL);
    xrtLz4OptionsInit(&options);
    assert(options.iAcceleration == 1);
    /* maxEntries 是帧数限额；单项、节点和深度不属于 LZ4 帧合同。 */
    assert(options.tLimits.iMaxItemBytes == 0 && options.tLimits.iMaxNodes == 0 &&
        options.tLimits.iMaxDepth == 0);
    for (i = 0; i < 4; ++i) {
        bad = options;
        if (i == 0) bad.tLimits.iFlags = 1;
        if (i == 1) bad.tLimits.iMaxItemBytes = 1;
        if (i == 2) bad.tLimits.iMaxNodes = 1;
        if (i == 3) bad.tLimits.iMaxDepth = 1;
        invalid_options(&bad);
    }
    bad = options; bad.iSize--; invalid_options(&bad);
    bad = options; bad.iVersion++; invalid_options(&bad);
    bad = options; bad.sReserved[3] = 1; invalid_options(&bad);
    bad = options; bad.tLimits.iSize--; invalid_options(&bad);
    bad = options; bad.tLimits.iVersion++; invalid_options(&bad);
    bad = options; bad.tLimits.iReserved = 1; invalid_options(&bad);
    bad = options; bad.iAcceleration = 0; invalid_options(&bad);
    bad = options; bad.iAcceleration = 65538; invalid_options(&bad);
    assert(!xrtLz4Compress(NULL, 1, NULL, &packed, &error));
    assert(packed == NULL && error.iCode == XRT_LZ4_ERROR_ARGUMENT);
    assert(!xrtLz4Compress("x", 1, NULL, NULL, &error));
    assert(error.iCode == XRT_LZ4_ERROR_ARGUMENT);
    assert(!xrtLz4Decompress(NULL, 0, NULL, &plain, &error));
    assert(plain == NULL && error.iCode == XRT_LZ4_ERROR_TRUNCATED);
    assert(!xrtLz4CompressorUpdate(NULL, NULL, 0));
    assert(!xrtLz4DecompressorFinish(NULL));
    assert(!xrtLz4CompressorDrain(NULL, NULL) && !xrtLz4DecompressorDrain(NULL, NULL));
    assert(xrtLz4CompressorError(NULL) == NULL && xrtLz4DecompressorError(NULL) == NULL);
    assert(xrtLz4CompressorInputBytes(NULL) == 0 && xrtLz4CompressorOutputBytes(NULL) == 0);
    assert(xrtLz4DecompressorInputBytes(NULL) == 0 && xrtLz4DecompressorOutputBytes(NULL) == 0);
    assert(!xrtLz4DecompressorFinished(NULL) && xrtLz4DecompressorMembers(NULL) == 0);
    xrtLz4CompressorDestroy(NULL);
    xrtLz4DecompressorDestroy(NULL);

    // 参数错误不消费输入，修正后可继续；take 转移的缓冲独立于原对象。
    c = xrtLz4CompressorCreate(NULL);
    assert(c && !xrtLz4CompressorUpdate(c, NULL, 1));
    assert(xrtLz4CompressorInputBytes(c) == 0);
    assert(!xrtLz4CompressorTakeBuffer(c) && !xrtLz4CompressorDrain(c, NULL));
    assert(xrtLz4CompressorUpdate(c, NULL, 0));
    assert(xrtLz4CompressorFinish(c) && xrtLz4CompressorFinish(c));
    assert(!xrtLz4CompressorUpdate(c, "late", 4));
    packed = xrtLz4CompressorTakeBuffer(c);
    assert(packed && packed->Size >= 15);
    assert(memcmp(packed->Data, "\x04\x22\x4d\x18", 4) == 0);
    assert(!xrtLz4CompressorTakeBuffer(c) && !xrtLz4CompressorDrainBuffer(c));
    xrtLz4CompressorDestroy(c);
    d = xrtLz4DecompressorCreate(NULL);
    assert(d && xrtLz4DecompressorUpdate(d, packed->Data, packed->Size));
    assert(xrtLz4DecompressorMembers(d) == 1 && xrtLz4DecompressorFinished(d));
    assert(xrtLz4DecompressorFinish(d) && xrtLz4DecompressorFinish(d));
    plain = xrtLz4DecompressorTakeBuffer(d);
    assert(plain && plain->Size == 0);
    assert(!xrtLz4DecompressorUpdate(d, NULL, 0) && !xrtLz4DecompressorTakeBuffer(d));
    xrtLz4DecompressorDestroy(d);
    xrtBufferDestroy(packed);
    xrtBufferDestroy(plain);
    xrtClearError();
}

static void append_skip(xbuffer* stream, uint8 variant, size_t length)
{
    uint8 header[8] = {0x50, 0x2a, 0x4d, 0x18, 0, 0, 0, 0};
    uint8 padding[256] = {0};
    size_t left = length;
    unsigned int i;
    header[0] += variant;
    for (i = 0; i < 4; ++i) header[4 + i] = (uint8)(length >> (i * 8));
    assert(xrtBufferAppend(stream, (xbytesview){header, sizeof(header)}));
    while (left) {
        size_t n = left > sizeof(padding) ? sizeof(padding) : left;
        assert(xrtBufferAppend(stream, (xbytesview){padding, n}));
        left -= n;
    }
}

static void frames_and_chunking(void)
{
    static const uint8 text[] = "\xe4\xbd\xa0\xe5\xa5\xbd\0\xff";
    static const size_t chunks[] = {1, 2, 3, 4, 5, 7, 8, 13, 64, 65536};
    xbuffer *packed = NULL, *empty = NULL, *stream = xrtBufferCreate(), *plain = NULL;
    xrtlz4options options;
    xrtlz4error error;
    size_t index;
    assert(stream && xrtLz4Compress(text, sizeof(text) - 1, NULL, &packed, NULL));
    assert(xrtLz4Compress(NULL, 0, NULL, &empty, NULL));
    append_skip(stream, 0, 0);
    assert(xrtBufferAppend(stream, xrtBufferView(packed)));
    append_skip(stream, 15, 259);
    assert(xrtBufferAppend(stream, xrtBufferView(empty)));
    append_skip(stream, 7, 0);
    for (index = 0; index < sizeof(chunks) / sizeof(chunks[0]); ++index) {
        xrtlz4decompressor* d = xrtLz4DecompressorCreate(NULL);
        size_t offset = 0;
        plain = xrtBufferCreate();
        assert(d && plain);
        while (offset < stream->Size) {
            size_t n = stream->Size - offset;
            xbuffer* part;
            if (n > chunks[index]) n = chunks[index];
            assert(xrtLz4DecompressorUpdate(d, stream->Data + offset, n));
            part = xrtLz4DecompressorDrainBuffer(d);
            assert(part && xrtBufferAppend(plain, xrtBufferView(part)));
            xrtBufferDestroy(part);
            offset += n;
        }
        assert(xrtLz4DecompressorFinished(d) && xrtLz4DecompressorFinish(d));
        assert(xrtLz4DecompressorMembers(d) == 2);
        assert(xrtLz4DecompressorInputBytes(d) == stream->Size);
        assert(xrtLz4DecompressorOutputBytes(d) == sizeof(text) - 1);
        assert(plain->Size == sizeof(text) - 1 && memcmp(plain->Data, text, plain->Size) == 0);
        xrtBufferDestroy(plain);
        xrtLz4DecompressorDestroy(d);
    }
    xrtLz4OptionsInit(&options);
    options.tLimits.iMaxEntries = 5;
    assert(xrtLz4Decompress(stream->Data, stream->Size, &options, &plain, &error));
    xrtBufferDestroy(plain);
    options.tLimits.iMaxEntries = 4;
    assert(!xrtLz4Decompress(stream->Data, stream->Size, &options, &plain, &error));
    assert(!plain && error.iCode == XRT_LZ4_ERROR_LIMIT);
    xrtBufferClear(stream);
    for (index = 0; index < 16; ++index) append_skip(stream, (uint8)index, index);
    assert(xrtLz4Decompress(stream->Data, stream->Size, NULL, &plain, &error));
    assert(plain && plain->Size == 0);
    xrtBufferDestroy(plain);
    xrtBufferDestroy(stream);
    xrtBufferDestroy(packed);
    xrtBufferDestroy(empty);
}

static void limits_and_corruption(void)
{
    static uint8 data[131073];
    xrtlz4options options;
    xrtlz4error error;
    xbuffer *packed = NULL, *small = NULL, *plain = NULL, *padded = xrtBufferCreate();
    xrtlz4compressor* c;
    xrtlz4decompressor* d;
    size_t i;
    memset(data, 'a', sizeof(data));
    xrtLz4OptionsInit(&options);
    options.iAcceleration = 65537;
    assert(xrtLz4Compress(data, sizeof(data), &options, &packed, &error));
    options.tLimits.iMaxInputBytes = packed->Size;
    options.tLimits.iMaxOutputBytes = sizeof(data);
    assert(xrtLz4Decompress(packed->Data, packed->Size, &options, &plain, &error));
    assert(plain->Size == sizeof(data) && memcmp(plain->Data, data, plain->Size) == 0);
    xrtBufferDestroy(plain);
    options.tLimits.iMaxInputBytes--;
    assert(!xrtLz4Decompress(packed->Data, packed->Size, &options, &plain, &error));
    assert(!plain && error.iCode == XRT_LZ4_ERROR_LIMIT);
    options.tLimits.iMaxInputBytes = 0;
    options.tLimits.iMaxOutputBytes--;
    assert(!xrtLz4Decompress(packed->Data, packed->Size, &options, &plain, &error));
    assert(!plain && error.iCode == XRT_LZ4_ERROR_LIMIT);

    // 跳过帧和后续输入不能稀释单帧解压倍率。
    options.tLimits.iMaxOutputBytes = 0;
    options.tLimits.iMaxCompressionRatio = 1;
    assert(padded);
    append_skip(padded, 0, sizeof(data));
    assert(xrtBufferAppend(padded, xrtBufferView(packed)));
    append_skip(padded, 0, sizeof(data));
    assert(!xrtLz4Decompress(padded->Data, padded->Size, &options, &plain, &error));
    assert(!plain && error.iCode == XRT_LZ4_ERROR_LIMIT);
    xrtBufferDestroy(padded);

    xrtLz4OptionsInit(&options);
    options.tLimits.iMaxInputBytes = 2;
    c = xrtLz4CompressorCreate(&options);
    assert(c && xrtLz4CompressorUpdate(c, "ab", 2));
    assert(!xrtLz4CompressorUpdate(c, "c", 1));
    assert(xrtLz4CompressorInputBytes(c) == 2);
    assert(!xrtLz4CompressorFinish(c) && !xrtLz4CompressorDrainBuffer(c));
    assert(xrtLz4CompressorError(c)->iCode == XRT_LZ4_ERROR_LIMIT);
    xrtLz4CompressorDestroy(c);
    options.tLimits.iMaxOutputBytes = 1;
    assert(xrtLz4CompressorCreate(&options) == NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    assert(xrtLz4Compress("checksum", 8, NULL, &small, NULL));
    for (i = 0; i < small->Size; ++i) {
        assert(!xrtLz4Decompress(small->Data, i, NULL, &plain, &error));
        assert(!plain && error.iCode == XRT_LZ4_ERROR_TRUNCATED);
    }
    for (i = 0; i < small->Size; ++i) {
        uint8 saved = small->Data[i];
        small->Data[i] ^= 1;
        assert(!xrtLz4Decompress(small->Data, small->Size, NULL, &plain, &error));
        assert(!plain && xrtGetError());
        small->Data[i] = saved;
    }
    assert(xrtBufferAppend(small, (xbytesview){(const uint8*)"x", 1}));
    assert(!xrtLz4Decompress(small->Data, small->Size, NULL, &plain, &error));
    assert(!plain && error.iCode == XRT_LZ4_ERROR_TRUNCATED);
    d = xrtLz4DecompressorCreate(NULL);
    assert(d && !xrtLz4DecompressorUpdate(d, "junk!", 5));
    xrtClearError();
    assert(!xrtLz4DecompressorUpdate(d, packed->Data, packed->Size));
    assert(!xrtLz4DecompressorFinished(d) && !xrtLz4DecompressorFinish(d));
    assert(!xrtLz4DecompressorDrainBuffer(d) && xrtErrorKind(xrtGetError()) == XERR_PROTOCOL);
    xrtLz4DecompressorDestroy(d);
    xrtBufferDestroy(small);
    xrtBufferDestroy(packed);
    xrtClearError();
}

int main(void)
{
    arguments_and_state();
    frames_and_chunking();
    limits_and_corruption();
    puts("lz4 C boundaries: options, state, limits, frames, chunks and corruption passed");
    return 0;
}
