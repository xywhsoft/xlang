#include "include/xrt_xxhash.h"

#define XXH_STATIC_LINKING_ONLY
#define XXH_VECTOR 0 /* XXH_SCALAR: TCC 无 SSE/AVX 内部函数，强制标量实现 */
#include "include/xrt_xxhash_tcc_compat.h" /* TCC 缺失 _umul128 的可移植实现 */
#include "third_party/xxhash.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#define XXXHASH_EXPORT __declspec(dllexport)
#else
#define XXXHASH_EXPORT
#endif

#if defined(__GNUC__)
#define XXXHASH_THREAD __thread
#elif defined(_MSC_VER)
#define XXXHASH_THREAD __declspec(thread)
#else
#define XXXHASH_THREAD
#endif

#define XXXHASH_HEX_SLOTS 8u

static XXXHASH_THREAD char g_xxxhash_hex[XXXHASH_HEX_SLOTS][24];
static XXXHASH_THREAD size_t g_xxxhash_hex_index;

static const char* xxxhash_format64(uint64 iValue)
{
	char* sSlot = g_xxxhash_hex[g_xxxhash_hex_index % XXXHASH_HEX_SLOTS];
	g_xxxhash_hex_index = (g_xxxhash_hex_index + 1u) % XXXHASH_HEX_SLOTS;
	snprintf(sSlot, sizeof(g_xxxhash_hex[0]), "%016llx", (unsigned long long)iValue);
	return sSlot;
}

static const char* xxxhash_format32(uint32 iValue)
{
	char* sSlot = g_xxxhash_hex[g_xxxhash_hex_index % XXXHASH_HEX_SLOTS];
	g_xxxhash_hex_index = (g_xxxhash_hex_index + 1u) % XXXHASH_HEX_SLOTS;
	snprintf(sSlot, sizeof(g_xxxhash_hex[0]), "%08x", (unsigned)iValue);
	return sSlot;
}

static void xxxhash_publish_error(const xrtxxhasherror* pError)
{
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "xxhash operation failed";
	xrtSetError(xrtFormat("%s", sMessage), TRUE);
}



XXXHASH_EXPORT bool xxxhash_available(void)
{
	return TRUE;
}

XXXHASH_EXPORT const char* xxxhash_version(void)
{
	static char sVersion[32];
	static int iReady = 0;
	if ( !iReady ) {
		snprintf(sVersion, sizeof(sVersion), "%d.%d.%d",
			XXH_VERSION_MAJOR, XXH_VERSION_MINOR, XXH_VERSION_RELEASE);
		iReady = 1;
	}
	return sVersion;
}

XXXHASH_EXPORT const char* xxxhash_last_error(void)
{
	return xrtGetError();
}



XXXHASH_EXPORT unsigned long long xxxhash_hash64(const void* pData, size_t iSize,
	unsigned long long iSeed)
{
	uint64 iHash = 0u;
	xrtxxhasherror tError;
	xrtClearError();
	if ( !xrtXxhash64(pData, iSize, (uint64)iSeed, &iHash, &tError) ) {
		xxxhash_publish_error(&tError);
		return 0u;
	}
	return (unsigned long long)iHash;
}

XXXHASH_EXPORT const char* xxxhash_hash64_hex(const void* pData, size_t iSize,
	unsigned long long iSeed)
{
	uint64 iHash = 0u;
	xrtxxhasherror tError;
	xrtClearError();
	if ( !xrtXxhash64(pData, iSize, (uint64)iSeed, &iHash, &tError) ) {
		xxxhash_publish_error(&tError);
		return "";
	}
	return xxxhash_format64(iHash);
}

XXXHASH_EXPORT unsigned int xxxhash_hash32(const void* pData, size_t iSize, unsigned int iSeed)
{
	uint32 iHash = 0u;
	xrtxxhasherror tError;
	xrtClearError();
	if ( !xrtXxhash32(pData, iSize, (uint32)iSeed, &iHash, &tError) ) {
		xxxhash_publish_error(&tError);
		return 0u;
	}
	return (unsigned int)iHash;
}

XXXHASH_EXPORT const char* xxxhash_hash32_hex(const void* pData, size_t iSize, unsigned int iSeed)
{
	uint32 iHash = 0u;
	xrtxxhasherror tError;
	xrtClearError();
	if ( !xrtXxhash32(pData, iSize, (uint32)iSeed, &iHash, &tError) ) {
		xxxhash_publish_error(&tError);
		return "";
	}
	return xxxhash_format32(iHash);
}

XXXHASH_EXPORT unsigned long long xxxhash_hash64_str(const char* sText, size_t iSize,
	unsigned long long iSeed)
{
	return xxxhash_hash64((const void*)sText, iSize, iSeed);
}

XXXHASH_EXPORT const char* xxxhash_hash64_str_hex(const char* sText, size_t iSize,
	unsigned long long iSeed)
{
	return xxxhash_hash64_hex((const void*)sText, iSize, iSeed);
}

XXXHASH_EXPORT const char* xxxhash_hash32_str_hex(const char* sText, size_t iSize,
	unsigned int iSeed)
{
	return xxxhash_hash32_hex((const void*)sText, iSize, iSeed);
}



static bool xxxhash_file_options(str sPath, uint64 iSeed, uint64 iMaxInput,
	xrtxxhashoptions* pOptions)
{
	xrtXxhashOptionsInit(pOptions);
	pOptions->iSeed = iSeed;
	if ( iMaxInput != 0u ) { pOptions->tLimits.iMaxInputBytes = iMaxInput; }
	if ( sPath == NULL || sPath[0] == 0 ) {
		xrtSetError(xrtFormat("xxhash file path is required"), TRUE);
		return FALSE;
	}
	if ( !xrtPathExists(sPath) ) {
		xrtSetError(xrtFormat("xxhash file does not exist: %s", sPath), TRUE);
		return FALSE;
	}
	return TRUE;
}

XXXHASH_EXPORT unsigned long long xxxhash_file64(str sPath, unsigned long long iSeed,
	unsigned long long iMaxInput)
{
	xrtxxhashoptions tOptions;
	xrtxxhasherror tError;
	xreader pReader = NULL;
	uint64 iHash = 0u;
	bool bResult;
	xrtClearError();
	if ( !xxxhash_file_options(sPath, (uint64)iSeed, (uint64)iMaxInput, &tOptions) ) { return 0u; }
	pReader = xrtReaderOpen(sPath);
	if ( pReader == NULL ) {
		xrtSetError(xrtFormat("cannot open xxhash file: %s", sPath), TRUE);
		return 0u;
	}
	bResult = xrtXxhashFile64(pReader, tOptions.iSeed, &tOptions.tLimits, &iHash, &tError);
	xrtReaderDestroy(pReader);
	if ( !bResult ) { xxxhash_publish_error(&tError); return 0u; }
	return (unsigned long long)iHash;
}

XXXHASH_EXPORT const char* xxxhash_file64_hex(str sPath, unsigned long long iSeed,
	unsigned long long iMaxInput)
{
	unsigned long long iHash = xxxhash_file64(sPath, iSeed, iMaxInput);
	if ( xrtGetError()[0] != 0 ) { return ""; }
	return xxxhash_format64((uint64)iHash);
}



XXXHASH_EXPORT void* xxxhash_hasher_create(unsigned long long iSeed, unsigned long long iMaxInput)
{
	xrtxxhashoptions tOptions;
	xrtxxhashhasher* pHasher;
	xrtClearError();
	xrtXxhashOptionsInit(&tOptions);
	tOptions.iSeed = (uint64)iSeed;
	if ( iMaxInput != 0u ) { tOptions.tLimits.iMaxInputBytes = (uint64)iMaxInput; }
	pHasher = xrtXxhashHasherCreate(&tOptions);
	if ( pHasher == NULL || xrtXxhashHasherError(pHasher)->iCode != XRT_XXHASH_OK ) {
		xxxhash_publish_error(xrtXxhashHasherError(pHasher));
		xrtXxhashHasherDestroy(pHasher);
		return NULL;
	}
	return pHasher;
}

XXXHASH_EXPORT void xxxhash_hasher_destroy(void* pHandle)
{
	xrtXxhashHasherDestroy((xrtxxhashhasher*)pHandle);
}

XXXHASH_EXPORT bool xxxhash_hasher_update(void* pHandle, const void* pData, size_t iSize)
{
	xrtxxhashhasher* pHasher = (xrtxxhashhasher*)pHandle;
	if ( !xrtXxhashHasherUpdate(pHasher, pData, iSize) ) {
		xxxhash_publish_error(xrtXxhashHasherError(pHasher));
		return FALSE;
	}
	return TRUE;
}

XXXHASH_EXPORT bool xxxhash_hasher_update_str(void* pHandle, const char* sText, size_t iSize)
{
	return xxxhash_hasher_update(pHandle, (const void*)sText, iSize);
}

XXXHASH_EXPORT unsigned long long xxxhash_hasher_digest(void* pHandle)
{
	xrtxxhashhasher* pHasher = (xrtxxhashhasher*)pHandle;
	uint64 iHash = 0u;
	if ( !xrtXxhashHasherDigest(pHasher, &iHash) ) {
		xxxhash_publish_error(xrtXxhashHasherError(pHasher));
		return 0u;
	}
	return (unsigned long long)iHash;
}

XXXHASH_EXPORT const char* xxxhash_hasher_digest_hex(void* pHandle)
{
	xrtxxhashhasher* pHasher = (xrtxxhashhasher*)pHandle;
	uint64 iHash = 0u;
	if ( !xrtXxhashHasherDigest(pHasher, &iHash) ) {
		xxxhash_publish_error(xrtXxhashHasherError(pHasher));
		return "";
	}
	return xxxhash_format64(iHash);
}

XXXHASH_EXPORT bool xxxhash_hasher_reset(void* pHandle)
{
	xrtxxhashhasher* pHasher = (xrtxxhashhasher*)pHandle;
	if ( !xrtXxhashHasherReset(pHasher) ) {
		xxxhash_publish_error(xrtXxhashHasherError(pHasher));
		return FALSE;
	}
	return TRUE;
}

XXXHASH_EXPORT unsigned long long xxxhash_hasher_input_bytes(void* pHandle)
{
	return (unsigned long long)xrtXxhashHasherInputBytes((const xrtxxhashhasher*)pHandle);
}
