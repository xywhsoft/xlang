#define XRT_MODULE_ERROR_FORMAT
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE

#include "xruntime.h"
#include "include/xrt_crc32.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#define XCRC32_EXPORT __declspec(dllexport)
#else
#define XCRC32_EXPORT
#endif

#if defined(__GNUC__)
#define XCRC32_THREAD __thread
#elif defined(_MSC_VER)
#define XCRC32_THREAD __declspec(thread)
#else
#define XCRC32_THREAD
#endif

#define XCRC32_HEX_SLOTS 8u

static XCRC32_THREAD char g_xcrc32_hex[XCRC32_HEX_SLOTS][12];
static XCRC32_THREAD size_t g_xcrc32_hex_index;
static XCRC32_THREAD int g_xcrc32_file_status;


/* 把 CRC32 模块错误映射为 XRT 通用错误类别。 */
static xerrkind xcrc32_error_kind(int iCode)
{
	switch ( iCode ) {
		case XRT_CRC32_ERROR_ARGUMENT:
			return XERR_ARGUMENT;
		case XRT_CRC32_ERROR_MEMORY:
			return XERR_MEMORY;
		case XRT_CRC32_ERROR_LIMIT:
			return XERR_RANGE;
		case XRT_CRC32_ERROR_STATE:
			return XERR_STATE;
		case XRT_CRC32_ERROR_CANCELLED:
			return XERR_CANCELLED;
		default:
			return XERR_IO;
	}
}


static const char* xcrc32_format32(uint32 iValue)
{
	char* sSlot = g_xcrc32_hex[g_xcrc32_hex_index % XCRC32_HEX_SLOTS];
	g_xcrc32_hex_index = (g_xcrc32_hex_index + 1u) % XCRC32_HEX_SLOTS;
	snprintf(sSlot, sizeof(g_xcrc32_hex[0]), "%08x", (unsigned)iValue);
	return sSlot;
}

static void xcrc32_publish_error(const xrtcrc32error* pError)
{
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "crc32 operation failed";
	int iCode = pError != NULL ? pError->iCode : XRT_CRC32_ERROR_IO;

	xrtSetErrorInfo(
		xcrc32_error_kind(iCode),
		"xlang.extlib.crc32",
		iCode,
		sMessage
	);
}



XCRC32_EXPORT bool xcrc32_available(void)
{
	return true;
}

XCRC32_EXPORT const char* xcrc32_version(void)
{
	return "1.0.0";
}

XCRC32_EXPORT const char* xcrc32_last_error(void)
{
	return xrtErrorMessage(xrtGetError());
}

XCRC32_EXPORT int xcrc32_file_status(void)
{
	return g_xcrc32_file_status;
}



/* ---- 一次性哈希 ---- */

XCRC32_EXPORT long long xcrc32_hash(const void* pData, size_t iSize)
{
	uint32 iCrc = 0u;
	xrtcrc32error tError;
	xrtClearError();
	if (!xrtCrc32Hash(pData, iSize, &iCrc, &tError)) {
		xcrc32_publish_error(&tError);
		return 0u;
	}
	return (unsigned int)iCrc;
}

XCRC32_EXPORT const char* xcrc32_hash_hex(const void* pData, size_t iSize)
{
	uint32 iCrc = 0u;
	xrtcrc32error tError;
	xrtClearError();
	if (!xrtCrc32Hash(pData, iSize, &iCrc, &tError)) {
		xcrc32_publish_error(&tError);
		return "";
	}
	return xcrc32_format32(iCrc);
}

XCRC32_EXPORT long long xcrc32_hash_str(const char* sText, size_t iSize)
{
	return xcrc32_hash((const void*)sText, iSize);
}

XCRC32_EXPORT const char* xcrc32_hash_str_hex(const char* sText, size_t iSize)
{
	return xcrc32_hash_hex((const void*)sText, iSize);
}



/* ---- 文件哈希 ---- */

static bool xcrc32_file_options(str sPath, uint64 iMaxInput, xrtcrc32options* pOptions)
{
	xrtCrc32OptionsInit(pOptions);
	if (iMaxInput != 0u) { pOptions->tLimits.iMaxInputBytes = iMaxInput; }
	if (sPath == NULL || sPath[0] == 0) {
		xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.crc32",
			XRT_CRC32_ERROR_ARGUMENT, "crc32 file path is required");
		return false;
	}
	if (!xrtPathExists(sPath)) {
		xrtSetErrorFormat(XERR_NOT_FOUND, "xlang.extlib.crc32",
			XRT_CRC32_ERROR_IO, "crc32 file does not exist: %s", sPath);
		return false;
	}
	return true;
}

XCRC32_EXPORT long long xcrc32_file(str sPath, unsigned long long iMaxInput,
	xvalue* pCallback)
{
	xrtcrc32options tOptions;
	xrtcrc32error tError;
	xrtprogresscall tProgress;
	xreader* pReader = NULL;
	uint32 iCrc = 0u;
	bool bResult;
	xrtClearError();
	g_xcrc32_file_status = 0;
	xrtProgressCallInit(&tProgress, pCallback);
	if (!xcrc32_file_options(sPath, (uint64)iMaxInput, &tOptions)) { return 0u; }
	pReader = xrtReaderOpen(sPath);
	if (pReader == NULL) {
		xrtSetErrorFormat(XERR_IO, "xlang.extlib.crc32",
			XRT_CRC32_ERROR_IO, "cannot open crc32 file: %s", sPath);
		return 0u;
	}
	bResult = xrtCrc32File(pReader, &tOptions.tLimits, &iCrc,
		tProgress.Callback != NULL ? xrtProgressCallInvoke : NULL, &tProgress, &tError);
	xrtReaderDestroy(pReader);
	if (!bResult) {
		if (tProgress.InvokeFailed) { g_xcrc32_file_status = -2; }
		else {
			g_xcrc32_file_status = tError.iCode == XRT_CRC32_ERROR_CANCELLED ? -1 : 0;
			xcrc32_publish_error(&tError);
		}
		return 0u;
	}
	g_xcrc32_file_status = 1;
	return (long long)iCrc;
}

XCRC32_EXPORT const char* xcrc32_file_hex(str sPath, unsigned long long iMaxInput,
	xvalue* pCallback)
{
	uint32 iCrc = (uint32)xcrc32_file(sPath, iMaxInput, pCallback);
	if (g_xcrc32_file_status != 1) { return ""; }
	return xcrc32_format32((uint32)iCrc);
}



/* ---- 流式哈希器 ---- */

XCRC32_EXPORT void* xcrc32_hasher_create(unsigned long long iMaxInput)
{
	xrtcrc32options tOptions;
	xrtcrc32hasher* pHasher;
	xrtClearError();
	xrtCrc32OptionsInit(&tOptions);
	if (iMaxInput != 0u) { tOptions.tLimits.iMaxInputBytes = (uint64)iMaxInput; }
	pHasher = xrtCrc32HasherCreate(&tOptions);
	if (pHasher == NULL || xrtCrc32HasherError(pHasher)->iCode != XRT_CRC32_OK) {
		xcrc32_publish_error(xrtCrc32HasherError(pHasher));
		xrtCrc32HasherDestroy(pHasher);
		return NULL;
	}
	return pHasher;
}

XCRC32_EXPORT void xcrc32_hasher_destroy(void* pHandle)
{
	xrtCrc32HasherDestroy((xrtcrc32hasher*)pHandle);
}

XCRC32_EXPORT bool xcrc32_hasher_update(void* pHandle, const void* pData, size_t iSize)
{
	xrtcrc32hasher* pHasher = (xrtcrc32hasher*)pHandle;
	if (!xrtCrc32HasherUpdate(pHasher, pData, iSize)) {
		xcrc32_publish_error(xrtCrc32HasherError(pHasher));
		return false;
	}
	return true;
}

XCRC32_EXPORT bool xcrc32_hasher_update_str(void* pHandle, const char* sText, size_t iSize)
{
	return xcrc32_hasher_update(pHandle, (const void*)sText, iSize);
}

XCRC32_EXPORT long long xcrc32_hasher_digest(void* pHandle)
{
	xrtcrc32hasher* pHasher = (xrtcrc32hasher*)pHandle;
	uint32 iCrc = 0u;
	if (!xrtCrc32HasherDigest(pHasher, &iCrc)) {
		xcrc32_publish_error(xrtCrc32HasherError(pHasher));
		return 0u;
	}
	return (unsigned int)iCrc;
}

XCRC32_EXPORT const char* xcrc32_hasher_digest_hex(void* pHandle)
{
	xrtcrc32hasher* pHasher = (xrtcrc32hasher*)pHandle;
	uint32 iCrc = 0u;
	if (!xrtCrc32HasherDigest(pHasher, &iCrc)) {
		xcrc32_publish_error(xrtCrc32HasherError(pHasher));
		return "";
	}
	return xcrc32_format32(iCrc);
}

XCRC32_EXPORT bool xcrc32_hasher_reset(void* pHandle)
{
	xrtcrc32hasher* pHasher = (xrtcrc32hasher*)pHandle;
	if (!xrtCrc32HasherReset(pHasher)) {
		xcrc32_publish_error(xrtCrc32HasherError(pHasher));
		return false;
	}
	return true;
}

XCRC32_EXPORT long long xcrc32_hasher_input_bytes(void* pHandle)
{
	return (unsigned long long)xrtCrc32HasherInputBytes((const xrtcrc32hasher*)pHandle);
}
