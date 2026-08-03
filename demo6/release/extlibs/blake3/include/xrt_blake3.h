#ifndef XRT_BLAKE3_H
#define XRT_BLAKE3_H

#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XRT_BLAKE3_OPTIONS_VERSION 1u
#define XRT_BLAKE3_OUT_LEN 32u
#define XRT_BLAKE3_KEY_LEN 32u

typedef enum {
	XRT_BLAKE3_OK = 0,
	XRT_BLAKE3_ERROR_ARGUMENT = 1,
	XRT_BLAKE3_ERROR_MEMORY = 2,
	XRT_BLAKE3_ERROR_IO = 3,
	XRT_BLAKE3_ERROR_LIMIT = 4,
	XRT_BLAKE3_ERROR_STATE = 5
} xrtblake3status;

typedef struct {
	int iCode;
	int iBackendCode;
	uint64 iInputOffset;
	uint64 iOutputOffset;
	char sMessage[256];
} xrtblake3error;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	xrtresourcelimits tLimits;
} xrtblake3options;

typedef struct xrtblake3hasher xrtblake3hasher;

XXAPI void xrtBlake3OptionsInit(xrtblake3options* pOptions);
XXAPI void xrtBlake3ErrorInit(xrtblake3error* pError);

/* 流式哈希器 */
XXAPI xrtblake3hasher* xrtBlake3HasherCreate(const xrtblake3options* pOptions);
XXAPI xrtblake3hasher* xrtBlake3HasherCreateKeyed(const xrtblake3options* pOptions,
	const uint8 pKey[XRT_BLAKE3_KEY_LEN]);
XXAPI xrtblake3hasher* xrtBlake3HasherCreateDeriveKey(const xrtblake3options* pOptions,
	const char* sContext);
XXAPI bool xrtBlake3HasherUpdate(xrtblake3hasher* pHasher, const void* pData, size_t iSize);
XXAPI bool xrtBlake3HasherDigest(const xrtblake3hasher* pHasher, uint8* pOut, size_t iOutLen);
XXAPI bool xrtBlake3HasherReset(xrtblake3hasher* pHasher);
XXAPI uint64 xrtBlake3HasherInputBytes(const xrtblake3hasher* pHasher);
XXAPI xrtblake3hasher* xrtBlake3HasherClone(const xrtblake3hasher* pSrc);
XXAPI const xrtblake3error* xrtBlake3HasherError(const xrtblake3hasher* pHasher);
XXAPI void xrtBlake3HasherDestroy(xrtblake3hasher* pHasher);

/* 一次性哈希 */
XXAPI bool xrtBlake3Hash(const void* pData, size_t iSize, uint8* pOut, size_t iOutLen,
	xrtblake3error* pError);
XXAPI bool xrtBlake3HashKeyed(const void* pData, size_t iSize,
	const uint8 pKey[XRT_BLAKE3_KEY_LEN], uint8* pOut, size_t iOutLen,
	xrtblake3error* pError);
XXAPI bool xrtBlake3HashKdf(const void* pKeyMaterial, size_t iKeyMatLen,
	const char* sContext, uint8* pOut, size_t iOutLen, xrtblake3error* pError);

/* 文件流式哈希 */
XXAPI bool xrtBlake3File(xreader pReader, const xrtresourcelimits* pLimits,
	uint8* pOut, size_t iOutLen, xrtblake3error* pError);

#ifdef __cplusplus
}
#endif

#endif
