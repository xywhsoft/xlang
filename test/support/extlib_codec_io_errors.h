/* 自定义 Reader/Writer 是公开接口；真实扩容 OOM 另测，不伪装系统 IO。 */
#include <stdio.h>
#include <string.h>

static const unsigned char abc[] = CODEC_IO_FRAME_BYTES;
static unsigned failures, cases;
#define EXPECT(value) do { if (!(value)) { ++failures; \
    fprintf(stderr, CODEC_IO_NAME " IO line=%d: %s\n", __LINE__, #value); } } while (0)
typedef struct { xerrkind Kind; unsigned Calls, Closes; const xerror *Cause; } failure;

static void set_cause(failure *state) {
    ++state->Calls;
    if (state->Kind != XERR_NONE) {
        xrtSetErrorInfo(state->Kind, "test." CODEC_IO_NAME ".io", 7431, "original stream failure");
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

static void check_error(const CODEC_IO_ERROR *error, const failure *state, int expected,
    unsigned path, uint64 input, uint64 output) {
    const xerror *cause = xrtGetError();
    bool matches = cause && error->iCode == expected && error->iBackendCode == xrtErrorCode(cause) &&
        !strcmp(error->sMessage, xrtErrorMessage(cause));
    if (!matches) fprintf(stderr, CODEC_IO_NAME " IO path=%u kind=%d expected=%d actual=%d backend=%d message=%s\n",
        path, state->Kind, expected, error->iCode, error->iBackendCode, error->sMessage);
    EXPECT(matches);
    EXPECT(error->iInputOffset == input && error->iOutputOffset == output);
    EXPECT(state->Calls == 1 && state->Closes == 0);
    if (state->Kind != XERR_NONE) {
        EXPECT(cause == state->Cause);
        EXPECT(cause && xrtErrorKind(cause) == state->Kind && xrtErrorCode(cause) == 7431 &&
            !strcmp(xrtErrorDomain(cause), "test." CODEC_IO_NAME ".io"));
    } else EXPECT(cause && xrtErrorKind(cause) == XERR_IO);
}

static void check_path(unsigned path, xerrkind kind, int expected) {
    static const xreaderops reader_ops = {fail_read, NULL, NULL, NULL, close_stream};
    static const xwriterops writer_ops = {fail_write, NULL, NULL, NULL, NULL, close_stream};
    failure state = {kind, 0, 0, NULL};
    xreader *reader = NULL; xwriter *writer = NULL; xbuffer *sink = NULL;
    CODEC_IO_COMPRESSOR *encoder = NULL; CODEC_IO_DECOMPRESSOR *decoder = NULL;
    CODEC_IO_ERROR error; xmemdebugsnapshot before, after;
    uint64 input = 0, output = 0;
    bool ready = false;
    ++cases; xrtClearError(); xrtMemDebugSnapshot(&before); CODEC_IO_API(ErrorInit)(&error);
    if (path < 2) {
        reader = xrtReaderCreate(&reader_ops, &state);
        sink = xrtBufferCreate(); writer = sink ? xrtWriterFromBuffer(sink) : NULL;
        if (!reader || !writer) goto cleanup;
        bool result = path == 0 ? CODEC_IO_API(CompressStream)(reader, writer, NULL, NULL, NULL, &error) :
            CODEC_IO_API(DecompressStream)(reader, writer, NULL, NULL, NULL, &error);
        EXPECT(!result && sink->Size == 0);
        /* 各 codec 创建时生成的头部不同；生成计数不等于已交付字节。 */
        output = path == 0 ? CODEC_IO_READER_OUTPUT : 0;
    } else {
        writer = xrtWriterCreate(&writer_ops, &state);
        if (!writer) goto cleanup;
        if (path == 2) {
            encoder = CODEC_IO_API(CompressorCreate)(NULL);
            if (!encoder || !CODEC_IO_API(CompressorUpdate)(encoder, "abc", 3) || !CODEC_IO_API(CompressorFinish)(encoder)) goto cleanup;
            input = CODEC_IO_API(CompressorInputBytes)(encoder); output = CODEC_IO_API(CompressorOutputBytes)(encoder);
            EXPECT(!CODEC_IO_API(CompressorDrain)(encoder, writer));
            error = *CODEC_IO_API(CompressorError)(encoder);
            EXPECT(!CODEC_IO_API(CompressorFinish)(encoder));
        } else {
            decoder = CODEC_IO_API(DecompressorCreate)(NULL);
            if (!decoder || !CODEC_IO_API(DecompressorUpdate)(decoder, abc, sizeof(abc)) || !CODEC_IO_API(DecompressorFinish)(decoder)) goto cleanup;
            input = CODEC_IO_API(DecompressorInputBytes)(decoder); output = CODEC_IO_API(DecompressorOutputBytes)(decoder);
            EXPECT(!CODEC_IO_API(DecompressorDrain)(decoder, writer));
            error = *CODEC_IO_API(DecompressorError)(decoder);
            EXPECT(!CODEC_IO_API(DecompressorFinish)(decoder));
        }
    }
    ready = true;
    check_error(&error, &state, expected, path, input, output);
cleanup:
    EXPECT(ready);
    CODEC_IO_API(CompressorDestroy)(encoder); CODEC_IO_API(DecompressorDestroy)(decoder);
    EXPECT(xrtReaderDestroy(reader)); EXPECT(xrtWriterDestroy(writer));
    EXPECT(state.Closes == 1); xrtBufferDestroy(sink);
    xrtClearError(); xrtMemDebugSnapshot(&after); EXPECT(balanced(&before, &after));
}

static void check_oom(bool decode) {
    xmemdebugsnapshot before, after;
    CODEC_IO_COMPRESSOR *encoder = NULL; CODEC_IO_DECOMPRESSOR *decoder = NULL;
    xbuffer *sink = NULL, *packed = NULL, *plain = NULL; xwriter *writer = NULL;
    CODEC_IO_ERROR recovery; const CODEC_IO_ERROR *error;
    bool ready = false;
    ++cases; xrtClearError(); xrtMemDebugSnapshot(&before);
    sink = xrtBufferCreate(); writer = sink ? xrtWriterFromBuffer(sink) : NULL;
    if (!writer) goto cleanup;
    if (decode) {
        decoder = CODEC_IO_API(DecompressorCreate)(NULL);
        if (!decoder || !CODEC_IO_API(DecompressorUpdate)(decoder, abc, sizeof(abc)) || !CODEC_IO_API(DecompressorFinish)(decoder)) goto cleanup;
    } else {
        encoder = CODEC_IO_API(CompressorCreate)(NULL);
        if (!encoder || !CODEC_IO_API(CompressorUpdate)(encoder, "abc", 3) || !CODEC_IO_API(CompressorFinish)(encoder)) goto cleanup;
    }
    if (!xrtMemDebugFailAfter(0)) goto cleanup;
    bool result = decode ? CODEC_IO_API(DecompressorDrain)(decoder, writer) : CODEC_IO_API(CompressorDrain)(encoder, writer);
    bool triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    error = decode ? CODEC_IO_API(DecompressorError)(decoder) : CODEC_IO_API(CompressorError)(encoder);
    EXPECT(triggered && !result && sink->Size == 0 && error->iCode == CODEC_IO_ERROR_CODE(MEMORY));
    EXPECT(xrtErrorKind(xrtGetError()) == XERR_MEMORY && error->iBackendCode == xrtErrorCode(xrtGetError()) &&
        !strcmp(error->sMessage, xrtErrorMessage(xrtGetError())));
    xrtClearError();
    ready = CODEC_IO_API(Compress)("abc", 3, NULL, &packed, &recovery) &&
        CODEC_IO_API(Decompress)(packed->Data, packed->Size, NULL, &plain, &recovery);
    EXPECT(ready && plain->Size == 3 && !memcmp(plain->Data, "abc", 3));
cleanup:
    xrtMemDebugFailClear(); EXPECT(ready);
    EXPECT(xrtWriterDestroy(writer)); xrtBufferDestroy(sink); xrtBufferDestroy(packed); xrtBufferDestroy(plain);
    CODEC_IO_API(CompressorDestroy)(encoder); CODEC_IO_API(DecompressorDestroy)(decoder);
    xrtClearError(); xrtMemDebugSnapshot(&after); EXPECT(balanced(&before, &after));
}

int main(void) {
    static const struct { xerrkind Kind; int Expected; } errors[] = {
        {XERR_ARGUMENT,CODEC_IO_ERROR_CODE(ARGUMENT)}, {XERR_MEMORY,CODEC_IO_ERROR_CODE(MEMORY)},
        {XERR_RANGE,CODEC_IO_ERROR_CODE(LIMIT)}, {XERR_STATE,CODEC_IO_ERROR_CODE(STATE)},
        {XERR_CANCELLED,CODEC_IO_ERROR_CODE(CANCELLED)}, {XERR_IO,CODEC_IO_ERROR_CODE(IO)}, {XERR_NONE,CODEC_IO_ERROR_CODE(IO)}
    };
    if (!(xrtMemDebugEnabled() || xrtMemDebugEnable(true))) return 2;
    for (unsigned path = 0; path < 4; ++path)
        for (unsigned i = 0; i < sizeof(errors) / sizeof(errors[0]); ++i) check_path(path, errors[i].Kind, errors[i].Expected);
    check_oom(false); check_oom(true);
    printf(CODEC_IO_NAME " IO: %u cases, %u assertion failures\n", cases, failures);
    return failures ? 1 : 0;
}

