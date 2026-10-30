#include "include/xrt_snappy.h"
#include "third_party/xrt_snappy.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>



/* 官方 framing 每个数据块最多包含 64 KiB 原文。 */
#define XRT_SNAPPY_BLOCK_SIZE 65536u
#define XRT_SNAPPY_CHUNK 65536u
#define XRT_SNAPPY_HEADER_SIZE 4u
static const uint8 __xrtSnappyStreamId[10] = {
	0xffu, 0x06u, 0x00u, 0x00u, 's', 'N', 'a', 'P', 'p', 'Y'
};

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

struct xrtsnappydecompressor {
	xbuffer* Carry;              /* 尚未消费完的输入字节 */
	xbuffer* Output;             /* 已产出的原始字节 */
	xrtsnappyoptions Options;
	xrtsnappyerror Error;
	uint64 InputBytes;
	uint64 OutputBytes;
	uint64 MemberCount;
	size_t ChunkLength;
	uint8 ChunkType;
	bool HaveChunkHeader;
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

/* Framing 使用 masked CRC32C；XRT 当前只提供普通 CRC32。 */
static uint32 __xrtSnappyMaskedCrc32c(const void* pData, size_t iLen)
{
	const uint8* pBuf = (const uint8*)pData;
	uint32 iCrc = 0xFFFFFFFFu;
	size_t i;
	for ( i = 0u; i < iLen; ++i ) {
		int j;
		iCrc ^= (uint32)pBuf[i];
		for ( j = 0; j < 8; ++j ) {
			iCrc = (iCrc >> 1) ^ (0x82F63B78u & (0u - (iCrc & 1u)));
		}
	}
	iCrc ^= 0xFFFFFFFFu;
	return ((iCrc >> 15) | (iCrc << 17)) + 0xa282ead8u;
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
	pOptions->tLimits.iMaxNodes = 0u;
	pOptions->tLimits.iMaxDepth = 0u;
}

/* 外层长度未确认前不可读取内嵌限额或复制整份结构。 */
static bool __xrtSnappyCopyOptions(xrtsnappyoptions* pOutput,
	const xrtsnappyoptions* pInput)
{
	xrtSnappyOptionsInit(pOutput);
	if ( pInput == NULL ) { return true; }
	if ( pInput->iSize != sizeof(*pInput) ||
		pInput->iVersion != XRT_SNAPPY_OPTIONS_VERSION ) { return false; }
	if ( pInput->sReserved[0] != 0u || pInput->sReserved[1] != 0u ||
		pInput->sReserved[2] != 0u || pInput->sReserved[3] != 0u ||
		pInput->tLimits.iSize != sizeof(pInput->tLimits) ||
		pInput->tLimits.iVersion != XRT_RESOURCE_LIMITS_VERSION ||
		pInput->tLimits.iReserved != 0u ||
		pInput->tLimits.iFlags != 0u ||
		pInput->tLimits.iMaxNodes != 0u ||
		pInput->tLimits.iMaxDepth != 0u ) { return false; }
	*pOutput = *pInput;
	return true;
}



// ------------------------------------ 压缩器 ------------------------------------

XRT_API xrtsnappycompressor* xrtSnappyCompressorCreate(const xrtsnappyoptions* pOptions)
{
	xrtsnappycompressor* pCompressor;
	pCompressor = (xrtsnappycompressor*)xrtMalloc(sizeof(xrtsnappycompressor));
	if ( pCompressor == NULL ) { return NULL; }
	memset(pCompressor, 0, sizeof(*pCompressor));
	xrtSnappyErrorInit(&pCompressor->Error);
	if ( !__xrtSnappyCopyOptions(&pCompressor->Options, pOptions) ) {
		__xrtSnappySetError(&pCompressor->Error, XRT_SNAPPY_ERROR_ARGUMENT,
			0, 0, 0, "invalid snappy options");
		pCompressor->Failed = true;
		return pCompressor;
	}
	pCompressor->Pending = __xrtSnappyBufferCreate(XRT_SNAPPY_CHUNK);
	pCompressor->Output = __xrtSnappyBufferCreate(XRT_SNAPPY_CHUNK);
	if ( pCompressor->Pending == NULL || pCompressor->Output == NULL ) {
		__xrtSnappySetError(&pCompressor->Error, XRT_SNAPPY_ERROR_MEMORY, 0, 0, 0,
			"cannot allocate snappy compressor");
		pCompressor->Failed = true;
		return pCompressor;
	}
	if ( __xrtSnappyAddOverLimit(0u, sizeof(__xrtSnappyStreamId),
		pCompressor->Options.tLimits.iMaxOutputBytes) ||
		!xrtBufferAppend(pCompressor->Output,
			(xbytesview){ __xrtSnappyStreamId, sizeof(__xrtSnappyStreamId) }) ) {
		__xrtSnappySetError(&pCompressor->Error, XRT_SNAPPY_ERROR_LIMIT, 0, 0, 0,
			"cannot write snappy stream identifier");
		pCompressor->Failed = true;
		return pCompressor;
	}
	pCompressor->OutputBytes = sizeof(__xrtSnappyStreamId);
	return pCompressor;
}

/* 写入标准 compressed/uncompressed data chunk。 */
static bool __xrtSnappyFlushBlock(xrtsnappycompressor* pCompressor, size_t iSize)
{
	size_t iBound;
	size_t iWritten;
	uint8 sHeader[XRT_SNAPPY_HEADER_SIZE];
	ptr pCompressed;
	int iResult;
	xbytesview Pending;
	size_t iGenerated;
	bool bCompressed;
	const uint8* pBody;
	size_t iBodySize;
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
	if ( iResult != XS_CODEC_OK ) {
		__xrtSnappySetError(&pCompressor->Error, XRT_SNAPPY_ERROR_DATA, iResult,
			pCompressor->InputBytes, pCompressor->OutputBytes, "snappy compression failed");
		xrtFree(pCompressed);
		pCompressor->Failed = true;
		return false;
	}
	bCompressed = iWritten < iSize;
	pBody = bCompressed ? (const uint8*)pCompressed : (const uint8*)Pending.Data;
	iBodySize = bCompressed ? iWritten : iSize;
	/* 长度字段只计 CRC 与块体，不含四字节 chunk header。 */
	sHeader[0] = bCompressed ? 0x00u : 0x01u;
	sHeader[1] = (uint8)((iBodySize + 4u) & 0xffu);
	sHeader[2] = (uint8)(((iBodySize + 4u) >> 8) & 0xffu);
	sHeader[3] = (uint8)(((iBodySize + 4u) >> 16) & 0xffu);
	iGenerated = XRT_SNAPPY_HEADER_SIZE + 4u + iBodySize;
	if ( __xrtSnappyAddOverLimit(pCompressor->OutputBytes, iGenerated,
		pCompressor->Options.tLimits.iMaxOutputBytes) ) {
		__xrtSnappySetError(&pCompressor->Error, XRT_SNAPPY_ERROR_LIMIT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "snappy output exceeds maxOutput limit");
		xrtFree(pCompressed);
		pCompressor->Failed = true;
		return false;
	}
	{
		uint8 sCrc[4];
		__xrtSnappyWriteU32LE(sCrc, __xrtSnappyMaskedCrc32c(Pending.Data, iSize));
		if ( !xrtBufferAppend(pCompressor->Output,
				(xbytesview){ sHeader, sizeof(sHeader) }) ||
			 !xrtBufferAppend(pCompressor->Output,
				(xbytesview){ sCrc, sizeof(sCrc) }) ||
			 !xrtBufferAppend(pCompressor->Output,
				(xbytesview){ pBody, iBodySize }) ) {
		__xrtSnappySetError(&pCompressor->Error, XRT_SNAPPY_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow snappy output buffer");
		xrtFree(pCompressed);
		pCompressor->Failed = true;
		return false;
		}
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
	const uint8* pInput = (const uint8*)pData;
	size_t iRemaining = iSize;
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
	while ( iRemaining != 0u ) {
		size_t iPending = xrtBufferView(pCompressor->Pending).Size;
		size_t iTake = XRT_SNAPPY_BLOCK_SIZE - iPending;
		if ( iTake > iRemaining ) { iTake = iRemaining; }
		if ( iTake != 0u &&
			!xrtBufferAppend(pCompressor->Pending, (xbytesview){ pInput, iTake }) ) {
			__xrtSnappySetError(&pCompressor->Error, XRT_SNAPPY_ERROR_MEMORY, 0,
				pCompressor->InputBytes, pCompressor->OutputBytes,
				"cannot grow snappy pending buffer");
			pCompressor->Failed = true;
			return false;
		}
		pInput += iTake;
		iRemaining -= iTake;
		pCompressor->InputBytes += iTake;
		if ( (iPending + iTake == XRT_SNAPPY_BLOCK_SIZE) &&
			!__xrtSnappyFlushBlock(pCompressor, XRT_SNAPPY_BLOCK_SIZE) ) { return false; }
	}
	return true;
}

XRT_API bool xrtSnappyCompressorFinish(xrtsnappycompressor* pCompressor)
{
	size_t iPending;
	if ( pCompressor == NULL || pCompressor->Failed ) { return false; }
	if ( pCompressor->Finished ) { return true; }
	iPending = xrtBufferView(pCompressor->Pending).Size;
	if ( iPending != 0u ) {
		if ( !__xrtSnappyFlushBlock(pCompressor, iPending) ) { return false; }
	}
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
	if ( !__xrtSnappyCopyOptions(&pDecompressor->Options, pOptions) ) {
		__xrtSnappySetError(&pDecompressor->Error, XRT_SNAPPY_ERROR_ARGUMENT,
			0, 0, 0, "invalid snappy options");
		pDecompressor->Failed = true;
		return pDecompressor;
	}
	pDecompressor->Carry = __xrtSnappyBufferCreate(XRT_SNAPPY_CHUNK);
	pDecompressor->Output = __xrtSnappyBufferCreate(XRT_SNAPPY_CHUNK);
	if ( pDecompressor->Carry == NULL || pDecompressor->Output == NULL ) {
		__xrtSnappySetError(&pDecompressor->Error, XRT_SNAPPY_ERROR_MEMORY, 0, 0, 0,
			"cannot allocate snappy decompressor");
		pDecompressor->Failed = true;
		return pDecompressor;
	}
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
		const uint8* pChunk;
		if ( !pDecompressor->HaveChunkHeader ) {
			if ( iLen - iPos < XRT_SNAPPY_HEADER_SIZE ) { break; }
			pDecompressor->ChunkType = pBuf[iPos];
			pDecompressor->ChunkLength = (size_t)pBuf[iPos + 1u] |
				((size_t)pBuf[iPos + 2u] << 8) |
				((size_t)pBuf[iPos + 3u] << 16);
			iPos += XRT_SNAPPY_HEADER_SIZE;
			pDecompressor->HaveChunkHeader = true;
			if ( (pDecompressor->MemberCount == 0u && pDecompressor->ChunkType != 0xffu) ||
				(pDecompressor->ChunkType == 0xffu && pDecompressor->ChunkLength != 6u) ||
				(pDecompressor->ChunkType >= 0x02u && pDecompressor->ChunkType <= 0x7fu) ||
				(pDecompressor->ChunkType <= 0x01u &&
				 (pDecompressor->ChunkLength < (pDecompressor->ChunkType == 0x00u ? 5u : 4u) ||
				  (pDecompressor->ChunkType == 0x01u &&
				   pDecompressor->ChunkLength > XRT_SNAPPY_BLOCK_SIZE + 4u))) ) {
				__xrtSnappySetError(&pDecompressor->Error, XRT_SNAPPY_ERROR_DATA, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"invalid snappy framed chunk");
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
		}
		/* 可跳过 chunk 不需要保留主体，跨 Update 逐段丢弃即可。 */
		if ( pDecompressor->ChunkType >= 0x80u &&
			pDecompressor->ChunkType <= 0xfeu ) {
			size_t iSkip = iLen - iPos;
			if ( iSkip > pDecompressor->ChunkLength ) {
				iSkip = pDecompressor->ChunkLength;
			}
			iPos += iSkip;
			pDecompressor->ChunkLength -= iSkip;
			if ( pDecompressor->ChunkLength != 0u ) { break; }
			pDecompressor->HaveChunkHeader = false;
			continue;
		}
		if ( iLen - iPos < pDecompressor->ChunkLength ) { break; }
		pChunk = pBuf + iPos;
		if ( pDecompressor->ChunkType == 0xffu ) {
			if ( memcmp(pChunk, __xrtSnappyStreamId + 4u, 6u) != 0 ) {
				__xrtSnappySetError(&pDecompressor->Error, XRT_SNAPPY_ERROR_DATA, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"invalid snappy stream identifier");
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			if ( pDecompressor->MemberCount == UINT64_MAX ||
				(pDecompressor->Options.tLimits.iMaxEntries != 0u &&
				 pDecompressor->MemberCount >=
				 pDecompressor->Options.tLimits.iMaxEntries) ) {
				__xrtSnappySetError(&pDecompressor->Error, XRT_SNAPPY_ERROR_LIMIT, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"snappy stream count exceeds maxEntries limit");
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			pDecompressor->MemberCount += 1u;
		} else if ( pDecompressor->ChunkType <= 0x01u ) {
			size_t iDeclared = pDecompressor->ChunkLength - 4u;
			size_t iWritten = 0u;
			const uint8* pPlain = pChunk + 4u;
			ptr pAllocated = NULL;
			int iResult = XS_CODEC_OK;
			if ( pDecompressor->ChunkType == 0x00u ) {
				iResult = xsCodecUncompressedLength(pChunk + 4u, iDeclared, &iDeclared);
			}
			if ( iResult != XS_CODEC_OK || iDeclared > XRT_SNAPPY_BLOCK_SIZE ) {
				__xrtSnappySetError(&pDecompressor->Error, XRT_SNAPPY_ERROR_DATA, iResult,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"invalid snappy data chunk size");
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			if ( ((pDecompressor->Options.tLimits.iMaxItemBytes != 0u) &&
				  (iDeclared > pDecompressor->Options.tLimits.iMaxItemBytes)) ||
				 __xrtSnappyAddOverLimit(pDecompressor->OutputBytes, iDeclared,
					pDecompressor->Options.tLimits.iMaxOutputBytes) ||
				 __xrtSnappyOverRatio(&pDecompressor->Options.tLimits,
					pDecompressor->InputBytes, pDecompressor->OutputBytes + (uint64)iDeclared) ) {
				__xrtSnappySetError(&pDecompressor->Error, XRT_SNAPPY_ERROR_LIMIT, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"snappy output exceeds decompression limit");
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			if ( pDecompressor->ChunkType == 0x00u ) {
				/* 空 raw 流不分配输出，但仍经过完整解码和 CRC 校验。 */
				pAllocated = iDeclared != 0u ? xrtMalloc(iDeclared) : NULL;
				if ( iDeclared != 0u && pAllocated == NULL ) {
					__xrtSnappySetError(&pDecompressor->Error, XRT_SNAPPY_ERROR_MEMORY, 0,
						pDecompressor->InputBytes, pDecompressor->OutputBytes, "out of memory");
					pDecompressor->Failed = true;
					*pConsumed = iPos;
					return false;
				}
				iResult = xsCodecDecompress(pChunk + 4u, pDecompressor->ChunkLength - 4u,
					(unsigned char*)pAllocated, iDeclared, &iWritten);
				pPlain = (const uint8*)pAllocated;
			} else {
				iWritten = iDeclared;
			}
			if ( iResult != XS_CODEC_OK || iWritten != iDeclared ||
				__xrtSnappyMaskedCrc32c(pPlain, iWritten) != __xrtSnappyReadU32LE(pChunk) ) {
				__xrtSnappySetError(&pDecompressor->Error, XRT_SNAPPY_ERROR_DATA, iResult,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"snappy data chunk or checksum is invalid");
				xrtFree(pAllocated);
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			if ( !xrtBufferAppend(pDecompressor->Output, (xbytesview){ pPlain, iWritten }) ) {
				__xrtSnappySetError(&pDecompressor->Error, XRT_SNAPPY_ERROR_MEMORY, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"cannot grow snappy output buffer");
				xrtFree(pAllocated);
				pDecompressor->Failed = true;
				*pConsumed = iPos;
				return false;
			}
			xrtFree(pAllocated);
			pDecompressor->OutputBytes += (uint64)iWritten;
		}
		iPos += pDecompressor->ChunkLength;
		pDecompressor->HaveChunkHeader = false;
	}
	*pConsumed = iPos;
	return true;
}

XRT_API bool xrtSnappyDecompressorUpdate(xrtsnappydecompressor* pDecompressor, const void* pData, size_t iSize)
{
	const uint8* pInput = (const uint8*)pData;
	size_t iRemaining = iSize;
	if ( pDecompressor == NULL || pDecompressor->Failed ) { return false; }
	if ( pDecompressor->Finished ) {
		__xrtSnappySetError(&pDecompressor->Error, XRT_SNAPPY_ERROR_STATE, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes,
			"snappy decompressor already finished");
		pDecompressor->Failed = true;
		return false;
	}
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
	while ( iRemaining != 0u ) {
		size_t iTake = iRemaining < XRT_SNAPPY_CHUNK ? iRemaining : XRT_SNAPPY_CHUNK;
		size_t iConsumed = 0u;
		if ( !xrtBufferAppend(pDecompressor->Carry,
				(xbytesview){ pInput, iTake }) ) {
			__xrtSnappySetError(&pDecompressor->Error, XRT_SNAPPY_ERROR_MEMORY, 0,
				pDecompressor->InputBytes, pDecompressor->OutputBytes,
				"cannot grow snappy input buffer");
			pDecompressor->Failed = true;
			return false;
		}
		pInput += iTake;
		iRemaining -= iTake;
		pDecompressor->InputBytes += iTake;
		if ( !__xrtSnappyDecompressConsume(pDecompressor, &iConsumed) ) { return false; }
		if ( iConsumed != 0u &&
			!xrtBufferRemove(pDecompressor->Carry, 0u, iConsumed) ) {
			__xrtSnappySetError(&pDecompressor->Error, XRT_SNAPPY_ERROR_STATE, 0,
				pDecompressor->InputBytes, pDecompressor->OutputBytes,
				"cannot consume snappy input buffer");
			pDecompressor->Failed = true;
			return false;
		}
	}
	return true;
}

XRT_API bool xrtSnappyDecompressorFinish(xrtsnappydecompressor* pDecompressor)
{
	if ( pDecompressor == NULL || pDecompressor->Failed ) { return false; }
	if ( pDecompressor->Finished ) { return true; }
	if ( (pDecompressor->MemberCount == 0u) || pDecompressor->HaveChunkHeader ||
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
	return pDecompressor != NULL && pDecompressor->Finished;
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
	if ( !__xrtSnappyCopyOptions(&tOptions, pOptions) ) {
		__xrtSnappySetError(pError, XRT_SNAPPY_ERROR_ARGUMENT,
			0, 0, 0, "invalid snappy options");
		return false;
	}
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
	if ( !__xrtSnappyCopyOptions(&tOptions, pOptions) ) {
		__xrtSnappySetError(pError, XRT_SNAPPY_ERROR_ARGUMENT,
			0, 0, 0, "invalid snappy options");
		return false;
	}
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

/* 进度总量只包含本次读取的剩余字节，不包含 Reader 已跳过的前缀。 */
static bool __xrtSnappyReaderRemaining(xreader* pReader, uint64* pRemaining)
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
	bTotalKnown = __xrtSnappyReaderRemaining(pReader, &iTotal);
	pCompressor = xrtSnappyCompressorCreate(pOptions);
	if ( pCompressor == NULL ) {
		__xrtSnappySetError(pError, XRT_SNAPPY_ERROR_MEMORY,
			0, 0, 0, "cannot allocate snappy compressor");
		return false;
	}
	if ( pCompressor->Failed ) {
		__xrtSnappyCopyError(pError, &pCompressor->Error);
		xrtSnappyCompressorDestroy(pCompressor);
		return false;
	}
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
	bTotalKnown = __xrtSnappyReaderRemaining(pReader, &iTotal);
	pDecompressor = xrtSnappyDecompressorCreate(pOptions);
	if ( pDecompressor == NULL ) {
		__xrtSnappySetError(pError, XRT_SNAPPY_ERROR_MEMORY,
			0, 0, 0, "cannot allocate snappy decompressor");
		return false;
	}
	if ( pDecompressor->Failed ) {
		__xrtSnappyCopyError(pError, &pDecompressor->Error);
		xrtSnappyDecompressorDestroy(pDecompressor);
		return false;
	}
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
