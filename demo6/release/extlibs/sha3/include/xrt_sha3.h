#ifndef XRT_SHA3_H
#define XRT_SHA3_H

#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XRT_SHA3_OPTIONS_VERSION 1u
#define XRT_SHA3_MAX_OUT 64u

typedef enum {
	XRT_SHA3_OK = 0,
	XRT_SHA3_ERROR_ARGUMENT = 1,
	XRT_SHA3_ERROR_MEMORY = 2,
	XRT_SHA3_ERROR_IO = 3,
	XRT_SHA3_ERROR_LIMIT = 4
} xrtsha3status;

typedef struct {
	int iCode;
	int iBackendCode;
	uint64 iInputOffset;
	uint64 iOutputOffset;
	char sMessage[256];
} xrtsha3error;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	xrtresourcelimits tLimits;
} xrtsha3options;

typedef struct xrtsha3hasher xrtsha3hasher;

XXAPI void xrtSha3OptionsInit(xrtsha3options* pOptions);
XXAPI void xrtSha3ErrorInit(xrtsha3error* pError);

/* 流式哈希器 */
XXAPI xrtsha3hasher* xrtSha3HasherCreate(const xrtsha3options* pOptions, int iBits);
XXAPI xrtsha3hasher* xrtSha3ShakeHasherCreate(const xrtsha3options* pOptions, int iStrength);
XXAPI bool xrtSha3HasherUpdate(xrtsha3hasher* pHasher, const void* pData, size_t iSize);
XXAPI bool xrtSha3HasherDigest(const xrtsha3hasher* pHasher, uint8* pOut, size_t iOutLen);
XXAPI bool xrtSha3HasherReset(xrtsha3hasher* pHasher);
XXAPI uint64 xrtSha3HasherInputBytes(const xrtsha3hasher* pHasher);
XXAPI int xrtSha3HasherOutBytes(const xrtsha3hasher* pHasher);
XXAPI xrtsha3hasher* xrtSha3HasherClone(const xrtsha3hasher* pSrc);
XXAPI const xrtsha3error* xrtSha3HasherError(const xrtsha3hasher* pHasher);
XXAPI void xrtSha3HasherDestroy(xrtsha3hasher* pHasher);

/* 一次性哈希 */
XXAPI bool xrtSha3Hash(const void* pData, size_t iSize, int iBits,
	uint8* pOut, size_t iOutLen, xrtsha3error* pError);
XXAPI bool xrtShakeHash(const void* pData, size_t iSize, int iStrength,
	uint8* pOut, size_t iOutLen, xrtsha3error* pError);

/* 文件流式哈希 */
XXAPI bool xrtSha3File(xreader pReader, const xrtresourcelimits* pLimits, int iBits,
	uint8* pOut, size_t iOutLen, xrtsha3error* pError);

#ifdef __cplusplus
}
#endif

#endif
