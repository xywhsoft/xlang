/* 自定义 Reader/Writer 是公开接口；真实扩容 OOM 另测，不伪装系统 IO。 */
#define XRT_MODULE_MEMORY_DEBUG
#include "../include/xrt_snappy.h"
#include <stdio.h>
#include <string.h>

static const unsigned char abc[] = {
    255,6,0,0,'s','N','a','P','p','Y', 1,7,0,0,110,87,241,33,'a','b','c'
};
static unsigned failures, cases;
#define EXPECT(value) do { if (!(value)) { ++failures; \
    fprintf(stderr, "snappy IO line=%d: %s\n", __LINE__, #value); } } while (0)
typedef struct { xerrkind Kind; unsigned Calls, Closes; const xerror *Cause; } failure;

static void set_cause(failure *state) {
    ++state->Calls;
    if (state->Kind != XERR_NONE) {
        xrtSetErrorInfo(state->Kind, "test.snappy.io", 7431, "original stream failure");
        state->Cause = xrtGetError();
    }
}
static bool fail_read(ptr context, ptr output, size_t request, size_t *read) {
    (void)output; (void)request; *read = 0;
    set_cause(context); return false;
}
static bool fail_write(ptr context, const void *data, size_t request, size_t *written) {
    (void)data; (void)request; *written = 0;
    set_cause(context); return false;
}
static bool close_stream(ptr context) { ++((failure*)context)->Closes; return true; }
static bool balanced(const xmemdebugsnapshot *a, const xmemdebugsnapshot *b) {
    return a->LiveCount == b->LiveCount && a->LiveBytes == b->LiveBytes &&
        a->InvalidFreeCount == b->InvalidFreeCount && a->DoubleFreeCount == b->DoubleFreeCount &&
        a->OverflowCount == b->OverflowCount && a->UnderflowCount == b->UnderflowCount &&
        a->UseAfterFreeCount == b->UseAfterFreeCount;
}

static void check_error(const xrtsnappyerror *error, const failure *state, int expected,
    unsigned path, uint64 input, uint64 output) {
    const xerror *cause = xrtGetError();
    bool matches = cause && error->iCode == expected && error->iBackendCode == xrtErrorCode(cause) &&
        !strcmp(error->sMessage, xrtErrorMessage(cause));
    if (!matches) fprintf(stderr, "snappy IO path=%u kind=%d expected=%d actual=%d backend=%d message=%s\n",
        path, state->Kind, expected, error->iCode, error->iBackendCode, error->sMessage);
    EXPECT(matches);
    EXPECT(error->iInputOffset == input && error->iOutputOffset == output);
    EXPECT(state->Calls == 1 && state->Closes == 0);
    if (state->Kind != XERR_NONE) {
        EXPECT(cause == state->Cause);
        EXPECT(cause && xrtErrorKind(cause) == state->Kind && xrtErrorCode(cause) == 7431 &&
            !strcmp(xrtErrorDomain(cause), "test.snappy.io"));
    } else EXPECT(cause && xrtErrorKind(cause) == XERR_IO);
}

static void check_path(unsigned path, xerrkind kind, int expected) {
    static const xreaderops reader_ops = {fail_read, NULL, NULL, NULL, close_stream};
    static const xwriterops writer_ops = {fail_write, NULL, NULL, NULL, NULL, close_stream};
    failure state = {kind, 0, 0, NULL};
    xreader *reader = NULL; xwriter *writer = NULL; xbuffer *sink = NULL;
    xrtsnappycompressor *encoder = NULL; xrtsnappydecompressor *decoder = NULL;
    xrtsnappyerror error; xmemdebugsnapshot before, after;
    uint64 input = 0, output = 0;
    bool ready = false;
    ++cases; xrtClearError(); xrtMemDebugSnapshot(&before); xrtSnappyErrorInit(&error);
    if (path < 2) {
        reader = xrtReaderCreate(&reader_ops, &state);
        sink = xrtBufferCreate(); writer = sink ? xrtWriterFromBuffer(sink) : NULL;
        if (!reader || !writer) goto cleanup;
        bool result = path == 0 ? xrtSnappyCompressStream(reader, writer, NULL, NULL, NULL, &error) :
            xrtSnappyDecompressStream(reader, writer, NULL, NULL, NULL, &error);
        EXPECT(!result && sink->Size == 0);
        /* 压缩器已生成但尚未 drain 的 identifier 也计入输出；并非已写出字节。 */
        output = path == 0 ? 10 : 0;
    } else {
        writer = xrtWriterCreate(&writer_ops, &state);
        if (!writer) goto cleanup;
        if (path == 2) {
            encoder = xrtSnappyCompressorCreate(NULL);
            if (!encoder || !xrtSnappyCompressorUpdate(encoder, "abc", 3) || !xrtSnappyCompressorFinish(encoder)) goto cleanup;
            input = xrtSnappyCompressorInputBytes(encoder); output = xrtSnappyCompressorOutputBytes(encoder);
            EXPECT(!xrtSnappyCompressorDrain(encoder, writer));
            error = *xrtSnappyCompressorError(encoder);
            EXPECT(!xrtSnappyCompressorFinish(encoder));
        } else {
            decoder = xrtSnappyDecompressorCreate(NULL);
            if (!decoder || !xrtSnappyDecompressorUpdate(decoder, abc, sizeof(abc)) || !xrtSnappyDecompressorFinish(decoder)) goto cleanup;
            input = xrtSnappyDecompressorInputBytes(decoder); output = xrtSnappyDecompressorOutputBytes(decoder);
            EXPECT(!xrtSnappyDecompressorDrain(decoder, writer));
            error = *xrtSnappyDecompressorError(decoder);
            EXPECT(!xrtSnappyDecompressorFinish(decoder));
        }
    }
    ready = true;
    check_error(&error, &state, expected, path, input, output);
cleanup:
    EXPECT(ready);
    xrtSnappyCompressorDestroy(encoder); xrtSnappyDecompressorDestroy(decoder);
    EXPECT(xrtReaderDestroy(reader)); EXPECT(xrtWriterDestroy(writer));
    EXPECT(state.Closes == 1); xrtBufferDestroy(sink);
    xrtClearError(); xrtMemDebugSnapshot(&after); EXPECT(balanced(&before, &after));
}

static void check_oom(bool decode) {
    xmemdebugsnapshot before, after;
    xrtsnappycompressor *encoder = NULL; xrtsnappydecompressor *decoder = NULL;
    xbuffer *sink = NULL, *packed = NULL, *plain = NULL; xwriter *writer = NULL;
    xrtsnappyerror recovery; const xrtsnappyerror *error;
    bool ready = false;
    ++cases; xrtClearError(); xrtMemDebugSnapshot(&before);
    sink = xrtBufferCreate(); writer = sink ? xrtWriterFromBuffer(sink) : NULL;
    if (!writer) goto cleanup;
    if (decode) {
        decoder = xrtSnappyDecompressorCreate(NULL);
        if (!decoder || !xrtSnappyDecompressorUpdate(decoder, abc, sizeof(abc)) || !xrtSnappyDecompressorFinish(decoder)) goto cleanup;
    } else {
        encoder = xrtSnappyCompressorCreate(NULL);
        if (!encoder || !xrtSnappyCompressorUpdate(encoder, "abc", 3) || !xrtSnappyCompressorFinish(encoder)) goto cleanup;
    }
    if (!xrtMemDebugFailAfter(0)) goto cleanup;
    bool result = decode ? xrtSnappyDecompressorDrain(decoder, writer) : xrtSnappyCompressorDrain(encoder, writer);
    bool triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    error = decode ? xrtSnappyDecompressorError(decoder) : xrtSnappyCompressorError(encoder);
    EXPECT(triggered && !result && sink->Size == 0 && error->iCode == XRT_SNAPPY_ERROR_MEMORY);
    EXPECT(xrtErrorKind(xrtGetError()) == XERR_MEMORY && error->iBackendCode == xrtErrorCode(xrtGetError()) &&
        !strcmp(error->sMessage, xrtErrorMessage(xrtGetError())));
    xrtClearError();
    ready = xrtSnappyCompress("abc", 3, NULL, &packed, &recovery) &&
        xrtSnappyDecompress(packed->Data, packed->Size, NULL, &plain, &recovery);
    EXPECT(ready && plain->Size == 3 && !memcmp(plain->Data, "abc", 3));
cleanup:
    xrtMemDebugFailClear(); EXPECT(ready);
    EXPECT(xrtWriterDestroy(writer)); xrtBufferDestroy(sink); xrtBufferDestroy(packed); xrtBufferDestroy(plain);
    xrtSnappyCompressorDestroy(encoder); xrtSnappyDecompressorDestroy(decoder);
    xrtClearError(); xrtMemDebugSnapshot(&after); EXPECT(balanced(&before, &after));
}

int main(void) {
    static const struct { xerrkind Kind; int Expected; } errors[] = {
        {XERR_ARGUMENT,XRT_SNAPPY_ERROR_ARGUMENT}, {XERR_MEMORY,XRT_SNAPPY_ERROR_MEMORY},
        {XERR_RANGE,XRT_SNAPPY_ERROR_LIMIT}, {XERR_STATE,XRT_SNAPPY_ERROR_STATE},
        {XERR_CANCELLED,XRT_SNAPPY_ERROR_CANCELLED}, {XERR_IO,XRT_SNAPPY_ERROR_IO}, {XERR_NONE,XRT_SNAPPY_ERROR_IO}
    };
    if (!(xrtMemDebugEnabled() || xrtMemDebugEnable(true))) return 2;
    for (unsigned path = 0; path < 4; ++path)
        for (unsigned i = 0; i < sizeof(errors) / sizeof(errors[0]); ++i) check_path(path, errors[i].Kind, errors[i].Expected);
    check_oom(false); check_oom(true);
    printf("snappy IO: %u cases, %u assertion failures\n", cases, failures);
    return failures ? 1 : 0;
}

#ifdef __TINYC__
#include "../third_party/xrt_snappy.c"
#include "../xrt_snappy.c"
#endif
