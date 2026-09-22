#define XRT_MODULE_ERROR_FORMAT
#define XRT_MODULE_FILE_TEMP
#define XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_FILE
#define XRT_MODULE_PATH
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE

#include "xruntime.h"
#include "include/xrt_bz2.h"

#if defined(_WIN32) || defined(_WIN64)
#define XBZ2_EXPORT __declspec(dllexport)
#else
#define XBZ2_EXPORT
#endif



/* 把 bz2 模块错误映射为 XRT 通用错误类别。 */
static xerrkind xbz2_error_kind(int iCode)
{
	switch ( iCode ) {
		case XRT_BZ2_ERROR_ARGUMENT:
			return XERR_ARGUMENT;
		case XRT_BZ2_ERROR_MEMORY:
			return XERR_MEMORY;
		case XRT_BZ2_ERROR_DATA:
		case XRT_BZ2_ERROR_TRUNCATED:
			return XERR_PROTOCOL;
		case XRT_BZ2_ERROR_LIMIT:
			return XERR_RANGE;
		case XRT_BZ2_ERROR_STATE:
			return XERR_STATE;
		case XRT_BZ2_ERROR_CANCELLED:
			return XERR_CANCELLED;
		default:
			return XERR_IO;
	}
}


/* 发布可被 XLang 和 C 调用方统一读取的结构化错误。 */
static void xbz2_publish_error(const xrtbz2error* pError)
{
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "bz2 operation failed";
	int iCode = pError != NULL ? pError->iCode : XRT_BZ2_ERROR_IO;

	xrtSetErrorInfo(xbz2_error_kind(iCode), "xlang.extlib.bz2", iCode, sMessage);
}


/* 用调用参数覆盖 bz2 默认资源限制。 */
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
	xbz2_options(pOptions, iLevel, false, iMaxInput, iMaxOutput, 0u);
	if ( iWorkFactor != 0 ) { pOptions->iWorkFactor = iWorkFactor; }
}


/* 删除失败事务的临时文件，同时保留原始失败原因。 */
static void xbz2_remove_temp(cstr sTemp)
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
static int xbz2_transform_file(str sSource, str sDestination, bool bCompress,
	bool bOverwrite, const xrtbz2options* pOptions, xvalue* pCallback)
{
	xrtbz2error tError;
	xrtprogresscall tProgress;
	xfile TempFile = NULL;
	xreader* pReader = NULL;
	xwriter* pWriter = NULL;
	str sDirectory = NULL;
	str sTemp = NULL;
	bool bResult = false;
	int iResult = 0;

	xrtBz2ErrorInit(&tError);
	xrtProgressCallInit(&tProgress, pCallback);
	pCallback = tProgress.Callback;
	if ( sSource == NULL || sSource[0] == 0 || sDestination == NULL || sDestination[0] == 0 ) {
		xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.bz2",
			XRT_BZ2_ERROR_ARGUMENT, "bz2 source and destination paths are required");
		return 0;
	}
	if ( xrtPathExists(sDestination) && !bOverwrite ) {
		xrtSetErrorFormat(XERR_EXISTS, "xlang.extlib.bz2", XRT_BZ2_ERROR_IO,
			"bz2 destination already exists: %s", sDestination);
		return 0;
	}
	pReader = xrtReaderOpen(sSource);
	if ( pReader == NULL ) { goto cleanup; }
	sDirectory = xrtPathParent(sDestination);
	if ( sDirectory == NULL ) { goto cleanup; }
	TempFile = xrtFileTemp(sDirectory[0] != 0 ? sDirectory : ".",
		".xrt-bz2-", ".tmp", &sTemp);
	if ( TempFile == NULL ) { goto cleanup; }
	pWriter = xrtWriterTakeFile(&TempFile);
	if ( pWriter == NULL ) { goto cleanup; }

	bResult = bCompress ? xrtBz2CompressStream(pReader, pWriter, pOptions,
		pCallback != NULL ? xrtProgressCallInvoke : NULL, &tProgress, &tError) :
		xrtBz2DecompressStream(pReader, pWriter, pOptions,
			pCallback != NULL ? xrtProgressCallInvoke : NULL, &tProgress, &tError);
	if ( !bResult && !tProgress.InvokeFailed ) { xbz2_publish_error(&tError); }
	if ( bResult && !xrtWriterFlush(pWriter) ) { bResult = false; }

cleanup:
	if ( (pWriter != NULL) && !xrtWriterDestroy(pWriter) && bResult ) { bResult = false; }
	if ( (TempFile != NULL) && !xrtClose(TempFile) && bResult ) { bResult = false; }
	if ( (pReader != NULL) && !xrtReaderDestroy(pReader) && bResult ) { bResult = false; }
	if ( bResult && !xrtPathRename(sTemp, sDestination, bOverwrite) ) { bResult = false; }
	if ( !bResult ) {
		if ( (xrtGetError() == NULL) && !tProgress.InvokeFailed &&
			(tError.iCode == XRT_BZ2_OK) ) {
			xrtSetErrorInfo(XERR_IO, "xlang.extlib.bz2",
				XRT_BZ2_ERROR_IO, "bz2 file operation failed");
		}
		xbz2_remove_temp(sTemp);
	}
	xrtFree(sDirectory);
	xrtFree(sTemp);
	if ( bResult ) { iResult = 1; }
	else if ( tProgress.InvokeFailed ) { iResult = -2; }
	else if ( tError.iCode == XRT_BZ2_ERROR_CANCELLED ) { iResult = -1; }
	return iResult;
}



XBZ2_EXPORT const char* xbz2_last_error(void)
{
	return xrtErrorMessage(xrtGetError());
}



XBZ2_EXPORT void* xbz2_compress(const void* pData, size_t iSize, int iLevel,
	uint64 iMaxInput, uint64 iMaxOutput)
{
	xrtbz2options tOptions;
	xrtbz2error tError;
	xbuffer* pOutput = NULL;
	xrtClearError();
	xbz2_options(&tOptions, iLevel, false, iMaxInput, iMaxOutput, 0u);
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
	xbuffer* pOutput = NULL;
	xrtClearError();
	xbz2_options(&tOptions, 9, bSmallMemory, iMaxInput, iMaxOutput, iMaxRatio);
	if ( !xrtBz2Decompress(pData, iSize, &tOptions, &pOutput, &tError) ) {
		xbz2_publish_error(&tError);
		return NULL;
	}
	return pOutput;
}

XBZ2_EXPORT int xbz2_compress_file(str sSource, str sDestination, int iLevel,
	bool bOverwrite, uint64 iMaxInput, uint64 iMaxOutput, xvalue* pCallback)
{
	xrtbz2options tOptions;
	xrtClearError();
	xbz2_options(&tOptions, iLevel, false, iMaxInput, iMaxOutput, 0u);
	return xbz2_transform_file(sSource, sDestination, true, bOverwrite, &tOptions, pCallback);
}

XBZ2_EXPORT int xbz2_decompress_file(str sSource, str sDestination,
	bool bSmallMemory, bool bOverwrite, uint64 iMaxInput, uint64 iMaxOutput,
	uint32 iMaxRatio, xvalue* pCallback)
{
	xrtbz2options tOptions;
	xrtClearError();
	xbz2_options(&tOptions, 9, bSmallMemory, iMaxInput, iMaxOutput, iMaxRatio);
	return xbz2_transform_file(sSource, sDestination, false, bOverwrite, &tOptions, pCallback);
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
		return false;
	}
	return true;
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
		return false;
	}
	return true;
}

XBZ2_EXPORT void* xbz2_compressor_drain(void* pHandle)
{
	xrtbz2compressor* pCompressor = (xrtbz2compressor*)pHandle;
	xbuffer* pOutput = xrtBz2CompressorDrainBuffer(pCompressor);
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
		return false;
	}
	return true;
}

XBZ2_EXPORT bool xbz2_decompressor_finish(void* pHandle)
{
	xrtbz2decompressor* pDecompressor = (xrtbz2decompressor*)pHandle;
	if ( !xrtBz2DecompressorFinish(pDecompressor) ) {
		xbz2_publish_error(xrtBz2DecompressorError(pDecompressor));
		return false;
	}
	return true;
}

XBZ2_EXPORT void* xbz2_decompressor_drain(void* pHandle)
{
	xrtbz2decompressor* pDecompressor = (xrtbz2decompressor*)pHandle;
	xbuffer* pOutput = xrtBz2DecompressorDrainBuffer(pDecompressor);
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
