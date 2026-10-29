#include "../include/xrt_brotli.h"
#include "../include/brotli_native.h"
#include "../third_party/include/brotli/encode.h"
#include "../third_party/include/brotli/decode.h"
/* 测试操作位于断言中，发布配置也必须执行。 */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

static xrtbrotlioptions options(void)
{
    xrtbrotlioptions result;
    xrtBrotliOptionsInit(&result);
    result.iQuality = 4;
    return result;
}

static xbuffer* compress(const void* data, size_t size)
{
    xrtbrotlioptions config = options();
    xrtbrotlierror error;
    xbuffer* result = NULL;
    assert(xrtBrotliCompress(data, size, &config, &result, &error));
    assert(result != NULL && error.iCode == XRT_BROTLI_OK);
    return result;
}

static void same(xbuffer* buffer, const void* data, size_t size)
{
    xbytesview view = xrtBufferView(buffer);
    assert(view.Size == size && (size == 0 || memcmp(view.Data, data, size) == 0));
}

/* 双向使用原版后端；避免只有同一私有格式自洽的假互操作测试。 */
static void interoperability(const unsigned char* data, size_t size)
{
    xbuffer* packed = compress(data, size);
    xbytesview bytes = xrtBufferView(packed);
    xbuffer* plain = NULL;
    xrtbrotlierror error;
    size_t capacity = BrotliEncoderMaxCompressedSize(size);
    size_t length = size + 1;
    unsigned char* encoded = xrtMalloc(capacity);
    unsigned char* decoded = xrtMalloc(length);
    assert(encoded && decoded);
    assert(BrotliDecoderDecompress(bytes.Size, bytes.Data, &length, decoded) == BROTLI_DECODER_RESULT_SUCCESS);
    assert(length == size && memcmp(decoded, data, size) == 0);
    assert(BrotliEncoderCompress(4, 22, BROTLI_MODE_GENERIC, size, data, &capacity, encoded));
    assert(xrtBrotliDecompress(encoded, capacity, NULL, &plain, &error));
    same(plain, data, size);
    xrtBufferDestroy(plain);
    xrtBufferDestroy(packed);
    xrtFree(encoded);
    xrtFree(decoded);
}

/* 穷举小流的全部二分边界，并验证 drain 转移不影响状态机和累计计数。 */
static void split_stream(void)
{
    static const unsigned char text[] = {'a', 0, 255, 0xe4, 0xbd, 0xa0, 'z'};
    xbuffer* packed = compress(text, sizeof(text));
    xbytesview bytes = xrtBufferView(packed);
    size_t split;
    for (split = 0; split <= bytes.Size; ++split) {
        xrtbrotlidecompressor* d = xrtBrotliDecompressorCreate(NULL);
        xbuffer* all = xrtBufferCreate();
        xbuffer* chunk;
        assert(d && all);
        assert(xrtBrotliDecompressorUpdate(d, bytes.Data, split));
        chunk = xrtBrotliDecompressorDrainBuffer(d);
        assert(chunk && xrtBufferAppend(all, xrtBufferView(chunk)));
        xrtBufferDestroy(chunk);
        assert(xrtBrotliDecompressorUpdate(d, bytes.Data + split, bytes.Size - split));
        assert(xrtBrotliDecompressorFinished(d));
        assert(xrtBrotliDecompressorFinish(d) && xrtBrotliDecompressorFinish(d));
        chunk = xrtBrotliDecompressorTakeBuffer(d);
        assert(chunk && xrtBufferAppend(all, xrtBufferView(chunk)));
        same(all, text, sizeof(text));
        assert(xrtBrotliDecompressorInputBytes(d) == bytes.Size);
        assert(xrtBrotliDecompressorOutputBytes(d) == sizeof(text));
        assert(xrtBrotliDecompressorMembers(d) == 1);
        assert(!xrtBrotliDecompressorTakeBuffer(d));
        assert(xrtBrotliDecompressorError(d)->iCode == XRT_BROTLI_ERROR_STATE);
        xrtBufferDestroy(chunk);
        xrtBufferDestroy(all);
        xrtBrotliDecompressorDestroy(d);
    }
    for (split = 0; split < bytes.Size; ++split) {
        xbuffer* output = NULL;
        xrtbrotlierror error;
        assert(!xrtBrotliDecompress(bytes.Data, split, NULL, &output, &error));
        assert(output == NULL && error.iCode == XRT_BROTLI_ERROR_TRUNCATED);
    }
    xrtBufferDestroy(packed);
}

static void incremental_encode(void)
{
    static const char text[] = "incremental encoder";
    xrtbrotlioptions config = options();
    xrtbrotlicompressor* c = xrtBrotliCompressorCreate(&config);
    xbuffer* all = xrtBufferCreate();
    xbuffer* chunk;
    xbuffer* plain = NULL;
    xrtbrotlierror error;
    size_t i;
    for (i = 0; i < sizeof(text) - 1; ++i) {
        assert(xrtBrotliCompressorUpdate(c, text + i, 1));
        chunk = xrtBrotliCompressorDrainBuffer(c);
        assert(chunk && xrtBufferAppend(all, xrtBufferView(chunk)));
        xrtBufferDestroy(chunk);
    }
    assert(xrtBrotliCompressorUpdate(c, NULL, 0));
    assert(xrtBrotliCompressorFinish(c) && xrtBrotliCompressorFinish(c));
    chunk = xrtBrotliCompressorTakeBuffer(c);
    assert(chunk && xrtBufferAppend(all, xrtBufferView(chunk)));
    assert(xrtBrotliCompressorInputBytes(c) == sizeof(text) - 1);
    assert(xrtBrotliCompressorOutputBytes(c) == xrtBufferView(all).Size);
    assert(xrtBrotliDecompress(xrtBufferView(all).Data, xrtBufferView(all).Size, NULL, &plain, &error));
    same(plain, text, sizeof(text) - 1);
    assert(!xrtBrotliCompressorUpdate(c, "x", 1));
    assert(xrtBrotliCompressorError(c)->iCode == XRT_BROTLI_ERROR_STATE);
    xrtBufferDestroy(plain);
    xrtBufferDestroy(chunk);
    xrtBufferDestroy(all);
    xrtBrotliCompressorDestroy(c);
}

static void limits_and_members(void)
{
    unsigned char text[4096];
    xbuffer* packed;
    xbuffer* output = NULL;
    xbuffer* members = xrtBufferCreate();
    xrtbrotlioptions config = options();
    xrtbrotlierror error;
    xrtbrotlicompressor* c;
    xrtbrotlidecompressor* d;
    size_t i;

    /* 通用限额中的归档/树字段对 Brotli 无意义，不得静默接受。 */
    assert(config.tLimits.iMaxItemBytes == 0 && config.tLimits.iMaxEntries == 0 &&
        config.tLimits.iMaxNodes == 0 && config.tLimits.iMaxDepth == 0);
    for (i = 0; i < 5; ++i) {
        config = options();
        if (i == 0) config.tLimits.iFlags = 1;
        if (i == 1) config.tLimits.iMaxItemBytes = 1;
        if (i == 2) config.tLimits.iMaxEntries = 1;
        if (i == 3) config.tLimits.iMaxNodes = 1;
        if (i == 4) config.tLimits.iMaxDepth = 1;
        c = xrtBrotliCompressorCreate(&config);
        assert(c && xrtBrotliCompressorError(c)->iCode == XRT_BROTLI_ERROR_ARGUMENT);
        xrtBrotliCompressorDestroy(c);
        d = xrtBrotliDecompressorCreate(&config);
        assert(d && xrtBrotliDecompressorError(d)->iCode == XRT_BROTLI_ERROR_ARGUMENT);
        xrtBrotliDecompressorDestroy(d);
    }
    config = options();
    memset(text, 'x', sizeof(text));
    packed = compress(text, sizeof(text));
    assert(xrtBufferAppend(members, xrtBufferView(packed)));
    assert(xrtBufferAppend(members, xrtBufferView(packed)));
    d = xrtBrotliDecompressorCreate(NULL);
    assert(xrtBrotliDecompressorUpdate(d, xrtBufferView(members).Data, xrtBufferView(members).Size));
    assert(xrtBrotliDecompressorFinish(d) && xrtBrotliDecompressorMembers(d) == 2);
    output = xrtBrotliDecompressorTakeBuffer(d);
    assert(xrtBufferView(output).Size == sizeof(text) * 2);
    xrtBufferDestroy(output);
    xrtBrotliDecompressorDestroy(d);
    assert(!xrtBrotliDecompress(xrtBufferView(members).Data,
        xrtBufferView(members).Size - 1, NULL, &output, &error));
    assert(error.iCode == XRT_BROTLI_ERROR_TRUNCATED && output == NULL);
    config.tLimits.iMaxInputBytes = sizeof(text) - 1;
    assert(!xrtBrotliCompress(text, sizeof(text), &config, &output, &error));
    assert(error.iCode == XRT_BROTLI_ERROR_LIMIT);
    config = options();
    config.tLimits.iMaxOutputBytes = sizeof(text) - 1;
    assert(!xrtBrotliDecompress(xrtBufferView(packed).Data, xrtBufferView(packed).Size,
        &config, &output, &error));
    assert(error.iCode == XRT_BROTLI_ERROR_LIMIT && output == NULL);
    config = options();
    config.tLimits.iMaxCompressionRatio = 2;
    d = xrtBrotliDecompressorCreate(&config);
    for (i = 0; i < xrtBufferView(packed).Size; ++i) {
        if (!xrtBrotliDecompressorUpdate(d, xrtBufferView(packed).Data + i, 1)) break;
    }
    assert(i < xrtBufferView(packed).Size && xrtBrotliDecompressorError(d)->iCode == XRT_BROTLI_ERROR_LIMIT);
    xrtBrotliDecompressorDestroy(d);
    config = options();
    config.iQuality = -1;
    assert(!xrtBrotliCompress(text, 1, &config, &output, &error));
    assert(error.iCode == XRT_BROTLI_ERROR_ARGUMENT && output == NULL);
    config = options();
    config.iVersion++;
    assert(!xrtBrotliDecompress(xrtBufferView(packed).Data, xrtBufferView(packed).Size, &config, &output, &error));
    assert(error.iCode == XRT_BROTLI_ERROR_ARGUMENT);
    config = options();
    config.tLimits.iReserved = 1;
    assert(!xrtBrotliCompress(text, 1, &config, &output, &error));
    assert(error.iCode == XRT_BROTLI_ERROR_ARGUMENT && output == NULL);
    d = xrtBrotliDecompressorCreate(&config);
    assert(d && xrtBrotliDecompressorError(d)->iCode == XRT_BROTLI_ERROR_ARGUMENT);
    xrtBrotliDecompressorDestroy(d);
    assert(!xrtBrotliCompress(NULL, 1, NULL, &output, &error));
    assert(error.iCode == XRT_BROTLI_ERROR_ARGUMENT);
    assert(!xrtBrotliCompress(text, 1, NULL, NULL, &error));
    assert(error.iCode == XRT_BROTLI_ERROR_ARGUMENT);
    xrtBufferDestroy(packed);
    xrtBufferDestroy(members);
}

typedef struct Progress {
    uint64 total;
    uint64 last;
    unsigned int calls;
    bool cancel;
    bool final;
} Progress;

static bool progress(const xrtprogress* event, ptr context)
{
    Progress* state = context;
    assert(event->iTotalInputBytes == state->total && event->iInputBytes >= state->last);
    state->last = event->iInputBytes;
    state->calls++;
    if (event->iFlags & XRT_PROGRESS_FINAL) state->final = true;
    return !state->cancel;
}

static void streams(void)
{
    static const char text[] = "skip:reader writer progress";
    xbuffer* packed = xrtBufferCreate();
    xreader* reader = xrtReaderFromMemory((xbytesview){(cbytes)text, sizeof(text) - 1});
    xwriter* writer = xrtWriterFromBuffer(packed);
    xrtbrotlioptions config = options();
    xrtbrotlierror error;
    Progress state = {sizeof(text) - 6, 0, 0, false, false};
    uint64 position;
    xbuffer* plain = NULL;
    assert(reader && writer && xrtReaderSeek(reader, 5, XSEEK_START, &position));
    config.iWindow = 9;
    assert(!xrtBrotliCompressStream(reader, writer, &config, progress, &state, &error));
    assert(error.iCode == XRT_BROTLI_ERROR_ARGUMENT);
    assert(xrtReaderTell(reader, &position) && position == 5 && state.calls == 0);
    config = options();
    config.tLimits.iReserved = 1;
    assert(!xrtBrotliCompressStream(reader, writer, &config, progress, &state, &error));
    assert(error.iCode == XRT_BROTLI_ERROR_ARGUMENT);
    assert(xrtReaderTell(reader, &position) && position == 5 && state.calls == 0);
    config = options();
    assert(xrtBrotliCompressStream(reader, writer, &config, progress, &state, &error));
    assert(state.calls >= 1 && state.final && state.last == state.total);
    assert(xrtReaderDestroy(reader) && xrtWriterDestroy(writer));
    assert(xrtBrotliDecompress(xrtBufferView(packed).Data, xrtBufferView(packed).Size, NULL, &plain, &error));
    same(plain, text + 5, sizeof(text) - 6);
    xrtBufferDestroy(plain);
    reader = xrtReaderFromBuffer(packed);
    writer = xrtWriterDiscard();
    state = (Progress){xrtBufferView(packed).Size, 0, 0, true, false};
    assert(!xrtBrotliDecompressStream(reader, writer, NULL, progress, &state, &error));
    assert(error.iCode == XRT_BROTLI_ERROR_CANCELLED && state.calls == 1);
    assert(xrtReaderDestroy(reader) && xrtWriterDestroy(writer));
    reader = xrtReaderFromBuffer(packed);
    writer = xrtWriterFromMemory(NULL, 0);
    assert(!xrtBrotliDecompressStream(reader, writer, NULL, NULL, NULL, &error));
    assert(error.iCode == XRT_BROTLI_ERROR_IO);
    xrtClearError();
    assert(xrtReaderDestroy(reader) && xrtWriterDestroy(writer));
    xrtBufferDestroy(packed);
}

static void owned_text(void)
{
    const unsigned char content[] = {'A', 0, 'B', 0xe4, 0xbd, 0xa0};
    const unsigned char invalid[] = {0xff};
    xbuffer* packed = NULL;
    xbuffer* result;
    void* original;
    char* text;
    size_t length;

    packed = xbrotli_compress((void*)content, sizeof(content), 4, 22, 0, 0, 0);
    assert(packed);
    result = xbrotli_decompress_text(packed->Data, packed->Size, 0, 0, 0);
    assert(result);
    length = xbrotli_text_size(result);
    original = result->Data;
    text = xbrotli_text_take(result);
    assert(text == original && length == sizeof(content));
    assert(memcmp(text, content, length) == 0 && text[length] == 0);
    xrtFree(text);
    xrtBufferDestroy(packed);

    packed = xbrotli_compress(NULL, 0, 4, 22, 0, 0, 0);
    assert(packed);
    result = xbrotli_decompress_text(packed->Data, packed->Size, 0, 0, 0);
    assert(result && xbrotli_text_size(result) == 0);
    text = xbrotli_text_take(result);
    assert(text && text[0] == 0);
    xrtFree(text);
    xrtBufferDestroy(packed);

    packed = xbrotli_compress((void*)invalid, sizeof(invalid), 4, 22, 0, 0, 0);
    assert(packed);
    assert(xbrotli_decompress_text(packed->Data, packed->Size, 0, 0, 0) == NULL);
    assert(xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtClearError();
    xrtBufferDestroy(packed);
}

int main(void)
{
    static const size_t sizes[] = {0, 1, 2, 7, 255, 65535, 65536, 65537, 131101};
    unsigned char* data = xrtMalloc(sizes[8]);
    unsigned int state = 7;
    size_t i;
    assert(data);
    for (i = 0; i < sizes[8]; ++i) {
        state = state * 1664525u + 1013904223u;
        data[i] = (unsigned char)(state >> 24);
    }
    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) interoperability(data, sizes[i]);
    split_stream();
    incremental_encode();
    limits_and_members();
    streams();
    owned_text();
    xrtFree(data);
    xrtClearError();
    puts("brotli C: interoperability, splits, limits, states, streams passed");
    return 0;
}
