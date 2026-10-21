#ifndef XRT_MODULE_DEFLATE
#define XRT_MODULE_DEFLATE
#endif
#ifndef XRT_MODULE_INFLATE
#define XRT_MODULE_INFLATE
#endif
#include "include/xrt_gzip.h"
#include <stdio.h>
#include <string.h>

#define GZIP_CHUNK 65536u

/* 两类对象共用限额与所有权规则，编解码算法完全由 XRT 提供。 */
typedef struct gzip_state {
    xrtgzipoptions Options;
    xrtgziperror Error;
    xbuffer* Output;
    uint64 InputBytes;
    uint64 OutputBytes;
    bool Finished;
    bool Failed;
    bool Busy;
} gzip_state;

struct xrtgzipcompressor { gzip_state State; xdeflate* Codec; };
struct xrtgzipdecompressor { gzip_state State; xinflate* Codec; };


// ----------------------------- 错误与参数 -----------------------------

static xerrkind gzip_kind(int code)
{
    switch (code) {
    case XRT_GZIP_ERROR_ARGUMENT: return XERR_ARGUMENT;
    case XRT_GZIP_ERROR_MEMORY: return XERR_MEMORY;
    case XRT_GZIP_ERROR_LIMIT: return XERR_RANGE;
    case XRT_GZIP_ERROR_STATE: return XERR_STATE;
    case XRT_GZIP_ERROR_CANCELLED: return XERR_CANCELLED;
    case XRT_GZIP_ERROR_DATA: case XRT_GZIP_ERROR_TRUNCATED: return XERR_PROTOCOL;
    default: return XERR_IO;
    }
}

static bool gzip_error(gzip_state* state, int code, const char* message)
{
    if (state != NULL) {
        state->Error.iCode = code;
        state->Error.iBackendCode = 0;
        state->Error.iInputOffset = state->InputBytes;
        state->Error.iOutputOffset = state->OutputBytes;
        snprintf(state->Error.sMessage, sizeof(state->Error.sMessage), "%s", message);
    }
    xrtSetErrorInfo(gzip_kind(code), "xlang.extlib.gzip", code, message);
    return false;
}

/* 底层 IO、分配器或用户回调有原始错误时只取快照，不替换错误对象。 */
static bool gzip_backend_error(gzip_state* state, int fallback, const char* message)
{
    const xerror* error = xrtGetError();
    int code = fallback;
    if (state->Error.iCode != XRT_GZIP_OK) {
        if (error == NULL)
            xrtSetErrorInfo(gzip_kind(state->Error.iCode), "xlang.extlib.gzip",
                state->Error.iCode, state->Error.sMessage);
        return false;
    }
    if (error == NULL) return gzip_error(state, fallback, message);
    switch (xrtErrorKind(error)) {
    case XERR_ARGUMENT: code = XRT_GZIP_ERROR_ARGUMENT; break;
    case XERR_MEMORY: code = XRT_GZIP_ERROR_MEMORY; break;
    case XERR_RANGE: code = XRT_GZIP_ERROR_LIMIT; break;
    case XERR_STATE: code = XRT_GZIP_ERROR_STATE; break;
    case XERR_CANCELLED: code = XRT_GZIP_ERROR_CANCELLED; break;
    default: break;
    }
    state->Error.iCode = code;
    state->Error.iBackendCode = xrtErrorCode(error);
    state->Error.iInputOffset = state->InputBytes;
    state->Error.iOutputOffset = state->OutputBytes;
    snprintf(state->Error.sMessage, sizeof(state->Error.sMessage), "%s", xrtErrorMessage(error));
    return false;
}

XRT_API void xrtGzipErrorInit(xrtgziperror* error)
{ if (error != NULL) memset(error, 0, sizeof(*error)); }

XRT_API void xrtGzipOptionsInit(xrtgzipoptions* options)
{
    if (options == NULL) return;
    memset(options, 0, sizeof(*options));
    options->iSize = sizeof(*options);
    options->iVersion = XRT_GZIP_OPTIONS_VERSION;
    options->iLevel = -1;
    options->iStrategy = XDEFLATE_STRATEGY_DEFAULT;
    xrtResourceLimitsInit(&options->tLimits);
    options->tLimits.iMaxItemBytes = 0;
    options->tLimits.iMaxEntries = 0;
    options->tLimits.iMaxNodes = 0;
    options->tLimits.iMaxDepth = 0;
}

static bool gzip_init(gzip_state* state, const xrtgzipoptions* options)
{
    xrtClearError();
    xrtGzipOptionsInit(&state->Options);
    if (options != NULL) {
        if (options->iSize != sizeof(*options) || options->iVersion != XRT_GZIP_OPTIONS_VERSION ||
            options->iReserved != 0 || options->tLimits.iSize != sizeof(options->tLimits) ||
            options->tLimits.iVersion != XRT_RESOURCE_LIMITS_VERSION || options->tLimits.iReserved != 0)
            return gzip_error(state, XRT_GZIP_ERROR_ARGUMENT, "invalid gzip options or limits version");
        if (options->tLimits.iFlags != 0 || options->tLimits.iMaxItemBytes != 0 ||
            options->tLimits.iMaxEntries != 0 || options->tLimits.iMaxNodes != 0 ||
            options->tLimits.iMaxDepth != 0)
            return gzip_error(state, XRT_GZIP_ERROR_ARGUMENT, "unsupported gzip resource limits");
        state->Options = *options;
    }
    if (state->Options.iLevel < -1 || state->Options.iLevel > 9)
        return gzip_error(state, XRT_GZIP_ERROR_ARGUMENT, "gzip level must be -1..9");
    if (state->Options.iStrategy < XDEFLATE_STRATEGY_DEFAULT || state->Options.iStrategy > XDEFLATE_STRATEGY_FIXED)
        return gzip_error(state, XRT_GZIP_ERROR_ARGUMENT, "invalid gzip strategy");
    state->Output = xrtBufferCreate();
    return state->Output != NULL || gzip_backend_error(state, XRT_GZIP_ERROR_MEMORY, "cannot allocate gzip buffer");
}

/* 参数/状态误用可纠正；编解码或写入失败是终态，不能继续交付部分结果。 */
static bool gzip_ready(gzip_state* state, bool writing)
{
    if (state == NULL) return gzip_error(NULL, XRT_GZIP_ERROR_ARGUMENT, "gzip object is null");
    if (state->Busy) {
        state->Failed = true;
        return gzip_error(state, XRT_GZIP_ERROR_STATE, "gzip object cannot be reentered from its writer");
    }
    if (state->Failed) {
        xrtSetErrorInfo(gzip_kind(state->Error.iCode), "xlang.extlib.gzip",
            state->Error.iCode, state->Error.sMessage);
        return false;
    }
    if (state->Output == NULL || (writing && state->Finished))
        return gzip_error(state, XRT_GZIP_ERROR_STATE, "gzip stream is finished or its buffer was taken");
    xrtGzipErrorInit(&state->Error);
    xrtClearError();
    return true;
}

static bool gzip_input(gzip_state* state, const void* data, size_t size)
{
    uint64 maximum;
    if (!gzip_ready(state, true)) return false;
    if (data == NULL && size != 0) return gzip_error(state, XRT_GZIP_ERROR_ARGUMENT, "gzip input is null");
    maximum = state->Options.tLimits.iMaxInputBytes;
    if ((uint64)size > UINT64_MAX - state->InputBytes ||
        (maximum != 0 && (state->InputBytes > maximum || (uint64)size > maximum - state->InputBytes))) {
        state->Failed = true;
        return gzip_error(state, XRT_GZIP_ERROR_LIMIT, "gzip input exceeds maxInput limit");
    }
    state->InputBytes += (uint64)size;
    return true;
}

static bool gzip_append(gzip_state* state, xbytesview data, bool decode)
{
    const xrtresourcelimits* limits = &state->Options.tLimits;
    uint64 size, ratio = limits->iMaxCompressionRatio;
    if ((uint64)data.Size > UINT64_MAX - state->OutputBytes)
        return gzip_error(state, XRT_GZIP_ERROR_LIMIT, "gzip output size overflow");
    size = state->OutputBytes + (uint64)data.Size;
    if ((limits->iMaxOutputBytes != 0 && size > limits->iMaxOutputBytes) ||
        (decode && ratio != 0 && state->InputBytes <= UINT64_MAX / ratio && size > state->InputBytes * ratio))
        return gzip_error(state, XRT_GZIP_ERROR_LIMIT, "gzip output exceeds size or expansion ratio limit");
    if (!xrtBufferAppend(state->Output, data))
        return gzip_backend_error(state, XRT_GZIP_ERROR_MEMORY, "cannot grow gzip output");
    state->OutputBytes = size;
    return true;
}

/* MTIME 只覆盖 RFC 1952 固定头的四字节；CRC、ISIZE 与 DEFLATE 不在包中重复实现。 */
static bool gzip_encode_output(xbytesview data, ptr user)
{
    gzip_state* state = user;
    uint64 offset = state->OutputBytes;
    size_t start = xrtBufferView(state->Output).Size;
    unsigned int i;
    if (!gzip_append(state, data, false)) return false;
    for (i = 4; i < 8; ++i) {
        if (offset <= i && i - offset < data.Size)
            state->Output->Data[start + (size_t)(i - offset)] = (uint8)(state->Options.iMTime >> ((i - 4) * 8));
    }
    return true;
}

static bool gzip_decode_output(xbytesview data, ptr user)
{ return gzip_append(user, data, true); }


// ----------------------------- 输出所有权 -----------------------------

static bool gzip_drain(gzip_state* state, xwriter* writer)
{
    xbytesview output;
    bool ok;
    if (!gzip_ready(state, false)) return false;
    if (writer == NULL) return gzip_error(state, XRT_GZIP_ERROR_ARGUMENT, "gzip writer is null");
    output = xrtBufferView(state->Output);
    state->Busy = true;
    ok = output.Size == 0 || xrtWriterWriteFull(writer, output.Data, output.Size, NULL);
    state->Busy = false;
    if (!ok || state->Failed) {
        state->Failed = true;
        return gzip_backend_error(state, XRT_GZIP_ERROR_IO, "cannot write gzip output");
    }
    xrtBufferClear(state->Output);
    return true;
}

static xbuffer* gzip_take(gzip_state* state, bool final)
{
    xbuffer* output;
    xbuffer* replacement = NULL;
    if (!gzip_ready(state, false)) return NULL;
    if (final && !state->Finished) {
        gzip_error(state, XRT_GZIP_ERROR_STATE, "finish gzip before taking its buffer");
        return NULL;
    }
    if (!final) {
        replacement = xrtBufferCreate();
        if (replacement == NULL) {
            gzip_backend_error(state, XRT_GZIP_ERROR_MEMORY, "cannot replace gzip output buffer");
            return NULL;
        }
    }
    output = state->Output;
    state->Output = replacement;
    return output;
}


// ----------------------------- 压缩器与解压器 -----------------------------

XRT_API xrtgzipcompressor* xrtGzipCompressorCreate(const xrtgzipoptions* options)
{
    xrtgzipcompressor* c = xrtMalloc(sizeof(*c));
    xdeflateconfig config;
    if (c == NULL) return NULL;
    memset(c, 0, sizeof(*c));
    if (!gzip_init(&c->State, options)) goto failed;
    xrtDeflateConfigInit(&config);
    config.Format = XDEFLATE_GZIP;
    config.Level = c->State.Options.iLevel < 0 ? XDEFLATE_LEVEL_DEFAULT : c->State.Options.iLevel;
    config.Strategy = (xdeflatestrategy)c->State.Options.iStrategy;
    config.OutputLimit = c->State.Options.tLimits.iMaxOutputBytes != 0 ?
        c->State.Options.tLimits.iMaxOutputBytes : XDEFLATE_OUTPUT_UNLIMITED;
    c->Codec = xrtDeflateCreate(&config);
    if (c->Codec != NULL) return c;
failed:
    xrtGzipCompressorDestroy(c);
    return NULL;
}

XRT_API xrtgzipdecompressor* xrtGzipDecompressorCreate(const xrtgzipoptions* options)
{
    xrtgzipdecompressor* d = xrtMalloc(sizeof(*d));
    xinflateconfig config;
    if (d == NULL) return NULL;
    memset(d, 0, sizeof(*d));
    if (!gzip_init(&d->State, options)) goto failed;
    xrtInflateConfigInit(&config);
    config.Format = XINFLATE_GZIP;
    config.OutputLimit = d->State.Options.tLimits.iMaxOutputBytes != 0 ?
        d->State.Options.tLimits.iMaxOutputBytes : XINFLATE_OUTPUT_UNLIMITED;
    d->Codec = xrtInflateCreate(&config);
    if (d->Codec != NULL) return d;
failed:
    xrtGzipDecompressorDestroy(d);
    return NULL;
}

XRT_API bool xrtGzipCompressorUpdate(xrtgzipcompressor* c, const void* data, size_t size)
{
    if (!gzip_input(c != NULL ? &c->State : NULL, data, size)) return false;
    if (size == 0 || xrtDeflateWrite(c->Codec, (xbytesview){data, size},
        XDEFLATE_FLUSH_NONE, gzip_encode_output, &c->State)) return true;
    c->State.Failed = true;
    return gzip_backend_error(&c->State, XRT_GZIP_ERROR_DATA, "gzip compression failed");
}

XRT_API bool xrtGzipDecompressorUpdate(xrtgzipdecompressor* d, const void* data, size_t size)
{
    if (!gzip_input(d != NULL ? &d->State : NULL, data, size)) return false;
    if (size == 0 || xrtInflateWrite(d->Codec, (xbytesview){data, size},
        false, gzip_decode_output, &d->State)) return true;
    d->State.Failed = true;
    return gzip_backend_error(&d->State, XRT_GZIP_ERROR_DATA, "invalid gzip data");
}

XRT_API bool xrtGzipCompressorFinish(xrtgzipcompressor* c)
{
    if (!gzip_ready(c != NULL ? &c->State : NULL, false)) return false;
    if (c->State.Finished) return true;
    if (!xrtDeflateWrite(c->Codec, (xbytesview){NULL, 0}, XDEFLATE_FLUSH_FINISH,
        gzip_encode_output, &c->State)) {
        c->State.Failed = true;
        return gzip_backend_error(&c->State, XRT_GZIP_ERROR_DATA, "cannot finish gzip compressor");
    }
    c->State.Finished = true;
    return true;
}

XRT_API bool xrtGzipDecompressorFinish(xrtgzipdecompressor* d)
{
    if (!gzip_ready(d != NULL ? &d->State : NULL, false)) return false;
    if (d->State.Finished) return true;
    if (!xrtInflateWrite(d->Codec, (xbytesview){NULL, 0}, true, gzip_decode_output, &d->State)) {
        d->State.Failed = true;
        return gzip_backend_error(&d->State, XRT_GZIP_ERROR_TRUNCATED, "gzip stream is truncated");
    }
    d->State.Finished = true;
    return true;
}

XRT_API bool xrtGzipCompressorDrain(xrtgzipcompressor* c, xwriter* writer)
{ return gzip_drain(c != NULL ? &c->State : NULL, writer); }
XRT_API bool xrtGzipDecompressorDrain(xrtgzipdecompressor* d, xwriter* writer)
{ return gzip_drain(d != NULL ? &d->State : NULL, writer); }
XRT_API xbuffer* xrtGzipCompressorTakeBuffer(xrtgzipcompressor* c)
{ return gzip_take(c != NULL ? &c->State : NULL, true); }
XRT_API xbuffer* xrtGzipDecompressorTakeBuffer(xrtgzipdecompressor* d)
{ return gzip_take(d != NULL ? &d->State : NULL, true); }
XRT_API xbuffer* xrtGzipCompressorDrainBuffer(xrtgzipcompressor* c)
{ return gzip_take(c != NULL ? &c->State : NULL, false); }
XRT_API xbuffer* xrtGzipDecompressorDrainBuffer(xrtgzipdecompressor* d)
{ return gzip_take(d != NULL ? &d->State : NULL, false); }
XRT_API uint64 xrtGzipCompressorInputBytes(const xrtgzipcompressor* c)
{ return c != NULL ? c->State.InputBytes : 0; }
XRT_API uint64 xrtGzipCompressorOutputBytes(const xrtgzipcompressor* c)
{ return c != NULL ? c->State.OutputBytes : 0; }
XRT_API uint64 xrtGzipDecompressorInputBytes(const xrtgzipdecompressor* d)
{ return d != NULL ? d->State.InputBytes : 0; }
XRT_API uint64 xrtGzipDecompressorOutputBytes(const xrtgzipdecompressor* d)
{ return d != NULL ? d->State.OutputBytes : 0; }
XRT_API bool xrtGzipCompressorFinished(const xrtgzipcompressor* c)
{ return c != NULL && c->State.Finished && !c->State.Failed; }
XRT_API bool xrtGzipDecompressorFinished(const xrtgzipdecompressor* d)
{ return d != NULL && d->State.Finished && !d->State.Failed; }
XRT_API const xrtgziperror* xrtGzipCompressorError(const xrtgzipcompressor* c)
{ return c != NULL ? &c->State.Error : NULL; }
XRT_API const xrtgziperror* xrtGzipDecompressorError(const xrtgzipdecompressor* d)
{ return d != NULL ? &d->State.Error : NULL; }

XRT_API void xrtGzipCompressorDestroy(xrtgzipcompressor* c)
{
    if (c == NULL) return;
    if (c->State.Busy) { (void)gzip_ready(&c->State, false); return; }
    xrtDeflateDestroy(c->Codec);
    xrtBufferDestroy(c->State.Output);
    xrtFree(c);
}

XRT_API void xrtGzipDecompressorDestroy(xrtgzipdecompressor* d)
{
    if (d == NULL) return;
    if (d->State.Busy) { (void)gzip_ready(&d->State, false); return; }
    xrtInflateDestroy(d->Codec);
    xrtBufferDestroy(d->State.Output);
    xrtFree(d);
}


// ----------------------------- 便捷与借用流 -----------------------------

static void gzip_result_error(xrtgziperror* error, gzip_state* state)
{
    if (error == NULL) return;
    if (state != NULL) *error = state->Error;
    else {
        gzip_state missing = {0};
        gzip_backend_error(&missing, XRT_GZIP_ERROR_MEMORY, "cannot create gzip stream");
        *error = missing.Error;
    }
}

static bool gzip_all(const void* data, size_t size, const xrtgzipoptions* options,
    xbuffer** output, xrtgziperror* error, bool decode)
{
    xrtgzipcompressor* c = NULL;
    xrtgzipdecompressor* d = NULL;
    gzip_state* state;
    bool ok;
    xrtGzipErrorInit(error);
    if (output != NULL) *output = NULL;
    if (output == NULL || (data == NULL && size != 0)) {
        gzip_state invalid = {0};
        gzip_error(&invalid, XRT_GZIP_ERROR_ARGUMENT, "invalid gzip input or output slot");
        gzip_result_error(error, &invalid);
        return false;
    }
    if (decode) d = xrtGzipDecompressorCreate(options);
    else c = xrtGzipCompressorCreate(options);
    state = d != NULL ? &d->State : (c != NULL ? &c->State : NULL);
    if (state == NULL) { gzip_result_error(error, NULL); return false; }
    ok = decode ? xrtGzipDecompressorUpdate(d, data, size) && xrtGzipDecompressorFinish(d) :
        xrtGzipCompressorUpdate(c, data, size) && xrtGzipCompressorFinish(c);
    if (ok) { *output = gzip_take(state, true); ok = *output != NULL; }
    if (!ok) gzip_result_error(error, state);
    xrtGzipCompressorDestroy(c);
    xrtGzipDecompressorDestroy(d);
    return ok;
}

XRT_API bool xrtGzipCompress(const void* data, size_t size, const xrtgzipoptions* options,
    xbuffer** output, xrtgziperror* error)
{ return gzip_all(data, size, options, output, error, false); }
XRT_API bool xrtGzipDecompress(const void* data, size_t size, const xrtgzipoptions* options,
    xbuffer** output, xrtgziperror* error)
{ return gzip_all(data, size, options, output, error, true); }

/* Reader/Writer 均为借用；进度不保存回调，不因最终取消而提前宣告成功。 */
static bool gzip_stream(xreader* reader, xwriter* writer, const xrtgzipoptions* options,
    xrtprogressproc progress, ptr user, xrtgziperror* error, bool decode)
{
    uint8 input[GZIP_CHUNK];
    xrtgzipcompressor* c = NULL;
    xrtgzipdecompressor* d = NULL;
    gzip_state* state;
    uint64 total = 0, position = 0;
    bool known = false, ok = true;
    xrtGzipErrorInit(error);
    if (reader == NULL || writer == NULL) {
        gzip_state invalid = {0};
        gzip_error(&invalid, XRT_GZIP_ERROR_ARGUMENT, "gzip reader and writer are required");
        gzip_result_error(error, &invalid);
        return false;
    }
    if (decode) d = xrtGzipDecompressorCreate(options);
    else c = xrtGzipCompressorCreate(options);
    state = d != NULL ? &d->State : (c != NULL ? &c->State : NULL);
    if (state == NULL) { gzip_result_error(error, NULL); return false; }
    if (xrtReaderCanSize(reader) && xrtReaderCanSeek(reader)) {
        ok = xrtReaderSize(reader, &total) && xrtReaderTell(reader, &position);
        if (!ok) gzip_backend_error(state, XRT_GZIP_ERROR_IO, "cannot query gzip input size");
        else if (position <= total) { total -= position; known = true; }
        else total = 0;
    }
    while (ok) {
        size_t size = 0;
        if (!xrtReaderRead(reader, input, sizeof(input), &size)) {
            ok = gzip_backend_error(state, XRT_GZIP_ERROR_IO, "cannot read gzip input");
            break;
        }
        if (size == 0) break;
        ok = decode ? xrtGzipDecompressorUpdate(d, input, size) : xrtGzipCompressorUpdate(c, input, size);
        if (ok) ok = gzip_drain(state, writer);
        if (ok && !xrtProgressReport(progress, user, state->InputBytes, total,
            state->OutputBytes, known ? XRT_PROGRESS_TOTAL_KNOWN : 0))
            ok = gzip_backend_error(state, XRT_GZIP_ERROR_CANCELLED, "gzip operation cancelled");
    }
    if (ok) ok = decode ? xrtGzipDecompressorFinish(d) : xrtGzipCompressorFinish(c);
    if (ok) ok = gzip_drain(state, writer);
    if (ok && !xrtProgressReport(progress, user, state->InputBytes, total,
        state->OutputBytes, XRT_PROGRESS_FINAL | (known ? XRT_PROGRESS_TOTAL_KNOWN : 0)))
        ok = gzip_backend_error(state, XRT_GZIP_ERROR_CANCELLED, "gzip operation cancelled");
    if (!ok) gzip_result_error(error, state);
    xrtGzipCompressorDestroy(c);
    xrtGzipDecompressorDestroy(d);
    return ok;
}

XRT_API bool xrtGzipCompressStream(xreader* reader, xwriter* writer, const xrtgzipoptions* options,
    xrtprogressproc progress, ptr user, xrtgziperror* error)
{ return gzip_stream(reader, writer, options, progress, user, error, false); }
XRT_API bool xrtGzipDecompressStream(xreader* reader, xwriter* writer, const xrtgzipoptions* options,
    xrtprogressproc progress, ptr user, xrtgziperror* error)
{ return gzip_stream(reader, writer, options, progress, user, error, true); }
