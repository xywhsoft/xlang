#include "include/xrt_brotli.h"
#include "third_party/include/brotli/encode.h"
#include "third_party/include/brotli/decode.h"

#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
#define XBROTLI_EXPORT __declspec(dllexport)
#else
#define XBROTLI_EXPORT
#endif



static void xbrotli_publish_error(const xrtbrotlierror* pError)
{
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "brotli operation failed";
	xrtSetError(xrtFormat("%s", sMessage), TRUE);
}

static void xbrotli_options(xrtbrotlioptions* pOptions, int iQuality, int iWindow,
	int iMode, uint64 iMaxInput, uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtBrotliOptionsInit(pOptions);
	if ( iQuality >= 0 ) { pOptions->iQuality = iQuality; }
	if ( iWindow >= 0 ) { pOptions->iWindow = iWindow; }
	if ( iMode >= 0 ) { pOptions->iMode = iMode; }
	if ( iMaxInput != 0u ) { pOptions->tLimits.iMaxInputBytes = iMaxInput; }
	if ( iMaxOutput != 0u ) { pOptions->tLimits.iMaxOutputBytes = iMaxOutput; }
	if ( iMaxRatio != 0u ) { pOptions->tLimits.iMaxCompressionRatio = iMaxRatio; }
}

static bool xbrotli_transform_file(str sSource, str sDestination, bool bCompress,
	bool bOverwrite, const xrtbrotlioptions* pOptions)
{
	xrtbrotlierror tError;
	xreader pReader = NULL;
	xwriter pWriter = NULL;
	str sTemp = NULL;
	bool bResult = FALSE;
	if ( sSource == NULL || sSource[0] == 0 || sDestination == NULL || sDestination[0] == 0 ) {
		xrtSetError(xrtFormat("brotli source and destination paths are required"), TRUE);
		return FALSE;
	}
	if ( xrtPathExists(sDestination) && !bOverwrite ) {
		xrtSetError(xrtFormat("brotli destination already exists: %s", sDestination), TRUE);
		return FALSE;
	}
	sTemp = xrtPathRandom(sDestination, 0u, (str)".xrt-brotli-tmp", 0u, 8u);
	pReader = xrtReaderOpen(sSource);
	pWriter = sTemp != NULL ? xrtWriterOpen(sTemp, TRUE) : NULL;
	if ( pReader == NULL || pWriter == NULL ) {
		xrtSetError(xrtFormat("cannot open brotli file stream: %s", sSource), TRUE);
		goto cleanup;
	}
	bResult = bCompress ? xrtBrotliCompressStream(pReader, pWriter, pOptions, &tError) :
		xrtBrotliDecompressStream(pReader, pWriter, pOptions, &tError);
	if ( !bResult ) { xbrotli_publish_error(&tError); }
	if ( bResult && !xrtWriterFlush(pWriter) ) {
		xrtSetError(xrtFormat("cannot flush brotli destination: %s", sDestination), TRUE);
		bResult = FALSE;
	}

cleanup:
	xrtReaderDestroy(pReader);
	xrtWriterDestroy(pWriter);
	if ( bResult && !xrtFileRename(sTemp, sDestination, bOverwrite) ) {
		xrtSetError(xrtFormat("cannot commit brotli destination: %s", sDestination), TRUE);
		bResult = FALSE;
	}
	if ( !bResult && sTemp != NULL && xrtFileExists(sTemp) ) { xrtFileDelete(sTemp); }
	xrtFree(sTemp);
	return bResult;
}

/* 将 uint32 版本号 0x01010000 格式化为 "主.次.补丁" 静态字符串。 */
static const char* xbrotli_format_version(uint32 iVersion)
{
	static char sVersion[32];
	snprintf(sVersion, sizeof(sVersion), "%u.%u.%u",
		(iVersion >> 24) & 0xffu, (iVersion >> 12) & 0xfffu, iVersion & 0xfffu);
	sVersion[sizeof(sVersion) - 1] = '\0';
	return sVersion;
}



XBROTLI_EXPORT bool xbrotli_available(void)
{
	return TRUE;
}

XBROTLI_EXPORT const char* xbrotli_version(void)
{
	return xbrotli_format_version(BrotliEncoderVersion());
}

XBROTLI_EXPORT const char* xbrotli_last_error(void)
{
	return xrtGetError();
}



XBROTLI_EXPORT void* xbrotli_compress(const void* pData, size_t iSize, int iQuality,
	int iWindow, int iMode, uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtbrotlioptions tOptions;
	xrtbrotlierror tError;
	xbuffer pOutput = NULL;
	xrtClearError();
	xbrotli_options(&tOptions, iQuality, iWindow, iMode, iMaxInput, iMaxOutput, 0u);
	if ( !xrtBrotliCompress(pData, iSize, &tOptions, &pOutput, &tError) ) {
		xbrotli_publish_error(&tError);
		return NULL;
	}
	return pOutput;
}

XBROTLI_EXPORT void* xbrotli_compress_str(const char* sText, size_t iSize, int iQuality,
	int iWindow, int iMode, uint64 iMaxInput, uint64 iMaxOutput)
{
	return xbrotli_compress((const void*)sText, iSize, iQuality, iWindow, iMode, iMaxInput, iMaxOutput);
}

XBROTLI_EXPORT void* xbrotli_decompress(const void* pData, size_t iSize, uint64 iMaxInput,
	uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtbrotlioptions tOptions;
	xrtbrotlierror tError;
	xbuffer pOutput = NULL;
	xrtClearError();
	xbrotli_options(&tOptions, -1, -1, -1, iMaxInput, iMaxOutput, iMaxRatio);
	if ( !xrtBrotliDecompress(pData, iSize, &tOptions, &pOutput, &tError) ) {
		xbrotli_publish_error(&tError);
		return NULL;
	}
	return pOutput;
}

XBROTLI_EXPORT bool xbrotli_compress_file(str sSource, str sDestination, int iQuality,
	int iWindow, int iMode, bool bOverwrite, uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtbrotlioptions tOptions;
	xrtClearError();
	xbrotli_options(&tOptions, iQuality, iWindow, iMode, iMaxInput, iMaxOutput, 0u);
	return xbrotli_transform_file(sSource, sDestination, TRUE, bOverwrite, &tOptions);
}

XBROTLI_EXPORT bool xbrotli_decompress_file(str sSource, str sDestination, bool bOverwrite,
	uint64 iMaxInput, uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtbrotlioptions tOptions;
	xrtClearError();
	xbrotli_options(&tOptions, -1, -1, -1, iMaxInput, iMaxOutput, iMaxRatio);
	return xbrotli_transform_file(sSource, sDestination, FALSE, bOverwrite, &tOptions);
}



XBROTLI_EXPORT void* xbrotli_compressor_create(int iQuality, int iWindow, int iMode,
	uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtbrotlioptions tOptions;
	xrtbrotlicompressor* pCompressor;
	xrtClearError();
	xbrotli_options(&tOptions, iQuality, iWindow, iMode, iMaxInput, iMaxOutput, 0u);
	pCompressor = xrtBrotliCompressorCreate(&tOptions);
	if ( pCompressor == NULL || xrtBrotliCompressorError(pCompressor)->iCode != XRT_BROTLI_OK ) {
		xbrotli_publish_error(xrtBrotliCompressorError(pCompressor));
		xrtBrotliCompressorDestroy(pCompressor);
		return NULL;
	}
	return pCompressor;
}

XBROTLI_EXPORT void xbrotli_compressor_destroy(void* pHandle)
{
	xrtBrotliCompressorDestroy((xrtbrotlicompressor*)pHandle);
}

XBROTLI_EXPORT bool xbrotli_compressor_update(void* pHandle, const void* pData, size_t iSize)
{
	xrtbrotlicompressor* pCompressor = (xrtbrotlicompressor*)pHandle;
	if ( !xrtBrotliCompressorUpdate(pCompressor, pData, iSize) ) {
		xbrotli_publish_error(xrtBrotliCompressorError(pCompressor));
		return FALSE;
	}
	return TRUE;
}

XBROTLI_EXPORT bool xbrotli_compressor_update_str(void* pHandle, const char* sText, size_t iSize)
{
	return xbrotli_compressor_update(pHandle, (const void*)sText, iSize);
}

XBROTLI_EXPORT bool xbrotli_compressor_finish(void* pHandle)
{
	xrtbrotlicompressor* pCompressor = (xrtbrotlicompressor*)pHandle;
	if ( !xrtBrotliCompressorFinish(pCompressor) ) {
		xbrotli_publish_error(xrtBrotliCompressorError(pCompressor));
		return FALSE;
	}
	return TRUE;
}

XBROTLI_EXPORT void* xbrotli_compressor_drain(void* pHandle)
{
	xrtbrotlicompressor* pCompressor = (xrtbrotlicompressor*)pHandle;
	xbuffer pOutput = xrtBrotliCompressorDrainBuffer(pCompressor);
	if ( pOutput == NULL ) {
		xbrotli_publish_error(xrtBrotliCompressorError(pCompressor));
		return NULL;
	}
	return pOutput;
}

XBROTLI_EXPORT uint64 xbrotli_compressor_input_bytes(void* pHandle)
{
	return xrtBrotliCompressorInputBytes((const xrtbrotlicompressor*)pHandle);
}

XBROTLI_EXPORT uint64 xbrotli_compressor_output_bytes(void* pHandle)
{
	return xrtBrotliCompressorOutputBytes((const xrtbrotlicompressor*)pHandle);
}



XBROTLI_EXPORT void* xbrotli_decompressor_create(uint64 iMaxInput, uint64 iMaxOutput,
	uint32 iMaxRatio)
{
	xrtbrotlioptions tOptions;
	xrtbrotlidecompressor* pDecompressor;
	xrtClearError();
	xbrotli_options(&tOptions, -1, -1, -1, iMaxInput, iMaxOutput, iMaxRatio);
	pDecompressor = xrtBrotliDecompressorCreate(&tOptions);
	if ( pDecompressor == NULL || xrtBrotliDecompressorError(pDecompressor)->iCode != XRT_BROTLI_OK ) {
		xbrotli_publish_error(xrtBrotliDecompressorError(pDecompressor));
		xrtBrotliDecompressorDestroy(pDecompressor);
		return NULL;
	}
	return pDecompressor;
}

XBROTLI_EXPORT void xbrotli_decompressor_destroy(void* pHandle)
{
	xrtBrotliDecompressorDestroy((xrtbrotlidecompressor*)pHandle);
}

XBROTLI_EXPORT bool xbrotli_decompressor_update(void* pHandle, const void* pData, size_t iSize)
{
	xrtbrotlidecompressor* pDecompressor = (xrtbrotlidecompressor*)pHandle;
	if ( !xrtBrotliDecompressorUpdate(pDecompressor, pData, iSize) ) {
		xbrotli_publish_error(xrtBrotliDecompressorError(pDecompressor));
		return FALSE;
	}
	return TRUE;
}

XBROTLI_EXPORT bool xbrotli_decompressor_finish(void* pHandle)
{
	xrtbrotlidecompressor* pDecompressor = (xrtbrotlidecompressor*)pHandle;
	if ( !xrtBrotliDecompressorFinish(pDecompressor) ) {
		xbrotli_publish_error(xrtBrotliDecompressorError(pDecompressor));
		return FALSE;
	}
	return TRUE;
}

XBROTLI_EXPORT void* xbrotli_decompressor_drain(void* pHandle)
{
	xrtbrotlidecompressor* pDecompressor = (xrtbrotlidecompressor*)pHandle;
	xbuffer pOutput = xrtBrotliDecompressorDrainBuffer(pDecompressor);
	if ( pOutput == NULL ) {
		xbrotli_publish_error(xrtBrotliDecompressorError(pDecompressor));
		return NULL;
	}
	return pOutput;
}

XBROTLI_EXPORT uint64 xbrotli_decompressor_input_bytes(void* pHandle)
{
	return xrtBrotliDecompressorInputBytes((const xrtbrotlidecompressor*)pHandle);
}

XBROTLI_EXPORT uint64 xbrotli_decompressor_output_bytes(void* pHandle)
{
	return xrtBrotliDecompressorOutputBytes((const xrtbrotlidecompressor*)pHandle);
}

XBROTLI_EXPORT bool xbrotli_decompressor_finished(void* pHandle)
{
	return xrtBrotliDecompressorFinished((const xrtbrotlidecompressor*)pHandle);
}

XBROTLI_EXPORT uint64 xbrotli_decompressor_members(void* pHandle)
{
	return xrtBrotliDecompressorMembers((const xrtbrotlidecompressor*)pHandle);
}
