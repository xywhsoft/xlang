#include "include/xrt_murmur3.h"
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE

#include "xruntime.h"
#include "../xlang_extlib_runtime.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#define XMURMUR3_EXPORT __declspec(dllexport)
#else
#define XMURMUR3_EXPORT
#endif

#if defined(__GNUC__)
#define XMURMUR3_THREAD __thread
#elif defined(_MSC_VER)
#define XMURMUR3_THREAD __declspec(thread)
#else
#define XMURMUR3_THREAD
#endif

#define XMURMUR3_HEX_SLOTS 8u

static XMURMUR3_THREAD char g_xmurmur3_hex[XMURMUR3_HEX_SLOTS][33];
static XMURMUR3_THREAD size_t g_xmurmur3_hex_index;
static XMURMUR3_THREAD int g_xmurmur3_file_status;

static void xmurmur3_path_error(const char *prefix, const char *path)
{
	char message[512];
	snprintf(message, sizeof(message), "%s%s", prefix,
		path != NULL ? path : "");
	message[sizeof(message) - 1u] = '\0';
	xl6ExtlibSetError(XERR_IO, "xlang.extlib.murmur3", message);
}

static const char* xmurmur3_format_hex(const uint8* pDigest, size_t iLen)
{
	static const char hex[] = "0123456789abcdef";
	char* sSlot = g_xmurmur3_hex[g_xmurmur3_hex_index % XMURMUR3_HEX_SLOTS];
	size_t i;
	g_xmurmur3_hex_index = (g_xmurmur3_hex_index + 1u) % XMURMUR3_HEX_SLOTS;
	if (iLen > 16) iLen = 16;
	for (i = 0; i < iLen; ++i) {
		sSlot[i * 2] = hex[(pDigest[i] >> 4) & 0x0f];
		sSlot[i * 2 + 1] = hex[pDigest[i] & 0x0f];
	}
	sSlot[iLen * 2] = '\0';
	return sSlot;
}

static const char* xmurmur3_format_hex32(uint32_t val)
{
	static const char hex[] = "0123456789abcdef";
	char* sSlot = g_xmurmur3_hex[g_xmurmur3_hex_index % XMURMUR3_HEX_SLOTS];
	int i;
	g_xmurmur3_hex_index = (g_xmurmur3_hex_index + 1u) % XMURMUR3_HEX_SLOTS;
	for (i = 0; i < 4; ++i) {
		uint8_t byte = (uint8_t)(val >> (i * 8));
		sSlot[i * 2] = hex[(byte >> 4) & 0x0f];
		sSlot[i * 2 + 1] = hex[byte & 0x0f];
	}
	sSlot[8] = '\0';
	return sSlot;
}

static void xmurmur3_publish_error(const xrtmurmur3error* pError)
{
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "murmur3 operation failed";
	xl6ExtlibSetError(XERR_VALUE, "xlang.extlib.murmur3", sMessage);
}



XMURMUR3_EXPORT bool xmurmur3_available(void)
{
	return true;
}

XMURMUR3_EXPORT const char* xmurmur3_version(void)
{
	return "3.0.0";
}

XMURMUR3_EXPORT const char* xmurmur3_last_error(void)
{
	return xrtGetError();
}

XMURMUR3_EXPORT int xmurmur3_file_status(void)
{
	return g_xmurmur3_file_status;
}



/* ---- 涓€娆℃€у搱甯?---- */

XMURMUR3_EXPORT long long xmurmur3_hash32(const void* pData, size_t iSize, unsigned int iSeed)
{
	xrtmurmur3error tError;
	xrtClearError();
	return (unsigned int)xrtMurmur3Hash32(pData, iSize, (uint32)iSeed, &tError);
}

XMURMUR3_EXPORT long long xmurmur3_hash32_str(const char* sText, size_t iSize, unsigned int iSeed)
{
	return xmurmur3_hash32((const void*)sText, iSize, iSeed);
}

XMURMUR3_EXPORT const char* xmurmur3_hash32_hex(const void* pData, size_t iSize, unsigned int iSeed)
{
	xrtmurmur3error tError;
	uint32 result;
	xrtClearError();
	result = xrtMurmur3Hash32(pData, iSize, (uint32)iSeed, &tError);
	return xmurmur3_format_hex32(result);
}

XMURMUR3_EXPORT const char* xmurmur3_hash32_str_hex(const char* sText, size_t iSize, unsigned int iSeed)
{
	return xmurmur3_hash32_hex((const void*)sText, iSize, iSeed);
}

XMURMUR3_EXPORT void* xmurmur3_hash32_bytes(const void* pData, size_t iSize, unsigned int iSeed)
{
	xrtmurmur3error tError;
	uint32 result;
	uint8* pOut;
	xrtClearError();
	result = xrtMurmur3Hash32(pData, iSize, (uint32)iSeed, &tError);
	pOut = (uint8*)xrtMalloc(4);
	if (pOut == NULL) { return NULL; }
	memcpy(pOut, &result, 4);
	return xl6ExtlibBufferTake(pOut, 4);
}

XMURMUR3_EXPORT void* xmurmur3_hash32_str_bytes(const char* sText, size_t iSize, unsigned int iSeed)
{
	return xmurmur3_hash32_bytes((const void*)sText, iSize, iSeed);
}

XMURMUR3_EXPORT const char* xmurmur3_hash128_hex(const void* pData, size_t iSize, unsigned int iSeed)
{
	uint8 pOut[16];
	xrtmurmur3error tError;
	xrtClearError();
	if (!xrtMurmur3Hash128(pData, iSize, (uint32)iSeed, pOut, &tError)) {
		xmurmur3_publish_error(&tError);
		return "";
	}
	return xmurmur3_format_hex(pOut, 16);
}

XMURMUR3_EXPORT const char* xmurmur3_hash128_str_hex(const char* sText, size_t iSize, unsigned int iSeed)
{
	return xmurmur3_hash128_hex((const void*)sText, iSize, iSeed);
}

XMURMUR3_EXPORT void* xmurmur3_hash128_bytes(const void* pData, size_t iSize, unsigned int iSeed)
{
	uint8* pOut;
	xrtmurmur3error tError;
	xrtClearError();
	pOut = (uint8*)xrtMalloc(16);
	if (pOut == NULL) { return NULL; }
	if (!xrtMurmur3Hash128(pData, iSize, (uint32)iSeed, pOut, &tError)) {
		xrtFree(pOut);
		xmurmur3_publish_error(&tError);
		return NULL;
	}
	return xl6ExtlibBufferTake(pOut, 16);
}

XMURMUR3_EXPORT void* xmurmur3_hash128_str_bytes(const char* sText, size_t iSize, unsigned int iSeed)
{
	return xmurmur3_hash128_bytes((const void*)sText, iSize, iSeed);
}



/* ---- 鏂囦欢鍝堝笇 ---- */

XMURMUR3_EXPORT long long xmurmur3_file32(str sPath, unsigned long long iMaxInput,
	unsigned int iSeed, xvalue* pCallback)
{
	xrtmurmur3options tOptions;
	xrtmurmur3error tError;
	xrtprogresscall tProgress;
	xreader* pReader = NULL;
	uint32 result;
	xrtClearError();
	g_xmurmur3_file_status = 0;
	xrtProgressCallInit(&tProgress, pCallback);
	xrtMurmur3OptionsInit(&tOptions);
	if (iMaxInput != 0u) { tOptions.tLimits.iMaxInputBytes = (uint64)iMaxInput; }
	if (sPath == NULL || sPath[0] == 0) {
		xl6ExtlibSetError(XERR_ARGUMENT, "xlang.extlib.murmur3", "murmur3 file path is required");
		return 0u;
	}
	if (!xrtPathExists(sPath)) {
		xmurmur3_path_error("murmur3 file does not exist: ", sPath);
		return 0u;
	}
	pReader = xrtReaderOpen(sPath);
	if (pReader == NULL) {
		xmurmur3_path_error("cannot open murmur3 file: ", sPath);
		return 0u;
	}
	result = xrtMurmur3File32(pReader, &tOptions.tLimits, (uint32)iSeed,
		tProgress.Callback != NULL ? xrtProgressCallInvoke : NULL, &tProgress, &tError);
	xrtReaderDestroy(pReader);
	if (tError.iCode != XRT_MURMUR3_OK) {
		if (tProgress.InvokeFailed) { g_xmurmur3_file_status = -2; }
		else {
			g_xmurmur3_file_status = tError.iCode == XRT_MURMUR3_ERROR_CANCELLED ? -1 : 0;
			xmurmur3_publish_error(&tError);
		}
		return 0u;
	}
	g_xmurmur3_file_status = 1;
	return (long long)result;
}

XMURMUR3_EXPORT const char* xmurmur3_file128_hex(str sPath, unsigned long long iMaxInput,
	unsigned int iSeed, xvalue* pCallback)
{
	xrtmurmur3options tOptions;
	xrtmurmur3error tError;
	xrtprogresscall tProgress;
	xreader* pReader = NULL;
	uint8 pOut[16];
	bool bResult;
	xrtClearError();
	g_xmurmur3_file_status = 0;
	xrtProgressCallInit(&tProgress, pCallback);
	xrtMurmur3OptionsInit(&tOptions);
	if (iMaxInput != 0u) { tOptions.tLimits.iMaxInputBytes = (uint64)iMaxInput; }
	if (sPath == NULL || sPath[0] == 0) {
		xl6ExtlibSetError(XERR_ARGUMENT, "xlang.extlib.murmur3", "murmur3 file path is required");
		return "";
	}
	if (!xrtPathExists(sPath)) {
		xmurmur3_path_error("murmur3 file does not exist: ", sPath);
		return "";
	}
	pReader = xrtReaderOpen(sPath);
	if (pReader == NULL) {
		xmurmur3_path_error("cannot open murmur3 file: ", sPath);
		return "";
	}
	bResult = xrtMurmur3File128(pReader, &tOptions.tLimits, (uint32)iSeed, pOut,
		tProgress.Callback != NULL ? xrtProgressCallInvoke : NULL, &tProgress, &tError);
	xrtReaderDestroy(pReader);
	if (!bResult) {
		if (tProgress.InvokeFailed) { g_xmurmur3_file_status = -2; }
		else {
			g_xmurmur3_file_status = tError.iCode == XRT_MURMUR3_ERROR_CANCELLED ? -1 : 0;
			xmurmur3_publish_error(&tError);
		}
		return "";
	}
	g_xmurmur3_file_status = 1;
	return xmurmur3_format_hex(pOut, 16);
}



/* ---- 娴佸紡鍝堝笇鍣?---- */

XMURMUR3_EXPORT void* xmurmur3_hasher_create(unsigned int iSeed, unsigned long long iMaxInput)
{
	xrtmurmur3options tOptions;
	xrtmurmur3hasher* pHasher;
	xrtClearError();
	xrtMurmur3OptionsInit(&tOptions);
	if (iMaxInput != 0u) { tOptions.tLimits.iMaxInputBytes = (uint64)iMaxInput; }
	pHasher = xrtMurmur3HasherCreate(&tOptions, (uint32)iSeed);
	if (pHasher == NULL) {
		xl6ExtlibSetError(XERR_MEMORY, "xlang.extlib.murmur3", "murmur3 hasher create failed");
		return NULL;
	}
	return pHasher;
}

XMURMUR3_EXPORT void xmurmur3_hasher_destroy(void* pHandle)
{
	xrtMurmur3HasherDestroy((xrtmurmur3hasher*)pHandle);
}

XMURMUR3_EXPORT bool xmurmur3_hasher_update(void* pHandle, const void* pData, size_t iSize)
{
	xrtmurmur3hasher* pHasher = (xrtmurmur3hasher*)pHandle;
	if (!xrtMurmur3HasherUpdate(pHasher, pData, iSize)) {
		xmurmur3_publish_error(xrtMurmur3HasherError(pHasher));
		return false;
	}
	return true;
}

XMURMUR3_EXPORT bool xmurmur3_hasher_update_str(void* pHandle, const char* sText, size_t iSize)
{
	return xmurmur3_hasher_update(pHandle, (const void*)sText, iSize);
}

XMURMUR3_EXPORT const char* xmurmur3_hasher_digest_hex(void* pHandle)
{
	xrtmurmur3hasher* pHasher = (xrtmurmur3hasher*)pHandle;
	uint8 pOut[16];
	if (!xrtMurmur3HasherDigest(pHasher, pOut)) {
		xmurmur3_publish_error(xrtMurmur3HasherError(pHasher));
		return "";
	}
	return xmurmur3_format_hex(pOut, 16);
}

XMURMUR3_EXPORT void* xmurmur3_hasher_digest_bytes(void* pHandle)
{
	xrtmurmur3hasher* pHasher = (xrtmurmur3hasher*)pHandle;
	uint8* pOut;
	xrtClearError();
	pOut = (uint8*)xrtMalloc(16);
	if (pOut == NULL) { return NULL; }
	if (!xrtMurmur3HasherDigest(pHasher, pOut)) {
		xrtFree(pOut);
		xmurmur3_publish_error(xrtMurmur3HasherError(pHasher));
		return NULL;
	}
	return xl6ExtlibBufferTake(pOut, 16);
}

XMURMUR3_EXPORT bool xmurmur3_hasher_reset(void* pHandle)
{
	xrtmurmur3hasher* pHasher = (xrtmurmur3hasher*)pHandle;
	if (!xrtMurmur3HasherReset(pHasher)) {
		xmurmur3_publish_error(xrtMurmur3HasherError(pHasher));
		return false;
	}
	return true;
}

XMURMUR3_EXPORT long long xmurmur3_hasher_input_bytes(void* pHandle)
{
	return (unsigned long long)xrtMurmur3HasherInputBytes((const xrtmurmur3hasher*)pHandle);
}

XMURMUR3_EXPORT void* xmurmur3_hasher_adopt(void* pHandle)
{
	return pHandle;
}

XMURMUR3_EXPORT void* xmurmur3_hasher_clone(void* pHandle)
{
	xrtmurmur3hasher* pSrc = (xrtmurmur3hasher*)pHandle;
	if (pSrc == NULL) { return NULL; }
	return xrtMurmur3HasherClone(pSrc);
}
