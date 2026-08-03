#ifndef XRT_SIPHASH_H
#define XRT_SIPHASH_H

#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XRT_SIPHASH_OPTIONS_VERSION 1u
#define XRT_SIPHASH_KEY_LEN 16u

typedef enum {
	XRT_SIPHASH_OK = 0,
	XRT_SIPHASH_ERROR_ARGUMENT = 1,
	XRT_SIPHASH_ERROR_MEMORY = 2,
	XRT_SIPHASH_ERROR_IO = 3,
	XRT_SIPHASH_ERROR_LIMIT = 4
} xrtsiphashstatus;

typedef struct {
	int iCode;
	int iBackendCode;
	uint64 iInputOffset;
	uint64 iOutputOffset;
	char sMessage[256];
} xrtsiphasherror;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	xrtresourcelimits tLimits;
} xrtsiphashoptions;

typedef struct xrtsiphasher xrtsiphasher;

XXAPI void xrtSipHashOptionsInit(xrtsiphashoptions* pOptions);
XXAPI void xrtSipHashErrorInit(xrtsiphasherror* pError);

/* 流式哈希器 */
XXAPI xrtsiphasher* xrtSipHasherCreate(const xrtsiphashoptions* pOptions,
	const uint8 pKey[XRT_SIPHASH_KEY_LEN]);
XXAPI bool xrtSipHasherUpdate(xrtsiphasher* pHasher, const void* pData, size_t iSize);
XXAPI uint64 xrtSipHasherDigest64(const xrtsiphasher* pHasher);
XXAPI bool xrtSipHasherDigest128(const xrtsiphasher* pHasher, uint8* pOut);
XXAPI bool xrtSipHasherReset(xrtsiphasher* pHasher);
XXAPI uint64 xrtSipHasherInputBytes(const xrtsiphasher* pHasher);
XXAPI xrtsiphasher* xrtSipHasherClone(const xrtsiphasher* pSrc);
XXAPI const xrtsiphasherror* xrtSipHasherError(const xrtsiphasher* pHasher);
XXAPI void xrtSipHasherDestroy(xrtsiphasher* pHasher);

/* 一次性哈希 */
XXAPI uint64 xrtSipHash64(const void* pData, size_t iSize,
	const uint8 pKey[XRT_SIPHASH_KEY_LEN], xrtsiphasherror* pError);
XXAPI bool xrtSipHash128(const void* pData, size_t iSize,
	const uint8 pKey[XRT_SIPHASH_KEY_LEN], uint8* pOut, xrtsiphasherror* pError);

/* 文件流式哈希 */
XXAPI uint64 xrtSipHashFile64(xreader pReader, const xrtresourcelimits* pLimits,
	const uint8 pKey[XRT_SIPHASH_KEY_LEN], xrtsiphasherror* pError);

#ifdef __cplusplus
}
#endif

#endif
