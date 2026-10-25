#define XRT_MODULE_MEMORY_DEBUG
#include "../include/csv_native.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void row_limit(void)
{
    const char* inputs[] = {"a\n", "a\r\n", "a", "\"a\"", "\n"};
    xrtcsvdialect config;
    xrtcsvrow row;
    xrtCsvDialectInit(&config);
    config.tLimits.iMaxEntries = 1;
    for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
        xrtcsvreader* reader = xrtCsvReaderOpenMemory(inputs[i], strlen(inputs[i]), &config);
        assert(reader && xrtCsvReaderNext(reader, &row));
        assert(!xrtCsvReaderNext(reader, &row));
        assert(xrtCsvReaderEOF(reader));
        assert(xrtCsvReaderError(reader)->iCode == XRT_CSV_OK);
        xrtCsvReaderDestroy(reader);
    }
}

static void empty_field(void)
{
    const str fields[] = {""};
    xrtcsvwriter* writer = xrtCsvWriterCreateMemory(NULL);
    xrtcsvreader* reader;
    xrtcsvrow row;
    xbuffer* data;
    assert(writer && xrtCsvWriterWrite(writer, 1, fields));
    assert(xrtCsvWriterClose(writer));
    data = xrtCsvWriterTakeBuffer(writer);
    xrtCsvWriterDestroy(writer);
    assert(data && data->Size == 3 && memcmp(data->Data, "\"\"\n", 3) == 0);
    reader = xrtCsvReaderOpenMemory(data->Data, data->Size, NULL);
    assert(reader && xrtCsvReaderNext(reader, &row));
    assert(row.iCount == 1 && strcmp(row.pFields[0], "") == 0);
    xrtCsvReaderDestroy(reader);
    xrtBufferDestroy(data);
}

static void lifetime(void)
{
    str data = xrtStrDup("name,note\nalice,hello\n");
    void* reader = xcsv_reader_open_memory(data, strlen(data), ",", "\"", true,
        1024, 1024, 128, 100, 100);
    str field;
    assert(reader);
    memset(data, '!', strlen(data));
    xrtFree(data);
    assert(xcsv_reader_next(reader));
    assert(xcsv_reader_count(reader) == 2);
    field = xcsv_reader_field(reader, 0);
    assert(field && strcmp(field, "name") == 0);
    assert(xcsv_reader_next(reader));
    xcsv_reader_destroy(reader);
    assert(strcmp(field, "name") == 0);
    xrtFree(field);
}

static void error_kind(void)
{
    xrtcsvreader* reader = xrtCsvReaderOpenMemory("\"broken", 7, NULL);
    xrtcsvrow row;
    assert(reader && !xrtCsvReaderNext(reader, &row));
    assert(xrtErrorKind(xrtGetError()) == XERR_VALUE);
    assert(xrtErrorCode(xrtGetError()) == XRT_CSV_ERROR_FORMAT);
    xrtCsvReaderDestroy(reader);
}

static bool discard_write(ptr context, const void* bytes, size_t size, size_t* written)
{
    (void)context; (void)bytes;
    *written = size;
    return true;
}

static bool count_close(ptr context)
{
    ++*(int*)context;
    return true;
}

static void close_once(void)
{
    xwriterops ops = {0};
    int closed = 0;
    xrtcsvwriter* writer;
    ops.Write = discard_write;
    ops.Close = count_close;
    writer = xrtCsvWriterCreate(xrtWriterCreate(&ops, &closed), true, NULL);
    assert(writer && xrtCsvWriterClose(writer));
    assert(closed == 1);
    assert(xrtCsvWriterClose(writer) && closed == 1);
    xrtCsvWriterDestroy(writer);
    assert(closed == 1);
}

/* C 字符串结果不能无声丢弃内嵌零字节后面的字段内容。 */
static void nul_input(void)
{
    const unsigned char data[] = {'a', 0, 'b', '\n'};
    xrtcsvreader* reader = xrtCsvReaderOpenMemory(data, sizeof(data), NULL);
    xrtcsvrow row;
    assert(reader && !xrtCsvReaderNext(reader, &row));
    assert(xrtCsvReaderError(reader)->iCode == XRT_CSV_ERROR_FORMAT);
    xrtCsvReaderDestroy(reader);
}

/* 重复 begin 是调用顺序错误，但不能丢掉前一行已经暂存的字段。 */
static void pending_row(void)
{
    void* writer = xcsv_writer_create_memory(",", "\"", 0, false, false, 0, 0, 0, 0, 0);
    xbuffer* result;
    assert(writer && xcsv_writer_begin(writer) && xcsv_writer_field(writer, "kept"));
    assert(!xcsv_writer_begin(writer) && xrtErrorKind(xrtGetError()) == XERR_STATE);
    xrtClearError();
    assert(xcsv_writer_end(writer));
    result = xcsv_writer_take_buffer(writer);
    assert(result && result->Size == 5 && !memcmp(result->Data, "kept\n", 5));
    xrtBufferDestroy(result);
    xcsv_writer_destroy(writer);
}

static void take_text(void)
{
    void* writer = xcsv_writer_create_memory(",", "\"", 0, false, false, 0, 0, 0, 0, 0);
    char* result;
    assert(writer && xcsv_writer_begin(writer) && xcsv_writer_field(writer, "a,b") &&
        xcsv_writer_end(writer));
    result = xcsv_writer_take_text(writer);
    assert(result && !strcmp(result, "\"a,b\"\n"));
    xrtFree(result);
    xcsv_writer_destroy(writer);

    writer = xcsv_writer_create_memory(",", "\"", 0, false, false, 0, 0, 0, 0, 0);
    assert(writer);
    result = xcsv_writer_take_text(writer);
    assert(result && !strcmp(result, ""));
    xrtFree(result);
    xcsv_writer_destroy(writer);
}

static void writer_length_limits(void)
{
    char oversized[8193];
    const str fields[] = { oversized };
    const str exact[] = { "abcd" };
    xrtcsvdialect dialect;
    xrtcsvwriter* direct;
    void* bridge;
    xbuffer* output;
    memset(oversized, 'x', sizeof(oversized) - 1u);
    oversized[sizeof(oversized) - 1u] = 0;

    xrtCsvDialectInit(&dialect);
    dialect.tLimits.iMaxItemBytes = 4u;
    direct = xrtCsvWriterCreateMemory(&dialect);
    assert(direct && !xrtCsvWriterWrite(direct, 1u, fields));
    assert(xrtCsvWriterError(direct)->iCode == XRT_CSV_ERROR_LIMIT);
    assert(xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtCsvWriterDestroy(direct);
    dialect.tLimits.iMaxItemBytes = 0u;
    dialect.tLimits.iMaxInputBytes = 4u;
    direct = xrtCsvWriterCreateMemory(&dialect);
    assert(direct && !xrtCsvWriterWrite(direct, 1u, fields));
    assert(xrtCsvWriterError(direct)->iCode == XRT_CSV_ERROR_LIMIT);
    xrtCsvWriterDestroy(direct);
    direct = xrtCsvWriterCreateMemory(&dialect);
    assert(direct && xrtCsvWriterWrite(direct, 1u, exact) &&
        xrtCsvWriterClose(direct));
    output = xrtCsvWriterTakeBuffer(direct);
    assert(output && output->Size == 5u &&
        memcmp(output->Data, "abcd\n", 5u) == 0);
    xrtBufferDestroy(output);
    xrtCsvWriterDestroy(direct);

    bridge = xcsv_writer_create_memory(",", "\"", 0, false, false,
        0u, 0u, 4u, 0u, 0u);
    assert(bridge && xcsv_writer_begin(bridge));
    assert(!xcsv_writer_field(bridge, oversized));
    assert(xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xcsv_writer_destroy(bridge);
    bridge = xcsv_writer_create_memory(",", "\"", 0, false, false,
        4u, 0u, 0u, 0u, 0u);
    assert(bridge && xcsv_writer_begin(bridge) &&
        xcsv_writer_field(bridge, "ab"));
    assert(!xcsv_writer_field(bridge, "cde"));
    assert(xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xcsv_writer_destroy(bridge);
    bridge = xcsv_writer_create_memory(",", "\"", 0, false, false,
        4u, 0u, 0u, 0u, 0u);
    assert(bridge && xcsv_writer_begin(bridge) &&
        xcsv_writer_field(bridge, "ab") && xcsv_writer_field(bridge, "cd") &&
        xcsv_writer_end(bridge));
    output = xcsv_writer_take_buffer(bridge);
    assert(output && output->Size == 6u &&
        memcmp(output->Data, "ab,cd\n", 6u) == 0);
    xrtBufferDestroy(output);
    xcsv_writer_destroy(bridge);
}

static void dialect_length(void)
{
    char unterminated[2] = {',', 'x'};
    void* reader = xcsv_reader_open_memory("a", 1u, unterminated, "\"", true,
        0u, 0u, 0u, 0u, 0u);
    assert(reader == NULL);
    assert(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
}

int main(int argc, char** argv)
{
    const char* name = argc > 1 ? argv[1] : "all";
    if (!strcmp(name, "all") || !strcmp(name, "row-limit")) row_limit();
    if (!strcmp(name, "all") || !strcmp(name, "empty-field")) empty_field();
    if (!strcmp(name, "all") || !strcmp(name, "lifetime")) lifetime();
    if (!strcmp(name, "all") || !strcmp(name, "error-kind")) error_kind();
    if (!strcmp(name, "all") || !strcmp(name, "close")) close_once();
    if (!strcmp(name, "all") || !strcmp(name, "nul")) nul_input();
    if (!strcmp(name, "all") || !strcmp(name, "pending-row")) pending_row();
    if (!strcmp(name, "all") || !strcmp(name, "take-text")) take_text();
    if (!strcmp(name, "all") || !strcmp(name, "writer-length-limits")) writer_length_limits();
    if (!strcmp(name, "all") || !strcmp(name, "dialect-length")) dialect_length();
    xrtClearError();
    puts("csv boundaries: PASS");
    return 0;
}
