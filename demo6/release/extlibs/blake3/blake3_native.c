#include "include/xrt_blake3.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#define XBLAKE3_EXPORT __declspec(dllexport)
#else
#define XBLAKE3_EXPORT
#endif

#if defined(__GNUC__)
#define XBLAKE3_THREAD __thread
#elif defined(_MSC_VER)
#define XBLAKE3_THREAD __declspec(thread)
#else
#define XBLAKE3_THREAD
#endif

#define XBLAKE3_HEX_SLOTS 8u
#define XBLAKE3_MAX_OUT 64u

static XBLAKE3_THREAD char g_xblake3_hex[XBLAKE3_HEX_SLOTS][XBLAKE3_MAX_OUT * 2 + 1];
static XBLAKE3_THREAD size_t g_xblake3_hex_index;

static const char* xblake3_format_hex(const uint8* pDigest, size_t iLen)
{
	static const char hex[] = "0123456789abcdef";
	char* sSlot = g_xblake3_hex[g_xblake3_hex_index % XBLAKE3_HEX_SLOTS];
	size_t i;
	g_xblake3_hex_index = (g_xblake3_hex_index + 1u) % XBLAKE3_HEX_SLOTS;
	if (iLen > XBLAKE3_MAX_OUT) iLen = XBLAKE3_MAX_OUT;
	for (i = 0; i < iLen; ++i) {
		sSlot[i * 2] = hex[(pDigest[i] >> 4) & 0x0f];
		sSlot[i * 2 + 1] = hex[pDigest[i] & 0x0f];
	}
	sSlot[iLen * 2] = '\0';
	return sSlot;
}

static void xblake3_publish_error(const xrtblake3error* pError)
{
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "blake3 operation failed";
	xrtSetError(xrtFormat("%s", sMessage), TRUE);
}



XBLAKE3_EXPORT bool xblake3_available(void)
{
	return TRUE;
}

XBLAKE3_EXPORT const char* xblake3_version(void)
{
	return "1.5.0";
}

XBLAKE3_EXPORT const char* xblake3_last_error(void)
{
	return xrtGetError();
}



/* ---- 一次性哈希 ---- */

XBLAKE3_EXPORT const char* xblake3_hash_hex(const void* pData, size_t iSize, size_t iOutLen)
{
	uint8 pOut[XBLAKE3_MAX_OUT];
	xrtblake3error tError;
	xrtClearError();
	if (iOutLen == 0 || iOutLen > XBLAKE3_MAX_OUT) iOutLen = 32;
	if (!xrtBlake3Hash(pData, iSize, pOut, iOutLen, &tError)) {
		xblake3_publish_error(&tError);
		return "";
	}
	return xblake3_format_hex(pOut, iOutLen);
}

XBLAKE3_EXPORT const char* xblake3_hash_str_hex(const char* sText, size_t iSize, size_t iOutLen)
{
	return xblake3_hash_hex((const void*)sText, iSize, iOutLen);
}

XBLAKE3_EXPORT void* xblake3_hash_bytes(const void* pData, size_t iSize, size_t iOutLen)
{
	uint8* pOut;
	xrtblake3error tError;
	xrtClearError();
	if (iOutLen == 0 || iOutLen > XBLAKE3_MAX_OUT) iOutLen = 32;
	pOut = (uint8*)xrtMalloc(iOutLen);
	if (pOut == NULL) { return NULL; }
	if (!xrtBlake3Hash(pData, iSize, pOut, iOutLen, &tError)) {
		xrtFree(pOut);
		xblake3_publish_error(&tError);
		return NULL;
	}
	return xrtBufferAdopt(pOut, iOutLen);
}

XBLAKE3_EXPORT void* xblake3_hash_str_bytes(const char* sText, size_t iSize, size_t iOutLen)
{
	return xblake3_hash_bytes((const void*)sText, iSize, iOutLen);
}

XBLAKE3_EXPORT const char* xblake3_hash_mac_hex(const void* pData, size_t iSize,
	const void* pKey, size_t iKeyLen, size_t iOutLen)
{
	uint8 pOut[XBLAKE3_MAX_OUT];
	uint8 pKeyBuf[XRT_BLAKE3_KEY_LEN];
	xrtblake3error tError;
	xrtClearError();
	if (iOutLen == 0 || iOutLen > XBLAKE3_MAX_OUT) iOutLen = 32;
	memset(pKeyBuf, 0, sizeof(pKeyBuf));
	if (pKey != NULL && iKeyLen > 0) {
		size_t take = iKeyLen < XRT_BLAKE3_KEY_LEN ? iKeyLen : XRT_BLAKE3_KEY_LEN;
		memcpy(pKeyBuf, pKey, take);
	}
	if (!xrtBlake3HashKeyed(pData, iSize, pKeyBuf, pOut, iOutLen, &tError)) {
		xblake3_publish_error(&tError);
		return "";
	}
	return xblake3_format_hex(pOut, iOutLen);
}

XBLAKE3_EXPORT const char* xblake3_hash_mac_str_hex(const char* sText, size_t iSize,
	const char* sKey, size_t iKeyLen, size_t iOutLen)
{
	return xblake3_hash_mac_hex((const void*)sText, iSize, (const void*)sKey, iKeyLen, iOutLen);
}

XBLAKE3_EXPORT const char* xblake3_hash_kdf_hex(const void* pKeyMat, size_t iKeyMatLen,
	const char* sContext, size_t iOutLen)
{
	uint8 pOut[XBLAKE3_MAX_OUT];
	xrtblake3error tError;
	xrtClearError();
	if (iOutLen == 0 || iOutLen > XBLAKE3_MAX_OUT) iOutLen = 32;
	if (!xrtBlake3HashKdf(pKeyMat, iKeyMatLen, sContext, pOut, iOutLen, &tError)) {
		xblake3_publish_error(&tError);
		return "";
	}
	return xblake3_format_hex(pOut, iOutLen);
}

XBLAKE3_EXPORT const char* xblake3_hash_kdf_str_hex(const char* sKeyMat, size_t iKeyMatLen,
	const char* sContext, size_t iOutLen)
{
	return xblake3_hash_kdf_hex((const void*)sKeyMat, iKeyMatLen, sContext, iOutLen);
}



/* ---- 文件哈希 ---- */

XBLAKE3_EXPORT const char* xblake3_file_hex(str sPath, unsigned long long iMaxInput,
	size_t iOutLen)
{
	xrtblake3options tOptions;
	xrtblake3error tError;
	xreader pReader = NULL;
	uint8 pOut[XBLAKE3_MAX_OUT];
	bool bResult;
	xrtClearError();
	if (iOutLen == 0 || iOutLen > XBLAKE3_MAX_OUT) iOutLen = 32;
	xrtBlake3OptionsInit(&tOptions);
	if (iMaxInput != 0u) { tOptions.tLimits.iMaxInputBytes = (uint64)iMaxInput; }
	if (sPath == NULL || sPath[0] == 0) {
		xrtSetError(xrtFormat("blake3 file path is required"), TRUE);
		return "";
	}
	if (!xrtPathExists(sPath)) {
		xrtSetError(xrtFormat("blake3 file does not exist: %s", sPath), TRUE);
		return "";
	}
	pReader = xrtReaderOpen(sPath);
	if (pReader == NULL) {
		xrtSetError(xrtFormat("cannot open blake3 file: %s", sPath), TRUE);
		return "";
	}
	bResult = xrtBlake3File(pReader, &tOptions.tLimits, pOut, iOutLen, &tError);
	xrtReaderDestroy(pReader);
	if (!bResult) { xblake3_publish_error(&tError); return ""; }
	return xblake3_format_hex(pOut, iOutLen);
}



/* ---- 流式哈希器 ---- */

XBLAKE3_EXPORT void* xblake3_hasher_create(unsigned long long iMaxInput)
{
	xrtblake3options tOptions;
	xrtblake3hasher* pHasher;
	xrtClearError();
	xrtBlake3OptionsInit(&tOptions);
	if (iMaxInput != 0u) { tOptions.tLimits.iMaxInputBytes = (uint64)iMaxInput; }
	pHasher = xrtBlake3HasherCreate(&tOptions);
	if (pHasher == NULL) {
		xrtSetError(xrtFormat("blake3 hasher create failed"), TRUE);
		return NULL;
	}
	return pHasher;
}

XBLAKE3_EXPORT void* xblake3_hasher_create_keyed(const void* pKey, size_t iKeyLen,
	unsigned long long iMaxInput)
{
	xrtblake3options tOptions;
	xrtblake3hasher* pHasher;
	uint8 pKeyBuf[XRT_BLAKE3_KEY_LEN];
	xrtClearError();
	xrtBlake3OptionsInit(&tOptions);
	if (iMaxInput != 0u) { tOptions.tLimits.iMaxInputBytes = (uint64)iMaxInput; }
	memset(pKeyBuf, 0, sizeof(pKeyBuf));
	if (pKey != NULL && iKeyLen > 0) {
		size_t take = iKeyLen < XRT_BLAKE3_KEY_LEN ? iKeyLen : XRT_BLAKE3_KEY_LEN;
		memcpy(pKeyBuf, pKey, take);
	}
	pHasher = xrtBlake3HasherCreateKeyed(&tOptions, pKeyBuf);
	if (pHasher == NULL) {
		xrtSetError(xrtFormat("blake3 keyed hasher create failed"), TRUE);
		return NULL;
	}
	return pHasher;
}

XBLAKE3_EXPORT void* xblake3_hasher_create_kdf(const char* sContext,
	unsigned long long iMaxInput)
{
	xrtblake3options tOptions;
	xrtblake3hasher* pHasher;
	xrtClearError();
	xrtBlake3OptionsInit(&tOptions);
	if (iMaxInput != 0u) { tOptions.tLimits.iMaxInputBytes = (uint64)iMaxInput; }
	pHasher = xrtBlake3HasherCreateDeriveKey(&tOptions, sContext);
	if (pHasher == NULL) {
		xrtSetError(xrtFormat("blake3 kdf hasher create failed"), TRUE);
		return NULL;
	}
	return pHasher;
}

XBLAKE3_EXPORT void xblake3_hasher_destroy(void* pHandle)
{
	xrtBlake3HasherDestroy((xrtblake3hasher*)pHandle);
}

XBLAKE3_EXPORT bool xblake3_hasher_update(void* pHandle, const void* pData, size_t iSize)
{
	xrtblake3hasher* pHasher = (xrtblake3hasher*)pHandle;
	if (!xrtBlake3HasherUpdate(pHasher, pData, iSize)) {
		xblake3_publish_error(xrtBlake3HasherError(pHasher));
		return FALSE;
	}
	return TRUE;
}

XBLAKE3_EXPORT bool xblake3_hasher_update_str(void* pHandle, const char* sText, size_t iSize)
{
	return xblake3_hasher_update(pHandle, (const void*)sText, iSize);
}

XBLAKE3_EXPORT void* xblake3_hasher_digest_bytes(void* pHandle, size_t iOutLen)
{
	xrtblake3hasher* pHasher = (xrtblake3hasher*)pHandle;
	uint8* pOut;
	if (iOutLen == 0 || iOutLen > XBLAKE3_MAX_OUT) iOutLen = 32;
	pOut = (uint8*)xrtMalloc(iOutLen);
	if (pOut == NULL) { return NULL; }
	if (!xrtBlake3HasherDigest(pHasher, pOut, iOutLen)) {
		xrtFree(pOut);
		xblake3_publish_error(xrtBlake3HasherError(pHasher));
		return NULL;
	}
	return xrtBufferAdopt(pOut, iOutLen);
}

XBLAKE3_EXPORT const char* xblake3_hasher_digest_hex(void* pHandle, size_t iOutLen)
{
	xrtblake3hasher* pHasher = (xrtblake3hasher*)pHandle;
	uint8 pOut[XBLAKE3_MAX_OUT];
	if (iOutLen == 0 || iOutLen > XBLAKE3_MAX_OUT) iOutLen = 32;
	if (!xrtBlake3HasherDigest(pHasher, pOut, iOutLen)) {
		xblake3_publish_error(xrtBlake3HasherError(pHasher));
		return "";
	}
	return xblake3_format_hex(pOut, iOutLen);
}

XBLAKE3_EXPORT bool xblake3_hasher_reset(void* pHandle)
{
	xrtblake3hasher* pHasher = (xrtblake3hasher*)pHandle;
	if (!xrtBlake3HasherReset(pHasher)) {
		xblake3_publish_error(xrtBlake3HasherError(pHasher));
		return FALSE;
	}
	return TRUE;
}

XBLAKE3_EXPORT unsigned long long xblake3_hasher_input_bytes(void* pHandle)
{
	return (unsigned long long)xrtBlake3HasherInputBytes((const xrtblake3hasher*)pHandle);
}

XBLAKE3_EXPORT void* xblake3_hasher_adopt(void* pHandle)
{
	return pHandle;
}

XBLAKE3_EXPORT void* xblake3_hasher_clone(void* pHandle)
{
	xrtblake3hasher* pSrc = (xrtblake3hasher*)pHandle;
	if (pSrc == NULL) { return NULL; }
	return xrtBlake3HasherClone(pSrc);
}
