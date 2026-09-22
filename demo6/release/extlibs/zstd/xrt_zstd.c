#include "include/xrt_zstd.h"
#include "third_party/zstd.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>



#define XRT_ZSTD_CHUNK 65536u

struct xrtzstdcompressor {
	ZSTD_CStream* Stream;
	xbuffer* Output;
	xrtzstdoptions Options;
	xrtzstderror Error;
	uint64 InputBytes;
	uint64 OutputBytes;
	bool Finished;
	bool Failed;
};

struct xrtzstddecompressor {
	ZSTD_DStream* Stream;
	xbuffer* Output;
	xrtzstdoptions Options;
	xrtzstderror Error;
	uint64 InputBytes;
	uint64 OutputBytes;
	uint64 MemberOutputBytes;
	uint64 MemberCount;
	bool FrameEnded;
	bool Finished;
	bool Failed;
};



static void __xrtZstdSetError(xrtzstderror* pError, int iCode, int iBackend,
	uint64 iInput, uint64 iOutput, const char* sFormat, ...)
{
	va_list pArgs;
	if ( pError == NULL ) { return; }
	pError->iCode = iCode;
	pError->iBackendCode = iBackend;
	pError->iInputOffset = iInput;
	pError->iOutputOffset = iOutput;
	va_start(pArgs, sFormat);
	vsnprintf(pError->sMessage, sizeof(pError->sMessage), sFormat, pArgs);
	va_end(pArgs);
	pError->sMessage[sizeof(pError->sMessage) - 1] = '\0';
}

static void __xrtZstdCopyError(xrtzstderror* pDst, const xrtzstderror* pSrc)
{
	if ( pDst == NULL || pSrc == NULL ) { return; }
	*pDst = *pSrc;
}

static bool __xrtZstdOverRatio(const xrtresourcelimits* pLimits, uint64 iInput, uint64 iOutput)
{
	uint64 iRatio = pLimits->iMaxCompressionRatio;
	if ( iRatio == 0u || iInput == 0u ) { return false; }
	if ( iInput > (UINT64_MAX / iRatio) ) { return false; }
	return iOutput > iInput * iRatio;
}

/* 检查计数累加是否溢出或越过非零硬上限。 */
static bool __xrtZstdAddOverLimit(uint64 iCurrent, size_t iAdded, uint64 iLimit)
{
	uint64 iIncrement = (uint64)iAdded;

	if ( iIncrement > (UINT64_MAX - iCurrent) ) { return true; }
	return (iLimit != 0u) && ((iCurrent + iIncrement) > iLimit);
}

/* 创建带初始容量的 Buffer，失败时不泄漏半初始化对象。 */
static xbuffer* __xrtZstdBufferCreate(size_t iCapacity)
{
	xbuffer* pBuffer = xrtBufferCreate();

	if ( (pBuffer != NULL) && !xrtBufferReserve(pBuffer, iCapacity) ) {
		xrtBufferDestroy(pBuffer);
		pBuffer = NULL;
	}
	return pBuffer;
}

/* 为一次性和流式压缩统一设置完整上下文参数。 */
static bool __xrtZstdConfigureCompressor(ZSTD_CCtx* pContext, int iLevel,
	xrtzstderror* pError)
{
	size_t tResult;

	if ( iLevel == 0 ) { iLevel = ZSTD_defaultCLevel(); }
	tResult = ZSTD_CCtx_setParameter(pContext, ZSTD_c_compressionLevel, iLevel);
	if ( !ZSTD_isError(tResult) ) {
		tResult = ZSTD_CCtx_setParameter(pContext, ZSTD_c_checksumFlag, 1);
	}
	/* 内嵌版未包含 zstd_ldm.c，必须关闭自动长距离匹配。 */
	if ( !ZSTD_isError(tResult) ) {
		tResult = ZSTD_CCtx_setParameter(pContext,
			ZSTD_c_enableLongDistanceMatching, 2 /* ZSTD_ps_disable */);
	}
	if ( ZSTD_isError(tResult) ) {
		__xrtZstdSetError(pError, XRT_ZSTD_ERROR_STATE, (int)tResult,
			0u, 0u, "%s", ZSTD_getErrorName(tResult));
		return false;
	}
	return true;
}



XRT_API void xrtZstdErrorInit(xrtzstderror* pError)
{
	if ( pError == NULL ) { return; }
	memset(pError, 0, sizeof(*pError));
	pError->iCode = XRT_ZSTD_OK;
}

XRT_API void xrtZstdOptionsInit(xrtzstdoptions* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_ZSTD_OPTIONS_VERSION;
	pOptions->iLevel = ZSTD_defaultCLevel();
	xrtResourceLimitsInit(&pOptions->tLimits);
}



// ------------------------------------ 压缩器 ------------------------------------

XRT_API xrtzstdcompressor* xrtZstdCompressorCreate(const xrtzstdoptions* pOptions)
{
	xrtzstdcompressor* pCompressor;
	pCompressor = (xrtzstdcompressor*)xrtMalloc(sizeof(xrtzstdcompressor));
	if ( pCompressor == NULL ) { return NULL; }
	memset(pCompressor, 0, sizeof(*pCompressor));
	xrtZstdErrorInit(&pCompressor->Error);
	if ( pOptions != NULL ) {
		pCompressor->Options = *pOptions;
	} else {
		xrtZstdOptionsInit(&pCompressor->Options);
	}
	pCompressor->Stream = ZSTD_createCStream();
	pCompressor->Output = __xrtZstdBufferCreate(XRT_ZSTD_CHUNK);
	if ( pCompressor->Stream == NULL || pCompressor->Output == NULL ) {
		__xrtZstdSetError(&pCompressor->Error, XRT_ZSTD_ERROR_MEMORY, 0, 0, 0,
			"cannot allocate zstd compressor");
		pCompressor->Failed = true;
		return pCompressor;
	}
	if ( !__xrtZstdConfigureCompressor(pCompressor->Stream,
		pCompressor->Options.iLevel, &pCompressor->Error) ) {
		pCompressor->Failed = true;
		return pCompressor;
	}
	return pCompressor;
}

static bool __xrtZstdCompressRun(xrtzstdcompressor* pCompressor, const uint8* pData,
	size_t iSize, bool bEnd)
{
	ZSTD_inBuffer tInput;
	ZSTD_EndDirective tMode;
	uint8 sChunk[XRT_ZSTD_CHUNK];
	bool bFinished;
	tInput.src = pData;
	tInput.size = iSize;
	tInput.pos = 0u;
	tMode = bEnd ? ZSTD_e_end : ZSTD_e_continue;
	do {
		ZSTD_outBuffer tOutput;
		size_t tRemaining;
		tOutput.dst = sChunk;
		tOutput.size = sizeof(sChunk);
		tOutput.pos = 0u;
		tRemaining = ZSTD_compressStream2(pCompressor->Stream, &tOutput, &tInput, tMode);
		if ( ZSTD_isError(tRemaining) ) {
			__xrtZstdSetError(&pCompressor->Error, XRT_ZSTD_ERROR_DATA, (int)tRemaining,
				pCompressor->InputBytes, pCompressor->OutputBytes, "%s", ZSTD_getErrorName(tRemaining));
			pCompressor->Failed = true;
			return false;
		}
		if ( tOutput.pos != 0u ) {
			if ( __xrtZstdAddOverLimit(pCompressor->OutputBytes, tOutput.pos,
				pCompressor->Options.tLimits.iMaxOutputBytes) ) {
				__xrtZstdSetError(&pCompressor->Error, XRT_ZSTD_ERROR_LIMIT, 0,
					pCompressor->InputBytes, pCompressor->OutputBytes,
					"zstd output exceeds maxOutput limit");
				pCompressor->Failed = true;
				return false;
			}
			if ( !xrtBufferAppend(pCompressor->Output,
				(xbytesview){ (const unsigned char*)sChunk, tOutput.pos }) ) {
				__xrtZstdSetError(&pCompressor->Error, XRT_ZSTD_ERROR_MEMORY, 0,
					pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow zstd output buffer");
				pCompressor->Failed = true;
				return false;
			}
			pCompressor->OutputBytes += tOutput.pos;
		}
		bFinished = bEnd ? (tRemaining == 0u) : (tInput.pos >= tInput.size);
	} while ( !bFinished );
	return true;
}

XRT_API bool xrtZstdCompressorUpdate(xrtzstdcompressor* pCompressor, const void* pData, size_t iSize)
{
	if ( pCompressor == NULL || pCompressor->Failed ) { return false; }
	if ( pCompressor->Finished ) {
		__xrtZstdSetError(&pCompressor->Error, XRT_ZSTD_ERROR_STATE, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "zstd compressor already finished");
		pCompressor->Failed = true;
		return false;
	}
	if ( iSize != 0u && pData == NULL ) {
		__xrtZstdSetError(&pCompressor->Error, XRT_ZSTD_ERROR_ARGUMENT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "invalid zstd input");
		pCompressor->Failed = true;
		return false;
	}
	if ( __xrtZstdAddOverLimit(pCompressor->InputBytes, iSize,
		pCompressor->Options.tLimits.iMaxInputBytes) ) {
		__xrtZstdSetError(&pCompressor->Error, XRT_ZSTD_ERROR_LIMIT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "zstd input exceeds maxInput limit");
		pCompressor->Failed = true;
		return false;
	}
	pCompressor->InputBytes += iSize;
	if ( iSize == 0u ) { return true; }
	return __xrtZstdCompressRun(pCompressor, (const uint8*)pData, iSize, false);
}

XRT_API bool xrtZstdCompressorFinish(xrtzstdcompressor* pCompressor)
{
	static const uint8 iEmpty = 0u;
	if ( pCompressor == NULL || pCompressor->Failed ) { return false; }
	if ( pCompressor->Finished ) { return true; }
	if ( !__xrtZstdCompressRun(pCompressor, &iEmpty, 0u, true) ) { return false; }
	pCompressor->Finished = true;
	return true;
}

XRT_API bool xrtZstdCompressorDrain(xrtzstdcompressor* pCompressor, xwriter* pWriter)
{
	xbytesview Output;
	if ( pCompressor == NULL || pCompressor->Failed || pWriter == NULL ) { return false; }
	Output = xrtBufferView(pCompressor->Output);
	if ( Output.Size != 0u &&
		!xrtWriterWriteFull(pWriter, Output.Data, Output.Size, NULL) ) {
		__xrtZstdSetError(&pCompressor->Error, XRT_ZSTD_ERROR_IO, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot write zstd output stream");
		pCompressor->Failed = true;
		return false;
	}
	xrtBufferClear(pCompressor->Output);
	return true;
}

XRT_API xbuffer* xrtZstdCompressorTakeBuffer(xrtzstdcompressor* pCompressor)
{
	xbuffer* pOutput;
	if ( pCompressor == NULL || pCompressor->Failed || !pCompressor->Finished ) { return NULL; }
	pOutput = pCompressor->Output;
	pCompressor->Output = NULL;
	return pOutput;
}

XRT_API xbuffer* xrtZstdCompressorDrainBuffer(xrtzstdcompressor* pCompressor)
{
	xbuffer* pOutput;
	if ( pCompressor == NULL || pCompressor->Failed || pCompressor->Output == NULL ) { return NULL; }
	pOutput = xrtBufferFrom(xrtBufferView(pCompressor->Output));
	if ( pOutput != NULL ) { xrtBufferClear(pCompressor->Output); }
	return pOutput;
}

XRT_API uint64 xrtZstdCompressorInputBytes(const xrtzstdcompressor* pCompressor)
{
	return pCompressor != NULL ? pCompressor->InputBytes : 0u;
}

XRT_API uint64 xrtZstdCompressorOutputBytes(const xrtzstdcompressor* pCompressor)
{
	return pCompressor != NULL ? pCompressor->OutputBytes : 0u;
}

XRT_API const xrtzstderror* xrtZstdCompressorError(const xrtzstdcompressor* pCompressor)
{
	return pCompressor != NULL ? &pCompressor->Error : NULL;
}

XRT_API void xrtZstdCompressorDestroy(xrtzstdcompressor* pCompressor)
{
	if ( pCompressor == NULL ) { return; }
	if ( pCompressor->Stream != NULL ) { ZSTD_freeCStream(pCompressor->Stream); }
	xrtBufferDestroy(pCompressor->Output);
	xrtFree(pCompressor);
}



// ------------------------------------ 解压器 ------------------------------------

XRT_API xrtzstddecompressor* xrtZstdDecompressorCreate(const xrtzstdoptions* pOptions)
{
	xrtzstddecompressor* pDecompressor;
	size_t tResult;
	pDecompressor = (xrtzstddecompressor*)xrtMalloc(sizeof(xrtzstddecompressor));
	if ( pDecompressor == NULL ) { return NULL; }
	memset(pDecompressor, 0, sizeof(*pDecompressor));
	xrtZstdErrorInit(&pDecompressor->Error);
	if ( pOptions != NULL ) {
		pDecompressor->Options = *pOptions;
	} else {
		xrtZstdOptionsInit(&pDecompressor->Options);
	}
	pDecompressor->Stream = ZSTD_createDStream();
	pDecompressor->Output = __xrtZstdBufferCreate(XRT_ZSTD_CHUNK);
	if ( pDecompressor->Stream == NULL || pDecompressor->Output == NULL ) {
		__xrtZstdSetError(&pDecompressor->Error, XRT_ZSTD_ERROR_MEMORY, 0, 0, 0,
			"cannot allocate zstd decompressor");
		pDecompressor->Failed = true;
		return pDecompressor;
	}
	tResult = ZSTD_initDStream(pDecompressor->Stream);
	if ( ZSTD_isError(tResult) ) {
		__xrtZstdSetError(&pDecompressor->Error, XRT_ZSTD_ERROR_STATE, (int)tResult, 0, 0,
			"%s", ZSTD_getErrorName(tResult));
		pDecompressor->Failed = true;
		return pDecompressor;
	}
	return pDecompressor;
}

static bool __xrtZstdDecompressRun(xrtzstddecompressor* pDecompressor, const uint8* pData,
	size_t iSize)
{
	ZSTD_inBuffer tInput;
	uint8 sChunk[XRT_ZSTD_CHUNK];
	tInput.src = pData;
	tInput.size = iSize;
	tInput.pos = 0u;
	while ( tInput.pos < tInput.size ) {
		ZSTD_outBuffer tOutput;
		size_t tResult;
		size_t iPreviousInput = tInput.pos;
		uint64 iProjectedOutput;
		uint64 iProjectedMember;
		tOutput.dst = sChunk;
		tOutput.size = sizeof(sChunk);
		tOutput.pos = 0u;
		tResult = ZSTD_decompressStream(pDecompressor->Stream, &tOutput, &tInput);
		if ( ZSTD_isError(tResult) ) {
			__xrtZstdSetError(&pDecompressor->Error, XRT_ZSTD_ERROR_DATA, (int)tResult,
				pDecompressor->InputBytes, pDecompressor->OutputBytes, "%s", ZSTD_getErrorName(tResult));
			pDecompressor->Failed = true;
			return false;
		}
		if ( tOutput.pos != 0u ) {
			if ( __xrtZstdAddOverLimit(pDecompressor->OutputBytes, tOutput.pos,
					pDecompressor->Options.tLimits.iMaxOutputBytes) ||
				 __xrtZstdAddOverLimit(pDecompressor->MemberOutputBytes, tOutput.pos,
					pDecompressor->Options.tLimits.iMaxItemBytes) ) {
				__xrtZstdSetError(&pDecompressor->Error, XRT_ZSTD_ERROR_LIMIT, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"zstd output exceeds decompression limit");
				pDecompressor->Failed = true;
				return false;
			}
			iProjectedOutput = pDecompressor->OutputBytes + (uint64)tOutput.pos;
			iProjectedMember = pDecompressor->MemberOutputBytes + (uint64)tOutput.pos;
			if ( __xrtZstdOverRatio(&pDecompressor->Options.tLimits,
				pDecompressor->InputBytes, iProjectedOutput) ) {
				__xrtZstdSetError(&pDecompressor->Error, XRT_ZSTD_ERROR_LIMIT, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"zstd output exceeds decompression limit");
				pDecompressor->Failed = true;
				return false;
			}
			if ( !xrtBufferAppend(pDecompressor->Output,
				(xbytesview){ (const unsigned char*)sChunk, tOutput.pos }) ) {
				__xrtZstdSetError(&pDecompressor->Error, XRT_ZSTD_ERROR_MEMORY, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot grow zstd output buffer");
				pDecompressor->Failed = true;
				return false;
			}
			pDecompressor->OutputBytes = iProjectedOutput;
			pDecompressor->MemberOutputBytes = iProjectedMember;
		}
		if ( tResult == 0u ) {
			pDecompressor->FrameEnded = true;
			pDecompressor->MemberCount += 1u;
			pDecompressor->MemberOutputBytes = 0u;
			if ( tInput.pos < tInput.size ) {
				size_t tReset = ZSTD_initDStream(pDecompressor->Stream);
				if ( ZSTD_isError(tReset) ) {
					__xrtZstdSetError(&pDecompressor->Error, XRT_ZSTD_ERROR_STATE, (int)tReset,
						pDecompressor->InputBytes, pDecompressor->OutputBytes, "%s", ZSTD_getErrorName(tReset));
					pDecompressor->Failed = true;
					return false;
				}
				pDecompressor->FrameEnded = false;
			}
		} else if ( (tInput.pos == iPreviousInput) && (tOutput.pos == 0u) ) {
			__xrtZstdSetError(&pDecompressor->Error, XRT_ZSTD_ERROR_DATA, 0,
				pDecompressor->InputBytes, pDecompressor->OutputBytes,
				"zstd decompressor made no progress");
			pDecompressor->Failed = true;
			return false;
		}
	}
	return true;
}

XRT_API bool xrtZstdDecompressorUpdate(xrtzstddecompressor* pDecompressor, const void* pData, size_t iSize)
{
	size_t tReset;

	if ( pDecompressor == NULL || pDecompressor->Failed ) { return false; }
	if ( iSize != 0u && pData == NULL ) {
		__xrtZstdSetError(&pDecompressor->Error, XRT_ZSTD_ERROR_ARGUMENT, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "invalid zstd input");
		pDecompressor->Failed = true;
		return false;
	}
	if ( __xrtZstdAddOverLimit(pDecompressor->InputBytes, iSize,
		pDecompressor->Options.tLimits.iMaxInputBytes) ) {
		__xrtZstdSetError(&pDecompressor->Error, XRT_ZSTD_ERROR_LIMIT, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "zstd input exceeds maxInput limit");
		pDecompressor->Failed = true;
		return false;
	}
	if ( iSize == 0u ) { return true; }
	if ( pDecompressor->FrameEnded ) {
		tReset = ZSTD_initDStream(pDecompressor->Stream);
		if ( ZSTD_isError(tReset) ) {
			__xrtZstdSetError(&pDecompressor->Error, XRT_ZSTD_ERROR_STATE, (int)tReset,
				pDecompressor->InputBytes, pDecompressor->OutputBytes,
				"%s", ZSTD_getErrorName(tReset));
			pDecompressor->Failed = true;
			return false;
		}
	}
	pDecompressor->FrameEnded = false;
	pDecompressor->InputBytes += iSize;
	return __xrtZstdDecompressRun(pDecompressor, (const uint8*)pData, iSize);
}

XRT_API bool xrtZstdDecompressorFinish(xrtzstddecompressor* pDecompressor)
{
	if ( pDecompressor == NULL || pDecompressor->Failed ) { return false; }
	if ( pDecompressor->Finished ) { return true; }
	if ( pDecompressor->MemberCount == 0u || !pDecompressor->FrameEnded ) {
		__xrtZstdSetError(&pDecompressor->Error, XRT_ZSTD_ERROR_TRUNCATED, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "zstd stream is truncated or empty");
		pDecompressor->Failed = true;
		return false;
	}
	pDecompressor->Finished = true;
	return true;
}

XRT_API bool xrtZstdDecompressorDrain(xrtzstddecompressor* pDecompressor, xwriter* pWriter)
{
	xbytesview Output;
	if ( pDecompressor == NULL || pDecompressor->Failed || pWriter == NULL ) { return false; }
	Output = xrtBufferView(pDecompressor->Output);
	if ( Output.Size != 0u &&
		!xrtWriterWriteFull(pWriter, Output.Data, Output.Size, NULL) ) {
		__xrtZstdSetError(&pDecompressor->Error, XRT_ZSTD_ERROR_IO, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot write zstd output stream");
		pDecompressor->Failed = true;
		return false;
	}
	xrtBufferClear(pDecompressor->Output);
	return true;
}

XRT_API xbuffer* xrtZstdDecompressorTakeBuffer(xrtzstddecompressor* pDecompressor)
{
	xbuffer* pOutput;
	if ( pDecompressor == NULL || pDecompressor->Failed || !pDecompressor->Finished ) { return NULL; }
	pOutput = pDecompressor->Output;
	pDecompressor->Output = NULL;
	return pOutput;
}

XRT_API xbuffer* xrtZstdDecompressorDrainBuffer(xrtzstddecompressor* pDecompressor)
{
	xbuffer* pOutput;
	if ( pDecompressor == NULL || pDecompressor->Failed || pDecompressor->Output == NULL ) { return NULL; }
	pOutput = xrtBufferFrom(xrtBufferView(pDecompressor->Output));
	if ( pOutput != NULL ) { xrtBufferClear(pDecompressor->Output); }
	return pOutput;
}

XRT_API uint64 xrtZstdDecompressorInputBytes(const xrtzstddecompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->InputBytes : 0u;
}

XRT_API uint64 xrtZstdDecompressorOutputBytes(const xrtzstddecompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->OutputBytes : 0u;
}

XRT_API bool xrtZstdDecompressorFinished(const xrtzstddecompressor* pDecompressor)
{
	return pDecompressor != NULL && pDecompressor->FrameEnded;
}

XRT_API uint64 xrtZstdDecompressorMembers(const xrtzstddecompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->MemberCount : 0u;
}

XRT_API const xrtzstderror* xrtZstdDecompressorError(const xrtzstddecompressor* pDecompressor)
{
	return pDecompressor != NULL ? &pDecompressor->Error : NULL;
}

XRT_API void xrtZstdDecompressorDestroy(xrtzstddecompressor* pDecompressor)
{
	if ( pDecompressor == NULL ) { return; }
	if ( pDecompressor->Stream != NULL ) { ZSTD_freeDStream(pDecompressor->Stream); }
	xrtBufferDestroy(pDecompressor->Output);
	xrtFree(pDecompressor);
}



// ------------------------------------ 一次性接口 ------------------------------------

XRT_API bool xrtZstdCompress(const void* pData, size_t iSize, const xrtzstdoptions* pOptions,
	xbuffer** ppOutput, xrtzstderror* pError)
{
	xrtzstdoptions tOptions;
	ZSTD_CCtx* pCtx;
	size_t tBound;
	size_t tCapacity;
	size_t tWritten;
	ptr pBuffer;

	xrtZstdErrorInit(pError);
	if ( ppOutput != NULL ) { *ppOutput = NULL; }
	if ( ppOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtZstdSetError(pError, XRT_ZSTD_ERROR_ARGUMENT, 0, 0, 0, "invalid zstd input");
		return false;
	}
	if ( pOptions != NULL ) { tOptions = *pOptions; } else { xrtZstdOptionsInit(&tOptions); }
	if ( tOptions.tLimits.iMaxInputBytes != 0u && iSize > tOptions.tLimits.iMaxInputBytes ) {
		__xrtZstdSetError(pError, XRT_ZSTD_ERROR_LIMIT, 0, iSize, 0, "zstd input exceeds maxInput limit");
		return false;
	}
	tBound = ZSTD_compressBound(iSize);
	if ( ZSTD_isError(tBound) || tBound == 0u ) {
		__xrtZstdSetError(pError, XRT_ZSTD_ERROR_ARGUMENT, 0, iSize, 0, "zstd input too large");
		return false;
	}
	tCapacity = tBound;
	if ( (tOptions.tLimits.iMaxOutputBytes != 0u) &&
		 (tOptions.tLimits.iMaxOutputBytes < (uint64)tCapacity) ) {
		tCapacity = (size_t)tOptions.tLimits.iMaxOutputBytes;
	}
	pBuffer = xrtMalloc(tCapacity);
	if ( pBuffer == NULL ) {
		__xrtZstdSetError(pError, XRT_ZSTD_ERROR_MEMORY, 0, iSize, 0, "out of memory");
		return false;
	}
	pCtx = ZSTD_createCCtx();
	if ( pCtx == NULL ) {
		__xrtZstdSetError(pError, XRT_ZSTD_ERROR_MEMORY, 0, iSize, 0, "out of memory");
		xrtFree(pBuffer);
		return false;
	}
	if ( !__xrtZstdConfigureCompressor(pCtx, tOptions.iLevel, pError) ) {
		ZSTD_freeCCtx(pCtx);
		xrtFree(pBuffer);
		return false;
	}
	tWritten = ZSTD_compress2(pCtx, pBuffer, tCapacity,
		pData == NULL ? "" : pData, iSize);
	ZSTD_freeCCtx(pCtx);
	if ( ZSTD_isError(tWritten) ) {
		if ( tCapacity < tBound ) {
			__xrtZstdSetError(pError, XRT_ZSTD_ERROR_LIMIT, (int)tWritten, iSize, 0,
				"zstd output exceeds maxOutput limit");
		} else {
			__xrtZstdSetError(pError, XRT_ZSTD_ERROR_DATA, (int)tWritten, iSize, 0,
				"%s", ZSTD_getErrorName(tWritten));
		}
		xrtFree(pBuffer);
		return false;
	}
	*ppOutput = xrtBufferFrom(
		(xbytesview){ (const unsigned char*)pBuffer, tWritten }
	);
	xrtFree(pBuffer);
	if ( *ppOutput == NULL ) {
		__xrtZstdSetError(pError, XRT_ZSTD_ERROR_MEMORY, 0, iSize, tWritten, "out of memory");
		return false;
	}
	return true;
}

XRT_API bool xrtZstdDecompress(const void* pData, size_t iSize, const xrtzstdoptions* pOptions,
	xbuffer** ppOutput, xrtzstderror* pError)
{
	xrtzstdoptions tOptions;
	xrtzstddecompressor* pDecompressor;
	unsigned long long tContentSize;
	bool bRet;

	xrtZstdErrorInit(pError);
	if ( ppOutput != NULL ) { *ppOutput = NULL; }
	if ( ppOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtZstdSetError(pError, XRT_ZSTD_ERROR_ARGUMENT, 0, 0, 0, "invalid zstd input");
		return false;
	}
	if ( pOptions != NULL ) { tOptions = *pOptions; } else { xrtZstdOptionsInit(&tOptions); }
	if ( tOptions.tLimits.iMaxInputBytes != 0u && iSize > tOptions.tLimits.iMaxInputBytes ) {
		__xrtZstdSetError(pError, XRT_ZSTD_ERROR_LIMIT, 0, iSize, 0, "zstd input exceeds maxInput limit");
		return false;
	}
	tContentSize = ZSTD_getFrameContentSize(pData == NULL ? "" : pData, iSize);
	if ( tContentSize == ZSTD_CONTENTSIZE_ERROR ) {
		__xrtZstdSetError(pError, XRT_ZSTD_ERROR_DATA, 0, iSize, 0, "not a zstd frame");
		return false;
	}
	if ( tContentSize != ZSTD_CONTENTSIZE_UNKNOWN ) {
		if ( ((tOptions.tLimits.iMaxOutputBytes != 0u) &&
			  ((uint64)tContentSize > tOptions.tLimits.iMaxOutputBytes)) ||
			 ((tOptions.tLimits.iMaxItemBytes != 0u) &&
			  ((uint64)tContentSize > tOptions.tLimits.iMaxItemBytes)) ||
			 __xrtZstdOverRatio(&tOptions.tLimits,
				(uint64)iSize, (uint64)tContentSize) ) {
			__xrtZstdSetError(pError, XRT_ZSTD_ERROR_LIMIT, 0, iSize, (uint64)tContentSize,
				"zstd output exceeds decompression limit");
			return false;
		}
		if ( tContentSize > (unsigned long long)SIZE_MAX ) {
			__xrtZstdSetError(pError, XRT_ZSTD_ERROR_DATA, 0, iSize, 0,
				"zstd frame content size overflows address space");
			return false;
		}
	}
	pDecompressor = xrtZstdDecompressorCreate(&tOptions);
	if ( pDecompressor == NULL ) {
		__xrtZstdSetError(pError, XRT_ZSTD_ERROR_MEMORY, 0, iSize, 0,
			"cannot allocate zstd decompressor");
		return false;
	}
	if ( (tContentSize != ZSTD_CONTENTSIZE_UNKNOWN) &&
		 (tContentSize != 0u) &&
		 !xrtBufferReserve(pDecompressor->Output, (size_t)tContentSize) ) {
		__xrtZstdSetError(pError, XRT_ZSTD_ERROR_MEMORY, 0, iSize, 0, "out of memory");
		xrtZstdDecompressorDestroy(pDecompressor);
		return false;
	}
	bRet = xrtZstdDecompressorUpdate(pDecompressor, pData, iSize) &&
		xrtZstdDecompressorFinish(pDecompressor);
	if ( bRet ) {
		*ppOutput = xrtZstdDecompressorTakeBuffer(pDecompressor);
		bRet = *ppOutput != NULL;
	}
	if ( !bRet ) { __xrtZstdCopyError(pError, &pDecompressor->Error); }
	xrtZstdDecompressorDestroy(pDecompressor);
	return bRet;
}



// ------------------------------------ 文件流接口 ------------------------------------

XRT_API bool xrtZstdCompressStream(xreader* pReader, xwriter* pWriter, const xrtzstdoptions* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtzstderror* pError)
{
	uint8 sInput[XRT_ZSTD_CHUNK];
	xrtzstdcompressor* pCompressor;
	uint64 iTotal = 0u;
	bool bTotalKnown;
	bool bRet = true;
	xrtZstdErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		__xrtZstdSetError(pError, XRT_ZSTD_ERROR_ARGUMENT, 0, 0, 0, "invalid zstd stream");
		return false;
	}
	bTotalKnown = xrtReaderCanSeek(pReader) && xrtReaderSize(pReader, &iTotal);
	if ( !bTotalKnown ) { xrtClearError(); }
	pCompressor = xrtZstdCompressorCreate(pOptions);
	if ( pCompressor == NULL ) { return false; }
	while ( bRet ) {
		size_t iRead = 0u;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtZstdSetError(&pCompressor->Error, XRT_ZSTD_ERROR_IO, 0,
				pCompressor->InputBytes, pCompressor->OutputBytes, "cannot read zstd input stream");
			bRet = false;
			break;
		}
		if ( iRead == 0u ) { break; }
		bRet = xrtZstdCompressorUpdate(pCompressor, sInput, (size_t)iRead) &&
			xrtZstdCompressorDrain(pCompressor, pWriter);
		if ( bRet && !xrtProgressReport(pProgress, pUserData, pCompressor->InputBytes,
			iTotal, pCompressor->OutputBytes,
			bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u) ) {
			__xrtZstdSetError(&pCompressor->Error, XRT_ZSTD_ERROR_CANCELLED, 0,
				pCompressor->InputBytes, pCompressor->OutputBytes, "zstd operation cancelled");
			bRet = false;
		}
	}
	if ( bRet ) { bRet = xrtZstdCompressorFinish(pCompressor) && xrtZstdCompressorDrain(pCompressor, pWriter); }
	if ( bRet && !xrtProgressReport(pProgress, pUserData, pCompressor->InputBytes,
		iTotal, pCompressor->OutputBytes, XRT_PROGRESS_FINAL |
		(bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u)) ) {
		__xrtZstdSetError(&pCompressor->Error, XRT_ZSTD_ERROR_CANCELLED, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "zstd operation cancelled");
		bRet = false;
	}
	if ( !bRet ) { __xrtZstdCopyError(pError, &pCompressor->Error); }
	xrtZstdCompressorDestroy(pCompressor);
	return bRet;
}

XRT_API bool xrtZstdDecompressStream(xreader* pReader, xwriter* pWriter, const xrtzstdoptions* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtzstderror* pError)
{
	uint8 sInput[XRT_ZSTD_CHUNK];
	xrtzstddecompressor* pDecompressor;
	uint64 iTotal = 0u;
	bool bTotalKnown;
	bool bRet = true;
	xrtZstdErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		__xrtZstdSetError(pError, XRT_ZSTD_ERROR_ARGUMENT, 0, 0, 0, "invalid zstd stream");
		return false;
	}
	bTotalKnown = xrtReaderCanSeek(pReader) && xrtReaderSize(pReader, &iTotal);
	if ( !bTotalKnown ) { xrtClearError(); }
	pDecompressor = xrtZstdDecompressorCreate(pOptions);
	if ( pDecompressor == NULL ) { return false; }
	while ( bRet ) {
		size_t iRead = 0u;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtZstdSetError(&pDecompressor->Error, XRT_ZSTD_ERROR_IO, 0,
				pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot read zstd input stream");
			bRet = false;
			break;
		}
		if ( iRead == 0u ) { break; }
		bRet = xrtZstdDecompressorUpdate(pDecompressor, sInput, (size_t)iRead) &&
			xrtZstdDecompressorDrain(pDecompressor, pWriter);
		if ( bRet && !xrtProgressReport(pProgress, pUserData, pDecompressor->InputBytes,
			iTotal, pDecompressor->OutputBytes,
			bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u) ) {
			__xrtZstdSetError(&pDecompressor->Error, XRT_ZSTD_ERROR_CANCELLED, 0,
				pDecompressor->InputBytes, pDecompressor->OutputBytes, "zstd operation cancelled");
			bRet = false;
		}
	}
	if ( bRet ) { bRet = xrtZstdDecompressorFinish(pDecompressor) && xrtZstdDecompressorDrain(pDecompressor, pWriter); }
	if ( bRet && !xrtProgressReport(pProgress, pUserData, pDecompressor->InputBytes,
		iTotal, pDecompressor->OutputBytes, XRT_PROGRESS_FINAL |
		(bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u)) ) {
		__xrtZstdSetError(&pDecompressor->Error, XRT_ZSTD_ERROR_CANCELLED, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "zstd operation cancelled");
		bRet = false;
	}
	if ( !bRet ) { __xrtZstdCopyError(pError, &pDecompressor->Error); }
	xrtZstdDecompressorDestroy(pDecompressor);
	return bRet;
}
