#ifndef XRT_MODULE_IO_FILE
#define XRT_MODULE_IO_FILE
#endif
#ifndef XRT_MODULE_FILE_TEMP
#define XRT_MODULE_FILE_TEMP
#endif
#include "include/xrt_brotli.h"
#include "brotli_error.h"

/* 同目录独占临时文件；最终回调、刷新与关闭都成功后才提交。 */
static bool brotli_file(const char* source, const char* destination, bool compress,
    bool overwrite, const xrtbrotlioptions* options, xrtprogressproc progress, ptr user)
{
    xreader* reader = NULL;
    xwriter* writer = NULL;
    xfile file = NULL, transaction = NULL;
    str directory = NULL, temporary = NULL;
    xrtbrotlierror error;
    xerror* saved;
    bool ok = false, removed = false;
    xrtClearError();
    if (source == NULL || source[0] == 0 || destination == NULL || destination[0] == 0) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.brotli", XRT_BROTLI_ERROR_ARGUMENT,
            "brotli source and destination paths are required");
        return false;
    }
    if (!overwrite) {
        xfileinfo info;
        if (xrtPathStat(destination, false, &info)) {
            xrtSetErrorInfo(XERR_EXISTS, "xlang.extlib.brotli", XRT_BROTLI_ERROR_IO,
                "brotli destination already exists");
            return false;
        }
        if (xrtErrorKind(xrtGetError()) != XERR_NOT_FOUND) return false;
        xrtClearError();
    }
    reader = xrtReaderOpen(source);
    if (reader == NULL) goto cleanup;
    directory = xrtPathParent(destination);
    if (directory == NULL) goto cleanup;
    file = xrtFileTemp(directory[0] != 0 ? directory : ".", ".xrt-brotli-", ".tmp", &temporary);
    if (file == NULL) goto cleanup;
    transaction = file; /* 仅借用到 Writer/文件关闭，不增加引用或重复关闭。 */
    writer = xrtWriterTakeFile(&file);
    if (writer == NULL) goto cleanup;
    ok = compress ? xrtBrotliCompressStream(reader, writer, options, progress, user, &error) :
        xrtBrotliDecompressStream(reader, writer, options, progress, user, &error);
    if (!ok && xrtGetError() == NULL) brotli_publish_error(&error);
    if (ok) ok = xrtWriterFlush(writer);

cleanup:
    // 保留正文/回调首因；仍打开时按句柄回滚，避免 OOM 下重新转换路径。
    saved = xrtTakeError();
    if (saved != NULL) ok = false;
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
    if (saved != NULL) xrtSetErrorTake(saved);
    else if (!ok) xrtSetErrorInfo(XERR_IO, "xlang.extlib.brotli", XRT_BROTLI_ERROR_IO,
        "brotli file operation failed");
    xrtFree(directory);
    xrtFree(temporary);
    return ok;
}

XRT_API bool xrtBrotliCompressFile(const char* source, const char* destination,
    const xrtbrotlioptions* options, bool overwrite, xrtprogressproc progress, ptr user)
{ return brotli_file(source, destination, true, overwrite, options, progress, user); }

XRT_API bool xrtBrotliDecompressFile(const char* source, const char* destination,
    const xrtbrotlioptions* options, bool overwrite, xrtprogressproc progress, ptr user)
{ return brotli_file(source, destination, false, overwrite, options, progress, user); }
