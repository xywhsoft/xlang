#include "include/xrt_bz2.h"
#include "../_bzip2/third_party/bzip2/bzlib.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>



#define XRT_BZ2_CHUNK 65536u

struct xrtbz2compressor {
	bz_stream Stream;
	xbuffer Output;
	xrtbz2options Options;
	xrtbz2error Error;
	uint64 InputBytes;
	uint64 OutputBytes;
	bool Initialized;
	bool Finished;
	bool Failed;
};

struct xrtbz2decompressor {
	bz_stream Stream;
	xbuffer Output;
	xrtbz2options Options;
	xrtbz2error Error;
	uint64 InputBytes;
	uint64 OutputBytes;
	uint64 MemberCount;
	bool Initialized;
	bool MemberEnded;
	bool Finished;
	bool Failed;
};



static void* __xrtBz2Alloc(void* pOpaque, int iItems, int iSize)
{
	size_t iBytes;
	ptr pData;
	(void)pOpaque;
	if ( iItems < 0 || iSize < 0 || (iSize != 0 && (size_t)iItems > (size_t)-1 / (size_t)iSize) ) {
		return NULL;
	}
	iBytes = (size_t)iItems * (size_t)iSize;
	pData = xrtMalloc(iBytes == 0u ? 1u : iBytes);
	if ( pData != NULL && iBytes != 0u ) { memset(pData, 0, iBytes); }
	return pData;
}

static void __xrtBz2Free(void* pOpaque, void* pAddress)
{
	(void)pOpaque;
	xrtFree(pAddress);
}

static void __xrtBz2SetError(xrtbz2error* pError, int iCode, int iBackend,
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

static void __xrtBz2CopyError(xrtbz2error* pTarget, const xrtbz2error* pSource)
{
	if ( pTarget == NULL ) { return; }
	if ( pSource != NULL ) { *pTarget = *pSource; }
	else { xrtBz2ErrorInit(pTarget); }
}

static bool __xrtBz2OptionsCopy(xrtbz2options* pTarget, const xrtbz2options* pSource,
	xrtbz2error* pError)
{
	xrtBz2OptionsInit(pTarget);
	if ( pSource == NULL ) { return TRUE; }
	if ( pSource->iSize < sizeof(xrtbz2options) || pSource->iVersion != XRT_BZ2_OPTIONS_VERSION ) {
		__xrtBz2SetError(pError, XRT_BZ2_ERROR_ARGUMENT, BZ_PARAM_ERROR, 0, 0,
			"invalid bz2 options version");
		return FALSE;
	}
	*pTarget = *pSource;
	if ( pTarget->iLevel < 1 || pTarget->iLevel > 9 ) {
		__xrtBz2SetError(pError, XRT_BZ2_ERROR_ARGUMENT, BZ_PARAM_ERROR, 0, 0,
			"bz2 compression level must be 1..9");
		return FALSE;
	}
	if ( pTarget->iWorkFactor < 0 || pTarget->iWorkFactor > 250 ) {
		__xrtBz2SetError(pError, XRT_BZ2_ERROR_ARGUMENT, BZ_PARAM_ERROR, 0, 0,
			"bz2 work factor must be 0..250");
		return FALSE;
	}
	return TRUE;
}

static bool __xrtBz2InputAllowed(const xrtresourcelimits* pLimits, uint64 iOld,
	uint64 iAdded, xrtbz2error* pError, uint64 iOutput)
{
	if ( iAdded > UINT64_MAX - iOld ||
		(pLimits->iMaxInputBytes != 0u && iOld + iAdded > pLimits->iMaxInputBytes) ) {
		__xrtBz2SetError(pError, XRT_BZ2_ERROR_LIMIT, BZ_PARAM_ERROR, iOld, iOutput,
			"bz2 input exceeds the configured limit");
		return FALSE;
	}
	return TRUE;
}

static bool __xrtBz2OutputAllowed(const xrtresourcelimits* pLimits, uint64 iInput,
	uint64 iOld, uint64 iAdded, xrtbz2error* pError)
{
	uint64 iNew;
	uint64 iAllowed;
	if ( iAdded > UINT64_MAX - iOld ) {
		__xrtBz2SetError(pError, XRT_BZ2_ERROR_LIMIT, BZ_MEM_ERROR, iInput, iOld,
			"bz2 output size overflow");
		return FALSE;
	}
	iNew = iOld + iAdded;
	if ( (pLimits->iMaxOutputBytes != 0u && iNew > pLimits->iMaxOutputBytes) || iNew > UINT32_MAX ) {
		__xrtBz2SetError(pError, XRT_BZ2_ERROR_LIMIT, BZ_MEM_ERROR, iInput, iOld,
			"bz2 output exceeds the configured limit");
		return FALSE;
	}
	if ( pLimits->iMaxCompressionRatio != 0u && iInput != 0u ) {
		iAllowed = iInput > UINT64_MAX / pLimits->iMaxCompressionRatio ?
			UINT64_MAX : iInput * pLimits->iMaxCompressionRatio;
		if ( iNew > iAllowed ) {
			__xrtBz2SetError(pError, XRT_BZ2_ERROR_LIMIT, BZ_MEM_ERROR, iInput, iOld,
				"bz2 expansion ratio exceeds the configured limit");
			return FALSE;
		}
	}
	return TRUE;
}

static bool __xrtBz2Append(xbuffer pOutput, const uint8* pData, size_t iSize,
	uint64 iInput, uint64* pOutputBytes, const xrtresourcelimits* pLimits, xrtbz2error* pError)
{
	if ( iSize == 0u ) { return TRUE; }
	if ( !__xrtBz2OutputAllowed(pLimits, iInput, *pOutputBytes, iSize, pError) ) { return FALSE; }
	if ( !xrtBufferAppend(pOutput, (ptr)pData, (uint32)iSize, XBUF_BINARY) ) {
		__xrtBz2SetError(pError, XRT_BZ2_ERROR_MEMORY, BZ_MEM_ERROR, iInput, *pOutputBytes,
			"cannot grow bz2 output buffer");
		return FALSE;
	}
	*pOutputBytes += iSize;
	return TRUE;
}



XXAPI void xrtBz2OptionsInit(xrtbz2options* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_BZ2_OPTIONS_VERSION;
	pOptions->iLevel = 9;
	pOptions->iWorkFactor = 0;
	xrtResourceLimitsInit(&pOptions->tLimits);
}

XXAPI void xrtBz2ErrorInit(xrtbz2error* pError)
{
	if ( pError != NULL ) { memset(pError, 0, sizeof(*pError)); }
}



// ------------------------------------ 压缩器 ------------------------------------

XXAPI xrtbz2compressor* xrtBz2CompressorCreate(const xrtbz2options* pOptions)
{
	xrtbz2compressor* pCompressor = (xrtbz2compressor*)xrtMalloc(sizeof(xrtbz2compressor));
	int iRet;
	if ( pCompressor == NULL ) { return NULL; }
	memset(pCompressor, 0, sizeof(*pCompressor));
	if ( !__xrtBz2OptionsCopy(&pCompressor->Options, pOptions, &pCompressor->Error) ) {
		pCompressor->Failed = TRUE;
		return pCompressor;
	}
	pCompressor->Output = xrtBufferCreate(XRT_BZ2_CHUNK);
	if ( pCompressor->Output == NULL ) {
		__xrtBz2SetError(&pCompressor->Error, XRT_BZ2_ERROR_MEMORY, BZ_MEM_ERROR, 0, 0,
			"cannot create bz2 output buffer");
		pCompressor->Failed = TRUE;
		return pCompressor;
	}
	pCompressor->Stream.bzalloc = __xrtBz2Alloc;
	pCompressor->Stream.bzfree = __xrtBz2Free;
	iRet = BZ2_bzCompressInit(&pCompressor->Stream, pCompressor->Options.iLevel, 0,
		pCompressor->Options.iWorkFactor);
	if ( iRet != BZ_OK ) {
		__xrtBz2SetError(&pCompressor->Error,
			iRet == BZ_MEM_ERROR ? XRT_BZ2_ERROR_MEMORY : XRT_BZ2_ERROR_STATE,
			iRet, 0, 0, "cannot initialize bz2 compressor");
		pCompressor->Failed = TRUE;
		return pCompressor;
	}
	pCompressor->Initialized = TRUE;
	return pCompressor;
}

static bool __xrtBz2CompressRun(xrtbz2compressor* pCompressor, const uint8* pData,
	size_t iSize, int iAction)
{
	uint8 sOutput[XRT_BZ2_CHUNK];
	size_t iOffset = 0u;
	if ( pCompressor == NULL || pCompressor->Failed || pCompressor->Finished ) { return FALSE; }
	while ( iOffset < iSize || iAction == BZ_FINISH ) {
		unsigned int iInput = (unsigned int)((iSize - iOffset) > UINT_MAX ? UINT_MAX : iSize - iOffset);
		int iRet;
		size_t iConsumed;
		size_t iProduced;
		pCompressor->Stream.next_in = (char*)(pData + iOffset);
		pCompressor->Stream.avail_in = iInput;
		pCompressor->Stream.next_out = (char*)sOutput;
		pCompressor->Stream.avail_out = sizeof(sOutput);
		iRet = BZ2_bzCompress(&pCompressor->Stream, iAction);
		iConsumed = iInput - pCompressor->Stream.avail_in;
		iProduced = sizeof(sOutput) - pCompressor->Stream.avail_out;
		iOffset += iConsumed;
		if ( !__xrtBz2Append(pCompressor->Output, sOutput, iProduced, pCompressor->InputBytes,
			&pCompressor->OutputBytes, &pCompressor->Options.tLimits, &pCompressor->Error) ) {
			pCompressor->Failed = TRUE;
			return FALSE;
		}
		if ( iRet == BZ_STREAM_END ) {
			pCompressor->Finished = TRUE;
			return TRUE;
		}
		if ( (iAction == BZ_RUN && iRet != BZ_RUN_OK) ||
			(iAction == BZ_FINISH && iRet != BZ_FINISH_OK) ) {
			__xrtBz2SetError(&pCompressor->Error, XRT_BZ2_ERROR_DATA, iRet,
				pCompressor->InputBytes, pCompressor->OutputBytes, "bz2 compression failed");
			pCompressor->Failed = TRUE;
			return FALSE;
		}
		if ( iConsumed == 0u && iProduced == 0u ) {
			__xrtBz2SetError(&pCompressor->Error, XRT_BZ2_ERROR_STATE, iRet,
				pCompressor->InputBytes, pCompressor->OutputBytes, "bz2 compressor made no progress");
			pCompressor->Failed = TRUE;
			return FALSE;
		}
		if ( iAction == BZ_RUN && iOffset == iSize && pCompressor->Stream.avail_out != 0u ) { break; }
	}
	return TRUE;
}

XXAPI bool xrtBz2CompressorUpdate(xrtbz2compressor* pCompressor, const void* pData, size_t iSize)
{
	if ( pCompressor == NULL || pCompressor->Failed || pCompressor->Finished ||
		(pData == NULL && iSize != 0u) ) { return FALSE; }
	if ( !__xrtBz2InputAllowed(&pCompressor->Options.tLimits, pCompressor->InputBytes, iSize,
		&pCompressor->Error, pCompressor->OutputBytes) ) {
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	pCompressor->InputBytes += iSize;
	return iSize == 0u || __xrtBz2CompressRun(pCompressor, (const uint8*)pData, iSize, BZ_RUN);
}

XXAPI bool xrtBz2CompressorFinish(xrtbz2compressor* pCompressor)
{
	static const uint8 iEmpty = 0u;
	if ( pCompressor == NULL || pCompressor->Failed ) { return FALSE; }
	if ( pCompressor->Finished ) { return TRUE; }
	return __xrtBz2CompressRun(pCompressor, &iEmpty, 0u, BZ_FINISH);
}

XXAPI bool xrtBz2CompressorDrain(xrtbz2compressor* pCompressor, xwriter pWriter)
{
	if ( pCompressor == NULL || pCompressor->Failed || pWriter == NULL ) { return FALSE; }
	if ( pCompressor->Output->Length != 0u &&
		!xrtWriterWriteAll(pWriter, pCompressor->Output->Buffer, pCompressor->Output->Length) ) {
		__xrtBz2SetError(&pCompressor->Error, XRT_BZ2_ERROR_IO, BZ_IO_ERROR,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot write bz2 output stream");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	pCompressor->Output->Length = 0u;
	return TRUE;
}

XXAPI xbuffer xrtBz2CompressorTakeBuffer(xrtbz2compressor* pCompressor)
{
	xbuffer pOutput;
	if ( pCompressor == NULL || pCompressor->Failed || !pCompressor->Finished ) { return NULL; }
	pOutput = pCompressor->Output;
	pCompressor->Output = NULL;
	return pOutput;
}

XXAPI const xrtbz2error* xrtBz2CompressorError(const xrtbz2compressor* pCompressor)
{
	return pCompressor != NULL ? &pCompressor->Error : NULL;
}

XXAPI void xrtBz2CompressorDestroy(xrtbz2compressor* pCompressor)
{
	if ( pCompressor == NULL ) { return; }
	if ( pCompressor->Initialized ) { BZ2_bzCompressEnd(&pCompressor->Stream); }
	xrtBufferDestroy(pCompressor->Output);
	xrtFree(pCompressor);
}



// ------------------------------------ 解压器 ------------------------------------

static bool __xrtBz2DecompressorInitStream(xrtbz2decompressor* pDecompressor)
{
	int iRet;
	memset(&pDecompressor->Stream, 0, sizeof(pDecompressor->Stream));
	pDecompressor->Stream.bzalloc = __xrtBz2Alloc;
	pDecompressor->Stream.bzfree = __xrtBz2Free;
	iRet = BZ2_bzDecompressInit(&pDecompressor->Stream, 0,
		pDecompressor->Options.bSmallMemory ? 1 : 0);
	if ( iRet != BZ_OK ) {
		__xrtBz2SetError(&pDecompressor->Error,
			iRet == BZ_MEM_ERROR ? XRT_BZ2_ERROR_MEMORY : XRT_BZ2_ERROR_STATE,
			iRet, pDecompressor->InputBytes, pDecompressor->OutputBytes,
			"cannot initialize bz2 decompressor");
		return FALSE;
	}
	pDecompressor->Initialized = TRUE;
	pDecompressor->MemberEnded = FALSE;
	return TRUE;
}

XXAPI xrtbz2decompressor* xrtBz2DecompressorCreate(const xrtbz2options* pOptions)
{
	xrtbz2decompressor* pDecompressor = (xrtbz2decompressor*)xrtMalloc(sizeof(xrtbz2decompressor));
	if ( pDecompressor == NULL ) { return NULL; }
	memset(pDecompressor, 0, sizeof(*pDecompressor));
	if ( !__xrtBz2OptionsCopy(&pDecompressor->Options, pOptions, &pDecompressor->Error) ) {
		pDecompressor->Failed = TRUE;
		return pDecompressor;
	}
	pDecompressor->Output = xrtBufferCreate(XRT_BZ2_CHUNK);
	if ( pDecompressor->Output == NULL ) {
		__xrtBz2SetError(&pDecompressor->Error, XRT_BZ2_ERROR_MEMORY, BZ_MEM_ERROR, 0, 0,
			"cannot create bz2 output buffer");
		pDecompressor->Failed = TRUE;
		return pDecompressor;
	}
	if ( !__xrtBz2DecompressorInitStream(pDecompressor) ) { pDecompressor->Failed = TRUE; }
	return pDecompressor;
}

XXAPI bool xrtBz2DecompressorUpdate(xrtbz2decompressor* pDecompressor, const void* pData, size_t iSize)
{
	uint8 sOutput[XRT_BZ2_CHUNK];
	const uint8* pInput = (const uint8*)pData;
	size_t iOffset = 0u;
	if ( pDecompressor == NULL || pDecompressor->Failed || pDecompressor->Finished ||
		(pData == NULL && iSize != 0u) ) { return FALSE; }
	if ( !__xrtBz2InputAllowed(&pDecompressor->Options.tLimits, pDecompressor->InputBytes, iSize,
		&pDecompressor->Error, pDecompressor->OutputBytes) ) {
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	pDecompressor->InputBytes += iSize;
	while ( iOffset < iSize ) {
		if ( pDecompressor->MemberEnded ) {
			BZ2_bzDecompressEnd(&pDecompressor->Stream);
			pDecompressor->Initialized = FALSE;
			if ( !__xrtBz2DecompressorInitStream(pDecompressor) ) {
				pDecompressor->Failed = TRUE;
				return FALSE;
			}
		}
		{
			unsigned int iInput = (unsigned int)((iSize - iOffset) > UINT_MAX ? UINT_MAX : iSize - iOffset);
			int iRet;
			size_t iConsumed;
			size_t iProduced;
			pDecompressor->Stream.next_in = (char*)(pInput + iOffset);
			pDecompressor->Stream.avail_in = iInput;
			pDecompressor->Stream.next_out = (char*)sOutput;
			pDecompressor->Stream.avail_out = sizeof(sOutput);
			iRet = BZ2_bzDecompress(&pDecompressor->Stream);
			iConsumed = iInput - pDecompressor->Stream.avail_in;
			iProduced = sizeof(sOutput) - pDecompressor->Stream.avail_out;
			iOffset += iConsumed;
			if ( !__xrtBz2Append(pDecompressor->Output, sOutput, iProduced,
				pDecompressor->InputBytes, &pDecompressor->OutputBytes,
				&pDecompressor->Options.tLimits, &pDecompressor->Error) ) {
				pDecompressor->Failed = TRUE;
				return FALSE;
			}
			if ( iRet == BZ_STREAM_END ) {
				pDecompressor->MemberEnded = TRUE;
				pDecompressor->MemberCount++;
				continue;
			}
			if ( iRet != BZ_OK ) {
				__xrtBz2SetError(&pDecompressor->Error, XRT_BZ2_ERROR_DATA, iRet,
					pDecompressor->InputBytes - (iSize - iOffset), pDecompressor->OutputBytes,
					"bz2 data is invalid");
				pDecompressor->Failed = TRUE;
				return FALSE;
			}
			if ( iConsumed == 0u && iProduced == 0u ) {
				__xrtBz2SetError(&pDecompressor->Error, XRT_BZ2_ERROR_DATA, iRet,
					pDecompressor->InputBytes - (iSize - iOffset), pDecompressor->OutputBytes,
					"bz2 decompressor made no progress");
				pDecompressor->Failed = TRUE;
				return FALSE;
			}
		}
	}
	return TRUE;
}

XXAPI bool xrtBz2DecompressorFinish(xrtbz2decompressor* pDecompressor)
{
	if ( pDecompressor == NULL || pDecompressor->Failed ) { return FALSE; }
	if ( pDecompressor->Finished ) { return TRUE; }
	if ( !pDecompressor->MemberEnded || pDecompressor->MemberCount == 0u ) {
		__xrtBz2SetError(&pDecompressor->Error, XRT_BZ2_ERROR_TRUNCATED, BZ_UNEXPECTED_EOF,
			pDecompressor->InputBytes, pDecompressor->OutputBytes,
			pDecompressor->MemberCount == 0u ? "input is not a complete bz2 stream" : "bz2 stream is truncated");
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	pDecompressor->Finished = TRUE;
	return TRUE;
}

XXAPI bool xrtBz2DecompressorDrain(xrtbz2decompressor* pDecompressor, xwriter pWriter)
{
	if ( pDecompressor == NULL || pDecompressor->Failed || pWriter == NULL ) { return FALSE; }
	if ( pDecompressor->Output->Length != 0u &&
		!xrtWriterWriteAll(pWriter, pDecompressor->Output->Buffer, pDecompressor->Output->Length) ) {
		__xrtBz2SetError(&pDecompressor->Error, XRT_BZ2_ERROR_IO, BZ_IO_ERROR,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot write decompressed stream");
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	pDecompressor->Output->Length = 0u;
	return TRUE;
}

XXAPI xbuffer xrtBz2DecompressorTakeBuffer(xrtbz2decompressor* pDecompressor)
{
	xbuffer pOutput;
	if ( pDecompressor == NULL || pDecompressor->Failed || !pDecompressor->Finished ) { return NULL; }
	pOutput = pDecompressor->Output;
	pDecompressor->Output = NULL;
	return pOutput;
}

XXAPI const xrtbz2error* xrtBz2DecompressorError(const xrtbz2decompressor* pDecompressor)
{
	return pDecompressor != NULL ? &pDecompressor->Error : NULL;
}

XXAPI void xrtBz2DecompressorDestroy(xrtbz2decompressor* pDecompressor)
{
	if ( pDecompressor == NULL ) { return; }
	if ( pDecompressor->Initialized ) { BZ2_bzDecompressEnd(&pDecompressor->Stream); }
	xrtBufferDestroy(pDecompressor->Output);
	xrtFree(pDecompressor);
}



// ------------------------------------ 便捷与流式接口 ------------------------------------

XXAPI bool xrtBz2Compress(const void* pData, size_t iSize, const xrtbz2options* pOptions,
	xbuffer* pOutput, xrtbz2error* pError)
{
	xrtbz2compressor* pCompressor;
	bool bRet;
	if ( pOutput != NULL ) { *pOutput = NULL; }
	xrtBz2ErrorInit(pError);
	if ( pOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtBz2SetError(pError, XRT_BZ2_ERROR_ARGUMENT, BZ_PARAM_ERROR, 0, 0, "invalid bz2 input");
		return FALSE;
	}
	pCompressor = xrtBz2CompressorCreate(pOptions);
	if ( pCompressor == NULL ) {
		__xrtBz2SetError(pError, XRT_BZ2_ERROR_MEMORY, BZ_MEM_ERROR, 0, 0,
			"cannot allocate bz2 compressor");
		return FALSE;
	}
	bRet = xrtBz2CompressorUpdate(pCompressor, pData, iSize) && xrtBz2CompressorFinish(pCompressor);
	if ( bRet ) { *pOutput = xrtBz2CompressorTakeBuffer(pCompressor); bRet = *pOutput != NULL; }
	if ( !bRet ) { __xrtBz2CopyError(pError, &pCompressor->Error); }
	xrtBz2CompressorDestroy(pCompressor);
	return bRet;
}

XXAPI bool xrtBz2Decompress(const void* pData, size_t iSize, const xrtbz2options* pOptions,
	xbuffer* pOutput, xrtbz2error* pError)
{
	xrtbz2decompressor* pDecompressor;
	bool bRet;
	if ( pOutput != NULL ) { *pOutput = NULL; }
	xrtBz2ErrorInit(pError);
	if ( pOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtBz2SetError(pError, XRT_BZ2_ERROR_ARGUMENT, BZ_PARAM_ERROR, 0, 0, "invalid bz2 input");
		return FALSE;
	}
	pDecompressor = xrtBz2DecompressorCreate(pOptions);
	if ( pDecompressor == NULL ) {
		__xrtBz2SetError(pError, XRT_BZ2_ERROR_MEMORY, BZ_MEM_ERROR, 0, 0,
			"cannot allocate bz2 decompressor");
		return FALSE;
	}
	bRet = xrtBz2DecompressorUpdate(pDecompressor, pData, iSize) &&
		xrtBz2DecompressorFinish(pDecompressor);
	if ( bRet ) { *pOutput = xrtBz2DecompressorTakeBuffer(pDecompressor); bRet = *pOutput != NULL; }
	if ( !bRet ) { __xrtBz2CopyError(pError, &pDecompressor->Error); }
	xrtBz2DecompressorDestroy(pDecompressor);
	return bRet;
}

XXAPI bool xrtBz2CompressStream(xreader pReader, xwriter pWriter, const xrtbz2options* pOptions,
	xrtbz2error* pError)
{
	uint8 sInput[XRT_BZ2_CHUNK];
	xrtbz2compressor* pCompressor;
	bool bRet = TRUE;
	xrtBz2ErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		__xrtBz2SetError(pError, XRT_BZ2_ERROR_ARGUMENT, BZ_PARAM_ERROR, 0, 0, "invalid bz2 stream");
		return FALSE;
	}
	pCompressor = xrtBz2CompressorCreate(pOptions);
	if ( pCompressor == NULL ) { return FALSE; }
	while ( bRet ) {
		uint64 iRead = 0u;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtBz2SetError(&pCompressor->Error, XRT_BZ2_ERROR_IO, BZ_IO_ERROR,
				pCompressor->InputBytes, pCompressor->OutputBytes, "cannot read bz2 input stream");
			bRet = FALSE;
			break;
		}
		if ( iRead == 0u ) { break; }
		bRet = xrtBz2CompressorUpdate(pCompressor, sInput, (size_t)iRead) &&
			xrtBz2CompressorDrain(pCompressor, pWriter);
	}
	if ( bRet ) { bRet = xrtBz2CompressorFinish(pCompressor) && xrtBz2CompressorDrain(pCompressor, pWriter); }
	if ( !bRet ) { __xrtBz2CopyError(pError, &pCompressor->Error); }
	xrtBz2CompressorDestroy(pCompressor);
	return bRet;
}

XXAPI bool xrtBz2DecompressStream(xreader pReader, xwriter pWriter, const xrtbz2options* pOptions,
	xrtbz2error* pError)
{
	uint8 sInput[XRT_BZ2_CHUNK];
	xrtbz2decompressor* pDecompressor;
	bool bRet = TRUE;
	xrtBz2ErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		__xrtBz2SetError(pError, XRT_BZ2_ERROR_ARGUMENT, BZ_PARAM_ERROR, 0, 0, "invalid bz2 stream");
		return FALSE;
	}
	pDecompressor = xrtBz2DecompressorCreate(pOptions);
	if ( pDecompressor == NULL ) { return FALSE; }
	while ( bRet ) {
		uint64 iRead = 0u;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtBz2SetError(&pDecompressor->Error, XRT_BZ2_ERROR_IO, BZ_IO_ERROR,
				pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot read bz2 input stream");
			bRet = FALSE;
			break;
		}
		if ( iRead == 0u ) { break; }
		bRet = xrtBz2DecompressorUpdate(pDecompressor, sInput, (size_t)iRead) &&
			xrtBz2DecompressorDrain(pDecompressor, pWriter);
	}
	if ( bRet ) { bRet = xrtBz2DecompressorFinish(pDecompressor) && xrtBz2DecompressorDrain(pDecompressor, pWriter); }
	if ( !bRet ) { __xrtBz2CopyError(pError, &pDecompressor->Error); }
	xrtBz2DecompressorDestroy(pDecompressor);
	return bRet;
}
