#include "include/xrt_zstd.h"
#include "third_party/zstd.h"

#if defined(_WIN32) || defined(_WIN64)
#define XZSTD_EXPORT __declspec(dllexport)
#else
#define XZSTD_EXPORT
#endif



static void xzstd_publish_error(const xrtzstderror* pError)
{
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "zstd operation failed";
	xrtSetError(xrtFormat("%s", sMessage), TRUE);
}

static void xzstd_options(xrtzstdoptions* pOptions, int iLevel, uint64 iMaxInput,
	uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtZstdOptionsInit(pOptions);
	if ( iLevel != 0 ) { pOptions->iLevel = iLevel; }
	if ( iMaxInput != 0u ) { pOptions->tLimits.iMaxInputBytes = iMaxInput; }
	if ( iMaxOutput != 0u ) { pOptions->tLimits.iMaxOutputBytes = iMaxOutput; }
	if ( iMaxRatio != 0u ) { pOptions->tLimits.iMaxCompressionRatio = iMaxRatio; }
}

static bool xzstd_transform_file(str sSource, str sDestination, bool bCompress,
	bool bOverwrite, const xrtzstdoptions* pOptions)
{
	xrtzstderror tError;
	xreader pReader = NULL;
	xwriter pWriter = NULL;
	str sTemp = NULL;
	bool bResult = FALSE;
	if ( sSource == NULL || sSource[0] == 0 || sDestination == NULL || sDestination[0] == 0 ) {
		xrtSetError(xrtFormat("zstd source and destination paths are required"), TRUE);
		return FALSE;
	}
	if ( xrtPathExists(sDestination) && !bOverwrite ) {
		xrtSetError(xrtFormat("zstd destination already exists: %s", sDestination), TRUE);
		return FALSE;
	}
	sTemp = xrtPathRandom(sDestination, 0u, (str)".xrt-zstd-tmp", 0u, 8u);
	pReader = xrtReaderOpen(sSource);
	pWriter = sTemp != NULL ? xrtWriterOpen(sTemp, TRUE) : NULL;
	if ( pReader == NULL || pWriter == NULL ) {
		xrtSetError(xrtFormat("cannot open zstd file stream: %s", sSource), TRUE);
		goto cleanup;
	}
	bResult = bCompress ? xrtZstdCompressStream(pReader, pWriter, pOptions, &tError) :
		xrtZstdDecompressStream(pReader, pWriter, pOptions, &tError);
	if ( !bResult ) { xzstd_publish_error(&tError); }
	if ( bResult && !xrtWriterFlush(pWriter) ) {
		xrtSetError(xrtFormat("cannot flush zstd destination: %s", sDestination), TRUE);
		bResult = FALSE;
	}

cleanup:
	xrtReaderDestroy(pReader);
	xrtWriterDestroy(pWriter);
	if ( bResult && !xrtFileRename(sTemp, sDestination, bOverwrite) ) {
		xrtSetError(xrtFormat("cannot commit zstd destination: %s", sDestination), TRUE);
		bResult = FALSE;
	}
	if ( !bResult && sTemp != NULL && xrtFileExists(sTemp) ) { xrtFileDelete(sTemp); }
	xrtFree(sTemp);
	return bResult;
}



XZSTD_EXPORT bool xzstd_available(void)
{
	return TRUE;
}

XZSTD_EXPORT const char* xzstd_version(void)
{
	return ZSTD_versionString();
}

XZSTD_EXPORT const char* xzstd_last_error(void)
{
	return xrtGetError();
}



XZSTD_EXPORT void* xzstd_compress(const void* pData, size_t iSize, int iLevel,
	uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtzstdoptions tOptions;
	xrtzstderror tError;
	xbuffer pOutput = NULL;
	xrtClearError();
	xzstd_options(&tOptions, iLevel, iMaxInput, iMaxOutput, 0u);
	if ( !xrtZstdCompress(pData, iSize, &tOptions, &pOutput, &tError) ) {
		xzstd_publish_error(&tError);
		return NULL;
	}
	return pOutput;
}

XZSTD_EXPORT void* xzstd_decompress(const void* pData, size_t iSize, uint64 iMaxInput,
	uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtzstdoptions tOptions;
	xrtzstderror tError;
	xbuffer pOutput = NULL;
	xrtClearError();
	xzstd_options(&tOptions, 0, iMaxInput, iMaxOutput, iMaxRatio);
	if ( !xrtZstdDecompress(pData, iSize, &tOptions, &pOutput, &tError) ) {
		xzstd_publish_error(&tError);
		return NULL;
	}
	return pOutput;
}

XZSTD_EXPORT bool xzstd_compress_file(str sSource, str sDestination, int iLevel,
	bool bOverwrite, uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtzstdoptions tOptions;
	xrtClearError();
	xzstd_options(&tOptions, iLevel, iMaxInput, iMaxOutput, 0u);
	return xzstd_transform_file(sSource, sDestination, TRUE, bOverwrite, &tOptions);
}

XZSTD_EXPORT bool xzstd_decompress_file(str sSource, str sDestination, bool bOverwrite,
	uint64 iMaxInput, uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtzstdoptions tOptions;
	xrtClearError();
	xzstd_options(&tOptions, 0, iMaxInput, iMaxOutput, iMaxRatio);
	return xzstd_transform_file(sSource, sDestination, FALSE, bOverwrite, &tOptions);
}



XZSTD_EXPORT void* xzstd_compressor_create(int iLevel, uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtzstdoptions tOptions;
	xrtzstdcompressor* pCompressor;
	xrtClearError();
	xzstd_options(&tOptions, iLevel, iMaxInput, iMaxOutput, 0u);
	pCompressor = xrtZstdCompressorCreate(&tOptions);
	if ( pCompressor == NULL || xrtZstdCompressorError(pCompressor)->iCode != XRT_ZSTD_OK ) {
		xzstd_publish_error(xrtZstdCompressorError(pCompressor));
		xrtZstdCompressorDestroy(pCompressor);
		return NULL;
	}
	return pCompressor;
}

XZSTD_EXPORT void xzstd_compressor_destroy(void* pHandle)
{
	xrtZstdCompressorDestroy((xrtzstdcompressor*)pHandle);
}

XZSTD_EXPORT bool xzstd_compressor_update(void* pHandle, const void* pData, size_t iSize)
{
	xrtzstdcompressor* pCompressor = (xrtzstdcompressor*)pHandle;
	if ( !xrtZstdCompressorUpdate(pCompressor, pData, iSize) ) {
		xzstd_publish_error(xrtZstdCompressorError(pCompressor));
		return FALSE;
	}
	return TRUE;
}

XZSTD_EXPORT bool xzstd_compressor_finish(void* pHandle)
{
	xrtzstdcompressor* pCompressor = (xrtzstdcompressor*)pHandle;
	if ( !xrtZstdCompressorFinish(pCompressor) ) {
		xzstd_publish_error(xrtZstdCompressorError(pCompressor));
		return FALSE;
	}
	return TRUE;
}

XZSTD_EXPORT void* xzstd_compressor_drain(void* pHandle)
{
	xrtzstdcompressor* pCompressor = (xrtzstdcompressor*)pHandle;
	xbuffer pOutput = xrtZstdCompressorDrainBuffer(pCompressor);
	if ( pOutput == NULL ) {
		xzstd_publish_error(xrtZstdCompressorError(pCompressor));
		return NULL;
	}
	return pOutput;
}

XZSTD_EXPORT uint64 xzstd_compressor_input_bytes(void* pHandle)
{
	return xrtZstdCompressorInputBytes((const xrtzstdcompressor*)pHandle);
}

XZSTD_EXPORT uint64 xzstd_compressor_output_bytes(void* pHandle)
{
	return xrtZstdCompressorOutputBytes((const xrtzstdcompressor*)pHandle);
}



XZSTD_EXPORT void* xzstd_decompressor_create(uint64 iMaxInput, uint64 iMaxOutput,
	uint32 iMaxRatio)
{
	xrtzstdoptions tOptions;
	xrtzstddecompressor* pDecompressor;
	xrtClearError();
	xzstd_options(&tOptions, 0, iMaxInput, iMaxOutput, iMaxRatio);
	pDecompressor = xrtZstdDecompressorCreate(&tOptions);
	if ( pDecompressor == NULL || xrtZstdDecompressorError(pDecompressor)->iCode != XRT_ZSTD_OK ) {
		xzstd_publish_error(xrtZstdDecompressorError(pDecompressor));
		xrtZstdDecompressorDestroy(pDecompressor);
		return NULL;
	}
	return pDecompressor;
}

XZSTD_EXPORT void xzstd_decompressor_destroy(void* pHandle)
{
	xrtZstdDecompressorDestroy((xrtzstddecompressor*)pHandle);
}

XZSTD_EXPORT bool xzstd_decompressor_update(void* pHandle, const void* pData, size_t iSize)
{
	xrtzstddecompressor* pDecompressor = (xrtzstddecompressor*)pHandle;
	if ( !xrtZstdDecompressorUpdate(pDecompressor, pData, iSize) ) {
		xzstd_publish_error(xrtZstdDecompressorError(pDecompressor));
		return FALSE;
	}
	return TRUE;
}

XZSTD_EXPORT bool xzstd_decompressor_finish(void* pHandle)
{
	xrtzstddecompressor* pDecompressor = (xrtzstddecompressor*)pHandle;
	if ( !xrtZstdDecompressorFinish(pDecompressor) ) {
		xzstd_publish_error(xrtZstdDecompressorError(pDecompressor));
		return FALSE;
	}
	return TRUE;
}

XZSTD_EXPORT void* xzstd_decompressor_drain(void* pHandle)
{
	xrtzstddecompressor* pDecompressor = (xrtzstddecompressor*)pHandle;
	xbuffer pOutput = xrtZstdDecompressorDrainBuffer(pDecompressor);
	if ( pOutput == NULL ) {
		xzstd_publish_error(xrtZstdDecompressorError(pDecompressor));
		return NULL;
	}
	return pOutput;
}

XZSTD_EXPORT uint64 xzstd_decompressor_input_bytes(void* pHandle)
{
	return xrtZstdDecompressorInputBytes((const xrtzstddecompressor*)pHandle);
}

XZSTD_EXPORT uint64 xzstd_decompressor_output_bytes(void* pHandle)
{
	return xrtZstdDecompressorOutputBytes((const xrtzstddecompressor*)pHandle);
}

XZSTD_EXPORT bool xzstd_decompressor_finished(void* pHandle)
{
	return xrtZstdDecompressorFinished((const xrtzstddecompressor*)pHandle);
}

XZSTD_EXPORT uint64 xzstd_decompressor_members(void* pHandle)
{
	return xrtZstdDecompressorMembers((const xrtzstddecompressor*)pHandle);
}
