/* 模块错误须保留 Reader/Writer 的首因，不能把 Memory/Range 等抹成 IO。 */
#define XRT_MODULE_MEMORY_DEBUG
#include "../include/xrt_miniz.h"
#include <stdio.h>
#include <string.h>

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "miniz IO failed at %d: %s\n", __LINE__, #test); return 1; \
} } while (0)

typedef struct {
    xerrkind kind;
    int code;
    unsigned calls;
    unsigned closes;
} failure;

static bool fail_read(ptr context, ptr output, size_t request, size_t* read)
{
    failure* state = (failure*)context;
    (void)output;
    (void)request;
    ++state->calls;
    *read = 0;
    if (state->kind != XERR_NONE)
        xrtSetErrorInfo(state->kind, "miniz.test.io", state->code, "original stream failure");
    return false;
}

static bool fail_write(ptr context, const void* data, size_t request, size_t* written)
{
    failure* state = (failure*)context;
    (void)data;
    (void)request;
    ++state->calls;
    *written = 0;
    if (state->kind != XERR_NONE)
        xrtSetErrorInfo(state->kind, "miniz.test.io", state->code, "original stream failure");
    return false;
}

static bool close_failure(ptr context)
{
    ++((failure*)context)->closes;
    return true;
}

static int check_error(const xrtminizerror* error, const failure* state, int expected,
    unsigned path, uint64 input, uint64 output)
{
    const xerror* cause = xrtGetError();
    if (error->iCode != expected || !cause || error->iBackendCode != xrtErrorCode(cause) ||
        strcmp(error->sMessage, xrtErrorMessage(cause)) != 0) {
        fprintf(stderr, "path=%u kind=%d expected=%d actual=%d backend=%d message=%s\n",
            path, (int)state->kind, expected, error->iCode, error->iBackendCode, error->sMessage);
        return 1;
    }
    CHECK(error->iInputOffset == input && error->iOutputOffset == output);
    if (state->kind != XERR_NONE) {
        CHECK(xrtErrorKind(cause) == state->kind && xrtErrorCode(cause) == state->code);
        CHECK(strcmp(xrtErrorDomain(cause), "miniz.test.io") == 0);
    } else CHECK(xrtErrorKind(cause) == XERR_IO);
    CHECK(state->calls == 1 && state->closes == 0);
    return 0;
}

static int check_path(unsigned path, xerrkind kind, int expected)
{
    static const xreaderops reader_ops = {fail_read, NULL, NULL, NULL, close_failure};
    static const xwriterops writer_ops = {fail_write, NULL, NULL, NULL, NULL, close_failure};
    static const unsigned char packed[] = {0x78, 0x9c, 0x4b, 0x4c, 0x4a, 0x06, 0, 2, 0x4d, 1, 0x27};
    failure state = {kind, 7100 + (int)kind, 0, 0};
    xmemdebugsnapshot before, after;
    xrtminizerror error;
    xreader* reader = NULL;
    xwriter* writer = NULL;
    xbuffer* sink = NULL;
    xrtminizcompressor* encoder = NULL;
    xrtminizdecompressor* decoder = NULL;
    uint64 input = 0, output = 0;
    bool result;
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    if (path < 2) {
        reader = xrtReaderCreate(&reader_ops, &state);
        sink = xrtBufferCreate();
        writer = sink ? xrtWriterFromBuffer(sink) : NULL;
        CHECK(reader && writer);
        result = path == 0 ? xrtMinizCompressStream(reader, writer, NULL, NULL, NULL, &error) :
            xrtMinizDecompressStream(reader, writer, NULL, NULL, NULL, &error);
        CHECK(!result && sink->Size == 0);
    } else {
        writer = xrtWriterCreate(&writer_ops, &state);
        CHECK(writer);
        if (path == 2) {
            encoder = xrtMinizCompressorCreate(NULL);
            CHECK(encoder && xrtMinizCompressorUpdate(encoder, "abc", 3) && xrtMinizCompressorFinish(encoder));
            input = xrtMinizCompressorInputBytes(encoder);
            output = xrtMinizCompressorOutputBytes(encoder);
            CHECK(!xrtMinizCompressorDrain(encoder, writer));
            error = *xrtMinizCompressorError(encoder);
            CHECK(!xrtMinizCompressorFinish(encoder));
        } else {
            decoder = xrtMinizDecompressorCreate(NULL);
            CHECK(decoder && xrtMinizDecompressorUpdate(decoder, packed, sizeof(packed)) &&
                xrtMinizDecompressorFinish(decoder));
            input = xrtMinizDecompressorInputBytes(decoder);
            output = xrtMinizDecompressorOutputBytes(decoder);
            CHECK(!xrtMinizDecompressorDrain(decoder, writer));
            error = *xrtMinizDecompressorError(decoder);
            CHECK(!xrtMinizDecompressorFinish(decoder));
        }
    }
    int failed = check_error(&error, &state, expected, path, input, output);
    xrtMinizCompressorDestroy(encoder);
    xrtMinizDecompressorDestroy(decoder);
    CHECK(xrtReaderDestroy(reader) && xrtWriterDestroy(writer));
    CHECK(state.closes == 1);
    xrtBufferDestroy(sink);
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    CHECK(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    return failed;
}

/* 真实 Buffer Writer 扩容失败，不依赖人工设置的错误种类。 */
static int check_drain_oom(bool decode)
{
    static const unsigned char packed[] = {0x78, 0x9c, 0x4b, 0x4c, 0x4a, 0x06, 0, 2, 0x4d, 1, 0x27};
    xmemdebugsnapshot before, after;
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    xbuffer* sink = xrtBufferCreate();
    xwriter* writer = sink ? xrtWriterFromBuffer(sink) : NULL;
    xrtminizcompressor* encoder = NULL;
    xrtminizdecompressor* decoder = NULL;
    CHECK(writer);
    if (decode) {
        decoder = xrtMinizDecompressorCreate(NULL);
        CHECK(decoder && xrtMinizDecompressorUpdate(decoder, packed, sizeof(packed)) &&
            xrtMinizDecompressorFinish(decoder));
    } else {
        encoder = xrtMinizCompressorCreate(NULL);
        CHECK(encoder && xrtMinizCompressorUpdate(encoder, "abc", 3) && xrtMinizCompressorFinish(encoder));
    }
    CHECK(xrtMemDebugFailAfter(0));
    bool result = decode ? xrtMinizDecompressorDrain(decoder, writer) : xrtMinizCompressorDrain(encoder, writer);
    bool triggered = xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    const xrtminizerror* error = decode ? xrtMinizDecompressorError(decoder) : xrtMinizCompressorError(encoder);
    CHECK(triggered && !result && sink->Size == 0 && error->iCode == XRT_MINIZ_ERROR_MEMORY);
    CHECK(xrtErrorKind(xrtGetError()) == XERR_MEMORY && error->iBackendCode == xrtErrorCode(xrtGetError()));
    CHECK(strcmp(error->sMessage, xrtErrorMessage(xrtGetError())) == 0);
    CHECK(xrtWriterDestroy(writer));
    xrtMinizCompressorDestroy(encoder);
    xrtMinizDecompressorDestroy(decoder);
    xrtBufferDestroy(sink);
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    CHECK(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    return 0;
}

int main(void)
{
    static const struct { xerrkind kind; int expected; } cases[] = {
        {XERR_ARGUMENT, XRT_MINIZ_ERROR_ARGUMENT}, {XERR_MEMORY, XRT_MINIZ_ERROR_MEMORY},
        {XERR_RANGE, XRT_MINIZ_ERROR_LIMIT}, {XERR_STATE, XRT_MINIZ_ERROR_STATE},
        {XERR_CANCELLED, XRT_MINIZ_ERROR_CANCELLED}, {XERR_IO, XRT_MINIZ_ERROR_IO},
        {XERR_NONE, XRT_MINIZ_ERROR_IO}
    };
    int failed = 0;
    for (unsigned path = 0; path < 4; ++path) {
        for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
            failed |= check_path(path, cases[i].kind, cases[i].expected);
    }
    failed |= check_drain_oom(false);
    failed |= check_drain_oom(true);
    if (!failed) puts("miniz IO error provenance: PASS (28 cases and real writer OOM)");
    return failed;
}

#ifdef __TINYC__
#include "../xrt_miniz.c"
#endif
