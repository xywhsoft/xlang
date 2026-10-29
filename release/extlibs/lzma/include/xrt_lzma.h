#ifndef XRT_LZMA_H
#define XRT_LZMA_H

/* LZMA 公共契约直接依赖连续 Buffer 与通用 Reader/Writer。 */
#ifndef XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_BUFFER
#endif
#ifndef XRT_MODULE_ONCE
#define XRT_MODULE_ONCE
#endif

#include "xrt.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XRT_LZMA_OPTIONS_VERSION 1u

/* 所有公开入口均使用标准 .xz 流；压缩器 update 缓冲输入，
 * finish 后 drain 才有压缩字节。文件流入口直接逐块读取和写出。 */

typedef enum {
	XRT_LZMA_OK = 0,
	XRT_LZMA_ERROR_ARGUMENT = 1,
	XRT_LZMA_ERROR_MEMORY = 2,
	XRT_LZMA_ERROR_DATA = 3,
	XRT_LZMA_ERROR_TRUNCATED = 4,
	XRT_LZMA_ERROR_LIMIT = 5,
	XRT_LZMA_ERROR_IO = 6,
	XRT_LZMA_ERROR_STATE = 7,
	XRT_LZMA_ERROR_CANCELLED = 8
} xrtlzmastatus;

typedef struct {
	int iCode;
	int iBackendCode;
	uint64 iInputOffset;
	uint64 iOutputOffset;
	char sMessage[256];
} xrtlzmaerror;

typedef struct {
	uint32 iSize;
	uint32 iVersion;
	int iLevel;
	uint8 sReserved[4];
	/* 单项限额保护解码分配；条目、节点、深度与标志必须为零。 */
	xrtresourcelimits tLimits;
} xrtlzmaoptions;

typedef struct xrtlzmacompressor xrtlzmacompressor;
typedef struct xrtlzmadecompressor xrtlzmadecompressor;

XRT_API void xrtLzmaOptionsInit(xrtlzmaoptions* pOptions);
XRT_API void xrtLzmaErrorInit(xrtlzmaerror* pError);

XRT_API xrtlzmacompressor* xrtLzmaCompressorCreate(const xrtlzmaoptions* pOptions);
XRT_API bool xrtLzmaCompressorUpdate(xrtlzmacompressor* pCompressor, const void* pData, size_t iSize);
XRT_API bool xrtLzmaCompressorFinish(xrtlzmacompressor* pCompressor);
XRT_API bool xrtLzmaCompressorDrain(xrtlzmacompressor* pCompressor, xwriter* pWriter);
XRT_API xbuffer* xrtLzmaCompressorTakeBuffer(xrtlzmacompressor* pCompressor);
XRT_API xbuffer* xrtLzmaCompressorDrainBuffer(xrtlzmacompressor* pCompressor);
XRT_API uint64 xrtLzmaCompressorInputBytes(const xrtlzmacompressor* pCompressor);
XRT_API uint64 xrtLzmaCompressorOutputBytes(const xrtlzmacompressor* pCompressor);
XRT_API const xrtlzmaerror* xrtLzmaCompressorError(const xrtlzmacompressor* pCompressor);
XRT_API void xrtLzmaCompressorDestroy(xrtlzmacompressor* pCompressor);

XRT_API xrtlzmadecompressor* xrtLzmaDecompressorCreate(const xrtlzmaoptions* pOptions);
XRT_API bool xrtLzmaDecompressorUpdate(xrtlzmadecompressor* pDecompressor, const void* pData, size_t iSize);
XRT_API bool xrtLzmaDecompressorFinish(xrtlzmadecompressor* pDecompressor);
XRT_API bool xrtLzmaDecompressorDrain(xrtlzmadecompressor* pDecompressor, xwriter* pWriter);
XRT_API xbuffer* xrtLzmaDecompressorTakeBuffer(xrtlzmadecompressor* pDecompressor);
XRT_API xbuffer* xrtLzmaDecompressorDrainBuffer(xrtlzmadecompressor* pDecompressor);
XRT_API uint64 xrtLzmaDecompressorInputBytes(const xrtlzmadecompressor* pDecompressor);
XRT_API uint64 xrtLzmaDecompressorOutputBytes(const xrtlzmadecompressor* pDecompressor);
XRT_API bool xrtLzmaDecompressorFinished(const xrtlzmadecompressor* pDecompressor);
XRT_API uint64 xrtLzmaDecompressorMembers(const xrtlzmadecompressor* pDecompressor);
XRT_API const xrtlzmaerror* xrtLzmaDecompressorError(const xrtlzmadecompressor* pDecompressor);
XRT_API void xrtLzmaDecompressorDestroy(xrtlzmadecompressor* pDecompressor);

XRT_API bool xrtLzmaCompress(const void* pData, size_t iSize, const xrtlzmaoptions* pOptions,
	xbuffer** ppOutput, xrtlzmaerror* pError);
XRT_API bool xrtLzmaDecompress(const void* pData, size_t iSize, const xrtlzmaoptions* pOptions,
	xbuffer** ppOutput, xrtlzmaerror* pError);
XRT_API bool xrtLzmaCompressStream(xreader* pReader, xwriter* pWriter, const xrtlzmaoptions* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtlzmaerror* pError);
XRT_API bool xrtLzmaDecompressStream(xreader* pReader, xwriter* pWriter, const xrtlzmaoptions* pOptions,
	xrtprogressproc pProgress, ptr pUserData, xrtlzmaerror* pError);

#ifdef __cplusplus
}
#endif

#endif
