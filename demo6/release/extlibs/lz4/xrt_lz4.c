#include "include/xrt_lz4.h"
#include "third_party/lz4.h"

#include <stdarg.h>
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
	xbuffer Pending;            /* 尚未成块的原始输入 */
	xbuffer Output;             /* 已产出的压缩字节 */
	xrtlz4options Options;
	xrtlz4error Error;
	uint64 InputBytes;
	uint64 OutputBytes;
	bool Initialized;
	bool Finished;
	bool Failed;
};

typedef enum {
	XRT_LZ4_DEC_HEADER = 0,     /* 读取 4 字节块大小 */
	XRT_LZ4_DEC_BLOCK = 1       /* 读取压缩块体 */
} xrtlz4decstate;

struct xrtlz4decompressor {
	xbuffer Carry;              /* 尚未消费完的输入字节 */
	xbuffer Output;             /* 已产出的原始字节 */
	xrtlz4options Options;
	xrtlz4error Error;
	uint64 InputBytes;
	uint64 OutputBytes;
	uint64 MemberCount;
	size_t BlockDeclared;       /* 当前块声明的原始大小 */
	size_t BlockCompressed;     /* 当前块声明的压缩大小 */
	uint32 BlockCrc;            /* 当前块声明的原始数据 CRC32 */
	size_t BlockConsumed;       /* 当前块已消费的压缩字节 */
	int State;
	bool Initialized;
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

static bool __xrtLz4OverOutputLimit(const xrtresourcelimits* pLimits, uint64 iOutput)
{
	return pLimits->iMaxOutputBytes != 0u && iOutput > pLimits->iMaxOutputBytes;
}

static bool __xrtLz4OverRatio(const xrtresourcelimits* pLimits, uint64 iInput, uint64 iOutput)
{
	uint64 iRatio = pLimits->iMaxCompressionRatio;
	if ( iRatio == 0u || iInput == 0u ) { return FALSE; }
	return iOutput > iInput * iRatio;
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



XXAPI void xrtLz4ErrorInit(xrtlz4error* pError)
{
	if ( pError == NULL ) { return; }
	memset(pError, 0, sizeof(*pError));
	pError->iCode = XRT_LZ4_OK;
}

XXAPI void xrtLz4OptionsInit(xrtlz4options* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_LZ4_OPTIONS_VERSION;
	pOptions->iAcceleration = 1;
	xrtResourceLimitsInit(&pOptions->tLimits);
}



// ------------------------------------ 压缩器 ------------------------------------

XXAPI xrtlz4compressor* xrtLz4CompressorCreate(const xrtlz4options* pOptions)
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
	pCompressor->Pending = xrtBufferCreate(XRT_LZ4_CHUNK);
	pCompressor->Output = xrtBufferCreate(XRT_LZ4_CHUNK);
	if ( pCompressor->Pending == NULL || pCompressor->Output == NULL ) {
		__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_MEMORY, 0, 0, 0,
			"cannot allocate lz4 compressor");
		pCompressor->Failed = TRUE;
		return pCompressor;
	}
	pCompressor->Initialized = TRUE;
	return pCompressor;
}

/* 将 Pending 中恰好 iSize 字节压缩为一个块（大小头 + 压缩体）写入 Output。 */
static bool __xrtLz4FlushBlock(xrtlz4compressor* pCompressor, size_t iSize)
{
	int iBound;
	int iWritten;
	uint8 sHeader[XRT_LZ4_HEADER_SIZE];
	ptr pCompressed;
	if ( iSize == 0u ) { return TRUE; }
	iBound = LZ4_compressBound((int)iSize);
	if ( iBound <= 0 ) {
		__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_ARGUMENT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "lz4 block too large");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	pCompressed = xrtMalloc((size_t)iBound);
	if ( pCompressed == NULL ) {
		__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "out of memory");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	iWritten = LZ4_compress_fast((const char*)pCompressor->Pending->Buffer,
		(char*)pCompressed, (int)iSize, iBound, pCompressor->Options.iAcceleration);
	if ( iWritten <= 0 ) {
		__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_DATA, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "lz4 block compression failed");
		xrtFree(pCompressed);
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	__xrtLz4WriteU32LE(sHeader, (uint32)iSize);
	__xrtLz4WriteU32LE(sHeader + 4u, (uint32)iWritten);
	__xrtLz4WriteU32LE(sHeader + 8u,
		__xrtLz4Crc32(pCompressor->Pending->Buffer, iSize));
	if ( !xrtBufferAppend(pCompressor->Output, (ptr)sHeader, XRT_LZ4_HEADER_SIZE, XBUF_BINARY) ||
		!xrtBufferAppend(pCompressor->Output, pCompressed, (uint32)iWritten, XBUF_BINARY) ) {
		__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow lz4 output buffer");
		xrtFree(pCompressed);
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	xrtFree(pCompressed);
	pCompressor->OutputBytes += (uint64)(XRT_LZ4_HEADER_SIZE + (size_t)iWritten);
	/* 移除已消费的前 iSize 字节。 */
	if ( pCompressor->Pending->Length > iSize ) {
		memmove(pCompressor->Pending->Buffer, (uint8*)pCompressor->Pending->Buffer + iSize,
			pCompressor->Pending->Length - iSize);
	}
	pCompressor->Pending->Length -= (uint32)iSize;
	if ( __xrtLz4OverOutputLimit(&pCompressor->Options.tLimits, pCompressor->OutputBytes) ) {
		__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_LIMIT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "lz4 output exceeds maxOutput limit");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	return TRUE;
}

XXAPI bool xrtLz4CompressorUpdate(xrtlz4compressor* pCompressor, const void* pData, size_t iSize)
{
	if ( pCompressor == NULL || pCompressor->Failed ) { return FALSE; }
	if ( pCompressor->Finished ) {
		__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_STATE, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "lz4 compressor already finished");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	if ( iSize != 0u && pData == NULL ) {
		__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_ARGUMENT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "invalid lz4 input");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	if ( pCompressor->Options.tLimits.iMaxInputBytes != 0u &&
		pCompressor->InputBytes + iSize > pCompressor->Options.tLimits.iMaxInputBytes ) {
		__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_LIMIT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "lz4 input exceeds maxInput limit");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	if ( iSize != 0u &&
		!xrtBufferAppend(pCompressor->Pending, (ptr)pData, (uint32)iSize, XBUF_BINARY) ) {
		__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow lz4 pending buffer");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	pCompressor->InputBytes += iSize;
	while ( pCompressor->Pending->Length >= XRT_LZ4_BLOCK_SIZE ) {
		if ( !__xrtLz4FlushBlock(pCompressor, XRT_LZ4_BLOCK_SIZE) ) { return FALSE; }
	}
	return TRUE;
}

XXAPI bool xrtLz4CompressorFinish(xrtlz4compressor* pCompressor)
{
	uint8 sEnd[XRT_LZ4_HEADER_SIZE];
	if ( pCompressor == NULL || pCompressor->Failed ) { return FALSE; }
	if ( pCompressor->Finished ) { return TRUE; }
	if ( pCompressor->Pending->Length != 0u ) {
		if ( !__xrtLz4FlushBlock(pCompressor, pCompressor->Pending->Length) ) { return FALSE; }
	}
	__xrtLz4WriteU32LE(sEnd, 0u);
	if ( !xrtBufferAppend(pCompressor->Output, (ptr)sEnd, XRT_LZ4_HEADER_SIZE, XBUF_BINARY) ) {
		__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow lz4 output buffer");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	pCompressor->OutputBytes += XRT_LZ4_HEADER_SIZE;
	pCompressor->Finished = TRUE;
	return TRUE;
}

XXAPI bool xrtLz4CompressorDrain(xrtlz4compressor* pCompressor, xwriter pWriter)
{
	if ( pCompressor == NULL || pCompressor->Failed || pWriter == NULL ) { return FALSE; }
	if ( pCompressor->Output->Length != 0u &&
		!xrtWriterWriteAll(pWriter, pCompressor->Output->Buffer, pCompressor->Output->Length) ) {
		__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_IO, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot write lz4 output stream");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	pCompressor->Output->Length = 0u;
	return TRUE;
}

XXAPI xbuffer xrtLz4CompressorTakeBuffer(xrtlz4compressor* pCompressor)
{
	xbuffer pOutput;
	if ( pCompressor == NULL || pCompressor->Failed || !pCompressor->Finished ) { return NULL; }
	pOutput = pCompressor->Output;
	pCompressor->Output = NULL;
	return pOutput;
}

XXAPI xbuffer xrtLz4CompressorDrainBuffer(xrtlz4compressor* pCompressor)
{
	xbuffer pOutput;
	if ( pCompressor == NULL || pCompressor->Failed || pCompressor->Output == NULL ) { return NULL; }
	pOutput = xrtBufferFrom((ptr)pCompressor->Output->Buffer, (size_t)pCompressor->Output->Length);
	pCompressor->Output->Length = 0u;
	return pOutput;
}

XXAPI uint64 xrtLz4CompressorInputBytes(const xrtlz4compressor* pCompressor)
{
	return pCompressor != NULL ? pCompressor->InputBytes : 0u;
}

XXAPI uint64 xrtLz4CompressorOutputBytes(const xrtlz4compressor* pCompressor)
{
	return pCompressor != NULL ? pCompressor->OutputBytes : 0u;
}

XXAPI const xrtlz4error* xrtLz4CompressorError(const xrtlz4compressor* pCompressor)
{
	return pCompressor != NULL ? &pCompressor->Error : NULL;
}

XXAPI void xrtLz4CompressorDestroy(xrtlz4compressor* pCompressor)
{
	if ( pCompressor == NULL ) { return; }
	xrtBufferDestroy(pCompressor->Pending);
	xrtBufferDestroy(pCompressor->Output);
	xrtFree(pCompressor);
}



// ------------------------------------ 解压器 ------------------------------------

XXAPI xrtlz4decompressor* xrtLz4DecompressorCreate(const xrtlz4options* pOptions)
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
	pDecompressor->Carry = xrtBufferCreate(XRT_LZ4_CHUNK);
	pDecompressor->Output = xrtBufferCreate(XRT_LZ4_CHUNK);
	if ( pDecompressor->Carry == NULL || pDecompressor->Output == NULL ) {
		__xrtLz4SetError(&pDecompressor->Error, XRT_LZ4_ERROR_MEMORY, 0, 0, 0,
			"cannot allocate lz4 decompressor");
		pDecompressor->Failed = TRUE;
		return pDecompressor;
	}
	pDecompressor->State = XRT_LZ4_DEC_HEADER;
	pDecompressor->Initialized = TRUE;
	return pDecompressor;
}

/* 消费 Carry 中的输入；*pConsumed 返回实际消费字节数。 */
static bool __xrtLz4DecompressConsume(xrtlz4decompressor* pDecompressor, size_t* pConsumed)
{
	size_t iPos = 0u;
	const uint8* pBuf = (const uint8*)pDecompressor->Carry->Buffer;
	size_t iLen = pDecompressor->Carry->Length;
	while ( iPos < iLen ) {
		if ( pDecompressor->State == XRT_LZ4_DEC_HEADER ) {
			uint32 iDeclared;
			uint32 iCompressed;
			if ( iLen - iPos < XRT_LZ4_HEADER_SIZE ) { break; }
			iDeclared = __xrtLz4ReadU32LE(pBuf + iPos);
			iCompressed = __xrtLz4ReadU32LE(pBuf + iPos + 4u);
			iPos += XRT_LZ4_HEADER_SIZE;
			if ( iDeclared == 0u ) {
				pDecompressor->MemberEnded = TRUE;
				pDecompressor->MemberCount += 1u;
				continue;   /* 可能紧跟下一个成员 */
			}
			pDecompressor->BlockDeclared = (size_t)iDeclared;
			pDecompressor->BlockCompressed = (size_t)iCompressed;
			pDecompressor->BlockCrc = __xrtLz4ReadU32LE(pBuf + iPos - 4u);
			pDecompressor->BlockConsumed = 0u;
			pDecompressor->State = XRT_LZ4_DEC_BLOCK;
		} else {
			int iWritten;
			ptr pPlain;
			if ( pDecompressor->BlockDeclared > LZ4_MAX_INPUT_SIZE ) {
				__xrtLz4SetError(&pDecompressor->Error, XRT_LZ4_ERROR_DATA, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"lz4 block declares too large output");
				pDecompressor->Failed = TRUE;
				*pConsumed = iPos;
				return FALSE;
			}
			if ( iLen - iPos < pDecompressor->BlockCompressed ) { break; }
			pPlain = xrtMalloc(pDecompressor->BlockDeclared + 1u);
			if ( pPlain == NULL ) {
				__xrtLz4SetError(&pDecompressor->Error, XRT_LZ4_ERROR_MEMORY, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes, "out of memory");
				pDecompressor->Failed = TRUE;
				*pConsumed = iPos;
				return FALSE;
			}
			iWritten = LZ4_decompress_safe((const char*)(pBuf + iPos), (char*)pPlain,
				(int)pDecompressor->BlockCompressed, (int)pDecompressor->BlockDeclared);
			if ( iWritten < 0 || (size_t)iWritten != pDecompressor->BlockDeclared ) {
				__xrtLz4SetError(&pDecompressor->Error, XRT_LZ4_ERROR_DATA, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"lz4 block decompression failed");
				xrtFree(pPlain);
				pDecompressor->Failed = TRUE;
				*pConsumed = iPos;
				return FALSE;
			}
			iPos += pDecompressor->BlockCompressed;
			if ( __xrtLz4Crc32(pPlain, (size_t)iWritten) != pDecompressor->BlockCrc ) {
				__xrtLz4SetError(&pDecompressor->Error, XRT_LZ4_ERROR_DATA, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"lz4 block checksum mismatch");
				xrtFree(pPlain);
				pDecompressor->Failed = TRUE;
				*pConsumed = iPos;
				return FALSE;
			}
			if ( !xrtBufferAppend(pDecompressor->Output, pPlain, (uint32)iWritten, XBUF_BINARY) ) {
				__xrtLz4SetError(&pDecompressor->Error, XRT_LZ4_ERROR_MEMORY, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"cannot grow lz4 output buffer");
				xrtFree(pPlain);
				pDecompressor->Failed = TRUE;
				*pConsumed = iPos;
				return FALSE;
			}
			xrtFree(pPlain);
			pDecompressor->OutputBytes += (uint64)iWritten;
			if ( __xrtLz4OverOutputLimit(&pDecompressor->Options.tLimits, pDecompressor->OutputBytes) ||
				__xrtLz4OverRatio(&pDecompressor->Options.tLimits, pDecompressor->InputBytes,
					pDecompressor->OutputBytes) ) {
				__xrtLz4SetError(&pDecompressor->Error, XRT_LZ4_ERROR_LIMIT, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"lz4 output exceeds decompression limit");
				pDecompressor->Failed = TRUE;
				*pConsumed = iPos;
				return FALSE;
			}
			pDecompressor->State = XRT_LZ4_DEC_HEADER;
		}
	}
	*pConsumed = iPos;
	return TRUE;
}

XXAPI bool xrtLz4DecompressorUpdate(xrtlz4decompressor* pDecompressor, const void* pData, size_t iSize)
{
	size_t iConsumed = 0u;
	size_t iRemaining;
	if ( pDecompressor == NULL || pDecompressor->Failed ) { return FALSE; }
	if ( iSize != 0u && pData == NULL ) {
		__xrtLz4SetError(&pDecompressor->Error, XRT_LZ4_ERROR_ARGUMENT, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "invalid lz4 input");
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	if ( pDecompressor->Options.tLimits.iMaxInputBytes != 0u &&
		pDecompressor->InputBytes + iSize > pDecompressor->Options.tLimits.iMaxInputBytes ) {
		__xrtLz4SetError(&pDecompressor->Error, XRT_LZ4_ERROR_LIMIT, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "lz4 input exceeds maxInput limit");
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	if ( iSize != 0u &&
		!xrtBufferAppend(pDecompressor->Carry, (ptr)pData, (uint32)iSize, XBUF_BINARY) ) {
		__xrtLz4SetError(&pDecompressor->Error, XRT_LZ4_ERROR_MEMORY, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot grow lz4 input buffer");
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	pDecompressor->MemberEnded = FALSE;
	pDecompressor->InputBytes += iSize;
	if ( !__xrtLz4DecompressConsume(pDecompressor, &iConsumed) ) { return FALSE; }
	/* 移除已消费字节。 */
	iRemaining = pDecompressor->Carry->Length - iConsumed;
	if ( iRemaining != 0u && iConsumed != 0u ) {
		memmove(pDecompressor->Carry->Buffer, (uint8*)pDecompressor->Carry->Buffer + iConsumed, iRemaining);
	}
	pDecompressor->Carry->Length = (uint32)iRemaining;
	return TRUE;
}

XXAPI bool xrtLz4DecompressorFinish(xrtlz4decompressor* pDecompressor)
{
	if ( pDecompressor == NULL || pDecompressor->Failed ) { return FALSE; }
	if ( pDecompressor->Finished ) { return TRUE; }
	if ( pDecompressor->MemberCount == 0u || !pDecompressor->MemberEnded ||
		pDecompressor->Carry->Length != 0u ) {
		__xrtLz4SetError(&pDecompressor->Error, XRT_LZ4_ERROR_TRUNCATED, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "lz4 stream is truncated or empty");
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	pDecompressor->Finished = TRUE;
	return TRUE;
}

XXAPI bool xrtLz4DecompressorDrain(xrtlz4decompressor* pDecompressor, xwriter pWriter)
{
	if ( pDecompressor == NULL || pDecompressor->Failed || pWriter == NULL ) { return FALSE; }
	if ( pDecompressor->Output->Length != 0u &&
		!xrtWriterWriteAll(pWriter, pDecompressor->Output->Buffer, pDecompressor->Output->Length) ) {
		__xrtLz4SetError(&pDecompressor->Error, XRT_LZ4_ERROR_IO, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot write lz4 output stream");
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	pDecompressor->Output->Length = 0u;
	return TRUE;
}

XXAPI xbuffer xrtLz4DecompressorTakeBuffer(xrtlz4decompressor* pDecompressor)
{
	xbuffer pOutput;
	if ( pDecompressor == NULL || pDecompressor->Failed || !pDecompressor->Finished ) { return NULL; }
	pOutput = pDecompressor->Output;
	pDecompressor->Output = NULL;
	return pOutput;
}

XXAPI xbuffer xrtLz4DecompressorDrainBuffer(xrtlz4decompressor* pDecompressor)
{
	xbuffer pOutput;
	if ( pDecompressor == NULL || pDecompressor->Failed || pDecompressor->Output == NULL ) { return NULL; }
	pOutput = xrtBufferFrom((ptr)pDecompressor->Output->Buffer, (size_t)pDecompressor->Output->Length);
	pDecompressor->Output->Length = 0u;
	return pOutput;
}

XXAPI uint64 xrtLz4DecompressorInputBytes(const xrtlz4decompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->InputBytes : 0u;
}

XXAPI uint64 xrtLz4DecompressorOutputBytes(const xrtlz4decompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->OutputBytes : 0u;
}

XXAPI bool xrtLz4DecompressorFinished(const xrtlz4decompressor* pDecompressor)
{
	return pDecompressor != NULL && pDecompressor->MemberEnded;
}

XXAPI uint64 xrtLz4DecompressorMembers(const xrtlz4decompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->MemberCount : 0u;
}

XXAPI const xrtlz4error* xrtLz4DecompressorError(const xrtlz4decompressor* pDecompressor)
{
	return pDecompressor != NULL ? &pDecompressor->Error : NULL;
}

XXAPI void xrtLz4DecompressorDestroy(xrtlz4decompressor* pDecompressor)
{
	if ( pDecompressor == NULL ) { return; }
	xrtBufferDestroy(pDecompressor->Carry);
	xrtBufferDestroy(pDecompressor->Output);
	xrtFree(pDecompressor);
}



// ------------------------------------ 一次性接口 ------------------------------------

XXAPI bool xrtLz4Compress(const void* pData, size_t iSize, const xrtlz4options* pOptions,
	xbuffer* pOutput, xrtlz4error* pError)
{
	xrtlz4options tOptions;
	xrtlz4compressor* pCompressor;
	bool bRet;
	xrtLz4ErrorInit(pError);
	if ( pOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtLz4SetError(pError, XRT_LZ4_ERROR_ARGUMENT, 0, 0, 0, "invalid lz4 input");
		return FALSE;
	}
	if ( pOptions != NULL ) { tOptions = *pOptions; } else { xrtLz4OptionsInit(&tOptions); }
	pCompressor = xrtLz4CompressorCreate(&tOptions);
	if ( pCompressor == NULL ) {
		__xrtLz4SetError(pError, XRT_LZ4_ERROR_MEMORY, 0, iSize, 0, "cannot allocate lz4 compressor");
		return FALSE;
	}
	bRet = xrtLz4CompressorUpdate(pCompressor, pData, iSize) &&
		xrtLz4CompressorFinish(pCompressor);
	if ( bRet ) { *pOutput = xrtLz4CompressorTakeBuffer(pCompressor); bRet = *pOutput != NULL; }
	if ( !bRet ) { __xrtLz4CopyError(pError, &pCompressor->Error); }
	xrtLz4CompressorDestroy(pCompressor);
	return bRet;
}

XXAPI bool xrtLz4Decompress(const void* pData, size_t iSize, const xrtlz4options* pOptions,
	xbuffer* pOutput, xrtlz4error* pError)
{
	xrtlz4options tOptions;
	xrtlz4decompressor* pDecompressor;
	bool bRet;
	xrtLz4ErrorInit(pError);
	if ( pOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtLz4SetError(pError, XRT_LZ4_ERROR_ARGUMENT, 0, 0, 0, "invalid lz4 input");
		return FALSE;
	}
	if ( pOptions != NULL ) { tOptions = *pOptions; } else { xrtLz4OptionsInit(&tOptions); }
	pDecompressor = xrtLz4DecompressorCreate(&tOptions);
	if ( pDecompressor == NULL ) {
		__xrtLz4SetError(pError, XRT_LZ4_ERROR_MEMORY, 0, iSize, 0, "cannot allocate lz4 decompressor");
		return FALSE;
	}
	bRet = xrtLz4DecompressorUpdate(pDecompressor, pData, iSize) &&
		xrtLz4DecompressorFinish(pDecompressor);
	if ( bRet ) { *pOutput = xrtLz4DecompressorTakeBuffer(pDecompressor); bRet = *pOutput != NULL; }
	if ( !bRet ) { __xrtLz4CopyError(pError, &pDecompressor->Error); }
	xrtLz4DecompressorDestroy(pDecompressor);
	return bRet;
}



// ------------------------------------ 文件流接口 ------------------------------------

XXAPI bool xrtLz4CompressStream(xreader pReader, xwriter pWriter, const xrtlz4options* pOptions,
	xrtlz4error* pError)
{
	uint8 sInput[XRT_LZ4_CHUNK];
	xrtlz4compressor* pCompressor;
	bool bRet = TRUE;
	xrtLz4ErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		__xrtLz4SetError(pError, XRT_LZ4_ERROR_ARGUMENT, 0, 0, 0, "invalid lz4 stream");
		return FALSE;
	}
	pCompressor = xrtLz4CompressorCreate(pOptions);
	if ( pCompressor == NULL ) { return FALSE; }
	while ( bRet ) {
		uint64 iRead = 0u;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtLz4SetError(&pCompressor->Error, XRT_LZ4_ERROR_IO, 0,
				pCompressor->InputBytes, pCompressor->OutputBytes, "cannot read lz4 input stream");
			bRet = FALSE;
			break;
		}
		if ( iRead == 0u ) { break; }
		bRet = xrtLz4CompressorUpdate(pCompressor, sInput, (size_t)iRead) &&
			xrtLz4CompressorDrain(pCompressor, pWriter);
	}
	if ( bRet ) { bRet = xrtLz4CompressorFinish(pCompressor) && xrtLz4CompressorDrain(pCompressor, pWriter); }
	if ( !bRet ) { __xrtLz4CopyError(pError, &pCompressor->Error); }
	xrtLz4CompressorDestroy(pCompressor);
	return bRet;
}

XXAPI bool xrtLz4DecompressStream(xreader pReader, xwriter pWriter, const xrtlz4options* pOptions,
	xrtlz4error* pError)
{
	uint8 sInput[XRT_LZ4_CHUNK];
	xrtlz4decompressor* pDecompressor;
	bool bRet = TRUE;
	xrtLz4ErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		__xrtLz4SetError(pError, XRT_LZ4_ERROR_ARGUMENT, 0, 0, 0, "invalid lz4 stream");
		return FALSE;
	}
	pDecompressor = xrtLz4DecompressorCreate(pOptions);
	if ( pDecompressor == NULL ) { return FALSE; }
	while ( bRet ) {
		uint64 iRead = 0u;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtLz4SetError(&pDecompressor->Error, XRT_LZ4_ERROR_IO, 0,
				pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot read lz4 input stream");
			bRet = FALSE;
			break;
		}
		if ( iRead == 0u ) { break; }
		bRet = xrtLz4DecompressorUpdate(pDecompressor, sInput, (size_t)iRead) &&
			xrtLz4DecompressorDrain(pDecompressor, pWriter);
	}
	if ( bRet ) { bRet = xrtLz4DecompressorFinish(pDecompressor) && xrtLz4DecompressorDrain(pDecompressor, pWriter); }
	if ( !bRet ) { __xrtLz4CopyError(pError, &pDecompressor->Error); }
	xrtLz4DecompressorDestroy(pDecompressor);
	return bRet;
}
