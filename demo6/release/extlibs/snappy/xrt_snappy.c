#include "include/xrt_snappy.h"
#include "third_party/xrt_snappy.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>



/* 单个块最大原始字节数。snappy 一次性 API 以 size_t 传递长度，
 * 这里取 1MB 以便流式产出粒度适中、内存占用可控。 */
#define XRT_SNAPPY_BLOCK_SIZE 1048576u
#define XRT_SNAPPY_CHUNK 65536u
#define XRT_SNAPPY_HEADER_SIZE 12u

/* 容器格式（自定义多块成员流，支持串联多成员）：
 *   成员 = 块* + 结束标记
 *   块   = u32le(原始大小 n, n>0) + u32le(压缩大小 c) + u32le(原始数据 CRC32) + snappy 压缩字节(c 字节)
 *   结束 = u32le(0)
 * snappy 流自身不含校验和，故额外记录原始数据 CRC32 以检出损坏，
 * 解压侧据此精确定位块边界、校验完整性并统计 members。 */

struct xrtsnappycompressor {
	xbuffer* Pending;            /* 尚未成块的原始输入 */
	xbuffer* Output;             /* 已产出的压缩字节 */
	xrtsnappyoptions Options;
	xrtsnappyerror Error;
	uint64 InputBytes;
	uint64 OutputBytes;
	bool Finished;
	bool Failed;
};

typedef enum {
	XRT_SNAPPY_DEC_HEADER = 0,   /* 读取 12 字节块头 */
	XRT_SNAPPY_DEC_BLOCK = 1     /* 读取压缩块体 */
} xrtsnappydecstate;

struct xrtsnappydecompressor {
	xbuffer* Carry;              /* 尚未消费完的输入字节 */
	xbuffer* Output;             /* 已产出的原始字节 */
	xrtsnappyoptions Options;
	xrtsnappyerror Error;
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



static void __xrtSnappySetError(xrtsnappyerror* pError, int iCode, int iBackend,
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

static void __xrtSnappyCopyError(xrtsnappyerror* pDst, const xrtsnappyerror* pSrc)
{
	if ( pDst == NULL || pSrc == NULL ) { return; }
	*pDst = *pSrc;
}

static bool __xrtSnappyOverRatio(const xrtresourcelimits* pLimits, uint64 iInput, uint64 iOutput)
{
	uint64 iRatio = pLimits->iMaxCompressionRatio;
	if ( iRatio == 0u || iInput == 0u ) { return false; }
	if ( iInput > (UINT64_MAX / iRatio) ) { return false; }
	return iOutput > iInput * iRatio;
}

/* 检查计数累加是否溢出或越过非零硬上限。 */
static bool __xrtSnappyAddOverLimit(uint64 iCurrent, size_t iAdded, uint64 iLimit)
{
	uint64 iIncrement = (uint64)iAdded;

	if ( iIncrement > (UINT64_MAX - iCurrent) ) { return true; }
	return (iLimit != 0u) && ((iCurrent + iIncrement) > iLimit);
}

/* 创建带初始容量的 Buffer，失败时不泄漏半初始化对象。 */
static xbuffer* __xrtSnappyBufferCreate(size_t iCapacity)
{
	xbuffer* pBuffer = xrtBufferCreate();

	if ( (pBuffer != NULL) && !xrtBufferReserve(pBuffer, iCapacity) ) {
		xrtBufferDestroy(pBuffer);
		pBuffer = NULL;
	}
	return pBuffer;
}

static void __xrtSnappyWriteU32LE(uint8* pDst, uint32 iValue)
{
	pDst[0] = (uint8)(iValue & 0xffu);
	pDst[1] = (uint8)((iValue >> 8) & 0xffu);
	pDst[2] = (uint8)((iValue >> 16) & 0xffu);
	pDst[3] = (uint8)((iValue >> 24) & 0xffu);
}

static uint32 __xrtSnappyReadU32LE(const uint8* pSrc)
{
	return (uint32)pSrc[0] | ((uint32)pSrc[1] << 8) |
		((uint32)pSrc[2] << 16) | ((uint32)pSrc[3] << 24);
}

/* 标准 CRC32（IEEE 802.3，多项式 0xEDB88320，反射输入输出，异或 0xFFFFFFFF）。
 * 逐位计算，无需查表，仅用于块级完整性校验，性能需求不高。 */
static uint32 __xrtSnappyCrc32(const void* pData, size_t iLen)
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



XRT_API void xrtSnappyErrorInit(xrtsnappyerror* pError)
{
	if ( pError == NULL ) { return; }
	memset(pError, 0, sizeof(*pError));
	pError->iCode = XRT_SNAPPY_OK;
}

XRT_API void xrtSnappyOptionsInit(xrtsnappyoptions* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_SNAPPY_OPTIONS_VERSION;
	xrtResourceLimitsInit(&pOptions->tLimits);
}



// ------------------------------------ 压缩器 ------------------------------------

XRT_API xrtsnappycompressor* xrtSnappyCompressorCreate(const xrtsnappyoptions* pOptions)
{
	xrtsnappycompressor* pCompressor;
	pCompressor = (xrtsnappycompressor*)xrtMalloc(sizeof(xrtsnappycompressor));
	if ( pCompressor == NULL ) { return NULL; }
	memset(pCompressor, 0, sizeof(*pCompressor));
	xrtSnappyErrorInit(&pCompressor->Error);
	if ( pOptions != NULL ) {
		pCompressor->Options = *pOptions;
	} else {
		xrtSnappyOptionsInit(&pCompressor->Options);
	}
	pCompressor->Pending = __xrtSnappyBufferCreate(XRT_SNAPPY_CHUNK);
	pCompressor->Output = __xrtSnappyBufferCreate(XRT_SNAPPY_CHUNK);
	if ( pCompressor->Pending == NULL || pCompressor->Output == NULL ) {
		__xrtSnappySetError(&pCompressor->Error, XRT_SNAPPY_ERROR_MEMORY, 0, 0, 0,
			"cannot allocate snappy compressor");
		pCompressor->Failed = true;
		return pCompressor;
	}
	return pCompressor;
}

/* 将 Pending 中恰好 iSize 字节压缩为一个块（大小头 + 压缩体）写入 Output。 */
static bool __xrtSnappyFlushBlock(xrtsnappycompressor* pCompressor, size_t iSize)
{
	size_t iBound;
	size_t iWritten;
	uint8 sHeader[XRT_SNAPPY_HEADER_SIZE];
	ptr pCompressed;
	int iResult;
	xbytesview Pending;
	size_t iGenerated;
	if ( iSize == 0u ) { return true; }
	Pending = xrtBufferView(pCompressor->Pending);
	if ( iSize > Pending.Size ) {
		__xrtSnappySetError(&pCompressor->Error, XRT_SNAPPY_ERROR_STATE, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "invalid snappy pending state");
		pCompressor->Failed = true;
		return false;
	}
	iBound = xsCodecMaxCompressedLength(iSize);
	if ( iBound == 0u ) {
		__xrtSnappySetError(&pCompressor->Error, XRT_SNAPPY_ERROR_ARGUMENT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "snappy block too large");
		pCompressor->Failed = true;
		return false;
	}
	pCompressed = xrtMalloc(iBound);
	if ( pCompressed == NULL ) {
		__xrtSnappySetError(&pCompressor->Error, XRT_SNAPPY_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "out of memory");
		pCompressor->Failed = true;
		return false;
	}
	iResult = xsCodecCompress((const unsigned char*)Pending.Data, iSize,
		(unsigned char*)pCompressed, iBound, &iWritten);
	if ( iResult != XRT_SNAPPY_OK ) {
		__xrtSnappySetError(&pCompressor->Error, XRT_SNAPPY_ERROR_DATA, iResult,
			pCompressor->InputBytes, pCompressor->OutputBytes, "snappy compression failed");
		xrtFree(pCompressed);
		pCompressor->Failed = true;
		return false;
	}
	if ( iWritten > UINT32_MAX ) {
		__xrtSnappySetError(&pCompressor->Error, XRT_SNAPPY_ERROR_DATA, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "snappy compressed block is too large");
		xrtFree(pCompressed);
		pCompressor->Failed = true;
		return false;
	}
	__xrtSnappyWriteU32LE(sHeader, (uint32)iSize);
	__xrtSnappyWriteU32LE(sHeader + 4u, (uint32)iWritten);
	__xrtSnappyWriteU32LE(sHeader + 8u,
		__xrtSnappyCrc32(Pending.Data, iSize));
	iGenerated = XRT_SNAPPY_HEADER_SIZE + iWritten;
	if ( __xrtSnappyAddOverLimit(pCompressor->OutputBytes, iGenerated,
		pCompressor->Options.tLimits.iMaxOutputBytes) ) {
		__xrtSnappySetError(&pCompressor->Error, XRT_SNAPPY_ERROR_LIMIT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "snappy output exceeds maxOutput limit");
		xrtFree(pCompressed);
		pCompressor->Failed = true;
		return false;
	}
	if ( !xrtBufferAppend(pCompressor->Output,
			(xbytesview){ (const unsigned char*)sHeader, XRT_SNAPPY_HEADER_SIZE }) ||
		 !xrtBufferAppend(pCompressor->Output,
			(xbytesview){ (const unsigned char*)pCompressed, iWritten }) ) {
		__xrtSnappySetError(&pCompressor->Error, XRT_SNAPPY_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow snappy output buffer");
		xrtFree(pCompressed);
		pCompressor->Failed = true;
		return false;
	}
	xrtFree(pCompressed);
	pCompressor->OutputBytes += (uint64)iGenerated;
	if ( !xrtBufferRemove(pCompressor->Pending, 0u, iSize) ) {
		__xrtSnappySetError(&pCompressor->Error, XRT_SNAPPY_ERROR_STATE, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot consume snappy pending buffer");
		pCompressor->Failed = true;
		return false;
	}
	return true;
}

XRT_API bool xrtSnappyCompressorUpdate(xrtsnappycompressor* pCompressor, const void* pData, size_t iSize)
{
	if ( pCompressor == NULL || pCompressor->Failed ) { return false; }
	if ( pCompressor->Finished ) {
		__xrtSnappySetError(&pCompressor->Error, XRT_SNAPPY_ERROR_STATE, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "snappy compressor already finished");
		pCompressor->Failed = true;
		return false;
	}
	if ( iSize != 0u && pData == NULL ) {
		__xrtSnappySetError(&pCompressor->Error, XRT_SNAPPY_ERROR_ARGUMENT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "invalid snappy input");
		pCompressor->Failed = true;
		return false;
	}
	if ( __xrtSnappyAddOverLimit(pCompressor->InputBytes, iSize,
		pCompressor->Options.tLimits.iMaxInputBytes) ) {
		__xrtSnappySetError(&pCompressor->Error, XRT_SNAPPY_ERROR_LIMIT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "snappy input exceeds maxInput limit");
		pCompressor->Failed = true;
		return false;
	}
	if ( iSize != 0u &&
		!xrtBufferAppend(pCompressor->Pending,
			(xbytesview){ (const unsigned char*)pData, iSize }) ) {
		__xrtSnappySetError(&pCompressor->Error, XRT_SNAPPY_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow snappy pending buffer");
		pCompressor->Failed = true;
		return false;
	}
	pCompressor->InputBytes += iSize;
	while ( xrtBufferView(pCompressor->Pending).Size >= XRT_SNAPPY_BLOCK_SIZE ) {
		if ( !__xrtSnappyFlushBlock(pCompressor, XRT_SNAPPY_BLOCK_SIZE) ) { return false; }
	}
	return true;
}

XRT_API bool xrtSnappyCompressorFinish(xrtsnappycompressor* pCompressor)
{
	uint8 sEnd[XRT_SNAPPY_HEADER_SIZE] = { 0 };
	size_t iPending;
	if ( pCompressor == NULL || pCompressor->Failed ) { return false; }
	if ( pCompressor->Finished ) { return true; }
	iPending = xrtBufferView(pCompressor->Pending).Size;
	if ( iPending != 0u ) {
		if ( !__xrtSnappyFlushBlock(pCompressor, iPending) ) { return false; }
	}
	if ( __xrtSnappyAddOverLimit(pCompressor->OutputBytes, XRT_SNAPPY_HEADER_SIZE,
		pCompressor->Options.tLimits.iMaxOutputBytes) ) {
		__xrtSnappySetError(&pCompressor->Error, XRT_SNAPPY_ERROR_LIMIT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "snappy output exceeds maxOutput limit");
		pCompressor->Failed = true;
		return false;
	}
	if ( !xrtBufferAppend(pCompressor->Output,
		(xbytesview){ (const unsigned char*)sEnd, sizeof(sEnd) }) ) {
		__xrtSnappySetError(&pCompressor->Error, XRT_SNAPPY_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow snappy output buffer");
		pCompressor->Failed = true;
		return false;
	}
	pCompressor->OutputBytes += XRT_SNAPPY_HEADER_SIZE;
	pCompressor->Finished = true;
	return true;
}

XRT_API bool xrtSnappyCompressorDrain(xrtsnappycompressor* pCompressor, xwriter* pWriter)
{
	xbytesview Output;
	if ( pCompressor == NULL || pCompressor->Failed || pWriter == NULL ) { return false; }
	Output = xrtBufferView(pCompressor->Output);
	if ( Output.Size != 0u &&
		!xrtWriterWriteFull(pWriter, Output.Data, Output.Size, NULL) ) {
		__xrtSnappySetError(&pCompressor->Error, XRT_SNAPPY_ERROR_IO, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot write snappy output stream");
		pCompressor->Failed = true;
		return false;
	}
	xrtBufferClear(pCompressor->Output);
	return true;
}

XRT_API xbuffer* xrtSnappyCompressorTakeBuffer(xrtsnappycompressor* pCompressor)
{
	xbuffer* pOutput;
	if ( pCompressor == NULL || pCompressor->Failed || !pCompressor->Finished ) { return NULL; }
	pOutput = pCompressor->Output;
	pCompressor->Output = NULL;
	return pOutput;
}

XRT_API xbuffer* xrtSnappyCompressorDrainBuffer(xrtsnappycompressor* pCompressor)
{
	xbuffer* pOutput;
	if ( pCompressor == NULL || pCompressor->Failed || pCompressor->Output == NULL ) { return NULL; }
	pOutput = xrtBufferFrom(xrtBufferView(pCompressor->Output));
	if ( pOutput != NULL ) { xrtBufferClear(pCompressor->Output); }
	return pOutput;
}

XRT_API uint64 xrtSnappyCompressorInputBytes(const xrtsnappycompressor* pCompressor)
{
	return pCompressor != NULL ? pCompressor->InputBytes : 0u;
}

XRT_API uint64 xrtSnappyCompressorOutputBytes(const xrtsnappycompressor* pCompressor)
{
	return pCompressor != NULL ? pCompressor->OutputBytes : 0u;
}

XRT_API const xrtsnappyerror* xrtSnappyCompressorError(const xrtsnappycompressor* pCompressor)
{
	return pCompressor != NULL ? &pCompressor->Error : NULL;
}

XRT_API void xrtSnappyCompressorDestroy(xrtsnappycompressor* pCompressor)
{
	if ( pCompressor == NULL ) { return; }
	xrtBufferDestroy(pCompressor->Pending);
	xrtBufferDestroy(pCompressor->Output);
	xrtFree(pCompressor);
}



// ------------------------------------ 解压器 ------------------------------------

XRT_API xrtsnappydecompressor* xrtSnappyDecompressorCreate(const xrtsnappyoptions* pOptions)
{
	xrtsnappydecompressor* pDecompressor;
	pDecompressor = (xrtsnappydecompressor*)xrtMalloc(sizeof(xrtsnappydecompressor));
	if ( pDecompressor == NULL ) { return NULL; }
	memset(pDecompressor, 0, sizeof(*pDecompressor));
	xrtSnappyErrorInit(&pDecompressor->Error);
	if ( pOptions != NULL ) {
		pDecompressor->Options = *pOptions;
	} else {
		xrtSnappyOptionsInit(&pDecompressor->Options);
	}
	pDecompressor->Carry = __xrtSnappyBufferCreate(XRT_SNAPPY_CHUNK);
	pDecompressor->Output = __xrtSnappyBufferCreate(XRT_SNAPPY_CHUNK);
	if ( pDecompressor->Carry == NULL || pDecompressor->Output == NULL ) {
		__xrtSnappySetError(&pDecompressor->Error, XRT_SNAPPY_ERROR_MEMORY, 0, 0, 0,
			"cannot allocate snappy decompressor");
		pDecompressor->Failed = true;
		return pDecompressor;
	}
	pDecompressor->State = XRT_SNAPPY_DEC_HEADER;
	return pDecompressor;
}

/* 消费 Carry 中的输入；*pConsumed 返回实际消费字节数。 */
static bool __xrtSnappyDecompressConsume(xrtsnappydecompressor* pDecompressor, size_t* pConsumed)
{
	size_t iPos = 0u;
	xbytesview Carry = xrtBufferView(pDecompressor->Carry);
	const uint8* pBuf = (const uint8*)Carry.Data;
	size_t iLen = Carry.Size;
	while ( iPos < iLen ) {
		if ( pDecompressor->State == XRT_SNAPPY_DEC_HEADER ) {
			uint32 iDeclared;
			uint32 iCompressed;
			if ( iLen - iPos < XRT_SNAPPY_HEADER_SIZE ) { break; }
			iDeclared = __xrtSnappyReadU32LE(pBuf + iPos);
			iCompressed = __xrtSnappyReadU32LE(pBuf + iPos + 4u);
			iPos += XRT_SNAPPY_HEADER_SIZE;
			if ( iDeclared == 0u ) {
				pDecompressor->MemberEnded = true;
				pDecompressor->MemberCount += 1u;
				continue;   /* 可能紧跟下一个成员 */
			}
			pDecompressor->MemberEnded = false;
			pDecompressor->BlockDeclared = (size_t)iDeclared;
			pDecompressor->BlockCompressed = (size_t)iCompressed;
			pDecompressor->BlockCrc = __xrtSnappyReadU32LE(pBuf + iPos - 4u);
			pDecompressor->State = XRT_SNAPPY_DEC_BLOCK;
		} else {
			size_t iWritten;
			ptr pPlain;
			int iResult;
			if ( pDecompressor->BlockCompressed == 0u ) {
				__xrtSnappySetError(&pDecompressor->Error, XRT_SNAPPY_ERROR_DATA, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"snappy block declares invalid compressed size");
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			if ( iLen - iPos < pDecompressor->BlockCompressed ) { break; }
			if ( ((pDecompressor->Options.tLimits.iMaxItemBytes != 0u) &&
				  (pDecompressor->BlockDeclared > pDecompressor->Options.tLimits.iMaxItemBytes)) ||
				 __xrtSnappyAddOverLimit(pDecompressor->OutputBytes,
					pDecompressor->BlockDeclared,
					pDecompressor->Options.tLimits.iMaxOutputBytes) ||
				 __xrtSnappyOverRatio(&pDecompressor->Options.tLimits,
					pDecompressor->InputBytes,
					pDecompressor->OutputBytes + (uint64)pDecompressor->BlockDeclared) ) {
				__xrtSnappySetError(&pDecompressor->Error, XRT_SNAPPY_ERROR_LIMIT, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"snappy output exceeds decompression limit");
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			pPlain = xrtMalloc(pDecompressor->BlockDeclared);
			if ( pPlain == NULL ) {
				__xrtSnappySetError(&pDecompressor->Error, XRT_SNAPPY_ERROR_MEMORY, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes, "out of memory");
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			iResult = xsCodecDecompress(pBuf + iPos, pDecompressor->BlockCompressed,
				(unsigned char*)pPlain, pDecompressor->BlockDeclared, &iWritten);
			if ( iResult != XRT_SNAPPY_OK || iWritten != pDecompressor->BlockDeclared ) {
				__xrtSnappySetError(&pDecompressor->Error, XRT_SNAPPY_ERROR_DATA, iResult,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"snappy decompression failed");
				xrtFree(pPlain);
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			iPos += pDecompressor->BlockCompressed;
			if ( __xrtSnappyCrc32(pPlain, iWritten) != pDecompressor->BlockCrc ) {
				__xrtSnappySetError(&pDecompressor->Error, XRT_SNAPPY_ERROR_DATA, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"snappy block checksum mismatch");
				xrtFree(pPlain);
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			if ( !xrtBufferAppend(pDecompressor->Output,
				(xbytesview){ (const unsigned char*)pPlain, iWritten }) ) {
				__xrtSnappySetError(&pDecompressor->Error, XRT_SNAPPY_ERROR_MEMORY, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"cannot grow snappy output buffer");
				xrtFree(pPlain);
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			xrtFree(pPlain);
			pDecompressor->OutputBytes += (uint64)iWritten;
			pDecompressor->State = XRT_SNAPPY_DEC_HEADER;
		}
	}
	*pConsumed = iPos;
	return true;
}

XRT_API bool xrtSnappyDecompressorUpdate(xrtsnappydecompressor* pDecompressor, const void* pData, size_t iSize)
{
	size_t iConsumed = 0u;
	if ( pDecompressor == NULL || pDecompressor->Failed ) { return false; }
	if ( iSize != 0u && pData == NULL ) {
		__xrtSnappySetError(&pDecompressor->Error, XRT_SNAPPY_ERROR_ARGUMENT, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "invalid snappy input");
		pDecompressor->Failed = true;
		return false;
	}
	if ( __xrtSnappyAddOverLimit(pDecompressor->InputBytes, iSize,
		pDecompressor->Options.tLimits.iMaxInputBytes) ) {
		__xrtSnappySetError(&pDecompressor->Error, XRT_SNAPPY_ERROR_LIMIT, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "snappy input exceeds maxInput limit");
		pDecompressor->Failed = true;
		return false;
	}
	if ( iSize != 0u &&
		!xrtBufferAppend(pDecompressor->Carry,
			(xbytesview){ (const unsigned char*)pData, iSize }) ) {
		__xrtSnappySetError(&pDecompressor->Error, XRT_SNAPPY_ERROR_MEMORY, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot grow snappy input buffer");
		pDecompressor->Failed = true;
		return false;
	}
	pDecompressor->InputBytes += iSize;
	if ( !__xrtSnappyDecompressConsume(pDecompressor, &iConsumed) ) { return false; }
	if ( (iConsumed != 0u) &&
		 !xrtBufferRemove(pDecompressor->Carry, 0u, iConsumed) ) {
		__xrtSnappySetError(&pDecompressor->Error, XRT_SNAPPY_ERROR_STATE, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot consume snappy input buffer");
		pDecompressor->Failed = true;
		return false;
	}
	return true;
}

XRT_API bool xrtSnappyDecompressorFinish(xrtsnappydecompressor* pDecompressor)
{
	if ( pDecompressor == NULL || pDecompressor->Failed ) { return false; }
	if ( pDecompressor->Finished ) { return true; }
	if ( (pDecompressor->MemberCount == 0u) || !pDecompressor->MemberEnded ||
		(pDecompressor->State != XRT_SNAPPY_DEC_HEADER) ||
		(xrtBufferView(pDecompressor->Carry).Size != 0u) ) {
		__xrtSnappySetError(&pDecompressor->Error, XRT_SNAPPY_ERROR_TRUNCATED, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "snappy stream is truncated or empty");
		pDecompressor->Failed = true;
		return false;
	}
	pDecompressor->Finished = true;
	return true;
}

XRT_API bool xrtSnappyDecompressorDrain(xrtsnappydecompressor* pDecompressor, xwriter* pWriter)
{
	xbytesview Output;
	if ( pDecompressor == NULL || pDecompressor->Failed || pWriter == NULL ) { return false; }
	Output = xrtBufferView(pDecompressor->Output);
	if ( Output.Size != 0u &&
		!xrtWriterWriteFull(pWriter, Output.Data, Output.Size, NULL) ) {
		__xrtSnappySetError(&pDecompressor->Error, XRT_SNAPPY_ERROR_IO, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot write snappy output stream");
		pDecompressor->Failed = true;
		return false;
	}
	xrtBufferClear(pDecompressor->Output);
	return true;
}

XRT_API xbuffer* xrtSnappyDecompressorTakeBuffer(xrtsnappydecompressor* pDecompressor)
{
	xbuffer* pOutput;
	if ( pDecompressor == NULL || pDecompressor->Failed || !pDecompressor->Finished ) { return NULL; }
	pOutput = pDecompressor->Output;
	pDecompressor->Output = NULL;
	return pOutput;
}

XRT_API xbuffer* xrtSnappyDecompressorDrainBuffer(xrtsnappydecompressor* pDecompressor)
{
	xbuffer* pOutput;
	if ( pDecompressor == NULL || pDecompressor->Failed || pDecompressor->Output == NULL ) { return NULL; }
	pOutput = xrtBufferFrom(xrtBufferView(pDecompressor->Output));
	if ( pOutput != NULL ) { xrtBufferClear(pDecompressor->Output); }
	return pOutput;
}

XRT_API uint64 xrtSnappyDecompressorInputBytes(const xrtsnappydecompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->InputBytes : 0u;
}

XRT_API uint64 xrtSnappyDecompressorOutputBytes(const xrtsnappydecompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->OutputBytes : 0u;
}

XRT_API bool xrtSnappyDecompressorFinished(const xrtsnappydecompressor* pDecompressor)
{
	return pDecompressor != NULL && pDecompressor->MemberEnded;
}

XRT_API uint64 xrtSnappyDecompressorMembers(const xrtsnappydecompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->MemberCount : 0u;
}

XRT_API const xrtsnappyerror* xrtSnappyDecompressorError(const xrtsnappydecompressor* pDecompressor)
{
	return pDecompressor != NULL ? &pDecompressor->Error : NULL;
}

XRT_API void xrtSnappyDecompressorDestroy(xrtsnappydecompressor* pDecompressor)
{
	if ( pDecompressor == NULL ) { return; }
	xrtBufferDestroy(pDecompressor->Carry);
	xrtBufferDestroy(pDecompressor->Output);
	xrtFree(pDecompressor);
}



// ------------------------------------ 一次性接口 ------------------------------------

XRT_API bool xrtSnappyCompress(const void* pData, size_t iSize, const xrtsnappyoptions* pOptions,
	xbuffer** ppOutput, xrtsnappyerror* pError)
{
	xrtsnappyoptions tOptions;
	xrtsnappycompressor* pCompressor;
	bool bRet;
	xrtSnappyErrorInit(pError);
	if ( ppOutput != NULL ) { *ppOutput = NULL; }
	if ( ppOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtSnappySetError(pError, XRT_SNAPPY_ERROR_ARGUMENT, 0, 0, 0, "invalid snappy input");
		return false;
	}
	if ( pOptions != NULL ) { tOptions = *pOptions; } else { xrtSnappyOptionsInit(&tOptions); }
	pCompressor = xrtSnappyCompressorCreate(&tOptions);
	if ( pCompressor == NULL ) {
		__xrtSnappySetError(pError, XRT_SNAPPY_ERROR_MEMORY, 0, iSize, 0, "cannot allocate snappy compressor");
		return false;
	}
	bRet = xrtSnappyCompressorUpdate(pCompressor, pData, iSize) &&
		xrtSnappyCompressorFinish(pCompressor);
	if ( bRet ) { *ppOutput = xrtSnappyCompressorTakeBuffer(pCompressor); bRet = *ppOutput != NULL; }
	if ( !bRet ) { __xrtSnappyCopyError(pError, &pCompressor->Error); }
	xrtSnappyCompressorDestroy(pCompressor);
	return bRet;
}

XRT_API bool xrtSnappyDecompress(const void* pData, size_t iSize, const xrtsnappyoptions* pOptions,
	xbuffer** ppOutput, xrtsnappyerror* pError)
{
	xrtsnappyoptions tOptions;
	xrtsnappydecompressor* pDecompressor;
	bool bRet;
	xrtSnappyErrorInit(pError);
	if ( ppOutput != NULL ) { *ppOutput = NULL; }
	if ( ppOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtSnappySetError(pError, XRT_SNAPPY_ERROR_ARGUMENT, 0, 0, 0, "invalid snappy input");
		return false;
	}
	if ( pOptions != NULL ) { tOptions = *pOptions; } else { xrtSnappyOptionsInit(&tOptions); }
	pDecompressor = xrtSnappyDecompressorCreate(&tOptions);
	if ( pDecompressor == NULL ) {
		__xrtSnappySetError(pError, XRT_SNAPPY_ERROR_MEMORY, 0, iSize, 0, "cannot allocate snappy decompressor");
		return false;
	}
	bRet = xrtSnappyDecompressorUpdate(pDecompressor, pData, iSize) &&
		xrtSnappyDecompressorFinish(pDecompressor);
	if ( bRet ) { *ppOutput = xrtSnappyDecompressorTakeBuffer(pDecompressor); bRet = *ppOutput != NULL; }
	if ( !bRet ) { __xrtSnappyCopyError(pError, &pDecompressor->Error); }
	xrtSnappyDecompressorDestroy(pDecompressor);
	return bRet;
}



// ------------------------------------ 文件流接口 ------------------------------------

XRT_API bool xrtSnappyCompressStream(xreader* pReader, xwriter* pWriter, const xrtsnappyoptions* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtsnappyerror* pError)
{
	uint8 sInput[XRT_SNAPPY_CHUNK];
	xrtsnappycompressor* pCompressor;
	uint64 iTotal = 0u;
	bool bTotalKnown;
	bool bRet = true;
	xrtSnappyErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		__xrtSnappySetError(pError, XRT_SNAPPY_ERROR_ARGUMENT, 0, 0, 0, "invalid snappy stream");
		return false;
	}
	bTotalKnown = xrtReaderCanSeek(pReader) && xrtReaderSize(pReader, &iTotal);
	if ( !bTotalKnown ) { xrtClearError(); }
	pCompressor = xrtSnappyCompressorCreate(pOptions);
	if ( pCompressor == NULL ) { return false; }
	while ( bRet ) {
		size_t iRead = 0u;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtSnappySetError(&pCompressor->Error, XRT_SNAPPY_ERROR_IO, 0,
				pCompressor->InputBytes, pCompressor->OutputBytes, "cannot read snappy input stream");
			bRet = false;
			break;
		}
		if ( iRead == 0u ) { break; }
		bRet = xrtSnappyCompressorUpdate(pCompressor, sInput, (size_t)iRead) &&
			xrtSnappyCompressorDrain(pCompressor, pWriter);
		if ( bRet && !xrtProgressReport(pProgress, pUserData, pCompressor->InputBytes,
			iTotal, pCompressor->OutputBytes,
			bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u) ) {
			__xrtSnappySetError(&pCompressor->Error, XRT_SNAPPY_ERROR_CANCELLED, 0,
				pCompressor->InputBytes, pCompressor->OutputBytes, "snappy operation cancelled");
			bRet = false;
		}
	}
	if ( bRet ) { bRet = xrtSnappyCompressorFinish(pCompressor) && xrtSnappyCompressorDrain(pCompressor, pWriter); }
	if ( bRet && !xrtProgressReport(pProgress, pUserData, pCompressor->InputBytes,
		iTotal, pCompressor->OutputBytes, XRT_PROGRESS_FINAL |
		(bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u)) ) {
		__xrtSnappySetError(&pCompressor->Error, XRT_SNAPPY_ERROR_CANCELLED, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "snappy operation cancelled");
		bRet = false;
	}
	if ( !bRet ) { __xrtSnappyCopyError(pError, &pCompressor->Error); }
	xrtSnappyCompressorDestroy(pCompressor);
	return bRet;
}

XRT_API bool xrtSnappyDecompressStream(xreader* pReader, xwriter* pWriter, const xrtsnappyoptions* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtsnappyerror* pError)
{
	uint8 sInput[XRT_SNAPPY_CHUNK];
	xrtsnappydecompressor* pDecompressor;
	uint64 iTotal = 0u;
	bool bTotalKnown;
	bool bRet = true;
	xrtSnappyErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		__xrtSnappySetError(pError, XRT_SNAPPY_ERROR_ARGUMENT, 0, 0, 0, "invalid snappy stream");
		return false;
	}
	bTotalKnown = xrtReaderCanSeek(pReader) && xrtReaderSize(pReader, &iTotal);
	if ( !bTotalKnown ) { xrtClearError(); }
	pDecompressor = xrtSnappyDecompressorCreate(pOptions);
	if ( pDecompressor == NULL ) { return false; }
	while ( bRet ) {
		size_t iRead = 0u;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtSnappySetError(&pDecompressor->Error, XRT_SNAPPY_ERROR_IO, 0,
				pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot read snappy input stream");
			bRet = false;
			break;
		}
		if ( iRead == 0u ) { break; }
		bRet = xrtSnappyDecompressorUpdate(pDecompressor, sInput, (size_t)iRead) &&
			xrtSnappyDecompressorDrain(pDecompressor, pWriter);
		if ( bRet && !xrtProgressReport(pProgress, pUserData, pDecompressor->InputBytes,
			iTotal, pDecompressor->OutputBytes,
			bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u) ) {
			__xrtSnappySetError(&pDecompressor->Error, XRT_SNAPPY_ERROR_CANCELLED, 0,
				pDecompressor->InputBytes, pDecompressor->OutputBytes, "snappy operation cancelled");
			bRet = false;
		}
	}
	if ( bRet ) { bRet = xrtSnappyDecompressorFinish(pDecompressor) && xrtSnappyDecompressorDrain(pDecompressor, pWriter); }
	if ( bRet && !xrtProgressReport(pProgress, pUserData, pDecompressor->InputBytes,
		iTotal, pDecompressor->OutputBytes, XRT_PROGRESS_FINAL |
		(bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u)) ) {
		__xrtSnappySetError(&pDecompressor->Error, XRT_SNAPPY_ERROR_CANCELLED, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "snappy operation cancelled");
		bRet = false;
	}
	if ( !bRet ) { __xrtSnappyCopyError(pError, &pDecompressor->Error); }
	xrtSnappyDecompressorDestroy(pDecompressor);
	return bRet;
}
