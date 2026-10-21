#include "../include/xrt_zstd.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "zstd C test failed at %d: %s\n", __LINE__, #expr); \
    return 1; \
} } while (0)

typedef struct progress_state {
    int calls;
    uint64 last_input;
    uint64 total;
    bool known;
    bool cancel;
} progress_state;

static bool record_progress(const xrtprogress *progress, ptr user_data) {
    progress_state *state = (progress_state *)user_data;
    state->calls++;
    state->last_input = progress->iInputBytes;
    state->total = progress->iTotalInputBytes;
    state->known = (progress->iFlags & XRT_PROGRESS_TOTAL_KNOWN) != 0u;
    return !state->cancel && (!state->known || state->last_input <= state->total);
}

typedef struct {
    const uint8* data;
    size_t size;
    size_t offset;
} unknown_reader;

static bool read_unknown(ptr context, ptr output, size_t requested, size_t* read) {
    unknown_reader* source = (unknown_reader*)context;
    size_t count = source->size - source->offset;
    if (count > requested) count = requested;
    if (count != 0u) {
        memcpy(output, source->data + source->offset, count);
        source->offset += count;
    }
    *read = count;
    return true;
}

/* 本次流的进度总量不包含 Reader 中已经跳过的前缀。 */
static int test_seeked_stream_progress(void) {
    static const unsigned char input[] = "XXXXXXXXXXabc";
    const uint64 prefix = 10u;
    xrtzstderror error;
    xreader* reader;
    xwriter* writer;
    xbuffer* packed = xrtBufferCreate();
    xbuffer* wrapped = xrtBufferCreate();
    xbuffer* plain = xrtBufferCreate();
    progress_state state = {0};
    uint64 position = 0u;
    CHECK(packed && wrapped && plain);
    reader = xrtReaderFromMemory((xbytesview){input, sizeof(input) - 1u});
    writer = xrtWriterFromBuffer(packed);
    CHECK(reader && writer && xrtReaderSeek(reader, (int64)prefix,
        XSEEK_START, &position) && position == prefix);
    CHECK(xrtZstdCompressStream(reader, writer, NULL, record_progress, &state, &error));
    CHECK(state.calls > 0 && state.known && state.last_input == 3u &&
        state.total == 3u);
    CHECK(xrtReaderDestroy(reader) && xrtWriterDestroy(writer));

    CHECK(xrtBufferAppend(wrapped, XRT_BYTES_LITERAL("XXXXXXXXXX")) &&
        xrtBufferAppend(wrapped, xrtBufferView(packed)));
    reader = xrtReaderFromMemory(xrtBufferView(wrapped));
    writer = xrtWriterFromBuffer(plain);
    position = 0u;
    state = (progress_state){0};
    CHECK(reader && writer && xrtReaderSeek(reader, (int64)prefix,
        XSEEK_START, &position) && position == prefix);
    CHECK(xrtZstdDecompressStream(reader, writer, NULL, record_progress, &state, &error));
    CHECK(state.calls > 0 && state.known && state.last_input == packed->Size &&
        state.total == packed->Size && plain->Size == 3u &&
        memcmp(plain->Data, "abc", 3u) == 0);
    CHECK(xrtReaderDestroy(reader) && xrtWriterDestroy(writer));
    xrtBufferDestroy(plain);
    xrtBufferDestroy(wrapped);
    xrtBufferDestroy(packed);

    /* 未知长度流仍能完成压缩，但不声明进度总量已知。 */
    {
        const xreaderops ops = {read_unknown, NULL, NULL, NULL, NULL};
        unknown_reader source = {(const uint8*)"abc", 3u, 0u};
        xbuffer* output = xrtBufferCreate();
        CHECK(output != NULL);
        reader = xrtReaderCreate(&ops, &source);
        writer = xrtWriterFromBuffer(output);
        state = (progress_state){0};
        CHECK(reader && writer && xrtZstdCompressStream(reader, writer,
            NULL, record_progress, &state, &error));
        CHECK(state.calls > 0 && state.last_input == 3u && !state.known &&
            state.total == 0u && xrtGetError() == NULL);
        CHECK(xrtReaderDestroy(reader) && xrtWriterDestroy(writer));
        xrtBufferDestroy(output);
    }
    return 0;
}

static int test_one_shot(void) {
    const unsigned char input[] = {'x', 'l', 'a', 'n', 'g', 0, 0xff};
    xbuffer *packed = NULL, *plain = NULL;
    xrtzstdoptions options;
    xrtzstderror error;
    xbytesview frame;

    CHECK(xrtZstdCompress(input, sizeof(input), NULL, &packed, &error));
    frame = xrtBufferView(packed);
    CHECK(frame.Size > 8u && memcmp(frame.Data, "\x28\xb5\x2f\xfd", 4u) == 0);
    CHECK(xrtZstdDecompress(frame.Data, frame.Size, NULL, &plain, &error));
    CHECK(xrtBufferView(plain).Size == sizeof(input));
    CHECK(memcmp(xrtBufferView(plain).Data, input, sizeof(input)) == 0);
    xrtBufferDestroy(plain);
    plain = NULL;

    xrtZstdOptionsInit(&options);
    options.tLimits.iMaxOutputBytes = sizeof(input) - 1u;
    CHECK(!xrtZstdDecompress(frame.Data, frame.Size, &options, &plain, &error));
    CHECK(plain == NULL && error.iCode == XRT_ZSTD_ERROR_LIMIT);
    CHECK(!xrtZstdDecompress(frame.Data, frame.Size - 1u, NULL, &plain, &error));
    CHECK(plain == NULL && error.iCode == XRT_ZSTD_ERROR_TRUNCATED);
    CHECK(!xrtZstdDecompress("not a frame", 11u, NULL, &plain, &error));
    CHECK(plain == NULL && error.iCode == XRT_ZSTD_ERROR_DATA);
    xrtBufferDestroy(packed);
    return 0;
}

static int test_external_frame(void) {
    /* Node.js 24 的 Zstd 标准帧，明文为六段 UTF-8 文本及 NUL、0xff、NUL。 */
    static const unsigned char frame[] = {
        0x28, 0xb5, 0x2f, 0xfd, 0x20, 0x7b, 0xf5, 0x00, 0x00, 0xb8,
        0xe4, 0xbd, 0xa0, 0xe5, 0xa5, 0xbd, 0x20, 0x68, 0x65, 0x6c,
        0x6c, 0x6f, 0x20, 0x7a, 0x73, 0x74, 0x64, 0x20, 0x7c, 0x20,
        0x00, 0xff, 0x00, 0x01, 0x00, 0xc2, 0xa5, 0x93, 0x09
    };
    static const char phrase[] = "\xe4\xbd\xa0\xe5\xa5\xbd hello zstd | ";
    unsigned char expected[6u * (sizeof(phrase) - 1u) + 3u];
    xbuffer *plain = NULL;
    xrtzstderror error;
    xrtzstdoptions options;
    size_t prefix = 6u * (sizeof(phrase) - 1u);
    size_t i;
    for (i = 0; i < 6u; ++i)
        memcpy(expected + i * (sizeof(phrase) - 1u), phrase, sizeof(phrase) - 1u);
    expected[prefix] = 0;
    expected[prefix + 1u] = 0xff;
    expected[prefix + 2u] = 0;
    CHECK(xrtZstdDecompress(frame, sizeof(frame), NULL, &plain, &error));
    CHECK(xrtBufferView(plain).Size == sizeof(expected));
    CHECK(memcmp(xrtBufferView(plain).Data, expected, sizeof(expected)) == 0);
    xrtBufferDestroy(plain);
    plain = NULL;

    xrtZstdOptionsInit(&options);
    options.tLimits.iMaxOutputBytes = sizeof(expected) - 1u;
    CHECK(!xrtZstdDecompress(frame, sizeof(frame), &options, &plain, &error));
    CHECK(plain == NULL && error.iCode == XRT_ZSTD_ERROR_LIMIT);
    return 0;
}

static int test_stream(void) {
    const char first[] = "hello ", second[] = "stream";
    xrtzstdcompressor *encoder = xrtZstdCompressorCreate(NULL);
    xrtzstddecompressor *decoder;
    xbuffer *packed, *plain;
    xbytesview frame;
    CHECK(encoder != NULL);
    CHECK(xrtZstdCompressorUpdate(encoder, first, sizeof(first) - 1u));
    CHECK(xrtZstdCompressorUpdate(encoder, second, sizeof(second) - 1u));
    CHECK(xrtZstdCompressorFinish(encoder));
    packed = xrtZstdCompressorTakeBuffer(encoder);
    CHECK(packed != NULL && xrtZstdCompressorInputBytes(encoder) == 12u);
    xrtZstdCompressorDestroy(encoder);

    frame = xrtBufferView(packed);
    decoder = xrtZstdDecompressorCreate(NULL);
    CHECK(decoder != NULL);
    CHECK(xrtZstdDecompressorUpdate(decoder, frame.Data, frame.Size / 2u));
    CHECK(xrtZstdDecompressorUpdate(decoder, frame.Data + frame.Size / 2u,
        frame.Size - frame.Size / 2u));
    CHECK(xrtZstdDecompressorFinish(decoder));
    CHECK(xrtZstdDecompressorMembers(decoder) == 1u);
    plain = xrtZstdDecompressorTakeBuffer(decoder);
    CHECK(plain != NULL && xrtBufferView(plain).Size == 12u);
    CHECK(memcmp(xrtBufferView(plain).Data, "hello stream", 12u) == 0);
    xrtBufferDestroy(plain);
    xrtZstdDecompressorDestroy(decoder);

    decoder = xrtZstdDecompressorCreate(NULL);
    CHECK(decoder != NULL);
    CHECK(xrtZstdDecompressorUpdate(decoder, frame.Data, frame.Size));
    CHECK(xrtZstdDecompressorFinish(decoder));
    CHECK(!xrtZstdDecompressorUpdate(decoder, frame.Data, frame.Size));
    CHECK(xrtZstdDecompressorError(decoder)->iCode == XRT_ZSTD_ERROR_STATE);
    CHECK(xrtZstdDecompressorInputBytes(decoder) == frame.Size);
    xrtZstdDecompressorDestroy(decoder);
    xrtBufferDestroy(packed);
    return 0;
}

static int test_frame_limits(void) {
    xbuffer *frame = NULL, *joined = xrtBufferCreate(), *plain = NULL;
    xrtzstdoptions options;
    xrtzstderror error;
    xrtzstddecompressor *decoder;
    xbytesview bytes;
    CHECK(joined != NULL);
    CHECK(xrtZstdCompress("abc", 3u, NULL, &frame, &error));
    bytes = xrtBufferView(frame);
    CHECK(xrtBufferAppend(joined, bytes) && xrtBufferAppend(joined, bytes));

    xrtZstdOptionsInit(&options);
    options.tLimits.iMaxEntries = 1u;
    CHECK(xrtZstdDecompress(bytes.Data, bytes.Size, &options, &plain, &error));
    xrtBufferDestroy(plain);
    plain = NULL;
    CHECK(!xrtZstdDecompress(joined->Data, joined->Size, &options, &plain, &error));
    CHECK(plain == NULL && error.iCode == XRT_ZSTD_ERROR_LIMIT);

    decoder = xrtZstdDecompressorCreate(&options);
    CHECK(decoder != NULL);
    CHECK(xrtZstdDecompressorUpdate(decoder, bytes.Data, bytes.Size));
    CHECK(xrtZstdDecompressorMembers(decoder) == 1u);
    CHECK(!xrtZstdDecompressorUpdate(decoder, bytes.Data, bytes.Size));
    CHECK(xrtZstdDecompressorError(decoder)->iCode == XRT_ZSTD_ERROR_LIMIT);
    xrtZstdDecompressorDestroy(decoder);

    options.tLimits.iMaxEntries = 2u;
    options.tLimits.iMaxItemBytes = 3u;
    CHECK(xrtZstdDecompress(joined->Data, joined->Size, &options, &plain, &error));
    CHECK(xrtBufferView(plain).Size == 6u &&
        memcmp(xrtBufferView(plain).Data, "abcabc", 6u) == 0);
    xrtBufferDestroy(plain);
    plain = NULL;
    options.tLimits.iMaxEntries = 0u;
    CHECK(xrtZstdDecompress(joined->Data, joined->Size, &options, &plain, &error));
    xrtBufferDestroy(plain);
    xrtBufferDestroy(joined);
    xrtBufferDestroy(frame);
    return 0;
}

static int test_stream_progress(void) {
    unsigned char source[70000];
    xbuffer *packed = xrtBufferCreate(), *plain = xrtBufferCreate();
    xreader *reader;
    xwriter *writer;
    xrtzstderror error;
    progress_state state = {0};
    size_t i;
    CHECK(packed != NULL && plain != NULL);
    for (i = 0; i < sizeof(source); i++) source[i] = (unsigned char)i;

    reader = xrtReaderFromMemory((xbytesview){source, sizeof(source)});
    writer = xrtWriterFromBuffer(packed);
    CHECK(reader != NULL && writer != NULL);
    CHECK(xrtZstdCompressStream(reader, writer, NULL, record_progress, &state, &error));
    CHECK(state.calls > 0 && state.last_input == sizeof(source));
    CHECK(xrtReaderDestroy(reader) && xrtWriterDestroy(writer));

    state.calls = 0;
    state.last_input = 0;
    reader = xrtReaderFromBuffer(packed);
    writer = xrtWriterFromBuffer(plain);
    CHECK(reader != NULL && writer != NULL);
    CHECK(xrtZstdDecompressStream(reader, writer, NULL, record_progress, &state, &error));
    CHECK(state.calls > 0 && state.last_input == xrtBufferView(packed).Size);
    CHECK(xrtReaderDestroy(reader) && xrtWriterDestroy(writer));
    CHECK(xrtBufferView(plain).Size == sizeof(source));
    CHECK(memcmp(xrtBufferView(plain).Data, source, sizeof(source)) == 0);

    state.cancel = true;
    reader = xrtReaderFromMemory((xbytesview){source, sizeof(source)});
    writer = xrtWriterFromBuffer(plain);
    CHECK(reader != NULL && writer != NULL);
    CHECK(!xrtZstdCompressStream(reader, writer, NULL, record_progress, &state, &error));
    CHECK(error.iCode == XRT_ZSTD_ERROR_CANCELLED);
    CHECK(xrtReaderDestroy(reader) && xrtWriterDestroy(writer));
    xrtBufferDestroy(plain);
    xrtBufferDestroy(packed);
    return 0;
}

/* 外层和嵌套限额结构的 ABI 均应在任何编解码前验证。 */
static int test_invalid_options(void) {
    xrtzstdoptions options;
    xrtzstdcompressor *encoder;
    xrtzstddecompressor *decoder;
    xrtzstderror error;
    xbuffer *output = NULL;
    xbuffer *sink;
    xreader *reader;
    xwriter *writer;
    int variant;
    xrtZstdOptionsInit(&options);
    CHECK(options.tLimits.iMaxNodes == 0u && options.tLimits.iMaxDepth == 0u);
    for (variant = 0; variant < 9; ++variant) {
        xrtZstdOptionsInit(&options);
        switch (variant) {
            case 0: options.iVersion++; break;
            case 1: options.iSize = sizeof(options) - 1u; break;
            case 2: options.tLimits.iVersion++; break;
            case 3: options.tLimits.iSize = 0u; break;
            case 4: options.sReserved[0] = 1u; break;
            case 5: options.tLimits.iReserved = 1u; break;
            case 6: options.tLimits.iFlags = 1u; break;
            case 7: options.tLimits.iMaxNodes = 1u; break;
            default: options.tLimits.iMaxDepth = 1u; break;
        }
        encoder = xrtZstdCompressorCreate(&options);
        CHECK(encoder != NULL && xrtZstdCompressorError(encoder)->iCode == XRT_ZSTD_ERROR_ARGUMENT);
        xrtZstdCompressorDestroy(encoder);
        decoder = xrtZstdDecompressorCreate(&options);
        CHECK(decoder != NULL && xrtZstdDecompressorError(decoder)->iCode == XRT_ZSTD_ERROR_ARGUMENT);
        xrtZstdDecompressorDestroy(decoder);
        CHECK(!xrtZstdCompress("x", 1u, &options, &output, &error));
        CHECK(output == NULL && error.iCode == XRT_ZSTD_ERROR_ARGUMENT);
        CHECK(!xrtZstdDecompress("x", 1u, &options, &output, &error));
        CHECK(output == NULL && error.iCode == XRT_ZSTD_ERROR_ARGUMENT);
        sink = xrtBufferCreate();
        CHECK(sink != NULL);
        reader = xrtReaderFromMemory((xbytesview){(const uint8 *)"x", 1u});
        writer = xrtWriterFromBuffer(sink);
        CHECK(reader != NULL && writer != NULL);
        CHECK(!xrtZstdCompressStream(reader, writer, &options, NULL, NULL, &error));
        CHECK(error.iCode == XRT_ZSTD_ERROR_ARGUMENT);
        CHECK(xrtReaderDestroy(reader) && xrtWriterDestroy(writer));
        reader = xrtReaderFromMemory((xbytesview){(const uint8 *)"x", 1u});
        writer = xrtWriterFromBuffer(sink);
        CHECK(reader != NULL && writer != NULL);
        CHECK(!xrtZstdDecompressStream(reader, writer, &options, NULL, NULL, &error));
        CHECK(error.iCode == XRT_ZSTD_ERROR_ARGUMENT);
        CHECK(xrtReaderDestroy(reader) && xrtWriterDestroy(writer));
        CHECK(xrtBufferView(sink).Size == 0u);
        xrtBufferDestroy(sink);
    }
    return 0;
}

int main(void) {
    CHECK(test_invalid_options() == 0);
    CHECK(test_one_shot() == 0);
    CHECK(test_external_frame() == 0);
    CHECK(test_stream() == 0);
    CHECK(test_frame_limits() == 0);
    CHECK(test_stream_progress() == 0);
    CHECK(test_seeked_stream_progress() == 0);
    puts("zstd C boundaries: PASS");
    return 0;
}
