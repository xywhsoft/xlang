#include "include/xrt_miniz.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>



#define XRT_MINIZ_CHUNK 65536u
/* 公开结果是 RFC 1950 zlib 流；块边界与 Adler-32 均由 XRT 维护。 */

struct xrtminizcompressor {
	xdeflate* Codec;
	xbuffer* Output;
	xrtminizoptions Options;
	xrtminizerror Error;
	uint64 InputBytes;
	uint64 OutputBytes;
	bool Finished;
	bool Failed;
};

struct xrtminizdecompressor {
	xinflate* Codec;
	xbuffer* Output;
	xrtminizoptions Options;
	xrtminizerror Error;
	uint64 InputBytes;
	uint64 OutputBytes;
	bool Finished;
	bool Failed;
};



static void __xrtMinizSetError(xrtminizerror* pError, int iCode, int iBackend,
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

static void __xrtMinizCopyError(xrtminizerror* pDst, const xrtminizerror* pSrc)
{
	if ( pDst == NULL || pSrc == NULL ) { return; }
	*pDst = *pSrc;
}

/* 把 XRT 压缩底座错误折叠到 miniz 的稳定模块错误码。 */
static int __xrtMinizMapError(int iFallback)
{
	const xerror* pError = xrtGetError();

	switch ( xrtErrorKind(pError) ) {
		case XERR_ARGUMENT:
			return XRT_MINIZ_ERROR_ARGUMENT;
		case XERR_MEMORY:
			return XRT_MINIZ_ERROR_MEMORY;
		case XERR_RANGE:
			return XRT_MINIZ_ERROR_LIMIT;
		case XERR_STATE:
			return XRT_MINIZ_ERROR_STATE;
		case XERR_CANCELLED:
			return XRT_MINIZ_ERROR_CANCELLED;
		default:
			return iFallback;
	}
}

/* 保留 XRT 底座诊断，同时补齐 miniz 流位置。 */
static void __xrtMinizSetCodecError(xrtminizerror* pError, int iFallback,
	uint64 iInput, uint64 iOutput, const char* sFallback)
{
	const xerror* pCodecError = xrtGetError();
	const char* sMessage = xrtErrorMessage(pCodecError);
	int iBackend = pCodecError != NULL ? xrtErrorCode(pCodecError) : 0;

	__xrtMinizSetError(pError, __xrtMinizMapError(iFallback), iBackend,
		iInput, iOutput, "%s", sMessage != NULL ? sMessage : sFallback);
}

static bool __xrtMinizOverRatio(const xrtresourcelimits* pLimits, uint64 iInput, uint64 iOutput)
{
	uint64 iRatio = pLimits->iMaxCompressionRatio;
	if ( iRatio == 0u || iInput == 0u ) { return false; }
	if ( iInput > (UINT64_MAX / iRatio) ) { return false; }
	return iOutput > iInput * iRatio;
}

/* 检查计数累加是否溢出或越过非零硬上限。 */
static bool __xrtMinizAddOverLimit(uint64 iCurrent, size_t iAdded, uint64 iLimit)
{
	uint64 iIncrement = (uint64)iAdded;

	if ( iIncrement > (UINT64_MAX - iCurrent) ) { return true; }
	return (iLimit != 0u) && ((iCurrent + iIncrement) > iLimit);
}

/* 创建带初始容量的 Buffer，失败时不泄漏半初始化对象。 */
static xbuffer* __xrtMinizBufferCreate(size_t iCapacity)
{
	xbuffer* pBuffer = xrtBufferCreate();

	if ( (pBuffer != NULL) && !xrtBufferReserve(pBuffer, iCapacity) ) {
		xrtBufferDestroy(pBuffer);
		pBuffer = NULL;
	}
	return pBuffer;
}

static bool __xrtMinizEncodeOutput(xbytesview Data, ptr pUserData)
{
	xrtminizcompressor* pCompressor = (xrtminizcompressor*)pUserData;
	if ( __xrtMinizAddOverLimit(pCompressor->OutputBytes, Data.Size,
		pCompressor->Options.tLimits.iMaxOutputBytes) ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_LIMIT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "miniz output exceeds maxOutput limit");
		return false;
	}
	if ( !xrtBufferAppend(pCompressor->Output, Data) ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow miniz output buffer");
		return false;
	}
	pCompressor->OutputBytes += Data.Size;
	return true;
}

static bool __xrtMinizDecodeOutput(xbytesview Data, ptr pUserData)
{
	xrtminizdecompressor* pDecompressor = (xrtminizdecompressor*)pUserData;
	if ( ((pDecompressor->Options.tLimits.iMaxItemBytes != 0u) &&
		  __xrtMinizAddOverLimit(pDecompressor->OutputBytes, Data.Size,
			pDecompressor->Options.tLimits.iMaxItemBytes)) ||
		 __xrtMinizAddOverLimit(pDecompressor->OutputBytes, Data.Size,
			pDecompressor->Options.tLimits.iMaxOutputBytes) ||
		 __xrtMinizOverRatio(&pDecompressor->Options.tLimits,
			pDecompressor->InputBytes, pDecompressor->OutputBytes + (uint64)Data.Size) ) {
		__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_LIMIT, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes,
			"miniz output exceeds decompression limit");
		return false;
	}
	if ( !xrtBufferAppend(pDecompressor->Output, Data) ) {
		__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_MEMORY, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes,
			"cannot grow miniz output buffer");
		return false;
	}
	pDecompressor->OutputBytes += Data.Size;
	return true;
}



XRT_API void xrtMinizErrorInit(xrtminizerror* pError)
{
	if ( pError == NULL ) { return; }
	memset(pError, 0, sizeof(*pError));
	pError->iCode = XRT_MINIZ_OK;
}

XRT_API void xrtMinizOptionsInit(xrtminizoptions* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_MINIZ_OPTIONS_VERSION;
	pOptions->iLevel = XDEFLATE_LEVEL_DEFAULT;
	xrtResourceLimitsInit(&pOptions->tLimits);
	pOptions->tLimits.iMaxEntries = 0u;
	pOptions->tLimits.iMaxNodes = 0u;
	pOptions->tLimits.iMaxDepth = 0u;
}

/* 外层长度未确认前不可复制选项或读取内嵌限额。 */
static bool __xrtMinizCopyOptions(xrtminizoptions* pOutput,
	const xrtminizoptions* pInput)
{
	xrtMinizOptionsInit(pOutput);
	if ( pInput == NULL ) { return true; }
	if ( pInput->iSize != sizeof(*pInput) ||
		pInput->iVersion != XRT_MINIZ_OPTIONS_VERSION ) { return false; }
	if ( pInput->sReserved[0] != 0u || pInput->sReserved[1] != 0u ||
		pInput->sReserved[2] != 0u || pInput->sReserved[3] != 0u ||
		pInput->iLevel < 0 || pInput->iLevel > 10 ||
		pInput->tLimits.iSize != sizeof(pInput->tLimits) ||
		pInput->tLimits.iVersion != XRT_RESOURCE_LIMITS_VERSION ||
		pInput->tLimits.iReserved != 0u ||
		pInput->tLimits.iFlags != 0u ||
		pInput->tLimits.iMaxEntries != 0u ||
		pInput->tLimits.iMaxNodes != 0u ||
		pInput->tLimits.iMaxDepth != 0u ) { return false; }
	*pOutput = *pInput;
	return true;
}



// ------------------------------------ 压缩器 ------------------------------------

XRT_API xrtminizcompressor* xrtMinizCompressorCreate(const xrtminizoptions* pOptions)
{
	xrtminizcompressor* pCompressor;
	xdeflateconfig Config;
	pCompressor = (xrtminizcompressor*)xrtMalloc(sizeof(xrtminizcompressor));
	if ( pCompressor == NULL ) { return NULL; }
	memset(pCompressor, 0, sizeof(*pCompressor));
	xrtMinizErrorInit(&pCompressor->Error);
	if ( !__xrtMinizCopyOptions(&pCompressor->Options, pOptions) ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_ARGUMENT,
			0, 0, 0, "invalid miniz options");
		pCompressor->Failed = true;
		return pCompressor;
	}
	pCompressor->Output = __xrtMinizBufferCreate(XRT_MINIZ_CHUNK);
	xrtDeflateConfigInit(&Config);
	Config.Format = XDEFLATE_ZLIB;
	Config.Level = pCompressor->Options.iLevel;
	if ( pCompressor->Options.tLimits.iMaxOutputBytes != 0u ) {
		Config.OutputLimit = pCompressor->Options.tLimits.iMaxOutputBytes;
	}
	pCompressor->Codec = xrtDeflateCreate(&Config);
	if ( pCompressor->Output == NULL || pCompressor->Codec == NULL ) {
		__xrtMinizSetCodecError(&pCompressor->Error, XRT_MINIZ_ERROR_MEMORY,
			0, 0, "cannot create miniz compressor");
		pCompressor->Failed = true;
		return pCompressor;
	}
	return pCompressor;
}

XRT_API bool xrtMinizCompressorUpdate(xrtminizcompressor* pCompressor, const void* pData, size_t iSize)
{
	if ( pCompressor == NULL || pCompressor->Failed ) { return false; }
	if ( pCompressor->Finished ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_STATE, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "miniz compressor already finished");
		pCompressor->Failed = true;
		return false;
	}
	if ( iSize != 0u && pData == NULL ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_ARGUMENT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "invalid miniz input");
		pCompressor->Failed = true;
		return false;
	}
	if ( __xrtMinizAddOverLimit(pCompressor->InputBytes, iSize,
		pCompressor->Options.tLimits.iMaxInputBytes) ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_LIMIT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "miniz input exceeds maxInput limit");
		pCompressor->Failed = true;
		return false;
	}
	pCompressor->InputBytes += iSize;
	if ( iSize != 0u && !xrtDeflateWrite(pCompressor->Codec,
		(xbytesview){ (const unsigned char*)pData, iSize }, XDEFLATE_FLUSH_NONE,
		__xrtMinizEncodeOutput, pCompressor) ) {
		if ( pCompressor->Error.iCode == XRT_MINIZ_OK ) {
			__xrtMinizSetCodecError(&pCompressor->Error, XRT_MINIZ_ERROR_DATA,
				pCompressor->InputBytes, pCompressor->OutputBytes, "miniz compression failed");
		}
		pCompressor->Failed = true;
		return false;
	}
	return true;
}

XRT_API bool xrtMinizCompressorFinish(xrtminizcompressor* pCompressor)
{
	if ( pCompressor == NULL || pCompressor->Failed ) { return false; }
	if ( pCompressor->Finished ) { return true; }
	if ( !xrtDeflateWrite(pCompressor->Codec, (xbytesview){ NULL, 0u },
		XDEFLATE_FLUSH_FINISH, __xrtMinizEncodeOutput, pCompressor) ) {
		if ( pCompressor->Error.iCode == XRT_MINIZ_OK ) {
			__xrtMinizSetCodecError(&pCompressor->Error, XRT_MINIZ_ERROR_DATA,
				pCompressor->InputBytes, pCompressor->OutputBytes, "miniz finish failed");
		}
		pCompressor->Failed = true;
		return false;
	}
	pCompressor->Finished = true;
	return true;
}

XRT_API bool xrtMinizCompressorDrain(xrtminizcompressor* pCompressor, xwriter* pWriter)
{
	xbytesview Output;
	if ( pCompressor == NULL || pCompressor->Failed || pWriter == NULL ) { return false; }
	Output = xrtBufferView(pCompressor->Output);
	if ( Output.Size != 0u &&
		!xrtWriterWriteFull(pWriter, Output.Data, Output.Size, NULL) ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_IO, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot write miniz output stream");
		pCompressor->Failed = true;
		return false;
	}
	xrtBufferClear(pCompressor->Output);
	return true;
}

XRT_API xbuffer* xrtMinizCompressorTakeBuffer(xrtminizcompressor* pCompressor)
{
	xbuffer* pOutput;
	if ( pCompressor == NULL || pCompressor->Failed || !pCompressor->Finished ) { return NULL; }
	pOutput = pCompressor->Output;
	pCompressor->Output = NULL;
	return pOutput;
}

XRT_API xbuffer* xrtMinizCompressorDrainBuffer(xrtminizcompressor* pCompressor)
{
	xbuffer* pOutput;
	if ( pCompressor == NULL || pCompressor->Failed || pCompressor->Output == NULL ) { return NULL; }
	pOutput = xrtBufferFrom(xrtBufferView(pCompressor->Output));
	if ( pOutput != NULL ) { xrtBufferClear(pCompressor->Output); }
	return pOutput;
}

XRT_API uint64 xrtMinizCompressorInputBytes(const xrtminizcompressor* pCompressor)
{
	return pCompressor != NULL ? pCompressor->InputBytes : 0u;
}

XRT_API uint64 xrtMinizCompressorOutputBytes(const xrtminizcompressor* pCompressor)
{
	return pCompressor != NULL ? pCompressor->OutputBytes : 0u;
}

XRT_API const xrtminizerror* xrtMinizCompressorError(const xrtminizcompressor* pCompressor)
{
	return pCompressor != NULL ? &pCompressor->Error : NULL;
}

XRT_API void xrtMinizCompressorDestroy(xrtminizcompressor* pCompressor)
{
	if ( pCompressor == NULL ) { return; }
	xrtDeflateDestroy(pCompressor->Codec);
	xrtBufferDestroy(pCompressor->Output);
	xrtFree(pCompressor);
}



// ------------------------------------ 解压器 ------------------------------------

XRT_API xrtminizdecompressor* xrtMinizDecompressorCreate(const xrtminizoptions* pOptions)
{
	xrtminizdecompressor* pDecompressor;
	xinflateconfig Config;
	pDecompressor = (xrtminizdecompressor*)xrtMalloc(sizeof(xrtminizdecompressor));
	if ( pDecompressor == NULL ) { return NULL; }
	memset(pDecompressor, 0, sizeof(*pDecompressor));
	xrtMinizErrorInit(&pDecompressor->Error);
	if ( !__xrtMinizCopyOptions(&pDecompressor->Options, pOptions) ) {
		__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_ARGUMENT,
			0, 0, 0, "invalid miniz options");
		pDecompressor->Failed = true;
		return pDecompressor;
	}
	pDecompressor->Output = __xrtMinizBufferCreate(XRT_MINIZ_CHUNK);
	xrtInflateConfigInit(&Config);
	Config.Format = XINFLATE_ZLIB;
	if ( pDecompressor->Options.tLimits.iMaxOutputBytes != 0u ) {
		Config.OutputLimit = pDecompressor->Options.tLimits.iMaxOutputBytes;
	}
	if ( pDecompressor->Options.tLimits.iMaxItemBytes != 0u &&
		pDecompressor->Options.tLimits.iMaxItemBytes < Config.OutputLimit ) {
		Config.OutputLimit = pDecompressor->Options.tLimits.iMaxItemBytes;
	}
	pDecompressor->Codec = xrtInflateCreate(&Config);
	if ( pDecompressor->Output == NULL || pDecompressor->Codec == NULL ) {
		__xrtMinizSetCodecError(&pDecompressor->Error, XRT_MINIZ_ERROR_MEMORY,
			0, 0, "cannot create miniz decompressor");
		pDecompressor->Failed = true;
		return pDecompressor;
	}
	return pDecompressor;
}

XRT_API bool xrtMinizDecompressorUpdate(xrtminizdecompressor* pDecompressor, const void* pData, size_t iSize)
{
	if ( pDecompressor == NULL || pDecompressor->Failed ) { return false; }
	if ( pDecompressor->Finished || xrtInflateDone(pDecompressor->Codec) ) {
		__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_STATE, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes,
			"miniz decompressor already reached stream end");
		pDecompressor->Failed = true;
		return false;
	}
	if ( iSize != 0u && pData == NULL ) {
		__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_ARGUMENT, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "invalid miniz input");
		pDecompressor->Failed = true;
		return false;
	}
	if ( __xrtMinizAddOverLimit(pDecompressor->InputBytes, iSize,
		pDecompressor->Options.tLimits.iMaxInputBytes) ) {
		__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_LIMIT, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "miniz input exceeds maxInput limit");
		pDecompressor->Failed = true;
		return false;
	}
	pDecompressor->InputBytes += iSize;
	if ( iSize != 0u && !xrtInflateWrite(pDecompressor->Codec,
		(xbytesview){ (const unsigned char*)pData, iSize }, false,
		__xrtMinizDecodeOutput, pDecompressor) ) {
		if ( pDecompressor->Error.iCode == XRT_MINIZ_OK ) {
			__xrtMinizSetCodecError(&pDecompressor->Error, XRT_MINIZ_ERROR_DATA,
				pDecompressor->InputBytes, pDecompressor->OutputBytes, "invalid zlib stream");
		}
		pDecompressor->Failed = true;
		return false;
	}
	return true;
}

XRT_API bool xrtMinizDecompressorFinish(xrtminizdecompressor* pDecompressor)
{
	if ( pDecompressor == NULL || pDecompressor->Failed ) { return false; }
	if ( pDecompressor->Finished ) { return true; }
	if ( !xrtInflateDone(pDecompressor->Codec) &&
		 !xrtInflateWrite(pDecompressor->Codec, (xbytesview){ NULL, 0u }, true,
			__xrtMinizDecodeOutput, pDecompressor) ) {
		if ( pDecompressor->Error.iCode == XRT_MINIZ_OK ) {
			__xrtMinizSetCodecError(&pDecompressor->Error, XRT_MINIZ_ERROR_TRUNCATED,
				pDecompressor->InputBytes, pDecompressor->OutputBytes, "zlib stream is incomplete");
		}
		pDecompressor->Failed = true;
		return false;
	}
	pDecompressor->Finished = true;
	return true;
}

XRT_API bool xrtMinizDecompressorDrain(xrtminizdecompressor* pDecompressor, xwriter* pWriter)
{
	xbytesview Output;
	if ( pDecompressor == NULL || pDecompressor->Failed || pWriter == NULL ) { return false; }
	Output = xrtBufferView(pDecompressor->Output);
	if ( Output.Size != 0u &&
		!xrtWriterWriteFull(pWriter, Output.Data, Output.Size, NULL) ) {
		__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_IO, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot write miniz output stream");
		pDecompressor->Failed = true;
		return false;
	}
	xrtBufferClear(pDecompressor->Output);
	return true;
}

XRT_API xbuffer* xrtMinizDecompressorTakeBuffer(xrtminizdecompressor* pDecompressor)
{
	xbuffer* pOutput;
	if ( pDecompressor == NULL || pDecompressor->Failed || !pDecompressor->Finished ) { return NULL; }
	pOutput = pDecompressor->Output;
	pDecompressor->Output = NULL;
	return pOutput;
}

XRT_API xbuffer* xrtMinizDecompressorDrainBuffer(xrtminizdecompressor* pDecompressor)
{
	xbuffer* pOutput;
	if ( pDecompressor == NULL || pDecompressor->Failed || pDecompressor->Output == NULL ) { return NULL; }
	pOutput = xrtBufferFrom(xrtBufferView(pDecompressor->Output));
	if ( pOutput != NULL ) { xrtBufferClear(pDecompressor->Output); }
	return pOutput;
}

XRT_API uint64 xrtMinizDecompressorInputBytes(const xrtminizdecompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->InputBytes : 0u;
}

XRT_API uint64 xrtMinizDecompressorOutputBytes(const xrtminizdecompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->OutputBytes : 0u;
}

XRT_API bool xrtMinizDecompressorFinished(const xrtminizdecompressor* pDecompressor)
{
	return pDecompressor != NULL && pDecompressor->Codec != NULL &&
		!pDecompressor->Failed && xrtInflateDone(pDecompressor->Codec);
}

XRT_API const xrtminizerror* xrtMinizDecompressorError(const xrtminizdecompressor* pDecompressor)
{
	return pDecompressor != NULL ? &pDecompressor->Error : NULL;
}

XRT_API void xrtMinizDecompressorDestroy(xrtminizdecompressor* pDecompressor)
{
	if ( pDecompressor == NULL ) { return; }
	xrtInflateDestroy(pDecompressor->Codec);
	xrtBufferDestroy(pDecompressor->Output);
	xrtFree(pDecompressor);
}



// ------------------------------------ 一次性接口 ------------------------------------

XRT_API bool xrtMinizCompress(const void* pData, size_t iSize, const xrtminizoptions* pOptions,
	xbuffer** ppOutput, xrtminizerror* pError)
{
	xrtminizcompressor* pCompressor;
	bool bRet;
	xrtMinizErrorInit(pError);
	if ( ppOutput != NULL ) { *ppOutput = NULL; }
	if ( ppOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtMinizSetError(pError, XRT_MINIZ_ERROR_ARGUMENT, 0, 0, 0, "invalid miniz input");
		return false;
	}
	pCompressor = xrtMinizCompressorCreate(pOptions);
	if ( pCompressor == NULL ) {
		__xrtMinizSetError(pError, XRT_MINIZ_ERROR_MEMORY, 0, iSize, 0, "cannot allocate miniz compressor");
		return false;
	}
	bRet = xrtMinizCompressorUpdate(pCompressor, pData, iSize) &&
		xrtMinizCompressorFinish(pCompressor);
	if ( bRet ) { *ppOutput = xrtMinizCompressorTakeBuffer(pCompressor); bRet = *ppOutput != NULL; }
	if ( !bRet ) { __xrtMinizCopyError(pError, &pCompressor->Error); }
	xrtMinizCompressorDestroy(pCompressor);
	return bRet;
}

XRT_API bool xrtMinizDecompress(const void* pData, size_t iSize, const xrtminizoptions* pOptions,
	xbuffer** ppOutput, xrtminizerror* pError)
{
	xrtminizdecompressor* pDecompressor;
	bool bRet;
	xrtMinizErrorInit(pError);
	if ( ppOutput != NULL ) { *ppOutput = NULL; }
	if ( ppOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtMinizSetError(pError, XRT_MINIZ_ERROR_ARGUMENT, 0, 0, 0, "invalid miniz input");
		return false;
	}
	pDecompressor = xrtMinizDecompressorCreate(pOptions);
	if ( pDecompressor == NULL ) {
		__xrtMinizSetError(pError, XRT_MINIZ_ERROR_MEMORY, 0, iSize, 0, "cannot allocate miniz decompressor");
		return false;
	}
	bRet = xrtMinizDecompressorUpdate(pDecompressor, pData, iSize) &&
		xrtMinizDecompressorFinish(pDecompressor);
	if ( bRet ) { *ppOutput = xrtMinizDecompressorTakeBuffer(pDecompressor); bRet = *ppOutput != NULL; }
	if ( !bRet ) { __xrtMinizCopyError(pError, &pDecompressor->Error); }
	xrtMinizDecompressorDestroy(pDecompressor);
	return bRet;
}



// ------------------------------------ 文件流接口 ------------------------------------

/* 进度总量以本次读取起点为基准，不能包含 Reader 中已跳过的前缀。 */
static bool __xrtMinizReaderRemaining(xreader* pReader, uint64* pRemaining)
{
	uint64 iSize = 0u;
	uint64 iPosition = 0u;
	if ( xrtReaderSize(pReader, &iSize) &&
		xrtReaderTell(pReader, &iPosition) && iPosition <= iSize ) {
		*pRemaining = iSize - iPosition;
		return true;
	}
	xrtClearError();
	return false;
}

XRT_API bool xrtMinizCompressStream(xreader* pReader, xwriter* pWriter, const xrtminizoptions* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtminizerror* pError)
{
	uint8 sInput[XRT_MINIZ_CHUNK];
	xrtminizcompressor* pCompressor;
	uint64 iTotal = 0u;
	bool bTotalKnown;
	bool bRet = true;
	xrtMinizErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		__xrtMinizSetError(pError, XRT_MINIZ_ERROR_ARGUMENT, 0, 0, 0, "invalid miniz stream");
		return false;
	}
	bTotalKnown = __xrtMinizReaderRemaining(pReader, &iTotal);
	pCompressor = xrtMinizCompressorCreate(pOptions);
	if ( pCompressor == NULL ) {
		__xrtMinizSetError(pError, XRT_MINIZ_ERROR_MEMORY,
			0, 0, 0, "cannot allocate miniz compressor");
		return false;
	}
	if ( pCompressor->Failed ) {
		__xrtMinizCopyError(pError, &pCompressor->Error);
		xrtMinizCompressorDestroy(pCompressor);
		return false;
	}
	while ( bRet ) {
		size_t iRead = 0u;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_IO, 0,
				pCompressor->InputBytes, pCompressor->OutputBytes, "cannot read miniz input stream");
			bRet = false;
			break;
		}
		if ( iRead == 0u ) { break; }
		bRet = xrtMinizCompressorUpdate(pCompressor, sInput, (size_t)iRead) &&
			xrtMinizCompressorDrain(pCompressor, pWriter);
		if ( bRet && !xrtProgressReport(pProgress, pUserData, pCompressor->InputBytes,
			iTotal, pCompressor->OutputBytes,
			bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u) ) {
			__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_CANCELLED, 0,
				pCompressor->InputBytes, pCompressor->OutputBytes, "miniz operation cancelled");
			bRet = false;
		}
	}
	if ( bRet ) { bRet = xrtMinizCompressorFinish(pCompressor) && xrtMinizCompressorDrain(pCompressor, pWriter); }
	if ( bRet && !xrtProgressReport(pProgress, pUserData, pCompressor->InputBytes,
		iTotal, pCompressor->OutputBytes, XRT_PROGRESS_FINAL |
		(bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u)) ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_CANCELLED, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "miniz operation cancelled");
		bRet = false;
	}
	if ( !bRet ) { __xrtMinizCopyError(pError, &pCompressor->Error); }
	xrtMinizCompressorDestroy(pCompressor);
	return bRet;
}

XRT_API bool xrtMinizDecompressStream(xreader* pReader, xwriter* pWriter, const xrtminizoptions* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtminizerror* pError)
{
	uint8 sInput[XRT_MINIZ_CHUNK];
	xrtminizdecompressor* pDecompressor;
	uint64 iTotal = 0u;
	bool bTotalKnown;
	bool bRet = true;
	xrtMinizErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		__xrtMinizSetError(pError, XRT_MINIZ_ERROR_ARGUMENT, 0, 0, 0, "invalid miniz stream");
		return false;
	}
	bTotalKnown = __xrtMinizReaderRemaining(pReader, &iTotal);
	pDecompressor = xrtMinizDecompressorCreate(pOptions);
	if ( pDecompressor == NULL ) {
		__xrtMinizSetError(pError, XRT_MINIZ_ERROR_MEMORY,
			0, 0, 0, "cannot allocate miniz decompressor");
		return false;
	}
	if ( pDecompressor->Failed ) {
		__xrtMinizCopyError(pError, &pDecompressor->Error);
		xrtMinizDecompressorDestroy(pDecompressor);
		return false;
	}
	while ( bRet ) {
		size_t iRead = 0u;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_IO, 0,
				pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot read miniz input stream");
			bRet = false;
			break;
		}
		if ( iRead == 0u ) { break; }
		bRet = xrtMinizDecompressorUpdate(pDecompressor, sInput, (size_t)iRead) &&
			xrtMinizDecompressorDrain(pDecompressor, pWriter);
		if ( bRet && !xrtProgressReport(pProgress, pUserData, pDecompressor->InputBytes,
			iTotal, pDecompressor->OutputBytes,
			bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u) ) {
			__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_CANCELLED, 0,
				pDecompressor->InputBytes, pDecompressor->OutputBytes, "miniz operation cancelled");
			bRet = false;
		}
	}
	if ( bRet ) { bRet = xrtMinizDecompressorFinish(pDecompressor) && xrtMinizDecompressorDrain(pDecompressor, pWriter); }
	if ( bRet && !xrtProgressReport(pProgress, pUserData, pDecompressor->InputBytes,
		iTotal, pDecompressor->OutputBytes, XRT_PROGRESS_FINAL |
		(bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u)) ) {
		__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_CANCELLED, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "miniz operation cancelled");
		bRet = false;
	}
	if ( !bRet ) { __xrtMinizCopyError(pError, &pDecompressor->Error); }
	xrtMinizDecompressorDestroy(pDecompressor);
	return bRet;
}
