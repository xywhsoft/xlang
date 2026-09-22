#include "include/xrt_xxhash.h"

#define XXH_STATIC_LINKING_ONLY
#define XXH_VECTOR 0 /* XXH_SCALAR: TCC 无 SSE/AVX 内部函数，强制标量实现 */
#include "include/xrt_xxhash_tcc_compat.h" /* TCC 缺失 _umul128 的可移植实现 */
#include "third_party/xxhash.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>



#define XRT_XXHASH_CHUNK 65536u

struct xrtxxhashhasher {
	XXH64_state_t* State;
	xrtxxhashoptions Options;
	xrtxxhasherror Error;
	uint64 InputBytes;
	bool Failed;
};



static void __xrtXxhashSetError(xrtxxhasherror* pError, int iCode, int iBackend,
	uint64 iInput, uint64 iOutput, const char* sFormat, ...)
{
	va_list pArgs;
	if ( pError == NULL ) { return; }
	pError->iCode = iCode;
	pError->iBackendCode = iBackend;
	pError->iInputOffset = iInput;
	pError->iOutputOffset = iOutput;
	va_start(pArgs, sFormat);
	vsnprintf(pError->sMessage, sizeof(pError->sMessage), sFormat, pArgs);
	va_end(pArgs);
	pError->sMessage[sizeof(pError->sMessage) - 1] = '\0';
}

/* 检查输入计数累加是否溢出或越过非零硬上限。 */
static bool __xrtXxhashAddOverLimit(uint64 iCurrent, size_t iAdded, uint64 iLimit)
{
	uint64 iIncrement = (uint64)iAdded;

	if ( iIncrement > (UINT64_MAX - iCurrent) ) { return true; }
	return (iLimit != 0u) && ((iCurrent + iIncrement) > iLimit);
}



XRT_API void xrtXxhashErrorInit(xrtxxhasherror* pError)
{
	if ( pError == NULL ) { return; }
	memset(pError, 0, sizeof(*pError));
	pError->iCode = XRT_XXHASH_OK;
}

XRT_API void xrtXxhashOptionsInit(xrtxxhashoptions* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_XXHASH_OPTIONS_VERSION;
	pOptions->iSeed = 0u;
	xrtResourceLimitsInit(&pOptions->tLimits);
}



// ------------------------------------ 流式哈希器 ------------------------------------

XRT_API xrtxxhashhasher* xrtXxhashHasherCreate(const xrtxxhashoptions* pOptions)
{
	xrtxxhashhasher* pHasher;
	pHasher = (xrtxxhashhasher*)xrtMalloc(sizeof(xrtxxhashhasher));
	if ( pHasher == NULL ) { return NULL; }
	memset(pHasher, 0, sizeof(*pHasher));
	xrtXxhashErrorInit(&pHasher->Error);
	if ( pOptions != NULL ) {
		pHasher->Options = *pOptions;
	} else {
		xrtXxhashOptionsInit(&pHasher->Options);
	}
	pHasher->State = XXH64_createState();
	if ( pHasher->State == NULL ) {
		__xrtXxhashSetError(&pHasher->Error, XRT_XXHASH_ERROR_MEMORY, 0, 0, 0,
			"cannot allocate xxhash state");
		pHasher->Failed = true;
		return pHasher;
	}
	if ( XXH64_reset(pHasher->State, (XXH64_hash_t)pHasher->Options.iSeed) != XXH_OK ) {
		__xrtXxhashSetError(&pHasher->Error, XRT_XXHASH_ERROR_STATE, 0, 0, 0,
			"cannot reset xxhash state");
		pHasher->Failed = true;
		return pHasher;
	}
	return pHasher;
}

XRT_API bool xrtXxhashHasherUpdate(xrtxxhashhasher* pHasher, const void* pData, size_t iSize)
{
	if ( pHasher == NULL || pHasher->Failed ) { return false; }
	if ( iSize != 0u && pData == NULL ) {
		__xrtXxhashSetError(&pHasher->Error, XRT_XXHASH_ERROR_ARGUMENT, 0,
			pHasher->InputBytes, 0, "invalid xxhash input");
		pHasher->Failed = true;
		return false;
	}
	if ( __xrtXxhashAddOverLimit(pHasher->InputBytes, iSize,
		pHasher->Options.tLimits.iMaxInputBytes) ) {
		__xrtXxhashSetError(&pHasher->Error, XRT_XXHASH_ERROR_LIMIT, 0,
			pHasher->InputBytes, 0, "xxhash input exceeds maxInput limit");
		pHasher->Failed = true;
		return false;
	}
	if ( iSize != 0u &&
		XXH64_update(pHasher->State, pData, iSize) != XXH_OK ) {
		__xrtXxhashSetError(&pHasher->Error, XRT_XXHASH_ERROR_STATE, 0,
			pHasher->InputBytes, 0, "xxhash update failed");
		pHasher->Failed = true;
		return false;
	}
	pHasher->InputBytes += (uint64)iSize;
	return true;
}

XRT_API bool xrtXxhashHasherDigest(const xrtxxhashhasher* pHasher, uint64* pHash)
{
	if ( pHasher == NULL || pHasher->Failed || pHash == NULL ) { return false; }
	*pHash = (uint64)XXH64_digest(pHasher->State);
	return true;
}

XRT_API bool xrtXxhashHasherReset(xrtxxhashhasher* pHasher)
{
	if ( pHasher == NULL || pHasher->Failed ) { return false; }
	if ( XXH64_reset(pHasher->State, (XXH64_hash_t)pHasher->Options.iSeed) != XXH_OK ) {
		__xrtXxhashSetError(&pHasher->Error, XRT_XXHASH_ERROR_STATE, 0,
			pHasher->InputBytes, 0, "cannot reset xxhash state");
		pHasher->Failed = true;
		return false;
	}
	pHasher->InputBytes = 0u;
	return true;
}

XRT_API uint64 xrtXxhashHasherInputBytes(const xrtxxhashhasher* pHasher)
{
	return pHasher != NULL ? pHasher->InputBytes : 0u;
}

XRT_API const xrtxxhasherror* xrtXxhashHasherError(const xrtxxhashhasher* pHasher)
{
	return pHasher != NULL ? &pHasher->Error : NULL;
}

XRT_API void xrtXxhashHasherDestroy(xrtxxhashhasher* pHasher)
{
	if ( pHasher == NULL ) { return; }
	if ( pHasher->State != NULL ) { XXH64_freeState(pHasher->State); }
	xrtFree(pHasher);
}



// ------------------------------------ 一次性接口 ------------------------------------

XRT_API bool xrtXxhash64(const void* pData, size_t iSize, uint64 iSeed, uint64* pHash,
	xrtxxhasherror* pError)
{
	xrtXxhashErrorInit(pError);
	if ( pHash == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtXxhashSetError(pError, XRT_XXHASH_ERROR_ARGUMENT, 0, 0, 0, "invalid xxhash input");
		return false;
	}
	*pHash = (uint64)XXH64(pData, iSize, (XXH64_hash_t)iSeed);
	return true;
}

XRT_API bool xrtXxhash32(const void* pData, size_t iSize, uint32 iSeed, uint32* pHash,
	xrtxxhasherror* pError)
{
	xrtXxhashErrorInit(pError);
	if ( pHash == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtXxhashSetError(pError, XRT_XXHASH_ERROR_ARGUMENT, 0, 0, 0, "invalid xxhash input");
		return false;
	}
	*pHash = (uint32)XXH32(pData, iSize, (XXH32_hash_t)iSeed);
	return true;
}



// ------------------------------------ 文件流接口 ------------------------------------

XRT_API bool xrtXxhashFile64(xreader* pReader, uint64 iSeed, const xrtresourcelimits* pLimits,
	uint64* pHash, xrtprogressproc pProgress, ptr pUserData, xrtxxhasherror* pError)
{
	uint8 sInput[XRT_XXHASH_CHUNK];
	XXH64_state_t* pState;
	xrtresourcelimits Limits;
	uint64 iTotal = 0u;
	uint64 iExpected = 0u;
	bool bTotalKnown;
	xrtXxhashErrorInit(pError);
	if ( pReader == NULL || pHash == NULL ) {
		__xrtXxhashSetError(pError, XRT_XXHASH_ERROR_ARGUMENT, 0, 0, 0, "invalid xxhash stream");
		return false;
	}
	*pHash = 0u;
	if ( pLimits != NULL ) { Limits = *pLimits; }
	else { xrtResourceLimitsInit(&Limits); }
	bTotalKnown = xrtReaderCanSeek(pReader) && xrtReaderSize(pReader, &iExpected);
	if (!bTotalKnown) { xrtClearError(); }
	pState = XXH64_createState();
	if ( pState == NULL ) {
		__xrtXxhashSetError(pError, XRT_XXHASH_ERROR_MEMORY, 0, 0, 0, "cannot allocate xxhash state");
		return false;
	}
	XXH64_reset(pState, (XXH64_hash_t)iSeed);
	for ( ;; ) {
		size_t iRead = 0u;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtXxhashSetError(pError, XRT_XXHASH_ERROR_IO, 0, iTotal, 0,
				"cannot read xxhash input stream");
			XXH64_freeState(pState);
			return false;
		}
		if ( iRead == 0u ) { break; }
		if ( __xrtXxhashAddOverLimit(iTotal, iRead, Limits.iMaxInputBytes) ) {
			__xrtXxhashSetError(pError, XRT_XXHASH_ERROR_LIMIT, 0, iTotal, 0,
				"xxhash input exceeds maxInput limit");
			XXH64_freeState(pState);
			return false;
		}
		iTotal += (uint64)iRead;
		if ( XXH64_update(pState, sInput, (size_t)iRead) != XXH_OK ) {
			__xrtXxhashSetError(pError, XRT_XXHASH_ERROR_STATE, 0, iTotal, 0,
				"xxhash update failed");
			XXH64_freeState(pState);
			return false;
		}
		if (!xrtProgressReport(pProgress, pUserData, iTotal, iExpected, 0u,
			bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u)) {
			__xrtXxhashSetError(pError, XRT_XXHASH_ERROR_CANCELLED, 0, iTotal, 0,
				"xxhash operation cancelled");
			XXH64_freeState(pState);
			return false;
		}
	}
	*pHash = (uint64)XXH64_digest(pState);
	if (!xrtProgressReport(pProgress, pUserData, iTotal, iExpected, 0u,
		XRT_PROGRESS_FINAL | (bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u))) {
		__xrtXxhashSetError(pError, XRT_XXHASH_ERROR_CANCELLED, 0, iTotal, 0,
			"xxhash operation cancelled");
		XXH64_freeState(pState);
		return false;
	}
	XXH64_freeState(pState);
	return true;
}
