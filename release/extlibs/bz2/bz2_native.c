#ifndef XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_BUFFER
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
#include "include/xrt_bz2.h"
#include "include/bz2_native.h"


// ----------------------------- 参数与文件事务 -----------------------------

static void bz2_options(xrtbz2options* options, int level, int workFactor,
    bool small, unsigned long long maxInput, unsigned long long maxOutput, unsigned int maxRatio)
{
    xrtBz2OptionsInit(options);
    options->iLevel = level;
    options->iWorkFactor = workFactor;
    options->bSmallMemory = small;
    options->tLimits.iMaxInputBytes = maxInput;
    options->tLimits.iMaxOutputBytes = maxOutput;
    options->tLimits.iMaxCompressionRatio = maxRatio;
}

/* 原生层只把借用的 xcall 回调接到公共 C 文件 API。 */
static int bz2_file(const char* source, const char* destination, bool compress,
    bool overwrite, const xrtbz2options* options, xvalue* callback)
{
    xrtprogresscall progress;
    const xerror* error;
    bool ok;
    xrtProgressCallInit(&progress, callback);
    ok = compress ? xrtBz2CompressFile(source, destination, options, overwrite,
            progress.Callback != NULL ? xrtProgressCallInvoke : NULL, &progress) :
        xrtBz2DecompressFile(source, destination, options, overwrite,
            progress.Callback != NULL ? xrtProgressCallInvoke : NULL, &progress);
    if (ok) return 1;
    if (progress.InvokeFailed) return -2;
    error = xrtGetError();
    return error != NULL && xrtErrorKind(error) == XERR_CANCELLED ? -1 : 0;
}


// ----------------------------- 与 xlang 严格等价的物理 ABI -----------------------------

void* xbz2_compress(void* data, size_t size, int level, unsigned long long maxInput, unsigned long long maxOutput)
{
    xrtbz2options options;
    xbuffer* output = NULL;
    bz2_options(&options, level, 30, false, maxInput, maxOutput, 0);
    return xrtBz2Compress(data, size, &options, &output, NULL) ? output : NULL;
}
void* xbz2_compress_str(const char* text, size_t size, int level, unsigned long long maxInput, unsigned long long maxOutput)
{ return xbz2_compress((void*)text, size, level, maxInput, maxOutput); }
void* xbz2_decompress(void* data, size_t size, bool small, unsigned long long maxInput, unsigned long long maxOutput, unsigned int ratio)
{
    xrtbz2options options;
    xbuffer* output = NULL;
    bz2_options(&options, 9, 30, small, maxInput, maxOutput, ratio);
    return xrtBz2Decompress(data, size, &options, &output, NULL) ? output : NULL;
}

/* 验证 UTF-8 后移交解压缓冲；终止字节扩容时仍由 XRT 管理所有权。 */
void* xbz2_decompress_text(void* data, size_t size, bool small,
    unsigned long long maxInput, unsigned long long maxOutput, unsigned int ratio)
{
    xbuffer* output = (xbuffer*)xbz2_decompress(data, size, small, maxInput, maxOutput, ratio);
    if (output == NULL) return NULL;
    if (!xrtUtf8Valid((xstrview){(const char*)output->Data, output->Size}, NULL)) {
        xrtSetErrorInfo(XERR_VALUE, "xlang.extlib.bz2", 0,
            "bz2 decompressed text is not valid UTF-8");
        xrtBufferDestroy(output);
        return NULL;
    }
    if (!xrtBufferAppendByte(output, 0)) {
        xrtBufferDestroy(output);
        return NULL;
    }
    return output;
}

size_t xbz2_text_size(void* raw)
{
    const xbuffer* output = (const xbuffer*)raw;
    return output != NULL && output->Size != 0u ? output->Size - 1u : 0u;
}

void* xbz2_text_take(void* raw)
{
    xbuffer* output = (xbuffer*)raw;
    void* text;
    if (output == NULL) return NULL;
    text = xrtBufferTake(output, NULL, NULL);
    xrtBufferDestroy(output);
    return text;
}

int xbz2_compress_file(const char* source, const char* dest, int level, bool overwrite,
    unsigned long long maxInput, unsigned long long maxOutput, xvalue* callback)
{
    xrtbz2options options;
    bz2_options(&options, level, 30, false, maxInput, maxOutput, 0);
    return bz2_file(source, dest, true, overwrite, &options, callback);
}
int xbz2_decompress_file(const char* source, const char* dest, bool small, bool overwrite,
    unsigned long long maxInput, unsigned long long maxOutput, unsigned int ratio, xvalue* callback)
{
    xrtbz2options options;
    bz2_options(&options, 9, 30, small, maxInput, maxOutput, ratio);
    return bz2_file(source, dest, false, overwrite, &options, callback);
}
void* xbz2_compressor_create(int level, int workFactor, unsigned long long maxInput, unsigned long long maxOutput)
{
    xrtbz2options options;
    bz2_options(&options, level, workFactor, false, maxInput, maxOutput, 0);
    return xrtBz2CompressorCreate(&options);
}
void xbz2_compressor_destroy(void* c) { xrtBz2CompressorDestroy(c); }
bool xbz2_compressor_update(void* c, void* data, size_t size) { return xrtBz2CompressorUpdate(c, data, size); }
bool xbz2_compressor_update_str(void* c, const char* text, size_t size) { return xrtBz2CompressorUpdate(c, text, size); }
bool xbz2_compressor_finish(void* c) { return xrtBz2CompressorFinish(c); }
void* xbz2_compressor_drain(void* c) { return xrtBz2CompressorDrainBuffer(c); }
unsigned long long xbz2_compressor_input_bytes(void* c) { return xrtBz2CompressorInputBytes(c); }
unsigned long long xbz2_compressor_output_bytes(void* c) { return xrtBz2CompressorOutputBytes(c); }
bool xbz2_compressor_finished(void* c) { return xrtBz2CompressorFinished(c); }
void* xbz2_decompressor_create(bool small, unsigned long long maxInput, unsigned long long maxOutput, unsigned int ratio)
{
    xrtbz2options options;
    bz2_options(&options, 9, 30, small, maxInput, maxOutput, ratio);
    return xrtBz2DecompressorCreate(&options);
}
void xbz2_decompressor_destroy(void* d) { xrtBz2DecompressorDestroy(d); }
bool xbz2_decompressor_update(void* d, void* data, size_t size) { return xrtBz2DecompressorUpdate(d, data, size); }
bool xbz2_decompressor_finish(void* d) { return xrtBz2DecompressorFinish(d); }
void* xbz2_decompressor_drain(void* d) { return xrtBz2DecompressorDrainBuffer(d); }
unsigned long long xbz2_decompressor_input_bytes(void* d) { return xrtBz2DecompressorInputBytes(d); }
unsigned long long xbz2_decompressor_output_bytes(void* d) { return xrtBz2DecompressorOutputBytes(d); }
bool xbz2_decompressor_finished(void* d) { return xrtBz2DecompressorFinished(d); }
unsigned long long xbz2_decompressor_members(void* d) { return xrtBz2DecompressorMembers(d); }
