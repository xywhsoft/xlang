#define XRT_MODULE_MEMORY_DEBUG
#ifndef XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_BUFFER
#endif
#ifndef XRT_MODULE_ONCE
#define XRT_MODULE_ONCE
#endif
#include "../include/lzma_native.h"
#include "../include/xrt_lzma.h"
#include "../third_party/xz/7zCrc.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "lzma C test failed at %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

typedef struct {
    unsigned calls;
    uint64 input;
    uint64 total;
} stream_progress;

static bool record_progress(const xrtprogress* progress, ptr user)
{
    stream_progress* state = (stream_progress*)user;
    if (progress->iSize != sizeof(*progress) ||
        progress->iVersion != XRT_PROGRESS_VERSION) return false;
    state->calls++;
    state->input = progress->iInputBytes;
    state->total = progress->iTotalInputBytes;
    return progress->iInputBytes <= progress->iTotalInputBytes;
}

static int test_stream_roundtrip(void)
{
    static const unsigned char input[] = {'x', 'l', 'a', 'n', 'g', 0, 0xff};
    xreader* reader = xrtReaderFromMemory((xbytesview){input, sizeof(input)});
    xbuffer* packed = xrtBufferCreate();
    xwriter* writer = packed != NULL ? xrtWriterFromBuffer(packed) : NULL;
    xbuffer* plain;
    xrtlzmaerror error;
    stream_progress progress = {0};
    CHECK(reader && writer);
    CHECK(xrtLzmaCompressStream(reader, writer, NULL,
        record_progress, &progress, &error));
    CHECK(progress.calls > 0u && progress.input == sizeof(input) &&
        progress.total == sizeof(input));
    CHECK(xrtReaderDestroy(reader) && xrtWriterDestroy(writer));

    reader = xrtReaderFromMemory(xrtBufferView(packed));
    plain = xrtBufferCreate();
    writer = plain != NULL ? xrtWriterFromBuffer(plain) : NULL;
    progress = (stream_progress){0};
    CHECK(reader && writer);
    CHECK(xrtLzmaDecompressStream(reader, writer, NULL,
        record_progress, &progress, &error));
    CHECK(progress.calls > 0u && progress.input == xrtBufferView(packed).Size &&
        progress.total == progress.input);
    CHECK(xrtReaderDestroy(reader) && xrtWriterDestroy(writer));
    CHECK(xrtBufferView(plain).Size == sizeof(input) &&
        memcmp(xrtBufferView(plain).Data, input, sizeof(input)) == 0);
    xrtBufferDestroy(plain);
    xrtBufferDestroy(packed);
    return 0;
}

static int test_stream_allocation_errors(void)
{
    static const unsigned char input[] = { 'x' };
    xreader* reader = xrtReaderFromMemory((xbytesview){ input, sizeof(input) });
    xbuffer* output = xrtBufferCreate();
    xwriter* writer = output != NULL ? xrtWriterFromBuffer(output) : NULL;
    xrtlzmaerror error;
    bool result;
    bool triggered;

    CHECK(reader && writer);
    CHECK(xrtMemDebugFailAfter(0u));
    result = xrtLzmaCompressStream(reader, writer, NULL, NULL, NULL, &error);
    triggered = xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    CHECK(triggered && !result && error.iCode == XRT_LZMA_ERROR_MEMORY);

    CHECK(xrtMemDebugFailAfter(0u));
    result = xrtLzmaDecompressStream(reader, writer, NULL, NULL, NULL, &error);
    triggered = xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    CHECK(triggered && !result && error.iCode == XRT_LZMA_ERROR_MEMORY);
    xrtReaderDestroy(reader);
    xrtWriterDestroy(writer);
    xrtBufferDestroy(output);
    return 0;
}

static int test_native_error_recovery(void)
{
    void* compressor = xlzma_compressor_create(5, 1024u, 1024u);
    void* decoder = xlzma_decompressor_create(1024u, 1024u, 1000u);
    xbuffer* packed;
    xbuffer* plain;
    CHECK(compressor && decoder);

    xrtSetErrorInfo(XERR_VALUE, "lzma.test", 1, "previous failure");
    CHECK(xlzma_compressor_update_str(compressor, "abc", 3u));
    CHECK(xrtGetError() == NULL);
    xrtSetErrorInfo(XERR_VALUE, "lzma.test", 1, "previous failure");
    CHECK(xlzma_compressor_finish(compressor));
    CHECK(xrtGetError() == NULL);
    xrtSetErrorInfo(XERR_VALUE, "lzma.test", 1, "previous failure");
    CHECK(xlzma_compressor_input_bytes(compressor) == 3u);
    CHECK(xrtGetError() == NULL);
    xrtSetErrorInfo(XERR_VALUE, "lzma.test", 1, "previous failure");
    CHECK(xlzma_compressor_output_bytes(compressor) > 0u);
    CHECK(xrtGetError() == NULL);
    xrtSetErrorInfo(XERR_VALUE, "lzma.test", 1, "previous failure");
    packed = xlzma_compressor_drain(compressor);
    CHECK(packed != NULL);
    CHECK(xrtGetError() == NULL);

    xrtSetErrorInfo(XERR_VALUE, "lzma.test", 1, "previous failure");
    CHECK(xlzma_decompressor_update(decoder, packed->Data, packed->Size));
    CHECK(xrtGetError() == NULL);
    xrtSetErrorInfo(XERR_VALUE, "lzma.test", 1, "previous failure");
    CHECK(xlzma_decompressor_finish(decoder));
    CHECK(xrtGetError() == NULL);
    xrtSetErrorInfo(XERR_VALUE, "lzma.test", 1, "previous failure");
    CHECK(xlzma_decompressor_input_bytes(decoder) == packed->Size);
    CHECK(xrtGetError() == NULL);
    xrtSetErrorInfo(XERR_VALUE, "lzma.test", 1, "previous failure");
    CHECK(xlzma_decompressor_output_bytes(decoder) == 3u);
    CHECK(xrtGetError() == NULL);
    xrtSetErrorInfo(XERR_VALUE, "lzma.test", 1, "previous failure");
    CHECK(xlzma_decompressor_finished(decoder));
    CHECK(xrtGetError() == NULL);
    xrtSetErrorInfo(XERR_VALUE, "lzma.test", 1, "previous failure");
    CHECK(xlzma_decompressor_members(decoder) == 1u);
    CHECK(xrtGetError() == NULL);
    xrtSetErrorInfo(XERR_VALUE, "lzma.test", 1, "previous failure");
    plain = xlzma_decompressor_drain(decoder);
    CHECK(plain != NULL && plain->Size == 3u && memcmp(plain->Data, "abc", 3u) == 0);
    CHECK(xrtGetError() == NULL);
    CHECK(!xlzma_compressor_update_str(compressor, "z", 1u));
    CHECK(xrtErrorKind(xrtGetError()) == XERR_STATE);
    CHECK(!xlzma_decompressor_update(decoder, "z", 1u));
    CHECK(xrtErrorKind(xrtGetError()) == XERR_STATE);

    xrtBufferDestroy(plain);
    xrtBufferDestroy(packed);
    xlzma_decompressor_destroy(decoder);
    xlzma_compressor_destroy(compressor);
    return 0;
}

static int test_native_null_handle(void)
{
    CHECK(!xlzma_compressor_update(NULL, "x", 1u));
    CHECK(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    CHECK(!xlzma_decompressor_finish(NULL));
    CHECK(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    CHECK(xlzma_decompressor_drain(NULL) == NULL);
    CHECK(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    return 0;
}

static int test_incremental_members(void)
{
	xrtlzmacompressor* pFirst = xrtLzmaCompressorCreate(NULL);
	xrtlzmacompressor* pSecond = xrtLzmaCompressorCreate(NULL);
	xrtlzmadecompressor* pDecoder = xrtLzmaDecompressorCreate(NULL);
	xbuffer* pEarly = NULL;
	xbuffer* pA = NULL;
	xbuffer* pB = NULL;
	xbuffer* pJoined = xrtBufferCreate();
	xbuffer* pPlain = NULL;
	xbytesview Joined;
	size_t i;
	CHECK(pFirst && pSecond && pDecoder && pJoined);
	CHECK(xrtLzmaCompressorUpdate(pFirst, "AAA", 3u));
	pEarly = xrtLzmaCompressorDrainBuffer(pFirst);
	CHECK(pEarly != NULL && xrtBufferView(pEarly).Size == 0u);
	xrtBufferDestroy(pEarly);
	CHECK(xrtLzmaCompressorFinish(pFirst));
	CHECK(xrtLzmaCompressorUpdate(pSecond, "BBB", 3u));
	CHECK(xrtLzmaCompressorFinish(pSecond));
	pA = xrtLzmaCompressorTakeBuffer(pFirst);
	pB = xrtLzmaCompressorTakeBuffer(pSecond);
	CHECK(pA && pB);
	CHECK(xrtBufferAppend(pJoined, xrtBufferView(pA)) &&
		xrtBufferAppend(pJoined, xrtBufferView(pB)));
	Joined = xrtBufferView(pJoined);
	for ( i = 0u; i < Joined.Size; ++i ) {
		CHECK(xrtLzmaDecompressorUpdate(pDecoder, Joined.Data + i, 1u));
	}
	CHECK(xrtLzmaDecompressorFinish(pDecoder));
	CHECK(xrtLzmaDecompressorMembers(pDecoder) == 2u);
	pPlain = xrtLzmaDecompressorTakeBuffer(pDecoder);
	CHECK(pPlain && xrtBufferView(pPlain).Size == 6u &&
		memcmp(xrtBufferView(pPlain).Data, "AAABBB", 6u) == 0);
	xrtBufferDestroy(pPlain);
	xrtBufferDestroy(pJoined);
	xrtBufferDestroy(pA);
	xrtBufferDestroy(pB);
	xrtLzmaDecompressorDestroy(pDecoder);
	xrtLzmaCompressorDestroy(pFirst);
	xrtLzmaCompressorDestroy(pSecond);
	return 0;
}

static int test_stream_padding(xbytesview frame, const unsigned char *input,
    size_t input_size)
{
    static const unsigned char padding[4] = {0, 0, 0, 0};
    static const unsigned char garbage[4] = {'j', 'u', 'n', 'k'};
    xbuffer *joined = xrtBufferCreate();
    xbuffer *plain = NULL;
    xrtlzmaerror error;
    CHECK(joined != NULL);

    CHECK(xrtBufferAppend(joined, frame) &&
        xrtBufferAppend(joined, (xbytesview){padding, sizeof(padding)}));
    CHECK(xrtLzmaDecompress(joined->Data, joined->Size, NULL, &plain, &error));
    CHECK(xrtBufferView(plain).Size == input_size &&
        memcmp(xrtBufferView(plain).Data, input, input_size) == 0);
    xrtBufferDestroy(plain);
    plain = NULL;

    CHECK(xrtBufferAppend(joined, frame));
    CHECK(xrtLzmaDecompress(joined->Data, joined->Size, NULL, &plain, &error));
    CHECK(xrtBufferView(plain).Size == input_size * 2u &&
        memcmp(xrtBufferView(plain).Data, input, input_size) == 0 &&
        memcmp(xrtBufferView(plain).Data + input_size, input, input_size) == 0);
    xrtBufferDestroy(plain);
    plain = NULL;

    xrtBufferClear(joined);
    CHECK(xrtBufferAppend(joined, frame) &&
        xrtBufferAppend(joined, (xbytesview){padding, 1u}));
    CHECK(!xrtLzmaDecompress(joined->Data, joined->Size, NULL, &plain, &error));
    CHECK(plain == NULL && error.iCode != XRT_LZMA_OK);

    xrtBufferClear(joined);
    CHECK(xrtBufferAppend(joined, frame) &&
        xrtBufferAppend(joined, (xbytesview){garbage, sizeof(garbage)}));
    CHECK(!xrtLzmaDecompress(joined->Data, joined->Size, NULL, &plain, &error));
    CHECK(plain == NULL && error.iCode != XRT_LZMA_OK);
    xrtBufferDestroy(joined);
    return 0;
}

/* 选项 ABI 校验应覆盖 XZ 的一次性、流对象和文件流入口。 */
static int test_invalid_options(void)
{
    xrtlzmaoptions options;
    xrtlzmaerror error;
    xrtlzmacompressor* encoder;
    xrtlzmadecompressor* decoder;
    xbuffer* output = NULL;
    xbuffer* sink;
    xreader* reader;
    xwriter* writer;
    int variant;
    xrtLzmaOptionsInit(&options);
    /* 单项限额保护字典分配；条目、节点和深度不属于 XZ 流。 */
    CHECK(options.tLimits.iMaxEntries == 0u && options.tLimits.iMaxNodes == 0u &&
        options.tLimits.iMaxDepth == 0u);
    for (variant = 0; variant < 10; ++variant) {
        xrtLzmaOptionsInit(&options);
        switch (variant) {
            case 0: options.iVersion++; break;
            case 1: options.iSize = sizeof(options) - 1u; break;
            case 2: options.tLimits.iVersion++; break;
            case 3: options.tLimits.iSize = 0u; break;
            case 4: options.sReserved[0] = 1u; break;
            case 5: options.tLimits.iReserved = 1u; break;
            case 6: options.tLimits.iFlags = 1u; break;
            case 7: options.tLimits.iMaxEntries = 1u; break;
            case 8: options.tLimits.iMaxNodes = 1u; break;
            default: options.tLimits.iMaxDepth = 1u; break;
        }
        encoder = xrtLzmaCompressorCreate(&options);
        CHECK(encoder != NULL &&
            xrtLzmaCompressorError(encoder)->iCode == XRT_LZMA_ERROR_ARGUMENT);
        xrtLzmaCompressorDestroy(encoder);
        decoder = xrtLzmaDecompressorCreate(&options);
        CHECK(decoder != NULL &&
            xrtLzmaDecompressorError(decoder)->iCode == XRT_LZMA_ERROR_ARGUMENT);
        xrtLzmaDecompressorDestroy(decoder);
        CHECK(!xrtLzmaCompress("x", 1u, &options, &output, &error));
        CHECK(output == NULL && error.iCode == XRT_LZMA_ERROR_ARGUMENT);
        CHECK(!xrtLzmaDecompress("x", 1u, &options, &output, &error));
        CHECK(output == NULL && error.iCode == XRT_LZMA_ERROR_ARGUMENT);
        sink = xrtBufferCreate();
        CHECK(sink != NULL);
        reader = xrtReaderFromMemory((xbytesview){ (const uint8*)"x", 1u });
        writer = xrtWriterFromBuffer(sink);
        CHECK(reader != NULL && writer != NULL);
        CHECK(!xrtLzmaCompressStream(reader, writer, &options,
            NULL, NULL, &error) && error.iCode == XRT_LZMA_ERROR_ARGUMENT);
        CHECK(xrtReaderDestroy(reader) && xrtWriterDestroy(writer));
        reader = xrtReaderFromMemory((xbytesview){ (const uint8*)"x", 1u });
        writer = xrtWriterFromBuffer(sink);
        CHECK(reader != NULL && writer != NULL);
        CHECK(!xrtLzmaDecompressStream(reader, writer, &options,
            NULL, NULL, &error) && error.iCode == XRT_LZMA_ERROR_ARGUMENT);
        CHECK(xrtReaderDestroy(reader) && xrtWriterDestroy(writer));
        CHECK(xrtBufferView(sink).Size == 0u);
        xrtBufferDestroy(sink);
    }
    return 0;
}

/* 小输入声明 1 GiB 字典时，先由默认分配限额拦截，而非尝试分配。 */
static int test_large_dictionary(xbuffer* packed)
{
    unsigned char saved[5];
    xbuffer* plain = NULL;
    xrtlzmaerror error;
    UInt32 crc;
    unsigned i;
    CHECK(packed->Size > 24u && packed->Data[12] == 2u &&
        packed->Data[13] == 0u && packed->Data[14] == 0x21u &&
        packed->Data[15] == 1u);
    saved[0] = packed->Data[16];
    memcpy(saved + 1, packed->Data + 20u, 4u);
    packed->Data[16] = 36u;
    crc = CrcCalc(packed->Data + 12u, 8u);
    for (i = 0u; i < 4u; ++i) {
        packed->Data[20u + i] = (unsigned char)(crc >> (i * 8u));
    }
    CHECK(!xrtLzmaDecompress(packed->Data, packed->Size, NULL, &plain, &error));
    CHECK(plain == NULL && error.iCode == XRT_LZMA_ERROR_LIMIT);
    packed->Data[16] = saved[0];
    memcpy(packed->Data + 20u, saved + 1, 4u);
    CHECK(xrtLzmaDecompress(packed->Data, packed->Size, NULL, &plain, &error));
    CHECK(plain != NULL && plain->Size == 7u);
    xrtBufferDestroy(plain);
    return 0;
}

int main(void)
{
    static const unsigned char input[] = {'x', 'l', 'a', 'n', 'g', 0, 0xff};
    xbuffer *packed = NULL, *plain = NULL;
    xrtlzmaoptions options;
    xrtlzmaerror error;
    xbytesview frame;
    unsigned char saved_property;

    CHECK(test_invalid_options() == 0);
    CHECK(test_stream_allocation_errors() == 0);
    CHECK(test_stream_roundtrip() == 0);
    CHECK(test_native_error_recovery() == 0);
    CHECK(test_native_null_handle() == 0);
    CHECK(test_incremental_members() == 0);

    CHECK(xrtLzmaCompress(input, sizeof(input), NULL, &packed, &error));
    frame = xrtBufferView(packed);
    CHECK(frame.Size > 12u);
    CHECK(memcmp(frame.Data, "\xfd" "7zXZ\0", 6u) == 0);
    CHECK(xrtLzmaDecompress(frame.Data, frame.Size, NULL, &plain, &error));
    CHECK(xrtBufferView(plain).Size == sizeof(input));
    CHECK(memcmp(xrtBufferView(plain).Data, input, sizeof(input)) == 0);
    xrtBufferDestroy(plain);
    plain = NULL;
    CHECK(test_stream_padding(frame, input, sizeof(input)) == 0);
    CHECK(test_large_dictionary(packed) == 0);

    xrtLzmaOptionsInit(&options);
    options.tLimits.iMaxOutputBytes = sizeof(input) - 1u;
    CHECK(!xrtLzmaDecompress(frame.Data, frame.Size, &options, &plain, &error));
    CHECK(plain == NULL && error.iCode == XRT_LZMA_ERROR_LIMIT);
    xrtLzmaOptionsInit(&options);
    options.tLimits.iMaxItemBytes = 1u;
    CHECK(!xrtLzmaDecompress(frame.Data, frame.Size, &options, &plain, &error));
    CHECK(plain == NULL && error.iCode == XRT_LZMA_ERROR_LIMIT);
    CHECK(!xrtLzmaDecompress(frame.Data, frame.Size - 1u, NULL, &plain, &error));
    CHECK(plain == NULL && error.iCode == XRT_LZMA_ERROR_TRUNCATED);

    /* 尾部 CRC 被篡改时不能泄露已解出的部分结果。 */
    packed->Data[frame.Size - 12u] ^= 1u;
    CHECK(!xrtLzmaDecompress(frame.Data, frame.Size, NULL, &plain, &error));
    CHECK(plain == NULL && error.iCode == XRT_LZMA_ERROR_DATA);
    packed->Data[frame.Size - 12u] ^= 1u;
    CHECK(xrtLzmaDecompress(frame.Data, frame.Size, NULL, &plain, &error));
    CHECK(xrtBufferView(plain).Size == sizeof(input) &&
        memcmp(xrtBufferView(plain).Data, input, sizeof(input)) == 0);
    xrtBufferDestroy(plain);
    plain = NULL;

    saved_property = packed->Data[0];
    packed->Data[0] = 0xffu;
    CHECK(!xrtLzmaDecompress(frame.Data, frame.Size, NULL, &plain, &error));
    CHECK(plain == NULL && error.iCode == XRT_LZMA_ERROR_DATA);
    packed->Data[0] = saved_property;
    xrtBufferDestroy(packed);
    puts("lzma C boundaries: PASS");
    return 0;
}
