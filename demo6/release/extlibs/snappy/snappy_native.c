#include "include/xrt_snappy.h"

#if defined(_WIN32) || defined(_WIN64)
#define XSNAPPY_EXPORT __declspec(dllexport)
#else
#define XSNAPPY_EXPORT
#endif



static void xsnappy_publish_error(const xrtsnappyerror* pError)
{
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "snappy operation failed";
	xrtSetError(xrtFormat("%s", sMessage), TRUE);
}

static void xsnappy_options(xrtsnappyoptions* pOptions, uint64 iMaxInput,
	uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtSnappyOptionsInit(pOptions);
	if ( iMaxInput != 0u ) { pOptions->tLimits.iMaxInputBytes = iMaxInput; }
	if ( iMaxOutput != 0u ) { pOptions->tLimits.iMaxOutputBytes = iMaxOutput; }
	if ( iMaxRatio != 0u ) { pOptions->tLimits.iMaxCompressionRatio = iMaxRatio; }
}

static bool xsnappy_transform_file(str sSource, str sDestination, bool bCompress,
	bool bOverwrite, const xrtsnappyoptions* pOptions)
{
	xrtsnappyerror tError;
	xreader pReader = NULL;
	xwriter pWriter = NULL;
	str sTemp = NULL;
	bool bResult = FALSE;
	if ( sSource == NULL || sSource[0] == 0 || sDestination == NULL || sDestination[0] == 0 ) {
		xrtSetError(xrtFormat("snappy source and destination paths are required"), TRUE);
		return FALSE;
	}
	if ( xrtPathExists(sDestination) && !bOverwrite ) {
		xrtSetError(xrtFormat("snappy destination already exists: %s", sDestination), TRUE);
		return FALSE;
	}
	sTemp = xrtPathRandom(sDestination, 0u, (str)".xrt-snappy-tmp", 0u, 8u);
	pReader = xrtReaderOpen(sSource);
	pWriter = sTemp != NULL ? xrtWriterOpen(sTemp, TRUE) : NULL;
	if ( pReader == NULL || pWriter == NULL ) {
		xrtSetError(xrtFormat("cannot open snappy file stream: %s", sSource), TRUE);
		goto cleanup;
	}
	bResult = bCompress ? xrtSnappyCompressStream(pReader, pWriter, pOptions, &tError) :
		xrtSnappyDecompressStream(pReader, pWriter, pOptions, &tError);
	if ( !bResult ) { xsnappy_publish_error(&tError); }
	if ( bResult && !xrtWriterFlush(pWriter) ) {
		xrtSetError(xrtFormat("cannot flush snappy destination: %s", sDestination), TRUE);
		bResult = FALSE;
	}

cleanup:
	xrtReaderDestroy(pReader);
	xrtWriterDestroy(pWriter);
	if ( bResult && !xrtFileRename(sTemp, sDestination, bOverwrite) ) {
		xrtSetError(xrtFormat("cannot commit snappy destination: %s", sDestination), TRUE);
		bResult = FALSE;
	}
	if ( !bResult && sTemp != NULL && xrtFileExists(sTemp) ) { xrtFileDelete(sTemp); }
	xrtFree(sTemp);
	return bResult;
}



XSNAPPY_EXPORT bool xsnappy_available(void)
{
	return TRUE;
}

XSNAPPY_EXPORT const char* xsnappy_version(void)
{
	/* 纯 C snappy 格式编解码器（格式兼容 Google Snappy），版本随 xlang 扩展库发布。 */
	return "1.0.0";
}

XSNAPPY_EXPORT const char* xsnappy_last_error(void)
{
	return xrtGetError();
}



XSNAPPY_EXPORT void* xsnappy_compress(const void* pData, size_t iSize,
	uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtsnappyoptions tOptions;
	xrtsnappyerror tError;
	xbuffer pOutput = NULL;
	xrtClearError();
	xsnappy_options(&tOptions, iMaxInput, iMaxOutput, 0u);
	if ( !xrtSnappyCompress(pData, iSize, &tOptions, &pOutput, &tError) ) {
		xsnappy_publish_error(&tError);
		return NULL;
	}
	return pOutput;
}

XSNAPPY_EXPORT void* xsnappy_compress_str(const char* sText, size_t iSize,
	uint64 iMaxInput, uint64 iMaxOutput)
{
	return xsnappy_compress((const void*)sText, iSize, iMaxInput, iMaxOutput);
}

XSNAPPY_EXPORT void* xsnappy_decompress(const void* pData, size_t iSize, uint64 iMaxInput,
	uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtsnappyoptions tOptions;
	xrtsnappyerror tError;
	xbuffer pOutput = NULL;
	xrtClearError();
	xsnappy_options(&tOptions, iMaxInput, iMaxOutput, iMaxRatio);
	if ( !xrtSnappyDecompress(pData, iSize, &tOptions, &pOutput, &tError) ) {
		xsnappy_publish_error(&tError);
		return NULL;
	}
	return pOutput;
}

XSNAPPY_EXPORT bool xsnappy_compress_file(str sSource, str sDestination, bool bOverwrite,
	uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtsnappyoptions tOptions;
	xrtClearError();
	xsnappy_options(&tOptions, iMaxInput, iMaxOutput, 0u);
	return xsnappy_transform_file(sSource, sDestination, TRUE, bOverwrite, &tOptions);
}

XSNAPPY_EXPORT bool xsnappy_decompress_file(str sSource, str sDestination, bool bOverwrite,
	uint64 iMaxInput, uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtsnappyoptions tOptions;
	xrtClearError();
	xsnappy_options(&tOptions, iMaxInput, iMaxOutput, iMaxRatio);
	return xsnappy_transform_file(sSource, sDestination, FALSE, bOverwrite, &tOptions);
}



XSNAPPY_EXPORT void* xsnappy_compressor_create(uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtsnappyoptions tOptions;
	xrtsnappycompressor* pCompressor;
	xrtClearError();
	xsnappy_options(&tOptions, iMaxInput, iMaxOutput, 0u);
	pCompressor = xrtSnappyCompressorCreate(&tOptions);
	if ( pCompressor == NULL || xrtSnappyCompressorError(pCompressor)->iCode != XRT_SNAPPY_OK ) {
		xsnappy_publish_error(xrtSnappyCompressorError(pCompressor));
		xrtSnappyCompressorDestroy(pCompressor);
		return NULL;
	}
	return pCompressor;
}

XSNAPPY_EXPORT void xsnappy_compressor_destroy(void* pHandle)
{
	xrtSnappyCompressorDestroy((xrtsnappycompressor*)pHandle);
}

XSNAPPY_EXPORT bool xsnappy_compressor_update(void* pHandle, const void* pData, size_t iSize)
{
	xrtsnappycompressor* pCompressor = (xrtsnappycompressor*)pHandle;
	if ( !xrtSnappyCompressorUpdate(pCompressor, pData, iSize) ) {
		xsnappy_publish_error(xrtSnappyCompressorError(pCompressor));
		return FALSE;
	}
	return TRUE;
}

XSNAPPY_EXPORT bool xsnappy_compressor_update_str(void* pHandle, const char* sText, size_t iSize)
{
	return xsnappy_compressor_update(pHandle, (const void*)sText, iSize);
}

XSNAPPY_EXPORT bool xsnappy_compressor_finish(void* pHandle)
{
	xrtsnappycompressor* pCompressor = (xrtsnappycompressor*)pHandle;
	if ( !xrtSnappyCompressorFinish(pCompressor) ) {
		xsnappy_publish_error(xrtSnappyCompressorError(pCompressor));
		return FALSE;
	}
	return TRUE;
}

XSNAPPY_EXPORT void* xsnappy_compressor_drain(void* pHandle)
{
	xrtsnappycompressor* pCompressor = (xrtsnappycompressor*)pHandle;
	xbuffer pOutput = xrtSnappyCompressorDrainBuffer(pCompressor);
	if ( pOutput == NULL ) {
		xsnappy_publish_error(xrtSnappyCompressorError(pCompressor));
		return NULL;
	}
	return pOutput;
}

XSNAPPY_EXPORT uint64 xsnappy_compressor_input_bytes(void* pHandle)
{
	return xrtSnappyCompressorInputBytes((const xrtsnappycompressor*)pHandle);
}

XSNAPPY_EXPORT uint64 xsnappy_compressor_output_bytes(void* pHandle)
{
	return xrtSnappyCompressorOutputBytes((const xrtsnappycompressor*)pHandle);
}



XSNAPPY_EXPORT void* xsnappy_decompressor_create(uint64 iMaxInput, uint64 iMaxOutput,
	uint32 iMaxRatio)
{
	xrtsnappyoptions tOptions;
	xrtsnappydecompressor* pDecompressor;
	xrtClearError();
	xsnappy_options(&tOptions, iMaxInput, iMaxOutput, iMaxRatio);
	pDecompressor = xrtSnappyDecompressorCreate(&tOptions);
	if ( pDecompressor == NULL || xrtSnappyDecompressorError(pDecompressor)->iCode != XRT_SNAPPY_OK ) {
		xsnappy_publish_error(xrtSnappyDecompressorError(pDecompressor));
		xrtSnappyDecompressorDestroy(pDecompressor);
		return NULL;
	}
	return pDecompressor;
}

XSNAPPY_EXPORT void xsnappy_decompressor_destroy(void* pHandle)
{
	xrtSnappyDecompressorDestroy((xrtsnappydecompressor*)pHandle);
}

XSNAPPY_EXPORT bool xsnappy_decompressor_update(void* pHandle, const void* pData, size_t iSize)
{
	xrtsnappydecompressor* pDecompressor = (xrtsnappydecompressor*)pHandle;
	if ( !xrtSnappyDecompressorUpdate(pDecompressor, pData, iSize) ) {
		xsnappy_publish_error(xrtSnappyDecompressorError(pDecompressor));
		return FALSE;
	}
	return TRUE;
}

XSNAPPY_EXPORT bool xsnappy_decompressor_finish(void* pHandle)
{
	xrtsnappydecompressor* pDecompressor = (xrtsnappydecompressor*)pHandle;
	if ( !xrtSnappyDecompressorFinish(pDecompressor) ) {
		xsnappy_publish_error(xrtSnappyDecompressorError(pDecompressor));
		return FALSE;
	}
	return TRUE;
}

XSNAPPY_EXPORT void* xsnappy_decompressor_drain(void* pHandle)
{
	xrtsnappydecompressor* pDecompressor = (xrtsnappydecompressor*)pHandle;
	xbuffer pOutput = xrtSnappyDecompressorDrainBuffer(pDecompressor);
	if ( pOutput == NULL ) {
		xsnappy_publish_error(xrtSnappyDecompressorError(pDecompressor));
		return NULL;
	}
	return pOutput;
}

XSNAPPY_EXPORT uint64 xsnappy_decompressor_input_bytes(void* pHandle)
{
	return xrtSnappyDecompressorInputBytes((const xrtsnappydecompressor*)pHandle);
}

XSNAPPY_EXPORT uint64 xsnappy_decompressor_output_bytes(void* pHandle)
{
	return xrtSnappyDecompressorOutputBytes((const xrtsnappydecompressor*)pHandle);
}

XSNAPPY_EXPORT bool xsnappy_decompressor_finished(void* pHandle)
{
	return xrtSnappyDecompressorFinished((const xrtsnappydecompressor*)pHandle);
}

XSNAPPY_EXPORT uint64 xsnappy_decompressor_members(void* pHandle)
{
	return xrtSnappyDecompressorMembers((const xrtsnappydecompressor*)pHandle);
}
