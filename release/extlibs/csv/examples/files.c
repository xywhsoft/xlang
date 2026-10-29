#define XRT_MODULE_DIR
#define XRT_MODULE_DIR_TEMP
#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_PATH
#include "../include/xrt_csv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool kept(cstr target)
{
    size_t size = 0;
    bytes text = xrtFileReadAll(target, &size);
    bool valid = text != NULL && size == 4 && memcmp(text, "keep", 4) == 0;
    xrtFree(text);
    return valid;
}

int main(void)
{
    str directory = xrtDirTemp(NULL, "xlang-csv-", NULL);
    str target = NULL;
    const str discarded[] = {"not committed"};
    const str header[] = {"name", "qty"};
    const str first[] = {"甲", "2"};
    const str second[] = {"乙", "3"};
    xrtcsvwriter* writer = NULL;
    xrtcsvreader* reader = NULL;
    xrtcsvdialect dialect;
    xrtcsvrow row;
    int rows = 0, total = 0, status = 1;
    if (directory == NULL) goto done;
    target = xrtPathJoin(directory, "报表.csv");
    if (target == NULL ||
        !xrtFileWriteAll(target, (xbytesview){(const uint8*)"keep", 4})) goto done;

    writer = xrtCsvWriterOpenFile(target, NULL);
    if (writer == NULL || !xrtCsvWriterWrite(writer, 1, discarded)) goto done;
    /* 销毁不代替提交；未提交的目标应保持原样。 */
    xrtCsvWriterDestroy(writer);
    writer = NULL;
    if (!kept(target)) goto done;

    xrtCsvDialectInit(&dialect);
    dialect.iNewline = XRT_CSV_NEWLINE_CRLF;
    dialect.bWriteBOM = true;
    writer = xrtCsvWriterOpenFile(target, &dialect);
    if (writer == NULL || !xrtCsvWriterWrite(writer, 2, header) ||
        !xrtCsvWriterWrite(writer, 2, first) ||
        !xrtCsvWriterWrite(writer, 2, second) || !kept(target) ||
        !xrtCsvWriterClose(writer)) goto done;
    xrtCsvWriterDestroy(writer);
    writer = NULL;

    reader = xrtCsvReaderOpenFile(target, NULL);
    if (reader == NULL || !xrtCsvReaderNext(reader, &row) ||
        row.iCount != 2 || strcmp(row.pFields[1], "qty") != 0) goto done;
    while (xrtCsvReaderNext(reader, &row)) {
        char* end;
        long qty;
        if (row.iCount != 2) goto done;
        qty = strtol(row.pFields[1], &end, 10);
        if (*end != 0 || qty < 0 || qty > 100) goto done;
        total += (int)qty;
        ++rows;
    }
    if (!xrtCsvReaderEOF(reader) || rows != 2 || total != 5) goto done;
    status = 0;
done:
    xrtCsvWriterDestroy(writer);
    xrtCsvReaderDestroy(reader);
    if (target != NULL && xrtFileExists(target) && !xrtFileDelete(target)) status = 1;
    if (directory != NULL && !xrtDirRemove(directory)) status = 1;
    xrtFree(target);
    xrtFree(directory);
    if (status == 0) {
        puts("uncommitted: preserved");
        printf("committed: %d rows, total %d\n", rows, total);
    }
    return status;
}

/* 教学范例可单文件运行；正式扩展包仍按清单分别编译。 */
#include "../xrt_csv_common.c"
#include "../xrt_csv_reader.c"
#include "../xrt_csv_writer.c"
