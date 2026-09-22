#include "include/xrt_siphash.h"
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE

#include "xruntime.h"
#include "../xlang_extlib_runtime.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#define XSIPHASH_EXPORT __declspec(dllexport)
#else
#define XSIPHASH_EXPORT
#endif

#if defined(__GNUC__)
#define XSIPHASH_THREAD __thread
#elif defined(_MSC_VER)
#define XSIPHASH_THREAD __declspec(thread)
#else
#define XSIPHASH_THREAD
#endif

#define XSIPHASH_HEX_SLOTS 8u

static XSIPHASH_THREAD char g_xsiphash_hex[XSIPHASH_HEX_SLOTS][33];
static XSIPHASH_THREAD size_t g_xsiphash_hex_index;
static XSIPHASH_THREAD int g_xsiphash_file_status;

static void xsiphash_path_error(const char *prefix, const char *path)
{
	char message[512];
	snprintf(message, sizeof(message), "%s%s", prefix,
		path != NULL ? path : "");
	message[sizeof(message) - 1u] = '\0';
	xl6ExtlibSetError(XERR_IO, "xlang.extlib.siphash", message);
}

static const char* xsiphash_format_hex64(uint64_t val)
{
	static const char hex[] = "0123456789abcdef";
	char* sSlot = g_xsiphash_hex[g_xsiphash_hex_index % XSIPHASH_HEX_SLOTS];
	int i;
	g_xsiphash_hex_index = (g_xsiphash_hex_index + 1u) % XSIPHASH_HEX_SLOTS;
	for (i = 0; i < 8; ++i) {
		uint8_t byte = (uint8_t)(val >> (i * 8));
		sSlot[i * 2] = hex[(byte >> 4) & 0x0f];
		sSlot[i * 2 + 1] = hex[byte & 0x0f];
	}
	sSlot[16] = '\0';
	return sSlot;
}

static const char* xsiphash_format_hex128(const uint8* pDigest)
{
	static const char hex[] = "0123456789abcdef";
	char* sSlot = g_xsiphash_hex[g_xsiphash_hex_index % XSIPHASH_HEX_SLOTS];
	size_t i;
	g_xsiphash_hex_index = (g_xsiphash_hex_index + 1u) % XSIPHASH_HEX_SLOTS;
	for (i = 0; i < 16; ++i) {
		sSlot[i * 2] = hex[(pDigest[i] >> 4) & 0x0f];
		sSlot[i * 2 + 1] = hex[pDigest[i] & 0x0f];
	}
	sSlot[32] = '\0';
	return sSlot;
}

static void xsiphash_publish_error(const xrtsiphasherror* pError)
{
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "siphash operation failed";
	xl6ExtlibSetError(XERR_VALUE, "xlang.extlib.siphash", sMessage);
}



XSIPHASH_EXPORT bool xsiphash_available(void)
{
	return true;
}

XSIPHASH_EXPORT const char* xsiphash_version(void)
{
	return "2.4";
}

XSIPHASH_EXPORT const char* xsiphash_last_error(void)
{
	return xrtGetError();
}

XSIPHASH_EXPORT int xsiphash_file_status(void)
{
	return g_xsiphash_file_status;
}



/* ---- 涓€娆℃€у搱甯?---- */

XSIPHASH_EXPORT long long xsiphash_hash64(const void* pData, size_t iSize,
	const void* pKey, size_t iKeyLen)
{
	uint8 pKeyBuf[XRT_SIPHASH_KEY_LEN];
	xrtsiphasherror tError;
	xrtClearError();
	memset(pKeyBuf, 0, sizeof(pKeyBuf));
	if (pKey != NULL && iKeyLen > 0) {
		size_t take = iKeyLen < XRT_SIPHASH_KEY_LEN ? iKeyLen : XRT_SIPHASH_KEY_LEN;
		memcpy(pKeyBuf, pKey, take);
	}
	return (unsigned long long)xrtSipHash64(pData, iSize, pKeyBuf, &tError);
}

XSIPHASH_EXPORT long long xsiphash_hash64_str(const char* sText, size_t iSize,
	const void* pKey, size_t iKeyLen)
{
	return xsiphash_hash64((const void*)sText, iSize, pKey, iKeyLen);
}

XSIPHASH_EXPORT const char* xsiphash_hash64_hex(const void* pData, size_t iSize,
	const void* pKey, size_t iKeyLen)
{
	uint8 pKeyBuf[XRT_SIPHASH_KEY_LEN];
	xrtsiphasherror tError;
	uint64 result;
	xrtClearError();
	memset(pKeyBuf, 0, sizeof(pKeyBuf));
	if (pKey != NULL && iKeyLen > 0) {
		size_t take = iKeyLen < XRT_SIPHASH_KEY_LEN ? iKeyLen : XRT_SIPHASH_KEY_LEN;
		memcpy(pKeyBuf, pKey, take);
	}
	result = xrtSipHash64(pData, iSize, pKeyBuf, &tError);
	return xsiphash_format_hex64(result);
}

XSIPHASH_EXPORT const char* xsiphash_hash64_str_hex(const char* sText, size_t iSize,
	const char* sKey, size_t iKeyLen)
{
	return xsiphash_hash64_hex((const void*)sText, iSize, (const void*)sKey, iKeyLen);
}

XSIPHASH_EXPORT const char* xsiphash_hash64_str_hex_buffer(const char* sText, size_t iSize,
	const void* pKey, size_t iKeyLen)
{
	return xsiphash_hash64_hex((const void*)sText, iSize, pKey, iKeyLen);
}

XSIPHASH_EXPORT void* xsiphash_hash64_bytes(const void* pData, size_t iSize,
	const void* pKey, size_t iKeyLen)
{
	uint8 pKeyBuf[XRT_SIPHASH_KEY_LEN];
	xrtsiphasherror tError;
	uint64 result;
	uint8* pOut;
	xrtClearError();
	memset(pKeyBuf, 0, sizeof(pKeyBuf));
	if (pKey != NULL && iKeyLen > 0) {
		size_t take = iKeyLen < XRT_SIPHASH_KEY_LEN ? iKeyLen : XRT_SIPHASH_KEY_LEN;
		memcpy(pKeyBuf, pKey, take);
	}
	result = xrtSipHash64(pData, iSize, pKeyBuf, &tError);
	pOut = (uint8*)xrtMalloc(8);
	if (pOut == NULL) { return NULL; }
	memcpy(pOut, &result, 8);
	return xl6ExtlibBufferTake(pOut, 8);
}

XSIPHASH_EXPORT void* xsiphash_hash64_str_bytes(const char* sText, size_t iSize,
	const void* pKey, size_t iKeyLen)
{
	return xsiphash_hash64_bytes((const void*)sText, iSize, pKey, iKeyLen);
}

XSIPHASH_EXPORT void* xsiphash_hash64_str_str_bytes(const char* sText, size_t iSize,
	const char* sKey, size_t iKeyLen)
{
	return xsiphash_hash64_bytes((const void*)sText, iSize, (const void*)sKey, iKeyLen);
}

XSIPHASH_EXPORT const char* xsiphash_hash128_hex(const void* pData, size_t iSize,
	const void* pKey, size_t iKeyLen)
{
	uint8 pKeyBuf[XRT_SIPHASH_KEY_LEN];
	uint8 pOut[16];
	xrtsiphasherror tError;
	xrtClearError();
	memset(pKeyBuf, 0, sizeof(pKeyBuf));
	if (pKey != NULL && iKeyLen > 0) {
		size_t take = iKeyLen < XRT_SIPHASH_KEY_LEN ? iKeyLen : XRT_SIPHASH_KEY_LEN;
		memcpy(pKeyBuf, pKey, take);
	}
	if (!xrtSipHash128(pData, iSize, pKeyBuf, pOut, &tError)) {
		xsiphash_publish_error(&tError);
		return "";
	}
	return xsiphash_format_hex128(pOut);
}

XSIPHASH_EXPORT const char* xsiphash_hash128_str_hex(const char* sText, size_t iSize,
	const char* sKey, size_t iKeyLen)
{
	return xsiphash_hash128_hex((const void*)sText, iSize, (const void*)sKey, iKeyLen);
}

XSIPHASH_EXPORT const char* xsiphash_hash128_str_hex_buffer(const char* sText, size_t iSize,
	const void* pKey, size_t iKeyLen)
{
	return xsiphash_hash128_hex((const void*)sText, iSize, pKey, iKeyLen);
}

XSIPHASH_EXPORT void* xsiphash_hash128_bytes(const void* pData, size_t iSize,
	const void* pKey, size_t iKeyLen)
{
	uint8 pKeyBuf[XRT_SIPHASH_KEY_LEN];
	uint8* pOut;
	xrtsiphasherror tError;
	xrtClearError();
	memset(pKeyBuf, 0, sizeof(pKeyBuf));
	if (pKey != NULL && iKeyLen > 0) {
		size_t take = iKeyLen < XRT_SIPHASH_KEY_LEN ? iKeyLen : XRT_SIPHASH_KEY_LEN;
		memcpy(pKeyBuf, pKey, take);
	}
	pOut = (uint8*)xrtMalloc(16);
	if (pOut == NULL) { return NULL; }
	if (!xrtSipHash128(pData, iSize, pKeyBuf, pOut, &tError)) {
		xrtFree(pOut);
		xsiphash_publish_error(&tError);
		return NULL;
	}
	return xl6ExtlibBufferTake(pOut, 16);
}

XSIPHASH_EXPORT void* xsiphash_hash128_str_bytes(const char* sText, size_t iSize,
	const void* pKey, size_t iKeyLen)
{
	return xsiphash_hash128_bytes((const void*)sText, iSize, pKey, iKeyLen);
}

XSIPHASH_EXPORT void* xsiphash_hash128_str_str_bytes(const char* sText, size_t iSize,
	const char* sKey, size_t iKeyLen)
{
	return xsiphash_hash128_bytes((const void*)sText, iSize, (const void*)sKey, iKeyLen);
}



/* ---- 鏂囦欢鍝堝笇 ---- */

XSIPHASH_EXPORT long long xsiphash_file64(str sPath, unsigned long long iMaxInput,
	const void* pKey, size_t iKeyLen, xvalue* pCallback)
{
	xrtsiphashoptions tOptions;
	xrtsiphasherror tError;
	xrtprogresscall tProgress;
	xreader* pReader = NULL;
	uint8 pKeyBuf[XRT_SIPHASH_KEY_LEN];
	uint64 result;
	xrtClearError();
	g_xsiphash_file_status = 0;
	xrtProgressCallInit(&tProgress, pCallback);
	memset(pKeyBuf, 0, sizeof(pKeyBuf));
	if (pKey != NULL && iKeyLen > 0) {
		size_t take = iKeyLen < XRT_SIPHASH_KEY_LEN ? iKeyLen : XRT_SIPHASH_KEY_LEN;
		memcpy(pKeyBuf, pKey, take);
	}
	xrtSipHashOptionsInit(&tOptions);
	if (iMaxInput != 0u) { tOptions.tLimits.iMaxInputBytes = (uint64)iMaxInput; }
	if (sPath == NULL || sPath[0] == 0) {
		xl6ExtlibSetError(XERR_ARGUMENT, "xlang.extlib.siphash", "siphash file path is required");
		return 0u;
	}
	if (!xrtPathExists(sPath)) {
		xsiphash_path_error("siphash file does not exist: ", sPath);
		return 0u;
	}
	pReader = xrtReaderOpen(sPath);
	if (pReader == NULL) {
		xsiphash_path_error("cannot open siphash file: ", sPath);
		return 0u;
	}
	result = xrtSipHashFile64(pReader, &tOptions.tLimits, pKeyBuf,
		tProgress.Callback != NULL ? xrtProgressCallInvoke : NULL, &tProgress, &tError);
	xrtReaderDestroy(pReader);
	if (tError.iCode != XRT_SIPHASH_OK) {
		if (tProgress.InvokeFailed) { g_xsiphash_file_status = -2; }
		else {
			g_xsiphash_file_status = tError.iCode == XRT_SIPHASH_ERROR_CANCELLED ? -1 : 0;
			xsiphash_publish_error(&tError);
		}
		return 0u;
	}
	g_xsiphash_file_status = 1;
	return (long long)result;
}



/* ---- 娴佸紡鍝堝笇鍣?---- */

XSIPHASH_EXPORT void* xsiphash_hasher_create(const void* pKey, size_t iKeyLen,
	unsigned long long iMaxInput)
{
	xrtsiphashoptions tOptions;
	xrtsiphasher* pHasher;
	uint8 pKeyBuf[XRT_SIPHASH_KEY_LEN];
	xrtClearError();
	memset(pKeyBuf, 0, sizeof(pKeyBuf));
	if (pKey != NULL && iKeyLen > 0) {
		size_t take = iKeyLen < XRT_SIPHASH_KEY_LEN ? iKeyLen : XRT_SIPHASH_KEY_LEN;
		memcpy(pKeyBuf, pKey, take);
	}
	xrtSipHashOptionsInit(&tOptions);
	if (iMaxInput != 0u) { tOptions.tLimits.iMaxInputBytes = (uint64)iMaxInput; }
	pHasher = xrtSipHasherCreate(&tOptions, pKeyBuf);
	if (pHasher == NULL) {
		xl6ExtlibSetError(XERR_MEMORY, "xlang.extlib.siphash", "siphash hasher create failed");
		return NULL;
	}
	return pHasher;
}

XSIPHASH_EXPORT void xsiphash_hasher_destroy(void* pHandle)
{
	xrtSipHasherDestroy((xrtsiphasher*)pHandle);
}

XSIPHASH_EXPORT bool xsiphash_hasher_update(void* pHandle, const void* pData, size_t iSize)
{
	xrtsiphasher* pHasher = (xrtsiphasher*)pHandle;
	if (!xrtSipHasherUpdate(pHasher, pData, iSize)) {
		xsiphash_publish_error(xrtSipHasherError(pHasher));
		return false;
	}
	return true;
}

XSIPHASH_EXPORT bool xsiphash_hasher_update_str(void* pHandle, const char* sText, size_t iSize)
{
	return xsiphash_hasher_update(pHandle, (const void*)sText, iSize);
}

XSIPHASH_EXPORT long long xsiphash_hasher_digest64(void* pHandle)
{
	xrtsiphasher* pHasher = (xrtsiphasher*)pHandle;
	return (unsigned long long)xrtSipHasherDigest64(pHasher);
}

XSIPHASH_EXPORT const char* xsiphash_hasher_digest64_hex(void* pHandle)
{
	xrtsiphasher* pHasher = (xrtsiphasher*)pHandle;
	uint64 result = xrtSipHasherDigest64(pHasher);
	return xsiphash_format_hex64(result);
}

XSIPHASH_EXPORT void* xsiphash_hasher_digest64_bytes(void* pHandle)
{
	xrtsiphasher* pHasher = (xrtsiphasher*)pHandle;
	uint64 result;
	uint8* pOut;
	xrtClearError();
	result = xrtSipHasherDigest64(pHasher);
	pOut = (uint8*)xrtMalloc(8);
	if (pOut == NULL) { return NULL; }
	memcpy(pOut, &result, 8);
	return xl6ExtlibBufferTake(pOut, 8);
}

XSIPHASH_EXPORT const char* xsiphash_hasher_digest128_hex(void* pHandle)
{
	xrtsiphasher* pHasher = (xrtsiphasher*)pHandle;
	uint8 pOut[16];
	if (!xrtSipHasherDigest128(pHasher, pOut)) {
		xsiphash_publish_error(xrtSipHasherError(pHasher));
		return "";
	}
	return xsiphash_format_hex128(pOut);
}

XSIPHASH_EXPORT void* xsiphash_hasher_digest128_bytes(void* pHandle)
{
	xrtsiphasher* pHasher = (xrtsiphasher*)pHandle;
	uint8* pOut;
	xrtClearError();
	pOut = (uint8*)xrtMalloc(16);
	if (pOut == NULL) { return NULL; }
	if (!xrtSipHasherDigest128(pHasher, pOut)) {
		xrtFree(pOut);
		xsiphash_publish_error(xrtSipHasherError(pHasher));
		return NULL;
	}
	return xl6ExtlibBufferTake(pOut, 16);
}

XSIPHASH_EXPORT bool xsiphash_hasher_reset(void* pHandle)
{
	xrtsiphasher* pHasher = (xrtsiphasher*)pHandle;
	if (!xrtSipHasherReset(pHasher)) {
		xsiphash_publish_error(xrtSipHasherError(pHasher));
		return false;
	}
	return true;
}

XSIPHASH_EXPORT long long xsiphash_hasher_input_bytes(void* pHandle)
{
	return (unsigned long long)xrtSipHasherInputBytes((const xrtsiphasher*)pHandle);
}

XSIPHASH_EXPORT void* xsiphash_hasher_adopt(void* pHandle)
{
	return pHandle;
}

XSIPHASH_EXPORT void* xsiphash_hasher_clone(void* pHandle)
{
	xrtsiphasher* pSrc = (xrtsiphasher*)pHandle;
	if (pSrc == NULL) { return NULL; }
	return xrtSipHasherClone(pSrc);
}
