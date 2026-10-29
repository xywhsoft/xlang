#include "include/xrt_bz2.h"
#include "../_bzip2/third_party/bzip2/bzlib.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define BZ2_CHUNK 65536u

/* 算法交给 bzip2；两种流共享限额、错误和输出所有权，不保存用户回调。 */
typedef struct bz2_state {
    bz_stream Codec;
    xrtbz2options Options;
    xrtbz2error Error;
    xbuffer* Output;
    uint64 InputBytes, OutputBytes, Members;
    bool Decode, Initialized, MemberEnded, Finished, Failed, Busy;
} bz2_state;

struct xrtbz2compressor { bz2_state State; };
struct xrtbz2decompressor { bz2_state State; };


// ----------------------------- 错误与初始化 -----------------------------

static xerrkind bz2_kind(int code)
{
    switch (code) {
    case XRT_BZ2_ERROR_ARGUMENT: return XERR_ARGUMENT;
    case XRT_BZ2_ERROR_MEMORY: return XERR_MEMORY;
    case XRT_BZ2_ERROR_LIMIT: return XERR_RANGE;
    case XRT_BZ2_ERROR_STATE: return XERR_STATE;
    case XRT_BZ2_ERROR_CANCELLED: return XERR_CANCELLED;
    case XRT_BZ2_ERROR_DATA: case XRT_BZ2_ERROR_TRUNCATED: return XERR_PROTOCOL;
    default: return XERR_IO;
    }
}

static bool bz2_error(bz2_state* s, int code, int backend, const char* message)
{
    if (s != NULL) {
        s->Error.iCode = code;
        s->Error.iBackendCode = backend;
        s->Error.iInputOffset = s->InputBytes;
        s->Error.iOutputOffset = s->OutputBytes;
        snprintf(s->Error.sMessage, sizeof(s->Error.sMessage), "%s", message);
    }
    xrtSetErrorInfo(bz2_kind(code), "xlang.extlib.bz2", code, message);
    return false;
}

/* IO、分配器和回调已有错误时只记录快照，不替换原错误对象。 */
static bool bz2_external_error(bz2_state* s, int fallback, const char* message)
{
    const xerror* error = xrtGetError();
    int code = fallback;
    if (s->Error.iCode != XRT_BZ2_OK) {
        if (error == NULL)
            xrtSetErrorInfo(bz2_kind(s->Error.iCode), "xlang.extlib.bz2",
                s->Error.iCode, s->Error.sMessage);
        return false;
    }
    if (error == NULL) return bz2_error(s, fallback, 0, message);
    switch (xrtErrorKind(error)) {
    case XERR_ARGUMENT: code = XRT_BZ2_ERROR_ARGUMENT; break;
    case XERR_MEMORY: code = XRT_BZ2_ERROR_MEMORY; break;
    case XERR_RANGE: code = XRT_BZ2_ERROR_LIMIT; break;
    case XERR_STATE: code = XRT_BZ2_ERROR_STATE; break;
    case XERR_CANCELLED: code = XRT_BZ2_ERROR_CANCELLED; break;
    default: break;
    }
    s->Error.iCode = code;
    s->Error.iBackendCode = xrtErrorCode(error);
    s->Error.iInputOffset = s->InputBytes;
    s->Error.iOutputOffset = s->OutputBytes;
    snprintf(s->Error.sMessage, sizeof(s->Error.sMessage), "%s", xrtErrorMessage(error));
    return false;
}

XRT_API void xrtBz2ErrorInit(xrtbz2error* error)
{ if (error != NULL) memset(error, 0, sizeof(*error)); }

XRT_API void xrtBz2OptionsInit(xrtbz2options* options)
{
    if (options == NULL) return;
    memset(options, 0, sizeof(*options));
    options->iSize = sizeof(*options);
    options->iVersion = XRT_BZ2_OPTIONS_VERSION;
    options->iLevel = 9;
    options->iWorkFactor = 30;
    xrtResourceLimitsInit(&options->tLimits);
    options->tLimits.iMaxItemBytes = 0;
    options->tLimits.iMaxEntries = 0;
    options->tLimits.iMaxNodes = 0;
    options->tLimits.iMaxDepth = 0;
}

static void* bz2_alloc(void* opaque, int items, int size)
{
    (void)opaque;
    if (items < 0 || size < 0 || (size != 0 && (size_t)items > SIZE_MAX / (size_t)size)) return NULL;
    return xrtCalloc((size_t)items, (size_t)size);
}
static void bz2_free(void* opaque, void* memory) { (void)opaque; xrtFree(memory); }

static bool bz2_codec_init(bz2_state* s)
{
    int result;
    memset(&s->Codec, 0, sizeof(s->Codec));
    s->Codec.bzalloc = bz2_alloc;
    s->Codec.bzfree = bz2_free;
    result = s->Decode ? BZ2_bzDecompressInit(&s->Codec, 0, s->Options.bSmallMemory ? 1 : 0) :
        BZ2_bzCompressInit(&s->Codec, s->Options.iLevel, 0, s->Options.iWorkFactor);
    if (result != BZ_OK) {
        if (result == BZ_MEM_ERROR) return bz2_external_error(s, XRT_BZ2_ERROR_MEMORY, "cannot allocate bz2 codec");
        return bz2_error(s, XRT_BZ2_ERROR_STATE, result, "cannot initialize bz2 codec");
    }
    s->Initialized = true;
    s->MemberEnded = false;
    return true;
}

static void bz2_codec_end(bz2_state* s)
{
    if (!s->Initialized) return;
    if (s->Decode) BZ2_bzDecompressEnd(&s->Codec);
    else BZ2_bzCompressEnd(&s->Codec);
    s->Initialized = false;
}

static bool bz2_init(bz2_state* s, const xrtbz2options* options, bool decode)
{
    static const uint8 reserved[7] = {0};
    memset(s, 0, sizeof(*s));
    s->Decode = decode;
    xrtBz2OptionsInit(&s->Options);
    if (options != NULL) {
        if (options->iSize != sizeof(*options) || options->iVersion != XRT_BZ2_OPTIONS_VERSION ||
            memcmp(options->sReserved, reserved, sizeof(reserved)) != 0 ||
            options->tLimits.iSize != sizeof(options->tLimits) ||
            options->tLimits.iVersion != XRT_RESOURCE_LIMITS_VERSION || options->tLimits.iReserved != 0)
            return bz2_error(s, XRT_BZ2_ERROR_ARGUMENT, BZ_PARAM_ERROR, "invalid bz2 options or limits version");
        if (options->tLimits.iFlags != 0 || options->tLimits.iMaxItemBytes != 0 ||
            options->tLimits.iMaxEntries != 0 || options->tLimits.iMaxNodes != 0 ||
            options->tLimits.iMaxDepth != 0)
            return bz2_error(s, XRT_BZ2_ERROR_ARGUMENT, BZ_PARAM_ERROR, "unsupported bz2 resource limits");
        s->Options = *options;
    }
    if (s->Options.iLevel < 1 || s->Options.iLevel > 9 ||
        s->Options.iWorkFactor < 0 || s->Options.iWorkFactor > 250)
        return bz2_error(s, XRT_BZ2_ERROR_ARGUMENT, BZ_PARAM_ERROR, "bz2 level must be 1..9 and workFactor 0..250");
    s->Output = xrtBufferCreate();
    if (s->Output == NULL) return bz2_external_error(s, XRT_BZ2_ERROR_MEMORY, "cannot allocate bz2 output");
    return bz2_codec_init(s);
}

/* 参数/状态误用可纠正；编解码、限额、写入和回调重入失败是终态。 */
static bool bz2_ready(bz2_state* s, bool writing)
{
    if (s == NULL) return bz2_error(NULL, XRT_BZ2_ERROR_ARGUMENT, 0, "bz2 object is null");
    if (s->Busy) {
        s->Failed = true;
        return bz2_error(s, XRT_BZ2_ERROR_STATE, 0, "bz2 object cannot be reentered from its writer");
    }
    if (s->Failed) {
        xrtSetErrorInfo(bz2_kind(s->Error.iCode), "xlang.extlib.bz2", s->Error.iCode, s->Error.sMessage);
        return false;
    }
    if (s->Output == NULL || (writing && s->Finished))
        return bz2_error(s, XRT_BZ2_ERROR_STATE, 0, "bz2 stream is finished or its buffer was taken");
    xrtClearError();
    xrtBz2ErrorInit(&s->Error);
    return true;
}

static bool bz2_append(bz2_state* s, const uint8* bytes, size_t size)
{
    const xrtresourcelimits* limits = &s->Options.tLimits;
    uint64 count, ratio = limits->iMaxCompressionRatio;
    if ((uint64)size > UINT64_MAX - s->OutputBytes)
        return bz2_error(s, XRT_BZ2_ERROR_LIMIT, 0, "bz2 output size overflow");
    count = s->OutputBytes + size;
    if ((limits->iMaxOutputBytes != 0 && count > limits->iMaxOutputBytes) ||
        (s->Decode && ratio != 0 && s->InputBytes <= UINT64_MAX / ratio && count > s->InputBytes * ratio))
        return bz2_error(s, XRT_BZ2_ERROR_LIMIT, 0, "bz2 output exceeds size or expansion ratio limit");
    if (!xrtBufferAppend(s->Output, (xbytesview){bytes, size}))
        return bz2_external_error(s, XRT_BZ2_ERROR_MEMORY, "cannot grow bz2 output");
    s->OutputBytes = count;
    return true;
}


// ----------------------------- bzip2 状态机 -----------------------------

static bool bz2_run(bz2_state* s, const void* data, size_t size, bool finish)
{
    uint8 output[BZ2_CHUNK];
    size_t offset = 0;
    bool drain = false;
    while (offset < size || drain || finish) {
        unsigned int input;
        size_t consumed, produced;
        int result;
        if (s->Decode && s->MemberEnded) {
            if (offset == size) break;
            bz2_codec_end(s);
            if (!bz2_codec_init(s)) goto failed;
        }
        input = (unsigned int)((size - offset) > UINT_MAX ? UINT_MAX : size - offset);
        s->Codec.next_in = input != 0 ? (char*)data + offset : NULL;
        s->Codec.avail_in = input;
        s->Codec.next_out = (char*)output;
        s->Codec.avail_out = sizeof(output);
        result = s->Decode ? BZ2_bzDecompress(&s->Codec) : BZ2_bzCompress(&s->Codec, finish ? BZ_FINISH : BZ_RUN);
        consumed = input - s->Codec.avail_in;
        produced = sizeof(output) - s->Codec.avail_out;
        offset += consumed;
        if (!bz2_append(s, output, produced)) goto failed;
        if (result == BZ_STREAM_END) {
            s->MemberEnded = true;
            ++s->Members;
            if (!s->Decode) { s->Finished = true; return true; }
            drain = false;
            continue;
        }
        if ((s->Decode && result != BZ_OK) ||
            (!s->Decode && result != (finish ? BZ_FINISH_OK : BZ_RUN_OK))) {
            if (result == BZ_MEM_ERROR) bz2_external_error(s, XRT_BZ2_ERROR_MEMORY, "cannot allocate bz2 block");
            else bz2_error(s, XRT_BZ2_ERROR_DATA, result, "bz2 data is invalid");
            goto failed;
        }
        /* 解压可能耗尽输入后仍有整个块待输出，不能只按 avail_in 退出。 */
        drain = s->Decode && s->Codec.avail_out == 0;
        if (consumed == 0 && produced == 0) {
            if (s->Decode && offset == size) break;
            bz2_error(s, XRT_BZ2_ERROR_STATE, result, "bz2 codec made no progress");
            goto failed;
        }
    }
    return true;
failed:
    s->Failed = true;
    return false;
}

static bool bz2_update(bz2_state* s, const void* data, size_t size)
{
    uint64 maximum;
    if (!bz2_ready(s, true)) return false;
    if (data == NULL && size != 0) return bz2_error(s, XRT_BZ2_ERROR_ARGUMENT, 0, "bz2 input is null");
    maximum = s->Options.tLimits.iMaxInputBytes;
    if ((uint64)size > UINT64_MAX - s->InputBytes ||
        (maximum != 0 && (s->InputBytes > maximum || (uint64)size > maximum - s->InputBytes))) {
        s->Failed = true;
        return bz2_error(s, XRT_BZ2_ERROR_LIMIT, 0, "bz2 input exceeds maxInput limit");
    }
    s->InputBytes += size;
    return size == 0 || bz2_run(s, data, size, false);
}

static bool bz2_finish(bz2_state* s)
{
    if (!bz2_ready(s, false)) return false;
    if (s->Finished) return true;
    if (!s->Decode) return bz2_run(s, NULL, 0, true);
    if (!s->MemberEnded || s->Members == 0) {
        s->Failed = true;
        return bz2_error(s, XRT_BZ2_ERROR_TRUNCATED, BZ_UNEXPECTED_EOF, "input is not a complete bz2 stream");
    }
    s->Finished = true;
    return true;
}


// ----------------------------- 输出与生命周期 -----------------------------

static bool bz2_drain(bz2_state* s, xwriter* writer)
{
    xbytesview bytes;
    bool ok;
    if (!bz2_ready(s, false)) return false;
    if (writer == NULL) return bz2_error(s, XRT_BZ2_ERROR_ARGUMENT, 0, "bz2 writer is null");
    bytes = xrtBufferView(s->Output);
    s->Busy = true;
    ok = bytes.Size == 0 || xrtWriterWriteFull(writer, bytes.Data, bytes.Size, NULL);
    s->Busy = false;
    if (!ok || s->Failed) {
        s->Failed = true;
        return bz2_external_error(s, XRT_BZ2_ERROR_IO, "cannot write bz2 output");
    }
    xrtBufferClear(s->Output);
    return true;
}

static xbuffer* bz2_take(bz2_state* s, bool final)
{
    xbuffer* output;
    xbuffer* replacement = NULL;
    if (!bz2_ready(s, false)) return NULL;
    if (final && !s->Finished) {
        bz2_error(s, XRT_BZ2_ERROR_STATE, 0, "finish bz2 before taking its buffer");
        return NULL;
    }
    if (!final) {
        replacement = xrtBufferCreate();
        if (replacement == NULL) {
            bz2_external_error(s, XRT_BZ2_ERROR_MEMORY, "cannot replace bz2 output buffer");
            return NULL;
        }
    }
    output = s->Output;
    s->Output = replacement;
    return output;
}

static bool bz2_destroy(bz2_state* s)
{
    if (s == NULL) return true;
    if (s->Busy) {
        s->Failed = true;
        return bz2_error(s, XRT_BZ2_ERROR_STATE, 0, "cannot destroy bz2 from its writer");
    }
    bz2_codec_end(s);
    xrtBufferDestroy(s->Output);
    return true;
}

XRT_API xrtbz2compressor* xrtBz2CompressorCreate(const xrtbz2options* options)
{
    xrtbz2compressor* c;
    xrtClearError();
    c = xrtMalloc(sizeof(*c));
    if (c == NULL) return NULL;
    if (bz2_init(&c->State, options, false)) return c;
    xrtBz2CompressorDestroy(c);
    return NULL;
}
XRT_API xrtbz2decompressor* xrtBz2DecompressorCreate(const xrtbz2options* options)
{
    xrtbz2decompressor* d;
    xrtClearError();
    d = xrtMalloc(sizeof(*d));
    if (d == NULL) return NULL;
    if (bz2_init(&d->State, options, true)) return d;
    xrtBz2DecompressorDestroy(d);
    return NULL;
}
XRT_API bool xrtBz2CompressorUpdate(xrtbz2compressor* c, const void* p, size_t n)
{ return bz2_update(c ? &c->State : NULL, p, n); }
XRT_API bool xrtBz2DecompressorUpdate(xrtbz2decompressor* d, const void* p, size_t n)
{ return bz2_update(d ? &d->State : NULL, p, n); }
XRT_API bool xrtBz2CompressorFinish(xrtbz2compressor* c) { return bz2_finish(c ? &c->State : NULL); }
XRT_API bool xrtBz2DecompressorFinish(xrtbz2decompressor* d) { return bz2_finish(d ? &d->State : NULL); }
XRT_API bool xrtBz2CompressorDrain(xrtbz2compressor* c, xwriter* w) { return bz2_drain(c ? &c->State : NULL, w); }
XRT_API bool xrtBz2DecompressorDrain(xrtbz2decompressor* d, xwriter* w) { return bz2_drain(d ? &d->State : NULL, w); }
XRT_API xbuffer* xrtBz2CompressorTakeBuffer(xrtbz2compressor* c) { return bz2_take(c ? &c->State : NULL, true); }
XRT_API xbuffer* xrtBz2DecompressorTakeBuffer(xrtbz2decompressor* d) { return bz2_take(d ? &d->State : NULL, true); }
XRT_API xbuffer* xrtBz2CompressorDrainBuffer(xrtbz2compressor* c) { return bz2_take(c ? &c->State : NULL, false); }
XRT_API xbuffer* xrtBz2DecompressorDrainBuffer(xrtbz2decompressor* d) { return bz2_take(d ? &d->State : NULL, false); }
XRT_API uint64 xrtBz2CompressorInputBytes(const xrtbz2compressor* c) { return c ? c->State.InputBytes : 0; }
XRT_API uint64 xrtBz2CompressorOutputBytes(const xrtbz2compressor* c) { return c ? c->State.OutputBytes : 0; }
XRT_API uint64 xrtBz2DecompressorInputBytes(const xrtbz2decompressor* d) { return d ? d->State.InputBytes : 0; }
XRT_API uint64 xrtBz2DecompressorOutputBytes(const xrtbz2decompressor* d) { return d ? d->State.OutputBytes : 0; }
XRT_API uint64 xrtBz2DecompressorMembers(const xrtbz2decompressor* d) { return d ? d->State.Members : 0; }
XRT_API bool xrtBz2CompressorFinished(const xrtbz2compressor* c) { return c && !c->State.Failed && c->State.Finished; }
XRT_API bool xrtBz2DecompressorFinished(const xrtbz2decompressor* d) { return d && !d->State.Failed && d->State.MemberEnded; }
XRT_API const xrtbz2error* xrtBz2CompressorError(const xrtbz2compressor* c) { return c ? &c->State.Error : NULL; }
XRT_API const xrtbz2error* xrtBz2DecompressorError(const xrtbz2decompressor* d) { return d ? &d->State.Error : NULL; }
XRT_API void xrtBz2CompressorDestroy(xrtbz2compressor* c) { if (bz2_destroy(c ? &c->State : NULL)) xrtFree(c); }
XRT_API void xrtBz2DecompressorDestroy(xrtbz2decompressor* d) { if (bz2_destroy(d ? &d->State : NULL)) xrtFree(d); }


// ----------------------------- 一次性与 Reader/Writer -----------------------------

static bool bz2_all(const void* data, size_t size, const xrtbz2options* options,
    xbuffer** output, xrtbz2error* error, bool decode)
{
    bz2_state state;
    bool ok;
    xrtClearError();
    xrtBz2ErrorInit(error);
    if (output != NULL) *output = NULL;
    if (output == NULL || (data == NULL && size != 0)) {
        memset(&state, 0, sizeof(state));
        bz2_error(&state, XRT_BZ2_ERROR_ARGUMENT, 0, "invalid bz2 input or output pointer");
        if (error != NULL) *error = state.Error;
        return false;
    }
    ok = bz2_init(&state, options, decode) && bz2_update(&state, data, size) && bz2_finish(&state);
    if (ok) { *output = bz2_take(&state, true); ok = *output != NULL; }
    if (!ok && error != NULL) *error = state.Error;
    bz2_destroy(&state);
    return ok;
}

XRT_API bool xrtBz2Compress(const void* p, size_t n, const xrtbz2options* o, xbuffer** b, xrtbz2error* e)
{ return bz2_all(p, n, o, b, e, false); }
XRT_API bool xrtBz2Decompress(const void* p, size_t n, const xrtbz2options* o, xbuffer** b, xrtbz2error* e)
{ return bz2_all(p, n, o, b, e, true); }

/* Reader/Writer 都由调用方持有；总量从当前游标计算，真正失败不能降级为未知大小。 */
static bool bz2_stream(xreader* reader, xwriter* writer, const xrtbz2options* options,
    xrtprogressproc progress, ptr user, xrtbz2error* error, bool decode)
{
    bz2_state s;
    uint8 input[BZ2_CHUNK];
    uint64 total = 0, position = 0;
    bool known = false, ok;
    xrtClearError();
    xrtBz2ErrorInit(error);
    memset(&s, 0, sizeof(s));
    if (reader == NULL || writer == NULL) {
        bz2_error(&s, XRT_BZ2_ERROR_ARGUMENT, 0, "bz2 reader and writer are required");
        if (error != NULL) *error = s.Error;
        return false;
    }
    ok = bz2_init(&s, options, decode);
    if (ok && xrtReaderCanSize(reader) && xrtReaderCanSeek(reader)) {
        ok = xrtReaderSize(reader, &total) && xrtReaderTell(reader, &position);
        if (!ok) bz2_external_error(&s, XRT_BZ2_ERROR_IO, "cannot query bz2 input size");
        else if (position <= total) { total -= position; known = true; }
        else total = 0;
    }
    while (ok) {
        size_t size = 0;
        if (!xrtReaderRead(reader, input, sizeof(input), &size)) {
            ok = bz2_external_error(&s, XRT_BZ2_ERROR_IO, "cannot read bz2 input");
            break;
        }
        if (size == 0) break;
        ok = bz2_update(&s, input, size) && bz2_drain(&s, writer);
        if (ok && !xrtProgressReport(progress, user, s.InputBytes, total, s.OutputBytes,
            known ? XRT_PROGRESS_TOTAL_KNOWN : 0))
            ok = bz2_external_error(&s, XRT_BZ2_ERROR_CANCELLED, "bz2 operation cancelled");
    }
    if (ok) ok = bz2_finish(&s) && bz2_drain(&s, writer);
    if (ok && !xrtProgressReport(progress, user, s.InputBytes, total, s.OutputBytes,
        XRT_PROGRESS_FINAL | (known ? XRT_PROGRESS_TOTAL_KNOWN : 0)))
        ok = bz2_external_error(&s, XRT_BZ2_ERROR_CANCELLED, "bz2 operation cancelled");
    if (!ok && error != NULL) *error = s.Error;
    bz2_destroy(&s);
    return ok;
}

XRT_API bool xrtBz2CompressStream(xreader* r, xwriter* w, const xrtbz2options* o,
    xrtprogressproc p, ptr u, xrtbz2error* e) { return bz2_stream(r, w, o, p, u, e, false); }
XRT_API bool xrtBz2DecompressStream(xreader* r, xwriter* w, const xrtbz2options* o,
    xrtprogressproc p, ptr u, xrtbz2error* e) { return bz2_stream(r, w, o, p, u, e, true); }
