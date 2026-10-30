#define XRT_MODULE_MEMORY_DEBUG
#include "../include/xrt_csv.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { READ, READER_CLOSE, WRITE, FLUSH, WRITER_CLOSE };

typedef struct {
    xerror* Cause;
    unsigned int Stage, Closed;
    bool FailDiagnostic;
} failing_stream;

/* 首错已存在，再单独击中 CSV 诊断对象的分配，不能混成底层 IO 的 OOM。 */
static bool fail(failing_stream* stream)
{
    xrtSetError(stream->Cause);
    if (stream->FailDiagnostic) assert(xrtMemDebugFailAfter(0));
    return false;
}

static bool read_stream(ptr context, ptr output, size_t request, size_t* count)
{
    failing_stream* stream = context;
    (void)output; (void)request;
    *count = 0;
    return stream->Stage == READ ? fail(stream) : true;
}

static bool write_stream(ptr context, const void* input, size_t request, size_t* count)
{
    failing_stream* stream = context;
    (void)input;
    *count = stream->Stage == WRITE ? 0 : request;
    return stream->Stage == WRITE ? fail(stream) : true;
}

static bool flush_stream(ptr context)
{
    failing_stream* stream = context;
    return stream->Stage == FLUSH ? fail(stream) : true;
}

static bool close_stream(ptr context)
{
    failing_stream* stream = context;
    ++stream->Closed;
    if (stream->Stage == READER_CLOSE || stream->Stage == WRITER_CLOSE)
        return fail(stream);
    /* 析构不能让清理回调替换已经发布的首错。 */
    xrtSetErrorKind(XERR_ARGUMENT);
    return true;
}

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before->LiveCount && after.LiveBytes == before->LiveBytes);
    assert(after.InvalidFreeCount == before->InvalidFreeCount);
    assert(after.DoubleFreeCount == before->DoubleFreeCount);
    assert(after.OverflowCount == before->OverflowCount);
    assert(after.UnderflowCount == before->UnderflowCount);
}

static void check(unsigned int stage, bool memory, bool fail_diagnostic)
{
    static const char* const operations[] = {"read", "reader-close", "write", "flush", "writer-close"};
    xmemdebugsnapshot before;
    failing_stream stream = {0};
    xerrordesc desc = {0};
    xrtcsvreader* reader = NULL;
    xrtcsvwriter* writer = NULL;
    xrtcsvrow row;
    const xrtcsverror* csv_error;
    const xerror* cause;
    xerror* saved;
    const str fields[] = {"a,b"};
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    desc.Kind = memory ? XERR_MEMORY : XERR_IO;
    desc.Code = 731;
    desc.SystemCode = 812;
    desc.Domain = "csv.test.source";
    desc.Operation = operations[stage];
    desc.Message = "original stream failure";
    desc.Data = "source-detail";
    stream.Cause = xrtErrorBuild(&desc);
    stream.Stage = stage;
    stream.FailDiagnostic = fail_diagnostic;
    assert(stream.Cause);

    if (stage <= READER_CLOSE) {
        xreaderops ops = {0};
        ops.Read = read_stream;
        ops.Close = close_stream;
        reader = xrtCsvReaderCreate(xrtReaderCreate(&ops, &stream), true, NULL);
        assert(reader && xrtCsvReaderError(reader)->iCode == XRT_CSV_OK);
        assert(!(stage == READ ? xrtCsvReaderNext(reader, &row) : xrtCsvReaderClose(reader)));
        csv_error = xrtCsvReaderError(reader);
    } else {
        xwriterops ops = {0};
        ops.Write = write_stream;
        ops.Flush = flush_stream;
        ops.Close = close_stream;
        writer = xrtCsvWriterCreate(xrtWriterCreate(&ops, &stream), true, NULL);
        assert(writer && xrtCsvWriterError(writer)->iCode == XRT_CSV_OK);
        if (stage == WRITE) assert(!xrtCsvWriterWrite(writer, 1, fields));
        else assert(xrtCsvWriterWrite(writer, 1, fields) && !xrtCsvWriterClose(writer));
        csv_error = xrtCsvWriterError(writer);
    }
    assert(xrtMemDebugFailTriggered() == fail_diagnostic);
    xrtMemDebugFailClear();
    saved = xrtErrorRef(xrtGetError());
    assert(csv_error->iCode == (memory ? XRT_CSV_ERROR_MEMORY : XRT_CSV_ERROR_IO));
    if (!saved || xrtErrorKind(saved) != desc.Kind)
        fprintf(stderr, "stage=%u memory=%d diagnostic-oom=%d: first error kind=%d, expected=%d\n",
            stage, memory, fail_diagnostic, xrtErrorKind(saved), desc.Kind);
    assert(saved && xrtErrorKind(saved) == desc.Kind);
    cause = fail_diagnostic ? saved : xrtErrorCause(saved);
    assert(cause == stream.Cause);
    assert(xrtErrorCode(cause) == 731 && xrtErrorSystemCode(cause) == 812);
    assert(!strcmp(xrtErrorDomain(cause), "csv.test.source"));
    assert(!strcmp(xrtErrorOperation(cause), operations[stage]));
    assert(!strcmp(xrtErrorMessage(cause), "original stream failure"));
    assert(!strcmp(xrtErrorData(cause), "source-detail"));
    if (!fail_diagnostic) {
        assert(xrtErrorCode(saved) == csv_error->iCode);
        assert(!strcmp(xrtErrorDomain(saved), "xrt.csv"));
    }

    /* 清除线程错误后重试，仍须从对象恢复同一个首错，且不重复释放流。 */
    xrtClearError();
    if (reader) {
        assert(!xrtCsvReaderNext(reader, &row) && xrtGetError() == saved);
        assert(!xrtCsvReaderClose(reader) && xrtGetError() == saved);
        xrtCsvReaderDestroy(reader);
    } else {
        assert(!xrtCsvWriterWrite(writer, 0, NULL) && xrtGetError() == saved);
        assert(!xrtCsvWriterClose(writer) && xrtGetError() == saved);
        assert(!xrtCsvWriterTakeBuffer(writer) && xrtGetError() == saved);
        xrtCsvWriterDestroy(writer);
    }
    assert(stream.Closed == 1 && xrtGetError() == saved);
    xrtMemDebugFailClear();
    xrtErrorFree(stream.Cause);
    xrtErrorFree(saved);
    xrtClearError();
    balanced(&before);
}

int main(void)
{
    assert(xrtMemDebugEnable(true));
    for (unsigned int stage = READ; stage <= WRITER_CLOSE; ++stage)
        for (unsigned int memory = 0; memory < 2; ++memory)
            for (unsigned int oom = 0; oom < 2; ++oom)
                check(stage, memory != 0, oom != 0);
    puts("csv first error: 20 stream/diagnostic/cleanup cases passed");
    return 0;
}
