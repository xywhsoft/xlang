#ifndef XRT_CRC32_H
#define XRT_CRC32_H

#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XRT_CRC32_OPTIONS_VERSION 1u

typedef enum {
	XRT_CRC32_OK = 0,
	XRT_CRC32_ERROR_ARGUMENT = 1,
	XRT_CRC32_ERROR_MEMORY = 2,
	XRT_CRC32_ERROR_IO = 3,
	XRT_CRC32_ERROR_LIMIT = 4,
	XRT_CRC32_ERROR_STATE = 5
} xrtcrc32status;

typedef struct {
	int iCode;
	int iBackendCode;
	uint64 iInputOffset;
	uint64 iOutputOffset;
	char sMessage[256];
} xrtcrc32error;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	xrtresourcelimits tLimits;
} xrtcrc32options;

typedef struct xrtcrc32hasher xrtcrc32hasher;

XXAPI void xrtCrc32OptionsInit(xrtcrc32options* pOptions);
XXAPI void xrtCrc32ErrorInit(xrtcrc32error* pError);

/* 流式哈希器 */
XXAPI xrtcrc32hasher* xrtCrc32HasherCreate(const xrtcrc32options* pOptions);
XXAPI bool xrtCrc32HasherUpdate(xrtcrc32hasher* pHasher, const void* pData, size_t iSize);
XXAPI bool xrtCrc32HasherDigest(const xrtcrc32hasher* pHasher, uint32* pCrc);
XXAPI bool xrtCrc32HasherReset(xrtcrc32hasher* pHasher);
XXAPI uint64 xrtCrc32HasherInputBytes(const xrtcrc32hasher* pHasher);
XXAPI const xrtcrc32error* xrtCrc32HasherError(const xrtcrc32hasher* pHasher);
XXAPI void xrtCrc32HasherDestroy(xrtcrc32hasher* pHasher);

/* 一次性哈希 */
XXAPI bool xrtCrc32Hash(const void* pData, size_t iSize, uint32* pCrc, xrtcrc32error* pError);

/* 文件流式哈希 */
XXAPI bool xrtCrc32File(xreader pReader, const xrtresourcelimits* pLimits,
	uint32* pCrc, xrtcrc32error* pError);

#ifdef __cplusplus
}
#endif

#endif
