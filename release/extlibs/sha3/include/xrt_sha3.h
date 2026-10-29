#ifndef XRT_SHA3_H
#define XRT_SHA3_H

#ifndef XRT_MODULE_IO_FILE
#define XRT_MODULE_IO_FILE
#endif
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
	XRT_SHA3_ERROR_LIMIT = 4,
	XRT_SHA3_ERROR_CANCELLED = 5,
	XRT_SHA3_ERROR_STATE = 6
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

XRT_API void xrtSha3OptionsInit(xrtsha3options* pOptions);
XRT_API void xrtSha3ErrorInit(xrtsha3error* pError);
/* 固定摘要的自然字节数；非法位宽返回零并设置 XRT 错误。 */
XRT_API size_t xrtSha3DigestSize(int iBits);

/* 流式哈希器 */
XRT_API xrtsha3hasher* xrtSha3HasherCreate(const xrtsha3options* pOptions, int iBits);
XRT_API xrtsha3hasher* xrtSha3ShakeHasherCreate(const xrtsha3options* pOptions, int iStrength);
XRT_API bool xrtSha3HasherUpdate(xrtsha3hasher* pHasher, const void* pData, size_t iSize);
XRT_API bool xrtSha3HasherDigest(const xrtsha3hasher* pHasher, uint8* pOut, size_t iOutLen);
/* 分配输出前检查状态、精确长度与预算，不改变流状态。 */
XRT_API bool xrtSha3HasherCheckOutput(const xrtsha3hasher* pHasher, size_t iOutLen);
XRT_API bool xrtSha3HasherReset(xrtsha3hasher* pHasher);
XRT_API uint64 xrtSha3HasherInputBytes(const xrtsha3hasher* pHasher);
/* SHA3 返回自然长度，SHAKE 返回零，表示变长输出。 */
XRT_API int xrtSha3HasherOutBytes(const xrtsha3hasher* pHasher);
XRT_API xrtsha3hasher* xrtSha3HasherClone(const xrtsha3hasher* pSrc);
XRT_API const xrtsha3error* xrtSha3HasherError(const xrtsha3hasher* pHasher);
XRT_API void xrtSha3HasherDestroy(xrtsha3hasher* pHasher);

/* 一次性哈希 */
XRT_API bool xrtSha3Hash(const void* pData, size_t iSize, int iBits,
	uint8* pOut, size_t iOutLen, xrtsha3error* pError);
XRT_API bool xrtShakeHash(const void* pData, size_t iSize, int iStrength,
	uint8* pOut, size_t iOutLen, xrtsha3error* pError);

/* 文件流式哈希 */
XRT_API bool xrtSha3File(xreader* pReader, const xrtresourcelimits* pLimits, int iBits,
	uint8* pOut, size_t iOutLen, xrtprogressproc pProgress, ptr pUserData,
	xrtsha3error* pError);

#ifdef __cplusplus
}
#endif

#endif
