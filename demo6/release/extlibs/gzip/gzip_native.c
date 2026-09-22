#define XRT_MODULE_ERROR_FORMAT
#define XRT_MODULE_FILE_TEMP
#define XRT_MODULE_IO_FILE
#define XRT_MODULE_PATH

#include "include/xrt_gzip.h"

#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#define XGZIP_EXPORT __declspec(dllexport)
#else
#define XGZIP_EXPORT
#endif



/* 把 gzip 模块错误映射为 XRT 通用错误类别。 */
static xerrkind xgzip_error_kind(int iCode)
{
	switch ( iCode ) {
		case XRT_GZIP_ERROR_ARGUMENT:
			return XERR_ARGUMENT;
		case XRT_GZIP_ERROR_MEMORY:
			return XERR_MEMORY;
		case XRT_GZIP_ERROR_DATA:
		case XRT_GZIP_ERROR_TRUNCATED:
			return XERR_PROTOCOL;
		case XRT_GZIP_ERROR_LIMIT:
			return XERR_RANGE;
		case XRT_GZIP_ERROR_STATE:
			return XERR_STATE;
		default:
			return XERR_IO;
	}
}


/* 发布可被 XLang 和 C 调用方统一读取的结构化错误。 */
static void xgzip_publish_error(const xrtgziperror* pError)
{
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "gzip operation failed";
	int iCode = pError != NULL ? pError->iCode : XRT_GZIP_ERROR_IO;

	xrtSetErrorInfo(
		xgzip_error_kind(iCode),
		"xlang.extlib.gzip",
		iCode,
		sMessage
	);
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


/* 删除失败事务的临时文件，同时保留原始失败原因。 */
static void xgzip_remove_temp(cstr sTemp)
{
	xerror* pError;

	if ( sTemp == NULL ) { return; }
	pError = xrtTakeError();
	if ( xrtFileExists(sTemp) ) { (void)xrtFileDelete(sTemp); }
	xrtClearError();
	if ( pError != NULL ) {
		xrtSetError(pError);
		xrtErrorFree(pError);
	}
}

static bool xgzip_transform_file(str sSource, str sDestination, bool bCompress,
	bool bOverwrite, const xrtgzipoptions* pOptions)
{
	xrtgziperror tError;
	xfile TempFile = NULL;
	xreader* pReader = NULL;
	xwriter* pWriter = NULL;
	str sDirectory = NULL;
	str sTemp = NULL;
	bool bResult = false;

	xrtGzipErrorInit(&tError);
	if ( sSource == NULL || sSource[0] == 0 || sDestination == NULL || sDestination[0] == 0 ) {
		xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.gzip",
			XRT_GZIP_ERROR_ARGUMENT, "gzip source and destination paths are required");
		return false;
	}
	if ( xrtPathExists(sDestination) && !bOverwrite ) {
		xrtSetErrorFormat(XERR_EXISTS, "xlang.extlib.gzip", XRT_GZIP_ERROR_IO,
			"gzip destination already exists: %s", sDestination);
		return false;
	}
	pReader = xrtReaderOpen(sSource);
	if ( pReader == NULL ) { goto cleanup; }
	sDirectory = xrtPathParent(sDestination);
	if ( sDirectory == NULL ) { goto cleanup; }
	TempFile = xrtFileTemp(sDirectory[0] != 0 ? sDirectory : ".",
		".xrt-gzip-", ".tmp", &sTemp);
	if ( TempFile == NULL ) { goto cleanup; }
	pWriter = xrtWriterTakeFile(&TempFile);
	if ( pWriter == NULL ) { goto cleanup; }

	bResult = bCompress ? xrtGzipCompressStream(pReader, pWriter, pOptions, &tError) :
		xrtGzipDecompressStream(pReader, pWriter, pOptions, &tError);
	if ( !bResult ) { xgzip_publish_error(&tError); }
	if ( bResult && !xrtWriterFlush(pWriter) ) {
		bResult = false;
	}

cleanup:
	if ( (pWriter != NULL) && !xrtWriterDestroy(pWriter) && bResult ) { bResult = false; }
	if ( (TempFile != NULL) && !xrtClose(TempFile) && bResult ) { bResult = false; }
	if ( (pReader != NULL) && !xrtReaderDestroy(pReader) && bResult ) { bResult = false; }
	if ( bResult && !xrtPathRename(sTemp, sDestination, bOverwrite) ) { bResult = false; }
	if ( !bResult ) {
		if ( (xrtGetError() == NULL) && (tError.iCode == XRT_GZIP_OK) ) {
			xrtSetErrorInfo(XERR_IO, "xlang.extlib.gzip",
				XRT_GZIP_ERROR_IO, "gzip file operation failed");
		}
		xgzip_remove_temp(sTemp);
	}
	xrtFree(sDirectory);
	xrtFree(sTemp);
	return bResult;
}



XGZIP_EXPORT const char* xgzip_last_error(void)
{
	return xrtErrorMessage(xrtGetError());
}



XGZIP_EXPORT void* xgzip_compress(const void* pData, size_t iSize, int iLevel,
	uint32 iMTime, uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtgzipoptions tOptions;
	xrtgziperror tError;
	xbuffer* pOutput = NULL;
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
	xbuffer* pOutput = NULL;
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
	return xgzip_transform_file(sSource, sDestination, true, bOverwrite, &tOptions);
}

XGZIP_EXPORT bool xgzip_decompress_file(str sSource, str sDestination,
	bool bOverwrite, uint64 iMaxInput, uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtgzipoptions tOptions;
	xrtClearError();
	xgzip_options(&tOptions, -1, 0u, iMaxInput, iMaxOutput, iMaxRatio);
	return xgzip_transform_file(sSource, sDestination, false, bOverwrite, &tOptions);
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
		return false;
	}
	return true;
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
		return false;
	}
	return true;
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
