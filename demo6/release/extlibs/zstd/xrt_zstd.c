#include "include/xrt_zstd.h"
#include "third_party/zstd.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>



#define XRT_ZSTD_CHUNK 65536u

struct xrtzstdcompressor {
	ZSTD_CStream* Stream;
	xbuffer Output;
	xrtzstdoptions Options;
	xrtzstderror Error;
	uint64 InputBytes;
	uint64 OutputBytes;
	bool Initialized;
	bool Finished;
	bool Failed;
};

struct xrtzstddecompressor {
	ZSTD_DStream* Stream;
	xbuffer Output;
	xrtzstdoptions Options;
	xrtzstderror Error;
	uint64 InputBytes;
	uint64 OutputBytes;
	uint64 MemberCount;
	bool Initialized;
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

static bool __xrtZstdOverOutputLimit(const xrtresourcelimits* pLimits, uint64 iOutput)
{
	return pLimits->iMaxOutputBytes != 0u && iOutput > pLimits->iMaxOutputBytes;
}

static bool __xrtZstdOverRatio(const xrtresourcelimits* pLimits, uint64 iInput, uint64 iOutput)
{
	uint64 iRatio = pLimits->iMaxCompressionRatio;
	if ( iRatio == 0u || iInput == 0u ) { return FALSE; }
	return iOutput > iInput * iRatio;
}



XXAPI void xrtZstdErrorInit(xrtzstderror* pError)
{
	if ( pError == NULL ) { return; }
	memset(pError, 0, sizeof(*pError));
	pError->iCode = XRT_ZSTD_OK;
}

XXAPI void xrtZstdOptionsInit(xrtzstdoptions* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_ZSTD_OPTIONS_VERSION;
	pOptions->iLevel = ZSTD_defaultCLevel();
	xrtResourceLimitsInit(&pOptions->tLimits);
}



// ------------------------------------ 压缩器 ------------------------------------

XXAPI xrtzstdcompressor* xrtZstdCompressorCreate(const xrtzstdoptions* pOptions)
{
	xrtzstdcompressor* pCompressor;
	size_t tResult;
	int iLevel;
	pCompressor = (xrtzstdcompressor*)xrtMalloc(sizeof(xrtzstdcompressor));
	if ( pCompressor == NULL ) { return NULL; }
	memset(pCompressor, 0, sizeof(*pCompressor));
	xrtZstdErrorInit(&pCompressor->Error);
	if ( pOptions != NULL ) {
		pCompressor->Options = *pOptions;
	} else {
		xrtZstdOptionsInit(&pCompressor->Options);
	}
	iLevel = pCompressor->Options.iLevel;
	if ( iLevel == 0 ) { iLevel = ZSTD_defaultCLevel(); }
	pCompressor->Stream = ZSTD_createCStream();
	pCompressor->Output = xrtBufferCreate(XRT_ZSTD_CHUNK);
	if ( pCompressor->Stream == NULL || pCompressor->Output == NULL ) {
		__xrtZstdSetError(&pCompressor->Error, XRT_ZSTD_ERROR_MEMORY, 0, 0, 0,
			"cannot allocate zstd compressor");
		pCompressor->Failed = TRUE;
		return pCompressor;
	}
	/* 使用现代参数 API 设定级别；zstd 1.6 默认不写内容校验和，显式开启以便解压侧检出损坏。 */
	tResult = ZSTD_CCtx_setParameter(pCompressor->Stream, ZSTD_c_compressionLevel, iLevel);
	if ( !ZSTD_isError(tResult) ) {
		tResult = ZSTD_CCtx_setParameter(pCompressor->Stream, ZSTD_c_checksumFlag, 1);
	}
	/* 本内嵌 zstd 合并版未包含 zstd_ldm.c（LDM 为 stub，会丢弃全部字面量）。
	 * 高级别 + 大窗口的流式压缩会被自动启用 LDM，从而产出损坏帧，故显式禁用。
	 * ZSTD_ps_auto=0 / ZSTD_ps_enable=1 / ZSTD_ps_disable=2；枚举在实验区未导出，这里直接传 2。 */
	if ( !ZSTD_isError(tResult) ) {
		tResult = ZSTD_CCtx_setParameter(pCompressor->Stream, ZSTD_c_enableLongDistanceMatching, 2 /* ZSTD_ps_disable */);
	}
	if ( ZSTD_isError(tResult) ) {
		__xrtZstdSetError(&pCompressor->Error, XRT_ZSTD_ERROR_STATE, (int)tResult, 0, 0,
			"%s", ZSTD_getErrorName(tResult));
		pCompressor->Failed = TRUE;
		return pCompressor;
	}
	pCompressor->Initialized = TRUE;
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
			pCompressor->Failed = TRUE;
			return FALSE;
		}
		if ( tOutput.pos != 0u ) {
			if ( !xrtBufferAppend(pCompressor->Output, (ptr)sChunk, (uint32)tOutput.pos, XBUF_BINARY) ) {
				__xrtZstdSetError(&pCompressor->Error, XRT_ZSTD_ERROR_MEMORY, 0,
					pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow zstd output buffer");
				pCompressor->Failed = TRUE;
				return FALSE;
			}
			pCompressor->OutputBytes += tOutput.pos;
			if ( __xrtZstdOverOutputLimit(&pCompressor->Options.tLimits, pCompressor->OutputBytes) ) {
				__xrtZstdSetError(&pCompressor->Error, XRT_ZSTD_ERROR_LIMIT, 0,
					pCompressor->InputBytes, pCompressor->OutputBytes, "zstd output exceeds maxOutput limit");
				pCompressor->Failed = TRUE;
				return FALSE;
			}
		}
		bFinished = bEnd ? (tRemaining == 0u) : (tInput.pos >= tInput.size);
	} while ( !bFinished );
	return TRUE;
}

XXAPI bool xrtZstdCompressorUpdate(xrtzstdcompressor* pCompressor, const void* pData, size_t iSize)
{
	if ( pCompressor == NULL || pCompressor->Failed ) { return FALSE; }
	if ( pCompressor->Finished ) {
		__xrtZstdSetError(&pCompressor->Error, XRT_ZSTD_ERROR_STATE, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "zstd compressor already finished");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	if ( iSize != 0u && pData == NULL ) {
		__xrtZstdSetError(&pCompressor->Error, XRT_ZSTD_ERROR_ARGUMENT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "invalid zstd input");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	if ( pCompressor->Options.tLimits.iMaxInputBytes != 0u &&
		pCompressor->InputBytes + iSize > pCompressor->Options.tLimits.iMaxInputBytes ) {
		__xrtZstdSetError(&pCompressor->Error, XRT_ZSTD_ERROR_LIMIT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "zstd input exceeds maxInput limit");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	pCompressor->InputBytes += iSize;
	if ( iSize == 0u ) { return TRUE; }
	return __xrtZstdCompressRun(pCompressor, (const uint8*)pData, iSize, FALSE);
}

XXAPI bool xrtZstdCompressorFinish(xrtzstdcompressor* pCompressor)
{
	static const uint8 iEmpty = 0u;
	if ( pCompressor == NULL || pCompressor->Failed ) { return FALSE; }
	if ( pCompressor->Finished ) { return TRUE; }
	if ( !__xrtZstdCompressRun(pCompressor, &iEmpty, 0u, TRUE) ) { return FALSE; }
	pCompressor->Finished = TRUE;
	return TRUE;
}

XXAPI bool xrtZstdCompressorDrain(xrtzstdcompressor* pCompressor, xwriter pWriter)
{
	if ( pCompressor == NULL || pCompressor->Failed || pWriter == NULL ) { return FALSE; }
	if ( pCompressor->Output->Length != 0u &&
		!xrtWriterWriteAll(pWriter, pCompressor->Output->Buffer, pCompressor->Output->Length) ) {
		__xrtZstdSetError(&pCompressor->Error, XRT_ZSTD_ERROR_IO, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot write zstd output stream");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	pCompressor->Output->Length = 0u;
	return TRUE;
}

XXAPI xbuffer xrtZstdCompressorTakeBuffer(xrtzstdcompressor* pCompressor)
{
	xbuffer pOutput;
	if ( pCompressor == NULL || pCompressor->Failed || !pCompressor->Finished ) { return NULL; }
	pOutput = pCompressor->Output;
	pCompressor->Output = NULL;
	return pOutput;
}

XXAPI xbuffer xrtZstdCompressorDrainBuffer(xrtzstdcompressor* pCompressor)
{
	xbuffer pOutput;
	if ( pCompressor == NULL || pCompressor->Failed || pCompressor->Output == NULL ) { return NULL; }
	pOutput = xrtBufferFrom((ptr)pCompressor->Output->Buffer, (size_t)pCompressor->Output->Length);
	pCompressor->Output->Length = 0u;
	return pOutput;
}

XXAPI uint64 xrtZstdCompressorInputBytes(const xrtzstdcompressor* pCompressor)
{
	return pCompressor != NULL ? pCompressor->InputBytes : 0u;
}

XXAPI uint64 xrtZstdCompressorOutputBytes(const xrtzstdcompressor* pCompressor)
{
	return pCompressor != NULL ? pCompressor->OutputBytes : 0u;
}

XXAPI const xrtzstderror* xrtZstdCompressorError(const xrtzstdcompressor* pCompressor)
{
	return pCompressor != NULL ? &pCompressor->Error : NULL;
}

XXAPI void xrtZstdCompressorDestroy(xrtzstdcompressor* pCompressor)
{
	if ( pCompressor == NULL ) { return; }
	if ( pCompressor->Stream != NULL ) { ZSTD_freeCStream(pCompressor->Stream); }
	xrtBufferDestroy(pCompressor->Output);
	xrtFree(pCompressor);
}



// ------------------------------------ 解压器 ------------------------------------

XXAPI xrtzstddecompressor* xrtZstdDecompressorCreate(const xrtzstdoptions* pOptions)
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
	pDecompressor->Output = xrtBufferCreate(XRT_ZSTD_CHUNK);
	if ( pDecompressor->Stream == NULL || pDecompressor->Output == NULL ) {
		__xrtZstdSetError(&pDecompressor->Error, XRT_ZSTD_ERROR_MEMORY, 0, 0, 0,
			"cannot allocate zstd decompressor");
		pDecompressor->Failed = TRUE;
		return pDecompressor;
	}
	tResult = ZSTD_initDStream(pDecompressor->Stream);
	if ( ZSTD_isError(tResult) ) {
		__xrtZstdSetError(&pDecompressor->Error, XRT_ZSTD_ERROR_STATE, (int)tResult, 0, 0,
			"%s", ZSTD_getErrorName(tResult));
		pDecompressor->Failed = TRUE;
		return pDecompressor;
	}
	pDecompressor->Initialized = TRUE;
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
		tOutput.dst = sChunk;
		tOutput.size = sizeof(sChunk);
		tOutput.pos = 0u;
		tResult = ZSTD_decompressStream(pDecompressor->Stream, &tOutput, &tInput);
		if ( ZSTD_isError(tResult) ) {
			__xrtZstdSetError(&pDecompressor->Error, XRT_ZSTD_ERROR_DATA, (int)tResult,
				pDecompressor->InputBytes, pDecompressor->OutputBytes, "%s", ZSTD_getErrorName(tResult));
			pDecompressor->Failed = TRUE;
			return FALSE;
		}
		if ( tOutput.pos != 0u ) {
			if ( !xrtBufferAppend(pDecompressor->Output, (ptr)sChunk, (uint32)tOutput.pos, XBUF_BINARY) ) {
				__xrtZstdSetError(&pDecompressor->Error, XRT_ZSTD_ERROR_MEMORY, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot grow zstd output buffer");
				pDecompressor->Failed = TRUE;
				return FALSE;
			}
			pDecompressor->OutputBytes += tOutput.pos;
			if ( __xrtZstdOverOutputLimit(&pDecompressor->Options.tLimits, pDecompressor->OutputBytes) ||
				__xrtZstdOverRatio(&pDecompressor->Options.tLimits, pDecompressor->InputBytes,
					pDecompressor->OutputBytes) ) {
				__xrtZstdSetError(&pDecompressor->Error, XRT_ZSTD_ERROR_LIMIT, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"zstd output exceeds decompression limit");
				pDecompressor->Failed = TRUE;
				return FALSE;
			}
		}
		if ( tResult == 0u ) {
			pDecompressor->FrameEnded = TRUE;
			pDecompressor->MemberCount += 1u;
			if ( tInput.pos < tInput.size ) {
				size_t tReset = ZSTD_initDStream(pDecompressor->Stream);
				if ( ZSTD_isError(tReset) ) {
					__xrtZstdSetError(&pDecompressor->Error, XRT_ZSTD_ERROR_STATE, (int)tReset,
						pDecompressor->InputBytes, pDecompressor->OutputBytes, "%s", ZSTD_getErrorName(tReset));
					pDecompressor->Failed = TRUE;
					return FALSE;
				}
			}
		} else if ( tOutput.pos == 0u ) {
			break;
		}
	}
	return TRUE;
}

XXAPI bool xrtZstdDecompressorUpdate(xrtzstddecompressor* pDecompressor, const void* pData, size_t iSize)
{
	if ( pDecompressor == NULL || pDecompressor->Failed ) { return FALSE; }
	if ( iSize != 0u && pData == NULL ) {
		__xrtZstdSetError(&pDecompressor->Error, XRT_ZSTD_ERROR_ARGUMENT, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "invalid zstd input");
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	if ( pDecompressor->Options.tLimits.iMaxInputBytes != 0u &&
		pDecompressor->InputBytes + iSize > pDecompressor->Options.tLimits.iMaxInputBytes ) {
		__xrtZstdSetError(&pDecompressor->Error, XRT_ZSTD_ERROR_LIMIT, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "zstd input exceeds maxInput limit");
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	pDecompressor->FrameEnded = FALSE;
	pDecompressor->InputBytes += iSize;
	if ( iSize == 0u ) { return TRUE; }
	return __xrtZstdDecompressRun(pDecompressor, (const uint8*)pData, iSize);
}

XXAPI bool xrtZstdDecompressorFinish(xrtzstddecompressor* pDecompressor)
{
	if ( pDecompressor == NULL || pDecompressor->Failed ) { return FALSE; }
	if ( pDecompressor->Finished ) { return TRUE; }
	if ( pDecompressor->MemberCount == 0u || !pDecompressor->FrameEnded ) {
		__xrtZstdSetError(&pDecompressor->Error, XRT_ZSTD_ERROR_TRUNCATED, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "zstd stream is truncated or empty");
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	pDecompressor->Finished = TRUE;
	return TRUE;
}

XXAPI bool xrtZstdDecompressorDrain(xrtzstddecompressor* pDecompressor, xwriter pWriter)
{
	if ( pDecompressor == NULL || pDecompressor->Failed || pWriter == NULL ) { return FALSE; }
	if ( pDecompressor->Output->Length != 0u &&
		!xrtWriterWriteAll(pWriter, pDecompressor->Output->Buffer, pDecompressor->Output->Length) ) {
		__xrtZstdSetError(&pDecompressor->Error, XRT_ZSTD_ERROR_IO, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot write zstd output stream");
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	pDecompressor->Output->Length = 0u;
	return TRUE;
}

XXAPI xbuffer xrtZstdDecompressorTakeBuffer(xrtzstddecompressor* pDecompressor)
{
	xbuffer pOutput;
	if ( pDecompressor == NULL || pDecompressor->Failed || !pDecompressor->Finished ) { return NULL; }
	pOutput = pDecompressor->Output;
	pDecompressor->Output = NULL;
	return pOutput;
}

XXAPI xbuffer xrtZstdDecompressorDrainBuffer(xrtzstddecompressor* pDecompressor)
{
	xbuffer pOutput;
	if ( pDecompressor == NULL || pDecompressor->Failed || pDecompressor->Output == NULL ) { return NULL; }
	pOutput = xrtBufferFrom((ptr)pDecompressor->Output->Buffer, (size_t)pDecompressor->Output->Length);
	pDecompressor->Output->Length = 0u;
	return pOutput;
}

XXAPI uint64 xrtZstdDecompressorInputBytes(const xrtzstddecompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->InputBytes : 0u;
}

XXAPI uint64 xrtZstdDecompressorOutputBytes(const xrtzstddecompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->OutputBytes : 0u;
}

XXAPI bool xrtZstdDecompressorFinished(const xrtzstddecompressor* pDecompressor)
{
	return pDecompressor != NULL && pDecompressor->FrameEnded;
}

XXAPI uint64 xrtZstdDecompressorMembers(const xrtzstddecompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->MemberCount : 0u;
}

XXAPI const xrtzstderror* xrtZstdDecompressorError(const xrtzstddecompressor* pDecompressor)
{
	return pDecompressor != NULL ? &pDecompressor->Error : NULL;
}

XXAPI void xrtZstdDecompressorDestroy(xrtzstddecompressor* pDecompressor)
{
	if ( pDecompressor == NULL ) { return; }
	if ( pDecompressor->Stream != NULL ) { ZSTD_freeDStream(pDecompressor->Stream); }
	xrtBufferDestroy(pDecompressor->Output);
	xrtFree(pDecompressor);
}



// ------------------------------------ 一次性接口 ------------------------------------

XXAPI bool xrtZstdCompress(const void* pData, size_t iSize, const xrtzstdoptions* pOptions,
	xbuffer* pOutput, xrtzstderror* pError)
{
	xrtzstdoptions tOptions;
	ZSTD_CCtx* pCtx;
	size_t tBound;
	size_t tWritten;
	ptr pBuffer;
	xrtZstdErrorInit(pError);
	if ( pOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtZstdSetError(pError, XRT_ZSTD_ERROR_ARGUMENT, 0, 0, 0, "invalid zstd input");
		return FALSE;
	}
	if ( pOptions != NULL ) { tOptions = *pOptions; } else { xrtZstdOptionsInit(&tOptions); }
	if ( tOptions.tLimits.iMaxInputBytes != 0u && iSize > tOptions.tLimits.iMaxInputBytes ) {
		__xrtZstdSetError(pError, XRT_ZSTD_ERROR_LIMIT, 0, iSize, 0, "zstd input exceeds maxInput limit");
		return FALSE;
	}
	tBound = ZSTD_compressBound(iSize);
	if ( ZSTD_isError(tBound) || tBound == 0u ) {
		__xrtZstdSetError(pError, XRT_ZSTD_ERROR_ARGUMENT, 0, iSize, 0, "zstd input too large");
		return FALSE;
	}
	pBuffer = xrtMalloc(tBound);
	if ( pBuffer == NULL ) {
		__xrtZstdSetError(pError, XRT_ZSTD_ERROR_MEMORY, 0, iSize, 0, "out of memory");
		return FALSE;
	}
	pCtx = ZSTD_createCCtx();
	if ( pCtx == NULL ) {
		__xrtZstdSetError(pError, XRT_ZSTD_ERROR_MEMORY, 0, iSize, 0, "out of memory");
		xrtFree(pBuffer);
		return FALSE;
	}
	ZSTD_CCtx_setParameter(pCtx, ZSTD_c_compressionLevel, tOptions.iLevel);
	/* zstd 1.6 默认不写内容校验和，显式开启以便解压侧检出损坏。 */
	ZSTD_CCtx_setParameter(pCtx, ZSTD_c_checksumFlag, 1);
	/* 本内嵌 zstd 合并版未包含 zstd_ldm.c（LDM 为 stub），显式禁用以免自动启用后损坏数据。
	 * 2 = ZSTD_ps_disable（0 为 auto，仍会被自动启用；枚举在实验区未导出）。 */
	ZSTD_CCtx_setParameter(pCtx, ZSTD_c_enableLongDistanceMatching, 2 /* ZSTD_ps_disable */);
	tWritten = ZSTD_compress2(pCtx, pBuffer, tBound, pData == NULL ? "" : pData, iSize);
	ZSTD_freeCCtx(pCtx);
	if ( ZSTD_isError(tWritten) ) {
		__xrtZstdSetError(pError, XRT_ZSTD_ERROR_DATA, (int)tWritten, iSize, 0,
			"%s", ZSTD_getErrorName(tWritten));
		xrtFree(pBuffer);
		return FALSE;
	}
	*pOutput = xrtBufferFrom(pBuffer, tWritten);
	xrtFree(pBuffer);
	if ( *pOutput == NULL ) {
		__xrtZstdSetError(pError, XRT_ZSTD_ERROR_MEMORY, 0, iSize, tWritten, "out of memory");
		return FALSE;
	}
	return TRUE;
}

XXAPI bool xrtZstdDecompress(const void* pData, size_t iSize, const xrtzstdoptions* pOptions,
	xbuffer* pOutput, xrtzstderror* pError)
{
	xrtzstdoptions tOptions;
	unsigned long long tContentSize;
	xrtZstdErrorInit(pError);
	if ( pOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtZstdSetError(pError, XRT_ZSTD_ERROR_ARGUMENT, 0, 0, 0, "invalid zstd input");
		return FALSE;
	}
	if ( pOptions != NULL ) { tOptions = *pOptions; } else { xrtZstdOptionsInit(&tOptions); }
	if ( tOptions.tLimits.iMaxInputBytes != 0u && iSize > tOptions.tLimits.iMaxInputBytes ) {
		__xrtZstdSetError(pError, XRT_ZSTD_ERROR_LIMIT, 0, iSize, 0, "zstd input exceeds maxInput limit");
		return FALSE;
	}
	tContentSize = ZSTD_getFrameContentSize(pData == NULL ? "" : pData, iSize);
	if ( tContentSize == ZSTD_CONTENTSIZE_ERROR ) {
		__xrtZstdSetError(pError, XRT_ZSTD_ERROR_DATA, 0, iSize, 0, "not a zstd frame");
		return FALSE;
	}
	if ( tContentSize != ZSTD_CONTENTSIZE_UNKNOWN ) {
		size_t tWritten;
		ptr pBuffer;
		if ( tOptions.tLimits.iMaxOutputBytes != 0u &&
			(uint64)tContentSize > tOptions.tLimits.iMaxOutputBytes ) {
			__xrtZstdSetError(pError, XRT_ZSTD_ERROR_LIMIT, 0, iSize, (uint64)tContentSize,
				"zstd output exceeds maxOutput limit");
			return FALSE;
		}
		if ( (uint64)tContentSize > (uint64)-1 - 1u ) {
			__xrtZstdSetError(pError, XRT_ZSTD_ERROR_DATA, 0, iSize, 0,
				"zstd frame content size overflows address space");
			return FALSE;
		}
		pBuffer = xrtMalloc((size_t)tContentSize + 1u);
		if ( pBuffer == NULL ) {
			__xrtZstdSetError(pError, XRT_ZSTD_ERROR_MEMORY, 0, iSize, 0, "out of memory");
			return FALSE;
		}
		tWritten = ZSTD_decompress(pBuffer, (size_t)tContentSize, pData, iSize);
		if ( ZSTD_isError(tWritten) ) {
			__xrtZstdSetError(pError, XRT_ZSTD_ERROR_DATA, (int)tWritten, iSize, 0,
				"%s", ZSTD_getErrorName(tWritten));
			xrtFree(pBuffer);
			return FALSE;
		}
		*pOutput = xrtBufferFrom(pBuffer, tWritten);
		xrtFree(pBuffer);
		if ( *pOutput == NULL ) {
			__xrtZstdSetError(pError, XRT_ZSTD_ERROR_MEMORY, 0, iSize, tWritten, "out of memory");
			return FALSE;
		}
		return TRUE;
	}
	{
		xrtzstddecompressor* pDecompressor = xrtZstdDecompressorCreate(&tOptions);
		bool bRet;
		if ( pDecompressor == NULL ) {
			__xrtZstdSetError(pError, XRT_ZSTD_ERROR_MEMORY, 0, iSize, 0,
				"cannot allocate zstd decompressor");
			return FALSE;
		}
		bRet = xrtZstdDecompressorUpdate(pDecompressor, pData, iSize) &&
			xrtZstdDecompressorFinish(pDecompressor);
		if ( bRet ) { *pOutput = xrtZstdDecompressorTakeBuffer(pDecompressor); bRet = *pOutput != NULL; }
		if ( !bRet ) { __xrtZstdCopyError(pError, &pDecompressor->Error); }
		xrtZstdDecompressorDestroy(pDecompressor);
		return bRet;
	}
}



// ------------------------------------ 文件流接口 ------------------------------------

XXAPI bool xrtZstdCompressStream(xreader pReader, xwriter pWriter, const xrtzstdoptions* pOptions,
	xrtzstderror* pError)
{
	uint8 sInput[XRT_ZSTD_CHUNK];
	xrtzstdcompressor* pCompressor;
	bool bRet = TRUE;
	xrtZstdErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		__xrtZstdSetError(pError, XRT_ZSTD_ERROR_ARGUMENT, 0, 0, 0, "invalid zstd stream");
		return FALSE;
	}
	pCompressor = xrtZstdCompressorCreate(pOptions);
	if ( pCompressor == NULL ) { return FALSE; }
	while ( bRet ) {
		uint64 iRead = 0u;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtZstdSetError(&pCompressor->Error, XRT_ZSTD_ERROR_IO, 0,
				pCompressor->InputBytes, pCompressor->OutputBytes, "cannot read zstd input stream");
			bRet = FALSE;
			break;
		}
		if ( iRead == 0u ) { break; }
		bRet = xrtZstdCompressorUpdate(pCompressor, sInput, (size_t)iRead) &&
			xrtZstdCompressorDrain(pCompressor, pWriter);
	}
	if ( bRet ) { bRet = xrtZstdCompressorFinish(pCompressor) && xrtZstdCompressorDrain(pCompressor, pWriter); }
	if ( !bRet ) { __xrtZstdCopyError(pError, &pCompressor->Error); }
	xrtZstdCompressorDestroy(pCompressor);
	return bRet;
}

XXAPI bool xrtZstdDecompressStream(xreader pReader, xwriter pWriter, const xrtzstdoptions* pOptions,
	xrtzstderror* pError)
{
	uint8 sInput[XRT_ZSTD_CHUNK];
	xrtzstddecompressor* pDecompressor;
	bool bRet = TRUE;
	xrtZstdErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		__xrtZstdSetError(pError, XRT_ZSTD_ERROR_ARGUMENT, 0, 0, 0, "invalid zstd stream");
		return FALSE;
	}
	pDecompressor = xrtZstdDecompressorCreate(pOptions);
	if ( pDecompressor == NULL ) { return FALSE; }
	while ( bRet ) {
		uint64 iRead = 0u;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtZstdSetError(&pDecompressor->Error, XRT_ZSTD_ERROR_IO, 0,
				pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot read zstd input stream");
			bRet = FALSE;
			break;
		}
		if ( iRead == 0u ) { break; }
		bRet = xrtZstdDecompressorUpdate(pDecompressor, sInput, (size_t)iRead) &&
			xrtZstdDecompressorDrain(pDecompressor, pWriter);
	}
	if ( bRet ) { bRet = xrtZstdDecompressorFinish(pDecompressor) && xrtZstdDecompressorDrain(pDecompressor, pWriter); }
	if ( !bRet ) { __xrtZstdCopyError(pError, &pDecompressor->Error); }
	xrtZstdDecompressorDestroy(pDecompressor);
	return bRet;
}
