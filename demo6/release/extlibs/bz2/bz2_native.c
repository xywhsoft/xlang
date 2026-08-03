#include "include/xrt_bz2.h"

#if defined(_WIN32) || defined(_WIN64)
#define XBZ2_EXPORT __declspec(dllexport)
#else
#define XBZ2_EXPORT
#endif



static void xbz2_publish_error(const xrtbz2error* pError)
{
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "bz2 operation failed";
	xrtSetError(xrtFormat("%s", sMessage), TRUE);
}

static void xbz2_options(xrtbz2options* pOptions, int iLevel, bool bSmallMemory,
	uint64 iMaxInput, uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtBz2OptionsInit(pOptions);
	pOptions->iLevel = iLevel;
	pOptions->bSmallMemory = bSmallMemory;
	if ( iMaxInput != 0u ) { pOptions->tLimits.iMaxInputBytes = iMaxInput; }
	if ( iMaxOutput != 0u ) { pOptions->tLimits.iMaxOutputBytes = iMaxOutput; }
	if ( iMaxRatio != 0u ) { pOptions->tLimits.iMaxCompressionRatio = iMaxRatio; }
}

static void xbz2_options_wf(xrtbz2options* pOptions, int iLevel, int iWorkFactor,
	uint64 iMaxInput, uint64 iMaxOutput)
{
	xbz2_options(pOptions, iLevel, FALSE, iMaxInput, iMaxOutput, 0u);
	if ( iWorkFactor != 0 ) { pOptions->iWorkFactor = iWorkFactor; }
}

static bool xbz2_transform_file(str sSource, str sDestination, bool bCompress,
	bool bOverwrite, const xrtbz2options* pOptions)
{
	xrtbz2error tError;
	xreader pReader = NULL;
	xwriter pWriter = NULL;
	str sTemp = NULL;
	bool bResult = FALSE;
	if ( sSource == NULL || sSource[0] == 0 || sDestination == NULL || sDestination[0] == 0 ) {
		xrtSetError(xrtFormat("bz2 source and destination paths are required"), TRUE);
		return FALSE;
	}
	if ( xrtPathExists(sDestination) && !bOverwrite ) {
		xrtSetError(xrtFormat("bz2 destination already exists: %s", sDestination), TRUE);
		return FALSE;
	}
	sTemp = xrtPathRandom(sDestination, 0u, (str)".xrt-bz2-tmp", 0u, 8u);
	pReader = xrtReaderOpen(sSource);
	pWriter = sTemp != NULL ? xrtWriterOpen(sTemp, TRUE) : NULL;
	if ( pReader == NULL || pWriter == NULL ) {
		xrtSetError(xrtFormat("cannot open bz2 file stream: %s", sSource), TRUE);
		goto cleanup;
	}
	bResult = bCompress ? xrtBz2CompressStream(pReader, pWriter, pOptions, &tError) :
		xrtBz2DecompressStream(pReader, pWriter, pOptions, &tError);
	if ( !bResult ) { xbz2_publish_error(&tError); }
	if ( bResult && !xrtWriterFlush(pWriter) ) {
		xrtSetError(xrtFormat("cannot flush bz2 destination: %s", sDestination), TRUE);
		bResult = FALSE;
	}

cleanup:
	xrtReaderDestroy(pReader);
	xrtWriterDestroy(pWriter);
	if ( bResult && !xrtFileRename(sTemp, sDestination, bOverwrite) ) {
		xrtSetError(xrtFormat("cannot commit bz2 destination: %s", sDestination), TRUE);
		bResult = FALSE;
	}
	if ( !bResult && sTemp != NULL && xrtFileExists(sTemp) ) { xrtFileDelete(sTemp); }
	xrtFree(sTemp);
	return bResult;
}



XBZ2_EXPORT void* xbz2_compress(const void* pData, size_t iSize, int iLevel,
	uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtbz2options tOptions;
	xrtbz2error tError;
	xbuffer pOutput = NULL;
	xrtClearError();
	xbz2_options(&tOptions, iLevel, FALSE, iMaxInput, iMaxOutput, 0u);
	if ( !xrtBz2Compress(pData, iSize, &tOptions, &pOutput, &tError) ) {
		xbz2_publish_error(&tError);
		return NULL;
	}
	return pOutput;
}

XBZ2_EXPORT void* xbz2_compress_str(const char* sText, size_t iSize, int iLevel,
	uint64 iMaxInput, uint64 iMaxOutput)
{
	return xbz2_compress((const void*)sText, iSize, iLevel, iMaxInput, iMaxOutput);
}

XBZ2_EXPORT void* xbz2_decompress(const void* pData, size_t iSize, bool bSmallMemory,
	uint64 iMaxInput, uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtbz2options tOptions;
	xrtbz2error tError;
	xbuffer pOutput = NULL;
	xrtClearError();
	xbz2_options(&tOptions, 9, bSmallMemory, iMaxInput, iMaxOutput, iMaxRatio);
	if ( !xrtBz2Decompress(pData, iSize, &tOptions, &pOutput, &tError) ) {
		xbz2_publish_error(&tError);
		return NULL;
	}
	return pOutput;
}

XBZ2_EXPORT bool xbz2_compress_file(str sSource, str sDestination, int iLevel,
	bool bOverwrite, uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtbz2options tOptions;
	xrtClearError();
	xbz2_options(&tOptions, iLevel, FALSE, iMaxInput, iMaxOutput, 0u);
	return xbz2_transform_file(sSource, sDestination, TRUE, bOverwrite, &tOptions);
}

XBZ2_EXPORT bool xbz2_decompress_file(str sSource, str sDestination,
	bool bSmallMemory, bool bOverwrite, uint64 iMaxInput, uint64 iMaxOutput,
	uint32 iMaxRatio)
{
	xrtbz2options tOptions;
	xrtClearError();
	xbz2_options(&tOptions, 9, bSmallMemory, iMaxInput, iMaxOutput, iMaxRatio);
	return xbz2_transform_file(sSource, sDestination, FALSE, bOverwrite, &tOptions);
}



XBZ2_EXPORT void* xbz2_compressor_create(int iLevel, int iWorkFactor,
	uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtbz2options tOptions;
	xrtbz2compressor* pCompressor;
	xrtClearError();
	xbz2_options_wf(&tOptions, iLevel, iWorkFactor, iMaxInput, iMaxOutput);
	pCompressor = xrtBz2CompressorCreate(&tOptions);
	if ( pCompressor == NULL || xrtBz2CompressorError(pCompressor)->iCode != XRT_BZ2_OK ) {
		xbz2_publish_error(xrtBz2CompressorError(pCompressor));
		xrtBz2CompressorDestroy(pCompressor);
		return NULL;
	}
	return pCompressor;
}

XBZ2_EXPORT void xbz2_compressor_destroy(void* pHandle)
{
	xrtBz2CompressorDestroy((xrtbz2compressor*)pHandle);
}

XBZ2_EXPORT bool xbz2_compressor_update(void* pHandle, const void* pData, size_t iSize)
{
	xrtbz2compressor* pCompressor = (xrtbz2compressor*)pHandle;
	if ( !xrtBz2CompressorUpdate(pCompressor, pData, iSize) ) {
		xbz2_publish_error(xrtBz2CompressorError(pCompressor));
		return FALSE;
	}
	return TRUE;
}

XBZ2_EXPORT bool xbz2_compressor_update_str(void* pHandle, const char* sText, size_t iSize)
{
	return xbz2_compressor_update(pHandle, (const void*)sText, iSize);
}

XBZ2_EXPORT bool xbz2_compressor_finish(void* pHandle)
{
	xrtbz2compressor* pCompressor = (xrtbz2compressor*)pHandle;
	if ( !xrtBz2CompressorFinish(pCompressor) ) {
		xbz2_publish_error(xrtBz2CompressorError(pCompressor));
		return FALSE;
	}
	return TRUE;
}

XBZ2_EXPORT void* xbz2_compressor_drain(void* pHandle)
{
	xrtbz2compressor* pCompressor = (xrtbz2compressor*)pHandle;
	xbuffer pOutput = xrtBz2CompressorDrainBuffer(pCompressor);
	if ( pOutput == NULL ) {
		xbz2_publish_error(xrtBz2CompressorError(pCompressor));
		return NULL;
	}
	return pOutput;
}

XBZ2_EXPORT uint64 xbz2_compressor_input_bytes(void* pHandle)
{
	return xrtBz2CompressorInputBytes((const xrtbz2compressor*)pHandle);
}

XBZ2_EXPORT uint64 xbz2_compressor_output_bytes(void* pHandle)
{
	return xrtBz2CompressorOutputBytes((const xrtbz2compressor*)pHandle);
}



XBZ2_EXPORT void* xbz2_decompressor_create(bool bSmallMemory, uint64 iMaxInput,
	uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtbz2options tOptions;
	xrtbz2decompressor* pDecompressor;
	xrtClearError();
	xbz2_options(&tOptions, 9, bSmallMemory, iMaxInput, iMaxOutput, iMaxRatio);
	pDecompressor = xrtBz2DecompressorCreate(&tOptions);
	if ( pDecompressor == NULL || xrtBz2DecompressorError(pDecompressor)->iCode != XRT_BZ2_OK ) {
		xbz2_publish_error(xrtBz2DecompressorError(pDecompressor));
		xrtBz2DecompressorDestroy(pDecompressor);
		return NULL;
	}
	return pDecompressor;
}

XBZ2_EXPORT void xbz2_decompressor_destroy(void* pHandle)
{
	xrtBz2DecompressorDestroy((xrtbz2decompressor*)pHandle);
}

XBZ2_EXPORT bool xbz2_decompressor_update(void* pHandle, const void* pData, size_t iSize)
{
	xrtbz2decompressor* pDecompressor = (xrtbz2decompressor*)pHandle;
	if ( !xrtBz2DecompressorUpdate(pDecompressor, pData, iSize) ) {
		xbz2_publish_error(xrtBz2DecompressorError(pDecompressor));
		return FALSE;
	}
	return TRUE;
}

XBZ2_EXPORT bool xbz2_decompressor_finish(void* pHandle)
{
	xrtbz2decompressor* pDecompressor = (xrtbz2decompressor*)pHandle;
	if ( !xrtBz2DecompressorFinish(pDecompressor) ) {
		xbz2_publish_error(xrtBz2DecompressorError(pDecompressor));
		return FALSE;
	}
	return TRUE;
}

XBZ2_EXPORT void* xbz2_decompressor_drain(void* pHandle)
{
	xrtbz2decompressor* pDecompressor = (xrtbz2decompressor*)pHandle;
	xbuffer pOutput = xrtBz2DecompressorDrainBuffer(pDecompressor);
	if ( pOutput == NULL ) {
		xbz2_publish_error(xrtBz2DecompressorError(pDecompressor));
		return NULL;
	}
	return pOutput;
}

XBZ2_EXPORT uint64 xbz2_decompressor_input_bytes(void* pHandle)
{
	return xrtBz2DecompressorInputBytes((const xrtbz2decompressor*)pHandle);
}

XBZ2_EXPORT uint64 xbz2_decompressor_output_bytes(void* pHandle)
{
	return xrtBz2DecompressorOutputBytes((const xrtbz2decompressor*)pHandle);
}

XBZ2_EXPORT bool xbz2_decompressor_finished(void* pHandle)
{
	return xrtBz2DecompressorFinished((const xrtbz2decompressor*)pHandle);
}

XBZ2_EXPORT uint64 xbz2_decompressor_members(void* pHandle)
{
	return xrtBz2DecompressorMembers((const xrtbz2decompressor*)pHandle);
}
