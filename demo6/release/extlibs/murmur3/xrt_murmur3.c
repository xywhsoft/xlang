#include "include/xrt_murmur3.h"
#include "third_party/MurmurHash3.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define XRT_MURMUR3_CHUNK 262144u

struct xrtmurmur3hasher {
	murmur3_128_state State;
	xrtmurmur3options Options;
	xrtmurmur3error Error;
	uint64 InputBytes;
	uint32 Seed;
	bool Initialized;
	bool Failed;
};

static void __xrtMurmur3SetError(xrtmurmur3error* pError, int iCode, int iBackend,
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

XXAPI void xrtMurmur3ErrorInit(xrtmurmur3error* pError)
{
	if (pError == NULL) { return; }
	memset(pError, 0, sizeof(*pError));
	pError->iCode = XRT_MURMUR3_OK;
}

XXAPI void xrtMurmur3OptionsInit(xrtmurmur3options* pOptions)
{
	if (pOptions == NULL) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_MURMUR3_OPTIONS_VERSION;
	xrtResourceLimitsInit(&pOptions->tLimits);
}

XXAPI xrtmurmur3hasher* xrtMurmur3HasherCreate(const xrtmurmur3options* pOptions, uint32 iSeed)
{
	xrtmurmur3hasher* pHasher = (xrtmurmur3hasher*)xrtMalloc(sizeof(xrtmurmur3hasher));
	if (pHasher == NULL) { return NULL; }
	memset(pHasher, 0, sizeof(*pHasher));
	xrtMurmur3ErrorInit(&pHasher->Error);
	if (pOptions != NULL) {
		pHasher->Options = *pOptions;
	} else {
		xrtMurmur3OptionsInit(&pHasher->Options);
	}
	pHasher->Seed = iSeed;
	murmur3_128_init(&pHasher->State, iSeed);
	pHasher->Initialized = TRUE;
	return pHasher;
}

XXAPI bool xrtMurmur3HasherUpdate(xrtmurmur3hasher* pHasher, const void* pData, size_t iSize)
{
	if (pHasher == NULL || pHasher->Failed) { return FALSE; }
	if (iSize != 0u && pData == NULL) {
		__xrtMurmur3SetError(&pHasher->Error, XRT_MURMUR3_ERROR_ARGUMENT, 0,
			pHasher->InputBytes, 0, "invalid murmur3 input");
		pHasher->Failed = TRUE;
		return FALSE;
	}
	if (pHasher->Options.tLimits.iMaxInputBytes != 0u &&
		pHasher->InputBytes + iSize > pHasher->Options.tLimits.iMaxInputBytes) {
		__xrtMurmur3SetError(&pHasher->Error, XRT_MURMUR3_ERROR_LIMIT, 0,
			pHasher->InputBytes, 0, "murmur3 input exceeds maxInput limit");
		pHasher->Failed = TRUE;
		return FALSE;
	}
	if (iSize != 0u) {
		murmur3_128_update(&pHasher->State, pData, iSize);
	}
	pHasher->InputBytes += (uint64)iSize;
	return TRUE;
}

XXAPI bool xrtMurmur3HasherDigest(const xrtmurmur3hasher* pHasher, uint8* pOut)
{
	murmur3_128_state copy;
	if (pHasher == NULL || pHasher->Failed || pOut == NULL) { return FALSE; }
	memcpy(&copy, &pHasher->State, sizeof(copy));
	murmur3_128_final(&copy, pOut);
	return TRUE;
}

XXAPI bool xrtMurmur3HasherReset(xrtmurmur3hasher* pHasher)
{
	if (pHasher == NULL || pHasher->Failed) { return FALSE; }
	murmur3_128_init(&pHasher->State, pHasher->Seed);
	pHasher->InputBytes = 0u;
	return TRUE;
}

XXAPI uint64 xrtMurmur3HasherInputBytes(const xrtmurmur3hasher* pHasher)
{
	return pHasher != NULL ? pHasher->InputBytes : 0u;
}

XXAPI xrtmurmur3hasher* xrtMurmur3HasherClone(const xrtmurmur3hasher* pSrc)
{
	xrtmurmur3hasher* pNew;
	if (pSrc == NULL) { return NULL; }
	pNew = (xrtmurmur3hasher*)xrtMalloc(sizeof(xrtmurmur3hasher));
	if (pNew == NULL) { return NULL; }
	memcpy(pNew, pSrc, sizeof(*pNew));
	return pNew;
}

XXAPI const xrtmurmur3error* xrtMurmur3HasherError(const xrtmurmur3hasher* pHasher)
{
	return pHasher != NULL ? &pHasher->Error : NULL;
}

XXAPI void xrtMurmur3HasherDestroy(xrtmurmur3hasher* pHasher)
{
	if (pHasher == NULL) { return; }
	xrtFree(pHasher);
}


/* ---- 一次性哈希 ---- */

XXAPI uint32 xrtMurmur3Hash32(const void* pData, size_t iSize, uint32 iSeed,
	xrtmurmur3error* pError)
{
	xrtMurmur3ErrorInit(pError);
	if (pData == NULL && iSize != 0u) {
		__xrtMurmur3SetError(pError, XRT_MURMUR3_ERROR_ARGUMENT, 0, 0, 0, "invalid murmur3 input");
		return 0u;
	}
	return murmur3_32(pData, iSize, iSeed);
}

XXAPI bool xrtMurmur3Hash128(const void* pData, size_t iSize, uint32 iSeed,
	uint8* pOut, xrtmurmur3error* pError)
{
	xrtMurmur3ErrorInit(pError);
	if (pOut == NULL || (pData == NULL && iSize != 0u)) {
		__xrtMurmur3SetError(pError, XRT_MURMUR3_ERROR_ARGUMENT, 0, 0, 0, "invalid murmur3 input");
		return FALSE;
	}
	murmur3_128(pData, iSize, iSeed, pOut);
	return TRUE;
}


/* ---- 文件流式哈希 ---- */

XXAPI uint32 xrtMurmur3File32(xreader pReader, const xrtresourcelimits* pLimits,
	uint32 iSeed, xrtmurmur3error* pError)
{
	uint8 sInput[XRT_MURMUR3_CHUNK];
	murmur3_32_state state;
	uint64 iTotal = 0u;
	xrtMurmur3ErrorInit(pError);
	if (pReader == NULL) {
		__xrtMurmur3SetError(pError, XRT_MURMUR3_ERROR_ARGUMENT, 0, 0, 0, "invalid murmur3 stream");
		return 0u;
	}
	murmur3_32_init(&state, iSeed);
	for (;;) {
		uint64 iRead = 0u;
		if (!xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead)) {
			__xrtMurmur3SetError(pError, XRT_MURMUR3_ERROR_IO, 0, iTotal, 0,
				"cannot read murmur3 input stream");
			return 0u;
		}
		if (iRead == 0u) { break; }
		iTotal += iRead;
		if (pLimits->iMaxInputBytes != 0u && iTotal > pLimits->iMaxInputBytes) {
			__xrtMurmur3SetError(pError, XRT_MURMUR3_ERROR_LIMIT, 0, iTotal, 0,
				"murmur3 input exceeds maxInput limit");
			return 0u;
		}
		murmur3_32_update(&state, sInput, (size_t)iRead);
	}
	return murmur3_32_final(&state);
}

XXAPI bool xrtMurmur3File128(xreader pReader, const xrtresourcelimits* pLimits,
	uint32 iSeed, uint8* pOut, xrtmurmur3error* pError)
{
	uint8 sInput[XRT_MURMUR3_CHUNK];
	murmur3_128_state state;
	uint64 iTotal = 0u;
	xrtMurmur3ErrorInit(pError);
	if (pReader == NULL || pOut == NULL) {
		__xrtMurmur3SetError(pError, XRT_MURMUR3_ERROR_ARGUMENT, 0, 0, 0, "invalid murmur3 stream");
		return FALSE;
	}
	murmur3_128_init(&state, iSeed);
	for (;;) {
		uint64 iRead = 0u;
		if (!xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead)) {
			__xrtMurmur3SetError(pError, XRT_MURMUR3_ERROR_IO, 0, iTotal, 0,
				"cannot read murmur3 input stream");
			return FALSE;
		}
		if (iRead == 0u) { break; }
		iTotal += iRead;
		if (pLimits->iMaxInputBytes != 0u && iTotal > pLimits->iMaxInputBytes) {
			__xrtMurmur3SetError(pError, XRT_MURMUR3_ERROR_LIMIT, 0, iTotal, 0,
				"murmur3 input exceeds maxInput limit");
			return FALSE;
		}
		murmur3_128_update(&state, sInput, (size_t)iRead);
	}
	murmur3_128_final(&state, pOut);
	return TRUE;
}
