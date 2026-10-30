#ifndef XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_BUFFER
#endif
#ifndef XRT_MODULE_DEFLATE
#define XRT_MODULE_DEFLATE
#endif
#ifndef XRT_MODULE_INFLATE
#define XRT_MODULE_INFLATE
#endif
#ifndef XRT_MODULE_IO_FILE
#define XRT_MODULE_IO_FILE
#endif
#ifndef XRT_MODULE_FILE_TEMP
#define XRT_MODULE_FILE_TEMP
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#endif
#include "xruntime.h"
#include "include/xrt_gzip.h"
#include "include/gzip_native.h"


// ----------------------------- 原生参数与文件事务 -----------------------------

static void gzip_options(xrtgzipoptions* options, int level, unsigned int mtime,
    unsigned long long maxInput, unsigned long long maxOutput, unsigned int maxRatio)
{
    xrtGzipOptionsInit(options);
    options->iLevel = level;
    options->iMTime = mtime;
    options->tLimits.iMaxInputBytes = maxInput;
    options->tLimits.iMaxOutputBytes = maxOutput;
    options->tLimits.iMaxCompressionRatio = maxRatio;
}

/* 临时文件与目标位于同目录；所有阶段成功后才替换目标，包含最终进度通知。 */
static int gzip_file(const char* source, const char* destination, bool compress,
    bool overwrite, const xrtgzipoptions* options, xvalue* callback)
{
    xreader* reader = NULL;
    xwriter* writer = NULL;
    xfile file = NULL;
    xfile transaction = NULL;
    str directory = NULL, temporary = NULL;
    xerror* saved;
    xrtgziperror error;
    xrtprogresscall progress;
    bool ok = false;
    bool removed = false;
    int result = 0;
    xrtClearError();
    xrtGzipErrorInit(&error);
    xrtProgressCallInit(&progress, callback);
    if (source == NULL || source[0] == 0 || destination == NULL || destination[0] == 0) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.gzip", XRT_GZIP_ERROR_ARGUMENT,
            "gzip source and destination paths are required");
        return 0;
    }
    if (!overwrite) {
        xfileinfo info;
        if (xrtPathStat(destination, false, &info)) {
            xrtSetErrorInfo(XERR_EXISTS, "xlang.extlib.gzip", XRT_GZIP_ERROR_IO,
                "gzip destination already exists");
            return 0;
        }
        if (xrtErrorKind(xrtGetError()) != XERR_NOT_FOUND) return 0;
        xrtClearError();
    }
    reader = xrtReaderOpen(source);
    if (reader == NULL) goto cleanup;
    directory = xrtPathParent(destination);
    if (directory == NULL) goto cleanup;
    file = xrtFileTemp(directory[0] != 0 ? directory : ".", ".xrt-gzip-", ".tmp", &temporary);
    if (file == NULL) goto cleanup;
    transaction = file; /* 只借用；关闭仍属于原来的 file/writer。 */
    writer = xrtWriterTakeFile(&file);
    if (writer == NULL) goto cleanup;
    ok = compress ? xrtGzipCompressStream(reader, writer, options,
            progress.Callback != NULL ? xrtProgressCallInvoke : NULL, &progress, &error) :
        xrtGzipDecompressStream(reader, writer, options,
            progress.Callback != NULL ? xrtProgressCallInvoke : NULL, &progress, &error);
    if (ok) ok = xrtWriterFlush(writer);

cleanup:
    /* 保存第一处失败；析构、删除临时文件的错误不能覆盖用户回调或 IO 原因。 */
    saved = xrtTakeError();
    if (saved != NULL) ok = false;
    /* 打开态先回滚，避免 Windows 关闭后删除路径还需要 UTF-16 分配。 */
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
    } else if (!ok && !progress.InvokeFailed) {
        xrtSetErrorInfo(XERR_IO, "xlang.extlib.gzip", XRT_GZIP_ERROR_IO, "gzip file operation failed");
    }
    xrtFree(directory);
    xrtFree(temporary);
    if (ok) result = 1;
    else if (progress.InvokeFailed) result = -2;
    else if (error.iCode == XRT_GZIP_ERROR_CANCELLED) result = -1;
    return result;
}


// ----------------------------- 与 xlang 声明严格等价的 ABI -----------------------------

void* xgzip_compress(void* data, size_t size, int level, unsigned int mtime,
    unsigned long long maxInput, unsigned long long maxOutput)
{
    xrtgzipoptions options;
    xbuffer* output = NULL;
    gzip_options(&options, level, mtime, maxInput, maxOutput, 0);
    return xrtGzipCompress(data, size, &options, &output, NULL) ? output : NULL;
}

void* xgzip_compress_str(const char* text, size_t size, int level, unsigned int mtime,
    unsigned long long maxInput, unsigned long long maxOutput)
{ return xgzip_compress((void*)text, size, level, mtime, maxInput, maxOutput); }

void* xgzip_decompress(void* data, size_t size, unsigned long long maxInput,
    unsigned long long maxOutput, unsigned int maxRatio)
{
    xrtgzipoptions options;
    xbuffer* output = NULL;
    gzip_options(&options, -1, 0, maxInput, maxOutput, maxRatio);
    return xrtGzipDecompress(data, size, &options, &output, NULL) ? output : NULL;
}

/* 文本结果取走解压缓冲；容量不足时，追加终止字节可能触发 XRT 扩容。 */
void* xgzip_decompress_text(void* data, size_t size, unsigned long long maxInput,
    unsigned long long maxOutput, unsigned int maxRatio)
{
    xbuffer* output = (xbuffer*)xgzip_decompress(data, size, maxInput, maxOutput, maxRatio);
    if (output == NULL) return NULL;
    if (!xrtUtf8Valid((xstrview){(const char*)output->Data, output->Size}, NULL)) {
        xrtSetErrorInfo(XERR_VALUE, "xlang.extlib.gzip", 0,
            "gzip decompressed text is not valid UTF-8");
        xrtBufferDestroy(output);
        return NULL;
    }
    if (!xrtBufferAppendByte(output, 0)) {
        xrtBufferDestroy(output);
        return NULL;
    }
    return output;
}

size_t xgzip_text_size(void* raw)
{
    const xbuffer* output = (const xbuffer*)raw;
    return output != NULL && output->Size != 0u ? output->Size - 1u : 0u;
}

void* xgzip_text_take(void* raw)
{
    xbuffer* output = (xbuffer*)raw;
    void* text;
    if (output == NULL) return NULL;
    text = xrtBufferTake(output, NULL, NULL);
    xrtBufferDestroy(output);
    return text;
}

int xgzip_compress_file(const char* source, const char* destination, int level,
    unsigned int mtime, bool overwrite, unsigned long long maxInput,
    unsigned long long maxOutput, xvalue* callback)
{
    xrtgzipoptions options;
    gzip_options(&options, level, mtime, maxInput, maxOutput, 0);
    return gzip_file(source, destination, true, overwrite, &options, callback);
}

int xgzip_decompress_file(const char* source, const char* destination, bool overwrite,
    unsigned long long maxInput, unsigned long long maxOutput, unsigned int maxRatio, xvalue* callback)
{
    xrtgzipoptions options;
    gzip_options(&options, -1, 0, maxInput, maxOutput, maxRatio);
    return gzip_file(source, destination, false, overwrite, &options, callback);
}

void* xgzip_compressor_create(int level, unsigned int mtime,
    unsigned long long maxInput, unsigned long long maxOutput)
{
    xrtgzipoptions options;
    gzip_options(&options, level, mtime, maxInput, maxOutput, 0);
    return xrtGzipCompressorCreate(&options);
}
void xgzip_compressor_destroy(void* handle) { xrtGzipCompressorDestroy(handle); }
bool xgzip_compressor_update(void* handle, void* data, size_t size)
{ return xrtGzipCompressorUpdate(handle, data, size); }
bool xgzip_compressor_update_str(void* handle, const char* text, size_t size)
{ return xrtGzipCompressorUpdate(handle, text, size); }
void* xgzip_compressor_finish(void* handle)
{ return xrtGzipCompressorFinish(handle) ? xrtGzipCompressorDrainBuffer(handle) : NULL; }
void* xgzip_compressor_drain(void* handle) { return xrtGzipCompressorDrainBuffer(handle); }
unsigned long long xgzip_compressor_input_bytes(void* handle) { return xrtGzipCompressorInputBytes(handle); }
unsigned long long xgzip_compressor_output_bytes(void* handle) { return xrtGzipCompressorOutputBytes(handle); }
bool xgzip_compressor_finished(void* handle) { return xrtGzipCompressorFinished(handle); }

void* xgzip_decompressor_create(unsigned long long maxInput, unsigned long long maxOutput, unsigned int maxRatio)
{
    xrtgzipoptions options;
    gzip_options(&options, -1, 0, maxInput, maxOutput, maxRatio);
    return xrtGzipDecompressorCreate(&options);
}
void xgzip_decompressor_destroy(void* handle) { xrtGzipDecompressorDestroy(handle); }
bool xgzip_decompressor_update(void* handle, void* data, size_t size)
{ return xrtGzipDecompressorUpdate(handle, data, size); }
void* xgzip_decompressor_finish(void* handle)
{ return xrtGzipDecompressorFinish(handle) ? xrtGzipDecompressorDrainBuffer(handle) : NULL; }
void* xgzip_decompressor_drain(void* handle) { return xrtGzipDecompressorDrainBuffer(handle); }
unsigned long long xgzip_decompressor_input_bytes(void* handle) { return xrtGzipDecompressorInputBytes(handle); }
unsigned long long xgzip_decompressor_output_bytes(void* handle) { return xrtGzipDecompressorOutputBytes(handle); }
bool xgzip_decompressor_finished(void* handle) { return xrtGzipDecompressorFinished(handle); }
