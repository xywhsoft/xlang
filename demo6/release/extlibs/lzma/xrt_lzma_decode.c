#include "xrt_lzma_internal.h"
#include <string.h>


// ------------------------------------ 增量解码状态 ------------------------------------

struct xrtlzmadecompressor {
	xbuffer* Carry;
	xbuffer* Output;
	xrtlzmaoptions Options;
	xrtlzmaerror Error;
	CXzUnpacker Decoder;
	struct {
		ISzAlloc VTable;
		uint64 MaxItemBytes;
	} Allocator;
	uint64 InputBytes;
	uint64 OutputBytes;
	bool Finished;
	bool Failed;
	bool LimitExceeded;
};

static void* xlzmaDecodeAlloc(ISzAllocPtr pAllocator, size_t iSize)
{
	xrtlzmadecompressor* pDecoder = (xrtlzmadecompressor*)((uint8*)pAllocator -
		offsetof(xrtlzmadecompressor, Allocator));
	if ( pDecoder->Allocator.MaxItemBytes != 0u &&
		(uint64)iSize > pDecoder->Allocator.MaxItemBytes ) {
		pDecoder->LimitExceeded = true;
		return NULL;
	}
	return iSize != 0u ? xrtMalloc(iSize) : NULL;
}

static void xlzmaDecodeFree(ISzAllocPtr pAllocator, void* pData)
{
	(void)pAllocator;
	xrtFree(pData);
}

static bool xlzmaConsume(xrtlzmadecompressor* pDecoder, bool bFinal,
	size_t* pConsumed)
{
	xbytesview Carry = xrtBufferView(pDecoder->Carry);
	size_t iPos = 0u;
	bool bMoreOutput = false;
	while ( iPos < Carry.Size || bMoreOutput || bFinal ) {
		uint8 Output[XRT_LZMA_CHUNK_SIZE];
		SizeT iSource = Carry.Size - iPos;
		SizeT iTarget = sizeof(Output);
		ECoderStatus Status = CODER_STATUS_NOT_SPECIFIED;
		SRes iResult;
		iResult = XzUnpacker_Code(&pDecoder->Decoder, Output, &iTarget,
			Carry.Data != NULL ? Carry.Data + iPos : (const uint8*)"",
			&iSource, bFinal, CODER_FINISH_ANY, &Status);
		if ( iResult != SZ_OK ) {
			xlzmaSetError(&pDecoder->Error,
				pDecoder->LimitExceeded ? XRT_LZMA_ERROR_LIMIT :
				(iResult == SZ_ERROR_MEM ? XRT_LZMA_ERROR_MEMORY : XRT_LZMA_ERROR_DATA),
				iResult,
				pDecoder->InputBytes, pDecoder->OutputBytes,
				xlzmaStatusName(iResult));
			goto fail;
		}
		iPos += (size_t)iSource;
		if ( xlzmaAddOverLimit(pDecoder->OutputBytes, (size_t)iTarget,
			pDecoder->Options.tLimits.iMaxOutputBytes) ||
			xlzmaOverRatio(&pDecoder->Options.tLimits,
				pDecoder->InputBytes, pDecoder->OutputBytes + (uint64)iTarget) ) {
			xlzmaSetError(&pDecoder->Error, XRT_LZMA_ERROR_LIMIT, 0,
				pDecoder->InputBytes, pDecoder->OutputBytes,
				"lzma output exceeds decompression limit");
			goto fail;
		}
		if ( iTarget != 0u && !xrtBufferAppend(pDecoder->Output,
			(xbytesview){ Output, (size_t)iTarget }) ) {
			xlzmaSetError(&pDecoder->Error, XRT_LZMA_ERROR_MEMORY, 0,
				pDecoder->InputBytes, pDecoder->OutputBytes,
				"cannot grow lzma output buffer");
			goto fail;
		}
		pDecoder->OutputBytes += (uint64)iTarget;
		bMoreOutput = iTarget == sizeof(Output);
		if ( Status == CODER_STATUS_NEEDS_MORE_INPUT &&
			iPos == Carry.Size ) { break; }
		if ( iSource == 0u && iTarget == 0u ) { break; }
	}
	*pConsumed = iPos;
	return true;
fail:
	pDecoder->Failed = true;
	*pConsumed = iPos;
	return false;
}


// ------------------------------------ 解压器公开接口 ------------------------------------

XRT_API xrtlzmadecompressor* xrtLzmaDecompressorCreate(
	const xrtlzmaoptions* pOptions)
{
	xrtlzmadecompressor* pDecoder = xrtCalloc(1u, sizeof(*pDecoder));
	if ( pDecoder == NULL ) { return NULL; }
	if ( !xlzmaPrepareOptions(pOptions, &pDecoder->Options,
		&pDecoder->Error) || !xlzmaInitXzTables() ) {
		pDecoder->Failed = true;
		return pDecoder;
	}
	pDecoder->Allocator.VTable.Alloc = xlzmaDecodeAlloc;
	pDecoder->Allocator.VTable.Free = xlzmaDecodeFree;
	pDecoder->Allocator.MaxItemBytes = pDecoder->Options.tLimits.iMaxItemBytes;
	XzUnpacker_Construct(&pDecoder->Decoder, &pDecoder->Allocator.VTable);
	pDecoder->Carry = xlzmaBufferCreate();
	pDecoder->Output = xlzmaBufferCreate();
	if ( pDecoder->Carry == NULL || pDecoder->Output == NULL ) {
		xlzmaSetError(&pDecoder->Error, XRT_LZMA_ERROR_MEMORY, 0,
			0, 0, "cannot allocate lzma decompressor buffers");
		pDecoder->Failed = true;
	}
	return pDecoder;
}

XRT_API bool xrtLzmaDecompressorUpdate(xrtlzmadecompressor* pDecoder,
	const void* pData, size_t iSize)
{
	size_t iConsumed = 0u;
	if ( pDecoder == NULL || pDecoder->Failed ) { return false; }
	if ( pDecoder->Finished || (pData == NULL && iSize != 0u) ) {
		xlzmaSetError(&pDecoder->Error, XRT_LZMA_ERROR_STATE, 0,
			pDecoder->InputBytes, pDecoder->OutputBytes,
			"invalid lzma decompressor update");
		pDecoder->Failed = true;
		return false;
	}
	if ( xlzmaAddOverLimit(pDecoder->InputBytes, iSize,
		pDecoder->Options.tLimits.iMaxInputBytes) ) {
		xlzmaSetError(&pDecoder->Error, XRT_LZMA_ERROR_LIMIT, 0,
			pDecoder->InputBytes, pDecoder->OutputBytes,
			"lzma input exceeds maxInput limit");
		pDecoder->Failed = true;
		return false;
	}
	if ( iSize != 0u && !xrtBufferAppend(pDecoder->Carry,
		(xbytesview){ (const uint8*)pData, iSize }) ) {
		xlzmaSetError(&pDecoder->Error, XRT_LZMA_ERROR_MEMORY, 0,
			pDecoder->InputBytes, pDecoder->OutputBytes,
			"cannot grow lzma input buffer");
		pDecoder->Failed = true;
		return false;
	}
	pDecoder->InputBytes += iSize;
	if ( !xlzmaConsume(pDecoder, false, &iConsumed) ) { return false; }
	if ( iConsumed != 0u && !xrtBufferRemove(pDecoder->Carry, 0u, iConsumed) ) {
		xlzmaSetError(&pDecoder->Error, XRT_LZMA_ERROR_STATE, 0,
			pDecoder->InputBytes, pDecoder->OutputBytes,
			"cannot consume lzma input buffer");
		pDecoder->Failed = true;
		return false;
	}
	return true;
}

XRT_API bool xrtLzmaDecompressorFinish(xrtlzmadecompressor* pDecoder)
{
	size_t iConsumed = 0u;
	if ( pDecoder == NULL || pDecoder->Failed ) { return false; }
	if ( pDecoder->Finished ) { return true; }
	if ( !xlzmaConsume(pDecoder, true, &iConsumed) ) { return false; }
	if ( pDecoder->Decoder.numFinishedStreams == 0u ||
		!XzUnpacker_IsStreamWasFinished(&pDecoder->Decoder) ||
		iConsumed != xrtBufferView(pDecoder->Carry).Size ) {
		xlzmaSetError(&pDecoder->Error, XRT_LZMA_ERROR_TRUNCATED, 0,
			pDecoder->InputBytes, pDecoder->OutputBytes,
			"xz stream is truncated or empty");
		pDecoder->Failed = true;
		return false;
	}
	pDecoder->Finished = true;
	return true;
}

XRT_API bool xrtLzmaDecompressorDrain(xrtlzmadecompressor* pDecoder,
	xwriter* pWriter)
{
	xbytesview Output;
	if ( pDecoder == NULL || pDecoder->Failed || pWriter == NULL ) { return false; }
	Output = xrtBufferView(pDecoder->Output);
	if ( Output.Size != 0u && !xrtWriterWriteFull(pWriter,
		Output.Data, Output.Size, NULL) ) {
		xlzmaSetError(&pDecoder->Error, xlzmaStreamErrorCode(), 0,
			pDecoder->InputBytes, pDecoder->OutputBytes,
			"cannot write lzma output stream");
		pDecoder->Failed = true;
		return false;
	}
	xrtBufferClear(pDecoder->Output);
	return true;
}

XRT_API xbuffer* xrtLzmaDecompressorTakeBuffer(xrtlzmadecompressor* pDecoder)
{
	xbuffer* pOutput;
	if ( pDecoder == NULL || pDecoder->Failed || !pDecoder->Finished ) {
		return NULL;
	}
	pOutput = pDecoder->Output;
	pDecoder->Output = NULL;
	return pOutput;
}

XRT_API xbuffer* xrtLzmaDecompressorDrainBuffer(xrtlzmadecompressor* pDecoder)
{
	xbuffer* pOutput;
	if ( pDecoder == NULL || pDecoder->Failed || pDecoder->Output == NULL ) {
		return NULL;
	}
	pOutput = xrtBufferFrom(xrtBufferView(pDecoder->Output));
	if ( pOutput != NULL ) { xrtBufferClear(pDecoder->Output); }
	return pOutput;
}

XRT_API uint64 xrtLzmaDecompressorInputBytes(const xrtlzmadecompressor* pDecoder)
{
	return pDecoder != NULL ? pDecoder->InputBytes : 0u;
}

XRT_API uint64 xrtLzmaDecompressorOutputBytes(const xrtlzmadecompressor* pDecoder)
{
	return pDecoder != NULL ? pDecoder->OutputBytes : 0u;
}

XRT_API bool xrtLzmaDecompressorFinished(const xrtlzmadecompressor* pDecoder)
{
	return pDecoder != NULL &&
		XzUnpacker_IsStreamWasFinished(&pDecoder->Decoder);
}

XRT_API uint64 xrtLzmaDecompressorMembers(const xrtlzmadecompressor* pDecoder)
{
	return pDecoder != NULL ? pDecoder->Decoder.numFinishedStreams : 0u;
}

XRT_API const xrtlzmaerror* xrtLzmaDecompressorError(
	const xrtlzmadecompressor* pDecoder)
{
	return pDecoder != NULL ? &pDecoder->Error : NULL;
}

XRT_API void xrtLzmaDecompressorDestroy(xrtlzmadecompressor* pDecoder)
{
	if ( pDecoder == NULL ) { return; }
	if ( pDecoder->Allocator.VTable.Alloc != NULL ) {
		XzUnpacker_Free(&pDecoder->Decoder);
	}
	xrtBufferDestroy(pDecoder->Carry);
	xrtBufferDestroy(pDecoder->Output);
	xrtFree(pDecoder);
}


// ------------------------------------ 文件流解压 ------------------------------------

XRT_API bool xrtLzmaDecompressStream(xreader* pReader, xwriter* pWriter,
	const xrtlzmaoptions* pOptions, xrtprogressproc pProgress,
	ptr pUserData, xrtlzmaerror* pError)
{
	xrtlzmaerror LocalError;
	xrtlzmadecompressor* pDecoder;
	uint8 Input[XRT_LZMA_CHUNK_SIZE];
	uint64 iSize = 0u;
	uint64 iPosition = 0u;
	uint64 iTotal = 0u;
	bool bKnown;
	bool bOk = true;
	if ( pError == NULL ) { pError = &LocalError; }
	xrtLzmaErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		xlzmaSetError(pError, XRT_LZMA_ERROR_ARGUMENT, 0,
			0, 0, "invalid lzma stream");
		return false;
	}
	bKnown = xrtReaderCanSeek(pReader) &&
		xrtReaderTell(pReader, &iPosition) &&
		xrtReaderSize(pReader, &iSize) && iSize >= iPosition;
	if ( !bKnown ) { xrtClearError(); }
	iTotal = bKnown ? iSize - iPosition : 0u;
	pDecoder = xrtLzmaDecompressorCreate(pOptions);
	if ( pDecoder == NULL ) {
		xlzmaSetError(pError, XRT_LZMA_ERROR_MEMORY, 0,
			0, 0, "cannot allocate lzma decompressor");
		return false;
	}
	if ( pDecoder->Failed ) { bOk = false; }
	while ( bOk ) {
		size_t iRead = 0u;
		if ( !xrtReaderRead(pReader, Input, sizeof(Input), &iRead) ) {
			xlzmaSetError(&pDecoder->Error, xlzmaStreamErrorCode(), 0,
				pDecoder->InputBytes, pDecoder->OutputBytes,
				"cannot read lzma input stream");
			bOk = false;
			break;
		}
		if ( iRead == 0u ) { break; }
		bOk = xrtLzmaDecompressorUpdate(pDecoder, Input, iRead) &&
			xrtLzmaDecompressorDrain(pDecoder, pWriter);
		if ( bOk && !xrtProgressReport(pProgress, pUserData,
			pDecoder->InputBytes, iTotal, pDecoder->OutputBytes,
			bKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u) ) {
			xlzmaSetError(&pDecoder->Error, XRT_LZMA_ERROR_CANCELLED, 0,
				pDecoder->InputBytes, pDecoder->OutputBytes,
				"lzma operation cancelled");
			bOk = false;
		}
	}
	if ( bOk ) { bOk = xrtLzmaDecompressorFinish(pDecoder) &&
		xrtLzmaDecompressorDrain(pDecoder, pWriter); }
	if ( bOk && !xrtProgressReport(pProgress, pUserData,
		pDecoder->InputBytes, iTotal, pDecoder->OutputBytes,
		XRT_PROGRESS_FINAL | (bKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u)) ) {
		xlzmaSetError(&pDecoder->Error, XRT_LZMA_ERROR_CANCELLED, 0,
			pDecoder->InputBytes, pDecoder->OutputBytes,
			"lzma operation cancelled");
		bOk = false;
	}
	if ( !bOk ) { *pError = pDecoder->Error; }
	xrtLzmaDecompressorDestroy(pDecoder);
	return bOk;
}
