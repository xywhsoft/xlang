/* 正文、刷新和关闭同时失败时，包错误与 XRT 错误都保留首因。 */
#define XRT_MODULE_MEMORY_DEBUG
#include "../xrt_netrc_internal.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    const char* Text;
    size_t Offset;
    xerrkind Body, Flush, Close;
    unsigned Closed;
} stream;

static unsigned checks, failures;

/* 仅在本测试翻译单元注入文件后端，真实文件/0600/原子替换另由旧套件验证。 */
static stream file_stream;
static xerrkind rename_error, delete_error;
static unsigned renamed, deleted;

static bool fail(xerrkind kind, const char* message)
{
    if (kind == XERR_NONE) return true;
    xrtSetErrorInfo(kind, "test.netrc.stream", 17, message);
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

static xfile temp_file(cstr directory, cstr prefix, cstr suffix, str* path)
{
    (void)directory;
    (void)prefix;
    (void)suffix;
    *path = xrtStrDup("temporary.netrc");
    return *path == NULL ? NULL : (xfile)&file_stream;
}

static bool write_file(xfile file, const void* data, size_t size, size_t* count)
{
    size_t written;
    bool ok = write_stream((ptr)file, data, size, &written);
    if (count != NULL) *count = written;
    return ok;
}

static bool flush_file(xfile file) { return flush_stream((ptr)file); }
static bool close_file(xfile file) { return close_stream((ptr)file); }

static bool rename_file(cstr source, cstr target, bool replace)
{
    (void)source;
    (void)target;
    (void)replace;
    ++renamed;
    return fail(rename_error, "rename failure");
}

static bool delete_file(cstr path)
{
    (void)path;
    ++deleted;
    return fail(delete_error, "delete failure");
}

static void check_error(const char* name, bool returned, unsigned closed,
    int actual, xerrkind kind, const char* message, const xmemdebugsnapshot* before)
{
    const xerror* cause = xrtGetError();
    int expected = kind == XERR_NONE ? XRT_NETRC_OK :
        (kind == XERR_MEMORY ? XRT_NETRC_ERROR_MEMORY : XRT_NETRC_ERROR_IO);
    bool ok = returned == (kind == XERR_NONE) && closed == 1 && actual == expected &&
        (kind == XERR_NONE ? cause == NULL : cause != NULL &&
        xrtErrorKind(cause) == kind && xrtErrorCode(cause) == 17 &&
        strcmp(xrtErrorDomain(cause), "test.netrc.stream") == 0 &&
        strcmp(xrtErrorMessage(cause), message) == 0);
    if (!ok) fprintf(stderr, "%s package=%d expected=%d global=%d\n", name, actual,
        expected, cause == NULL ? -1 : (int)xrtErrorKind(cause));
    xrtClearError();
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    ok = ok && before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes;
    ++checks;
    if (!ok) ++failures;
}

static void reader_case(xerrkind body, xerrkind close)
{
    stream input = {"machine h login u\n", 0, body, XERR_NONE, close, 0};
    xreaderops ops = {.Read = read_stream, .Close = close_stream};
    xrtnetrcerror error;
    xmemdebugsnapshot before;
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    xreader* reader = xrtReaderCreate(&ops, &input);
    if (reader == NULL) { ++checks; ++failures; return; }
    xvalue* document = xrtNetrcParse(reader, true, NULL, &error);
    bool returned = document != NULL;
    xrtValueRelease(document);
    xerrkind expected = body != XERR_NONE ? body : close;
    check_error("reader first cause", returned, input.Closed, error.iCode, expected,
        body != XERR_NONE ? "body failure" : "close failure", &before);
}

static void writer_case(xvalue* document, xerrkind body, xerrkind flush, xerrkind close)
{
    stream output = {NULL, 0, body, flush, close, 0};
    xwriterops ops = {.Write = write_stream, .Flush = flush_stream, .Close = close_stream};
    xrtnetrcerror error;
    xmemdebugsnapshot before;
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    xwriter* writer = xrtWriterCreate(&ops, &output);
    if (writer == NULL) { ++checks; ++failures; return; }
    bool returned = xrtNetrcWrite(document, writer, true, NULL, &error);
    xerrkind expected = body != XERR_NONE ? body : (flush != XERR_NONE ? flush : close);
    check_error("writer first cause", returned, output.Closed, error.iCode, expected,
        body != XERR_NONE ? "body failure" : (flush != XERR_NONE ? "flush failure" :
        "close failure"), &before);
}

static void save_case(xvalue* document, xerrkind body, xerrkind flush,
    xerrkind close, xerrkind rename, xerrkind cleanup)
{
    file_stream = (stream){NULL, 0, body, flush, close, 0};
    rename_error = rename;
    delete_error = cleanup;
    renamed = deleted = 0;
    xrtnetrcerror error;
    xmemdebugsnapshot before;
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    bool returned = xrtNetrcSave((str)"credentials.netrc", document, NULL, &error);
    xerrkind expected = body != XERR_NONE ? body : (flush != XERR_NONE ? flush :
        (close != XERR_NONE ? close : rename));
    bool reached_rename = body == XERR_NONE && flush == XERR_NONE && close == XERR_NONE;
    bool lifecycle = renamed == (unsigned)reached_rename && deleted == (unsigned)!returned;
    if (!lifecycle) { ++checks; ++failures; }
    check_error("save first cause", returned, file_stream.Closed, error.iCode, expected,
        body != XERR_NONE ? "body failure" : (flush != XERR_NONE ? "flush failure" :
        (close != XERR_NONE ? "close failure" : "rename failure")), &before);
}

int main(void)
{
    if (!xrtMemDebugEnabled()) return 2;
    xrtSetErrorInfo(XERR_STATE, "test.netrc", 0, "warm error slot");
    xrtClearError();
    xrtnetrcerror error;
    xvalue* document = xrtNetrcParseMemory("machine h login u\n", 18, NULL, &error);
    if (document == NULL) return 2;
    static const xerrkind kinds[] = {XERR_NONE, XERR_MEMORY, XERR_IO};
    for (unsigned body = 0; body != 3; ++body) {
        for (unsigned close = 0; close != 3; ++close) {
            reader_case(kinds[body], kinds[close]);
            for (unsigned flush = 0; flush != 3; ++flush)
                writer_case(document, kinds[body], kinds[flush], kinds[close]);
            for (unsigned flush = 0; flush != 3; ++flush)
                for (unsigned cleanup = 0; cleanup != 3; ++cleanup)
                    save_case(document, kinds[body], kinds[flush], kinds[close],
                        XERR_NONE, kinds[cleanup]);
        }
    }
    for (unsigned rename = 0; rename != 3; ++rename)
        for (unsigned cleanup = 0; cleanup != 3; ++cleanup)
            save_case(document, XERR_NONE, XERR_NONE, XERR_NONE, kinds[rename], kinds[cleanup]);
    xrtValueRelease(document);
    printf("netrc stream first cause: %u checks, %u failures\n", checks, failures);
    return failures != 0;
}

#include "../xrt_netrc_common.c"
#include "../xrt_netrc_parser.c"
#define xrtFileTemp temp_file
#define xrtWriteFull write_file
#define xrtFlush flush_file
#define xrtClose close_file
#define xrtPathRename rename_file
#define xrtFileDelete delete_file
#include "../xrt_netrc_writer.c"
#undef xrtFileTemp
#undef xrtWriteFull
#undef xrtFlush
#undef xrtClose
#undef xrtPathRename
#undef xrtFileDelete
