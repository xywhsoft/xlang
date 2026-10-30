/* 与 memory_roundtrip.xl 同输入同断言；拥有型读取结果跨归档关闭存活。 */
#ifndef XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_FILE_WHOLE
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#include "../include/xrt_zip.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    xrtzipoptions options;
    xrtziparchive *writer = NULL, *reader = NULL;
    xbuffer *packed = NULL, *snapshot = NULL;
    xrtzipentry info;
    char *comment = NULL;
    unsigned char *payload = NULL;
    int result = 1;
    if (argc != 1 && argc != 2) return 2;
    xrtZipOptionsInit(&options);
    options.iDefaultMethod = XRT_ZIP_METHOD_STORE;
    options.iDefaultLevel = 0;
    writer = xrtZipArchiveCreate(&options);
    payload = (unsigned char *)xrtMalloc(131072);
    if (!writer || !payload || !xrtZipArchiveIsOpen(writer)) goto done;
    memset(payload, 'z', 131072);
    if (!xrtZipArchiveAdd(writer, "payload.txt", payload, 131072, false, XRT_ZIP_METHOD_STORE, 0) ||
        !xrtZipArchiveSetEntryComment(writer, "payload.txt", "stable")) goto done;
    xrtFree(payload); payload = NULL;
    if (!xrtZipArchiveClose(writer)) goto done;
    packed = xrtZipArchiveTakeBuffer(writer);
    if (!packed) goto done;
    if (argc == 2 && !xrtFileWriteAtomic(argv[1], xrtBufferView(packed))) goto done;
    reader = xrtZipArchiveOpenMemory(packed->Data, packed->Size, false, NULL);
    if (!reader || !xrtZipArchiveIsOpen(reader) || xrtZipArchiveCount(reader) != 1 ||
        !xrtZipArchiveEntryAt(reader, 0, &info) || info.iUncompressedSize != 131072 ||
        strcmp(info.sName, "payload.txt") || strcmp(info.sComment, "stable")) goto done;
    /* 元数据字符串借用归档；跨关闭持有时显式复制。 */
    comment = xrtStrDup(info.sComment);
    snapshot = xrtZipArchiveRead(reader, "payload.txt");
    if (!comment || !snapshot || snapshot->Size != 131072 || !xrtZipArchiveClose(reader)) goto done;
    for (size_t i = 0; i < snapshot->Size; ++i) if (snapshot->Data[i] != 'z') goto done;
    printf("entries: 1\nbytes: 131072\ncomment: %s\nsnapshot: true\n", comment);
    result = ferror(stdout) ? 1 : 0;
done:
    xrtFree(payload);
    xrtFree(comment);
    xrtBufferDestroy(snapshot);
    xrtZipArchiveDestroy(reader);
    xrtBufferDestroy(packed);
    xrtZipArchiveDestroy(writer);
    return result;
}
