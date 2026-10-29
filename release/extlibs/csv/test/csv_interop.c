#define XRT_MODULE_FILE_WHOLE
#include "../include/xrt_csv.h"
#include <stdio.h>
#include <string.h>

/* 互操作夹具使用 u32le 字段数和以 NUL 结尾的字段长度，不借用 CSV 来解析预期值。 */
static bool take32(const unsigned char* data, size_t size, size_t* at, uint32* value)
{
    if (size - *at < 4) return false;
    const unsigned char* p = data + *at;
    *value = (uint32)p[0] | ((uint32)p[1] << 8) | ((uint32)p[2] << 16) | ((uint32)p[3] << 24);
    *at += 4;
    return true;
}

static bool write_fixture(cstr input, cstr output, const xrtcsvdialect* dialect)
{
    size_t size = 0, at = 0;
    bytes data = xrtFileReadAll(input, &size);
    xrtcsvwriter* writer = data ? xrtCsvWriterOpenFile(output, dialect) : NULL;
    bool ok = writer != NULL;
    while (ok && at < size) {
        uint32 count = 0;
        str fields[64];
        ok = take32(data, size, &at, &count) && count <= 64;
        for (uint32 i = 0; ok && i < count; ++i) {
            uint32 length = 0;
            ok = take32(data, size, &at, &length) && length > 0 && length <= size - at;
            if (ok) {
                ok = data[at + length - 1] == 0 && memchr(data + at, 0, length - 1) == NULL;
                fields[i] = (str)(data + at);
                at += length;
            }
        }
        if (ok) ok = xrtCsvWriterWrite(writer, count, fields);
    }
    if (ok) ok = xrtCsvWriterClose(writer);
    if (!ok && writer) fprintf(stderr, "%s\n", xrtCsvWriterError(writer)->sMessage);
    xrtCsvWriterDestroy(writer);
    xrtFree(data);
    return ok;
}

static bool read_fixture(cstr input, const xrtcsvdialect* dialect)
{
    xrtcsvreader* reader = xrtCsvReaderOpenFile(input, dialect);
    xrtcsvrow row;
    bool ok;
    if (!reader) return false;
    while (xrtCsvReaderNext(reader, &row)) {
        printf("%u", (unsigned)row.iCount);
        for (uint32 i = 0; i < row.iCount; ++i) {
            const unsigned char* field = (const unsigned char*)row.pFields[i];
            putchar(':');
            for (; *field; ++field) printf("%02x", (unsigned)*field);
        }
        putchar('\n');
    }
    ok = xrtCsvReaderEOF(reader) && xrtCsvReaderError(reader)->iCode == XRT_CSV_OK;
    if (!ok) fprintf(stderr, "%s\n", xrtCsvReaderError(reader)->sMessage);
    xrtCsvReaderDestroy(reader);
    return ok && !ferror(stdout);
}

int main(int argc, char** argv)
{
    xrtcsvdialect dialect;
    bool ok;
    /* mode input output delimiter quote newline quoteAll BOM */
    if (argc != 9 || strlen(argv[4]) != 1 || strlen(argv[5]) != 1) return 2;
    xrtCsvDialectInit(&dialect);
    dialect.iDelimiter = argv[4][0];
    dialect.iQuote = argv[5][0];
    dialect.iNewline = strcmp(argv[6], "crlf") == 0 ? XRT_CSV_NEWLINE_CRLF : XRT_CSV_NEWLINE_LF;
    dialect.bQuoteAll = strcmp(argv[7], "1") == 0;
    dialect.bWriteBOM = strcmp(argv[8], "1") == 0;
    if (strcmp(argv[1], "write") == 0) ok = write_fixture(argv[2], argv[3], &dialect);
    else if (strcmp(argv[1], "read") == 0) ok = read_fixture(argv[2], &dialect);
    else return 2;
    xrtClearError();
    return ok ? 0 : 1;
}
