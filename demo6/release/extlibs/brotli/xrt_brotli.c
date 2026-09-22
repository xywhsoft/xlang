#include "include/xrt_brotli.h"
#include "third_party/include/brotli/encode.h"
#include "third_party/include/brotli/decode.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>



/* 单个块最大原始字节数。brotli 一次性 API 以 size_t 传递长度，
 * 这里取 1MB 以便流式产出粒度适中、内存占用可控。 */
#define XRT_BROTLI_BLOCK_SIZE 1048576u
#define XRT_BROTLI_CHUNK 65536u
#define XRT_BROTLI_HEADER_SIZE 12u

/* 容器格式（自定义多块成员流，支持串联多成员）：
 *   成员 = 块* + 结束标记
 *   块   = u32le(原始大小 n, n>0) + u32le(压缩大小 c) + u32le(原始数据 CRC32) + brotli 压缩字节(c 字节)
 *   结束 = u32le(0)
 * brotli 流自带完整性校验，但仍额外记录原始数据 CRC32 以双重检出损坏，
 * 解压侧据此精确定位块边界、校验完整性并统计 members。 */

struct xrtbrotlicompressor {
	xbuffer* Pending;            /* 尚未成块的原始输入 */
	xbuffer* Output;             /* 已产出的压缩字节 */
	xrtbrotlioptions Options;
	xrtbrotlierror Error;
	uint64 InputBytes;
	uint64 OutputBytes;
	bool Finished;
	bool Failed;
};

typedef enum {
	XRT_BROTLI_DEC_HEADER = 0,   /* 读取 12 字节块头 */
	XRT_BROTLI_DEC_BLOCK = 1     /* 读取压缩块体 */
} xrtbrotlidecstate;

struct xrtbrotlidecompressor {
	xbuffer* Carry;              /* 尚未消费完的输入字节 */
	xbuffer* Output;             /* 已产出的原始字节 */
	xrtbrotlioptions Options;
	xrtbrotlierror Error;
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



static void __xrtBrotliSetError(xrtbrotlierror* pError, int iCode, int iBackend,
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

static void __xrtBrotliCopyError(xrtbrotlierror* pDst, const xrtbrotlierror* pSrc)
{
	if ( pDst == NULL || pSrc == NULL ) { return; }
	*pDst = *pSrc;
}

static bool __xrtBrotliOverRatio(const xrtresourcelimits* pLimits, uint64 iInput, uint64 iOutput)
{
	uint64 iRatio = pLimits->iMaxCompressionRatio;
	if ( iRatio == 0u || iInput == 0u ) { return false; }
	if ( iInput > (UINT64_MAX / iRatio) ) { return false; }
	return iOutput > iInput * iRatio;
}

/* 检查计数累加是否溢出或越过非零硬上限。 */
static bool __xrtBrotliAddOverLimit(uint64 iCurrent, size_t iAdded, uint64 iLimit)
{
	uint64 iIncrement = (uint64)iAdded;

	if ( iIncrement > (UINT64_MAX - iCurrent) ) { return true; }
	return (iLimit != 0u) && ((iCurrent + iIncrement) > iLimit);
}

/* 创建带初始容量的 Buffer，失败时不泄漏半初始化对象。 */
static xbuffer* __xrtBrotliBufferCreate(size_t iCapacity)
{
	xbuffer* pBuffer = xrtBufferCreate();

	if ( (pBuffer != NULL) && !xrtBufferReserve(pBuffer, iCapacity) ) {
		xrtBufferDestroy(pBuffer);
		pBuffer = NULL;
	}
	return pBuffer;
}

static void __xrtBrotliWriteU32LE(uint8* pDst, uint32 iValue)
{
	pDst[0] = (uint8)(iValue & 0xffu);
	pDst[1] = (uint8)((iValue >> 8) & 0xffu);
	pDst[2] = (uint8)((iValue >> 16) & 0xffu);
	pDst[3] = (uint8)((iValue >> 24) & 0xffu);
}

static uint32 __xrtBrotliReadU32LE(const uint8* pSrc)
{
	return (uint32)pSrc[0] | ((uint32)pSrc[1] << 8) |
		((uint32)pSrc[2] << 16) | ((uint32)pSrc[3] << 24);
}

/* 标准 CRC32（IEEE 802.3，多项式 0xEDB88320，反射输入输出，异或 0xFFFFFFFF）。
 * 逐位计算，无需查表，仅用于块级完整性校验，性能需求不高。 */
static uint32 __xrtBrotliCrc32(const void* pData, size_t iLen)
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



XRT_API void xrtBrotliErrorInit(xrtbrotlierror* pError)
{
	if ( pError == NULL ) { return; }
	memset(pError, 0, sizeof(*pError));
	pError->iCode = XRT_BROTLI_OK;
}

XRT_API void xrtBrotliOptionsInit(xrtbrotlioptions* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_BROTLI_OPTIONS_VERSION;
	pOptions->iQuality = BROTLI_DEFAULT_QUALITY;
	pOptions->iWindow = BROTLI_DEFAULT_WINDOW;
	pOptions->iMode = BROTLI_DEFAULT_MODE;
	xrtResourceLimitsInit(&pOptions->tLimits);
}



// ------------------------------------ 压缩器 ------------------------------------

XRT_API xrtbrotlicompressor* xrtBrotliCompressorCreate(const xrtbrotlioptions* pOptions)
{
	xrtbrotlicompressor* pCompressor;
	pCompressor = (xrtbrotlicompressor*)xrtMalloc(sizeof(xrtbrotlicompressor));
	if ( pCompressor == NULL ) { return NULL; }
	memset(pCompressor, 0, sizeof(*pCompressor));
	xrtBrotliErrorInit(&pCompressor->Error);
	if ( pOptions != NULL ) {
		pCompressor->Options = *pOptions;
	} else {
		xrtBrotliOptionsInit(&pCompressor->Options);
	}
	pCompressor->Pending = __xrtBrotliBufferCreate(XRT_BROTLI_CHUNK);
	pCompressor->Output = __xrtBrotliBufferCreate(XRT_BROTLI_CHUNK);
	if ( pCompressor->Pending == NULL || pCompressor->Output == NULL ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_MEMORY, 0, 0, 0,
			"cannot allocate brotli compressor");
		pCompressor->Failed = true;
		return pCompressor;
	}
	return pCompressor;
}

/* 将 Pending 中恰好 iSize 字节压缩为一个块（大小头 + 压缩体）写入 Output。 */
static bool __xrtBrotliFlushBlock(xrtbrotlicompressor* pCompressor, size_t iSize)
{
	size_t iBound;
	size_t iWritten;
	uint8 sHeader[XRT_BROTLI_HEADER_SIZE];
	ptr pCompressed;
	xbytesview Pending;
	size_t iGenerated;
	if ( iSize == 0u ) { return true; }
	Pending = xrtBufferView(pCompressor->Pending);
	if ( iSize > Pending.Size ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_STATE, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "invalid brotli pending state");
		pCompressor->Failed = true;
		return false;
	}
	iBound = BrotliEncoderMaxCompressedSize(iSize);
	if ( iBound == 0u ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_ARGUMENT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "brotli block too large");
		pCompressor->Failed = true;
		return false;
	}
	pCompressed = xrtMalloc(iBound);
	if ( pCompressed == NULL ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "out of memory");
		pCompressor->Failed = true;
		return false;
	}
	iWritten = iBound;
	if ( BrotliEncoderCompress(pCompressor->Options.iQuality, pCompressor->Options.iWindow,
		(BrotliEncoderMode)pCompressor->Options.iMode, iSize,
		(const uint8*)Pending.Data, &iWritten,
		(uint8*)pCompressed) != BROTLI_TRUE ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_DATA, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "brotli compression failed");
		xrtFree(pCompressed);
		pCompressor->Failed = true;
		return false;
	}
	__xrtBrotliWriteU32LE(sHeader, (uint32)iSize);
	__xrtBrotliWriteU32LE(sHeader + 4u, (uint32)iWritten);
	__xrtBrotliWriteU32LE(sHeader + 8u,
		__xrtBrotliCrc32(Pending.Data, iSize));
	iGenerated = XRT_BROTLI_HEADER_SIZE + iWritten;
	if ( __xrtBrotliAddOverLimit(pCompressor->OutputBytes, iGenerated,
		pCompressor->Options.tLimits.iMaxOutputBytes) ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_LIMIT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "brotli output exceeds maxOutput limit");
		xrtFree(pCompressed);
		pCompressor->Failed = true;
		return false;
	}
	if ( iWritten > UINT32_MAX ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_DATA, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "brotli compressed block is too large");
		xrtFree(pCompressed);
		pCompressor->Failed = true;
		return false;
	}
	if ( !xrtBufferAppend(pCompressor->Output,
			(xbytesview){ (const unsigned char*)sHeader, XRT_BROTLI_HEADER_SIZE }) ||
		 !xrtBufferAppend(pCompressor->Output,
			(xbytesview){ (const unsigned char*)pCompressed, iWritten }) ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow brotli output buffer");
		xrtFree(pCompressed);
		pCompressor->Failed = true;
		return false;
	}
	xrtFree(pCompressed);
	pCompressor->OutputBytes += (uint64)iGenerated;
	if ( !xrtBufferRemove(pCompressor->Pending, 0u, iSize) ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_STATE, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot consume brotli pending buffer");
		pCompressor->Failed = true;
		return false;
	}
	return true;
}

XRT_API bool xrtBrotliCompressorUpdate(xrtbrotlicompressor* pCompressor, const void* pData, size_t iSize)
{
	if ( pCompressor == NULL || pCompressor->Failed ) { return false; }
	if ( pCompressor->Finished ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_STATE, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "brotli compressor already finished");
		pCompressor->Failed = true;
		return false;
	}
	if ( iSize != 0u && pData == NULL ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_ARGUMENT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "invalid brotli input");
		pCompressor->Failed = true;
		return false;
	}
	if ( __xrtBrotliAddOverLimit(pCompressor->InputBytes, iSize,
		pCompressor->Options.tLimits.iMaxInputBytes) ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_LIMIT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "brotli input exceeds maxInput limit");
		pCompressor->Failed = true;
		return false;
	}
	if ( iSize != 0u &&
		!xrtBufferAppend(pCompressor->Pending,
			(xbytesview){ (const unsigned char*)pData, iSize }) ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow brotli pending buffer");
		pCompressor->Failed = true;
		return false;
	}
	pCompressor->InputBytes += iSize;
	while ( xrtBufferView(pCompressor->Pending).Size >= XRT_BROTLI_BLOCK_SIZE ) {
		if ( !__xrtBrotliFlushBlock(pCompressor, XRT_BROTLI_BLOCK_SIZE) ) { return false; }
	}
	return true;
}

XRT_API bool xrtBrotliCompressorFinish(xrtbrotlicompressor* pCompressor)
{
	uint8 sEnd[XRT_BROTLI_HEADER_SIZE] = { 0 };
	size_t iPending;
	if ( pCompressor == NULL || pCompressor->Failed ) { return false; }
	if ( pCompressor->Finished ) { return true; }
	iPending = xrtBufferView(pCompressor->Pending).Size;
	if ( iPending != 0u ) {
		if ( !__xrtBrotliFlushBlock(pCompressor, iPending) ) { return false; }
	}
	if ( __xrtBrotliAddOverLimit(pCompressor->OutputBytes, XRT_BROTLI_HEADER_SIZE,
		pCompressor->Options.tLimits.iMaxOutputBytes) ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_LIMIT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "brotli output exceeds maxOutput limit");
		pCompressor->Failed = true;
		return false;
	}
	if ( !xrtBufferAppend(pCompressor->Output,
		(xbytesview){ (const unsigned char*)sEnd, sizeof(sEnd) }) ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow brotli output buffer");
		pCompressor->Failed = true;
		return false;
	}
	pCompressor->OutputBytes += XRT_BROTLI_HEADER_SIZE;
	pCompressor->Finished = true;
	return true;
}

XRT_API bool xrtBrotliCompressorDrain(xrtbrotlicompressor* pCompressor, xwriter* pWriter)
{
	xbytesview Output;
	if ( pCompressor == NULL || pCompressor->Failed || pWriter == NULL ) { return false; }
	Output = xrtBufferView(pCompressor->Output);
	if ( Output.Size != 0u &&
		!xrtWriterWriteFull(pWriter, Output.Data, Output.Size, NULL) ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_IO, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot write brotli output stream");
		pCompressor->Failed = true;
		return false;
	}
	xrtBufferClear(pCompressor->Output);
	return true;
}

XRT_API xbuffer* xrtBrotliCompressorTakeBuffer(xrtbrotlicompressor* pCompressor)
{
	xbuffer* pOutput;
	if ( pCompressor == NULL || pCompressor->Failed || !pCompressor->Finished ) { return NULL; }
	pOutput = pCompressor->Output;
	pCompressor->Output = NULL;
	return pOutput;
}

XRT_API xbuffer* xrtBrotliCompressorDrainBuffer(xrtbrotlicompressor* pCompressor)
{
	xbuffer* pOutput;
	if ( pCompressor == NULL || pCompressor->Failed || pCompressor->Output == NULL ) { return NULL; }
	pOutput = xrtBufferFrom(xrtBufferView(pCompressor->Output));
	if ( pOutput != NULL ) { xrtBufferClear(pCompressor->Output); }
	return pOutput;
}

XRT_API uint64 xrtBrotliCompressorInputBytes(const xrtbrotlicompressor* pCompressor)
{
	return pCompressor != NULL ? pCompressor->InputBytes : 0u;
}

XRT_API uint64 xrtBrotliCompressorOutputBytes(const xrtbrotlicompressor* pCompressor)
{
	return pCompressor != NULL ? pCompressor->OutputBytes : 0u;
}

XRT_API const xrtbrotlierror* xrtBrotliCompressorError(const xrtbrotlicompressor* pCompressor)
{
	return pCompressor != NULL ? &pCompressor->Error : NULL;
}

XRT_API void xrtBrotliCompressorDestroy(xrtbrotlicompressor* pCompressor)
{
	if ( pCompressor == NULL ) { return; }
	xrtBufferDestroy(pCompressor->Pending);
	xrtBufferDestroy(pCompressor->Output);
	xrtFree(pCompressor);
}



// ------------------------------------ 解压器 ------------------------------------

XRT_API xrtbrotlidecompressor* xrtBrotliDecompressorCreate(const xrtbrotlioptions* pOptions)
{
	xrtbrotlidecompressor* pDecompressor;
	pDecompressor = (xrtbrotlidecompressor*)xrtMalloc(sizeof(xrtbrotlidecompressor));
	if ( pDecompressor == NULL ) { return NULL; }
	memset(pDecompressor, 0, sizeof(*pDecompressor));
	xrtBrotliErrorInit(&pDecompressor->Error);
	if ( pOptions != NULL ) {
		pDecompressor->Options = *pOptions;
	} else {
		xrtBrotliOptionsInit(&pDecompressor->Options);
	}
	pDecompressor->Carry = __xrtBrotliBufferCreate(XRT_BROTLI_CHUNK);
	pDecompressor->Output = __xrtBrotliBufferCreate(XRT_BROTLI_CHUNK);
	if ( pDecompressor->Carry == NULL || pDecompressor->Output == NULL ) {
		__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_MEMORY, 0, 0, 0,
			"cannot allocate brotli decompressor");
		pDecompressor->Failed = true;
		return pDecompressor;
	}
	pDecompressor->State = XRT_BROTLI_DEC_HEADER;
	return pDecompressor;
}

/* 消费 Carry 中的输入；*pConsumed 返回实际消费字节数。 */
static bool __xrtBrotliDecompressConsume(xrtbrotlidecompressor* pDecompressor, size_t* pConsumed)
{
	size_t iPos = 0u;
	xbytesview Carry = xrtBufferView(pDecompressor->Carry);
	const uint8* pBuf = (const uint8*)Carry.Data;
	size_t iLen = Carry.Size;
	while ( iPos < iLen ) {
		if ( pDecompressor->State == XRT_BROTLI_DEC_HEADER ) {
			uint32 iDeclared;
			uint32 iCompressed;
			if ( iLen - iPos < XRT_BROTLI_HEADER_SIZE ) { break; }
			iDeclared = __xrtBrotliReadU32LE(pBuf + iPos);
			iCompressed = __xrtBrotliReadU32LE(pBuf + iPos + 4u);
			iPos += XRT_BROTLI_HEADER_SIZE;
			if ( iDeclared == 0u ) {
				pDecompressor->MemberEnded = true;
				pDecompressor->MemberCount += 1u;
				continue;   /* 可能紧跟下一个成员 */
			}
			pDecompressor->MemberEnded = false;
			pDecompressor->BlockDeclared = (size_t)iDeclared;
			pDecompressor->BlockCompressed = (size_t)iCompressed;
			pDecompressor->BlockCrc = __xrtBrotliReadU32LE(pBuf + iPos - 4u);
			pDecompressor->State = XRT_BROTLI_DEC_BLOCK;
		} else {
			size_t iWritten;
			ptr pPlain;
			if ( pDecompressor->BlockCompressed == 0u ) {
				__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_DATA, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"brotli block declares invalid compressed size");
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			if ( iLen - iPos < pDecompressor->BlockCompressed ) { break; }
			if ( ((pDecompressor->Options.tLimits.iMaxItemBytes != 0u) &&
				  (pDecompressor->BlockDeclared > pDecompressor->Options.tLimits.iMaxItemBytes)) ||
				 __xrtBrotliAddOverLimit(pDecompressor->OutputBytes,
					pDecompressor->BlockDeclared,
					pDecompressor->Options.tLimits.iMaxOutputBytes) ||
				 __xrtBrotliOverRatio(&pDecompressor->Options.tLimits,
					pDecompressor->InputBytes,
					pDecompressor->OutputBytes + (uint64)pDecompressor->BlockDeclared) ) {
				__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_LIMIT, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"brotli output exceeds decompression limit");
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			pPlain = xrtMalloc(pDecompressor->BlockDeclared);
			if ( pPlain == NULL ) {
				__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_MEMORY, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes, "out of memory");
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			iWritten = pDecompressor->BlockDeclared;
			if ( BrotliDecoderDecompress(pDecompressor->BlockCompressed, pBuf + iPos,
				&iWritten, (uint8*)pPlain) != BROTLI_DECODER_RESULT_SUCCESS ||
				iWritten != pDecompressor->BlockDeclared ) {
				__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_DATA, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"brotli decompression failed");
				xrtFree(pPlain);
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			iPos += pDecompressor->BlockCompressed;
			if ( __xrtBrotliCrc32(pPlain, iWritten) != pDecompressor->BlockCrc ) {
				__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_DATA, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"brotli block checksum mismatch");
				xrtFree(pPlain);
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			if ( !xrtBufferAppend(pDecompressor->Output,
				(xbytesview){ (const unsigned char*)pPlain, iWritten }) ) {
				__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_MEMORY, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"cannot grow brotli output buffer");
				xrtFree(pPlain);
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			xrtFree(pPlain);
			pDecompressor->OutputBytes += (uint64)iWritten;
			pDecompressor->State = XRT_BROTLI_DEC_HEADER;
		}
	}
	*pConsumed = iPos;
	return true;
}

XRT_API bool xrtBrotliDecompressorUpdate(xrtbrotlidecompressor* pDecompressor, const void* pData, size_t iSize)
{
	size_t iConsumed = 0u;
	if ( pDecompressor == NULL || pDecompressor->Failed ) { return false; }
	if ( iSize != 0u && pData == NULL ) {
		__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_ARGUMENT, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "invalid brotli input");
		pDecompressor->Failed = true;
		return false;
	}
	if ( __xrtBrotliAddOverLimit(pDecompressor->InputBytes, iSize,
		pDecompressor->Options.tLimits.iMaxInputBytes) ) {
		__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_LIMIT, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "brotli input exceeds maxInput limit");
		pDecompressor->Failed = true;
		return false;
	}
	if ( iSize != 0u &&
		!xrtBufferAppend(pDecompressor->Carry,
			(xbytesview){ (const unsigned char*)pData, iSize }) ) {
		__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_MEMORY, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot grow brotli input buffer");
		pDecompressor->Failed = true;
		return false;
	}
	pDecompressor->InputBytes += iSize;
	if ( !__xrtBrotliDecompressConsume(pDecompressor, &iConsumed) ) { return false; }
	if ( (iConsumed != 0u) &&
		 !xrtBufferRemove(pDecompressor->Carry, 0u, iConsumed) ) {
		__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_STATE, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot consume brotli input buffer");
		pDecompressor->Failed = true;
		return false;
	}
	return true;
}

XRT_API bool xrtBrotliDecompressorFinish(xrtbrotlidecompressor* pDecompressor)
{
	if ( pDecompressor == NULL || pDecompressor->Failed ) { return false; }
	if ( pDecompressor->Finished ) { return true; }
	if ( (pDecompressor->MemberCount == 0u) || !pDecompressor->MemberEnded ||
		(pDecompressor->State != XRT_BROTLI_DEC_HEADER) ||
		(xrtBufferView(pDecompressor->Carry).Size != 0u) ) {
		__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_TRUNCATED, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "brotli stream is truncated or empty");
		pDecompressor->Failed = true;
		return false;
	}
	pDecompressor->Finished = true;
	return true;
}

XRT_API bool xrtBrotliDecompressorDrain(xrtbrotlidecompressor* pDecompressor, xwriter* pWriter)
{
	xbytesview Output;
	if ( pDecompressor == NULL || pDecompressor->Failed || pWriter == NULL ) { return false; }
	Output = xrtBufferView(pDecompressor->Output);
	if ( Output.Size != 0u &&
		!xrtWriterWriteFull(pWriter, Output.Data, Output.Size, NULL) ) {
		__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_IO, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot write brotli output stream");
		pDecompressor->Failed = true;
		return false;
	}
	xrtBufferClear(pDecompressor->Output);
	return true;
}

XRT_API xbuffer* xrtBrotliDecompressorTakeBuffer(xrtbrotlidecompressor* pDecompressor)
{
	xbuffer* pOutput;
	if ( pDecompressor == NULL || pDecompressor->Failed || !pDecompressor->Finished ) { return NULL; }
	pOutput = pDecompressor->Output;
	pDecompressor->Output = NULL;
	return pOutput;
}

XRT_API xbuffer* xrtBrotliDecompressorDrainBuffer(xrtbrotlidecompressor* pDecompressor)
{
	xbuffer* pOutput;
	if ( pDecompressor == NULL || pDecompressor->Failed || pDecompressor->Output == NULL ) { return NULL; }
	pOutput = xrtBufferFrom(xrtBufferView(pDecompressor->Output));
	if ( pOutput != NULL ) { xrtBufferClear(pDecompressor->Output); }
	return pOutput;
}

XRT_API uint64 xrtBrotliDecompressorInputBytes(const xrtbrotlidecompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->InputBytes : 0u;
}

XRT_API uint64 xrtBrotliDecompressorOutputBytes(const xrtbrotlidecompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->OutputBytes : 0u;
}

XRT_API bool xrtBrotliDecompressorFinished(const xrtbrotlidecompressor* pDecompressor)
{
	return pDecompressor != NULL && pDecompressor->MemberEnded;
}

XRT_API uint64 xrtBrotliDecompressorMembers(const xrtbrotlidecompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->MemberCount : 0u;
}

XRT_API const xrtbrotlierror* xrtBrotliDecompressorError(const xrtbrotlidecompressor* pDecompressor)
{
	return pDecompressor != NULL ? &pDecompressor->Error : NULL;
}

XRT_API void xrtBrotliDecompressorDestroy(xrtbrotlidecompressor* pDecompressor)
{
	if ( pDecompressor == NULL ) { return; }
	xrtBufferDestroy(pDecompressor->Carry);
	xrtBufferDestroy(pDecompressor->Output);
	xrtFree(pDecompressor);
}



// ------------------------------------ 一次性接口 ------------------------------------

XRT_API bool xrtBrotliCompress(const void* pData, size_t iSize, const xrtbrotlioptions* pOptions,
	xbuffer** ppOutput, xrtbrotlierror* pError)
{
	xrtbrotlioptions tOptions;
	xrtbrotlicompressor* pCompressor;
	bool bRet;
	xrtBrotliErrorInit(pError);
	if ( ppOutput != NULL ) { *ppOutput = NULL; }
	if ( ppOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtBrotliSetError(pError, XRT_BROTLI_ERROR_ARGUMENT, 0, 0, 0, "invalid brotli input");
		return false;
	}
	if ( pOptions != NULL ) { tOptions = *pOptions; } else { xrtBrotliOptionsInit(&tOptions); }
	pCompressor = xrtBrotliCompressorCreate(&tOptions);
	if ( pCompressor == NULL ) {
		__xrtBrotliSetError(pError, XRT_BROTLI_ERROR_MEMORY, 0, iSize, 0, "cannot allocate brotli compressor");
		return false;
	}
	bRet = xrtBrotliCompressorUpdate(pCompressor, pData, iSize) &&
		xrtBrotliCompressorFinish(pCompressor);
	if ( bRet ) { *ppOutput = xrtBrotliCompressorTakeBuffer(pCompressor); bRet = *ppOutput != NULL; }
	if ( !bRet ) { __xrtBrotliCopyError(pError, &pCompressor->Error); }
	xrtBrotliCompressorDestroy(pCompressor);
	return bRet;
}

XRT_API bool xrtBrotliDecompress(const void* pData, size_t iSize, const xrtbrotlioptions* pOptions,
	xbuffer** ppOutput, xrtbrotlierror* pError)
{
	xrtbrotlioptions tOptions;
	xrtbrotlidecompressor* pDecompressor;
	bool bRet;
	xrtBrotliErrorInit(pError);
	if ( ppOutput != NULL ) { *ppOutput = NULL; }
	if ( ppOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtBrotliSetError(pError, XRT_BROTLI_ERROR_ARGUMENT, 0, 0, 0, "invalid brotli input");
		return false;
	}
	if ( pOptions != NULL ) { tOptions = *pOptions; } else { xrtBrotliOptionsInit(&tOptions); }
	pDecompressor = xrtBrotliDecompressorCreate(&tOptions);
	if ( pDecompressor == NULL ) {
		__xrtBrotliSetError(pError, XRT_BROTLI_ERROR_MEMORY, 0, iSize, 0, "cannot allocate brotli decompressor");
		return false;
	}
	bRet = xrtBrotliDecompressorUpdate(pDecompressor, pData, iSize) &&
		xrtBrotliDecompressorFinish(pDecompressor);
	if ( bRet ) { *ppOutput = xrtBrotliDecompressorTakeBuffer(pDecompressor); bRet = *ppOutput != NULL; }
	if ( !bRet ) { __xrtBrotliCopyError(pError, &pDecompressor->Error); }
	xrtBrotliDecompressorDestroy(pDecompressor);
	return bRet;
}



// ------------------------------------ 文件流接口 ------------------------------------

XRT_API bool xrtBrotliCompressStream(xreader* pReader, xwriter* pWriter, const xrtbrotlioptions* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtbrotlierror* pError)
{
	uint8 sInput[XRT_BROTLI_CHUNK];
	xrtbrotlicompressor* pCompressor;
	uint64 iTotal = 0u;
	bool bTotalKnown;
	bool bRet = true;
	xrtBrotliErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		__xrtBrotliSetError(pError, XRT_BROTLI_ERROR_ARGUMENT, 0, 0, 0, "invalid brotli stream");
		return false;
	}
	bTotalKnown = xrtReaderCanSeek(pReader) && xrtReaderSize(pReader, &iTotal);
	if ( !bTotalKnown ) { xrtClearError(); }
	pCompressor = xrtBrotliCompressorCreate(pOptions);
	if ( pCompressor == NULL ) { return false; }
	while ( bRet ) {
		size_t iRead = 0u;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_IO, 0,
				pCompressor->InputBytes, pCompressor->OutputBytes, "cannot read brotli input stream");
			bRet = false;
			break;
		}
		if ( iRead == 0u ) { break; }
		bRet = xrtBrotliCompressorUpdate(pCompressor, sInput, (size_t)iRead) &&
			xrtBrotliCompressorDrain(pCompressor, pWriter);
		if ( bRet && !xrtProgressReport(pProgress, pUserData, pCompressor->InputBytes,
			iTotal, pCompressor->OutputBytes,
			bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u) ) {
			__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_CANCELLED, 0,
				pCompressor->InputBytes, pCompressor->OutputBytes, "brotli operation cancelled");
			bRet = false;
		}
	}
	if ( bRet ) { bRet = xrtBrotliCompressorFinish(pCompressor) && xrtBrotliCompressorDrain(pCompressor, pWriter); }
	if ( bRet && !xrtProgressReport(pProgress, pUserData, pCompressor->InputBytes,
		iTotal, pCompressor->OutputBytes, XRT_PROGRESS_FINAL |
		(bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u)) ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_CANCELLED, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "brotli operation cancelled");
		bRet = false;
	}
	if ( !bRet ) { __xrtBrotliCopyError(pError, &pCompressor->Error); }
	xrtBrotliCompressorDestroy(pCompressor);
	return bRet;
}

XRT_API bool xrtBrotliDecompressStream(xreader* pReader, xwriter* pWriter, const xrtbrotlioptions* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtbrotlierror* pError)
{
	uint8 sInput[XRT_BROTLI_CHUNK];
	xrtbrotlidecompressor* pDecompressor;
	uint64 iTotal = 0u;
	bool bTotalKnown;
	bool bRet = true;
	xrtBrotliErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		__xrtBrotliSetError(pError, XRT_BROTLI_ERROR_ARGUMENT, 0, 0, 0, "invalid brotli stream");
		return false;
	}
	bTotalKnown = xrtReaderCanSeek(pReader) && xrtReaderSize(pReader, &iTotal);
	if ( !bTotalKnown ) { xrtClearError(); }
	pDecompressor = xrtBrotliDecompressorCreate(pOptions);
	if ( pDecompressor == NULL ) { return false; }
	while ( bRet ) {
		size_t iRead = 0u;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_IO, 0,
				pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot read brotli input stream");
			bRet = false;
			break;
		}
		if ( iRead == 0u ) { break; }
		bRet = xrtBrotliDecompressorUpdate(pDecompressor, sInput, (size_t)iRead) &&
			xrtBrotliDecompressorDrain(pDecompressor, pWriter);
		if ( bRet && !xrtProgressReport(pProgress, pUserData, pDecompressor->InputBytes,
			iTotal, pDecompressor->OutputBytes,
			bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u) ) {
			__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_CANCELLED, 0,
				pDecompressor->InputBytes, pDecompressor->OutputBytes, "brotli operation cancelled");
			bRet = false;
		}
	}
	if ( bRet ) { bRet = xrtBrotliDecompressorFinish(pDecompressor) && xrtBrotliDecompressorDrain(pDecompressor, pWriter); }
	if ( bRet && !xrtProgressReport(pProgress, pUserData, pDecompressor->InputBytes,
		iTotal, pDecompressor->OutputBytes, XRT_PROGRESS_FINAL |
		(bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u)) ) {
		__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_CANCELLED, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "brotli operation cancelled");
		bRet = false;
	}
	if ( !bRet ) { __xrtBrotliCopyError(pError, &pDecompressor->Error); }
	xrtBrotliDecompressorDestroy(pDecompressor);
	return bRet;
}
