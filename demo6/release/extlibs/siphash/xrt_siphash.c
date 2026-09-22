#include "include/xrt_siphash.h"
#include "third_party/siphash.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define XRT_SIPHASH_CHUNK 262144u

struct xrtsiphasher {
	siphash_state State;
	siphash_state State128; /* parallel state with v1 ^= 0xee for 128-bit variant */
	xrtsiphashoptions Options;
	xrtsiphasherror Error;
	uint64 InputBytes;
	uint8 Key[XRT_SIPHASH_KEY_LEN];
	bool Initialized;
	bool Failed;
};

static void __xrtSipHashSetError(xrtsiphasherror* pError, int iCode, int iBackend,
	uint64 iInput, uint64 iOutput, const char* sFormat, ...)
{
	va_list pArgs;
	if (pError == NULL) { return; }
	pError->iCode = iCode;
	pError->iBackendCode = iBackend;
	pError->iInputOffset = iInput;
	pError->iOutputOffset = iOutput;
	va_start(pArgs, sFormat);
	vsnprintf(pError->sMessage, sizeof(pError->sMessage), sFormat, pArgs);
	va_end(pArgs);
	pError->sMessage[sizeof(pError->sMessage) - 1] = '\0';
}

XRT_API void xrtSipHashErrorInit(xrtsiphasherror* pError)
{
	if (pError == NULL) { return; }
	memset(pError, 0, sizeof(*pError));
	pError->iCode = XRT_SIPHASH_OK;
}

XRT_API void xrtSipHashOptionsInit(xrtsiphashoptions* pOptions)
{
	if (pOptions == NULL) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_SIPHASH_OPTIONS_VERSION;
	xrtResourceLimitsInit(&pOptions->tLimits);
}

XRT_API xrtsiphasher* xrtSipHasherCreate(const xrtsiphashoptions* pOptions,
	const uint8 pKey[XRT_SIPHASH_KEY_LEN])
{
	xrtsiphasher* pHasher;
	if (pKey == NULL) { return NULL; }
	pHasher = (xrtsiphasher*)xrtMalloc(sizeof(xrtsiphasher));
	if (pHasher == NULL) { return NULL; }
	memset(pHasher, 0, sizeof(*pHasher));
	xrtSipHashErrorInit(&pHasher->Error);
	if (pOptions != NULL) {
		pHasher->Options = *pOptions;
	} else {
		xrtSipHashOptionsInit(&pHasher->Options);
	}
	memcpy(pHasher->Key, pKey, XRT_SIPHASH_KEY_LEN);
	siphash_init(&pHasher->State, pKey);
	siphash_init(&pHasher->State128, pKey);
	pHasher->State128.v1 ^= 0xee; /* 128-bit variant: XOR before message processing */
	pHasher->Initialized = true;
	return pHasher;
}

XRT_API bool xrtSipHasherUpdate(xrtsiphasher* pHasher, const void* pData, size_t iSize)
{
	if (pHasher == NULL || pHasher->Failed) { return false; }
	if (iSize != 0u && pData == NULL) {
		__xrtSipHashSetError(&pHasher->Error, XRT_SIPHASH_ERROR_ARGUMENT, 0,
			pHasher->InputBytes, 0, "invalid siphash input");
		pHasher->Failed = true;
		return false;
	}
	if (pHasher->Options.tLimits.iMaxInputBytes != 0u &&
		pHasher->InputBytes + iSize > pHasher->Options.tLimits.iMaxInputBytes) {
		__xrtSipHashSetError(&pHasher->Error, XRT_SIPHASH_ERROR_LIMIT, 0,
			pHasher->InputBytes, 0, "siphash input exceeds maxInput limit");
		pHasher->Failed = true;
		return false;
	}
	if (iSize != 0u) {
		siphash_update(&pHasher->State, pData, iSize);
		siphash_update(&pHasher->State128, pData, iSize);
	}
	pHasher->InputBytes += (uint64)iSize;
	return true;
}

XRT_API uint64 xrtSipHasherDigest64(const xrtsiphasher* pHasher)
{
	siphash_state copy;
	if (pHasher == NULL || pHasher->Failed) { return 0u; }
	memcpy(&copy, &pHasher->State, sizeof(copy));
	return siphash_final(&copy);
}

XRT_API bool xrtSipHasherDigest128(const xrtsiphasher* pHasher, uint8* pOut)
{
	siphash_state copy;
	if (pHasher == NULL || pHasher->Failed || pOut == NULL) { return false; }
	memcpy(&copy, &pHasher->State128, sizeof(copy));
	siphash_final128(&copy, pOut);
	return true;
}

XRT_API bool xrtSipHasherReset(xrtsiphasher* pHasher)
{
	if (pHasher == NULL || pHasher->Failed) { return false; }
	siphash_init(&pHasher->State, pHasher->Key);
	siphash_init(&pHasher->State128, pHasher->Key);
	pHasher->State128.v1 ^= 0xee;
	pHasher->InputBytes = 0u;
	return true;
}

XRT_API uint64 xrtSipHasherInputBytes(const xrtsiphasher* pHasher)
{
	return pHasher != NULL ? pHasher->InputBytes : 0u;
}

XRT_API xrtsiphasher* xrtSipHasherClone(const xrtsiphasher* pSrc)
{
	xrtsiphasher* pNew;
	if (pSrc == NULL) { return NULL; }
	pNew = (xrtsiphasher*)xrtMalloc(sizeof(xrtsiphasher));
	if (pNew == NULL) { return NULL; }
	memcpy(pNew, pSrc, sizeof(*pNew));
	return pNew;
}

XRT_API const xrtsiphasherror* xrtSipHasherError(const xrtsiphasher* pHasher)
{
	return pHasher != NULL ? &pHasher->Error : NULL;
}

XRT_API void xrtSipHasherDestroy(xrtsiphasher* pHasher)
{
	if (pHasher == NULL) { return; }
	xrtFree(pHasher);
}


/* ---- 一次性哈希 ---- */

XRT_API uint64 xrtSipHash64(const void* pData, size_t iSize,
	const uint8 pKey[XRT_SIPHASH_KEY_LEN], xrtsiphasherror* pError)
{
	xrtSipHashErrorInit(pError);
	if (pKey == NULL || (pData == NULL && iSize != 0u)) {
		__xrtSipHashSetError(pError, XRT_SIPHASH_ERROR_ARGUMENT, 0, 0, 0, "invalid siphash input");
		return 0u;
	}
	return siphash_2_4(pData, iSize, pKey);
}

XRT_API bool xrtSipHash128(const void* pData, size_t iSize,
	const uint8 pKey[XRT_SIPHASH_KEY_LEN], uint8* pOut, xrtsiphasherror* pError)
{
	xrtSipHashErrorInit(pError);
	if (pKey == NULL || pOut == NULL || (pData == NULL && iSize != 0u)) {
		__xrtSipHashSetError(pError, XRT_SIPHASH_ERROR_ARGUMENT, 0, 0, 0, "invalid siphash input");
		return false;
	}
	siphash_2_4_128(pData, iSize, pKey, pOut);
	return true;
}


/* ---- 文件流式哈希 ---- */

XRT_API uint64 xrtSipHashFile64(xreader* pReader, const xrtresourcelimits* pLimits,
	const uint8 pKey[XRT_SIPHASH_KEY_LEN], xrtprogressproc pProgress, ptr pUserData,
	xrtsiphasherror* pError)
{
	uint8 sInput[XRT_SIPHASH_CHUNK];
	siphash_state state;
	uint64 iTotal = 0u;
	uint64 iExpected = 0u;
	bool bTotalKnown;
	xrtSipHashErrorInit(pError);
	if (pReader == NULL || pKey == NULL) {
		__xrtSipHashSetError(pError, XRT_SIPHASH_ERROR_ARGUMENT, 0, 0, 0, "invalid siphash stream");
		return 0u;
	}
	bTotalKnown = xrtReaderCanSeek(pReader) && xrtReaderSize(pReader, &iExpected);
	if (!bTotalKnown) { xrtClearError(); }
	siphash_init(&state, pKey);
	for (;;) {
		uint64 iRead = 0u;
		if (!xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead)) {
			__xrtSipHashSetError(pError, XRT_SIPHASH_ERROR_IO, 0, iTotal, 0,
				"cannot read siphash input stream");
			return 0u;
		}
		if (iRead == 0u) { break; }
		iTotal += iRead;
		if (pLimits->iMaxInputBytes != 0u && iTotal > pLimits->iMaxInputBytes) {
			__xrtSipHashSetError(pError, XRT_SIPHASH_ERROR_LIMIT, 0, iTotal, 0,
				"siphash input exceeds maxInput limit");
			return 0u;
		}
		siphash_update(&state, sInput, (size_t)iRead);
		if (!xrtProgressReport(pProgress, pUserData, iTotal, iExpected, 0u,
			bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u)) {
			__xrtSipHashSetError(pError, XRT_SIPHASH_ERROR_CANCELLED, 0, iTotal, 0,
				"siphash operation cancelled");
			return 0u;
		}
	}
	if (!xrtProgressReport(pProgress, pUserData, iTotal, iExpected, 0u,
		XRT_PROGRESS_FINAL | (bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u))) {
		__xrtSipHashSetError(pError, XRT_SIPHASH_ERROR_CANCELLED, 0, iTotal, 0,
			"siphash operation cancelled");
		return 0u;
	}
	return siphash_final(&state);
}
