#ifndef XRT_MURMUR3_H
#define XRT_MURMUR3_H

#ifndef XRT_MODULE_IO_FILE
#define XRT_MODULE_IO_FILE
#endif
#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XRT_MURMUR3_OPTIONS_VERSION 1u

typedef enum {
	XRT_MURMUR3_OK = 0,
	XRT_MURMUR3_ERROR_ARGUMENT = 1,
	XRT_MURMUR3_ERROR_MEMORY = 2,
	XRT_MURMUR3_ERROR_IO = 3,
	XRT_MURMUR3_ERROR_LIMIT = 4,
	XRT_MURMUR3_ERROR_CANCELLED = 5,
	XRT_MURMUR3_ERROR_STATE = 6
} xrtmurmur3status;

typedef struct {
	int iCode;
	int iBackendCode;
	uint64 iInputOffset;
	uint64 iOutputOffset;
	char sMessage[256];
} xrtmurmur3error;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	xrtresourcelimits tLimits;
} xrtmurmur3options;

typedef struct xrtmurmur3hasher xrtmurmur3hasher;

XRT_API void xrtMurmur3OptionsInit(xrtmurmur3options* pOptions);
XRT_API void xrtMurmur3ErrorInit(xrtmurmur3error* pError);

/* x64_128 流式对象；digest 不终结状态，reset 保留 seed/预算并恢复失败状态。 */
XRT_API xrtmurmur3hasher* xrtMurmur3HasherCreate(const xrtmurmur3options* pOptions, uint32 iSeed);
XRT_API bool xrtMurmur3HasherUpdate(xrtmurmur3hasher* pHasher, const void* pData, size_t iSize);
XRT_API bool xrtMurmur3HasherDigest(const xrtmurmur3hasher* pHasher, uint8* pOut);
XRT_API bool xrtMurmur3HasherReset(xrtmurmur3hasher* pHasher);
XRT_API uint64 xrtMurmur3HasherInputBytes(const xrtmurmur3hasher* pHasher);
XRT_API xrtmurmur3hasher* xrtMurmur3HasherClone(const xrtmurmur3hasher* pSrc);
XRT_API const xrtmurmur3error* xrtMurmur3HasherError(const xrtmurmur3hasher* pHasher);
XRT_API void xrtMurmur3HasherDestroy(xrtmurmur3hasher* pHasher);

/* 一次性 x86_32 / x64_128；零是有效摘要，失败须检查 XRT 错误或详情。 */
XRT_API uint32 xrtMurmur3Hash32(const void* pData, size_t iSize, uint32 iSeed,
	xrtmurmur3error* pError);
XRT_API bool xrtMurmur3Hash128(const void* pData, size_t iSize, uint32 iSeed,
	uint8* pOut, xrtmurmur3error* pError);

/* 借用 Reader，从当前游标到 EOF；NULL limits 使用默认预算，不关闭 Reader。 */
XRT_API uint32 xrtMurmur3File32(xreader* pReader, const xrtresourcelimits* pLimits,
	uint32 iSeed, xrtprogressproc pProgress, ptr pUserData, xrtmurmur3error* pError);
XRT_API bool xrtMurmur3File128(xreader* pReader, const xrtresourcelimits* pLimits,
	uint32 iSeed, uint8* pOut, xrtprogressproc pProgress, ptr pUserData,
	xrtmurmur3error* pError);

#ifdef __cplusplus
}
#endif

#endif
