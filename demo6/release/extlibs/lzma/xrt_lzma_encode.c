#include "xrt_lzma_internal.h"
#include <string.h>


// ------------------------------------ 标准 .xz 连续流 ------------------------------------

typedef struct {
	ISeqInStream VTable;
	xreader* Reader;
	xrtlzmaerror* Error;
	const xrtresourcelimits* Limits;
	uint64 InputBytes;
} xlzma_input;

typedef struct {
	ISeqOutStream VTable;
	xwriter* Writer;
	xrtlzmaerror* Error;
	const xrtresourcelimits* Limits;
	uint64 OutputBytes;
} xlzma_output;

typedef struct {
	ICompressProgress VTable;
	xlzma_input* Input;
	xlzma_output* Output;
	xrtprogressproc Callback;
	ptr UserData;
	uint64 TotalBytes;
	bool TotalKnown;
} xlzma_progress;

static SRes xlzmaRead(ISeqInStreamPtr pStream, void* pData, size_t* pSize)
{
	xlzma_input* pInput = (xlzma_input*)pStream;
	size_t iRequest = *pSize;
	size_t iRead = 0u;
	uint64 iLimit = pInput->Limits->iMaxInputBytes;
	if ( iLimit != 0u && pInput->InputBytes <= iLimit &&
		(uint64)iRequest > iLimit - pInput->InputBytes ) {
		iRequest = (size_t)(iLimit - pInput->InputBytes + 1u);
	}
	if ( !xrtReaderRead(pInput->Reader, pData, iRequest, &iRead) ) {
		xlzmaSetError(pInput->Error, xlzmaStreamErrorCode(), SZ_ERROR_READ,
			pInput->InputBytes, 0u, "cannot read lzma input stream");
		return SZ_ERROR_READ;
	}
	if ( xlzmaAddOverLimit(pInput->InputBytes, iRead, iLimit) ) {
		xlzmaSetError(pInput->Error, XRT_LZMA_ERROR_LIMIT, 0,
			pInput->InputBytes, 0u, "lzma input exceeds maxInput limit");
		return SZ_ERROR_READ;
	}
	pInput->InputBytes += iRead;
	*pSize = iRead;
	return SZ_OK;
}

static size_t xlzmaWrite(ISeqOutStreamPtr pStream, const void* pData, size_t iSize)
{
	xlzma_output* pOutput = (xlzma_output*)pStream;
	if ( xlzmaAddOverLimit(pOutput->OutputBytes, iSize,
		pOutput->Limits->iMaxOutputBytes) ) {
		xlzmaSetError(pOutput->Error, XRT_LZMA_ERROR_LIMIT, 0,
			0u, pOutput->OutputBytes, "lzma output exceeds maxOutput limit");
		return 0u;
	}
	if ( !xrtWriterWriteFull(pOutput->Writer, pData, iSize, NULL) ) {
		xlzmaSetError(pOutput->Error, xlzmaStreamErrorCode(), SZ_ERROR_WRITE,
			0u, pOutput->OutputBytes, "cannot write lzma output stream");
		return 0u;
	}
	pOutput->OutputBytes += iSize;
	return iSize;
}

static SRes xlzmaReport(ICompressProgressPtr pState,
	UInt64 iInput, UInt64 iOutput)
{
	xlzma_progress* pProgress = (xlzma_progress*)pState;
	(void)iInput;
	(void)iOutput;
	if ( !xrtProgressReport(pProgress->Callback, pProgress->UserData,
		pProgress->Input->InputBytes, pProgress->TotalBytes,
		pProgress->Output->OutputBytes,
		pProgress->TotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u) ) {
		xlzmaSetError(pProgress->Input->Error, XRT_LZMA_ERROR_CANCELLED,
			SZ_ERROR_PROGRESS, pProgress->Input->InputBytes,
			pProgress->Output->OutputBytes, "lzma operation cancelled");
		return SZ_ERROR_PROGRESS;
	}
	return SZ_OK;
}

XRT_API bool xrtLzmaCompressStream(xreader* pReader, xwriter* pWriter,
	const xrtlzmaoptions* pOptions, xrtprogressproc pProgress,
	ptr pUserData, xrtlzmaerror* pError)
{
	xrtlzmaerror LocalError;
	xrtlzmaoptions Options;
	xlzma_input Input;
	xlzma_output Output;
	xlzma_progress Progress;
	CXzProps Props;
	CXzEncHandle Encoder = NULL;
	uint64 iPosition = 0u;
	uint64 iSize = 0u;
	SRes iResult = SZ_OK;
	bool bOk = false;
	if ( pError == NULL ) { pError = &LocalError; }
	xrtLzmaErrorInit(pError);
	if ( pReader == NULL || pWriter == NULL ) {
		xlzmaSetError(pError, XRT_LZMA_ERROR_ARGUMENT, 0, 0, 0,
			"invalid lzma stream");
		return false;
	}
	if ( !xlzmaPrepareOptions(pOptions, &Options, pError) ||
		!xlzmaInitXzTables() ) { return false; }
	memset(&Input, 0, sizeof(Input));
	memset(&Output, 0, sizeof(Output));
	memset(&Progress, 0, sizeof(Progress));
	Input.VTable.Read = xlzmaRead;
	Input.Reader = pReader;
	Input.Error = pError;
	Input.Limits = &Options.tLimits;
	Output.VTable.Write = xlzmaWrite;
	Output.Writer = pWriter;
	Output.Error = pError;
	Output.Limits = &Options.tLimits;
	Progress.VTable.Progress = xlzmaReport;
	Progress.Input = &Input;
	Progress.Output = &Output;
	Progress.Callback = pProgress;
	Progress.UserData = pUserData;
	Progress.TotalKnown = xrtReaderCanSeek(pReader) &&
		xrtReaderTell(pReader, &iPosition) &&
		xrtReaderSize(pReader, &iSize) && iSize >= iPosition;
	Progress.TotalBytes = Progress.TotalKnown ? iSize - iPosition : 0u;
	if ( !Progress.TotalKnown ) { xrtClearError(); }
	Encoder = XzEnc_Create(&xlzmaAllocator, &xlzmaAllocator);
	if ( Encoder == NULL ) {
		xlzmaSetError(pError, XRT_LZMA_ERROR_MEMORY, SZ_ERROR_MEM, 0, 0,
			"cannot allocate xz encoder");
		goto cleanup;
	}
	XzProps_Init(&Props);
	Props.lzma2Props.lzmaProps.level = Options.iLevel;
	Props.lzma2Props.lzmaProps.numThreads = 1;
	Props.lzma2Props.numTotalThreads = 1;
	Props.numTotalThreads = 1;
	Props.checkId = XZ_CHECK_CRC64;
	if ( Progress.TotalKnown ) {
		Props.reduceSize = Progress.TotalBytes;
		XzEnc_SetDataSize(Encoder, Progress.TotalBytes);
	}
	iResult = XzEnc_SetProps(Encoder, &Props);
	if ( iResult != SZ_OK ) { goto cleanup; }
	iResult = XzEnc_Encode(Encoder, &Output.VTable, &Input.VTable,
		pProgress != NULL ? &Progress.VTable : NULL);
	if ( iResult != SZ_OK ) { goto cleanup; }
	if ( !xrtProgressReport(pProgress, pUserData, Input.InputBytes,
		Progress.TotalBytes, Output.OutputBytes, XRT_PROGRESS_FINAL |
		(Progress.TotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u)) ) {
		xlzmaSetError(pError, XRT_LZMA_ERROR_CANCELLED, 0,
			Input.InputBytes, Output.OutputBytes, "lzma operation cancelled");
		goto cleanup;
	}
	bOk = true;
cleanup:
	if ( !bOk && pError->iCode == XRT_LZMA_OK ) {
		xlzmaSetError(pError, iResult == SZ_ERROR_MEM ? XRT_LZMA_ERROR_MEMORY :
			XRT_LZMA_ERROR_DATA, iResult, Input.InputBytes,
			Output.OutputBytes, xlzmaStatusName(iResult));
	}
	if ( Encoder != NULL ) {
		XzEnc_Destroy(Encoder);
	}
	return bOk;
}


// ------------------------------------ 缓冲式增量对象 ------------------------------------

struct xrtlzmacompressor {
	xbuffer* Pending;
	xbuffer* Output;
	xrtlzmaoptions Options;
	xrtlzmaerror Error;
	uint64 InputBytes;
	uint64 OutputBytes;
	bool Finished;
	bool Failed;
};

XRT_API xrtlzmacompressor* xrtLzmaCompressorCreate(
	const xrtlzmaoptions* pOptions)
{
	xrtlzmacompressor* pCompressor = xrtCalloc(1u, sizeof(*pCompressor));
	if ( pCompressor == NULL ) { return NULL; }
	if ( !xlzmaPrepareOptions(pOptions, &pCompressor->Options,
		&pCompressor->Error) ) {
		pCompressor->Failed = true;
		return pCompressor;
	}
	pCompressor->Pending = xlzmaBufferCreate();
	pCompressor->Output = xlzmaBufferCreate();
	if ( pCompressor->Pending == NULL || pCompressor->Output == NULL ) {
		xlzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_MEMORY, 0,
			0, 0, "cannot allocate lzma compressor buffers");
		pCompressor->Failed = true;
	}
	return pCompressor;
}

XRT_API bool xrtLzmaCompressorUpdate(xrtlzmacompressor* pCompressor,
	const void* pData, size_t iSize)
{
	if ( pCompressor == NULL || pCompressor->Failed ) { return false; }
	if ( pCompressor->Finished || (pData == NULL && iSize != 0u) ) {
		xlzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_STATE, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes,
			"invalid lzma compressor update");
		pCompressor->Failed = true;
		return false;
	}
	if ( xlzmaAddOverLimit(pCompressor->InputBytes, iSize,
		pCompressor->Options.tLimits.iMaxInputBytes) ) {
		xlzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_LIMIT, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes,
			"lzma input exceeds maxInput limit");
		pCompressor->Failed = true;
		return false;
	}
	if ( iSize != 0u && !xrtBufferAppend(pCompressor->Pending,
		(xbytesview){ (const unsigned char*)pData, iSize }) ) {
		xlzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_MEMORY, 0,
			pCompressor->InputBytes, pCompressor->OutputBytes,
			"cannot grow lzma input buffer");
		pCompressor->Failed = true;
		return false;
	}
	pCompressor->InputBytes += iSize;
	return true;
}

XRT_API bool xrtLzmaCompressorFinish(xrtlzmacompressor* pCompressor)
{
	xreader* pReader;
	xwriter* pWriter;
	xbytesview Input;
	bool bOk;
	if ( pCompressor == NULL || pCompressor->Failed ) { return false; }
	if ( pCompressor->Finished ) { return true; }
	Input = xrtBufferView(pCompressor->Pending);
	pReader = xrtReaderFromMemory(Input);
	pWriter = xrtWriterFromBuffer(pCompressor->Output);
	if ( pReader == NULL || pWriter == NULL ) {
		xlzmaSetError(&pCompressor->Error, XRT_LZMA_ERROR_MEMORY, 0,
			pCompressor->InputBytes, 0, "cannot allocate lzma stream adapters");
		xrtReaderDestroy(pReader);
		xrtWriterDestroy(pWriter);
		pCompressor->Failed = true;
		return false;
	}
	bOk = xrtLzmaCompressStream(pReader, pWriter,
		&pCompressor->Options, NULL, NULL, &pCompressor->Error);
	xrtReaderDestroy(pReader);
	xrtWriterDestroy(pWriter);
	pCompressor->OutputBytes = xrtBufferView(pCompressor->Output).Size;
	pCompressor->Finished = bOk;
	pCompressor->Failed = !bOk;
	return bOk;
}

XRT_API bool xrtLzmaCompressorDrain(xrtlzmacompressor* pCompressor,
	xwriter* pWriter)
{
	xbytesview Output;
	if ( pCompressor == NULL || pCompressor->Failed || pWriter == NULL ) {
		return false;
	}
	Output = xrtBufferView(pCompressor->Output);
	if ( Output.Size != 0u && !xrtWriterWriteFull(pWriter,
		Output.Data, Output.Size, NULL) ) {
		xlzmaSetError(&pCompressor->Error, xlzmaStreamErrorCode(), 0,
			pCompressor->InputBytes, pCompressor->OutputBytes,
			"cannot write lzma output stream");
		pCompressor->Failed = true;
		return false;
	}
	xrtBufferClear(pCompressor->Output);
	return true;
}

XRT_API xbuffer* xrtLzmaCompressorTakeBuffer(xrtlzmacompressor* pCompressor)
{
	xbuffer* pOutput;
	if ( pCompressor == NULL || pCompressor->Failed ||
		!pCompressor->Finished ) { return NULL; }
	pOutput = pCompressor->Output;
	pCompressor->Output = NULL;
	return pOutput;
}

XRT_API xbuffer* xrtLzmaCompressorDrainBuffer(xrtlzmacompressor* pCompressor)
{
	xbuffer* pOutput;
	if ( pCompressor == NULL || pCompressor->Failed ||
		pCompressor->Output == NULL ) { return NULL; }
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

XRT_API const xrtlzmaerror* xrtLzmaCompressorError(
	const xrtlzmacompressor* pCompressor)
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
