#ifndef XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_BUFFER
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#endif
#include "xruntime.h"
#include "include/xrt_lz4.h"
#include "include/lz4_native.h"
#include "third_party/lz4.h"


// ----------------------------- 参数与进度回调 -----------------------------

static void lz4_options(xrtlz4options* options, int acceleration,
    unsigned long long maxInput, unsigned long long maxOutput, unsigned int ratio)
{
    xrtLz4OptionsInit(options);
    options->iAcceleration = acceleration;
    options->tLimits.iMaxInputBytes = maxInput;
    options->tLimits.iMaxOutputBytes = maxOutput;
    options->tLimits.iMaxCompressionRatio = ratio;
}

/* XRT 区分主动取消和调用失败；无结果的 C callable 不能被误报为用户取消。 */
static bool lz4_progress(const xrtprogress* value, ptr user)
{
    xrtprogresscall* state = user;
    bool ok = xrtProgressCallInvoke(value, state);
    if (!ok && state->InvokeFailed && xrtGetError() == NULL)
        xrtSetErrorInfo(XERR_STATE, "xlang.extlib.lz4", XRT_LZ4_ERROR_STATE,
            "lz4 progress callback failed to return a boolean");
    return ok;
}

/* 只适配借用回调；文件事务由 C 和 xlang 共用的公共接口完成。 */
static int lz4_file(const char* source, const char* destination, bool compress,
    bool overwrite, const xrtlz4options* options, xvalue* callback)
{
    xrtprogresscall progress;
    bool ok;
    xrtProgressCallInit(&progress, callback);
    ok = compress ? xrtLz4CompressFile(source, destination, options, overwrite,
        progress.Callback != NULL ? lz4_progress : NULL, &progress) :
        xrtLz4DecompressFile(source, destination, options, overwrite,
        progress.Callback != NULL ? lz4_progress : NULL, &progress);
    if (ok) return 1;
    if (progress.InvokeFailed) return -2;
    return xrtErrorKind(xrtGetError()) == XERR_CANCELLED ? -1 : 0;
}


// ----------------------------- 与 xlang 严格等价的物理 ABI -----------------------------

bool xlz4_available(void) { return true; }
const char* xlz4_version(void) { return LZ4_versionString(); }
const char* xlz4_last_error(void) { return xrtErrorMessage(xrtGetError()); }

void* xlz4_compress(void* data, size_t size, int acceleration,
    unsigned long long maxInput, unsigned long long maxOutput)
{
    xrtlz4options options;
    xbuffer* output = NULL;
    lz4_options(&options, acceleration, maxInput, maxOutput, 0);
    return xrtLz4Compress(data, size, &options, &output, NULL) ? output : NULL;
}
void* xlz4_compress_str(const char* text, size_t size, int acceleration,
    unsigned long long maxInput, unsigned long long maxOutput)
{ return xlz4_compress((void*)text, size, acceleration, maxInput, maxOutput); }

void* xlz4_decompress(void* data, size_t size,
    unsigned long long maxInput, unsigned long long maxOutput, unsigned int ratio)
{
    xrtlz4options options;
    xbuffer* output = NULL;
    lz4_options(&options, 1, maxInput, maxOutput, ratio);
    return xrtLz4Decompress(data, size, &options, &output, NULL) ? output : NULL;
}

/* 文本结果直接取走解压缓冲；末尾 NUL 可能触发 XRT 扩容。 */
void* xlz4_decompress_text(void* data, size_t size,
    unsigned long long maxInput, unsigned long long maxOutput, unsigned int ratio)
{
    xbuffer* output = (xbuffer*)xlz4_decompress(data, size, maxInput, maxOutput, ratio);
    if (output == NULL) return NULL;
    if (!xrtUtf8Valid((xstrview){(const char*)output->Data, output->Size}, NULL)) {
        xrtSetErrorInfo(XERR_VALUE, "xlang.extlib.lz4", 0,
            "lz4 decompressed text is not valid UTF-8");
        xrtBufferDestroy(output);
        return NULL;
    }
    if (!xrtBufferAppendByte(output, 0)) {
        xrtBufferDestroy(output);
        return NULL;
    }
    return output;
}

size_t xlz4_text_size(void* raw)
{
    const xbuffer* output = (const xbuffer*)raw;
    return output != NULL && output->Size != 0u ? output->Size - 1u : 0u;
}

void* xlz4_text_take(void* raw)
{
    xbuffer* output = (xbuffer*)raw;
    void* text;
    if (output == NULL) return NULL;
    text = xrtBufferTake(output, NULL, NULL);
    xrtBufferDestroy(output);
    return text;
}
int xlz4_compress_file(const char* source, const char* destination, int acceleration,
    bool overwrite, unsigned long long maxInput, unsigned long long maxOutput, xvalue* callback)
{
    xrtlz4options options;
    lz4_options(&options, acceleration, maxInput, maxOutput, 0);
    return lz4_file(source, destination, true, overwrite, &options, callback);
}
int xlz4_decompress_file(const char* source, const char* destination, bool overwrite,
    unsigned long long maxInput, unsigned long long maxOutput, unsigned int ratio, xvalue* callback)
{
    xrtlz4options options;
    lz4_options(&options, 1, maxInput, maxOutput, ratio);
    return lz4_file(source, destination, false, overwrite, &options, callback);
}

void* xlz4_compressor_create(int acceleration, unsigned long long maxInput, unsigned long long maxOutput)
{
    xrtlz4options options;
    lz4_options(&options, acceleration, maxInput, maxOutput, 0);
    return xrtLz4CompressorCreate(&options);
}
void xlz4_compressor_destroy(void* c) { xrtLz4CompressorDestroy(c); }
bool xlz4_compressor_update(void* c, void* data, size_t size) { return xrtLz4CompressorUpdate(c, data, size); }
bool xlz4_compressor_update_str(void* c, const char* text, size_t size) { return xrtLz4CompressorUpdate(c, text, size); }
bool xlz4_compressor_finish(void* c) { return xrtLz4CompressorFinish(c); }
void* xlz4_compressor_drain(void* c) { return xrtLz4CompressorDrainBuffer(c); }
unsigned long long xlz4_compressor_input_bytes(void* c) { return xrtLz4CompressorInputBytes(c); }
unsigned long long xlz4_compressor_output_bytes(void* c) { return xrtLz4CompressorOutputBytes(c); }

void* xlz4_decompressor_create(unsigned long long maxInput, unsigned long long maxOutput, unsigned int ratio)
{
    xrtlz4options options;
    lz4_options(&options, 1, maxInput, maxOutput, ratio);
    return xrtLz4DecompressorCreate(&options);
}
void xlz4_decompressor_destroy(void* d) { xrtLz4DecompressorDestroy(d); }
bool xlz4_decompressor_update(void* d, void* data, size_t size) { return xrtLz4DecompressorUpdate(d, data, size); }
bool xlz4_decompressor_finish(void* d) { return xrtLz4DecompressorFinish(d); }
void* xlz4_decompressor_drain(void* d) { return xrtLz4DecompressorDrainBuffer(d); }
unsigned long long xlz4_decompressor_input_bytes(void* d) { return xrtLz4DecompressorInputBytes(d); }
unsigned long long xlz4_decompressor_output_bytes(void* d) { return xrtLz4DecompressorOutputBytes(d); }
bool xlz4_decompressor_finished(void* d) { return xrtLz4DecompressorFinished(d); }
unsigned long long xlz4_decompressor_members(void* d) { return xrtLz4DecompressorMembers(d); }
