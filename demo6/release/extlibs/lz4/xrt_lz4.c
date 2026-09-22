#include "include/xrt_lz4.h"
#include "third_party/lz4.h"

#include <stdarg.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>



/* 单个块的最大原始字节数。LZ4 块上限为 LZ4_MAX_INPUT_SIZE(0x7E000000)，
 * 这里取 1MB 以便流式产出粒度适中、内存占用可控。 */
#define XRT_LZ4_BLOCK_SIZE 1048576u
#define XRT_LZ4_CHUNK 65536u
#define XRT_LZ4_HEADER_SIZE 12u

/* 容器格式（自定义多块成员流，支持串联多成员）：
 *   成员 = 块* + 结束标记
 *   块   = u32le(原始大小 n, n>0) + u32le(压缩大小 c) + u32le(原始数据 CRC32) + LZ4 压缩字节(c 字节)
 *   结束 = u32le(0)
 * 压缩块实际长度通常小于 LZ4_compressBound(n)，故必须显式记录 c；
 * LZ4 块级 API 无内容校验和，故额外记录原始数据 CRC32 以检出损坏，
 * 解压侧据此精确定位块边界、校验完整性并统计 members。 */

struct xrtlz4compressor {
	xbuffer* Pending;            /* 尚未成块的原始输入 */
	xbuffer* Output;             /* 已产出的压缩字节 */
	xrtlz4options Options;
	xrtlz4error Error;
	uint64 InputBytes;
	uint64 OutputBytes;
	bool Finished;
	bool Failed;
};

typedef enum {
	XRT_LZ4_DEC_HEADER = 0,     /* 读取 4 字节块大小 */
	XRT_LZ4_DEC_BLOCK = 1       /* 读取压缩块体 */
} xrtlz4decstate;

struct xrtlz4decompressor {
	xbuffer* Carry;              /* 尚未消费完的输入字节 */
	xbuffer* Output;             /* 已产出的原始字节 */
	xrtlz4options Options;
	xrtlz4error Error;
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



static void __xrtLz4SetError(xrtlz4error* pError, int iCode, int iBackend,
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

static void __xrtLz4CopyError(xrtlz4error* pDst, const xrtlz4error* pSrc)
{
	if ( pDst == NULL || pSrc == NULL ) { return; }
	*pDst = *pSrc;
}

static bool __xrtLz4OverRatio(const xrtresourcelimits* pLimits, uint64 iInput, uint64 iOutput)
{
	uint64 iRatio = pLimits->iMaxCompressionRatio;
	if ( iRatio == 0u || iInput == 0u ) { return false; }
	if ( iInput > (UINT64_MAX / iRatio) ) { return false; }
	return iOutput > iInput * iRatio;
}

/* 检查计数累加是否溢出或越过非零硬上限。 */
static bool __xrtLz4AddOverLimit(uint64 iCurrent, size_t iAdded, uint64 iLimit)
{
	uint64 iIncrement = (uint64)iAdded;

	if ( iIncrement > (UINT64_MAX - iCurrent) ) { return true; }
	return (iLimit != 0u) && ((iCurrent + iIncrement) > iLimit);
}

/* 创建带初始容量的 Buffer，失败时不泄漏半初始化对象。 */
static xbuffer* __xrtLz4BufferCreate(size_t iCapacity)
{
	xbuffer* pBuffer = xrtBufferCreate();

	if ( (pBuffer != NULL) && !xrtBufferReserve(pBuffer, iCapacity) ) {
		xrtBufferDestroy(pBuffer);
		pBuffer = NULL;
	}
	return pBuffer;
}

static void __xrtLz4WriteU32LE(uint8* pDst, uint32 iValue)
{
	pDst[0] = (uint8)(iValue & 0xffu);
	pDst[1] = (uint8)((iValue >> 8) & 0xffu);
	pDst[2] = (uint8)((iValue >> 16) & 0xffu);
	pDst[3] = (uint8)((iValue >> 24) & 0xffu);
}

static uint32 __xrtLz4ReadU32LE(const uint8* pSrc)
{
	return (uint32)pSrc[0] | ((uint32)pSrc[1] << 8) |
		((uint32)pSrc[2] << 16) | ((uint32)pSrc[3] << 24);
}

/* 标准 CRC32（IEEE 802.3，多项式 0xEDB88320，反射输入输出，异或 0xFFFFFFFF）。
 * 逐位计算，无需查表，仅用于块级完整性校验，性能需求不高。 */
static uint32 __xrtLz4Crc32(const void* pData, size_t iLen)
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



XRT_API void xrtLz4ErrorInit(xrtlz4error* pError)
{
	if ( pError == NULL ) { return; }
	memset(pError, 0, sizeof(*pError));
	pError->iCode = XRT_LZ4_OK;
}

XRT_API void xrtLz4OptionsInit(xrtlz4options* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_LZ4_OPTIONS_VERSION;
	pOptions->iAcceleration = 1;
	xrtResourceLimitsInit(&pOptions->tLimits);
}



// ------------------------------------ 压缩器 ------------------------------------

XRT_API xrtlz4compressor* xrtLz4CompressorCreate(const xrtlz4options* pOptions)
{
	xrtlz4compressor* pCompressor;
	pCompressor = (xrtlz4compressor*)xrtMalloc(sizeof(xrtlz4compressor));
	if ( pCompressor == NULL ) { return NULL; }
	memset(pCompressor, 0, sizeof(*pCompressor));
	xrtLz4ErrorInit(&pCompressor->Error);
	if ( pOptions != NULL ) {
		pCompressor->Options = *pOptions;
	} else {
		xrtLz4OptionsInit(&pCompressor->Options);
	}
	if ( pCompressor->Options.iAcceleration < 1 ) { pCompressor->Options.iAcceleration = 1; }
	pCompressor->Pending = __xrtLz4BufferCreate(XRT_LZ4_CHUNK);
	pCompressor->Output = __xrtLz4BufferCreate(XRT_LZ4_CHUNK);
	if ( pCompressor->Pending == NULL || pCompressor->Output == NULL ) {
		__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_MEMORY, 0, 0, 0,
			"cannot allocate lz4 compressor");
		pCompressor->Failed = true;
		return pCompressor;
	}
	return pCompressor;
}

/* 将 Pending 中恰好 iSize 字节压缩为一个块（大小头 + 压缩体）写入 Output。 */
static bool __xrtLz4FlushBlock(xrtlz4compressor* pCompressor, size_t iSize)
{
	int iBound;
	int iWritten;
	uint8 sHeader[XRT_LZ4_HEADER_SIZE];
	ptr pCompressed;
	xbytesview Pending;
	size_t iGenerated;
	if ( iSize == 0u ) { return true; }
	Pending = xrtBufferView(pCompressor->Pending);
	if ( iSize > Pending.Size ) {
		__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_STATE, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "invalid lz4 pending state");
		pCompressor->Failed = true;
		return false;
	}
	iBound = LZ4_compressBound((int)iSize);
	if ( iBound <= 0 ) {
		__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_ARGUMENT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "lz4 block too large");
		pCompressor->Failed = true;
		return false;
	}
	pCompressed = xrtMalloc((size_t)iBound);
	if ( pCompressed == NULL ) {
		__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "out of memory");
		pCompressor->Failed = true;
		return false;
	}
	iWritten = LZ4_compress_fast((const char*)Pending.Data,
		(char*)pCompressed, (int)iSize, iBound, pCompressor->Options.iAcceleration);
	if ( iWritten <= 0 ) {
		__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_DATA, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "lz4 block compression failed");
		xrtFree(pCompressed);
		pCompressor->Failed = true;
		return false;
	}
	__xrtLz4WriteU32LE(sHeader, (uint32)iSize);
	__xrtLz4WriteU32LE(sHeader + 4u, (uint32)iWritten);
	__xrtLz4WriteU32LE(sHeader + 8u,
		__xrtLz4Crc32(Pending.Data, iSize));
	iGenerated = XRT_LZ4_HEADER_SIZE + (size_t)iWritten;
	if ( __xrtLz4AddOverLimit(pCompressor->OutputBytes, iGenerated,
		pCompressor->Options.tLimits.iMaxOutputBytes) ) {
		__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_LIMIT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "lz4 output exceeds maxOutput limit");
		xrtFree(pCompressed);
		pCompressor->Failed = true;
		return false;
	}
	if ( !xrtBufferAppend(pCompressor->Output,
			(xbytesview){ (const unsigned char*)sHeader, XRT_LZ4_HEADER_SIZE }) ||
		 !xrtBufferAppend(pCompressor->Output,
			(xbytesview){ (const unsigned char*)pCompressed, (size_t)iWritten }) ) {
		__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow lz4 output buffer");
		xrtFree(pCompressed);
		pCompressor->Failed = true;
		return false;
	}
	xrtFree(pCompressed);
	pCompressor->OutputBytes += (uint64)iGenerated;
	if ( !xrtBufferRemove(pCompressor->Pending, 0u, iSize) ) {
		__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_STATE, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot consume lz4 pending buffer");
		pCompressor->Failed = true;
		return false;
	}
	return true;
}

XRT_API bool xrtLz4CompressorUpdate(xrtlz4compressor* pCompressor, const void* pData, size_t iSize)
{
	if ( pCompressor == NULL || pCompressor->Failed ) { return false; }
	if ( pCompressor->Finished ) {
		__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_STATE, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "lz4 compressor already finished");
		pCompressor->Failed = true;
		return false;
	}
	if ( iSize != 0u && pData == NULL ) {
		__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_ARGUMENT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "invalid lz4 input");
		pCompressor->Failed = true;
		return false;
	}
	if ( __xrtLz4AddOverLimit(pCompressor->InputBytes, iSize,
		pCompressor->Options.tLimits.iMaxInputBytes) ) {
		__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_LIMIT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "lz4 input exceeds maxInput limit");
		pCompressor->Failed = true;
		return false;
	}
	if ( iSize != 0u &&
		!xrtBufferAppend(pCompressor->Pending,
			(xbytesview){ (const unsigned char*)pData, iSize }) ) {
		__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow lz4 pending buffer");
		pCompressor->Failed = true;
		return false;
	}
	pCompressor->InputBytes += iSize;
	while ( xrtBufferView(pCompressor->Pending).Size >= XRT_LZ4_BLOCK_SIZE ) {
		if ( !__xrtLz4FlushBlock(pCompressor, XRT_LZ4_BLOCK_SIZE) ) { return false; }
	}
	return true;
}

XRT_API bool xrtLz4CompressorFinish(xrtlz4compressor* pCompressor)
{
	uint8 sEnd[XRT_LZ4_HEADER_SIZE] = { 0 };
	size_t iPending;
	if ( pCompressor == NULL || pCompressor->Failed ) { return false; }
	if ( pCompressor->Finished ) { return true; }
	iPending = xrtBufferView(pCompressor->Pending).Size;
	if ( iPending != 0u ) {
		if ( !__xrtLz4FlushBlock(pCompressor, iPending) ) { return false; }
	}
	if ( __xrtLz4AddOverLimit(pCompressor->OutputBytes, XRT_LZ4_HEADER_SIZE,
		pCompressor->Options.tLimits.iMaxOutputBytes) ) {
		__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_LIMIT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "lz4 output exceeds maxOutput limit");
		pCompressor->Failed = true;
		return false;
	}
	if ( !xrtBufferAppend(pCompressor->Output,
		(xbytesview){ (const unsigned char*)sEnd, sizeof(sEnd) }) ) {
		__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow lz4 output buffer");
		pCompressor->Failed = true;
		return false;
	}
	pCompressor->OutputBytes += XRT_LZ4_HEADER_SIZE;
	pCompressor->Finished = true;
	return true;
}

XRT_API bool xrtLz4CompressorDrain(xrtlz4compressor* pCompressor, xwriter* pWriter)
{
	xbytesview Output;
	if ( pCompressor == NULL || pCompressor->Failed || pWriter == NULL ) { return false; }
	Output = xrtBufferView(pCompressor->Output);
	if ( Output.Size != 0u &&
		!xrtWriterWriteFull(pWriter, Output.Data, Output.Size, NULL) ) {
		__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_IO, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot write lz4 output stream");
		pCompressor->Failed = true;
		return false;
	}
	xrtBufferClear(pCompressor->Output);
	return true;
}

XRT_API xbuffer* xrtLz4CompressorTakeBuffer(xrtlz4compressor* pCompressor)
{
	xbuffer* pOutput;
	if ( pCompressor == NULL || pCompressor->Failed || !pCompressor->Finished ) { return NULL; }
	pOutput = pCompressor->Output;
	pCompressor->Output = NULL;
	return pOutput;
}

XRT_API xbuffer* xrtLz4CompressorDrainBuffer(xrtlz4compressor* pCompressor)
{
	xbuffer* pOutput;
	if ( pCompressor == NULL || pCompressor->Failed || pCompressor->Output == NULL ) { return NULL; }
	pOutput = xrtBufferFrom(xrtBufferView(pCompressor->Output));
	if ( pOutput != NULL ) { xrtBufferClear(pCompressor->Output); }
	return pOutput;
}

XRT_API uint64 xrtLz4CompressorInputBytes(const xrtlz4compressor* pCompressor)
{
	return pCompressor != NULL ? pCompressor->InputBytes : 0u;
}

XRT_API uint64 xrtLz4CompressorOutputBytes(const xrtlz4compressor* pCompressor)
{
	return pCompressor != NULL ? pCompressor->OutputBytes : 0u;
}

XRT_API const xrtlz4error* xrtLz4CompressorError(const xrtlz4compressor* pCompressor)
{
	return pCompressor != NULL ? &pCompressor->Error : NULL;
}

XRT_API void xrtLz4CompressorDestroy(xrtlz4compressor* pCompressor)
{
	if ( pCompressor == NULL ) { return; }
	xrtBufferDestroy(pCompressor->Pending);
	xrtBufferDestroy(pCompressor->Output);
	xrtFree(pCompressor);
}



// ------------------------------------ 解压器 ------------------------------------

XRT_API xrtlz4decompressor* xrtLz4DecompressorCreate(const xrtlz4options* pOptions)
{
	xrtlz4decompressor* pDecompressor;
	pDecompressor = (xrtlz4decompressor*)xrtMalloc(sizeof(xrtlz4decompressor));
	if ( pDecompressor == NULL ) { return NULL; }
	memset(pDecompressor, 0, sizeof(*pDecompressor));
	xrtLz4ErrorInit(&pDecompressor->Error);
	if ( pOptions != NULL ) {
		pDecompressor->Options = *pOptions;
	} else {
		xrtLz4OptionsInit(&pDecompressor->Options);
	}
	pDecompressor->Carry = __xrtLz4BufferCreate(XRT_LZ4_CHUNK);
	pDecompressor->Output = __xrtLz4BufferCreate(XRT_LZ4_CHUNK);
	if ( pDecompressor->Carry == NULL || pDecompressor->Output == NULL ) {
		__xrtLz4SetError(&pDecompressor->Error, XRT_LZ4_ERROR_MEMORY, 0, 0, 0,
			"cannot allocate lz4 decompressor");
		pDecompressor->Failed = true;
		return pDecompressor;
	}
	pDecompressor->State = XRT_LZ4_DEC_HEADER;
	return pDecompressor;
}

/* 消费 Carry 中的输入；*pConsumed 返回实际消费字节数。 */
static bool __xrtLz4DecompressConsume(xrtlz4decompressor* pDecompressor, size_t* pConsumed)
{
	size_t iPos = 0u;
	xbytesview Carry = xrtBufferView(pDecompressor->Carry);
	const uint8* pBuf = (const uint8*)Carry.Data;
	size_t iLen = Carry.Size;
	while ( iPos < iLen ) {
		if ( pDecompressor->State == XRT_LZ4_DEC_HEADER ) {
			uint32 iDeclared;
			uint32 iCompressed;
			if ( iLen - iPos < XRT_LZ4_HEADER_SIZE ) { break; }
			iDeclared = __xrtLz4ReadU32LE(pBuf + iPos);
			iCompressed = __xrtLz4ReadU32LE(pBuf + iPos + 4u);
			iPos += XRT_LZ4_HEADER_SIZE;
			if ( iDeclared == 0u ) {
				pDecompressor->MemberEnded = true;
				pDecompressor->MemberCount += 1u;
				continue;   /* 可能紧跟下一个成员 */
			}
			pDecompressor->MemberEnded = false;
			pDecompressor->BlockDeclared = (size_t)iDeclared;
			pDecompressor->BlockCompressed = (size_t)iCompressed;
			pDecompressor->BlockCrc = __xrtLz4ReadU32LE(pBuf + iPos - 4u);
			pDecompressor->State = XRT_LZ4_DEC_BLOCK;
		} else {
			int iWritten;
			ptr pPlain;
			if ( (pDecompressor->BlockDeclared > LZ4_MAX_INPUT_SIZE) ||
				 (pDecompressor->BlockCompressed == 0u) ||
				 (pDecompressor->BlockCompressed > INT_MAX) ) {
				__xrtLz4SetError(&pDecompressor->Error, XRT_LZ4_ERROR_DATA, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"lz4 block declares too large output");
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			if ( iLen - iPos < pDecompressor->BlockCompressed ) { break; }
			if ( ((pDecompressor->Options.tLimits.iMaxItemBytes != 0u) &&
				  (pDecompressor->BlockDeclared > pDecompressor->Options.tLimits.iMaxItemBytes)) ||
				 __xrtLz4AddOverLimit(pDecompressor->OutputBytes,
					pDecompressor->BlockDeclared,
					pDecompressor->Options.tLimits.iMaxOutputBytes) ||
				 __xrtLz4OverRatio(&pDecompressor->Options.tLimits,
					pDecompressor->InputBytes,
					pDecompressor->OutputBytes + (uint64)pDecompressor->BlockDeclared) ) {
				__xrtLz4SetError(&pDecompressor->Error, XRT_LZ4_ERROR_LIMIT, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"lz4 output exceeds decompression limit");
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			pPlain = xrtMalloc(pDecompressor->BlockDeclared);
			if ( pPlain == NULL ) {
				__xrtLz4SetError(&pDecompressor->Error, XRT_LZ4_ERROR_MEMORY, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes, "out of memory");
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			iWritten = LZ4_decompress_safe((const char*)(pBuf + iPos), (char*)pPlain,
				(int)pDecompressor->BlockCompressed, (int)pDecompressor->BlockDeclared);
			if ( iWritten < 0 || (size_t)iWritten != pDecompressor->BlockDeclared ) {
				__xrtLz4SetError(&pDecompressor->Error, XRT_LZ4_ERROR_DATA, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"lz4 block decompression failed");
				xrtFree(pPlain);
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			iPos += pDecompressor->BlockCompressed;
			if ( __xrtLz4Crc32(pPlain, (size_t)iWritten) != pDecompressor->BlockCrc ) {
				__xrtLz4SetError(&pDecompressor->Error, XRT_LZ4_ERROR_DATA, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"lz4 block checksum mismatch");
				xrtFree(pPlain);
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			if ( !xrtBufferAppend(pDecompressor->Output,
				(xbytesview){ (const unsigned char*)pPlain, (size_t)iWritten }) ) {
				__xrtLz4SetError(&pDecompressor->Error, XRT_LZ4_ERROR_MEMORY, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"cannot grow lz4 output buffer");
				xrtFree(pPlain);
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			xrtFree(pPlain);
			pDecompressor->OutputBytes += (uint64)iWritten;
			pDecompressor->State = XRT_LZ4_DEC_HEADER;
		}
	}
	*pConsumed = iPos;
	return true;
}

XRT_API bool xrtLz4DecompressorUpdate(xrtlz4decompressor* pDecompressor, const void* pData, size_t iSize)
{
	size_t iConsumed = 0u;
	if ( pDecompressor == NULL || pDecompressor->Failed ) { return false; }
	if ( iSize != 0u && pData == NULL ) {
		__xrtLz4SetError(&pDecompressor->Error, XRT_LZ4_ERROR_ARGUMENT, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "invalid lz4 input");
		pDecompressor->Failed = true;
		return false;
	}
	if ( __xrtLz4AddOverLimit(pDecompressor->InputBytes, iSize,
		pDecompressor->Options.tLimits.iMaxInputBytes) ) {
		__xrtLz4SetError(&pDecompressor->Error, XRT_LZ4_ERROR_LIMIT, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "lz4 input exceeds maxInput limit");
		pDecompressor->Failed = true;
		return false;
	}
	if ( iSize != 0u &&
		!xrtBufferAppend(pDecompressor->Carry,
			(xbytesview){ (const unsigned char*)pData, iSize }) ) {
		__xrtLz4SetError(&pDecompressor->Error, XRT_LZ4_ERROR_MEMORY, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot grow lz4 input buffer");
		pDecompressor->Failed = true;
		return false;
	}
	pDecompressor->MemberEnded = false;
	pDecompressor->InputBytes += iSize;
	if ( !__xrtLz4DecompressConsume(pDecompressor, &iConsumed) ) { return false; }
	if ( (iConsumed != 0u) &&
		 !xrtBufferRemove(pDecompressor->Carry, 0u, iConsumed) ) {
		__xrtLz4SetError(&pDecompressor->Error, XRT_LZ4_ERROR_STATE, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot consume lz4 input buffer");
		pDecompressor->Failed = true;
		return false;
	}
	return true;
}

XRT_API bool xrtLz4DecompressorFinish(xrtlz4decompressor* pDecompressor)
{
	if ( pDecompressor == NULL || pDecompressor->Failed ) { return false; }
	if ( pDecompressor->Finished ) { return true; }
	if ( (pDecompressor->MemberCount == 0u) || !pDecompressor->MemberEnded ||
		(pDecompressor->State != XRT_LZ4_DEC_HEADER) ||
		(xrtBufferView(pDecompressor->Carry).Size != 0u) ) {
		__xrtLz4SetError(&pDecompressor->Error, XRT_LZ4_ERROR_TRUNCATED, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "lz4 stream is truncated or empty");
		pDecompressor->Failed = true;
		return false;
	}
	pDecompressor->Finished = true;
	return true;
}

XRT_API bool xrtLz4DecompressorDrain(xrtlz4decompressor* pDecompressor, xwriter* pWriter)
{
	xbytesview Output;
	if ( pDecompressor == NULL || pDecompressor->Failed || pWriter == NULL ) { return false; }
	Output = xrtBufferView(pDecompressor->Output);
	if ( Output.Size != 0u &&
		!xrtWriterWriteFull(pWriter, Output.Data, Output.Size, NULL) ) {
		__xrtLz4SetError(&pDecompressor->Error, XRT_LZ4_ERROR_IO, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot write lz4 output stream");
		pDecompressor->Failed = true;
		return false;
	}
	xrtBufferClear(pDecompressor->Output);
	return true;
}

XRT_API xbuffer* xrtLz4DecompressorTakeBuffer(xrtlz4decompressor* pDecompressor)
{
	xbuffer* pOutput;
	if ( pDecompressor == NULL || pDecompressor->Failed || !pDecompressor->Finished ) { return NULL; }
	pOutput = pDecompressor->Output;
	pDecompressor->Output = NULL;
	return pOutput;
}

XRT_API xbuffer* xrtLz4DecompressorDrainBuffer(xrtlz4decompressor* pDecompressor)
{
	xbuffer* pOutput;
	if ( pDecompressor == NULL || pDecompressor->Failed || pDecompressor->Output == NULL ) { return NULL; }
	pOutput = xrtBufferFrom(xrtBufferView(pDecompressor->Output));
	if ( pOutput != NULL ) { xrtBufferClear(pDecompressor->Output); }
	return pOutput;
}

XRT_API uint64 xrtLz4DecompressorInputBytes(const xrtlz4decompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->InputBytes : 0u;
}

XRT_API uint64 xrtLz4DecompressorOutputBytes(const xrtlz4decompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->OutputBytes : 0u;
}

XRT_API bool xrtLz4DecompressorFinished(const xrtlz4decompressor* pDecompressor)
{
	return pDecompressor != NULL && pDecompressor->MemberEnded;
}

XRT_API uint64 xrtLz4DecompressorMembers(const xrtlz4decompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->MemberCount : 0u;
}

XRT_API const xrtlz4error* xrtLz4DecompressorError(const xrtlz4decompressor* pDecompressor)
{
	return pDecompressor != NULL ? &pDecompressor->Error : NULL;
}

XRT_API void xrtLz4DecompressorDestroy(xrtlz4decompressor* pDecompressor)
{
	if ( pDecompressor == NULL ) { return; }
	xrtBufferDestroy(pDecompressor->Carry);
	xrtBufferDestroy(pDecompressor->Output);
	xrtFree(pDecompressor);
}



// ------------------------------------ 一次性接口 ------------------------------------

XRT_API bool xrtLz4Compress(const void* pData, size_t iSize, const xrtlz4options* pOptions,
	xbuffer** ppOutput, xrtlz4error* pError)
{
	xrtlz4options tOptions;
	xrtlz4compressor* pCompressor;
	bool bRet;
	xrtLz4ErrorInit(pError);
	if ( ppOutput != NULL ) { *ppOutput = NULL; }
	if ( ppOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtLz4SetError(pError, XRT_LZ4_ERROR_ARGUMENT, 0, 0, 0, "invalid lz4 input");
		return false;
	}
	if ( pOptions != NULL ) { tOptions = *pOptions; } else { xrtLz4OptionsInit(&tOptions); }
	pCompressor = xrtLz4CompressorCreate(&tOptions);
	if ( pCompressor == NULL ) {
		__xrtLz4SetError(pError, XRT_LZ4_ERROR_MEMORY, 0, iSize, 0, "cannot allocate lz4 compressor");
		return false;
	}
	bRet = xrtLz4CompressorUpdate(pCompressor, pData, iSize) &&
		xrtLz4CompressorFinish(pCompressor);
	if ( bRet ) { *ppOutput = xrtLz4CompressorTakeBuffer(pCompressor); bRet = *ppOutput != NULL; }
	if ( !bRet ) { __xrtLz4CopyError(pError, &pCompressor->Error); }
	xrtLz4CompressorDestroy(pCompressor);
	return bRet;
}

XRT_API bool xrtLz4Decompress(const void* pData, size_t iSize, const xrtlz4options* pOptions,
	xbuffer** ppOutput, xrtlz4error* pError)
{
	xrtlz4options tOptions;
	xrtlz4decompressor* pDecompressor;
	bool bRet;
	xrtLz4ErrorInit(pError);
	if ( ppOutput != NULL ) { *ppOutput = NULL; }
	if ( ppOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtLz4SetError(pError, XRT_LZ4_ERROR_ARGUMENT, 0, 0, 0, "invalid lz4 input");
		return false;
	}
	if ( pOptions != NULL ) { tOptions = *pOptions; } else { xrtLz4OptionsInit(&tOptions); }
	pDecompressor = xrtLz4DecompressorCreate(&tOptions);
	if ( pDecompressor == NULL ) {
		__xrtLz4SetError(pError, XRT_LZ4_ERROR_MEMORY, 0, iSize, 0, "cannot allocate lz4 decompressor");
		return false;
	}
	bRet = xrtLz4DecompressorUpdate(pDecompressor, pData, iSize) &&
		xrtLz4DecompressorFinish(pDecompressor);
	if ( bRet ) { *ppOutput = xrtLz4DecompressorTakeBuffer(pDecompressor); bRet = *ppOutput != NULL; }
	if ( !bRet ) { __xrtLz4CopyError(pError, &pDecompressor->Error); }
	xrtLz4DecompressorDestroy(pDecompressor);
	return bRet;
}



// ------------------------------------ 文件流接口 ------------------------------------

XRT_API bool xrtLz4CompressStream(xreader* pReader, xwriter* pWriter, const xrtlz4options* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtlz4error* pError)
{
	uint8 sInput[XRT_LZ4_CHUNK];
	xrtlz4compressor* pCompressor;
	uint64 iTotal = 0u;
	bool bTotalKnown;
	bool bRet = true;
	xrtLz4ErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		__xrtLz4SetError(pError, XRT_LZ4_ERROR_ARGUMENT, 0, 0, 0, "invalid lz4 stream");
		return false;
	}
	bTotalKnown = xrtReaderCanSeek(pReader) && xrtReaderSize(pReader, &iTotal);
	if ( !bTotalKnown ) { xrtClearError(); }
	pCompressor = xrtLz4CompressorCreate(pOptions);
	if ( pCompressor == NULL ) { return false; }
	while ( bRet ) {
		size_t iRead = 0u;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_IO, 0,
				pCompressor->InputBytes, pCompressor->OutputBytes, "cannot read lz4 input stream");
			bRet = false;
			break;
		}
		if ( iRead == 0u ) { break; }
		bRet = xrtLz4CompressorUpdate(pCompressor, sInput, (size_t)iRead) &&
			xrtLz4CompressorDrain(pCompressor, pWriter);
		if ( bRet && !xrtProgressReport(pProgress, pUserData, pCompressor->InputBytes,
			iTotal, pCompressor->OutputBytes,
			bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u) ) {
			__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_CANCELLED, 0,
				pCompressor->InputBytes, pCompressor->OutputBytes, "lz4 operation cancelled");
			bRet = false;
		}
	}
	if ( bRet ) { bRet = xrtLz4CompressorFinish(pCompressor) && xrtLz4CompressorDrain(pCompressor, pWriter); }
	if ( bRet && !xrtProgressReport(pProgress, pUserData, pCompressor->InputBytes,
		iTotal, pCompressor->OutputBytes, XRT_PROGRESS_FINAL |
		(bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u)) ) {
		__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_CANCELLED, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "lz4 operation cancelled");
		bRet = false;
	}
	if ( !bRet ) { __xrtLz4CopyError(pError, &pCompressor->Error); }
	xrtLz4CompressorDestroy(pCompressor);
	return bRet;
}

XRT_API bool xrtLz4DecompressStream(xreader* pReader, xwriter* pWriter, const xrtlz4options* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtlz4error* pError)
{
	uint8 sInput[XRT_LZ4_CHUNK];
	xrtlz4decompressor* pDecompressor;
	uint64 iTotal = 0u;
	bool bTotalKnown;
	bool bRet = true;
	xrtLz4ErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		__xrtLz4SetError(pError, XRT_LZ4_ERROR_ARGUMENT, 0, 0, 0, "invalid lz4 stream");
		return false;
	}
	bTotalKnown = xrtReaderCanSeek(pReader) && xrtReaderSize(pReader, &iTotal);
	if ( !bTotalKnown ) { xrtClearError(); }
	pDecompressor = xrtLz4DecompressorCreate(pOptions);
	if ( pDecompressor == NULL ) { return false; }
	while ( bRet ) {
		size_t iRead = 0u;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtLz4SetError(&pDecompressor->Error, XRT_LZ4_ERROR_IO, 0,
				pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot read lz4 input stream");
			bRet = false;
			break;
		}
		if ( iRead == 0u ) { break; }
		bRet = xrtLz4DecompressorUpdate(pDecompressor, sInput, (size_t)iRead) &&
			xrtLz4DecompressorDrain(pDecompressor, pWriter);
		if ( bRet && !xrtProgressReport(pProgress, pUserData, pDecompressor->InputBytes,
			iTotal, pDecompressor->OutputBytes,
			bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u) ) {
			__xrtLz4SetError(&pDecompressor->Error, XRT_LZ4_ERROR_CANCELLED, 0,
				pDecompressor->InputBytes, pDecompressor->OutputBytes, "lz4 operation cancelled");
			bRet = false;
		}
	}
	if ( bRet ) { bRet = xrtLz4DecompressorFinish(pDecompressor) && xrtLz4DecompressorDrain(pDecompressor, pWriter); }
	if ( bRet && !xrtProgressReport(pProgress, pUserData, pDecompressor->InputBytes,
		iTotal, pDecompressor->OutputBytes, XRT_PROGRESS_FINAL |
		(bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u)) ) {
		__xrtLz4SetError(&pDecompressor->Error, XRT_LZ4_ERROR_CANCELLED, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "lz4 operation cancelled");
		bRet = false;
	}
	if ( !bRet ) { __xrtLz4CopyError(pError, &pDecompressor->Error); }
	xrtLz4DecompressorDestroy(pDecompressor);
	return bRet;
}
