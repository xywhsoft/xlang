#ifndef XRT_SIPHASH_H
#define XRT_SIPHASH_H

/* 独立 C 用户可在一个编译单元包含本头前定义 XRT_IMPLEMENTATION。
 * 已有 XRT 宿主应在首次包含 xrt.h 前启用下面的模块，不能重复编译实现。 */
#ifndef XRT_MODULE_HASH_KEYED
#define XRT_MODULE_HASH_KEYED
#endif
#ifndef XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_BUFFER
#endif
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
	XRT_SIPHASH_ERROR_LIMIT = 4,
	XRT_SIPHASH_ERROR_CANCELLED = 5,
	XRT_SIPHASH_ERROR_STATE = 6
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

XRT_API void xrtSipHashOptionsInit(xrtsiphashoptions* pOptions);
XRT_API void xrtSipHashErrorInit(xrtsiphasherror* pError);

/* key 必须指向恰好 16 字节；create 复制密钥，digest 不终结流。
 * update 失败后只能 reset 恢复。对象不可被多线程同时访问。
 * 64 位结果为无符号值，0 也可能是有效摘要；用 xrtGetError 判断错误。
 * Error 参数可省略；失败同时记录 XRT 错误，不覆盖 Reader/回调原始错误。 */
XRT_API xrtsiphasher* xrtSipHasherCreate(const xrtsiphashoptions* pOptions,
	const uint8 pKey[XRT_SIPHASH_KEY_LEN]);
XRT_API bool xrtSipHasherUpdate(xrtsiphasher* pHasher, const void* pData, size_t iSize);
XRT_API uint64 xrtSipHasherDigest64(const xrtsiphasher* pHasher);
XRT_API bool xrtSipHasherDigest128(const xrtsiphasher* pHasher, uint8* pOut);
XRT_API bool xrtSipHasherReset(xrtsiphasher* pHasher);
XRT_API uint64 xrtSipHasherInputBytes(const xrtsiphasher* pHasher);
XRT_API xrtsiphasher* xrtSipHasherClone(const xrtsiphasher* pSrc);
XRT_API const xrtsiphasherror* xrtSipHasherError(const xrtsiphasher* pHasher);
XRT_API void xrtSipHasherDestroy(xrtsiphasher* pHasher);

/* 一次性哈希 */
XRT_API uint64 xrtSipHash64(const void* pData, size_t iSize,
	const uint8 pKey[XRT_SIPHASH_KEY_LEN], xrtsiphasherror* pError);
XRT_API bool xrtSipHash128(const void* pData, size_t iSize,
	const uint8 pKey[XRT_SIPHASH_KEY_LEN], uint8* pOut, xrtsiphasherror* pError);

/* 借用 Reader：从当前游标读取，不负责关闭；取消不会提交 128 位输出。 */
XRT_API uint64 xrtSipHashFile64(xreader* pReader, const xrtresourcelimits* pLimits,
	const uint8 pKey[XRT_SIPHASH_KEY_LEN], xrtprogressproc pProgress, ptr pUserData,
	xrtsiphasherror* pError);

XRT_API bool xrtSipHashFile128(xreader* pReader, const xrtresourcelimits* pLimits,
	const uint8 pKey[XRT_SIPHASH_KEY_LEN], uint8 pOut[16], xrtprogressproc pProgress,
	ptr pUserData, xrtsiphasherror* pError);

#ifdef __cplusplus
}
#endif

#endif
