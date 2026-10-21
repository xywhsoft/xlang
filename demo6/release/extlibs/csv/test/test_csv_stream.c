#include "../include/xrt_csv.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const unsigned char* Input;
    size_t Size, Position, Chunk, FailAt, Calls;
    unsigned char Output[1024];
    unsigned int Closed, Flushed;
    bool FailClose, FailFlush, FailMemory, NoProgress;
} stream;

static bool io_error(const stream* state)
{
    xrtSetErrorInfo(state->FailMemory ? XERR_MEMORY : XERR_IO,
        "csv.test", 731, "injected stream failure");
    return false;
}

static bool read_chunk(ptr context, ptr output, size_t request, size_t* count)
{
    stream* state = context;
    size_t size = state->Size - state->Position;
    ++state->Calls;
    *count = 0;
    if (state->Position >= state->FailAt) return io_error(state);
    if (size > request) size = request;
    if (size > state->Chunk) size = state->Chunk;
    if (size) memcpy(output, state->Input + state->Position, size);
    state->Position += size;
    *count = size;
    return true;
}

static bool write_chunk(ptr context, const void* data, size_t request, size_t* count)
{
    stream* state = context;
    size_t size = request;
    ++state->Calls;
    *count = 0;
    if (state->Position >= state->FailAt) return io_error(state);
    if (state->NoProgress) return true;
    if (size > state->Chunk) size = state->Chunk;
    assert(size <= sizeof(state->Output) - state->Position);
    memcpy(state->Output + state->Position, data, size);
    state->Position += size;
    *count = size;
    return true;
}

static bool close_stream(ptr context)
{
    stream* state = context;
    ++state->Closed;
    return state->FailClose ? io_error(state) : true;
}

static bool flush_stream(ptr context)
{
    stream* state = context;
    ++state->Flushed;
    return state->FailFlush ? io_error(state) : true;
}

static xreader* make_reader(stream* state)
{
    xreaderops ops = {0};
    ops.Read = read_chunk;
    ops.Close = close_stream;
    return xrtReaderCreate(&ops, state);
}

static xwriter* make_writer(stream* state)
{
    xwriterops ops = {0};
    ops.Write = write_chunk;
    ops.Flush = flush_stream;
    ops.Close = close_stream;
    return xrtWriterCreate(&ops, state);
}

/* 分块可以切在 BOM、CRLF、UTF-8 和双引号转义的任意字节之间。 */
static void short_reads(void)
{
    const unsigned char text[] = "\xef\xbb\xbf" "name,note\r\n\xe4\xbd\xa0\xe5\xa5\xbd,\"a\r\nb\"\"c\"\r\n,\r\nx,y";
    for (size_t chunk = 1; chunk <= sizeof(text); ++chunk) {
        stream source = {0};
        xrtcsvdialect dialect;
        xrtcsvrow row;
        xrtcsvreader* reader;
        source.Input = text; source.Size = sizeof(text) - 1;
        source.Chunk = chunk; source.FailAt = SIZE_MAX;
        xrtCsvDialectInit(&dialect);
        dialect.tLimits.iMaxInputBytes = source.Size;
        dialect.tLimits.iMaxEntries = 4;
        reader = xrtCsvReaderCreate(make_reader(&source), true, &dialect);
        assert(reader && !xrtGetError());
        assert(xrtCsvReaderNext(reader, &row) && row.iRow == 0 && row.iCount == 2);
        assert(!strcmp(row.pFields[0], "name") && !strcmp(row.pFields[1], "note"));
        assert(xrtCsvReaderNext(reader, &row) && row.iRow == 1 && row.iCount == 2);
        assert(!strcmp(row.pFields[0], "\xe4\xbd\xa0\xe5\xa5\xbd") && !strcmp(row.pFields[1], "a\r\nb\"c"));
        assert(xrtCsvReaderNext(reader, &row) && row.iCount == 2 && !*row.pFields[0] && !*row.pFields[1]);
        assert(xrtCsvReaderNext(reader, &row) && row.iCount == 2 && !strcmp(row.pFields[1], "y"));
        assert(!xrtCsvReaderNext(reader, &row) && xrtCsvReaderEOF(reader) && !xrtGetError());
        assert(row.iCount == 0 && row.pFields == NULL);
        assert(xrtCsvReaderClose(reader) && xrtCsvReaderClose(reader) && source.Closed == 1);
        assert(!xrtCsvReaderNext(reader, &row) && xrtErrorKind(xrtGetError()) == XERR_STATE);
        xrtCsvReaderDestroy(reader);
        assert(source.Closed == 1);
        xrtClearError();
    }
}

static void sticky_errors_and_positions(void)
{
    const char text[] = "h\r\n\"a\r\nb\"x\r\n";
    xrtcsvrow row;
    xrtcsvreader* reader = xrtCsvReaderOpenMemory(text, sizeof(text) - 1, NULL);
    xerror* saved;
    const xrtcsverror* error;
    assert(reader && xrtCsvReaderNext(reader, &row));
    assert(!xrtCsvReaderNext(reader, &row));
    error = xrtCsvReaderError(reader);
    assert(error->iCode == XRT_CSV_ERROR_FORMAT && error->iRow == 1);
    assert(error->iLine == 3 && error->iColumn == 3 && error->iOffset == 9);
    assert(!strcmp(xrtErrorData(xrtGetError()), "{\"row\":1,\"line\":3,\"column\":3,\"offset\":9}"));
    saved = xrtErrorRef(xrtGetError());
    xrtSetErrorKind(XERR_ARGUMENT);
    assert(!xrtCsvReaderNext(reader, &row) && xrtGetError() == saved);
    assert(!xrtCsvReaderClose(reader) && xrtGetError() == saved);
    xrtCsvReaderDestroy(reader);
    assert(xrtGetError() == saved && xrtErrorCode(saved) == XRT_CSV_ERROR_FORMAT);
    xrtClearError();
    assert(!strcmp(xrtErrorDomain(saved), "xrt.csv"));
    xrtErrorFree(saved);

    for (unsigned int mode = 0; mode < 2; ++mode) {
        stream source = {0};
        source.Input = (const unsigned char*)"a\nb\n"; source.Size = 4;
        source.Chunk = 1; source.FailAt = mode == 0 ? 2 : SIZE_MAX;
        source.FailClose = true;
        reader = xrtCsvReaderCreate(make_reader(&source), true, NULL);
        assert(reader && xrtCsvReaderNext(reader, &row));
        if (mode == 0) assert(!xrtCsvReaderNext(reader, &row));
        else assert(!xrtCsvReaderClose(reader));
        saved = xrtErrorRef(xrtGetError());
        assert(xrtErrorKind(saved) == XERR_IO && xrtErrorCode(saved) == XRT_CSV_ERROR_IO);
        assert(xrtErrorCode(xrtErrorCause(saved)) == 731);
        assert(!xrtCsvReaderClose(reader) && xrtGetError() == saved);
        xrtCsvReaderDestroy(reader);
        assert(source.Closed == 1 && xrtGetError() == saved);
        xrtErrorFree(saved);
        xrtClearError();
    }
    for (unsigned int mode = 0; mode < 2; ++mode) {
        stream source = {0};
        source.Input = (const unsigned char*)"a\nb\n";
        source.Size = 4;
        source.Chunk = 1;
        source.FailAt = mode == 0 ? 2 : SIZE_MAX;
        source.FailClose = mode == 1;
        source.FailMemory = true;
        reader = xrtCsvReaderCreate(make_reader(&source), true, NULL);
        assert(reader && xrtCsvReaderNext(reader, &row));
        if (mode == 0) assert(!xrtCsvReaderNext(reader, &row));
        else assert(!xrtCsvReaderClose(reader));
        saved = xrtErrorRef(xrtGetError());
        assert(saved && xrtErrorKind(saved) == XERR_MEMORY);
        assert(xrtErrorCode(saved) == XRT_CSV_ERROR_MEMORY);
        assert(xrtErrorKind(xrtErrorCause(saved)) == XERR_MEMORY);
        assert(!xrtCsvReaderClose(reader) && xrtGetError() == saved);
        xrtCsvReaderDestroy(reader);
        assert(source.Closed == 1 && xrtGetError() == saved);
        xrtErrorFree(saved);
        xrtClearError();
    }
}

static void short_writes(void)
{
    const str fields[] = {"a,b", "x\"y", ""};
    const char expected[] = "\"a,b\",\"x\"\"y\",\n";
    for (size_t chunk = 1; chunk <= sizeof(expected); ++chunk) {
        stream sink = {0};
        xrtcsvwriter* writer;
        sink.Chunk = chunk; sink.FailAt = SIZE_MAX;
        writer = xrtCsvWriterCreate(make_writer(&sink), true, NULL);
        assert(writer && xrtCsvWriterWrite(writer, 3, fields));
        assert(xrtCsvWriterClose(writer) && sink.Closed == 1 && sink.Flushed == 1);
        assert(xrtCsvWriterClose(writer) && sink.Closed == 1 && sink.Flushed == 1);
        assert(sink.Position == sizeof(expected) - 1 && !memcmp(sink.Output, expected, sink.Position));
        assert(!xrtCsvWriterWrite(writer, 3, fields) && xrtErrorKind(xrtGetError()) == XERR_STATE);
        assert(xrtCsvWriterTakeBuffer(writer) == NULL && xrtErrorKind(xrtGetError()) == XERR_STATE);
        xrtCsvWriterDestroy(writer);
        assert(sink.Closed == 1);
        xrtClearError();
    }
    for (unsigned int mode = 0; mode < 4; ++mode) {
        stream sink = {0};
        xrtcsvwriter* writer;
        xerror* saved;
        sink.Chunk = 1; sink.FailAt = mode == 0 ? 3 : SIZE_MAX;
        sink.NoProgress = mode == 1; sink.FailFlush = mode == 2; sink.FailClose = mode == 3;
        writer = xrtCsvWriterCreate(make_writer(&sink), true, NULL);
        assert(writer);
        if (mode < 2) assert(!xrtCsvWriterWrite(writer, 3, fields));
        else assert(xrtCsvWriterWrite(writer, 3, fields) && !xrtCsvWriterClose(writer));
        saved = xrtErrorRef(xrtGetError());
        assert(saved && xrtErrorKind(saved) == XERR_IO);
        if (mode != 1) assert(xrtErrorCode(xrtErrorCause(saved)) == 731);
        assert(!xrtCsvWriterClose(writer) && xrtGetError() == saved);
        assert(!xrtCsvWriterWrite(writer, 0, NULL) && xrtGetError() == saved);
        assert(xrtCsvWriterTakeBuffer(writer) == NULL && xrtGetError() == saved);
        xrtCsvWriterDestroy(writer);
        assert(sink.Closed == 1 && xrtGetError() == saved);
        xrtErrorFree(saved);
        xrtClearError();
    }
    for (unsigned int mode = 0; mode < 2; ++mode) {
        stream sink = {0};
        xrtcsvwriter* writer;
        xerror* saved;
        sink.Chunk = sizeof(sink.Output);
        sink.FailAt = SIZE_MAX;
        sink.FailMemory = true;
        sink.FailFlush = mode == 0;
        sink.FailClose = mode == 1;
        writer = xrtCsvWriterCreate(make_writer(&sink), true, NULL);
        assert(writer && xrtCsvWriterWrite(writer, 3, fields));
        assert(!xrtCsvWriterClose(writer));
        saved = xrtErrorRef(xrtGetError());
        assert(saved && xrtErrorKind(saved) == XERR_MEMORY);
        assert(xrtErrorCode(saved) == XRT_CSV_ERROR_MEMORY);
        assert(xrtErrorKind(xrtErrorCause(saved)) == XERR_MEMORY);
        assert(!xrtCsvWriterClose(writer) && xrtGetError() == saved);
        xrtCsvWriterDestroy(writer);
        assert(sink.Closed == 1 && xrtGetError() == saved);
        xrtErrorFree(saved);
        xrtClearError();
    }
}

static void borrowed_streams(void)
{
    stream source = {0}, sink = {0};
    xreader* input;
    xwriter* output;
    xrtcsvreader* reader;
    xrtcsvwriter* writer;
    source.Input = (const unsigned char*)"a"; source.Size = 1;
    source.Chunk = sink.Chunk = 1; source.FailAt = sink.FailAt = SIZE_MAX;
    input = make_reader(&source); output = make_writer(&sink);
    reader = xrtCsvReaderCreate(input, false, NULL);
    writer = xrtCsvWriterCreate(output, false, NULL);
    assert(reader && writer && xrtCsvReaderClose(reader) && xrtCsvWriterClose(writer));
    xrtCsvReaderDestroy(reader); xrtCsvWriterDestroy(writer);
    assert(source.Closed == 0 && sink.Closed == 0 && sink.Flushed == 1);
    assert(xrtReaderDestroy(input) && xrtWriterDestroy(output));
    assert(source.Closed == 1 && sink.Closed == 1);
}

static void constructor_errors(void)
{
    xrtcsvreader* reader;
    xrtcsvwriter* writer;
    xrtSetErrorKind(XERR_MEMORY);
    reader = xrtCsvReaderCreate(NULL, false, NULL);
    assert(reader && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    xrtCsvReaderDestroy(reader);
    xrtSetErrorKind(XERR_MEMORY);
    writer = xrtCsvWriterCreate(NULL, false, NULL);
    assert(writer && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    xrtCsvWriterDestroy(writer);
    xrtClearError();
}

int main(void)
{
    short_reads();
    sticky_errors_and_positions();
    short_writes();
    borrowed_streams();
    constructor_errors();
    puts("csv stream boundaries: PASS");
    return 0;
}
