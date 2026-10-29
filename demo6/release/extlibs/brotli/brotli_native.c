/* C 独立编译的声明依赖；包清单同时声明这些模块以参与运行库裁剪。 */
#ifndef XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_BUFFER
#endif
#ifndef XRT_MODULE_ERROR_FORMAT
#define XRT_MODULE_ERROR_FORMAT
#endif
#ifndef XRT_MODULE_TEMP_MEMORY
#define XRT_MODULE_TEMP_MEMORY
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#endif
#include "xruntime.h"
#include "include/xrt_brotli.h"
#include "include/brotli_native.h"
#include "brotli_error.h"
#include "third_party/include/brotli/encode.h"
#include "third_party/include/brotli/decode.h"

#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
#define XBROTLI_EXPORT __declspec(dllexport)
#else
#define XBROTLI_EXPORT
#endif



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


/* 无结果的 C callable 属于回调失败，不能伪装成普通用户取消。 */
static bool xbrotli_progress(const xrtprogress* value, ptr user)
{
	xrtprogresscall* state = user;
	bool ok = xrtProgressCallInvoke(value, state);
	if (!ok && state->InvokeFailed && xrtGetError() == NULL)
		xrtSetErrorInfo(XERR_STATE, "xlang.extlib.brotli", XRT_BROTLI_ERROR_STATE,
			"brotli progress callback failed to return a boolean");
	return ok;
}

/* 只适配回调和状态：1 成功，0 失败，-1 取消，-2 回调抛错。 */
static int xbrotli_transform_file(cstr sSource, cstr sDestination, bool bCompress,
	bool bOverwrite, const xrtbrotlioptions* pOptions, xvalue* pCallback)
{
	xrtprogresscall tProgress;
	xrtProgressCallInit(&tProgress, pCallback);
	bool ok = bCompress ? xrtBrotliCompressFile(sSource, sDestination, pOptions, bOverwrite,
		tProgress.Callback != NULL ? xbrotli_progress : NULL, &tProgress) :
		xrtBrotliDecompressFile(sSource, sDestination, pOptions, bOverwrite,
		tProgress.Callback != NULL ? xbrotli_progress : NULL, &tProgress);
	if (ok) return 1;
	if (tProgress.InvokeFailed) return -2;
	return xrtErrorKind(xrtGetError()) == XERR_CANCELLED ? -1 : 0;
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
		brotli_publish_error(&tError);
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
		brotli_publish_error(&tError);
		return NULL;
	}
	return pOutput;
}


/* 解压文本取走 XRT 缓冲；追加末尾 NUL 时容量不足可能扩容。 */
XBROTLI_EXPORT void* xbrotli_decompress_text(void* pData, size_t iSize,
	unsigned long long iMaxInput, unsigned long long iMaxOutput, unsigned int iMaxRatio)
{
	xbuffer* pOutput = (xbuffer*)xbrotli_decompress(pData, iSize, iMaxInput, iMaxOutput, iMaxRatio);
	if ( pOutput == NULL ) { return NULL; }
	if ( !xrtUtf8Valid((xstrview){(const char*)pOutput->Data, pOutput->Size}, NULL) ) {
		xrtSetErrorInfo(XERR_VALUE, "xlang.extlib.brotli", 0,
			"brotli decompressed text is not valid UTF-8");
		xrtBufferDestroy(pOutput);
		return NULL;
	}
	if ( !xrtBufferAppendByte(pOutput, 0) ) {
		xrtBufferDestroy(pOutput);
		return NULL;
	}
	return pOutput;
}

XBROTLI_EXPORT size_t xbrotli_text_size(void* pHandle)
{
	const xbuffer* pOutput = (const xbuffer*)pHandle;
	return pOutput != NULL && pOutput->Size != 0u ? pOutput->Size - 1u : 0u;
}

XBROTLI_EXPORT void* xbrotli_text_take(void* pHandle)
{
	xbuffer* pOutput = (xbuffer*)pHandle;
	void* pText;
	if ( pOutput == NULL ) { return NULL; }
	pText = xrtBufferTake(pOutput, NULL, NULL);
	xrtBufferDestroy(pOutput);
	return pText;
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
		brotli_publish_error(xrtBrotliCompressorError(pCompressor));
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
		brotli_publish_error(xrtBrotliCompressorError(pCompressor));
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
		brotli_publish_error(xrtBrotliCompressorError(pCompressor));
		return false;
	}
	return true;
}

XBROTLI_EXPORT void* xbrotli_compressor_drain(void* pHandle)
{
	xrtbrotlicompressor* pCompressor = (xrtbrotlicompressor*)pHandle;
	xbuffer* pOutput = xrtBrotliCompressorDrainBuffer(pCompressor);
	if ( pOutput == NULL ) {
		brotli_publish_error(xrtBrotliCompressorError(pCompressor));
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
		brotli_publish_error(xrtBrotliDecompressorError(pDecompressor));
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
		brotli_publish_error(xrtBrotliDecompressorError(pDecompressor));
		return false;
	}
	return true;
}

XBROTLI_EXPORT bool xbrotli_decompressor_finish(void* pHandle)
{
	xrtbrotlidecompressor* pDecompressor = (xrtbrotlidecompressor*)pHandle;
	if ( !xrtBrotliDecompressorFinish(pDecompressor) ) {
		brotli_publish_error(xrtBrotliDecompressorError(pDecompressor));
		return false;
	}
	return true;
}

XBROTLI_EXPORT void* xbrotli_decompressor_drain(void* pHandle)
{
	xrtbrotlidecompressor* pDecompressor = (xrtbrotlidecompressor*)pHandle;
	xbuffer* pOutput = xrtBrotliDecompressorDrainBuffer(pDecompressor);
	if ( pOutput == NULL ) {
		brotli_publish_error(xrtBrotliDecompressorError(pDecompressor));
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
