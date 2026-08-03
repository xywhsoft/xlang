#include "include/xrt_lzma.h"
#include "third_party/Alloc.h"
#include "third_party/LzmaEnc.h"
#include "third_party/LzmaDec.h"

#include <stdarg.h>
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
	xbuffer Pending;            /* 尚未成块的原始输入 */
	xbuffer Output;             /* 已产出的压缩字节 */
	xrtlzmaoptions Options;
	xrtlzmaerror Error;
	uint64 InputBytes;
	uint64 OutputBytes;
	bool Initialized;
	bool Finished;
	bool Failed;
};

typedef enum {
	XRT_LZMA_DEC_HEADER = 0,    /* 读取块头 */
	XRT_LZMA_DEC_BLOCK = 1      /* 读取压缩块体 */
} xrtlzmadecstate;

struct xrtlzmadecompressor {
	xbuffer Carry;              /* 尚未消费完的输入字节 */
	xbuffer Output;             /* 已产出的原始字节 */
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
	bool Initialized;
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

static bool __xrtLzmaOverOutputLimit(const xrtresourcelimits* pLimits, uint64 iOutput)
{
	return pLimits->iMaxOutputBytes != 0u && iOutput > pLimits->iMaxOutputBytes;
}

static bool __xrtLzmaOverRatio(const xrtresourcelimits* pLimits, uint64 iInput, uint64 iOutput)
{
	uint64 iRatio = pLimits->iMaxCompressionRatio;
	if ( iRatio == 0u || iInput == 0u ) { return FALSE; }
	return iOutput > iInput * iRatio;
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



XXAPI void xrtLzmaErrorInit(xrtlzmaerror* pError)
{
	if ( pError == NULL ) { return; }
	memset(pError, 0, sizeof(*pError));
	pError->iCode = XRT_LZMA_OK;
}

XXAPI void xrtLzmaOptionsInit(xrtlzmaoptions* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_LZMA_OPTIONS_VERSION;
	pOptions->iLevel = 5;
	xrtResourceLimitsInit(&pOptions->tLimits);
}



// ------------------------------------ 压缩器 ------------------------------------

XXAPI xrtlzmacompressor* xrtLzmaCompressorCreate(const xrtlzmaoptions* pOptions)
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
	pCompressor->Pending = xrtBufferCreate(XRT_LZMA_CHUNK);
	pCompressor->Output = xrtBufferCreate(XRT_LZMA_CHUNK);
	if ( pCompressor->Pending == NULL || pCompressor->Output == NULL ) {
		__xrtLzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_MEMORY, 0, 0, 0,
			"cannot allocate lzma compressor");
		pCompressor->Failed = TRUE;
		return pCompressor;
	}
	pCompressor->Initialized = TRUE;
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
	if ( iSize == 0u ) { return TRUE; }
	iBound = (SizeT)iSize + (SizeT)iSize / 3u + 128u;
	pCompressed = xrtMalloc((size_t)iBound);
	if ( pCompressed == NULL ) {
		__xrtLzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "out of memory");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	LzmaEncProps_Init(&tProps);
	tProps.level = pCompressor->Options.iLevel;
	tProps.reduceSize = (UInt64)iSize;
	iWritten = iBound;
	iResult = LzmaEncode((Byte*)pCompressed, &iWritten,
		(const Byte*)pCompressor->Pending->Buffer, (SizeT)iSize, &tProps,
		sProps, &iPropsSize, 0, NULL, &g_Alloc, &g_Alloc);
	if ( iResult != SZ_OK || iPropsSize != LZMA_PROPS_SIZE ) {
		__xrtLzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_DATA, (int)iResult,
			pCompressor->InputBytes, pCompressor->OutputBytes, "%s",
			__xrtLzmaStatusName(iResult));
		xrtFree(pCompressed);
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	__xrtLzmaWriteU32LE(sHeader, (uint32)iSize);
	__xrtLzmaWriteU32LE(sHeader + 4u, (uint32)iWritten);
	__xrtLzmaWriteU32LE(sHeader + 8u,
		__xrtLzmaCrc32(pCompressor->Pending->Buffer, iSize));
	memcpy(sHeader + 12u, sProps, LZMA_PROPS_SIZE);
	if ( !xrtBufferAppend(pCompressor->Output, (ptr)sHeader, XRT_LZMA_HEADER_SIZE, XBUF_BINARY) ||
		!xrtBufferAppend(pCompressor->Output, pCompressed, (uint32)iWritten, XBUF_BINARY) ) {
		__xrtLzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow lzma output buffer");
		xrtFree(pCompressed);
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	xrtFree(pCompressed);
	pCompressor->OutputBytes += (uint64)(XRT_LZMA_HEADER_SIZE + (size_t)iWritten);
	/* 移除已消费的前 iSize 字节。 */
	if ( pCompressor->Pending->Length > iSize ) {
		memmove(pCompressor->Pending->Buffer, (uint8*)pCompressor->Pending->Buffer + iSize,
			pCompressor->Pending->Length - iSize);
	}
	pCompressor->Pending->Length -= (uint32)iSize;
	if ( __xrtLzmaOverOutputLimit(&pCompressor->Options.tLimits, pCompressor->OutputBytes) ) {
		__xrtLzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_LIMIT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "lzma output exceeds maxOutput limit");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	return TRUE;
}

XXAPI bool xrtLzmaCompressorUpdate(xrtlzmacompressor* pCompressor, const void* pData, size_t iSize)
{
	if ( pCompressor == NULL || pCompressor->Failed ) { return FALSE; }
	if ( pCompressor->Finished ) {
		__xrtLzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_STATE, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "lzma compressor already finished");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	if ( iSize != 0u && pData == NULL ) {
		__xrtLzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_ARGUMENT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "invalid lzma input");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	if ( pCompressor->Options.tLimits.iMaxInputBytes != 0u &&
		pCompressor->InputBytes + iSize > pCompressor->Options.tLimits.iMaxInputBytes ) {
		__xrtLzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_LIMIT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "lzma input exceeds maxInput limit");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	if ( iSize != 0u &&
		!xrtBufferAppend(pCompressor->Pending, (ptr)pData, (uint32)iSize, XBUF_BINARY) ) {
		__xrtLzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow lzma pending buffer");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	pCompressor->InputBytes += iSize;
	while ( pCompressor->Pending->Length >= XRT_LZMA_BLOCK_SIZE ) {
		if ( !__xrtLzmaFlushBlock(pCompressor, XRT_LZMA_BLOCK_SIZE) ) { return FALSE; }
	}
	return TRUE;
}

XXAPI bool xrtLzmaCompressorFinish(xrtlzmacompressor* pCompressor)
{
	uint8 sEnd[4];
	if ( pCompressor == NULL || pCompressor->Failed ) { return FALSE; }
	if ( pCompressor->Finished ) { return TRUE; }
	if ( pCompressor->Pending->Length != 0u ) {
		if ( !__xrtLzmaFlushBlock(pCompressor, pCompressor->Pending->Length) ) { return FALSE; }
	}
	__xrtLzmaWriteU32LE(sEnd, 0u);
	if ( !xrtBufferAppend(pCompressor->Output, (ptr)sEnd, 4u, XBUF_BINARY) ) {
		__xrtLzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow lzma output buffer");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	pCompressor->OutputBytes += 4u;
	pCompressor->Finished = TRUE;
	return TRUE;
}

XXAPI bool xrtLzmaCompressorDrain(xrtlzmacompressor* pCompressor, xwriter pWriter)
{
	if ( pCompressor == NULL || pCompressor->Failed || pWriter == NULL ) { return FALSE; }
	if ( pCompressor->Output->Length != 0u &&
		!xrtWriterWriteAll(pWriter, pCompressor->Output->Buffer, pCompressor->Output->Length) ) {
		__xrtLzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_IO, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot write lzma output stream");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	pCompressor->Output->Length = 0u;
	return TRUE;
}

XXAPI xbuffer xrtLzmaCompressorTakeBuffer(xrtlzmacompressor* pCompressor)
{
	xbuffer pOutput;
	if ( pCompressor == NULL || pCompressor->Failed || !pCompressor->Finished ) { return NULL; }
	pOutput = pCompressor->Output;
	pCompressor->Output = NULL;
	return pOutput;
}

XXAPI xbuffer xrtLzmaCompressorDrainBuffer(xrtlzmacompressor* pCompressor)
{
	xbuffer pOutput;
	if ( pCompressor == NULL || pCompressor->Failed || pCompressor->Output == NULL ) { return NULL; }
	pOutput = xrtBufferFrom((ptr)pCompressor->Output->Buffer, (size_t)pCompressor->Output->Length);
	pCompressor->Output->Length = 0u;
	return pOutput;
}

XXAPI uint64 xrtLzmaCompressorInputBytes(const xrtlzmacompressor* pCompressor)
{
	return pCompressor != NULL ? pCompressor->InputBytes : 0u;
}

XXAPI uint64 xrtLzmaCompressorOutputBytes(const xrtlzmacompressor* pCompressor)
{
	return pCompressor != NULL ? pCompressor->OutputBytes : 0u;
}

XXAPI const xrtlzmaerror* xrtLzmaCompressorError(const xrtlzmacompressor* pCompressor)
{
	return pCompressor != NULL ? &pCompressor->Error : NULL;
}

XXAPI void xrtLzmaCompressorDestroy(xrtlzmacompressor* pCompressor)
{
	if ( pCompressor == NULL ) { return; }
	xrtBufferDestroy(pCompressor->Pending);
	xrtBufferDestroy(pCompressor->Output);
	xrtFree(pCompressor);
}



// ------------------------------------ 解压器 ------------------------------------

XXAPI xrtlzmadecompressor* xrtLzmaDecompressorCreate(const xrtlzmaoptions* pOptions)
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
	pDecompressor->Carry = xrtBufferCreate(XRT_LZMA_CHUNK);
	pDecompressor->Output = xrtBufferCreate(XRT_LZMA_CHUNK);
	if ( pDecompressor->Carry == NULL || pDecompressor->Output == NULL ) {
		__xrtLzmaSetError(&pDecompressor->Error, XRT_LZMA_ERROR_MEMORY, 0, 0, 0,
			"cannot allocate lzma decompressor");
		pDecompressor->Failed = TRUE;
		return pDecompressor;
	}
	pDecompressor->State = XRT_LZMA_DEC_HEADER;
	pDecompressor->Initialized = TRUE;
	return pDecompressor;
}

/* 消费 Carry 中的输入；*pConsumed 返回实际消费字节数。 */
static bool __xrtLzmaDecompressConsume(xrtlzmadecompressor* pDecompressor, size_t* pConsumed)
{
	size_t iPos = 0u;
	const uint8* pBuf = (const uint8*)pDecompressor->Carry->Buffer;
	size_t iLen = pDecompressor->Carry->Length;
	while ( iPos < iLen ) {
		if ( pDecompressor->State == XRT_LZMA_DEC_HEADER ) {
			uint32 iDeclared;
			if ( iLen - iPos < 4u ) { break; }
			iDeclared = __xrtLzmaReadU32LE(pBuf + iPos);
			if ( iDeclared == 0u ) {
				iPos += 4u;
				pDecompressor->MemberEnded = TRUE;
				pDecompressor->MemberCount += 1u;
				continue;   /* 可能紧跟下一个成员 */
			}
			if ( iLen - iPos < XRT_LZMA_HEADER_SIZE ) { break; }
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
			pPlain = xrtMalloc(pDecompressor->BlockDeclared + 1u);
			if ( pPlain == NULL ) {
				__xrtLzmaSetError(&pDecompressor->Error, XRT_LZMA_ERROR_MEMORY, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes, "out of memory");
				pDecompressor->Failed = TRUE;
				*pConsumed = iPos;
				return FALSE;
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
				pDecompressor->Failed = TRUE;
				*pConsumed = iPos;
				return FALSE;
			}
			iPos += pDecompressor->BlockCompressed;
			if ( __xrtLzmaCrc32(pPlain, (size_t)iDstLen) != pDecompressor->BlockCrc ) {
				__xrtLzmaSetError(&pDecompressor->Error, XRT_LZMA_ERROR_DATA, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"lzma block checksum mismatch");
				xrtFree(pPlain);
				pDecompressor->Failed = TRUE;
				*pConsumed = iPos;
				return FALSE;
			}
			if ( !xrtBufferAppend(pDecompressor->Output, pPlain, (uint32)iDstLen, XBUF_BINARY) ) {
				__xrtLzmaSetError(&pDecompressor->Error, XRT_LZMA_ERROR_MEMORY, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"cannot grow lzma output buffer");
				xrtFree(pPlain);
				pDecompressor->Failed = TRUE;
				*pConsumed = iPos;
				return FALSE;
			}
			xrtFree(pPlain);
			pDecompressor->OutputBytes += (uint64)iDstLen;
			if ( __xrtLzmaOverOutputLimit(&pDecompressor->Options.tLimits, pDecompressor->OutputBytes) ||
				__xrtLzmaOverRatio(&pDecompressor->Options.tLimits, pDecompressor->InputBytes,
					pDecompressor->OutputBytes) ) {
				__xrtLzmaSetError(&pDecompressor->Error, XRT_LZMA_ERROR_LIMIT, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"lzma output exceeds decompression limit");
				pDecompressor->Failed = TRUE;
				*pConsumed = iPos;
				return FALSE;
			}
			pDecompressor->State = XRT_LZMA_DEC_HEADER;
		}
	}
	*pConsumed = iPos;
	return TRUE;
}

XXAPI bool xrtLzmaDecompressorUpdate(xrtlzmadecompressor* pDecompressor, const void* pData, size_t iSize)
{
	size_t iConsumed = 0u;
	size_t iRemaining;
	if ( pDecompressor == NULL || pDecompressor->Failed ) { return FALSE; }
	if ( iSize != 0u && pData == NULL ) {
		__xrtLzmaSetError(&pDecompressor->Error, XRT_LZMA_ERROR_ARGUMENT, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "invalid lzma input");
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	if ( pDecompressor->Options.tLimits.iMaxInputBytes != 0u &&
		pDecompressor->InputBytes + iSize > pDecompressor->Options.tLimits.iMaxInputBytes ) {
		__xrtLzmaSetError(&pDecompressor->Error, XRT_LZMA_ERROR_LIMIT, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "lzma input exceeds maxInput limit");
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	if ( iSize != 0u &&
		!xrtBufferAppend(pDecompressor->Carry, (ptr)pData, (uint32)iSize, XBUF_BINARY) ) {
		__xrtLzmaSetError(&pDecompressor->Error, XRT_LZMA_ERROR_MEMORY, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot grow lzma input buffer");
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	pDecompressor->MemberEnded = FALSE;
	pDecompressor->InputBytes += iSize;
	if ( !__xrtLzmaDecompressConsume(pDecompressor, &iConsumed) ) { return FALSE; }
	/* 移除已消费字节。 */
	iRemaining = pDecompressor->Carry->Length - iConsumed;
	if ( iRemaining != 0u && iConsumed != 0u ) {
		memmove(pDecompressor->Carry->Buffer, (uint8*)pDecompressor->Carry->Buffer + iConsumed, iRemaining);
	}
	pDecompressor->Carry->Length = (uint32)iRemaining;
	return TRUE;
}

XXAPI bool xrtLzmaDecompressorFinish(xrtlzmadecompressor* pDecompressor)
{
	if ( pDecompressor == NULL || pDecompressor->Failed ) { return FALSE; }
	if ( pDecompressor->Finished ) { return TRUE; }
	if ( pDecompressor->MemberCount == 0u || !pDecompressor->MemberEnded ||
		pDecompressor->Carry->Length != 0u ) {
		__xrtLzmaSetError(&pDecompressor->Error, XRT_LZMA_ERROR_TRUNCATED, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "lzma stream is truncated or empty");
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	pDecompressor->Finished = TRUE;
	return TRUE;
}

XXAPI bool xrtLzmaDecompressorDrain(xrtlzmadecompressor* pDecompressor, xwriter pWriter)
{
	if ( pDecompressor == NULL || pDecompressor->Failed || pWriter == NULL ) { return FALSE; }
	if ( pDecompressor->Output->Length != 0u &&
		!xrtWriterWriteAll(pWriter, pDecompressor->Output->Buffer, pDecompressor->Output->Length) ) {
		__xrtLzmaSetError(&pDecompressor->Error, XRT_LZMA_ERROR_IO, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot write lzma output stream");
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	pDecompressor->Output->Length = 0u;
	return TRUE;
}

XXAPI xbuffer xrtLzmaDecompressorTakeBuffer(xrtlzmadecompressor* pDecompressor)
{
	xbuffer pOutput;
	if ( pDecompressor == NULL || pDecompressor->Failed || !pDecompressor->Finished ) { return NULL; }
	pOutput = pDecompressor->Output;
	pDecompressor->Output = NULL;
	return pOutput;
}

XXAPI xbuffer xrtLzmaDecompressorDrainBuffer(xrtlzmadecompressor* pDecompressor)
{
	xbuffer pOutput;
	if ( pDecompressor == NULL || pDecompressor->Failed || pDecompressor->Output == NULL ) { return NULL; }
	pOutput = xrtBufferFrom((ptr)pDecompressor->Output->Buffer, (size_t)pDecompressor->Output->Length);
	pDecompressor->Output->Length = 0u;
	return pOutput;
}

XXAPI uint64 xrtLzmaDecompressorInputBytes(const xrtlzmadecompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->InputBytes : 0u;
}

XXAPI uint64 xrtLzmaDecompressorOutputBytes(const xrtlzmadecompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->OutputBytes : 0u;
}

XXAPI bool xrtLzmaDecompressorFinished(const xrtlzmadecompressor* pDecompressor)
{
	return pDecompressor != NULL && pDecompressor->MemberEnded;
}

XXAPI uint64 xrtLzmaDecompressorMembers(const xrtlzmadecompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->MemberCount : 0u;
}

XXAPI const xrtlzmaerror* xrtLzmaDecompressorError(const xrtlzmadecompressor* pDecompressor)
{
	return pDecompressor != NULL ? &pDecompressor->Error : NULL;
}

XXAPI void xrtLzmaDecompressorDestroy(xrtlzmadecompressor* pDecompressor)
{
	if ( pDecompressor == NULL ) { return; }
	xrtBufferDestroy(pDecompressor->Carry);
	xrtBufferDestroy(pDecompressor->Output);
	xrtFree(pDecompressor);
}



// ------------------------------------ 一次性接口 ------------------------------------

XXAPI bool xrtLzmaCompress(const void* pData, size_t iSize, const xrtlzmaoptions* pOptions,
	xbuffer* pOutput, xrtlzmaerror* pError)
{
	xrtlzmaoptions tOptions;
	xrtlzmacompressor* pCompressor;
	bool bRet;
	xrtLzmaErrorInit(pError);
	if ( pOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtLzmaSetError(pError, XRT_LZMA_ERROR_ARGUMENT, 0, 0, 0, "invalid lzma input");
		return FALSE;
	}
	if ( pOptions != NULL ) { tOptions = *pOptions; } else { xrtLzmaOptionsInit(&tOptions); }
	pCompressor = xrtLzmaCompressorCreate(&tOptions);
	if ( pCompressor == NULL ) {
		__xrtLzmaSetError(pError, XRT_LZMA_ERROR_MEMORY, 0, iSize, 0, "cannot allocate lzma compressor");
		return FALSE;
	}
	bRet = xrtLzmaCompressorUpdate(pCompressor, pData, iSize) &&
		xrtLzmaCompressorFinish(pCompressor);
	if ( bRet ) { *pOutput = xrtLzmaCompressorTakeBuffer(pCompressor); bRet = *pOutput != NULL; }
	if ( !bRet ) { __xrtLzmaCopyError(pError, &pCompressor->Error); }
	xrtLzmaCompressorDestroy(pCompressor);
	return bRet;
}

XXAPI bool xrtLzmaDecompress(const void* pData, size_t iSize, const xrtlzmaoptions* pOptions,
	xbuffer* pOutput, xrtlzmaerror* pError)
{
	xrtlzmaoptions tOptions;
	xrtlzmadecompressor* pDecompressor;
	bool bRet;
	xrtLzmaErrorInit(pError);
	if ( pOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtLzmaSetError(pError, XRT_LZMA_ERROR_ARGUMENT, 0, 0, 0, "invalid lzma input");
		return FALSE;
	}
	if ( pOptions != NULL ) { tOptions = *pOptions; } else { xrtLzmaOptionsInit(&tOptions); }
	pDecompressor = xrtLzmaDecompressorCreate(&tOptions);
	if ( pDecompressor == NULL ) {
		__xrtLzmaSetError(pError, XRT_LZMA_ERROR_MEMORY, 0, iSize, 0, "cannot allocate lzma decompressor");
		return FALSE;
	}
	bRet = xrtLzmaDecompressorUpdate(pDecompressor, pData, iSize) &&
		xrtLzmaDecompressorFinish(pDecompressor);
	if ( bRet ) { *pOutput = xrtLzmaDecompressorTakeBuffer(pDecompressor); bRet = *pOutput != NULL; }
	if ( !bRet ) { __xrtLzmaCopyError(pError, &pDecompressor->Error); }
	xrtLzmaDecompressorDestroy(pDecompressor);
	return bRet;
}



// ------------------------------------ 文件流接口 ------------------------------------

XXAPI bool xrtLzmaCompressStream(xreader pReader, xwriter pWriter, const xrtlzmaoptions* pOptions,
	xrtlzmaerror* pError)
{
	uint8 sInput[XRT_LZMA_CHUNK];
	xrtlzmacompressor* pCompressor;
	bool bRet = TRUE;
	xrtLzmaErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		__xrtLzmaSetError(pError, XRT_LZMA_ERROR_ARGUMENT, 0, 0, 0, "invalid lzma stream");
		return FALSE;
	}
	pCompressor = xrtLzmaCompressorCreate(pOptions);
	if ( pCompressor == NULL ) { return FALSE; }
	while ( bRet ) {
		uint64 iRead = 0u;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtLzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_IO, 0,
				pCompressor->InputBytes, pCompressor->OutputBytes, "cannot read lzma input stream");
			bRet = FALSE;
			break;
		}
		if ( iRead == 0u ) { break; }
		bRet = xrtLzmaCompressorUpdate(pCompressor, sInput, (size_t)iRead) &&
			xrtLzmaCompressorDrain(pCompressor, pWriter);
	}
	if ( bRet ) { bRet = xrtLzmaCompressorFinish(pCompressor) && xrtLzmaCompressorDrain(pCompressor, pWriter); }
	if ( !bRet ) { __xrtLzmaCopyError(pError, &pCompressor->Error); }
	xrtLzmaCompressorDestroy(pCompressor);
	return bRet;
}

XXAPI bool xrtLzmaDecompressStream(xreader pReader, xwriter pWriter, const xrtlzmaoptions* pOptions,
	xrtlzmaerror* pError)
{
	uint8 sInput[XRT_LZMA_CHUNK];
	xrtlzmadecompressor* pDecompressor;
	bool bRet = TRUE;
	xrtLzmaErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		__xrtLzmaSetError(pError, XRT_LZMA_ERROR_ARGUMENT, 0, 0, 0, "invalid lzma stream");
		return FALSE;
	}
	pDecompressor = xrtLzmaDecompressorCreate(pOptions);
	if ( pDecompressor == NULL ) { return FALSE; }
	while ( bRet ) {
		uint64 iRead = 0u;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtLzmaSetError(&pDecompressor->Error, XRT_LZMA_ERROR_IO, 0,
				pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot read lzma input stream");
			bRet = FALSE;
			break;
		}
		if ( iRead == 0u ) { break; }
		bRet = xrtLzmaDecompressorUpdate(pDecompressor, sInput, (size_t)iRead) &&
			xrtLzmaDecompressorDrain(pDecompressor, pWriter);
	}
	if ( bRet ) { bRet = xrtLzmaDecompressorFinish(pDecompressor) && xrtLzmaDecompressorDrain(pDecompressor, pWriter); }
	if ( !bRet ) { __xrtLzmaCopyError(pError, &pDecompressor->Error); }
	xrtLzmaDecompressorDestroy(pDecompressor);
	return bRet;
}
