#include "include/xrt_murmur3.h"

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
	xrtSetError(xrtFormat("%s", sMessage), TRUE);
}



XMURMUR3_EXPORT bool xmurmur3_available(void)
{
	return TRUE;
}

XMURMUR3_EXPORT const char* xmurmur3_version(void)
{
	return "3.0.0";
}

XMURMUR3_EXPORT const char* xmurmur3_last_error(void)
{
	return xrtGetError();
}



/* ---- 一次性哈希 ---- */

XMURMUR3_EXPORT unsigned int xmurmur3_hash32(const void* pData, size_t iSize, unsigned int iSeed)
{
	xrtmurmur3error tError;
	xrtClearError();
	return (unsigned int)xrtMurmur3Hash32(pData, iSize, (uint32)iSeed, &tError);
}

XMURMUR3_EXPORT unsigned int xmurmur3_hash32_str(const char* sText, size_t iSize, unsigned int iSeed)
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
	return xrtBufferAdopt(pOut, 4);
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
	return xrtBufferAdopt(pOut, 16);
}

XMURMUR3_EXPORT void* xmurmur3_hash128_str_bytes(const char* sText, size_t iSize, unsigned int iSeed)
{
	return xmurmur3_hash128_bytes((const void*)sText, iSize, iSeed);
}



/* ---- 文件哈希 ---- */

XMURMUR3_EXPORT unsigned int xmurmur3_file32(str sPath, unsigned long long iMaxInput,
	unsigned int iSeed)
{
	xrtmurmur3options tOptions;
	xrtmurmur3error tError;
	xreader pReader = NULL;
	uint32 result;
	xrtClearError();
	xrtMurmur3OptionsInit(&tOptions);
	if (iMaxInput != 0u) { tOptions.tLimits.iMaxInputBytes = (uint64)iMaxInput; }
	if (sPath == NULL || sPath[0] == 0) {
		xrtSetError(xrtFormat("murmur3 file path is required"), TRUE);
		return 0u;
	}
	if (!xrtPathExists(sPath)) {
		xrtSetError(xrtFormat("murmur3 file does not exist: %s", sPath), TRUE);
		return 0u;
	}
	pReader = xrtReaderOpen(sPath);
	if (pReader == NULL) {
		xrtSetError(xrtFormat("cannot open murmur3 file: %s", sPath), TRUE);
		return 0u;
	}
	result = xrtMurmur3File32(pReader, &tOptions.tLimits, (uint32)iSeed, &tError);
	xrtReaderDestroy(pReader);
	return (unsigned int)result;
}

XMURMUR3_EXPORT const char* xmurmur3_file128_hex(str sPath, unsigned long long iMaxInput,
	unsigned int iSeed)
{
	xrtmurmur3options tOptions;
	xrtmurmur3error tError;
	xreader pReader = NULL;
	uint8 pOut[16];
	bool bResult;
	xrtClearError();
	xrtMurmur3OptionsInit(&tOptions);
	if (iMaxInput != 0u) { tOptions.tLimits.iMaxInputBytes = (uint64)iMaxInput; }
	if (sPath == NULL || sPath[0] == 0) {
		xrtSetError(xrtFormat("murmur3 file path is required"), TRUE);
		return "";
	}
	if (!xrtPathExists(sPath)) {
		xrtSetError(xrtFormat("murmur3 file does not exist: %s", sPath), TRUE);
		return "";
	}
	pReader = xrtReaderOpen(sPath);
	if (pReader == NULL) {
		xrtSetError(xrtFormat("cannot open murmur3 file: %s", sPath), TRUE);
		return "";
	}
	bResult = xrtMurmur3File128(pReader, &tOptions.tLimits, (uint32)iSeed, pOut, &tError);
	xrtReaderDestroy(pReader);
	if (!bResult) { xmurmur3_publish_error(&tError); return ""; }
	return xmurmur3_format_hex(pOut, 16);
}



/* ---- 流式哈希器 ---- */

XMURMUR3_EXPORT void* xmurmur3_hasher_create(unsigned int iSeed, unsigned long long iMaxInput)
{
	xrtmurmur3options tOptions;
	xrtmurmur3hasher* pHasher;
	xrtClearError();
	xrtMurmur3OptionsInit(&tOptions);
	if (iMaxInput != 0u) { tOptions.tLimits.iMaxInputBytes = (uint64)iMaxInput; }
	pHasher = xrtMurmur3HasherCreate(&tOptions, (uint32)iSeed);
	if (pHasher == NULL) {
		xrtSetError(xrtFormat("murmur3 hasher create failed"), TRUE);
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
		return FALSE;
	}
	return TRUE;
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
	return xrtBufferAdopt(pOut, 16);
}

XMURMUR3_EXPORT bool xmurmur3_hasher_reset(void* pHandle)
{
	xrtmurmur3hasher* pHasher = (xrtmurmur3hasher*)pHandle;
	if (!xrtMurmur3HasherReset(pHasher)) {
		xmurmur3_publish_error(xrtMurmur3HasherError(pHasher));
		return FALSE;
	}
	return TRUE;
}

XMURMUR3_EXPORT unsigned long long xmurmur3_hasher_input_bytes(void* pHandle)
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
