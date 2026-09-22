#define XRT_MODULE_ERROR_FORMAT
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE

#include "xruntime.h"
#include "include/xrt_snappy.h"

#if defined(_WIN32) || defined(_WIN64)
#define XSNAPPY_EXPORT __declspec(dllexport)
#else
#define XSNAPPY_EXPORT
#endif



/* 把 Snappy 模块错误映射为 XRT 通用错误类别。 */
static xerrkind xsnappy_error_kind(int iCode)
{
	switch ( iCode ) {
		case XRT_SNAPPY_ERROR_ARGUMENT:
			return XERR_ARGUMENT;
		case XRT_SNAPPY_ERROR_MEMORY:
			return XERR_MEMORY;
		case XRT_SNAPPY_ERROR_DATA:
		case XRT_SNAPPY_ERROR_TRUNCATED:
			return XERR_PROTOCOL;
		case XRT_SNAPPY_ERROR_LIMIT:
			return XERR_RANGE;
		case XRT_SNAPPY_ERROR_STATE:
			return XERR_STATE;
		case XRT_SNAPPY_ERROR_CANCELLED:
			return XERR_CANCELLED;
		default:
			return XERR_IO;
	}
}


/* 发布可被 XLang 和 C 调用方统一读取的结构化错误。 */
static void xsnappy_publish_error(const xrtsnappyerror* pError)
{
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "snappy operation failed";
	int iCode = pError != NULL ? pError->iCode : XRT_SNAPPY_ERROR_IO;

	xrtSetErrorInfo(
		xsnappy_error_kind(iCode),
		"xlang.extlib.snappy",
		iCode,
		sMessage
	);
}


/* 用调用参数覆盖 Snappy 默认资源限制。 */
static void xsnappy_options(xrtsnappyoptions* pOptions, uint64 iMaxInput,
	uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtSnappyOptionsInit(pOptions);
	if ( iMaxInput != 0u ) { pOptions->tLimits.iMaxInputBytes = iMaxInput; }
	if ( iMaxOutput != 0u ) { pOptions->tLimits.iMaxOutputBytes = iMaxOutput; }
	if ( iMaxRatio != 0u ) { pOptions->tLimits.iMaxCompressionRatio = iMaxRatio; }
}


/* 删除失败事务的临时文件，同时保留原始失败原因。 */
static void xsnappy_remove_temp(cstr sTemp)
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
static int xsnappy_transform_file(str sSource, str sDestination, bool bCompress,
	bool bOverwrite, const xrtsnappyoptions* pOptions, xvalue* pCallback)
{
	xrtsnappyerror tError;
	xrtprogresscall tProgress;
	xfile TempFile = NULL;
	xreader* pReader = NULL;
	xwriter* pWriter = NULL;
	str sDirectory = NULL;
	str sTemp = NULL;
	bool bResult = false;
	int iResult = 0;

	xrtSnappyErrorInit(&tError);
	xrtProgressCallInit(&tProgress, pCallback);
	pCallback = tProgress.Callback;
	if ( sSource == NULL || sSource[0] == 0 || sDestination == NULL || sDestination[0] == 0 ) {
		xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.snappy",
			XRT_SNAPPY_ERROR_ARGUMENT, "snappy source and destination paths are required");
		return 0;
	}
	if ( xrtPathExists(sDestination) && !bOverwrite ) {
		xrtSetErrorFormat(XERR_EXISTS, "xlang.extlib.snappy", XRT_SNAPPY_ERROR_IO,
			"snappy destination already exists: %s", sDestination);
		return 0;
	}
	pReader = xrtReaderOpen(sSource);
	if ( pReader == NULL ) { goto cleanup; }
	sDirectory = xrtPathParent(sDestination);
	if ( sDirectory == NULL ) { goto cleanup; }
	TempFile = xrtFileTemp(sDirectory[0] != 0 ? sDirectory : ".",
		".xrt-snappy-", ".tmp", &sTemp);
	if ( TempFile == NULL ) { goto cleanup; }
	pWriter = xrtWriterTakeFile(&TempFile);
	if ( pWriter == NULL ) { goto cleanup; }

	bResult = bCompress ? xrtSnappyCompressStream(pReader, pWriter, pOptions,
		pCallback != NULL ? xrtProgressCallInvoke : NULL, &tProgress, &tError) :
		xrtSnappyDecompressStream(pReader, pWriter, pOptions,
			pCallback != NULL ? xrtProgressCallInvoke : NULL, &tProgress, &tError);
	if ( !bResult && !tProgress.InvokeFailed ) { xsnappy_publish_error(&tError); }
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
			(tError.iCode == XRT_SNAPPY_OK) ) {
			xrtSetErrorInfo(XERR_IO, "xlang.extlib.snappy",
				XRT_SNAPPY_ERROR_IO, "snappy file operation failed");
		}
		xsnappy_remove_temp(sTemp);
	}
	xrtFree(sDirectory);
	xrtFree(sTemp);
	if ( bResult ) { iResult = 1; }
	else if ( tProgress.InvokeFailed ) { iResult = -2; }
	else if ( tError.iCode == XRT_SNAPPY_ERROR_CANCELLED ) { iResult = -1; }
	return iResult;
}



XSNAPPY_EXPORT bool xsnappy_available(void)
{
	return true;
}

XSNAPPY_EXPORT const char* xsnappy_version(void)
{
	/* 纯 C snappy 格式编解码器（格式兼容 Google Snappy），版本随 xlang 扩展库发布。 */
	return "1.0.0";
}

XSNAPPY_EXPORT const char* xsnappy_last_error(void)
{
	return xrtErrorMessage(xrtGetError());
}



XSNAPPY_EXPORT void* xsnappy_compress(const void* pData, size_t iSize,
	uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtsnappyoptions tOptions;
	xrtsnappyerror tError;
	xbuffer* pOutput = NULL;
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
	xbuffer* pOutput = NULL;
	xrtClearError();
	xsnappy_options(&tOptions, iMaxInput, iMaxOutput, iMaxRatio);
	if ( !xrtSnappyDecompress(pData, iSize, &tOptions, &pOutput, &tError) ) {
		xsnappy_publish_error(&tError);
		return NULL;
	}
	return pOutput;
}

XSNAPPY_EXPORT int xsnappy_compress_file(str sSource, str sDestination, bool bOverwrite,
	uint64 iMaxInput, uint64 iMaxOutput, xvalue* pCallback)
{
	xrtsnappyoptions tOptions;
	xrtClearError();
	xsnappy_options(&tOptions, iMaxInput, iMaxOutput, 0u);
	return xsnappy_transform_file(sSource, sDestination, true, bOverwrite, &tOptions, pCallback);
}

XSNAPPY_EXPORT int xsnappy_decompress_file(str sSource, str sDestination, bool bOverwrite,
	uint64 iMaxInput, uint64 iMaxOutput, uint32 iMaxRatio, xvalue* pCallback)
{
	xrtsnappyoptions tOptions;
	xrtClearError();
	xsnappy_options(&tOptions, iMaxInput, iMaxOutput, iMaxRatio);
	return xsnappy_transform_file(sSource, sDestination, false, bOverwrite, &tOptions, pCallback);
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
		return false;
	}
	return true;
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
		return false;
	}
	return true;
}

XSNAPPY_EXPORT void* xsnappy_compressor_drain(void* pHandle)
{
	xrtsnappycompressor* pCompressor = (xrtsnappycompressor*)pHandle;
	xbuffer* pOutput = xrtSnappyCompressorDrainBuffer(pCompressor);
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
		return false;
	}
	return true;
}

XSNAPPY_EXPORT bool xsnappy_decompressor_finish(void* pHandle)
{
	xrtsnappydecompressor* pDecompressor = (xrtsnappydecompressor*)pHandle;
	if ( !xrtSnappyDecompressorFinish(pDecompressor) ) {
		xsnappy_publish_error(xrtSnappyDecompressorError(pDecompressor));
		return false;
	}
	return true;
}

XSNAPPY_EXPORT void* xsnappy_decompressor_drain(void* pHandle)
{
	xrtsnappydecompressor* pDecompressor = (xrtsnappydecompressor*)pHandle;
	xbuffer* pOutput = xrtSnappyDecompressorDrainBuffer(pDecompressor);
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
