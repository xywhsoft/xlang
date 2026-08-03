#include "include/xrt_lz4.h"
#include "third_party/lz4.h"

#if defined(_WIN32) || defined(_WIN64)
#define XLZ4_EXPORT __declspec(dllexport)
#else
#define XLZ4_EXPORT
#endif



static void xlz4_publish_error(const xrtlz4error* pError)
{
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "lz4 operation failed";
	xrtSetError(xrtFormat("%s", sMessage), TRUE);
}

static void xlz4_options(xrtlz4options* pOptions, int iAcceleration, uint64 iMaxInput,
	uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtLz4OptionsInit(pOptions);
	if ( iAcceleration > 1 ) { pOptions->iAcceleration = iAcceleration; }
	if ( iMaxInput != 0u ) { pOptions->tLimits.iMaxInputBytes = iMaxInput; }
	if ( iMaxOutput != 0u ) { pOptions->tLimits.iMaxOutputBytes = iMaxOutput; }
	if ( iMaxRatio != 0u ) { pOptions->tLimits.iMaxCompressionRatio = iMaxRatio; }
}

static bool xlz4_transform_file(str sSource, str sDestination, bool bCompress,
	bool bOverwrite, const xrtlz4options* pOptions)
{
	xrtlz4error tError;
	xreader pReader = NULL;
	xwriter pWriter = NULL;
	str sTemp = NULL;
	bool bResult = FALSE;
	if ( sSource == NULL || sSource[0] == 0 || sDestination == NULL || sDestination[0] == 0 ) {
		xrtSetError(xrtFormat("lz4 source and destination paths are required"), TRUE);
		return FALSE;
	}
	if ( xrtPathExists(sDestination) && !bOverwrite ) {
		xrtSetError(xrtFormat("lz4 destination already exists: %s", sDestination), TRUE);
		return FALSE;
	}
	sTemp = xrtPathRandom(sDestination, 0u, (str)".xrt-lz4-tmp", 0u, 8u);
	pReader = xrtReaderOpen(sSource);
	pWriter = sTemp != NULL ? xrtWriterOpen(sTemp, TRUE) : NULL;
	if ( pReader == NULL || pWriter == NULL ) {
		xrtSetError(xrtFormat("cannot open lz4 file stream: %s", sSource), TRUE);
		goto cleanup;
	}
	bResult = bCompress ? xrtLz4CompressStream(pReader, pWriter, pOptions, &tError) :
		xrtLz4DecompressStream(pReader, pWriter, pOptions, &tError);
	if ( !bResult ) { xlz4_publish_error(&tError); }
	if ( bResult && !xrtWriterFlush(pWriter) ) {
		xrtSetError(xrtFormat("cannot flush lz4 destination: %s", sDestination), TRUE);
		bResult = FALSE;
	}

cleanup:
	xrtReaderDestroy(pReader);
	xrtWriterDestroy(pWriter);
	if ( bResult && !xrtFileRename(sTemp, sDestination, bOverwrite) ) {
		xrtSetError(xrtFormat("cannot commit lz4 destination: %s", sDestination), TRUE);
		bResult = FALSE;
	}
	if ( !bResult && sTemp != NULL && xrtFileExists(sTemp) ) { xrtFileDelete(sTemp); }
	xrtFree(sTemp);
	return bResult;
}



XLZ4_EXPORT bool xlz4_available(void)
{
	return TRUE;
}

XLZ4_EXPORT const char* xlz4_version(void)
{
	return LZ4_versionString();
}

XLZ4_EXPORT const char* xlz4_last_error(void)
{
	return xrtGetError();
}



XLZ4_EXPORT void* xlz4_compress(const void* pData, size_t iSize, int iAcceleration,
	uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtlz4options tOptions;
	xrtlz4error tError;
	xbuffer pOutput = NULL;
	xrtClearError();
	xlz4_options(&tOptions, iAcceleration, iMaxInput, iMaxOutput, 0u);
	if ( !xrtLz4Compress(pData, iSize, &tOptions, &pOutput, &tError) ) {
		xlz4_publish_error(&tError);
		return NULL;
	}
	return pOutput;
}

XLZ4_EXPORT void* xlz4_compress_str(const char* sText, size_t iSize, int iAcceleration,
	uint64 iMaxInput, uint64 iMaxOutput)
{
	return xlz4_compress((const void*)sText, iSize, iAcceleration, iMaxInput, iMaxOutput);
}

XLZ4_EXPORT void* xlz4_decompress(const void* pData, size_t iSize, uint64 iMaxInput,
	uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtlz4options tOptions;
	xrtlz4error tError;
	xbuffer pOutput = NULL;
	xrtClearError();
	xlz4_options(&tOptions, 0, iMaxInput, iMaxOutput, iMaxRatio);
	if ( !xrtLz4Decompress(pData, iSize, &tOptions, &pOutput, &tError) ) {
		xlz4_publish_error(&tError);
		return NULL;
	}
	return pOutput;
}

XLZ4_EXPORT bool xlz4_compress_file(str sSource, str sDestination, int iAcceleration,
	bool bOverwrite, uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtlz4options tOptions;
	xrtClearError();
	xlz4_options(&tOptions, iAcceleration, iMaxInput, iMaxOutput, 0u);
	return xlz4_transform_file(sSource, sDestination, TRUE, bOverwrite, &tOptions);
}

XLZ4_EXPORT bool xlz4_decompress_file(str sSource, str sDestination, bool bOverwrite,
	uint64 iMaxInput, uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtlz4options tOptions;
	xrtClearError();
	xlz4_options(&tOptions, 0, iMaxInput, iMaxOutput, iMaxRatio);
	return xlz4_transform_file(sSource, sDestination, FALSE, bOverwrite, &tOptions);
}



XLZ4_EXPORT void* xlz4_compressor_create(int iAcceleration, uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtlz4options tOptions;
	xrtlz4compressor* pCompressor;
	xrtClearError();
	xlz4_options(&tOptions, iAcceleration, iMaxInput, iMaxOutput, 0u);
	pCompressor = xrtLz4CompressorCreate(&tOptions);
	if ( pCompressor == NULL || xrtLz4CompressorError(pCompressor)->iCode != XRT_LZ4_OK ) {
		xlz4_publish_error(xrtLz4CompressorError(pCompressor));
		xrtLz4CompressorDestroy(pCompressor);
		return NULL;
	}
	return pCompressor;
}

XLZ4_EXPORT void xlz4_compressor_destroy(void* pHandle)
{
	xrtLz4CompressorDestroy((xrtlz4compressor*)pHandle);
}

XLZ4_EXPORT bool xlz4_compressor_update(void* pHandle, const void* pData, size_t iSize)
{
	xrtlz4compressor* pCompressor = (xrtlz4compressor*)pHandle;
	if ( !xrtLz4CompressorUpdate(pCompressor, pData, iSize) ) {
		xlz4_publish_error(xrtLz4CompressorError(pCompressor));
		return FALSE;
	}
	return TRUE;
}

XLZ4_EXPORT bool xlz4_compressor_update_str(void* pHandle, const char* sText, size_t iSize)
{
	return xlz4_compressor_update(pHandle, (const void*)sText, iSize);
}

XLZ4_EXPORT bool xlz4_compressor_finish(void* pHandle)
{
	xrtlz4compressor* pCompressor = (xrtlz4compressor*)pHandle;
	if ( !xrtLz4CompressorFinish(pCompressor) ) {
		xlz4_publish_error(xrtLz4CompressorError(pCompressor));
		return FALSE;
	}
	return TRUE;
}

XLZ4_EXPORT void* xlz4_compressor_drain(void* pHandle)
{
	xrtlz4compressor* pCompressor = (xrtlz4compressor*)pHandle;
	xbuffer pOutput = xrtLz4CompressorDrainBuffer(pCompressor);
	if ( pOutput == NULL ) {
		xlz4_publish_error(xrtLz4CompressorError(pCompressor));
		return NULL;
	}
	return pOutput;
}

XLZ4_EXPORT uint64 xlz4_compressor_input_bytes(void* pHandle)
{
	return xrtLz4CompressorInputBytes((const xrtlz4compressor*)pHandle);
}

XLZ4_EXPORT uint64 xlz4_compressor_output_bytes(void* pHandle)
{
	return xrtLz4CompressorOutputBytes((const xrtlz4compressor*)pHandle);
}



XLZ4_EXPORT void* xlz4_decompressor_create(uint64 iMaxInput, uint64 iMaxOutput,
	uint32 iMaxRatio)
{
	xrtlz4options tOptions;
	xrtlz4decompressor* pDecompressor;
	xrtClearError();
	xlz4_options(&tOptions, 0, iMaxInput, iMaxOutput, iMaxRatio);
	pDecompressor = xrtLz4DecompressorCreate(&tOptions);
	if ( pDecompressor == NULL || xrtLz4DecompressorError(pDecompressor)->iCode != XRT_LZ4_OK ) {
		xlz4_publish_error(xrtLz4DecompressorError(pDecompressor));
		xrtLz4DecompressorDestroy(pDecompressor);
		return NULL;
	}
	return pDecompressor;
}

XLZ4_EXPORT void xlz4_decompressor_destroy(void* pHandle)
{
	xrtLz4DecompressorDestroy((xrtlz4decompressor*)pHandle);
}

XLZ4_EXPORT bool xlz4_decompressor_update(void* pHandle, const void* pData, size_t iSize)
{
	xrtlz4decompressor* pDecompressor = (xrtlz4decompressor*)pHandle;
	if ( !xrtLz4DecompressorUpdate(pDecompressor, pData, iSize) ) {
		xlz4_publish_error(xrtLz4DecompressorError(pDecompressor));
		return FALSE;
	}
	return TRUE;
}

XLZ4_EXPORT bool xlz4_decompressor_finish(void* pHandle)
{
	xrtlz4decompressor* pDecompressor = (xrtlz4decompressor*)pHandle;
	if ( !xrtLz4DecompressorFinish(pDecompressor) ) {
		xlz4_publish_error(xrtLz4DecompressorError(pDecompressor));
		return FALSE;
	}
	return TRUE;
}

XLZ4_EXPORT void* xlz4_decompressor_drain(void* pHandle)
{
	xrtlz4decompressor* pDecompressor = (xrtlz4decompressor*)pHandle;
	xbuffer pOutput = xrtLz4DecompressorDrainBuffer(pDecompressor);
	if ( pOutput == NULL ) {
		xlz4_publish_error(xrtLz4DecompressorError(pDecompressor));
		return NULL;
	}
	return pOutput;
}

XLZ4_EXPORT uint64 xlz4_decompressor_input_bytes(void* pHandle)
{
	return xrtLz4DecompressorInputBytes((const xrtlz4decompressor*)pHandle);
}

XLZ4_EXPORT uint64 xlz4_decompressor_output_bytes(void* pHandle)
{
	return xrtLz4DecompressorOutputBytes((const xrtlz4decompressor*)pHandle);
}

XLZ4_EXPORT bool xlz4_decompressor_finished(void* pHandle)
{
	return xrtLz4DecompressorFinished((const xrtlz4decompressor*)pHandle);
}

XLZ4_EXPORT uint64 xlz4_decompressor_members(void* pHandle)
{
	return xrtLz4DecompressorMembers((const xrtlz4decompressor*)pHandle);
}
