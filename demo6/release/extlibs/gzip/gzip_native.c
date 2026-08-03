#include "include/xrt_gzip.h"

#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#define XGZIP_EXPORT __declspec(dllexport)
#else
#define XGZIP_EXPORT
#endif



static void xgzip_publish_error(const xrtgziperror* pError)
{
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "gzip operation failed";
	xrtSetError(xrtFormat("%s", sMessage), TRUE);
}

static void xgzip_options(xrtgzipoptions* pOptions, int iLevel, uint32 iMTime,
	uint64 iMaxInput, uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtGzipOptionsInit(pOptions);
	pOptions->iLevel = iLevel;
	pOptions->iMTime = iMTime;
	if ( iMaxInput != 0u ) { pOptions->tLimits.iMaxInputBytes = iMaxInput; }
	if ( iMaxOutput != 0u ) { pOptions->tLimits.iMaxOutputBytes = iMaxOutput; }
	if ( iMaxRatio != 0u ) { pOptions->tLimits.iMaxCompressionRatio = iMaxRatio; }
}

static bool xgzip_transform_file(str sSource, str sDestination, bool bCompress,
	bool bOverwrite, const xrtgzipoptions* pOptions)
{
	xrtgziperror tError;
	xreader pReader = NULL;
	xwriter pWriter = NULL;
	str sTemp = NULL;
	bool bResult = FALSE;
	if ( sSource == NULL || sSource[0] == 0 || sDestination == NULL || sDestination[0] == 0 ) {
		xrtSetError(xrtFormat("gzip source and destination paths are required"), TRUE);
		return FALSE;
	}
	if ( xrtPathExists(sDestination) && !bOverwrite ) {
		xrtSetError(xrtFormat("gzip destination already exists: %s", sDestination), TRUE);
		return FALSE;
	}
	sTemp = xrtPathRandom(sDestination, 0u, (str)".xrt-gzip-tmp", 0u, 8u);
	pReader = xrtReaderOpen(sSource);
	pWriter = sTemp != NULL ? xrtWriterOpen(sTemp, TRUE) : NULL;
	if ( pReader == NULL || pWriter == NULL ) {
		xrtSetError(xrtFormat("cannot open gzip file stream: %s", sSource), TRUE);
		goto cleanup;
	}
	bResult = bCompress ? xrtGzipCompressStream(pReader, pWriter, pOptions, &tError) :
		xrtGzipDecompressStream(pReader, pWriter, pOptions, &tError);
	if ( !bResult ) { xgzip_publish_error(&tError); }
	if ( bResult && !xrtWriterFlush(pWriter) ) {
		xrtSetError(xrtFormat("cannot flush gzip destination: %s", sDestination), TRUE);
		bResult = FALSE;
	}

cleanup:
	xrtReaderDestroy(pReader);
	xrtWriterDestroy(pWriter);
	if ( bResult && !xrtFileRename(sTemp, sDestination, bOverwrite) ) {
		xrtSetError(xrtFormat("cannot commit gzip destination: %s", sDestination), TRUE);
		bResult = FALSE;
	}
	if ( !bResult && sTemp != NULL && xrtFileExists(sTemp) ) { xrtFileDelete(sTemp); }
	xrtFree(sTemp);
	return bResult;
}



XGZIP_EXPORT void* xgzip_compress(const void* pData, size_t iSize, int iLevel,
	uint32 iMTime, uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtgzipoptions tOptions;
	xrtgziperror tError;
	xbuffer pOutput = NULL;
	xrtClearError();
	xgzip_options(&tOptions, iLevel, iMTime, iMaxInput, iMaxOutput, 0u);
	if ( !xrtGzipCompress(pData, iSize, &tOptions, &pOutput, &tError) ) {
		xgzip_publish_error(&tError);
		return NULL;
	}
	return pOutput;
}

XGZIP_EXPORT void* xgzip_compress_str(const char* sText, size_t iSize, int iLevel,
	uint32 iMTime, uint64 iMaxInput, uint64 iMaxOutput)
{
	return xgzip_compress((const void*)sText, iSize, iLevel, iMTime, iMaxInput, iMaxOutput);
}

XGZIP_EXPORT void* xgzip_decompress(const void* pData, size_t iSize,
	uint64 iMaxInput, uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtgzipoptions tOptions;
	xrtgziperror tError;
	xbuffer pOutput = NULL;
	xrtClearError();
	xgzip_options(&tOptions, -1, 0u, iMaxInput, iMaxOutput, iMaxRatio);
	if ( !xrtGzipDecompress(pData, iSize, &tOptions, &pOutput, &tError) ) {
		xgzip_publish_error(&tError);
		return NULL;
	}
	return pOutput;
}

XGZIP_EXPORT bool xgzip_compress_file(str sSource, str sDestination, int iLevel,
	uint32 iMTime, bool bOverwrite, uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtgzipoptions tOptions;
	xrtClearError();
	xgzip_options(&tOptions, iLevel, iMTime, iMaxInput, iMaxOutput, 0u);
	return xgzip_transform_file(sSource, sDestination, TRUE, bOverwrite, &tOptions);
}

XGZIP_EXPORT bool xgzip_decompress_file(str sSource, str sDestination,
	bool bOverwrite, uint64 iMaxInput, uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtgzipoptions tOptions;
	xrtClearError();
	xgzip_options(&tOptions, -1, 0u, iMaxInput, iMaxOutput, iMaxRatio);
	return xgzip_transform_file(sSource, sDestination, FALSE, bOverwrite, &tOptions);
}



XGZIP_EXPORT void* xgzip_compressor_create(int iLevel, uint32 iMTime,
	uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtgzipoptions tOptions;
	xrtgzipcompressor* pCompressor;
	xrtClearError();
	xgzip_options(&tOptions, iLevel, iMTime, iMaxInput, iMaxOutput, 0u);
	pCompressor = xrtGzipCompressorCreate(&tOptions);
	if ( pCompressor == NULL || xrtGzipCompressorError(pCompressor)->iCode != XRT_GZIP_OK ) {
		xgzip_publish_error(xrtGzipCompressorError(pCompressor));
		xrtGzipCompressorDestroy(pCompressor);
		return NULL;
	}
	return pCompressor;
}

XGZIP_EXPORT void xgzip_compressor_destroy(void* pHandle)
{
	xrtGzipCompressorDestroy((xrtgzipcompressor*)pHandle);
}

XGZIP_EXPORT bool xgzip_compressor_update(void* pHandle, const void* pData, size_t iSize)
{
	xrtgzipcompressor* pCompressor = (xrtgzipcompressor*)pHandle;
	if ( !xrtGzipCompressorUpdate(pCompressor, pData, iSize) ) {
		xgzip_publish_error(xrtGzipCompressorError(pCompressor));
		return FALSE;
	}
	return TRUE;
}

XGZIP_EXPORT bool xgzip_compressor_update_str(void* pHandle, const char* sText, size_t iSize)
{
	return xgzip_compressor_update(pHandle, (const void*)sText, iSize);
}

XGZIP_EXPORT void* xgzip_compressor_finish(void* pHandle)
{
	xrtgzipcompressor* pCompressor = (xrtgzipcompressor*)pHandle;
	if ( !xrtGzipCompressorFinish(pCompressor) ) {
		xgzip_publish_error(xrtGzipCompressorError(pCompressor));
		return NULL;
	}
	return xrtGzipCompressorTakeBuffer(pCompressor);
}



XGZIP_EXPORT void* xgzip_decompressor_create(uint64 iMaxInput, uint64 iMaxOutput,
	uint32 iMaxRatio)
{
	xrtgzipoptions tOptions;
	xrtgzipdecompressor* pDecompressor;
	xrtClearError();
	xgzip_options(&tOptions, -1, 0u, iMaxInput, iMaxOutput, iMaxRatio);
	pDecompressor = xrtGzipDecompressorCreate(&tOptions);
	if ( pDecompressor == NULL || xrtGzipDecompressorError(pDecompressor)->iCode != XRT_GZIP_OK ) {
		xgzip_publish_error(xrtGzipDecompressorError(pDecompressor));
		xrtGzipDecompressorDestroy(pDecompressor);
		return NULL;
	}
	return pDecompressor;
}

XGZIP_EXPORT void xgzip_decompressor_destroy(void* pHandle)
{
	xrtGzipDecompressorDestroy((xrtgzipdecompressor*)pHandle);
}

XGZIP_EXPORT bool xgzip_decompressor_update(void* pHandle, const void* pData, size_t iSize)
{
	xrtgzipdecompressor* pDecompressor = (xrtgzipdecompressor*)pHandle;
	if ( !xrtGzipDecompressorUpdate(pDecompressor, pData, iSize) ) {
		xgzip_publish_error(xrtGzipDecompressorError(pDecompressor));
		return FALSE;
	}
	return TRUE;
}

XGZIP_EXPORT void* xgzip_decompressor_finish(void* pHandle)
{
	xrtgzipdecompressor* pDecompressor = (xrtgzipdecompressor*)pHandle;
	if ( !xrtGzipDecompressorFinish(pDecompressor) ) {
		xgzip_publish_error(xrtGzipDecompressorError(pDecompressor));
		return NULL;
	}
	return xrtGzipDecompressorTakeBuffer(pDecompressor);
}
