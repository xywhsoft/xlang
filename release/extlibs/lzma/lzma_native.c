#ifndef XRT_MODULE_ERROR_FORMAT
#define XRT_MODULE_ERROR_FORMAT
#endif
#ifndef XRT_MODULE_FILE_TEMP
#define XRT_MODULE_FILE_TEMP
#endif
#ifndef XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_BUFFER
#endif
#ifndef XRT_MODULE_IO_FILE
#define XRT_MODULE_IO_FILE
#endif
#ifndef XRT_MODULE_PATH
#define XRT_MODULE_PATH
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#endif

#include "xruntime.h"
#include "include/xrt_lzma.h"
#include "include/lzma_native.h"

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

	/* 保留分配器、IO 或回调已经发布的首个错误。 */
	if ( xrtGetError() != NULL ) { return; }

	xrtSetErrorInfo(xlzma_error_kind(iCode), "xlang.extlib.lzma",
		iCode, sMessage);
}

static bool xlzma_handle_required(const void* pHandle)
{
	if ( pHandle != NULL ) { return true; }
	xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.lzma",
		XRT_LZMA_ERROR_ARGUMENT, "lzma object handle is required");
	return false;
}


/* 用调用参数覆盖 LZMA 默认资源限制。 */
static void xlzma_options(xrtlzmaoptions* pOptions, int iLevel, uint64 iMaxInput,
	uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtLzmaOptionsInit(pOptions);
	pOptions->iLevel = iLevel;
	if ( iMaxInput != 0u ) { pOptions->tLimits.iMaxInputBytes = iMaxInput; }
	if ( iMaxOutput != 0u ) { pOptions->tLimits.iMaxOutputBytes = iMaxOutput; }
	if ( iMaxRatio != 0u ) { pOptions->tLimits.iMaxCompressionRatio = iMaxRatio; }
}


/* 1 成功，0 处理失败，-1 用户取消，-2 回调抛错。 */
static int xlzma_transform_file(const char* sSource, const char* sDestination, bool bCompress,
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
	xerror* pSaved;

	xrtLzmaErrorInit(&tError);
	xrtProgressCallInit(&tProgress, pCallback);
	pCallback = tProgress.Callback;
	if ( sSource == NULL || sSource[0] == 0 || sDestination == NULL || sDestination[0] == 0 ) {
		xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.lzma",
			XRT_LZMA_ERROR_ARGUMENT, "lzma source and destination paths are required");
		return 0;
	}
	if ( !bOverwrite ) {
		xfileinfo Info;
		if ( xrtPathStat(sDestination, false, &Info) ) {
			xrtSetErrorFormat(XERR_EXISTS, "xlang.extlib.lzma", XRT_LZMA_ERROR_IO,
				"lzma destination already exists: %s", sDestination);
			return 0;
		}
		if ( xrtErrorKind(xrtGetError()) != XERR_NOT_FOUND ) { return 0; }
		xrtClearError();
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
	/* 析构或删除临时文件不得覆盖回调、内存和首个 IO 错误。 */
	pSaved = xrtTakeError();
	if ( pSaved != NULL ) { bResult = false; }
	if ( (pWriter != NULL) && !xrtWriterDestroy(pWriter) && bResult ) {
		bResult = false; pSaved = xrtTakeError();
	}
	xrtClearError();
	if ( (TempFile != NULL) && !xrtClose(TempFile) && bResult ) {
		bResult = false; pSaved = xrtTakeError();
	}
	xrtClearError();
	if ( (pReader != NULL) && !xrtReaderDestroy(pReader) && bResult ) {
		bResult = false; pSaved = xrtTakeError();
	}
	xrtClearError();
	if ( bResult && !xrtPathRename(sTemp, sDestination, bOverwrite) ) {
		bResult = false; pSaved = xrtTakeError();
	}
	if ( !bResult && sTemp != NULL ) { (void)xrtFileDelete(sTemp); }
	xrtClearError();
	if ( pSaved != NULL ) {
		xrtSetError(pSaved);
		xrtErrorFree(pSaved);
	} else if ( !bResult && !tProgress.InvokeFailed ) {
		xrtSetErrorInfo(XERR_IO, "xlang.extlib.lzma", XRT_LZMA_ERROR_IO,
			"lzma file operation failed");
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
	xrtClearError();
	return true;
}

XLZMA_EXPORT const char* xlzma_version(void)
{
	xrtClearError();
	return "lzma sdk 26.03";
}

XLZMA_EXPORT const char* xlzma_last_error(void)
{
	return xrtErrorMessage(xrtGetError());
}



XLZMA_EXPORT void* xlzma_compress(void* pData, size_t iSize, int iLevel,
	unsigned long long iMaxInput, unsigned long long iMaxOutput)
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
	unsigned long long iMaxInput, unsigned long long iMaxOutput)
{
	return xlzma_compress((void*)sText, iSize, iLevel, iMaxInput, iMaxOutput);
}

XLZMA_EXPORT void* xlzma_decompress(void* pData, size_t iSize, unsigned long long iMaxInput,
	unsigned long long iMaxOutput, unsigned int iMaxRatio)
{
	xrtlzmaoptions tOptions;
	xrtlzmaerror tError;
	xbuffer* pOutput = NULL;
	xrtClearError();
	xlzma_options(&tOptions, 5, iMaxInput, iMaxOutput, iMaxRatio);
	if ( !xrtLzmaDecompress(pData, iSize, &tOptions, &pOutput, &tError) ) {
		xlzma_publish_error(&tError);
		return NULL;
	}
	return pOutput;
}

/* UTF-8 验证后将解压结果移交给字符串，避免再复制整份文本。 */
XLZMA_EXPORT void* xlzma_decompress_text(void* pData, size_t iSize,
	unsigned long long iMaxInput, unsigned long long iMaxOutput, unsigned int iMaxRatio)
{
	xbuffer* pOutput = (xbuffer*)xlzma_decompress(pData, iSize, iMaxInput, iMaxOutput, iMaxRatio);
	if ( pOutput == NULL ) { return NULL; }
	if ( !xrtUtf8Valid((xstrview){(const char*)pOutput->Data, pOutput->Size}, NULL) ) {
		xrtSetErrorInfo(XERR_VALUE, "xlang.extlib.lzma", 0,
			"lzma decompressed text is not valid UTF-8");
		xrtBufferDestroy(pOutput);
		return NULL;
	}
	if ( !xrtBufferAppendByte(pOutput, 0) ) {
		xrtBufferDestroy(pOutput);
		return NULL;
	}
	return pOutput;
}

XLZMA_EXPORT size_t xlzma_text_size(void* pRaw)
{
	const xbuffer* pOutput = (const xbuffer*)pRaw;
	return pOutput != NULL && pOutput->Size != 0u ? pOutput->Size - 1u : 0u;
}

XLZMA_EXPORT void* xlzma_text_take(void* pRaw)
{
	xbuffer* pOutput = (xbuffer*)pRaw;
	void* pText;
	if ( pOutput == NULL ) { return NULL; }
	pText = xrtBufferTake(pOutput, NULL, NULL);
	xrtBufferDestroy(pOutput);
	return pText;
}

XLZMA_EXPORT int xlzma_compress_file(const char* sSource, const char* sDestination, int iLevel,
	bool bOverwrite, unsigned long long iMaxInput, unsigned long long iMaxOutput, xvalue* pCallback)
{
	xrtlzmaoptions tOptions;
	xrtClearError();
	xlzma_options(&tOptions, iLevel, iMaxInput, iMaxOutput, 0u);
	return xlzma_transform_file(sSource, sDestination, true, bOverwrite, &tOptions, pCallback);
}

XLZMA_EXPORT int xlzma_decompress_file(const char* sSource, const char* sDestination, bool bOverwrite,
	unsigned long long iMaxInput, unsigned long long iMaxOutput, unsigned int iMaxRatio, xvalue* pCallback)
{
	xrtlzmaoptions tOptions;
	xrtClearError();
	xlzma_options(&tOptions, 5, iMaxInput, iMaxOutput, iMaxRatio);
	return xlzma_transform_file(sSource, sDestination, false, bOverwrite, &tOptions, pCallback);
}



XLZMA_EXPORT void* xlzma_compressor_create(int iLevel, unsigned long long iMaxInput, unsigned long long iMaxOutput)
{
	xrtlzmaoptions tOptions;
	xrtlzmacompressor* pCompressor;
	xrtClearError();
	xlzma_options(&tOptions, iLevel, iMaxInput, iMaxOutput, 0u);
	pCompressor = xrtLzmaCompressorCreate(&tOptions);
	if ( pCompressor == NULL ) {
		if ( xrtGetError() == NULL ) {
			xrtSetErrorInfo(XERR_MEMORY, "xlang.extlib.lzma",
				XRT_LZMA_ERROR_MEMORY, "cannot allocate lzma compressor");
		}
		return NULL;
	}
	if ( xrtLzmaCompressorError(pCompressor)->iCode != XRT_LZMA_OK ) {
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

XLZMA_EXPORT bool xlzma_compressor_update(void* pHandle, void* pData, size_t iSize)
{
	xrtlzmacompressor* pCompressor = (xrtlzmacompressor*)pHandle;
	xrtClearError();
	if ( !xlzma_handle_required(pHandle) ) { return false; }
	if ( !xrtLzmaCompressorUpdate(pCompressor, pData, iSize) ) {
		xlzma_publish_error(xrtLzmaCompressorError(pCompressor));
		return false;
	}
	return true;
}

XLZMA_EXPORT bool xlzma_compressor_update_str(void* pHandle, const char* sText, size_t iSize)
{
	return xlzma_compressor_update(pHandle, (void*)sText, iSize);
}

XLZMA_EXPORT bool xlzma_compressor_finish(void* pHandle)
{
	xrtlzmacompressor* pCompressor = (xrtlzmacompressor*)pHandle;
	xrtClearError();
	if ( !xlzma_handle_required(pHandle) ) { return false; }
	if ( !xrtLzmaCompressorFinish(pCompressor) ) {
		xlzma_publish_error(xrtLzmaCompressorError(pCompressor));
		return false;
	}
	return true;
}

XLZMA_EXPORT void* xlzma_compressor_drain(void* pHandle)
{
	xrtlzmacompressor* pCompressor = (xrtlzmacompressor*)pHandle;
	xrtClearError();
	if ( !xlzma_handle_required(pHandle) ) { return NULL; }
	xbuffer* pOutput = xrtLzmaCompressorDrainBuffer(pCompressor);
	if ( pOutput == NULL ) {
		xlzma_publish_error(xrtLzmaCompressorError(pCompressor));
		return NULL;
	}
	return pOutput;
}

XLZMA_EXPORT unsigned long long xlzma_compressor_input_bytes(void* pHandle)
{
	xrtClearError();
	if ( !xlzma_handle_required(pHandle) ) { return 0u; }
	return xrtLzmaCompressorInputBytes((const xrtlzmacompressor*)pHandle);
}

XLZMA_EXPORT unsigned long long xlzma_compressor_output_bytes(void* pHandle)
{
	xrtClearError();
	if ( !xlzma_handle_required(pHandle) ) { return 0u; }
	return xrtLzmaCompressorOutputBytes((const xrtlzmacompressor*)pHandle);
}



XLZMA_EXPORT void* xlzma_decompressor_create(unsigned long long iMaxInput, unsigned long long iMaxOutput,
	unsigned int iMaxRatio)
{
	xrtlzmaoptions tOptions;
	xrtlzmadecompressor* pDecompressor;
	xrtClearError();
	xlzma_options(&tOptions, 5, iMaxInput, iMaxOutput, iMaxRatio);
	pDecompressor = xrtLzmaDecompressorCreate(&tOptions);
	if ( pDecompressor == NULL ) {
		if ( xrtGetError() == NULL ) {
			xrtSetErrorInfo(XERR_MEMORY, "xlang.extlib.lzma",
				XRT_LZMA_ERROR_MEMORY, "cannot allocate lzma decompressor");
		}
		return NULL;
	}
	if ( xrtLzmaDecompressorError(pDecompressor)->iCode != XRT_LZMA_OK ) {
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

XLZMA_EXPORT bool xlzma_decompressor_update(void* pHandle, void* pData, size_t iSize)
{
	xrtlzmadecompressor* pDecompressor = (xrtlzmadecompressor*)pHandle;
	xrtClearError();
	if ( !xlzma_handle_required(pHandle) ) { return false; }
	if ( !xrtLzmaDecompressorUpdate(pDecompressor, pData, iSize) ) {
		xlzma_publish_error(xrtLzmaDecompressorError(pDecompressor));
		return false;
	}
	return true;
}

XLZMA_EXPORT bool xlzma_decompressor_finish(void* pHandle)
{
	xrtlzmadecompressor* pDecompressor = (xrtlzmadecompressor*)pHandle;
	xrtClearError();
	if ( !xlzma_handle_required(pHandle) ) { return false; }
	if ( !xrtLzmaDecompressorFinish(pDecompressor) ) {
		xlzma_publish_error(xrtLzmaDecompressorError(pDecompressor));
		return false;
	}
	return true;
}

XLZMA_EXPORT void* xlzma_decompressor_drain(void* pHandle)
{
	xrtlzmadecompressor* pDecompressor = (xrtlzmadecompressor*)pHandle;
	xrtClearError();
	if ( !xlzma_handle_required(pHandle) ) { return NULL; }
	xbuffer* pOutput = xrtLzmaDecompressorDrainBuffer(pDecompressor);
	if ( pOutput == NULL ) {
		xlzma_publish_error(xrtLzmaDecompressorError(pDecompressor));
		return NULL;
	}
	return pOutput;
}

XLZMA_EXPORT unsigned long long xlzma_decompressor_input_bytes(void* pHandle)
{
	xrtClearError();
	if ( !xlzma_handle_required(pHandle) ) { return 0u; }
	return xrtLzmaDecompressorInputBytes((const xrtlzmadecompressor*)pHandle);
}

XLZMA_EXPORT unsigned long long xlzma_decompressor_output_bytes(void* pHandle)
{
	xrtClearError();
	if ( !xlzma_handle_required(pHandle) ) { return 0u; }
	return xrtLzmaDecompressorOutputBytes((const xrtlzmadecompressor*)pHandle);
}

XLZMA_EXPORT bool xlzma_decompressor_finished(void* pHandle)
{
	xrtClearError();
	if ( !xlzma_handle_required(pHandle) ) { return false; }
	return xrtLzmaDecompressorFinished((const xrtlzmadecompressor*)pHandle);
}

XLZMA_EXPORT unsigned long long xlzma_decompressor_members(void* pHandle)
{
	xrtClearError();
	if ( !xlzma_handle_required(pHandle) ) { return 0u; }
	return xrtLzmaDecompressorMembers((const xrtlzmadecompressor*)pHandle);
}
