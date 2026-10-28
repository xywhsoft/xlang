/* 单文件范例可由 xl run/build 直接执行；正式包仍独立编译原生单元。 */
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

int main(void)
{
    xrttarwriter* writer = xrtTarWriterCreateMemory(NULL);
    xrttarreader* reader = NULL;
    xbuffer* packed = NULL;
    xrttarentry entry;
    int ok = 0;
    /* PAX 使 ID 不受传统八字节八进制字段的范围限制。 */
    xrtTarEntryInit(&entry);
    entry.sName = "metadata.txt";
    entry.iMode = 0644u;
    entry.iMTime = -1;
    entry.iUID = UINT64_MAX;
    entry.iGID = (uint64)INT64_MAX + 1u;
    if (writer == NULL || !xrtTarWriterAdd(writer, &entry, NULL, 0) ||
        !xrtTarWriterClose(writer)) goto done;
    packed = xrtTarWriterTakeBuffer(writer);
    if (packed == NULL) goto done;
    reader = xrtTarReaderOpenMemory(packed->Data, packed->Size, NULL);
    if (reader == NULL || !xrtTarReaderNext(reader, &entry) || entry.iMTime != -1 || entry.iMode != 0644u ||
        entry.iUID != UINT64_MAX || entry.iGID != (uint64)INT64_MAX + 1u) goto done;
    printf("mtime=%lld;uid=%llu;gid=%llu\n", (long long)entry.iMTime,
        (unsigned long long)entry.iUID, (unsigned long long)entry.iGID);
    ok = !xrtTarReaderNext(reader, &entry) && xrtTarReaderEOF(reader);
done:
    xrtTarReaderDestroy(reader);
    xrtBufferDestroy(packed);
    xrtTarWriterDestroy(writer);
    return ok ? 0 : 1;
}
