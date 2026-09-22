#define XRT_MODULE_BUFFER
#define XRT_MODULE_ERROR_FORMAT
#define XRT_MODULE_IO_FILE
#define XRT_MODULE_PATH
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE

#include "xruntime.h"
#include "include/xrt_md5.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#define XMD5_EXPORT __declspec(dllexport)
#else
#define XMD5_EXPORT
#endif

#if defined(__GNUC__)
#define XMD5_THREAD __thread
#elif defined(_MSC_VER)
#define XMD5_THREAD __declspec(thread)
#else
#define XMD5_THREAD
#endif

#define XMD5_HEX_SLOTS 8u

static XMD5_THREAD char g_xmd5_hex[XMD5_HEX_SLOTS][33];
static XMD5_THREAD size_t g_xmd5_hex_index;
static XMD5_THREAD int g_xmd5_file_status;


/* 把 MD5 模块错误映射为 XRT 通用错误类别。 */
static xerrkind xmd5_error_kind(int iCode)
{
	switch ( iCode ) {
		case XRT_MD5_ERROR_ARGUMENT:
			return XERR_ARGUMENT;
		case XRT_MD5_ERROR_MEMORY:
			return XERR_MEMORY;
		case XRT_MD5_ERROR_LIMIT:
			return XERR_RANGE;
		case XRT_MD5_ERROR_STATE:
			return XERR_STATE;
		case XRT_MD5_ERROR_CANCELLED:
			return XERR_CANCELLED;
		default:
			return XERR_IO;
	}
}


/* 把已分配摘要内存移交给 Buffer，创建失败时释放来源。 */
static xbuffer* xmd5_take_buffer(uint8* pData, size_t iSize)
{
	bytes pOwned = pData;
	xbuffer* pBuffer = xrtBufferCreateTake(&pOwned, iSize, iSize);

	xrtFree(pOwned);
	return pBuffer;
}

static const char* xmd5_format_digest(const uint8 pDigest[XRT_MD5_DIGEST_SIZE])
{
	static const char hex[] = "0123456789abcdef";
	char* sSlot = g_xmd5_hex[g_xmd5_hex_index % XMD5_HEX_SLOTS];
	int i;
	g_xmd5_hex_index = (g_xmd5_hex_index + 1u) % XMD5_HEX_SLOTS;
	for (i = 0; i < (int)XRT_MD5_DIGEST_SIZE; ++i) {
		sSlot[i * 2] = hex[(pDigest[i] >> 4) & 0x0f];
		sSlot[i * 2 + 1] = hex[pDigest[i] & 0x0f];
	}
	sSlot[32] = '\0';
	return sSlot;
}

static void xmd5_publish_error(const xrtmd5error* pError)
{
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "md5 operation failed";
	int iCode = pError != NULL ? pError->iCode : XRT_MD5_ERROR_IO;

	xrtSetErrorInfo(
		xmd5_error_kind(iCode),
		"xlang.extlib.md5",
		iCode,
		sMessage
	);
}



XMD5_EXPORT bool xmd5_available(void)
{
	return true;
}

XMD5_EXPORT const char* xmd5_version(void)
{
	return "1.0.0";
}

XMD5_EXPORT const char* xmd5_last_error(void)
{
	return xrtErrorMessage(xrtGetError());
}

XMD5_EXPORT int xmd5_file_status(void)
{
	return g_xmd5_file_status;
}



/* ---- 一次性哈希 ---- */

XMD5_EXPORT const char* xmd5_hash_hex(const void* pData, size_t iSize)
{
	uint8 pDigest[XRT_MD5_DIGEST_SIZE];
	xrtmd5error tError;
	xrtClearError();
	if (!xrtMd5Hash(pData, iSize, pDigest, &tError)) {
		xmd5_publish_error(&tError);
		return "";
	}
	return xmd5_format_digest(pDigest);
}

XMD5_EXPORT const char* xmd5_hash_str_hex(const char* sText, size_t iSize)
{
	return xmd5_hash_hex((const void*)sText, iSize);
}

XMD5_EXPORT void* xmd5_hash_bytes(const void* pData, size_t iSize)
{
	uint8* pOut;
	xrtmd5error tError;
	xrtClearError();
	pOut = (uint8*)xrtMalloc(XRT_MD5_DIGEST_SIZE);
	if (pOut == NULL) { return NULL; }
	if (!xrtMd5Hash(pData, iSize, pOut, &tError)) {
		xrtFree(pOut);
		xmd5_publish_error(&tError);
		return NULL;
	}
	return xmd5_take_buffer(pOut, XRT_MD5_DIGEST_SIZE);
}

XMD5_EXPORT void* xmd5_hash_str_bytes(const char* sText, size_t iSize)
{
	return xmd5_hash_bytes((const void*)sText, iSize);
}



/* ---- 文件哈希 ---- */

static bool xmd5_file_options(str sPath, uint64 iMaxInput, xrtmd5options* pOptions)
{
	xrtMd5OptionsInit(pOptions);
	if (iMaxInput != 0u) { pOptions->tLimits.iMaxInputBytes = iMaxInput; }
	if (sPath == NULL || sPath[0] == 0) {
		xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.md5",
			XRT_MD5_ERROR_ARGUMENT, "md5 file path is required");
		return false;
	}
	if (!xrtPathExists(sPath)) {
		xrtSetErrorFormat(XERR_NOT_FOUND, "xlang.extlib.md5",
			XRT_MD5_ERROR_IO, "md5 file does not exist: %s", sPath);
		return false;
	}
	return true;
}

XMD5_EXPORT const char* xmd5_file_hex(str sPath, unsigned long long iMaxInput,
	xvalue* pCallback)
{
	xrtmd5options tOptions;
	xrtmd5error tError;
	xrtprogresscall tProgress;
	xreader* pReader = NULL;
	uint8 pDigest[XRT_MD5_DIGEST_SIZE];
	bool bResult;
	xrtClearError();
	g_xmd5_file_status = 0;
	xrtProgressCallInit(&tProgress, pCallback);
	if (!xmd5_file_options(sPath, (uint64)iMaxInput, &tOptions)) { return ""; }
	pReader = xrtReaderOpen(sPath);
	if (pReader == NULL) {
		xrtSetErrorFormat(XERR_IO, "xlang.extlib.md5",
			XRT_MD5_ERROR_IO, "cannot open md5 file: %s", sPath);
		return "";
	}
	bResult = xrtMd5File(pReader, &tOptions.tLimits, pDigest,
		tProgress.Callback != NULL ? xrtProgressCallInvoke : NULL, &tProgress, &tError);
	xrtReaderDestroy(pReader);
	if (!bResult) {
		if (tProgress.InvokeFailed) { g_xmd5_file_status = -2; }
		else {
			g_xmd5_file_status = tError.iCode == XRT_MD5_ERROR_CANCELLED ? -1 : 0;
			xmd5_publish_error(&tError);
		}
		return "";
	}
	g_xmd5_file_status = 1;
	return xmd5_format_digest(pDigest);
}

XMD5_EXPORT void* xmd5_file_bytes(str sPath, unsigned long long iMaxInput,
	xvalue* pCallback)
{
	xrtmd5options tOptions;
	xrtmd5error tError;
	xrtprogresscall tProgress;
	xreader* pReader = NULL;
	uint8* pOut;
	bool bResult;
	xrtClearError();
	g_xmd5_file_status = 0;
	xrtProgressCallInit(&tProgress, pCallback);
	if (!xmd5_file_options(sPath, (uint64)iMaxInput, &tOptions)) { return NULL; }
	pReader = xrtReaderOpen(sPath);
	if (pReader == NULL) {
		xrtSetErrorFormat(XERR_IO, "xlang.extlib.md5",
			XRT_MD5_ERROR_IO, "cannot open md5 file: %s", sPath);
		return NULL;
	}
	pOut = (uint8*)xrtMalloc(XRT_MD5_DIGEST_SIZE);
	if (pOut == NULL) { xrtReaderDestroy(pReader); return NULL; }
	bResult = xrtMd5File(pReader, &tOptions.tLimits, pOut,
		tProgress.Callback != NULL ? xrtProgressCallInvoke : NULL, &tProgress, &tError);
	xrtReaderDestroy(pReader);
	if (!bResult) {
		xrtFree(pOut);
		if (tProgress.InvokeFailed) { g_xmd5_file_status = -2; }
		else {
			g_xmd5_file_status = tError.iCode == XRT_MD5_ERROR_CANCELLED ? -1 : 0;
			xmd5_publish_error(&tError);
		}
		return NULL;
	}
	g_xmd5_file_status = 1;
	return xmd5_take_buffer(pOut, XRT_MD5_DIGEST_SIZE);
}



/* ---- 流式哈希器 ---- */

XMD5_EXPORT void* xmd5_hasher_create(unsigned long long iMaxInput)
{
	xrtmd5options tOptions;
	xrtmd5hasher* pHasher;
	xrtClearError();
	xrtMd5OptionsInit(&tOptions);
	if (iMaxInput != 0u) { tOptions.tLimits.iMaxInputBytes = (uint64)iMaxInput; }
	pHasher = xrtMd5HasherCreate(&tOptions);
	if (pHasher == NULL || xrtMd5HasherError(pHasher)->iCode != XRT_MD5_OK) {
		xmd5_publish_error(xrtMd5HasherError(pHasher));
		xrtMd5HasherDestroy(pHasher);
		return NULL;
	}
	return pHasher;
}

XMD5_EXPORT void xmd5_hasher_destroy(void* pHandle)
{
	xrtMd5HasherDestroy((xrtmd5hasher*)pHandle);
}

XMD5_EXPORT bool xmd5_hasher_update(void* pHandle, const void* pData, size_t iSize)
{
	xrtmd5hasher* pHasher = (xrtmd5hasher*)pHandle;
	if (!xrtMd5HasherUpdate(pHasher, pData, iSize)) {
		xmd5_publish_error(xrtMd5HasherError(pHasher));
		return false;
	}
	return true;
}

XMD5_EXPORT bool xmd5_hasher_update_str(void* pHandle, const char* sText, size_t iSize)
{
	return xmd5_hasher_update(pHandle, (const void*)sText, iSize);
}

XMD5_EXPORT const char* xmd5_hasher_digest_hex(void* pHandle)
{
	xrtmd5hasher* pHasher = (xrtmd5hasher*)pHandle;
	uint8 pDigest[XRT_MD5_DIGEST_SIZE];
	if (!xrtMd5HasherDigest(pHasher, pDigest)) {
		xmd5_publish_error(xrtMd5HasherError(pHasher));
		return "";
	}
	return xmd5_format_digest(pDigest);
}

XMD5_EXPORT void* xmd5_hasher_digest_bytes(void* pHandle)
{
	xrtmd5hasher* pHasher = (xrtmd5hasher*)pHandle;
	uint8* pOut;
	pOut = (uint8*)xrtMalloc(XRT_MD5_DIGEST_SIZE);
	if (pOut == NULL) { return NULL; }
	if (!xrtMd5HasherDigest(pHasher, pOut)) {
		xrtFree(pOut);
		xmd5_publish_error(xrtMd5HasherError(pHasher));
		return NULL;
	}
	return xmd5_take_buffer(pOut, XRT_MD5_DIGEST_SIZE);
}

XMD5_EXPORT bool xmd5_hasher_reset(void* pHandle)
{
	xrtmd5hasher* pHasher = (xrtmd5hasher*)pHandle;
	if (!xrtMd5HasherReset(pHasher)) {
		xmd5_publish_error(xrtMd5HasherError(pHasher));
		return false;
	}
	return true;
}

XMD5_EXPORT unsigned long long xmd5_hasher_input_bytes(void* pHandle)
{
	return (unsigned long long)xrtMd5HasherInputBytes((const xrtmd5hasher*)pHandle);
}
