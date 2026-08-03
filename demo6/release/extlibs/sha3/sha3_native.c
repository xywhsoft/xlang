#include "include/xrt_sha3.h"

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
	xrtSetError(xrtFormat("%s", sMessage), TRUE);
}



XSHA3_EXPORT bool xsha3_available(void)
{
	return TRUE;
}

XSHA3_EXPORT const char* xsha3_version(void)
{
	return "3.0.2";
}

XSHA3_EXPORT const char* xsha3_last_error(void)
{
	return xrtGetError();
}



/* ---- 一次性哈希 ---- */

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
	return xrtBufferAdopt(pOut, iOutLen);
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



/* ---- 文件哈希 ---- */

XSHA3_EXPORT const char* xsha3_file_hex(str sPath, unsigned long long iMaxInput, int iBits)
{
	xrtsha3options tOptions;
	xrtsha3error tError;
	xreader pReader = NULL;
	uint8 pOut[XRT_SHA3_MAX_OUT];
	size_t iOutLen;
	bool bResult;
	xrtClearError();
	iOutLen = (size_t)(iBits / 8);
	if (iOutLen == 0 || iOutLen > XRT_SHA3_MAX_OUT) iOutLen = 32;
	xrtSha3OptionsInit(&tOptions);
	if (iMaxInput != 0u) { tOptions.tLimits.iMaxInputBytes = (uint64)iMaxInput; }
	if (sPath == NULL || sPath[0] == 0) {
		xrtSetError(xrtFormat("sha3 file path is required"), TRUE);
		return "";
	}
	if (!xrtPathExists(sPath)) {
		xrtSetError(xrtFormat("sha3 file does not exist: %s", sPath), TRUE);
		return "";
	}
	pReader = xrtReaderOpen(sPath);
	if (pReader == NULL) {
		xrtSetError(xrtFormat("cannot open sha3 file: %s", sPath), TRUE);
		return "";
	}
	bResult = xrtSha3File(pReader, &tOptions.tLimits, iBits, pOut, iOutLen, &tError);
	xrtReaderDestroy(pReader);
	if (!bResult) { xsha3_publish_error(&tError); return ""; }
	return xsha3_format_hex(pOut, iOutLen);
}



/* ---- 流式哈希器 ---- */

XSHA3_EXPORT void* xsha3_hasher_create(int iBits, unsigned long long iMaxInput)
{
	xrtsha3options tOptions;
	xrtsha3hasher* pHasher;
	xrtClearError();
	xrtSha3OptionsInit(&tOptions);
	if (iMaxInput != 0u) { tOptions.tLimits.iMaxInputBytes = (uint64)iMaxInput; }
	pHasher = xrtSha3HasherCreate(&tOptions, iBits);
	if (pHasher == NULL) {
		xrtSetError(xrtFormat("sha3 hasher create failed"), TRUE);
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
		xrtSetError(xrtFormat("shake hasher create failed"), TRUE);
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
		return FALSE;
	}
	return TRUE;
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
	return xrtBufferAdopt(pOut, iOutLen);
}

XSHA3_EXPORT bool xsha3_hasher_reset(void* pHandle)
{
	xrtsha3hasher* pHasher = (xrtsha3hasher*)pHandle;
	if (!xrtSha3HasherReset(pHasher)) {
		xsha3_publish_error(xrtSha3HasherError(pHasher));
		return FALSE;
	}
	return TRUE;
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
