/* C 独立编译的声明依赖；包清单同时声明这些模块以参与运行库裁剪。 */
#ifndef XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_BUFFER
#endif
#ifndef XRT_MODULE_IO_FILE
#define XRT_MODULE_IO_FILE
#endif
#ifndef XRT_MODULE_FILE_TEMP
#define XRT_MODULE_FILE_TEMP
#endif
#ifndef XRT_MODULE_ERROR_FORMAT
#define XRT_MODULE_ERROR_FORMAT
#endif
#ifndef XRT_MODULE_TEMP_MEMORY
#define XRT_MODULE_TEMP_MEMORY
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#endif
#include "xruntime.h"
#include "include/xrt_brotli.h"
#include "include/brotli_native.h"
#include "third_party/include/brotli/encode.h"
#include "third_party/include/brotli/decode.h"

#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
#define XBROTLI_EXPORT __declspec(dllexport)
#else
#define XBROTLI_EXPORT
#endif



/* 把 Brotli 模块错误映射为 XRT 通用错误类别。 */
static xerrkind xbrotli_error_kind(int iCode)
{
	switch ( iCode ) {
		case XRT_BROTLI_ERROR_ARGUMENT:
			return XERR_ARGUMENT;
		case XRT_BROTLI_ERROR_MEMORY:
			return XERR_MEMORY;
		case XRT_BROTLI_ERROR_DATA:
		case XRT_BROTLI_ERROR_TRUNCATED:
			return XERR_PROTOCOL;
		case XRT_BROTLI_ERROR_LIMIT:
			return XERR_RANGE;
		case XRT_BROTLI_ERROR_STATE:
			return XERR_STATE;
		case XRT_BROTLI_ERROR_CANCELLED:
			return XERR_CANCELLED;
		default:
			return XERR_IO;
	}
}


/* 发布可被 XLang 和 C 调用方统一读取的结构化错误。 */
static void xbrotli_publish_error(const xrtbrotlierror* pError)
{
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "brotli operation failed";
	int iCode = pError != NULL ? pError->iCode : XRT_BROTLI_ERROR_IO;

	xrtSetErrorInfo(
		xbrotli_error_kind(iCode),
		"xlang.extlib.brotli",
		iCode,
		sMessage
	);
}


/* 用调用参数覆盖 Brotli 默认资源限制。 */
static void xbrotli_options(xrtbrotlioptions* pOptions, int iQuality, int iWindow,
	int iMode, uint64 iMaxInput, uint64 iMaxOutput, uint32 iMaxRatio)
{
	xrtBrotliOptionsInit(pOptions);
	pOptions->iQuality = iQuality;
	pOptions->iWindow = iWindow;
	pOptions->iMode = iMode;
	if ( iMaxInput != 0u ) { pOptions->tLimits.iMaxInputBytes = iMaxInput; }
	if ( iMaxOutput != 0u ) { pOptions->tLimits.iMaxOutputBytes = iMaxOutput; }
	if ( iMaxRatio != 0u ) { pOptions->tLimits.iMaxCompressionRatio = iMaxRatio; }
}


/* 1 成功，0 处理失败，-1 用户取消，-2 回调抛错。 */
static int xbrotli_transform_file(cstr sSource, cstr sDestination, bool bCompress,
	bool bOverwrite, const xrtbrotlioptions* pOptions, xvalue* pCallback)
{
	xrtbrotlierror tError;
	xrtprogresscall tProgress;
	xfile TempFile = NULL;
	xreader* pReader = NULL;
	xwriter* pWriter = NULL;
	str sDirectory = NULL;
	str sTemp = NULL;
	xerror* pSaved;
	bool bResult = false;
	int iResult = 0;

	xrtBrotliErrorInit(&tError);
	xrtProgressCallInit(&tProgress, pCallback);
	pCallback = tProgress.Callback;
	if ( sSource == NULL || sSource[0] == 0 || sDestination == NULL || sDestination[0] == 0 ) {
		xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.brotli",
			XRT_BROTLI_ERROR_ARGUMENT, "brotli source and destination paths are required");
		return 0;
	}
	if ( !bOverwrite ) {
		xfileinfo Info;
		if ( xrtPathStat(sDestination, false, &Info) ) {
			xrtSetErrorInfo(XERR_EXISTS, "xlang.extlib.brotli", XRT_BROTLI_ERROR_IO,
				"brotli destination already exists");
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
		".xrt-brotli-", ".tmp", &sTemp);
	if ( TempFile == NULL ) { goto cleanup; }
	pWriter = xrtWriterTakeFile(&TempFile);
	if ( pWriter == NULL ) { goto cleanup; }

	bResult = bCompress ? xrtBrotliCompressStream(pReader, pWriter, pOptions,
		pCallback != NULL ? xrtProgressCallInvoke : NULL, &tProgress, &tError) :
		xrtBrotliDecompressStream(pReader, pWriter, pOptions,
			pCallback != NULL ? xrtProgressCallInvoke : NULL, &tProgress, &tError);
	if ( !bResult && !tProgress.InvokeFailed && xrtGetError() == NULL ) { xbrotli_publish_error(&tError); }
	if ( bResult && !xrtWriterFlush(pWriter) ) {
		bResult = false;
	}

cleanup:
	/* 清理阶段可以再失败，但不能覆盖第一个 IO、内存或回调错误。 */
	pSaved = xrtTakeError();
	if ( pSaved != NULL ) { bResult = false; }
	if ( pWriter != NULL && !xrtWriterDestroy(pWriter) && bResult ) { bResult = false; pSaved = xrtTakeError(); }
	xrtClearError();
	if ( TempFile != NULL && !xrtClose(TempFile) && bResult ) { bResult = false; pSaved = xrtTakeError(); }
	xrtClearError();
	if ( pReader != NULL && !xrtReaderDestroy(pReader) && bResult ) { bResult = false; pSaved = xrtTakeError(); }
	xrtClearError();
	if ( bResult && !xrtPathRename(sTemp, sDestination, bOverwrite) ) { bResult = false; pSaved = xrtTakeError(); }
	if ( !bResult && sTemp != NULL ) { (void)xrtFileDelete(sTemp); }
	xrtClearError();
	if ( pSaved != NULL ) {
		xrtSetError(pSaved);
		xrtErrorFree(pSaved);
	} else if ( !bResult && !tProgress.InvokeFailed ) {
		xrtSetErrorInfo(XERR_IO, "xlang.extlib.brotli", XRT_BROTLI_ERROR_IO, "brotli file operation failed");
	}
	xrtFree(sDirectory);
	xrtFree(sTemp);
	if ( bResult ) { iResult = 1; }
	else if ( tProgress.InvokeFailed ) { iResult = -2; }
	else if ( tError.iCode == XRT_BROTLI_ERROR_CANCELLED ) { iResult = -1; }
	return iResult;
}

/* 返回当前 XRT 临时区字符串，不使用跨线程共享的可写静态缓冲。 */
static const char* xbrotli_format_version(uint32 iVersion)
{
	char* sVersion = (char*)xrtTemp(32);
	if (sVersion == NULL) { return ""; }
	snprintf(sVersion, 32, "%u.%u.%u",
		(iVersion >> 24) & 0xffu, (iVersion >> 12) & 0xfffu, iVersion & 0xfffu);
	return sVersion;
}



XBROTLI_EXPORT bool xbrotli_available(void)
{
	return true;
}

XBROTLI_EXPORT const char* xbrotli_version(void)
{
	return xbrotli_format_version(BrotliEncoderVersion());
}

XBROTLI_EXPORT const char* xbrotli_last_error(void)
{
	return xrtErrorMessage(xrtGetError());
}



XBROTLI_EXPORT void* xbrotli_compress(void* pData, size_t iSize, int iQuality,
	int iWindow, int iMode, unsigned long long iMaxInput, unsigned long long iMaxOutput)
{
	xrtbrotlioptions tOptions;
	xrtbrotlierror tError;
	xbuffer* pOutput = NULL;
	xrtClearError();
	xbrotli_options(&tOptions, iQuality, iWindow, iMode, iMaxInput, iMaxOutput, 0u);
	if ( !xrtBrotliCompress(pData, iSize, &tOptions, &pOutput, &tError) ) {
		xbrotli_publish_error(&tError);
		return NULL;
	}
	return pOutput;
}

XBROTLI_EXPORT void* xbrotli_compress_str(const char* sText, size_t iSize, int iQuality,
	int iWindow, int iMode, unsigned long long iMaxInput, unsigned long long iMaxOutput)
{
	return xbrotli_compress((void*)sText, iSize, iQuality, iWindow, iMode, iMaxInput, iMaxOutput);
}

XBROTLI_EXPORT void* xbrotli_decompress(void* pData, size_t iSize, unsigned long long iMaxInput,
	unsigned long long iMaxOutput, unsigned int iMaxRatio)
{
	xrtbrotlioptions tOptions;
	xrtbrotlierror tError;
	xbuffer* pOutput = NULL;
	xrtClearError();
	xbrotli_options(&tOptions, BROTLI_DEFAULT_QUALITY, BROTLI_DEFAULT_WINDOW,
		BROTLI_DEFAULT_MODE, iMaxInput, iMaxOutput, iMaxRatio);
	if ( !xrtBrotliDecompress(pData, iSize, &tOptions, &pOutput, &tError) ) {
		xbrotli_publish_error(&tError);
		return NULL;
	}
	return pOutput;
}

XBROTLI_EXPORT int xbrotli_compress_file(cstr sSource, cstr sDestination, int iQuality,
	int iWindow, int iMode, bool bOverwrite, unsigned long long iMaxInput, unsigned long long iMaxOutput,
	xvalue* pCallback)
{
	xrtbrotlioptions tOptions;
	xrtClearError();
	xbrotli_options(&tOptions, iQuality, iWindow, iMode, iMaxInput, iMaxOutput, 0u);
	return xbrotli_transform_file(sSource, sDestination, true, bOverwrite, &tOptions, pCallback);
}

XBROTLI_EXPORT int xbrotli_decompress_file(cstr sSource, cstr sDestination, bool bOverwrite,
	unsigned long long iMaxInput, unsigned long long iMaxOutput, unsigned int iMaxRatio, xvalue* pCallback)
{
	xrtbrotlioptions tOptions;
	xrtClearError();
	xbrotli_options(&tOptions, BROTLI_DEFAULT_QUALITY, BROTLI_DEFAULT_WINDOW,
		BROTLI_DEFAULT_MODE, iMaxInput, iMaxOutput, iMaxRatio);
	return xbrotli_transform_file(sSource, sDestination, false, bOverwrite, &tOptions, pCallback);
}



XBROTLI_EXPORT void* xbrotli_compressor_create(int iQuality, int iWindow, int iMode,
	unsigned long long iMaxInput, unsigned long long iMaxOutput)
{
	xrtbrotlioptions tOptions;
	xrtbrotlicompressor* pCompressor;
	xrtClearError();
	xbrotli_options(&tOptions, iQuality, iWindow, iMode, iMaxInput, iMaxOutput, 0u);
	pCompressor = xrtBrotliCompressorCreate(&tOptions);
	if (pCompressor == NULL) { return NULL; }
	if ( xrtBrotliCompressorError(pCompressor)->iCode != XRT_BROTLI_OK ) {
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

XBROTLI_EXPORT bool xbrotli_compressor_update(void* pHandle, void* pData, size_t iSize)
{
	xrtbrotlicompressor* pCompressor = (xrtbrotlicompressor*)pHandle;
	if ( !xrtBrotliCompressorUpdate(pCompressor, pData, iSize) ) {
		xbrotli_publish_error(xrtBrotliCompressorError(pCompressor));
		return false;
	}
	return true;
}

XBROTLI_EXPORT bool xbrotli_compressor_update_str(void* pHandle, const char* sText, size_t iSize)
{
	return xbrotli_compressor_update(pHandle, (void*)sText, iSize);
}

XBROTLI_EXPORT bool xbrotli_compressor_finish(void* pHandle)
{
	xrtbrotlicompressor* pCompressor = (xrtbrotlicompressor*)pHandle;
	if ( !xrtBrotliCompressorFinish(pCompressor) ) {
		xbrotli_publish_error(xrtBrotliCompressorError(pCompressor));
		return false;
	}
	return true;
}

XBROTLI_EXPORT void* xbrotli_compressor_drain(void* pHandle)
{
	xrtbrotlicompressor* pCompressor = (xrtbrotlicompressor*)pHandle;
	xbuffer* pOutput = xrtBrotliCompressorDrainBuffer(pCompressor);
	if ( pOutput == NULL ) {
		xbrotli_publish_error(xrtBrotliCompressorError(pCompressor));
		return NULL;
	}
	return pOutput;
}

XBROTLI_EXPORT unsigned long long xbrotli_compressor_input_bytes(void* pHandle)
{
	return xrtBrotliCompressorInputBytes((const xrtbrotlicompressor*)pHandle);
}

XBROTLI_EXPORT unsigned long long xbrotli_compressor_output_bytes(void* pHandle)
{
	return xrtBrotliCompressorOutputBytes((const xrtbrotlicompressor*)pHandle);
}



XBROTLI_EXPORT void* xbrotli_decompressor_create(unsigned long long iMaxInput, unsigned long long iMaxOutput,
	unsigned int iMaxRatio)
{
	xrtbrotlioptions tOptions;
	xrtbrotlidecompressor* pDecompressor;
	xrtClearError();
	xbrotli_options(&tOptions, BROTLI_DEFAULT_QUALITY, BROTLI_DEFAULT_WINDOW,
		BROTLI_DEFAULT_MODE, iMaxInput, iMaxOutput, iMaxRatio);
	pDecompressor = xrtBrotliDecompressorCreate(&tOptions);
	if (pDecompressor == NULL) { return NULL; }
	if ( xrtBrotliDecompressorError(pDecompressor)->iCode != XRT_BROTLI_OK ) {
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

XBROTLI_EXPORT bool xbrotli_decompressor_update(void* pHandle, void* pData, size_t iSize)
{
	xrtbrotlidecompressor* pDecompressor = (xrtbrotlidecompressor*)pHandle;
	if ( !xrtBrotliDecompressorUpdate(pDecompressor, pData, iSize) ) {
		xbrotli_publish_error(xrtBrotliDecompressorError(pDecompressor));
		return false;
	}
	return true;
}

XBROTLI_EXPORT bool xbrotli_decompressor_finish(void* pHandle)
{
	xrtbrotlidecompressor* pDecompressor = (xrtbrotlidecompressor*)pHandle;
	if ( !xrtBrotliDecompressorFinish(pDecompressor) ) {
		xbrotli_publish_error(xrtBrotliDecompressorError(pDecompressor));
		return false;
	}
	return true;
}

XBROTLI_EXPORT void* xbrotli_decompressor_drain(void* pHandle)
{
	xrtbrotlidecompressor* pDecompressor = (xrtbrotlidecompressor*)pHandle;
	xbuffer* pOutput = xrtBrotliDecompressorDrainBuffer(pDecompressor);
	if ( pOutput == NULL ) {
		xbrotli_publish_error(xrtBrotliDecompressorError(pDecompressor));
		return NULL;
	}
	return pOutput;
}

XBROTLI_EXPORT unsigned long long xbrotli_decompressor_input_bytes(void* pHandle)
{
	return xrtBrotliDecompressorInputBytes((const xrtbrotlidecompressor*)pHandle);
}

XBROTLI_EXPORT unsigned long long xbrotli_decompressor_output_bytes(void* pHandle)
{
	return xrtBrotliDecompressorOutputBytes((const xrtbrotlidecompressor*)pHandle);
}

XBROTLI_EXPORT bool xbrotli_decompressor_finished(void* pHandle)
{
	return xrtBrotliDecompressorFinished((const xrtbrotlidecompressor*)pHandle);
}

XBROTLI_EXPORT unsigned long long xbrotli_decompressor_members(void* pHandle)
{
	return xrtBrotliDecompressorMembers((const xrtbrotlidecompressor*)pHandle);
}
