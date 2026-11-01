/* 与XLang范例相同的输入/输出；所有操作都在内存中进行。 */
#include "../include/xrt_csv.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    const str fields[] = {"\xef\xbb\xbf" "title"};
    for (unsigned bom = 0; bom < 2u; ++bom) {
        xrtcsvdialect d;
        xrtcsvrow row;
        xrtCsvDialectInit(&d);
        d.bWriteBOM = bom != 0u;
        xrtcsvwriter* writer = xrtCsvWriterCreateMemory(&d);
        xbuffer* data = NULL;
        xrtcsvreader* reader = NULL;
        bool ok = writer != NULL && xrtCsvWriterWrite(writer, 1u, fields) && xrtCsvWriterClose(writer);
        if (ok) { data = xrtCsvWriterTakeBuffer(writer); ok = data != NULL; }
        if (ok) ok = data->Size == (bom ? 12u : 11u) && data->Data[0] == (bom ? 239u : 34u);
        if (ok) { reader = xrtCsvReaderOpenMemory(data->Data, data->Size, NULL); ok = reader != NULL; }
        if (ok) ok = xrtCsvReaderNext(reader, &row) && row.iCount == 1u && strcmp(row.pFields[0], fields[0]) == 0 &&
            !xrtCsvReaderNext(reader, &row) && xrtCsvReaderEOF(reader) && xrtGetError() == NULL;
        /* C读取器借用data，先释放读取器，再释放缓冲。 */
        xrtCsvReaderDestroy(reader);
        xrtBufferDestroy(data);
        xrtCsvWriterDestroy(writer);
        if (!ok) return 1;
    }
    puts("plain bytes: 11\nfirst byte: 34\nvalue bytes: 8\nwith BOM bytes: 12\nroundtrip: preserved");
    return 0;
}

#include "../xrt_csv_common.c"
#include "../xrt_csv_reader.c"
#include "../xrt_csv_writer.c"
