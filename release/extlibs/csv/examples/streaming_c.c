#include "../include/csv_native.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    const str header[] = {"name", "qty", "note"};
    const str alice[] = {"Alice", "20", "a,b"};
    const str bob[] = {"Bob", "22", "第一行\n第二行"};
    const str field[] = {"a,b"};
    const str too_long[] = {"abc"};
    xrtcsvdialect dialect;
    xrtcsvwriter* writer = NULL;
    xrtcsvreader* reader = NULL;
    xbuffer* data = NULL;
    char *name = NULL, *note = NULL;
    char* text = NULL;
    void* text_writer = NULL;
    xrtcsvrow row;
    unsigned int count = 1;
    long total;
    int status = 1;
    xrtCsvDialectInit(&dialect);
    dialect.iNewline = XRT_CSV_NEWLINE_CRLF;
    writer = xrtCsvWriterCreateMemory(&dialect);
    if (!writer || !xrtCsvWriterWrite(writer, 3, header) ||
        !xrtCsvWriterWrite(writer, 3, alice) || !xrtCsvWriterWrite(writer, 3, bob) ||
        !xrtCsvWriterClose(writer)) goto done;
    data = xrtCsvWriterTakeBuffer(writer);
    if (!data) goto done;
    xrtCsvWriterDestroy(writer);
    writer = NULL;
    reader = xrtCsvReaderOpenMemory(data->Data, data->Size, NULL);
    if (!reader || !xrtCsvReaderNext(reader, &row) || row.iCount != 3 ||
        strcmp(row.pFields[0], "name") || !xrtCsvReaderNext(reader, &row) ||
        row.iCount != 3) goto done;
    // C 行指针只借用；跨 Next/Close 的字段需要显式复制。
    name = xrtStrDup(row.pFields[0]);
    note = xrtStrDup(row.pFields[2]);
    if (!name || !note) goto done;
    total = strtol(row.pFields[1], NULL, 10);
    while (xrtCsvReaderNext(reader, &row)) {
        if (row.iCount != 3 || strcmp(row.pFields[2], "第一行\n第二行")) goto done;
        total += strtol(row.pFields[1], NULL, 10);
        ++count;
    }
    if (!xrtCsvReaderEOF(reader) || count != 2 || total != 42 ||
        !xrtCsvReaderClose(reader)) goto done;
    xrtCsvReaderDestroy(reader);
    reader = NULL;
    xrtBufferDestroy(data);
    data = NULL;
    printf("rows: %u\nsum: %ld\nsnapshot: %s|%s\n", count, total, name, note);
    puts("multiline: preserved");

    // 与 XL finishText 相同的所有权入口；返回缓冲由调用方 xrtFree。
    text_writer = xcsv_writer_create_memory(",", "\"", 0, false, false, 0, 0, 0, 0, 0);
    if (!text_writer || !xcsv_writer_begin(text_writer) ||
        !xcsv_writer_field(text_writer, field[0]) || !xcsv_writer_end(text_writer)) goto done;
    text = xcsv_writer_take_text(text_writer);
    if (!text || strcmp(text, "\"a,b\"\n")) goto done;
    puts("finished-text: adopted");

    xrtCsvDialectInit(&dialect);
    dialect.tLimits.iMaxItemBytes = 2;
    writer = xrtCsvWriterCreateMemory(&dialect);
    if (!writer || xrtCsvWriterWrite(writer, 1, too_long) ||
        xrtCsvWriterError(writer)->iCode != XRT_CSV_ERROR_LIMIT ||
        xrtCsvWriterClose(writer) || xrtCsvWriterError(writer)->iCode != XRT_CSV_ERROR_LIMIT) goto done;
    puts("limit: rejected");
    status = 0;
done:
    if (status) fputs("CSV streaming example failed\n", stderr);
    xrtCsvReaderDestroy(reader);
    xrtCsvWriterDestroy(writer);
    xrtBufferDestroy(data);
    xcsv_writer_destroy(text_writer);
    xrtFree(text);
    xrtFree(name);
    xrtFree(note);
    xrtClearError();
    return status;
}

/* 教学范例单文件可运行，正式包按 manifest 独立编译。 */
#include "../xrt_csv_common.c"
#include "../xrt_csv_reader.c"
#include "../xrt_csv_writer.c"
#include "../csv_native.c"
