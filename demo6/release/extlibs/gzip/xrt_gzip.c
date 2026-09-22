#define XRT_MODULE_DEFLATE
#define XRT_MODULE_INFLATE

#include "include/xrt_gzip.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>



#define XRT_GZIP_CHUNK 65536u
#define XRT_GZIP_HEADER_SIZE 10u
#define XRT_GZIP_TRAILER_SIZE 8u



struct xrtgzipcompressor {
	xdeflate* Deflate;
	xbuffer* Output;
	xrtgzipoptions Options;
	xrtgziperror Error;
	uint64 InputBytes;
	uint64 OutputBytes;
	uint32 Crc;
	bool Finished;
	bool Failed;
};


struct xrtgzipdecompressor {
	xinflate* Inflate;
	xbuffer* Output;
	xrtgzipoptions Options;
	xrtgziperror Error;
	uint64 InputBytes;
	uint64 OutputBytes;
	bool Finished;
	bool Failed;
};



/* 写入 gzip 模块错误，并记录输入输出位置。 */
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


/* 复制模块错误；空来源表示成功状态。 */
static void __xrtGzipCopyError(xrtgziperror* pTarget, const xrtgziperror* pSource)
{
	if ( pTarget == NULL ) { return; }
	if ( pSource != NULL ) { *pTarget = *pSource; }
	else { xrtGzipErrorInit(pTarget); }
}


/* 把 XRT 压缩底座错误折叠到 gzip 的稳定模块错误码。 */
static int __xrtGzipMapError(int iFallback)
{
	const xerror* pError = xrtGetError();

	switch ( xrtErrorKind(pError) ) {
		case XERR_ARGUMENT:
			return XRT_GZIP_ERROR_ARGUMENT;
		case XERR_MEMORY:
			return XRT_GZIP_ERROR_MEMORY;
		case XERR_RANGE:
			return XRT_GZIP_ERROR_LIMIT;
		case XERR_STATE:
			return XRT_GZIP_ERROR_STATE;
		case XERR_PROTOCOL:
			return iFallback == XRT_GZIP_ERROR_TRUNCATED ?
				XRT_GZIP_ERROR_TRUNCATED : XRT_GZIP_ERROR_DATA;
		default:
			return iFallback;
	}
}


/* 保留 XRT 底座诊断，同时补齐 gzip 流位置。 */
static void __xrtGzipSetCodecError(xrtgziperror* pError, int iFallback,
	uint64 iInput, uint64 iOutput, const char* sFallback)
{
	const xerror* pCodecError = xrtGetError();
	const char* sMessage = xrtErrorMessage(pCodecError);
	int iBackend = pCodecError != NULL ? xrtErrorCode(pCodecError) : 0;

	if ( (pError != NULL) && (pError->iCode != XRT_GZIP_OK) ) { return; }
	__xrtGzipSetError(pError, __xrtGzipMapError(iFallback), iBackend,
		iInput, iOutput, "%s", sMessage != NULL ? sMessage : sFallback);
}


/* 验证并复制完整配置。 */
static bool __xrtGzipOptionsCopy(xrtgzipoptions* pTarget,
	const xrtgzipoptions* pSource, xrtgziperror* pError)
{
	xrtGzipOptionsInit(pTarget);
	if ( pSource == NULL ) { return true; }
	if ( (pSource->iSize < sizeof(xrtgzipoptions)) ||
		(pSource->iVersion != XRT_GZIP_OPTIONS_VERSION) ) {
		__xrtGzipSetError(pError, XRT_GZIP_ERROR_ARGUMENT, 0, 0, 0,
			"invalid gzip options version");
		return false;
	}
	*pTarget = *pSource;
	if ( (pTarget->iLevel < -1) || (pTarget->iLevel > 9) ) {
		__xrtGzipSetError(pError, XRT_GZIP_ERROR_ARGUMENT, 0, 0, 0,
			"gzip compression level must be -1..9");
		return false;
	}
	if ( (pTarget->iStrategy < XDEFLATE_STRATEGY_DEFAULT) ||
		(pTarget->iStrategy > XDEFLATE_STRATEGY_FIXED) ) {
		__xrtGzipSetError(pError, XRT_GZIP_ERROR_ARGUMENT, 0, 0, 0,
			"invalid gzip compression strategy");
		return false;
	}
	return true;
}


/* 检查输入计数累加是否溢出或越过非零硬上限。 */
static bool __xrtGzipInputAllowed(const xrtresourcelimits* pLimits,
	uint64 iOld, size_t iAdded, xrtgziperror* pError, uint64 iOutput)
{
	uint64 iIncrement = (uint64)iAdded;

	if ( (iIncrement > (UINT64_MAX - iOld)) ||
		((pLimits->iMaxInputBytes != 0u) &&
		 ((iOld + iIncrement) > pLimits->iMaxInputBytes)) ) {
		__xrtGzipSetError(pError, XRT_GZIP_ERROR_LIMIT, 0, iOld, iOutput,
			"gzip input exceeds the configured limit");
		return false;
	}
	return true;
}


/* 检查输出计数、硬上限和解压倍率。 */
static bool __xrtGzipOutputAllowed(const xrtresourcelimits* pLimits,
	uint64 iInput, uint64 iOld, size_t iAdded, bool bCheckRatio,
	xrtgziperror* pError)
{
	uint64 iIncrement = (uint64)iAdded;
	uint64 iNew;
	uint64 iRatio = pLimits->iMaxCompressionRatio;

	if ( iIncrement > (UINT64_MAX - iOld) ) {
		__xrtGzipSetError(pError, XRT_GZIP_ERROR_LIMIT, 0, iInput, iOld,
			"gzip output size overflow");
		return false;
	}
	iNew = iOld + iIncrement;
	if ( (pLimits->iMaxOutputBytes != 0u) &&
		(iNew > pLimits->iMaxOutputBytes) ) {
		__xrtGzipSetError(pError, XRT_GZIP_ERROR_LIMIT, 0, iInput, iOld,
			"gzip output exceeds the configured limit");
		return false;
	}
	if ( bCheckRatio && (iRatio != 0u) && (iInput != 0u) &&
		(iInput <= (UINT64_MAX / iRatio)) &&
		(iNew > (iInput * iRatio)) ) {
		__xrtGzipSetError(pError, XRT_GZIP_ERROR_LIMIT, 0, iInput, iOld,
			"gzip expansion ratio exceeds the configured limit");
		return false;
	}
	return true;
}


/* 创建带初始容量的 Buffer。 */
static xbuffer* __xrtGzipBufferCreate(size_t iCapacity)
{
	xbuffer* pBuffer = xrtBufferCreate();

	if ( (pBuffer != NULL) && !xrtBufferReserve(pBuffer, iCapacity) ) {
		xrtBufferDestroy(pBuffer);
		pBuffer = NULL;
	}
	return pBuffer;
}


/* 在已验证限额后追加输出。 */
static bool __xrtGzipAppend(xbuffer* pOutput, xbytesview Data,
	uint64 iInput, uint64* pOutputBytes, const xrtresourcelimits* pLimits,
	bool bCheckRatio, xrtgziperror* pError)
{
	if ( Data.Size == 0u ) { return true; }
	if ( !__xrtGzipOutputAllowed(pLimits, iInput, *pOutputBytes,
		Data.Size, bCheckRatio, pError) ) {
		return false;
	}
	if ( !xrtBufferAppend(pOutput, Data) ) {
		__xrtGzipSetError(pError, XRT_GZIP_ERROR_MEMORY, 0,
			iInput, *pOutputBytes, "cannot grow gzip output buffer");
		return false;
	}
	*pOutputBytes += (uint64)Data.Size;
	return true;
}


/* 更新 IEEE CRC32 状态，初值和结束异或均由调用方管理。 */
static uint32 __xrtGzipCrc32Update(uint32 iCrc, const void* pData, size_t iSize)
{
	const uint8* pBytes = (const uint8*)pData;
	size_t i;

	for ( i = 0u; i < iSize; ++i ) {
		int j;
		iCrc ^= (uint32)pBytes[i];
		for ( j = 0; j < 8; ++j ) {
			iCrc = (iCrc >> 1) ^ (UINT32_C(0xedb88320) &
				(UINT32_C(0) - (iCrc & UINT32_C(1))));
		}
	}
	return iCrc;
}


/* 按小端序写出 gzip 头尾字段。 */
static void __xrtGzipWriteU32(uint8* pOutput, uint32 iValue)
{
	pOutput[0] = (uint8)(iValue & UINT32_C(0xff));
	pOutput[1] = (uint8)((iValue >> 8) & UINT32_C(0xff));
	pOutput[2] = (uint8)((iValue >> 16) & UINT32_C(0xff));
	pOutput[3] = (uint8)((iValue >> 24) & UINT32_C(0xff));
}



XRT_API void xrtGzipOptionsInit(xrtgzipoptions* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_GZIP_OPTIONS_VERSION;
	pOptions->iLevel = -1;
	pOptions->iStrategy = XDEFLATE_STRATEGY_DEFAULT;
	xrtResourceLimitsInit(&pOptions->tLimits);
}


XRT_API void xrtGzipErrorInit(xrtgziperror* pError)
{
	if ( pError != NULL ) { memset(pError, 0, sizeof(*pError)); }
}



// ------------------------------------ 压缩器 ------------------------------------

/* 接收 XRT raw-deflate 输出并追加到 gzip 成员。 */
static bool __xrtGzipCompressOutput(xbytesview Data, ptr pData)
{
	xrtgzipcompressor* pCompressor = (xrtgzipcompressor*)pData;

	return __xrtGzipAppend(pCompressor->Output, Data,
		pCompressor->InputBytes, &pCompressor->OutputBytes,
		&pCompressor->Options.tLimits, false, &pCompressor->Error);
}


XRT_API xrtgzipcompressor* xrtGzipCompressorCreate(const xrtgzipoptions* pOptions)
{
	xrtgzipcompressor* pCompressor;
	xdeflateconfig Config;
	uint8 sHeader[XRT_GZIP_HEADER_SIZE] = {
		0x1fu, 0x8bu, 0x08u, 0x00u, 0u, 0u, 0u, 0u, 0u, 0xffu
	};
	int iLevel;

	pCompressor = (xrtgzipcompressor*)xrtMalloc(sizeof(*pCompressor));
	if ( pCompressor == NULL ) { return NULL; }
	memset(pCompressor, 0, sizeof(*pCompressor));
	if ( !__xrtGzipOptionsCopy(&pCompressor->Options, pOptions,
		&pCompressor->Error) ) {
		pCompressor->Failed = true;
		return pCompressor;
	}
	pCompressor->Output = __xrtGzipBufferCreate(XRT_GZIP_CHUNK);
	if ( pCompressor->Output == NULL ) {
		__xrtGzipSetError(&pCompressor->Error, XRT_GZIP_ERROR_MEMORY,
			0, 0, 0, "cannot create gzip output buffer");
		pCompressor->Failed = true;
		return pCompressor;
	}
	xrtDeflateConfigInit(&Config);
	Config.Format = XDEFLATE_RAW;
	Config.Level = pCompressor->Options.iLevel < 0 ?
		XDEFLATE_LEVEL_DEFAULT : pCompressor->Options.iLevel;
	Config.Strategy = (xdeflatestrategy)pCompressor->Options.iStrategy;
	if ( pCompressor->Options.tLimits.iMaxOutputBytes != 0u ) {
		if ( pCompressor->Options.tLimits.iMaxOutputBytes <
			(XRT_GZIP_HEADER_SIZE + XRT_GZIP_TRAILER_SIZE) ) {
			__xrtGzipSetError(&pCompressor->Error, XRT_GZIP_ERROR_LIMIT,
				0, 0, 0, "gzip output limit is too small");
			pCompressor->Failed = true;
			return pCompressor;
		}
		Config.OutputLimit = pCompressor->Options.tLimits.iMaxOutputBytes -
			XRT_GZIP_HEADER_SIZE - XRT_GZIP_TRAILER_SIZE;
	}
	pCompressor->Deflate = xrtDeflateCreate(&Config);
	if ( pCompressor->Deflate == NULL ) {
		__xrtGzipSetCodecError(&pCompressor->Error, XRT_GZIP_ERROR_MEMORY,
			0, 0, "cannot initialize gzip compressor");
		pCompressor->Failed = true;
		return pCompressor;
	}
	__xrtGzipWriteU32(sHeader + 4u, pCompressor->Options.iMTime);
	iLevel = Config.Level;
	sHeader[8] = (uint8)(iLevel >= 9 ? 2 : (iLevel == 1 ? 4 : 0));
	if ( !__xrtGzipAppend(pCompressor->Output,
		(xbytesview){ sHeader, sizeof(sHeader) }, 0u,
		&pCompressor->OutputBytes, &pCompressor->Options.tLimits,
		false, &pCompressor->Error) ) {
		pCompressor->Failed = true;
		return pCompressor;
	}
	pCompressor->Crc = UINT32_MAX;
	return pCompressor;
}


XRT_API bool xrtGzipCompressorUpdate(xrtgzipcompressor* pCompressor,
	const void* pData, size_t iSize)
{
	if ( (pCompressor == NULL) || pCompressor->Failed ||
		pCompressor->Finished || ((pData == NULL) && (iSize != 0u)) ) {
		return false;
	}
	if ( !__xrtGzipInputAllowed(&pCompressor->Options.tLimits,
		pCompressor->InputBytes, iSize, &pCompressor->Error,
		pCompressor->OutputBytes) ) {
		pCompressor->Failed = true;
		return false;
	}
	if ( (iSize != 0u) && !xrtDeflateWrite(pCompressor->Deflate,
		(xbytesview){ (const unsigned char*)pData, iSize },
		XDEFLATE_FLUSH_NONE, __xrtGzipCompressOutput, pCompressor) ) {
		__xrtGzipSetCodecError(&pCompressor->Error, XRT_GZIP_ERROR_DATA,
			pCompressor->InputBytes, pCompressor->OutputBytes,
			"gzip compression failed");
		pCompressor->Failed = true;
		return false;
	}
	pCompressor->Crc = __xrtGzipCrc32Update(pCompressor->Crc,
		pData, iSize);
	pCompressor->InputBytes += (uint64)iSize;
	return true;
}


XRT_API bool xrtGzipCompressorFinish(xrtgzipcompressor* pCompressor)
{
	uint8 sTrailer[XRT_GZIP_TRAILER_SIZE];

	if ( (pCompressor == NULL) || pCompressor->Failed ) { return false; }
	if ( pCompressor->Finished ) { return true; }
	if ( !xrtDeflateWrite(pCompressor->Deflate,
		(xbytesview){ NULL, 0u }, XDEFLATE_FLUSH_FINISH,
		__xrtGzipCompressOutput, pCompressor) ) {
		__xrtGzipSetCodecError(&pCompressor->Error, XRT_GZIP_ERROR_DATA,
			pCompressor->InputBytes, pCompressor->OutputBytes,
			"cannot finish gzip compressor");
		pCompressor->Failed = true;
		return false;
	}
	__xrtGzipWriteU32(sTrailer, pCompressor->Crc ^ UINT32_MAX);
	__xrtGzipWriteU32(sTrailer + 4u, (uint32)pCompressor->InputBytes);
	if ( !__xrtGzipAppend(pCompressor->Output,
		(xbytesview){ sTrailer, sizeof(sTrailer) },
		pCompressor->InputBytes, &pCompressor->OutputBytes,
		&pCompressor->Options.tLimits, false, &pCompressor->Error) ) {
		pCompressor->Failed = true;
		return false;
	}
	pCompressor->Finished = true;
	return true;
}


XRT_API bool xrtGzipCompressorDrain(xrtgzipcompressor* pCompressor,
	xwriter* pWriter)
{
	xbytesview Output;

	if ( (pCompressor == NULL) || pCompressor->Failed ||
		(pWriter == NULL) ) {
		return false;
	}
	Output = xrtBufferView(pCompressor->Output);
	if ( (Output.Size != 0u) &&
		!xrtWriterWriteFull(pWriter, Output.Data, Output.Size, NULL) ) {
		__xrtGzipSetError(&pCompressor->Error, XRT_GZIP_ERROR_IO, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes,
			"cannot write gzip output stream");
		pCompressor->Failed = true;
		return false;
	}
	xrtBufferClear(pCompressor->Output);
	return true;
}


XRT_API xbuffer* xrtGzipCompressorTakeBuffer(xrtgzipcompressor* pCompressor)
{
	xbuffer* pOutput;

	if ( (pCompressor == NULL) || pCompressor->Failed ||
		!pCompressor->Finished ) {
		return NULL;
	}
	pOutput = pCompressor->Output;
	pCompressor->Output = NULL;
	return pOutput;
}


XRT_API const xrtgziperror* xrtGzipCompressorError(
	const xrtgzipcompressor* pCompressor)
{
	return pCompressor != NULL ? &pCompressor->Error : NULL;
}


XRT_API void xrtGzipCompressorDestroy(xrtgzipcompressor* pCompressor)
{
	if ( pCompressor == NULL ) { return; }
	xrtDeflateDestroy(pCompressor->Deflate);
	xrtBufferDestroy(pCompressor->Output);
	xrtFree(pCompressor);
}



// ------------------------------------ 解压器 ------------------------------------

/* 接收 XRT gzip-inflate 输出并执行总量与倍率限制。 */
static bool __xrtGzipDecompressOutput(xbytesview Data, ptr pData)
{
	xrtgzipdecompressor* pDecompressor = (xrtgzipdecompressor*)pData;

	return __xrtGzipAppend(pDecompressor->Output, Data,
		pDecompressor->InputBytes, &pDecompressor->OutputBytes,
		&pDecompressor->Options.tLimits, true, &pDecompressor->Error);
}


XRT_API xrtgzipdecompressor* xrtGzipDecompressorCreate(
	const xrtgzipoptions* pOptions)
{
	xrtgzipdecompressor* pDecompressor;
	xinflateconfig Config;

	pDecompressor = (xrtgzipdecompressor*)xrtMalloc(sizeof(*pDecompressor));
	if ( pDecompressor == NULL ) { return NULL; }
	memset(pDecompressor, 0, sizeof(*pDecompressor));
	if ( !__xrtGzipOptionsCopy(&pDecompressor->Options, pOptions,
		&pDecompressor->Error) ) {
		pDecompressor->Failed = true;
		return pDecompressor;
	}
	pDecompressor->Output = __xrtGzipBufferCreate(XRT_GZIP_CHUNK);
	if ( pDecompressor->Output == NULL ) {
		__xrtGzipSetError(&pDecompressor->Error, XRT_GZIP_ERROR_MEMORY,
			0, 0, 0, "cannot create gzip output buffer");
		pDecompressor->Failed = true;
		return pDecompressor;
	}
	xrtInflateConfigInit(&Config);
	Config.Format = XINFLATE_GZIP;
	Config.OutputLimit = pDecompressor->Options.tLimits.iMaxOutputBytes != 0u ?
		pDecompressor->Options.tLimits.iMaxOutputBytes :
		XINFLATE_OUTPUT_UNLIMITED;
	pDecompressor->Inflate = xrtInflateCreate(&Config);
	if ( pDecompressor->Inflate == NULL ) {
		__xrtGzipSetCodecError(&pDecompressor->Error,
			XRT_GZIP_ERROR_MEMORY, 0, 0,
			"cannot initialize gzip decompressor");
		pDecompressor->Failed = true;
	}
	return pDecompressor;
}


XRT_API bool xrtGzipDecompressorUpdate(xrtgzipdecompressor* pDecompressor,
	const void* pData, size_t iSize)
{
	if ( (pDecompressor == NULL) || pDecompressor->Failed ||
		pDecompressor->Finished || ((pData == NULL) && (iSize != 0u)) ) {
		return false;
	}
	if ( !__xrtGzipInputAllowed(&pDecompressor->Options.tLimits,
		pDecompressor->InputBytes, iSize, &pDecompressor->Error,
		pDecompressor->OutputBytes) ) {
		pDecompressor->Failed = true;
		return false;
	}
	pDecompressor->InputBytes += (uint64)iSize;
	if ( (iSize != 0u) && !xrtInflateWrite(pDecompressor->Inflate,
		(xbytesview){ (const unsigned char*)pData, iSize }, false,
		__xrtGzipDecompressOutput, pDecompressor) ) {
		__xrtGzipSetCodecError(&pDecompressor->Error, XRT_GZIP_ERROR_DATA,
			pDecompressor->InputBytes, pDecompressor->OutputBytes,
			"gzip data is invalid");
		pDecompressor->Failed = true;
		return false;
	}
	return true;
}


XRT_API bool xrtGzipDecompressorFinish(xrtgzipdecompressor* pDecompressor)
{
	if ( (pDecompressor == NULL) || pDecompressor->Failed ) { return false; }
	if ( pDecompressor->Finished ) { return true; }
	if ( !xrtInflateWrite(pDecompressor->Inflate,
		(xbytesview){ NULL, 0u }, true,
		__xrtGzipDecompressOutput, pDecompressor) ) {
		__xrtGzipSetCodecError(&pDecompressor->Error,
			XRT_GZIP_ERROR_TRUNCATED, pDecompressor->InputBytes,
			pDecompressor->OutputBytes, "gzip stream is truncated");
		pDecompressor->Failed = true;
		return false;
	}
	pDecompressor->Finished = true;
	return true;
}


XRT_API bool xrtGzipDecompressorDrain(xrtgzipdecompressor* pDecompressor,
	xwriter* pWriter)
{
	xbytesview Output;

	if ( (pDecompressor == NULL) || pDecompressor->Failed ||
		(pWriter == NULL) ) {
		return false;
	}
	Output = xrtBufferView(pDecompressor->Output);
	if ( (Output.Size != 0u) &&
		!xrtWriterWriteFull(pWriter, Output.Data, Output.Size, NULL) ) {
		__xrtGzipSetError(&pDecompressor->Error, XRT_GZIP_ERROR_IO, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes,
			"cannot write decompressed stream");
		pDecompressor->Failed = true;
		return false;
	}
	xrtBufferClear(pDecompressor->Output);
	return true;
}


XRT_API xbuffer* xrtGzipDecompressorTakeBuffer(
	xrtgzipdecompressor* pDecompressor)
{
	xbuffer* pOutput;

	if ( (pDecompressor == NULL) || pDecompressor->Failed ||
		!pDecompressor->Finished ) {
		return NULL;
	}
	pOutput = pDecompressor->Output;
	pDecompressor->Output = NULL;
	return pOutput;
}


XRT_API const xrtgziperror* xrtGzipDecompressorError(
	const xrtgzipdecompressor* pDecompressor)
{
	return pDecompressor != NULL ? &pDecompressor->Error : NULL;
}


XRT_API void xrtGzipDecompressorDestroy(xrtgzipdecompressor* pDecompressor)
{
	if ( pDecompressor == NULL ) { return; }
	xrtInflateDestroy(pDecompressor->Inflate);
	xrtBufferDestroy(pDecompressor->Output);
	xrtFree(pDecompressor);
}



// ------------------------------------ 便捷与流式接口 ------------------------------------

XRT_API bool xrtGzipCompress(const void* pData, size_t iSize,
	const xrtgzipoptions* pOptions, xbuffer** ppOutput,
	xrtgziperror* pError)
{
	xrtgzipcompressor* pCompressor;
	bool bResult;

	if ( ppOutput != NULL ) { *ppOutput = NULL; }
	xrtGzipErrorInit(pError);
	if ( (ppOutput == NULL) || ((pData == NULL) && (iSize != 0u)) ) {
		__xrtGzipSetError(pError, XRT_GZIP_ERROR_ARGUMENT, 0,
			0, 0, "invalid gzip input");
		return false;
	}
	pCompressor = xrtGzipCompressorCreate(pOptions);
	if ( pCompressor == NULL ) {
		__xrtGzipSetError(pError, XRT_GZIP_ERROR_MEMORY, 0,
			0, 0, "cannot allocate gzip compressor");
		return false;
	}
	bResult = xrtGzipCompressorUpdate(pCompressor, pData, iSize) &&
		xrtGzipCompressorFinish(pCompressor);
	if ( bResult ) {
		*ppOutput = xrtGzipCompressorTakeBuffer(pCompressor);
		bResult = *ppOutput != NULL;
	}
	if ( !bResult ) {
		__xrtGzipCopyError(pError, &pCompressor->Error);
	}
	xrtGzipCompressorDestroy(pCompressor);
	return bResult;
}


XRT_API bool xrtGzipDecompress(const void* pData, size_t iSize,
	const xrtgzipoptions* pOptions, xbuffer** ppOutput,
	xrtgziperror* pError)
{
	xrtgzipdecompressor* pDecompressor;
	bool bResult;

	if ( ppOutput != NULL ) { *ppOutput = NULL; }
	xrtGzipErrorInit(pError);
	if ( (ppOutput == NULL) || ((pData == NULL) && (iSize != 0u)) ) {
		__xrtGzipSetError(pError, XRT_GZIP_ERROR_ARGUMENT, 0,
			0, 0, "invalid gzip input");
		return false;
	}
	pDecompressor = xrtGzipDecompressorCreate(pOptions);
	if ( pDecompressor == NULL ) {
		__xrtGzipSetError(pError, XRT_GZIP_ERROR_MEMORY, 0,
			0, 0, "cannot allocate gzip decompressor");
		return false;
	}
	bResult = xrtGzipDecompressorUpdate(pDecompressor, pData, iSize) &&
		xrtGzipDecompressorFinish(pDecompressor);
	if ( bResult ) {
		*ppOutput = xrtGzipDecompressorTakeBuffer(pDecompressor);
		bResult = *ppOutput != NULL;
	}
	if ( !bResult ) {
		__xrtGzipCopyError(pError, &pDecompressor->Error);
	}
	xrtGzipDecompressorDestroy(pDecompressor);
	return bResult;
}


XRT_API bool xrtGzipCompressStream(xreader* pReader, xwriter* pWriter,
	const xrtgzipoptions* pOptions, xrtgziperror* pError)
{
	uint8 sInput[XRT_GZIP_CHUNK];
	xrtgzipcompressor* pCompressor;
	bool bResult = true;

	xrtGzipErrorInit(pError);
	if ( (pReader == NULL) || (pWriter == NULL) ) {
		__xrtGzipSetError(pError, XRT_GZIP_ERROR_ARGUMENT, 0,
			0, 0, "invalid gzip stream");
		return false;
	}
	pCompressor = xrtGzipCompressorCreate(pOptions);
	if ( pCompressor == NULL ) { return false; }
	while ( bResult ) {
		size_t iRead = 0u;

		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtGzipSetError(&pCompressor->Error, XRT_GZIP_ERROR_IO,
				0, pCompressor->InputBytes, pCompressor->OutputBytes,
				"cannot read gzip input stream");
			bResult = false;
			break;
		}
		if ( iRead == 0u ) { break; }
		bResult = xrtGzipCompressorUpdate(pCompressor, sInput, iRead) &&
			xrtGzipCompressorDrain(pCompressor, pWriter);
	}
	if ( bResult ) {
		bResult = xrtGzipCompressorFinish(pCompressor) &&
			xrtGzipCompressorDrain(pCompressor, pWriter);
	}
	if ( !bResult ) {
		__xrtGzipCopyError(pError, &pCompressor->Error);
	}
	xrtGzipCompressorDestroy(pCompressor);
	return bResult;
}


XRT_API bool xrtGzipDecompressStream(xreader* pReader, xwriter* pWriter,
	const xrtgzipoptions* pOptions, xrtgziperror* pError)
{
	uint8 sInput[XRT_GZIP_CHUNK];
	xrtgzipdecompressor* pDecompressor;
	bool bResult = true;

	xrtGzipErrorInit(pError);
	if ( (pReader == NULL) || (pWriter == NULL) ) {
		__xrtGzipSetError(pError, XRT_GZIP_ERROR_ARGUMENT, 0,
			0, 0, "invalid gzip stream");
		return false;
	}
	pDecompressor = xrtGzipDecompressorCreate(pOptions);
	if ( pDecompressor == NULL ) { return false; }
	while ( bResult ) {
		size_t iRead = 0u;

		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtGzipSetError(&pDecompressor->Error, XRT_GZIP_ERROR_IO,
				0, pDecompressor->InputBytes, pDecompressor->OutputBytes,
				"cannot read gzip input stream");
			bResult = false;
			break;
		}
		if ( iRead == 0u ) { break; }
		bResult = xrtGzipDecompressorUpdate(pDecompressor, sInput, iRead) &&
			xrtGzipDecompressorDrain(pDecompressor, pWriter);
	}
	if ( bResult ) {
		bResult = xrtGzipDecompressorFinish(pDecompressor) &&
			xrtGzipDecompressorDrain(pDecompressor, pWriter);
	}
	if ( !bResult ) {
		__xrtGzipCopyError(pError, &pDecompressor->Error);
	}
	xrtGzipDecompressorDestroy(pDecompressor);
	return bResult;
}
