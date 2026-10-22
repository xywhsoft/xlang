/* 单文件范例与正式包使用相同的 TAR/XRT 实现。 */
#define XRT_MODULE_FILE_ROOT
#define XRT_MODULE_FILE_TEMP
#define XRT_MODULE_FILE_TREE
#define XRT_MODULE_DIR_TEMP
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
    str root = xrtDirTemp(NULL, "xlang-tar-append-c-", NULL);
    str archive = NULL;
    xrttarwriter *writer = NULL;
    xrttarreader *reader = NULL;
    xrttarentry entry;
    xbuffer *body = NULL;
    const char *names[] = {"first.txt", "second.txt"};
    const char *contents[] = {"first", "second"};
    int result = 1;
    int i;

    if (root == NULL) goto done;
    archive = xrtPathJoin(root, "entries.tar");
    if (archive == NULL) goto done;

    /* 关闭第一个写入器后，追加写入器只添加第二个条目。 */
    writer = xrtTarWriterOpenFile(archive, NULL);
    xrtTarEntryInit(&entry);
    entry.sName = names[0];
    entry.iType = XRT_TAR_ENTRY_FILE;
    entry.iMode = 0644u;
    if (writer == NULL || !xrtTarWriterAdd(writer, &entry, contents[0], 5u) ||
        !xrtTarWriterClose(writer)) goto done;
    xrtTarWriterDestroy(writer);
    writer = xrtTarWriterOpenAppend(archive, NULL);
    entry.sName = names[1];
    if (writer == NULL || !xrtTarWriterAdd(writer, &entry, contents[1], 6u) ||
        !xrtTarWriterClose(writer)) goto done;

    reader = xrtTarReaderOpenFile(archive, NULL);
    if (reader == NULL) goto done;
    for (i = 0; i < 2; ++i) {
        if (!xrtTarReaderNext(reader, &entry) || strcmp(entry.sName, names[i]) != 0)
            goto done;
        body = xrtTarReaderRead(reader);
        if (body == NULL || body->Size != strlen(contents[i]) ||
            memcmp(body->Data, contents[i], body->Size) != 0) goto done;
        puts(contents[i]);
        xrtBufferDestroy(body);
        body = NULL;
    }
    if (xrtTarReaderNext(reader, &entry) || !xrtTarReaderEOF(reader)) goto done;
    result = 0;

done:
    xrtBufferDestroy(body);
    xrtTarReaderDestroy(reader);
    xrtTarWriterDestroy(writer);
    xrtFree(archive);
    if (root != NULL && !xrtDirRemoveAll(root)) result = 1;
    xrtFree(root);
    if (result != 0) fputs("TAR append example failed\n", stderr);
    return result;
}
