#ifndef XRT_MD5_H
#define XRT_MD5_H

/* MD5 文件哈希契约直接依赖通用 Reader。 */
#ifndef XRT_MODULE_IO
#define XRT_MODULE_IO
#endif

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
	XRT_MD5_ERROR_STATE = 5,
	XRT_MD5_ERROR_CANCELLED = 6
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

XRT_API void xrtMd5OptionsInit(xrtmd5options* pOptions);
XRT_API void xrtMd5ErrorInit(xrtmd5error* pError);

/* 流式哈希器：update() 喂入数据，digest() 读取当前哈希值，reset() 复位。 */
XRT_API xrtmd5hasher* xrtMd5HasherCreate(const xrtmd5options* pOptions);
XRT_API bool xrtMd5HasherUpdate(xrtmd5hasher* pHasher, const void* pData, size_t iSize);
XRT_API bool xrtMd5HasherDigest(const xrtmd5hasher* pHasher, uint8 pDigest[XRT_MD5_DIGEST_SIZE]);
XRT_API bool xrtMd5HasherReset(xrtmd5hasher* pHasher);
XRT_API uint64 xrtMd5HasherInputBytes(const xrtmd5hasher* pHasher);
XRT_API const xrtmd5error* xrtMd5HasherError(const xrtmd5hasher* pHasher);
XRT_API void xrtMd5HasherDestroy(xrtmd5hasher* pHasher);

/* 一次性哈希。 */
XRT_API bool xrtMd5Hash(const void* pData, size_t iSize, uint8 pDigest[XRT_MD5_DIGEST_SIZE],
	xrtmd5error* pError);

/* 文件流式哈希（XRT reader 分块读取，受 maxInput 限制）。 */
XRT_API bool xrtMd5File(xreader* pReader, const xrtresourcelimits* pLimits,
	uint8 pDigest[XRT_MD5_DIGEST_SIZE], xrtprogressproc pProgress, ptr pUserData,
	xrtmd5error* pError);

#ifdef __cplusplus
}
#endif

#endif
