#ifndef XRT_BLAKE3_H
#define XRT_BLAKE3_H

#ifndef XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_BUFFER
#endif
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
	XRT_BLAKE3_ERROR_STATE = 5,
	XRT_BLAKE3_ERROR_CANCELLED = 6
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

XRT_API void xrtBlake3OptionsInit(xrtblake3options* pOptions);
/* 文件接口只使用输入与 XOF 输出预算；其余资源字段保持零。 */
XRT_API void xrtBlake3LimitsInit(xrtresourcelimits* pLimits);
XRT_API void xrtBlake3ErrorInit(xrtblake3error* pError);

/* 流式哈希器 */
XRT_API xrtblake3hasher* xrtBlake3HasherCreate(const xrtblake3options* pOptions);
XRT_API xrtblake3hasher* xrtBlake3HasherCreateKeyed(const xrtblake3options* pOptions,
	const uint8 pKey[XRT_BLAKE3_KEY_LEN]);
/* 派生上下文按字节长度读取，允许嵌入 NUL；不使用 C 字符串截断规则。 */
XRT_API xrtblake3hasher* xrtBlake3HasherCreateDeriveKey(const xrtblake3options* pOptions,
	const void* pContext, size_t iContextLen);
XRT_API bool xrtBlake3HasherUpdate(xrtblake3hasher* pHasher, const void* pData, size_t iSize);
XRT_API bool xrtBlake3HasherDigest(xrtblake3hasher* pHasher, uint8* pOut, size_t iOutLen);
XRT_API bool xrtBlake3HasherReset(xrtblake3hasher* pHasher);
XRT_API uint64 xrtBlake3HasherInputBytes(const xrtblake3hasher* pHasher);
XRT_API xrtblake3hasher* xrtBlake3HasherClone(const xrtblake3hasher* pSrc);
XRT_API const xrtblake3error* xrtBlake3HasherError(const xrtblake3hasher* pHasher);
XRT_API void xrtBlake3HasherDestroy(xrtblake3hasher* pHasher);

/* 一次性哈希 */
XRT_API bool xrtBlake3Hash(const void* pData, size_t iSize, uint8* pOut, size_t iOutLen,
	xrtblake3error* pError);
XRT_API bool xrtBlake3HashKeyed(const void* pData, size_t iSize,
	const uint8 pKey[XRT_BLAKE3_KEY_LEN], uint8* pOut, size_t iOutLen,
	xrtblake3error* pError);
XRT_API bool xrtBlake3HashKdf(const void* pKeyMaterial, size_t iKeyMatLen,
	const void* pContext, size_t iContextLen, uint8* pOut, size_t iOutLen,
	xrtblake3error* pError);

/* 文件流式哈希 */
XRT_API bool xrtBlake3File(xreader* pReader, const xrtresourcelimits* pLimits,
	uint8* pOut, size_t iOutLen, xrtprogressproc pProgress, ptr pUserData,
	xrtblake3error* pError);

#ifdef __cplusplus
}
#endif

#endif
