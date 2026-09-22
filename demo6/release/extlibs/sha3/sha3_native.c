#include "include/xrt_sha3.h"
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE

#include "xruntime.h"
#include "../xlang_extlib_runtime.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#define XSHA3_EXPORT __declspec(dllexport)
#else
#define XSHA3_EXPORT
#endif

#if defined(__GNUC__)
#define XSHA3_THREAD __thread
#elif defined(_MSC_VER)
#define XSHA3_THREAD __declspec(thread)
#else
#define XSHA3_THREAD
#endif

#define XSHA3_HEX_SLOTS 8u
#define XSHA3_MAX_HEX 256u

static XSHA3_THREAD char g_xsha3_hex[XSHA3_HEX_SLOTS][XSHA3_MAX_HEX + 1];
static XSHA3_THREAD size_t g_xsha3_hex_index;
static XSHA3_THREAD int g_xsha3_file_status;

static void xsha3_path_error(const char *prefix, const char *path)
{
	char message[512];
	snprintf(message, sizeof(message), "%s%s", prefix,
		path != NULL ? path : "");
	message[sizeof(message) - 1u] = '\0';
	xl6ExtlibSetError(XERR_IO, "xlang.extlib.sha3", message);
}

static const char* xsha3_format_hex(const uint8* pDigest, size_t iLen)
{
	static const char hex[] = "0123456789abcdef";
	char* sSlot = g_xsha3_hex[g_xsha3_hex_index % XSHA3_HEX_SLOTS];
	size_t i;
	g_xsha3_hex_index = (g_xsha3_hex_index + 1u) % XSHA3_HEX_SLOTS;
	if (iLen * 2 > XSHA3_MAX_HEX) iLen = XSHA3_MAX_HEX / 2;
	for (i = 0; i < iLen; ++i) {
		sSlot[i * 2] = hex[(pDigest[i] >> 4) & 0x0f];
		sSlot[i * 2 + 1] = hex[pDigest[i] & 0x0f];
	}
	sSlot[iLen * 2] = '\0';
	return sSlot;
}

static void xsha3_publish_error(const xrtsha3error* pError)
{
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "sha3 operation failed";
	xl6ExtlibSetError(XERR_VALUE, "xlang.extlib.sha3", sMessage);
}



XSHA3_EXPORT bool xsha3_available(void)
{
	return true;
}

XSHA3_EXPORT const char* xsha3_version(void)
{
	return "3.0.2";
}

XSHA3_EXPORT const char* xsha3_last_error(void)
{
	return xrtGetError();
}

XSHA3_EXPORT int xsha3_file_status(void)
{
	return g_xsha3_file_status;
}



/* ---- 涓€娆℃€у搱甯?---- */

XSHA3_EXPORT const char* xsha3_hash_hex(const void* pData, size_t iSize, int iBits)
{
	uint8 pOut[XRT_SHA3_MAX_OUT];
	xrtsha3error tError;
	size_t iOutLen;
	xrtClearError();
	iOutLen = (size_t)(iBits / 8);
	if (iOutLen == 0 || iOutLen > XRT_SHA3_MAX_OUT) iOutLen = 32;
	if (!xrtSha3Hash(pData, iSize, iBits, pOut, iOutLen, &tError)) {
		xsha3_publish_error(&tError);
		return "";
	}
	return xsha3_format_hex(pOut, iOutLen);
}

XSHA3_EXPORT const char* xsha3_hash_str_hex(const char* sText, size_t iSize, int iBits)
{
	return xsha3_hash_hex((const void*)sText, iSize, iBits);
}

XSHA3_EXPORT void* xsha3_hash_bytes(const void* pData, size_t iSize, int iBits)
{
	uint8* pOut;
	xrtsha3error tError;
	size_t iOutLen;
	xrtClearError();
	iOutLen = (size_t)(iBits / 8);
	if (iOutLen == 0 || iOutLen > XRT_SHA3_MAX_OUT) iOutLen = 32;
	pOut = (uint8*)xrtMalloc(iOutLen);
	if (pOut == NULL) { return NULL; }
	if (!xrtSha3Hash(pData, iSize, iBits, pOut, iOutLen, &tError)) {
		xrtFree(pOut);
		xsha3_publish_error(&tError);
		return NULL;
	}
	return xl6ExtlibBufferTake(pOut, iOutLen);
}

XSHA3_EXPORT void* xsha3_hash_str_bytes(const char* sText, size_t iSize, int iBits)
{
	return xsha3_hash_bytes((const void*)sText, iSize, iBits);
}

XSHA3_EXPORT const char* xshake_hash_hex(const void* pData, size_t iSize, int iStrength,
	size_t iOutLen)
{
	uint8 pOut[XSHA3_MAX_HEX / 2];
	xrtsha3error tError;
	xrtClearError();
	if (iOutLen == 0 || iOutLen > XSHA3_MAX_HEX / 2) iOutLen = 32;
	if (!xrtShakeHash(pData, iSize, iStrength, pOut, iOutLen, &tError)) {
		xsha3_publish_error(&tError);
		return "";
	}
	return xsha3_format_hex(pOut, iOutLen);
}

XSHA3_EXPORT const char* xshake_hash_str_hex(const char* sText, size_t iSize, int iStrength,
	size_t iOutLen)
{
	return xshake_hash_hex((const void*)sText, iSize, iStrength, iOutLen);
}



/* ---- 鏂囦欢鍝堝笇 ---- */

XSHA3_EXPORT const char* xsha3_file_hex(str sPath, unsigned long long iMaxInput, int iBits,
	xvalue* pCallback)
{
	xrtsha3options tOptions;
	xrtsha3error tError;
	xrtprogresscall tProgress;
	xreader* pReader = NULL;
	uint8 pOut[XRT_SHA3_MAX_OUT];
	size_t iOutLen;
	bool bResult;
	xrtClearError();
	g_xsha3_file_status = 0;
	xrtProgressCallInit(&tProgress, pCallback);
	iOutLen = (size_t)(iBits / 8);
	if (iOutLen == 0 || iOutLen > XRT_SHA3_MAX_OUT) iOutLen = 32;
	xrtSha3OptionsInit(&tOptions);
	if (iMaxInput != 0u) { tOptions.tLimits.iMaxInputBytes = (uint64)iMaxInput; }
	if (sPath == NULL || sPath[0] == 0) {
		xl6ExtlibSetError(XERR_ARGUMENT, "xlang.extlib.sha3", "sha3 file path is required");
		return "";
	}
	if (!xrtPathExists(sPath)) {
		xsha3_path_error("sha3 file does not exist: ", sPath);
		return "";
	}
	pReader = xrtReaderOpen(sPath);
	if (pReader == NULL) {
		xsha3_path_error("cannot open sha3 file: ", sPath);
		return "";
	}
	bResult = xrtSha3File(pReader, &tOptions.tLimits, iBits, pOut, iOutLen,
		tProgress.Callback != NULL ? xrtProgressCallInvoke : NULL, &tProgress, &tError);
	xrtReaderDestroy(pReader);
	if (!bResult) {
		if (tProgress.InvokeFailed) { g_xsha3_file_status = -2; }
		else {
			g_xsha3_file_status = tError.iCode == XRT_SHA3_ERROR_CANCELLED ? -1 : 0;
			xsha3_publish_error(&tError);
		}
		return "";
	}
	g_xsha3_file_status = 1;
	return xsha3_format_hex(pOut, iOutLen);
}



/* ---- 娴佸紡鍝堝笇鍣?---- */

XSHA3_EXPORT void* xsha3_hasher_create(int iBits, unsigned long long iMaxInput)
{
	xrtsha3options tOptions;
	xrtsha3hasher* pHasher;
	xrtClearError();
	xrtSha3OptionsInit(&tOptions);
	if (iMaxInput != 0u) { tOptions.tLimits.iMaxInputBytes = (uint64)iMaxInput; }
	pHasher = xrtSha3HasherCreate(&tOptions, iBits);
	if (pHasher == NULL) {
		xl6ExtlibSetError(XERR_MEMORY, "xlang.extlib.sha3", "sha3 hasher create failed");
		return NULL;
	}
	return pHasher;
}

XSHA3_EXPORT void* xsha3_shake_hasher_create(int iStrength, unsigned long long iMaxInput)
{
	xrtsha3options tOptions;
	xrtsha3hasher* pHasher;
	xrtClearError();
	xrtSha3OptionsInit(&tOptions);
	if (iMaxInput != 0u) { tOptions.tLimits.iMaxInputBytes = (uint64)iMaxInput; }
	pHasher = xrtSha3ShakeHasherCreate(&tOptions, iStrength);
	if (pHasher == NULL) {
		xl6ExtlibSetError(XERR_MEMORY, "xlang.extlib.sha3", "shake hasher create failed");
		return NULL;
	}
	return pHasher;
}

XSHA3_EXPORT void xsha3_hasher_destroy(void* pHandle)
{
	xrtSha3HasherDestroy((xrtsha3hasher*)pHandle);
}

XSHA3_EXPORT bool xsha3_hasher_update(void* pHandle, const void* pData, size_t iSize)
{
	xrtsha3hasher* pHasher = (xrtsha3hasher*)pHandle;
	if (!xrtSha3HasherUpdate(pHasher, pData, iSize)) {
		xsha3_publish_error(xrtSha3HasherError(pHasher));
		return false;
	}
	return true;
}

XSHA3_EXPORT bool xsha3_hasher_update_str(void* pHandle, const char* sText, size_t iSize)
{
	return xsha3_hasher_update(pHandle, (const void*)sText, iSize);
}

XSHA3_EXPORT const char* xsha3_hasher_digest_hex(void* pHandle, size_t iOutLen)
{
	xrtsha3hasher* pHasher = (xrtsha3hasher*)pHandle;
	uint8 pOut[XSHA3_MAX_HEX / 2];
	if (iOutLen == 0 || iOutLen > XSHA3_MAX_HEX / 2) iOutLen = 32;
	if (!xrtSha3HasherDigest(pHasher, pOut, iOutLen)) {
		xsha3_publish_error(xrtSha3HasherError(pHasher));
		return "";
	}
	return xsha3_format_hex(pOut, iOutLen);
}

XSHA3_EXPORT void* xsha3_hasher_digest_bytes(void* pHandle, size_t iOutLen)
{
	xrtsha3hasher* pHasher = (xrtsha3hasher*)pHandle;
	uint8* pOut;
	if (iOutLen == 0 || iOutLen > XSHA3_MAX_HEX / 2) iOutLen = 32;
	pOut = (uint8*)xrtMalloc(iOutLen);
	if (pOut == NULL) { return NULL; }
	if (!xrtSha3HasherDigest(pHasher, pOut, iOutLen)) {
		xrtFree(pOut);
		xsha3_publish_error(xrtSha3HasherError(pHasher));
		return NULL;
	}
	return xl6ExtlibBufferTake(pOut, iOutLen);
}

XSHA3_EXPORT bool xsha3_hasher_reset(void* pHandle)
{
	xrtsha3hasher* pHasher = (xrtsha3hasher*)pHandle;
	if (!xrtSha3HasherReset(pHasher)) {
		xsha3_publish_error(xrtSha3HasherError(pHasher));
		return false;
	}
	return true;
}

XSHA3_EXPORT unsigned long long xsha3_hasher_input_bytes(void* pHandle)
{
	return (unsigned long long)xrtSha3HasherInputBytes((const xrtsha3hasher*)pHandle);
}

XSHA3_EXPORT int xsha3_hasher_out_bytes(void* pHandle)
{
	return xrtSha3HasherOutBytes((const xrtsha3hasher*)pHandle);
}

XSHA3_EXPORT void* xsha3_hasher_adopt(void* pHandle)
{
	return pHandle;
}

XSHA3_EXPORT void* xsha3_hasher_clone(void* pHandle)
{
	xrtsha3hasher* pSrc = (xrtsha3hasher*)pHandle;
	if (pSrc == NULL) { return NULL; }
	return xrtSha3HasherClone(pSrc);
}
