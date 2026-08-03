#include "include/xrt_brotli.h"
#include "third_party/include/brotli/encode.h"
#include "third_party/include/brotli/decode.h"

#include <stdarg.h>
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
	xbuffer Pending;            /* 尚未成块的原始输入 */
	xbuffer Output;             /* 已产出的压缩字节 */
	xrtbrotlioptions Options;
	xrtbrotlierror Error;
	uint64 InputBytes;
	uint64 OutputBytes;
	bool Initialized;
	bool Finished;
	bool Failed;
};

typedef enum {
	XRT_BROTLI_DEC_HEADER = 0,   /* 读取 12 字节块头 */
	XRT_BROTLI_DEC_BLOCK = 1     /* 读取压缩块体 */
} xrtbrotlidecstate;

struct xrtbrotlidecompressor {
	xbuffer Carry;              /* 尚未消费完的输入字节 */
	xbuffer Output;             /* 已产出的原始字节 */
	xrtbrotlioptions Options;
	xrtbrotlierror Error;
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

static bool __xrtBrotliOverOutputLimit(const xrtresourcelimits* pLimits, uint64 iOutput)
{
	return pLimits->iMaxOutputBytes != 0u && iOutput > pLimits->iMaxOutputBytes;
}

static bool __xrtBrotliOverRatio(const xrtresourcelimits* pLimits, uint64 iInput, uint64 iOutput)
{
	uint64 iRatio = pLimits->iMaxCompressionRatio;
	if ( iRatio == 0u || iInput == 0u ) { return FALSE; }
	return iOutput > iInput * iRatio;
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



XXAPI void xrtBrotliErrorInit(xrtbrotlierror* pError)
{
	if ( pError == NULL ) { return; }
	memset(pError, 0, sizeof(*pError));
	pError->iCode = XRT_BROTLI_OK;
}

XXAPI void xrtBrotliOptionsInit(xrtbrotlioptions* pOptions)
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

XXAPI xrtbrotlicompressor* xrtBrotliCompressorCreate(const xrtbrotlioptions* pOptions)
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
	pCompressor->Pending = xrtBufferCreate(XRT_BROTLI_CHUNK);
	pCompressor->Output = xrtBufferCreate(XRT_BROTLI_CHUNK);
	if ( pCompressor->Pending == NULL || pCompressor->Output == NULL ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_MEMORY, 0, 0, 0,
			"cannot allocate brotli compressor");
		pCompressor->Failed = TRUE;
		return pCompressor;
	}
	pCompressor->Initialized = TRUE;
	return pCompressor;
}

/* 将 Pending 中恰好 iSize 字节压缩为一个块（大小头 + 压缩体）写入 Output。 */
static bool __xrtBrotliFlushBlock(xrtbrotlicompressor* pCompressor, size_t iSize)
{
	size_t iBound;
	size_t iWritten;
	uint8 sHeader[XRT_BROTLI_HEADER_SIZE];
	ptr pCompressed;
	if ( iSize == 0u ) { return TRUE; }
	iBound = BrotliEncoderMaxCompressedSize(iSize);
	if ( iBound == 0u ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_ARGUMENT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "brotli block too large");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	pCompressed = xrtMalloc(iBound);
	if ( pCompressed == NULL ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "out of memory");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	iWritten = iBound;
	if ( BrotliEncoderCompress(pCompressor->Options.iQuality, pCompressor->Options.iWindow,
		(BrotliEncoderMode)pCompressor->Options.iMode, iSize,
		(const uint8*)pCompressor->Pending->Buffer, &iWritten,
		(uint8*)pCompressed) != BROTLI_TRUE ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_DATA, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "brotli compression failed");
		xrtFree(pCompressed);
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	__xrtBrotliWriteU32LE(sHeader, (uint32)iSize);
	__xrtBrotliWriteU32LE(sHeader + 4u, (uint32)iWritten);
	__xrtBrotliWriteU32LE(sHeader + 8u,
		__xrtBrotliCrc32(pCompressor->Pending->Buffer, iSize));
	if ( !xrtBufferAppend(pCompressor->Output, (ptr)sHeader, XRT_BROTLI_HEADER_SIZE, XBUF_BINARY) ||
		!xrtBufferAppend(pCompressor->Output, pCompressed, (uint32)iWritten, XBUF_BINARY) ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow brotli output buffer");
		xrtFree(pCompressed);
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	xrtFree(pCompressed);
	pCompressor->OutputBytes += (uint64)(XRT_BROTLI_HEADER_SIZE + (size_t)iWritten);
	/* 移除已消费的前 iSize 字节。 */
	if ( pCompressor->Pending->Length > iSize ) {
		memmove(pCompressor->Pending->Buffer, (uint8*)pCompressor->Pending->Buffer + iSize,
			pCompressor->Pending->Length - iSize);
	}
	pCompressor->Pending->Length -= (uint32)iSize;
	if ( __xrtBrotliOverOutputLimit(&pCompressor->Options.tLimits, pCompressor->OutputBytes) ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_LIMIT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "brotli output exceeds maxOutput limit");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	return TRUE;
}

XXAPI bool xrtBrotliCompressorUpdate(xrtbrotlicompressor* pCompressor, const void* pData, size_t iSize)
{
	if ( pCompressor == NULL || pCompressor->Failed ) { return FALSE; }
	if ( pCompressor->Finished ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_STATE, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "brotli compressor already finished");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	if ( iSize != 0u && pData == NULL ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_ARGUMENT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "invalid brotli input");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	if ( pCompressor->Options.tLimits.iMaxInputBytes != 0u &&
		pCompressor->InputBytes + iSize > pCompressor->Options.tLimits.iMaxInputBytes ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_LIMIT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "brotli input exceeds maxInput limit");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	if ( iSize != 0u &&
		!xrtBufferAppend(pCompressor->Pending, (ptr)pData, (uint32)iSize, XBUF_BINARY) ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow brotli pending buffer");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	pCompressor->InputBytes += iSize;
	while ( pCompressor->Pending->Length >= XRT_BROTLI_BLOCK_SIZE ) {
		if ( !__xrtBrotliFlushBlock(pCompressor, XRT_BROTLI_BLOCK_SIZE) ) { return FALSE; }
	}
	return TRUE;
}

XXAPI bool xrtBrotliCompressorFinish(xrtbrotlicompressor* pCompressor)
{
	uint8 sEnd[XRT_BROTLI_HEADER_SIZE];
	if ( pCompressor == NULL || pCompressor->Failed ) { return FALSE; }
	if ( pCompressor->Finished ) { return TRUE; }
	if ( pCompressor->Pending->Length != 0u ) {
		if ( !__xrtBrotliFlushBlock(pCompressor, pCompressor->Pending->Length) ) { return FALSE; }
	}
	memset(sEnd, 0, sizeof(sEnd));
	if ( !xrtBufferAppend(pCompressor->Output, (ptr)sEnd, XRT_BROTLI_HEADER_SIZE, XBUF_BINARY) ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot grow brotli output buffer");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	pCompressor->OutputBytes += XRT_BROTLI_HEADER_SIZE;
	pCompressor->Finished = TRUE;
	return TRUE;
}

XXAPI bool xrtBrotliCompressorDrain(xrtbrotlicompressor* pCompressor, xwriter pWriter)
{
	if ( pCompressor == NULL || pCompressor->Failed || pWriter == NULL ) { return FALSE; }
	if ( pCompressor->Output->Length != 0u &&
		!xrtWriterWriteAll(pWriter, pCompressor->Output->Buffer, pCompressor->Output->Length) ) {
		__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_IO, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes, "cannot write brotli output stream");
		pCompressor->Failed = TRUE;
		return FALSE;
	}
	pCompressor->Output->Length = 0u;
	return TRUE;
}

XXAPI xbuffer xrtBrotliCompressorTakeBuffer(xrtbrotlicompressor* pCompressor)
{
	xbuffer pOutput;
	if ( pCompressor == NULL || pCompressor->Failed || !pCompressor->Finished ) { return NULL; }
	pOutput = pCompressor->Output;
	pCompressor->Output = NULL;
	return pOutput;
}

XXAPI xbuffer xrtBrotliCompressorDrainBuffer(xrtbrotlicompressor* pCompressor)
{
	xbuffer pOutput;
	if ( pCompressor == NULL || pCompressor->Failed || pCompressor->Output == NULL ) { return NULL; }
	pOutput = xrtBufferFrom((ptr)pCompressor->Output->Buffer, (size_t)pCompressor->Output->Length);
	pCompressor->Output->Length = 0u;
	return pOutput;
}

XXAPI uint64 xrtBrotliCompressorInputBytes(const xrtbrotlicompressor* pCompressor)
{
	return pCompressor != NULL ? pCompressor->InputBytes : 0u;
}

XXAPI uint64 xrtBrotliCompressorOutputBytes(const xrtbrotlicompressor* pCompressor)
{
	return pCompressor != NULL ? pCompressor->OutputBytes : 0u;
}

XXAPI const xrtbrotlierror* xrtBrotliCompressorError(const xrtbrotlicompressor* pCompressor)
{
	return pCompressor != NULL ? &pCompressor->Error : NULL;
}

XXAPI void xrtBrotliCompressorDestroy(xrtbrotlicompressor* pCompressor)
{
	if ( pCompressor == NULL ) { return; }
	xrtBufferDestroy(pCompressor->Pending);
	xrtBufferDestroy(pCompressor->Output);
	xrtFree(pCompressor);
}



// ------------------------------------ 解压器 ------------------------------------

XXAPI xrtbrotlidecompressor* xrtBrotliDecompressorCreate(const xrtbrotlioptions* pOptions)
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
	pDecompressor->Carry = xrtBufferCreate(XRT_BROTLI_CHUNK);
	pDecompressor->Output = xrtBufferCreate(XRT_BROTLI_CHUNK);
	if ( pDecompressor->Carry == NULL || pDecompressor->Output == NULL ) {
		__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_MEMORY, 0, 0, 0,
			"cannot allocate brotli decompressor");
		pDecompressor->Failed = TRUE;
		return pDecompressor;
	}
	pDecompressor->State = XRT_BROTLI_DEC_HEADER;
	pDecompressor->Initialized = TRUE;
	return pDecompressor;
}

/* 消费 Carry 中的输入；*pConsumed 返回实际消费字节数。 */
static bool __xrtBrotliDecompressConsume(xrtbrotlidecompressor* pDecompressor, size_t* pConsumed)
{
	size_t iPos = 0u;
	const uint8* pBuf = (const uint8*)pDecompressor->Carry->Buffer;
	size_t iLen = pDecompressor->Carry->Length;
	while ( iPos < iLen ) {
		if ( pDecompressor->State == XRT_BROTLI_DEC_HEADER ) {
			uint32 iDeclared;
			if ( iLen - iPos < XRT_BROTLI_HEADER_SIZE ) { break; }
			iDeclared = __xrtBrotliReadU32LE(pBuf + iPos);
			if ( iDeclared == 0u ) {
				iPos += XRT_BROTLI_HEADER_SIZE;
				pDecompressor->MemberEnded = TRUE;
				pDecompressor->MemberCount += 1u;
				continue;   /* 可能紧跟下一个成员 */
			}
			pDecompressor->BlockDeclared = (size_t)iDeclared;
			pDecompressor->BlockCompressed = (size_t)__xrtBrotliReadU32LE(pBuf + iPos + 4u);
			pDecompressor->BlockCrc = __xrtBrotliReadU32LE(pBuf + iPos + 8u);
			iPos += XRT_BROTLI_HEADER_SIZE;
			pDecompressor->State = XRT_BROTLI_DEC_BLOCK;
		} else {
			size_t iWritten;
			ptr pPlain;
			if ( iLen - iPos < pDecompressor->BlockCompressed ) { break; }
			pPlain = xrtMalloc(pDecompressor->BlockDeclared + 1u);
			if ( pPlain == NULL ) {
				__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_MEMORY, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes, "out of memory");
				pDecompressor->Failed = TRUE;
				*pConsumed = iPos;
				return FALSE;
			}
			iWritten = pDecompressor->BlockDeclared;
			if ( BrotliDecoderDecompress(pDecompressor->BlockCompressed, pBuf + iPos,
				&iWritten, (uint8*)pPlain) != BROTLI_DECODER_RESULT_SUCCESS ||
				iWritten != pDecompressor->BlockDeclared ) {
				__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_DATA, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"brotli decompression failed");
				xrtFree(pPlain);
				pDecompressor->Failed = TRUE;
				*pConsumed = iPos;
				return FALSE;
			}
			iPos += pDecompressor->BlockCompressed;
			if ( __xrtBrotliCrc32(pPlain, iWritten) != pDecompressor->BlockCrc ) {
				__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_DATA, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"brotli block checksum mismatch");
				xrtFree(pPlain);
				pDecompressor->Failed = TRUE;
				*pConsumed = iPos;
				return FALSE;
			}
			if ( !xrtBufferAppend(pDecompressor->Output, pPlain, (uint32)iWritten, XBUF_BINARY) ) {
				__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_MEMORY, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"cannot grow brotli output buffer");
				xrtFree(pPlain);
				pDecompressor->Failed = TRUE;
				*pConsumed = iPos;
				return FALSE;
			}
			xrtFree(pPlain);
			pDecompressor->OutputBytes += (uint64)iWritten;
			if ( __xrtBrotliOverOutputLimit(&pDecompressor->Options.tLimits, pDecompressor->OutputBytes) ||
				__xrtBrotliOverRatio(&pDecompressor->Options.tLimits, pDecompressor->InputBytes,
					pDecompressor->OutputBytes) ) {
				__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_LIMIT, 0,
					pDecompressor->InputBytes, pDecompressor->OutputBytes,
					"brotli output exceeds decompression limit");
				pDecompressor->Failed = TRUE;
				*pConsumed = iPos;
				return FALSE;
			}
			pDecompressor->State = XRT_BROTLI_DEC_HEADER;
		}
	}
	*pConsumed = iPos;
	return TRUE;
}

XXAPI bool xrtBrotliDecompressorUpdate(xrtbrotlidecompressor* pDecompressor, const void* pData, size_t iSize)
{
	size_t iConsumed = 0u;
	size_t iRemaining;
	if ( pDecompressor == NULL || pDecompressor->Failed ) { return FALSE; }
	if ( iSize != 0u && pData == NULL ) {
		__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_ARGUMENT, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "invalid brotli input");
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	if ( pDecompressor->Options.tLimits.iMaxInputBytes != 0u &&
		pDecompressor->InputBytes + iSize > pDecompressor->Options.tLimits.iMaxInputBytes ) {
		__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_LIMIT, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "brotli input exceeds maxInput limit");
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	if ( iSize != 0u &&
		!xrtBufferAppend(pDecompressor->Carry, (ptr)pData, (uint32)iSize, XBUF_BINARY) ) {
		__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_MEMORY, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot grow brotli input buffer");
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	pDecompressor->MemberEnded = FALSE;
	pDecompressor->InputBytes += iSize;
	if ( !__xrtBrotliDecompressConsume(pDecompressor, &iConsumed) ) { return FALSE; }
	/* 移除已消费字节。 */
	iRemaining = pDecompressor->Carry->Length - iConsumed;
	if ( iRemaining != 0u && iConsumed != 0u ) {
		memmove(pDecompressor->Carry->Buffer, (uint8*)pDecompressor->Carry->Buffer + iConsumed, iRemaining);
	}
	pDecompressor->Carry->Length = (uint32)iRemaining;
	return TRUE;
}

XXAPI bool xrtBrotliDecompressorFinish(xrtbrotlidecompressor* pDecompressor)
{
	if ( pDecompressor == NULL || pDecompressor->Failed ) { return FALSE; }
	if ( pDecompressor->Finished ) { return TRUE; }
	if ( pDecompressor->MemberCount == 0u || !pDecompressor->MemberEnded ||
		pDecompressor->Carry->Length != 0u ) {
		__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_TRUNCATED, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "brotli stream is truncated or empty");
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	pDecompressor->Finished = TRUE;
	return TRUE;
}

XXAPI bool xrtBrotliDecompressorDrain(xrtbrotlidecompressor* pDecompressor, xwriter pWriter)
{
	if ( pDecompressor == NULL || pDecompressor->Failed || pWriter == NULL ) { return FALSE; }
	if ( pDecompressor->Output->Length != 0u &&
		!xrtWriterWriteAll(pWriter, pDecompressor->Output->Buffer, pDecompressor->Output->Length) ) {
		__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_IO, 0,
			pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot write brotli output stream");
		pDecompressor->Failed = TRUE;
		return FALSE;
	}
	pDecompressor->Output->Length = 0u;
	return TRUE;
}

XXAPI xbuffer xrtBrotliDecompressorTakeBuffer(xrtbrotlidecompressor* pDecompressor)
{
	xbuffer pOutput;
	if ( pDecompressor == NULL || pDecompressor->Failed || !pDecompressor->Finished ) { return NULL; }
	pOutput = pDecompressor->Output;
	pDecompressor->Output = NULL;
	return pOutput;
}

XXAPI xbuffer xrtBrotliDecompressorDrainBuffer(xrtbrotlidecompressor* pDecompressor)
{
	xbuffer pOutput;
	if ( pDecompressor == NULL || pDecompressor->Failed || pDecompressor->Output == NULL ) { return NULL; }
	pOutput = xrtBufferFrom((ptr)pDecompressor->Output->Buffer, (size_t)pDecompressor->Output->Length);
	pDecompressor->Output->Length = 0u;
	return pOutput;
}

XXAPI uint64 xrtBrotliDecompressorInputBytes(const xrtbrotlidecompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->InputBytes : 0u;
}

XXAPI uint64 xrtBrotliDecompressorOutputBytes(const xrtbrotlidecompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->OutputBytes : 0u;
}

XXAPI bool xrtBrotliDecompressorFinished(const xrtbrotlidecompressor* pDecompressor)
{
	return pDecompressor != NULL && pDecompressor->MemberEnded;
}

XXAPI uint64 xrtBrotliDecompressorMembers(const xrtbrotlidecompressor* pDecompressor)
{
	return pDecompressor != NULL ? pDecompressor->MemberCount : 0u;
}

XXAPI const xrtbrotlierror* xrtBrotliDecompressorError(const xrtbrotlidecompressor* pDecompressor)
{
	return pDecompressor != NULL ? &pDecompressor->Error : NULL;
}

XXAPI void xrtBrotliDecompressorDestroy(xrtbrotlidecompressor* pDecompressor)
{
	if ( pDecompressor == NULL ) { return; }
	xrtBufferDestroy(pDecompressor->Carry);
	xrtBufferDestroy(pDecompressor->Output);
	xrtFree(pDecompressor);
}



// ------------------------------------ 一次性接口 ------------------------------------

XXAPI bool xrtBrotliCompress(const void* pData, size_t iSize, const xrtbrotlioptions* pOptions,
	xbuffer* pOutput, xrtbrotlierror* pError)
{
	xrtbrotlioptions tOptions;
	xrtbrotlicompressor* pCompressor;
	bool bRet;
	xrtBrotliErrorInit(pError);
	if ( pOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtBrotliSetError(pError, XRT_BROTLI_ERROR_ARGUMENT, 0, 0, 0, "invalid brotli input");
		return FALSE;
	}
	if ( pOptions != NULL ) { tOptions = *pOptions; } else { xrtBrotliOptionsInit(&tOptions); }
	pCompressor = xrtBrotliCompressorCreate(&tOptions);
	if ( pCompressor == NULL ) {
		__xrtBrotliSetError(pError, XRT_BROTLI_ERROR_MEMORY, 0, iSize, 0, "cannot allocate brotli compressor");
		return FALSE;
	}
	bRet = xrtBrotliCompressorUpdate(pCompressor, pData, iSize) &&
		xrtBrotliCompressorFinish(pCompressor);
	if ( bRet ) { *pOutput = xrtBrotliCompressorTakeBuffer(pCompressor); bRet = *pOutput != NULL; }
	if ( !bRet ) { __xrtBrotliCopyError(pError, &pCompressor->Error); }
	xrtBrotliCompressorDestroy(pCompressor);
	return bRet;
}

XXAPI bool xrtBrotliDecompress(const void* pData, size_t iSize, const xrtbrotlioptions* pOptions,
	xbuffer* pOutput, xrtbrotlierror* pError)
{
	xrtbrotlioptions tOptions;
	xrtbrotlidecompressor* pDecompressor;
	bool bRet;
	xrtBrotliErrorInit(pError);
	if ( pOutput == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtBrotliSetError(pError, XRT_BROTLI_ERROR_ARGUMENT, 0, 0, 0, "invalid brotli input");
		return FALSE;
	}
	if ( pOptions != NULL ) { tOptions = *pOptions; } else { xrtBrotliOptionsInit(&tOptions); }
	pDecompressor = xrtBrotliDecompressorCreate(&tOptions);
	if ( pDecompressor == NULL ) {
		__xrtBrotliSetError(pError, XRT_BROTLI_ERROR_MEMORY, 0, iSize, 0, "cannot allocate brotli decompressor");
		return FALSE;
	}
	bRet = xrtBrotliDecompressorUpdate(pDecompressor, pData, iSize) &&
		xrtBrotliDecompressorFinish(pDecompressor);
	if ( bRet ) { *pOutput = xrtBrotliDecompressorTakeBuffer(pDecompressor); bRet = *pOutput != NULL; }
	if ( !bRet ) { __xrtBrotliCopyError(pError, &pDecompressor->Error); }
	xrtBrotliDecompressorDestroy(pDecompressor);
	return bRet;
}



// ------------------------------------ 文件流接口 ------------------------------------

XXAPI bool xrtBrotliCompressStream(xreader pReader, xwriter pWriter, const xrtbrotlioptions* pOptions,
	xrtbrotlierror* pError)
{
	uint8 sInput[XRT_BROTLI_CHUNK];
	xrtbrotlicompressor* pCompressor;
	bool bRet = TRUE;
	xrtBrotliErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		__xrtBrotliSetError(pError, XRT_BROTLI_ERROR_ARGUMENT, 0, 0, 0, "invalid brotli stream");
		return FALSE;
	}
	pCompressor = xrtBrotliCompressorCreate(pOptions);
	if ( pCompressor == NULL ) { return FALSE; }
	while ( bRet ) {
		uint64 iRead = 0u;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtBrotliSetError(&pCompressor->Error, XRT_BROTLI_ERROR_IO, 0,
				pCompressor->InputBytes, pCompressor->OutputBytes, "cannot read brotli input stream");
			bRet = FALSE;
			break;
		}
		if ( iRead == 0u ) { break; }
		bRet = xrtBrotliCompressorUpdate(pCompressor, sInput, (size_t)iRead) &&
			xrtBrotliCompressorDrain(pCompressor, pWriter);
	}
	if ( bRet ) { bRet = xrtBrotliCompressorFinish(pCompressor) && xrtBrotliCompressorDrain(pCompressor, pWriter); }
	if ( !bRet ) { __xrtBrotliCopyError(pError, &pCompressor->Error); }
	xrtBrotliCompressorDestroy(pCompressor);
	return bRet;
}

XXAPI bool xrtBrotliDecompressStream(xreader pReader, xwriter pWriter, const xrtbrotlioptions* pOptions,
	xrtbrotlierror* pError)
{
	uint8 sInput[XRT_BROTLI_CHUNK];
	xrtbrotlidecompressor* pDecompressor;
	bool bRet = TRUE;
	xrtBrotliErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		__xrtBrotliSetError(pError, XRT_BROTLI_ERROR_ARGUMENT, 0, 0, 0, "invalid brotli stream");
		return FALSE;
	}
	pDecompressor = xrtBrotliDecompressorCreate(pOptions);
	if ( pDecompressor == NULL ) { return FALSE; }
	while ( bRet ) {
		uint64 iRead = 0u;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtBrotliSetError(&pDecompressor->Error, XRT_BROTLI_ERROR_IO, 0,
				pDecompressor->InputBytes, pDecompressor->OutputBytes, "cannot read brotli input stream");
			bRet = FALSE;
			break;
		}
		if ( iRead == 0u ) { break; }
		bRet = xrtBrotliDecompressorUpdate(pDecompressor, sInput, (size_t)iRead) &&
			xrtBrotliDecompressorDrain(pDecompressor, pWriter);
	}
	if ( bRet ) { bRet = xrtBrotliDecompressorFinish(pDecompressor) && xrtBrotliDecompressorDrain(pDecompressor, pWriter); }
	if ( !bRet ) { __xrtBrotliCopyError(pError, &pDecompressor->Error); }
	xrtBrotliDecompressorDestroy(pDecompressor);
	return bRet;
}
