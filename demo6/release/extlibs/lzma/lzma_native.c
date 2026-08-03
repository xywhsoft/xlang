#include "include/xrt_lzma.h"

#if defined(_WIN32) || defined(_WIN64)
#define XLZMA_EXPORT __declspec(dllexport)
#else
#define XLZMA_EXPORT
#endif



static void xlzma_publish_error(const xrtlzmaerror* pError)
{
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "lzma operation failed";
	xrtSetError(xrtFormat("%s", sMessage), TRUE);
}

static void xlzma_options(xrtlzmaoptions* pOptions, int iLevel, uint64 iMaxInput,
	uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtLzmaOptionsInit(pOptions);
	if ( iLevel >= 0 ) { pOptions->iLevel = iLevel; }
	if ( iMaxInput != 0u ) { pOptions->tLimits.iMaxInputBytes = iMaxInput; }
	if ( iMaxOutput != 0u ) { pOptions->tLimits.iMaxOutputBytes = iMaxOutput; }
	if ( iMaxRatio != 0u ) { pOptions->tLimits.iMaxCompressionRatio = iMaxRatio; }
}

static bool xlzma_transform_file(str sSource, str sDestination, bool bCompress,
	bool bOverwrite, const xrtlzmaoptions* pOptions)
{
	xrtlzmaerror tError;
	xreader pReader = NULL;
	xwriter pWriter = NULL;
	str sTemp = NULL;
	bool bResult = FALSE;
	if ( sSource == NULL || sSource[0] == 0 || sDestination == NULL || sDestination[0] == 0 ) {
		xrtSetError(xrtFormat("lzma source and destination paths are required"), TRUE);
		return FALSE;
	}
	if ( xrtPathExists(sDestination) && !bOverwrite ) {
		xrtSetError(xrtFormat("lzma destination already exists: %s", sDestination), TRUE);
		return FALSE;
	}
	sTemp = xrtPathRandom(sDestination, 0u, (str)".xrt-lzma-tmp", 0u, 8u);
	pReader = xrtReaderOpen(sSource);
	pWriter = sTemp != NULL ? xrtWriterOpen(sTemp, TRUE) : NULL;
	if ( pReader == NULL || pWriter == NULL ) {
		xrtSetError(xrtFormat("cannot open lzma file stream: %s", sSource), TRUE);
		goto cleanup;
	}
	bResult = bCompress ? xrtLzmaCompressStream(pReader, pWriter, pOptions, &tError) :
		xrtLzmaDecompressStream(pReader, pWriter, pOptions, &tError);
	if ( !bResult ) { xlzma_publish_error(&tError); }
	if ( bResult && !xrtWriterFlush(pWriter) ) {
		xrtSetError(xrtFormat("cannot flush lzma destination: %s", sDestination), TRUE);
		bResult = FALSE;
	}

cleanup:
	xrtReaderDestroy(pReader);
	xrtWriterDestroy(pWriter);
	if ( bResult && !xrtFileRename(sTemp, sDestination, bOverwrite) ) {
		xrtSetError(xrtFormat("cannot commit lzma destination: %s", sDestination), TRUE);
		bResult = FALSE;
	}
	if ( !bResult && sTemp != NULL && xrtFileExists(sTemp) ) { xrtFileDelete(sTemp); }
	xrtFree(sTemp);
	return bResult;
}



XLZMA_EXPORT bool xlzma_available(void)
{
	return TRUE;
}

XLZMA_EXPORT const char* xlzma_version(void)
{
	return "lzma sdk 23.01";
}

XLZMA_EXPORT const char* xlzma_last_error(void)
{
	return xrtGetError();
}



XLZMA_EXPORT void* xlzma_compress(const void* pData, size_t iSize, int iLevel,
	uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtlzmaoptions tOptions;
	xrtlzmaerror tError;
	xbuffer pOutput = NULL;
	xrtClearError();
	xlzma_options(&tOptions, iLevel, iMaxInput, iMaxOutput, 0u);
	if ( !xrtLzmaCompress(pData, iSize, &tOptions, &pOutput, &tError) ) {
		xlzma_publish_error(&tError);
		return NULL;
	}
	return pOutput;
}

XLZMA_EXPORT void* xlzma_compress_str(const char* sText, size_t iSize, int iLevel,
	uint64 iMaxInput, uint64 iMaxOutput)
{
	return xlzma_compress((const void*)sText, iSize, iLevel, iMaxInput, iMaxOutput);
}

XLZMA_EXPORT void* xlzma_decompress(const void* pData, size_t iSize, uint64 iMaxInput,
	uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtlzmaoptions tOptions;
	xrtlzmaerror tError;
	xbuffer pOutput = NULL;
	xrtClearError();
	xlzma_options(&tOptions, -1, iMaxInput, iMaxOutput, iMaxRatio);
	if ( !xrtLzmaDecompress(pData, iSize, &tOptions, &pOutput, &tError) ) {
		xlzma_publish_error(&tError);
		return NULL;
	}
	return pOutput;
}

XLZMA_EXPORT bool xlzma_compress_file(str sSource, str sDestination, int iLevel,
	bool bOverwrite, uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtlzmaoptions tOptions;
	xrtClearError();
	xlzma_options(&tOptions, iLevel, iMaxInput, iMaxOutput, 0u);
	return xlzma_transform_file(sSource, sDestination, TRUE, bOverwrite, &tOptions);
}

XLZMA_EXPORT bool xlzma_decompress_file(str sSource, str sDestination, bool bOverwrite,
	uint64 iMaxInput, uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtlzmaoptions tOptions;
	xrtClearError();
	xlzma_options(&tOptions, -1, iMaxInput, iMaxOutput, iMaxRatio);
	return xlzma_transform_file(sSource, sDestination, FALSE, bOverwrite, &tOptions);
}



XLZMA_EXPORT void* xlzma_compressor_create(int iLevel, uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtlzmaoptions tOptions;
	xrtlzmacompressor* pCompressor;
	xrtClearError();
	xlzma_options(&tOptions, iLevel, iMaxInput, iMaxOutput, 0u);
	pCompressor = xrtLzmaCompressorCreate(&tOptions);
	if ( pCompressor == NULL || xrtLzmaCompressorError(pCompressor)->iCode != XRT_LZMA_OK ) {
		xlzma_publish_error(xrtLzmaCompressorError(pCompressor));
		xrtLzmaCompressorDestroy(pCompressor);
		return NULL;
	}
	return pCompressor;
}

XLZMA_EXPORT void xlzma_compressor_destroy(void* pHandle)
{
	xrtLzmaCompressorDestroy((xrtlzmacompressor*)pHandle);
}

XLZMA_EXPORT bool xlzma_compressor_update(void* pHandle, const void* pData, size_t iSize)
{
	xrtlzmacompressor* pCompressor = (xrtlzmacompressor*)pHandle;
	if ( !xrtLzmaCompressorUpdate(pCompressor, pData, iSize) ) {
		xlzma_publish_error(xrtLzmaCompressorError(pCompressor));
		return FALSE;
	}
	return TRUE;
}

XLZMA_EXPORT bool xlzma_compressor_update_str(void* pHandle, const char* sText, size_t iSize)
{
	return xlzma_compressor_update(pHandle, (const void*)sText, iSize);
}

XLZMA_EXPORT bool xlzma_compressor_finish(void* pHandle)
{
	xrtlzmacompressor* pCompressor = (xrtlzmacompressor*)pHandle;
	if ( !xrtLzmaCompressorFinish(pCompressor) ) {
		xlzma_publish_error(xrtLzmaCompressorError(pCompressor));
		return FALSE;
	}
	return TRUE;
}

XLZMA_EXPORT void* xlzma_compressor_drain(void* pHandle)
{
	xrtlzmacompressor* pCompressor = (xrtlzmacompressor*)pHandle;
	xbuffer pOutput = xrtLzmaCompressorDrainBuffer(pCompressor);
	if ( pOutput == NULL ) {
		xlzma_publish_error(xrtLzmaCompressorError(pCompressor));
		return NULL;
	}
	return pOutput;
}

XLZMA_EXPORT uint64 xlzma_compressor_input_bytes(void* pHandle)
{
	return xrtLzmaCompressorInputBytes((const xrtlzmacompressor*)pHandle);
}

XLZMA_EXPORT uint64 xlzma_compressor_output_bytes(void* pHandle)
{
	return xrtLzmaCompressorOutputBytes((const xrtlzmacompressor*)pHandle);
}



XLZMA_EXPORT void* xlzma_decompressor_create(uint64 iMaxInput, uint64 iMaxOutput,
	uint32 iMaxRatio)
{
	xrtlzmaoptions tOptions;
	xrtlzmadecompressor* pDecompressor;
	xrtClearError();
	xlzma_options(&tOptions, -1, iMaxInput, iMaxOutput, iMaxRatio);
	pDecompressor = xrtLzmaDecompressorCreate(&tOptions);
	if ( pDecompressor == NULL || xrtLzmaDecompressorError(pDecompressor)->iCode != XRT_LZMA_OK ) {
		xlzma_publish_error(xrtLzmaDecompressorError(pDecompressor));
		xrtLzmaDecompressorDestroy(pDecompressor);
		return NULL;
	}
	return pDecompressor;
}

XLZMA_EXPORT void xlzma_decompressor_destroy(void* pHandle)
{
	xrtLzmaDecompressorDestroy((xrtlzmadecompressor*)pHandle);
}

XLZMA_EXPORT bool xlzma_decompressor_update(void* pHandle, const void* pData, size_t iSize)
{
	xrtlzmadecompressor* pDecompressor = (xrtlzmadecompressor*)pHandle;
	if ( !xrtLzmaDecompressorUpdate(pDecompressor, pData, iSize) ) {
		xlzma_publish_error(xrtLzmaDecompressorError(pDecompressor));
		return FALSE;
	}
	return TRUE;
}

XLZMA_EXPORT bool xlzma_decompressor_finish(void* pHandle)
{
	xrtlzmadecompressor* pDecompressor = (xrtlzmadecompressor*)pHandle;
	if ( !xrtLzmaDecompressorFinish(pDecompressor) ) {
		xlzma_publish_error(xrtLzmaDecompressorError(pDecompressor));
		return FALSE;
	}
	return TRUE;
}

XLZMA_EXPORT void* xlzma_decompressor_drain(void* pHandle)
{
	xrtlzmadecompressor* pDecompressor = (xrtlzmadecompressor*)pHandle;
	xbuffer pOutput = xrtLzmaDecompressorDrainBuffer(pDecompressor);
	if ( pOutput == NULL ) {
		xlzma_publish_error(xrtLzmaDecompressorError(pDecompressor));
		return NULL;
	}
	return pOutput;
}

XLZMA_EXPORT uint64 xlzma_decompressor_input_bytes(void* pHandle)
{
	return xrtLzmaDecompressorInputBytes((const xrtlzmadecompressor*)pHandle);
}

XLZMA_EXPORT uint64 xlzma_decompressor_output_bytes(void* pHandle)
{
	return xrtLzmaDecompressorOutputBytes((const xrtlzmadecompressor*)pHandle);
}

XLZMA_EXPORT bool xlzma_decompressor_finished(void* pHandle)
{
	return xrtLzmaDecompressorFinished((const xrtlzmadecompressor*)pHandle);
}

XLZMA_EXPORT uint64 xlzma_decompressor_members(void* pHandle)
{
	return xrtLzmaDecompressorMembers((const xrtlzmadecompressor*)pHandle);
}
