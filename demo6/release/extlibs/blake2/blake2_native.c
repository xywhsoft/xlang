#include "include/xrt_blake2.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#define XBLAKE2_EXPORT __declspec(dllexport)
#else
#define XBLAKE2_EXPORT
#endif

#if defined(__GNUC__)
#define XBLAKE2_THREAD __thread
#elif defined(_MSC_VER)
#define XBLAKE2_THREAD __declspec(thread)
#else
#define XBLAKE2_THREAD
#endif

#define XBLAKE2_HEX_SLOTS 8u
#define XBLAKE2_MAX_OUT 64u

static XBLAKE2_THREAD char g_xblake2_hex[XBLAKE2_HEX_SLOTS][XBLAKE2_MAX_OUT * 2 + 1];
static XBLAKE2_THREAD size_t g_xblake2_hex_index;

static const char* xblake2_format_hex(const uint8* pDigest, size_t iLen)
{
	static const char hex[] = "0123456789abcdef";
	char* sSlot = g_xblake2_hex[g_xblake2_hex_index % XBLAKE2_HEX_SLOTS];
	size_t i;
	g_xblake2_hex_index = (g_xblake2_hex_index + 1u) % XBLAKE2_HEX_SLOTS;
	if (iLen > XBLAKE2_MAX_OUT) iLen = XBLAKE2_MAX_OUT;
	for (i = 0; i < iLen; ++i) {
		sSlot[i * 2] = hex[(pDigest[i] >> 4) & 0x0f];
		sSlot[i * 2 + 1] = hex[pDigest[i] & 0x0f];
	}
	sSlot[iLen * 2] = '\0';
	return sSlot;
}

static void xblake2_publish_error(const xrtblake2error* pError)
{
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "blake2 operation failed";
	xrtSetError(xrtFormat("%s", sMessage), TRUE);
}



XBLAKE2_EXPORT bool xblake2_available(void)
{
	return TRUE;
}

XBLAKE2_EXPORT const char* xblake2_version(void)
{
	return "1.1.0";
}

XBLAKE2_EXPORT const char* xblake2_last_error(void)
{
	return xrtGetError();
}



/* ---- BLAKE2b 一次性哈希 ---- */

XBLAKE2_EXPORT const char* xblake2b_hash_hex(const void* pData, size_t iSize,
	size_t iOutLen, const void* pKey, size_t iKeyLen)
{
	uint8 pOut[XRT_BLAKE2B_OUT_MAX];
	xrtblake2error tError;
	xrtClearError();
	if (iOutLen == 0 || iOutLen > XRT_BLAKE2B_OUT_MAX) iOutLen = XRT_BLAKE2B_OUT_MAX;
	if (!xrtBlake2bHash(pData, iSize, iOutLen, pKey, iKeyLen, pOut, &tError)) {
		xblake2_publish_error(&tError);
		return "";
	}
	return xblake2_format_hex(pOut, iOutLen);
}

XBLAKE2_EXPORT const char* xblake2b_hash_str_hex(const char* sText, size_t iSize,
	size_t iOutLen, const char* sKey, size_t iKeyLen)
{
	return xblake2b_hash_hex((const void*)sText, iSize, iOutLen, (const void*)sKey, iKeyLen);
}

XBLAKE2_EXPORT void* xblake2b_hash_bytes(const void* pData, size_t iSize, size_t iOutLen)
{
	uint8* pOut;
	xrtblake2error tError;
	xrtClearError();
	if (iOutLen == 0 || iOutLen > XRT_BLAKE2B_OUT_MAX) iOutLen = XRT_BLAKE2B_OUT_MAX;
	pOut = (uint8*)xrtMalloc(iOutLen);
	if (pOut == NULL) { return NULL; }
	if (!xrtBlake2bHash(pData, iSize, iOutLen, NULL, 0, pOut, &tError)) {
		xrtFree(pOut);
		xblake2_publish_error(&tError);
		return NULL;
	}
	return xrtBufferAdopt(pOut, iOutLen);
}

XBLAKE2_EXPORT void* xblake2b_hash_str_bytes(const char* sText, size_t iSize, size_t iOutLen)
{
	return xblake2b_hash_bytes((const void*)sText, iSize, iOutLen);
}

XBLAKE2_EXPORT void* xblake2b_hash_mac_bytes(const void* pData, size_t iSize,
	const void* pKey, size_t iKeyLen, size_t iOutLen)
{
	uint8* pOut;
	xrtblake2error tError;
	xrtClearError();
	if (iOutLen == 0 || iOutLen > XRT_BLAKE2B_OUT_MAX) iOutLen = XRT_BLAKE2B_OUT_MAX;
	pOut = (uint8*)xrtMalloc(iOutLen);
	if (pOut == NULL) { return NULL; }
	if (!xrtBlake2bHash(pData, iSize, iOutLen, pKey, iKeyLen, pOut, &tError)) {
		xrtFree(pOut);
		xblake2_publish_error(&tError);
		return NULL;
	}
	return xrtBufferAdopt(pOut, iOutLen);
}

XBLAKE2_EXPORT void* xblake2b_hash_mac_str_bytes(const char* sText, size_t iSize,
	const char* sKey, size_t iKeyLen, size_t iOutLen)
{
	return xblake2b_hash_mac_bytes((const void*)sText, iSize, (const void*)sKey, iKeyLen, iOutLen);
}

/* ---- BLAKE2s 一次性哈希 ---- */

XBLAKE2_EXPORT const char* xblake2s_hash_hex(const void* pData, size_t iSize,
	size_t iOutLen, const void* pKey, size_t iKeyLen)
{
	uint8 pOut[XRT_BLAKE2S_OUT_MAX];
	xrtblake2error tError;
	xrtClearError();
	if (iOutLen == 0 || iOutLen > XRT_BLAKE2S_OUT_MAX) iOutLen = XRT_BLAKE2S_OUT_MAX;
	if (!xrtBlake2sHash(pData, iSize, iOutLen, pKey, iKeyLen, pOut, &tError)) {
		xblake2_publish_error(&tError);
		return "";
	}
	return xblake2_format_hex(pOut, iOutLen);
}

XBLAKE2_EXPORT const char* xblake2s_hash_str_hex(const char* sText, size_t iSize,
	size_t iOutLen, const char* sKey, size_t iKeyLen)
{
	return xblake2s_hash_hex((const void*)sText, iSize, iOutLen, (const void*)sKey, iKeyLen);
}

XBLAKE2_EXPORT void* xblake2s_hash_bytes(const void* pData, size_t iSize, size_t iOutLen)
{
	uint8* pOut;
	xrtblake2error tError;
	xrtClearError();
	if (iOutLen == 0 || iOutLen > XRT_BLAKE2S_OUT_MAX) iOutLen = XRT_BLAKE2S_OUT_MAX;
	pOut = (uint8*)xrtMalloc(iOutLen);
	if (pOut == NULL) { return NULL; }
	if (!xrtBlake2sHash(pData, iSize, iOutLen, NULL, 0, pOut, &tError)) {
		xrtFree(pOut);
		xblake2_publish_error(&tError);
		return NULL;
	}
	return xrtBufferAdopt(pOut, iOutLen);
}

XBLAKE2_EXPORT void* xblake2s_hash_str_bytes(const char* sText, size_t iSize, size_t iOutLen)
{
	return xblake2s_hash_bytes((const void*)sText, iSize, iOutLen);
}

XBLAKE2_EXPORT void* xblake2s_hash_mac_bytes(const void* pData, size_t iSize,
	const void* pKey, size_t iKeyLen, size_t iOutLen)
{
	uint8* pOut;
	xrtblake2error tError;
	xrtClearError();
	if (iOutLen == 0 || iOutLen > XRT_BLAKE2S_OUT_MAX) iOutLen = XRT_BLAKE2S_OUT_MAX;
	pOut = (uint8*)xrtMalloc(iOutLen);
	if (pOut == NULL) { return NULL; }
	if (!xrtBlake2sHash(pData, iSize, iOutLen, pKey, iKeyLen, pOut, &tError)) {
		xrtFree(pOut);
		xblake2_publish_error(&tError);
		return NULL;
	}
	return xrtBufferAdopt(pOut, iOutLen);
}

XBLAKE2_EXPORT void* xblake2s_hash_mac_str_bytes(const char* sText, size_t iSize,
	const char* sKey, size_t iKeyLen, size_t iOutLen)
{
	return xblake2s_hash_mac_bytes((const void*)sText, iSize, (const void*)sKey, iKeyLen, iOutLen);
}



/* ---- 文件哈希 ---- */

XBLAKE2_EXPORT const char* xblake2b_file_hex(str sPath, unsigned long long iMaxInput,
	size_t iOutLen)
{
	xrtblake2options tOptions;
	xrtblake2error tError;
	xreader pReader = NULL;
	uint8 pOut[XRT_BLAKE2B_OUT_MAX];
	bool bResult;
	xrtClearError();
	if (iOutLen == 0 || iOutLen > XRT_BLAKE2B_OUT_MAX) iOutLen = XRT_BLAKE2B_OUT_MAX;
	xrtBlake2OptionsInit(&tOptions);
	if (iMaxInput != 0u) { tOptions.tLimits.iMaxInputBytes = (uint64)iMaxInput; }
	if (sPath == NULL || sPath[0] == 0) {
		xrtSetError(xrtFormat("blake2b file path is required"), TRUE);
		return "";
	}
	if (!xrtPathExists(sPath)) {
		xrtSetError(xrtFormat("blake2b file does not exist: %s", sPath), TRUE);
		return "";
	}
	pReader = xrtReaderOpen(sPath);
	if (pReader == NULL) {
		xrtSetError(xrtFormat("cannot open blake2b file: %s", sPath), TRUE);
		return "";
	}
	bResult = xrtBlake2bFile(pReader, &tOptions.tLimits, iOutLen, pOut, &tError);
	xrtReaderDestroy(pReader);
	if (!bResult) { xblake2_publish_error(&tError); return ""; }
	return xblake2_format_hex(pOut, iOutLen);
}

XBLAKE2_EXPORT const char* xblake2s_file_hex(str sPath, unsigned long long iMaxInput,
	size_t iOutLen)
{
	xrtblake2options tOptions;
	xrtblake2error tError;
	xreader pReader = NULL;
	uint8 pOut[XRT_BLAKE2S_OUT_MAX];
	bool bResult;
	xrtClearError();
	if (iOutLen == 0 || iOutLen > XRT_BLAKE2S_OUT_MAX) iOutLen = XRT_BLAKE2S_OUT_MAX;
	xrtBlake2OptionsInit(&tOptions);
	if (iMaxInput != 0u) { tOptions.tLimits.iMaxInputBytes = (uint64)iMaxInput; }
	if (sPath == NULL || sPath[0] == 0) {
		xrtSetError(xrtFormat("blake2s file path is required"), TRUE);
		return "";
	}
	if (!xrtPathExists(sPath)) {
		xrtSetError(xrtFormat("blake2s file does not exist: %s", sPath), TRUE);
		return "";
	}
	pReader = xrtReaderOpen(sPath);
	if (pReader == NULL) {
		xrtSetError(xrtFormat("cannot open blake2s file: %s", sPath), TRUE);
		return "";
	}
	bResult = xrtBlake2sFile(pReader, &tOptions.tLimits, iOutLen, pOut, &tError);
	xrtReaderDestroy(pReader);
	if (!bResult) { xblake2_publish_error(&tError); return ""; }
	return xblake2_format_hex(pOut, iOutLen);
}



/* ---- BLAKE2b 流式哈希器 ---- */

XBLAKE2_EXPORT void* xblake2b_hasher_create(size_t iOutLen, unsigned long long iMaxInput)
{
	xrtblake2options tOptions;
	xrtblake2bhasher* pHasher;
	xrtClearError();
	xrtBlake2OptionsInit(&tOptions);
	if (iMaxInput != 0u) { tOptions.tLimits.iMaxInputBytes = (uint64)iMaxInput; }
	pHasher = xrtBlake2bHasherCreate(&tOptions, (size_t)iOutLen, NULL, 0);
	if (pHasher == NULL) {
		xrtSetError(xrtFormat("blake2b hasher create failed"), TRUE);
		return NULL;
	}
	return pHasher;
}

XBLAKE2_EXPORT void* xblake2b_hasher_create_keyed(size_t iOutLen, const void* pKey,
	size_t iKeyLen, unsigned long long iMaxInput)
{
	xrtblake2options tOptions;
	xrtblake2bhasher* pHasher;
	xrtClearError();
	xrtBlake2OptionsInit(&tOptions);
	if (iMaxInput != 0u) { tOptions.tLimits.iMaxInputBytes = (uint64)iMaxInput; }
	pHasher = xrtBlake2bHasherCreate(&tOptions, (size_t)iOutLen, pKey, (size_t)iKeyLen);
	if (pHasher == NULL) {
		xrtSetError(xrtFormat("blake2b keyed hasher create failed"), TRUE);
		return NULL;
	}
	return pHasher;
}

XBLAKE2_EXPORT void xblake2b_hasher_destroy(void* pHandle)
{
	xrtBlake2bHasherDestroy((xrtblake2bhasher*)pHandle);
}

XBLAKE2_EXPORT bool xblake2b_hasher_update(void* pHandle, const void* pData, size_t iSize)
{
	xrtblake2bhasher* pHasher = (xrtblake2bhasher*)pHandle;
	if (!xrtBlake2bHasherUpdate(pHasher, pData, iSize)) {
		xblake2_publish_error(xrtBlake2bHasherError(pHasher));
		return FALSE;
	}
	return TRUE;
}

XBLAKE2_EXPORT bool xblake2b_hasher_update_str(void* pHandle, const char* sText, size_t iSize)
{
	return xblake2b_hasher_update(pHandle, (const void*)sText, iSize);
}

XBLAKE2_EXPORT const char* xblake2b_hasher_digest_hex(void* pHandle, size_t iOutLen)
{
	xrtblake2bhasher* pHasher = (xrtblake2bhasher*)pHandle;
	uint8 pOut[XRT_BLAKE2B_OUT_MAX];
	if (iOutLen == 0 || iOutLen > XRT_BLAKE2B_OUT_MAX) iOutLen = XRT_BLAKE2B_OUT_MAX;
	if (!xrtBlake2bHasherDigest(pHasher, pOut, iOutLen)) {
		xblake2_publish_error(xrtBlake2bHasherError(pHasher));
		return "";
	}
	return xblake2_format_hex(pOut, iOutLen);
}

XBLAKE2_EXPORT void* xblake2b_hasher_digest_bytes(void* pHandle, size_t iOutLen)
{
	xrtblake2bhasher* pHasher = (xrtblake2bhasher*)pHandle;
	uint8* pOut;
	if (iOutLen == 0 || iOutLen > XRT_BLAKE2B_OUT_MAX) iOutLen = XRT_BLAKE2B_OUT_MAX;
	pOut = (uint8*)xrtMalloc(iOutLen);
	if (pOut == NULL) { return NULL; }
	if (!xrtBlake2bHasherDigest(pHasher, pOut, iOutLen)) {
		xrtFree(pOut);
		xblake2_publish_error(xrtBlake2bHasherError(pHasher));
		return NULL;
	}
	return xrtBufferAdopt(pOut, iOutLen);
}

XBLAKE2_EXPORT bool xblake2b_hasher_reset(void* pHandle)
{
	xrtblake2bhasher* pHasher = (xrtblake2bhasher*)pHandle;
	if (!xrtBlake2bHasherReset(pHasher)) {
		xblake2_publish_error(xrtBlake2bHasherError(pHasher));
		return FALSE;
	}
	return TRUE;
}

XBLAKE2_EXPORT unsigned long long xblake2b_hasher_input_bytes(void* pHandle)
{
	return (unsigned long long)xrtBlake2bHasherInputBytes((const xrtblake2bhasher*)pHandle);
}

XBLAKE2_EXPORT void* xblake2b_hasher_adopt(void* pHandle)
{
	return pHandle;
}

XBLAKE2_EXPORT void* xblake2b_hasher_clone(void* pHandle)
{
	xrtblake2bhasher* pSrc = (xrtblake2bhasher*)pHandle;
	if (pSrc == NULL) { return NULL; }
	return xrtBlake2bHasherClone(pSrc);
}



/* ---- BLAKE2s 流式哈希器 ---- */

XBLAKE2_EXPORT void* xblake2s_hasher_create(size_t iOutLen, unsigned long long iMaxInput)
{
	xrtblake2options tOptions;
	xrtblake2shasher* pHasher;
	xrtClearError();
	xrtBlake2OptionsInit(&tOptions);
	if (iMaxInput != 0u) { tOptions.tLimits.iMaxInputBytes = (uint64)iMaxInput; }
	pHasher = xrtBlake2sHasherCreate(&tOptions, (size_t)iOutLen, NULL, 0);
	if (pHasher == NULL) {
		xrtSetError(xrtFormat("blake2s hasher create failed"), TRUE);
		return NULL;
	}
	return pHasher;
}

XBLAKE2_EXPORT void* xblake2s_hasher_create_keyed(size_t iOutLen, const void* pKey,
	size_t iKeyLen, unsigned long long iMaxInput)
{
	xrtblake2options tOptions;
	xrtblake2shasher* pHasher;
	xrtClearError();
	xrtBlake2OptionsInit(&tOptions);
	if (iMaxInput != 0u) { tOptions.tLimits.iMaxInputBytes = (uint64)iMaxInput; }
	pHasher = xrtBlake2sHasherCreate(&tOptions, (size_t)iOutLen, pKey, (size_t)iKeyLen);
	if (pHasher == NULL) {
		xrtSetError(xrtFormat("blake2s keyed hasher create failed"), TRUE);
		return NULL;
	}
	return pHasher;
}

XBLAKE2_EXPORT void xblake2s_hasher_destroy(void* pHandle)
{
	xrtBlake2sHasherDestroy((xrtblake2shasher*)pHandle);
}

XBLAKE2_EXPORT bool xblake2s_hasher_update(void* pHandle, const void* pData, size_t iSize)
{
	xrtblake2shasher* pHasher = (xrtblake2shasher*)pHandle;
	if (!xrtBlake2sHasherUpdate(pHasher, pData, iSize)) {
		xblake2_publish_error(xrtBlake2sHasherError(pHasher));
		return FALSE;
	}
	return TRUE;
}

XBLAKE2_EXPORT bool xblake2s_hasher_update_str(void* pHandle, const char* sText, size_t iSize)
{
	return xblake2s_hasher_update(pHandle, (const void*)sText, iSize);
}

XBLAKE2_EXPORT const char* xblake2s_hasher_digest_hex(void* pHandle, size_t iOutLen)
{
	xrtblake2shasher* pHasher = (xrtblake2shasher*)pHandle;
	uint8 pOut[XRT_BLAKE2S_OUT_MAX];
	if (iOutLen == 0 || iOutLen > XRT_BLAKE2S_OUT_MAX) iOutLen = XRT_BLAKE2S_OUT_MAX;
	if (!xrtBlake2sHasherDigest(pHasher, pOut, iOutLen)) {
		xblake2_publish_error(xrtBlake2sHasherError(pHasher));
		return "";
	}
	return xblake2_format_hex(pOut, iOutLen);
}

XBLAKE2_EXPORT void* xblake2s_hasher_digest_bytes(void* pHandle, size_t iOutLen)
{
	xrtblake2shasher* pHasher = (xrtblake2shasher*)pHandle;
	uint8* pOut;
	if (iOutLen == 0 || iOutLen > XRT_BLAKE2S_OUT_MAX) iOutLen = XRT_BLAKE2S_OUT_MAX;
	pOut = (uint8*)xrtMalloc(iOutLen);
	if (pOut == NULL) { return NULL; }
	if (!xrtBlake2sHasherDigest(pHasher, pOut, iOutLen)) {
		xrtFree(pOut);
		xblake2_publish_error(xrtBlake2sHasherError(pHasher));
		return NULL;
	}
	return xrtBufferAdopt(pOut, iOutLen);
}

XBLAKE2_EXPORT bool xblake2s_hasher_reset(void* pHandle)
{
	xrtblake2shasher* pHasher = (xrtblake2shasher*)pHandle;
	if (!xrtBlake2sHasherReset(pHasher)) {
		xblake2_publish_error(xrtBlake2sHasherError(pHasher));
		return FALSE;
	}
	return TRUE;
}

XBLAKE2_EXPORT unsigned long long xblake2s_hasher_input_bytes(void* pHandle)
{
	return (unsigned long long)xrtBlake2sHasherInputBytes((const xrtblake2shasher*)pHandle);
}

XBLAKE2_EXPORT void* xblake2s_hasher_adopt(void* pHandle)
{
	return pHandle;
}

XBLAKE2_EXPORT void* xblake2s_hasher_clone(void* pHandle)
{
	xrtblake2shasher* pSrc = (xrtblake2shasher*)pHandle;
	if (pSrc == NULL) { return NULL; }
	return xrtBlake2sHasherClone(pSrc);
}
