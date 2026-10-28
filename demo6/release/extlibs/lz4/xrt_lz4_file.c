#ifndef XRT_MODULE_IO_FILE
#define XRT_MODULE_IO_FILE
#endif
#ifndef XRT_MODULE_FILE_TEMP
#define XRT_MODULE_FILE_TEMP
#endif
#include "include/xrt_lz4.h"

/* 同目录临时文件：处理、最终通知、flush/close 全成功后才提交。 */
static bool lz4_file(const char* source, const char* destination, bool compress,
    bool overwrite, const xrtlz4options* options, xrtprogressproc progress, ptr user)
{
    xreader* reader = NULL;
    xwriter* writer = NULL;
    xfile file = NULL, transaction = NULL;
    str directory = NULL, temporary = NULL;
    xerror* saved;
    bool ok = false, removed = false;
    xrtClearError();
    if (source == NULL || source[0] == 0 || destination == NULL || destination[0] == 0) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.lz4", XRT_LZ4_ERROR_ARGUMENT,
            "lz4 source and destination paths are required");
        return false;
    }
    if (!overwrite) {
        xfileinfo info;
        if (xrtPathStat(destination, false, &info)) {
            xrtSetErrorInfo(XERR_EXISTS, "xlang.extlib.lz4", XRT_LZ4_ERROR_IO, "lz4 destination already exists");
            return false;
        }
        if (xrtErrorKind(xrtGetError()) != XERR_NOT_FOUND) return false;
        xrtClearError();
    }
    reader = xrtReaderOpen(source);
    if (reader == NULL) goto cleanup;
    directory = xrtPathParent(destination);
    if (directory == NULL) goto cleanup;
    file = xrtFileTemp(directory[0] != 0 ? directory : ".", ".xrt-lz4-", ".tmp", &temporary);
    if (file == NULL) goto cleanup;
    transaction = file; /* 借用至 Writer/文件关闭，不额外持有或关闭。 */
    writer = xrtWriterTakeFile(&file);
    if (writer == NULL) goto cleanup;
    ok = compress ? xrtLz4CompressStream(reader, writer, options, progress, user, NULL) :
        xrtLz4DecompressStream(reader, writer, options, progress, user, NULL);
    if (ok) ok = xrtWriterFlush(writer);

cleanup:
    // 清理错误不能覆盖首个失败，包括调用方的回调错误。
    saved = xrtTakeError();
    if (saved != NULL) ok = false;
    // 正文失败时文件仍打开；按句柄回滚，避免 OOM 下再分配路径转换缓冲。
    if (!ok && transaction != NULL && temporary != NULL)
        removed = xrtFileDeleteOpen(transaction, temporary);
    if (writer != NULL && !xrtWriterDestroy(writer) && ok) { ok = false; saved = xrtTakeError(); }
    xrtClearError();
    if (file != NULL && !xrtClose(file) && ok) { ok = false; saved = xrtTakeError(); }
    xrtClearError();
    if (reader != NULL && !xrtReaderDestroy(reader) && ok) { ok = false; saved = xrtTakeError(); }
    xrtClearError();
    if (ok && !xrtPathRename(temporary, destination, overwrite)) { ok = false; saved = xrtTakeError(); }
    if (!ok && temporary != NULL && !removed) (void)xrtFileDelete(temporary);
    xrtClearError();
    if (saved != NULL) {
        xrtSetError(saved);
        xrtErrorFree(saved);
    } else if (!ok) {
        xrtSetErrorInfo(XERR_IO, "xlang.extlib.lz4", XRT_LZ4_ERROR_IO, "lz4 file operation failed");
    }
    xrtFree(directory);
    xrtFree(temporary);
    return ok;
}

XRT_API bool xrtLz4CompressFile(const char* source, const char* destination,
    const xrtlz4options* options, bool overwrite, xrtprogressproc progress, ptr user)
{ return lz4_file(source, destination, true, overwrite, options, progress, user); }

XRT_API bool xrtLz4DecompressFile(const char* source, const char* destination,
    const xrtlz4options* options, bool overwrite, xrtprogressproc progress, ptr user)
{ return lz4_file(source, destination, false, overwrite, options, progress, user); }
