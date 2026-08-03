#ifndef XRT_MD5_H
#define XRT_MD5_H

#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XRT_MD5_OPTIONS_VERSION 1u
#define XRT_MD5_DIGEST_SIZE 16u

typedef enum {
	XRT_MD5_OK = 0,
	XRT_MD5_ERROR_ARGUMENT = 1,
	XRT_MD5_ERROR_MEMORY = 2,
	XRT_MD5_ERROR_IO = 3,
	XRT_MD5_ERROR_LIMIT = 4,
	XRT_MD5_ERROR_STATE = 5
} xrtmd5status;

typedef struct {
	int iCode;
	int iBackendCode;
	uint64 iInputOffset;
	uint64 iOutputOffset;
	char sMessage[256];
} xrtmd5error;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	xrtresourcelimits tLimits;
} xrtmd5options;

typedef struct xrtmd5hasher xrtmd5hasher;

XXAPI void xrtMd5OptionsInit(xrtmd5options* pOptions);
XXAPI void xrtMd5ErrorInit(xrtmd5error* pError);

/* 流式哈希器：update() 喂入数据，digest() 读取当前哈希值，reset() 复位。 */
XXAPI xrtmd5hasher* xrtMd5HasherCreate(const xrtmd5options* pOptions);
XXAPI bool xrtMd5HasherUpdate(xrtmd5hasher* pHasher, const void* pData, size_t iSize);
XXAPI bool xrtMd5HasherDigest(const xrtmd5hasher* pHasher, uint8 pDigest[XRT_MD5_DIGEST_SIZE]);
XXAPI bool xrtMd5HasherReset(xrtmd5hasher* pHasher);
XXAPI uint64 xrtMd5HasherInputBytes(const xrtmd5hasher* pHasher);
XXAPI const xrtmd5error* xrtMd5HasherError(const xrtmd5hasher* pHasher);
XXAPI void xrtMd5HasherDestroy(xrtmd5hasher* pHasher);

/* 一次性哈希。 */
XXAPI bool xrtMd5Hash(const void* pData, size_t iSize, uint8 pDigest[XRT_MD5_DIGEST_SIZE],
	xrtmd5error* pError);

/* 文件流式哈希（XRT reader 分块读取，受 maxInput 限制）。 */
XXAPI bool xrtMd5File(xreader pReader, const xrtresourcelimits* pLimits,
	uint8 pDigest[XRT_MD5_DIGEST_SIZE], xrtmd5error* pError);

#ifdef __cplusplus
}
#endif

#endif
