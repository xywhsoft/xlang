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
#ifndef XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#endif

#include "xruntime.h"
#include "include/xrt_zstd.h"
#include "include/zstd_native.h"
#include "third_party/zstd.h"

#if defined(_WIN32) || defined(_WIN64)
#define XZSTD_EXPORT __declspec(dllexport)
#else
#define XZSTD_EXPORT
#endif



/* 把 Zstandard 模块错误映射为 XRT 通用错误类别。 */
static xerrkind xzstd_error_kind(int iCode)
{
	switch ( iCode ) {
		case XRT_ZSTD_ERROR_ARGUMENT:
			return XERR_ARGUMENT;
		case XRT_ZSTD_ERROR_MEMORY:
			return XERR_MEMORY;
		case XRT_ZSTD_ERROR_DATA:
		case XRT_ZSTD_ERROR_TRUNCATED:
			return XERR_PROTOCOL;
		case XRT_ZSTD_ERROR_LIMIT:
			return XERR_RANGE;
		case XRT_ZSTD_ERROR_STATE:
			return XERR_STATE;
		case XRT_ZSTD_ERROR_CANCELLED:
			return XERR_CANCELLED;
		default:
			return XERR_IO;
	}
}


/* 发布可被 XLang 和 C 调用方统一读取的结构化错误。 */
static void xzstd_publish_error(const xrtzstderror* pError)
{
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "zstd operation failed";
	int iCode = pError != NULL ? pError->iCode : XRT_ZSTD_ERROR_IO;

	/* 保留分配器、IO 或回调已经发布的首个错误。 */
	if ( xrtGetError() != NULL ) { return; }

	xrtSetErrorInfo(
		xzstd_error_kind(iCode),
		"xlang.extlib.zstd",
		iCode,
		sMessage
	);
}


/* 用调用参数覆盖 Zstandard 默认资源限制。 */
static void xzstd_options(xrtzstdoptions* pOptions, int iLevel, uint64 iMaxInput,
	uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtZstdOptionsInit(pOptions);
	if ( iLevel != 0 ) { pOptions->iLevel = iLevel; }
	if ( iMaxInput != 0u ) { pOptions->tLimits.iMaxInputBytes = iMaxInput; }
	if ( iMaxOutput != 0u ) { pOptions->tLimits.iMaxOutputBytes = iMaxOutput; }
	if ( iMaxRatio != 0u ) { pOptions->tLimits.iMaxCompressionRatio = iMaxRatio; }
}


/* 1 成功，0 处理失败，-1 用户取消，-2 回调抛错。 */
static int xzstd_transform_file(const char* sSource, const char* sDestination, bool bCompress,
	bool bOverwrite, const xrtzstdoptions* pOptions, xvalue* pCallback)
{
	xrtzstderror tError;
	xrtprogresscall tProgress;
	xfile TempFile = NULL;
	xreader* pReader = NULL;
	xwriter* pWriter = NULL;
	str sDirectory = NULL;
	str sTemp = NULL;
	bool bResult = false;
	int iResult = 0;
	xerror* pSaved;

	xrtZstdErrorInit(&tError);
	xrtProgressCallInit(&tProgress, pCallback);
	pCallback = tProgress.Callback;
	if ( sSource == NULL || sSource[0] == 0 || sDestination == NULL || sDestination[0] == 0 ) {
		xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.zstd",
			XRT_ZSTD_ERROR_ARGUMENT, "zstd source and destination paths are required");
		return 0;
	}
	if ( !bOverwrite ) {
		xfileinfo Info;
		if ( xrtPathStat(sDestination, false, &Info) ) {
			xrtSetErrorFormat(XERR_EXISTS, "xlang.extlib.zstd", XRT_ZSTD_ERROR_IO,
				"zstd destination already exists: %s", sDestination);
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
		".xrt-zstd-", ".tmp", &sTemp);
	if ( TempFile == NULL ) { goto cleanup; }
	pWriter = xrtWriterTakeFile(&TempFile);
	if ( pWriter == NULL ) { goto cleanup; }

	bResult = bCompress ? xrtZstdCompressStream(pReader, pWriter, pOptions,
		pCallback != NULL ? xrtProgressCallInvoke : NULL, &tProgress, &tError) :
		xrtZstdDecompressStream(pReader, pWriter, pOptions,
			pCallback != NULL ? xrtProgressCallInvoke : NULL, &tProgress, &tError);
	if ( !bResult && !tProgress.InvokeFailed ) { xzstd_publish_error(&tError); }
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
		xrtSetErrorInfo(XERR_IO, "xlang.extlib.zstd", XRT_ZSTD_ERROR_IO,
			"zstd file operation failed");
	}
	xrtFree(sDirectory);
	xrtFree(sTemp);
	if ( bResult ) { iResult = 1; }
	else if ( tProgress.InvokeFailed ) { iResult = -2; }
	else if ( tError.iCode == XRT_ZSTD_ERROR_CANCELLED ) { iResult = -1; }
	return iResult;
}



XZSTD_EXPORT bool xzstd_available(void)
{
	return true;
}

XZSTD_EXPORT const char* xzstd_version(void)
{
	return ZSTD_versionString();
}

XZSTD_EXPORT const char* xzstd_last_error(void)
{
	return xrtErrorMessage(xrtGetError());
}

XZSTD_EXPORT bool xzstd_last_error_is_memory(void)
{
	const xerror* pError = xrtGetError();
	return pError != NULL && xrtErrorKind(pError) == XERR_MEMORY;
}



XZSTD_EXPORT void* xzstd_compress(void* pData, size_t iSize, int iLevel,
	unsigned long long iMaxInput, unsigned long long iMaxOutput)
{
	xrtzstdoptions tOptions;
	xrtzstderror tError;
	xbuffer* pOutput = NULL;
	xrtClearError();
	xzstd_options(&tOptions, iLevel, iMaxInput, iMaxOutput, 0u);
	if ( !xrtZstdCompress(pData, iSize, &tOptions, &pOutput, &tError) ) {
		xzstd_publish_error(&tError);
		return NULL;
	}
	return pOutput;
}

XZSTD_EXPORT void* xzstd_decompress(void* pData, size_t iSize, unsigned long long iMaxInput,
	unsigned long long iMaxOutput, unsigned int iMaxRatio)
{
	xrtzstdoptions tOptions;
	xrtzstderror tError;
	xbuffer* pOutput = NULL;
	xrtClearError();
	xzstd_options(&tOptions, 0, iMaxInput, iMaxOutput, iMaxRatio);
	if ( !xrtZstdDecompress(pData, iSize, &tOptions, &pOutput, &tError) ) {
		xzstd_publish_error(&tError);
		return NULL;
	}
	return pOutput;
}

XZSTD_EXPORT int xzstd_compress_file(const char* sSource, const char* sDestination, int iLevel,
	bool bOverwrite, unsigned long long iMaxInput, unsigned long long iMaxOutput, xvalue* pCallback)
{
	xrtzstdoptions tOptions;
	xrtClearError();
	xzstd_options(&tOptions, iLevel, iMaxInput, iMaxOutput, 0u);
	return xzstd_transform_file(sSource, sDestination, true, bOverwrite, &tOptions, pCallback);
}

XZSTD_EXPORT int xzstd_decompress_file(const char* sSource, const char* sDestination, bool bOverwrite,
	unsigned long long iMaxInput, unsigned long long iMaxOutput, unsigned int iMaxRatio, xvalue* pCallback)
{
	xrtzstdoptions tOptions;
	xrtClearError();
	xzstd_options(&tOptions, 0, iMaxInput, iMaxOutput, iMaxRatio);
	return xzstd_transform_file(sSource, sDestination, false, bOverwrite, &tOptions, pCallback);
}



XZSTD_EXPORT void* xzstd_compressor_create(int iLevel, unsigned long long iMaxInput, unsigned long long iMaxOutput)
{
	xrtzstdoptions tOptions;
	xrtzstdcompressor* pCompressor;
	xrtClearError();
	xzstd_options(&tOptions, iLevel, iMaxInput, iMaxOutput, 0u);
	pCompressor = xrtZstdCompressorCreate(&tOptions);
	if ( pCompressor == NULL ) {
		if ( xrtGetError() == NULL ) {
			xrtSetErrorInfo(XERR_MEMORY, "xlang.extlib.zstd",
				XRT_ZSTD_ERROR_MEMORY, "cannot allocate zstd compressor");
		}
		return NULL;
	}
	if ( xrtZstdCompressorError(pCompressor)->iCode != XRT_ZSTD_OK ) {
		xzstd_publish_error(xrtZstdCompressorError(pCompressor));
		xrtZstdCompressorDestroy(pCompressor);
		return NULL;
	}
	return pCompressor;
}

XZSTD_EXPORT void xzstd_compressor_destroy(void* pHandle)
{
	xrtZstdCompressorDestroy((xrtzstdcompressor*)pHandle);
}

XZSTD_EXPORT bool xzstd_compressor_update(void* pHandle, void* pData, size_t iSize)
{
	xrtzstdcompressor* pCompressor = (xrtzstdcompressor*)pHandle;
	if ( !xrtZstdCompressorUpdate(pCompressor, pData, iSize) ) {
		xzstd_publish_error(xrtZstdCompressorError(pCompressor));
		return false;
	}
	return true;
}

XZSTD_EXPORT bool xzstd_compressor_finish(void* pHandle)
{
	xrtzstdcompressor* pCompressor = (xrtzstdcompressor*)pHandle;
	if ( !xrtZstdCompressorFinish(pCompressor) ) {
		xzstd_publish_error(xrtZstdCompressorError(pCompressor));
		return false;
	}
	return true;
}

XZSTD_EXPORT void* xzstd_compressor_drain(void* pHandle)
{
	xrtzstdcompressor* pCompressor = (xrtzstdcompressor*)pHandle;
	xbuffer* pOutput = xrtZstdCompressorDrainBuffer(pCompressor);
	if ( pOutput == NULL ) {
		xzstd_publish_error(xrtZstdCompressorError(pCompressor));
		return NULL;
	}
	return pOutput;
}

XZSTD_EXPORT unsigned long long xzstd_compressor_input_bytes(void* pHandle)
{
	return xrtZstdCompressorInputBytes((const xrtzstdcompressor*)pHandle);
}

XZSTD_EXPORT unsigned long long xzstd_compressor_output_bytes(void* pHandle)
{
	return xrtZstdCompressorOutputBytes((const xrtzstdcompressor*)pHandle);
}



XZSTD_EXPORT void* xzstd_decompressor_create(unsigned long long iMaxInput, unsigned long long iMaxOutput,
	unsigned int iMaxRatio)
{
	xrtzstdoptions tOptions;
	xrtzstddecompressor* pDecompressor;
	xrtClearError();
	xzstd_options(&tOptions, 0, iMaxInput, iMaxOutput, iMaxRatio);
	pDecompressor = xrtZstdDecompressorCreate(&tOptions);
	if ( pDecompressor == NULL ) {
		if ( xrtGetError() == NULL ) {
			xrtSetErrorInfo(XERR_MEMORY, "xlang.extlib.zstd",
				XRT_ZSTD_ERROR_MEMORY, "cannot allocate zstd decompressor");
		}
		return NULL;
	}
	if ( xrtZstdDecompressorError(pDecompressor)->iCode != XRT_ZSTD_OK ) {
		xzstd_publish_error(xrtZstdDecompressorError(pDecompressor));
		xrtZstdDecompressorDestroy(pDecompressor);
		return NULL;
	}
	return pDecompressor;
}

XZSTD_EXPORT void xzstd_decompressor_destroy(void* pHandle)
{
	xrtZstdDecompressorDestroy((xrtzstddecompressor*)pHandle);
}

XZSTD_EXPORT bool xzstd_decompressor_update(void* pHandle, void* pData, size_t iSize)
{
	xrtzstddecompressor* pDecompressor = (xrtzstddecompressor*)pHandle;
	if ( !xrtZstdDecompressorUpdate(pDecompressor, pData, iSize) ) {
		xzstd_publish_error(xrtZstdDecompressorError(pDecompressor));
		return false;
	}
	return true;
}

XZSTD_EXPORT bool xzstd_decompressor_finish(void* pHandle)
{
	xrtzstddecompressor* pDecompressor = (xrtzstddecompressor*)pHandle;
	if ( !xrtZstdDecompressorFinish(pDecompressor) ) {
		xzstd_publish_error(xrtZstdDecompressorError(pDecompressor));
		return false;
	}
	return true;
}

XZSTD_EXPORT void* xzstd_decompressor_drain(void* pHandle)
{
	xrtzstddecompressor* pDecompressor = (xrtzstddecompressor*)pHandle;
	xbuffer* pOutput = xrtZstdDecompressorDrainBuffer(pDecompressor);
	if ( pOutput == NULL ) {
		xzstd_publish_error(xrtZstdDecompressorError(pDecompressor));
		return NULL;
	}
	return pOutput;
}

XZSTD_EXPORT unsigned long long xzstd_decompressor_input_bytes(void* pHandle)
{
	return xrtZstdDecompressorInputBytes((const xrtzstddecompressor*)pHandle);
}

XZSTD_EXPORT unsigned long long xzstd_decompressor_output_bytes(void* pHandle)
{
	return xrtZstdDecompressorOutputBytes((const xrtzstddecompressor*)pHandle);
}

XZSTD_EXPORT bool xzstd_decompressor_finished(void* pHandle)
{
	return xrtZstdDecompressorFinished((const xrtzstddecompressor*)pHandle);
}

XZSTD_EXPORT unsigned long long xzstd_decompressor_members(void* pHandle)
{
	return xrtZstdDecompressorMembers((const xrtzstddecompressor*)pHandle);
}

XZSTD_EXPORT void* xzstd_compress_text(xstrview text, int level,
    unsigned long long maxInput, unsigned long long maxOutput)
{
    return xzstd_compress((void*)text.Data, text.Size, level, maxInput, maxOutput);
}
XZSTD_EXPORT bool xzstd_compressor_update_text(void* handle, xstrview text)
{
    return xzstd_compressor_update(handle, (void*)text.Data, text.Size);
}
