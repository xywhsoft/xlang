#include "xrt_lzma_internal.h"


// ------------------------------------ 一次性 Buffer 接口 ------------------------------------

static bool xlzmaTransform(const void* pData, size_t iSize,
	const xrtlzmaoptions* pOptions, xbuffer** ppOutput,
	xrtlzmaerror* pError, bool bEncode)
{
	xrtlzmaerror LocalError;
	xreader* pReader = NULL;
	xwriter* pWriter = NULL;
	xbuffer* pOutput = NULL;
	bool bOk = false;
	if ( pError == NULL ) { pError = &LocalError; }
	xrtLzmaErrorInit(pError);
	if ( ppOutput != NULL ) { *ppOutput = NULL; }
	if ( ppOutput == NULL || (pData == NULL && iSize != 0u) ) {
		xlzmaSetError(pError, XRT_LZMA_ERROR_ARGUMENT, 0,
			0, 0, "invalid lzma input");
		return false;
	}
	pReader = xrtReaderFromMemory((xbytesview){ (const uint8*)pData, iSize });
	pOutput = xlzmaBufferCreate();
	pWriter = pOutput != NULL ? xrtWriterFromBuffer(pOutput) : NULL;
	if ( pReader == NULL || pWriter == NULL ) {
		xlzmaSetError(pError, XRT_LZMA_ERROR_MEMORY, 0,
			0, 0, "cannot allocate lzma memory adapters");
		goto cleanup;
	}
	bOk = bEncode ? xrtLzmaCompressStream(pReader, pWriter,
		pOptions, NULL, NULL, pError) : xrtLzmaDecompressStream(
		pReader, pWriter, pOptions, NULL, NULL, pError);
	if ( bOk ) { *ppOutput = pOutput; pOutput = NULL; }
cleanup:
	xrtReaderDestroy(pReader);
	xrtWriterDestroy(pWriter);
	xrtBufferDestroy(pOutput);
	return bOk;
}

XRT_API bool xrtLzmaCompress(const void* pData, size_t iSize,
	const xrtlzmaoptions* pOptions, xbuffer** ppOutput,
	xrtlzmaerror* pError)
{
	return xlzmaTransform(pData, iSize, pOptions, ppOutput, pError, true);
}

XRT_API bool xrtLzmaDecompress(const void* pData, size_t iSize,
	const xrtlzmaoptions* pOptions, xbuffer** ppOutput,
	xrtlzmaerror* pError)
{
	return xlzmaTransform(pData, iSize, pOptions, ppOutput, pError, false);
}
