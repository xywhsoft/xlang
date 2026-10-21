#include "include/xrt_lz4.h"
#define LZ4F_STATIC_LINKING_ONLY
#include "third_party/lz4frame.h"
#include <stdio.h>
#include <string.h>

#define LZ4_CHUNK 65536u

/* 官方 Frame 编解码器负责格式；公共层只管理限额、错误、流和结果所有权。 */
typedef struct lz4_state {
    xrtlz4options Options;
    xrtlz4error Error;
    xbuffer* Output;
    LZ4F_cctx* Encoder;
    LZ4F_dctx* Decoder;
    uint8* Scratch;
    size_t ScratchSize;
    uint64 InputBytes, OutputBytes, Members, Frames, FrameInput, FrameOutput;
    uint8 Header[LZ4F_HEADER_SIZE_MAX];
    size_t HeaderSize, HeaderNeed;
    bool Decode, AtHeader, Skippable, MemberEnded, Finished, Failed, Busy;
} lz4_state;

struct xrtlz4compressor { lz4_state State; };
struct xrtlz4decompressor { lz4_state State; };


// ----------------------------- 错误与初始化 -----------------------------

static xerrkind lz4_kind(int code)
{
    switch (code) {
    case XRT_LZ4_ERROR_ARGUMENT: return XERR_ARGUMENT;
    case XRT_LZ4_ERROR_MEMORY: return XERR_MEMORY;
    case XRT_LZ4_ERROR_LIMIT: return XERR_RANGE;
    case XRT_LZ4_ERROR_STATE: return XERR_STATE;
    case XRT_LZ4_ERROR_CANCELLED: return XERR_CANCELLED;
    case XRT_LZ4_ERROR_DATA: case XRT_LZ4_ERROR_TRUNCATED: return XERR_PROTOCOL;
    default: return XERR_IO;
    }
}

static bool lz4_error(lz4_state* s, int code, int backend, const char* message)
{
    if (s != NULL) {
        s->Error.iCode = code;
        s->Error.iBackendCode = backend;
        s->Error.iInputOffset = s->InputBytes;
        s->Error.iOutputOffset = s->OutputBytes;
        snprintf(s->Error.sMessage, sizeof(s->Error.sMessage), "%s", message);
    }
    xrtSetErrorInfo(lz4_kind(code), "xlang.extlib.lz4", code, message);
    return false;
}

/* IO、分配器和回调已有错误时只记录快照，不替换原错误对象。 */
static bool lz4_external_error(lz4_state* s, int fallback, const char* message)
{
    const xerror* error = xrtGetError();
    int code = fallback;
    if (s->Error.iCode != XRT_LZ4_OK) {
        if (error == NULL)
            xrtSetErrorInfo(lz4_kind(s->Error.iCode), "xlang.extlib.lz4",
                s->Error.iCode, s->Error.sMessage);
        return false;
    }
    if (error == NULL) return lz4_error(s, fallback, 0, message);
    switch (xrtErrorKind(error)) {
    case XERR_ARGUMENT: code = XRT_LZ4_ERROR_ARGUMENT; break;
    case XERR_MEMORY: code = XRT_LZ4_ERROR_MEMORY; break;
    case XERR_RANGE: code = XRT_LZ4_ERROR_LIMIT; break;
    case XERR_STATE: code = XRT_LZ4_ERROR_STATE; break;
    case XERR_CANCELLED: code = XRT_LZ4_ERROR_CANCELLED; break;
    default: break;
    }
    s->Error.iCode = code;
    s->Error.iBackendCode = xrtErrorCode(error);
    s->Error.iInputOffset = s->InputBytes;
    s->Error.iOutputOffset = s->OutputBytes;
    snprintf(s->Error.sMessage, sizeof(s->Error.sMessage), "%s", xrtErrorMessage(error));
    return false;
}

XRT_API void xrtLz4ErrorInit(xrtlz4error* error)
{ if (error != NULL) memset(error, 0, sizeof(*error)); }

XRT_API void xrtLz4OptionsInit(xrtlz4options* options)
{
    if (options == NULL) return;
    memset(options, 0, sizeof(*options));
    options->iSize = sizeof(*options);
    options->iVersion = XRT_LZ4_OPTIONS_VERSION;
    options->iAcceleration = 1;
    xrtResourceLimitsInit(&options->tLimits);
    options->tLimits.iMaxItemBytes = 0;
    options->tLimits.iMaxNodes = 0;
    options->tLimits.iMaxDepth = 0;
}

/* 使用官方自定义分配器接口，不修改第三方代码，也不另建 CRT 堆。 */
static void* lz4_alloc(void* opaque, size_t size) { (void)opaque; return xrtMalloc(size); }
static void lz4_free(void* opaque, void* memory) { (void)opaque; xrtFree(memory); }
static const LZ4F_CustomMem lz4_memory = {lz4_alloc, NULL, lz4_free, NULL};

static bool lz4_codec_error(lz4_state* s, size_t result)
{
    if (LZ4F_getErrorCode(result) == LZ4F_ERROR_allocation_failed)
        return lz4_external_error(s, XRT_LZ4_ERROR_MEMORY, "cannot allocate lz4 frame state");
    return lz4_error(s, XRT_LZ4_ERROR_DATA, (int)LZ4F_getErrorCode(result), LZ4F_getErrorName(result));
}

static bool lz4_append(lz4_state* s, const uint8* bytes, size_t size);

static bool lz4_init(lz4_state* s, const xrtlz4options* options, bool decode)
{
    static const uint8 reserved[4] = {0};
    LZ4F_preferences_t prefs = LZ4F_INIT_PREFERENCES;
    size_t result;
    memset(s, 0, sizeof(*s));
    s->Decode = decode;
    s->AtHeader = decode;
    s->HeaderNeed = LZ4F_MIN_SIZE_TO_KNOW_HEADER_LENGTH;
    xrtLz4OptionsInit(&s->Options);
    if (options != NULL) {
        if (options->iSize != sizeof(*options) || options->iVersion != XRT_LZ4_OPTIONS_VERSION ||
            memcmp(options->sReserved, reserved, sizeof(reserved)) != 0 ||
            options->tLimits.iSize != sizeof(options->tLimits) ||
            options->tLimits.iVersion != XRT_RESOURCE_LIMITS_VERSION || options->tLimits.iReserved != 0)
            return lz4_error(s, XRT_LZ4_ERROR_ARGUMENT, 0, "invalid lz4 options or limits version");
        if (options->tLimits.iFlags != 0 || options->tLimits.iMaxItemBytes != 0 ||
            options->tLimits.iMaxNodes != 0 || options->tLimits.iMaxDepth != 0)
            return lz4_error(s, XRT_LZ4_ERROR_ARGUMENT, 0, "unsupported lz4 resource limits");
        s->Options = *options;
    }
    if (s->Options.iAcceleration < 1 || s->Options.iAcceleration > 65537)
        return lz4_error(s, XRT_LZ4_ERROR_ARGUMENT, 0, "lz4 acceleration must be 1..65537");
    s->Output = xrtBufferCreate();
    if (s->Output == NULL) return lz4_external_error(s, XRT_LZ4_ERROR_MEMORY, "cannot allocate lz4 output");
    /* 固定 64 KiB 独立块，块和整帧均校验；update 后可直接 drain。 */
    prefs.frameInfo.blockSizeID = LZ4F_max64KB;
    prefs.frameInfo.blockMode = LZ4F_blockIndependent;
    prefs.frameInfo.blockChecksumFlag = LZ4F_blockChecksumEnabled;
    prefs.frameInfo.contentChecksumFlag = LZ4F_contentChecksumEnabled;
    prefs.compressionLevel = s->Options.iAcceleration == 1 ? 0 : -s->Options.iAcceleration;
    prefs.autoFlush = 1;
    s->ScratchSize = decode ? LZ4_CHUNK : LZ4F_compressBound(LZ4_CHUNK, &prefs);
    s->Scratch = xrtMalloc(s->ScratchSize);
    if (s->Scratch == NULL) return lz4_external_error(s, XRT_LZ4_ERROR_MEMORY, "cannot allocate lz4 scratch");
    if (decode) {
        s->Decoder = LZ4F_createDecompressionContext_advanced(lz4_memory, LZ4F_VERSION);
        return s->Decoder != NULL || lz4_external_error(s, XRT_LZ4_ERROR_MEMORY, "cannot allocate lz4 decoder");
    }
    s->Encoder = LZ4F_createCompressionContext_advanced(lz4_memory, LZ4F_VERSION);
    if (s->Encoder == NULL) return lz4_external_error(s, XRT_LZ4_ERROR_MEMORY, "cannot allocate lz4 encoder");
    result = LZ4F_compressBegin(s->Encoder, s->Scratch, s->ScratchSize, &prefs);
    if (LZ4F_isError(result)) return lz4_codec_error(s, result);
    return lz4_append(s, s->Scratch, result);
}

/* 参数/状态误用可纠正；编解码、限额、写入和回调重入失败是终态。 */
static bool lz4_ready(lz4_state* s, bool writing)
{
    if (s == NULL) return lz4_error(NULL, XRT_LZ4_ERROR_ARGUMENT, 0, "lz4 object is null");
    if (s->Busy) {
        s->Failed = true;
        return lz4_error(s, XRT_LZ4_ERROR_STATE, 0, "lz4 object cannot be reentered from its writer");
    }
    if (s->Failed) {
        xrtSetErrorInfo(lz4_kind(s->Error.iCode), "xlang.extlib.lz4", s->Error.iCode, s->Error.sMessage);
        return false;
    }
    if (s->Output == NULL || (writing && s->Finished))
        return lz4_error(s, XRT_LZ4_ERROR_STATE, 0, "lz4 stream is finished or its buffer was taken");
    xrtClearError();
    xrtLz4ErrorInit(&s->Error);
    return true;
}

static bool lz4_append(lz4_state* s, const uint8* bytes, size_t size)
{
    const xrtresourcelimits* limits = &s->Options.tLimits;
    uint64 count, frame, ratio = limits->iMaxCompressionRatio;
    if ((uint64)size > UINT64_MAX - s->OutputBytes)
        return lz4_error(s, XRT_LZ4_ERROR_LIMIT, 0, "lz4 output size overflow");
    count = s->OutputBytes + size;
    frame = s->FrameOutput + size;
    if ((limits->iMaxOutputBytes != 0 && count > limits->iMaxOutputBytes) ||
        (s->Decode && ratio != 0 && s->FrameInput <= UINT64_MAX / ratio && frame > s->FrameInput * ratio))
        return lz4_error(s, XRT_LZ4_ERROR_LIMIT, 0, "lz4 output exceeds size or per-frame expansion limit");
    if (size != 0 && !xrtBufferAppend(s->Output, (xbytesview){bytes, size}))
        return lz4_external_error(s, XRT_LZ4_ERROR_MEMORY, "cannot grow lz4 output");
    s->OutputBytes = count;
    if (s->Decode) s->FrameOutput = frame;
    return true;
}


// ----------------------------- 标准 Frame 状态机 -----------------------------

static bool lz4_encode(lz4_state* s, const uint8* data, size_t size, bool finish)
{
    while (size != 0) {
        size_t step = size > LZ4_CHUNK ? LZ4_CHUNK : size;
        size_t result = LZ4F_compressUpdate(s->Encoder, s->Scratch, s->ScratchSize, data, step, NULL);
        if (LZ4F_isError(result)) return lz4_codec_error(s, result);
        if (!lz4_append(s, s->Scratch, result)) return false;
        data += step;
        size -= step;
    }
    if (finish) {
        size_t result = LZ4F_compressEnd(s->Encoder, s->Scratch, s->ScratchSize, NULL);
        if (LZ4F_isError(result)) return lz4_codec_error(s, result);
        if (!lz4_append(s, s->Scratch, result)) return false;
        s->Finished = true;
    }
    return true;
}

/* 输入只借用到本次调用结束，未齐的帧头最多保存 19 字节，其余增量状态由 LZ4F 持有。 */
static bool lz4_decode_step(lz4_state* s, const uint8* data, size_t* consumed, size_t* produced, bool count)
{
    size_t result;
    *produced = s->ScratchSize;
    result = LZ4F_decompress(s->Decoder, s->Scratch, produced, data, consumed, NULL);
    if (count) s->FrameInput += *consumed;
    if (LZ4F_isError(result)) return lz4_codec_error(s, result);
    if (!lz4_append(s, s->Scratch, *produced)) return false;
    if (result == 0) {
        s->MemberEnded = true;
        s->AtHeader = true;
        s->HeaderSize = 0;
        s->HeaderNeed = LZ4F_MIN_SIZE_TO_KNOW_HEADER_LENGTH;
        ++s->Frames;
        if (!s->Skippable) ++s->Members;
    }
    return true;
}

static bool lz4_decode(lz4_state* s, const uint8* data, size_t size)
{
    size_t offset = 0;
    bool drain = false;
    while (offset < size || drain) {
        size_t consumed, produced;
        if (s->AtHeader) {
            LZ4F_frameInfo_t info;
            size_t result, step;
            if (offset == size) break;
            if (s->HeaderSize == 0) {
                s->FrameInput = s->FrameOutput = 0;
                s->MemberEnded = false;
                if (s->Frames == UINT64_MAX ||
                    (s->Options.tLimits.iMaxEntries != 0 && s->Frames >= s->Options.tLimits.iMaxEntries))
                    return lz4_error(s, XRT_LZ4_ERROR_LIMIT, 0, "lz4 frame count exceeds maxEntries limit");
            }
            step = s->HeaderNeed - s->HeaderSize;
            if (step > size - offset) step = size - offset;
            memcpy(s->Header + s->HeaderSize, data + offset, step);
            s->HeaderSize += step;
            s->FrameInput += step;
            offset += step;
            if (s->HeaderSize != s->HeaderNeed) continue;
            if (s->HeaderNeed == LZ4F_MIN_SIZE_TO_KNOW_HEADER_LENGTH) {
                result = LZ4F_headerSize(s->Header, s->HeaderSize);
                if (LZ4F_isError(result)) return lz4_codec_error(s, result);
                s->HeaderNeed = result;
                continue;
            }
            consumed = s->HeaderSize;
            result = LZ4F_getFrameInfo(s->Decoder, &info, s->Header, &consumed);
            if (LZ4F_isError(result)) return lz4_codec_error(s, result);
            s->Skippable = info.frameType == LZ4F_skippableFrame;
            if (info.dictID != 0)
                return lz4_error(s, XRT_LZ4_ERROR_DATA, 0, "lz4 external dictionaries are not supported");
            if (info.contentSize > UINT64_MAX - s->OutputBytes ||
                (s->Options.tLimits.iMaxOutputBytes != 0 &&
                 info.contentSize > s->Options.tLimits.iMaxOutputBytes - s->OutputBytes))
                return lz4_error(s, XRT_LZ4_ERROR_LIMIT, 0, "lz4 declared content exceeds maxOutput limit");
            s->AtHeader = false;
            /* Skippable 的 getFrameInfo 仅消费 magic，剩余长度字段仍须交给官方解码器。 */
            if (consumed < s->HeaderSize) {
                size_t expected = s->HeaderSize - consumed;
                size_t rest = expected;
                if (!lz4_decode_step(s, s->Header + consumed, &rest, &produced, false)) return false;
                if (rest != expected)
                    return lz4_error(s, XRT_LZ4_ERROR_STATE, 0, "lz4 header was not fully consumed");
            }
            drain = false;
            continue;
        }
        consumed = size - offset;
        if (!lz4_decode_step(s, data + offset, &consumed, &produced, true)) return false;
        offset += consumed;
        drain = !s->AtHeader && produced == s->ScratchSize;
        if (consumed == 0 && produced == 0) {
            if (offset == size) break;
            return lz4_error(s, XRT_LZ4_ERROR_STATE, 0, "lz4 decoder made no progress");
        }
    }
    return true;
}

static bool lz4_update(lz4_state* s, const void* data, size_t size)
{
    uint64 maximum;
    if (!lz4_ready(s, true)) return false;
    if (data == NULL && size != 0) return lz4_error(s, XRT_LZ4_ERROR_ARGUMENT, 0, "lz4 input is null");
    maximum = s->Options.tLimits.iMaxInputBytes;
    if ((uint64)size > UINT64_MAX - s->InputBytes ||
        (maximum != 0 && (s->InputBytes > maximum || (uint64)size > maximum - s->InputBytes))) {
        s->Failed = true;
        return lz4_error(s, XRT_LZ4_ERROR_LIMIT, 0, "lz4 input exceeds maxInput limit");
    }
    s->InputBytes += size;
    if (size == 0) return true;
    if (s->Decode ? lz4_decode(s, data, size) : lz4_encode(s, data, size, false)) return true;
    s->Failed = true;
    return false;
}

static bool lz4_finish(lz4_state* s)
{
    bool ok;
    if (!lz4_ready(s, false)) return false;
    if (s->Finished) return true;
    if (s->Decode) {
        if (!s->MemberEnded || s->Frames == 0)
            ok = lz4_error(s, XRT_LZ4_ERROR_TRUNCATED, 0, "input is not a complete lz4 frame stream");
        else { s->Finished = true; return true; }
    } else {
        ok = lz4_encode(s, NULL, 0, true);
    }
    if (!ok) s->Failed = true;
    return ok;
}


// ----------------------------- 输出与生命周期 -----------------------------

static bool lz4_drain(lz4_state* s, xwriter* writer)
{
    xbytesview bytes;
    bool ok;
    if (!lz4_ready(s, false)) return false;
    if (writer == NULL) return lz4_error(s, XRT_LZ4_ERROR_ARGUMENT, 0, "lz4 writer is null");
    bytes = xrtBufferView(s->Output);
    s->Busy = true;
    ok = bytes.Size == 0 || xrtWriterWriteFull(writer, bytes.Data, bytes.Size, NULL);
    s->Busy = false;
    if (!ok || s->Failed) {
        s->Failed = true;
        return lz4_external_error(s, XRT_LZ4_ERROR_IO, "cannot write lz4 output");
    }
    xrtBufferClear(s->Output);
    return true;
}

static xbuffer* lz4_take(lz4_state* s, bool final)
{
    xbuffer* output;
    xbuffer* replacement = NULL;
    if (!lz4_ready(s, false)) return NULL;
    if (final && !s->Finished) {
        lz4_error(s, XRT_LZ4_ERROR_STATE, 0, "finish lz4 before taking its buffer");
        return NULL;
    }
    if (!final) {
        replacement = xrtBufferCreate();
        if (replacement == NULL) {
            lz4_external_error(s, XRT_LZ4_ERROR_MEMORY, "cannot replace lz4 output buffer");
            return NULL;
        }
    }
    output = s->Output;
    s->Output = replacement;
    return output;
}

static bool lz4_destroy(lz4_state* s)
{
    if (s == NULL) return true;
    if (s->Busy) {
        s->Failed = true;
        return lz4_error(s, XRT_LZ4_ERROR_STATE, 0, "cannot destroy lz4 from its writer");
    }
    if (s->Encoder != NULL) LZ4F_freeCompressionContext(s->Encoder);
    if (s->Decoder != NULL) LZ4F_freeDecompressionContext(s->Decoder);
    xrtFree(s->Scratch);
    xrtBufferDestroy(s->Output);
    return true;
}

XRT_API xrtlz4compressor* xrtLz4CompressorCreate(const xrtlz4options* options)
{
    xrtlz4compressor* c;
    xrtClearError();
    c = xrtMalloc(sizeof(*c));
    if (c == NULL) return NULL;
    if (lz4_init(&c->State, options, false)) return c;
    xrtLz4CompressorDestroy(c);
    return NULL;
}
XRT_API xrtlz4decompressor* xrtLz4DecompressorCreate(const xrtlz4options* options)
{
    xrtlz4decompressor* d;
    xrtClearError();
    d = xrtMalloc(sizeof(*d));
    if (d == NULL) return NULL;
    if (lz4_init(&d->State, options, true)) return d;
    xrtLz4DecompressorDestroy(d);
    return NULL;
}
XRT_API bool xrtLz4CompressorUpdate(xrtlz4compressor* c, const void* p, size_t n)
{ return lz4_update(c ? &c->State : NULL, p, n); }
XRT_API bool xrtLz4DecompressorUpdate(xrtlz4decompressor* d, const void* p, size_t n)
{ return lz4_update(d ? &d->State : NULL, p, n); }
XRT_API bool xrtLz4CompressorFinish(xrtlz4compressor* c) { return lz4_finish(c ? &c->State : NULL); }
XRT_API bool xrtLz4DecompressorFinish(xrtlz4decompressor* d) { return lz4_finish(d ? &d->State : NULL); }
XRT_API bool xrtLz4CompressorDrain(xrtlz4compressor* c, xwriter* w) { return lz4_drain(c ? &c->State : NULL, w); }
XRT_API bool xrtLz4DecompressorDrain(xrtlz4decompressor* d, xwriter* w) { return lz4_drain(d ? &d->State : NULL, w); }
XRT_API xbuffer* xrtLz4CompressorTakeBuffer(xrtlz4compressor* c) { return lz4_take(c ? &c->State : NULL, true); }
XRT_API xbuffer* xrtLz4DecompressorTakeBuffer(xrtlz4decompressor* d) { return lz4_take(d ? &d->State : NULL, true); }
XRT_API xbuffer* xrtLz4CompressorDrainBuffer(xrtlz4compressor* c) { return lz4_take(c ? &c->State : NULL, false); }
XRT_API xbuffer* xrtLz4DecompressorDrainBuffer(xrtlz4decompressor* d) { return lz4_take(d ? &d->State : NULL, false); }
XRT_API uint64 xrtLz4CompressorInputBytes(const xrtlz4compressor* c) { return c ? c->State.InputBytes : 0; }
XRT_API uint64 xrtLz4CompressorOutputBytes(const xrtlz4compressor* c) { return c ? c->State.OutputBytes : 0; }
XRT_API uint64 xrtLz4DecompressorInputBytes(const xrtlz4decompressor* d) { return d ? d->State.InputBytes : 0; }
XRT_API uint64 xrtLz4DecompressorOutputBytes(const xrtlz4decompressor* d) { return d ? d->State.OutputBytes : 0; }
XRT_API uint64 xrtLz4DecompressorMembers(const xrtlz4decompressor* d) { return d ? d->State.Members : 0; }
XRT_API bool xrtLz4DecompressorFinished(const xrtlz4decompressor* d) { return d && !d->State.Failed && d->State.MemberEnded; }
XRT_API const xrtlz4error* xrtLz4CompressorError(const xrtlz4compressor* c) { return c ? &c->State.Error : NULL; }
XRT_API const xrtlz4error* xrtLz4DecompressorError(const xrtlz4decompressor* d) { return d ? &d->State.Error : NULL; }
XRT_API void xrtLz4CompressorDestroy(xrtlz4compressor* c) { if (lz4_destroy(c ? &c->State : NULL)) xrtFree(c); }
XRT_API void xrtLz4DecompressorDestroy(xrtlz4decompressor* d) { if (lz4_destroy(d ? &d->State : NULL)) xrtFree(d); }


// ----------------------------- 一次性与 Reader/Writer -----------------------------

static bool lz4_all(const void* data, size_t size, const xrtlz4options* options,
    xbuffer** output, xrtlz4error* error, bool decode)
{
    lz4_state state;
    bool ok;
    xrtClearError();
    xrtLz4ErrorInit(error);
    if (output != NULL) *output = NULL;
    if (output == NULL || (data == NULL && size != 0)) {
        memset(&state, 0, sizeof(state));
        lz4_error(&state, XRT_LZ4_ERROR_ARGUMENT, 0, "invalid lz4 input or output pointer");
        if (error != NULL) *error = state.Error;
        return false;
    }
    ok = lz4_init(&state, options, decode) && lz4_update(&state, data, size) && lz4_finish(&state);
    if (ok) { *output = lz4_take(&state, true); ok = *output != NULL; }
    if (!ok && error != NULL) *error = state.Error;
    lz4_destroy(&state);
    return ok;
}

XRT_API bool xrtLz4Compress(const void* p, size_t n, const xrtlz4options* o, xbuffer** b, xrtlz4error* e)
{ return lz4_all(p, n, o, b, e, false); }
XRT_API bool xrtLz4Decompress(const void* p, size_t n, const xrtlz4options* o, xbuffer** b, xrtlz4error* e)
{ return lz4_all(p, n, o, b, e, true); }

/* Reader/Writer 都由调用方持有；总量从当前游标计算，真正失败不能降级为未知大小。 */
static bool lz4_stream(xreader* reader, xwriter* writer, const xrtlz4options* options,
    xrtprogressproc progress, ptr user, xrtlz4error* error, bool decode)
{
    lz4_state s;
    uint8 input[LZ4_CHUNK];
    uint64 total = 0, position = 0;
    bool known = false, ok;
    xrtClearError();
    xrtLz4ErrorInit(error);
    memset(&s, 0, sizeof(s));
    if (reader == NULL || writer == NULL) {
        lz4_error(&s, XRT_LZ4_ERROR_ARGUMENT, 0, "lz4 reader and writer are required");
        if (error != NULL) *error = s.Error;
        return false;
    }
    ok = lz4_init(&s, options, decode);
    if (ok && xrtReaderCanSize(reader) && xrtReaderCanSeek(reader)) {
        ok = xrtReaderSize(reader, &total) && xrtReaderTell(reader, &position);
        if (!ok) lz4_external_error(&s, XRT_LZ4_ERROR_IO, "cannot query lz4 input size");
        else if (position <= total) { total -= position; known = true; }
        else total = 0;
    }
    while (ok) {
        size_t size = 0;
        if (!xrtReaderRead(reader, input, sizeof(input), &size)) {
            ok = lz4_external_error(&s, XRT_LZ4_ERROR_IO, "cannot read lz4 input");
            break;
        }
        if (size == 0) break;
        ok = lz4_update(&s, input, size) && lz4_drain(&s, writer);
        if (ok && !xrtProgressReport(progress, user, s.InputBytes, total, s.OutputBytes,
            known ? XRT_PROGRESS_TOTAL_KNOWN : 0))
            ok = lz4_external_error(&s, XRT_LZ4_ERROR_CANCELLED, "lz4 operation cancelled");
    }
    if (ok) ok = lz4_finish(&s) && lz4_drain(&s, writer);
    if (ok && !xrtProgressReport(progress, user, s.InputBytes, total, s.OutputBytes,
        XRT_PROGRESS_FINAL | (known ? XRT_PROGRESS_TOTAL_KNOWN : 0)))
        ok = lz4_external_error(&s, XRT_LZ4_ERROR_CANCELLED, "lz4 operation cancelled");
    if (!ok && error != NULL) *error = s.Error;
    lz4_destroy(&s);
    return ok;
}

XRT_API bool xrtLz4CompressStream(xreader* r, xwriter* w, const xrtlz4options* o,
    xrtprogressproc p, ptr u, xrtlz4error* e) { return lz4_stream(r, w, o, p, u, e, false); }
XRT_API bool xrtLz4DecompressStream(xreader* r, xwriter* w, const xrtlz4options* o,
    xrtprogressproc p, ptr u, xrtlz4error* e) { return lz4_stream(r, w, o, p, u, e, true); }
