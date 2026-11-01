#define XRT_MODULE_DIR_TEMP
#define XRT_MODULE_FILE_TREE
#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_PATH
#include "../include/xrt_zip.h"
#include <stdio.h>
#include <string.h>

static bool contains(const char* filename, const char* expected)
{
    size_t size = 0;
    unsigned char* data = xrtFileReadAll(filename, &size);
    bool same = data && size == strlen(expected) && memcmp(data, expected, size) == 0;
    xrtFree(data);
    return same;
}

int main(void)
{
    xrtziparchive *writer = xrtZipArchiveCreate(NULL), *reader = NULL;
    xbuffer* packed = NULL;
    char *root = NULL, *destination = NULL, *filename = NULL;
    int result = 1;
    if (!writer || !xrtZipArchiveAddDirectory(writer, "目录/") ||
        !xrtZipArchiveAdd(writer, "目录/内容.txt", "你好 ZIP", strlen("你好 ZIP"),
            false, XRT_ZIP_METHOD_STORE, 0) || !xrtZipArchiveClose(writer)) goto done;
    packed = xrtZipArchiveTakeBuffer(writer);
    reader = packed ? xrtZipArchiveOpenMemory(packed->Data, packed->Size, false, NULL) : NULL;
    root = xrtDirTemp(NULL, "xlang-zip-files-", "");
    destination = root ? xrtPathJoin(root, "output") : NULL;
    filename = destination ? xrtPathJoin(destination, "目录/内容.txt") : NULL;
    if (!reader || !xrtZipArchiveIsOpen(reader) || !filename ||
        !xrtZipArchiveExtractAll(reader, destination, false) ||
        xrtZipArchiveCount(reader) != 2 || !contains(filename, "你好 ZIP")) goto done;
    puts("entries: 2");
    puts("utf8 path: true");
    if (!xrtFileWriteAll(filename, (xbytesview){(const unsigned char*)"changed", 7}) ||
        xrtZipArchiveExtractAll(reader, destination, false) ||
        xrtZipArchiveError(reader)->iCode != XRT_ZIP_ERROR_EXISTS ||
        !contains(filename, "changed")) goto done;
    puts("no overwrite: preserved");
    if (!xrtZipArchiveExtractAll(reader, destination, true) || !contains(filename, "你好 ZIP")) goto done;
    puts("overwrite: updated");
    result = ferror(stdout) ? 1 : 0;
done:
    xrtZipArchiveDestroy(reader);
    xrtZipArchiveDestroy(writer);
    xrtBufferDestroy(packed);
    // 仅删除排他创建的目录，不接收或递归清理任何用户路径。
    if (root && !xrtDirRemoveAll(root)) result = 1;
    xrtFree(filename);
    xrtFree(destination);
    xrtFree(root);
    return result;
}
