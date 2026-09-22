#define XRT_MODULE_ERROR_FORMAT
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE

#include "xruntime.h"
#include "include/xrt_miniz.h"

#if defined(_WIN32) || defined(_WIN64)
#define XMINIZ_EXPORT __declspec(dllexport)
#else
#define XMINIZ_EXPORT
#endif



/* 把 miniz 模块错误映射为 XRT 通用错误类别。 */
static xerrkind xminiz_error_kind(int iCode)
{
	switch ( iCode ) {
		case XRT_MINIZ_ERROR_ARGUMENT:
			return XERR_ARGUMENT;
		case XRT_MINIZ_ERROR_MEMORY:
			return XERR_MEMORY;
		case XRT_MINIZ_ERROR_DATA:
		case XRT_MINIZ_ERROR_TRUNCATED:
			return XERR_PROTOCOL;
		case XRT_MINIZ_ERROR_LIMIT:
			return XERR_RANGE;
		case XRT_MINIZ_ERROR_STATE:
			return XERR_STATE;
		case XRT_MINIZ_ERROR_CANCELLED:
			return XERR_CANCELLED;
		default:
			return XERR_IO;
	}
}


/* 发布可被 XLang 和 C 调用方统一读取的结构化错误。 */
static void xminiz_publish_error(const xrtminizerror* pError)
{
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "miniz operation failed";
	int iCode = pError != NULL ? pError->iCode : XRT_MINIZ_ERROR_IO;

	xrtSetErrorInfo(
		xminiz_error_kind(iCode),
		"xlang.extlib.miniz",
		iCode,
		sMessage
	);
}


/* 用调用参数覆盖 miniz 默认资源限制。 */
static void xminiz_options(xrtminizoptions* pOptions, int iLevel, uint64 iMaxInput,
	uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtMinizOptionsInit(pOptions);
	if ( iLevel >= 0 ) { pOptions->iLevel = iLevel; }
	if ( iMaxInput != 0u ) { pOptions->tLimits.iMaxInputBytes = iMaxInput; }
	if ( iMaxOutput != 0u ) { pOptions->tLimits.iMaxOutputBytes = iMaxOutput; }
	if ( iMaxRatio != 0u ) { pOptions->tLimits.iMaxCompressionRatio = iMaxRatio; }
}


/* 删除失败事务的临时文件，同时保留原始失败原因。 */
static void xminiz_remove_temp(cstr sTemp)
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
static int xminiz_transform_file(str sSource, str sDestination, bool bCompress,
	bool bOverwrite, const xrtminizoptions* pOptions, xvalue* pCallback)
{
	xrtminizerror tError;
	xrtprogresscall tProgress;
	xfile TempFile = NULL;
	xreader* pReader = NULL;
	xwriter* pWriter = NULL;
	str sDirectory = NULL;
	str sTemp = NULL;
	bool bResult = false;
	int iResult = 0;

	xrtMinizErrorInit(&tError);
	xrtProgressCallInit(&tProgress, pCallback);
	pCallback = tProgress.Callback;
	if ( sSource == NULL || sSource[0] == 0 || sDestination == NULL || sDestination[0] == 0 ) {
		xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.miniz",
			XRT_MINIZ_ERROR_ARGUMENT, "miniz source and destination paths are required");
		return 0;
	}
	if ( xrtPathExists(sDestination) && !bOverwrite ) {
		xrtSetErrorFormat(XERR_EXISTS, "xlang.extlib.miniz", XRT_MINIZ_ERROR_IO,
			"miniz destination already exists: %s", sDestination);
		return 0;
	}
	pReader = xrtReaderOpen(sSource);
	if ( pReader == NULL ) { goto cleanup; }
	sDirectory = xrtPathParent(sDestination);
	if ( sDirectory == NULL ) { goto cleanup; }
	TempFile = xrtFileTemp(sDirectory[0] != 0 ? sDirectory : ".",
		".xrt-miniz-", ".tmp", &sTemp);
	if ( TempFile == NULL ) { goto cleanup; }
	pWriter = xrtWriterTakeFile(&TempFile);
	if ( pWriter == NULL ) { goto cleanup; }

	bResult = bCompress ? xrtMinizCompressStream(pReader, pWriter, pOptions,
		pCallback != NULL ? xrtProgressCallInvoke : NULL, &tProgress, &tError) :
		xrtMinizDecompressStream(pReader, pWriter, pOptions,
			pCallback != NULL ? xrtProgressCallInvoke : NULL, &tProgress, &tError);
	if ( !bResult && !tProgress.InvokeFailed ) { xminiz_publish_error(&tError); }
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
			(tError.iCode == XRT_MINIZ_OK) ) {
			xrtSetErrorInfo(XERR_IO, "xlang.extlib.miniz",
				XRT_MINIZ_ERROR_IO, "miniz file operation failed");
		}
		xminiz_remove_temp(sTemp);
	}
	xrtFree(sDirectory);
	xrtFree(sTemp);
	if ( bResult ) { iResult = 1; }
	else if ( tProgress.InvokeFailed ) { iResult = -2; }
	else if ( tError.iCode == XRT_MINIZ_ERROR_CANCELLED ) { iResult = -1; }
	return iResult;
}



XMINIZ_EXPORT bool xminiz_available(void)
{
	return true;
}

XMINIZ_EXPORT const char* xminiz_version(void)
{
	return xrtVersion();
}

XMINIZ_EXPORT const char* xminiz_last_error(void)
{
	return xrtErrorMessage(xrtGetError());
}



XMINIZ_EXPORT void* xminiz_compress(const void* pData, size_t iSize, int iLevel,
	uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtminizoptions tOptions;
	xrtminizerror tError;
	xbuffer* pOutput = NULL;
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
	xbuffer* pOutput = NULL;
	xrtClearError();
	xminiz_options(&tOptions, -1, iMaxInput, iMaxOutput, iMaxRatio);
	if ( !xrtMinizDecompress(pData, iSize, &tOptions, &pOutput, &tError) ) {
		xminiz_publish_error(&tError);
		return NULL;
	}
	return pOutput;
}

XMINIZ_EXPORT int xminiz_compress_file(str sSource, str sDestination, int iLevel,
	bool bOverwrite, uint64 iMaxInput, uint64 iMaxOutput, xvalue* pCallback)
{
	xrtminizoptions tOptions;
	xrtClearError();
	xminiz_options(&tOptions, iLevel, iMaxInput, iMaxOutput, 0u);
	return xminiz_transform_file(sSource, sDestination, true, bOverwrite, &tOptions, pCallback);
}

XMINIZ_EXPORT int xminiz_decompress_file(str sSource, str sDestination, bool bOverwrite,
	uint64 iMaxInput, uint64 iMaxOutput, uint32 iMaxRatio, xvalue* pCallback)
{
	xrtminizoptions tOptions;
	xrtClearError();
	xminiz_options(&tOptions, -1, iMaxInput, iMaxOutput, iMaxRatio);
	return xminiz_transform_file(sSource, sDestination, false, bOverwrite, &tOptions, pCallback);
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
		return false;
	}
	return true;
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
		return false;
	}
	return true;
}

XMINIZ_EXPORT void* xminiz_compressor_drain(void* pHandle)
{
	xrtminizcompressor* pCompressor = (xrtminizcompressor*)pHandle;
	xbuffer* pOutput = xrtMinizCompressorDrainBuffer(pCompressor);
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
		return false;
	}
	return true;
}

XMINIZ_EXPORT bool xminiz_decompressor_finish(void* pHandle)
{
	xrtminizdecompressor* pDecompressor = (xrtminizdecompressor*)pHandle;
	if ( !xrtMinizDecompressorFinish(pDecompressor) ) {
		xminiz_publish_error(xrtMinizDecompressorError(pDecompressor));
		return false;
	}
	return true;
}

XMINIZ_EXPORT void* xminiz_decompressor_drain(void* pHandle)
{
	xrtminizdecompressor* pDecompressor = (xrtminizdecompressor*)pHandle;
	xbuffer* pOutput = xrtMinizDecompressorDrainBuffer(pDecompressor);
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
