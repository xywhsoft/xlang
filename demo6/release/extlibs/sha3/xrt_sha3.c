#include "include/xrt_sha3.h"
#include "third_party/sha3.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define XRT_SHA3_CHUNK 262144u

struct xrtsha3hasher {
	sha3_ctx Ctx;
	xrtsha3options Options;
	xrtsha3error Error;
	uint64 InputBytes;
	int Bits;       /* 224/256/384/512 for SHA3, 128/256 for SHAKE */
	int IsShake;    /* 0=SHA3, 1=SHAKE */
	bool Initialized;
	bool Failed;
};

static void __xrtSha3SetError(xrtsha3error* pError, int iCode, int iBackend,
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

XRT_API void xrtSha3ErrorInit(xrtsha3error* pError)
{
	if (pError == NULL) { return; }
	memset(pError, 0, sizeof(*pError));
	pError->iCode = XRT_SHA3_OK;
}

XRT_API void xrtSha3OptionsInit(xrtsha3options* pOptions)
{
	if (pOptions == NULL) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_SHA3_OPTIONS_VERSION;
	xrtResourceLimitsInit(&pOptions->tLimits);
}

XRT_API xrtsha3hasher* xrtSha3HasherCreate(const xrtsha3options* pOptions, int iBits)
{
	xrtsha3hasher* pHasher = (xrtsha3hasher*)xrtMalloc(sizeof(xrtsha3hasher));
	if (pHasher == NULL) { return NULL; }
	memset(pHasher, 0, sizeof(*pHasher));
	xrtSha3ErrorInit(&pHasher->Error);
	if (pOptions != NULL) {
		pHasher->Options = *pOptions;
	} else {
		xrtSha3OptionsInit(&pHasher->Options);
	}
	pHasher->Bits = iBits;
	pHasher->IsShake = 0;
	sha3_init(&pHasher->Ctx, iBits);
	pHasher->Initialized = true;
	return pHasher;
}

XRT_API xrtsha3hasher* xrtSha3ShakeHasherCreate(const xrtsha3options* pOptions, int iStrength)
{
	xrtsha3hasher* pHasher = (xrtsha3hasher*)xrtMalloc(sizeof(xrtsha3hasher));
	if (pHasher == NULL) { return NULL; }
	memset(pHasher, 0, sizeof(*pHasher));
	xrtSha3ErrorInit(&pHasher->Error);
	if (pOptions != NULL) {
		pHasher->Options = *pOptions;
	} else {
		xrtSha3OptionsInit(&pHasher->Options);
	}
	pHasher->Bits = iStrength;
	pHasher->IsShake = 1;
	shake_init(&pHasher->Ctx, iStrength);
	pHasher->Initialized = true;
	return pHasher;
}

XRT_API bool xrtSha3HasherUpdate(xrtsha3hasher* pHasher, const void* pData, size_t iSize)
{
	if (pHasher == NULL || pHasher->Failed) { return false; }
	if (iSize != 0u && pData == NULL) {
		__xrtSha3SetError(&pHasher->Error, XRT_SHA3_ERROR_ARGUMENT, 0,
			pHasher->InputBytes, 0, "invalid sha3 input");
		pHasher->Failed = true;
		return false;
	}
	if (pHasher->Options.tLimits.iMaxInputBytes != 0u &&
		pHasher->InputBytes + iSize > pHasher->Options.tLimits.iMaxInputBytes) {
		__xrtSha3SetError(&pHasher->Error, XRT_SHA3_ERROR_LIMIT, 0,
			pHasher->InputBytes, 0, "sha3 input exceeds maxInput limit");
		pHasher->Failed = true;
		return false;
	}
	if (iSize != 0u) {
		sha3_update(&pHasher->Ctx, pData, iSize);
	}
	pHasher->InputBytes += (uint64)iSize;
	return true;
}

XRT_API bool xrtSha3HasherDigest(const xrtsha3hasher* pHasher, uint8* pOut, size_t iOutLen)
{
	sha3_ctx copy;
	if (pHasher == NULL || pHasher->Failed || pOut == NULL || iOutLen == 0u) { return false; }
	memcpy(&copy, &pHasher->Ctx, sizeof(copy));
	if (pHasher->IsShake) {
		shake_final(&copy, pOut, iOutLen);
	} else {
		sha3_final(&copy, pOut, iOutLen);
	}
	return true;
}

XRT_API bool xrtSha3HasherReset(xrtsha3hasher* pHasher)
{
	if (pHasher == NULL || pHasher->Failed) { return false; }
	if (pHasher->IsShake) {
		shake_init(&pHasher->Ctx, pHasher->Bits);
	} else {
		sha3_init(&pHasher->Ctx, pHasher->Bits);
	}
	pHasher->InputBytes = 0u;
	return true;
}

XRT_API uint64 xrtSha3HasherInputBytes(const xrtsha3hasher* pHasher)
{
	return pHasher != NULL ? pHasher->InputBytes : 0u;
}

XRT_API int xrtSha3HasherOutBytes(const xrtsha3hasher* pHasher)
{
	if (pHasher == NULL) { return 32; }
	if (pHasher->IsShake) { return 32; }
	return pHasher->Bits / 8;
}

XRT_API xrtsha3hasher* xrtSha3HasherClone(const xrtsha3hasher* pSrc)
{
	xrtsha3hasher* pNew;
	if (pSrc == NULL) { return NULL; }
	pNew = (xrtsha3hasher*)xrtMalloc(sizeof(xrtsha3hasher));
	if (pNew == NULL) { return NULL; }
	memcpy(pNew, pSrc, sizeof(*pNew));
	return pNew;
}

XRT_API const xrtsha3error* xrtSha3HasherError(const xrtsha3hasher* pHasher)
{
	return pHasher != NULL ? &pHasher->Error : NULL;
}

XRT_API void xrtSha3HasherDestroy(xrtsha3hasher* pHasher)
{
	if (pHasher == NULL) { return; }
	xrtFree(pHasher);
}


/* ---- 一次性哈希 ---- */

XRT_API bool xrtSha3Hash(const void* pData, size_t iSize, int iBits,
	uint8* pOut, size_t iOutLen, xrtsha3error* pError)
{
	xrtSha3ErrorInit(pError);
	if (pOut == NULL || iOutLen == 0 || (pData == NULL && iSize != 0u)) {
		__xrtSha3SetError(pError, XRT_SHA3_ERROR_ARGUMENT, 0, 0, 0, "invalid sha3 input");
		return false;
	}
	sha3_hash(pData, iSize, iBits, pOut, iOutLen);
	return true;
}

XRT_API bool xrtShakeHash(const void* pData, size_t iSize, int iStrength,
	uint8* pOut, size_t iOutLen, xrtsha3error* pError)
{
	xrtSha3ErrorInit(pError);
	if (pOut == NULL || iOutLen == 0 || (pData == NULL && iSize != 0u)) {
		__xrtSha3SetError(pError, XRT_SHA3_ERROR_ARGUMENT, 0, 0, 0, "invalid shake input");
		return false;
	}
	shake_hash(pData, iSize, iStrength, pOut, iOutLen);
	return true;
}


/* ---- 文件流式哈希 ---- */

XRT_API bool xrtSha3File(xreader* pReader, const xrtresourcelimits* pLimits, int iBits,
	uint8* pOut, size_t iOutLen, xrtprogressproc pProgress, ptr pUserData,
	xrtsha3error* pError)
{
	uint8 sInput[XRT_SHA3_CHUNK];
	sha3_ctx ctx;
	uint64 iTotal = 0u;
	uint64 iExpected = 0u;
	bool bTotalKnown;
	xrtSha3ErrorInit(pError);
	if (pReader == NULL || pOut == NULL || iOutLen == 0) {
		__xrtSha3SetError(pError, XRT_SHA3_ERROR_ARGUMENT, 0, 0, 0, "invalid sha3 stream");
		return false;
	}
	bTotalKnown = xrtReaderCanSeek(pReader) && xrtReaderSize(pReader, &iExpected);
	if (!bTotalKnown) { xrtClearError(); }
	sha3_init(&ctx, iBits);
	for (;;) {
		uint64 iRead = 0u;
		if (!xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead)) {
			__xrtSha3SetError(pError, XRT_SHA3_ERROR_IO, 0, iTotal, 0,
				"cannot read sha3 input stream");
			return false;
		}
		if (iRead == 0u) { break; }
		iTotal += iRead;
		if (pLimits->iMaxInputBytes != 0u && iTotal > pLimits->iMaxInputBytes) {
			__xrtSha3SetError(pError, XRT_SHA3_ERROR_LIMIT, 0, iTotal, 0,
				"sha3 input exceeds maxInput limit");
			return false;
		}
		sha3_update(&ctx, sInput, (size_t)iRead);
		if (!xrtProgressReport(pProgress, pUserData, iTotal, iExpected, 0u,
			bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u)) {
			__xrtSha3SetError(pError, XRT_SHA3_ERROR_CANCELLED, 0, iTotal, 0,
				"sha3 operation cancelled");
			return false;
		}
	}
	sha3_final(&ctx, pOut, iOutLen);
	if (!xrtProgressReport(pProgress, pUserData, iTotal, iExpected, 0u,
		XRT_PROGRESS_FINAL | (bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u))) {
		__xrtSha3SetError(pError, XRT_SHA3_ERROR_CANCELLED, 0, iTotal, 0,
			"sha3 operation cancelled");
		return false;
	}
	return true;
}
