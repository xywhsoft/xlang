#ifndef XRT_BLAKE2_H
#define XRT_BLAKE2_H

#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XRT_BLAKE2_OPTIONS_VERSION 1u
#define XRT_BLAKE2B_OUT_MAX 64u
#define XRT_BLAKE2S_OUT_MAX 32u
#define XRT_BLAKE2B_KEY_MAX 64u
#define XRT_BLAKE2S_KEY_MAX 32u

typedef enum {
	XRT_BLAKE2_OK = 0,
	XRT_BLAKE2_ERROR_ARGUMENT = 1,
	XRT_BLAKE2_ERROR_MEMORY = 2,
	XRT_BLAKE2_ERROR_IO = 3,
	XRT_BLAKE2_ERROR_LIMIT = 4,
	XRT_BLAKE2_ERROR_STATE = 5
} xrtblake2status;

typedef struct {
	int iCode;
	int iBackendCode;
	uint64 iInputOffset;
	uint64 iOutputOffset;
	char sMessage[256];
} xrtblake2error;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	xrtresourcelimits tLimits;
} xrtblake2options;

typedef struct xrtblake2bhasher xrtblake2bhasher;
typedef struct xrtblake2shasher xrtblake2shasher;

XXAPI void xrtBlake2OptionsInit(xrtblake2options* pOptions);
XXAPI void xrtBlake2ErrorInit(xrtblake2error* pError);

/* BLAKE2b 流式哈希器 */
XXAPI xrtblake2bhasher* xrtBlake2bHasherCreate(const xrtblake2options* pOptions,
	size_t iOutLen, const void* pKey, size_t iKeyLen);
XXAPI bool xrtBlake2bHasherUpdate(xrtblake2bhasher* pHasher, const void* pData, size_t iSize);
XXAPI bool xrtBlake2bHasherDigest(const xrtblake2bhasher* pHasher, uint8* pOut, size_t iOutLen);
XXAPI bool xrtBlake2bHasherReset(xrtblake2bhasher* pHasher);
XXAPI uint64 xrtBlake2bHasherInputBytes(const xrtblake2bhasher* pHasher);
XXAPI xrtblake2bhasher* xrtBlake2bHasherClone(const xrtblake2bhasher* pSrc);
XXAPI const xrtblake2error* xrtBlake2bHasherError(const xrtblake2bhasher* pHasher);
XXAPI void xrtBlake2bHasherDestroy(xrtblake2bhasher* pHasher);

/* BLAKE2s 流式哈希器 */
XXAPI xrtblake2shasher* xrtBlake2sHasherCreate(const xrtblake2options* pOptions,
	size_t iOutLen, const void* pKey, size_t iKeyLen);
XXAPI bool xrtBlake2sHasherUpdate(xrtblake2shasher* pHasher, const void* pData, size_t iSize);
XXAPI bool xrtBlake2sHasherDigest(const xrtblake2shasher* pHasher, uint8* pOut, size_t iOutLen);
XXAPI bool xrtBlake2sHasherReset(xrtblake2shasher* pHasher);
XXAPI uint64 xrtBlake2sHasherInputBytes(const xrtblake2shasher* pHasher);
XXAPI xrtblake2shasher* xrtBlake2sHasherClone(const xrtblake2shasher* pSrc);
XXAPI const xrtblake2error* xrtBlake2sHasherError(const xrtblake2shasher* pHasher);
XXAPI void xrtBlake2sHasherDestroy(xrtblake2shasher* pHasher);

/* 一次性哈希 */
XXAPI bool xrtBlake2bHash(const void* pData, size_t iSize, size_t iOutLen,
	const void* pKey, size_t iKeyLen, uint8* pOut, xrtblake2error* pError);
XXAPI bool xrtBlake2sHash(const void* pData, size_t iSize, size_t iOutLen,
	const void* pKey, size_t iKeyLen, uint8* pOut, xrtblake2error* pError);

/* 文件流式哈希 */
XXAPI bool xrtBlake2bFile(xreader pReader, const xrtresourcelimits* pLimits,
	size_t iOutLen, uint8* pOut, xrtblake2error* pError);
XXAPI bool xrtBlake2sFile(xreader pReader, const xrtresourcelimits* pLimits,
	size_t iOutLen, uint8* pOut, xrtblake2error* pError);

#ifdef __cplusplus
}
#endif

#endif
