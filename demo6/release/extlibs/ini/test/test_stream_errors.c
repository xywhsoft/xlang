#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#include "../xrt_ini_internal.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    const char* Text;
    size_t Offset;
    xerrkind Body;
    xerrkind Flush;
    xerrkind Close;
    unsigned Closed;
} stream;

static bool fail(xerrkind kind, const char* message)
{
    if (kind == XERR_NONE) return true;
    xrtSetErrorInfo(kind, "test.ini.stream", 17, message);
    return false;
}

static bool read_stream(ptr context, ptr output, size_t size, size_t* count)
{
    stream* input = context;
    size_t available = strlen(input->Text) - input->Offset;
    *count = 0;
    if (!fail(input->Body, "body failure")) return false;
    *count = size < available ? size : available;
    memcpy(output, input->Text + input->Offset, *count);
    input->Offset += *count;
    return true;
}

static bool write_stream(ptr context, const void* input, size_t size, size_t* count)
{
    stream* output = context;
    (void)input;
    *count = 0;
    if (!fail(output->Body, "body failure")) return false;
    *count = size;
    return true;
}

static bool flush_stream(ptr context)
{
    return fail(((stream*)context)->Flush, "flush failure");
}

static bool close_stream(ptr context)
{
    stream* value = context;
    ++value->Closed;
    return fail(value->Close, "close failure");
}

static int error_matches(xrtiniconfig* config, int code)
{
    const xerror* error = xrtGetError();
    return config != NULL && xrtIniError(config)->iCode == code && error != NULL &&
        xrtErrorKind(error) == (code == XRT_INI_ERROR_MEMORY ? XERR_MEMORY : XERR_STATE) &&
        xrtErrorCode(error) == code && strcmp(xrtErrorDomain(error), "xlang.extlib.ini") == 0 &&
        strcmp(xrtErrorMessage(error), xrtIniError(config)->sMessage) == 0;
}

static int balanced(const xmemdebugsnapshot* before, const xmemdebugsnapshot* after)
{
    if (before->LiveCount == after->LiveCount && before->LiveBytes == after->LiveBytes) return 1;
    fprintf(stderr, "INI stream allocation imbalance: count=%zu/%zu bytes=%zu/%zu\n",
        before->LiveCount, after->LiveCount, before->LiveBytes, after->LiveBytes);
    return 0;
}

static int reader_case(cstr text, xerrkind body, xerrkind close, unsigned mode, int code)
{
    stream input = {text, 0, body, XERR_NONE, close, 0};
    xreaderops ops = {.Read = read_stream, .Close = close_stream};
    xrtinioptions options;
    xmemdebugsnapshot before, after;
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    xrtIniOptionsInit(&options);
    if (mode == 1) options.tLimits.iMaxInputBytes = 2;
    if (mode == 2) ++options.iVersion;
    xreader* reader = xrtReaderCreate(&ops, &input);
    if (reader == NULL) return 0;
    if (mode == 3 && !xrtMemDebugFailAfter(0)) { (void)xrtReaderDestroy(reader); return 0; }
    xrtiniconfig* config = xrtIniParse(reader, true, &options);
    int triggered = mode != 3 || xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    int ok = input.Closed == 1 && triggered && (mode == 3 ?
        config == NULL && xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY :
        error_matches(config, code));
    if (!ok) fprintf(stderr, "INI Reader mode=%u body=%d close=%d object=%d global=%d/%lld\n",
        mode, (int)body, (int)close, config ? xrtIniError(config)->iCode : -1,
        xrtGetError() ? (int)xrtErrorKind(xrtGetError()) : -1,
        (long long)(xrtGetError() ? xrtErrorCode(xrtGetError()) : -1));
    xrtIniDestroy(config);
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    ok &= balanced(&before, &after);
    return ok;
}

static int writer_case(xerrkind body, xerrkind flush, xerrkind close, unsigned mode, int code)
{
    stream output = {NULL, 0, body, flush, close, 0};
    xwriterops ops = {.Write = write_stream, .Flush = flush_stream, .Close = close_stream};
    xrtinioptions options;
    xmemdebugsnapshot before, after;
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    xrtIniOptionsInit(&options);
    if (mode == 1) options.tLimits.iMaxOutputBytes = 2;
    xrtiniconfig* config = xrtIniCreate(&options);
    xwriter* writer = xrtWriterCreate(&ops, &output);
    int ok = config != NULL && writer != NULL &&
        xrtIniSet(config, (str)"", (str)"key", (str)(mode == 2 ? "bad\n" : "value"));
    if (ok) ok = !xrtIniWrite(config, writer, true) && output.Closed == 1 && error_matches(config, code);
    else (void)xrtWriterDestroy(writer);
    if (!ok) fprintf(stderr, "INI Writer mode=%u body=%d flush=%d close=%d object=%d global=%d/%lld\n",
        mode, (int)body, (int)flush, (int)close, config ? xrtIniError(config)->iCode : -1,
        xrtGetError() ? (int)xrtErrorKind(xrtGetError()) : -1,
        (long long)(xrtGetError() ? xrtErrorCode(xrtGetError()) : -1));
    xrtIniDestroy(config);
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    ok &= balanced(&before, &after);
    return ok;
}

int main(void)
{
    int ok = 1;
    if (!xrtMemDebugEnabled() || !xrtMemDebugFailAfter(UINT64_MAX)) return 1;
    xrtMemDebugFailClear();
    xrtSetErrorInfo(XERR_STATE, "test.ini.stream", 0, "warm error slot");
    xrtClearError();
    ok &= reader_case("key=value\n", XERR_NONE, XERR_MEMORY, 0, XRT_INI_ERROR_MEMORY);
    ok &= reader_case("key=value\n", XERR_MEMORY, XERR_IO, 0, XRT_INI_ERROR_MEMORY);
    ok &= reader_case("key=value\n", XERR_IO, XERR_MEMORY, 0, XRT_INI_ERROR_IO);
    ok &= reader_case("[bad\n", XERR_NONE, XERR_MEMORY, 0, XRT_INI_ERROR_FORMAT);
    ok &= reader_case("key=value\n", XERR_NONE, XERR_MEMORY, 1, XRT_INI_ERROR_LIMIT);
    ok &= reader_case("key=value\n", XERR_NONE, XERR_MEMORY, 2, XRT_INI_ERROR_ARGUMENT);
    ok &= reader_case("key=value\n", XERR_NONE, XERR_IO, 3, XRT_INI_ERROR_MEMORY);
    ok &= writer_case(XERR_NONE, XERR_NONE, XERR_MEMORY, 0, XRT_INI_ERROR_MEMORY);
    ok &= writer_case(XERR_MEMORY, XERR_NONE, XERR_IO, 0, XRT_INI_ERROR_MEMORY);
    ok &= writer_case(XERR_IO, XERR_NONE, XERR_MEMORY, 0, XRT_INI_ERROR_IO);
    ok &= writer_case(XERR_NONE, XERR_MEMORY, XERR_IO, 0, XRT_INI_ERROR_MEMORY);
    ok &= writer_case(XERR_NONE, XERR_IO, XERR_MEMORY, 0, XRT_INI_ERROR_IO);
    ok &= writer_case(XERR_NONE, XERR_NONE, XERR_MEMORY, 1, XRT_INI_ERROR_LIMIT);
    ok &= writer_case(XERR_NONE, XERR_NONE, XERR_MEMORY, 2, XRT_INI_ERROR_FORMAT);
    if (ok) puts("INI stream first-cause and close matrix: 14 cases PASS");
    return ok ? 0 : 1;
}

#include "../xrt_ini_model.c"
#include "../xrt_ini_parser.c"
#include "../xrt_ini_writer.c"
