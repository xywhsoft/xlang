#include "include/xrt_gzip.h"
#include "../_zlib/third_party/zlib/zlib.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>



#define XRT_GZIP_CHUNK 65536u

struct xrtgzipcompressor {
	z_stream Stream;
	gz_header Header;
	xbuffer Output;
	xrtgzipoptions Options;
	xrtgziperror Error;
	uint64 InputBytes;
	uint64 OutputBytes;
	bool Initialized;
	bool Finished;
	bool Failed;
};

struct xrtgzipdecompressor {
	z_stream Stream;
	xbuffer Output;
	xrtgzipoptions Options;
	xrtgziperror Error;
	uint64 InputBytes;
	uint64 OutputBytes;
	uint64 MemberCount;
	bool Initialized;
	bool MemberEnded;
	bool Finished;
	bool Failed;
};



static voidpf __xrtGzipAlloc(voidpf pOpaque, uInt iItems, uInt iSize)
{
	size_t iBytes;
	ptr pData;
	(void)pOpaque;
	if ( iSize != 0u && iItems > (uInt)((size_t)-1 / iSize) ) { return NULL; }
	iBytes = (size_t)iItems * iSize;
	pData = xrtMalloc(iBytes == 0u ? 1u : iBytes);
	if ( pData ) { memset(pData, 0, iBytes); }
	return pData;
}

static void __xrtGzipFree(voidpf pOpaque, voidpf pAddress)
{
	(void)pOpaque;
	xrtFree(pAddress);
}

static void __xrtGzipSetError(xrtgziperror* pError, int iCode, int iBackend,
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
	pError->sMessage[sizeof(pError->sMessage) - 1u] = 0;
}

static void __xrtGzipCopyError(xrtgziperror* pTarget, const xrtgziperror* pSource)
{
	if ( pTarget == NULL ) { return; }
	if ( pSource ) { *pTarget = *pSource; }
	else { xrtGzipErrorInit(pTarget); }
}

static bool __xrtGzipOptionsCopy(xrtgzipoptions* pTarget, const xrtgzipoptions* pSource,
	xrtgziperror* pError)
{
	xrtGzipOptionsInit(pTarget);
	if ( pSource == NULL ) { return TRUE; }
	if ( pSource->iSize < sizeof(xrtgzipoptions) || pSource->iVersion != XRT_GZIP_OPTIONS_VERSION ) {
		__xrtGzipSetError(pError, XRT_GZIP_ERROR_ARGUMENT, Z_STREAM_ERROR, 0, 0,
			"invalid gzip options version");
		return FALSE;
	}
	*pTarget = *pSource;
	if ( pTarget->iLevel < Z_DEFAULT_COMPRESSION || pTarget->iLevel > Z_BEST_COMPRESSION ) {
		__xrtGzipSetError(pError, XRT_GZIP_ERROR_ARGUMENT, Z_STREAM_ERROR, 0, 0,
			"gzip compression level must be -1..9");
		return FALSE;
	}
	if ( pTarget->iStrategy < Z_DEFAULT_STRATEGY || pTarget->iStrategy > Z_FIXED ) {
		__xrtGzipSetError(pError, XRT_GZIP_ERROR_ARGUMENT, Z_STREAM_ERROR, 0, 0,
			"invalid gzip compression strategy");
		return FALSE;
	}
	return TRUE;
}

static bool __xrtGzipInputAllowed(const xrtresourcelimits* pLimits, uint64 iOld,
	uint64 iAdded, xrtgziperror* pError, uint64 iOutput)
{
	if ( iAdded > UINT64_MAX - iOld ||
		(pLimits->iMaxInputBytes != 0u && iOld + iAdded > pLimits->iMaxInputBytes) ) {
		__xrtGzipSetError(pError, XRT_GZIP_ERROR_LIMIT, Z_MEM_ERROR, iOld, iOutput,
			"gzip input exceeds the configured limit");
		return FALSE;
	}
	return TRUE;
}

static bool __xrtGzipOutputAllowed(const xrtresourcelimits* pLimits, uint64 iInput,
	uint64 iOld, uint64 iAdded, xrtgziperror* pError)
{
	uint64 iNew;
	if ( iAdded > UINT64_MAX - iOld ) {
		__xrtGzipSetError(pError, XRT_GZIP_ERROR_LIMIT, Z_MEM_ERROR, iInput, iOld,
			"gzip output size overflow");
		return FALSE;
	}
	iNew = iOld + iAdded;
	if ( (pLimits->iMaxOutputBytes != 0u && iNew > pLimits->iMaxOutputBytes) || iNew > UINT32_MAX ) {
		__xrtGzipSetError(pError, XRT_GZIP_ERROR_LIMIT, Z_MEM_ERROR, iInput, iOld,
			"gzip output exceeds the configured limit");
		return FALSE;
	}
	if ( pLimits->iMaxCompressionRatio != 0u && iInput != 0u &&
		iNew / iInput > pLimits->iMaxCompressionRatio ) {
		__xrtGzipSetError(pError, XRT_GZIP_ERROR_LIMIT, Z_MEM_ERROR, iInput, iOld,
			"gzip expansion ratio exceeds the configured limit");
		return FALSE;
	}
	return TRUE;
}

static bool __xrtGzipAppend(xbuffer pOutput, const uint8* pData, size_t iSize,
	uint64 iInput, uint64* pOutputBytes, const xrtresourcelimits* pLimits, xrtgziperror* pError)
{
	if ( iSize == 0u ) { return TRUE; }
	if ( !__xrtGzipOutputAllowed(pLimits, iInput, *pOutputBytes, iSize, pError) ) { return FALSE; }
	if ( !xrtBufferAppend(pOutput, (ptr)pData, (uint32)iSize, XBUF_BINARY) ) {
		__xrtGzipSetError(pError, XRT_GZIP_ERROR_MEMORY, Z_MEM_ERROR, iInput, *pOutputBytes,
			"cannot grow gzip output buffer");
		return FALSE;
	}
	*pOutputBytes += iSize;
	return TRUE;
}



XXAPI void xrtGzipOptionsInit(xrtgzipoptions* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_GZIP_OPTIONS_VERSION;
	pOptions->iLevel = Z_DEFAULT_COMPRESSION;
	pOptions->iStrategy = Z_DEFAULT_STRATEGY;
	pOptions->iMTime = 0u;
	xrtResourceLimitsInit(&pOptions->tLimits);
}

XXAPI void xrtGzipErrorInit(xrtgziperror* pError)
{
	if ( pError ) { memset(pError, 0, sizeof(*pError)); }
}



// ------------------------------------ Compressor ------------------------------------

XXAPI xrtgzipcompressor* xrtGzipCompressorCreate(const xrtgzipoptions* pOptions)
{
	xrtgzipcompressor* pCompressor = (xrtgzipcompressor*)xrtMalloc(sizeof(xrtgzipcompressor));
	int iRet;
	if ( pCompressor == NULL ) { return NULL; }
	memset(pCompressor, 0, sizeof(*pCompressor));
	if ( !__xrtGzipOptionsCopy(&pCompressor->Options, pOptions, &pCompressor->Error) ) {
		pCompressor->Failed = TRUE;
		return pCompressor;
	}
	pCompressor->Output = xrtBufferCreate(XRT_GZIP_CHUNK);
	if ( pCompressor->Output == NULL ) {
		__xrtGzipSetError(&pCompressor->Error, XRT_GZIP_ERROR_MEMORY, Z_MEM_ERROR, 0, 0,
			"cannot create gzip output buffer");
		pCompressor->Failed = TRUE;
		return pCompressor;
	}
	pCompressor->Stream.zalloc = __xrtGzipAlloc;
	pCompressor->Stream.zfree = __xrtGzipFree;
	iRet = deflateInit2(&pCompressor->Stream, pCompressor->Options.iLevel, Z_DEFLATED,
		MAX_WBITS + 16, 8, pCompressor->Options.iStrategy);
	if ( iRet != Z_OK ) {
		__xrtGzipSetError(&pCompressor->Error, XRT_GZIP_ERROR_MEMORY, iRet, 0, 0,
			"cannot initialize gzip compressor");
		pCompressor->Failed = TRUE;
		return pCompressor;
	}
	pCompressor->Initialized = TRUE;
	memset(&pCompressor->Header, 0, sizeof(pCompressor->Header));
	pCompressor->Header.time = pCompressor->Options.iMTime;
	pCompressor->Header.os = 255;
	iRet = deflateSetHeader(&pCompressor->Stream, &pCompressor->Header);
	if ( iRet != Z_OK ) {
		__xrtGzipSetError(&pCompressor->Error, XRT_GZIP_ERROR_STATE, iRet, 0, 0,
			"cannot configure gzip header");
		pCompressor->Failed = TRUE;
	}
	return pCompressor;
}

static bool __xrtGzipCompressRun(xrtgzipcompressor* pCompressor, const uint8* pData,
	size_t iSize, int iFlush)
{
	uint8 sOutput[XRT_GZIP_CHUNK];
	size_t iOffset = 0;
	if ( pCompressor == NULL || pCompressor->Failed || pCompressor->Finished ) { return FALSE; }
	while ( iOffset < iSize || iFlush == Z_FINISH ) {
		uInt iInput = (uInt)((iSize - iOffset) > UINT_MAX ? UINT_MAX : (iSize - iOffset));
		int iRet;
		size_t iProduced;
		pCompressor->Stream.next_in = (Bytef*)(pData + iOffset);
		pCompressor->Stream.avail_in = iInput;
		pCompressor->Stream.next_out = sOutput;
		pCompressor->Stream.avail_out = sizeof(sOutput);
		iRet = deflate(&pCompressor->Stream, iFlush);
		iOffset += iInput - pCompressor->Stream.avail_in;
		iProduced = sizeof(sOutput) - pCompressor->Stream.avail_out;
		if ( !__xrtGzipAppend(pCompressor->Output, sOutput, iProduced, pCompressor->InputBytes,
			&pCompressor->OutputBytes, &pCompressor->Options.tLimits, &pCompressor->Error) ) {
			pCompressor->Failed = TRUE;
			return FALSE;
		}
		if ( iRet == Z_STREAM_END ) {
			pCompressor->Finished = TRUE;
			return TRUE;
		}
		if ( iRet != Z_OK ) {
			__xrtGzipSetError(&pCompressor->Error, XRT_GZIP_ERROR_DATA, iRet,
				pCompressor->InputBytes, pCompressor->OutputBytes, "gzip compression failed: %s",
				pCompressor->Stream.msg ? pCompressor->Stream.msg : "zlib error");
			pCompressor->Failed = TRUE;
			return FALSE;
		}
		if ( iFlush != Z_FINISH && iOffset == iSize && pCompressor->Stream.avail_out != 0u ) { break; }
	}
	return TRUE;
}

XXAPI bool xrtGzipCompressorUpdate(xrtgzipcompressor* pCompressor, const void* pData, size_t iSize)
{
	if ( pCompressor == NULL || pCompressor->Failed || pCompressor->Finished ||
		(pData == NULL && iSize != 0u) ) { return FALSE; }
	if ( !__xrtGzipInputAllowed(&pCompressor->Options.tLimits, pCompressor->InputBytes, iSize,
		&pCompressor->Error, pCompressor->OutputBytes) ) {
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	pCompressor->InputBytes += iSize;
	return iSize == 0u || __xrtGzipCompressRun(pCompressor, (const uint8*)pData, iSize, Z_NO_FLUSH);
}

XXAPI bool xrtGzipCompressorFinish(xrtgzipcompressor* pCompressor)
{
	static const uint8 iEmpty = 0;
	if ( pCompressor == NULL || pCompressor->Failed ) { return FALSE; }
	if ( pCompressor->Finished ) { return TRUE; }
	return __xrtGzipCompressRun(pCompressor, &iEmpty, 0u, Z_FINISH);
}

XXAPI bool xrtGzipCompressorDrain(xrtgzipcompressor* pCompressor, xwriter pWriter)
{
	if ( pCompressor == NULL || pCompressor->Failed || pWriter == NULL ) { return FALSE; }
	if ( pCompressor->Output->Length != 0u &&
		!xrtWriterWriteAll(pWriter, pCompressor->Output->Buffer, pCompressor->Output->Length) ) {
		__xrtGzipSetError(&pCompressor->Error, XRT_GZIP_ERROR_IO, Z_ERRNO,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot write gzip output stream");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	pCompressor->Output->Length = 0u;
	return TRUE;
}

XXAPI xbuffer xrtGzipCompressorTakeBuffer(xrtgzipcompressor* pCompressor)
{
	xbuffer pOutput;
	if ( pCompressor == NULL || pCompressor->Failed || !pCompressor->Finished ) { return NULL; }
	pOutput = pCompressor->Output;
	pCompressor->Output = NULL;
	return pOutput;
}

XXAPI const xrtgziperror* xrtGzipCompressorError(const xrtgzipcompressor* pCompressor)
{
	return pCompressor ? &pCompressor->Error : NULL;
}

XXAPI void xrtGzipCompressorDestroy(xrtgzipcompressor* pCompressor)
{
	if ( pCompressor == NULL ) { return; }
	if ( pCompressor->Initialized ) { deflateEnd(&pCompressor->Stream); }
	xrtBufferDestroy(pCompressor->Output);
	xrtFree(pCompressor);
}



// ------------------------------------ Decompressor ------------------------------------

XXAPI xrtgzipdecompressor* xrtGzipDecompressorCreate(const xrtgzipoptions* pOptions)
{
	xrtgzipdecompressor* pDecompressor = (xrtgzipdecompressor*)xrtMalloc(sizeof(xrtgzipdecompressor));
	int iRet;
	if ( pDecompressor == NULL ) { return NULL; }
	memset(pDecompressor, 0, sizeof(*pDecompressor));
	if ( !__xrtGzipOptionsCopy(&pDecompressor->Options, pOptions, &pDecompressor->Error) ) {
		pDecompressor->Failed = TRUE;
		return pDecompressor;
	}
	pDecompressor->Output = xrtBufferCreate(XRT_GZIP_CHUNK);
	if ( pDecompressor->Output == NULL ) {
		__xrtGzipSetError(&pDecompressor->Error, XRT_GZIP_ERROR_MEMORY, Z_MEM_ERROR, 0, 0,
			"cannot create gzip output buffer");
		pDecompressor->Failed = TRUE;
		return pDecompressor;
	}
	pDecompressor->Stream.zalloc = __xrtGzipAlloc;
	pDecompressor->Stream.zfree = __xrtGzipFree;
	iRet = inflateInit2(&pDecompressor->Stream, MAX_WBITS + 16);
	if ( iRet != Z_OK ) {
		__xrtGzipSetError(&pDecompressor->Error, XRT_GZIP_ERROR_MEMORY, iRet, 0, 0,
			"cannot initialize gzip decompressor");
		pDecompressor->Failed = TRUE;
		return pDecompressor;
	}
	pDecompressor->Initialized = TRUE;
	return pDecompressor;
}

XXAPI bool xrtGzipDecompressorUpdate(xrtgzipdecompressor* pDecompressor, const void* pData, size_t iSize)
{
	uint8 sOutput[XRT_GZIP_CHUNK];
	const uint8* pInput = (const uint8*)pData;
	size_t iOffset = 0;
	if ( pDecompressor == NULL || pDecompressor->Failed || pDecompressor->Finished ||
		(pData == NULL && iSize != 0u) ) { return FALSE; }
	if ( !__xrtGzipInputAllowed(&pDecompressor->Options.tLimits, pDecompressor->InputBytes, iSize,
		&pDecompressor->Error, pDecompressor->OutputBytes) ) {
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	pDecompressor->InputBytes += iSize;
	while ( iOffset < iSize ) {
		if ( pDecompressor->MemberEnded ) {
			while ( iOffset < iSize && pInput[iOffset] == 0u ) { ++iOffset; }
			if ( iOffset == iSize ) { return TRUE; }
			if ( inflateReset2(&pDecompressor->Stream, MAX_WBITS + 16) != Z_OK ) {
				__xrtGzipSetError(&pDecompressor->Error, XRT_GZIP_ERROR_STATE, Z_STREAM_ERROR,
					pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot reset gzip decompressor");
				pDecompressor->Failed = TRUE;
				return FALSE;
			}
			pDecompressor->MemberEnded = FALSE;
		}
		{
			uInt iInput = (uInt)((iSize - iOffset) > UINT_MAX ? UINT_MAX : (iSize - iOffset));
			int iRet;
			size_t iConsumed;
			size_t iProduced;
			pDecompressor->Stream.next_in = (Bytef*)(pInput + iOffset);
			pDecompressor->Stream.avail_in = iInput;
			pDecompressor->Stream.next_out = sOutput;
			pDecompressor->Stream.avail_out = sizeof(sOutput);
			iRet = inflate(&pDecompressor->Stream, Z_NO_FLUSH);
			iConsumed = iInput - pDecompressor->Stream.avail_in;
			iProduced = sizeof(sOutput) - pDecompressor->Stream.avail_out;
			iOffset += iConsumed;
			if ( !__xrtGzipAppend(pDecompressor->Output, sOutput, iProduced,
				pDecompressor->InputBytes, &pDecompressor->OutputBytes,
				&pDecompressor->Options.tLimits, &pDecompressor->Error) ) {
				pDecompressor->Failed = TRUE;
				return FALSE;
			}
			if ( iRet == Z_STREAM_END ) {
				pDecompressor->MemberEnded = TRUE;
				pDecompressor->MemberCount++;
				continue;
			}
			if ( iRet != Z_OK && iRet != Z_BUF_ERROR ) {
				__xrtGzipSetError(&pDecompressor->Error, XRT_GZIP_ERROR_DATA, iRet,
					pDecompressor->InputBytes - (iSize - iOffset), pDecompressor->OutputBytes,
					"gzip data is invalid: %s", pDecompressor->Stream.msg ? pDecompressor->Stream.msg : "zlib error");
				pDecompressor->Failed = TRUE;
				return FALSE;
			}
			if ( iConsumed == 0u && iProduced == 0u ) {
				__xrtGzipSetError(&pDecompressor->Error, XRT_GZIP_ERROR_DATA, iRet,
					pDecompressor->InputBytes - (iSize - iOffset), pDecompressor->OutputBytes,
					"gzip decompressor made no progress");
				pDecompressor->Failed = TRUE;
				return FALSE;
			}
		}
	}
	return TRUE;
}

XXAPI bool xrtGzipDecompressorFinish(xrtgzipdecompressor* pDecompressor)
{
	if ( pDecompressor == NULL || pDecompressor->Failed ) { return FALSE; }
	if ( pDecompressor->Finished ) { return TRUE; }
	if ( !pDecompressor->MemberEnded || pDecompressor->MemberCount == 0u ) {
		__xrtGzipSetError(&pDecompressor->Error, XRT_GZIP_ERROR_TRUNCATED, Z_BUF_ERROR,
			pDecompressor->InputBytes, pDecompressor->OutputBytes,
			pDecompressor->MemberCount == 0u ? "input is not a complete gzip stream" : "gzip stream is truncated");
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	pDecompressor->Finished = TRUE;
	return TRUE;
}

XXAPI bool xrtGzipDecompressorDrain(xrtgzipdecompressor* pDecompressor, xwriter pWriter)
{
	if ( pDecompressor == NULL || pDecompressor->Failed || pWriter == NULL ) { return FALSE; }
	if ( pDecompressor->Output->Length != 0u &&
		!xrtWriterWriteAll(pWriter, pDecompressor->Output->Buffer, pDecompressor->Output->Length) ) {
		__xrtGzipSetError(&pDecompressor->Error, XRT_GZIP_ERROR_IO, Z_ERRNO,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot write decompressed stream");
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	pDecompressor->Output->Length = 0u;
	return TRUE;
}

XXAPI xbuffer xrtGzipDecompressorTakeBuffer(xrtgzipdecompressor* pDecompressor)
{
	xbuffer pOutput;
	if ( pDecompressor == NULL || pDecompressor->Failed || !pDecompressor->Finished ) { return NULL; }
	pOutput = pDecompressor->Output;
	pDecompressor->Output = NULL;
	return pOutput;
}

XXAPI const xrtgziperror* xrtGzipDecompressorError(const xrtgzipdecompressor* pDecompressor)
{
	return pDecompressor ? &pDecompressor->Error : NULL;
}

XXAPI void xrtGzipDecompressorDestroy(xrtgzipdecompressor* pDecompressor)
{
	if ( pDecompressor == NULL ) { return; }
	if ( pDecompressor->Initialized ) { inflateEnd(&pDecompressor->Stream); }
	xrtBufferDestroy(pDecompressor->Output);
	xrtFree(pDecompressor);
}



// ------------------------------------ 便捷与流式接口 ------------------------------------

XXAPI bool xrtGzipCompress(const void* pData, size_t iSize, const xrtgzipoptions* pOptions,
	xbuffer* pOutput, xrtgziperror* pError)
{
	xrtgzipcompressor* pCompressor;
	bool bRet;
	if ( pOutput ) { *pOutput = NULL; }
	xrtGzipErrorInit(pError);
	if ( pOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtGzipSetError(pError, XRT_GZIP_ERROR_ARGUMENT, Z_STREAM_ERROR, 0, 0, "invalid gzip input");
		return FALSE;
	}
	pCompressor = xrtGzipCompressorCreate(pOptions);
	if ( pCompressor == NULL ) {
		__xrtGzipSetError(pError, XRT_GZIP_ERROR_MEMORY, Z_MEM_ERROR, 0, 0, "cannot allocate gzip compressor");
		return FALSE;
	}
	bRet = xrtGzipCompressorUpdate(pCompressor, pData, iSize) && xrtGzipCompressorFinish(pCompressor);
	if ( bRet ) { *pOutput = xrtGzipCompressorTakeBuffer(pCompressor); bRet = *pOutput != NULL; }
	if ( !bRet ) { __xrtGzipCopyError(pError, &pCompressor->Error); }
	xrtGzipCompressorDestroy(pCompressor);
	return bRet;
}

XXAPI bool xrtGzipDecompress(const void* pData, size_t iSize, const xrtgzipoptions* pOptions,
	xbuffer* pOutput, xrtgziperror* pError)
{
	xrtgzipdecompressor* pDecompressor;
	bool bRet;
	if ( pOutput ) { *pOutput = NULL; }
	xrtGzipErrorInit(pError);
	if ( pOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtGzipSetError(pError, XRT_GZIP_ERROR_ARGUMENT, Z_STREAM_ERROR, 0, 0, "invalid gzip input");
		return FALSE;
	}
	pDecompressor = xrtGzipDecompressorCreate(pOptions);
	if ( pDecompressor == NULL ) {
		__xrtGzipSetError(pError, XRT_GZIP_ERROR_MEMORY, Z_MEM_ERROR, 0, 0, "cannot allocate gzip decompressor");
		return FALSE;
	}
	bRet = xrtGzipDecompressorUpdate(pDecompressor, pData, iSize) && xrtGzipDecompressorFinish(pDecompressor);
	if ( bRet ) { *pOutput = xrtGzipDecompressorTakeBuffer(pDecompressor); bRet = *pOutput != NULL; }
	if ( !bRet ) { __xrtGzipCopyError(pError, &pDecompressor->Error); }
	xrtGzipDecompressorDestroy(pDecompressor);
	return bRet;
}

XXAPI bool xrtGzipCompressStream(xreader pReader, xwriter pWriter, const xrtgzipoptions* pOptions,
	xrtgziperror* pError)
{
	uint8 sInput[XRT_GZIP_CHUNK];
	xrtgzipcompressor* pCompressor;
	bool bRet = TRUE;
	xrtGzipErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		__xrtGzipSetError(pError, XRT_GZIP_ERROR_ARGUMENT, Z_STREAM_ERROR, 0, 0, "invalid gzip stream");
		return FALSE;
	}
	pCompressor = xrtGzipCompressorCreate(pOptions);
	if ( pCompressor == NULL ) { return FALSE; }
	while ( bRet ) {
		uint64 iRead = 0;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) { bRet = FALSE; break; }
		if ( iRead == 0u ) { break; }
		bRet = xrtGzipCompressorUpdate(pCompressor, sInput, (size_t)iRead) &&
			xrtGzipCompressorDrain(pCompressor, pWriter);
	}
	if ( bRet ) { bRet = xrtGzipCompressorFinish(pCompressor) && xrtGzipCompressorDrain(pCompressor, pWriter); }
	if ( !bRet ) { __xrtGzipCopyError(pError, &pCompressor->Error); }
	xrtGzipCompressorDestroy(pCompressor);
	return bRet;
}

XXAPI bool xrtGzipDecompressStream(xreader pReader, xwriter pWriter, const xrtgzipoptions* pOptions,
	xrtgziperror* pError)
{
	uint8 sInput[XRT_GZIP_CHUNK];
	xrtgzipdecompressor* pDecompressor;
	bool bRet = TRUE;
	xrtGzipErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		__xrtGzipSetError(pError, XRT_GZIP_ERROR_ARGUMENT, Z_STREAM_ERROR, 0, 0, "invalid gzip stream");
		return FALSE;
	}
	pDecompressor = xrtGzipDecompressorCreate(pOptions);
	if ( pDecompressor == NULL ) { return FALSE; }
	while ( bRet ) {
		uint64 iRead = 0;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) { bRet = FALSE; break; }
		if ( iRead == 0u ) { break; }
		bRet = xrtGzipDecompressorUpdate(pDecompressor, sInput, (size_t)iRead) &&
			xrtGzipDecompressorDrain(pDecompressor, pWriter);
	}
	if ( bRet ) { bRet = xrtGzipDecompressorFinish(pDecompressor) && xrtGzipDecompressorDrain(pDecompressor, pWriter); }
	if ( !bRet ) { __xrtGzipCopyError(pError, &pDecompressor->Error); }
	xrtGzipDecompressorDestroy(pDecompressor);
	return bRet;
}
