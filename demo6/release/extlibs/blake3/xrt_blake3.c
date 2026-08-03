#include "include/xrt_blake3.h"
#include "third_party/blake3.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define XRT_BLAKE3_CHUNK 262144u

struct xrtblake3hasher {
	blake3_hasher Hasher;
	xrtblake3options Options;
	xrtblake3error Error;
	uint64 InputBytes;
	uint8 Mode; /* 0=normal, 1=keyed, 2=derive */
	uint8 Key[XRT_BLAKE3_KEY_LEN];
	char Context[256];
	bool Initialized;
	bool Failed;
};

static void __xrtBlake3SetError(xrtblake3error* pError, int iCode, int iBackend,
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

XXAPI void xrtBlake3ErrorInit(xrtblake3error* pError)
{
	if (pError == NULL) { return; }
	memset(pError, 0, sizeof(*pError));
	pError->iCode = XRT_BLAKE3_OK;
}

XXAPI void xrtBlake3OptionsInit(xrtblake3options* pOptions)
{
	if (pOptions == NULL) { return; }
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->iSize = sizeof(*pOptions);
	pOptions->iVersion = XRT_BLAKE3_OPTIONS_VERSION;
	xrtResourceLimitsInit(&pOptions->tLimits);
}

static xrtblake3hasher* __xrtBlake3Alloc(const xrtblake3options* pOptions)
{
	xrtblake3hasher* pHasher = (xrtblake3hasher*)xrtMalloc(sizeof(xrtblake3hasher));
	if (pHasher == NULL) { return NULL; }
	memset(pHasher, 0, sizeof(*pHasher));
	xrtBlake3ErrorInit(&pHasher->Error);
	if (pOptions != NULL) {
		pHasher->Options = *pOptions;
	} else {
		xrtBlake3OptionsInit(&pHasher->Options);
	}
	pHasher->Initialized = TRUE;
	return pHasher;
}

XXAPI xrtblake3hasher* xrtBlake3HasherCreate(const xrtblake3options* pOptions)
{
	xrtblake3hasher* pHasher = __xrtBlake3Alloc(pOptions);
	if (pHasher == NULL) { return NULL; }
	blake3_hasher_init(&pHasher->Hasher);
	pHasher->Mode = 0;
	return pHasher;
}

XXAPI xrtblake3hasher* xrtBlake3HasherCreateKeyed(const xrtblake3options* pOptions,
	const uint8 pKey[XRT_BLAKE3_KEY_LEN])
{
	xrtblake3hasher* pHasher = __xrtBlake3Alloc(pOptions);
	if (pHasher == NULL) { return NULL; }
	blake3_hasher_init_keyed(&pHasher->Hasher, pKey);
	memcpy(pHasher->Key, pKey, XRT_BLAKE3_KEY_LEN);
	pHasher->Mode = 1;
	return pHasher;
}

XXAPI xrtblake3hasher* xrtBlake3HasherCreateDeriveKey(const xrtblake3options* pOptions,
	const char* sContext)
{
	xrtblake3hasher* pHasher = __xrtBlake3Alloc(pOptions);
	if (pHasher == NULL) { return NULL; }
	blake3_hasher_init_derive_key(&pHasher->Hasher, sContext);
	strncpy(pHasher->Context, sContext, sizeof(pHasher->Context) - 1);
	pHasher->Mode = 2;
	return pHasher;
}

XXAPI bool xrtBlake3HasherUpdate(xrtblake3hasher* pHasher, const void* pData, size_t iSize)
{
	if (pHasher == NULL || pHasher->Failed) { return FALSE; }
	if (iSize != 0u && pData == NULL) {
		__xrtBlake3SetError(&pHasher->Error, XRT_BLAKE3_ERROR_ARGUMENT, 0,
			pHasher->InputBytes, 0, "invalid blake3 input");
		pHasher->Failed = TRUE;
		return FALSE;
	}
	if (pHasher->Options.tLimits.iMaxInputBytes != 0u &&
		pHasher->InputBytes + iSize > pHasher->Options.tLimits.iMaxInputBytes) {
		__xrtBlake3SetError(&pHasher->Error, XRT_BLAKE3_ERROR_LIMIT, 0,
			pHasher->InputBytes, 0, "blake3 input exceeds maxInput limit");
		pHasher->Failed = TRUE;
		return FALSE;
	}
	if (iSize != 0u) {
		blake3_hasher_update(&pHasher->Hasher, pData, iSize);
	}
	pHasher->InputBytes += (uint64)iSize;
	return TRUE;
}

XXAPI bool xrtBlake3HasherDigest(const xrtblake3hasher* pHasher, uint8* pOut, size_t iOutLen)
{
	if (pHasher == NULL || pHasher->Failed || pOut == NULL || iOutLen == 0u) { return FALSE; }
	blake3_hasher_finalize(&pHasher->Hasher, pOut, iOutLen);
	return TRUE;
}

XXAPI bool xrtBlake3HasherReset(xrtblake3hasher* pHasher)
{
	if (pHasher == NULL || pHasher->Failed) { return FALSE; }
	if (pHasher->Mode == 0) {
		blake3_hasher_init(&pHasher->Hasher);
	} else if (pHasher->Mode == 1) {
		blake3_hasher_init_keyed(&pHasher->Hasher, pHasher->Key);
	} else {
		blake3_hasher_init_derive_key(&pHasher->Hasher, pHasher->Context);
	}
	pHasher->InputBytes = 0u;
	return TRUE;
}

XXAPI uint64 xrtBlake3HasherInputBytes(const xrtblake3hasher* pHasher)
{
	return pHasher != NULL ? pHasher->InputBytes : 0u;
}

XXAPI xrtblake3hasher* xrtBlake3HasherClone(const xrtblake3hasher* pSrc)
{
	xrtblake3hasher* pNew;
	if (pSrc == NULL) { return NULL; }
	pNew = (xrtblake3hasher*)xrtMalloc(sizeof(xrtblake3hasher));
	if (pNew == NULL) { return NULL; }
	memcpy(pNew, pSrc, sizeof(*pNew));
	return pNew;
}

XXAPI const xrtblake3error* xrtBlake3HasherError(const xrtblake3hasher* pHasher)
{
	return pHasher != NULL ? &pHasher->Error : NULL;
}

XXAPI void xrtBlake3HasherDestroy(xrtblake3hasher* pHasher)
{
	if (pHasher == NULL) { return; }
	xrtFree(pHasher);
}


/* ---- 一次性哈希 ---- */

XXAPI bool xrtBlake3Hash(const void* pData, size_t iSize, uint8* pOut, size_t iOutLen,
	xrtblake3error* pError)
{
	blake3_hasher hasher;
	xrtBlake3ErrorInit(pError);
	if (pOut == NULL || iOutLen == 0u || (pData == NULL && iSize != 0u)) {
		__xrtBlake3SetError(pError, XRT_BLAKE3_ERROR_ARGUMENT, 0, 0, 0, "invalid blake3 input");
		return FALSE;
	}
	blake3_hasher_init(&hasher);
	if (iSize != 0u) blake3_hasher_update(&hasher, pData, iSize);
	blake3_hasher_finalize(&hasher, pOut, iOutLen);
	return TRUE;
}

XXAPI bool xrtBlake3HashKeyed(const void* pData, size_t iSize,
	const uint8 pKey[XRT_BLAKE3_KEY_LEN], uint8* pOut, size_t iOutLen,
	xrtblake3error* pError)
{
	blake3_hasher hasher;
	xrtBlake3ErrorInit(pError);
	if (pOut == NULL || iOutLen == 0u || pKey == NULL) {
		__xrtBlake3SetError(pError, XRT_BLAKE3_ERROR_ARGUMENT, 0, 0, 0, "invalid blake3 key");
		return FALSE;
	}
	blake3_hasher_init_keyed(&hasher, pKey);
	if (iSize != 0u) blake3_hasher_update(&hasher, pData, iSize);
	blake3_hasher_finalize(&hasher, pOut, iOutLen);
	return TRUE;
}

XXAPI bool xrtBlake3HashKdf(const void* pKeyMaterial, size_t iKeyMatLen,
	const char* sContext, uint8* pOut, size_t iOutLen, xrtblake3error* pError)
{
	blake3_hasher hasher;
	xrtBlake3ErrorInit(pError);
	if (pOut == NULL || iOutLen == 0u || sContext == NULL) {
		__xrtBlake3SetError(pError, XRT_BLAKE3_ERROR_ARGUMENT, 0, 0, 0, "invalid blake3 kdf");
		return FALSE;
	}
	blake3_hasher_init_derive_key(&hasher, sContext);
	if (iKeyMatLen != 0u) blake3_hasher_update(&hasher, pKeyMaterial, iKeyMatLen);
	blake3_hasher_finalize(&hasher, pOut, iOutLen);
	return TRUE;
}


/* ---- 文件流式哈希 ---- */

XXAPI bool xrtBlake3File(xreader pReader, const xrtresourcelimits* pLimits,
	uint8* pOut, size_t iOutLen, xrtblake3error* pError)
{
	uint8 sInput[XRT_BLAKE3_CHUNK];
	blake3_hasher hasher;
	uint64 iTotal = 0u;
	xrtBlake3ErrorInit(pError);
	if (pReader == NULL || pOut == NULL || iOutLen == 0u) {
		__xrtBlake3SetError(pError, XRT_BLAKE3_ERROR_ARGUMENT, 0, 0, 0, "invalid blake3 stream");
		return FALSE;
	}
	blake3_hasher_init(&hasher);
	for (;;) {
		uint64 iRead = 0u;
		if (!xrtReaderRead(pReader, sInput, sizeof(sInput), &iRead)) {
			__xrtBlake3SetError(pError, XRT_BLAKE3_ERROR_IO, 0, iTotal, 0,
				"cannot read blake3 input stream");
			return FALSE;
		}
		if (iRead == 0u) { break; }
		iTotal += iRead;
		if (pLimits->iMaxInputBytes != 0u && iTotal > pLimits->iMaxInputBytes) {
			__xrtBlake3SetError(pError, XRT_BLAKE3_ERROR_LIMIT, 0, iTotal, 0,
				"blake3 input exceeds maxInput limit");
			return FALSE;
		}
		blake3_hasher_update(&hasher, sInput, (size_t)iRead);
	}
	blake3_hasher_finalize(&hasher, pOut, iOutLen);
	return TRUE;
}
