#include "include/xrt_miniz.h"
#include "third_party/miniz.h"

#include <stdarg.h>
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
	xbuffer Pending;            /* 尚未成块的原始输入 */
	xbuffer Output;             /* 已产出的压缩字节 */
	xrtminizoptions Options;
	xrtminizerror Error;
	uint64 InputBytes;
	uint64 OutputBytes;
	bool Initialized;
	bool Finished;
	bool Failed;
};

typedef enum {
	XRT_MINIZ_DEC_HEADER = 0,   /* 读取 12 字节块头 */
	XRT_MINIZ_DEC_BLOCK = 1     /* 读取压缩块体 */
} xrtminizdecstate;

struct xrtminizdecompressor {
	xbuffer Carry;              /* 尚未消费完的输入字节 */
	xbuffer Output;             /* 已产出的原始字节 */
	xrtminizoptions Options;
	xrtminizerror Error;
	uint64 InputBytes;
	uint64 OutputBytes;
	uint64 MemberCount;
	size_t BlockDeclared;       /* 当前块声明的原始大小 */
	size_t BlockCompressed;     /* 当前块声明的压缩大小 */
	uint32 BlockCrc;            /* 当前块声明的原始数据 CRC32 */
	int State;
	bool Initialized;
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

static bool __xrtMinizOverOutputLimit(const xrtresourcelimits* pLimits, uint64 iOutput)
{
	return pLimits->iMaxOutputBytes != 0u && iOutput > pLimits->iMaxOutputBytes;
}

static bool __xrtMinizOverRatio(const xrtresourcelimits* pLimits, uint64 iInput, uint64 iOutput)
{
	uint64 iRatio = pLimits->iMaxCompressionRatio;
	if ( iRatio == 0u || iInput == 0u ) { return FALSE; }
	return iOutput > iInput * iRatio;
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



XXAPI void xrtMinizErrorInit(xrtminizerror* pError)
{
	if ( pError == NULL ) { return; }
	memset(pError, 0, sizeof(*pError));
	pError->iCode = XRT_MINIZ_OK;
}

XXAPI void xrtMinizOptionsInit(xrtminizoptions* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_MINIZ_OPTIONS_VERSION;
	pOptions->iLevel = MZ_DEFAULT_LEVEL;
	xrtResourceLimitsInit(&pOptions->tLimits);
}



// ------------------------------------ 压缩器 ------------------------------------

XXAPI xrtminizcompressor* xrtMinizCompressorCreate(const xrtminizoptions* pOptions)
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
	pCompressor->Pending = xrtBufferCreate(XRT_MINIZ_CHUNK);
	pCompressor->Output = xrtBufferCreate(XRT_MINIZ_CHUNK);
	if ( pCompressor->Pending == NULL || pCompressor->Output == NULL ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_MEMORY, 0, 0, 0,
			"cannot allocate miniz compressor");
		pCompressor->Failed = TRUE;
		return pCompressor;
	}
	pCompressor->Initialized = TRUE;
	return pCompressor;
}

/* 将 Pending 中恰好 iSize 字节压缩为一个块（大小头 + 压缩体）写入 Output。 */
static bool __xrtMinizFlushBlock(xrtminizcompressor* pCompressor, size_t iSize)
{
	mz_ulong iBound;
	mz_ulong iWritten;
	int iResult;
	uint8 sHeader[XRT_MINIZ_HEADER_SIZE];
	ptr pCompressed;
	if ( iSize == 0u ) { return TRUE; }
	if ( iSize > XRT_MINIZ_MAX_PLAIN ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_ARGUMENT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "miniz block too large");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	iBound = mz_compressBound((mz_ulong)iSize);
	if ( iBound == 0u ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_ARGUMENT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "miniz block too large");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	pCompressed = xrtMalloc((size_t)iBound);
	if ( pCompressed == NULL ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "out of memory");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	iWritten = iBound;
	iResult = mz_compress2((unsigned char*)pCompressed, &iWritten,
		(const unsigned char*)pCompressor->Pending->Buffer, (mz_ulong)iSize,
		pCompressor->Options.iLevel);
	if ( iResult != MZ_OK ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_DATA, iResult,
			pCompressor->InputBytes, pCompressor->OutputBytes, "%s",
			mz_error(iResult));
		xrtFree(pCompressed);
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	__xrtMinizWriteU32LE(sHeader, (uint32)iSize);
	__xrtMinizWriteU32LE(sHeader + 4u, (uint32)iWritten);
	__xrtMinizWriteU32LE(sHeader + 8u,
		__xrtMinizCrc32(pCompressor->Pending->Buffer, iSize));
	if ( !xrtBufferAppend(pCompressor->Output, (ptr)sHeader, XRT_MINIZ_HEADER_SIZE, XBUF_BINARY) ||
		!xrtBufferAppend(pCompressor->Output, pCompressed, (uint32)iWritten, XBUF_BINARY) ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow miniz output buffer");
		xrtFree(pCompressed);
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	xrtFree(pCompressed);
	pCompressor->OutputBytes += (uint64)(XRT_MINIZ_HEADER_SIZE + (size_t)iWritten);
	/* 移除已消费的前 iSize 字节。 */
	if ( pCompressor->Pending->Length > iSize ) {
		memmove(pCompressor->Pending->Buffer, (uint8*)pCompressor->Pending->Buffer + iSize,
			pCompressor->Pending->Length - iSize);
	}
	pCompressor->Pending->Length -= (uint32)iSize;
	if ( __xrtMinizOverOutputLimit(&pCompressor->Options.tLimits, pCompressor->OutputBytes) ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_LIMIT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "miniz output exceeds maxOutput limit");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	return TRUE;
}

XXAPI bool xrtMinizCompressorUpdate(xrtminizcompressor* pCompressor, const void* pData, size_t iSize)
{
	if ( pCompressor == NULL || pCompressor->Failed ) { return FALSE; }
	if ( pCompressor->Finished ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_STATE, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "miniz compressor already finished");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	if ( iSize != 0u && pData == NULL ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_ARGUMENT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "invalid miniz input");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	if ( pCompressor->Options.tLimits.iMaxInputBytes != 0u &&
		pCompressor->InputBytes + iSize > pCompressor->Options.tLimits.iMaxInputBytes ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_LIMIT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "miniz input exceeds maxInput limit");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	if ( iSize != 0u &&
		!xrtBufferAppend(pCompressor->Pending, (ptr)pData, (uint32)iSize, XBUF_BINARY) ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow miniz pending buffer");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	pCompressor->InputBytes += iSize;
	while ( pCompressor->Pending->Length >= XRT_MINIZ_BLOCK_SIZE ) {
		if ( !__xrtMinizFlushBlock(pCompressor, XRT_MINIZ_BLOCK_SIZE) ) { return FALSE; }
	}
	return TRUE;
}

XXAPI bool xrtMinizCompressorFinish(xrtminizcompressor* pCompressor)
{
	uint8 sEnd[XRT_MINIZ_HEADER_SIZE];
	if ( pCompressor == NULL || pCompressor->Failed ) { return FALSE; }
	if ( pCompressor->Finished ) { return TRUE; }
	if ( pCompressor->Pending->Length != 0u ) {
		if ( !__xrtMinizFlushBlock(pCompressor, pCompressor->Pending->Length) ) { return FALSE; }
	}
	memset(sEnd, 0, sizeof(sEnd));
	if ( !xrtBufferAppend(pCompressor->Output, (ptr)sEnd, XRT_MINIZ_HEADER_SIZE, XBUF_BINARY) ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow miniz output buffer");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	pCompressor->OutputBytes += XRT_MINIZ_HEADER_SIZE;
	pCompressor->Finished = TRUE;
	return TRUE;
}

XXAPI bool xrtMinizCompressorDrain(xrtminizcompressor* pCompressor, xwriter pWriter)
{
	if ( pCompressor == NULL || pCompressor->Failed || pWriter == NULL ) { return FALSE; }
	if ( pCompressor->Output->Length != 0u &&
		!xrtWriterWriteAll(pWriter, pCompressor->Output->Buffer, pCompressor->Output->Length) ) {
		__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_IO, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot write miniz output stream");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	pCompressor->Output->Length = 0u;
	return TRUE;
}

XXAPI xbuffer xrtMinizCompressorTakeBuffer(xrtminizcompressor* pCompressor)
{
	xbuffer pOutput;
	if ( pCompressor == NULL || pCompressor->Failed || !pCompressor->Finished ) { return NULL; }
	pOutput = pCompressor->Output;
	pCompressor->Output = NULL;
	return pOutput;
}

XXAPI xbuffer xrtMinizCompressorDrainBuffer(xrtminizcompressor* pCompressor)
{
	xbuffer pOutput;
	if ( pCompressor == NULL || pCompressor->Failed || pCompressor->Output == NULL ) { return NULL; }
	pOutput = xrtBufferFrom((ptr)pCompressor->Output->Buffer, (size_t)pCompressor->Output->Length);
	pCompressor->Output->Length = 0u;
	return pOutput;
}

XXAPI uint64 xrtMinizCompressorInputBytes(const xrtminizcompressor* pCompressor)
{
	return pCompressor != NULL ? pCompressor->InputBytes : 0u;
}

XXAPI uint64 xrtMinizCompressorOutputBytes(const xrtminizcompressor* pCompressor)
{
	return pCompressor != NULL ? pCompressor->OutputBytes : 0u;
}

XXAPI const xrtminizerror* xrtMinizCompressorError(const xrtminizcompressor* pCompressor)
{
	return pCompressor != NULL ? &pCompressor->Error : NULL;
}

XXAPI void xrtMinizCompressorDestroy(xrtminizcompressor* pCompressor)
{
	if ( pCompressor == NULL ) { return; }
	xrtBufferDestroy(pCompressor->Pending);
	xrtBufferDestroy(pCompressor->Output);
	xrtFree(pCompressor);
}



// ------------------------------------ 解压器 ------------------------------------

XXAPI xrtminizdecompressor* xrtMinizDecompressorCreate(const xrtminizoptions* pOptions)
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
	pDecompressor->Carry = xrtBufferCreate(XRT_MINIZ_CHUNK);
	pDecompressor->Output = xrtBufferCreate(XRT_MINIZ_CHUNK);
	if ( pDecompressor->Carry == NULL || pDecompressor->Output == NULL ) {
		__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_MEMORY, 0, 0, 0,
			"cannot allocate miniz decompressor");
		pDecompressor->Failed = TRUE;
		return pDecompressor;
	}
	pDecompressor->State = XRT_MINIZ_DEC_HEADER;
	pDecompressor->Initialized = TRUE;
	return pDecompressor;
}

/* 消费 Carry 中的输入；*pConsumed 返回实际消费字节数。 */
static bool __xrtMinizDecompressConsume(xrtminizdecompressor* pDecompressor, size_t* pConsumed)
{
	size_t iPos = 0u;
	const uint8* pBuf = (const uint8*)pDecompressor->Carry->Buffer;
	size_t iLen = pDecompressor->Carry->Length;
	while ( iPos < iLen ) {
		if ( pDecompressor->State == XRT_MINIZ_DEC_HEADER ) {
			uint32 iDeclared;
			if ( iLen - iPos < XRT_MINIZ_HEADER_SIZE ) { break; }
			iDeclared = __xrtMinizReadU32LE(pBuf + iPos);
			if ( iDeclared == 0u ) {
				iPos += XRT_MINIZ_HEADER_SIZE;
				pDecompressor->MemberEnded = TRUE;
				pDecompressor->MemberCount += 1u;
				continue;   /* 可能紧跟下一个成员 */
			}
			pDecompressor->BlockDeclared = (size_t)iDeclared;
			pDecompressor->BlockCompressed = (size_t)__xrtMinizReadU32LE(pBuf + iPos + 4u);
			pDecompressor->BlockCrc = __xrtMinizReadU32LE(pBuf + iPos + 8u);
			iPos += XRT_MINIZ_HEADER_SIZE;
			pDecompressor->State = XRT_MINIZ_DEC_BLOCK;
		} else {
			mz_ulong iWritten;
			int iResult;
			ptr pPlain;
			if ( pDecompressor->BlockDeclared > XRT_MINIZ_MAX_PLAIN ) {
				__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_DATA, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"miniz block declares too large output");
				pDecompressor->Failed = TRUE;
				*pConsumed = iPos;
				return FALSE;
			}
			if ( iLen - iPos < pDecompressor->BlockCompressed ) { break; }
			pPlain = xrtMalloc(pDecompressor->BlockDeclared + 1u);
			if ( pPlain == NULL ) {
				__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_MEMORY, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes, "out of memory");
				pDecompressor->Failed = TRUE;
				*pConsumed = iPos;
				return FALSE;
			}
			iWritten = (mz_ulong)pDecompressor->BlockDeclared;
			iResult = mz_uncompress((unsigned char*)pPlain, &iWritten,
				(const unsigned char*)(pBuf + iPos), (mz_ulong)pDecompressor->BlockCompressed);
			if ( iResult != MZ_OK || (size_t)iWritten != pDecompressor->BlockDeclared ) {
				__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_DATA, iResult,
					pDecompressor->InputBytes, pDecompressor->OutputBytes, "%s",
					iResult == MZ_OK ? "miniz decompression size mismatch" : mz_error(iResult));
				xrtFree(pPlain);
				pDecompressor->Failed = TRUE;
				*pConsumed = iPos;
				return FALSE;
			}
			iPos += pDecompressor->BlockCompressed;
			if ( __xrtMinizCrc32(pPlain, (size_t)iWritten) != pDecompressor->BlockCrc ) {
				__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_DATA, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"miniz block checksum mismatch");
				xrtFree(pPlain);
				pDecompressor->Failed = TRUE;
				*pConsumed = iPos;
				return FALSE;
			}
			if ( !xrtBufferAppend(pDecompressor->Output, pPlain, (uint32)iWritten, XBUF_BINARY) ) {
				__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_MEMORY, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"cannot grow miniz output buffer");
				xrtFree(pPlain);
				pDecompressor->Failed = TRUE;
				*pConsumed = iPos;
				return FALSE;
			}
			xrtFree(pPlain);
			pDecompressor->OutputBytes += (uint64)iWritten;
			if ( __xrtMinizOverOutputLimit(&pDecompressor->Options.tLimits, pDecompressor->OutputBytes) ||
				__xrtMinizOverRatio(&pDecompressor->Options.tLimits, pDecompressor->InputBytes,
					pDecompressor->OutputBytes) ) {
				__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_LIMIT, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"miniz output exceeds decompression limit");
				pDecompressor->Failed = TRUE;
				*pConsumed = iPos;
				return FALSE;
			}
			pDecompressor->State = XRT_MINIZ_DEC_HEADER;
		}
	}
	*pConsumed = iPos;
	return TRUE;
}

XXAPI bool xrtMinizDecompressorUpdate(xrtminizdecompressor* pDecompressor, const void* pData, size_t iSize)
{
	size_t iConsumed = 0u;
	size_t iRemaining;
	if ( pDecompressor == NULL || pDecompressor->Failed ) { return FALSE; }
	if ( iSize != 0u && pData == NULL ) {
		__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_ARGUMENT, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "invalid miniz input");
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	if ( pDecompressor->Options.tLimits.iMaxInputBytes != 0u &&
		pDecompressor->InputBytes + iSize > pDecompressor->Options.tLimits.iMaxInputBytes ) {
		__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_LIMIT, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "miniz input exceeds maxInput limit");
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	if ( iSize != 0u &&
		!xrtBufferAppend(pDecompressor->Carry, (ptr)pData, (uint32)iSize, XBUF_BINARY) ) {
		__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_MEMORY, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot grow miniz input buffer");
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	pDecompressor->MemberEnded = FALSE;
	pDecompressor->InputBytes += iSize;
	if ( !__xrtMinizDecompressConsume(pDecompressor, &iConsumed) ) { return FALSE; }
	/* 移除已消费字节。 */
	iRemaining = pDecompressor->Carry->Length - iConsumed;
	if ( iRemaining != 0u && iConsumed != 0u ) {
		memmove(pDecompressor->Carry->Buffer, (uint8*)pDecompressor->Carry->Buffer + iConsumed, iRemaining);
	}
	pDecompressor->Carry->Length = (uint32)iRemaining;
	return TRUE;
}

XXAPI bool xrtMinizDecompressorFinish(xrtminizdecompressor* pDecompressor)
{
	if ( pDecompressor == NULL || pDecompressor->Failed ) { return FALSE; }
	if ( pDecompressor->Finished ) { return TRUE; }
	if ( pDecompressor->MemberCount == 0u || !pDecompressor->MemberEnded ||
		pDecompressor->Carry->Length != 0u ) {
		__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_TRUNCATED, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "miniz stream is truncated or empty");
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	pDecompressor->Finished = TRUE;
	return TRUE;
}

XXAPI bool xrtMinizDecompressorDrain(xrtminizdecompressor* pDecompressor, xwriter pWriter)
{
	if ( pDecompressor == NULL || pDecompressor->Failed || pWriter == NULL ) { return FALSE; }
	if ( pDecompressor->Output->Length != 0u &&
		!xrtWriterWriteAll(pWriter, pDecompressor->Output->Buffer, pDecompressor->Output->Length) ) {
		__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_IO, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot write miniz output stream");
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	pDecompressor->Output->Length = 0u;
	return TRUE;
}

XXAPI xbuffer xrtMinizDecompressorTakeBuffer(xrtminizdecompressor* pDecompressor)
{
	xbuffer pOutput;
	if ( pDecompressor == NULL || pDecompressor->Failed || !pDecompressor->Finished ) { return NULL; }
	pOutput = pDecompressor->Output;
	pDecompressor->Output = NULL;
	return pOutput;
}

XXAPI xbuffer xrtMinizDecompressorDrainBuffer(xrtminizdecompressor* pDecompressor)
{
	xbuffer pOutput;
	if ( pDecompressor == NULL || pDecompressor->Failed || pDecompressor->Output == NULL ) { return NULL; }
	pOutput = xrtBufferFrom((ptr)pDecompressor->Output->Buffer, (size_t)pDecompressor->Output->Length);
	pDecompressor->Output->Length = 0u;
	return pOutput;
}

XXAPI uint64 xrtMinizDecompressorInputBytes(const xrtminizdecompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->InputBytes : 0u;
}

XXAPI uint64 xrtMinizDecompressorOutputBytes(const xrtminizdecompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->OutputBytes : 0u;
}

XXAPI bool xrtMinizDecompressorFinished(const xrtminizdecompressor* pDecompressor)
{
	return pDecompressor != NULL && pDecompressor->MemberEnded;
}

XXAPI uint64 xrtMinizDecompressorMembers(const xrtminizdecompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->MemberCount : 0u;
}

XXAPI const xrtminizerror* xrtMinizDecompressorError(const xrtminizdecompressor* pDecompressor)
{
	return pDecompressor != NULL ? &pDecompressor->Error : NULL;
}

XXAPI void xrtMinizDecompressorDestroy(xrtminizdecompressor* pDecompressor)
{
	if ( pDecompressor == NULL ) { return; }
	xrtBufferDestroy(pDecompressor->Carry);
	xrtBufferDestroy(pDecompressor->Output);
	xrtFree(pDecompressor);
}



// ------------------------------------ 一次性接口 ------------------------------------

XXAPI bool xrtMinizCompress(const void* pData, size_t iSize, const xrtminizoptions* pOptions,
	xbuffer* pOutput, xrtminizerror* pError)
{
	xrtminizoptions tOptions;
	xrtminizcompressor* pCompressor;
	bool bRet;
	xrtMinizErrorInit(pError);
	if ( pOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtMinizSetError(pError, XRT_MINIZ_ERROR_ARGUMENT, 0, 0, 0, "invalid miniz input");
		return FALSE;
	}
	if ( pOptions != NULL ) { tOptions = *pOptions; } else { xrtMinizOptionsInit(&tOptions); }
	pCompressor = xrtMinizCompressorCreate(&tOptions);
	if ( pCompressor == NULL ) {
		__xrtMinizSetError(pError, XRT_MINIZ_ERROR_MEMORY, 0, iSize, 0, "cannot allocate miniz compressor");
		return FALSE;
	}
	bRet = xrtMinizCompressorUpdate(pCompressor, pData, iSize) &&
		xrtMinizCompressorFinish(pCompressor);
	if ( bRet ) { *pOutput = xrtMinizCompressorTakeBuffer(pCompressor); bRet = *pOutput != NULL; }
	if ( !bRet ) { __xrtMinizCopyError(pError, &pCompressor->Error); }
	xrtMinizCompressorDestroy(pCompressor);
	return bRet;
}

XXAPI bool xrtMinizDecompress(const void* pData, size_t iSize, const xrtminizoptions* pOptions,
	xbuffer* pOutput, xrtminizerror* pError)
{
	xrtminizoptions tOptions;
	xrtminizdecompressor* pDecompressor;
	bool bRet;
	xrtMinizErrorInit(pError);
	if ( pOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtMinizSetError(pError, XRT_MINIZ_ERROR_ARGUMENT, 0, 0, 0, "invalid miniz input");
		return FALSE;
	}
	if ( pOptions != NULL ) { tOptions = *pOptions; } else { xrtMinizOptionsInit(&tOptions); }
	pDecompressor = xrtMinizDecompressorCreate(&tOptions);
	if ( pDecompressor == NULL ) {
		__xrtMinizSetError(pError, XRT_MINIZ_ERROR_MEMORY, 0, iSize, 0, "cannot allocate miniz decompressor");
		return FALSE;
	}
	bRet = xrtMinizDecompressorUpdate(pDecompressor, pData, iSize) &&
		xrtMinizDecompressorFinish(pDecompressor);
	if ( bRet ) { *pOutput = xrtMinizDecompressorTakeBuffer(pDecompressor); bRet = *pOutput != NULL; }
	if ( !bRet ) { __xrtMinizCopyError(pError, &pDecompressor->Error); }
	xrtMinizDecompressorDestroy(pDecompressor);
	return bRet;
}



// ------------------------------------ 文件流接口 ------------------------------------

XXAPI bool xrtMinizCompressStream(xreader pReader, xwriter pWriter, const xrtminizoptions* pOptions,
	xrtminizerror* pError)
{
	uint8 sInput[XRT_MINIZ_CHUNK];
	xrtminizcompressor* pCompressor;
	bool bRet = TRUE;
	xrtMinizErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		__xrtMinizSetError(pError, XRT_MINIZ_ERROR_ARGUMENT, 0, 0, 0, "invalid miniz stream");
		return FALSE;
	}
	pCompressor = xrtMinizCompressorCreate(pOptions);
	if ( pCompressor == NULL ) { return FALSE; }
	while ( bRet ) {
		uint64 iRead = 0u;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtMinizSetError(&pCompressor->Error, XRT_MINIZ_ERROR_IO, 0,
				pCompressor->InputBytes, pCompressor->OutputBytes, "cannot read miniz input stream");
			bRet = FALSE;
			break;
		}
		if ( iRead == 0u ) { break; }
		bRet = xrtMinizCompressorUpdate(pCompressor, sInput, (size_t)iRead) &&
			xrtMinizCompressorDrain(pCompressor, pWriter);
	}
	if ( bRet ) { bRet = xrtMinizCompressorFinish(pCompressor) && xrtMinizCompressorDrain(pCompressor, pWriter); }
	if ( !bRet ) { __xrtMinizCopyError(pError, &pCompressor->Error); }
	xrtMinizCompressorDestroy(pCompressor);
	return bRet;
}

XXAPI bool xrtMinizDecompressStream(xreader pReader, xwriter pWriter, const xrtminizoptions* pOptions,
	xrtminizerror* pError)
{
	uint8 sInput[XRT_MINIZ_CHUNK];
	xrtminizdecompressor* pDecompressor;
	bool bRet = TRUE;
	xrtMinizErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		__xrtMinizSetError(pError, XRT_MINIZ_ERROR_ARGUMENT, 0, 0, 0, "invalid miniz stream");
		return FALSE;
	}
	pDecompressor = xrtMinizDecompressorCreate(pOptions);
	if ( pDecompressor == NULL ) { return FALSE; }
	while ( bRet ) {
		uint64 iRead = 0u;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtMinizSetError(&pDecompressor->Error, XRT_MINIZ_ERROR_IO, 0,
				pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot read miniz input stream");
			bRet = FALSE;
			break;
		}
		if ( iRead == 0u ) { break; }
		bRet = xrtMinizDecompressorUpdate(pDecompressor, sInput, (size_t)iRead) &&
			xrtMinizDecompressorDrain(pDecompressor, pWriter);
	}
	if ( bRet ) { bRet = xrtMinizDecompressorFinish(pDecompressor) && xrtMinizDecompressorDrain(pDecompressor, pWriter); }
	if ( !bRet ) { __xrtMinizCopyError(pError, &pDecompressor->Error); }
	xrtMinizDecompressorDestroy(pDecompressor);
	return bRet;
}
