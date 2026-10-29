#include "../include/xrt_csv.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    const str header[] = {"name", "city", "note"};
    const str alice[] = {"Alice", "上海", "a,b"};
    const str bob[] = {"Bob", "深圳", "say \"hi\""};
    static const char expected[] =
        "name,city,note\nAlice,上海,\"a,b\"\nBob,深圳,\"say \"\"hi\"\"\"\n";
    xrtcsvwriter* writer = xrtCsvWriterCreateMemory(NULL);
    xrtcsvreader* reader = NULL;
    xbuffer* data = NULL;
    xrtcsvrow row;
    unsigned int count = 0;
    int status = 1;
    if (writer == NULL || !xrtCsvWriterWrite(writer, 3, header) ||
        !xrtCsvWriterWrite(writer, 3, alice) ||
        !xrtCsvWriterWrite(writer, 3, bob)) goto done;
    // C API 显式 close 后移交缓冲；XL finish 合并这两个步骤。
    if (!xrtCsvWriterClose(writer)) goto done;
    data = xrtCsvWriterTakeBuffer(writer);
    if (data == NULL) goto done;
    if (data->Size != sizeof(expected) - 1u ||
        memcmp(data->Data, expected, sizeof(expected) - 1u) != 0) goto done;
    xrtCsvWriterDestroy(writer);
    writer = NULL;
    reader = xrtCsvReaderOpenMemory(data->Data, data->Size, NULL);
    if (reader == NULL || !xrtCsvReaderNext(reader, &row) || row.iCount != 3) goto done;
    while (xrtCsvReaderNext(reader, &row)) {
        if (row.iCount != 3) goto done;
        printf("%s|%s|%s\n", row.pFields[0], row.pFields[1], row.pFields[2]);
        ++count;
    }
    if (!xrtCsvReaderEOF(reader) || count != 2) goto done;
    xrtCsvReaderDestroy(reader);
    reader = NULL;
    xrtBufferDestroy(data);
    data = NULL;
    printf("rows: %u\n", count);
    puts("stringify: preserved");

    xrtcsvdialect dialect;
    xrtCsvDialectInit(&dialect);
    dialect.iDelimiter = '\t';
    const str fields[] = {"甲", "a\tb", ""};
    writer = xrtCsvWriterCreateMemory(&dialect);
    if (writer == NULL || !xrtCsvWriterWrite(writer, 3, fields) ||
        !xrtCsvWriterClose(writer)) goto done;
    data = xrtCsvWriterTakeBuffer(writer);
    if (data == NULL) goto done;
    xrtCsvWriterDestroy(writer);
    writer = NULL;
    reader = xrtCsvReaderOpenMemory(data->Data, data->Size, &dialect);
    if (reader == NULL || !xrtCsvReaderNext(reader, &row) || row.iCount != 3 ||
        strcmp(row.pFields[1], "a\tb") != 0) goto done;
    xrtCsvReaderDestroy(reader);
    reader = NULL;
    xrtBufferDestroy(data);
    data = NULL;
    puts("tsv: preserved");

    reader = xrtCsvReaderOpenMemory("\"unfinished", 11, NULL);
    if (reader == NULL || xrtCsvReaderNext(reader, &row) ||
        xrtCsvReaderError(reader)->iCode != XRT_CSV_ERROR_FORMAT) goto done;
    xrtCsvReaderDestroy(reader);
    reader = NULL;
    xrtClearError();
    puts("invalid: CsvError");
    status = 0;
done:
    xrtCsvWriterDestroy(writer);
    xrtCsvReaderDestroy(reader);
    xrtBufferDestroy(data);
    return status;
}

/* 教学范例可单文件运行；正式扩展包仍按清单分别编译。 */
#include "../xrt_csv_common.c"
#include "../xrt_csv_reader.c"
#include "../xrt_csv_writer.c"
