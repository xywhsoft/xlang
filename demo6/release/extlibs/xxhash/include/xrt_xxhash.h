#ifndef XRT_XXHASH_H
#define XRT_XXHASH_H

#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XRT_XXHASH_OPTIONS_VERSION 1u

typedef enum {
	XRT_XXHASH_OK = 0,
	XRT_XXHASH_ERROR_ARGUMENT = 1,
	XRT_XXHASH_ERROR_MEMORY = 2,
	XRT_XXHASH_ERROR_IO = 3,
	XRT_XXHASH_ERROR_LIMIT = 4,
	XRT_XXHASH_ERROR_STATE = 5
} xrtxxhashstatus;

typedef struct {
	int iCode;
	int iBackendCode;
	uint64 iInputOffset;
	uint64 iOutputOffset;
	char sMessage[256];
} xrtxxhasherror;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	uint64 iSeed;
	xrtresourcelimits tLimits;
} xrtxxhashoptions;

typedef struct xrtxxhashhasher xrtxxhashhasher;

XXAPI void xrtXxhashOptionsInit(xrtxxhashoptions* pOptions);
XXAPI void xrtXxhashErrorInit(xrtxxhasherror* pError);

/* 流式哈希器（XXH64）：update() 喂入数据，digest() 读取当前哈希值，reset() 复位。 */
XXAPI xrtxxhashhasher* xrtXxhashHasherCreate(const xrtxxhashoptions* pOptions);
XXAPI bool xrtXxhashHasherUpdate(xrtxxhashhasher* pHasher, const void* pData, size_t iSize);
XXAPI bool xrtXxhashHasherDigest(const xrtxxhashhasher* pHasher, uint64* pHash);
XXAPI bool xrtXxhashHasherReset(xrtxxhashhasher* pHasher);
XXAPI uint64 xrtXxhashHasherInputBytes(const xrtxxhashhasher* pHasher);
XXAPI const xrtxxhasherror* xrtXxhashHasherError(const xrtxxhashhasher* pHasher);
XXAPI void xrtXxhashHasherDestroy(xrtxxhashhasher* pHasher);

/* 一次性哈希。 */
XXAPI bool xrtXxhash64(const void* pData, size_t iSize, uint64 iSeed, uint64* pHash,
	xrtxxhasherror* pError);
XXAPI bool xrtXxhash32(const void* pData, size_t iSize, uint32 iSeed, uint32* pHash,
	xrtxxhasherror* pError);

/* 文件流式哈希（XRT reader 分块读取，受 maxInput 限制）。 */
XXAPI bool xrtXxhashFile64(xreader pReader, uint64 iSeed, const xrtresourcelimits* pLimits,
	uint64* pHash, xrtxxhasherror* pError);

#ifdef __cplusplus
}
#endif

#endif
