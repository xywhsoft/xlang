/* 首次包含 XRT 之前选择与 TAR 核心相同的模块。 */
#define XRT_MODULE_FILE_ROOT
#define XRT_MODULE_FILE_TEMP
#define XRT_MODULE_FILE_TREE
#define XRT_MODULE_IO_FILE
#define XRT_MODULE_IO_BUFFER
#define XRT_MODULE_PATH_SAFE
#define XRT_MODULE_STRING_FORMAT
#define XRT_MODULE_SET
#define XRT_MODULE_HASH64
#define XRT_MODULE_NUMBER_INTEGER
#include "../include/xrt_tar.h"
#include "../xrt_tar_common.c"
#include "../xrt_tar_reader.c"
#include "../xrt_tar_writer.c"
#include <stdio.h>
#include <string.h>

int main(void)
{
    xrttarwriter *writer = xrtTarWriterCreateMemory(NULL);
    xrttarreader *reader = NULL;
    xbuffer *packed = NULL;
    xbuffer *body = NULL;
    xrttarentry entry;
    int ok = 0;

    if (!writer) goto done;
    xrtTarEntryInit(&entry);
    entry.sName = "hello.txt";
    entry.iType = XRT_TAR_ENTRY_FILE;
    entry.iMode = 0644u;
    if (!xrtTarWriterAdd(writer, &entry, "hello", 5u) ||
        !xrtTarWriterClose(writer)) goto done;
    packed = xrtTarWriterTakeBuffer(writer);
    if (!packed) goto done;

    reader = xrtTarReaderOpenMemory(packed->Data, packed->Size, NULL);
    if (!reader || !xrtTarReaderNext(reader, &entry) ||
        strcmp(entry.sName, "hello.txt") != 0) goto done;
    body = xrtTarReaderRead(reader);
    if (!body || body->Size != 5u || memcmp(body->Data, "hello", 5u) != 0 ||
        xrtTarReaderNext(reader, &entry) || !xrtTarReaderEOF(reader)) goto done;
    puts("hello");
    ok = 1;

done:
    xrtBufferDestroy(body);
    xrtTarReaderDestroy(reader);
    xrtBufferDestroy(packed);
    xrtTarWriterDestroy(writer);
    return ok ? 0 : 1;
}
