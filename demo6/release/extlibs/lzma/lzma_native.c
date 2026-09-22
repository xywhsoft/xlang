#define XRT_MODULE_ERROR_FORMAT
#define XRT_MODULE_FILE_TEMP
#define XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_FILE
#define XRT_MODULE_PATH
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE

#include "xruntime.h"
#include "include/xrt_lzma.h"

#if defined(_WIN32) || defined(_WIN64)
#define XLZMA_EXPORT __declspec(dllexport)
#else
#define XLZMA_EXPORT
#endif



/* 把 LZMA 模块错误映射为 XRT 通用错误类别。 */
static xerrkind xlzma_error_kind(int iCode)
{
	switch ( iCode ) {
		case XRT_LZMA_ERROR_ARGUMENT:
			return XERR_ARGUMENT;
		case XRT_LZMA_ERROR_MEMORY:
			return XERR_MEMORY;
		case XRT_LZMA_ERROR_DATA:
		case XRT_LZMA_ERROR_TRUNCATED:
			return XERR_PROTOCOL;
		case XRT_LZMA_ERROR_LIMIT:
			return XERR_RANGE;
		case XRT_LZMA_ERROR_STATE:
			return XERR_STATE;
		case XRT_LZMA_ERROR_CANCELLED:
			return XERR_CANCELLED;
		default:
			return XERR_IO;
	}
}


/* 发布可被 XLang 和 C 调用方统一读取的结构化错误。 */
static void xlzma_publish_error(const xrtlzmaerror* pError)
{
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "lzma operation failed";
	int iCode = pError != NULL ? pError->iCode : XRT_LZMA_ERROR_IO;

	xrtSetErrorInfo(xlzma_error_kind(iCode), "xlang.extlib.lzma",
		iCode, sMessage);
}


/* 用调用参数覆盖 LZMA 默认资源限制。 */
static void xlzma_options(xrtlzmaoptions* pOptions, int iLevel, uint64 iMaxInput,
	uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtLzmaOptionsInit(pOptions);
	if ( iLevel >= 0 ) { pOptions->iLevel = iLevel; }
	if ( iMaxInput != 0u ) { pOptions->tLimits.iMaxInputBytes = iMaxInput; }
	if ( iMaxOutput != 0u ) { pOptions->tLimits.iMaxOutputBytes = iMaxOutput; }
	if ( iMaxRatio != 0u ) { pOptions->tLimits.iMaxCompressionRatio = iMaxRatio; }
}


/* 删除失败事务的临时文件，同时保留原始失败原因。 */
static void xlzma_remove_temp(cstr sTemp)
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


/* 1 成功，0 处理失败，-1 用户取消，-2 回调抛错。 */
static int xlzma_transform_file(str sSource, str sDestination, bool bCompress,
	bool bOverwrite, const xrtlzmaoptions* pOptions, xvalue* pCallback)
{
	xrtlzmaerror tError;
	xrtprogresscall tProgress;
	xfile TempFile = NULL;
	xreader* pReader = NULL;
	xwriter* pWriter = NULL;
	str sDirectory = NULL;
	str sTemp = NULL;
	bool bResult = false;
	int iResult = 0;

	xrtLzmaErrorInit(&tError);
	xrtProgressCallInit(&tProgress, pCallback);
	pCallback = tProgress.Callback;
	if ( sSource == NULL || sSource[0] == 0 || sDestination == NULL || sDestination[0] == 0 ) {
		xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.lzma",
			XRT_LZMA_ERROR_ARGUMENT, "lzma source and destination paths are required");
		return 0;
	}
	if ( xrtPathExists(sDestination) && !bOverwrite ) {
		xrtSetErrorFormat(XERR_EXISTS, "xlang.extlib.lzma", XRT_LZMA_ERROR_IO,
			"lzma destination already exists: %s", sDestination);
		return 0;
	}
	pReader = xrtReaderOpen(sSource);
	if ( pReader == NULL ) { goto cleanup; }
	sDirectory = xrtPathParent(sDestination);
	if ( sDirectory == NULL ) { goto cleanup; }
	TempFile = xrtFileTemp(sDirectory[0] != 0 ? sDirectory : ".",
		".xrt-lzma-", ".tmp", &sTemp);
	if ( TempFile == NULL ) { goto cleanup; }
	pWriter = xrtWriterTakeFile(&TempFile);
	if ( pWriter == NULL ) { goto cleanup; }

	bResult = bCompress ? xrtLzmaCompressStream(pReader, pWriter, pOptions,
		pCallback != NULL ? xrtProgressCallInvoke : NULL, &tProgress, &tError) :
		xrtLzmaDecompressStream(pReader, pWriter, pOptions,
			pCallback != NULL ? xrtProgressCallInvoke : NULL, &tProgress, &tError);
	if ( !bResult && !tProgress.InvokeFailed ) { xlzma_publish_error(&tError); }
	if ( bResult && !xrtWriterFlush(pWriter) ) {
		bResult = false;
	}

cleanup:
	if ( (pWriter != NULL) && !xrtWriterDestroy(pWriter) && bResult ) { bResult = false; }
	if ( (TempFile != NULL) && !xrtClose(TempFile) && bResult ) { bResult = false; }
	if ( (pReader != NULL) && !xrtReaderDestroy(pReader) && bResult ) { bResult = false; }
	if ( bResult && !xrtPathRename(sTemp, sDestination, bOverwrite) ) { bResult = false; }
	if ( !bResult ) {
		if ( (xrtGetError() == NULL) && !tProgress.InvokeFailed &&
			(tError.iCode == XRT_LZMA_OK) ) {
			xrtSetErrorInfo(XERR_IO, "xlang.extlib.lzma",
				XRT_LZMA_ERROR_IO, "lzma file operation failed");
		}
		xlzma_remove_temp(sTemp);
	}
	xrtFree(sDirectory);
	xrtFree(sTemp);
	if ( bResult ) { iResult = 1; }
	else if ( tProgress.InvokeFailed ) { iResult = -2; }
	else if ( tError.iCode == XRT_LZMA_ERROR_CANCELLED ) { iResult = -1; }
	return iResult;
}



XLZMA_EXPORT bool xlzma_available(void)
{
	return true;
}

XLZMA_EXPORT const char* xlzma_version(void)
{
	return "lzma sdk 23.01";
}

XLZMA_EXPORT const char* xlzma_last_error(void)
{
	return xrtErrorMessage(xrtGetError());
}



XLZMA_EXPORT void* xlzma_compress(const void* pData, size_t iSize, int iLevel,
	uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtlzmaoptions tOptions;
	xrtlzmaerror tError;
	xbuffer* pOutput = NULL;
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
	xbuffer* pOutput = NULL;
	xrtClearError();
	xlzma_options(&tOptions, -1, iMaxInput, iMaxOutput, iMaxRatio);
	if ( !xrtLzmaDecompress(pData, iSize, &tOptions, &pOutput, &tError) ) {
		xlzma_publish_error(&tError);
		return NULL;
	}
	return pOutput;
}

XLZMA_EXPORT int xlzma_compress_file(str sSource, str sDestination, int iLevel,
	bool bOverwrite, uint64 iMaxInput, uint64 iMaxOutput, xvalue* pCallback)
{
	xrtlzmaoptions tOptions;
	xrtClearError();
	xlzma_options(&tOptions, iLevel, iMaxInput, iMaxOutput, 0u);
	return xlzma_transform_file(sSource, sDestination, true, bOverwrite, &tOptions, pCallback);
}

XLZMA_EXPORT int xlzma_decompress_file(str sSource, str sDestination, bool bOverwrite,
	uint64 iMaxInput, uint64 iMaxOutput, uint32 iMaxRatio, xvalue* pCallback)
{
	xrtlzmaoptions tOptions;
	xrtClearError();
	xlzma_options(&tOptions, -1, iMaxInput, iMaxOutput, iMaxRatio);
	return xlzma_transform_file(sSource, sDestination, false, bOverwrite, &tOptions, pCallback);
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
		return false;
	}
	return true;
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
		return false;
	}
	return true;
}

XLZMA_EXPORT void* xlzma_compressor_drain(void* pHandle)
{
	xrtlzmacompressor* pCompressor = (xrtlzmacompressor*)pHandle;
	xbuffer* pOutput = xrtLzmaCompressorDrainBuffer(pCompressor);
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
		return false;
	}
	return true;
}

XLZMA_EXPORT bool xlzma_decompressor_finish(void* pHandle)
{
	xrtlzmadecompressor* pDecompressor = (xrtlzmadecompressor*)pHandle;
	if ( !xrtLzmaDecompressorFinish(pDecompressor) ) {
		xlzma_publish_error(xrtLzmaDecompressorError(pDecompressor));
		return false;
	}
	return true;
}

XLZMA_EXPORT void* xlzma_decompressor_drain(void* pHandle)
{
	xrtlzmadecompressor* pDecompressor = (xrtlzmadecompressor*)pHandle;
	xbuffer* pOutput = xrtLzmaDecompressorDrainBuffer(pDecompressor);
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
