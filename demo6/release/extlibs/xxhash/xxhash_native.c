#define XRT_MODULE_ERROR_FORMAT
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE

#include "xruntime.h"
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
static XXXHASH_THREAD int g_xxxhash_file_status;

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

/* 把 xxHash 模块错误映射为 XRT 通用错误类别。 */
static xerrkind xxxhash_error_kind(int iCode)
{
	switch ( iCode ) {
		case XRT_XXHASH_ERROR_ARGUMENT:
			return XERR_ARGUMENT;
		case XRT_XXHASH_ERROR_MEMORY:
			return XERR_MEMORY;
		case XRT_XXHASH_ERROR_LIMIT:
			return XERR_RANGE;
		case XRT_XXHASH_ERROR_STATE:
			return XERR_STATE;
		case XRT_XXHASH_ERROR_CANCELLED:
			return XERR_CANCELLED;
		default:
			return XERR_IO;
	}
}


/* 发布可被 XLang 和 C 调用方统一读取的结构化错误。 */
static void xxxhash_publish_error(const xrtxxhasherror* pError)
{
	const char* sMessage = pError != NULL && pError->sMessage[0] != 0 ?
		pError->sMessage : "xxhash operation failed";
	int iCode = pError != NULL ? pError->iCode : XRT_XXHASH_ERROR_IO;

	xrtSetErrorInfo(
		xxxhash_error_kind(iCode),
		"xlang.extlib.xxhash",
		iCode,
		sMessage
	);
}



XXXHASH_EXPORT bool xxxhash_available(void)
{
	return true;
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
	return xrtErrorMessage(xrtGetError());
}

XXXHASH_EXPORT int xxxhash_file_status(void)
{
	return g_xxxhash_file_status;
}



XXXHASH_EXPORT long long xxxhash_hash64(const void* pData, size_t iSize,
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

XXXHASH_EXPORT long long xxxhash_hash32(const void* pData, size_t iSize, unsigned int iSeed)
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

XXXHASH_EXPORT long long xxxhash_hash64_str(const char* sText, size_t iSize,
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
		xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.xxhash",
			XRT_XXHASH_ERROR_ARGUMENT, "xxhash file path is required");
		return false;
	}
	if ( !xrtPathExists(sPath) ) {
		xrtSetErrorFormat(XERR_NOT_FOUND, "xlang.extlib.xxhash", XRT_XXHASH_ERROR_IO,
			"xxhash file does not exist: %s", sPath);
		return false;
	}
	return true;
}

XXXHASH_EXPORT long long xxxhash_file64(str sPath, unsigned long long iSeed,
	unsigned long long iMaxInput, xvalue* pCallback)
{
	xrtxxhashoptions tOptions;
	xrtxxhasherror tError;
	xrtprogresscall tProgress;
	xreader* pReader = NULL;
	uint64 iHash = 0u;
	bool bResult;
	xrtClearError();
	g_xxxhash_file_status = 0;
	xrtProgressCallInit(&tProgress, pCallback);
	if ( !xxxhash_file_options(sPath, (uint64)iSeed, (uint64)iMaxInput, &tOptions) ) { return 0u; }
	pReader = xrtReaderOpen(sPath);
	if ( pReader == NULL ) { return 0u; }
	bResult = xrtXxhashFile64(pReader, tOptions.iSeed, &tOptions.tLimits, &iHash,
		tProgress.Callback != NULL ? xrtProgressCallInvoke : NULL, &tProgress, &tError);
	if ( !xrtReaderDestroy(pReader) && bResult ) {
		g_xxxhash_file_status = 0;
		return 0u;
	}
	if ( !bResult ) {
		if ( tProgress.InvokeFailed ) { g_xxxhash_file_status = -2; }
		else {
			g_xxxhash_file_status = tError.iCode == XRT_XXHASH_ERROR_CANCELLED ? -1 : 0;
			xxxhash_publish_error(&tError);
		}
		return 0u;
	}
	g_xxxhash_file_status = 1;
	return (long long)iHash;
}

XXXHASH_EXPORT const char* xxxhash_file64_hex(str sPath, unsigned long long iSeed,
	unsigned long long iMaxInput, xvalue* pCallback)
{
	uint64 iHash = (uint64)xxxhash_file64(sPath, iSeed, iMaxInput, pCallback);
	if ( g_xxxhash_file_status != 1 ) { return ""; }
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
		return false;
	}
	return true;
}

XXXHASH_EXPORT bool xxxhash_hasher_update_str(void* pHandle, const char* sText, size_t iSize)
{
	return xxxhash_hasher_update(pHandle, (const void*)sText, iSize);
}

XXXHASH_EXPORT long long xxxhash_hasher_digest(void* pHandle)
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
		return false;
	}
	return true;
}

XXXHASH_EXPORT long long xxxhash_hasher_input_bytes(void* pHandle)
{
	return (unsigned long long)xrtXxhashHasherInputBytes((const xrtxxhashhasher*)pHandle);
}
