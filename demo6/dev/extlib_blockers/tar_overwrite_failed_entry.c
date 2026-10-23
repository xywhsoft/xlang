#define XRT_MODULE_DIR_TEMP
#include "xrt_tar_internal.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    str root = xrtDirTemp(NULL, "xlang-tar-atomicity-", NULL);
    str output = NULL;
    str target = NULL;
    xrttarwriter* writer = NULL;
    xrttarreader* reader = NULL;
    xbuffer* archive = NULL;
    bytes actual = NULL;
    size_t actual_size = 0u;
    xrttarentry entry;
    int result = 1;

    if (root == NULL) goto done;
    output = xrtPathJoin(root, "output");
    if (output == NULL) goto done;
    target = xrtPathJoin(output, "one.txt");
    if (target == NULL || !xrtDirCreateAll(output) ||
        !xrtFileWriteAll(target, XRT_BYTES_LITERAL("OLD"))) goto done;

    writer = xrtTarWriterCreateMemory(NULL);
    if (writer == NULL) goto done;
    xrtTarEntryInit(&entry);
    entry.iType = XRT_TAR_ENTRY_FILE;
    entry.sName = "one.txt";
    if (!xrtTarWriterAdd(writer, &entry, "NEW", 3u) ||
        !xrtTarWriterClose(writer)) goto done;
    archive = xrtTarWriterTakeBuffer(writer);
    if (archive == NULL) goto done;

    /* 条目头完整，但正文缺少最后一个字节；该条目本身必须失败。 */
    reader = xrtTarReaderOpenMemory(archive->Data, 512u + 2u, NULL);
    if (reader == NULL ||
        xrtTarReaderExtractAll(reader, output, true) ||
        xrtTarReaderError(reader)->iCode == XRT_TAR_OK) goto done;
    actual = xrtFileReadAll(target, &actual_size);
    if (actual == NULL) goto done;

    if (actual_size != 3u || memcmp(actual, "OLD", 3u) != 0) {
        puts("reproduced: failed entry changed existing OLD file");
        result = 0;
    }

done:
    xrtFree(actual);
    xrtTarReaderDestroy(reader);
    xrtBufferDestroy(archive);
    xrtTarWriterDestroy(writer);
    xrtFree(target);
    xrtFree(output);
    if (root != NULL && !xrtDirRemoveAll(root)) result = 1;
    xrtFree(root);
    if (result != 0) fputs("TAR atomicity reproduction failed\n", stderr);
    return result;
}
