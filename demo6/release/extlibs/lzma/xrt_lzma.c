#include "include/xrt_lzma.h"
#include "third_party/Alloc.h"
#include "third_party/LzmaEnc.h"
#include "third_party/LzmaDec.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>



/* 单个块最大原始字节数。LZMA 字典上限为 1<<27(64位) / 1<<29，这里取 1MB
 * 以便流式产出粒度适中、字典内存占用可控。 */
#define XRT_LZMA_BLOCK_SIZE 1048576u
#define XRT_LZMA_CHUNK 65536u
/* 块头 = u32le(原始大小) + u32le(压缩大小) + u32le(原始数据CRC32) + 5字节LZMA属性。 */
#define XRT_LZMA_HEADER_SIZE 17u

/* 容器格式（自定义多块成员流，支持串联多成员）：
 *   成员 = 块* + 结束标记
 *   块   = u32le(原始大小 n, n>0) + u32le(压缩大小 c) + u32le(原始数据 CRC32)
 *          + 5字节 LZMA 属性 + LZMA 压缩字节(c 字节)
 *   结束 = u32le(0)
 * LZMA 一次性块 API 产出的压缩长度不固定，故必须显式记录 c；属性逐块存储；
 * 额外记录原始数据 CRC32 以可靠检出损坏。解压侧据此精确定位块边界、校验完整性
 * 并统计 members。 */

struct xrtlzmacompressor {
	xbuffer* Pending;            /* 尚未成块的原始输入 */
	xbuffer* Output;             /* 已产出的压缩字节 */
	xrtlzmaoptions Options;
	xrtlzmaerror Error;
	uint64 InputBytes;
	uint64 OutputBytes;
	bool Finished;
	bool Failed;
};

typedef enum {
	XRT_LZMA_DEC_HEADER = 0,    /* 读取块头 */
	XRT_LZMA_DEC_BLOCK = 1      /* 读取压缩块体 */
} xrtlzmadecstate;

struct xrtlzmadecompressor {
	xbuffer* Carry;              /* 尚未消费完的输入字节 */
	xbuffer* Output;             /* 已产出的原始字节 */
	xrtlzmaoptions Options;
	xrtlzmaerror Error;
	uint64 InputBytes;
	uint64 OutputBytes;
	uint64 MemberCount;
	size_t BlockDeclared;       /* 当前块声明的原始大小 */
	size_t BlockCompressed;     /* 当前块声明的压缩大小 */
	uint32 BlockCrc;            /* 当前块声明的原始数据 CRC32 */
	uint8 sBlockProps[LZMA_PROPS_SIZE]; /* 当前块的 LZMA 属性 */
	int State;
	bool MemberEnded;
	bool Finished;
	bool Failed;
};



static void __xrtLzmaSetError(xrtlzmaerror* pError, int iCode, int iBackend,
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

static void __xrtLzmaCopyError(xrtlzmaerror* pDst, const xrtlzmaerror* pSrc)
{
	if ( pDst == NULL || pSrc == NULL ) { return; }
	*pDst = *pSrc;
}

static bool __xrtLzmaOverRatio(const xrtresourcelimits* pLimits, uint64 iInput, uint64 iOutput)
{
	uint64 iRatio = pLimits->iMaxCompressionRatio;
	if ( iRatio == 0u || iInput == 0u ) { return false; }
	if ( iInput > (UINT64_MAX / iRatio) ) { return false; }
	return iOutput > iInput * iRatio;
}

/* 检查计数累加是否溢出或越过非零硬上限。 */
static bool __xrtLzmaAddOverLimit(uint64 iCurrent, size_t iAdded, uint64 iLimit)
{
	uint64 iIncrement = (uint64)iAdded;

	if ( iIncrement > (UINT64_MAX - iCurrent) ) { return true; }
	return (iLimit != 0u) && ((iCurrent + iIncrement) > iLimit);
}

/* 创建带初始容量的 Buffer，失败时不泄漏半初始化对象。 */
static xbuffer* __xrtLzmaBufferCreate(size_t iCapacity)
{
	xbuffer* pBuffer = xrtBufferCreate();

	if ( (pBuffer != NULL) && !xrtBufferReserve(pBuffer, iCapacity) ) {
		xrtBufferDestroy(pBuffer);
		pBuffer = NULL;
	}
	return pBuffer;
}

static void __xrtLzmaWriteU32LE(uint8* pDst, uint32 iValue)
{
	pDst[0] = (uint8)(iValue & 0xffu);
	pDst[1] = (uint8)((iValue >> 8) & 0xffu);
	pDst[2] = (uint8)((iValue >> 16) & 0xffu);
	pDst[3] = (uint8)((iValue >> 24) & 0xffu);
}

static uint32 __xrtLzmaReadU32LE(const uint8* pSrc)
{
	return (uint32)pSrc[0] | ((uint32)pSrc[1] << 8) |
		((uint32)pSrc[2] << 16) | ((uint32)pSrc[3] << 24);
}

/* 标准 CRC32（IEEE 802.3，多项式 0xEDB88320，反射输入输出，异或 0xFFFFFFFF）。
 * 逐位计算，无需查表，仅用于块级完整性校验，性能需求不高。 */
static uint32 __xrtLzmaCrc32(const void* pData, size_t iLen)
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

static const char* __xrtLzmaStatusName(SRes iResult)
{
	switch ( iResult ) {
	case SZ_OK: return "ok";
	case SZ_ERROR_DATA: return "lzma data error";
	case SZ_ERROR_MEM: return "lzma memory error";
	case SZ_ERROR_CRC: return "lzma crc error";
	case SZ_ERROR_UNSUPPORTED: return "lzma unsupported data";
	case SZ_ERROR_PARAM: return "lzma parameter error";
	case SZ_ERROR_INPUT_EOF: return "lzma truncated input";
	case SZ_ERROR_OUTPUT_EOF: return "lzma output overflow";
	default: return "lzma operation failed";
	}
}



XRT_API void xrtLzmaErrorInit(xrtlzmaerror* pError)
{
	if ( pError == NULL ) { return; }
	memset(pError, 0, sizeof(*pError));
	pError->iCode = XRT_LZMA_OK;
}

XRT_API void xrtLzmaOptionsInit(xrtlzmaoptions* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_LZMA_OPTIONS_VERSION;
	pOptions->iLevel = 5;
	xrtResourceLimitsInit(&pOptions->tLimits);
}



// ------------------------------------ 压缩器 ------------------------------------

XRT_API xrtlzmacompressor* xrtLzmaCompressorCreate(const xrtlzmaoptions* pOptions)
{
	xrtlzmacompressor* pCompressor;
	pCompressor = (xrtlzmacompressor*)xrtMalloc(sizeof(xrtlzmacompressor));
	if ( pCompressor == NULL ) { return NULL; }
	memset(pCompressor, 0, sizeof(*pCompressor));
	xrtLzmaErrorInit(&pCompressor->Error);
	if ( pOptions != NULL ) {
		pCompressor->Options = *pOptions;
	} else {
		xrtLzmaOptionsInit(&pCompressor->Options);
	}
	if ( pCompressor->Options.iLevel < 0 ) { pCompressor->Options.iLevel = 0; }
	if ( pCompressor->Options.iLevel > 9 ) { pCompressor->Options.iLevel = 9; }
	pCompressor->Pending = __xrtLzmaBufferCreate(XRT_LZMA_CHUNK);
	pCompressor->Output = __xrtLzmaBufferCreate(XRT_LZMA_CHUNK);
	if ( pCompressor->Pending == NULL || pCompressor->Output == NULL ) {
		__xrtLzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_MEMORY, 0, 0, 0,
			"cannot allocate lzma compressor");
		pCompressor->Failed = true;
		return pCompressor;
	}
	return pCompressor;
}

/* 将 Pending 中恰好 iSize 字节压缩为一个块（大小头 + CRC + 属性 + 压缩体）写入 Output。 */
static bool __xrtLzmaFlushBlock(xrtlzmacompressor* pCompressor, size_t iSize)
{
	CLzmaEncProps tProps;
	uint8 sProps[LZMA_PROPS_SIZE];
	SizeT iPropsSize = LZMA_PROPS_SIZE;
	SizeT iBound;
	SizeT iWritten;
	SRes iResult;
	uint8 sHeader[XRT_LZMA_HEADER_SIZE];
	ptr pCompressed;
	xbytesview Pending;
	size_t iGenerated;
	if ( iSize == 0u ) { return true; }
	Pending = xrtBufferView(pCompressor->Pending);
	if ( iSize > Pending.Size ) {
		__xrtLzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_STATE, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "invalid lzma pending state");
		pCompressor->Failed = true;
		return false;
	}
	iBound = (SizeT)iSize + (SizeT)iSize / 3u + 128u;
	pCompressed = xrtMalloc((size_t)iBound);
	if ( pCompressed == NULL ) {
		__xrtLzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "out of memory");
		pCompressor->Failed = true;
		return false;
	}
	LzmaEncProps_Init(&tProps);
	tProps.level = pCompressor->Options.iLevel;
	tProps.reduceSize = (UInt64)iSize;
	iWritten = iBound;
	iResult = LzmaEncode((Byte*)pCompressed, &iWritten,
		(const Byte*)Pending.Data, (SizeT)iSize, &tProps,
		sProps, &iPropsSize, 0, NULL, &g_Alloc, &g_Alloc);
	if ( iResult != SZ_OK || iPropsSize != LZMA_PROPS_SIZE ) {
		__xrtLzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_DATA, (int)iResult,
			pCompressor->InputBytes, pCompressor->OutputBytes, "%s",
			__xrtLzmaStatusName(iResult));
		xrtFree(pCompressed);
		pCompressor->Failed = true;
		return false;
	}
	__xrtLzmaWriteU32LE(sHeader, (uint32)iSize);
	__xrtLzmaWriteU32LE(sHeader + 4u, (uint32)iWritten);
	__xrtLzmaWriteU32LE(sHeader + 8u,
		__xrtLzmaCrc32(Pending.Data, iSize));
	memcpy(sHeader + 12u, sProps, LZMA_PROPS_SIZE);
	iGenerated = XRT_LZMA_HEADER_SIZE + (size_t)iWritten;
	if ( __xrtLzmaAddOverLimit(pCompressor->OutputBytes, iGenerated,
		pCompressor->Options.tLimits.iMaxOutputBytes) ) {
		__xrtLzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_LIMIT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "lzma output exceeds maxOutput limit");
		xrtFree(pCompressed);
		pCompressor->Failed = true;
		return false;
	}
	if ( !xrtBufferAppend(pCompressor->Output,
			(xbytesview){ (const unsigned char*)sHeader, XRT_LZMA_HEADER_SIZE }) ||
		 !xrtBufferAppend(pCompressor->Output,
			(xbytesview){ (const unsigned char*)pCompressed, (size_t)iWritten }) ) {
		__xrtLzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow lzma output buffer");
		xrtFree(pCompressed);
		pCompressor->Failed = true;
		return false;
	}
	xrtFree(pCompressed);
	pCompressor->OutputBytes += (uint64)iGenerated;
	if ( !xrtBufferRemove(pCompressor->Pending, 0u, iSize) ) {
		__xrtLzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_STATE, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot consume lzma pending buffer");
		pCompressor->Failed = true;
		return false;
	}
	return true;
}

XRT_API bool xrtLzmaCompressorUpdate(xrtlzmacompressor* pCompressor, const void* pData, size_t iSize)
{
	if ( pCompressor == NULL || pCompressor->Failed ) { return false; }
	if ( pCompressor->Finished ) {
		__xrtLzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_STATE, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "lzma compressor already finished");
		pCompressor->Failed = true;
		return false;
	}
	if ( iSize != 0u && pData == NULL ) {
		__xrtLzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_ARGUMENT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "invalid lzma input");
		pCompressor->Failed = true;
		return false;
	}
	if ( __xrtLzmaAddOverLimit(pCompressor->InputBytes, iSize,
		pCompressor->Options.tLimits.iMaxInputBytes) ) {
		__xrtLzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_LIMIT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "lzma input exceeds maxInput limit");
		pCompressor->Failed = true;
		return false;
	}
	if ( iSize != 0u &&
		!xrtBufferAppend(pCompressor->Pending,
			(xbytesview){ (const unsigned char*)pData, iSize }) ) {
		__xrtLzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow lzma pending buffer");
		pCompressor->Failed = true;
		return false;
	}
	pCompressor->InputBytes += iSize;
	while ( xrtBufferView(pCompressor->Pending).Size >= XRT_LZMA_BLOCK_SIZE ) {
		if ( !__xrtLzmaFlushBlock(pCompressor, XRT_LZMA_BLOCK_SIZE) ) { return false; }
	}
	return true;
}

XRT_API bool xrtLzmaCompressorFinish(xrtlzmacompressor* pCompressor)
{
	uint8 sEnd[4] = { 0 };
	size_t iPending;
	if ( pCompressor == NULL || pCompressor->Failed ) { return false; }
	if ( pCompressor->Finished ) { return true; }
	iPending = xrtBufferView(pCompressor->Pending).Size;
	if ( iPending != 0u ) {
		if ( !__xrtLzmaFlushBlock(pCompressor, iPending) ) { return false; }
	}
	if ( __xrtLzmaAddOverLimit(pCompressor->OutputBytes, sizeof(sEnd),
		pCompressor->Options.tLimits.iMaxOutputBytes) ) {
		__xrtLzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_LIMIT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "lzma output exceeds maxOutput limit");
		pCompressor->Failed = true;
		return false;
	}
	if ( !xrtBufferAppend(pCompressor->Output,
		(xbytesview){ (const unsigned char*)sEnd, sizeof(sEnd) }) ) {
		__xrtLzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow lzma output buffer");
		pCompressor->Failed = true;
		return false;
	}
	pCompressor->OutputBytes += 4u;
	pCompressor->Finished = true;
	return true;
}

XRT_API bool xrtLzmaCompressorDrain(xrtlzmacompressor* pCompressor, xwriter* pWriter)
{
	xbytesview Output;
	if ( pCompressor == NULL || pCompressor->Failed || pWriter == NULL ) { return false; }
	Output = xrtBufferView(pCompressor->Output);
	if ( Output.Size != 0u &&
		!xrtWriterWriteFull(pWriter, Output.Data, Output.Size, NULL) ) {
		__xrtLzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_IO, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot write lzma output stream");
		pCompressor->Failed = true;
		return false;
	}
	xrtBufferClear(pCompressor->Output);
	return true;
}

XRT_API xbuffer* xrtLzmaCompressorTakeBuffer(xrtlzmacompressor* pCompressor)
{
	xbuffer* pOutput;
	if ( pCompressor == NULL || pCompressor->Failed || !pCompressor->Finished ) { return NULL; }
	pOutput = pCompressor->Output;
	pCompressor->Output = NULL;
	return pOutput;
}

XRT_API xbuffer* xrtLzmaCompressorDrainBuffer(xrtlzmacompressor* pCompressor)
{
	xbuffer* pOutput;
	if ( pCompressor == NULL || pCompressor->Failed || pCompressor->Output == NULL ) { return NULL; }
	pOutput = xrtBufferFrom(xrtBufferView(pCompressor->Output));
	if ( pOutput != NULL ) { xrtBufferClear(pCompressor->Output); }
	return pOutput;
}

XRT_API uint64 xrtLzmaCompressorInputBytes(const xrtlzmacompressor* pCompressor)
{
	return pCompressor != NULL ? pCompressor->InputBytes : 0u;
}

XRT_API uint64 xrtLzmaCompressorOutputBytes(const xrtlzmacompressor* pCompressor)
{
	return pCompressor != NULL ? pCompressor->OutputBytes : 0u;
}

XRT_API const xrtlzmaerror* xrtLzmaCompressorError(const xrtlzmacompressor* pCompressor)
{
	return pCompressor != NULL ? &pCompressor->Error : NULL;
}

XRT_API void xrtLzmaCompressorDestroy(xrtlzmacompressor* pCompressor)
{
	if ( pCompressor == NULL ) { return; }
	xrtBufferDestroy(pCompressor->Pending);
	xrtBufferDestroy(pCompressor->Output);
	xrtFree(pCompressor);
}



// ------------------------------------ 解压器 ------------------------------------

XRT_API xrtlzmadecompressor* xrtLzmaDecompressorCreate(const xrtlzmaoptions* pOptions)
{
	xrtlzmadecompressor* pDecompressor;
	pDecompressor = (xrtlzmadecompressor*)xrtMalloc(sizeof(xrtlzmadecompressor));
	if ( pDecompressor == NULL ) { return NULL; }
	memset(pDecompressor, 0, sizeof(*pDecompressor));
	xrtLzmaErrorInit(&pDecompressor->Error);
	if ( pOptions != NULL ) {
		pDecompressor->Options = *pOptions;
	} else {
		xrtLzmaOptionsInit(&pDecompressor->Options);
	}
	pDecompressor->Carry = __xrtLzmaBufferCreate(XRT_LZMA_CHUNK);
	pDecompressor->Output = __xrtLzmaBufferCreate(XRT_LZMA_CHUNK);
	if ( pDecompressor->Carry == NULL || pDecompressor->Output == NULL ) {
		__xrtLzmaSetError(&pDecompressor->Error, XRT_LZMA_ERROR_MEMORY, 0, 0, 0,
			"cannot allocate lzma decompressor");
		pDecompressor->Failed = true;
		return pDecompressor;
	}
	pDecompressor->State = XRT_LZMA_DEC_HEADER;
	return pDecompressor;
}

/* 消费 Carry 中的输入；*pConsumed 返回实际消费字节数。 */
static bool __xrtLzmaDecompressConsume(xrtlzmadecompressor* pDecompressor, size_t* pConsumed)
{
	size_t iPos = 0u;
	xbytesview Carry = xrtBufferView(pDecompressor->Carry);
	const uint8* pBuf = (const uint8*)Carry.Data;
	size_t iLen = Carry.Size;
	while ( iPos < iLen ) {
		if ( pDecompressor->State == XRT_LZMA_DEC_HEADER ) {
			uint32 iDeclared;
			if ( iLen - iPos < 4u ) { break; }
			iDeclared = __xrtLzmaReadU32LE(pBuf + iPos);
			if ( iDeclared == 0u ) {
				iPos += 4u;
				pDecompressor->MemberEnded = true;
				pDecompressor->MemberCount += 1u;
				continue;   /* 可能紧跟下一个成员 */
			}
			if ( iLen - iPos < XRT_LZMA_HEADER_SIZE ) { break; }
			pDecompressor->MemberEnded = false;
			pDecompressor->BlockDeclared = (size_t)iDeclared;
			pDecompressor->BlockCompressed = (size_t)__xrtLzmaReadU32LE(pBuf + iPos + 4u);
			pDecompressor->BlockCrc = __xrtLzmaReadU32LE(pBuf + iPos + 8u);
			memcpy(pDecompressor->sBlockProps, pBuf + iPos + 12u, LZMA_PROPS_SIZE);
			iPos += XRT_LZMA_HEADER_SIZE;
			pDecompressor->State = XRT_LZMA_DEC_BLOCK;
		} else {
			SizeT iDstLen;
			SizeT iSrcLen;
			ELzmaStatus eStatus = LZMA_STATUS_NOT_SPECIFIED;
			SRes iResult;
			ptr pPlain;
			if ( iLen - iPos < pDecompressor->BlockCompressed ) { break; }
			if ( (pDecompressor->BlockCompressed == 0u) ||
				 ((pDecompressor->Options.tLimits.iMaxItemBytes != 0u) &&
				  (pDecompressor->BlockDeclared > pDecompressor->Options.tLimits.iMaxItemBytes)) ||
				 __xrtLzmaAddOverLimit(pDecompressor->OutputBytes,
					pDecompressor->BlockDeclared,
					pDecompressor->Options.tLimits.iMaxOutputBytes) ||
				 __xrtLzmaOverRatio(&pDecompressor->Options.tLimits,
					pDecompressor->InputBytes,
					pDecompressor->OutputBytes + (uint64)pDecompressor->BlockDeclared) ) {
				__xrtLzmaSetError(&pDecompressor->Error, XRT_LZMA_ERROR_LIMIT, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"lzma output exceeds decompression limit");
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			pPlain = xrtMalloc(pDecompressor->BlockDeclared);
			if ( pPlain == NULL ) {
				__xrtLzmaSetError(&pDecompressor->Error, XRT_LZMA_ERROR_MEMORY, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes, "out of memory");
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			iDstLen = (SizeT)pDecompressor->BlockDeclared;
			iSrcLen = (SizeT)pDecompressor->BlockCompressed;
			iResult = LzmaDecode((Byte*)pPlain, &iDstLen, (const Byte*)(pBuf + iPos), &iSrcLen,
				pDecompressor->sBlockProps, LZMA_PROPS_SIZE, LZMA_FINISH_END, &eStatus, &g_Alloc);
			if ( iResult != SZ_OK || (size_t)iDstLen != pDecompressor->BlockDeclared ||
				(size_t)iSrcLen != pDecompressor->BlockCompressed ) {
				__xrtLzmaSetError(&pDecompressor->Error, XRT_LZMA_ERROR_DATA, (int)iResult,
					pDecompressor->InputBytes, pDecompressor->OutputBytes, "%s",
					__xrtLzmaStatusName(iResult == SZ_OK ? SZ_ERROR_DATA : iResult));
				xrtFree(pPlain);
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			iPos += pDecompressor->BlockCompressed;
			if ( __xrtLzmaCrc32(pPlain, (size_t)iDstLen) != pDecompressor->BlockCrc ) {
				__xrtLzmaSetError(&pDecompressor->Error, XRT_LZMA_ERROR_DATA, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"lzma block checksum mismatch");
				xrtFree(pPlain);
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			if ( !xrtBufferAppend(pDecompressor->Output,
				(xbytesview){ (const unsigned char*)pPlain, (size_t)iDstLen }) ) {
				__xrtLzmaSetError(&pDecompressor->Error, XRT_LZMA_ERROR_MEMORY, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"cannot grow lzma output buffer");
				xrtFree(pPlain);
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			xrtFree(pPlain);
			pDecompressor->OutputBytes += (uint64)iDstLen;
			pDecompressor->State = XRT_LZMA_DEC_HEADER;
		}
	}
	*pConsumed = iPos;
	return true;
}

XRT_API bool xrtLzmaDecompressorUpdate(xrtlzmadecompressor* pDecompressor, const void* pData, size_t iSize)
{
	size_t iConsumed = 0u;
	if ( pDecompressor == NULL || pDecompressor->Failed ) { return false; }
	if ( iSize != 0u && pData == NULL ) {
		__xrtLzmaSetError(&pDecompressor->Error, XRT_LZMA_ERROR_ARGUMENT, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "invalid lzma input");
		pDecompressor->Failed = true;
		return false;
	}
	if ( __xrtLzmaAddOverLimit(pDecompressor->InputBytes, iSize,
		pDecompressor->Options.tLimits.iMaxInputBytes) ) {
		__xrtLzmaSetError(&pDecompressor->Error, XRT_LZMA_ERROR_LIMIT, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "lzma input exceeds maxInput limit");
		pDecompressor->Failed = true;
		return false;
	}
	if ( iSize != 0u &&
		!xrtBufferAppend(pDecompressor->Carry,
			(xbytesview){ (const unsigned char*)pData, iSize }) ) {
		__xrtLzmaSetError(&pDecompressor->Error, XRT_LZMA_ERROR_MEMORY, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot grow lzma input buffer");
		pDecompressor->Failed = true;
		return false;
	}
	pDecompressor->MemberEnded = false;
	pDecompressor->InputBytes += iSize;
	if ( !__xrtLzmaDecompressConsume(pDecompressor, &iConsumed) ) { return false; }
	if ( (iConsumed != 0u) &&
		 !xrtBufferRemove(pDecompressor->Carry, 0u, iConsumed) ) {
		__xrtLzmaSetError(&pDecompressor->Error, XRT_LZMA_ERROR_STATE, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot consume lzma input buffer");
		pDecompressor->Failed = true;
		return false;
	}
	return true;
}

XRT_API bool xrtLzmaDecompressorFinish(xrtlzmadecompressor* pDecompressor)
{
	if ( pDecompressor == NULL || pDecompressor->Failed ) { return false; }
	if ( pDecompressor->Finished ) { return true; }
	if ( (pDecompressor->MemberCount == 0u) || !pDecompressor->MemberEnded ||
		(pDecompressor->State != XRT_LZMA_DEC_HEADER) ||
		(xrtBufferView(pDecompressor->Carry).Size != 0u) ) {
		__xrtLzmaSetError(&pDecompressor->Error, XRT_LZMA_ERROR_TRUNCATED, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "lzma stream is truncated or empty");
		pDecompressor->Failed = true;
		return false;
	}
	pDecompressor->Finished = true;
	return true;
}

XRT_API bool xrtLzmaDecompressorDrain(xrtlzmadecompressor* pDecompressor, xwriter* pWriter)
{
	xbytesview Output;
	if ( pDecompressor == NULL || pDecompressor->Failed || pWriter == NULL ) { return false; }
	Output = xrtBufferView(pDecompressor->Output);
	if ( Output.Size != 0u &&
		!xrtWriterWriteFull(pWriter, Output.Data, Output.Size, NULL) ) {
		__xrtLzmaSetError(&pDecompressor->Error, XRT_LZMA_ERROR_IO, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot write lzma output stream");
		pDecompressor->Failed = true;
		return false;
	}
	xrtBufferClear(pDecompressor->Output);
	return true;
}

XRT_API xbuffer* xrtLzmaDecompressorTakeBuffer(xrtlzmadecompressor* pDecompressor)
{
	xbuffer* pOutput;
	if ( pDecompressor == NULL || pDecompressor->Failed || !pDecompressor->Finished ) { return NULL; }
	pOutput = pDecompressor->Output;
	pDecompressor->Output = NULL;
	return pOutput;
}

XRT_API xbuffer* xrtLzmaDecompressorDrainBuffer(xrtlzmadecompressor* pDecompressor)
{
	xbuffer* pOutput;
	if ( pDecompressor == NULL || pDecompressor->Failed || pDecompressor->Output == NULL ) { return NULL; }
	pOutput = xrtBufferFrom(xrtBufferView(pDecompressor->Output));
	if ( pOutput != NULL ) { xrtBufferClear(pDecompressor->Output); }
	return pOutput;
}

XRT_API uint64 xrtLzmaDecompressorInputBytes(const xrtlzmadecompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->InputBytes : 0u;
}

XRT_API uint64 xrtLzmaDecompressorOutputBytes(const xrtlzmadecompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->OutputBytes : 0u;
}

XRT_API bool xrtLzmaDecompressorFinished(const xrtlzmadecompressor* pDecompressor)
{
	return pDecompressor != NULL && pDecompressor->MemberEnded;
}

XRT_API uint64 xrtLzmaDecompressorMembers(const xrtlzmadecompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->MemberCount : 0u;
}

XRT_API const xrtlzmaerror* xrtLzmaDecompressorError(const xrtlzmadecompressor* pDecompressor)
{
	return pDecompressor != NULL ? &pDecompressor->Error : NULL;
}

XRT_API void xrtLzmaDecompressorDestroy(xrtlzmadecompressor* pDecompressor)
{
	if ( pDecompressor == NULL ) { return; }
	xrtBufferDestroy(pDecompressor->Carry);
	xrtBufferDestroy(pDecompressor->Output);
	xrtFree(pDecompressor);
}



// ------------------------------------ 一次性接口 ------------------------------------

XRT_API bool xrtLzmaCompress(const void* pData, size_t iSize, const xrtlzmaoptions* pOptions,
	xbuffer** ppOutput, xrtlzmaerror* pError)
{
	xrtlzmaoptions tOptions;
	xrtlzmacompressor* pCompressor;
	bool bRet;
	xrtLzmaErrorInit(pError);
	if ( ppOutput != NULL ) { *ppOutput = NULL; }
	if ( ppOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtLzmaSetError(pError, XRT_LZMA_ERROR_ARGUMENT, 0, 0, 0, "invalid lzma input");
		return false;
	}
	if ( pOptions != NULL ) { tOptions = *pOptions; } else { xrtLzmaOptionsInit(&tOptions); }
	pCompressor = xrtLzmaCompressorCreate(&tOptions);
	if ( pCompressor == NULL ) {
		__xrtLzmaSetError(pError, XRT_LZMA_ERROR_MEMORY, 0, iSize, 0, "cannot allocate lzma compressor");
		return false;
	}
	bRet = xrtLzmaCompressorUpdate(pCompressor, pData, iSize) &&
		xrtLzmaCompressorFinish(pCompressor);
	if ( bRet ) { *ppOutput = xrtLzmaCompressorTakeBuffer(pCompressor); bRet = *ppOutput != NULL; }
	if ( !bRet ) { __xrtLzmaCopyError(pError, &pCompressor->Error); }
	xrtLzmaCompressorDestroy(pCompressor);
	return bRet;
}

XRT_API bool xrtLzmaDecompress(const void* pData, size_t iSize, const xrtlzmaoptions* pOptions,
	xbuffer** ppOutput, xrtlzmaerror* pError)
{
	xrtlzmaoptions tOptions;
	xrtlzmadecompressor* pDecompressor;
	bool bRet;
	xrtLzmaErrorInit(pError);
	if ( ppOutput != NULL ) { *ppOutput = NULL; }
	if ( ppOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtLzmaSetError(pError, XRT_LZMA_ERROR_ARGUMENT, 0, 0, 0, "invalid lzma input");
		return false;
	}
	if ( pOptions != NULL ) { tOptions = *pOptions; } else { xrtLzmaOptionsInit(&tOptions); }
	pDecompressor = xrtLzmaDecompressorCreate(&tOptions);
	if ( pDecompressor == NULL ) {
		__xrtLzmaSetError(pError, XRT_LZMA_ERROR_MEMORY, 0, iSize, 0, "cannot allocate lzma decompressor");
		return false;
	}
	bRet = xrtLzmaDecompressorUpdate(pDecompressor, pData, iSize) &&
		xrtLzmaDecompressorFinish(pDecompressor);
	if ( bRet ) { *ppOutput = xrtLzmaDecompressorTakeBuffer(pDecompressor); bRet = *ppOutput != NULL; }
	if ( !bRet ) { __xrtLzmaCopyError(pError, &pDecompressor->Error); }
	xrtLzmaDecompressorDestroy(pDecompressor);
	return bRet;
}



// ------------------------------------ 文件流接口 ------------------------------------

XRT_API bool xrtLzmaCompressStream(xreader* pReader, xwriter* pWriter, const xrtlzmaoptions* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtlzmaerror* pError)
{
	uint8 sInput[XRT_LZMA_CHUNK];
	xrtlzmacompressor* pCompressor;
	uint64 iTotal = 0u;
	bool bTotalKnown;
	bool bRet = true;
	xrtLzmaErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		__xrtLzmaSetError(pError, XRT_LZMA_ERROR_ARGUMENT, 0, 0, 0, "invalid lzma stream");
		return false;
	}
	bTotalKnown = xrtReaderCanSeek(pReader) && xrtReaderSize(pReader, &iTotal);
	if ( !bTotalKnown ) { xrtClearError(); }
	pCompressor = xrtLzmaCompressorCreate(pOptions);
	if ( pCompressor == NULL ) { return false; }
	while ( bRet ) {
		size_t iRead = 0u;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtLzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_IO, 0,
				pCompressor->InputBytes, pCompressor->OutputBytes, "cannot read lzma input stream");
			bRet = false;
			break;
		}
		if ( iRead == 0u ) { break; }
		bRet = xrtLzmaCompressorUpdate(pCompressor, sInput, (size_t)iRead) &&
			xrtLzmaCompressorDrain(pCompressor, pWriter);
		if ( bRet && !xrtProgressReport(pProgress, pUserData, pCompressor->InputBytes,
			iTotal, pCompressor->OutputBytes,
			bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u) ) {
			__xrtLzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_CANCELLED, 0,
				pCompressor->InputBytes, pCompressor->OutputBytes, "lzma operation cancelled");
			bRet = false;
		}
	}
	if ( bRet ) { bRet = xrtLzmaCompressorFinish(pCompressor) && xrtLzmaCompressorDrain(pCompressor, pWriter); }
	if ( bRet && !xrtProgressReport(pProgress, pUserData, pCompressor->InputBytes,
		iTotal, pCompressor->OutputBytes, XRT_PROGRESS_FINAL |
		(bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u)) ) {
		__xrtLzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_CANCELLED, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "lzma operation cancelled");
		bRet = false;
	}
	if ( !bRet ) { __xrtLzmaCopyError(pError, &pCompressor->Error); }
	xrtLzmaCompressorDestroy(pCompressor);
	return bRet;
}

XRT_API bool xrtLzmaDecompressStream(xreader* pReader, xwriter* pWriter, const xrtlzmaoptions* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtlzmaerror* pError)
{
	uint8 sInput[XRT_LZMA_CHUNK];
	xrtlzmadecompressor* pDecompressor;
	uint64 iTotal = 0u;
	bool bTotalKnown;
	bool bRet = true;
	xrtLzmaErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		__xrtLzmaSetError(pError, XRT_LZMA_ERROR_ARGUMENT, 0, 0, 0, "invalid lzma stream");
		return false;
	}
	bTotalKnown = xrtReaderCanSeek(pReader) && xrtReaderSize(pReader, &iTotal);
	if ( !bTotalKnown ) { xrtClearError(); }
	pDecompressor = xrtLzmaDecompressorCreate(pOptions);
	if ( pDecompressor == NULL ) { return false; }
	while ( bRet ) {
		size_t iRead = 0u;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtLzmaSetError(&pDecompressor->Error, XRT_LZMA_ERROR_IO, 0,
				pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot read lzma input stream");
			bRet = false;
			break;
		}
		if ( iRead == 0u ) { break; }
		bRet = xrtLzmaDecompressorUpdate(pDecompressor, sInput, (size_t)iRead) &&
			xrtLzmaDecompressorDrain(pDecompressor, pWriter);
		if ( bRet && !xrtProgressReport(pProgress, pUserData, pDecompressor->InputBytes,
			iTotal, pDecompressor->OutputBytes,
			bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u) ) {
			__xrtLzmaSetError(&pDecompressor->Error, XRT_LZMA_ERROR_CANCELLED, 0,
				pDecompressor->InputBytes, pDecompressor->OutputBytes, "lzma operation cancelled");
			bRet = false;
		}
	}
	if ( bRet ) { bRet = xrtLzmaDecompressorFinish(pDecompressor) && xrtLzmaDecompressorDrain(pDecompressor, pWriter); }
	if ( bRet && !xrtProgressReport(pProgress, pUserData, pDecompressor->InputBytes,
		iTotal, pDecompressor->OutputBytes, XRT_PROGRESS_FINAL |
		(bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u)) ) {
		__xrtLzmaSetError(&pDecompressor->Error, XRT_LZMA_ERROR_CANCELLED, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "lzma operation cancelled");
		bRet = false;
	}
	if ( !bRet ) { __xrtLzmaCopyError(pError, &pDecompressor->Error); }
	xrtLzmaDecompressorDestroy(pDecompressor);
	return bRet;
}
