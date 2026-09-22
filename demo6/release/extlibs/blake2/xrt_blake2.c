#include "include/xrt_blake2.h"
#include "third_party/blake2.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define XRT_BLAKE2_CHUNK 262144u

/* ═══════════════ 内部工具 ═══════════════ */

static void __xrtBlake2SetError(xrtblake2error* pError, int iCode, int iBackend,
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

XRT_API void xrtBlake2ErrorInit(xrtblake2error* pError)
{
	if (pError == NULL) { return; }
	memset(pError, 0, sizeof(*pError));
	pError->iCode = XRT_BLAKE2_OK;
}

XRT_API void xrtBlake2OptionsInit(xrtblake2options* pOptions)
{
	if (pOptions == NULL) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_BLAKE2_OPTIONS_VERSION;
	xrtResourceLimitsInit(&pOptions->tLimits);
}


/* ═══════════════ BLAKE2b 流式哈希器 ═══════════════ */

struct xrtblake2bhasher {
	blake2b_state State;
	xrtblake2options Options;
	xrtblake2error Error;
	uint64 InputBytes;
	size_t OutLen;
	uint8 Key[XRT_BLAKE2B_KEY_MAX];
	size_t KeyLen;
	bool Initialized;
	bool Failed;
};

XRT_API xrtblake2bhasher* xrtBlake2bHasherCreate(const xrtblake2options* pOptions,
	size_t iOutLen, const void* pKey, size_t iKeyLen)
{
	xrtblake2bhasher* pHasher;
	if (iOutLen == 0 || iOutLen > XRT_BLAKE2B_OUT_MAX) { iOutLen = XRT_BLAKE2B_OUT_MAX; }
	if (iKeyLen > XRT_BLAKE2B_KEY_MAX) { iKeyLen = XRT_BLAKE2B_KEY_MAX; }
	pHasher = (xrtblake2bhasher*)xrtMalloc(sizeof(xrtblake2bhasher));
	if (pHasher == NULL) { return NULL; }
	memset(pHasher, 0, sizeof(*pHasher));
	xrtBlake2ErrorInit(&pHasher->Error);
	if (pOptions != NULL) {
		pHasher->Options = *pOptions;
	} else {
		xrtBlake2OptionsInit(&pHasher->Options);
	}
	pHasher->OutLen = iOutLen;
	pHasher->KeyLen = iKeyLen;
	if (iKeyLen > 0 && pKey != NULL) {
		memcpy(pHasher->Key, pKey, iKeyLen);
		blake2b_init_key(&pHasher->State, iOutLen, pKey, iKeyLen);
	} else {
		blake2b_init(&pHasher->State, iOutLen);
	}
	pHasher->Initialized = true;
	return pHasher;
}

XRT_API bool xrtBlake2bHasherUpdate(xrtblake2bhasher* pHasher, const void* pData, size_t iSize)
{
	if (pHasher == NULL || pHasher->Failed) { return false; }
	if (iSize != 0u && pData == NULL) {
		__xrtBlake2SetError(&pHasher->Error, XRT_BLAKE2_ERROR_ARGUMENT, 0,
			pHasher->InputBytes, 0, "invalid blake2b input");
		pHasher->Failed = true;
		return false;
	}
	if (pHasher->Options.tLimits.iMaxInputBytes != 0u &&
		pHasher->InputBytes + iSize > pHasher->Options.tLimits.iMaxInputBytes) {
		__xrtBlake2SetError(&pHasher->Error, XRT_BLAKE2_ERROR_LIMIT, 0,
			pHasher->InputBytes, 0, "blake2b input exceeds maxInput limit");
		pHasher->Failed = true;
		return false;
	}
	if (iSize != 0u) {
		blake2b_update(&pHasher->State, pData, iSize);
	}
	pHasher->InputBytes += (uint64)iSize;
	return true;
}

XRT_API bool xrtBlake2bHasherDigest(const xrtblake2bhasher* pHasher, uint8* pOut, size_t iOutLen)
{
	blake2b_state copy;
	if (pHasher == NULL || pHasher->Failed || pOut == NULL || iOutLen == 0u) { return false; }
	if (iOutLen > pHasher->OutLen) { iOutLen = pHasher->OutLen; }
	memcpy(&copy, &pHasher->State, sizeof(copy));
	blake2b_final(&copy, pOut, iOutLen);
	return true;
}

XRT_API bool xrtBlake2bHasherReset(xrtblake2bhasher* pHasher)
{
	if (pHasher == NULL || pHasher->Failed) { return false; }
	if (pHasher->KeyLen > 0) {
		blake2b_init_key(&pHasher->State, pHasher->OutLen, pHasher->Key, pHasher->KeyLen);
	} else {
		blake2b_init(&pHasher->State, pHasher->OutLen);
	}
	pHasher->InputBytes = 0u;
	return true;
}

XRT_API uint64 xrtBlake2bHasherInputBytes(const xrtblake2bhasher* pHasher)
{
	return pHasher != NULL ? pHasher->InputBytes : 0u;
}

XRT_API xrtblake2bhasher* xrtBlake2bHasherClone(const xrtblake2bhasher* pSrc)
{
	xrtblake2bhasher* pNew;
	if (pSrc == NULL) { return NULL; }
	pNew = (xrtblake2bhasher*)xrtMalloc(sizeof(xrtblake2bhasher));
	if (pNew == NULL) { return NULL; }
	memcpy(pNew, pSrc, sizeof(*pNew));
	return pNew;
}

XRT_API const xrtblake2error* xrtBlake2bHasherError(const xrtblake2bhasher* pHasher)
{
	return pHasher != NULL ? &pHasher->Error : NULL;
}

XRT_API void xrtBlake2bHasherDestroy(xrtblake2bhasher* pHasher)
{
	if (pHasher == NULL) { return; }
	xrtFree(pHasher);
}


/* ═══════════════ BLAKE2s 流式哈希器 ═══════════════ */

struct xrtblake2shasher {
	blake2s_state State;
	xrtblake2options Options;
	xrtblake2error Error;
	uint64 InputBytes;
	size_t OutLen;
	uint8 Key[XRT_BLAKE2S_KEY_MAX];
	size_t KeyLen;
	bool Initialized;
	bool Failed;
};

XRT_API xrtblake2shasher* xrtBlake2sHasherCreate(const xrtblake2options* pOptions,
	size_t iOutLen, const void* pKey, size_t iKeyLen)
{
	xrtblake2shasher* pHasher;
	if (iOutLen == 0 || iOutLen > XRT_BLAKE2S_OUT_MAX) { iOutLen = XRT_BLAKE2S_OUT_MAX; }
	if (iKeyLen > XRT_BLAKE2S_KEY_MAX) { iKeyLen = XRT_BLAKE2S_KEY_MAX; }
	pHasher = (xrtblake2shasher*)xrtMalloc(sizeof(xrtblake2shasher));
	if (pHasher == NULL) { return NULL; }
	memset(pHasher, 0, sizeof(*pHasher));
	xrtBlake2ErrorInit(&pHasher->Error);
	if (pOptions != NULL) {
		pHasher->Options = *pOptions;
	} else {
		xrtBlake2OptionsInit(&pHasher->Options);
	}
	pHasher->OutLen = iOutLen;
	pHasher->KeyLen = iKeyLen;
	if (iKeyLen > 0 && pKey != NULL) {
		memcpy(pHasher->Key, pKey, iKeyLen);
		blake2s_init_key(&pHasher->State, iOutLen, pKey, iKeyLen);
	} else {
		blake2s_init(&pHasher->State, iOutLen);
	}
	pHasher->Initialized = true;
	return pHasher;
}

XRT_API bool xrtBlake2sHasherUpdate(xrtblake2shasher* pHasher, const void* pData, size_t iSize)
{
	if (pHasher == NULL || pHasher->Failed) { return false; }
	if (iSize != 0u && pData == NULL) {
		__xrtBlake2SetError(&pHasher->Error, XRT_BLAKE2_ERROR_ARGUMENT, 0,
			pHasher->InputBytes, 0, "invalid blake2s input");
		pHasher->Failed = true;
		return false;
	}
	if (pHasher->Options.tLimits.iMaxInputBytes != 0u &&
		pHasher->InputBytes + iSize > pHasher->Options.tLimits.iMaxInputBytes) {
		__xrtBlake2SetError(&pHasher->Error, XRT_BLAKE2_ERROR_LIMIT, 0,
			pHasher->InputBytes, 0, "blake2s input exceeds maxInput limit");
		pHasher->Failed = true;
		return false;
	}
	if (iSize != 0u) {
		blake2s_update(&pHasher->State, pData, iSize);
	}
	pHasher->InputBytes += (uint64)iSize;
	return true;
}

XRT_API bool xrtBlake2sHasherDigest(const xrtblake2shasher* pHasher, uint8* pOut, size_t iOutLen)
{
	blake2s_state copy;
	if (pHasher == NULL || pHasher->Failed || pOut == NULL || iOutLen == 0u) { return false; }
	if (iOutLen > pHasher->OutLen) { iOutLen = pHasher->OutLen; }
	memcpy(&copy, &pHasher->State, sizeof(copy));
	blake2s_final(&copy, pOut, iOutLen);
	return true;
}

XRT_API bool xrtBlake2sHasherReset(xrtblake2shasher* pHasher)
{
	if (pHasher == NULL || pHasher->Failed) { return false; }
	if (pHasher->KeyLen > 0) {
		blake2s_init_key(&pHasher->State, pHasher->OutLen, pHasher->Key, pHasher->KeyLen);
	} else {
		blake2s_init(&pHasher->State, pHasher->OutLen);
	}
	pHasher->InputBytes = 0u;
	return true;
}

XRT_API uint64 xrtBlake2sHasherInputBytes(const xrtblake2shasher* pHasher)
{
	return pHasher != NULL ? pHasher->InputBytes : 0u;
}

XRT_API xrtblake2shasher* xrtBlake2sHasherClone(const xrtblake2shasher* pSrc)
{
	xrtblake2shasher* pNew;
	if (pSrc == NULL) { return NULL; }
	pNew = (xrtblake2shasher*)xrtMalloc(sizeof(xrtblake2shasher));
	if (pNew == NULL) { return NULL; }
	memcpy(pNew, pSrc, sizeof(*pNew));
	return pNew;
}

XRT_API const xrtblake2error* xrtBlake2sHasherError(const xrtblake2shasher* pHasher)
{
	return pHasher != NULL ? &pHasher->Error : NULL;
}

XRT_API void xrtBlake2sHasherDestroy(xrtblake2shasher* pHasher)
{
	if (pHasher == NULL) { return; }
	xrtFree(pHasher);
}


/* ═══════════════ 一次性哈希 ═══════════════ */

XRT_API bool xrtBlake2bHash(const void* pData, size_t iSize, size_t iOutLen,
	const void* pKey, size_t iKeyLen, uint8* pOut, xrtblake2error* pError)
{
	blake2b_state state;
	xrtBlake2ErrorInit(pError);
	if (pOut == NULL || iOutLen == 0 || iOutLen > XRT_BLAKE2B_OUT_MAX) {
		__xrtBlake2SetError(pError, XRT_BLAKE2_ERROR_ARGUMENT, 0, 0, 0, "invalid blake2b outlen");
		return false;
	}
	if (pData == NULL && iSize != 0u) {
		__xrtBlake2SetError(pError, XRT_BLAKE2_ERROR_ARGUMENT, 0, 0, 0, "invalid blake2b input");
		return false;
	}
	if (iKeyLen > 0 && pKey != NULL) {
		blake2b_init_key(&state, iOutLen, pKey, iKeyLen);
	} else {
		blake2b_init(&state, iOutLen);
	}
	if (iSize != 0u) blake2b_update(&state, pData, iSize);
	blake2b_final(&state, pOut, iOutLen);
	return true;
}

XRT_API bool xrtBlake2sHash(const void* pData, size_t iSize, size_t iOutLen,
	const void* pKey, size_t iKeyLen, uint8* pOut, xrtblake2error* pError)
{
	blake2s_state state;
	xrtBlake2ErrorInit(pError);
	if (pOut == NULL || iOutLen == 0 || iOutLen > XRT_BLAKE2S_OUT_MAX) {
		__xrtBlake2SetError(pError, XRT_BLAKE2_ERROR_ARGUMENT, 0, 0, 0, "invalid blake2s outlen");
		return false;
	}
	if (pData == NULL && iSize != 0u) {
		__xrtBlake2SetError(pError, XRT_BLAKE2_ERROR_ARGUMENT, 0, 0, 0, "invalid blake2s input");
		return false;
	}
	if (iKeyLen > 0 && pKey != NULL) {
		blake2s_init_key(&state, iOutLen, pKey, iKeyLen);
	} else {
		blake2s_init(&state, iOutLen);
	}
	if (iSize != 0u) blake2s_update(&state, pData, iSize);
	blake2s_final(&state, pOut, iOutLen);
	return true;
}


/* ═══════════════ 文件流式哈希 ═══════════════ */

XRT_API bool xrtBlake2bFile(xreader* pReader, const xrtresourcelimits* pLimits,
	size_t iOutLen, uint8* pOut, xrtprogressproc pProgress, ptr pUserData,
	xrtblake2error* pError)
{
	uint8 sInput[XRT_BLAKE2_CHUNK];
	blake2b_state state;
	uint64 iTotal = 0u;
	uint64 iExpected = 0u;
	bool bTotalKnown;
	xrtBlake2ErrorInit(pError);
	if (pReader == NULL || pOut == NULL || iOutLen == 0 || iOutLen > XRT_BLAKE2B_OUT_MAX) {
		__xrtBlake2SetError(pError, XRT_BLAKE2_ERROR_ARGUMENT, 0, 0, 0, "invalid blake2b stream");
		return false;
	}
	bTotalKnown = xrtReaderCanSeek(pReader) && xrtReaderSize(pReader, &iExpected);
	if (!bTotalKnown) { xrtClearError(); }
	blake2b_init(&state, iOutLen);
	for (;;) {
		uint64 iRead = 0u;
		if (!xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead)) {
			__xrtBlake2SetError(pError, XRT_BLAKE2_ERROR_IO, 0, iTotal, 0,
				"cannot read blake2b input stream");
			return false;
		}
		if (iRead == 0u) { break; }
		iTotal += iRead;
		if (pLimits->iMaxInputBytes != 0u && iTotal > pLimits->iMaxInputBytes) {
			__xrtBlake2SetError(pError, XRT_BLAKE2_ERROR_LIMIT, 0, iTotal, 0,
				"blake2b input exceeds maxInput limit");
			return false;
		}
		blake2b_update(&state, sInput, (size_t)iRead);
		if (!xrtProgressReport(pProgress, pUserData, iTotal, iExpected, 0u,
			bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u)) {
			__xrtBlake2SetError(pError, XRT_BLAKE2_ERROR_CANCELLED, 0, iTotal, 0,
				"blake2b operation cancelled");
			return false;
		}
	}
	blake2b_final(&state, pOut, iOutLen);
	if (!xrtProgressReport(pProgress, pUserData, iTotal, iExpected, 0u,
		XRT_PROGRESS_FINAL | (bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u))) {
		__xrtBlake2SetError(pError, XRT_BLAKE2_ERROR_CANCELLED, 0, iTotal, 0,
			"blake2b operation cancelled");
		return false;
	}
	return true;
}

XRT_API bool xrtBlake2sFile(xreader* pReader, const xrtresourcelimits* pLimits,
	size_t iOutLen, uint8* pOut, xrtprogressproc pProgress, ptr pUserData,
	xrtblake2error* pError)
{
	uint8 sInput[XRT_BLAKE2_CHUNK];
	blake2s_state state;
	uint64 iTotal = 0u;
	uint64 iExpected = 0u;
	bool bTotalKnown;
	xrtBlake2ErrorInit(pError);
	if (pReader == NULL || pOut == NULL || iOutLen == 0 || iOutLen > XRT_BLAKE2S_OUT_MAX) {
		__xrtBlake2SetError(pError, XRT_BLAKE2_ERROR_ARGUMENT, 0, 0, 0, "invalid blake2s stream");
		return false;
	}
	bTotalKnown = xrtReaderCanSeek(pReader) && xrtReaderSize(pReader, &iExpected);
	if (!bTotalKnown) { xrtClearError(); }
	blake2s_init(&state, iOutLen);
	for (;;) {
		uint64 iRead = 0u;
		if (!xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead)) {
			__xrtBlake2SetError(pError, XRT_BLAKE2_ERROR_IO, 0, iTotal, 0,
				"cannot read blake2s input stream");
			return false;
		}
		if (iRead == 0u) { break; }
		iTotal += iRead;
		if (pLimits->iMaxInputBytes != 0u && iTotal > pLimits->iMaxInputBytes) {
			__xrtBlake2SetError(pError, XRT_BLAKE2_ERROR_LIMIT, 0, iTotal, 0,
				"blake2s input exceeds maxInput limit");
			return false;
		}
		blake2s_update(&state, sInput, (size_t)iRead);
		if (!xrtProgressReport(pProgress, pUserData, iTotal, iExpected, 0u,
			bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u)) {
			__xrtBlake2SetError(pError, XRT_BLAKE2_ERROR_CANCELLED, 0, iTotal, 0,
				"blake2s operation cancelled");
			return false;
		}
	}
	blake2s_final(&state, pOut, iOutLen);
	if (!xrtProgressReport(pProgress, pUserData, iTotal, iExpected, 0u,
		XRT_PROGRESS_FINAL | (bTotalKnown ? XRT_PROGRESS_TOTAL_KNOWN : 0u))) {
		__xrtBlake2SetError(pError, XRT_BLAKE2_ERROR_CANCELLED, 0, iTotal, 0,
			"blake2s operation cancelled");
		return false;
	}
	return true;
}
