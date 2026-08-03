#include "include/xrt_miniz.h"
#include "third_party/miniz.h"

#if defined(_WIN32) || defined(_WIN64)
#define XMINIZ_EXPORT __declspec(dllexport)
#else
#define XMINIZ_EXPORT
#endif



static void xminiz_publish_error(const xrtminizerror* pError)
{
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "miniz operation failed";
	xrtSetError(xrtFormat("%s", sMessage), TRUE);
}

static void xminiz_options(xrtminizoptions* pOptions, int iLevel, uint64 iMaxInput,
	uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtMinizOptionsInit(pOptions);
	if ( iLevel >= 0 ) { pOptions->iLevel = iLevel; }
	if ( iMaxInput != 0u ) { pOptions->tLimits.iMaxInputBytes = iMaxInput; }
	if ( iMaxOutput != 0u ) { pOptions->tLimits.iMaxOutputBytes = iMaxOutput; }
	if ( iMaxRatio != 0u ) { pOptions->tLimits.iMaxCompressionRatio = iMaxRatio; }
}

static bool xminiz_transform_file(str sSource, str sDestination, bool bCompress,
	bool bOverwrite, const xrtminizoptions* pOptions)
{
	xrtminizerror tError;
	xreader pReader = NULL;
	xwriter pWriter = NULL;
	str sTemp = NULL;
	bool bResult = FALSE;
	if ( sSource == NULL || sSource[0] == 0 || sDestination == NULL || sDestination[0] == 0 ) {
		xrtSetError(xrtFormat("miniz source and destination paths are required"), TRUE);
		return FALSE;
	}
	if ( xrtPathExists(sDestination) && !bOverwrite ) {
		xrtSetError(xrtFormat("miniz destination already exists: %s", sDestination), TRUE);
		return FALSE;
	}
	sTemp = xrtPathRandom(sDestination, 0u, (str)".xrt-miniz-tmp", 0u, 8u);
	pReader = xrtReaderOpen(sSource);
	pWriter = sTemp != NULL ? xrtWriterOpen(sTemp, TRUE) : NULL;
	if ( pReader == NULL || pWriter == NULL ) {
		xrtSetError(xrtFormat("cannot open miniz file stream: %s", sSource), TRUE);
		goto cleanup;
	}
	bResult = bCompress ? xrtMinizCompressStream(pReader, pWriter, pOptions, &tError) :
		xrtMinizDecompressStream(pReader, pWriter, pOptions, &tError);
	if ( !bResult ) { xminiz_publish_error(&tError); }
	if ( bResult && !xrtWriterFlush(pWriter) ) {
		xrtSetError(xrtFormat("cannot flush miniz destination: %s", sDestination), TRUE);
		bResult = FALSE;
	}

cleanup:
	xrtReaderDestroy(pReader);
	xrtWriterDestroy(pWriter);
	if ( bResult && !xrtFileRename(sTemp, sDestination, bOverwrite) ) {
		xrtSetError(xrtFormat("cannot commit miniz destination: %s", sDestination), TRUE);
		bResult = FALSE;
	}
	if ( !bResult && sTemp != NULL && xrtFileExists(sTemp) ) { xrtFileDelete(sTemp); }
	xrtFree(sTemp);
	return bResult;
}



XMINIZ_EXPORT bool xminiz_available(void)
{
	return TRUE;
}

XMINIZ_EXPORT const char* xminiz_version(void)
{
	return mz_version();
}

XMINIZ_EXPORT const char* xminiz_last_error(void)
{
	return xrtGetError();
}



XMINIZ_EXPORT void* xminiz_compress(const void* pData, size_t iSize, int iLevel,
	uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtminizoptions tOptions;
	xrtminizerror tError;
	xbuffer pOutput = NULL;
	xrtClearError();
	xminiz_options(&tOptions, iLevel, iMaxInput, iMaxOutput, 0u);
	if ( !xrtMinizCompress(pData, iSize, &tOptions, &pOutput, &tError) ) {
		xminiz_publish_error(&tError);
		return NULL;
	}
	return pOutput;
}

XMINIZ_EXPORT void* xminiz_compress_str(const char* sText, size_t iSize, int iLevel,
	uint64 iMaxInput, uint64 iMaxOutput)
{
	return xminiz_compress((const void*)sText, iSize, iLevel, iMaxInput, iMaxOutput);
}

XMINIZ_EXPORT void* xminiz_decompress(const void* pData, size_t iSize, uint64 iMaxInput,
	uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtminizoptions tOptions;
	xrtminizerror tError;
	xbuffer pOutput = NULL;
	xrtClearError();
	xminiz_options(&tOptions, -1, iMaxInput, iMaxOutput, iMaxRatio);
	if ( !xrtMinizDecompress(pData, iSize, &tOptions, &pOutput, &tError) ) {
		xminiz_publish_error(&tError);
		return NULL;
	}
	return pOutput;
}

XMINIZ_EXPORT bool xminiz_compress_file(str sSource, str sDestination, int iLevel,
	bool bOverwrite, uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtminizoptions tOptions;
	xrtClearError();
	xminiz_options(&tOptions, iLevel, iMaxInput, iMaxOutput, 0u);
	return xminiz_transform_file(sSource, sDestination, TRUE, bOverwrite, &tOptions);
}

XMINIZ_EXPORT bool xminiz_decompress_file(str sSource, str sDestination, bool bOverwrite,
	uint64 iMaxInput, uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtminizoptions tOptions;
	xrtClearError();
	xminiz_options(&tOptions, -1, iMaxInput, iMaxOutput, iMaxRatio);
	return xminiz_transform_file(sSource, sDestination, FALSE, bOverwrite, &tOptions);
}



XMINIZ_EXPORT void* xminiz_compressor_create(int iLevel, uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtminizoptions tOptions;
	xrtminizcompressor* pCompressor;
	xrtClearError();
	xminiz_options(&tOptions, iLevel, iMaxInput, iMaxOutput, 0u);
	pCompressor = xrtMinizCompressorCreate(&tOptions);
	if ( pCompressor == NULL || xrtMinizCompressorError(pCompressor)->iCode != XRT_MINIZ_OK ) {
		xminiz_publish_error(xrtMinizCompressorError(pCompressor));
		xrtMinizCompressorDestroy(pCompressor);
		return NULL;
	}
	return pCompressor;
}

XMINIZ_EXPORT void xminiz_compressor_destroy(void* pHandle)
{
	xrtMinizCompressorDestroy((xrtminizcompressor*)pHandle);
}

XMINIZ_EXPORT bool xminiz_compressor_update(void* pHandle, const void* pData, size_t iSize)
{
	xrtminizcompressor* pCompressor = (xrtminizcompressor*)pHandle;
	if ( !xrtMinizCompressorUpdate(pCompressor, pData, iSize) ) {
		xminiz_publish_error(xrtMinizCompressorError(pCompressor));
		return FALSE;
	}
	return TRUE;
}

XMINIZ_EXPORT bool xminiz_compressor_update_str(void* pHandle, const char* sText, size_t iSize)
{
	return xminiz_compressor_update(pHandle, (const void*)sText, iSize);
}

XMINIZ_EXPORT bool xminiz_compressor_finish(void* pHandle)
{
	xrtminizcompressor* pCompressor = (xrtminizcompressor*)pHandle;
	if ( !xrtMinizCompressorFinish(pCompressor) ) {
		xminiz_publish_error(xrtMinizCompressorError(pCompressor));
		return FALSE;
	}
	return TRUE;
}

XMINIZ_EXPORT void* xminiz_compressor_drain(void* pHandle)
{
	xrtminizcompressor* pCompressor = (xrtminizcompressor*)pHandle;
	xbuffer pOutput = xrtMinizCompressorDrainBuffer(pCompressor);
	if ( pOutput == NULL ) {
		xminiz_publish_error(xrtMinizCompressorError(pCompressor));
		return NULL;
	}
	return pOutput;
}

XMINIZ_EXPORT uint64 xminiz_compressor_input_bytes(void* pHandle)
{
	return xrtMinizCompressorInputBytes((const xrtminizcompressor*)pHandle);
}

XMINIZ_EXPORT uint64 xminiz_compressor_output_bytes(void* pHandle)
{
	return xrtMinizCompressorOutputBytes((const xrtminizcompressor*)pHandle);
}



XMINIZ_EXPORT void* xminiz_decompressor_create(uint64 iMaxInput, uint64 iMaxOutput,
	uint32 iMaxRatio)
{
	xrtminizoptions tOptions;
	xrtminizdecompressor* pDecompressor;
	xrtClearError();
	xminiz_options(&tOptions, -1, iMaxInput, iMaxOutput, iMaxRatio);
	pDecompressor = xrtMinizDecompressorCreate(&tOptions);
	if ( pDecompressor == NULL || xrtMinizDecompressorError(pDecompressor)->iCode != XRT_MINIZ_OK ) {
		xminiz_publish_error(xrtMinizDecompressorError(pDecompressor));
		xrtMinizDecompressorDestroy(pDecompressor);
		return NULL;
	}
	return pDecompressor;
}

XMINIZ_EXPORT void xminiz_decompressor_destroy(void* pHandle)
{
	xrtMinizDecompressorDestroy((xrtminizdecompressor*)pHandle);
}

XMINIZ_EXPORT bool xminiz_decompressor_update(void* pHandle, const void* pData, size_t iSize)
{
	xrtminizdecompressor* pDecompressor = (xrtminizdecompressor*)pHandle;
	if ( !xrtMinizDecompressorUpdate(pDecompressor, pData, iSize) ) {
		xminiz_publish_error(xrtMinizDecompressorError(pDecompressor));
		return FALSE;
	}
	return TRUE;
}

XMINIZ_EXPORT bool xminiz_decompressor_finish(void* pHandle)
{
	xrtminizdecompressor* pDecompressor = (xrtminizdecompressor*)pHandle;
	if ( !xrtMinizDecompressorFinish(pDecompressor) ) {
		xminiz_publish_error(xrtMinizDecompressorError(pDecompressor));
		return FALSE;
	}
	return TRUE;
}

XMINIZ_EXPORT void* xminiz_decompressor_drain(void* pHandle)
{
	xrtminizdecompressor* pDecompressor = (xrtminizdecompressor*)pHandle;
	xbuffer pOutput = xrtMinizDecompressorDrainBuffer(pDecompressor);
	if ( pOutput == NULL ) {
		xminiz_publish_error(xrtMinizDecompressorError(pDecompressor));
		return NULL;
	}
	return pOutput;
}

XMINIZ_EXPORT uint64 xminiz_decompressor_input_bytes(void* pHandle)
{
	return xrtMinizDecompressorInputBytes((const xrtminizdecompressor*)pHandle);
}

XMINIZ_EXPORT uint64 xminiz_decompressor_output_bytes(void* pHandle)
{
	return xrtMinizDecompressorOutputBytes((const xrtminizdecompressor*)pHandle);
}

XMINIZ_EXPORT bool xminiz_decompressor_finished(void* pHandle)
{
	return xrtMinizDecompressorFinished((const xrtminizdecompressor*)pHandle);
}

XMINIZ_EXPORT uint64 xminiz_decompressor_members(void* pHandle)
{
	return xrtMinizDecompressorMembers((const xrtminizdecompressor*)pHandle);
}
