#include "include/xrt_xxhash.h"

#define XXH_STATIC_LINKING_ONLY
#define XXH_VECTOR 0 /* XXH_SCALAR: TCC 无 SSE/AVX 内部函数，强制标量实现 */
#include "include/xrt_xxhash_tcc_compat.h" /* TCC 缺失 _umul128 的可移植实现 */
#include "third_party/xxhash.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>



#define XRT_XXHASH_CHUNK 65536u

struct xrtxxhashhasher {
	XXH64_state_t* State;
	xrtxxhashoptions Options;
	xrtxxhasherror Error;
	uint64 InputBytes;
	bool Initialized;
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

static bool __xrtXxhashOverInputLimit(const xrtresourcelimits* pLimits, uint64 iInput)
{
	return pLimits->iMaxInputBytes != 0u && iInput > pLimits->iMaxInputBytes;
}



XXAPI void xrtXxhashErrorInit(xrtxxhasherror* pError)
{
	if ( pError == NULL ) { return; }
	memset(pError, 0, sizeof(*pError));
	pError->iCode = XRT_XXHASH_OK;
}

XXAPI void xrtXxhashOptionsInit(xrtxxhashoptions* pOptions)
{
	if ( pOptions == NULL ) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_XXHASH_OPTIONS_VERSION;
	pOptions->iSeed = 0u;
	xrtResourceLimitsInit(&pOptions->tLimits);
}



// ------------------------------------ 流式哈希器 ------------------------------------

XXAPI xrtxxhashhasher* xrtXxhashHasherCreate(const xrtxxhashoptions* pOptions)
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
		pHasher->Failed = TRUE;
		return pHasher;
	}
	if ( XXH64_reset(pHasher->State, (XXH64_hash_t)pHasher->Options.iSeed) != XXH_OK ) {
		__xrtXxhashSetError(&pHasher->Error, XRT_XXHASH_ERROR_STATE, 0, 0, 0,
			"cannot reset xxhash state");
		pHasher->Failed = TRUE;
		return pHasher;
	}
	pHasher->Initialized = TRUE;
	return pHasher;
}

XXAPI bool xrtXxhashHasherUpdate(xrtxxhashhasher* pHasher, const void* pData, size_t iSize)
{
	if ( pHasher == NULL || pHasher->Failed ) { return FALSE; }
	if ( iSize != 0u && pData == NULL ) {
		__xrtXxhashSetError(&pHasher->Error, XRT_XXHASH_ERROR_ARGUMENT, 0,
			pHasher->InputBytes, 0, "invalid xxhash input");
		pHasher->Failed = TRUE;
		return FALSE;
	}
	if ( pHasher->Options.tLimits.iMaxInputBytes != 0u &&
		pHasher->InputBytes + iSize > pHasher->Options.tLimits.iMaxInputBytes ) {
		__xrtXxhashSetError(&pHasher->Error, XRT_XXHASH_ERROR_LIMIT, 0,
			pHasher->InputBytes, 0, "xxhash input exceeds maxInput limit");
		pHasher->Failed = TRUE;
		return FALSE;
	}
	if ( iSize != 0u &&
		XXH64_update(pHasher->State, pData, iSize) != XXH_OK ) {
		__xrtXxhashSetError(&pHasher->Error, XRT_XXHASH_ERROR_STATE, 0,
			pHasher->InputBytes, 0, "xxhash update failed");
		pHasher->Failed = TRUE;
		return FALSE;
	}
	pHasher->InputBytes += (uint64)iSize;
	return TRUE;
}

XXAPI bool xrtXxhashHasherDigest(const xrtxxhashhasher* pHasher, uint64* pHash)
{
	if ( pHasher == NULL || pHasher->Failed || pHash == NULL ) { return FALSE; }
	*pHash = (uint64)XXH64_digest(pHasher->State);
	return TRUE;
}

XXAPI bool xrtXxhashHasherReset(xrtxxhashhasher* pHasher)
{
	if ( pHasher == NULL || pHasher->Failed ) { return FALSE; }
	if ( XXH64_reset(pHasher->State, (XXH64_hash_t)pHasher->Options.iSeed) != XXH_OK ) {
		__xrtXxhashSetError(&pHasher->Error, XRT_XXHASH_ERROR_STATE, 0,
			pHasher->InputBytes, 0, "cannot reset xxhash state");
		pHasher->Failed = TRUE;
		return FALSE;
	}
	pHasher->InputBytes = 0u;
	return TRUE;
}

XXAPI uint64 xrtXxhashHasherInputBytes(const xrtxxhashhasher* pHasher)
{
	return pHasher != NULL ? pHasher->InputBytes : 0u;
}

XXAPI const xrtxxhasherror* xrtXxhashHasherError(const xrtxxhashhasher* pHasher)
{
	return pHasher != NULL ? &pHasher->Error : NULL;
}

XXAPI void xrtXxhashHasherDestroy(xrtxxhashhasher* pHasher)
{
	if ( pHasher == NULL ) { return; }
	if ( pHasher->State != NULL ) { XXH64_freeState(pHasher->State); }
	xrtFree(pHasher);
}



// ------------------------------------ 一次性接口 ------------------------------------

XXAPI bool xrtXxhash64(const void* pData, size_t iSize, uint64 iSeed, uint64* pHash,
	xrtxxhasherror* pError)
{
	xrtXxhashErrorInit(pError);
	if ( pHash == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtXxhashSetError(pError, XRT_XXHASH_ERROR_ARGUMENT, 0, 0, 0, "invalid xxhash input");
		return FALSE;
	}
	*pHash = (uint64)XXH64(pData, iSize, (XXH64_hash_t)iSeed);
	return TRUE;
}

XXAPI bool xrtXxhash32(const void* pData, size_t iSize, uint32 iSeed, uint32* pHash,
	xrtxxhasherror* pError)
{
	xrtXxhashErrorInit(pError);
	if ( pHash == NULL || (pData == NULL && iSize != 0u) ) {
		__xrtXxhashSetError(pError, XRT_XXHASH_ERROR_ARGUMENT, 0, 0, 0, "invalid xxhash input");
		return FALSE;
	}
	*pHash = (uint32)XXH32(pData, iSize, (XXH32_hash_t)iSeed);
	return TRUE;
}



// ------------------------------------ 文件流接口 ------------------------------------

XXAPI bool xrtXxhashFile64(xreader pReader, uint64 iSeed, const xrtresourcelimits* pLimits,
	uint64* pHash, xrtxxhasherror* pError)
{
	uint8 sInput[XRT_XXHASH_CHUNK];
	XXH64_state_t* pState;
	uint64 iTotal = 0u;
	xrtXxhashErrorInit(pError);
	if ( pReader == NULL || pHash == NULL ) {
		__xrtXxhashSetError(pError, XRT_XXHASH_ERROR_ARGUMENT, 0, 0, 0, "invalid xxhash stream");
		return FALSE;
	}
	pState = XXH64_createState();
	if ( pState == NULL ) {
		__xrtXxhashSetError(pError, XRT_XXHASH_ERROR_MEMORY, 0, 0, 0, "cannot allocate xxhash state");
		return FALSE;
	}
	XXH64_reset(pState, (XXH64_hash_t)iSeed);
	for ( ;; ) {
		uint64 iRead = 0u;
		if ( !xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead) ) {
			__xrtXxhashSetError(pError, XRT_XXHASH_ERROR_IO, 0, iTotal, 0,
				"cannot read xxhash input stream");
			XXH64_freeState(pState);
			return FALSE;
		}
		if ( iRead == 0u ) { break; }
		iTotal += iRead;
		if ( __xrtXxhashOverInputLimit(pLimits, iTotal) ) {
			__xrtXxhashSetError(pError, XRT_XXHASH_ERROR_LIMIT, 0, iTotal, 0,
				"xxhash input exceeds maxInput limit");
			XXH64_freeState(pState);
			return FALSE;
		}
		if ( XXH64_update(pState, sInput, (size_t)iRead) != XXH_OK ) {
			__xrtXxhashSetError(pError, XRT_XXHASH_ERROR_STATE, 0, iTotal, 0,
				"xxhash update failed");
			XXH64_freeState(pState);
			return FALSE;
		}
	}
	*pHash = (uint64)XXH64_digest(pState);
	XXH64_freeState(pState);
	return TRUE;
}
