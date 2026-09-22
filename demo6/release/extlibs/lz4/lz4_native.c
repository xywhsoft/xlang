#define XRT_MODULE_ERROR_FORMAT
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE

#include "xruntime.h"
#include "include/xrt_lz4.h"
#include "third_party/lz4.h"

#if defined(_WIN32) || defined(_WIN64)
#define XLZ4_EXPORT __declspec(dllexport)
#else
#define XLZ4_EXPORT
#endif



/* 把 LZ4 模块错误映射为 XRT 通用错误类别。 */
static xerrkind xlz4_error_kind(int iCode)
{
	switch ( iCode ) {
		case XRT_LZ4_ERROR_ARGUMENT:
			return XERR_ARGUMENT;
		case XRT_LZ4_ERROR_MEMORY:
			return XERR_MEMORY;
		case XRT_LZ4_ERROR_DATA:
		case XRT_LZ4_ERROR_TRUNCATED:
			return XERR_PROTOCOL;
		case XRT_LZ4_ERROR_LIMIT:
			return XERR_RANGE;
		case XRT_LZ4_ERROR_STATE:
			return XERR_STATE;
		case XRT_LZ4_ERROR_CANCELLED:
			return XERR_CANCELLED;
		default:
			return XERR_IO;
	}
}


/* 发布可被 XLang 和 C 调用方统一读取的结构化错误。 */
static void xlz4_publish_error(const xrtlz4error* pError)
{
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "lz4 operation failed";
	int iCode = pError != NULL ? pError->iCode : XRT_LZ4_ERROR_IO;

	xrtSetErrorInfo(
		xlz4_error_kind(iCode),
		"xlang.extlib.lz4",
		iCode,
		sMessage
	);
}


/* 用调用参数覆盖 LZ4 默认资源限制。 */
static void xlz4_options(xrtlz4options* pOptions, int iAcceleration, uint64 iMaxInput,
	uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtLz4OptionsInit(pOptions);
	if ( iAcceleration > 1 ) { pOptions->iAcceleration = iAcceleration; }
	if ( iMaxInput != 0u ) { pOptions->tLimits.iMaxInputBytes = iMaxInput; }
	if ( iMaxOutput != 0u ) { pOptions->tLimits.iMaxOutputBytes = iMaxOutput; }
	if ( iMaxRatio != 0u ) { pOptions->tLimits.iMaxCompressionRatio = iMaxRatio; }
}


/* 删除失败事务的临时文件，同时保留原始失败原因。 */
static void xlz4_remove_temp(cstr sTemp)
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
static int xlz4_transform_file(str sSource, str sDestination, bool bCompress,
	bool bOverwrite, const xrtlz4options* pOptions, xvalue* pCallback)
{
	xrtlz4error tError;
	xrtprogresscall tProgress;
	xfile TempFile = NULL;
	xreader* pReader = NULL;
	xwriter* pWriter = NULL;
	str sDirectory = NULL;
	str sTemp = NULL;
	bool bResult = false;
	int iResult = 0;

	xrtLz4ErrorInit(&tError);
	xrtProgressCallInit(&tProgress, pCallback);
	pCallback = tProgress.Callback;
	if ( sSource == NULL || sSource[0] == 0 || sDestination == NULL || sDestination[0] == 0 ) {
		xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.lz4",
			XRT_LZ4_ERROR_ARGUMENT, "lz4 source and destination paths are required");
		return 0;
	}
	if ( xrtPathExists(sDestination) && !bOverwrite ) {
		xrtSetErrorFormat(XERR_EXISTS, "xlang.extlib.lz4", XRT_LZ4_ERROR_IO,
			"lz4 destination already exists: %s", sDestination);
		return 0;
	}
	pReader = xrtReaderOpen(sSource);
	if ( pReader == NULL ) { goto cleanup; }
	sDirectory = xrtPathParent(sDestination);
	if ( sDirectory == NULL ) { goto cleanup; }
	TempFile = xrtFileTemp(sDirectory[0] != 0 ? sDirectory : ".",
		".xrt-lz4-", ".tmp", &sTemp);
	if ( TempFile == NULL ) { goto cleanup; }
	pWriter = xrtWriterTakeFile(&TempFile);
	if ( pWriter == NULL ) { goto cleanup; }

	bResult = bCompress ? xrtLz4CompressStream(pReader, pWriter, pOptions,
		pCallback != NULL ? xrtProgressCallInvoke : NULL, &tProgress, &tError) :
		xrtLz4DecompressStream(pReader, pWriter, pOptions,
			pCallback != NULL ? xrtProgressCallInvoke : NULL, &tProgress, &tError);
	if ( !bResult && !tProgress.InvokeFailed ) { xlz4_publish_error(&tError); }
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
			(tError.iCode == XRT_LZ4_OK) ) {
			xrtSetErrorInfo(XERR_IO, "xlang.extlib.lz4",
				XRT_LZ4_ERROR_IO, "lz4 file operation failed");
		}
		xlz4_remove_temp(sTemp);
	}
	xrtFree(sDirectory);
	xrtFree(sTemp);
	if ( bResult ) { iResult = 1; }
	else if ( tProgress.InvokeFailed ) { iResult = -2; }
	else if ( tError.iCode == XRT_LZ4_ERROR_CANCELLED ) { iResult = -1; }
	return iResult;
}



XLZ4_EXPORT bool xlz4_available(void)
{
	return true;
}

XLZ4_EXPORT const char* xlz4_version(void)
{
	return LZ4_versionString();
}

XLZ4_EXPORT const char* xlz4_last_error(void)
{
	return xrtErrorMessage(xrtGetError());
}



XLZ4_EXPORT void* xlz4_compress(const void* pData, size_t iSize, int iAcceleration,
	uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtlz4options tOptions;
	xrtlz4error tError;
	xbuffer* pOutput = NULL;
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
	xbuffer* pOutput = NULL;
	xrtClearError();
	xlz4_options(&tOptions, 0, iMaxInput, iMaxOutput, iMaxRatio);
	if ( !xrtLz4Decompress(pData, iSize, &tOptions, &pOutput, &tError) ) {
		xlz4_publish_error(&tError);
		return NULL;
	}
	return pOutput;
}

XLZ4_EXPORT int xlz4_compress_file(str sSource, str sDestination, int iAcceleration,
	bool bOverwrite, uint64 iMaxInput, uint64 iMaxOutput, xvalue* pCallback)
{
	xrtlz4options tOptions;
	xrtClearError();
	xlz4_options(&tOptions, iAcceleration, iMaxInput, iMaxOutput, 0u);
	return xlz4_transform_file(sSource, sDestination, true, bOverwrite, &tOptions, pCallback);
}

XLZ4_EXPORT int xlz4_decompress_file(str sSource, str sDestination, bool bOverwrite,
	uint64 iMaxInput, uint64 iMaxOutput, uint32 iMaxRatio, xvalue* pCallback)
{
	xrtlz4options tOptions;
	xrtClearError();
	xlz4_options(&tOptions, 0, iMaxInput, iMaxOutput, iMaxRatio);
	return xlz4_transform_file(sSource, sDestination, false, bOverwrite, &tOptions, pCallback);
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
		return false;
	}
	return true;
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
		return false;
	}
	return true;
}

XLZ4_EXPORT void* xlz4_compressor_drain(void* pHandle)
{
	xrtlz4compressor* pCompressor = (xrtlz4compressor*)pHandle;
	xbuffer* pOutput = xrtLz4CompressorDrainBuffer(pCompressor);
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
		return false;
	}
	return true;
}

XLZ4_EXPORT bool xlz4_decompressor_finish(void* pHandle)
{
	xrtlz4decompressor* pDecompressor = (xrtlz4decompressor*)pHandle;
	if ( !xrtLz4DecompressorFinish(pDecompressor) ) {
		xlz4_publish_error(xrtLz4DecompressorError(pDecompressor));
		return false;
	}
	return true;
}

XLZ4_EXPORT void* xlz4_decompressor_drain(void* pHandle)
{
	xrtlz4decompressor* pDecompressor = (xrtlz4decompressor*)pHandle;
	xbuffer* pOutput = xrtLz4DecompressorDrainBuffer(pDecompressor);
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
