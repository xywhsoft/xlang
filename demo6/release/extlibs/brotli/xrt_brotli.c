#include "include/xrt_brotli.h"
#include "third_party/include/brotli/encode.h"
#include "third_party/include/brotli/decode.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define XRT_BROTLI_CHUNK 65536u

/* 使用官方流状态机；输出直接是 Brotli 字节流，不附加私有块头或校验码。 */
struct xrtbrotlicompressor {
    BrotliEncoderState* Backend;
    xbuffer* Output;
    xrtbrotlioptions Options;
    xrtbrotlierror Error;
    uint64 InputBytes;
    uint64 OutputBytes;
    bool Finished;
    bool Failed;
    bool AllocationFailed;
};

struct xrtbrotlidecompressor {
    BrotliDecoderState* Backend;
    xbuffer* Output;
    xrtbrotlioptions Options;
    xrtbrotlierror Error;
    uint64 InputBytes;
    uint64 OutputBytes;
    uint64 MemberCount;
    bool Finished;
    bool Sealed;
    bool Failed;
    bool AllocationFailed;
};

static void __xrtBrotliSetError(xrtbrotlierror* error, int code, int backend,
    uint64 input, uint64 output, const char* format, ...)
{
    va_list args;
    if (error == NULL) return;
    error->iCode = code;
    error->iBackendCode = backend;
    error->iInputOffset = input;
    error->iOutputOffset = output;
    va_start(args, format);
    vsnprintf(error->sMessage, sizeof(error->sMessage), format, args);
    va_end(args);
    error->sMessage[sizeof(error->sMessage) - 1] = '\0';
}

static void __xrtBrotliCopyError(xrtbrotlierror* dst, const xrtbrotlierror* src)
{
    if (dst != NULL && src != NULL) *dst = *src;
}

/* 后端同样使用 XRT 分配器；单独记录 OOM，避免把它误报为压缩数据损坏。 */
static void* __xrtBrotliAlloc(void* context, size_t size)
{
    void* result = xrtMalloc(size);
    if (result == NULL) *(bool*)context = true;
    return result;
}

static void __xrtBrotliFree(void* context, void* data)
{
    (void)context;
    xrtFree(data);
}

static bool __xrtBrotliAddOverLimit(uint64 current, size_t added, uint64 limit)
{
    if ((uint64)added > UINT64_MAX - current) return true;
    return limit != 0 && current + (uint64)added > limit;
}

static bool __xrtBrotliAppend(xbuffer* buffer, const void* data, size_t size,
    const xrtresourcelimits* limits, xrtbrotlierror* error, uint64 input, uint64* output)
{
    if (__xrtBrotliAddOverLimit(*output, size, limits->iMaxOutputBytes)) {
        __xrtBrotliSetError(error, XRT_BROTLI_ERROR_LIMIT, 0, input, *output,
            "brotli output exceeds maxOutput limit");
        return false;
    }
    if (!xrtBufferAppend(buffer, (xbytesview){data, size})) {
        __xrtBrotliSetError(error, XRT_BROTLI_ERROR_MEMORY, 0, input, *output,
            "cannot grow brotli output buffer");
        return false;
    }
    *output += (uint64)size;
    return true;
}

static bool __xrtBrotliOptionsValid(const xrtbrotlioptions* options)
{
    return options->iSize == sizeof(*options) &&
        options->iVersion == XRT_BROTLI_OPTIONS_VERSION &&
        options->tLimits.iSize == sizeof(options->tLimits) &&
        options->tLimits.iVersion == XRT_RESOURCE_LIMITS_VERSION &&
        options->tLimits.iReserved == 0 &&
        options->tLimits.iFlags == 0 && options->tLimits.iMaxItemBytes == 0 &&
        options->tLimits.iMaxEntries == 0 && options->tLimits.iMaxNodes == 0 &&
        options->tLimits.iMaxDepth == 0 &&
        options->iQuality >= BROTLI_MIN_QUALITY && options->iQuality <= BROTLI_MAX_QUALITY &&
        options->iWindow >= BROTLI_MIN_WINDOW_BITS && options->iWindow <= BROTLI_MAX_WINDOW_BITS &&
        options->iMode >= BROTLI_MODE_GENERIC && options->iMode <= BROTLI_MODE_FONT;
}

XRT_API void xrtBrotliErrorInit(xrtbrotlierror* error)
{
    if (error != NULL) memset(error, 0, sizeof(*error));
}

XRT_API void xrtBrotliOptionsInit(xrtbrotlioptions* options)
{
    if (options == NULL) return;
    memset(options, 0, sizeof(*options));
    options->iSize = sizeof(*options);
    options->iVersion = XRT_BROTLI_OPTIONS_VERSION;
    options->iQuality = BROTLI_DEFAULT_QUALITY;
    options->iWindow = BROTLI_DEFAULT_WINDOW;
    options->iMode = BROTLI_DEFAULT_MODE;
    xrtResourceLimitsInit(&options->tLimits);
    options->tLimits.iMaxItemBytes = 0;
    options->tLimits.iMaxEntries = 0;
    options->tLimits.iMaxNodes = 0;
    options->tLimits.iMaxDepth = 0;
}

/* 先分配替换的空 Buffer 再转移拥有权；失败不消耗已有输出，可清错后重试。 */
static xbuffer* __xrtBrotliDrainBuffer(xbuffer** slot, xrtbrotlierror* error,
    uint64 input, uint64 output)
{
    xbuffer* replacement = xrtBufferCreate();
    xbuffer* result;
    if (replacement == NULL) {
        __xrtBrotliSetError(error, XRT_BROTLI_ERROR_MEMORY, 0, input, output,
            "cannot allocate brotli drain buffer");
        return NULL;
    }
    result = *slot;
    *slot = replacement;
    xrtBrotliErrorInit(error);
    return result;
}

/* ------------------------------------ 压缩器 ------------------------------------ */

XRT_API xrtbrotlicompressor* xrtBrotliCompressorCreate(const xrtbrotlioptions* options)
{
    xrtbrotlicompressor* c = (xrtbrotlicompressor*)xrtCalloc(1, sizeof(*c));
    if (c == NULL) return NULL;
    if (options != NULL) c->Options = *options;
    else xrtBrotliOptionsInit(&c->Options);
    if (!__xrtBrotliOptionsValid(&c->Options)) {
        __xrtBrotliSetError(&c->Error, XRT_BROTLI_ERROR_ARGUMENT, 0, 0, 0,
            "invalid brotli options");
        c->Failed = true;
        return c;
    }
    c->Output = xrtBufferCreate();
    c->Backend = BrotliEncoderCreateInstance(__xrtBrotliAlloc, __xrtBrotliFree, &c->AllocationFailed);
    if (c->Output == NULL || c->Backend == NULL) {
        __xrtBrotliSetError(&c->Error, XRT_BROTLI_ERROR_MEMORY, 0, 0, 0,
            "cannot allocate brotli compressor");
        c->Failed = true;
    } else if (!BrotliEncoderSetParameter(c->Backend, BROTLI_PARAM_QUALITY, (uint32_t)c->Options.iQuality) ||
        !BrotliEncoderSetParameter(c->Backend, BROTLI_PARAM_LGWIN, (uint32_t)c->Options.iWindow) ||
        !BrotliEncoderSetParameter(c->Backend, BROTLI_PARAM_MODE, (uint32_t)c->Options.iMode)) {
        __xrtBrotliSetError(&c->Error, XRT_BROTLI_ERROR_ARGUMENT, 0, 0, 0,
            "brotli encoder rejected options");
        c->Failed = true;
    }
    return c;
}

static bool __xrtBrotliEncode(xrtbrotlicompressor* c, const void* data, size_t size,
    BrotliEncoderOperation operation)
{
    const uint8_t* next = (const uint8_t*)data;
    size_t remaining = size;
    uint8_t chunk[XRT_BROTLI_CHUNK];
    do {
        uint8_t* output = chunk;
        size_t capacity = sizeof(chunk);
        size_t before = remaining;
        if (!BrotliEncoderCompressStream(c->Backend, operation, &remaining, &next,
                &capacity, &output, NULL)) {
            __xrtBrotliSetError(&c->Error, c->AllocationFailed ? XRT_BROTLI_ERROR_MEMORY :
                XRT_BROTLI_ERROR_STATE, 0, c->InputBytes, c->OutputBytes,
                "brotli encoder failed");
            c->Failed = true;
            return false;
        }
        c->InputBytes += (uint64)(before - remaining);
        if (!__xrtBrotliAppend(c->Output, chunk, sizeof(chunk) - capacity,
                &c->Options.tLimits, &c->Error, c->InputBytes, &c->OutputBytes)) {
            c->Failed = true;
            return false;
        }
    } while (remaining != 0 || BrotliEncoderHasMoreOutput(c->Backend) ||
        (operation == BROTLI_OPERATION_FINISH && !BrotliEncoderIsFinished(c->Backend)));
    return true;
}

XRT_API bool xrtBrotliCompressorUpdate(xrtbrotlicompressor* c, const void* data, size_t size)
{
    if (c == NULL || c->Failed) return false;
    if (c->Finished || c->Output == NULL || (data == NULL && size != 0)) {
        __xrtBrotliSetError(&c->Error, c->Finished || c->Output == NULL ?
            XRT_BROTLI_ERROR_STATE : XRT_BROTLI_ERROR_ARGUMENT, 0, c->InputBytes,
            c->OutputBytes, "invalid brotli compressor update");
        c->Failed = true;
        return false;
    }
    if (__xrtBrotliAddOverLimit(c->InputBytes, size, c->Options.tLimits.iMaxInputBytes)) {
        __xrtBrotliSetError(&c->Error, XRT_BROTLI_ERROR_LIMIT, 0, c->InputBytes,
            c->OutputBytes, "brotli input exceeds maxInput limit");
        c->Failed = true;
        return false;
    }
    return __xrtBrotliEncode(c, data, size, BROTLI_OPERATION_PROCESS);
}

XRT_API bool xrtBrotliCompressorFinish(xrtbrotlicompressor* c)
{
    if (c == NULL || c->Failed) return false;
    if (c->Finished) return true;
    if (!__xrtBrotliEncode(c, NULL, 0, BROTLI_OPERATION_FINISH)) return false;
    c->Finished = true;
    return true;
}

XRT_API bool xrtBrotliCompressorDrain(xrtbrotlicompressor* c, xwriter* writer)
{
    xbytesview output;
    if (c == NULL || c->Failed) return false;
    if (c->Output == NULL) {
        __xrtBrotliSetError(&c->Error, XRT_BROTLI_ERROR_STATE, 0, c->InputBytes,
            c->OutputBytes, "brotli output was already taken");
        c->Failed = true;
        return false;
    }
    output = xrtBufferView(c->Output);
    if (writer == NULL || !xrtWriterWriteFull(writer, output.Data, output.Size, NULL)) {
        __xrtBrotliSetError(&c->Error, XRT_BROTLI_ERROR_IO, 0, c->InputBytes,
            c->OutputBytes, "cannot write brotli output stream");
        c->Failed = true;
        return false;
    }
    xrtBufferClear(c->Output);
    return true;
}

XRT_API xbuffer* xrtBrotliCompressorTakeBuffer(xrtbrotlicompressor* c)
{
    xbuffer* result;
    if (c == NULL || c->Failed) return NULL;
    if (!c->Finished || c->Output == NULL) {
        __xrtBrotliSetError(&c->Error, XRT_BROTLI_ERROR_STATE, 0, c->InputBytes,
            c->OutputBytes, "brotli take requires finished, owned output");
        c->Failed = true;
        return NULL;
    }
    result = c->Output;
    c->Output = NULL;
    return result;
}

XRT_API xbuffer* xrtBrotliCompressorDrainBuffer(xrtbrotlicompressor* c)
{
    if (c == NULL || c->Failed) return NULL;
    if (c->Output == NULL) {
        __xrtBrotliSetError(&c->Error, XRT_BROTLI_ERROR_STATE, 0, c->InputBytes,
            c->OutputBytes, "brotli output was already taken");
        c->Failed = true;
        return NULL;
    }
    return __xrtBrotliDrainBuffer(&c->Output, &c->Error, c->InputBytes, c->OutputBytes);
}

XRT_API uint64 xrtBrotliCompressorInputBytes(const xrtbrotlicompressor* c)
{
    return c != NULL ? c->InputBytes : 0;
}

XRT_API uint64 xrtBrotliCompressorOutputBytes(const xrtbrotlicompressor* c)
{
    return c != NULL ? c->OutputBytes : 0;
}

XRT_API const xrtbrotlierror* xrtBrotliCompressorError(const xrtbrotlicompressor* c)
{
    return c != NULL ? &c->Error : NULL;
}

XRT_API void xrtBrotliCompressorDestroy(xrtbrotlicompressor* c)
{
    if (c == NULL) return;
    BrotliEncoderDestroyInstance(c->Backend);
    xrtBufferDestroy(c->Output);
    xrtFree(c);
}

/* ------------------------------------ 解压器 ------------------------------------ */

XRT_API xrtbrotlidecompressor* xrtBrotliDecompressorCreate(const xrtbrotlioptions* options)
{
    xrtbrotlidecompressor* d = (xrtbrotlidecompressor*)xrtCalloc(1, sizeof(*d));
    if (d == NULL) return NULL;
    if (options != NULL) d->Options = *options;
    else xrtBrotliOptionsInit(&d->Options);
    if (!__xrtBrotliOptionsValid(&d->Options)) {
        __xrtBrotliSetError(&d->Error, XRT_BROTLI_ERROR_ARGUMENT, 0, 0, 0,
            "invalid brotli options");
        d->Failed = true;
        return d;
    }
    d->Output = xrtBufferCreate();
    if (d->Output == NULL) {
        __xrtBrotliSetError(&d->Error, XRT_BROTLI_ERROR_MEMORY, 0, 0, 0,
            "cannot allocate brotli decompressor");
        d->Failed = true;
    }
    return d;
}

XRT_API bool xrtBrotliDecompressorUpdate(xrtbrotlidecompressor* d, const void* data, size_t size)
{
    const uint8_t* next = (const uint8_t*)data;
    size_t remaining = size;
    uint8_t chunk[XRT_BROTLI_CHUNK];
    if (d == NULL || d->Failed) return false;
    if (d->Sealed || d->Output == NULL || (data == NULL && size != 0)) {
        __xrtBrotliSetError(&d->Error, d->Sealed || d->Output == NULL ?
            XRT_BROTLI_ERROR_STATE : XRT_BROTLI_ERROR_ARGUMENT, 0, d->InputBytes,
            d->OutputBytes, "invalid brotli decompressor update");
        d->Failed = true;
        return false;
    }
    if (__xrtBrotliAddOverLimit(d->InputBytes, size, d->Options.tLimits.iMaxInputBytes)) {
        __xrtBrotliSetError(&d->Error, XRT_BROTLI_ERROR_LIMIT, 0, d->InputBytes,
            d->OutputBytes, "brotli input exceeds maxInput limit");
        d->Failed = true;
        return false;
    }
    if (size == 0) return true;
    for (;;) {
        BrotliDecoderResult result;
        uint8_t* output = chunk;
        size_t capacity = sizeof(chunk);
        size_t before = remaining;
        if (d->Backend == NULL) {
            d->Backend = BrotliDecoderCreateInstance(__xrtBrotliAlloc, __xrtBrotliFree, &d->AllocationFailed);
            if (d->Backend == NULL) {
                __xrtBrotliSetError(&d->Error, XRT_BROTLI_ERROR_MEMORY, 0, d->InputBytes,
                    d->OutputBytes, "cannot allocate brotli decoder state");
                d->Failed = true;
                return false;
            }
            d->Finished = false;
        }
        result = BrotliDecoderDecompressStream(d->Backend, &remaining, &next, &capacity, &output, NULL);
        d->InputBytes += (uint64)(before - remaining);
        if (result == BROTLI_DECODER_RESULT_ERROR) {
            int code = (int)BrotliDecoderGetErrorCode(d->Backend);
            __xrtBrotliSetError(&d->Error, d->AllocationFailed ? XRT_BROTLI_ERROR_MEMORY :
                XRT_BROTLI_ERROR_DATA, code, d->InputBytes, d->OutputBytes,
                "brotli decoder failed: %s", BrotliDecoderErrorString((BrotliDecoderErrorCode)code));
            d->Failed = true;
            return false;
        }
        if (!__xrtBrotliAppend(d->Output, chunk, sizeof(chunk) - capacity,
                &d->Options.tLimits, &d->Error, d->InputBytes, &d->OutputBytes)) {
            d->Failed = true;
            return false;
        }
        /* 比例在完整成员边界检查，避免输入分块大小改变是否被接纳；输出硬限额始终生效。 */
        if (result == BROTLI_DECODER_RESULT_SUCCESS) {
            uint64 ratio = d->Options.tLimits.iMaxCompressionRatio;
            if (ratio != 0 && d->InputBytes <= UINT64_MAX / ratio &&
                d->OutputBytes > d->InputBytes * ratio) {
                __xrtBrotliSetError(&d->Error, XRT_BROTLI_ERROR_LIMIT, 0, d->InputBytes,
                    d->OutputBytes, "brotli output exceeds maxRatio limit");
                d->Failed = true;
                return false;
            }
            ++d->MemberCount;
            d->Finished = true;
            BrotliDecoderDestroyInstance(d->Backend);
            d->Backend = NULL;
            if (remaining == 0) return true;
        } else if (result == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT) {
            return true;
        }
    }
}

XRT_API bool xrtBrotliDecompressorFinish(xrtbrotlidecompressor* d)
{
    if (d == NULL || d->Failed) return false;
    if (!d->Finished || d->MemberCount == 0) {
        __xrtBrotliSetError(&d->Error, XRT_BROTLI_ERROR_TRUNCATED, 0, d->InputBytes,
            d->OutputBytes, "brotli input is empty or truncated");
        d->Failed = true;
        return false;
    }
    d->Sealed = true;
    return true;
}

XRT_API bool xrtBrotliDecompressorDrain(xrtbrotlidecompressor* d, xwriter* writer)
{
    xbytesview output;
    if (d == NULL || d->Failed) return false;
    if (d->Output == NULL) {
        __xrtBrotliSetError(&d->Error, XRT_BROTLI_ERROR_STATE, 0, d->InputBytes,
            d->OutputBytes, "brotli output was already taken");
        d->Failed = true;
        return false;
    }
    output = xrtBufferView(d->Output);
    if (writer == NULL || !xrtWriterWriteFull(writer, output.Data, output.Size, NULL)) {
        __xrtBrotliSetError(&d->Error, XRT_BROTLI_ERROR_IO, 0, d->InputBytes,
            d->OutputBytes, "cannot write brotli output stream");
        d->Failed = true;
        return false;
    }
    xrtBufferClear(d->Output);
    return true;
}

XRT_API xbuffer* xrtBrotliDecompressorTakeBuffer(xrtbrotlidecompressor* d)
{
    xbuffer* result;
    if (d == NULL || d->Failed) return NULL;
    if (!d->Sealed || d->Output == NULL) {
        __xrtBrotliSetError(&d->Error, XRT_BROTLI_ERROR_STATE, 0, d->InputBytes,
            d->OutputBytes, "brotli take requires finished, owned output");
        d->Failed = true;
        return NULL;
    }
    result = d->Output;
    d->Output = NULL;
    return result;
}

XRT_API xbuffer* xrtBrotliDecompressorDrainBuffer(xrtbrotlidecompressor* d)
{
    if (d == NULL || d->Failed) return NULL;
    if (d->Output == NULL) {
        __xrtBrotliSetError(&d->Error, XRT_BROTLI_ERROR_STATE, 0, d->InputBytes,
            d->OutputBytes, "brotli output was already taken");
        d->Failed = true;
        return NULL;
    }
    return __xrtBrotliDrainBuffer(&d->Output, &d->Error, d->InputBytes, d->OutputBytes);
}

XRT_API uint64 xrtBrotliDecompressorInputBytes(const xrtbrotlidecompressor* d)
{
    return d != NULL ? d->InputBytes : 0;
}

XRT_API uint64 xrtBrotliDecompressorOutputBytes(const xrtbrotlidecompressor* d)
{
    return d != NULL ? d->OutputBytes : 0;
}

XRT_API bool xrtBrotliDecompressorFinished(const xrtbrotlidecompressor* d)
{
    return d != NULL && !d->Failed && d->Finished;
}

XRT_API uint64 xrtBrotliDecompressorMembers(const xrtbrotlidecompressor* d)
{
    return d != NULL ? d->MemberCount : 0;
}

XRT_API const xrtbrotlierror* xrtBrotliDecompressorError(const xrtbrotlidecompressor* d)
{
    return d != NULL ? &d->Error : NULL;
}

XRT_API void xrtBrotliDecompressorDestroy(xrtbrotlidecompressor* d)
{
    if (d == NULL) return;
    BrotliDecoderDestroyInstance(d->Backend);
    xrtBufferDestroy(d->Output);
    xrtFree(d);
}

// ------------------------------------ 一次性接口 ------------------------------------

XRT_API bool xrtBrotliCompress(const void* pData, size_t iSize, const xrtbrotlioptions* pOptions,
	xbuffer** ppOutput, xrtbrotlierror* pError)
{
	xrtbrotlioptions tOptions;
	xrtbrotlicompressor* pCompressor;
	bool bRet;
	xrtBrotliErrorInit(pError);
	if ( ppOutput != NULL ) { *ppOutput = NULL; }
	if ( ppOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtBrotliSetError(pError, XRT_BROTLI_ERROR_ARGUMENT, 0, 0, 0, "invalid brotli input");
		return false;
	}
	if ( pOptions != NULL ) { tOptions = *pOptions; } else { xrtBrotliOptionsInit(&tOptions); }
	pCompressor = xrtBrotliCompressorCreate(&tOptions);
	if ( pCompressor == NULL ) {
		__xrtBrotliSetError(pError, XRT_BROTLI_ERROR_MEMORY, 0, 0, 0, "cannot allocate brotli compressor");
		return false;
	}
	bRet = xrtBrotliCompressorUpdate(pCompressor, pData, iSize) &&
		xrtBrotliCompressorFinish(pCompressor);
	if ( bRet ) { *ppOutput = xrtBrotliCompressorTakeBuffer(pCompressor); bRet = *ppOutput != NULL; }
	if ( !bRet ) { __xrtBrotliCopyError(pError, &pCompressor->Error); }
	xrtBrotliCompressorDestroy(pCompressor);
	return bRet;
}

XRT_API bool xrtBrotliDecompress(const void* pData, size_t iSize, const xrtbrotlioptions* pOptions,
	xbuffer** ppOutput, xrtbrotlierror* pError)
{
	xrtbrotlioptions tOptions;
	xrtbrotlidecompressor* pDecompressor;
	bool bRet;
	xrtBrotliErrorInit(pError);
	if ( ppOutput != NULL ) { *ppOutput = NULL; }
	if ( ppOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtBrotliSetError(pError, XRT_BROTLI_ERROR_ARGUMENT, 0, 0, 0, "invalid brotli input");
		return false;
	}
	if ( pOptions != NULL ) { tOptions = *pOptions; } else { xrtBrotliOptionsInit(&tOptions); }
	pDecompressor = xrtBrotliDecompressorCreate(&tOptions);
	if ( pDecompressor == NULL ) {
		__xrtBrotliSetError(pError, XRT_BROTLI_ERROR_MEMORY, 0, 0, 0, "cannot allocate brotli decompressor");
		return false;
	}
	bRet = xrtBrotliDecompressorUpdate(pDecompressor, pData, iSize) &&
		xrtBrotliDecompressorFinish(pDecompressor);
	if ( bRet ) { *ppOutput = xrtBrotliDecompressorTakeBuffer(pDecompressor); bRet = *ppOutput != NULL; }
	if ( !bRet ) { __xrtBrotliCopyError(pError, &pDecompressor->Error); }
	xrtBrotliDecompressorDestroy(pDecompressor);
	return bRet;
}



// ------------------------------------ 文件流接口 ------------------------------------

/* 进度总量指本次尚未读取的范围；无法查询时报告未知，不改变读取位置。 */
static bool __xrtBrotliRemainingSize(xreader* reader, uint64* total)
{
    uint64 position;
    if (xrtReaderCanSeek(reader) && xrtReaderSize(reader, total) &&
        xrtReaderTell(reader, &position) && position <= *total) {
        *total -= position;
        return true;
    }
    *total = 0;
    xrtClearError();
    return false;
}

XRT_API bool xrtBrotliCompressStream(xreader* pReader, xwriter* pWriter, const xrtbrotlioptions* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtbrotlierror* pError)
{
	uint8 sInput[XRT_BROTLI_CHUNK];
	xrtbrotlicompressor* pCompressor;
	uint64 iTotal = 0u;
	bool bTotalKnown;
	bool bRet = true;
	xrtBrotliErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		__xrtBrotliSetError(pError, XRT_BROTLI_ERROR_ARGUMENT, 0, 0, 0, "invalid brotli stream");
		return false;
	}
	bTotalKnown = __xrtBrotliRemainingSize(pReader, &iTotal);
	pCompressor = xrtBrotliCompressorCreate(pOptions);
	if ( pCompressor == NULL ) {
		__xrtBrotliSetError(pError, XRT_BROTLI_ERROR_MEMORY, 0, 0, 0,
			"cannot allocate brotli compressor");
		return false;
	}
	if ( pCompressor->Failed ) {
		__xrtBrotliCopyError(pError, &pCompressor->Error);
		xrtBrotliCompressorDestroy(pCompressor);
		return false;
	}
	while ( bRet ) {
		size_t iRead = 0u;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_IO, 0,
				pCompressor->InputBytes, pCompressor->OutputBytes, "cannot read brotli input stream");
			bRet = false;
			break;
		}
		if ( iRead == 0u ) { break; }
		bRet = xrtBrotliCompressorUpdate(pCompressor, sInput, (size_t)iRead) &&
			xrtBrotliCompressorDrain(pCompressor, pWriter);
		if ( bRet && !xrtProgressReport(pProgress, pUserData, pCompressor->InputBytes,
			iTotal, pCompressor->OutputBytes,
			bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u) ) {
			__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_CANCELLED, 0,
				pCompressor->InputBytes, pCompressor->OutputBytes, "brotli operation cancelled");
			bRet = false;
		}
	}
	if ( bRet ) { bRet = xrtBrotliCompressorFinish(pCompressor) && xrtBrotliCompressorDrain(pCompressor, pWriter); }
	if ( bRet && !xrtProgressReport(pProgress, pUserData, pCompressor->InputBytes,
		iTotal, pCompressor->OutputBytes, XRT_PROGRESS_FINAL |
		(bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u)) ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_CANCELLED, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "brotli operation cancelled");
		bRet = false;
	}
	if ( !bRet ) { __xrtBrotliCopyError(pError, &pCompressor->Error); }
	xrtBrotliCompressorDestroy(pCompressor);
	return bRet;
}

XRT_API bool xrtBrotliDecompressStream(xreader* pReader, xwriter* pWriter, const xrtbrotlioptions* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtbrotlierror* pError)
{
	uint8 sInput[XRT_BROTLI_CHUNK];
	xrtbrotlidecompressor* pDecompressor;
	uint64 iTotal = 0u;
	bool bTotalKnown;
	bool bRet = true;
	xrtBrotliErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		__xrtBrotliSetError(pError, XRT_BROTLI_ERROR_ARGUMENT, 0, 0, 0, "invalid brotli stream");
		return false;
	}
	bTotalKnown = __xrtBrotliRemainingSize(pReader, &iTotal);
	pDecompressor = xrtBrotliDecompressorCreate(pOptions);
	if ( pDecompressor == NULL ) {
		__xrtBrotliSetError(pError, XRT_BROTLI_ERROR_MEMORY, 0, 0, 0,
			"cannot allocate brotli decompressor");
		return false;
	}
	if ( pDecompressor->Failed ) {
		__xrtBrotliCopyError(pError, &pDecompressor->Error);
		xrtBrotliDecompressorDestroy(pDecompressor);
		return false;
	}
	while ( bRet ) {
		size_t iRead = 0u;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_IO, 0,
				pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot read brotli input stream");
			bRet = false;
			break;
		}
		if ( iRead == 0u ) { break; }
		bRet = xrtBrotliDecompressorUpdate(pDecompressor, sInput, (size_t)iRead) &&
			xrtBrotliDecompressorDrain(pDecompressor, pWriter);
		if ( bRet && !xrtProgressReport(pProgress, pUserData, pDecompressor->InputBytes,
			iTotal, pDecompressor->OutputBytes,
			bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u) ) {
			__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_CANCELLED, 0,
				pDecompressor->InputBytes, pDecompressor->OutputBytes, "brotli operation cancelled");
			bRet = false;
		}
	}
	if ( bRet ) { bRet = xrtBrotliDecompressorFinish(pDecompressor) && xrtBrotliDecompressorDrain(pDecompressor, pWriter); }
	if ( bRet && !xrtProgressReport(pProgress, pUserData, pDecompressor->InputBytes,
		iTotal, pDecompressor->OutputBytes, XRT_PROGRESS_FINAL |
		(bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u)) ) {
		__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_CANCELLED, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "brotli operation cancelled");
		bRet = false;
	}
	if ( !bRet ) { __xrtBrotliCopyError(pError, &pDecompressor->Error); }
	xrtBrotliDecompressorDestroy(pDecompressor);
	return bRet;
}
