#include "include/xrt_miniz.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>



/* 单个块的最大原始字节数。miniz 一次性 API 以 mz_ulong 传递长度，
 * 这里取 1MB 以便流式产出粒度适中、内存占用可控。 */
#define XRT_MINIZ_BLOCK_SIZE 1048576u
#define XRT_MINIZ_CHUNK 65536u
#define XRT_MINIZ_HEADER_SIZE 12u
/* miniz 一次性 API 的长度上限（与 mz_ulong 安全范围保持一致）。 */
#define XRT_MINIZ_MAX_PLAIN 0x7e000000u

/* 容器格式（自定义多块成员流，支持串联多成员）：
 *   成员 = 块* + 结束标记
 *   块   = u32le(原始大小 n, n>0) + u32le(压缩大小 c) + u32le(原始数据 CRC32) + zlib 压缩字节(c 字节)
 *   结束 = u32le(0)
 * 压缩块实际长度通常小于 mz_compressBound(n)，故必须显式记录 c；
 * zlib 流自带 Adler32，但仍额外记录原始数据 CRC32 以双重检出损坏，
 * 解压侧据此精确定位块边界、校验完整性并统计 members。 */

struct xrtminizcompressor {
	xbuffer* Pending;            /* 尚未成块的原始输入 */
	xbuffer* Output;             /* 已产出的压缩字节 */
	xrtminizoptions Options;
	xrtminizerror Error;
	uint64 InputBytes;
	uint64 OutputBytes;
	bool Finished;
	bool Failed;
};

typedef enum {
	XRT_MINIZ_DEC_HEADER = 0,   /* 读取 12 字节块头 */
	XRT_MINIZ_DEC_BLOCK = 1     /* 读取压缩块体 */
} xrtminizdecstate;

struct xrtminizdecompressor {
	xbuffer* Carry;              /* 尚未消费完的输入字节 */
	xbuffer* Output;             /* 已产出的原始字节 */
	xrtminizoptions Options;
	xrtminizerror Error;
	uint64 InputBytes;
	uint64 OutputBytes;
	uint64 MemberCount;
	size_t BlockDeclared;       /* 当前块声明的原始大小 */
	size_t BlockCompressed;     /* 当前块声明的压缩大小 */
	uint32 BlockCrc;            /* 当前块声明的原始数据 CRC32 */
	int State;
	bool MemberEnded;
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

static void __xrtMinizWriteU32LE(uint8* pDst, uint32 iValue)
{
	pDst[0] = (uint8)(iValue & 0xffu);
	pDst[1] = (uint8)((iValue >> 8) & 0xffu);
	pDst[2] = (uint8)((iValue >> 16) & 0xffu);
	pDst[3] = (uint8)((iValue >> 24) & 0xffu);
}

static uint32 __xrtMinizReadU32LE(const uint8* pSrc)
{
	return (uint32)pSrc[0] | ((uint32)pSrc[1] << 8) |
		((uint32)pSrc[2] << 16) | ((uint32)pSrc[3] << 24);
}

/* 标准 CRC32（IEEE 802.3，多项式 0xEDB88320，反射输入输出，异或 0xFFFFFFFF）。
 * 逐位计算，无需查表，仅用于块级完整性校验，性能需求不高。 */
static uint32 __xrtMinizCrc32(const void* pData, size_t iLen)
{
	const uint8* pBuf = (const uint8*)pData;
	uint32 iCrc = 0xFFFFFFFFu;
	size_t i;
	for ( i = 0u; i < iLen; ++i ) {
		int j;
		iCrc ^= (uint32)pBuf[i];
		for ( j = 0; j < 8; ++j ) {
			iCrc = (iCrc >> 1) ^ (0xEDB88320u & (0u - (iCrc & 1u)));
		}
	}
	return iCrc ^ 0xFFFFFFFFu;
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
}



// ------------------------------------ 压缩器 ------------------------------------

XRT_API xrtminizcompressor* xrtMinizCompressorCreate(const xrtminizoptions* pOptions)
{
	xrtminizcompressor* pCompressor;
	pCompressor = (xrtminizcompressor*)xrtMalloc(sizeof(xrtminizcompressor));
	if ( pCompressor == NULL ) { return NULL; }
	memset(pCompressor, 0, sizeof(*pCompressor));
	xrtMinizErrorInit(&pCompressor->Error);
	if ( pOptions != NULL ) {
		pCompressor->Options = *pOptions;
	} else {
		xrtMinizOptionsInit(&pCompressor->Options);
	}
	pCompressor->Pending = __xrtMinizBufferCreate(XRT_MINIZ_CHUNK);
	pCompressor->Output = __xrtMinizBufferCreate(XRT_MINIZ_CHUNK);
	if ( pCompressor->Pending == NULL || pCompressor->Output == NULL ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_MEMORY, 0, 0, 0,
			"cannot allocate miniz compressor");
		pCompressor->Failed = true;
		return pCompressor;
	}
	return pCompressor;
}

/* 将 Pending 中恰好 iSize 字节压缩为一个块（大小头 + 压缩体）写入 Output。 */
static bool __xrtMinizFlushBlock(xrtminizcompressor* pCompressor, size_t iSize)
{
	xdeflateconfig Config;
	uint8 sHeader[XRT_MINIZ_HEADER_SIZE];
	bytes pCompressed;
	xbytesview Pending;
	size_t iWritten = 0u;
	size_t iGenerated;
	if ( iSize == 0u ) { return true; }
	Pending = xrtBufferView(pCompressor->Pending);
	if ( iSize > Pending.Size ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_STATE, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "invalid miniz pending state");
		pCompressor->Failed = true;
		return false;
	}
	if ( iSize > XRT_MINIZ_MAX_PLAIN ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_ARGUMENT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "miniz block too large");
		pCompressor->Failed = true;
		return false;
	}
	if ( __xrtMinizAddOverLimit(pCompressor->OutputBytes, XRT_MINIZ_HEADER_SIZE,
		pCompressor->Options.tLimits.iMaxOutputBytes) ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_LIMIT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "miniz output exceeds maxOutput limit");
		pCompressor->Failed = true;
		return false;
	}
	xrtDeflateConfigInit(&Config);
	Config.Format = XDEFLATE_ZLIB;
	Config.Level = pCompressor->Options.iLevel;
	if ( pCompressor->Options.tLimits.iMaxOutputBytes != 0u ) {
		Config.OutputLimit = pCompressor->Options.tLimits.iMaxOutputBytes -
			pCompressor->OutputBytes - XRT_MINIZ_HEADER_SIZE;
	}
	pCompressed = xrtDeflateAll(
		(xbytesview){ Pending.Data, iSize },
		&Config,
		&iWritten
	);
	if ( pCompressed == NULL ) {
		__xrtMinizSetCodecError(&pCompressor->Error, XRT_MINIZ_ERROR_DATA,
			pCompressor->InputBytes, pCompressor->OutputBytes, "miniz block compression failed");
		pCompressor->Failed = true;
		return false;
	}
	if ( iWritten > UINT32_MAX ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_DATA, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "miniz compressed block is too large");
		xrtFree(pCompressed);
		pCompressor->Failed = true;
		return false;
	}
	__xrtMinizWriteU32LE(sHeader, (uint32)iSize);
	__xrtMinizWriteU32LE(sHeader + 4u, (uint32)iWritten);
	__xrtMinizWriteU32LE(sHeader + 8u,
		__xrtMinizCrc32(Pending.Data, iSize));
	iGenerated = XRT_MINIZ_HEADER_SIZE + (size_t)iWritten;
	if ( __xrtMinizAddOverLimit(pCompressor->OutputBytes, iGenerated,
		pCompressor->Options.tLimits.iMaxOutputBytes) ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_LIMIT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "miniz output exceeds maxOutput limit");
		xrtFree(pCompressed);
		pCompressor->Failed = true;
		return false;
	}
	if ( !xrtBufferAppend(pCompressor->Output,
			(xbytesview){ (const unsigned char*)sHeader, XRT_MINIZ_HEADER_SIZE }) ||
		 !xrtBufferAppend(pCompressor->Output,
			(xbytesview){ (const unsigned char*)pCompressed, (size_t)iWritten }) ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow miniz output buffer");
		xrtFree(pCompressed);
		pCompressor->Failed = true;
		return false;
	}
	xrtFree(pCompressed);
	pCompressor->OutputBytes += (uint64)iGenerated;
	if ( !xrtBufferRemove(pCompressor->Pending, 0u, iSize) ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_STATE, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot consume miniz pending buffer");
		pCompressor->Failed = true;
		return false;
	}
	return true;
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
	if ( iSize != 0u &&
		!xrtBufferAppend(pCompressor->Pending,
			(xbytesview){ (const unsigned char*)pData, iSize }) ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow miniz pending buffer");
		pCompressor->Failed = true;
		return false;
	}
	pCompressor->InputBytes += iSize;
	while ( xrtBufferView(pCompressor->Pending).Size >= XRT_MINIZ_BLOCK_SIZE ) {
		if ( !__xrtMinizFlushBlock(pCompressor, XRT_MINIZ_BLOCK_SIZE) ) { return false; }
	}
	return true;
}

XRT_API bool xrtMinizCompressorFinish(xrtminizcompressor* pCompressor)
{
	uint8 sEnd[XRT_MINIZ_HEADER_SIZE] = { 0 };
	size_t iPending;
	if ( pCompressor == NULL || pCompressor->Failed ) { return false; }
	if ( pCompressor->Finished ) { return true; }
	iPending = xrtBufferView(pCompressor->Pending).Size;
	if ( iPending != 0u ) {
		if ( !__xrtMinizFlushBlock(pCompressor, iPending) ) { return false; }
	}
	if ( __xrtMinizAddOverLimit(pCompressor->OutputBytes, XRT_MINIZ_HEADER_SIZE,
		pCompressor->Options.tLimits.iMaxOutputBytes) ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_LIMIT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "miniz output exceeds maxOutput limit");
		pCompressor->Failed = true;
		return false;
	}
	if ( !xrtBufferAppend(pCompressor->Output,
		(xbytesview){ (const unsigned char*)sEnd, sizeof(sEnd) }) ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow miniz output buffer");
		pCompressor->Failed = true;
		return false;
	}
	pCompressor->OutputBytes += XRT_MINIZ_HEADER_SIZE;
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
	xrtBufferDestroy(pCompressor->Pending);
	xrtBufferDestroy(pCompressor->Output);
	xrtFree(pCompressor);
}



// ------------------------------------ 解压器 ------------------------------------

XRT_API xrtminizdecompressor* xrtMinizDecompressorCreate(const xrtminizoptions* pOptions)
{
	xrtminizdecompressor* pDecompressor;
	pDecompressor = (xrtminizdecompressor*)xrtMalloc(sizeof(xrtminizdecompressor));
	if ( pDecompressor == NULL ) { return NULL; }
	memset(pDecompressor, 0, sizeof(*pDecompressor));
	xrtMinizErrorInit(&pDecompressor->Error);
	if ( pOptions != NULL ) {
		pDecompressor->Options = *pOptions;
	} else {
		xrtMinizOptionsInit(&pDecompressor->Options);
	}
	pDecompressor->Carry = __xrtMinizBufferCreate(XRT_MINIZ_CHUNK);
	pDecompressor->Output = __xrtMinizBufferCreate(XRT_MINIZ_CHUNK);
	if ( pDecompressor->Carry == NULL || pDecompressor->Output == NULL ) {
		__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_MEMORY, 0, 0, 0,
			"cannot allocate miniz decompressor");
		pDecompressor->Failed = true;
		return pDecompressor;
	}
	pDecompressor->State = XRT_MINIZ_DEC_HEADER;
	return pDecompressor;
}

/* 消费 Carry 中的输入；*pConsumed 返回实际消费字节数。 */
static bool __xrtMinizDecompressConsume(xrtminizdecompressor* pDecompressor, size_t* pConsumed)
{
	size_t iPos = 0u;
	xbytesview Carry = xrtBufferView(pDecompressor->Carry);
	const uint8* pBuf = (const uint8*)Carry.Data;
	size_t iLen = Carry.Size;
	while ( iPos < iLen ) {
		if ( pDecompressor->State == XRT_MINIZ_DEC_HEADER ) {
			uint32 iDeclared;
			uint32 iCompressed;
			if ( iLen - iPos < XRT_MINIZ_HEADER_SIZE ) { break; }
			iDeclared = __xrtMinizReadU32LE(pBuf + iPos);
			iCompressed = __xrtMinizReadU32LE(pBuf + iPos + 4u);
			iPos += XRT_MINIZ_HEADER_SIZE;
			if ( iDeclared == 0u ) {
				pDecompressor->MemberEnded = true;
				pDecompressor->MemberCount += 1u;
				continue;   /* 可能紧跟下一个成员 */
			}
			pDecompressor->MemberEnded = false;
			pDecompressor->BlockDeclared = (size_t)iDeclared;
			pDecompressor->BlockCompressed = (size_t)iCompressed;
			pDecompressor->BlockCrc = __xrtMinizReadU32LE(pBuf + iPos - 4u);
			pDecompressor->State = XRT_MINIZ_DEC_BLOCK;
		} else {
			xinflateconfig Config;
			bytes pPlain;
			size_t iWritten = 0u;
			if ( (pDecompressor->BlockDeclared > XRT_MINIZ_MAX_PLAIN) ||
				 (pDecompressor->BlockCompressed == 0u) ) {
				__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_DATA, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"miniz block declares too large output");
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			if ( iLen - iPos < pDecompressor->BlockCompressed ) { break; }
			if ( ((pDecompressor->Options.tLimits.iMaxItemBytes != 0u) &&
				  (pDecompressor->BlockDeclared > pDecompressor->Options.tLimits.iMaxItemBytes)) ||
				 __xrtMinizAddOverLimit(pDecompressor->OutputBytes,
					pDecompressor->BlockDeclared,
					pDecompressor->Options.tLimits.iMaxOutputBytes) ||
				 __xrtMinizOverRatio(&pDecompressor->Options.tLimits,
					pDecompressor->InputBytes,
					pDecompressor->OutputBytes + (uint64)pDecompressor->BlockDeclared) ) {
				__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_LIMIT, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"miniz output exceeds decompression limit");
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			xrtInflateConfigInit(&Config);
			Config.Format = XINFLATE_ZLIB;
			Config.OutputLimit = (uint64)pDecompressor->BlockDeclared;
			pPlain = xrtInflateAll(
				(xbytesview){ pBuf + iPos, pDecompressor->BlockCompressed },
				&Config,
				&iWritten
			);
			if ( pPlain == NULL ) {
				__xrtMinizSetCodecError(&pDecompressor->Error, XRT_MINIZ_ERROR_DATA,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"miniz block decompression failed");
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			if ( iWritten != pDecompressor->BlockDeclared ) {
				__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_DATA, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"miniz decompression size mismatch");
				xrtFree(pPlain);
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			iPos += pDecompressor->BlockCompressed;
			if ( __xrtMinizCrc32(pPlain, (size_t)iWritten) != pDecompressor->BlockCrc ) {
				__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_DATA, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"miniz block checksum mismatch");
				xrtFree(pPlain);
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			if ( !xrtBufferAppend(pDecompressor->Output,
				(xbytesview){ (const unsigned char*)pPlain, (size_t)iWritten }) ) {
				__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_MEMORY, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"cannot grow miniz output buffer");
				xrtFree(pPlain);
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			xrtFree(pPlain);
			pDecompressor->OutputBytes += (uint64)iWritten;
			pDecompressor->State = XRT_MINIZ_DEC_HEADER;
		}
	}
	*pConsumed = iPos;
	return true;
}

XRT_API bool xrtMinizDecompressorUpdate(xrtminizdecompressor* pDecompressor, const void* pData, size_t iSize)
{
	size_t iConsumed = 0u;
	if ( pDecompressor == NULL || pDecompressor->Failed ) { return false; }
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
	if ( iSize != 0u &&
		!xrtBufferAppend(pDecompressor->Carry,
			(xbytesview){ (const unsigned char*)pData, iSize }) ) {
		__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_MEMORY, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot grow miniz input buffer");
		pDecompressor->Failed = true;
		return false;
	}
	pDecompressor->InputBytes += iSize;
	if ( !__xrtMinizDecompressConsume(pDecompressor, &iConsumed) ) { return false; }
	if ( (iConsumed != 0u) &&
		 !xrtBufferRemove(pDecompressor->Carry, 0u, iConsumed) ) {
		__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_STATE, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot consume miniz input buffer");
		pDecompressor->Failed = true;
		return false;
	}
	return true;
}

XRT_API bool xrtMinizDecompressorFinish(xrtminizdecompressor* pDecompressor)
{
	if ( pDecompressor == NULL || pDecompressor->Failed ) { return false; }
	if ( pDecompressor->Finished ) { return true; }
	if ( (pDecompressor->MemberCount == 0u) || !pDecompressor->MemberEnded ||
		(pDecompressor->State != XRT_MINIZ_DEC_HEADER) ||
		(xrtBufferView(pDecompressor->Carry).Size != 0u) ) {
		__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_TRUNCATED, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "miniz stream is truncated or empty");
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
	return pDecompressor != NULL && pDecompressor->MemberEnded;
}

XRT_API uint64 xrtMinizDecompressorMembers(const xrtminizdecompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->MemberCount : 0u;
}

XRT_API const xrtminizerror* xrtMinizDecompressorError(const xrtminizdecompressor* pDecompressor)
{
	return pDecompressor != NULL ? &pDecompressor->Error : NULL;
}

XRT_API void xrtMinizDecompressorDestroy(xrtminizdecompressor* pDecompressor)
{
	if ( pDecompressor == NULL ) { return; }
	xrtBufferDestroy(pDecompressor->Carry);
	xrtBufferDestroy(pDecompressor->Output);
	xrtFree(pDecompressor);
}



// ------------------------------------ 一次性接口 ------------------------------------

XRT_API bool xrtMinizCompress(const void* pData, size_t iSize, const xrtminizoptions* pOptions,
	xbuffer** ppOutput, xrtminizerror* pError)
{
	xrtminizoptions tOptions;
	xrtminizcompressor* pCompressor;
	bool bRet;
	xrtMinizErrorInit(pError);
	if ( ppOutput != NULL ) { *ppOutput = NULL; }
	if ( ppOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtMinizSetError(pError, XRT_MINIZ_ERROR_ARGUMENT, 0, 0, 0, "invalid miniz input");
		return false;
	}
	if ( pOptions != NULL ) { tOptions = *pOptions; } else { xrtMinizOptionsInit(&tOptions); }
	pCompressor = xrtMinizCompressorCreate(&tOptions);
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
	xrtminizoptions tOptions;
	xrtminizdecompressor* pDecompressor;
	bool bRet;
	xrtMinizErrorInit(pError);
	if ( ppOutput != NULL ) { *ppOutput = NULL; }
	if ( ppOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtMinizSetError(pError, XRT_MINIZ_ERROR_ARGUMENT, 0, 0, 0, "invalid miniz input");
		return false;
	}
	if ( pOptions != NULL ) { tOptions = *pOptions; } else { xrtMinizOptionsInit(&tOptions); }
	pDecompressor = xrtMinizDecompressorCreate(&tOptions);
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
	bTotalKnown = xrtReaderCanSeek(pReader) && xrtReaderSize(pReader, &iTotal);
	if ( !bTotalKnown ) { xrtClearError(); }
	pCompressor = xrtMinizCompressorCreate(pOptions);
	if ( pCompressor == NULL ) { return false; }
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
	bTotalKnown = xrtReaderCanSeek(pReader) && xrtReaderSize(pReader, &iTotal);
	if ( !bTotalKnown ) { xrtClearError(); }
	pDecompressor = xrtMinizDecompressorCreate(pOptions);
	if ( pDecompressor == NULL ) { return false; }
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
